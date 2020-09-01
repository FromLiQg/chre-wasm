/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef CHPP_PLATFORM_LINK_H_
#define CHPP_PLATFORM_LINK_H_

#ifdef __cplusplus
extern "C" {
#endif

//! A signal to use when there is RX data to process at the UART.
#define CHPP_TRANSPORT_SIGNAL_LINK_RX_PROCESS UINT32_C(1 << 16)

//! A signal to use when there is an interrupt from the wake_in GPIO.
#define CHPP_TRANSPORT_SIGNAL_LINK_WAKE_IN_IRQ UINT32_C(1 << 17)

//! A signal to use when waiting for the handshaking wake_in GPIO IRQs.
//! This signal is reserved for use internally in the chpp::UartLinkManager.
#define CHPP_TRANSPORT_SIGNAL_LINK_HANDSHAKE_IRQ UINT32_C(1 << 18)

#define CHPP_PLATFORM_LINK_TX_MTU_BYTES 1280
#define CHPP_PLATFORM_LINK_RX_MTU_BYTES 1280

#define CHPP_PLATFORM_TRANSPORT_TIMEOUT_MS 1000

enum ChppLinkType {
  CHPP_LINK_TYPE_WIFI = 0,
  CHPP_LINK_TYPE_GNSS,
  CHPP_LINK_TYPE_WWAN,
  CHPP_LINK_TYPE_TOTAL,
};

struct ChppPlatformLinkParameters {
  //! Must be of type chpp::UartLinkManager
  void *uartLinkManager;
};

#ifdef __cplusplus
}
#endif

#endif  // CHPP_PLATFORM_LINK_H_
