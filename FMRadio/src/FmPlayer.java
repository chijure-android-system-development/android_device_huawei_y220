package com.mediatek.FMRadio;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.media.AudioSystem;
import android.media.MediaPlayer;
import android.util.Log;

import java.io.IOException;

/*
 * CM7 has no MediaTek FM player type. The chip feeds the AFE directly, so
 * the FM "player" only has to switch the digital FM path in libaudio.
 *
 * Volume and speaker go to libaudio too: GB has no STREAM_FM (the activities
 * use STREAM_MUSIC instead) and rejects the MediaTek FOR_PROPRIETARY force use.
 */
public class FmPlayer extends MediaPlayer {
    private static final String TAG = "FmPlayer";
    private static final String FM_SOURCE = "MEDIATEK://MEDIAPLAYER_PLAYERTYPE_FM";
    /* MediaTek AudioSystem.FOR_PROPRIETARY, used by FMRadioService for the speaker. */
    private static final int FOR_PROPRIETARY = 4;

    private static Context sContext;
    private static boolean sReceiverRegistered;
    private boolean mPlaying;

    private static final BroadcastReceiver sVolumeReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            int stream = intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_TYPE, -1);
            if (stream == AudioManager.STREAM_MUSIC)
                sendVolume(intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_VALUE, 0));
        }
    };

    private static void sendVolume(int index) {
        int max = 15;
        if (sContext != null) {
            AudioManager am = (AudioManager) sContext.getSystemService(Context.AUDIO_SERVICE);
            max = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        }
        Log.d(TAG, "AudioSetFmVolume=" + index + " (max " + max + ")");
        AudioSystem.setParameters("AudioSetFmVolume=" + index + ";AudioSetFmVolumeMax=" + max);
    }

    /* Replaces AudioSystem.setForceUse(FOR_PROPRIETARY, FORCE_SPEAKER/NONE). */
    public static int setForceUse(int usage, int config) {
        if (usage != FOR_PROPRIETARY)
            return AudioSystem.setForceUse(usage, config);
        Log.d(TAG, "AudioSetFmSpeaker=" + (config == AudioSystem.FORCE_SPEAKER ? 1 : 0));
        AudioSystem.setParameters("AudioSetFmSpeaker=" +
                (config == AudioSystem.FORCE_SPEAKER ? 1 : 0));
        return 0;
    }

    private void setFm(boolean on) {
        if (mPlaying == on)
            return;
        mPlaying = on;
        if (on && sContext != null) {
            AudioManager am = (AudioManager) sContext.getSystemService(Context.AUDIO_SERVICE);
            sendVolume(am.getStreamVolume(AudioManager.STREAM_MUSIC));
        }
        Log.d(TAG, "AudioSetFmDigitalEnable=" + (on ? 1 : 0));
        AudioSystem.setParameters("AudioSetFmDigitalEnable=" + (on ? 1 : 0));
    }

    @Override
    public void setWakeMode(Context context, int mode) {
        super.setWakeMode(context, mode);
        if (sContext == null && context != null)
            sContext = context.getApplicationContext();
        if (sContext != null && !sReceiverRegistered) {
            sContext.registerReceiver(sVolumeReceiver,
                    new IntentFilter(AudioManager.VOLUME_CHANGED_ACTION));
            sReceiverRegistered = true;
        }
    }

    @Override
    public void setDataSource(String path) throws IOException {
        if (!FM_SOURCE.equals(path))
            super.setDataSource(path);
    }

    @Override
    public void prepare() {
    }

    @Override
    public void setAudioStreamType(int streamtype) {
    }

    @Override
    public void start() {
        setFm(true);
    }

    @Override
    public void stop() {
        setFm(false);
    }

    @Override
    public void pause() {
        setFm(false);
    }

    @Override
    public boolean isPlaying() {
        return mPlaying;
    }

    @Override
    public void reset() {
        setFm(false);
        super.reset();
    }

    @Override
    public void release() {
        setFm(false);
        super.release();
    }
}
