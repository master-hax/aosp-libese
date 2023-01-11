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

#define LOG_TAG "javacard.keymint.device.strongbox-impl"

#include "JavacardKeyMint3Device.h"

#include <regex.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <KeyMintUtils.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <hardware/hw_auth_token.h>
#include <keymaster/android_keymaster_messages.h>
#include <keymaster/wrapped_key.h>

#include "JavacardKeyMintOperation.h"
#include "JavacardSharedSecret.h"

namespace aidl::android::hardware::security::keymint {
using cppbor::Bstr;
using cppbor::EncodedItem;
using cppbor::Uint;
using ::keymaster::AuthorizationSet;
using ::keymaster::dup_buffer;
using ::keymaster::KeymasterBlob;
using ::keymaster::KeymasterKeyBlob;
using ::keymint::javacard::Instruction;
using std::string;

ScopedAStatus JavacardKeyMint3Device::defaultHwInfo(KeyMintHardwareInfo* info) {
    info->versionNumber = 3;
    info->keyMintAuthorName = "Google";
    info->keyMintName = "JavacardKeymintDevice";
    info->securityLevel = securitylevel_;
    info->timestampTokenRequired = true;
    return ScopedAStatus::ok();
}

ScopedAStatus JavacardKeyMint3Device::getHardwareInfo(KeyMintHardwareInfo* info) {
    auto [item, err] = card_->sendRequest(Instruction::INS_GET_HW_INFO_CMD);
    std::optional<string> optKeyMintName;
    std::optional<string> optKeyMintAuthorName;
    std::optional<uint64_t> optSecLevel;
    std::optional<uint64_t> optVersion;
    std::optional<uint64_t> optTsRequired;
    if (err != KM_ERROR_OK || !(optVersion = cbor_.getUint64(item, 1)) ||
        !(optSecLevel = cbor_.getUint64(item, 2)) ||
        !(optKeyMintName = cbor_.getByteArrayStr(item, 3)) ||
        !(optKeyMintAuthorName = cbor_.getByteArrayStr(item, 4)) ||
        !(optTsRequired = cbor_.getUint64(item, 5))) {
        LOG(ERROR) << "Error in response of getHardwareInfo.";
        LOG(INFO) << "Returning defaultHwInfo in getHardwareInfo.";
        return defaultHwInfo(info);
    }
    card_->initializeJavacard();
    info->keyMintName = std::move(optKeyMintName.value());
    info->keyMintAuthorName = std::move(optKeyMintAuthorName.value());
    info->timestampTokenRequired = (optTsRequired.value() == 1);
    info->securityLevel = static_cast<SecurityLevel>(std::move(optSecLevel.value()));
    info->versionNumber = static_cast<int32_t>(std::move(optVersion.value()));
    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::security::keymint
