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
 * Android/linux platform wrappers for spidev and gpio use.
 */

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ese/hw/nxp/pn80t/platform.h>

#define MAX_TRANSFER_SIZE 258
#define DEV_PATH "/dev/nq-nci-ese"

int platform_toggle_ven(void *blob, int val) {
  int fd = (int)blob;
  fd += val;
  return 0;
}

int platform_toggle_reset(void *blob, int val) {
  int fd = (int)blob;
  fd += val;
  /* TODO:XXX 
  struct Handle *handle = blob;
  return ioctl(fd, ESE_RST, val);
  */
  return 0;
}

int platform_toggle_power_req(void *blob, int val) {
  int fd = (int)blob;
  fd += val;
  /* XXX: add ioctl */
  return 0;
}

void *platform_init(void *hwopts) {
  int fd;
  if (hwopts != NULL)
    return NULL;

  fd = open(DEV_PATH, O_RDWR);
  if (fd < 0) {
    return NULL;
  }
  /* Give the eSE a chance to boot. */
  usleep(20000);
  return (void *)(long)fd;
}

int platform_receive(void *blob, const uint8_t *buf, int32_t len, int done __attribute__((unused))) {
  int fd = (int)blob;
  if (len < 0 || len > MAX_TRANSFER_SIZE)
    return -1;
  /* TODO(wad) handle EAGAIN, etc */
  /* XXX: ignore done and see if nxp cares about CS flipping with pending data. */
  if (read(fd, (void *)buf, len) != len) {
    return -1;
  }
  return len;
}

int platform_transmit(void *blob, const uint8_t *buf, int32_t len, int done  __attribute__((unused))) {
  if (len < 0 || len > MAX_TRANSFER_SIZE)
    return -1;
  int fd = (int)blob;
  /* TODO(wad) handelk EAGAIN, etc */
  if (write(fd, (void *)buf, len) != len)
    return -1;
  return len;
}

int platform_release(void *blob) {
  int fd = (int)blob;
  close(fd);
  return 0;
}

int platform_wait(void *blob __attribute__((unused)), long usec) {
  return usleep((useconds_t)usec);
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
};
