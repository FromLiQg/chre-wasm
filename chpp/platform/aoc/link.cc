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

#include "chpp/link.h"

#include "chpp/macros.h"
#include "chpp/platform/chpp_uart_link_manager.h"
#include "chpp/transport.h"

using chpp::UartLinkManager;

void chppPlatformLinkInit(struct ChppPlatformLinkParameters *params) {
  UartLinkManager *manager =
      static_cast<UartLinkManager *>(params->uartLinkManager);
  manager->init();
}

void chppPlatformLinkDeinit(struct ChppPlatformLinkParameters *params) {
  UartLinkManager *manager =
      static_cast<UartLinkManager *>(params->uartLinkManager);
  manager->deinit();
}

enum ChppLinkErrorCode chppPlatformLinkSend(
    struct ChppPlatformLinkParameters *params, uint8_t *buf, size_t len) {
  bool success = false;

  UartLinkManager *manager =
      static_cast<UartLinkManager *>(params->uartLinkManager);
  if (manager->prepareTxPacket(buf, len)) {
    // TODO: Consider some other cases where asynchronous reporting
    // is necessary, e.g. delayed transaction when waiting for a
    // timeout.
    success = manager->startTransaction();
  }

  return success ? CHPP_LINK_ERROR_NONE_SENT : CHPP_LINK_ERROR_UNSPECIFIED;
}

void chppPlatformLinkDoWork(struct ChppPlatformLinkParameters *params,
                            uint32_t signal) {
  UartLinkManager *manager =
      static_cast<UartLinkManager *>(params->uartLinkManager);

  if (signal & CHPP_TRANSPORT_SIGNAL_LINK_WAKE_IN_IRQ) {
    // This indicates that the remote end wants to start a transaction.
    manager->startTransaction();
  }
  if (signal & CHPP_TRANSPORT_SIGNAL_LINK_RX_PROCESS) {
    manager->processRxSamples();
  }
}

void chppPlatformLinkReset(struct ChppPlatformLinkParameters *params) {
  UartLinkManager *manager =
      static_cast<UartLinkManager *>(params->uartLinkManager);
  manager->deinit();
  manager->init();
}
