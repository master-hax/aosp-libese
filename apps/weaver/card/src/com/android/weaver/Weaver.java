package com.android.weaver;

import javacard.framework.AID;
import javacard.framework.APDU;
import javacard.framework.Applet;
import javacard.framework.ISO7816;
import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Shareable;
import javacard.framework.Util;

public class Weaver extends Applet {
    // Keep constants in sync with esed
    public static final byte[] CORE_APPLET_AID
            = new byte[] {(byte)0xa0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x0c, 0x02, 0x01};
    public static final byte CORE_APPLET_SLOTS_INTERFACE = 0;

    private Slots mSlots;

    protected Weaver() {
        register();
    }

    /**
     * Installs this applet.
     *
     * @param params the installation parameters
     * @param offset the starting offset of the parameters
     * @param length the length of the parameters
     */
    public static void install(byte[] params, short offset, byte length) {
        new Weaver();
    }

    /**
     * Get a handle on the slots after the applet is registered but before and APDUs are received.
     */
    @Override
    public boolean select() {
        AID coreAid = JCSystem.lookupAID(CORE_APPLET_AID, (short) 0, (byte) CORE_APPLET_AID.length);
        if (coreAid == null) {
            return false;
        }

        mSlots = (Slots) JCSystem.getAppletShareableInterfaceObject(
                coreAid, CORE_APPLET_SLOTS_INTERFACE);
        if (mSlots == null) {
            return false;
        }

        return true;
    }

    /**
     * Processes an incoming APDU.
     *
     * @param apdu the incoming APDU
     * @exception ISOException with the response bytes per ISO 7816-4
     */
    @Override
    public void process(APDU apdu) {
        final byte buffer[] = apdu.getBuffer();
        final byte cla = buffer[ISO7816.OFFSET_CLA];
        final byte ins = buffer[ISO7816.OFFSET_INS];

        // Handle standard commands
        if (apdu.isISOInterindustryCLA()) {
            if (cla == ISO7816.CLA_ISO7816) {
                switch (ins) {
                    case ISO7816.INS_SELECT:
                        // Do nothing, successfully
                        return;
                    default:
                        ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
                }
            } else {
                ISOException.throwIt(ISO7816.SW_CLA_NOT_SUPPORTED);
            }
        }

        // Handle custom applet commands
        if (cla != Consts.CLA) {
            ISOException.throwIt(ISO7816.SW_CLA_NOT_SUPPORTED);
        }

        switch (ins) {
            case Consts.INS_GET_NUM_SLOTS:
                getNumSlots(apdu);
                return;

            case Consts.INS_WRITE:
                write(apdu);
                return;

            case Consts.INS_READ:
                read(apdu);
                return;

            case Consts.INS_ERASE_ALL:
                eraseAll(apdu);
                return;

            default:
                ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
        }
    }

    /**
     * Get the number of slots.
     *
     * p1: 0
     * p2: 0
     * data: _
     */
    private void getNumSlots(APDU apdu) {
        p1p2Unused(apdu);
        //dataUnused(apdu);
        // TODO(ascull): how to handle the cases of APDU properly?
        prepareToSend(apdu, (short) 4);

        final byte buffer[] = apdu.getBuffer();
        buffer[(short) 0] = 0;
        buffer[(short) 1] = 0;
        Util.setShort(buffer, (short) 2, mSlots.getNumSlots());

        apdu.sendBytes((short) 0, (byte) 4);
    }

    public static final short WRITE_DATA_BYTES
            = Consts.SLOT_ID_BYTES + Consts.SLOT_KEY_BYTES + Consts.SLOT_VALUE_BYTES;
    private static final byte WRITE_DATA_SLOT_ID_OFFSET = ISO7816.OFFSET_CDATA;
    private static final byte WRITE_DATA_KEY_OFFSET
            = WRITE_DATA_SLOT_ID_OFFSET + Consts.SLOT_ID_BYTES;
    private static final byte WRITE_DATA_VALUE_OFFSET
            = WRITE_DATA_KEY_OFFSET + Consts.SLOT_KEY_BYTES;

    /**
     * Write to a slot.
     *
     * p1: 0
     * p2: 0
     * data: [slot ID] [key data] [value data]
     */
    private void write(APDU apdu) {
        p1p2Unused(apdu);
        receiveData(apdu, WRITE_DATA_BYTES);

        final byte buffer[] = apdu.getBuffer();
        final short slotId = getSlotId(buffer, WRITE_DATA_SLOT_ID_OFFSET);
        mSlots.write(slotId, buffer, WRITE_DATA_KEY_OFFSET, buffer, WRITE_DATA_VALUE_OFFSET);
    }

    public static final short READ_DATA_BYTES
            = Consts.SLOT_ID_BYTES + Consts.SLOT_KEY_BYTES;
    private static final byte READ_DATA_SLOT_ID_OFFSET = ISO7816.OFFSET_CDATA;
    private static final byte READ_DATA_KEY_OFFSET
            = WRITE_DATA_SLOT_ID_OFFSET + Consts.SLOT_ID_BYTES;

    /**
     * Read a slot.
     *
     * p1: 0
     * p2: 0
     * data: [slot ID] [key data]
     */
    private void read(APDU apdu) {
        p1p2Unused(apdu);
        receiveData(apdu, READ_DATA_BYTES);
        prepareToSend(apdu, Consts.SLOT_VALUE_BYTES);

        final byte buffer[] = apdu.getBuffer();
        final short slotId = getSlotId(buffer, READ_DATA_SLOT_ID_OFFSET);

        final short err = mSlots.read(slotId, buffer, READ_DATA_KEY_OFFSET, buffer, (short) 0);
        if (err != ISO7816.SW_NO_ERROR) {
            if (err == Consts.SW_BACK_OFF || err == Consts.SW_WRONG_KEY) {
                // TODO: standard is unclear as to whether this works, but it works in cref
                apdu.sendBytes((short) 0, (byte) 4);
            }
            ISOException.throwIt(err);
        }
        apdu.sendBytes((short) 0, Consts.SLOT_VALUE_BYTES);
    }

    /**
     * Erase all slots.
     *
     * p1: 0
     * p2: 0
     * data: _
     */
    private void eraseAll(APDU apdu) {
        p1p2Unused(apdu);
        dataUnused(apdu);
        mSlots.eraseAll();
    }

    /**
     * Check that the parameters are 0.
     *
     * They are not being used but should be under control.
     */
    private void p1p2Unused(APDU apdu) {
        final byte buffer[] = apdu.getBuffer();
        if (buffer[ISO7816.OFFSET_P1] != 0 || buffer[ISO7816.OFFSET_P2] != 0) {
            ISOException.throwIt(ISO7816.SW_INCORRECT_P1P2);
        }
    }

    /**
     * Check that no data was provided.
     */
    private void dataUnused(APDU apdu) {
        if (apdu.getIncomingLength() != 0) {
            ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        }
    }

    /**
     * Calls setIncomingAndReceive() on the APDU and checks the length is as expected.
     */
    private void receiveData(APDU apdu, short expectedLength) {
        if (apdu.getIncomingLength() != expectedLength) {
            ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        }

        final short bytesRead = apdu.setIncomingAndReceive();
        if (bytesRead != expectedLength) {
            ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        }
    }

    /**
     * The slot ID in the API is 32-bits but the applet works with 16-bit IDs.
     */
    private short getSlotId(byte[] bArray, short bOff) {
        if (bArray[bOff] != 0 || bArray[(short) (bOff + 1)] != 0) {
            ISOException.throwIt(Consts.SW_INVALID_SLOT_ID);
        }
        return Util.getShort(bArray,(short) (bOff + 2));
    }

    /**
     * Calls setOutgoing() on the APDU, checks the length is as expected and calls
     * setOutgoingLength() with that length.
     */
    private void prepareToSend(APDU apdu, short expectedLength) {
        final short outDataLen = apdu.setOutgoing();
        if (outDataLen != expectedLength) {
            ISOException.throwIt(ISO7816.SW_WRONG_LENGTH);
        }
        apdu.setOutgoingLength(expectedLength);
    }
}
