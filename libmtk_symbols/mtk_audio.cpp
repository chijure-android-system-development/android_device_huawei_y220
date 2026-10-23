/*
 * MTK additions to libmedia that the stock audio HAL
 * (libaudio.primary.default.so, Y330-U01 4.2.2) links against.
 *
 * The AudioSystem calls feed MTK's "voice unlock" downlink capture, which
 * CM has no app for: report that nothing is there. IATVCtrlService is the
 * analog TV service; this phone has no ATV tuner.
 */

#include <stddef.h>
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>

namespace android {

class IBinder;

class AudioSystem {
public:
    static int ReadRefFromRing(void *buf, unsigned int size, void *dlTime);
    static int SetVoiceUnlockSRC(unsigned int outSampleRate, unsigned int outChannels);
    static bool startVoiceUnlockDL();
    static bool stopVoiceUnlockDL();
    static int GetVoiceUnlockULTime(void *dlTime);
    static int GetVoiceUnlockDLLatency();
    static void *getVoiceUnlockDLInstance();
    static bool freeVoiceUnlockDLInstance();
};

int AudioSystem::ReadRefFromRing(void *, unsigned int, void *) { return 0; }
int AudioSystem::SetVoiceUnlockSRC(unsigned int, unsigned int) { return 0; }
bool AudioSystem::startVoiceUnlockDL() { return false; }
bool AudioSystem::stopVoiceUnlockDL() { return false; }
int AudioSystem::GetVoiceUnlockULTime(void *) { return 0; }
int AudioSystem::GetVoiceUnlockDLLatency() { return 0; }
void *AudioSystem::getVoiceUnlockDLInstance() { return NULL; }
bool AudioSystem::freeVoiceUnlockDLInstance() { return false; }

class IATVCtrlService : public virtual RefBase {
public:
    static sp<IATVCtrlService> asInterface(const sp<IBinder>& obj);
};

sp<IATVCtrlService> IATVCtrlService::asInterface(const sp<IBinder>&)
{
    return NULL;
}

}; // namespace android
