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

#include <ese/ese.h>
#include <ese/hw/nxp/pn80t/platform.h>
#include <ese/hw/nxp/spi_board.h>
#include <ese/teq1.h>
#define LOG_TAG "libese-hw"
#include <ese/log.h>

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

/* Card state is _required_ to be at the front of eSE pad. */
struct NxpState {
  struct Teq1CardState card_state;
  struct NxpSpiBoard *board;
  void *handle;
};
#define NXP_PN80T_STATE(ese) ((struct NxpState *)(&ese->pad[0]))

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
    if (frame->header.NAD != opts->host_address)
      ALOGV("Rewriting from unknown NAD: %x", frame->header.NAD);
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
  if (!ese)
    return -1;
  if (sizeof(ese->pad) < sizeof(struct NxpState *)) {
    /* This is a compile-time correctable error only. */
    ALOGE("Pad size too small to use NXP HW (%zu < %zu)", sizeof(ese->pad),
          sizeof(struct NxpState));
    return -1;
  }
  platform = ese->ops->opts;
  ns = NXP_PN80T_STATE(ese);
  TEQ1_INIT_CARD_STATE((&ns->card_state));

  ns->handle = platform->initialize(board);
  if (!ns->handle) {
    ALOGE("platform initialization failed");
    ese_set_error(ese, 4);
    return -1;
  }
  return 0;
}

int nxp_pn80t_close(struct EseInterface *ese) {
  struct NxpState *ns;
  const struct Pn80tPlatform *platform = ese->ops->opts;
  if (!ese)
    return -1;
  ns = NXP_PN80T_STATE(ese);
  platform->toggle_power_req(ns->handle, 0);
  platform->release(ns->handle);
  ns->handle = NULL;
  return 0;
}

size_t nxp_pn80t_receive(struct EseInterface *ese, uint8_t *buf, size_t len,
                         int complete) {
  const struct Pn80tPlatform *platform = ese->ops->opts;
  struct NxpState *ns = NXP_PN80T_STATE(ese);
  size_t recvd = len;
  if (len > INT_MAX) {
    ese_set_error(ese, 6);
    ALOGE("Unexpectedly large receive attempted: %zu", len);
    return 0;
  }
  ALOGV("interface attempting to receive card data");
  if (platform->receive(ns->handle, buf, len, complete) != (int)len) {
    ese_set_error(ese, 5);
    return 0;
  }
  ALOGV("card sent %zu bytes", len);
  for (recvd = 0; recvd < len; ++recvd)
    ALOGV("RX[%zu]: %.2X", recvd, buf[recvd]);
  if (complete) {
    ALOGV("card sent a frame");
    /* XXX: cool off the bus for 1ms [t_3] */
  }
  return recvd;
}

int nxp_pn80t_reset(struct EseInterface *ese) {
  const struct Pn80tPlatform *platform = ese->ops->opts;
  struct NxpState *ns = NXP_PN80T_STATE(ese);
  if (platform->toggle_reset(ns->handle, 0) < 0) {
    ese_set_error(ese, 13);
    return -1;
  }
  platform->wait(ns->handle, 1000);
  if (platform->toggle_reset(ns->handle, 1) < 0) {
    ese_set_error(ese, 13);
    return -1;
  }
  return 0;
}

size_t nxp_pn80t_transmit(struct EseInterface *ese, const uint8_t *buf,
                          size_t len, int complete) {
  struct NxpState *ns = NXP_PN80T_STATE(ese);
  const struct Pn80tPlatform *platform = ese->ops->opts;
  size_t recvd = len;
  ALOGV("interface transmitting data");
  if (len > INT_MAX) {
    ese_set_error(ese, 7);
    ALOGE("Unexpectedly large transfer attempted: %zu", len);
    return 0;
  }

  ALOGV("interface attempting to transmit data");
  for (recvd = 0; recvd < len; ++recvd)
    ALOGV("TX[%zu]: %.2X", recvd, buf[recvd]);
  if (platform->transmit(ns->handle, buf, len, complete) != (int)len) {
    ese_set_error(ese, 8);
    return 0;
  }
  ALOGV("interface sent %zu bytes", len);
  if (complete) {
    ALOGV("interface sent a frame");
    /* TODO(wad): Remove this once we have live testing. */
    platform->wait(ns->handle, 1000); /* t3 = 1ms */
  }
  return recvd;
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
  /* If we weren't using spidev, we could just get notified by the driver. */
  do {
    /*
     * In practice, if complete=true, then no transmission
     * should attempt again until after 1000usec.
     */
    if (platform->receive(ns->handle, &byte, 1, complete) != 1) {
      ALOGV("failed to read one byte");
      ese_set_error(ese, 3);
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
  return -1;
}

size_t nxp_pn80t_transceive(struct EseInterface *ese,
                            const uint8_t *const tx_buf, size_t tx_len,
                            uint8_t *rx_buf, size_t rx_len) {
  return teq1_transceive(ese, &kTeq1Options, tx_buf, tx_len, rx_buf, rx_len);
}

static const char *kErrorMessages[] = {
    "T=1 hard failure.",                        /* TEQ1_ERROR_HARD_FAIL */
    "T=1 abort.",                               /* TEQ1_ERROR_ABORT */
    "T=1 device reset failed.",                 /* TEQ1_ERROR_DEVICE_ABORT */
    "spidev failed to read one byte",           /* 3 */
    "unable to initialize platform",            /* 4 */
    "failed to read",                           /* 5 */
    "attempted to receive more than uint_max",  /* 6 */
    "attempted to transfer more than uint_max", /* 7 */
    "failed to transmit",                       /* 8 */
};
