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
 * RIL for the MediaTek mtk-ril.so on the Y220-U05 (MT6572).
 *
 * Mobile data needs three things the AOSP framework does not do:
 * - The modem boots with GPRS connect type "when needed" (AT+EGTYPE=0) and
 *   does not attach. The stock MediaTek framework sends
 *   RIL_REQUEST_SET_GPRS_CONNECT_TYPE; ask for "always" when the radio
 *   comes on, like the MediaTekRIL of later CM trees.
 * - +CGREG comes as an MTK URC that AOSP drops.
 * - SETUP_DATA_CALL needs an extra cid argument.
 *
 * mtk-ril sets net.ccmni*.dns1/dns2/gw itself (init lets radio write
 * net.ccmni*), and mobiledata.interfaces in system.prop names ccmni0.
 *
 * {@hide}
 */
public class MediaTekRIL extends RIL implements CommandsInterface {
    /* RIL_REQUEST_MTK_BASE (2000) + 16, from the MT6572 ril.h. */
    static final int RIL_REQUEST_SET_GPRS_CONNECT_TYPE = 2016;
    /*
     * mtk-ril reports +CGREG with this. The stock librilmtk.so sends 3020
     * (seen in the RILC hex dump); later MTK ril.h files put it at 3022.
     */
    static final int RIL_UNSOL_RESPONSE_PS_NETWORK_STATE_CHANGED = 3020;
    /* +ECPI call progress (strings), RIL_UNSOL_MTK_BASE (3000) + 4. */
    static final int RIL_UNSOL_CALL_PROGRESS_INFO = 3004;

    public MediaTekRIL(Context context, int networkMode, int cdmaSubscription) {
        super(context, networkMode, cdmaSubscription);
    }

    @Override
    protected void setRadioState(RadioState newState) {
        boolean wasOn = getRadioState().isOn();

        super.setRadioState(newState);

        if (!wasOn && newState.isOn()) {
            setGprsConnectType(1);
        }
    }

    /*
     * mtk-ril wants an 8th string, the PDP cid (1-3), and fails the request
     * without sending any AT command if it is missing. GB brings up one
     * data connection at a time, so cid 1 is enough.
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

    /*
     * AOSP does not know the MTK PS state URC and would never poll the
     * GPRS state again after the attach. Pass it on as the plain network
     * state change, which makes the service state tracker poll.
     *
     * mtk-ril reports call progress (alerting, connected, released) only
     * as +ECPI and never sends CALL_STATE_CHANGED, so the call tracker
     * keeps the call DIALING and the in-call mute and dialpad buttons stay
     * off. Pass +ECPI on as the call state change, which makes it poll.
     */
    @Override
    protected void processUnsolicited(Parcel p) {
        int dataPosition = p.dataPosition();
        int response = p.readInt();

        if (response == RIL_UNSOL_RESPONSE_PS_NETWORK_STATE_CHANGED) {
            p.setDataPosition(dataPosition);
            p.writeInt(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED);
        } else if (response == RIL_UNSOL_CALL_PROGRESS_INFO) {
            p.setDataPosition(dataPosition);
            p.writeInt(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
        }
        p.setDataPosition(dataPosition);

        super.processUnsolicited(p);
    }

    /* 1 attaches now and stays attached; 0 attaches only when needed. */
    private void setGprsConnectType(int type) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SET_GPRS_CONNECT_TYPE, null);

        rr.mp.writeInt(1);
        rr.mp.writeInt(type);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> SET_GPRS_CONNECT_TYPE " + type);

        send(rr);
    }
}
