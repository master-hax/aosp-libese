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

const uint8_t kEseBootRollbackLocationCount = 8;
const uint16_t kEseBootOwnerKeyFlagOffset = (8 * 8);
const uint16_t kEseBootOwnerKeyOffset = (8 * 8) + 1;

void ese_boot_session_init(struct EseBootSession *session);
EseAppResult ese_boot_session_open(struct EseInterface *ese, struct EseBootSession *session);
EseAppResult ese_boot_session_close(struct EseBootSession *session);

/* Store an uint64_t from the SE identified by an |location| from 0-7. */
EseAppResult ese_boot_rollback_index_write(struct EseBootSession *session, uint8_t location, uint64_t rollback_index);
/* Read an uint64_t from the SE identified by an |location| from 0-7. */
EseAppResult ese_boot_rollback_index_read(struct EseBootSession *session, uint8_t location, uint64_t *rollback_index);
/* Resets all 8 slots back to 0. */
EseAppResult ese_boot_rollback_index_reset(struct EseBootSession *session);

/* Store an owner boot key. */
EseAppResult ese_boot_owner_key_store(struct EseBootSession *session, const uint8_t *key_data, uint16_t length);
EseAppResult ese_boot_owner_key_read(struct EseBootSession *session, uint8_t *key_data, uint16_t length);

/* Note: will be called by lock accessors below. */
EseAppResult ese_boot_is_provisioned(struct EseBootSession *session);
EseAppResult ese_boot_is_carrier_locked(struct EseBootSession *session);
EseAppResult ese_boot_is_device_locked(struct EseBootSession *session);
EseAppResult ese_boot_is_boot_locked(struct EseBootSession *session);
/* Returns true if an owner key flag is set regardless of other lock state. */
EseAppResult ese_boot_is_owner_locked(struct EseBootSession *session);

/* Transitions the boot storage to a production state. */
EseAppResult ese_boot_provision(struct EseBootSession *session);
/* Should only be called during RMA with boot priv asserted. */
EseAppResult ese_boot_unprovision(struct EseBootSession *session);

EseAppResult ese_boot_carrier_lock(struct EseBootSession *session, const uint8_t *device_data, uint32_t size);
/* Usually called from the HLOS */
EseAppResult ese_boot_carrier_unlock(struct EseBootSession *session, const uint8_t *unlock_token, uint32_t size);
/* Must be called in the boot loader. */
EseAppResult ese_boot_set_boot_lock(struct EseBootSession *session, bool lock);
/* Must be called from HLOS. */
EseAppResult ese_boot_set_device_lock(struct EseBootSession *session, bool lock);
/* Must be called in the boot loader. */
EseAppResult ese_boot_set_owner_lock(struct EseBootSession *session, bool lock);


/* TODO: Provide some higher level helpers for specific transitions if needed:
 * - Initial provisioning
 * - Boot Lock -> Unlock
 * - Boot Unlock -> Lock
 */


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* ESE_APP_BOOT_H_ */
