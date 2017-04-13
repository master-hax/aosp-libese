package com.android.weaver;

// Keep in sync with native code
public class Consts {
    public static final byte SLOT_ID_BYTES = 4; // 32-bit
    public static final byte SLOT_KEY_BYTES = 16; // 128-bit
    public static final byte SLOT_VALUE_BYTES = 16; // 128-bit

    public static final short SW_WRONG_KEY = 0x6a85;
    public static final short SW_INVALID_SLOT_ID = 0x6a86;
    public static final short SW_BACK_OFF = 0x6a87;

    public static final byte CLA = (byte) 0x80;

    // Instructions
    public static final byte INS_GET_NUM_SLOTS = 0x02;
    public static final byte INS_WRITE = 0x4;
    public static final byte INS_READ = 0x6;
    public static final byte INS_ERASE_ALL = 0x8;
}
