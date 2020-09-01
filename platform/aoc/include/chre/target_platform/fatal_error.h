/**
 * Copyright 2019 Google LLC. All Rights Reserved
 *
 * @file fatal_error.h
 *
 * @brief Fatal Error Handling
 */

#ifndef CHRE_PLATFORM_AOC_FATAL_ERROR_H_
#define CHRE_PLATFORM_AOC_FATAL_ERROR_H_

#include <cstdlib>

// Abort doesn't currently cause a coredump, so dereference an invalid address
// instead.
// TODO(b/166532395): Use abort once it causes a coredump.
#define FATAL_ERROR_QUIT()    \
  do {                        \
    chre::preFatalError();    \
    *(volatile int *)0x4 = 0; \
  } while (0)

namespace chre {

/**
 * Do preparation for an impending fatal error including flushing logs.
 *
 * It must not be possible for FATAL_ERROR() to be called by this function or
 * any of its callees.
 */
void preFatalError();

}  // namespace chre

#endif  // CHRE_PLATFORM_AOC_FATAL_ERROR_H_
