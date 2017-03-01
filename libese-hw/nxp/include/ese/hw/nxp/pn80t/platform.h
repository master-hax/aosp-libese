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

#ifndef ESE_HW_NXP_PN80T_PLATFORM_H_
#define ESE_HW_NXP_PN80T_PLATFORM_H_ 1

/* Initializes GPIO and SPI as needed. Returns an opaque handle. */
typedef void *(pn80t_platform_initialize_t)(void *);
typedef int (pn80t_platform_release_t)(void *);
typedef int (pn80t_platform_toggle_t)(void *, int);
typedef int (pn80t_platform_transmit_t)(void *, const uint8_t *buf, int32_t len, int done);
typedef int (pn80t_platform_receive_t)(void *, const uint8_t *buf, int32_t len, int done);
typedef int (pn80t_platform_poll_t)(void *, uint8_t poll_for, float timeout, int done);
typedef int (pn80t_platform_wait_t)(void *, long usec);

struct Pn80tPlatform {
  pn80t_platform_initialize_t *const initialize;   /* returns a handle */
  pn80t_platform_release_t *const release;
  pn80t_platform_toggle_t *const toggle_reset;  // ESE_RST
  pn80t_platform_toggle_t *const toggle_ven;  // NFC_VEN
  pn80t_platform_toggle_t *const toggle_power_req; // SVDD_PWR_REQ
  pn80t_platform_transmit_t *const transmit;
  pn80t_platform_receive_t *const receive;
  pn80t_platform_wait_t *const wait;
  pn80t_platform_poll_t *const poll;
};

#endif
