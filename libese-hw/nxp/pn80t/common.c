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
 * Support SPI communication with NXP PN553/PN80T secure element.
 */

#include "include/ese/hw/nxp/pn80t/common.h"

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

int nxp_pn80t_preprocess(const struct Teq1ProtocolOptions *const opts,
                         struct Teq1Frame *frame, int tx) {
  if (tx) {
    /* Recompute the LRC with the NAD of 0x00 */
    frame->header.NAD = 0x00;
    frame->INF[frame->header.LEN] = teq1_compute_LRC(frame);
    frame->header.NAD = opts->node_address;
    ALOGV("interface is preprocessing outbound frame");
  } else {
    /* Replace the NAD with 0x00 so the LRC check passes. */
    ALOGV("interface is preprocessing inbound frame (%x->%x)",
          frame->header.NAD, 0x00);
    if (frame->header.NAD != opts->host_address) {
      ALOGV("Rewriting from unknown NAD: %x", frame->header.NAD);
    }
    frame->header.NAD = 0x00;
    ALOGV("Frame length: %x", frame->header.LEN);
  }
  return 0;
}

static const struct Teq1ProtocolOptions kTeq1Options = {
    .host_address = 0xA5,
    .node_address = 0x5A,
    .bwt = 1.624f,   /* cwt by default would be ~8k * 1.05s */
    .etu = 0.00105f, /* seconds */
    .preprocess = &nxp_pn80t_preprocess,
};

int nxp_pn80t_open(struct EseInterface *ese, void *board) {
  struct NxpState *ns;
  const struct Pn80tPlatform *platform;
  if (sizeof(ese->pad) < sizeof(struct NxpState *)) {
    /* This is a compile-time correctable error only. */
    ALOGE("Pad size too small to use NXP HW (%zu < %zu)", sizeof(ese->pad),
          sizeof(struct NxpState));
    return -1;
  }
  platform = ese->ops->opts;

  /* Ensure all required functions exist */
  if (!platform->initialize || !platform->release || !platform->toggle_reset ||
      !platform->wait) {
    ALOGE("Required functions not implemented in supplied platform");
    return -1;
  }

  ns = NXP_PN80T_STATE(ese);
  TEQ1_INIT_CARD_STATE((struct Teq1CardState *)(&ese->pad[0]));
  ns->handle = platform->initialize(board);
  if (!ns->handle) {
    ALOGE("platform initialization failed");
    ese_set_error(ese, kNxpPn80tErrorPlatformInit);
    return -1;
  }
  /* Toggle all required power GPIOs.
   * Each platform may prefer to handle the power
   * muxing specific. E.g., if NFC is in use, it would
   * be unwise to unset VEN.  However, the implementation
   * here will attempt it if supported.
   */
  if (platform->toggle_ven) {
    platform->toggle_ven(ns->handle, 1);
  }
  if (platform->toggle_power_req) {
    platform->toggle_power_req(ns->handle, 1);
  }
  /* Power on eSE */
  platform->toggle_reset(ns->handle, 1);
  /* Let the eSE boot. */
  platform->wait(ns->handle, 5000);
  return 0;
}

int nxp_pn80t_reset(struct EseInterface *ese) {
  const struct Pn80tPlatform *platform = ese->ops->opts;
  struct NxpState *ns = NXP_PN80T_STATE(ese);
  if (platform->toggle_reset(ns->handle, 0) < 0) {
    ese_set_error(ese, kNxpPn80tErrorResetToggle);
    return -1;
  }
  platform->wait(ns->handle, 1000);
  if (platform->toggle_reset(ns->handle, 1) < 0) {
    ese_set_error(ese, kNxpPn80tErrorResetToggle);
    return -1;
  }
  return 0;
}

int nxp_pn80t_poll(struct EseInterface *ese, uint8_t poll_for, float timeout,
                   int complete) {
  struct NxpState *ns = NXP_PN80T_STATE(ese);
  const struct Pn80tPlatform *platform = ese->ops->opts;
  /* Attempt to read a 8-bit character once per 8-bit character transmission
   * window (in seconds). */
  int intervals = (int)(0.5f + timeout / (7.0f * kTeq1Options.etu));
  uint8_t byte = 0xff;
  ALOGV("interface polling for start of frame/host node address: %x", poll_for);
  /* If we had interrupts, we could just get notified by the driver. */
  do {
    /*
     * In practice, if complete=true, then no transmission
     * should attempt again until after 1000usec.
     */
    if (ese->ops->hw_receive(ese, &byte, 1, complete) != 1) {
      ALOGE("failed to read one byte");
      ese_set_error(ese, kNxpPn80tErrorPollRead);
      return -1;
    }
    if (byte == poll_for) {
      ALOGV("Polled for byte seen: %x with %d intervals remaining.", poll_for,
            intervals);
      ALOGV("RX[0]: %.2X", byte);
      return 1;
    } else {
      ALOGV("No match (saw %x)", byte);
    }
    platform->wait(ns->handle,
                   7.0f * kTeq1Options.etu * 1000000.0f); /* s -> us */
    ALOGV("poll interval %d: no match.", intervals);
  } while (intervals-- > 0);
  ALOGW("polling timed out.");
  return -1;
}

uint32_t nxp_pn80t_transceive(struct EseInterface *ese,
                              const struct EseSgBuffer *tx_buf, uint32_t tx_len,
                              struct EseSgBuffer *rx_buf, uint32_t rx_len) {
  return teq1_transceive(ese, &kTeq1Options, tx_buf, tx_len, rx_buf, rx_len);
}

/* Returns the minutes needed to decrement the attack counter. */
uint32_t nxp_pn80t_send_cooldown(struct EseInterface *ese) {
  const struct Pn80tPlatform *platform = ese->ops->opts;
  const static uint8_t kEndofApduSession[] = {0x5a, 0xc5, 0x00, 0xc5};
  uint8_t rx_buf[32];
  uint32_t bytes_read = 0;
  uint32_t *secure_timer = NULL;
  uint32_t *attack_counter_decrement = NULL;
  uint32_t *restricted_mode_penalty = NULL;
  ese->ops->hw_transmit(ese, kEndofApduSession, sizeof(kEndofApduSession), 1);
  nxp_pn80t_poll(ese, kTeq1Options.host_address, 5.0f, 0);
  bytes_read = ese->ops->hw_receive(ese, rx_buf, sizeof(rx_buf), 1);
  ALOGI("End of APDU Session:");
  if (bytes_read >= 0x8 && rx_buf[0] == 0xe5 && rx_buf[1] == 0x12) {
    uint8_t *tag = &rx_buf[2];
    while (tag < (rx_buf + bytes_read)) {
      uint8_t *tag_len = tag + 1;
      switch (*tag) {
      case 0xf1:
        if (*tag_len == 4) {
          secure_timer = (uint32_t *)(tag + 2);
        }
        tag += *tag_len + 1;
        break;
      case 0xf2:
        if (*tag_len == 4) {
          attack_counter_decrement = (uint32_t *)(tag + 2);
        }
        tag += *tag_len + 1;
        break;
      case 0xf3:
        if (*tag_len == 4) {
          restricted_mode_penalty = (uint32_t *)(tag + 2);
        }
        tag += *tag_len + 1;
        break;
      default:
        tag += 1;
      }
    }
  }
  if (secure_timer) {
    ALOGI("- Secure Timer (minutes): %d", *secure_timer);
  }
  if (restricted_mode_penalty) {
    ALOGI("- Restricted Mode Penalty (minutes): %d", *restricted_mode_penalty);
  }
  if (attack_counter_decrement) {
    ALOGI("- Attack Counter Decrement (minutes): %d",
          *attack_counter_decrement);
    return *attack_counter_decrement;
  }
  return 0;
}

void nxp_pn80t_close(struct EseInterface *ese) {
  struct NxpState *ns;
  const struct Pn80tPlatform *platform = ese->ops->opts;
  uint32_t wait_min = 0;
  ALOGV("%s: called", __func__);
  ns = NXP_PN80T_STATE(ese);
  wait_min = nxp_pn80t_send_cooldown(ese);
  /* TODO(wad): Move to the non-common code.
   * E.g., platform->wait(ns->handle, 60000000 * wait_min);
   * would not be practical in many cases.
   * In practice, this should probably migrate into the kernel
   * and into the spidev code such that each platform can handle it
   * as needed.
   */
  if (!wait_min) {
    platform->toggle_reset(ns->handle, 0);
    if (platform->toggle_power_req) {
      platform->toggle_power_req(ns->handle, 0);
    }
    if (platform->toggle_ven) {
      platform->toggle_ven(ns->handle, 0);
    }
  }
  platform->release(ns->handle);
  ns->handle = NULL;
}

const char *kNxpPn80tErrorMessages[] = {
    /* The first three are required by teq1_transceive use. */
    TEQ1_ERROR_MESSAGES,
    /* The rest are pn80t impl specific. */
    [kNxpPn80tErrorPlatformInit] = "unable to initialize platform",
    [kNxpPn80tErrorPollRead] = "failed to read one byte",
    [kNxpPn80tErrorReceive] = "failed to read",
    [kNxpPn80tErrorReceiveSize] = "attempted to receive too much data",
    [kNxpPn80tErrorTransmitSize] = "attempted to transfer too much data",
    [kNxpPn80tErrorTransmit] = "failed to transmit",
    [kNxpPn80tErrorResetToggle] = "failed to toggle reset",
};
