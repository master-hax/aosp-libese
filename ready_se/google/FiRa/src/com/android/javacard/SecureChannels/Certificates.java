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

import static com.android.javacard.SecureChannels.ScpConstant.*;

import com.android.javacard.ber.BerTlvParser;

import javacard.framework.ISOException;
import javacard.framework.JCSystem;
import javacard.framework.Util;

public class Certificates {

    private static short[] sSignatureOffset;
    private static short[] sSignatureDataOffset;
    private static short[] sSignatureLength;
    private static byte[] sCertificateKeyUsageStatus;   // 0x82 = Digital signature verification
                                                        // 0x80 = Key Agreement
                                                        // 0x88 = encipherment for confidentiality
    private static byte[] sPkOceEcka;
    private static short[] sPkOceEckaSize;

    private static Certificates sCertificate;

    private Certificates() {
        sSignatureOffset = JCSystem.makeTransientShortArray((short) 1, JCSystem.CLEAR_ON_RESET);
        sSignatureDataOffset = JCSystem.makeTransientShortArray((short) 1, JCSystem.CLEAR_ON_RESET);
        sSignatureLength = JCSystem.makeTransientShortArray((short) 1, JCSystem.CLEAR_ON_RESET);
        sCertificateKeyUsageStatus = JCSystem.makeTransientByteArray((short) 1,
                JCSystem.CLEAR_ON_RESET);
        sPkOceEcka = JCSystem.makeTransientByteArray((short) 65, JCSystem.CLEAR_ON_RESET);
        sPkOceEckaSize = JCSystem.makeTransientShortArray((short) 1, JCSystem.CLEAR_ON_RESET);
    }

    public static Certificates getInstance() {
        if (sCertificate == null) {
            sCertificate = new Certificates();
        }

        return sCertificate;
    }

    private boolean extractPublicKey(byte[] buffer, short bufferOffset, short bufferLen) {
        short index = 0, lenByteCnt = 0, dataLen = 0, offsetIndex = 0;
        byte status = 0x00;

        while (index < bufferLen) {

            offsetIndex = (short) (bufferOffset + index);
            // Table 6-14: Public Key Data Object
            // only single tag byte
            lenByteCnt = BerTlvParser.getTotalLengthBytesCount(buffer, (short) (offsetIndex + 1));
            dataLen = BerTlvParser.getDataLength(buffer, (short)(offsetIndex + 1));

            switch (buffer[offsetIndex]) {
                case TAG_PUBLIC_KEY_Q:
                    sPkOceEckaSize[0] = dataLen;
                    Util.arrayCopyNonAtomic(buffer, (short) (offsetIndex + lenByteCnt + 1),
                                            sPkOceEcka, (short) 0, sPkOceEckaSize[0]);
                    status |= 0x01;
                    break;
                case TAG_KEY_PARAMETERS_REF:
                    status |= 0x02;
                    break;
                default:
                    return false;
            }
            index += (1 + lenByteCnt + dataLen);
        }

        // M = 2
        if (status != 0x03) {
            return false;
        }

        return true;
    }

    private boolean verifyCSN(byte[] csnBuff, short csnBuffOffset, short csnBuffLength) {
        return true;
    }

    /**
     * verify certificate defined in GPC_2.3_F_SCP11_v1.2.1 (Table 6-12: Certificate Format)
     *
     * @param buffer : buffer which contains certificate
     * @param bufferOffset : buffer offset
     * @param bufferLen : length of buffer/certificate
     * @param checkCSN : set if serial number verification required
     *
     * @return true if certificate verification is successful else false (exception)
     */
    protected boolean verifyCert(byte[] buffer, short bufferOffset, short bufferLen,
            boolean checkCSN) {

        short certStatus = 0;
        short tagByteCnt = 0, lenByteCnt = 0, dataLen = 0;
        short index = 0, tag = 0, offsetIndex = 0;
        short bf20AuthOffset = 0, bf20AuthLength = 0;

        // Note:- No need to add BER parser (To avoid another loop)
        while (index < bufferLen) {
            tagByteCnt = BerTlvParser.getTotalTagBytesCount(buffer, (short) (bufferOffset + index));

            tag = tagByteCnt == 1 ?
                    (short) (buffer[(short) (bufferOffset + index)] & (short) 0xFF) :
                        Util.getShort(buffer, (short) (bufferOffset + index));

            index += tagByteCnt;
            lenByteCnt = BerTlvParser.getTotalLengthBytesCount(buffer,
                    (short) (bufferOffset + index));
            dataLen = BerTlvParser.getDataLength(buffer, (short) (bufferOffset + index));
            index += (lenByteCnt + dataLen);

            // new index offset
            offsetIndex = (short) (bufferOffset + index);

            // GPC_2.3_F_SCP11_v1.2.1
            // Table 6-12: Certificate Format
            switch (tag) {
                case TAG_CSN:
                    // Certificate Serial Number
                    certStatus |= 0x1;
                    // check csn is whitelisted
                    if (checkCSN && !verifyCSN(buffer, (short) (offsetIndex - dataLen) , dataLen)) {
                        ISOException.throwIt(CERT_NOT_IN_WHITELIST);
                    }
                    break;
                case TAG_KLOC_IDENTIFIER:
                    // CA-KLOC (or KA-KLOC) Identifier
                    certStatus |= 0x2;
                    break;
                case TAG_SUBJECT_IDENTIFIER:
                    // Subject Identifier
                    certStatus |= 0x4;
                    break;
                case TAG_KEY_USAGE:
                    //Key Usage:
                    certStatus |= 0x8;
                    if (dataLen == 1)
                        sCertificateKeyUsageStatus[0] = buffer[(short) (offsetIndex - dataLen)];
                    else
                        sCertificateKeyUsageStatus[0] = buffer[(short) (offsetIndex - dataLen + 1)];
                    break;
                case TAG_EFFECTIVE_DATE:
                    // Effective Date (YYYYMMDD, BCD format)
                    certStatus |= 0x10;
                    break;
                case TAG_EXPIRATION_DATE:
                    // Expiration Date
                    certStatus |= 0x20;
                    break;
                case TAG_DISCRETIONARY_DATE:
                case TAG_DISCRETIONARY_DATE2:
                    // Discretionary Data
                    certStatus |= 0x40;
                    break;
                case TAG_SCP11C_AUTHORIZATION:
                    // Authorizations under SCP11c
                    certStatus |= 0x80;
                    bf20AuthOffset = (short) (index - dataLen);
                    bf20AuthLength = dataLen;
                    break;
                case TAG_PUBLIC_KEY:
                    // Public Key – for details, see tables below
                    certStatus |= 0x100;

                    if (!extractPublicKey(buffer, (short) (offsetIndex - dataLen), dataLen)) {
                        return false;
                    }

                    break;
                case TAG_SIGNATURE:
                    // Signature
                    // Signature should be last tag information from the certificate format
                    certStatus |= 0x200;
                    sSignatureOffset[0] = (short) (index - lenByteCnt - dataLen - tagByteCnt);
                    sSignatureDataOffset[0] = (short) (index - dataLen);
                    sSignatureLength[0] = dataLen;
                    break;
                default:
                    // return false if tags other than certificate format tags found
                    return false;
            }
        }

        // Check if all mandatory fields are present
        // M = 7
        if ((short) (certStatus & (short) 0x32f) != (short) 0x32f) {
            return false;
        }

        // tag 'BF20' shall be absent in a signature verification
        // certificate (i.e. value '82' for tag '95').
        if ((sCertificateKeyUsageStatus[0] == 0x82) &&
                (short) (certStatus & (short) 0x80) == (short) 0x80) {
            ISOException.throwIt(BF20_NOT_SUPPORTED);
        }

        return true;
    }

    protected short getSignatureOffset() {
        return sSignatureOffset[0];
    }

    protected short getSignatureDataOffset() {
        return sSignatureDataOffset[0];
    }

    protected short getSignatureLength() {
        return sSignatureLength[0];
    }

    protected byte getKeyUsageStatus() {
        return sCertificateKeyUsageStatus[0];
    }

    protected byte[] getOcePublicKey() {
        return sPkOceEcka;
    }

    protected short getOcePublicKeySize() {
        return sPkOceEckaSize[0];
    }
}
