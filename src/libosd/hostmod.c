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


#include <osd/osd.h>
#include <osd/hostmod.h>
#include <osd/packet.h>
#include <osd/reg.h>
#include "osd-private.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "hostmod-private.h"

/**
 * Timeout (in ms) when receiving data from a ZeroMQ socket. This can be either
 * data from the host controller, or data from the I/O thread.
 */
#define ZMQ_RCV_TIMEOUT (1*1000) // 1 s

/**
 * Send a DI Packet
 */
static osd_result osd_hostmod_send_packet(struct osd_hostmod_ctx *ctx,
                                          struct osd_packet *packet)
{
    int rv;
    zmsg_t *msg = zmsg_new();
    assert(msg);

    rv = zmsg_addstr(msg, "D");
    assert(rv == 0);
    dbg(ctx->log_ctx, "size: %d\n", packet->data_size_words);
    rv = zmsg_addmem(msg, packet->data_raw, osd_packet_sizeof(packet));
    assert(rv == 0);

    rv = zmsg_send(&msg, ctx->inproc_ctrl_io_socket);
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
    zmsg_t* msg = zmsg_recv(ctx->inproc_ctrl_io_socket);
    if (!msg && errno == EAGAIN) {
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

    rv = osd_hostmod_reg_read(ctx, di_addr, OSD_REG_BASE_MOD_VENDOR, 16,
                              &desc->vendor, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    rv = osd_hostmod_reg_read(ctx, di_addr, OSD_REG_BASE_MOD_TYPE, 16,
                              &desc->type, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    rv = osd_hostmod_reg_read(ctx, di_addr, OSD_REG_BASE_MOD_VERSION, 16,
                              &desc->version, 0);
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

/**
 * Process messages from the main thread to the ctrl_io thread
 *
 * @return 0 if the message was processed, -1 if @p loop should be terminated
 */
static int ctrl_io_inproc_rcv(zloop_t *loop, zsock_t *reader, void *ctx_void)
{
    struct osd_hostmod_ctrl_io_ctx* ctx = (struct osd_hostmod_ctrl_io_ctx*)ctx_void;
    int retval;
    int rv;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }

    zframe_t *type_frame = zmsg_first(msg);
    assert(type_frame);

    if (zframe_streq(type_frame, "I-DISCONNECT")) {
        // End ctrl_io thread by returning -1, which will terminate zloop
        retval = -1;
        goto free_return;

    } else if (zframe_streq(type_frame, "D")) {
        // Forward data packet to the host controller
        rv = zmsg_send(&msg, ctx->ctrl_socket);
        assert(rv == 0);
        retval = 0;

    } else {
        assert(0 && "Unknown message received from main thread.");
    }

    retval = 0;
free_return:
    zmsg_destroy(&msg);
    return retval;
}

/**
 * Process incoming messages from the host controller
 *
 * @return 0 if the message was processed, -1 if @p loop should be terminated
 */
static int ctrl_io_ext_rcv(zloop_t *loop, zsock_t *reader, void *ctx_void)
{
    struct osd_hostmod_ctrl_io_ctx* ctx = (struct osd_hostmod_ctrl_io_ctx*)ctx_void;
    int rv;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }

    zframe_t *type_frame = zmsg_first(msg);
    assert(type_frame);
    if (zframe_streq(type_frame, "D")) {
        // forward data messages to the main thread
        rv = zmsg_send(&msg, ctx->inproc_ctrl_io_socket);
        assert(rv == 0);

    } else if (zframe_streq(type_frame, "M")) {
        assert(0 && "TODO: Handle incoming management messages.");

    } else {
        assert(0 && "Message of unknown type received.");
    }


    return 0;
}

/**
 * Send a data message to another thread over a ZeroMQ socket
 *
 * @see threadcom_send_status()
 */
static void threadcom_send_data(zsock_t *socket, const char* status,
                                const void* data, size_t size)
{
    int zmq_rv;

    zmsg_t *msg = zmsg_new();
    assert(msg);
    zmq_rv = zmsg_addstr(msg, status);
    assert(zmq_rv == 0);
    if (data != NULL && size > 0) {
        zmq_rv = zmsg_addmem(msg, data, size);
        assert(zmq_rv == 0);
    }
    zmq_rv = zmsg_send(&msg, socket);
    assert(zmq_rv == 0);
}

/**
 * Send a status message to another thread over a ZeroMQ socket
 *
 * @see threadcom_send_data()
 */
static void threadcom_send_status(zsock_t *socket, const char* status)
{
    threadcom_send_data(socket, status, NULL, 0);
}

/**
 * Obtain a DI address for this host debug module from the host controller
 */
static osd_result obtain_diaddr(struct osd_hostmod_ctrl_io_ctx *ctx, uint16_t *di_addr)
{
    int rv;

    zsock_t *sock = ctx->ctrl_socket;

    // request
    zmsg_t *msg_req = zmsg_new();
    assert(msg_req);

    rv = zmsg_addstr(msg_req, "M");
    assert(rv == 0);
    rv = zmsg_addstr(msg_req, "DIADDR_REQUEST");
    assert(rv == 0);
    rv = zmsg_send(&msg_req, sock);
    if (rv != 0) {
        err(ctx->log_ctx, "Unable to send DIADDR_REQUEST request to host controller\n");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    // response
    errno = 0;
    zmsg_t *msg_resp = zmsg_recv(sock);
    if (!msg_resp) {
        err(ctx->log_ctx, "No response received from host controller at %s: %s (%d)\n",
            ctx->host_controller_address, strerror(errno), errno);
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

    dbg(ctx->log_ctx, "Obtained DI address %u from host controller.\n",
        *di_addr);

    return OSD_OK;
}

/**
 * I/O Thread for all control traffic
 *
 * This thread handles all control communication with the host controller.
 * Inside the host module communicate with this thread using threadcom_*
 * functions, which make use of a inproc PAIR ZeroMQ socket.
 *
 * @param ctrl_io_ctx_void the thread context object. The thread takes over
 *                         ownership of this object and frees it when its done.
 */
static void* thread_ctrl_io_main(void *ctrl_io_ctx_void)
{
    struct osd_hostmod_ctrl_io_ctx *ctrl_io_ctx = (struct osd_hostmod_ctrl_io_ctx*) ctrl_io_ctx_void;

    int zmq_rv;
    osd_result osd_rv;
    zloop_t* ctrl_zloop = NULL;

    // create new PAIR socket for the communication of the main thread in this
    // host module with this I/O thread
    ctrl_io_ctx->inproc_ctrl_io_socket = zsock_new_pair(">inproc://ctrl_io");
    assert(ctrl_io_ctx->inproc_ctrl_io_socket);

    // create new DIALER socket for the control connection of this module
    ctrl_io_ctx->ctrl_socket = zsock_new_dealer(ctrl_io_ctx->host_controller_address);
    if (!ctrl_io_ctx->ctrl_socket) {
        err(ctrl_io_ctx->log_ctx, "Unable to connect to %s\n",
            ctrl_io_ctx->host_controller_address);
        threadcom_send_status(ctrl_io_ctx->inproc_ctrl_io_socket, "I-CONNECT-FAIL");
        goto free_return;
    }

    zsock_set_rcvtimeo(ctrl_io_ctx->ctrl_socket, ZMQ_RCV_TIMEOUT);

    // Get our DI address
    uint16_t di_addr;
    osd_rv = obtain_diaddr(ctrl_io_ctx, &di_addr);
    if (OSD_FAILED(osd_rv)) {
        threadcom_send_status(ctrl_io_ctx->inproc_ctrl_io_socket, "I-CONNECT-FAIL");
        goto free_return;
    }

    // prepare processing loop
    ctrl_zloop = zloop_new();
    assert(ctrl_zloop);

#ifdef DEBUG
    zloop_set_verbose(ctrl_zloop, 1);
#endif

    zmq_rv = zloop_reader(ctrl_zloop, ctrl_io_ctx->ctrl_socket, ctrl_io_ext_rcv,
                          ctrl_io_ctx);
    assert(zmq_rv == 0);
    zloop_reader_set_tolerant(ctrl_zloop, ctrl_io_ctx->ctrl_socket);

    zmq_rv = zloop_reader(ctrl_zloop, ctrl_io_ctx->inproc_ctrl_io_socket,
                          ctrl_io_inproc_rcv, ctrl_io_ctx);
    zloop_reader_set_tolerant(ctrl_zloop, ctrl_io_ctx->inproc_ctrl_io_socket);
    assert(zmq_rv == 0);

    // connection successful: inform main thread
    threadcom_send_data(ctrl_io_ctx->inproc_ctrl_io_socket, "I-CONNECT-OK",
                        &di_addr, sizeof(uint16_t));

    // start event loop -- takes over thread
    zloop_start(ctrl_zloop);

    // when we reach this point the loop has been terminated either through a
    // signal or a control message
    threadcom_send_status(ctrl_io_ctx->inproc_ctrl_io_socket, "I-DISCONNECT-OK");

free_return:
    zloop_destroy(&ctrl_zloop);

    zsock_destroy(&ctrl_io_ctx->ctrl_socket);
    zsock_destroy(&ctrl_io_ctx->inproc_ctrl_io_socket);

    free(ctrl_io_ctx->host_controller_address);
    ctrl_io_ctx->host_controller_address = NULL;

    free(ctrl_io_ctx);
    ctrl_io_ctx = NULL;

    return 0;
}

API_EXPORT
osd_result osd_hostmod_new(struct osd_hostmod_ctx **ctx,
                           struct osd_log_ctx *log_ctx)
{
    struct osd_hostmod_ctx *c = calloc(1, sizeof(struct osd_hostmod_ctx));
    assert(c);

    c->log_ctx = log_ctx;
    c->is_connected = 0;

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
osd_result osd_hostmod_connect(struct osd_hostmod_ctx *ctx,
                               const char* host_controller_address)
{
    int rv;
    assert(ctx);
    assert(ctx->is_connected == 0);

    // Establish connection with control I/O thread
    zsock_t *inproc_ctrl_io_socket = zsock_new_pair("@inproc://ctrl_io");
    assert(inproc_ctrl_io_socket);
    ctx->inproc_ctrl_io_socket = inproc_ctrl_io_socket;

    // To support I/O with timeouts (e.g. reading a register with a timeout)
    // we need the ZeroMQ receive functions to time out as well.
    // If fully blocking behavior is required, manually loop on the zmsg_recv()
    // calls.
    // We need to use a slightly higher timeout for the internal communication
    // than for the external communication: if an external communication fails,
    // the I/O thread must be able to recognize this by the timeout, and then
    // inform the main thread. If both threads follow the same timeout, the
    // I/O thread cannot inform the main thread of timeouts.
    zsock_set_rcvtimeo(ctx->inproc_ctrl_io_socket, 1.5 * ZMQ_RCV_TIMEOUT);

    // prepare I/O thread context (will be handed over to the thread)
    struct osd_hostmod_ctrl_io_ctx* ctrl_io_ctx = calloc(1, sizeof(struct osd_hostmod_ctrl_io_ctx));
    ctrl_io_ctx->host_controller_address = strdup(host_controller_address);
    assert(ctrl_io_ctx->host_controller_address);
    ctrl_io_ctx->log_ctx = ctx->log_ctx;

    // set up control I/O thread
    rv = pthread_create(&ctx->thread_ctrl_io, 0,
                        thread_ctrl_io_main, (void*)ctrl_io_ctx);
    assert(rv == 0);

    // wait for connection to be established
    zmsg_t *msg = zmsg_recv(ctx->inproc_ctrl_io_socket);
    if (!msg) {
        ctx->is_connected = 0;
        goto ret;
    }
    zframe_t *status_frame = zmsg_pop(msg);
    if (zframe_streq(status_frame, "I-CONNECT-OK")) {
        zframe_t *di_addr_frame = zmsg_pop(msg);
        assert(zframe_size(di_addr_frame) == sizeof(uint16_t));
        uint16_t *di_addr = (uint16_t*)zframe_data(di_addr_frame);
        ctx->diaddr = *di_addr;
        zframe_destroy(&di_addr_frame);

        ctx->is_connected = 1;
    } else if (zframe_streq(status_frame, "I-CONNECT-FAIL")) {
        ctx->is_connected = 0;
    }
    zframe_destroy(&status_frame);
    zmsg_destroy(&msg);

ret:
    if (!ctx->is_connected) {
        pthread_join(ctx->thread_ctrl_io, NULL);
        zsock_destroy(&ctx->inproc_ctrl_io_socket);
        err(ctx->log_ctx, "Unable to establish connection to host controller.\n");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    dbg(ctx->log_ctx, "Connection established, DI address is %u\n", ctx->diaddr);

    return OSD_OK;
}

API_EXPORT
int osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);

    return ctx->is_connected;
}
API_EXPORT
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);

    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    // signal control I/O thread to shut down
    threadcom_send_status(ctx->inproc_ctrl_io_socket, "I-DISCONNECT");

    // wait for disconnect to happen
    zmsg_t *msg = zmsg_recv(ctx->inproc_ctrl_io_socket);
    if (!msg) {
        return OSD_ERROR_FAILURE;
    }
    zframe_t *status_frame = zmsg_pop(msg);
    if (!zframe_streq(status_frame, "I-DISCONNECT-OK")) {
        zframe_destroy(&status_frame);
        zmsg_destroy(&msg);
        return OSD_ERROR_FAILURE;
    }
    zframe_destroy(&status_frame);
    zmsg_destroy(&msg);

    ctx->is_connected = 0;

    // wait until control I/O thread has finished its cleanup
    pthread_join(ctx->thread_ctrl_io, NULL);

    zsock_destroy(&ctx->inproc_ctrl_io_socket);

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

    assert(ctx->is_connected == 0);

    free(ctx);
    *ctx_p = NULL;
}

static osd_result osd_hostmod_regaccess(struct osd_hostmod_ctx *ctx,
                                        uint16_t module_addr,
                                        uint16_t reg_addr,
                                        enum osd_packet_type_reg_subtype subtype_req,
                                        enum osd_packet_type_reg_subtype subtype_resp,
                                        uint16_t *wr_data,
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
            "Device returned error packet %u when accessing the register.\n",
            osd_packet_get_type_sub(pkg_resp));
        retval = OSD_ERROR_DEVICE_ERROR;
        goto err_free_resp;
    }

    // validate response subtype
    if (osd_packet_get_type_sub(pkg_resp) != subtype_resp) {
        err(ctx->log_ctx, "Expected register response of subtype %d, got %d\n",
            subtype_resp,
            osd_packet_get_type_sub(pkg_resp));
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
                                uint16_t module_addr, uint16_t reg_addr,
                                int reg_size_bit, void *result, int flags)
{
    osd_result rv;
    osd_result retval;

    assert(reg_size_bit % 16 == 0 && reg_size_bit <= 128);

    dbg(ctx->log_ctx, "Issuing %d bit read request to register 0x%x of module "
        "0x%x\n", reg_size_bit, reg_addr, module_addr);

    struct osd_packet *response_pkg;
    rv = osd_hostmod_regaccess(ctx, module_addr, reg_addr,
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
            "response, got %d.\n",
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
                                 uint16_t module_addr, uint16_t reg_addr,
                                 int reg_size_bit,
                                 void *data, int flags)
{
    assert(reg_size_bit % 16 == 0 && reg_size_bit <= 128);

    osd_result rv;
    osd_result retval;

    dbg(ctx->log_ctx, "Issuing %d bit write request to register 0x%x of module "
        "0x%x\n", reg_size_bit, reg_addr, module_addr);

    struct osd_packet *response_pkg;
    rv = osd_hostmod_regaccess(ctx, module_addr, reg_addr,
                               get_subtype_reg_write_req(reg_size_bit),
                               RESP_WRITE_REG_SUCCESS,
                               data, reg_size_bit / 16,
                               &response_pkg, flags);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    // validate response size
    if (response_pkg->data_size_words != 0) {
        err(ctx->log_ctx, "Expected packet with no payload, got %d payload words.\n",
            response_pkg->data_size_words);
        retval = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    retval = OSD_OK;

err_free_resp:
    free(response_pkg);

    return retval;
}
