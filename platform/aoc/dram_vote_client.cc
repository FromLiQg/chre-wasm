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

#include "chre/platform/shared/dram_vote_client.h"

#include "chre/platform/assert.h"

#include "sysmem.h"

namespace chre {

void DramVoteClient::issueDramVote(bool enabled) {
  if (mLastDramVote != enabled) {
    int rc;
    if (enabled) {
      rc = SysMem::Instance()->MemoryRequest(SysMem::MIF, true);
    } else {
      rc = SysMem::Instance()->MemoryRelease(SysMem::MIF);
    }

    CHRE_ASSERT_LOG(rc == 0,
                    "Unable to change DRAM access to %d with error code %d",
                    enabled, rc);
  }
}

}  // namespace chre
