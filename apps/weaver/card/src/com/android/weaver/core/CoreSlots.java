/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.weaver.core;

import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Util;

import com.android.weaver.Consts;
import com.android.weaver.Slots;

import com.nxp.id.jcopx.util.DSTimer;

class CoreSlots implements Slots {
    static final byte NUM_SLOTS = 64;

    private Slot[] mSlots;

    CoreSlots() {
        // Allocate all memory up front
        mSlots = new Slot[NUM_SLOTS];
        for (short i = 0; i < NUM_SLOTS; ++i) {
            mSlots[i] = new Slot();
        }

        // Make the same size as the value so the whole buffer can be copied in read() so there is
        // no time difference between success and failure.
        Slot.sRemainingBackoff = JCSystem.makeTransientByteArray(
                Consts.SLOT_VALUE_BYTES, JCSystem.CLEAR_ON_RESET);
    }

    @Override
    public short getNumSlots() {
        return NUM_SLOTS;
    }

    @Override
    public void write(short rawSlotId, byte[] key, short keyOffset,
            byte[] value, short valueOffset) {
        final short slotId = validateSlotId(rawSlotId);
        mSlots[slotId].write(key, keyOffset, value, valueOffset);
    }

    @Override
    public byte read(short rawSlotId, byte[] key, short keyOffset,
            byte[] outValue, short outOffset) {
        final short slotId = validateSlotId(rawSlotId);
        return mSlots[slotId].read(key, keyOffset, outValue, outOffset);
    }

    @Override
    public void eraseValue(short rawSlotId) {
        final short slotId = validateSlotId(rawSlotId);
        mSlots[slotId].eraseValue();
    }

    @Override
    public void eraseAll() {
        for (short i = 0; i < NUM_SLOTS; ++i) {
            mSlots[i].erase();
        }
    }

    /**
     * Check the slot ID is within range and convert it to a short.
     */
    private short validateSlotId(short slotId) {
        // slotId is unsigned so if the signed version is negative then it is far too big
        if (slotId < 0 || slotId >= NUM_SLOTS) {
            ISOException.throwIt(Consts.SW_INVALID_SLOT_ID);
        }
        return slotId;
    }

    private static class Slot {
        private static byte[] sRemainingBackoff;

        // Throttle timeouts
        // Use two separate arrays since static initializers must be primitive types or arrays of
        // primitive types.
        private static final short[] THROTTLE_SECONDS_LOW = {
            0,
            0,
            0,
            0,
            0,
            60,
            300,
            900,
            1800,
            5400,
            14580,
            (short) 43740,
            (short) (131220 & 0xffff),
            (short) (393660 & 0xffff),
            (short) (1180980 & 0xffff),
            (short) (3542940 & 0xffff),
            (short) (10628820 & 0xffff),
            (short) (31886460 & 0xffff),
            (short) (95659380 & 0xffff),
            (short) (286978140 & 0xffff),
            (short) (860934420 & 0xffff),
            (short) (2582803260L & 0xffff)
        };

        private static final short[] THROTTLE_SECONDS_HIGH = {
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            (short) (131220 >> 16),
            (short) (393660 >> 16),
            (short) (1180980 >> 16),
            (short) (3542940 >> 16),
            (short) (10628820 >> 16),
            (short) (31886460 >> 16),
            (short) (95659380 >> 16),
            (short) (286978140 >> 16),
            (short) (860934420 >> 16),
            (short) (2582803260L >> 16)
        };

        private byte[] mKey = new byte[Consts.SLOT_KEY_BYTES];
        private byte[] mValue = new byte[Consts.SLOT_VALUE_BYTES];
        private short mFailureCount;
        private DSTimer mBackoffTimer;

        /**
         * Transactionally reset the slot with a new key and value.
         *
         * @param keyBuffer   the buffer containing the key data
         * @param keyOffset   the offset of the key in its buffer
         * @param valueBuffer the buffer containing the value data
         * @param valueOffset the offset of the value in its buffer
         */
        public void write(
                byte[] keyBuffer, short keyOffset, byte[] valueBuffer, short valueOffset) {
            JCSystem.beginTransaction();
            Util.arrayCopy(keyBuffer, keyOffset, mKey, (short) 0, Consts.SLOT_KEY_BYTES);
            Util.arrayCopy(valueBuffer, valueOffset, mValue, (short) 0, Consts.SLOT_VALUE_BYTES);
            mFailureCount = 0;
            mBackoffTimer = DSTimer.getInstance();
            JCSystem.commitTransaction();
        }

        /**
         * Clear the slot's value.
         */
        public void eraseValue() {
            // This is intended to be destructive so a partial update is not a problem
            Util.arrayFillNonAtomic(mValue, (short) 0, Consts.SLOT_VALUE_BYTES, (byte) 0);
        }

        /**
         * Transactionally clear the slot.
         */
        public void erase() {
            JCSystem.beginTransaction();
            arrayFill(mKey, (short) 0, Consts.SLOT_KEY_BYTES, (byte) 0);
            arrayFill(mValue, (short) 0, Consts.SLOT_VALUE_BYTES, (byte) 0);
            mFailureCount = 0;
            mBackoffTimer.stopTimer();
            JCSystem.commitTransaction();
        }

        private boolean hasRemainingBackOff() {
            return ((0 != Util.getShort(sRemainingBackoff, (short) 0)) ||
                (0 != Util.getShort(sRemainingBackoff, (short) 2)));
        }

        /**
         * Copy the slot's value to the buffer if the provided key matches the slot's key.
         *
         * @param keyBuffer the buffer containing the key
         * @param keyOffset the offset of the key in its buffer
         * @param outBuffer the buffer to copy the value or backoff time into
         * @param outOffset the offset into the output buffer
         * @return status code
         */
        public byte read(byte[] keyBuffer, short keyOffset, byte[] outBuffer, short outOffset) {
            // Check timeout has expired or hasn't been started
            mBackoffTimer.getRemainingTime(sRemainingBackoff, (short) 0);
            if (hasRemainingBackOff()) {
                Util.arrayCopyNonAtomic(
                        sRemainingBackoff, (short) 0, outBuffer, outOffset, (byte) 4);
                return Consts.READ_BACK_OFF;
            }

            // Assume this read will fail
            if (mFailureCount != 0x7fff) {
                mFailureCount += 1;
            }
            byte result = Consts.READ_WRONG_KEY;

            // Start the timer on a failure
            if (throttle(sRemainingBackoff, (short) 0, mFailureCount)) {
                mBackoffTimer.startTimer(
                        sRemainingBackoff, (short) 0, DSTimer.DST_POWEROFFMODE_FALLBACK);
                result = Consts.READ_BACK_OFF;
            } else {
                mBackoffTimer.stopTimer();
            }

            // Check the key matches in constant time and copy out the value if it does
            result = (Util.arrayCompare(
                    keyBuffer, keyOffset, mKey, (short) 0, Consts.SLOT_KEY_BYTES) == 0) ?
                    Consts.READ_SUCCESS : result;

            // Keep track of the number of failures
            if (result == Consts.READ_SUCCESS) {
                // This read was successful so reset the failures
                mFailureCount = 0;
                mBackoffTimer.stopTimer();
            }

            final byte[] data = (result == Consts.READ_SUCCESS) ? mValue : sRemainingBackoff;
            Util.arrayCopyNonAtomic(data, (short) 0, outBuffer, outOffset, Consts.SLOT_VALUE_BYTES);

            return result;
        }

        /**
         * 3.0.3 does not offer Util.arrayFill
         */
        private static void arrayFill(byte[] bArray, short bOff, short bLen, byte bValue) {
            for (short i = 0; i < bLen; ++i) {
                bArray[(short) (bOff + i)] = bValue;
            }
        }

        /**
         * Calculates the timeout in seconds as a function of the failure
         * counter 'x' as follows:
         *
         * [0, 5) -> 0
         * 5 -> 60
         * 6 -> 300
         * 7 -> 900
         * 8 -> 1800
         * 9 -> 5400
         * [10, 20] -> 60*3^(x-5)
         * [21, inf) -> 60*3^(21-5)
         *
         * The 32-bit timeout in seconds is written to the array.
         *
         * @return Whether there is any throttle time.
         */
        private boolean throttle(byte[] bArray, short bOff, short failureCount) {
            short highWord = 0;
            short lowWord = 0;

            if (failureCount >= THROTTLE_SECONDS_LOW.length) {
                failureCount = (short) (THROTTLE_SECONDS_LOW.length - 1);
            }

            highWord = THROTTLE_SECONDS_HIGH[failureCount];
            lowWord = THROTTLE_SECONDS_LOW[failureCount];

            // Write the value to the buffer
            Util.setShort(bArray, bOff, highWord);
            Util.setShort(bArray, (short) (bOff + 2), lowWord);

            return highWord != 0 || lowWord != 0;
        }
    }
}
