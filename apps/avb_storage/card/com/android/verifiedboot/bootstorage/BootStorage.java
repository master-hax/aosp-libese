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
// Assuming AID is Google's RID and PIXELBOOT0100 for the app (0000 for the pkg)
//${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80000000

package com.android.verifiedboot.bootstorage;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Util;

import javacard.security.MessageDigest;


public class BootStorage extends Applet {
  final static byte APPLET_CLA = (byte)0x80;

/*j
  private final static byte INS_LOAD = (byte) 0x01;
  private final static byte INS_EXTEND = (byte) 0x02;
  private final static byte INS_STORE = (byte) 0x03;

  private final static byte INS_FLIP_PRIVS = (byte) 0x00;
  */


  /* Need to allocate:
     Slot 0, 32 bytes: extend anytime/store w/BOOT_PRIV
     Slot [1-8], 8 bytes each: load/store(BOOT_PRIV required)
     Slot [9-10] 64 bytes: load store[32]= {user bytes, current Slot 0 (or BOOT_PRIV)}
   */
  /* SHA256 extensible register. May be EXTENDed at any time, but only STOREd with BOOT_PRIV is asserted. */
  static byte[] configuration_register; /* SHA256 {state, image hashes} */
  /* 8 byte blob of storage may be LOADed at any time, but only STOREd when BOOT_PRIV is asserted. */
  static byte[] storage_register;  /* 64 bits * 8 slots {AVB stored rollback index} */
  /* 32 bytes of storage bound to a configuration register. May be LOADed at any time
   * and may STOREd when BOOT_PRIV is asserted _or_ the upper 32 bytes == |configuraton_register|[0]
   */
  static byte[] bound_register;  /* SHA256 bound to SHA256 * 2 slots  + 1 byte of flags*/

  // For configuration_register extension.
  static MessageDigest md_sha256;

  // Placeholder for actual signal.
  static byte BOOT_PRIV;

  public static void install(byte[] bArray, short bOffset, byte bLength) {
    BOOT_PRIV = 1;
    md_sha256 = MessageDigest.getInstance(MessageDigest.ALG_SHA_256, false);
    configuration_register = new byte[32 * 2]; /* SHA256 {state, image hashes} */
    storage_register = new byte[8 * 8];  /* 64 bits * 8 slots {AVB stored rollback index} */
    bound_register = new byte[(64 * 2) + 2];  /* SHA256 bound to SHA256 * 2 slots */
    // GP-compliant JavaCard applet registration
    new BootStorage().register(bArray, (short) (bOffset + 1),
        bArray[bOffset]);
  }


  public void process(APDU apdu) {
    // Return 9000 on SELECT
    if (selectingApplet()) {
      // On every select, check if BOOT_PRIV is asserted. If it is asserted, ensure that the configuration
      // registers are set to their starting value.  They may be extending in the same select session as
      // BOOT_PRIV is set, but they will not be guaranteed to keep their state until BOOT_PRIV is deasserted.
      // TODO(wad): Determine if this will be too much flash use over device lifetime.
      if (BOOT_PRIV == 1) {
        byte[] tempBuffer = JCSystem.makeTransientByteArray((short)32, JCSystem.CLEAR_ON_DESELECT);
        Util.arrayFillNonAtomic(tempBuffer, (short)0, (short)32, (byte)0x00);
        if (0 != Util.arrayCompare(configuration_register, (short)0, tempBuffer, (short)0, (short)32) ||
            0 != Util.arrayCompare(configuration_register, (short)32, tempBuffer, (short)0, (short)32)) {
          Util.arrayCopy(configuration_register, (short)0, tempBuffer, (short)0, (short)32);
          Util.arrayCopy(configuration_register, (short)32, tempBuffer, (short)0, (short)32);
        }
       }
      return;
    }

    byte[] buf = apdu.getBuffer();
    byte numBytes = (byte)(buf[ISO7816.OFFSET_LC]);
    byte bytesRead;

    //if (buf[ISO7816.OFFSET_CLA] != APPLET_CLA) {
    //  ISOException.throwIt(ISO7816.SW_CLA_NOT_SUPPORTED);
    //}

    if ((buf[ISO7816.OFFSET_INS] & 0xff) == (byte)(0x10)) {
      // Proof of life.
      return;
    }
    if ((buf[ISO7816.OFFSET_INS] & 0xff) == (byte)(0x11)) {
      // Proof of life.
      return;
    }

    byte p1 = (byte)(buf[ISO7816.OFFSET_P1] & (byte)0xff);

    switch (buf[ISO7816.OFFSET_INS]) {
    case (byte)0x00:
      BOOT_PRIV = (byte)((BOOT_PRIV + 1) % 2);
      // Return the value so we can test better.
      buf[0] = BOOT_PRIV;
      apdu.setOutgoingAndSend((short)0, (short)1);
      return;
    case (byte)0x01: // LOAD
      switch (p1) {
      /* Hash extended. */
      case 0x00: case 0x01:
        // Ignore the expected length for now.
        if (apdu.setOutgoing() != 32) {
          // TODO: throw something!
        }
        apdu.setOutgoingLength((byte)32);
        Util.arrayCopy(configuration_register, (short)(p1 * (byte)32), buf, (short)0, (short) 32);
        apdu.sendBytes((short)0, (byte)32);
        return;
      case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
      case 0x08: case 0x09:
        apdu.setOutgoing();
        apdu.setOutgoingLength((byte)8);
        Util.arrayCopy(storage_register, (short)((p1 - (byte)0x02) * (byte)8), buf, (short)0, (short)8);
        apdu.sendBytes((short)0, (byte)8);
        return;
      case 0x0a: case 0x0b:
        // Load the stored 32 bytes only.
        // TODO(wad) Should we disclose the binding?
        apdu.setOutgoing();
        apdu.setOutgoingLength((byte)32);
        Util.arrayCopy(bound_register, (short)((p1 - (byte)0x0a) * (byte)64), buf, (short)0, (short) 32);
        apdu.sendBytes((short)0, (byte)32);
        return;
      default:
        ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
      }
    case (byte)0x02: // EXTEND
      if (p1 != 0x0 &&
          p1 != 0x1) {
        ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
      }
      // Only supports 32 bytes of data to extend with.
      bytesRead = (byte)(apdu.setIncomingAndReceive());
      if ((byte)32 != numBytes || numBytes != bytesRead)
        ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
      // Compute the extension: SHA256(CR || data)
      md_sha256.reset();
      md_sha256.update(configuration_register, (short)(p1 * (byte)32), (short)32);
      md_sha256.doFinal(buf, (short)ISO7816.OFFSET_CDATA, (short)32, configuration_register, (short)(p1 * (byte)32));
      // Return the new value.
      apdu.setOutgoing();
      apdu.setOutgoingLength((byte)32);
      Util.arrayCopy(configuration_register, (short)(p1 * (byte)32), buf, (short)0, (short)32);
      apdu.sendBytes((short)0, (byte)32);
      return;
    case (byte)0x03: // STORE
      switch (p1) {
      /* Hash extended. */
      case 0x00: case 0x01:
        /* CRs are reset when BOOT_PRIV is set and the applet is selected. */
        ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
      case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
      case 0x08: case 0x09:
        if (BOOT_PRIV != 1)
          ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
        bytesRead = (byte)(apdu.setIncomingAndReceive());
        if ((byte)8 != numBytes || numBytes != bytesRead)
          ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        Util.arrayCopy(buf, (short)ISO7816.OFFSET_CDATA, storage_register, (short)((p1 - (byte)0x02) * (byte)8), (short)8);
        return;
      case 0x0a: case 0x0b:  /* With BOOT_PRIV, the setting can be written incl. bound value. */
        // P2 indicates desired binding.
        if (buf[ISO7816.OFFSET_P2] != 0x0 &&
            buf[ISO7816.OFFSET_P2] != 0x1) {
          ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
        }
        // Check if the bound value matches the current configuration register or for a BOOT_PRIV override.
        // E.g., on lock/unlock, all bound slots should be cleared with above slots.
        if (BOOT_PRIV != 1 &&
            0 != Util.arrayCompare(bound_register, (short)((byte)32 + ((byte)(p1 - (byte)0x0a) * (byte)65)),
                                   configuration_register, (short)(bound_register[(byte)((p1 - (byte)0x0a) * (byte)65) + (byte)64] * (byte)32), (short)32)) {
          ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
        }
        bytesRead = (byte)(apdu.setIncomingAndReceive());
        if ((byte)64 != numBytes || numBytes != bytesRead)
          ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);

        JCSystem.beginTransaction();
        Util.arrayCopy(buf, (short)ISO7816.OFFSET_CDATA, bound_register, (short)((p1 - (byte)0x0a) * (byte)65), (short)64);
        bound_register[(byte)(((byte)p1 - (byte)0x0a) * (byte)65 + (byte)64)] = (byte)(buf[ISO7816.OFFSET_P2] & (byte)0xff);
        JCSystem.commitTransaction();
        return;
      default:
        ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
      }
      break;
    default:
      ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
    }
  }
}
