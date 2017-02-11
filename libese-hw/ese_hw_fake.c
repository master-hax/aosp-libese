/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * Minimal functions that only validate arguments.
 */

#include "../libese/include/ese/ese.h"

enum EseFakeHwError {
  kEseFakeHwErrorEarlyClose,
  kEseFakeHwErrorReceiveDuringTransmit,
  kEseFakeHwErrorInvalidReceiveSize,
  kEseFakeHwErrorTransmitDuringReceive,
  kEseFakeHwErrorInvalidTransmitSize,
  kEseFakeHwErrorTranscieveWhileBusy,
  kEseFakeHwErrorEmptyTransmit,
  kEseFakeHwErrorMax,
};

static const char *kErrorMessages[] = {
    "Interface closed without finishing transmission.",
    "Receive called without completing transmission.",
    "Invalid receive buffer supplied with non-zero length.",
    "Transmit called without completing reception.",
    "Invalid transmit buffer supplied with non-zero length.",
    "Transceive called while other I/O in process.",
    "Transmitted no data.", /* Can reach this by setting tx_len = 0. */
};

static int fake_open(struct EseInterface *ese,
                     void *hw_opts __attribute__((unused))) {
  ese->pad[0] = 1; /* rx complete */
  ese->pad[1] = 1; /* tx complete */
  return 0;
}

static void fake_close(struct EseInterface *ese) {
  if (!ese->pad[0] || !ese->pad[1]) {
    /* Set by caller. ese->error.is_error = 1; */
    ese_set_error(ese, kEseFakeHwErrorEarlyClose);
    return;
  }
}

static uint32_t fake_receive(struct EseInterface *ese, uint8_t *buf,
                             uint32_t len, int complete) {
  if (!ese->pad[1]) {
    ese_set_error(ese, kEseFakeHwErrorReceiveDuringTransmit);
    return -1;
  }
  ese->pad[0] = complete;
  if (!buf && len) {
    ese_set_error(ese, kEseFakeHwErrorInvalidReceiveSize);
    return -1;
  }
  if (!len) {
    return 0;
  }
  return len;
}

static uint32_t fake_transmit(struct EseInterface *ese, const uint8_t *buf,
                              uint32_t len, int complete) {
  if (!ese->pad[0]) {
    ese_set_error(ese, kEseFakeHwErrorTransmitDuringReceive);
    return -1;
  }
  ese->pad[1] = complete;
  if (!buf && len) {
    ese_set_error(ese, kEseFakeHwErrorInvalidTransmitSize);
    return -1;
  }
  if (!len) {
    return 0;
  }
  return len;
}

static int fake_poll(struct EseInterface *ese, uint8_t poll_for, float timeout,
                     int complete) {
  /* Poll begins a receive-train so transmit needs to be completed. */
  if (!ese->pad[1]) {
    ese_set_error(ese, kEseFakeHwErrorReceiveDuringTransmit);
    return -1;
  }
  if (timeout == 0.0f) {
    /* Instant timeout. */
    return 0;
  }
  /* Only expect one value to work. */
  if (poll_for == 0xad) {
    return 1;
  }
  ese->pad[0] = complete;
  return 0;
}

uint32_t fake_transceive(struct EseInterface *ese, const uint8_t *tx_buf,
                         uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len) {
  uint32_t processed = 0;
  if (!ese->pad[0] || !ese->pad[1]) {
    ese_set_error(ese, kEseFakeHwErrorTranscieveWhileBusy);
    return 0;
  }
  while (processed < tx_len) {
    uint32_t sent = fake_transmit(ese, tx_buf, tx_len, 0);
    if (sent == 0) {
      if (ese->error.is_err) {
        return 0;
      }
      ese_set_error(ese, kEseFakeHwErrorEmptyTransmit);
      return 0;
    }
    processed += sent;
  }
  fake_transmit(ese, NULL, 0, 1); /* Complete. */
  if (fake_poll(ese, 0xad, 10, 0) != 1) {
    ese_set_error(ese, kEseGlobalErrorPollTimedOut);
    return 0;
  }
  /* A real implementation would have protocol errors to contend with. */
  processed = fake_receive(ese, rx_buf, rx_len, 1);
  return processed;
}

static const struct EseOperations ops = {
    .name = "eSE Fake Hardware",
    .open = &fake_open,
    .hw_receive = &fake_receive,
    .hw_transmit = &fake_transmit,
    .transceive = &fake_transceive,
    .poll = &fake_poll,
    .close = &fake_close,
    .opts = NULL,
    .errors = kErrorMessages,
    .errors_count = sizeof(kErrorMessages),
};
ESE_DEFINE_HW_OPS(ESE_HW_FAKE, ops);

