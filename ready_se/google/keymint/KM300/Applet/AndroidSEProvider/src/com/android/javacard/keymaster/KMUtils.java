/*
 * Copyright(C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" (short)0IS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.javacard.keymaster;

import com.android.javacard.seprovider.KMException;
import javacard.framework.Util;

/**
 * This is a utility class which helps in converting date to UTC format and doing some arithmetic
 * Operations.
 */
public class KMUtils {

  // 64 bit unsigned calculations for time
  public static final byte[] oneSecMsec = {0, 0, 0, 0, 0, 0, 0x03, (byte) 0xE8}; // 1000 msec
  public static final byte[] oneMinMsec = {0, 0, 0, 0, 0, 0, (byte) 0xEA, 0x60}; // 60000 msec
  public static final byte[] oneHourMsec = {
    0, 0, 0, 0, 0, 0x36, (byte) 0xEE, (byte) 0x80
  }; // 3600000 msec
  public static final byte[] oneDayMsec = {0, 0, 0, 0, 0x05, 0x26, 0x5C, 0x00}; // 86400000 msec
  public static final byte[] leapYearMsec = {
    0, 0, 0, 0x07, (byte) 0x5C, (byte) 0xD7, (byte) 0x88, 0x00
  }; // 31622400000;
  public static final byte[] yearMsec = {
    0, 0, 0, 0x07, 0x57, (byte) 0xB1, 0x2C, 0x00
  }; // 31536000000
  // Leap year(366) + 3 * 365
  public static final byte[] fourYrsMsec = {
    0, 0, 0, 0x1D, 0x63, (byte) 0xEB, 0x0C, 0x00
  }; // 126230400000
  public static final byte[] hundredYrsMSec = {
    0x00, 0x00, 0x02, (byte) 0xDE, (byte) 0xC1, (byte) 0xF4, (byte) 0x2C, 0x00
  }; // 3155760000000
  public static final byte[] OneDayLessHundredYrsMSec = {
    0x00, 0x00, 0x02, (byte) 0xDE, (byte) 0xBC, (byte) 0xCD, (byte) 0xD0, 0x00
  }; // 3155673600000
  public static final byte[] firstJan8000MSec = {
    0x00, 0x00, (byte) 0xAD, 0x10, (byte) 0xF8, 0x4B, (byte) 0xD0, 0x00
  }; // 190288396800000
  public static final byte[] firstJan6000MSec = {
    0x00, 0x00, 0x73, (byte) 0xAA, 0x1E, 0x77, (byte) 0xC4, 0x00
  }; // 127174492800000
  public static final byte[] firstJan4000MSec = {
    0x00, 0x00, 0x3A, 0x43, 0x44, (byte) 0xA3, (byte) 0xB8, 0x00
  }; // 64060588800000
  public static final byte[] firstJan2000MSec = {
    0, 0, 0, (byte) 0xDC, 0x6A, (byte) 0xCF, (byte) 0xAC, 0x00
  }; // 946684800000 msec
  public static final byte[] firstJan1970MSec = {0, 0, 0, 0, 0, 0, 0, 0};
  // Constant for Dec 31, 9999 in milliseconds in hex.
  private static final byte[] dec319999Ms = {
    (byte) 0, (byte) 0, (byte) 0xE6, 0x77, (byte) 0xD2, 0x1F, (byte) 0xD8, 0x18
  }; // 253402300799000
  // msec
  public static final byte[] febMonthLeapMSec = {
    0, 0, 0, 0, (byte) 0x95, 0x58, 0x6C, 0x00
  }; // 2505600000
  public static final byte[] febMonthMsec = {
    0, 0, 0, 0, (byte) 0x90, 0x32, 0x10, 0x00
  }; // 2419200000
  public static final byte[] ThirtyOneDaysMonthMsec = {
    0, 0, 0, 0, (byte) 0x9F, (byte) 0xA5, 0x24, 0x00
  }; // 2678400000
  public static final byte[] ThirtDaysMonthMsec = {
    0, 0, 0, 0, (byte) 0x9A, 0x7E, (byte) 0xC8, 0x00
  }; // 2592000000
  public static final short year2050 = 2050;
  public static final short year1970 = 1970;
  public static final short year2000 = 2000;
  public static final short year4000 = 4000;
  public static final short year6000 = 6000;
  public static final short year8000 = 8000;
  public static final short year10000 = 10000;
  public static final byte UINT8 = 8;
  // Convert to milliseconds constants
  public static final byte[] SEC_TO_MILLIS_SHIFT_POS = {9, 8, 7, 6, 5, 3};

  // --------------------------------------
  public static short convertToDate(short time, byte[] scratchPad) {

    short yrsCount = 0;
    short monthCount = 1;
    short dayCount = 1;
    short hhCount = 0;
    short mmCount = 0;
    short ssCount = 0;
    short inputOffset = 0;
    short scratchPadOffset = UINT8;
    byte Z = 0x5A;
    Util.arrayFillNonAtomic(scratchPad, inputOffset, (short) 256, (byte) 0);
    Util.arrayCopyNonAtomic(
        KMInteger.cast(time).getBuffer(),
        KMInteger.cast(time).getStartOff(),
        scratchPad,
        (short) (8 - KMInteger.cast(time).length()),
        KMInteger.cast(time).length());
    if (KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, dec319999Ms, (short) 0, UINT8)
        > 0) {
      KMException.throwIt(KMError.INVALID_ARGUMENT);
    }
    short roundedBaseYearMillisOff = scratchPadOffset;
    scratchPadOffset = (short) (roundedBaseYearMillisOff + UINT8);
    short roundedBaseYear =
        getNearestLowerHundredthBoundMills(
            scratchPad, inputOffset, roundedBaseYearMillisOff, scratchPadOffset);
    roundedBaseYear =
        adjustBaseYearToLeapYearsRange(
            roundedBaseYear, scratchPad, inputOffset, roundedBaseYearMillisOff, scratchPadOffset);
    // Find the difference in millis from the base year.
    subtract(scratchPad, inputOffset, roundedBaseYearMillisOff, scratchPadOffset, UINT8);
    Util.arrayCopyNonAtomic(scratchPad, scratchPadOffset, scratchPad, inputOffset, UINT8);
    scratchPadOffset = (short) (inputOffset + 8);

    // divide the given time with four years msec count
    if (KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, fourYrsMsec, (short) 0, UINT8)
        >= 0) {
      Util.arrayCopyNonAtomic(fourYrsMsec, (short) 0, scratchPad, scratchPadOffset, UINT8);
      // quotient is multiple of 4
      yrsCount = divide(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8));
      yrsCount = (short) (yrsCount * 4); // number of yrs.
      // copy reminder as new dividend
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    yrsCount += roundedBaseYear;
    roundedBaseYear = (short) (yrsCount + 4);
    // Increment the yrsCount until the difference becomes less than an year.
    for (; yrsCount <= roundedBaseYear; yrsCount++) {
      byte[] yearMillis = yearToMillis(yrsCount);
      if ((KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, yearMillis, (short) 0, UINT8)
          < 0)) {
        break;
      }
      Util.arrayCopyNonAtomic(yearMillis, (short) 0, scratchPad, scratchPadOffset, UINT8);
      subtract(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8), UINT8);
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    // divide the given time with one month msec count
    for (; monthCount <= 12; monthCount++) {
      byte[] monthMillis = monthToMillis(yrsCount, monthCount);
      if ((KMInteger.unsignedByteArrayCompare(
              scratchPad, inputOffset, monthMillis, (short) 0, UINT8)
          < 0)) {
        break;
      }
      Util.arrayCopyNonAtomic(monthMillis, (short) 0, scratchPad, scratchPadOffset, UINT8);
      subtract(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8), UINT8);
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    // divide the given time with one day msec count
    if (KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, oneDayMsec, (short) 0, UINT8)
        >= 0) {
      Util.arrayCopyNonAtomic(oneDayMsec, (short) 0, scratchPad, scratchPadOffset, UINT8);
      dayCount = divide(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8));
      dayCount++;
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    // divide the given time with one hour msec count
    if (KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, oneHourMsec, (short) 0, UINT8)
        >= 0) {
      Util.arrayCopyNonAtomic(oneHourMsec, (short) 0, scratchPad, scratchPadOffset, UINT8);
      hhCount = divide(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8));
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    // divide the given time with one minute msec count
    if (KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, oneMinMsec, (short) 0, UINT8)
        >= 0) {
      Util.arrayCopyNonAtomic(oneMinMsec, (short) 0, scratchPad, scratchPadOffset, UINT8);
      mmCount = divide(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8));
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    // divide the given time with one second msec count
    if (KMInteger.unsignedByteArrayCompare(scratchPad, inputOffset, oneSecMsec, (short) 0, UINT8)
        >= 0) {
      Util.arrayCopyNonAtomic(oneSecMsec, (short) 0, scratchPad, scratchPadOffset, UINT8);
      ssCount = divide(scratchPad, inputOffset, scratchPadOffset, (short) (scratchPadOffset + 8));
      Util.arrayCopyNonAtomic(
          scratchPad, (short) (scratchPadOffset + 8), scratchPad, inputOffset, UINT8);
    }

    // Now convert to ascii string YYMMDDhhmmssZ or YYYYMMDDhhmmssZ
    Util.arrayFillNonAtomic(scratchPad, inputOffset, (short) 256, (byte) 0);
    short len = numberToString(yrsCount, scratchPad, inputOffset); // returns YYYY
    len += numberToString(monthCount, scratchPad, len);
    len += numberToString(dayCount, scratchPad, len);
    len += numberToString(hhCount, scratchPad, len);
    len += numberToString(mmCount, scratchPad, len);
    len += numberToString(ssCount, scratchPad, len);
    scratchPad[len] = Z;
    len++;
    if (yrsCount < year2050) {
      return KMByteBlob.instance(scratchPad, (short) (inputOffset + 2), (short) (len - 2)); // YY
    } else {
      return KMByteBlob.instance(scratchPad, inputOffset, len); // YYYY
    }
  }

  private static short getNearestLowerHundredthBoundMills(
      byte[] scratchPad, short timeOff, short outputMillisOffset, short scratchPadOff) {
    short start = 0;
    short end = 0;
    byte[] currentTimeMillis = null;
    byte[] prevTimeMillis = null;
    if (KMInteger.unsignedByteArrayCompare(
            scratchPad, timeOff, firstJan2000MSec, (short) 0, (short) 8)
        < 0) {
      start = year2000;
      end = year2000;
      currentTimeMillis = firstJan2000MSec;
      prevTimeMillis = firstJan1970MSec;
    } else if (KMInteger.unsignedByteArrayCompare(
            scratchPad, timeOff, firstJan4000MSec, (short) 0, (short) 8)
        < 0) {
      start = year2000;
      end = year4000;
      currentTimeMillis = firstJan2000MSec;
      prevTimeMillis = firstJan2000MSec;
    } else if (KMInteger.unsignedByteArrayCompare(
            scratchPad, timeOff, firstJan6000MSec, (short) 0, (short) 8)
        < 0) {
      start = year4000;
      end = year6000;
      currentTimeMillis = firstJan4000MSec;
      prevTimeMillis = firstJan4000MSec;
    } else if (KMInteger.unsignedByteArrayCompare(
            scratchPad, timeOff, firstJan8000MSec, (short) 0, (short) 8)
        < 0) {
      start = year6000;
      end = year8000;
      currentTimeMillis = firstJan6000MSec;
      prevTimeMillis = firstJan6000MSec;
    } else if (KMInteger.unsignedByteArrayCompare(
            scratchPad, timeOff, dec319999Ms, (short) 0, (short) 8)
        <= 0) {
      start = year8000;
      end = year10000;
      currentTimeMillis = firstJan8000MSec;
      prevTimeMillis = firstJan8000MSec;
    }
    short prevTimeOff = scratchPadOff;
    short curTimeOff = (short) (prevTimeOff + 8);
    scratchPadOff = (short) (curTimeOff + 8);
    Util.arrayCopyNonAtomic(currentTimeMillis, (short) 0, scratchPad, curTimeOff, (short) 8);
    Util.arrayCopyNonAtomic(prevTimeMillis, (short) 0, scratchPad, prevTimeOff, (short) 8);
    return getNearestLowerHundredthBoundMillsInRange(
        scratchPad,
        timeOff,
        curTimeOff,
        prevTimeOff,
        outputMillisOffset,
        scratchPadOff,
        start,
        end);
  }

  private static short getNearestLowerHundredthBoundMillsInRange(
      byte[] scratchPad,
      short timeOff,
      short curTimeOff,
      short prevTimeOff,
      short outputMillisOffset,
      short scratchPadOff,
      short startYear,
      short endYear) {
    // A leap year misses for every 100 years if it is not divisible by 400.
    short nearestHundredthBaseYear = 0;
    for (short year = startYear; year <= endYear; year += 100) {
      byte result = compare(scratchPad, timeOff, curTimeOff);
      if (result < 0) {
        Util.arrayCopyNonAtomic(scratchPad, prevTimeOff, scratchPad, outputMillisOffset, (short) 8);
        nearestHundredthBaseYear = (short) ((year == year2000) ? year1970 : (year - 100));
        break;
      } else if (result == 0) {
        Util.arrayCopyNonAtomic(scratchPad, curTimeOff, scratchPad, outputMillisOffset, (short) 8);
        nearestHundredthBaseYear = year;
        break;
      } else {
        if (isLeapYear(year)) {
          Util.arrayCopyNonAtomic(hundredYrsMSec, (short) 0, scratchPad, scratchPadOff, (short) 8);
        } else {
          Util.arrayCopyNonAtomic(
              OneDayLessHundredYrsMSec, (short) 0, scratchPad, scratchPadOff, (short) 8);
        }
        add(scratchPad, curTimeOff, scratchPadOff, (short) (scratchPadOff + 8));
        Util.arrayCopyNonAtomic(scratchPad, curTimeOff, scratchPad, prevTimeOff, (short) 8);
        Util.arrayCopyNonAtomic(
            scratchPad, (short) (scratchPadOff + 8), scratchPad, curTimeOff, (short) 8);
      }
    }
    return nearestHundredthBaseYear;
  }

  private static short adjustBaseYearToLeapYearsRange(
      short roundedBaseYear,
      byte[] scratchPad,
      short inputOffset,
      short roundedBaseYearMillisOff,
      short scratchPadOffset) {
    if (roundedBaseYear != year1970 && !isLeapYear(roundedBaseYear)) {
      // The rounded base year must fall within the range of leap years, which occur every
      // four years. If the rounded base year is not a leap year then add one year to it
      // so that it comes in the range of leap years. This is necessary when we divide the
      // difference of the given time and rounded base year with four year milliseconds
      // value.
      Util.arrayCopyNonAtomic(yearMsec, (short) 0, scratchPad, scratchPadOffset, UINT8);
      add(scratchPad, roundedBaseYearMillisOff, scratchPadOffset, (short) (scratchPadOffset + 8));
      if (compare(scratchPad, inputOffset, (short) (scratchPadOffset + 8)) > 0) {
        roundedBaseYear += 1;
        Util.arrayCopyNonAtomic(
            scratchPad,
            (short) (scratchPadOffset + 8),
            scratchPad,
            roundedBaseYearMillisOff,
            UINT8);
      }
    }
    return roundedBaseYear;
  }

  public static short numberToString(short number, byte[] scratchPad, short offset) {
    byte zero = 0x30;
    byte len = 2;
    byte digit;
    if (number > 999) {
      len = 4;
    }
    byte index = len;
    while (index > 0) {
      digit = (byte) (number % 10);
      number = (short) (number / 10);
      scratchPad[(short) (offset + index - 1)] = (byte) (digit + zero);
      index--;
    }
    return len;
  }

  // Use Euclid's formula: dividend = quotient*divisor + remainder
  // i.e. dividend - quotient*divisor = remainder where remainder < divisor.
  // so this is division by subtraction until remainder remains.
  public static short divide(byte[] buf, short dividend, short divisor, short remainder) {
    short expCnt = 1;
    short q = 0;
    // first increase divisor so that it becomes greater then dividend.
    while (compare(buf, divisor, dividend) < 0) {
      shiftLeft(buf, divisor);
      expCnt = (short) (expCnt << 1);
    }
    // Now subtract divisor from dividend if dividend is greater then divisor.
    // Copy remainder in the dividend and repeat.
    while (expCnt != 0) {
      if (compare(buf, dividend, divisor) >= 0) {
        subtract(buf, dividend, divisor, remainder, (byte) 8);
        copy(buf, remainder, dividend);
        q = (short) (q + expCnt);
      }
      expCnt = (short) (expCnt >> 1);
      shiftRight(buf, divisor);
    }
    return q;
  }

  public static void copy(byte[] buf, short from, short to) {
    Util.arrayCopyNonAtomic(buf, from, buf, to, (short) 8);
  }

  public static byte compare(byte[] buf, short lhs, short rhs) {
    return KMInteger.unsignedByteArrayCompare(buf, lhs, buf, rhs, (short) 8);
  }

  public static void shiftLeft(byte[] buf, short start, short count) {
    short index = 0;
    while (index < count) {
      shiftLeft(buf, start);
      index++;
    }
  }

  public static void shiftLeft(byte[] buf, short start) {
    byte index = 7;
    byte carry = 0;
    byte tmp;
    while (index >= 0) {
      tmp = buf[(short) (start + index)];
      buf[(short) (start + index)] = (byte) (buf[(short) (start + index)] << 1);
      buf[(short) (start + index)] = (byte) (buf[(short) (start + index)] + carry);
      if (tmp < 0) {
        carry = 1;
      } else {
        carry = 0;
      }
      index--;
    }
  }

  public static void shiftRight(byte[] buf, short start) {
    byte index = 0;
    byte carry = 0;
    byte tmp;
    while (index < 8) {
      tmp = (byte) (buf[(short) (start + index)] & 0x01);
      buf[(short) (start + index)] = (byte) (buf[(short) (start + index)] >> 1);
      buf[(short) (start + index)] = (byte) (buf[(short) (start + index)] & 0x7F);
      buf[(short) (start + index)] = (byte) (buf[(short) (start + index)] | carry);
      if (tmp == 1) {
        carry = (byte) 0x80;
      } else {
        carry = 0;
      }
      index++;
    }
  }

  public static void add(byte[] buf, short op1, short op2, short result) {
    byte index = 7;
    byte carry = 0;
    short tmp;
    short val1 = 0;
    short val2 = 0;
    while (index >= 0) {
      val1 = (short) (buf[(short) (op1 + index)] & 0x00FF);
      val2 = (short) (buf[(short) (op2 + index)] & 0x00FF);
      tmp = (short) (val1 + val2 + carry);
      carry = 0;
      if (tmp > 255) {
        carry = 1; // max unsigned byte value is 255
      }
      buf[(short) (result + index)] = (byte) (tmp & (byte) 0xFF);
      index--;
    }
  }

  // subtraction by borrowing.
  public static void subtract(byte[] buf, short op1, short op2, short result, byte sizeBytes) {
    byte borrow = 0;
    byte index = (byte) (sizeBytes - 1);
    short r;
    short x;
    short y;
    while (index >= 0) {
      x = (short) (buf[(short) (op1 + index)] & 0xFF);
      y = (short) (buf[(short) (op2 + index)] & 0xFF);
      r = (short) (x - y - borrow);
      borrow = 0;
      if (r < 0) {
        borrow = 1;
        r = (short) (r + 256); // max unsigned byte value is 255
      }
      buf[(short) (result + index)] = (byte) (r & 0xFF);
      index--;
    }
  }

  public static short countTemporalCount(
      byte[] bufTime, short timeOff, short timeLen, byte[] scratchPad, short offset) {
    Util.arrayFillNonAtomic(scratchPad, (short) offset, (short) 24, (byte) 0);
    Util.arrayCopyNonAtomic(bufTime, timeOff, scratchPad, (short) (offset + 8 - timeLen), timeLen);
    Util.arrayCopyNonAtomic(
        ThirtDaysMonthMsec, (short) 0, scratchPad, (short) (offset + 8), (short) 8);
    return divide(scratchPad, (short) 0, (short) 8, (short) 16);
  }

  public static boolean isLeapYear(short year) {
    if ((short) (year % 4) == (short) 0) {
      if (((short) (year % 100) == (short) 0) && ((short) (year % 400)) != (short) 0) {
        return false;
      }
      return true;
    }
    return false;
  }

  public static void computeOnesCompliment(byte[] buf, short offset, short len) {
    short index = offset;
    // Compute 1s compliment
    while (index < (short) (len + offset)) {
      buf[index] = (byte) ~buf[index];
      index++;
    }
  }

  private static byte[] yearToMillis(short year) {
    if (isLeapYear(year)) {
      return leapYearMsec;
    } else {
      return yearMsec;
    }
  }

  private static byte[] monthToMillis(short year, short month) {
    if (month == 2) {
      if (isLeapYear(year)) {
        return febMonthLeapMSec;
      } else {
        return febMonthMsec;
      }
    } else if (((month <= 7) && ((month % 2 == 1))) || ((month > 7) && ((month % 2 == 0)))) {
      return ThirtyOneDaysMonthMsec;
    } else {
      return ThirtDaysMonthMsec;
    }
  }

  // i * 1000 = (i << 9) + (i << 8) + (i << 7) + (i << 6) + (i << 5) + ( i << 3)
  public static void convertToMilliseconds(
      byte[] buf, short inputOff, short outputOff, short scratchPadOff) {
    short index = 0;
    short length = (short) SEC_TO_MILLIS_SHIFT_POS.length;
    while (index < length) {
      Util.arrayCopyNonAtomic(buf, inputOff, buf, scratchPadOff, (short) 8);
      shiftLeft(buf, scratchPadOff, SEC_TO_MILLIS_SHIFT_POS[index]);
      Util.arrayCopyNonAtomic(buf, outputOff, buf, (short) (scratchPadOff + 8), (short) 8);
      add(buf, scratchPadOff, (short) (8 + scratchPadOff), (short) (16 + scratchPadOff));
      Util.arrayCopyNonAtomic(buf, (short) (scratchPadOff + 16), buf, outputOff, (short) 8);
      Util.arrayFillNonAtomic(buf, scratchPadOff, (short) 24, (byte) 0);
      index++;
    }
  }
}
