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
#include <osd/hostmod.h>
#include <osd/packet.h>
#include <czmq.h>
#include <pthread.h>

struct osd_hostmod_ctx *hostmod_ctx;
struct osd_log_ctx* log_ctx;

pthread_t mock_host_controller_thread;
volatile int mock_host_controller_ready;
volatile int mock_host_controller_thread_cancel;

zlist_t *mock_exp_req_list;
zlist_t *mock_exp_resp_list;

// DI address assigned to the host module tested here
const unsigned int mock_hostmod_diaddr = 7;


/**
 * Log handler for OSD
 */
static void osd_log_handler(struct osd_log_ctx *ctx, int priority,
                            const char *file, int line, const char *fn,
                            const char *format, va_list args)
{
    vfprintf(stderr, format, args);
}


static int mock_host_controller_shutdown_reactor(zloop_t *loop, int timer_id, void *arg)
{
    if (mock_host_controller_thread_cancel) {
        // Returning -1 ends zloop
        return -1;
    }

    return 0;
}

/**
 * Thread working as mock host controller
 */
static int mock_host_controller_msg_reactor(zloop_t *loop, zsock_t *reader, void *arg)
{
    zmsg_t * msg_req = zmsg_recv(reader);
    assert(msg_req);

    printf("Received message: \n");
    zmsg_print(msg_req);

    zmsg_t* msg_req_exp = zlist_pop(mock_exp_req_list);
    printf("Expecting message: \n");
    zmsg_print(msg_req_exp);

    // save the message source as destination for the response
    zframe_t *src_frame = zmsg_pop(msg_req);

    // ensure that the request message is what we expect
    zframe_t *f_rcv = zmsg_first(msg_req);
    zframe_t *f_exp = zmsg_first(msg_req_exp);
    while (f_rcv && f_exp) {
        ck_assert(zframe_eq(f_rcv, f_exp));

        f_rcv = zmsg_next(msg_req);
        f_exp = zmsg_next(msg_req_exp);

        ck_assert_msg((f_rcv && f_exp) || (!f_rcv && !f_exp),
                      "Number of received and expected frames doesn't match.");
    }

    zmsg_destroy(&msg_req);
    zmsg_destroy(&msg_req_exp);

    // send response message
    zmsg_t *msg_resp = zlist_pop(mock_exp_resp_list);
    if (msg_resp) {
        zmsg_prepend(msg_resp, &src_frame);
        zmsg_send(&msg_resp, reader);
    }

    return 0;
}


static void* mock_host_controller(void* arg)
{
    int rv;

    zsock_t *server_socket;
    zloop_t *mock_host_controller_loop;

    // init
    server_socket = zsock_new_router("inproc://testing");
    assert(server_socket);

    mock_host_controller_loop = zloop_new();
    rv = zloop_reader(mock_host_controller_loop, server_socket,
                      mock_host_controller_msg_reactor, NULL);
    ck_assert_int_eq(rv, 0);
    zloop_reader_set_tolerant(mock_host_controller_loop, server_socket);

    zloop_timer(mock_host_controller_loop, 200, 0,
                mock_host_controller_shutdown_reactor, NULL);
    ck_assert_int_eq(rv, 0);

    // start processing
    mock_host_controller_ready = 1;
    zloop_start(mock_host_controller_loop);

    // cleanup
    mock_host_controller_ready = 0;
    zloop_destroy(&mock_host_controller_loop);
    zsock_destroy(&server_socket);

    return 0;
}

/**
 * Expect a management message with a given command and a given response
 */
void mock_add_expected_mgmt_req(const char* cmd, const char* resp)
{
    int rv;

    // request
    zmsg_t *req_msg = zmsg_new();
    ck_assert_ptr_ne(req_msg, NULL);
    rv = zmsg_addstr(req_msg, "M");
    ck_assert_int_eq(rv, 0);
    rv = zmsg_addstr(req_msg, cmd);
    ck_assert_int_eq(rv, 0);
    rv = zlist_append(mock_exp_req_list, req_msg);
    ck_assert_int_eq(rv, 0);

    // response
    zmsg_t *resp_msg = zmsg_new();
    ck_assert_ptr_ne(req_msg, NULL);
    rv = zmsg_addstr(resp_msg, "M");
    ck_assert_int_eq(rv, 0);
    rv = zmsg_addstr(resp_msg, resp);
    ck_assert_int_eq(rv, 0);
    rv = zlist_append(mock_exp_resp_list, resp_msg);
    ck_assert_int_eq(rv, 0);
}

void mock_add_expected_di_packet(zlist_t *list, struct osd_packet *packet)
{
    int rv;

    zmsg_t *msg = zmsg_new();
    ck_assert_ptr_ne(msg, NULL);

    rv = zmsg_addstr(msg, "D");
    ck_assert_int_eq(rv, 0);
    rv = zmsg_addmem(msg, packet->data_raw, osd_packet_sizeof(packet));
    ck_assert_int_eq(rv, 0);

    rv = zlist_append(list, msg);
    ck_assert_int_eq(rv, 0);
}

/**
 * Add a 16 bit register access to the read/write mock
 *
 * Note that the @p ret_value is not checked by the mock, you need to check
 * in the test if the returned value arrived where it should.
 */
static void mock_add_expected_reg_access(unsigned int dest,
                                         unsigned int reg_addr,
                                         unsigned int ret_value)
{
    osd_result rv;

    // request
    struct osd_packet *pkg_read_req;
    rv = osd_packet_new(&pkg_read_req,
                        osd_packet_get_data_size_words_from_payload(1));
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(pkg_read_req, NULL);

    osd_packet_set_header(pkg_read_req, dest, mock_hostmod_diaddr,
                          OSD_PACKET_TYPE_REG, REQ_READ_REG_16);
    pkg_read_req->data.payload[0] = reg_addr;

    mock_add_expected_di_packet(mock_exp_req_list, pkg_read_req);
    osd_packet_free(&pkg_read_req);

    // response
    struct osd_packet *pkg_read_resp;
    rv = osd_packet_new(&pkg_read_resp,
                        osd_packet_get_data_size_words_from_payload(1));
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg_read_resp, mock_hostmod_diaddr, dest,
                          OSD_PACKET_TYPE_REG, RESP_READ_REG_SUCCESS_16);
    pkg_read_resp->data.payload[0] = ret_value;

    mock_add_expected_di_packet(mock_exp_resp_list, pkg_read_resp);
    osd_packet_free(&pkg_read_resp);
}

/**
 * Setup the ZeroMQ router standing in for the host controller in this test
 */
void setup_zeromq(void)
{
    int rv;

    mock_host_controller_thread_cancel = 0;
    mock_host_controller_ready = 0;
    rv = pthread_create(&mock_host_controller_thread, 0, mock_host_controller,
                        NULL);
    ck_assert_int_eq(rv, 0);

    mock_exp_req_list = zlist_new();
    mock_exp_resp_list = zlist_new();

    // it takes a bit for the ZeroMQ socket to be ready
    while (!mock_host_controller_ready) {
        usleep(10);
    }
}

/**
 * Setup everything related to osd_hostmod
 */
void setup_hostmod(void)
{
    osd_result rv;

    // log context
    rv = osd_log_new(&log_ctx, LOG_DEBUG, osd_log_handler);
    ck_assert_int_eq(rv, OSD_OK);

    // initialize hostmod context
    rv = osd_hostmod_new(&hostmod_ctx, log_ctx);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(hostmod_ctx, NULL);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);

    // connect
    char mock_hostmod_diaddr_str[12];
    snprintf(mock_hostmod_diaddr_str, 12, "%d", mock_hostmod_diaddr);
    mock_add_expected_mgmt_req("DIADDR_REQUEST", mock_hostmod_diaddr_str);

    rv = osd_hostmod_connect(hostmod_ctx, "inproc://testing");
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

void teardown_zeromq(void)
{
    mock_host_controller_thread_cancel = 1;

    pthread_join(mock_host_controller_thread, NULL);

    // make sure all expected requests and responses have been received/sent
    ck_assert_uint_eq(zlist_size(mock_exp_req_list), 0);
    ck_assert_uint_eq(zlist_size(mock_exp_resp_list), 0);

    zlist_destroy(&mock_exp_req_list);
    zlist_destroy(&mock_exp_resp_list);
}

/**
 * Test fixture: setup (called before each tests)
 */
void setup(void)
{
    setup_zeromq();
    setup_hostmod();
}

/**
 * Test fixture: setup (called after each test)
 */
void teardown(void)
{
    teardown_hostmod();
    teardown_zeromq();

    zlist_destroy(&mock_exp_req_list);
    zlist_destroy(&mock_exp_resp_list);
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
    rv = osd_hostmod_new(&hostmod_ctx, log_ctx);
    ck_assert_int_eq(rv, OSD_OK);
    ck_assert_ptr_ne(hostmod_ctx, NULL);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);

    // try to connect
    rv = osd_hostmod_connect(hostmod_ctx, "inproc://testing");
    ck_assert_int_eq(rv, OSD_ERROR_CONNECTION_FAILED);

    ck_assert_int_eq(osd_hostmod_is_connected(hostmod_ctx), 0);
}
END_TEST

START_TEST(test_core_read_register)
{
    osd_result rv;

    uint16_t reg_read_result;

    mock_add_expected_reg_access(1, 0x0000, 0x0001);

    rv = osd_hostmod_reg_read(hostmod_ctx, 1, 0x0000, 16, &reg_read_result, 0);
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

    mock_add_expected_di_packet(mock_exp_req_list, pkg_read_req);
    osd_packet_free(&pkg_read_req);

    rv = osd_hostmod_reg_read(hostmod_ctx, 1, 0x0000, 16, &reg_read_result, 0);
    ck_assert_int_eq(rv, OSD_ERROR_TIMEDOUT);
}
END_TEST

Suite * suite(void)
{
    Suite *s;
    TCase *tc_init, *tc_core;

    s = suite_create("libosd-hostmod");

    // Initialization
    // As the setup and teardown functions are pretty heavy, we check them
    // here independently and use them as test fixtures after this test
    // succeeds.
    tc_init = tcase_create("Init");
    tcase_add_test(tc_init, test_init_base);
    tcase_add_test(tc_init, test_init_hostctrl_unreachable);
    suite_add_tcase(s, tc_init);

    // Core functionality
    tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_core_read_register);
    tcase_add_test(tc_core, test_core_read_register_timeout);
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
