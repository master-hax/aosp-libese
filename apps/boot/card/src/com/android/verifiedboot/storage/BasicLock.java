//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

package com.android.verifiedboot.storage;

import javacard.framework.CardRuntimeException;
import javacard.framework.JCSystem;
import javacard.framework.Util;

import javacard.security.KeyBuilder;
import javacard.security.MessageDigest;
import javacard.security.RSAPublicKey;
import javacard.security.Signature;

import com.android.verifiedboot.storage.LockInterface;
import com.android.verifiedboot.globalstate.owner.OwnerInterface;

class BasicLock implements LockInterface {
    // Layout: LockValue (byte)
    private byte[] storage;
    private short storageOffset;
    private OwnerInterface globalState;
    private boolean bootloaderValue;
    private boolean requireMetadata;
    private short metadataSize;

    /**
     * Initializes the instance.
     *
     * @param inBootloader indicates if bootloader is required for setting.
     * @param setWithMeta if true, requires metadata be set when the lock is set.
     * @param metaLen length of the metadata
     *
     */
    public BasicLock(boolean inBootloader,
                     boolean lockWithMeta,
                     short metaLen) {
        bootloaderValue = inBootloader;
        requireMetadata = lockWithMeta;
        metadataSize = metaLen;
    }

    /**
     * {@inheritDoc}
     *
     * Return the error states useful for diagnostics.
     */
    @Override
    public short initialized() {
        if (storage == null) {
            return 1;
        }
        if (globalState == null) {
            return 2;
        }
        return 0;
    }
    /**
     * {@inheritDoc}
     */
    @Override
    public short getStorageNeeded() {
        return (short)(1 + metadataLength());
    }

    /**
     * Sets the backing store to use for state.
     *
     * @param extStorage  external array to use for storage
     * @param extStorageOffset where to begin storing data
     *
     * This should be called before use.
     */
    @Override
    public void initialize(OwnerInterface globalStateOwner, byte[] extStorage, short extStorageOffset) {
        globalState = globalStateOwner;
        // Zero it first (in case we are interrupted).
        Util.arrayFillNonAtomic(extStorage, extStorageOffset,
                                getStorageNeeded(), (byte) 0x00);
        storage = extStorage;
        storageOffset = extStorageOffset;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public short get(byte[] lockOut, short lockOffset) {
        if (storage == null) {
            return 0x0001;
        }
        try {
            Util.arrayCopy(storage, storageOffset,
                           lockOut, lockOffset, (short) 1);
        } catch (CardRuntimeException e) {
            return 0x0002;
        }
        return 0;
    }

    /**
     * {@inheritDoc}
     *
     * Returns 0xffff if {@link #initialize()} has not yet been called.
     */
    @Override
    public short lockOffset() {
        if (storage == null) {
            return (short) 0xffff;
        }
        return storageOffset;
    }


    /**
     * {@inheritDoc}
     *
     */
    @Override
    public short metadataOffset() {
        if (storage == null) {
            return (short) 0xffff;
        }
        return (short)(lockOffset() + 1);
    }

    /**
     * {@inheritDoc}
     *
     * @return length of metadata.
     */
    public short metadataLength() {
        return metadataSize;
    }

    /**
     * {@inheritDoc}
     *
     * Returns 0x0 on success.
     */
    @Override
    public short set(byte val) {
        if (storage == null) {
            return 0x0001;
        }
        // Do not require meta on unlock.
        if (val != 0) {
            // While an invalid combo, we can just make the require flag
            // pointless if metadataLength == 0.
            if (requireMetadata == true && metadataLength() > 0) {
                return 0x0002;
            }
        }
        if (globalState.production() == true) {
            if (globalState.inBootloader() != bootloaderValue) {
                return 0x0003;
            }
        }
        try {
            storage[storageOffset] = val;
        } catch (CardRuntimeException e) {
            return 0x0004;
        }
        return 0;
    }

   /**
     * {@inheritDoc}
     *
     * Always returns 0xffff as metadata isn't supported.
     */
    @Override
    public short setWithMetadata(byte lockValue, byte[] lockMeta,
                                 short lockMetaOffset, short lockMetaLength) {
        if (storage == null) {
            return 0x0001;
        }
        // No overruns, please.
        if (lockMetaLength > metadataLength()) {
          return 0x0002;
        }
        if (metadataLength() == 0) {
          return set(lockValue);
        }
        try {
            JCSystem.beginTransaction();
            storage[lockOffset()] = lockValue;
            Util.arrayCopy(lockMeta, lockMetaOffset,
                           storage, metadataOffset(),
                           lockMetaLength);
            JCSystem.commitTransaction();
        } catch (CardRuntimeException e) {
            return 0x0003;
        }
        return 0;
    }
}
