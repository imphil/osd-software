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



#include "worker.h"

#include <osd/osd.h>
#include <osd/hostmod.h>
#include <osd/packet.h>
#include <osd/reg.h>
#include "osd-private.h"

#include <assert.h>
#include <errno.h>
#include <string.h>


/**
 * Host module context
 */
struct osd_hostmod_ctx {
    /** Is the library connected to a device? */
    bool is_connected;

    /** Logging context */
    struct osd_log_ctx *log_ctx;

    /** Address assigned to this module in the debug interconnect */
    uint16_t diaddr;

    /** I/O worker */
    struct worker_ctx *ioworker_ctx;
};

struct thread_ctx_usr {
    /** Control communication socket with the host controller */
    zsock_t *ctrl_socket;

    /** ZeroMQ address/URL of the host controller */
    char* host_controller_address;

    /** Event packet handler function */
    osd_hostmod_event_handler_fn event_handler;

    /** Argument passed to event_handler */
    void* event_handler_arg;
};

/**
 * Send a DI Packet to the host controller
 *
 * The actual sending is done through the I/O worker.
 */
static osd_result osd_hostmod_send_packet(struct osd_hostmod_ctx *ctx,
                                          struct osd_packet *packet)
{
    int rv;
    zmsg_t *msg = zmsg_new();
    assert(msg);

    rv = zmsg_addstr(msg, "D");
    assert(rv == 0);
    rv = zmsg_addmem(msg, packet->data_raw, osd_packet_sizeof(packet));
    assert(rv == 0);

    rv = zmsg_send(&msg, ctx->ioworker_ctx->inproc_socket);
    if (rv != 0) {
        return OSD_ERROR_COM;
    }

    return OSD_OK;
}

/**
 * Receive a DI Packet (on the main thread)
 *
 * @return OSD_OK if the operation was successful,
 *         OSD_ERROR_TIMEDOUT if the operation timed out.
 *         Any other value indicates an error
 */
static osd_result osd_hostmod_receive_packet(struct osd_hostmod_ctx *ctx,
                                             struct osd_packet **packet)
{
    osd_result osd_rv;

    errno = 0;
    printf("osd_hostmod_receive_packet(): waiting here\n");
    zmsg_t* msg = zmsg_recv(ctx->ioworker_ctx->inproc_socket);
    printf("osd_hostmod_receive_packet(): done waiting\n");
    if (!msg && errno == EAGAIN) {
        printf("osd_hostmod_receive_packet(): got timeout\n");
        return OSD_ERROR_TIMEDOUT;
    }
    assert(msg);

    // ensure that the message we got from the I/O thread is packet data
    // XXX: possibly extend to hand off non-packet messages to their appropriate
    // handler
    zframe_t *type_frame = zmsg_pop(msg);
    assert(type_frame);
    assert(zframe_streq(type_frame, "D"));
    zframe_destroy(&type_frame);

    // get osd_packet from frame data
    zframe_t *data_frame = zmsg_pop(msg);
    fprintf(stderr, "supertest: %s\n", zframe_strhex(data_frame));
    assert(data_frame);
    struct osd_packet *p;
    osd_rv = osd_packet_new_from_zframe(&p, data_frame);
    assert(OSD_SUCCEEDED(osd_rv));

    zframe_destroy(&data_frame);
    zmsg_destroy(&msg);

    *packet = p;

    return OSD_OK;
}

/**
 * Process incoming messages from the host controller
 *
 * @return 0 if the message was processed, -1 if @p loop should be terminated
 */
static int ctrl_io_ext_rcv(zloop_t *loop, zsock_t *reader, void *thread_ctx_void)
{
    struct worker_thread_ctx* thread_ctx = (struct worker_thread_ctx*)thread_ctx_void;
    assert(thread_ctx);

    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    int rv;
    osd_result osd_rv;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }

    zframe_t *type_frame = zmsg_first(msg);
    assert(type_frame);
    if (zframe_streq(type_frame, "D")) {
        zframe_t *data_frame = zmsg_next(msg);
        assert(data_frame);

        printf("HERE\n");
        zframe_print(data_frame, "PREFIX");
        fprintf(stderr, "supertest: %s\n", zframe_strhex(data_frame));

        struct osd_packet *pkg;
        osd_rv = osd_packet_new_from_zframe(&pkg, data_frame);
        assert(OSD_SUCCEEDED(osd_rv));

        // Forward EVENT packets to handler function.
        // Ownership of |pkg| is transferred to the event handler.
        if (osd_packet_get_type(pkg) == OSD_PACKET_TYPE_EVENT) {
            zmsg_destroy(&msg);
            osd_rv = usrctx->event_handler(usrctx->event_handler_arg, pkg);
            if (OSD_FAILED(osd_rv)) {
                err(thread_ctx->log_ctx, "Handling EVENT packet failed: %d", osd_rv);
            }
            return 0;
        }

        osd_packet_free(&pkg);

        // Forward all other data messages to the main thread
        rv = zmsg_send(&msg, thread_ctx->inproc_socket);
        assert(rv == 0);

    } else if (zframe_streq(type_frame, "M")) {
        assert(0 && "TODO: Handle incoming management messages.");

    } else {
        assert(0 && "Message of unknown type received.");
    }


    return 0;
}

/**
 * Obtain a DI address for this host debug module from the host controller
 */
static osd_result obtain_diaddr(struct worker_thread_ctx *thread_ctx,
                                uint16_t *di_addr)
{
    int rv;

    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);
    zsock_t *sock = usrctx->ctrl_socket;

    // request
    zmsg_t *msg_req = zmsg_new();
    assert(msg_req);

    rv = zmsg_addstr(msg_req, "M");
    assert(rv == 0);
    rv = zmsg_addstr(msg_req, "DIADDR_REQUEST");
    assert(rv == 0);
    rv = zmsg_send(&msg_req, sock);
    if (rv != 0) {
        err(thread_ctx->log_ctx, "Unable to send DIADDR_REQUEST request to host controller");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    // response
    errno = 0;
    zmsg_t *msg_resp = zmsg_recv(sock);
    if (!msg_resp) {
        err(thread_ctx->log_ctx, "No response received from host controller at %s: %s (%d)",
            usrctx->host_controller_address, strerror(errno), errno);
        return OSD_ERROR_CONNECTION_FAILED;
    }

    zframe_t *type_frame = zmsg_pop(msg_resp);
    assert(zframe_streq(type_frame, "M"));
    zframe_destroy(&type_frame);

    char* addr_string = zmsg_popstr(msg_resp);
    assert(addr_string);
    char* end;
    long int addr = strtol(addr_string, &end, 10);
    assert(!*end);
    assert(addr <= UINT16_MAX);
    *di_addr = (uint16_t) addr;
    free(addr_string);

    zmsg_destroy(&msg_resp);

    dbg(thread_ctx->log_ctx, "Obtained DI address %u from host controller.",
        *di_addr);

    return OSD_OK;
}

/**
 * Connect to the host controller in the I/O thread
 *
 * This function is called by the inprochelper as response to the I-CONNECT
 * message. It creates a new DIALER ZeroMQ socket and uses it to connect to the
 * host controller. After completion the function sends out a I-CONNECT-DONE
 * message. The message value is -1 if the connection failed for any reason,
 * or the DI address assigned to the host module if the connection was
 * successfully established.
 */
static void connect_to_hostctrl(struct worker_thread_ctx *thread_ctx)
{
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result retval;
    osd_result osd_rv;

    // create new DIALER socket to connect with the host controller
    usrctx->ctrl_socket = zsock_new_dealer(usrctx->host_controller_address);
    if (!usrctx->ctrl_socket) {
        err(thread_ctx->log_ctx, "Unable to connect to %s",
            usrctx->host_controller_address);
        retval = -1;
        goto free_return;
    }
    zsock_set_rcvtimeo(usrctx->ctrl_socket, ZMQ_RCV_TIMEOUT);

    // Get our DI address
    uint16_t di_addr;
    osd_rv = obtain_diaddr(thread_ctx, &di_addr);
    if (OSD_FAILED(osd_rv)) {
        retval = -1;
        goto free_return;
    }
    retval = di_addr;

    // register handler for messages coming from the host controller
    int zmq_rv;
    zmq_rv = zloop_reader(thread_ctx->zloop, usrctx->ctrl_socket,
                          ctrl_io_ext_rcv, thread_ctx);
    assert(zmq_rv == 0);
    zloop_reader_set_tolerant(thread_ctx->zloop, usrctx->ctrl_socket);

free_return:
    if (retval == -1) {
        zsock_destroy(&usrctx->ctrl_socket);
    }
    worker_send_status(thread_ctx->inproc_socket, "I-CONNECT-DONE",
                             retval);
}

/**
 * Disconnect from the host controller in the I/O thread
 *
 * This function is called when receiving a I-DISCONNECT message in the
 * I/O thread. After the disconnect is done a I-DISCONNECT-DONE message is sent
 * to the main thread.
 */
static void disconnect_from_hostctrl(struct worker_thread_ctx *thread_ctx)
{
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result retval;

    zloop_reader_end(thread_ctx->zloop, usrctx->ctrl_socket);
    zsock_destroy(&usrctx->ctrl_socket);

    retval = OSD_OK;

    worker_send_status(thread_ctx->inproc_socket, "I-DISCONNECT-DONE",
                             retval);
}

static osd_result iothread_handle_inproc_request(struct worker_thread_ctx *thread_ctx,
                                                 const char *name, zmsg_t *msg)
{
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    int rv;

    if (!strcmp(name, "I-CONNECT")) {
        connect_to_hostctrl(thread_ctx);

    } else if (!strcmp(name, "I-DISCONNECT")) {
        disconnect_from_hostctrl(thread_ctx);

    } else if (!strcmp(name, "D")) {
        // Forward data packet to the host controller
        rv = zmsg_send(&msg, usrctx->ctrl_socket);
        assert(rv == 0);

    } else {
        assert(0 && "Received unknown message from main thread.");
    }

    zmsg_destroy(&msg);

    return OSD_OK;
}

static osd_result iothread_destroy(struct worker_thread_ctx *thread_ctx)
{
    assert(thread_ctx);
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    free(usrctx->host_controller_address);
    free(usrctx);
    thread_ctx->usr = NULL;

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostmod_new(struct osd_hostmod_ctx **ctx,
                           struct osd_log_ctx *log_ctx,
                           const char *host_controller_address,
                           osd_hostmod_event_handler_fn event_handler,
                           void* event_handler_arg)
{
    osd_result rv;

    struct osd_hostmod_ctx *c = calloc(1, sizeof(struct osd_hostmod_ctx));
    assert(c);

    c->log_ctx = log_ctx;
    c->is_connected = false;

    // prepare custom data passed to I/O thread
    struct thread_ctx_usr *iothread_usr_data = calloc(1, sizeof(struct thread_ctx_usr));
    assert(iothread_usr_data);

    iothread_usr_data->event_handler = event_handler;
    iothread_usr_data->event_handler_arg = event_handler_arg;
    iothread_usr_data->host_controller_address = strdup(host_controller_address);

    rv = worker_new(&c->ioworker_ctx, log_ctx, NULL, iothread_destroy,
                          iothread_handle_inproc_request, iothread_usr_data);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    *ctx = c;

    return OSD_OK;
}

API_EXPORT
uint16_t osd_hostmod_get_diaddr(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);
    assert(ctx->is_connected);
    return ctx->diaddr;
}


API_EXPORT
osd_result osd_hostmod_connect(struct osd_hostmod_ctx *ctx)
{
    osd_result rv;
    assert(ctx);
    assert(!ctx->is_connected);

    worker_send_status(ctx->ioworker_ctx->inproc_socket, "I-CONNECT",
                             0);
    int retval;
    rv = worker_wait_for_status(ctx->ioworker_ctx->inproc_socket,
                                      "I-CONNECT-DONE", &retval);
    if (OSD_FAILED(rv) || retval == -1) {
        err(ctx->log_ctx, "Unable to establish connection to host controller.");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    ctx->diaddr = retval;
    ctx->is_connected = true;

    dbg(ctx->log_ctx, "Connection established, DI address is %u.", ctx->diaddr);

    return OSD_OK;
}

API_EXPORT
bool osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);
    return ctx->is_connected;
}

API_EXPORT
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx)
{
    osd_result rv;

    assert(ctx);

    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    worker_send_status(ctx->ioworker_ctx->inproc_socket,
                             "I-DISCONNECT", 0);
    osd_result retval;
    rv = worker_wait_for_status(ctx->ioworker_ctx->inproc_socket,
                                      "I-DISCONNECT-DONE", &retval);
    if (OSD_FAILED(rv)) {
        return rv;
    }
    if (OSD_FAILED(retval)) {
        return retval;
    }

    ctx->is_connected = false;

    return OSD_OK;
}

API_EXPORT
void osd_hostmod_free(struct osd_hostmod_ctx **ctx_p)
{
    assert(ctx_p);
    struct osd_hostmod_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    assert(!ctx->is_connected);

    worker_free(&ctx->ioworker_ctx);

    free(ctx);
    *ctx_p = NULL;
}

static osd_result osd_hostmod_regaccess(struct osd_hostmod_ctx *ctx,
                                        uint16_t module_addr,
                                        uint16_t reg_addr,
                                        enum osd_packet_type_reg_subtype subtype_req,
                                        enum osd_packet_type_reg_subtype subtype_resp,
                                        const uint16_t *wr_data,
                                        size_t wr_data_len_words,
                                        struct osd_packet **response,
                                        int flags)
{
    assert(ctx);
    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    *response = NULL;
    osd_result retval = OSD_ERROR_FAILURE;
    osd_result rv;

    // block register read indefinitely until response has been received
    bool do_block = (flags & OSD_HOSTMOD_BLOCKING);

    // assemble request packet
    struct osd_packet *pkg_req;
    unsigned int payload_size = osd_packet_get_data_size_words_from_payload(1 + wr_data_len_words);
    rv = osd_packet_new(&pkg_req, payload_size);
    if (OSD_FAILED(rv)) {
        retval = rv;
        goto err_free_req;
    }

    osd_packet_set_header(pkg_req, module_addr, ctx->diaddr,
                          OSD_PACKET_TYPE_REG, subtype_req);
    pkg_req->data.payload[0] = reg_addr;
    for (unsigned int i = 0; i < wr_data_len_words; i++) {
        pkg_req->data.payload[1 + i] = wr_data[i];
    }


    // send register read request
    rv = osd_hostmod_send_packet(ctx, pkg_req);
    if (OSD_FAILED(rv)) {
        retval = rv;
        goto err_free_req;
    }

    // wait for response
    struct osd_packet *pkg_resp;
    do {
        rv = osd_hostmod_receive_packet(ctx, &pkg_resp);
    } while (rv == OSD_ERROR_TIMEDOUT && do_block);
    if (OSD_FAILED(rv)) {
        retval = rv;
        goto err_free_req;
    }

    // parse response
    assert(osd_packet_get_type(pkg_resp) == OSD_PACKET_TYPE_REG);

    // handle register read error
    if (osd_packet_get_type_sub(pkg_resp) == RESP_READ_REG_ERROR ||
        osd_packet_get_type_sub(pkg_resp) == RESP_WRITE_REG_ERROR) {
        err(ctx->log_ctx,
            "Device returned error packet %u when accessing the register.",
            osd_packet_get_type_sub(pkg_resp));
        retval = OSD_ERROR_DEVICE_ERROR;
        goto err_free_resp;
    }

    // validate response subtype
    if (osd_packet_get_type_sub(pkg_resp) != subtype_resp) {
        err(ctx->log_ctx, "Expected register response of subtype %d, got %d",
            subtype_resp, osd_packet_get_type_sub(pkg_resp));
        retval = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }


    retval = OSD_OK;
    *response = pkg_resp;
    goto err_free_req;

err_free_resp:
    free(pkg_resp);

err_free_req:
    free(pkg_req);

    return retval;
}

static enum osd_packet_type_reg_subtype get_subtype_reg_read_req(unsigned int reg_size_bit)
{
    return (reg_size_bit / 16) - 1;
}

static enum osd_packet_type_reg_subtype get_subtype_reg_read_success_resp(unsigned int reg_size_bit)
{
    return get_subtype_reg_read_req(reg_size_bit) | 0b1000;
}

static enum osd_packet_type_reg_subtype get_subtype_reg_write_req(unsigned int reg_size_bit)
{
    return ((reg_size_bit / 16) - 1) | 0b0100;
}

API_EXPORT
osd_result osd_hostmod_reg_read(struct osd_hostmod_ctx *ctx,
                                void *result,
                                uint16_t diaddr, uint16_t reg_addr,
                                int reg_size_bit, int flags)
{
    osd_result rv;
    osd_result retval;

    assert(reg_size_bit % 16 == 0 && reg_size_bit <= 128);

    dbg(ctx->log_ctx, "Issuing %d bit read request to register 0x%x of module "
        "0x%x", reg_size_bit, reg_addr, diaddr);

    struct osd_packet *response_pkg;
    rv = osd_hostmod_regaccess(ctx, diaddr, reg_addr,
                               get_subtype_reg_read_req(reg_size_bit),
                               get_subtype_reg_read_success_resp(reg_size_bit),
                               NULL, 0,
                               &response_pkg, flags);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    // validate response size
    unsigned int exp_data_size_words = osd_packet_get_data_size_words_from_payload(reg_size_bit / 16);
    if (response_pkg->data_size_words != exp_data_size_words) {
        err(ctx->log_ctx, "Expected %d 16 bit data words in register read "
            "response, got %d.",
            exp_data_size_words,
            response_pkg->data_size_words);
        retval = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    // make result available to caller
    memcpy(result, response_pkg->data.payload, reg_size_bit / 8);


    retval = OSD_OK;

err_free_resp:
    free(response_pkg);

    return retval;
}

API_EXPORT
osd_result osd_hostmod_reg_write(struct osd_hostmod_ctx *ctx,
                                 const void *data,
                                 uint16_t diaddr, uint16_t reg_addr,
                                 int reg_size_bit, int flags)
{
    assert(reg_size_bit % 16 == 0 && reg_size_bit <= 128);

    osd_result rv;
    osd_result retval;

    dbg(ctx->log_ctx, "Issuing %d bit write request to register 0x%x of module "
        "0x%x", reg_size_bit, reg_addr, diaddr);

    struct osd_packet *response_pkg;
    rv = osd_hostmod_regaccess(ctx, diaddr, reg_addr,
                               get_subtype_reg_write_req(reg_size_bit),
                               RESP_WRITE_REG_SUCCESS,
                               data, reg_size_bit / 16,
                               &response_pkg, flags);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    // validate response size
    unsigned int data_size_words_exp = osd_packet_get_data_size_words_from_payload(0);
    if (response_pkg->data_size_words != data_size_words_exp) {
        err(ctx->log_ctx, "Invalid write response received. Expected packet "
            "with %u data words, got %u words.", data_size_words_exp,
            response_pkg->data_size_words);
        retval = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    retval = OSD_OK;

err_free_resp:
    free(response_pkg);

    return retval;
}


/**
 * Read the system information from the device, as stored in the SCM
 */
#if 0
static osd_result read_system_info_from_device(struct osd_hostmod_ctx *ctx)
{
    osd_result rv;

    rv = osd_hostmod_reg_read(ctx, osd_diaddr_build(0, 0), REG_SCM_SYSTEM_VENDOR_ID, 16,
                          &ctx->system_info.vendor_id, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read VENDOR_ID from SCM (rv=%d)\n", rv);
        return rv;
    }
    rv = osd_hostmod_reg_read(ctx, osd_diaddr_build(0, 0), REG_SCM_SYSTEM_DEVICE_ID, 16,
                          &ctx->system_info.device_id, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read DEVICE_ID from SCM (rv=%d)\n", rv);
        return rv;
    }
    rv = osd_hostmod_reg_read(ctx, osd_diaddr_build(0, 0), REG_SCM_MAX_PKT_LEN, 16,
                          &ctx->system_info.max_pkt_len, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read MAX_PKT_LEN from SCM (rv=%d)\n", rv);
        return rv;
    }

    dbg(ctx->log_ctx, "Got system information: VENDOR_ID = %u, DEVICE_ID = %u, "
        "MAX_PKT_LEN = %u\n\n", ctx->system_info.vendor_id,
        ctx->system_info.device_id, ctx->system_info.max_pkt_len);

    return OSD_OK;
}
#endif

API_EXPORT
osd_result osd_hostmod_describe_module(struct osd_hostmod_ctx *ctx,
                                       uint16_t di_addr,
                                       struct osd_module_desc *desc)
{
    osd_result rv;

    rv = osd_hostmod_reg_read(ctx, &desc->vendor, di_addr,
                              OSD_REG_BASE_MOD_VENDOR, 16, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    rv = osd_hostmod_reg_read(ctx, &desc->type, di_addr, OSD_REG_BASE_MOD_TYPE,
                              16, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    rv = osd_hostmod_reg_read(ctx, &desc->version, di_addr,
                              OSD_REG_BASE_MOD_VERSION, 16, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    return OSD_OK;
}

#if 0
/**
 * Enumerate all modules in the debug system
 *
 * @return OSD_ENUMERATION_INCOMPLETE if at least one module failed to enumerate
 */
static osd_result enumerate_debug_modules(struct osd_hostmod_ctx *ctx)
{
    osd_result ret = OSD_OK;
    osd_result rv;
    uint16_t num_modules;
    rv = osd_hostmod_reg_read(ctx, osd_diaddr_build(0, 0), REG_SCM_NUM_MOD, 16,
                          &num_modules, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read NUM_MOD from SCM\n");
        return rv;
    }
    dbg(ctx->log_ctx, "Debug system with %u modules found.\n", num_modules);

    ctx->modules = calloc(num_modules, sizeof(struct osd_module_desc));
    ctx->modules_len = num_modules;

    for (uint16_t module_addr = 2; module_addr < num_modules; module_addr++) {
        rv = discover_debug_module(ctx, module_addr);
        if (OSD_FAILED(rv)) {
            err(ctx->log_ctx, "Failed to obtain information about debug "
                "module at address %u (rv=%d)\n", module_addr, rv);
            ret = OSD_ERROR_ENUMERATION_INCOMPLETE;
            // continue with the next module anyways
        } else {
            dbg(ctx->log_ctx,
                "Found debug module at address %u of type %u.%u (v%u)\n",
                module_addr, ctx->modules->vendor, ctx->modules->type,
                ctx->modules->version);
        }
    }
    dbg(ctx->log_ctx, "Enumerated completed.\n");
    return ret;
}
#endif
