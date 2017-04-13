package com.android.weaver;

public interface Slots extends javacard.framework.Shareable {
    /** @return The number of slots available. */
    short getNumSlots();

    /**
     * Write the key and value to the identified slot.
     *
     * @param slotId ID of the slot to write to.
     * @param key Buffer containing the key.
     * @param keyOffset Offset of the key in the buffer.
     * @param value Buffer containing the value.
     * @param valueOffset Offset of the value in the buffer.
     */
    void write(short slotId, byte[] key, short keyOffset, byte[] value, short valueOffset);

    /**
     * Read the value from the identified slot.
     *
     * This is only successful if the key matches that stored in the slot.
     *
     * @param slotId ID of the slot to write to.
     * @param key Buffer containing the key.
     * @param keyOffset Offset of the key in the buffer.
     * @param value Buffer to receive the value.
     * @param valueOffset Offset into the buffer to write the value.
     * @return Status word indicating the success or otherwise of the read.
     */
    short read(short slotId, byte[] key, short keyOffset, byte[] value, short valueOffset);

    /** Erases the contents of all slots. */
    void eraseAll();
}
