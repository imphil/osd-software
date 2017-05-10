#include <osd.h>

#pragma once

#define API_EXPORT __attribute__ ((visibility("default")))

static inline void __attribute__((always_inline, format(printf, 2, 3)))
osd_log_null(struct osd_log_ctx *ctx, const char *format, ...) {}

#define osd_log_cond(ctx, prio, arg...) \
  do { \
    if (osd_get_log_priority(ctx) >= prio) \
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
