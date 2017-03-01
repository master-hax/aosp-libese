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
 * Platform implementation for a nq-nci extension driver.
 *
 * The driver presents the following interface on a miscdev:
 * - ioctl():
 *   - for setting and getting power
 *   - for setting the poll byte
 * - poll():
 *   - for filling the receive buffer (258 bytes)
 *   - will only fill if the receive buffer has been fully drained.
 * - read():
 *   - for reading the recieve buffer which can be done in one
 *     read call or multiple as long as the buffer is drained.
 * - write():
 *   - for sending an entire frame at once.
 *
 * All GPIO toggling and chip select requirements are handled behind this
 * interface.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>

#define LOG_TAG "libese-hw"
#include <ese/log.h>

#include <ese/hw/nxp/pn80t/platform.h>

#define MAX_TRANSFER_SIZE 258
#define ESE_SET_PWR _IOW(0xE9, 0x02, unsigned int)
#define ESE_GET_PWR _IOR(0xE9, 0x03, unsigned int)
#define ESE_SET_POLL_BYTE _IOW(0xE9, 0x11, unsigned int)
#define DEV_PATH "/dev/nq-nci-ese"

#define ese_unused __attribute__((unused))

/* This platform is not thread-safe. A single pair of I/O buffers
 * are used to handle frame transmission. If multiple simultaneous
 * callers of this platform implementation for different chips are
 * expected, these buffers will need to be moved into a handle-specific
 * buffer.
 */
struct PlatformState {
  uint32_t in_available; /* available data to read */
  uint8_t *in_cur;
  uint8_t in[MAX_TRANSFER_SIZE];
  uint32_t out_available; /* available space left to write. */
  uint8_t *out_cur;
  uint8_t out[MAX_TRANSFER_SIZE];
};
static struct PlatformState state;

void reset_state(struct PlatformState *s, int in, int out) {
  if (in) {
    s->in_available = 0;
    s->in_cur = &s->in[0];
    memset(s->in, 0, sizeof(s->in));
  }
  if (out) {
    s->out_available = sizeof(s->out);
    s->out_cur = &s->out[0];
    memset(s->out, 0, sizeof(s->out));
  }
}

int platform_toggle_ven(void *blob ese_unused, int val ese_unused) { return 0; }

int platform_toggle_reset(void *blob, int val) {
  int fd = (int)blob;
  /* 0=power and 1=no power in the kernel. */
  return ioctl(fd, ESE_SET_PWR, !val);
}

int platform_toggle_power_req(void *blob ese_unused, int val ese_unused) {
  return 0;
}

void *platform_init(void *hwopts) {
  int fd;

  reset_state(&state, 1, 1);
  if (hwopts != NULL)
    return NULL;

  fd = open(DEV_PATH, O_RDWR);
  if (fd < 0) {
    return NULL;
  }
  /* Power on. */
  if (ioctl(fd, ESE_SET_PWR, 0) < 0) {
    return NULL;
  }
  /* Give the eSE a chance to boot. */
  usleep(20000);
  return (void *)(long)fd;
}

/* Should only be called when a frame is available. */
int platform_receive(void *blob, const uint8_t *buf, int32_t len, int done) {
  int fd = (int)blob;
  ssize_t bytes = -1;
  ALOGV("nq_nci:%s: called [%d]", __func__, len);
  if (len < 0 || len > MAX_TRANSFER_SIZE)
    return -1;
  ALOGV("%s: available: %u", __func__, state.in_available);
  if (state.in_available == 0) {
    /* Drain the hardware buffer - it should always be full at this point. */
    while (bytes < 0) {
      bytes = read(fd, (void *)state.in, MAX_TRANSFER_SIZE);
      if (bytes == MAX_TRANSFER_SIZE)
        break;
      if (errno == EAGAIN || errno == EINTR)
        continue;
      ALOGE("%s: failed to drain hw buffer (ret=%zd)", bytes);
      return -1;
    }
    state.in_available = bytes;
    ALOGV("%s: read bytes: %zd", __func__, bytes);
  }

  if ((uint32_t)len > state.in_available) {
    /* Fail, but don't reset. */
    len = -1;
  } else if (len) {
    memcpy((void *)buf, state.in_cur, len);
    state.in_cur += len;
    state.in_available -= len;
    if (!state.in_available && !done) {
      done = 1;
    }
  }
  ALOGV("%s: remaining: %u", __func__, state.in_available);
  reset_state(&state, done, 0);
  return len;
}

int platform_transmit(void *blob, const uint8_t *buf, int32_t len, int done) {
  if (len < 0 || len > MAX_TRANSFER_SIZE)
    return -1;
  int fd = (int)blob;
  ALOGV("nq_nci:%s: called [%d]", __func__, len);
  if ((uint32_t)len > state.out_available) {
    return -1;
  }
  if (len) {
    memcpy(state.out_cur, buf, len);
    state.out_available -= len;
    state.out_cur += len;
  }
  if (done) {
    if (write(fd, (void *)state.out, len) != len) {
      len = -1; /* Pass along failure but forget the buffer. */
    }
    reset_state(&state, 0, 1);
  }
  return len;
}

int platform_release(void *blob) {
  int fd = (int)blob;
  /* Power off and cooldown should've happened via common code. */
  close(fd);
  return 0;
}

int platform_wait(void *blob __attribute__((unused)), long usec) {
  return usleep((useconds_t)usec);
}

int platform_poll(void *blob, uint8_t poll_for, float timeout,
                  int complete ese_unused) {
  struct pollfd ufds[1];
  int timeout_ms = (int)((0.5f + timeout) * 1000.0f);
  int ret;

  /* Tell the kernel what we're looking for */
  if (ioctl((int)blob, ESE_SET_POLL_BYTE, poll_for) < 0) {
    ALOGE("failed to set the poll byte");
    return -1;
  }

  ALOGV("computed timeout in ms is %d\n", timeout_ms);
  ufds[0].fd = (int)blob;
  ufds[0].events = POLLIN | POLLHUP; /* New data or issue. */
  ret = poll(ufds, 1, timeout_ms);
  if (ret < 0) {
    ALOGE("poll error");
    return ret;
  } else if (ret == 0) {
    ALOGV("poll timeout");
    return -1;
  }
  ALOGV("%s: poll successful", __func__);
  /* Indicated that it was not consumed. */
  return 0;
}

const struct Pn80tPlatform kPn80tNqNciPlatform = {
    .initialize = &platform_init,
    .release = &platform_release,
    .toggle_reset = &platform_toggle_reset,
    .toggle_ven = &platform_toggle_ven,
    .toggle_power_req = &platform_toggle_power_req,
    .transmit = &platform_transmit,
    .receive = &platform_receive,
    .wait = &platform_wait,
    .poll = &platform_poll,
};
