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
    "    rollback set <іndex> <value>\n"
    "             get <іndex>\n"
    "    owner-key get <length in bytes>\n"
    "              set <value>\n"
    "    lock-state get {provision,carrier,device,boot,owner}\n"
    "               set provision {0|1}\n"
    "                   carrier 0 <unlockToken>\n"
    "                           1 {IMEI,MEID}\n"
    "                   device {0,1}\n"
    "                   boot {0,1}\n"
    "                   owner {0,1}\n"
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
  static const char kSep = 0;
  const char **key = &kDeviceKeys[0];
  do {
    if (strlen(*key) == 0) {
      device_data->append(modem_id);
      device_data->push_back(kSep);
      continue;
    }
    std::string value;
    value.reserve(PROPERTY_VALUE_MAX);
    //  Thanks C11 for continguous string allocations!
    if (property_get(*key, &value[0], NULL) == 0) {
      fprintf(stderr, "Property '%s' is empty!\n", *key);
      return false;
    }
    device_data->append(value);
    if (*++key == NULL) {
      break;
    }
    device_data->push_back(kSep);
  } while (*key != NULL);
  return true;
}


int handle_lock_state(struct EseBootSession *session, std::vector<std::string> &args) {
  EseAppResult res;
  if (args[1] == "get") {
    if (args[2] == "provision") {
      res = ese_boot_is_provisioned(session);
    } else if (args[2] == "carrier") {
      res = ese_boot_is_carrier_locked(session);
    } else if (args[2] == "device") {
      res = ese_boot_is_device_locked(session);
    } else if (args[2] == "boot") {
      res = ese_boot_is_boot_locked(session);
    } else if (args[2] == "owner") {
      res = ese_boot_is_owner_locked(session);
    } else {
      fprintf(stderr, "lock-state: unknown state '%s'\n", args[2].c_str());
      return 1;
    }
    if (res == ESE_APP_RESULT_TRUE) {
      printf("true\n");
      return 0;
     }
    if (res == ESE_APP_RESULT_FALSE) {
      printf("false\n");
      return 0;
    }
    fprintf(stderr, "lock-state: failed to get %s state\n", args[2].c_str());
    handle_error(session->ese, res);
    return 2;
  } else if (args[1] == "set") {
    if (args.size() < 4) {
      fprintf(stderr, "lock-state set: not enough arguments supplied\n");
      return 2;
    }
    int enable = std::stoi(args[3]);
    if (args[2] == "provision") {
      if (enable) {
        res = ese_boot_provision(session);
      } else {
        res = ese_boot_unprovision(session);
      }
    } else if (args[2] == "carrier") {
      res = ESE_APP_RESULT_ERROR_UNCONFIGURED;
      if (enable) {
        std::string device_data;
        if (!collect_device_data(args[4], &device_data)) {
          fprintf(stderr, "carrier set 1: failed to aggregate device data\n");
          return 3;
        }
        printf("Setting carrier lock with '");
        for (std::string::iterator it = device_data.begin();
             it != device_data.end(); ++it) {
          printf("%c", *it == 0 ? '|': *it);
        }
        printf("'\n");
        const uint8_t *data = reinterpret_cast<const uint8_t *>(device_data.data());
        res = ese_boot_carrier_lock(session, data, device_data.length());
      } else {
        // TODO(wad): Check format.
        const uint8_t *data = reinterpret_cast<const uint8_t *>(args[4].data());
        res = ese_boot_carrier_unlock(session, data, args[4].length());
      }
    } else if (args[2] == "device") {
      res = ese_boot_set_device_lock(session, enable);
    } else if (args[2] == "boot") {
      res = ese_boot_set_boot_lock(session, enable);
    } else if (args[2] == "owner") {
      res = ese_boot_set_owner_lock(session, enable);
    } else {
      fprintf(stderr, "lock-state: unknown state '%s'\n", args[2].c_str());
      return 3;
    }
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "lock-state: failed to set %s state\n", args[2].c_str());
      handle_error(session->ese, res);
      return 4;
    }
    return 0;
  }
  fprintf(stderr, "lock-state: invalid command\n");
  return -1;
}

int handle_rollback(struct EseBootSession *session, std::vector<std::string> &args) {
  int index = std::stoi(args[2], nullptr, 0);
  uint8_t location = static_cast<uint8_t>(index & 0xff);
  if (location > 7) {
    fprintf(stderr, "rollback: location must be one of [0-7]\n");
    return 2;
  }

  uint64_t value = 0;
  if (args.size() > 3) {
    unsigned long long conv = std::stoull(args[3], nullptr, 0);
    value = static_cast<uint64_t>(conv);
  }

  EseAppResult res;
  if (args[1] == "get") {
    res = ese_boot_rollback_index_read(session, location, &value);
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "rollback: failed to read location %2x: %d\n",
              location, res);
      handle_error(session->ese, res);
      return 3;
    }
    printf("%" PRIu64 "\n", value);
    return 0;
  } else if (args[1] == "set") {
    res = ese_boot_rollback_index_write(session, location, value);
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "rollback: failed to write location %2x: %d\n",
              location, res);
      handle_error(session->ese, res);
      return 4;
    }
    return 0;
  }
  fprintf(stderr, "rollback: unknown command '%s'\n", args[1].c_str());
  return -1;
}

int handle_owner_key(struct EseBootSession *session, std::vector<std::string> &args) {
  if (args.size() != 3) {
    fprintf(stderr, "owner-key: incorrect number of arguments supplied\n");
    return 1;
  }

  if (args[1] == "get") {
    int length_i = std::stoi(args[2], nullptr, 10);
    uint16_t length = static_cast<uint16_t>(length_i & 0xffff);
    std::vector<uint8_t> data(length);
    EseAppResult res = ese_boot_owner_key_read(session, data.data(), length);
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "owner-key: failed to read key: %d\n", res);
      handle_error(session->ese, res);
      return 2;
    }
    for (std::vector<uint8_t>::iterator it = data.begin(); it != data.end(); it++) {
      printf("%2x", *it);
    }
    return 0;
  } else if (args[1] == "set") {
    std::vector<uint8_t> data;
    for (std::string::iterator it = args[2].begin(); it != args[2].end(); ++it) {
      std::string hex;
      hex.push_back(*it++);
      hex.push_back(*it);
      printf("XXX -- %s\n", hex.c_str());
      data.push_back(static_cast<uint8_t>(std::stoi(hex, nullptr, 16)));
    }
    uint16_t length = static_cast<uint16_t>(data.size() & 0xffff);
    EseAppResult res = ese_boot_owner_key_store(session, data.data(), length);
    if (res != ESE_APP_RESULT_OK) {
      fprintf(stderr, "owner-key: failed to write key: %d\n", res);
      handle_error(session->ese, res);
      return 2;
    }
    return 0;
  }
  fprintf(stderr, "owner-key: unknown command '%s'\n", args[1].c_str());
  return -1;
}

int handle_args(struct EseBootSession *session, const char *prog, std::vector<std::string> &args) {
  if (args[0] == "rollback") {
    return handle_rollback(session, args);
  } else if (args[0] == "owner-key") {
    return handle_owner_key(session, args);
  } else if (args[0] == "lock-state") {
    return handle_lock_state(session, args);
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
