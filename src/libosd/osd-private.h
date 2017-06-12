#include <osd/osd.h>

#pragma once

/*
 * Mark functions to be exported from the library as part of the API
 *
 * We set compiler options to mark all functions as hidden by default, causing
 * them to be private to the library. If a function is part of the API, you
 * must explicitly mark it with this makro.
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

#define BIT(x)                (1UL << (x))
//#define BIT_MASK(x)           (1UL << ((x) % BITS_PER_LONG))
