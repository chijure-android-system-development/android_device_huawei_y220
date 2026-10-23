/*
 * Copyright (C) 2026 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.internal.telephony;

import static com.android.internal.telephony.RILConstants.*;

import android.content.Context;
import android.os.Message;
import android.os.Parcel;

/**
 * RIL for the MediaTek mtk-ril.so of the Y330-U01 4.2.2 (MT6572), which
 * the Y220-U05 runs on CM10.1.
 *
 * What the AOSP framework does not do with this RIL:
 * - Data: the modem boots with GPRS connect type "when needed" and does
 *   not attach; ask for "always" when the radio comes on. SETUP_DATA_CALL
 *   needs an 8th string, the PDP cid.
 * - Incoming calls: mtk-ril announces each one with
 *   UNSOL_INCOMING_CALL_INDICATION and holds it until the framework allows
 *   it with SET_CALL_INDICATION. Without that the phone never rings.
 * - Call progress (+ECPI), the PS registration state and SIM hot plug come
 *   as MTK URCs; pass them on as the AOSP state changes that make the
 *   trackers poll.
 *
 * The MTK request and URC numbers are the ones in the Y330 librilmtk.so
 * (its URC table runs 3000..3029). The GB mtk-ril of the Y220 numbered
 * the PS state URC 3020; this one uses 3022.
 *
 * {@hide}
 */
public class MediaTekRIL extends RIL implements CommandsInterface {
    static final int RIL_REQUEST_MTK_BASE = 2000;
    static final int RIL_REQUEST_SET_GPRS_CONNECT_TYPE = RIL_REQUEST_MTK_BASE + 16;
    static final int RIL_REQUEST_SET_GPRS_TRANSFER_TYPE = RIL_REQUEST_MTK_BASE + 17;
    static final int RIL_REQUEST_SET_CALL_INDICATION = RIL_REQUEST_MTK_BASE + 36;

    static final int RIL_UNSOL_MTK_BASE = 3000;
    static final int RIL_UNSOL_CALL_PROGRESS_INFO = RIL_UNSOL_MTK_BASE + 4;
    static final int RIL_UNSOL_SIM_INSERTED_STATUS = RIL_UNSOL_MTK_BASE + 7;
    static final int RIL_UNSOL_INCOMING_CALL_INDICATION = RIL_UNSOL_MTK_BASE + 14;
    static final int RIL_UNSOL_SIM_MISSING = RIL_UNSOL_MTK_BASE + 15;
    static final int RIL_UNSOL_RESPONSE_PS_NETWORK_STATE_CHANGED = RIL_UNSOL_MTK_BASE + 22;
    static final int RIL_UNSOL_SIM_PLUG_OUT = RIL_UNSOL_MTK_BASE + 27;
    static final int RIL_UNSOL_SIM_PLUG_IN = RIL_UNSOL_MTK_BASE + 28;

    public MediaTekRIL(Context context, int networkMode, int cdmaSubscription) {
        super(context, networkMode, cdmaSubscription);
    }

    @Override
    protected void setRadioState(RadioState newState) {
        boolean wasOn = getRadioState().isOn();

        super.setRadioState(newState);

        if (!wasOn && newState.isOn()) {
            setGprsType(RIL_REQUEST_SET_GPRS_TRANSFER_TYPE, 1);
            setGprsType(RIL_REQUEST_SET_GPRS_CONNECT_TYPE, 1);
        }
    }

    /*
     * mtk-ril fails the request without sending any AT command if the cid
     * is missing. One data connection at a time, so cid 1.
     */
    @Override
    public void setupDataCall(String radioTechnology, String profile, String apn,
            String user, String password, String authType, String protocol,
            Message result) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SETUP_DATA_CALL, result);

        rr.mp.writeInt(8);

        rr.mp.writeString(radioTechnology);
        rr.mp.writeString(profile);
        rr.mp.writeString(apn);
        rr.mp.writeString(user);
        rr.mp.writeString(password);
        rr.mp.writeString(authType);
        rr.mp.writeString(protocol);
        rr.mp.writeString("1");

        if (RILJ_LOGD) riljLog(rr.serialString() + "> "
                + requestToString(rr.mRequest) + " " + radioTechnology + " "
                + profile + " " + apn + " " + user + " "
                + password + " " + authType + " " + protocol + " cid 1");

        send(rr);
    }

    @Override
    protected void processUnsolicited(Parcel p) {
        int dataPosition = p.dataPosition();
        int response = p.readInt();
        int replacement;

        switch (response) {
            case RIL_UNSOL_CALL_PROGRESS_INFO:
                replacement = RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED;
                break;
            case RIL_UNSOL_INCOMING_CALL_INDICATION:
                allowIncomingCall(p.readStringArray());
                replacement = RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED;
                break;
            case RIL_UNSOL_RESPONSE_PS_NETWORK_STATE_CHANGED:
                replacement = RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED;
                break;
            case RIL_UNSOL_SIM_INSERTED_STATUS:
            case RIL_UNSOL_SIM_MISSING:
            case RIL_UNSOL_SIM_PLUG_OUT:
            case RIL_UNSOL_SIM_PLUG_IN:
                replacement = RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED;
                break;
            default:
                if (response >= RIL_UNSOL_MTK_BASE) {
                    /* The rest of the MTK URCs have no AOSP counterpart. */
                    if (RILJ_LOGD) riljLog("[UNSL]< MTK " + response + " ignored");
                    return;
                }
                p.setDataPosition(dataPosition);
                super.processUnsolicited(p);
                return;
        }

        /* Hand the AOSP code on; its handlers read no payload. */
        p.setDataPosition(dataPosition);
        p.writeInt(replacement);
        p.setDataPosition(dataPosition);
        super.processUnsolicited(p);
    }

    /* +EAIC: call id, number, type, call mode, sequence number. */
    private void allowIncomingCall(String[] info) {
        if (info == null || info.length < 5) {
            riljLog("INCOMING_CALL_INDICATION without call id and sequence");
            return;
        }

        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SET_CALL_INDICATION, null);
        int callId = Integer.parseInt(info[0]);
        int seqNumber = Integer.parseInt(info[4]);

        /* Mode 0 allows the call. */
        rr.mp.writeInt(3);
        rr.mp.writeInt(0);
        rr.mp.writeInt(callId);
        rr.mp.writeInt(seqNumber);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> SET_CALL_INDICATION allow "
                + callId + " " + seqNumber);

        send(rr);
    }

    /* Connect type 1 attaches now and stays attached; transfer type 1
     * prefers data over calls. */
    private void setGprsType(int request, int type) {
        RILRequest rr = RILRequest.obtain(request, null);

        rr.mp.writeInt(1);
        rr.mp.writeInt(type);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> MTK " + request + " " + type);

        send(rr);
    }
}
