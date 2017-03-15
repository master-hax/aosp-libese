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
 * Usage:
 *   $0 [impl name] < line-by-line-input-and-expect
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "ese-sh"
#include <ese/ese.h>
#include <ese/log.h>

#include "hw.h"

const struct SupportedHardware kSupportedHardware = {
  .len = 3,
  .hw = {
    {
      .name = "nq-nci",
      .sym = "ESE_HW_NXP_PN80T_NQ_NCI_ops",
      .lib = "libese-hw-nxp-pn80t-nq-nci.so",
      .options = NULL,
    },
    {
      .name = "fake",
      .sym = "ESE_HW_FAKE_ops",
      .lib = "libese-hw-fake.so",
      .options = NULL,
    },
    {
      .name = "echo",
      .sym = "ESE_HW_ECHO_ops",
      .lib = "libese-hw-echo.so",
      .options = NULL,
    },
  },
};

struct Payload {
  uint32_t tx_len;
  uint32_t rx_len;
  uint32_t max;
  uint8_t *buffer;
};

bool payload_from_stdin(struct Payload *p) {
  uint32_t bin_len = 0;
  uint8_t *expected = NULL;
  char *io_buffer = NULL;
  size_t line_len = 0;
  ssize_t read_len;

  read_len = getline(&io_buffer, &line_len, stdin);
  if (read_len == -1) {
    return false;
  }
  if (p->max < read_len) {
    printf("Allocating payload buffer (%zu)\n", line_len);
    p->max = line_len;
    p->buffer = realloc(p->buffer, p->max);
    if (!p->buffer) {
      printf("Not enough mem\n");
      free(io_buffer);
      return false;
    }
  }
  /* Translated io_buffer -> bin */
  char *cur_ascii = io_buffer;
  uint8_t *cur_bin = p->buffer;
  p->tx_len = 0;
  p->rx_len = 0;
  while (*cur_ascii && *(cur_ascii + 1)) {
    char hex[3] = { *cur_ascii, *(cur_ascii + 1), 0};
    if (isspace(*cur_ascii)) {
      cur_ascii++;
      /* Treat spaces as delineation between tx and expected trailing rx bytes. */
      expected = cur_bin;
      continue;
    }
    /* Just ignore non-space, non-alnum. */
    if (!isalnum(*cur_ascii)) {
      cur_ascii++;
      continue;
    }
    /* If the hex is unbalanced, this will miss the reply. */
    cur_ascii += 2;
    unsigned long val = strtoul(hex, NULL, 16);
    if (val > 255) {
      fprintf(stderr, "Input error: %s\n", hex);
      free(io_buffer);
      return false;
    }
    *cur_bin++ = (uint8_t)val;
    bin_len++;
  }
  /* Done. */
  if (bin_len == 0) {
    free(io_buffer);
    return false;
  }
  if (expected == NULL) {
    /* Assume 9000 */
    expected = cur_bin;
    *cur_bin++ = 0x90;
    bin_len++;
    *cur_bin++ = 0x00;
    bin_len++;
  }
  p->tx_len = bin_len - (cur_bin - expected);
  p->rx_len = (cur_bin - expected);
  free(io_buffer);
  return true;
}

void payload_print(const struct Payload *payload) {
  printf("Payload {\n");
  printf("  TX length: %u\n", payload->tx_len);
  printf("  Expected RX length: %u\n", payload->rx_len);
  if (payload->tx_len > 1536 || payload->rx_len > 160) {
    printf("  ...\n}\n");
    return;
 }
  printf("  TX payload:\n    ");
  for (uint32_t i = 0; i < payload->tx_len; ++i)
    printf("%.2x ", payload->buffer[i]);
  printf("\n");
  printf("  Expected response:\n    ");
  for (uint32_t i = payload->tx_len; i - payload->tx_len < payload->rx_len; ++i)
    printf("%.2x ", payload->buffer[i]);
  printf("\n}\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage:\n%s [hw_impl] < file_with_apdus\n\n"
           "File format:\n"
           "  hex-apdu-to-send hex-trailing-response-bytes\\n\n"
           "\n"
           "For example,\n"
           "  echo -e '00A4040000 9000\\n80CA9F7F00 9000\\n' | %s nq-nci\n",
            argv[0], argv[0]);
    print_supported_hardware(&kSupportedHardware);
    return 1;
  }
  int hw_id = find_supported_hardware(&kSupportedHardware, argv[1]);
  if (hw_id < 0) {
    fprintf(stderr, "Unknown hardware name: %s\n", argv[1]);
    return 3;
  }
  const struct Hardware *hw = &kSupportedHardware.hw[hw_id];

  struct EseInterface ese;
  printf("[-] Initializing eSE\n");

  if (!initialize_hardware(&ese, hw)) {
    fprintf(stderr, "Could not initialize hardware\n");
    return 2;
  }
  printf("eSE implementation selected: %s\n", ese_name(&ese));
  if (ese_open(&ese, hw->options)) {
    ALOGE("Cannot open hw");
    if (ese_error(&ese)) {
      ALOGE("eSE error (%d): %s", ese_error_code(&ese),
            ese_error_message(&ese));
    }
    return 5;
  }
  printf("eSE is open\n");
  struct Payload *payload = malloc(sizeof(struct Payload));
  payload->max = 0;
  payload->buffer = NULL;
  while (!feof(stdin)) {
    uint8_t rx_buf[1536];
    int rx_len = 0;
    if (!payload_from_stdin(payload)) {
      printf("No more data available\n");
      break;
    }
    payload_print(payload);
    rx_len = ese_transceive(&ese, payload->buffer, payload->tx_len, rx_buf, sizeof(rx_buf));
    if (rx_len < 0 || ese_error(&ese)) {
      printf("transceived returned failure: %d\n", rx_len);
      if (ese_error(&ese)) {
        fprintf(stderr, "An error (%d) occurred: %s", ese_error_code(&ese),
              ese_error_message(&ese));
        ALOGE("An error (%d) occurred: %s", ese_error_code(&ese),
              ese_error_message(&ese));
      }
      break;
    }
    printf("Received: \n");
    for (int i = 0; i < rx_len; ++i) {
      printf("%.2x ", rx_buf[i]);
    }
    printf("\n");
    if ((uint32_t)rx_len < payload->rx_len) {
      printf("Received less data than expected: %u < %u\n", rx_len, payload->rx_len);
      break;
    }

    /* Only compare the end. This allows a simple APDU success match. */
    if (memcmp(payload->buffer + payload->tx_len, (rx_buf + rx_len) - payload->rx_len, payload->rx_len)) {
      printf("Response was not as expected. Aborting.\n");
      break;
    }
    printf("\n");
    printf("Response matched! Continuing...\n");
  }
  printf("Transmissions complete.\n");
  ese_close(&ese);
  release_hardware(hw);
  return 0;
}
