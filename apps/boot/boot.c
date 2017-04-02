/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "include/ese/app/boot.h"
#include "boot_private.h"

const uint16_t kBootStorageLength = 4096;
/* Non-static, but visibility=hidden so they can be used in test. */
const uint8_t kManageChannelOpen[] = {0x00, 0x70, 0x00, 0x00, 0x01};
const uint32_t kManageChannelOpenLength = (uint32_t)sizeof(kManageChannelOpen);
const uint8_t kManageChannelClose[] = {0x00, 0x70, 0x80, 0x00, 0x00};

const uint8_t kSelectApplet[] = {0x00, 0xA4, 0x04, 0x00, 0x10, 0xA0, 0x00, 0x00,
                                 0x04, 0x76, 0x50, 0x49, 0x58, 0x4C, 0x42, 0x4F,
                                 0x4F, 0x54, 0x00, 0x01, 0x01, 0x00};
const uint32_t kSelectAppletLength = (uint32_t)sizeof(kSelectApplet);
/* p1 and p2 are offset; data appended and len be updated. */
const uint8_t kStoreCmd[] = {0x80, 0x01};
/* p12 and p2 are the offset; data is always a 2 byte short. */
const uint8_t kLoadCmd[] = {0x80, 0x02};
/* P1 will be lock type, P2 is always 0. Data should be nothing. */
const uint8_t kGetLockState[] = {0x80, 0x03, 0x00, 0x00, 0x00};
/* P1 will be lock type, P2 is always 0. Data will be nothing, NONCE||SIGNATURE,
 * carrier-lock hash */
const uint8_t kSetLockState[] = {0x80, 0x04, 0x00, 0x00, 0x00};

EseAppResult check_apdu_return(uint8_t code[2]) {
  if (code[0] == 0x90 && code[1] == 0x00) {
    return ESE_APP_RESULT_OK;
  }
  if (code[0] == 0x66 && code[1] == 0xA5) {
    return ESE_APP_RESULT_ERROR_COOLDOWN;
  }
  if (code[0] == 0x6A && code[1] == 0x83) {
    return ESE_APP_RESULT_ERROR_UNCONFIGURED;
  }
  /* TODO(wad) Bubble up the error code if needed. */
  ALOGE("unhandled response %.2x %.2x", code[0], code[1]);
  return ESE_APP_RESULT_ERROR_OS;
}

ESE_API void ese_boot_session_init(struct EseBootSession *session) {
  session->ese = NULL;
  session->active = false;
  session->channel_id = 0;
}

ESE_API EseAppResult ese_boot_session_open(struct EseInterface *ese,
                                           struct EseBootSession *session) {
  struct EseSgBuffer tx[2];
  struct EseSgBuffer rx;
  uint8_t rx_buf[32];
  int rx_len;
  if (!ese || !session) {
    ALOGE("Invalid |ese| or |session|");
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  if (session->active == true) {
    ALOGE("|session| is already active");
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  /* Instantiate a logical channel */
  rx_len = ese_transceive(ese, kManageChannelOpen, sizeof(kManageChannelOpen),
                          rx_buf, sizeof(rx_buf));
  if (ese_error(ese)) {
    ALOGE("transceive error: code:%d message:'%s'", ese_error_code(ese),
          ese_error_message(ese));
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (rx_len < 0) {
    ALOGE("transceive error: rx_len: %d", rx_len);
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (rx_len < 2) {
    ALOGE("transceive error: reply too short");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  EseAppResult ret;
  ret = check_apdu_return(&rx_buf[rx_len - 2]);
  if (ret != ESE_APP_RESULT_OK) {
    ALOGE("MANAGE CHANNEL OPEN failed with error code: %x %x",
          rx_buf[rx_len - 2], rx_buf[rx_len - 1]);
    return ret;
  }
  if (rx_len < 3) {
    ALOGE("transceive error: successful reply unexpectedly short");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  session->ese = ese;
  session->channel_id = rx_buf[rx_len - 3];

  /* Select Boot Applet. */
  uint8_t chan = kSelectApplet[0] | session->channel_id;
  tx[0].base = &chan;
  tx[0].len = 1;
  tx[1].base = (uint8_t *)&kSelectApplet[1];
  tx[1].len = sizeof(kSelectApplet) - 1;
  rx.base = &rx_buf[0];
  rx.len = sizeof(rx_buf);
  rx_len = ese_transceive_sg(ese, tx, 2, &rx, 1);
  if (rx_len < 0 || ese_error(ese)) {
    ALOGE("transceive error: caller should check ese_error()");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (rx_len < 2) {
    ALOGE("transceive error: reply too short");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  ret = check_apdu_return(&rx_buf[rx_len - 2]);
  if (ret != ESE_APP_RESULT_OK) {
    ALOGE("SELECT failed with error code: %x %x", rx_buf[rx_len - 2],
          rx_buf[rx_len - 1]);
    return ret;
  }
  session->active = true;
  return ESE_APP_RESULT_OK;
}

ESE_API EseAppResult ese_boot_session_close(struct EseBootSession *session) {
  uint8_t rx_buf[32];
  int rx_len;
  if (!session || !session->ese) {
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  if (!session->active || session->channel_id == 0) {
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  /* Release the channel */
  uint8_t close_channel[sizeof(kManageChannelClose)];
  ese_memcpy(close_channel, kManageChannelClose, sizeof(kManageChannelClose));
  close_channel[0] |= session->channel_id;
  close_channel[3] |= session->channel_id;
  rx_len = ese_transceive(session->ese, close_channel, sizeof(close_channel),
                          rx_buf, sizeof(rx_buf));
  if (rx_len < 0 || ese_error(session->ese)) {
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (rx_len < 2) {
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  EseAppResult ret;
  ret = check_apdu_return(&rx_buf[rx_len - 2]);
  if (ret != ESE_APP_RESULT_OK) {
    return ret;
  }
  session->channel_id = 0;
  session->active = false;
  return ESE_APP_RESULT_OK;
}

EseAppResult boot_storage_write(struct EseBootSession *session, uint16_t offset,
                                const uint8_t *data, uint16_t length) {
  struct EseSgBuffer tx[5];
  struct EseSgBuffer rx;
  uint8_t chan;
  uint8_t apdu_len[] = {0x0, (length >> 8), (length & 0xff)};
  uint8_t p1p2[] = {(offset >> 8), (offset & 0xff)};
  uint8_t rx_buf[32];
  int rx_len;
  if (!session || !session->ese || !session->active) {
    ALOGE("boot_storage_write: invalid session");
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  if (length > kBootStorageLength) {
    ALOGE("boot_storage_write: oversized length");
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }

  // APDU CLA
  chan = kStoreCmd[0] | session->channel_id;
  tx[0].base = &chan;
  tx[0].len = 1;
  // APDU INS
  tx[1].base = (uint8_t *)&kStoreCmd[2];
  tx[1].len = 1;
  // APDU P1 - P2
  tx[2].base = &p1p2[0];
  tx[2].len = sizeof(p1p2);
  // APDU Lc
  tx[3].base = &apdu_len[0];
  tx[3].len = sizeof(apdu_len);
  // APDU data
  tx[4].base = (uint8_t *)data;
  tx[4].len = length;
  rx.base = &rx_buf[0];
  rx.len = sizeof(rx_buf);

  rx_len = ese_transceive_sg(session->ese, tx, 5, &rx, 1);
  if (rx_len < 0 || ese_error(session->ese)) {
    ALOGE("boot_storage_write: comm error");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (rx_len < 2) {
    ALOGE("boot_storage_write: too few bytes recieved.");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  // The only response is success or failure -- no data.
  // TODO(wad): move to ALOGD
  return check_apdu_return(&rx_buf[rx_len - 2]);
}

EseAppResult boot_storage_read(struct EseBootSession *session, uint16_t offset,
                               uint8_t *data, uint16_t length) {
  struct EseSgBuffer tx[5];
  struct EseSgBuffer rx[2];
  uint8_t chan;
  uint8_t apdu_len[] = {2};
  uint8_t load_len[] = {(length >> 8), (length & 0xff)};
  uint8_t p1p2[] = {(offset >> 8), (offset & 0xff)};
  uint8_t code[2];
  int rx_len;
  if (!session || !session->ese || !session->active) {
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  if (length > kBootStorageLength) {
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }

  chan = kLoadCmd[0] | session->channel_id;
  tx[0].base = &chan;
  tx[0].len = 1;
  tx[1].base = (uint8_t *)&kLoadCmd[1];
  tx[1].len = 1;
  tx[2].base = &p1p2[0];
  tx[2].len = sizeof(p1p2);
  tx[3].base = &apdu_len[0];
  tx[3].len = sizeof(apdu_len);
  tx[4].base = &load_len[0];
  tx[4].len = sizeof(load_len);

  rx[0].base = data;
  rx[0].len = length;
  rx[1].base = &code[0];
  rx[1].len = sizeof(code);

  rx_len = ese_transceive_sg(session->ese, tx, 4, rx, 2);
  if (rx_len < 0 || ese_error(session->ese)) {
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  // On success, all the data was read. On failure, it wasn't.
  if (rx_len < length + 2) {
    // TODO(wad) log the response code when it is not in code.
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  return check_apdu_return(code);
}

EseAppResult get_lock_state(struct EseBootSession *session, LockState lock) {
  struct EseSgBuffer tx[4];
  struct EseSgBuffer rx[1];
  int rx_len;
  if (!session || !session->ese || !session->active) {
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }

  uint8_t chan = kGetLockState[0] | session->channel_id;
  tx[0].base = &chan;
  tx[0].len = 1;
  tx[1].base = (uint8_t *)&kGetLockState[1];
  tx[1].len = 1;

  uint8_t p1p2[] = {lock, 0x0};
  tx[2].base = &p1p2[0];
  tx[2].len = sizeof(p1p2);

  uint8_t apdu_len[] = {0x0};
  tx[3].base = &apdu_len[0];
  tx[3].len = sizeof(apdu_len);

  uint8_t reply[3];
  rx[0].base = &reply[0];
  rx[0].len = sizeof(reply);

  rx_len = ese_transceive_sg(session->ese, tx, 4, rx, 1);
  if (rx_len < 2 || ese_error(session->ese)) {
    ALOGE("Failed to read lock state (%d).", lock);
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  EseAppResult ret = check_apdu_return(&reply[rx_len - 2]);
  if (ret != ESE_APP_RESULT_OK) {
    ALOGE("Get lock state returned a SE OS error.");
    return ret;
  }
  if (rx_len < 3) {
    ALOGE("Get lock state did not receive any data.");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (reply[0] & lock) {
    return ESE_APP_RESULT_TRUE;
  }
  return ESE_APP_RESULT_FALSE;
}

EseAppResult set_lock_state(struct EseBootSession *session, LockState lock,
                            bool enable, const uint8_t *data, uint16_t len) {
  struct EseSgBuffer tx[5];
  uint32_t tx_nsegs = 4;
  struct EseSgBuffer rx[1];
  int rx_len;
  if (!session || !session->ese || !session->active) {
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }

  uint8_t chan = kSetLockState[0] | session->channel_id;
  tx[0].base = &chan;
  tx[0].len = 1;
  tx[1].base = (uint8_t *)&kSetLockState[1];
  tx[1].len = 1;

  uint8_t lock_value = enable ? 0x1 : 0x0;
  uint8_t p1p2[] = {lock, lock_value};
  tx[2].base = &p1p2[0];
  tx[2].len = sizeof(p1p2);

  uint8_t apdu_len[3];
  apdu_len[0] = 0x00;
  tx[3].base = &apdu_len[0];
  tx[3].len = 1;

  if (len) {
    apdu_len[1] = len >> 8;
    apdu_len[2] = len & 0xff;
    tx[3].len = 3;

    tx_nsegs++;
    tx[4].base = (uint8_t *)data;
    tx[4].len = len;
  }

  uint8_t code[2];
  rx[0].base = &code[0];
  rx[0].len = sizeof(code);

  rx_len = ese_transceive_sg(session->ese, tx, tx_nsegs, rx, 1);
  if (rx_len < 0 || ese_error(session->ese)) {
    ALOGE("Failed to read lock state (%d).", lock);
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  if (rx_len != 2) {
    ALOGE("Set lock state returned invalid amount of data");
    return ESE_APP_RESULT_ERROR_COMM_FAILED;
  }
  return check_apdu_return(code);
}

ESE_API EseAppResult ese_boot_rollback_index_write(
    struct EseBootSession *session, uint8_t location, uint64_t rollback_index) {
  if (location >= kEseBootRollbackLocationCount) {
    ALOGE("|location| is too large");
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  // First 64 bytes reserved for indices.
  uint16_t offset = location * sizeof(rollback_index);
  // The rollback_index is written in host-byte order as it is the only
  // consumer.
  uint8_t *data = (uint8_t *)&rollback_index;
  return boot_storage_write(session, offset, data, sizeof(rollback_index));
}

ESE_API EseAppResult
ese_boot_rollback_index_read(struct EseBootSession *session, uint8_t location,
                             uint64_t *rollback_index) {
  if (location >= kEseBootRollbackLocationCount) {
    ALOGE("|location| is too large");
    return ESE_APP_RESULT_ERROR_ARGUMENTS;
  }
  // First 64 bytes reserved for indices.
  uint16_t offset = location * sizeof(*rollback_index);
  return boot_storage_read(session, offset, (uint8_t *)rollback_index,
                           sizeof(*rollback_index));
}

ESE_API EseAppResult
ese_boot_rollback_index_reset(struct EseBootSession *session) {
  uint8_t kZero[sizeof(uint64_t) * kEseBootRollbackLocationCount];
  ese_memset(&kZero[0], 0, sizeof(kZero));
  return boot_storage_write(session, 0, &kZero[0], sizeof(kZero));
}

ESE_API EseAppResult ese_boot_owner_key_store(struct EseBootSession *session,
                                              const uint8_t *key_data,
                                              uint16_t length) {
  return boot_storage_write(session, kEseBootOwnerKeyOffset, key_data, length);
}

ESE_API EseAppResult ese_boot_owner_key_read(struct EseBootSession *session,
                                             uint8_t *key_data,
                                             uint16_t length) {
  return boot_storage_read(session, kEseBootOwnerKeyOffset, key_data, length);
}

ESE_API EseAppResult ese_boot_set_owner_lock(struct EseBootSession *session,
                                             bool lock) {
  // TODO(wad) consider making this applet internal bound to BOOT_LOCK.
  uint8_t value = lock ? 0x1 : 0x0;
  return boot_storage_write(session, kEseBootOwnerKeyFlagOffset, &value,
                            sizeof(value));
}

ESE_API EseAppResult ese_boot_is_owner_locked(struct EseBootSession *session) {
  uint8_t value = 0x0;
  // TODO(wad) Should we short-circuit the response by asking about boot lock
  // state first?
  EseAppResult ret = boot_storage_read(session, kEseBootOwnerKeyFlagOffset,
                                       &value, sizeof(value));
  if (ret != ESE_APP_RESULT_OK) {
    return ret;
  }
  if (value == 0x1) {
    return ESE_APP_RESULT_TRUE;
  }
  return ESE_APP_RESULT_FALSE;
}

ESE_API EseAppResult ese_boot_is_provisioned(struct EseBootSession *session) {
  return get_lock_state(session, LOCK_STATE_PROVISION);
}

ESE_API EseAppResult
ese_boot_is_carrier_locked(struct EseBootSession *session) {
  return get_lock_state(session, LOCK_STATE_CARRIER);
}

ESE_API EseAppResult ese_boot_is_device_locked(struct EseBootSession *session) {
  return get_lock_state(session, LOCK_STATE_DEVICE);
}

ESE_API EseAppResult ese_boot_is_boot_locked(struct EseBootSession *session) {
  return get_lock_state(session, LOCK_STATE_BOOT);
}

// Call _AFTER_ calling carrier lock
ESE_API EseAppResult ese_boot_provision(struct EseBootSession *session) {
  return set_lock_state(session, LOCK_STATE_PROVISION, true, NULL, 0);
}

ESE_API EseAppResult ese_boot_unprovision(struct EseBootSession *session) {
  return set_lock_state(session, LOCK_STATE_PROVISION, false, NULL, 0);
}

ESE_API EseAppResult ese_boot_carrier_lock(struct EseBootSession *session,
                                           const uint8_t *device_data,
                                           uint32_t size) {
  return set_lock_state(session, LOCK_STATE_CARRIER, true, device_data, size);
}

ESE_API EseAppResult ese_boot_carrier_unlock(struct EseBootSession *session,
                                             const uint8_t *unlock_token,
                                             uint32_t size) {
  // Should be sending a nonce || signed-blob.
  return set_lock_state(session, LOCK_STATE_CARRIER, false, unlock_token, size);
}

// XXX: Make owner key an optional data and length.
// XXX Then when checking boot-lock, it will return the key (in either state) if
// set.
// XXX Internally, it will keep an atomic flag for whether the key is set as
// well.
ESE_API EseAppResult ese_boot_set_boot_lock(struct EseBootSession *session,
                                            bool lock) {
  return set_lock_state(session, LOCK_STATE_BOOT, lock, NULL, 0);
}

ESE_API EseAppResult ese_boot_set_device_lock(struct EseBootSession *session,
                                              bool lock) {
  return set_lock_state(session, LOCK_STATE_DEVICE, lock, NULL, 0);
}
