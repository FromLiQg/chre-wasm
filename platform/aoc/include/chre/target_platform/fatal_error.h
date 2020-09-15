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

#define FATAL_ERROR_QUIT() \
  do {                     \
    chre::preFatalError(); \
    abort();               \
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
