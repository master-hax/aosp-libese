/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <aidl/android/hardware/security/keymint/BnKeyMintDevice.h>
#include <aidl/android/hardware/security/keymint/BnKeyMintOperation.h>
#include <aidl/android/hardware/security/keymint/HardwareAuthToken.h>
#include <aidl/android/hardware/security/sharedsecret/SharedSecretParameters.h>

#include "CborConverter.h"
#include "JavacardSecureElement.h"
#include "JavacardKeyMintDevice.h"

namespace aidl::android::hardware::security::keymint {
using cppbor::Item;
using ::keymint::javacard::CborConverter;
using ::keymint::javacard::JavacardSecureElement;
using ndk::ScopedAStatus;
using secureclock::TimeStampToken;
using std::array;
using std::optional;
using std::shared_ptr;
using std::vector;

class JavacardKeyMint3Device : public JavacardKeyMintDevice {
  public:
    explicit JavacardKeyMint3Device(shared_ptr<JavacardSecureElement> card)
        : JavacardKeyMintDevice(card), securitylevel_(SecurityLevel::STRONGBOX), card_(card) {
    }
    virtual ~JavacardKeyMint3Device() {}

    ScopedAStatus getHardwareInfo(KeyMintHardwareInfo* info);


  private:
    ScopedAStatus defaultHwInfo(KeyMintHardwareInfo* info);

    const SecurityLevel securitylevel_;
    const shared_ptr<JavacardSecureElement> card_;
    CborConverter cbor_;
};

}  // namespace aidl::android::hardware::security::keymint
