/*
 * Speaker playback and mic capture through the MediaTek /dev/eac misc device.
 * Register values are the 44100 Hz speaker path from the stock HAL.
 */

#define LOG_TAG "AudioEac"

#include <hardware_legacy/AudioHardwareInterface.h>

#include "speech_param_default.h"

#include <cutils/log.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace android {

#define AUD_MAGIC 'C'

#define SET_AUDSYS_REG      _IOWR(AUD_MAGIC, 0x00, unsigned int)
#define SET_ANAAFE_REG      _IOWR(AUD_MAGIC, 0x02, unsigned int)
#define ALLOCATE_MEMIF_DL1  _IOWR(AUD_MAGIC, 0x10, unsigned int)
#define ALLOCATE_MEMIF_ADC  _IOWR(AUD_MAGIC, 0x16, unsigned int)
#define FREE_MEMIF_ADC      _IOWR(AUD_MAGIC, 0x17, unsigned int)
#define AUD_RESTART         _IOWR(AUD_MAGIC, 0x1F, unsigned int)
#define START_MEMIF_TYPE    _IOWR(AUD_MAGIC, 0x20, unsigned int)
#define STANDBY_MEMIF_TYPE  _IOWR(AUD_MAGIC, 0x21, unsigned int)
#define SET_AUDIO_STATE     _IOWR(AUD_MAGIC, 0x42, unsigned int)
#define AUD_SET_CLOCK       _IOWR(AUD_MAGIC, 0x51, int)
#define AUD_SET_26MCLOCK    _IOWR(AUD_MAGIC, 0x52, int)
#define AUD_SET_ADC_CLOCK   _IOWR(AUD_MAGIC, 0x53, int)
#define AUD_SET_ANA_CLOCK   _IOWR(AUD_MAGIC, 0x55, int)
#define SET_SPEAKER_ON      _IOW(AUD_MAGIC, 0xa1, int)
#define SET_SPEAKER_OFF     _IOW(AUD_MAGIC, 0xa2, int)
#define SET_HEADPHONE_ON    _IOW(AUD_MAGIC, 0xa4, int)
#define SET_HEADPHONE_OFF   _IOW(AUD_MAGIC, 0xa5, int)
#define SET_EARPIECE_ON     _IOW(AUD_MAGIC, 0xa6, int)
#define SET_EARPIECE_OFF    _IOW(AUD_MAGIC, 0xa7, int)

/* Modem speech control (stock SpeechMessengerCCCI): 16-byte messages
 * {0xffffffff, id << 16 | param16, 5, param32} written to ccci_pcm_tx; the
 * modem answers on ccci_pcm_rx with id | 0x8000. */
#define CCCI_PCM_TX          "/dev/ccci_pcm_tx"
#define CCCI_PCM_RX          "/dev/ccci_pcm_rx"
#define CCCI_IOC_GET_MD_STATE 0x80044301
#define CCCI_IOC_SHARE_SIZE  0x80044303
#define SPH_ENH_MASK         0x2f24
#define SPH_NB_PARAM         0x2f71
#define SPH_WB_PARAM         0x2f73
#define SPH_PARAM_REQUEST    0xaf70
/* SetVariousKindsOfSpeechParameters: {0xa2a2, 7, len} then data, here. */
#define SPH_PARAM_OFFSET     0x68a
#define SPH_DL_GAIN          0x2f00
#define SPH_UL_GAIN          0x2f01
#define SPH_UL_MUTE          0x2f02
#define SPH_DL_MUTE          0x2f03
#define SPH_SIDETONE_GAIN    0x2f04
#define SPH_SAMPLE_RATE      0x2f10
#define SPH_SPEECH_ON        0x2f20
#define SPH_SPEECH_OFF       0x2f21
#define SPH_SPEECH_MODE      0x2f22
#define SPH_ACK              0x8000
#define SPH_RATE             16000
/* Stock default mic gain for a call: PGA 18 dB (field 2), digital 12 dB. */
#define SPH_MIC_PGA_FIELD    2
#define SPH_UL_DIGITAL       (12 * 4)
/* Combo chip audio interface: AIF_2 is digital FM, AIF_0 is off. */
#define AUDDRV_RESET_BT_FM_GPIO _IOWR(AUD_MAGIC, 0x60, unsigned int)
#define AUDDRV_SET_FM_I2S_GPIO  _IOWR(AUD_MAGIC, 0x62, unsigned int)

/* AFE_GAIN2_CON1 steps for FM, from the stock SetHwDigitalGain table;
 * 0x80000 is 0 dB. */
static const unsigned int kFmGain2[] = {
    0x0, 0x20c, 0x33e, 0x524, 0x827, 0xcec, 0x147a, 0x2075, 0x3371,
    0x5188, 0x8138, 0xcccc, 0x14496, 0x2026f, 0x32f52, 0x50c33, 0x80000,
};
#define FM_GAIN2_STEPS (sizeof(kFmGain2) / sizeof(kFmGain2[0]))

/* Kernel MEMIF_BUFFER_TYPE: MEM_DL1 is 0. */
#define MEM_DL1 0
#define MEM_VUL 2

#define DL1_BYTES (16 * 1024)
#define VUL_BYTES (16 * 1024)
/* The mic path runs at 48000 stereo, as stock records; reads are decimated. */
#define VUL_RATE 48000

struct aud_reg {
    unsigned int offset;
    unsigned int value;
    unsigned int mask;
};

struct sph_ctrl {
    int speech;
    int bgs;
    int record;
    int tty;
    int vt;
    int audio_play;
};

class EacAudio;

class EacStreamOut : public AudioStreamOut {
public:
    EacStreamOut(EacAudio *hw) : mHw(hw) {}
    virtual uint32_t sampleRate() const { return 44100; }
    virtual size_t bufferSize() const { return 4096; }
    virtual uint32_t channels() const { return AudioSystem::CHANNEL_OUT_STEREO; }
    virtual int format() const { return AudioSystem::PCM_16_BIT; }
    virtual uint32_t latency() const { return 80; }
    virtual status_t setVolume(float left, float right);
    virtual ssize_t write(const void *buffer, size_t bytes);
    virtual status_t standby();
    virtual status_t dump(int fd, const Vector<String16> &args);
    virtual status_t setParameters(const String8 &kv);
    virtual String8 getParameters(const String8 &keys);
    virtual status_t getRenderPosition(uint32_t *dspFrames);

private:
    EacAudio *mHw;
};

class EacStreamIn : public AudioStreamIn {
public:
    EacStreamIn(EacAudio *hw, uint32_t rate, uint32_t channels);
    virtual ~EacStreamIn();

    status_t initCheck() const { return mFd >= 0 ? NO_ERROR : NO_INIT; }
    virtual uint32_t sampleRate() const { return mRate; }
    virtual size_t bufferSize() const;
    virtual uint32_t channels() const { return mChannels; }
    virtual int format() const { return AudioSystem::PCM_16_BIT; }
    virtual status_t setGain(float gain) { (void)gain; return NO_ERROR; }
    virtual ssize_t read(void *buffer, ssize_t bytes);
    virtual status_t dump(int fd, const Vector<String16> &args);
    virtual status_t standby();
    virtual status_t setParameters(const String8 &kv) { (void)kv; return NO_ERROR; }
    virtual String8 getParameters(const String8 &keys) { (void)keys; return String8(""); }
    virtual unsigned int getInputFramesLost() const { return 0; }

    static bool supportedRate(uint32_t rate) { return rate >= 8000 && VUL_RATE % rate == 0; }
    bool started() const { return mStarted; }

private:
    status_t start();
    int xioctl(unsigned int cmd, unsigned long arg, const char *what);

    EacAudio *mHw;
    int mFd;
    bool mStarted;
    uint32_t mRate;
    uint32_t mChannels;
    int16_t mRaw[2 * 480];
};

class EacAudio : public AudioHardwareInterface {
public:
    EacAudio();
    virtual ~EacAudio();

    virtual status_t initCheck() { return mFd >= 0 ? NO_ERROR : NO_INIT; }
    virtual status_t setVoiceVolume(float volume);
    virtual status_t setMasterVolume(float volume);
    virtual status_t setMode(int mode);
    virtual status_t setMicMute(bool state);
    virtual status_t getMicMute(bool *state);
    virtual status_t setParameters(const String8 &kv);
    virtual String8 getParameters(const String8 &keys);
    virtual size_t getInputBufferSize(uint32_t sampleRate, int format, int channelCount);
    virtual AudioStreamOut *openOutputStream(uint32_t devices, int *format,
            uint32_t *channels, uint32_t *sampleRate, status_t *status);
    virtual void closeOutputStream(AudioStreamOut *out);
    virtual AudioStreamIn *openInputStream(uint32_t devices, int *format,
            uint32_t *channels, uint32_t *sampleRate, status_t *status,
            AudioSystem::audio_in_acoustics acoustics);
    virtual void closeInputStream(AudioStreamIn *in);
    virtual status_t dumpState(int fd, const Vector<String16> &args);
    virtual status_t dump(int fd, const Vector<String16> &args);

    status_t start();
    void enableDl();
    status_t standbyOut();
    bool micOn() const { return mIn != 0 && mIn->started(); }
    void setFm(bool on);
    void setFmVolume(int index, int max);
    void setFmSpeaker(bool on);
    void outputClose();
    void outputOpen(bool spk, bool hp);
    void setOutDevices(uint32_t devices);
    void applyOutput();
    bool inCall() const { return mInCall; }
    ssize_t writePcm(const void *buffer, size_t bytes);
    uint32_t frames() const { return mFrames; }
    bool micMuted() const { return mMicMute; }

private:
    friend class EacStreamIn;
    int xioctl(unsigned int cmd, unsigned long arg, const char *what);
    void afe(unsigned int off, unsigned int val, unsigned int mask);
    void ana(unsigned int off, unsigned int val, unsigned int mask);

    /* Voice call (stock SpeechPhoneCallController). */
    bool sphOpen();
    void sphClose();
    int sphSend(unsigned int id, unsigned int p16, unsigned int p32, bool ack);
    int sphSendParam(unsigned int id, const unsigned char *data, unsigned int len);
    void sphParams();
    int sphDevice(uint32_t devices) const;
    void callDigital(bool on);
    void callMic(bool on, int dev);
    void callOutput(bool on, int dev);
    void callStart(int dev);
    void callStop();
    void callRoute(int dev);
    void callVolume();

    int mFd;
    bool mStarted;
    bool mDlOn;
    bool mAllocated;
    bool mMicMute;
    bool mFmOn;
    bool mFmSpeaker;
    bool mSpkOn;
    bool mHpOn;
    uint32_t mOutDevices;
    int mMode;
    bool mInCall;
    int mCallDev;
    int mSphTx;
    int mSphRx;
    unsigned char *mSphBuf;
    size_t mSphBufSize;
    bool mSphParamsSent;
    float mVoiceVolume;
    unsigned int mFmGain;
    uint32_t mFrames;
    EacStreamOut *mOut;
    EacStreamIn *mIn;
};

EacAudio::EacAudio()
    : mFd(-1), mStarted(false), mDlOn(false), mAllocated(false), mMicMute(false),
      mFmOn(false), mFmSpeaker(false), mSpkOn(false), mHpOn(false),
      mOutDevices(AudioSystem::DEVICE_OUT_SPEAKER), mMode(AudioSystem::MODE_NORMAL),
      mInCall(false), mCallDev(0), mSphTx(-1), mSphRx(-1),
      mSphBuf(0), mSphBufSize(0), mSphParamsSent(false), mVoiceVolume(1.0f),
      mFmGain(kFmGain2[FM_GAIN2_STEPS - 1]),
      mFrames(0), mOut(0), mIn(0)
{
    mFd = open("/dev/eac", O_RDWR);
    if (mFd < 0)
        LOGE("open /dev/eac failed errno %d", errno);
    else {
        LOGD("open /dev/eac fd %d", mFd);
        /* First call only clears the boot flag. The next one drops a
         * DL1 owner left behind when mediaserver died. */
        xioctl(AUD_RESTART, 0, "restart");
        xioctl(AUD_RESTART, 0, "restart");
    }
}

EacAudio::~EacAudio()
{
    standbyOut();
    if (mFd >= 0)
        close(mFd);
    delete mOut;
    delete mIn;
}

int EacAudio::xioctl(unsigned int cmd, unsigned long arg, const char *what)
{
    int r = ioctl(mFd, cmd, arg);
    if (r < 0)
        LOGE("%s ioctl 0x%x failed errno %d", what, cmd, errno);
    return r;
}

void EacAudio::afe(unsigned int off, unsigned int val, unsigned int mask)
{
    struct aud_reg r;
    r.offset = off;
    r.value = val;
    r.mask = mask;
    xioctl(SET_AUDSYS_REG, (unsigned long)&r, "afe");
}

void EacAudio::ana(unsigned int off, unsigned int val, unsigned int mask)
{
    struct aud_reg r;
    r.offset = off;
    r.value = val;
    r.mask = mask;
    xioctl(SET_ANAAFE_REG, (unsigned long)&r, "ana");
}

status_t EacAudio::start()
{
    struct sph_ctrl st;

    if (mFd < 0)
        return NO_INIT;
    if (mStarted)
        return NO_ERROR;

    xioctl(AUD_SET_CLOCK, 1, "clk");
    xioctl(AUD_SET_ANA_CLOCK, 1, "ana_clk");
    xioctl(AUD_SET_26MCLOCK, 1, "clk26");

    /* The AUDTOP output stage (headset/speaker) opens in applyOutput(),
     * after the platform part, as the stock AnalogOpen does. */
    /* AUDTOP_CON0 bit 1 and AUDTOP_CON1 bit 5 are set on stock while playing. */
    ana(0x700, 0x0002, 0x0002);
    ana(0x702, 0x0020, 0x0020);

    /* PMIC downlink, 44100. New-IF code 7, ABB code 9. */
    ana(0x10c, 0x0100, 0x0100);
    /* TOP_CKPDN1 bit 15 is clear on stock; set here the downlink never pulls DL1. */
    ana(0x10c, 0x8000, 0x8000);
    ana(0x4024, 0x7330, 0xffff);
    ana(0x4002, 0x0009, 0x000f);
    ana(0x4014, 0x0001, 0x0001);
    /* Speaker uses platform device 0: latch ABB_AFE_CON11[9:8], enable bit 1. */
    ana(0x4016, 0x0000, 0x0300);
    ana(0x4016, 0x0300, 0x0300);
    /* ABB_AFE_CON0: stock has bit 0 (downlink on), not bit 1. */
    ana(0x4000, 0x0001, 0x0003);

    afe(0x260, 0, 0xffffffff);
    afe(0x264, 0, 0xffffffff);
    afe(0x108, 0x73001803, 0xffffffff);
    afe(0x10c, 0xf74f0000, 0xffffffff);
    /* I2S rate index 9 is 44100. I2S_CON3 stays off, as on stock. */
    afe(0x4c, 0x00000908, 0xffffffff);
    afe(0x34, 0x00000908, 0xffffffff);
    afe(0x24, 0x00200000, 0x00200000);
    afe(0x28, 0x00000040, 0x00000040);
    /* DL1 rate index 9 (44100), as AFE_DAC_CON1 reads on stock. */
    afe(0x14, 9, 0xf);
    /* IRQ rate index 0x9 is 44100. The IRQ itself stays off until PCM is queued. */
    afe(0x3ac, 1024, 0xffffffff);

    if (!mAllocated) {
        if (xioctl(ALLOCATE_MEMIF_DL1, DL1_BYTES, "alloc") == 0)
            mAllocated = true;
    }
    xioctl(START_MEMIF_TYPE, MEM_DL1, "start");

    memset(&st, 0, sizeof(st));
    st.audio_play = 1;
    xioctl(SET_AUDIO_STATE, (unsigned long)&st, "state");

    mStarted = true;
    applyOutput();
    LOGD("speaker path on");
    return NO_ERROR;
}

/*
 * Stock AudioAnalogControl AnalogOpen/AnalogClose, machine part. Headset
 * (device 2) and speaker (device 4) leave AUDTOP_CON4/6 (0x708/0x70c) in
 * different states, so switching closes the old output before opening the
 * new one. Both at once is the stock headset+speaker device (6).
 */
void EacAudio::outputClose()
{
    if (!mSpkOn && !mHpOn)
        return;
    if (mHpOn) {
        xioctl(SET_HEADPHONE_OFF, 0, "hp_off");
        ana(0x70a, 0x0014, 0xffff);
    }
    if (mSpkOn) {
        xioctl(SET_SPEAKER_OFF, 0, "spk_off");
        ana(0x052, 0x0004, 0xffff);
        ana(0x06a, 0x0000, 0xffff);
        ana(0x70e, 0x2400, 0xffff);
    }
    /* Stock AnalogClose also clears AUDTOP_CON4 (0x708) and parks 0x70c, but
     * the mic path (EacStreamIn::start) never sets 0x708 and records zeros
     * without it. outputOpen() rewrites both, so leave them. */
    mSpkOn = mHpOn = false;
}

void EacAudio::outputOpen(bool spk, bool hp)
{
    if (hp) {
        ana(0x70c, 0xf7f2, 0xffff);
        ana(0x700, 0x7000, 0xf000);
        ana(0x70a, 0x0014, 0xffff);
        ana(0x708, 0x007c, 0xffff);
        usleep(10000);
        ana(0x70c, 0xf5ba, 0xffff);
        ana(0x70a, 0x2214, 0xffff);
    } else if (spk) {
        /* Not in the stock speaker open; the speaker-only path has always
         * run with it, so keep it. */
        ana(0x70a, 0x5514, 0xffff);
        ana(0x70e, 0x2400, 0xffff);
        ana(0x70c, 0xb7f6, 0xffff);
        ana(0x700, 0x7000, 0xf000);
        ana(0x708, 0x0014, 0xffff);
        usleep(10000);
    }
    if (spk) {
        /* Class-D loudspeaker. SET_SPEAKER_ON does not touch the PMIC on MT6572. */
        ana(0x70e, 0x35b0, 0xffff);
        ana(0x10c, 0x000e, 0x000e);
        ana(0x056, 0x0214, 0xffff);
        ana(0x064, 0x0600, 0xffff);
        ana(0x054, 0x8055, 0xffff);
        ana(0x052, 0x3008, 0xffff);
        ana(0x052, 0x3009, 0xffff);
        usleep(5000);
        ana(0x052, 0x3001, 0xffff);
        ana(0x06a, 0x0a00, 0xffff);
        xioctl(SET_SPEAKER_ON, 0, "spk");
    }
    if (hp)
        xioctl(SET_HEADPHONE_ON, 0, "hp");
    mSpkOn = spk;
    mHpOn = hp;
}

void EacAudio::setOutDevices(uint32_t devices)
{
    if (devices == 0)
        return;
    /* As stock doSetMode: the routing after setMode(IN_CALL) opens the call. */
    if (mMode == AudioSystem::MODE_IN_CALL) {
        mOutDevices = devices;
        if (!mInCall)
            callStart(sphDevice(devices));
        else if (sphDevice(devices) != mCallDev)
            callRoute(sphDevice(devices));
        return;
    }
    if (devices == mOutDevices)
        return;
    LOGD("routing 0x%x", devices);
    mOutDevices = devices;
    applyOutput();
}

/*
 * Music follows the policy routing. FM follows its speaker button: the
 * headset is its antenna, so "not speaker" means the headset.
 */
void EacAudio::applyOutput()
{
    const uint32_t hpMask = AudioSystem::DEVICE_OUT_WIRED_HEADSET |
                            AudioSystem::DEVICE_OUT_WIRED_HEADPHONE;
    bool spk, hp;

    if (!mStarted)
        return;
    if (mFmOn) {
        spk = mFmSpeaker;
        hp = !mFmSpeaker;
    } else {
        spk = (mOutDevices & AudioSystem::DEVICE_OUT_SPEAKER) != 0;
        hp = (mOutDevices & hpMask) != 0;
        if (!spk && !hp)
            spk = true;
    }
    if (spk == mSpkOn && hp == mHpOn)
        return;
    outputClose();
    outputOpen(spk, hp);
    LOGD("output speaker %d headphone %d", spk, hp);
}

/*
 * DL1 and the mic (VUL) share AFE_DAC_CON0 bit 0 (AFE on) and AFE_IRQ_CON:
 * IRQ1 (bit 0, rate in bits 4-7) is DL1, IRQ2 (bit 1, rate in bits 8-11)
 * is the mic. The output path touches only its own bits, so a sound played
 * or stopped while recording no longer stalls AudioRecord.
 */
void EacAudio::enableDl()
{
    afe(0x10, 0, micOn() ? 0x2 : 0x3);
    afe(0x10, 0x2, 0x2);
    afe(0x34, 0x1, 0x1);
    /* AFE_DAC_CON0 bit 10 is set on stock; it is not named in AudDrv_Afe.h. */
    afe(0x10, 0x400, 0x400);
    afe(0x108, 0x1, 0x1);
    afe(0x124, 0x1, 0x1);
    afe(0x3a0, 0x91, 0xf1);
    afe(0x10, 0x1, 0x1);
    mDlOn = true;
}

status_t EacAudio::standbyOut()
{
    if (mFd < 0 || !mStarted)
        return NO_ERROR;
    if (mFmOn) {
        /* FM keeps the AFE, DAC and amplifiers; only DL1 stops. */
        afe(0x3a0, 0, 0x1);
        afe(0x10, 0, 0x2);
        mDlOn = false;
        xioctl(STANDBY_MEMIF_TYPE, MEM_DL1, "standby");
        return NO_ERROR;
    }
    afe(0x3a0, 0, 0x1);
    afe(0x10, 0, micOn() ? 0x2 : 0x3);
    mDlOn = false;
    xioctl(STANDBY_MEMIF_TYPE, MEM_DL1, "standby");
    outputClose();
    ana(0x10a, 0x0014, 0xffff);
    mStarted = false;
    LOGD("speaker path standby");
    return NO_ERROR;
}

/*
 * Digital FM, as the stock SetFmChip + SetFmDirectConnection: the MT6627
 * merge interface arrives on the 2nd I2S-in (I00/I01) at 32 kHz, ASRC brings
 * it to the 44100 DAC, and GAIN2 (I12/I13 -> O03/O04) sets its level. It
 * mixes in hardware with DL1 (I05/I06), so music keeps working.
 */
void EacAudio::setFm(bool on)
{
    if (mFd < 0 || on == mFmOn)
        return;
    if (on) {
        if (start() != NO_ERROR)
            return;
        xioctl(AUDDRV_SET_FM_I2S_GPIO, 0, "fm_gpio");
        /* DAC I2S out on, as SetI2SDacEnable; DL1 stays off without PCM. */
        afe(0x108, 0x1, 0x1);
        afe(0x34, 0x1, 0x1);
        afe(0x124, 0x1, 0x1);
        afe(0x4c4, 0, 0x10);
        /* I00->O15, I12->O03, I01->O16, I13->O04 (AFE_GAIN2_CONN). */
        afe(0x438, 1 << 16, 1 << 16);
        afe(0x438, 1 << 8, 1 << 8);
        afe(0x438, 1 << 23, 1 << 23);
        afe(0x438, 1 << 10, 1 << 10);
        /* GAIN2 at 44100 (rate 9), step 200. */
        afe(0x428, 0xc890, 0xfff0);
        afe(0x42c, mFmGain, 0xffffffff);
        afe(0x43c, 0, 0xffffffff);
        afe(0x428, 0x1, 0x1);
        /* 2nd I2S-in: slave, I2S, 16 bit. */
        afe(0x18, 0x8000000c, 0xfffffffe);
        afe(0x4c4, 0, 0x100);
        /* ASRC 32000 -> 44100. */
        afe(0x30, 0, 0x40000000);
        afe(0x14, 0x900, 0xf00);
        afe(0x550, 0, 0x10000);
        afe(0x554, 0xdc8000, 0xffffffff);
        afe(0x558, 0xa00000, 0xffffffff);
        afe(0x560, 0x1fbd, 0xffffffff);
        afe(0x55c, 0x75987, 0xffffffff);
        afe(0x56c, 0x1b00, 0xffffffff);
        afe(0x500, 0x41, 0x41);
        afe(0x18, 0x1, 0x1);
        afe(0x10, 0x1, 0x1);
        mFmOn = true;
        /* FMRadio starts on the headset; its speaker button sends AudioSetFmSpeaker. */
        applyOutput();
        LOGD("fm digital path on, speaker %d", mFmSpeaker);
    } else {
        xioctl(AUDDRV_RESET_BT_FM_GPIO, 0, "fm_gpio_off");
        afe(0x438, 0, 1 << 16);
        afe(0x438, 0, 1 << 8);
        afe(0x438, 0, 1 << 23);
        afe(0x438, 0, 1 << 10);
        afe(0x428, 0, 0x1);
        afe(0x500, 0, 0x41);
        afe(0x30, 0x40000000, 0x40000000);
        afe(0x18, 0, 0x1);
        mFmOn = false;
        LOGD("fm digital path off");
        if (!mDlOn)
            standbyOut();
        else
            applyOutput();
    }
}

void EacAudio::setFmVolume(int index, int max)
{
    if (max <= 0)
        return;
    if (index < 0)
        index = 0;
    if (index > max)
        index = max;
    mFmGain = kFmGain2[(index * (FM_GAIN2_STEPS - 1) + max / 2) / max];
    LOGD("fm volume %d/%d gain2 0x%x", index, max, mFmGain);
    if (mFmOn)
        afe(0x42c, mFmGain, 0xffffffff);
}

void EacAudio::setFmSpeaker(bool on)
{
    mFmSpeaker = on;
    applyOutput();
}

/*
 * Voice call, following the stock SpeechPhoneCallController and
 * SpeechDriverLAD (libaudioflinger.so). The modem does the speech; the AP
 * routes the mic to it and its downlink to the output over PCM2 at 16 kHz.
 * Devices: 0 earpiece, 1 wired headset (headset mic), 2 speaker,
 * 3 wired headphone (main mic).
 */
int EacAudio::sphDevice(uint32_t devices) const
{
    if (devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET)
        return 1;
    if (devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)
        return 3;
    if (devices & AudioSystem::DEVICE_OUT_SPEAKER)
        return 2;
    return 0;
}

static int sphMode(int dev)
{
    /* Stock speech mode: earpiece 0, headset 1, loudspeaker 2. */
    return dev == 2 ? 2 : (dev == 0 ? 0 : 1);
}

bool EacAudio::sphOpen()
{
    int state = -1;

    if (mSphTx >= 0)
        return true;
    mSphRx = open(CCCI_PCM_RX, O_RDWR);
    mSphTx = open(CCCI_PCM_TX, O_RDWR);
    if (mSphRx < 0 || mSphTx < 0) {
        LOGE("open ccci_pcm failed errno %d", errno);
        sphClose();
        return false;
    }
    if (ioctl(mSphRx, CCCI_IOC_GET_MD_STATE, &state) < 0)
        LOGW("modem state ioctl errno %d", errno);
    /* Stock maps the shared buffer on the rx fd; parameters go through it. */
    unsigned int size = 0;
    if (ioctl(mSphRx, CCCI_IOC_SHARE_SIZE, &size) == 0 && size > 0) {
        void *p = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, mSphRx, 0);
        if (p != MAP_FAILED) {
            mSphBuf = (unsigned char *)p;
            mSphBufSize = size;
        } else {
            LOGE("ccci share mmap %u errno %d", size, errno);
        }
    }
    LOGD("speech ccci open, modem state %d, share %u", state, mSphBufSize);
    return true;
}

void EacAudio::sphClose()
{
    if (mSphBuf)
        munmap(mSphBuf, mSphBufSize);
    mSphBuf = 0;
    mSphBufSize = 0;
    if (mSphTx >= 0)
        close(mSphTx);
    if (mSphRx >= 0)
        close(mSphRx);
    mSphTx = mSphRx = -1;
}

int EacAudio::sphSend(unsigned int id, unsigned int p16, unsigned int p32, bool ack)
{
    unsigned int msg[4] = { 0xffffffff, (id << 16) | (p16 & 0xffff), 5, p32 };
    struct pollfd pfd;
    int i;

    if (mSphTx < 0)
        return -1;
    if (write(mSphTx, msg, sizeof(msg)) != (ssize_t)sizeof(msg)) {
        LOGE("speech msg 0x%x write errno %d", id, errno);
        return -1;
    }
    if (!ack)
        return 0;
    pfd.fd = mSphRx;
    pfd.events = POLLIN;
    for (i = 0; i < 4; i++) {
        unsigned int r[4];
        pfd.revents = 0;
        if (poll(&pfd, 1, 250) <= 0)
            continue;
        if (read(mSphRx, r, sizeof(r)) != (ssize_t)sizeof(r))
            continue;
        if ((r[1] >> 16) == (id | SPH_ACK))
            return 0;
        if ((r[1] >> 16) == SPH_PARAM_REQUEST)
            mSphParamsSent = false;
        LOGD("speech rx 0x%08x 0x%08x while waiting 0x%x", r[1], r[3], id);
    }
    LOGE("speech msg 0x%x: no ack", id);
    return -1;
}

int EacAudio::sphSendParam(unsigned int id, const unsigned char *data, unsigned int len)
{
    unsigned char *p = mSphBuf + SPH_PARAM_OFFSET;
    unsigned short hdr[3] = { 0xa2a2, 7, (unsigned short)len };

    if (!mSphBuf || SPH_PARAM_OFFSET + 6 + len > mSphBufSize) {
        LOGE("speech param 0x%x: no share buffer (%u)", id, mSphBufSize);
        return -1;
    }
    memcpy(p, hdr, sizeof(hdr));
    memcpy(p + 6, data, len);
    return sphSend(id, len + 6, SPH_PARAM_OFFSET, true);
}

/* Stock SetAllSpeechEnhancementInfoToModem, answer to the modem's 0xAF70. */
void EacAudio::sphParams()
{
    if (mSphParamsSent)
        return;
    if (sphSendParam(SPH_NB_PARAM, kSpeechParamNB, sizeof(kSpeechParamNB)) == 0 &&
            sphSendParam(SPH_WB_PARAM, kSpeechParamWB, sizeof(kSpeechParamWB)) == 0) {
        sphSend(SPH_ENH_MASK, 0xffbf, 0xffffffff, false);
        mSphParamsSent = true;
        LOGD("speech parameters sent");
    }
}

/* Stock OpenModemSpeechControlFlow digital part, 16 kHz. */
void EacAudio::callDigital(bool on)
{
    if (on) {
        /* I03->O17, I04->O18 (ADC to modem), I14->O03/O04 (modem to DAC). */
        afe(0x30, 1 << 13, 1 << 13);
        afe(0x30, 1 << 16, 1 << 16);
        afe(0x2c, 1 << 27, 1 << 27);
        afe(0x30, 1, 1);
        afe(0x120, 0, 1);
        afe(0x114, 0xa0000, 0x1e0000);
        afe(0x138, 0x03f87201, 0xffffffff);
        afe(0x13c, 0x400, 0xc00);
        afe(0x260, 0, 0xffffffff);
        afe(0x264, 0, 0xffffffff);
        afe(0x108, 0x33001823, 0xffffffff);
        afe(0x10c, 0xf74f0000, 0xffffffff);
        afe(0x34, 0x408, 0xffffffff);
        afe(0x114, 1, 1);
        afe(0x124, 1, 1);
        afe(0x108, 1, 1);
        afe(0x34, 1, 1);
        afe(0x124, 1, 1);
        afe(0x4c4, 0, 0x10);
    } else {
        afe(0x108, 0, 1);
        afe(0x34, 0, 1);
        afe(0x4c4, 0x10, 0x10);
        afe(0x114, 0, 1);
        afe(0x124, 0, 1);
        afe(0x30, 0, 1 << 13);
        afe(0x30, 0, 1 << 16);
        afe(0x2c, 0, 1 << 27);
        afe(0x30, 0, 1);
    }
}

/* Uplink analog: stock AnalogOpen/Close for the main or headset mic. */
void EacAudio::callMic(bool on, int dev)
{
    if (on) {
        ana(0x10c, 0x100, 0x100);
        ana(0x4028, 0x342f, 0xffff);
        ana(0x4002, 0, 0x10);
        ana(0x4016, 0, 0x300);
        ana(0x4016, 0x300, 0x300);
        ana(0x4000, 2, 2);
        ana(0x700, 0x7800, 0x7f80);
        ana(0x704, 0xf0, 0xff);
        ana(0x706, 0, 0x100);
        if (dev == 1) {
            ana(0x700, 1, 0xf);
            ana(0x706, 0, 0x200);
            ana(0x702, 0x10, 0xf0);
        } else {
            ana(0x700, 0, 0xf);
            ana(0x706, 0x200, 0x200);
            ana(0x702, 0x40, 0xf0);
        }
        ana(0x702, 0x200, 0x700);
        ana(0x700, 0x20, 0x70);
        ana(0x710, 8, 8);
        ana(0x700, 0x180, 0x180);
        ana(0x704, 3, 3);
        ana(0x700, SPH_MIC_PGA_FIELD << 4, 0x70);
        ana(0x702, SPH_MIC_PGA_FIELD << 8, 0x700);
    } else {
        ana(0x700, 0, 0x180);
        ana(0x704, 0, 3);
        ana(0x700, 0x6000, 0x7f80);
        ana(0x704, 0xc0, 0xff);
        ana(0x706, 0, 0x100);
        ana(0x700, 3, 0xf);
        ana(0x706, 0, 0x200);
        ana(0x702, 0x20, 0xf0);
        ana(0x710, 0, 8);
        ana(0x4000, 0, 2);
        ana(0x10a, 0x100, 0x100);
    }
}

/* Downlink analog: stock AnalogOpen/Close at 16 kHz for the call device. */
void EacAudio::callOutput(bool on, int dev)
{
    bool hp = dev == 1 || dev == 3;
    bool spk = dev == 2;

    if (on) {
        ana(0x10c, 0x100, 0x100);
        ana(0x4024, 0x3330, 0xffff);
        ana(0x4002, 4, 0xf);
        if (hp)
            ana(0x4014, 1, 1);
        ana(0x4016, 0, 0x300);
        ana(0x4016, 0x300, 0x300);
        ana(0x4000, 1, 1);
        if (hp) {
            ana(0x70c, 0xf7f2, 0xffff);
            ana(0x700, 0x7000, 0xf000);
            ana(0x70a, 0x0014, 0xffff);
            ana(0x708, 0x007c, 0xffff);
            usleep(10000);
            ana(0x70c, 0xf5ba, 0xffff);
            ana(0x70a, 0x2214, 0xffff);
            xioctl(SET_HEADPHONE_ON, 0, "hp");
            return;
        }
        ana(0x70e, 0x2400, 0xffff);
        ana(0x70c, 0xb7f6, 0xffff);
        ana(0x700, 0x7000, 0xf000);
        ana(0x708, 0x0014, 0xffff);
        usleep(10000);
        if (spk) {
            ana(0x70e, 0x35b0, 0xffff);
            ana(0x10c, 0x000e, 0x000e);
            ana(0x056, 0x0214, 0xffff);
            ana(0x064, 0x0400, 0xffff);
            ana(0x052, 0x3008, 0xffff);
            ana(0x052, 0x3009, 0xffff);
            usleep(5000);
            ana(0x052, 0x3001, 0xffff);
            ana(0x06a, 0x0a00, 0xffff);
            xioctl(SET_SPEAKER_ON, 0, "spk");
        } else {
            ana(0x70e, 0x25b0, 0xffff);
            xioctl(SET_EARPIECE_ON, 0, "ear");
        }
    } else {
        if (hp) {
            xioctl(SET_HEADPHONE_OFF, 0, "hp_off");
            ana(0x70a, 0x0014, 0xffff);
            ana(0x70c, 0xf7f2, 0xffff);
        } else {
            if (spk) {
                xioctl(SET_SPEAKER_OFF, 0, "spk_off");
                ana(0x052, 0x0004, 0xffff);
                ana(0x06a, 0x0000, 0xffff);
                ana(0x10a, 0x000e, 0x000e);
            }
            xioctl(SET_EARPIECE_OFF, 0, "ear_off");
            ana(0x70e, 0x2500, 0xffff);
            ana(0x70e, 0x2400, 0xffff);
        }
        ana(0x708, 0x0000, 0xffff);
        ana(0x70c, 0x37e2, 0xffff);
        ana(0x4000, 0, 1);
    }
}

/* Stock voice volume: 0.25 dB steps, analog buffer first, rest on the modem. */
void EacAudio::callVolume()
{
    int deg, dl;

    if (mVoiceVolume <= 0.0f)
        deg = 64;
    else
        deg = (int)(-34.74 * log(mVoiceVolume) + 0.5) >> 2;
    if (deg < 0)
        deg = 0;
    if (mCallDev == 1 || mCallDev == 3) {
        int h = 7 - deg / 2;
        if (h < 0) {
            dl = -4 * (deg - 14);
            h = 0;
        } else {
            dl = -4 * (deg & 1);
        }
        ana(0x70a, h << 12, 0x7000);
        ana(0x70a, h << 8, 0x700);
    } else {
        if (mCallDev == 2) {
            ana(0x064, 0xa00, 0xf00);
            deg += 3;
        }
        if (deg < 15) {
            int f = 15 - deg / 2;
            ana(0x70e, (f < 8 ? 8 : f) << 4, 0xf0);
            dl = -4 * (deg & 1);
        } else {
            ana(0x70e, 8 << 4, 0xf0);
            dl = -4 * (deg - 14);
        }
    }
    sphSend(SPH_DL_GAIN, dl, 0, false);
    LOGD("call volume %.2f deg %d dl %d", mVoiceVolume, deg, dl);
}

void EacAudio::callStart(int dev)
{
    struct sph_ctrl st;

    if (!sphOpen())
        return;
    sphParams();
    /* Stock ForceAllStandby: the call takes the AFE and analog path. */
    standbyOut();
    memset(&st, 0, sizeof(st));
    st.speech = 1;
    xioctl(SET_AUDIO_STATE, (unsigned long)&st, "state");
    xioctl(AUD_SET_CLOCK, 1, "clk");
    xioctl(AUD_SET_ANA_CLOCK, 1, "ana_clk");
    xioctl(AUD_SET_26MCLOCK, 1, "clk26");
    callDigital(true);
    afe(0x10, 1, 1);
    sphSend(SPH_SIDETONE_GAIN, 0, 0, false);
    callMic(true, dev);
    sphSend(SPH_UL_GAIN, SPH_UL_DIGITAL, 0, false);
    usleep(100000);
    callOutput(true, dev);
    mCallDev = dev;
    mInCall = true;
    callVolume();
    /* PCM2 to the modem. */
    afe(0x53c, 0x0e, 0xffffffff);
    afe(0x53c, 1, 1);
    sphSend(SPH_SAMPLE_RATE, SPH_RATE, 0, false);
    sphSend(SPH_SPEECH_MODE, sphMode(dev), 0, true);
    usleep(1000);
    sphSend(SPH_SPEECH_ON, 0, 0, true);
    if (mMicMute)
        sphSend(SPH_UL_MUTE, 1, 0, false);
    LOGD("call on, device %d", dev);
}

void EacAudio::callRoute(int dev)
{
    LOGD("call device %d -> %d", mCallDev, dev);
    sphSend(SPH_DL_MUTE, 1, 0, false);
    sphSend(SPH_UL_MUTE, 1, 0, false);
    callOutput(false, mCallDev);
    afe(0x53c, 0, 1);
    callMic(false, mCallDev);
    callDigital(false);
    callDigital(true);
    afe(0x10, 1, 1);
    sphSend(SPH_SIDETONE_GAIN, 0, 0, false);
    callMic(true, dev);
    sphSend(SPH_UL_GAIN, SPH_UL_DIGITAL, 0, false);
    usleep(100000);
    callOutput(true, dev);
    mCallDev = dev;
    callVolume();
    afe(0x53c, 0x0e, 0xffffffff);
    afe(0x53c, 1, 1);
    sphSend(SPH_SAMPLE_RATE, SPH_RATE, 0, false);
    sphSend(SPH_SPEECH_MODE, sphMode(dev), 0, true);
    sphSend(SPH_UL_MUTE, mMicMute ? 1 : 0, 0, false);
    sphSend(SPH_DL_MUTE, 0, 0, false);
}

void EacAudio::callStop()
{
    struct sph_ctrl st;

    callOutput(false, mCallDev);
    afe(0x53c, 0, 1);
    callMic(false, mCallDev);
    callDigital(false);
    sphSend(SPH_SPEECH_OFF, 0, 0, true);
    if (!micOn())
        afe(0x10, 0, 1);
    xioctl(AUD_SET_ANA_CLOCK, 0, "ana_clk_off");
    xioctl(AUD_SET_CLOCK, 0, "clk_off");
    memset(&st, 0, sizeof(st));
    xioctl(SET_AUDIO_STATE, (unsigned long)&st, "state");
    mInCall = false;
    sphClose();
    LOGD("call off");
}

ssize_t EacAudio::writePcm(const void *buffer, size_t bytes)
{
    ssize_t n;
    if (mInCall) {
        /* The call owns the DAC and the analog path; drop app sounds at
         * the stream rate, as stock keeps playback in standby. */
        usleep(bytes * 1000000ULL / (44100 * 4));
        return bytes;
    }
    if (start() != NO_ERROR)
        return -1;
    n = ::write(mFd, buffer, bytes);
    if (n > 0) {
        if (!mDlOn)
            enableDl();
        mFrames += n / 4;
    }
    else if (n < 0)
        LOGE("write %d errno %d", (int)bytes, errno);
    return n;
}

status_t EacStreamOut::setVolume(float left, float right)
{
    (void)left;
    (void)right;
    return NO_ERROR;
}

ssize_t EacStreamOut::write(const void *buffer, size_t bytes)
{
    return mHw->writePcm(buffer, bytes);
}

status_t EacStreamOut::standby()
{
    return mHw->standbyOut();
}

status_t EacStreamOut::dump(int fd, const Vector<String16> &args)
{
    (void)fd;
    (void)args;
    return NO_ERROR;
}

status_t EacStreamOut::setParameters(const String8 &kv)
{
    AudioParameter param(kv);
    int devices;

    if (param.getInt(String8(AudioParameter::keyRouting), devices) == NO_ERROR)
        mHw->setOutDevices(devices);
    return NO_ERROR;
}

String8 EacStreamOut::getParameters(const String8 &keys)
{
    (void)keys;
    return String8("");
}

status_t EacStreamOut::getRenderPosition(uint32_t *dspFrames)
{
    if (dspFrames == 0)
        return BAD_VALUE;
    *dspFrames = mHw->frames();
    return NO_ERROR;
}

status_t EacAudio::setVoiceVolume(float volume)
{
    mVoiceVolume = volume;
    if (mInCall)
        callVolume();
    return NO_ERROR;
}

status_t EacAudio::setMasterVolume(float volume)
{
    (void)volume;
    return INVALID_OPERATION;
}

status_t EacAudio::setMode(int mode)
{
    LOGD("setMode %d", mode);
    mMode = mode;
    if (mode != AudioSystem::MODE_IN_CALL && mInCall)
        callStop();
    return NO_ERROR;
}

status_t EacAudio::setMicMute(bool state)
{
    mMicMute = state;
    if (mInCall)
        sphSend(SPH_UL_MUTE, state ? 1 : 0, 0, false);
    return NO_ERROR;
}

status_t EacAudio::getMicMute(bool *state)
{
    if (state)
        *state = mMicMute;
    return NO_ERROR;
}

status_t EacAudio::setParameters(const String8 &kv)
{
    AudioParameter param(kv);
    int fm;
    int max = 15;

    LOGD("setParameters %s", kv.string());

    if (param.getInt(String8("AudioSetFmVolume"), fm) == NO_ERROR) {
        param.getInt(String8("AudioSetFmVolumeMax"), max);
        setFmVolume(fm, max);
    }
    if (param.getInt(String8("AudioSetFmSpeaker"), fm) == NO_ERROR)
        setFmSpeaker(fm != 0);
    if (param.getInt(String8("AudioSetFmDigitalEnable"), fm) == NO_ERROR)
        setFm(fm != 0);
    return NO_ERROR;
}

String8 EacAudio::getParameters(const String8 &keys)
{
    (void)keys;
    return String8("");
}

size_t EacAudio::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    if (format != AudioSystem::PCM_16_BIT || !EacStreamIn::supportedRate(sampleRate) ||
            channelCount < 1 || channelCount > 2)
        return 0;
    /* 20 ms */
    return (sampleRate / 50) * channelCount * 2;
}

AudioStreamOut *EacAudio::openOutputStream(uint32_t devices, int *format,
        uint32_t *channels, uint32_t *sampleRate, status_t *status)
{
    LOGD("openOutputStream devices 0x%x", devices);
    setOutDevices(devices);
    if (format)
        *format = AudioSystem::PCM_16_BIT;
    if (channels)
        *channels = AudioSystem::CHANNEL_OUT_STEREO;
    if (sampleRate)
        *sampleRate = 44100;
    if (mOut == 0)
        mOut = new EacStreamOut(this);
    if (status)
        *status = mOut ? NO_ERROR : NO_MEMORY;
    start();
    return mOut;
}

void EacAudio::closeOutputStream(AudioStreamOut *out)
{
    if (out == mOut) {
        standbyOut();
        delete mOut;
        mOut = 0;
    }
}

AudioStreamIn *EacAudio::openInputStream(uint32_t devices, int *format,
        uint32_t *channels, uint32_t *sampleRate, status_t *status,
        AudioSystem::audio_in_acoustics acoustics)
{
    uint32_t rate = sampleRate ? *sampleRate : 8000;
    uint32_t ch = channels ? *channels : AudioSystem::CHANNEL_IN_MONO;
    bool bad = false;

    (void)acoustics;
    LOGD("openInputStream devices 0x%x rate %u channels 0x%x", devices, rate, ch);

    /* Propose something AudioFlinger can convert from (rate <= 2x request). */
    if (format && *format != AudioSystem::PCM_16_BIT) {
        *format = AudioSystem::PCM_16_BIT;
        bad = true;
    }
    if (ch != AudioSystem::CHANNEL_IN_MONO && ch != AudioSystem::CHANNEL_IN_STEREO) {
        ch = AudioSystem::CHANNEL_IN_MONO;
        bad = true;
    }
    if (!EacStreamIn::supportedRate(rate)) {
        rate = rate > 24000 ? 48000 : (rate > 16000 ? 24000 : (rate > 8000 ? 16000 : 8000));
        bad = true;
    }
    if (bad || mIn) {
        if (sampleRate)
            *sampleRate = rate;
        if (channels)
            *channels = ch;
        if (status)
            *status = mIn ? INVALID_OPERATION : BAD_VALUE;
        return 0;
    }

    EacStreamIn *in = new EacStreamIn(this, rate, ch);
    if (in->initCheck() != NO_ERROR) {
        delete in;
        if (status)
            *status = NO_INIT;
        return 0;
    }
    mIn = in;
    if (status)
        *status = NO_ERROR;
    return in;
}

void EacAudio::closeInputStream(AudioStreamIn *in)
{
    if (in == mIn) {
        delete mIn;
        mIn = 0;
    }
}

/*
 * Mic capture through VUL. The memif belongs to the file it was allocated
 * on, so the stream has its own /dev/eac. Register values are what a stock
 * Y220 shows in /proc/audio while its sound recorder runs.
 */
EacStreamIn::EacStreamIn(EacAudio *hw, uint32_t rate, uint32_t channels)
    : mHw(hw), mFd(-1), mStarted(false), mRate(rate), mChannels(channels)
{
    mFd = open("/dev/eac", O_RDWR);
    if (mFd < 0)
        LOGE("open /dev/eac for input failed errno %d", errno);
}

EacStreamIn::~EacStreamIn()
{
    standby();
    if (mFd >= 0)
        close(mFd);
}

int EacStreamIn::xioctl(unsigned int cmd, unsigned long arg, const char *what)
{
    int r = ioctl(mFd, cmd, arg);
    if (r < 0)
        LOGE("in %s ioctl 0x%x failed errno %d", what, cmd, errno);
    return r;
}

size_t EacStreamIn::bufferSize() const
{
    return (mRate / 50) * AudioSystem::popCount(mChannels) * 2;
}

status_t EacStreamIn::start()
{
    if (mStarted)
        return NO_ERROR;
    if (mFd < 0)
        return NO_INIT;

    xioctl(AUD_SET_ANA_CLOCK, 1, "ana_clk");
    xioctl(AUD_SET_26MCLOCK, 1, "clk26");
    xioctl(AUD_SET_ADC_CLOCK, 1, "adc_clk");

    /* PMIC: uplink on (ABB_AFE_CON0 bit 1), uplink rate, mic bias and ADC. */
    mHw->ana(0x10c, 0x8100, 0x8100);
    mHw->ana(0x4002, 0x0010, 0x0010);
    mHw->ana(0x4028, 0x0c00, 0x0f00);
    mHw->ana(0x4000, 0x0002, 0x0002);
    mHw->ana(0x700, 0x79b0, 0xffff);
    mHw->ana(0x702, 0x0340, 0xffff);
    mHw->ana(0x704, 0x00f3, 0xffff);
    mHw->ana(0x706, 0x0200, 0xffff);
    mHw->ana(0x70c, 0x37e2, 0xffff);
    mHw->ana(0x70e, 0x2400, 0xffff);
    mHw->ana(0x710, 0x0208, 0xffff);

    if (xioctl(ALLOCATE_MEMIF_ADC, VUL_BYTES, "alloc_vul") < 0)
        return NO_INIT;
    xioctl(START_MEMIF_TYPE, MEM_VUL, "start_vul");

    /* AFE: ADC to VUL (CONN3), VUL at 48000, UL source, IRQ2 every 800. */
    mHw->afe(0x2c, 0x00000009, 0xffffffff);
    mHw->afe(0x14, 0x000a0000, 0x000f0000);
    mHw->afe(0x114, 0x001e0001, 0xffffffff);
    mHw->afe(0x138, 0x00000001, 0x00000001);
    mHw->afe(0x13c, 0x00000c00, 0x00000c00);
    mHw->afe(0x3d4, 0x00000001, 0x0000000f);
    mHw->afe(0x3b0, 0x00000320, 0xffffffff);
    mHw->afe(0x3a0, 0x00000402, 0x00000f02);
    mHw->afe(0x10, 0x00000009, 0x00000009);

    mStarted = true;
    LOGD("mic path on, %u Hz to %u Hz", VUL_RATE, mRate);
    return NO_ERROR;
}

status_t EacStreamIn::standby()
{
    if (!mStarted)
        return NO_ERROR;
    mHw->afe(0x10, 0, 0x8);
    mHw->afe(0x3a0, 0, 0x2);
    mHw->afe(0x114, 0, 0x1);
    xioctl(STANDBY_MEMIF_TYPE, MEM_VUL, "standby_vul");
    xioctl(FREE_MEMIF_ADC, 0, "free_vul");
    mHw->ana(0x4000, 0, 0x2);
    xioctl(AUD_SET_ADC_CLOCK, 0, "adc_clk");
    mStarted = false;
    LOGD("mic path standby");
    return NO_ERROR;
}

/* Read 48000 stereo from VUL and average it down to mRate, mono or stereo. */
ssize_t EacStreamIn::read(void *buffer, ssize_t bytes)
{
    if (start() != NO_ERROR)
        return -1;

    const int outCh = AudioSystem::popCount(mChannels);
    const int factor = VUL_RATE / mRate;
    const int frames = bytes / (2 * outCh);
    int16_t *out = (int16_t *)buffer;
    int done = 0;

    while (done < frames) {
        int want = frames - done;
        if (want * factor > 480)
            want = 480 / factor;
        size_t need = want * factor * 4;
        size_t got = 0;
        int idle = 0;

        while (got < need) {
            ssize_t n = ::read(mFd, (char *)mRaw + got, need - got);
            if (n < 0) {
                LOGE("in read errno %d", errno);
                return -1;
            }
            if (n == 0) {
                /* The driver does not block; one second without data is an error. */
                if (++idle > 200)
                    return -1;
                usleep(5000);
                continue;
            }
            idle = 0;
            got += n;
        }

        for (int i = 0; i < want; i++) {
            int l = 0, r = 0;
            for (int k = 0; k < factor; k++) {
                l += mRaw[2 * (i * factor + k)];
                r += mRaw[2 * (i * factor + k) + 1];
            }
            l /= factor;
            r /= factor;
            *out++ = (int16_t)l;
            if (outCh == 2)
                *out++ = (int16_t)r;
        }
        done += want;
    }

    if (mHw->micMuted())
        memset(buffer, 0, bytes);
    return bytes;
}

status_t EacStreamIn::dump(int fd, const Vector<String16> &args)
{
    (void)fd;
    (void)args;
    return NO_ERROR;
}

status_t EacAudio::dumpState(int fd, const Vector<String16> &args)
{
    (void)fd;
    (void)args;
    return NO_ERROR;
}

status_t EacAudio::dump(int fd, const Vector<String16> &args)
{
    return dumpState(fd, args);
}

extern "C" AudioHardwareInterface *createAudioHardware(void)
{
    return new EacAudio();
}

} // namespace android
