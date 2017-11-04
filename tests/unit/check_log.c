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

#define TEST_SUITE_NAME "check_log"

#include "testutil.h"

#include <osd/osd.h>
#include "../../src/libosd/osd-private.h"

int log_handler_called = 0;

static void log_handler(struct osd_log_ctx *ctx, int priority,
                        const char *file, int line, const char *fn,
                        const char *format, va_list args)
{
    ck_assert_int_eq(priority, LOG_ERR);
    ck_assert_str_eq(format, "testmsg");
    log_handler_called = 1;
}

START_TEST(test_log_basic)
{
    osd_result rv;
    struct osd_log_ctx *log_ctx;

    // initialization
    rv = osd_log_new(&log_ctx, 0, 0);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_int_eq(osd_log_get_priority(log_ctx), LOG_ERR);

    // priority setting
    osd_log_set_priority(log_ctx, LOG_DEBUG);
    ck_assert_int_eq(osd_log_get_priority(log_ctx), LOG_DEBUG);

    // caller context
    osd_log_set_caller_ctx(log_ctx, (void*)1337);
    ck_assert_ptr_eq(osd_log_get_caller_ctx(log_ctx), (void*)1337);

    // log function
    osd_log_set_fn(log_ctx, &log_handler);

    // logging a message
    log_handler_called = 0;
    osd_log(log_ctx, LOG_ERR, __FILE__, __LINE__, __FUNCTION__,
            "testmsg");
    ck_assert_int_eq(log_handler_called, 1);

    // logging without log handler set
    osd_log_set_fn(log_ctx, NULL);
    log_handler_called = 0;
    osd_log(log_ctx, LOG_ERR, __FILE__, __LINE__, __FUNCTION__,
            "testmsg");
    ck_assert_int_eq(log_handler_called, 0);

    osd_log_free(&log_ctx);
    ck_assert_ptr_eq(log_ctx, NULL);
}
END_TEST

START_TEST(test_log_constructorparams)
{
    osd_result rv;
    struct osd_log_ctx *log_ctx;

    rv = osd_log_new(&log_ctx, LOG_DEBUG, &log_handler);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_int_eq(osd_log_get_priority(log_ctx), LOG_DEBUG);

    // logging a message (tests if log handler is set correctly)
    log_handler_called = 0;
    osd_log(log_ctx, LOG_ERR, __FILE__, __LINE__, __FUNCTION__,
            "testmsg");
    ck_assert_int_eq(log_handler_called, 1);

    osd_log_free(&log_ctx);
}
END_TEST

Suite * suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create(TEST_SUITE_NAME);

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_log_basic);
    tcase_add_test(tc_core, test_log_constructorparams);
    suite_add_tcase(s, tc_core);

    return s;
}
