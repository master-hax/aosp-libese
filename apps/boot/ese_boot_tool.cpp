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
 *
 * Commandline tool for interfacing with the Boot Storage app.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <cutils/properties.h>
#include <ese/ese.h>
#include "include/ese/app/boot.h"
ESE_INCLUDE_HW(ESE_HW_NXP_PN80T_NQ_NCI);

void usage(const char *prog) {
  fprintf(stderr,
    "Usage:\n"
    "%s <cmd> <args>\n"
    "    state    get\n"
    "    production set {true,false}\n"
    "    rollback set <іndex> <value>\n"
    "             get <іndex>\n"
    "    lock get {carrier,device,boot,owner}\n"
    "         set carrier 0 <unlockToken>\n"
    "                     <nonzero byte> {IMEI,MEID}\n"
    "             device <byte>\n"
    "             boot <byte>\n"
    "             owner 0\n"
    "             owner <non-zero byte> <keyValue>\n"
    "    verify-key test <blob>\n"
    "\n"
    "Note, any non-zero byte value is considered 'locked'.\n"
    "\n\n", prog);
}

#define handle_error(ese, result) ;
#if 0  // TODO
void handle_error(struct EseInterface *ese, EseAppResult result) {
  uint32_t minutes =  ese_cooldown_get(ese, ESE_COOLDOWN_ATTACK);
  set_timer_cb(minutes * 60, power_down_ese);
  ...
}
#endif

static uint16_t hexify(const std::string& input, std::vector<uint8_t> *output) {
  for (std::string::const_iterator it = input.begin();
       it != input.end();
       ++it) {
    std::string hex;
    hex.push_back(*it++);
    hex.push_back(*it);
    output->push_back(static_cast<uint8_t>(std::stoi(hex, nullptr, 16)));
  }
  return static_cast<uint16_t>(output->size() & 0xffff);
}


bool collect_device_data(const std::string &modem_id, std::string *device_data) {
  static const char *kDeviceKeys[] = {
    "ro.product.brand",
    "ro.product.device",
    "ro.build.product",
    "ro.serialno",
    "",
    "ro.product.manufacturer",
    "ro.product.model",
    NULL,
  };
  uint8_t len = 0;
  const char **key = &kDeviceKeys[0];
  do {
    if (strlen(*key) == 0) {
      len = static_cast<uint8_t>(modem_id.length());
      device_data->push_back(len);
      device_data->append(modem_id);
    } else {
      char value[PROPERTY_VALUE_MAX];
      //  Thanks C11 for continguous string allocations!
      if (property_get(*key, &value[0], NULL) == 0) {
        fprintf(stderr, "Property '%s' is empty!\n", *key);
        return false;
      }
      len = static_cast<uint8_t>(strlen(value));
      device_data->push_back(len);
      device_data->append(value);
    }
    if (*++key == NULL) {
      break;
    }
  } while (*key != NULL);
  return true;
}

int handle_production(struct EseBootSession *session, std::vector<std::string> &args) {
  EseAppResult res;
  if (args[1] != "set") {
    fprintf(stderr, "production: unknown command '%s'\n", args[2].c_str());
    return -1;
  }
  if (args.size() < 3) {
    fprintf(stderr, "production: not enough arguments\n");
    return -1;
  }
  bool prod = false;
  if (args[2] == "true") {
    prod = true;
  } else if (args[2] == "false") {
    prod = false;
  } else {
    fprintf(stderr, "production: must be 'true' or 'false'\n");
    return -1;
  }
  res = ese_boot_set_production(session, prod);
  if (res == ESE_APP_RESULT_OK) {
    printf("production mode changed\n");
    return 0;
  }
  fprintf(stderr, "production: failed to change\n");
  return 1;
}

int handle_state(struct EseBootSession *session, std::vector<std::string> &args) {
  EseAppResult res;
  if (args[1] != "get") {
    fprintf(stderr, "state: unknown command '%s'\n", args[2].c_str());
    return -1;
  }
  // Read in the hex unlockToken and hope for the best.
  std::vector<uint8_t> data;
  data.resize(8192);
  uint16_t len = static_cast<uint16_t>(data.size());
  res = ese_boot_get_state(session, data.data(), len);
  if (res != ESE_APP_RESULT_OK) {
    fprintf(stderr, "state: failed (%d)\n", res);
    return 1;
  }
  // TODO: ese_boot_get_state should guarantee length is safe...
  len = (data[1] << 8) | (data[2]) + 3;
  printf("Boot Storage State:\n    ");
  for (int i = 3; i < len; ++i) {
    if (i % 20 == 0) {
      printf("\n    ");
    }
    printf("%.2x ", data[i]);
  }
  printf("\n\n");
  return 0;
}


int handle_verify_key(struct EseBootSession *session, std::vector<std::string> &args) {
  EseAppResult res;
  if (args[2] != "test") {
    fprintf(stderr, "verify-key: unknown command '%s'\n", args[2].c_str());
    return -1;
  }
  // Read in the hex unlockToken and hope for the best.
  std::vector<uint8_t> data;
  uint16_t len = hexify(args[3], &data);
  printf("Passing an test blob of length %hu to the eSE\n", len);
  res = ese_boot_carrier_lock_test(session, data.data(), len);
  if (res == ESE_APP_RESULT_OK) {
    printf("verified\n");
    return 0;
  }
  printf("failed to verify\n");
  return 1;
}

int handle_lock_state(struct EseBootSession *session, std::vector<std::string> &args) {
  EseAppResult res;
  EseBootLockId lockId;
  if (args[2] == "carrier") {
    lockId = kEseBootLockIdCarrier;
  } else if (args[2] == "device") {
    lockId = kEseBootLockIdDevice;
  } else if (args[2] == "boot") {
    lockId = kEseBootLockIdBoot;
  } else if (args[2] == "owner") {
    lockId = kEseBootLockIdOwner;
  } else {
    fprintf(stderr, "lock: unknown lock '%s'\n", args[2].c_str());
    return 1;
  }

  if (args[1] == "get") {
    uint8_t lockVal = 0;
    res = ese_boot_lock_get(session, lockId, &lockVal);
    if (res == ESE_APP_RESULT_OK) {
      printf("%.2x\n", lockVal);
      return 0;
     }
    fprintf(stderr, "lock: failed to get '%s' (%d)\n", args[2].c_str(), res);
    handle_error(session->ese, res);
    return 2;
  } else if (args[1] == "set") {
    if (args.size() < 4) {
      fprintf(stderr, "lock set: not enough arguments supplied\n");
      return 2;
    }
    uint8_t lockVal = static_cast<uint8_t>(std::stoi(args[3], nullptr, 0));
    if (lockId == kEseBootLockIdCarrier) {
      res = ESE_APP_RESULT_ERROR_UNCONFIGURED;
      if (lockVal != 0) {
        std::string device_data;
        device_data.push_back(lockVal);
        if (!collect_device_data(args[4], &device_data)) {
          fprintf(stderr, "carrier set 1: failed to aggregate device data\n");
          return 3;
        }
        printf("Setting carrier lock with '");
        for (std::string::iterator it = device_data.begin();
             it != device_data.end(); ++it) {
          printf("%c", isprint(*it) ? *it : '_');
        }
        printf("'\n");
        const uint8_t *data = reinterpret_cast<const uint8_t *>(device_data.data());
        res = ese_boot_lock_xset(session, lockId, data, device_data.length());
      } else {
        // Read in the hex unlockToken and hope for the best.
        std::vector<uint8_t> data;
        data.push_back(lockVal);
        uint16_t len = hexify(args[4], &data);
        printf("Passing an unlockToken of length %hu to the eSE\n", len);
        res = ese_boot_lock_xset(session, lockId, data.data(), len);
      }
    } else if (lockId == kEseBootLockIdOwner && lockVal != 0) {
      std::vector<uint8_t> data;
      data.push_back(lockVal);
      uint16_t len = hexify(args[4], &data);
      res = ese_boot_lock_xset(session, lockId, data.data(), len);
    } else {
      res = ese_boot_lock_set(session, lockId, lockVal);
    }
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "lock: failed to set %s state\n", args[2].c_str());
      handle_error(session->ese, res);
      return 4;
    }
    return 0;
  }
  fprintf(stderr, "lock: invalid command\n");
  return -1;
}

int handle_rollback(struct EseBootSession *session, std::vector<std::string> &args) {
  int index = std::stoi(args[2], nullptr, 0);
  uint8_t slot = static_cast<uint8_t>(index & 0xff);
  if (slot > 7) {
    fprintf(stderr, "rollback: slot must be one of [0-7]\n");
    return 2;
  }

  uint64_t value = 0;
  if (args.size() > 3) {
    unsigned long long conv = std::stoull(args[3], nullptr, 0);
    value = static_cast<uint64_t>(conv);
  }

  EseAppResult res;
  if (args[1] == "get") {
    res = ese_boot_rollback_index_read(session, slot, &value);
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "rollback: failed to read slot %2x: %d\n",
              slot, res);
      handle_error(session->ese, res);
      return 3;
    }
    printf("%" PRIu64 "\n", value);
    return 0;
  } else if (args[1] == "set") {
    res = ese_boot_rollback_index_write(session, slot, value);
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "rollback: failed to write slot %2x: %d\n",
              slot, res);
      handle_error(session->ese, res);
      return 4;
    }
    return 0;
  }
  fprintf(stderr, "rollback: unknown command '%s'\n", args[1].c_str());
  return -1;
}

int handle_args(struct EseBootSession *session, const char *prog, std::vector<std::string> &args) {
  if (args[0] == "rollback") {
    return handle_rollback(session, args);
  } else if (args[0] == "lock") {
    return handle_lock_state(session, args);
  } else if (args[0] == "verify-key") {
    return handle_verify_key(session, args);
  } else if (args[0] == "production") {
    return handle_production(session, args);
  } else if (args[0] == "state") {
    return handle_state(session, args);
  } else {
    usage(prog);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage(argv[0]);
    return 1;
  }
  // TODO(wad): move main to a class so we can just dep inject the hw.
  struct EseInterface ese = ESE_INITIALIZER(ESE_HW_NXP_PN80T_NQ_NCI);
  ese_open(&ese, nullptr);
  EseBootSession session;
  ese_boot_session_init(&session);
  EseAppResult res = ese_boot_session_open(&ese, &session);
  if (res != ESE_APP_RESULT_OK) {
    fprintf(stderr, "failed to initiate session: %d\n", res);
    handle_error(ese, res);
    return 1;
  }
  std::vector<std::string> args;
  args.assign(argv + 1, argv + argc);
  int ret = handle_args(&session, argv[0], args);

  res = ese_boot_session_close(&session);
  if (res != ESE_APP_RESULT_OK) {
    handle_error(&ese, res);
  }
  ese_close(&ese);
  return ret;
}
