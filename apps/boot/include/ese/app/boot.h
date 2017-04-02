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

#ifndef ESE_APP_BOOT_H_
#define ESE_APP_BOOT_H_ 1

#include "../../../../../libese/include/ese/ese.h"
#include "../../../../../libese/include/ese/log.h"
#include "../../../../../libese-sysdeps/include/ese/sysdeps.h"

#include "../../../../include/ese/app/result.h"

#ifdef __cplusplus
extern "C" {
#endif

struct EseBootSession {
  struct EseInterface *ese;
  bool active;
  uint8_t channel_id;
};

const uint8_t kEseBootRollbackSlotCount = 8;
const uint16_t kEseBootOwnerKeyMax = 1024;


/* Keep in sync with card/src/com/android/verifiedboot/storage/Storage.java */
typedef enum {
  kEseBootLockIdCarrier = 0,
  kEseBootLockIdDevice,
  kEseBootLockIdBoot,
  kEseBootLockIdOwner,
  kEseBootLockIdMax = kEseBootLockIdOwner,
} EseBootLockId;


void ese_boot_session_init(struct EseBootSession *session);
EseAppResult ese_boot_session_open(struct EseInterface *ese, struct EseBootSession *session);
EseAppResult ese_boot_session_close(struct EseBootSession *session);

EseAppResult ese_boot_lock_get(struct EseBootSession *session, EseBootLockId lockId, uint8_t *lockVal);
/* Returns the lock value suffixed with the exact amount of lock metadata. */
EseAppResult ese_boot_lock_xget(
    struct EseBootSession *session, EseBootLockId lockId, uint8_t *lockData,
    uint16_t maxSize, uint16_t *dataLen);
EseAppResult ese_boot_lock_set(struct EseBootSession *session, EseBootLockId lockId, uint8_t lockVal);
// First byte is the lock value.
EseAppResult ese_boot_lock_xset(struct EseBootSession *session, EseBootLockId lockId, const uint8_t *lockData, uint16_t dataLen);

EseAppResult ese_boot_carrier_lock_test(struct EseBootSession *session, const uint8_t *testdata, uint16_t len);
EseAppResult ese_boot_set_production(struct EseBootSession *session, bool production_mode);
EseAppResult ese_boot_get_state(struct EseBootSession *session, uint8_t *state, uint16_t maxSize);

/* Store an uint64_t from the SE identified by an |slot| from 0-7. */
EseAppResult ese_boot_rollback_index_write(struct EseBootSession *session, uint8_t slot, uint64_t value);
/* Read an uint64_t from the SE identified by an |slot| from 0-7. */
EseAppResult ese_boot_rollback_index_read(struct EseBootSession *session, uint8_t slot, uint64_t *value);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* ESE_APP_BOOT_H_ */
