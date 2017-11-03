#include <check.h>

#include <osd/osd.h>
#include <osd/hostmod.h>
#include <osd/packet.h>
#include <czmq.h>
#include <pthread.h>



pthread_t mock_host_controller_thread;
volatile int mock_host_controller_ready;
volatile int mock_host_controller_thread_cancel;

zlist_t *mock_exp_req_list;
zlist_t *mock_exp_resp_list;

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

    // do we expect a packet data message?
    bool is_data_msg_rcv = zframe_streq(f_rcv, "D");
    bool is_data_msg_exp = zframe_streq(f_exp, "D");

    unsigned int frame_idx = 0;

    while (f_rcv && f_exp) {
        bool frame_is_expected = zframe_eq(f_rcv, f_exp);

        // debugging aid: print formatted packet data if contents are unexpected
        if (!frame_is_expected) {
            struct osd_packet *p;
            osd_result rv;

            if (is_data_msg_rcv) {
                rv = osd_packet_new_from_zframe(&p, f_rcv);
                ck_assert(OSD_SUCCEEDED(rv));
                printf("Received packet:\n");
                osd_packet_dump(p, stdout);
                osd_packet_free(&p);
            }

            if (is_data_msg_exp) {
                rv = osd_packet_new_from_zframe(&p, f_exp);
                ck_assert(OSD_SUCCEEDED(rv));
                printf("Expected packet:\n");
                osd_packet_dump(p, stdout);
                osd_packet_free(&p);
            }
        }
        fflush(stdout);
        ck_assert_msg(frame_is_expected,
                      "Received unexpected data in frame %u.", frame_idx);


        f_rcv = zmsg_next(msg_req);
        f_exp = zmsg_next(msg_req_exp);

        ck_assert_msg((f_rcv && f_exp) || (!f_rcv && !f_exp),
                      "Number of received and expected frames doesn't match.");
        frame_idx++;
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
void mock_host_controller_expect_mgmt_req(const char* cmd, const char* resp)
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

static void queue_data_packet(zlist_t *list, const struct osd_packet *packet)
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

void mock_host_controller_expect_data_req(const struct osd_packet *req,
                                          const struct osd_packet *resp)
{
    queue_data_packet(mock_exp_req_list, req);
    if (resp) {
        queue_data_packet(mock_exp_resp_list, resp);
    }
}

/**
 * Expect a request for a DI address from the module
 */
void mock_host_controller_expect_diaddr_req(unsigned int diaddr)
{
    char diaddr_str[12];
    snprintf(diaddr_str, 12, "%d", diaddr);
    mock_host_controller_expect_mgmt_req("DIADDR_REQUEST", diaddr_str);
}

/**
 * Add a 16 bit register access to the read/write mock
 *
 * Note that the @p ret_value is not checked by the mock, you need to check
 * in the test if the returned value arrived where it should.
 */
void mock_host_controller_expect_reg_access(unsigned int src,
                                            unsigned int dest,
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

    osd_packet_set_header(pkg_read_req, dest, src,
                          OSD_PACKET_TYPE_REG, REQ_READ_REG_16);
    pkg_read_req->data.payload[0] = reg_addr;

    // response
    struct osd_packet *pkg_read_resp;
    rv = osd_packet_new(&pkg_read_resp,
                        osd_packet_get_data_size_words_from_payload(1));
    ck_assert_int_eq(rv, OSD_OK);

    osd_packet_set_header(pkg_read_resp, src, dest,
                          OSD_PACKET_TYPE_REG, RESP_READ_REG_SUCCESS_16);
    pkg_read_resp->data.payload[0] = ret_value;

    mock_host_controller_expect_data_req(pkg_read_req, pkg_read_resp);
    osd_packet_free(&pkg_read_req);
    osd_packet_free(&pkg_read_resp);
}



/**
 * Setup the ZeroMQ router standing in for the host controller in this test
 */
void mock_host_controller_setup(void)
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

void mock_host_controller_teardown(void)
{
    mock_host_controller_thread_cancel = 1;

    pthread_join(mock_host_controller_thread, NULL);

    // make sure all expected requests and responses have been received/sent
    ck_assert_uint_eq(zlist_size(mock_exp_req_list), 0);
    ck_assert_uint_eq(zlist_size(mock_exp_resp_list), 0);

    zlist_destroy(&mock_exp_req_list);
    zlist_destroy(&mock_exp_resp_list);
}
