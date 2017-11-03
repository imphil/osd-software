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

#include <check.h>

#include <osd/osd.h>
#include <osd/hostmod_stmlogger.h>
#include <osd/packet.h>
#include <osd/reg.h>
#include <czmq.h>

#include "testutil.h"
#include "mock_host_controller.h"

struct osd_hostmod_stmlogger_ctx *mod_ctx;
struct osd_log_ctx* log_ctx;

const unsigned int mock_hostmod_diaddr = 7;
const unsigned int mock_stm_diaddr = 10;

/**
 * Setup everything related to osd_hostmod
 */
void setup_hostmod(void)
{
    osd_result rv;

    log_ctx = testutil_get_log_ctx();

    // initialize module context
    rv = osd_hostmod_stmlogger_new(&mod_ctx, log_ctx, mock_stm_diaddr);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(mod_ctx, NULL);

    // connect
    mock_host_controller_expect_diaddr_req(mock_hostmod_diaddr);

    rv = osd_hostmod_stmlogger_connect(mod_ctx, "inproc://testing");
    ck_assert_int_eq(rv, OSD_OK);
}

void teardown_hostmod(void)
{
    osd_result rv;
    rv = osd_hostmod_stmlogger_disconnect(mod_ctx);
    ck_assert_int_eq(rv, OSD_OK);

    osd_hostmod_stmlogger_free(&mod_ctx);
    ck_assert_ptr_eq(mod_ctx, NULL);
}

/**
 * Test fixture: setup (called before each tests)
 */
void setup(void)
{
    mock_host_controller_setup();
    setup_hostmod();
}

/**
 * Test fixture: setup (called after each test)
 */
void teardown(void)
{
    teardown_hostmod();
    mock_host_controller_teardown();
}

START_TEST(test_init_base)
{
    setup();
    teardown();
}
END_TEST

START_TEST(test_core_tracestart)
{
    mock_host_controller_expect_reg_access(mock_hostmod_diaddr,
                                           mock_stm_diaddr,
                                           OSD_REG_BASE_MOD_VENDOR,
                                           OSD_MODULE_VENDOR_OSD);
    mock_host_controller_expect_reg_access(mock_hostmod_diaddr,
                                           mock_stm_diaddr,
                                           OSD_REG_BASE_MOD_TYPE,
                                           OSD_MODULE_TYPE_STD_STM);
    mock_host_controller_expect_reg_access(mock_hostmod_diaddr,
                                           mock_stm_diaddr,
                                           OSD_REG_BASE_MOD_VERSION,
                                           0);
    osd_hostmod_stmlogger_tracestart(mod_ctx);
}
END_TEST

Suite * suite(void)
{
    Suite *s;
    TCase *tc_init, *tc_core;

    s = suite_create("libosd-hostmod_stmlogger");

    // Initialization
    // As the setup and teardown functions are pretty heavy, we check them
    // here independently and use them as test fixtures after this test
    // succeeds.
    tc_init = tcase_create("Init");
    tcase_add_test(tc_init, test_init_base);
    suite_add_tcase(s, tc_init);

    // Core functionality
    tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_core_tracestart);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
