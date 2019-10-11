/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef _CHRE_RE_H_
#define _CHRE_RE_H_

/**
 * @file
 * Some of the core Runtime Environment utilities of the Context Hub
 * Runtime Environment.
 *
 * This includes functions for memory allocation, logging, and timers.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The instance ID for the CHRE.
 *
 * This ID is used to identify events generated by the CHRE (as
 * opposed to events generated by another nanoapp).
 */
#define CHRE_INSTANCE_ID  UINT32_C(0)

/**
 * A timer ID representing an invalid timer.
 *
 * This valid is returned by chreTimerSet() if a timer cannot be
 * started.
 */
#define CHRE_TIMER_INVALID  UINT32_C(-1)


/**
 * Logging levels used to indicate severity level of logging messages.
 *
 * CHRE_LOG_ERROR: Something fatal has happened, i.e. something that will have
 *     user-visible consequences and won't be recoverable without explicitly
 *     deleting some data, uninstalling applications, wiping the data
 *     partitions or reflashing the entire phone (or worse).
 * CHRE_LOG_WARN: Something that will have user-visible consequences but is
 *     likely to be recoverable without data loss by performing some explicit
 *     action, ranging from waiting or restarting an app all the way to
 *     re-downloading a new version of an application or rebooting the device.
 * CHRE_LOG_INFO: Something interesting to most people happened, i.e. when a
 *     situation is detected that is likely to have widespread impact, though
 *     isn't necessarily an error.
 * CHRE_LOG_DEBUG: Used to further note what is happening on the device that
 *     could be relevant to investigate and debug unexpected behaviors. You
 *     should log only what is needed to gather enough information about what
 *     is going on about your component.
 *
 * There is currently no API to turn on/off logging by level, but we anticipate
 * adding such in future releases.
 *
 * @see chreLog
 */
enum chreLogLevel {
    CHRE_LOG_ERROR,
    CHRE_LOG_WARN,
    CHRE_LOG_INFO,
    CHRE_LOG_DEBUG
};


/**
 * Get the application ID.
 *
 * The application ID is set by the loader of the nanoapp.  This is not
 * assured to be unique among all nanoapps running in the system.
 *
 * @returns The application ID.
 */
uint64_t chreGetAppId(void);

/**
 * Get the instance ID.
 *
 * The instance ID is the CHRE handle to this nanoapp.  This is assured
 * to be unique among all nanoapps running in the system, and to be
 * different from the CHRE_INSTANCE_ID.  This is the ID used to communicate
 * between nanoapps.
 *
 * @returns The instance ID
 */
uint32_t chreGetInstanceId(void);

/**
 * A method for logging information about the system.
 *
 * The chreLog logging activity alone must not cause host wake-ups. For
 * example, logs could be buffered in internal memory when the host is asleep,
 * and delivered when appropriate (e.g. the host wakes up). If done this way,
 * the internal buffer is recommended to be large enough (at least a few KB), so
 * that multiple messages can be buffered. When these logs are sent to the host,
 * they are strongly recommended to be made visible under the tag 'CHRE' in
 * logcat - a future version of the CHRE API may make this a hard requirement.
 *
 * A log entry can have a variety of levels (@see LogLevel).  This function
 * allows a variable number of arguments, in a printf-style format.
 *
 * A nanoapp needs to be able to rely upon consistent printf format
 * recognition across any platform, and thus we establish formats which
 * are required to be handled by every CHRE implementation.  Some of the
 * integral formats may seem obscure, but this API heavily uses types like
 * uint32_t and uint16_t.  The platform independent macros for those printf
 * formats, like PRId32 or PRIx16, end up using some of these "obscure"
 * formats on some platforms, and thus are required.
 *
 * For the initial N release, our emphasis is on correctly getting information
 * into the log, and minimizing the requirements for CHRE implementations
 * beyond that.  We're not as concerned about how the information is visually
 * displayed.  As a result, there are a number of format sub-specifiers which
 * are "OPTIONAL" for the N implementation.  "OPTIONAL" in this context means
 * that a CHRE implementation is allowed to essentially ignore the specifier,
 * but it must understand the specifier enough in order to properly skip it.
 *
 * For a nanoapp author, an OPTIONAL format means you might not get exactly
 * what you want on every CHRE implementation, but you will always get
 * something sane.
 *
 * To be clearer, here's an example with the OPTIONAL 0-padding for integers
 * for different hypothetical CHRE implementations.
 * Compliant, chose to implement OPTIONAL format:
 *   chreLog(level, "%04x", 20) ==> "0014"
 * Compliant, chose not to implement OPTIONAL format:
 *   chreLog(level, "%04x", 20) ==> "14"
 * Non-compliant, discarded format because the '0' was assumed to be incorrect:
 *   chreLog(level, "%04x", 20) ==> ""
 *
 * Note that some of the OPTIONAL specifiers will probably become
 * required in future APIs.
 *
 * We also have NOT_SUPPORTED specifiers.  Nanoapp authors should not use any
 * NOT_SUPPORTED specifiers, as unexpected things could happen on any given
 * CHRE implementation.  A CHRE implementation is allowed to support this
 * (for example, when using shared code which already supports this), but
 * nanoapp authors need to avoid these.
 *
 * Unless specifically noted as OPTIONAL or NOT_SUPPORTED, format
 * (sub-)specifiers listed below are required.
 *
 * OPTIONAL format sub-specifiers:
 * - '-' (left-justify within the given field width)
 * - '+' (preceed the result with a '+' sign if it is positive)
 * - ' ' (preceed the result with a blank space if no sign is going to be
 *        output)
 * - '#' (For 'o', 'x' or 'X', preceed output with "0", "0x" or "0X",
 *        respectively.  For floating point, unconditionally output a decimal
 *        point.)
 * - '0' (left pad the number with zeroes instead of spaces when <width>
 *        needs padding)
 * - <width> (A number representing the minimum number of characters to be
 *            output, left-padding with blank spaces if needed to meet the
 *            minimum)
 * - '.'<precision> (A number which has different meaning depending on context.)
 *    - Integer context: Minimum number of digits to output, padding with
 *          leading zeros if needed to meet the minimum.
 *    - 'f' context: Number of digits to output after the decimal
 *          point (to the right of it).
 *    - 's' context: Maximum number of characters to output.
 *
 * Integral format specifiers:
 * - 'd' (signed)
 * - 'u' (unsigned)
 * - 'o' (octal)
 * - 'x' (hexadecimal, lower case)
 * - 'X' (hexadecimal, upper case)
 *
 * Integral format sub-specifiers (as prefixes to an above integral format):
 * - 'hh' (char)
 * - 'h' (short)
 * - 'l' (long)
 * - 'll' (long long)
 * - 'z' (size_t)
 * - 't' (ptrdiff_t)
 *
 * Other format specifiers:
 * - 'f' (floating point)
 * - 'c' (character)
 * - 's' (character string, terminated by '\0')
 * - 'p' (pointer)
 * - '%' (escaping the percent sign (i.e. "%%" becomes "%"))
 *
 * NOT_SUPPORTED specifiers:
 * - 'n' (output nothing, but fill in a given pointer with the number
 *        of characters written so far)
 * - '*' (indicates that the width/precision value comes from one of the
 *        arguments to the function)
 * - 'e', 'E' (scientific notation output)
 * - 'g', 'G' (Shortest floating point representation)
 *
 * @param level  The severity level for this message.
 * @param formatStr  Either the entirety of the message, or a printf-style
 *     format string of the format documented above.
 * @param ...  A variable number of arguments necessary for the given
 *     'formatStr' (there may be no additional arguments for some 'formatStr's).
 */
void chreLog(enum chreLogLevel level, const char *formatStr, ...);

/**
 * Get the system time.
 *
 * This returns a time in nanoseconds in reference to some arbitrary
 * time in the past.  This method is only useful for determining timing
 * between events on the system, and is not useful for determining
 * any sort of absolute time.
 *
 * This value must always increase (and must never roll over).  This
 * value has no meaning across CHRE reboots.
 *
 * @returns The system time, in nanoseconds.
 */
uint64_t chreGetTime(void);

/**
 * Retrieves CHRE's current estimated offset between the local CHRE clock
 * exposed in chreGetTime(), and the host-side clock exposed in the Android API
 * SystemClock.elapsedRealtimeNanos().  This offset is formed as host time minus
 * CHRE time, so that it can be added to the value returned by chreGetTime() to
 * determine the current estimate of the host time.
 *
 * A call to this function must not require waking up the host and should return
 * quickly.
 *
 * This function must always return a valid value from the earliest point that
 * it can be called by a nanoapp.  In other words, it is not valid to return
 * some fixed/invalid value while waiting for the initial offset estimate to be
 * determined - this initial offset must be ready before nanoapps are started.
 *
 * @returns An estimate of the offset between CHRE's time returned in
 *     chreGetTime() and the time on the host given in the Android API
 *     SystemClock.elapsedRealtimeNanos(), accurate to within +/- 10
 *     milliseconds, such that adding this offset to chreGetTime() produces the
 *     estimated current time on the host.  This value may change over time to
 *     account for drift, etc., so multiple calls to this API may produce
 *     different results.
 *
 * @since v1.1
 */
int64_t chreGetEstimatedHostTimeOffset(void);

/**
 * Convenience function to retrieve CHRE's estimate of the current time on the
 * host, corresponding to the Android API SystemClock.elapsedRealtimeNanos().
 *
 * @returns An estimate of the current time on the host, accurate to within
 *     +/- 10 milliseconds.  This estimate is *not* guaranteed to be
 *     monotonically increasing, and may move backwards as a result of receiving
 *     new information from the host.
 *
 * @since v1.1
 */
static inline uint64_t chreGetEstimatedHostTime(void) {
    int64_t offset = chreGetEstimatedHostTimeOffset();
    uint64_t time = chreGetTime();

    // Just casting time to int64_t and adding the (potentially negative) offset
    // should be OK under most conditions, but this way avoids issues if
    // time >= 2^63, which is technically allowed since we don't specify a start
    // value for chreGetTime(), though one would assume 0 is roughly boot time.
    if (offset >= 0) {
        time += (uint64_t) offset;
    } else {
        // Assuming chreGetEstimatedHostTimeOffset() is implemented properly,
        // this will never underflow, because offset = hostTime - chreTime,
        // and both times are monotonically increasing (e.g. when determining
        // the offset, if hostTime is 0 and chreTime is 100 we'll have
        // offset = -100, but chreGetTime() will always return >= 100 after that
        // point).
        time -= (uint64_t) (offset * -1);
    }

    return time;
}

/**
 * Set a timer.
 *
 * When the timer fires, nanoappHandleEvent will be invoked with
 * CHRE_EVENT_TIMER and with the given 'cookie'.
 *
 * A CHRE implementation is required to provide at least 32
 * timers.  However, there's no assurance there will be any available
 * for any given nanoapp (if it's loaded late, etc).
 *
 * @param duration  Time, in nanoseconds, before the timer fires.
 * @param cookie  Argument that will be sent to nanoappHandleEvent upon the
 *     timer firing.  This is allowed to be NULL and does not need to be
 *     a valid pointer (assuming the nanoappHandleEvent code is expecting such).
 * @param oneShot  If true, the timer will just fire once.  If false, the
 *     timer will continue to refire every 'duration', until this timer is
 *     canceled (@see chreTimerCancel).
 *
 * @returns  The timer ID.  If the system is unable to set a timer
 *     (no more available timers, etc.) then CHRE_TIMER_INVALID will
 *     be returned.
 *
 * @see nanoappHandleEvent
 */
uint32_t chreTimerSet(uint64_t duration, const void *cookie, bool oneShot);

/**
 * Cancel a timer.
 *
 * After this method returns, the CHRE assures there will be no more
 * events sent from this timer, and any enqueued events from this timer
 * will need to be evicted from the queue by the CHRE.
 *
 * @param timerId  A timer ID obtained by this nanoapp via chreTimerSet().
 * @returns true if the timer was cancelled, false otherwise.  We may
 *     fail to cancel the timer if it's a one shot which (just) fired,
 *     or if the given timer ID is not owned by the calling app.
 */
bool chreTimerCancel(uint32_t timerId);

/**
 * Terminate this nanoapp.
 *
 * This takes effect immediately.
 *
 * The CHRE will no longer execute this nanoapp.  The CHRE will not invoke
 * nanoappEnd(), nor will it call any memory free callbacks in the nanoapp.
 *
 * The CHRE will unload/evict this nanoapp's code.
 *
 * @param abortCode  A value indicating the reason for aborting.  (Note that
 *    in this version of the API, there is no way for anyone to access this
 *    code, but future APIs may expose it.)
 * @returns Never.  This method does not return, as the CHRE stops nanoapp
 *    execution immediately.
 */
void chreAbort(uint32_t abortCode);

/**
 * Allocate a given number of bytes from the system heap.
 *
 * The nanoapp is required to free this memory via chreHeapFree() prior to
 * the nanoapp ending.
 *
 * While the CHRE implementation is required to free up heap resources of
 * a nanoapp when unloading it, future requirements and tests focused on
 * nanoapps themselves may check for memory leaks, and will require nanoapps
 * to properly manage their heap resources.
 *
 * @param bytes  The number of bytes requested.
 * @returns  A pointer to 'bytes' contiguous bytes of heap memory, or NULL
 *     if the allocation could not be performed.  This pointer must be suitably
 *     aligned for any kind of variable.
 *
 * @see chreHeapFree.
 */
void *chreHeapAlloc(uint32_t bytes);

/**
 * Free a heap allocation.
 *
 * This allocation must be from a value returned from a chreHeapAlloc() call
 * made by this nanoapp.  In other words, it is illegal to free memory
 * allocated by another nanoapp (or the CHRE).
 *
 * @param ptr  'ptr' is required to be a value returned from chreHeapAlloc().
 *     Note that since chreHeapAlloc can return NULL, CHRE
 *     implementations must safely handle 'ptr' being NULL.
 *
 * @see chreHeapAlloc.
 */
void chreHeapFree(void *ptr);


#ifdef __cplusplus
}
#endif

#endif  /* _CHRE_RE_H_ */

