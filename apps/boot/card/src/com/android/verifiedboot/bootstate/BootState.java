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
// Test with GlobalPlatformPro as:
// Assuming AID is Google's RID and PIXLBOOT0101 for the app (0000 for the pkg)
// ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A0000004765049584C424F4F54000101 -a 80000000
//

package com.android.verifiedboot.bootstate;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Util;

import javacard.security.KeyBuilder;
import javacard.security.MessageDigest;
import javacard.security.RSAPublicKey;
import javacard.security.Signature;

// Enables processing longer messages atomically.
import javacardx.apdu.ExtendedLength;

import com.nxp.id.jcopx.util.SystemInfo;


public class BootState extends Applet implements ExtendedLength {
  final static byte APPLET_CLA = (byte)0x80;

  /* P1=<value>.  This must be removed. */
  private final static byte INS_SET_BOOT_PRIV = (byte) 0x00;

  private final static byte INS_STORE = (byte) 0x01;
  private final static byte INS_LOAD = (byte) 0x02;
  // P1 = { CARRIER_LOCK, DEVICE_LOCK, BOOT_LOCK }, P2 = 0, DATA = { }
  // TODO: Returns 0 for unlocked or 1 as locked or 1|(flags) for locked-with-flags.
  // Returns 1 or 0 for the selected bit or the total lockState value.
  private final static byte INS_GET_LOCK_STATE = (byte) 0x03;
  /* P1 = { CARRIER_LOCK, DEVICE_LOCK, BOOT_LOCK }, P2 = { 1, 0 }, DATA = { NONE | NONCE, SIGNATURE | CARRIER-LOCK-STRINGS (NUL sep) } */
  /* TODO: P2 = byte: bit0=locked, bit1..7=flags.
  /* Returns success or failure. */
  private final static byte INS_SET_LOCK_STATE = (byte) 0x04;


  final static byte[] pkExponent = { (byte) 0x01, (byte) 0x00, (byte) 0x01 };  /* 65537 */
  /* Local test key. */
  final static byte[] pkModulus = {
    (byte) 0x00, (byte) 0xaa, (byte) 0x61, (byte) 0x74, (byte) 0xb8, (byte) 0x9e, (byte) 0xdd, (byte) 0x39, (byte) 0x79, (byte) 0xca, (byte) 0x4d, (byte) 0x79, (byte) 0xd8, (byte) 0x5b, (byte) 0xdd,
    (byte) 0x5f, (byte) 0xab, (byte) 0x1d, (byte) 0xfd, (byte) 0x6c, (byte) 0x50, (byte) 0xc2, (byte) 0xfb, (byte) 0x93, (byte) 0x73, (byte) 0x20, (byte) 0x33, (byte) 0xd9, (byte) 0x2c, (byte) 0xe9,
    (byte) 0x19, (byte) 0x70, (byte) 0x73, (byte) 0xcb, (byte) 0x0f, (byte) 0x51, (byte) 0x52, (byte) 0xad, (byte) 0xa6, (byte) 0x85, (byte) 0xfa, (byte) 0x57, (byte) 0x6f, (byte) 0xd3, (byte) 0x06,
    (byte) 0xe1, (byte) 0x8f, (byte) 0x96, (byte) 0xf6, (byte) 0x9f, (byte) 0xdc, (byte) 0xc8, (byte) 0x21, (byte) 0x4c, (byte) 0xbe, (byte) 0x0a, (byte) 0x59, (byte) 0xca, (byte) 0xd6, (byte) 0x78,
    (byte) 0x8f, (byte) 0x4f, (byte) 0xeb, (byte) 0xd9, (byte) 0xbb, (byte) 0x77, (byte) 0x14, (byte) 0x92, (byte) 0xf3, (byte) 0x6c, (byte) 0x24, (byte) 0xbc, (byte) 0x74, (byte) 0xe5, (byte) 0xfa,
    (byte) 0xc7, (byte) 0x59, (byte) 0x4e, (byte) 0x17, (byte) 0xeb, (byte) 0x0c, (byte) 0xdf, (byte) 0x50, (byte) 0xfe, (byte) 0x85, (byte) 0xed, (byte) 0x8f, (byte) 0xfb, (byte) 0xad, (byte) 0x58,
    (byte) 0x75, (byte) 0xac, (byte) 0xde, (byte) 0x02, (byte) 0xa9, (byte) 0xcd, (byte) 0xb3, (byte) 0x08, (byte) 0xd3, (byte) 0xef, (byte) 0xcf, (byte) 0xf8, (byte) 0xda, (byte) 0x0f, (byte) 0xac,
    (byte) 0xfa, (byte) 0xce, (byte) 0xa7, (byte) 0xaf, (byte) 0x2c, (byte) 0x38, (byte) 0x87, (byte) 0xce, (byte) 0x49, (byte) 0x16, (byte) 0x2b, (byte) 0xfa, (byte) 0x1f, (byte) 0xf8, (byte) 0x9c,
    (byte) 0x9a, (byte) 0x1b, (byte) 0xe4, (byte) 0xa4, (byte) 0x9d, (byte) 0x63, (byte) 0x5b, (byte) 0xbf, (byte) 0x6e, (byte) 0x6c, (byte) 0x7f, (byte) 0x29, (byte) 0x91, (byte) 0x9a, (byte) 0x64,
    (byte) 0xdf, (byte) 0xe3, (byte) 0x69, (byte) 0x8d, (byte) 0xe7, (byte) 0x8e, (byte) 0x2e, (byte) 0xd3, (byte) 0x01, (byte) 0x5d, (byte) 0xef, (byte) 0xf9, (byte) 0x0e, (byte) 0x08, (byte) 0x6d,
    (byte) 0xa9, (byte) 0xf1, (byte) 0x63, (byte) 0x36, (byte) 0xb4, (byte) 0x62, (byte) 0x4b, (byte) 0x7e, (byte) 0x83, (byte) 0xb4, (byte) 0x88, (byte) 0x93, (byte) 0x08, (byte) 0x13, (byte) 0x0c,
    (byte) 0x26, (byte) 0x52, (byte) 0x16, (byte) 0x33, (byte) 0x4a, (byte) 0x4d, (byte) 0xe3, (byte) 0xa5, (byte) 0x4b, (byte) 0xcb, (byte) 0x2b, (byte) 0x63, (byte) 0x81, (byte) 0xf8, (byte) 0x31,
    (byte) 0xf3, (byte) 0xbf, (byte) 0xce, (byte) 0xaa, (byte) 0x93, (byte) 0x01, (byte) 0xd6, (byte) 0x49, (byte) 0x6d, (byte) 0x13, (byte) 0x7b, (byte) 0xfe, (byte) 0x06, (byte) 0xa2, (byte) 0x57,
    (byte) 0x05, (byte) 0xde, (byte) 0x48, (byte) 0x48, (byte) 0x8c, (byte) 0xd6, (byte) 0x2c, (byte) 0x54, (byte) 0x1f, (byte) 0xcf, (byte) 0x24, (byte) 0x97, (byte) 0x31, (byte) 0x51, (byte) 0x9a,
    (byte) 0x7f, (byte) 0x1e, (byte) 0x10, (byte) 0xae, (byte) 0xff, (byte) 0xe3, (byte) 0x6c, (byte) 0xc3, (byte) 0x11, (byte) 0x73, (byte) 0xd8, (byte) 0xb8, (byte) 0x5b, (byte) 0x3f, (byte) 0x98,
    (byte) 0x82, (byte) 0x67, (byte) 0x6d, (byte) 0x6e, (byte) 0x2a, (byte) 0xe8, (byte) 0x08, (byte) 0x46, (byte) 0x64, (byte) 0x06, (byte) 0x84, (byte) 0x3b, (byte) 0xa7, (byte) 0x50, (byte) 0x81,
    (byte) 0x5c, (byte) 0x78, (byte) 0x77, (byte) 0xc1, (byte) 0x35, (byte) 0xd7, (byte) 0x75, (byte) 0x17, (byte) 0xb3, (byte) 0x6a, (byte) 0x79, (byte) 0xa6, (byte) 0x2f, (byte) 0x62, (byte) 0xdb,
    (byte) 0x87, (byte) 0x41
  };
  final static byte kCarrierLockDataLength = (byte) (256 / 8);
  final static byte kCarrierLockNonceLength = (byte) 8;
  // 4096 bytes of storage for use during boot.
  final static short kBootStorageLength = (byte) 4096;

  final static byte LOCK_STATE_PROVISION =  (0x1 << 0);
  final static byte LOCK_STATE_CARRIER_LOCK = (0x1 << 1);
  final static byte LOCK_STATE_DEVICE_LOCK = (0x1 << 2);
  final static byte LOCK_STATE_BOOT_LOCK = (0x1 << 3);
  // TODO(wad) MOve locks to one byte each at the end of boot storage.
  static byte lockState;  // Bitmask of LOCK_STATE_*


  static byte[] bootStorage;

  static byte[] carrierLockNonce;  /* Last 8 byte nonce: little endian 64-bit unsigned integer */
  static byte[] carrierLockData;  /* SHA-256 of device unique data. */
  static MessageDigest md_sha256;  /* For creating the lock data hash from the input. */
  static RSAPublicKey carrierLockKey;
  static Signature verifier;

  // Placeholder for actual signal.
  static byte BOOT_PRIV;

  // XXX: XXX Support LSBackup for key rotation
  public static void resetLockState() {
    lockState = 0;

    Util.arrayFillNonAtomic(carrierLockNonce, (short) 0,
      (short)carrierLockNonce.length, (byte) 0x00);

    Util.arrayFillNonAtomic(carrierLockData, (short)0,
      (short)carrierLockData.length, (byte)0x00);
  }

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    BOOT_PRIV = 1; // XXX: temporary

    bootStorage = new byte[kBootStorageLength];
    Util.arrayFillNonAtomic(bootStorage, (short) 0,
      (short)bootStorage.length, (byte) 0x00);

    carrierLockNonce = new byte[kCarrierLockNonceLength];
    carrierLockData = new byte[kCarrierLockDataLength];
    carrierLockKey = (RSAPublicKey)KeyBuilder.buildKey(
      KeyBuilder.TYPE_RSA_PUBLIC, KeyBuilder.LENGTH_RSA_2048, false);
    /*
    carrierLockKey.setExponent(pkExponent, (short)0, (short)pkExponent.length);
    carrierLockKey.setModulus(pkModulus, (short)0, (short)pkModulus.length);

    verifier = Signature.getInstance(Signature.ALG_RSA_SHA_256_PKCS1, false);
    verifier.init(carrierLockKey, Signature.MODE_VERIFY);
    */

    md_sha256 = MessageDigest.getInstance(MessageDigest.ALG_SHA_256, false);

    resetLockState();

    // GP-compliant JavaCard applet registration
    new BootState().register(bArray, (short) (bOffset + 1),
        bArray[bOffset]);
  }

  public void sendLockState(APDU apdu, byte p1, boolean p2) {
    byte[] buf = apdu.getBuffer();
    if (p2 != false) {
      ISOException.throwIt(ISO7816.SW_SECURITY_STATUS_NOT_SATISFIED);
      return;
    }
    // Send the unfiltered lockState.
    if (p1 == 0) {
      buf[0] = lockState;
      apdu.setOutgoingAndSend((short)0, (short)1);
      return;
    }
    switch (p1) {
    case LOCK_STATE_PROVISION:
       if ((lockState & p1) != LOCK_STATE_PROVISION) {
         // XXX: Should this always return |locked| until provisioned?
       }
      /* fallthrough */
    case LOCK_STATE_CARRIER_LOCK:
      /* fallthrough */
    case LOCK_STATE_DEVICE_LOCK:
      /* fallthrough */
    case LOCK_STATE_BOOT_LOCK:
      buf[0] = (byte)(lockState & p1);
      apdu.setOutgoingAndSend((short)0, (short)1);
      return;
    default:
      ISOException.throwIt(ISO7816.SW_WRONG_P1P2);
    };
    return;
  }

  public void setLockState(APDU apdu, byte lock_bit, boolean enable) {
    byte[] buf = apdu.getBuffer();
    // Move this down so we can configure locks prior to provisioning.
    // Maybe no reading until?
    if (lock_bit != LOCK_STATE_PROVISION &&
        (lockState & LOCK_STATE_PROVISION) == 0) {
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
      return;
    }

    short bytesRead = 0;
    short numBytes = apdu.getIncomingLength();
    short cdataOffset = apdu.getOffsetCdata();
    byte oneZero = 0;
    switch (lock_bit) {
    // setLockState(LOCK_STATE_PROVISION, {0, 1});
    case LOCK_STATE_PROVISION:
      // If we're already provisioned, only continue if BOOT_PRIV overrides.
      if ((lockState & LOCK_STATE_PROVISION) == LOCK_STATE_PROVISION) {
        if (BOOT_PRIV != 1) {
          ISOException.throwIt(ISO7816.SW_SECURITY_STATUS_NOT_SATISFIED);
        }
      }

      // Unprovisioning clears ALL lockState data.
      if (enable == false) {
        resetLockState();
        return;
      }

      lockState |= LOCK_STATE_PROVISION;
      /* Let the caller preconfigure then commit with provision.
       * That will allow the factory to keep it unlocked then flip it to boot locked
       * without the auth blobs.
       */
      return;
    // setLockState(LOCK_STATE_CARRIER_LOCK, 0, [8 byte nonce][RSA-SHA256-PKCS1.5 signature])
    case LOCK_STATE_CARRIER_LOCK:
      // Note, BOOT_PRIV is not relevant for carrier unlock.
      if (enable == true) {
        if ((lockState & LOCK_STATE_PROVISION) == LOCK_STATE_PROVISION) {
          // Carrier lock can only be set while unprovisioned.
          ISOException.throwIt(ISO7816.SW_WARNING_STATE_UNCHANGED);
          return;
        }
        // LOCK_STATE_PROVISION is still not set, so we can proceed and even be
        // called multiple times.
        bytesRead = apdu.setIncomingAndReceive();
        if (numBytes != bytesRead) {
           ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        }
        // SHA256 the data and store the hash.
        md_sha256.reset();
        JCSystem.beginTransaction();
        md_sha256.doFinal(buf, cdataOffset, bytesRead, carrierLockData, (short) 0x0);
        // Note that we never clear the nonce except on unprovisioning.
        lockState |= LOCK_STATE_CARRIER_LOCK;
        JCSystem.commitTransaction();
        return;
      }

      // Only unlock once.
      if ((lockState & LOCK_STATE_CARRIER_LOCK) == 0) {
        // TODO(wad): Would it make sense to allow reusbmitting to advance the nonce?
        return;
      }
      // Collect the signature and nonce from the client.
      bytesRead = apdu.setIncomingAndReceive();
      if ((short)(verifier.getLength() + kCarrierLockNonceLength) != numBytes || numBytes != bytesRead) {
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        return;
      }

      byte[] message = JCSystem.makeTransientByteArray((short)(kCarrierLockNonceLength + kCarrierLockDataLength), JCSystem.CLEAR_ON_DESELECT);
      Util.arrayCopy(buf, cdataOffset, message, (short) 0x0, (short) kCarrierLockNonceLength);
      Util.arrayCopy(carrierLockData, (short) 0, message, (short) kCarrierLockNonceLength, (short) kCarrierLockDataLength);

      if (verifier.verify(message, (short) 0, (short)(kCarrierLockNonceLength + kCarrierLockDataLength),
                          buf, (short)kCarrierLockNonceLength, verifier.getLength()) == false) {
         ISOException.throwIt(ISO7816.SW_SECURITY_STATUS_NOT_SATISFIED);
         return;
       }
       // The signature is valid. Let's make sure the nonce is the larger than the stored nonce.
       // It is a 8 byte, little endian unsigned integer.
       byte i = kCarrierLockNonceLength;
       while (i > 0) {
          if (carrierLockNonce[i] > buf[i]) {
            ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
            return;
          }
          i -= 1;
       }

       JCSystem.beginTransaction();
       // We only ever move from 1 to 0.
       lockState ^= LOCK_STATE_CARRIER_LOCK;
       // Update the monotonically increasing "nonce" value.
       Util.arrayCopy(buf, cdataOffset, carrierLockNonce,
         (short) 0x0, (short) kCarrierLockNonceLength);
       // Delete the device-unique data.
       // TODO(wad): Really would prefer atomic here.
       Util.arrayFillNonAtomic(carrierLockData, (short)0,
         (short)kCarrierLockDataLength, (byte)0x00);
       JCSystem.commitTransaction();
       return;
    case LOCK_STATE_DEVICE_LOCK:
      // This flag can only be flipped by the unprivileged environment.
      if (BOOT_PRIV == 1) {
        ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
        return;
      }
      // enable is only 0 or 1, so we can simple clear the bit, then set.
      if (enable) {
        oneZero = 1;
      }
      lockState = (byte)((lockState & ((byte)0xff ^ LOCK_STATE_DEVICE_LOCK)) |
                   (LOCK_STATE_DEVICE_LOCK * oneZero));
      return;
    case LOCK_STATE_BOOT_LOCK:
      // This flag can only be changed in the bootloader and after carrier
      // and device lock flags are cleared. Beyond this, any additional bootloader
      // flags can be managed inside the bootStorage.
      //
      // TODO(wad): Should we allow transition to lock from outside the bootloader?
      if (BOOT_PRIV == 0) {
        ISOException.throwIt(ISO7816.SW_COMMAND_NOT_ALLOWED);
        return;
      }
      // Allow BOOT_LOCKED with carrier lock, just not unlocked.
      if (enable == false && (lockState & LOCK_STATE_CARRIER_LOCK) == LOCK_STATE_CARRIER_LOCK) {
        ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
        return;
      }
      if (enable == false && (lockState & LOCK_STATE_DEVICE_LOCK) == LOCK_STATE_DEVICE_LOCK) {
        ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
        return;
      }
      // enable is only 0 or 1, so we can simple clear the bit, then set.
      if (enable) {
        oneZero = 1;
      }
      lockState = (byte)((lockState & ((byte)0xff ^ LOCK_STATE_BOOT_LOCK)) |
                   (LOCK_STATE_BOOT_LOCK * oneZero));
      return;
    default:
      ISOException.throwIt(ISO7816.SW_WRONG_P1P2);
    }
    return;
  }

  public void process(APDU apdu) {
    // Return 9000 on SELECT
    if (apdu.isISOInterindustryCLA()) {
      if (this.selectingApplet()) {
        return;
      } else {
        ISOException.throwIt (ISO7816.SW_CLA_NOT_SUPPORTED);
      }
    }

    byte[] buf = apdu.getBuffer();
    short numBytes = apdu.getIncomingLength();
    short cdataOffset = apdu.getOffsetCdata();
    short bytesRead = (short) 0;

    byte p1 = (byte)(buf[ISO7816.OFFSET_P1] & (byte)0xff);
    byte p2 = (byte)(buf[ISO7816.OFFSET_P2] & (byte)0xff);
    short offset = Util.makeShort(p1, p2);
    short length = (short) 0;
    boolean enable = false;
    if (p2 != 0) {
      enable = true;
    }

    switch (buf[ISO7816.OFFSET_INS]) {
    case INS_SET_BOOT_PRIV:
      BOOT_PRIV = (byte)(p1 % 2);
      // Return the new value.
      buf[0] = BOOT_PRIV;
      apdu.setOutgoingAndSend((short)0, (short)1);
      return;
    // getLockState(LOCK_STATE_{}, 0)
    case INS_GET_LOCK_STATE:
      sendLockState(apdu, p1, enable);
      return;
    // setLockState(LOCK_STATE_{}, enable-boolean, {data})
    case INS_SET_LOCK_STATE:
      setLockState(apdu, p1, enable);
      return;
    // setBootStorage(offset_hi, offset_lo, {data})
    case INS_STORE:
      // p1 << 0xff | p2 is the offset to write 
      if (BOOT_PRIV == 0) {
        ISOException.throwIt(ISO7816.SW_SECURITY_STATUS_NOT_SATISFIED);
      }
      if (offset >= kBootStorageLength) {
        ISOException.throwIt(ISO7816.SW_WRONG_P1P2);
        return;
      }
      bytesRead = apdu.setIncomingAndReceive();
      if (numBytes != bytesRead) {
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        return;
      }
      if ((short)(kBootStorageLength - offset) < numBytes) {
        ISOException.throwIt(ISO7816.SW_DATA_INVALID);
      }
      Util.arrayCopy(buf, cdataOffset, bootStorage, offset, numBytes);
      return;
    // getBootStorage(offset) data=short_length
    case INS_LOAD:
      if (offset >= kBootStorageLength) {
        ISOException.throwIt(ISO7816.SW_WRONG_P1P2);
        return;
      }
      bytesRead = apdu.setIncomingAndReceive();
      if (numBytes != bytesRead && numBytes != 2) {
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        return;
      }
      length = Util.makeShort(buf[cdataOffset], buf[(short)(cdataOffset + 1)]);

      if ((short)(kBootStorageLength - offset) < length) {
        ISOException.throwIt(ISO7816.SW_WRONG_P1P2);
        return;
      }
      Util.arrayCopy(bootStorage, offset, buf, (short)0, length);
      apdu.setOutgoingAndSend((short)0, length);
      return;
    default:
      ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
  }
}
