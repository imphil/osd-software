#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <osd/osd.h>

/**
 * Log handler for OSD
 */
void osd_log_handler(struct osd_log_ctx *ctx, int priority,
                            const char *file, int line, const char *fn,
                            const char *format, va_list args)
{
    vfprintf(stderr, format, args);
}

/**
 * Get a log context to be used in the unit tests
 */
struct osd_log_ctx* testutil_get_log_ctx()
{
    osd_result rv;
    struct osd_log_ctx* log_ctx;
    rv = osd_log_new(&log_ctx, LOG_DEBUG, osd_log_handler);
    ck_assert_int_eq(rv, OSD_OK);

    return log_ctx;
}

#endif // TESTUTIL_H
