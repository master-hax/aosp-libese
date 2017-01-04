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
 * Test program to select the CardManager and request the card production
 * lifcycle data (CPLC).
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <ese/ese.h>
/* Note, the struct could be build just as well. */
#include <ese/hw/nxp/pn80t/boards/hikey-spidev.h>
ESE_INCLUDE_HW(ESE_HW_NXP_PN80T_SPIDEV);

/* APDU: CLA INS P1-P2 Lc Data Le */
uint8_t kSelectCardManager[] = {0x00, 0xa4, 0x04, 0x00, 0x00};
uint8_t kGetCplc[] = {0x80, 0xCA, 0x9F, 0x7F, 0x00};

int main() {
  struct EseInterface ese = ESE_INITIALIZER(ESE_HW_NXP_PN80T_SPIDEV);
  uint8_t rx_buf[1024];
  int recvd;
  if (ese_open(&ese, (void *)(&nxp_boards_hikey_spidev))) {
    printf("Cannot open hw\n");
    if (ese_error(&ese))
      printf("eSE error (%d): %s\n", ese_error_code(&ese),
             ese_error_message(&ese));
    return 1;
  }
  if (sizeof(kSelectCardManager)) {
    size_t i;
    printf("Sending %zu bytes to card\n", sizeof(kSelectCardManager));
    printf("TX: ");
    for (i = 0; i < sizeof(kSelectCardManager); ++i)
      printf("%.2X ", kSelectCardManager[i]);
    printf("\n");
  }

  recvd = ese_transceive(&ese, kSelectCardManager, sizeof(kSelectCardManager),
                         rx_buf, sizeof(rx_buf));
  if (ese_error(&ese)) {
    printf("An error (%d) occurred: %s", ese_error_code(&ese),
           ese_error_message(&ese));
    return -1;
  }
  if (recvd > 0) {
    int i;
    printf("Read %d bytes from card\n", recvd);
    printf("RX: ");
    for (i = 0; i < recvd; ++i)
      printf("%.2X ", rx_buf[i]);
    printf("\n");
  }
  usleep(1000);

  if (sizeof(kGetCplc)) {
    size_t i;
    printf("Sending %zu bytes to card\n", sizeof(kGetCplc));
    printf("TX: ");
    for (i = 0; i < sizeof(kGetCplc); ++i)
      printf("%.2X ", kGetCplc[i]);
    printf("\n");
  }
  recvd =
      ese_transceive(&ese, kGetCplc, sizeof(kGetCplc), rx_buf, sizeof(rx_buf));
  if (ese_error(&ese)) {
    printf("An error (%d) occurred: '%s'\n", ese_error_code(&ese),
           ese_error_message(&ese));
    return -1;
  }
  if (recvd > 0) {
    int i;
    printf("Read %d bytes from card\n", recvd);
    printf("RX: ");
    for (i = 0; i < recvd; ++i)
      printf("%.2X ", rx_buf[i]);
    printf("\n");
  }

  ese_close(&ese);
  return 0;
}
