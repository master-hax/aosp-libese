/*
 * Copyright 2021, The Android Open Source Project
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

#include <cppbor.h>

#include <aidl/android/hardware/security/keymint/BnRemotelyProvisionedComponent.h>
#include <aidl/android/hardware/security/keymint/RpcHardwareInfo.h>
#include <aidl/android/hardware/security/keymint/SecurityLevel.h>

#include <keymaster/UniquePtr.h>
#include <keymaster/android_keymaster.h>

#include "CborConverter.h"
#include "JavacardSecureElement.h"

namespace aidl::android::hardware::security::keymint {
using ::keymint::javacard::CborConverter;
using ::keymint::javacard::JavacardSecureElement;
using ndk::ScopedAStatus;
using std::shared_ptr;

class JavacardRemotelyProvisionedComponent3Device : public BnRemotelyProvisionedComponent {
  public:
    explicit JavacardRemotelyProvisionedComponent3Device(shared_ptr<JavacardSecureElement> card)
        : card_(card) {}

    virtual ~JavacardRemotelyProvisionedComponent3Device() = default;

    ScopedAStatus getHardwareInfo(RpcHardwareInfo* info) override;

    ScopedAStatus generateEcdsaP256KeyPair(bool testMode, MacedPublicKey* macedPublicKey,
                                           std::vector<uint8_t>* privateKeyHandle) override;

    ScopedAStatus generateCertificateRequest(bool testMode,
                                             const std::vector<MacedPublicKey>& keysToSign,
                                             const std::vector<uint8_t>& endpointEncCertChain,
                                             const std::vector<uint8_t>& challenge,
                                             DeviceInfo* deviceInfo, ProtectedData* protectedData,
                                             std::vector<uint8_t>* keysToSignMac) override;

    ScopedAStatus generateCertificateRequestV2(const std::vector<MacedPublicKey>& keysToSign,
                                               const std::vector<uint8_t>& challenge,
                                               std::vector<uint8_t>* csr) override;

  private:
    ScopedAStatus beginSendData(const std::vector<MacedPublicKey>& keysToSign,
                                const std::vector<uint8_t>& challenge, DeviceInfo* deviceInfo,
                                uint32_t* version, std::string* certificateType);

    ScopedAStatus updateMacedKey(const std::vector<MacedPublicKey>& keysToSign,
                                 cppbor::Array& coseKeys);

    ScopedAStatus finishSendData(std::vector<uint8_t>& coseEncryptProtectedHeader,
                                 std::vector<uint8_t>& signature, uint32_t& version,
                                 uint32_t& respFlag);

    ScopedAStatus getDiceCertChain(std::vector<uint8_t>& diceCertChain);
    ScopedAStatus getUdsCertsChain(std::vector<uint8_t>& udsCertsChain);
    std::shared_ptr<JavacardSecureElement> card_;
    CborConverter cbor_;
};

}  // namespace aidl::android::hardware::security::keymint
