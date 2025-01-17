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
package com.android.javacard.seprovider;

import javacard.security.AESKey;
import org.globalplatform.upgrade.Element;

/** This is a wrapper class for AESKey. */
public class KMAESKey implements KMKey {

  public AESKey aesKey;

  public KMAESKey(AESKey key) {
    aesKey = key;
  }

  public static void onSave(Element element, KMAESKey kmKey) {
    element.write(kmKey.aesKey);
  }

  public static KMAESKey onRestore(AESKey aesKey) {
    if (aesKey == null) {
      return null;
    }
    return new KMAESKey(aesKey);
  }

  public static short getBackupPrimitiveByteCount() {
    return (short) 0;
  }

  public static short getBackupObjectCount() {
    return (short) 1;
  }

  @Override
  public short getPublicKey(byte[] buf, short offset) {
    return (short) 0;
  }
}
