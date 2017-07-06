#include <check.h>

#include <osd/osd.h>
#include "../../src/libosd/packet.h"

#include <stdio.h>
#include <unistd.h>
#include "../../src/libosd/hostmod-private.h"
#include "../../src/libosd/include/osd/hostmod.h"

struct osd_com_ctx *com_ctx;

struct mock_packet {
    struct osd_packet *packet;
    size_t pos;
    struct mock_packet *nxt;
};

struct mock_packet_queue {
    struct mock_packet *head;
    struct mock_packet *last;
};

struct mock_packet_queue mock_write_packet_exp_queue;
struct mock_packet_queue mock_read_packet_exp_queue;
volatile int trigger_response = 0;


static void mock_packet_queue_push(struct mock_packet_queue *queue,
                                   struct osd_packet* pkg)
{
    struct mock_packet *p = calloc(1, sizeof(struct mock_packet));
    p->packet = pkg;
    p->nxt = NULL;
    p->pos = 0;

    if (queue->head == NULL && queue->last == NULL) {
        queue->head = p;
        queue->last = p;
    } else {
        queue->last->nxt = p;
        queue->last = p;
    }
}

static struct mock_packet* mock_packet_queue_pop(struct mock_packet_queue *queue)
{
    struct mock_packet *head = queue->head;
    if (head == NULL) {
        return NULL;
    }

    if (queue->head == queue->last) {
        queue->head = NULL;
        queue->last = NULL;
    } else {
        queue->head = head->nxt;
    }

    free(head->packet);
    free(head);

    return head;
}

/**
 * Mock function: called by osd-com to write data to the device
 *
 * We check if the written data is what we expect.
 */
static ssize_t mock_device_write(uint16_t *buf, size_t size_words, int flags)
{
    struct mock_packet *p;
    p = mock_write_packet_exp_queue.head;

    ck_assert_ptr_ne(p, NULL);

    osd_dtd dtd;
    osd_packet_get_dtd(p->packet, &dtd);

    if (size_words + p->pos > osd_dtd_get_size_words(dtd)) {
        ck_abort_msg("Wrote more than one packet of data.");
    }

    size_t w;
    for (w = 0; w < size_words; w++) {
        uint16_t exp_data_word = dtd[p->pos++];
        ck_assert_uint_eq(buf[w], exp_data_word);
    }

    /*
     * A complete packet has been written to the device.
     * Make the response available for the reader by setting trigger_response.
     */
    if (p->pos == osd_dtd_get_size_words(dtd)) {
        mock_packet_queue_pop(&mock_write_packet_exp_queue);
        trigger_response = 1;
    }

    return w;
}

/**
 * Mock function: called by osd-com to read data from the device
 *
 * This function only returns data if trigger_response is set. It then
 * returns one packet and waits for the next trigger.
 */
static ssize_t mock_device_read(uint16_t *buf, size_t size_words, int flags)
{
    while (!trigger_response) {
        usleep(10);
    }

    struct mock_packet *p;
    p = mock_read_packet_exp_queue.head;

    while (!p) {
        /*
         * Block "forever" (i.e. until the test runs into a timeout) if no data
         * is available to be transferred to the host.
         */
        sleep(100);
    }

    osd_dtd dtd;
    osd_packet_get_dtd(p->packet, &dtd);


    if (size_words + p->pos > osd_dtd_get_size_words(dtd)) {
        ck_abort_msg("Less read data provided than requested.");
    }

    size_t w;
    for (w = 0; w < size_words; w++) {
        buf[w] = dtd[p->pos++];
    }

    /*
     * A complete packet has been read by OSD, wait for the next trigger before
     * we continue.
     */
    if (p->pos == osd_dtd_get_size_words(dtd)) {
        mock_packet_queue_pop(&mock_read_packet_exp_queue);
        trigger_response = 0;
    }

    return w;
}

static struct osd_com_device_if device_communication_functions = {
    .write = &mock_device_write,
    .read = &mock_device_read
};

/**
 * Add a 16 bit register access to the read/write mock
 *
 * Note that the @p ret_value is not checked by the mock, you need to check
 * in the test if the returned value arrived where it should.
 */
static void mock_add_expected_reg_access(unsigned int module_addr,
                                         unsigned int reg_addr,
                                         unsigned int ret_value)
{
    osd_result rv;

    // request
    struct osd_packet *pkg_read_req;
    rv = osd_packet_new(&pkg_read_req, 1);
    ck_assert_ptr_ne(pkg_read_req, NULL);
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg_read_req, module_addr, OSD_MOD_ADDR_HIM,
                          OSD_PACKET_TYPE_REG, REQ_READ_REG_16);
    pkg_read_req->data.payload[0] = reg_addr;

    mock_packet_queue_push(&mock_write_packet_exp_queue, pkg_read_req);

    // response
    struct osd_packet *pkg_read_resp;
    rv = osd_packet_new(&pkg_read_resp, 1);
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg_read_resp, OSD_MOD_ADDR_HIM, module_addr,
                          OSD_PACKET_TYPE_REG, RESP_READ_REG_SUCCESS_16);
    pkg_read_resp->data.payload[0] = ret_value;

    mock_packet_queue_push(&mock_read_packet_exp_queue, pkg_read_resp);
}

/**
 * Test fixture: setup (called before each tests)
 */
void setup(void)
{
    osd_result rv;

    rv = osd_com_new(&com_ctx, 0);
    ck_assert_int_eq(rv, OSD_OK);

    rv = osd_com_set_device_ctrl_if(com_ctx, &device_communication_functions);
    ck_assert_int_eq(rv, OSD_OK);

    // Read VENDOR_ID (0x200) from SCM -> 0x01
    mock_add_expected_reg_access(1, 0x200, 0x1);
    // Read DEVICE_ID (0x201) from SCM -> 0x02
    mock_add_expected_reg_access(1, 0x201, 0x2);
    // Read MAX_PKT_LEN (0x203) from SCM -> 0x08
    mock_add_expected_reg_access(1, 0x203, 0x8);

    rv = osd_com_connect(com_ctx);
    ck_assert_int_eq(rv, OSD_OK);

    // all data should have been read and written after the setup routine
    ck_assert_ptr_eq(mock_read_packet_exp_queue.head, NULL);
    ck_assert_ptr_eq(mock_write_packet_exp_queue.head, NULL);
}

/**
 * Test fixture: setup (called after each test)
 */
void teardown(void)
{
    osd_result rv;

    rv = osd_com_disconnect(com_ctx);
    ck_assert_int_eq(rv, OSD_OK);

    osd_com_free(com_ctx);

    // all data should have been read and written at the end of the test
    ck_assert_ptr_eq(mock_read_packet_exp_queue.head, NULL);
    ck_assert_ptr_eq(mock_write_packet_exp_queue.head, NULL);
}

START_TEST(test_init_base)
{
    setup();
    teardown();
}
END_TEST

START_TEST(test_core_read_register)
{
    osd_result rv;

    uint16_t reg_read_result;

    mock_add_expected_reg_access(1 /* SCM */, 0x0000 /* MOD_ID */, 0x0001);

    rv = osd_com_reg_read(com_ctx, 1, 0x0000, 16, &reg_read_result, 0);
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
    rv = osd_packet_new(&pkg_read_req, 1);
    ck_assert_ptr_ne(pkg_read_req, NULL);
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg_read_req, 1, OSD_MOD_ADDR_HIM,
                          OSD_PACKET_TYPE_REG, REQ_READ_REG_16);
    pkg_read_req->data.payload[0] = 0x0000;

    mock_packet_queue_push(&mock_write_packet_exp_queue, pkg_read_req);

    rv = osd_com_reg_read(com_ctx, 1, 0x0000, 16, &reg_read_result, 0);
    ck_assert_int_eq(rv, OSD_ERROR_TIMEDOUT);
}
END_TEST

Suite * com_suite(void)
{
    Suite *s;
    TCase *tc_init, *tc_core;

    s = suite_create("libosd-com");

    // Initialization
    // As the setup and teardown functions are pretty heavy, we check them
    // here independently and use them as test fixtures after this test
    // succeeds.
    tc_init = tcase_create("Init");
    tcase_add_test(tc_core, test_init_base);
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

    s = com_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
