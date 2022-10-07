/*
 * Copyright(C) 2022 The Android Open Source Project
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
package com.android.javacard.SecureChannels;

import static com.android.javacard.SecureChannels.FiraConstant.*;
import static com.android.javacard.SecureChannels.ScpConstant.*;

import javacard.framework.JCSystem;
import javacard.framework.Util;
import javacard.security.RandomData;

public class FiraContext {

    // To avoid getter and setter making the members public
    public byte[] mBuf;

    // 'O_*' -> offset of the fields present in 'mBuf'
    public static final short O_SELECTED_OID = 0;
    public static final short O_SELECTED_OID_LEN = (short) (O_SELECTED_OID + MAX_OID_SIZE);
    // Add variable length
    public static final short O_DEVICE_IDENTIFIER = (short) (O_SELECTED_OID_LEN + 1); // TBD
    public static final short O_KEY_PRI_ENC = (short) (O_DEVICE_IDENTIFIER + DEVICE_IDENTIFIER_SIZE);
    public static final short O_KEY_PUB_ENC = (short) (O_KEY_PRI_ENC + EC_SK_KEY_LENGTH); // TBD
    public static final short O_KSES_AUTHENC = (short) (O_KEY_PUB_ENC + EC_PK_KEY_LENGTH);
    public static final short O_EC_KEY_PRIV1 = (short) (O_KSES_AUTHENC + EC_SK_KEY_LENGTH);
    public static final short O_EC_KEY_PUB1 = (short) (O_EC_KEY_PRIV1 + EC_SK_KEY_LENGTH);
    public static final short O_EC_KEY_PRIV2 = (short) (O_EC_KEY_PUB1 + EC_PK_KEY_LENGTH);
    public static final short O_EC_KEY_PUB2 = (short) (O_EC_KEY_PRIV2 + EC_SK_KEY_LENGTH);
    public static final short O_EPHEMERAL_PUBKEY1 = (short) (O_EC_KEY_PUB2 + EC_PK_KEY_LENGTH);
    public static final short O_EPHEMERAL_PUBKEY2 = (short) (O_EPHEMERAL_PUBKEY1 + EC_PK_KEY_LENGTH);
    public static final short O_EPHEMERAL_PRIKEY1 = (short) (O_EPHEMERAL_PUBKEY2 + EC_PK_KEY_LENGTH);
    public static final short O_EPHEMERAL_PRIKEY2 = (short) (O_EPHEMERAL_PRIKEY1 + EC_SK_KEY_LENGTH);
    // RANDOM_DATA0 is 'RandomData2', just following CSML convention
    public static final short O_RANDOM_DATA0 = (short) (O_EPHEMERAL_PRIKEY2 + EC_SK_KEY_LENGTH);
    public static final short O_RANDOM_DATA1 = (short) (O_RANDOM_DATA0 + BlOCK_16BYTES);
    public static final short O_RANDOM_IV = (short) (O_RANDOM_DATA1 + BlOCK_16BYTES);
    public static final short O_RANDOM_IFD = (short) (O_RANDOM_IV + BlOCK_16BYTES);
    public static final short O_KIFD = (short) (O_RANDOM_IFD + BlOCK_16BYTES);
    public static final short O_RANDOM_ICC = (short) (O_KIFD + BlOCK_16BYTES);
    public static final short O_CRYPTOGRAM2 = (short) (O_RANDOM_ICC + BlOCK_16BYTES);
    public static final short O_CHALLENGE1 = (short) (O_CRYPTOGRAM2 + BlOCK_16BYTES);
    public static final short O_CHALLENGE2 = (short) (O_CHALLENGE1 + BlOCK_16BYTES);
    public static final short O_P2 = (short) (O_CHALLENGE2 + BlOCK_16BYTES);
    public static final short O_SELECTION_INDEX = (short) (O_P2 + 1);
    public static final short O_SECURITY_LEVEL = (short) (O_SELECTION_INDEX + 1);
    public static final short O_UWB_SESSIONKEY = (short) (O_SECURITY_LEVEL + 1);
    public static final short O_UWB_SESSIONID = (short) (O_UWB_SESSIONKEY + BlOCK_16BYTES);
    public static final short O_SC1_TAGNUMBER = (short) (O_UWB_SESSIONID + UWB_SESSION_ID_SIZE);
    private static final short O_OCCUPIED = (short) (O_SC1_TAGNUMBER + 2);
    public static final short O_SCP_STATUS = (short) (O_OCCUPIED + 1);
    public static final short O_ROLE = (short) (O_SCP_STATUS + 1);
    public static final short O_STATE = (short) (O_ROLE + 1);
    public static final short O_SC_KVN = (short) (O_STATE + 1);
    public static final short O_PRIV_KVN = (short) (O_SC_KVN + 2);
    public static final short O_AUTH_METHOD = (short) (O_PRIV_KVN + 2);
    public static final short O_BASE_KEYSET_SELECTED_KVN = (short) (O_AUTH_METHOD + 1);
    public static final short O_PRIVACY_KEYSET_SELECTED_KVN = (short) (O_BASE_KEYSET_SELECTED_KVN + 1);
    public static final short O_SC_KEYSET_SELECTED_KVN = (short) (O_PRIVACY_KEYSET_SELECTED_KVN + 1);
    public static final short O_UWB_ROOT_KEYSET_SELECTED_KVN = (short) (O_SC_KEYSET_SELECTED_KVN + 1);

    public FiraContext() {
        mBuf = JCSystem.makeTransientByteArray((short) (O_UWB_ROOT_KEYSET_SELECTED_KVN + 1),
                JCSystem.CLEAR_ON_RESET);
        RandomData.getInstance(RandomData.ALG_FAST).nextBytes(mBuf, O_DEVICE_IDENTIFIER,
                DEVICE_IDENTIFIER_SIZE);

        mBuf[O_SECURITY_LEVEL] = NO_SECURITY_LEVEL;
        mBuf[O_BASE_KEYSET_SELECTED_KVN] = INVALID_VALUE;
        mBuf[O_PRIVACY_KEYSET_SELECTED_KVN] = INVALID_VALUE;
        mBuf[O_SC_KEYSET_SELECTED_KVN] = INVALID_VALUE;
        mBuf[O_UWB_ROOT_KEYSET_SELECTED_KVN] = INVALID_VALUE;

        setRole(FiraConstant.RESPONDER);
        setState(FiraConstant.UNSECURE);
    }

    public boolean isFree() {
        return mBuf[O_OCCUPIED] == 1 ? false : true;
    }

    public void setOccupied(boolean val) {
        mBuf[O_OCCUPIED] = (byte) (val == true ? 1 : 0);
    }

    public void resetContext() {
        Util.arrayFillNonAtomic(mBuf, (short) 0,
                (short) mBuf.length, (byte) 0x00);

        mBuf[O_ROLE] = RESPONDER;
        mBuf[O_STATE] = UNSECURE;
        mBuf[O_BASE_KEYSET_SELECTED_KVN] = INVALID_VALUE;
        mBuf[O_PRIVACY_KEYSET_SELECTED_KVN] = INVALID_VALUE;
        mBuf[O_SC_KEYSET_SELECTED_KVN] = INVALID_VALUE;
        mBuf[O_UWB_ROOT_KEYSET_SELECTED_KVN] = INVALID_VALUE;
    }

    public void setState(byte state) {
        mBuf[O_STATE] = state;
    }

    public byte getState(){
        return mBuf[O_STATE];
    }

    public void setRole(byte role) {
        mBuf[O_ROLE] = role;
    }

    public boolean isInitiator() {
        return mBuf[O_ROLE] == FiraConstant.INITIATOR;
    }

    public void setOrResetContext(boolean val) {
        mBuf[O_OCCUPIED] = (byte) (val == true ? 1 : 0);
    }
}
