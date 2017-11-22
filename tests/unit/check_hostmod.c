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

#define TEST_SUITE_NAME "check_hostmod"

#include "testutil.h"

#include <osd/osd.h>
#include <osd/hostmod.h>
#include <osd/packet.h>
#include <czmq.h>

#include "testutil.h"
#include "mock_host_controller.h"

struct osd_hostmod_ctx *hostmod_ctx;
struct osd_log_ctx* log_ctx;

const unsigned int mock_hostmod_diaddr = 7;

/**
 * Setup everything related to osd_hostmod
 */
void setup_hostmod(void)
{
    osd_result rv;

    log_ctx = testutil_get_log_ctx();

    // initialize hostmod context
    rv = osd_hostmod_new(&hostmod_ctx, log_ctx, "inproc://testing", NULL, NULL);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(hostmod_ctx, NULL);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);

    // connect
    mock_host_controller_expect_diaddr_req(mock_hostmod_diaddr);

    rv = osd_hostmod_connect(hostmod_ctx);
    ck_assert_int_eq(rv, OSD_OK);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 1);

    ck_assert_uint_eq(osd_hostmod_get_diaddr(hostmod_ctx), mock_hostmod_diaddr);
}

void teardown_hostmod(void)
{
    osd_result rv;

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 1);

    rv = osd_hostmod_disconnect(hostmod_ctx);
    ck_assert_int_eq(rv, OSD_OK);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);

    osd_hostmod_free(&hostmod_ctx);
    ck_assert_ptr_eq(hostmod_ctx, NULL);
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

/**
 * Test how hostmod copes with the host controller not being reachable
 */
START_TEST(test_init_hostctrl_unreachable)
{
    osd_result rv;

    // log context
    rv = osd_log_new(&log_ctx, LOG_DEBUG, osd_log_handler);
    ck_assert_int_eq(rv, OSD_OK);

    // initialize hostmod context
    rv = osd_hostmod_new(&hostmod_ctx, log_ctx, "inproc://testing", NULL, NULL);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(hostmod_ctx, NULL);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);

    // try to connect
    rv = osd_hostmod_connect(hostmod_ctx);
    ck_assert_int_eq(rv, OSD_ERROR_CONNECTION_FAILED);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);

    osd_hostmod_free(&hostmod_ctx);
}
END_TEST

START_TEST(test_core_read_register)
{
    osd_result rv;

    uint16_t reg_read_result;

    mock_host_controller_expect_reg_read(mock_hostmod_diaddr, 1, 0x0000, 0x0001);

    rv = osd_hostmod_reg_read(hostmod_ctx, &reg_read_result, 1, 0x0000, 16, 0);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_uint_eq(reg_read_result, 0x0001);
}
END_TEST

/**
 * Test timeout handling if a debug module doesn't respond to a register read
 * request.
 */
START_TEST(test_core_read_register_timeout)
{
    osd_result rv;

    uint16_t reg_read_result;

    // add only request to mock, create no response
    struct osd_packet *pkg_read_req;
    rv = osd_packet_new(&pkg_read_req,
                        osd_packet_get_data_size_words_from_payload(1));
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(pkg_read_req, NULL);

    osd_packet_set_header(pkg_read_req, 1, mock_hostmod_diaddr,
                          OSD_PACKET_TYPE_REG, REQ_READ_REG_16);
    pkg_read_req->data.payload[0] = 0x0000;

    mock_host_controller_expect_data_req(pkg_read_req, NULL);
    osd_packet_free(&pkg_read_req);

    rv = osd_hostmod_reg_read(hostmod_ctx, &reg_read_result, 1, 0x0000, 16, 0);
    ck_assert_int_eq(rv, OSD_ERROR_TIMEDOUT);
}
END_TEST

Suite * suite(void)
{
    Suite *s;
    TCase *tc_init, *tc_core;

    s = suite_create(TEST_SUITE_NAME);

    // Initialization
    // As the setup and teardown functions are pretty heavy, we check them
    // here independently and use them as test fixtures after this test
    // succeeds.
    /*tc_init = tcase_create("Init");
    tcase_add_test(tc_init, test_init_base);
    tcase_add_test(tc_init, test_init_hostctrl_unreachable);
    suite_add_tcase(s, tc_init);*/

    // Core functionality
    tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    //tcase_add_test(tc_core, test_core_read_register);
    tcase_add_test(tc_core, test_core_read_register_timeout);
    suite_add_tcase(s, tc_core);

    return s;
}
