#ifndef TESTUTIL_H
#define TESTUTIL_H

#include <check.h>
#include <osd/osd.h>
#include <stdio.h>

/**
 * Log handler for OSD
 */
void osd_log_handler(struct osd_log_ctx *ctx, int priority,
                     const char *file, int line, const char *fn,
                     const char *format, va_list args)
{
    FILE* fd = stderr;

    switch (priority) {
    case LOG_ERR:
        fprintf(fd, "ERROR");
        break;
    case LOG_WARNING:
        fprintf(fd, "WARNING");
        break;
    case LOG_DEBUG:
        fprintf(fd, "DEBUG");
        break;
    }
    fprintf(fd, " %s:%d %s ", file, line, fn);
    vfprintf(fd, format, args);
    fprintf(fd, "\n");
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

/**
 * Test suite setup function. Implement this inside your test.
 */
Suite* suite(void);

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, TEST_SUITE_NAME".xml");

    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif // TESTUTIL_H
