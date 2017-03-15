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

#define LOG_TAG "ese-replay"
#include <ese/ese.h>
#include <ese/log.h>

#include "hw.h"

const struct SupportedHardware kSupportedHardware = {
    .len = 3,
    .hw =
        {
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

struct Buffer {
  uint32_t len;
  uint32_t size;
  uint8_t *buffer;
};

struct Payload {
  struct Buffer tx;
  struct Buffer expected;
};

bool buffer_init(struct Buffer *b, uint32_t len) {
  if (!b)
    return false;
  b->buffer = calloc(len, 1);
  if (!b->buffer)
    return false;
  b->len = 0;
  b->size = len;
  return true;
}

void buffer_free(struct Buffer *b) {
  if (b && b->buffer)
    free(b->buffer);
}

bool payload_init(struct Payload *p, uint32_t tx_len, uint32_t exp_len) {
  if (!p)
    return false;
  if (!buffer_init(&p->tx, tx_len))
    return false;
  if (buffer_init(&p->expected, exp_len))
    return true;
  buffer_free(&p->tx);
  return false;
}

void payload_free(struct Payload *p) {
  if (p) {
    buffer_free(&p->tx);
    buffer_free(&p->expected);
  }
}

bool buffer_read_hex(struct Buffer *b, FILE *fp, bool consume_newline) {
  int offset;
  uint32_t i;

  if (feof(fp))
    return false;
  /* Consume leading whitespace. */
  while (!feof(fp)) {
    char c = fgetc(fp);
    if (!consume_newline && (c == 0x0d || c == 0x0a)) {
      return false;
    }
    if (!isspace(c)) {
      ungetc(c, fp);
      break;
    }
  }

  for (; !feof(fp) && b->len < b->size; ++b->len) {
    b->buffer[b->len] = fgetc(fp);
    if (!isalnum(b->buffer[b->len])) {
      ungetc(b->buffer[b->len], fp);
      break;
    }
  }
  if (b->len == 0)
    return false;

  for (offset = 0, i = 0; offset < (int)b->len && i < b->size; ++i) {
    int last_offset = offset;
    if (sscanf((char *)b->buffer + offset, "%2hhx%n", &b->buffer[i], &offset) !=
        1)
      break;
    offset += last_offset;
    if (offset == 0)
      break;
  }
  b->len = i;
  return true;
}

bool payload_read(struct Payload *p, FILE *fp) {
  if (!p)
    return false;
  p->tx.len = 0;
  p->expected.len = 0;
  while (buffer_read_hex(&p->tx, fp, true) && p->tx.len == 0)
    ;
  if (p->tx.len == 0)
    return false;
  if (!buffer_read_hex(&p->expected, fp, false)) {
    p->expected.buffer[0] = 0x90;
    p->expected.buffer[1] = 0x00;
    p->expected.len = 2;
  }
  return true;
}

void buffer_dump(const struct Buffer *b, const char *prefix, const char *name,
                 uint32_t limit, FILE *fp) {
  fprintf(fp, "%s%s {\n", prefix, name);
  fprintf(fp, "%s  .length = %u\n", prefix, b->len);
  fprintf(fp, "%s  .size = %u\n", prefix, b->size);
  fprintf(fp, "%s  .buffer = {\n", prefix);
  fprintf(fp, "%s    ", prefix);
  for (uint32_t i = 0; i < b->len; ++i) {
    if (i > 15 && i % 16 == 0) {
      fprintf(fp, "\n%s    ", prefix);
    }
    if (i > limit) {
      fprintf(fp, ". . .");
      break;
    }
    fprintf(fp, "%.2x ", b->buffer[i]);
  }
  fprintf(fp, "\n%s}\n", prefix);
}

void payload_dump(const struct Payload *payload, FILE *fp) {
  fprintf(fp, "Payload {\n");
  buffer_dump(&payload->tx, "  ", "Transmit", 240, fp);
  buffer_dump(&payload->expected, "  ", "Expected", 240, fp);
  fprintf(fp, "}\n");
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
  struct Payload payload;
  if (!payload_init(&payload, 10 * 1024 * 1024, 1024 * 4)) {
    ALOGE("Failed to initialize payload.");
    return -1;
  }
  while (!feof(stdin) && payload_read(&payload, stdin)) {
    struct Buffer reply;
    uint8_t rx_buf[1536];
    reply.buffer = rx_buf;
    reply.size = sizeof(rx_buf);
    reply.len = 0;

    payload_dump(&payload, stdout);
    reply.len = (uint32_t)ese_transceive(
        &ese, payload.tx.buffer, payload.tx.len, reply.buffer, reply.size);
    if ((int)reply.len < 0 || ese_error(&ese)) {
      ALOGE("transceived returned failure: %d\n", (int)reply.len);
      if (ese_error(&ese)) {
        ALOGE("An error (%d) occurred: %s", ese_error_code(&ese),
              ese_error_message(&ese));
      }
      break;
    }
    buffer_dump(&reply, "", "Response", 240, stdout);
    if (reply.len < payload.expected.len) {
      printf("Received less data than expected: %u < %u\n", reply.len,
             payload.expected.len);
      break;
    }

    /* Only compare the end. This allows a simple APDU success match. */
    if (memcmp(payload.expected.buffer,
               (reply.buffer + reply.len) - payload.expected.len,
               payload.expected.len)) {
      printf("Response did not match. Aborting!\n");
      break;
    }
  }
  printf("Transmissions complete.\n");
  ese_close(&ese);
  release_hardware(hw);
  return 0;
}
