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
 * Linux SPIdev hw support.
 */

#include "pn80t_common.c"

extern struct Pn80tPlatform kPn80tLinuxSpidevPlatform;
static const struct EseOperations ops = {
    .name = "NXP PN80T/PN81A (PN553)",
    .open = &nxp_pn80t_open,
    .hw_receive = &nxp_pn80t_receive,
    .hw_transmit = &nxp_pn80t_transmit,
    .hw_reset = &nxp_pn80t_reset,
    .transceive = &nxp_pn80t_transceive,
    .poll = &nxp_pn80t_poll,
    .close = &nxp_pn80t_close,
    .opts = &kPn80tLinuxSpidevPlatform,
};
ESE_DEFINE_HW_OPS(ESE_HW_NXP_PN80T_SPIDEV, ops);
ESE_DEFINE_HW_ERRORS(ESE_HW_NXP_PN80T_SPIDEV, kErrorMessages);
