package com.android.javacard.keymaster;

import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.Util;

/**
 * KMCosePairTextStringTag represents a key-value type, where key can be KMInteger or KMNInteger and
 * value is KMTextString type. struct{byte TAG_TYPE; short length; struct{short TXT_STR_VALUE_TYPE;
 * short key; short value}}.
 */
public class KMCosePairTextStringTag extends KMCosePairTagType {

  public static Object[] validKeys;

  private static void createValidKeys() {
    if (validKeys == null) {
      validKeys =
          new Object[] {
            (Object) new byte[] {0, 0, 0, KMCose.ISSUER},
            (Object) new byte[] {0, 0, 0, KMCose.SUBJECT},
            (Object) KMCose.PROFILE_NAME,
          };
    }
  }

  private static KMCosePairTextStringTag prototype;

  private KMCosePairTextStringTag() {}

  private static KMCosePairTextStringTag proto(short ptr) {
    if (prototype == null) {
      prototype = new KMCosePairTextStringTag();
    }
    instanceTable[KM_COSE_KEY_TXT_STR_VAL_OFFSET] = ptr;
    return prototype;
  }

  // pointer to an empty instance used as expression
  public static short exp() {
    short ptr = instance(COSE_PAIR_TAG_TYPE, (short) 6);
    Util.setShort(heap, (short) (ptr + TLV_HEADER_SIZE), KMType.COSE_PAIR_TEXT_STR_TAG_TYPE);
    Util.setShort(heap, (short) (ptr + TLV_HEADER_SIZE + 2), KMType.INVALID_VALUE);
    Util.setShort(heap, (short) (ptr + TLV_HEADER_SIZE + 4), KMTextString.exp());
    return ptr;
  }

  public static short instance(short keyPtr, short valuePtr) {
    if (!isKeyValueValid(keyPtr)) {
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    }
    if (KMType.getType(valuePtr) != TEXT_STRING_TYPE) {
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    }
    short ptr = KMType.instance(COSE_PAIR_TAG_TYPE, (short) 6);
    Util.setShort(heap, (short) (ptr + TLV_HEADER_SIZE), KMType.COSE_PAIR_TEXT_STR_TAG_TYPE);
    Util.setShort(heap, (short) (ptr + TLV_HEADER_SIZE + 2), keyPtr);
    Util.setShort(heap, (short) (ptr + TLV_HEADER_SIZE + 4), valuePtr);
    return ptr;
  }

  public static KMCosePairTextStringTag cast(short ptr) {
    byte[] heap = repository.getHeap();
    if (heap[ptr] != COSE_PAIR_TAG_TYPE) {
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    }
    // Validate the value pointer.
    short valuePtr = Util.getShort(heap, (short) (ptr + TLV_HEADER_SIZE + 4));
    if (KMType.getType(valuePtr) != TEXT_STRING_TYPE) {
      ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    }
    return proto(ptr);
  }

  public static boolean isKeyValueValid(short keyPtr) {
    createValidKeys();
    byte type = KMType.getType(keyPtr);
    short offset = 0;
    switch (type) {
      case KMType.NEG_INTEGER_TYPE:
        offset = KMNInteger.cast(keyPtr).getStartOff();
        break;
      case KMType.INTEGER_TYPE:
        offset = KMInteger.cast(keyPtr).getStartOff();
        break;
      default:
        ISOException.throwIt(ISO7816.SW_CONDITIONS_NOT_SATISFIED);
    }
    short index = 0;
    while (index < (short) validKeys.length) {
      if (0
          == KMInteger.unsignedByteArrayCompare(
              heap,
              offset,
              (byte[]) validKeys[index],
              (short) 0,
              (short) ((byte[]) (validKeys[index])).length)) {
        return true;
      }
      index++;
    }
    return false;
  }

  @Override
  public short getKeyPtr() {
    return Util.getShort(
        heap, (short) (instanceTable[KM_COSE_KEY_TXT_STR_VAL_OFFSET] + TLV_HEADER_SIZE + 2));
  }

  @Override
  public short getValuePtr() {
    return Util.getShort(
        heap, (short) (instanceTable[KM_COSE_KEY_TXT_STR_VAL_OFFSET] + TLV_HEADER_SIZE + 4));
  }
}
