/* Copyright (c) 2017 by the author(s)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ============================================================================
 *
 * Author(s):
 *   Philipp Wagner <philipp.wagner@tum.de>
 */

#ifndef OSD_OSD_PRIVATE_H
#define OSD_OSD_PRIVATE_H

#include <osd/osd.h>
#include <pthread.h>

/*
 * Mark functions to be exported from the library as part of the API
 *
 * We set compiler options to mark all functions as hidden by default, causing
 * them to be private to the library. If a function is part of the API, you
 * must explicitly mark it with this macro.
 */
#define API_EXPORT __attribute__ ((visibility("default")))



/**
 * "Null" log handler: discard log messages
 */
static inline void __attribute__((always_inline, format(printf, 2, 3)))
osd_log_null(struct osd_log_ctx *ctx, const char *format, ...) {}

/**
 * Conditional logging: call osd_log only if priority is higher than
 * osd_log_get_priority()
 */
#define osd_log_cond(ctx, prio, arg...) \
  do { \
    if (ctx && osd_log_get_priority(ctx) >= prio) \
      osd_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
  } while (0)

#ifdef LOGGING
#  ifdef DEBUG
#    define dbg(ctx, arg...) osd_log_cond(ctx, LOG_DEBUG, ## arg)
#  else
#    define dbg(ctx, arg...) osd_log_null(ctx, ## arg)
#  endif
#  define info(ctx, arg...) osd_log_cond(ctx, LOG_INFO, ## arg)
#  define err(ctx, arg...) osd_log_cond(ctx, LOG_ERR, ## arg)
#else
#  define dbg(ctx, arg...) osd_log_null(ctx, ## arg)
#  define info(ctx, arg...) osd_log_null(ctx, ## arg)
#  define err(ctx, arg...) osd_log_null(ctx, ## arg)
#endif


/**
 * Log a message
 *
 * This calls the registered logging function to output (or possibly discard)
 * the log message.
 *
 * Don't use this function directly, use the dbg(), info() and err() macros
 * instead, which fill in all details for you (e.g. file name, line number,
 * etc.).
 *
 * Each call to osd_log() creates a self-contained log record. This has two
 * implications:
 * - Do not use repeated calls to osd_log() like printf(). Instead, assemble
 *   the full message first, and then call osd_log() to create a log entry.
 * - Do not add a newline character at the end of the message.
 *
 * This function is thread safe.
 *
 * @param ctx      the log context
 * @param priority the priority of the log message
 * @param file     the file the log message originates from (use __FILE__)
 * @param line     the line number the message originates from
 * @param fn       the C function the message originates from
 * @param format   the format string of the message (as used in printf() and
 *                 friends)
 *
 * @see dbg()
 * @see info()
 * @see err()
 */
void osd_log(struct osd_log_ctx *ctx,
             int priority, const char *file, int line, const char *fn,
             const char *format, ...)
             __attribute__((format(printf, 6, 7)));


static inline uint32_t __iter_div_u64_rem(uint64_t dividend, uint32_t divisor,
                                          uint64_t *remainder)
{
    uint32_t ret = 0;

    while (dividend >= divisor) {
        /* The following asm() prevents the compiler from
           optimising this loop into a modulo operation. */
        asm("" : "+rm"(dividend));
        dividend -= divisor;
        ret++;
    }
    *remainder = dividend;
    return ret;
}

#define NSEC_PER_SEC 1000000000L
static inline void timespec_add_ns(struct timespec *a, uint64_t ns)
{
    a->tv_sec += __iter_div_u64_rem(a->tv_nsec + ns, NSEC_PER_SEC, &ns);
    a->tv_nsec = ns;
}

//#define BIT_MASK(x)           (1UL << ((x) % BITS_PER_LONG))

#endif // OSD_OSD_PRIVATE_H
