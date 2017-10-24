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
#include "osd-private.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "hostmod-private.h"

/** Timeout when accessing debug registers (in ns) */
#define REG_ACCESS_TIMEOUT_NS (1*1000*1000*1000) // 1 s

/** Receive timeout when talking to the host controller over ZeroMQ */
#define ZMQ_RCV_TIMEOUT (1*1000) // 1 s

/** Debugging aid: log all sent and received packets */
#define LOG_TRANSMITTED_PACKETS 1

/**
 * Send a DI Packet
 */
static osd_result osd_hostmod_send_packet(struct osd_hostmod_ctx *ctx,
                                          struct osd_packet *packet)
{
    int rv;
    zmsg_t *msg = zmsg_new();
    assert(msg);

    rv = zmsg_addstr(msg, "DI_PKG");
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
 * Receive a DI Packet
 */
static osd_result osd_hostmod_receive_packet(struct osd_hostmod_ctx *ctx,
                                             struct osd_packet **packet)
{
    osd_result osd_rv;

    zmsg_t* msg = zmsg_recv(ctx->inproc_ctrl_io_socket);

    // ensure that the message we got from the I/O thread is packet data
    // XXX: possibly extend to hand off non DI_PKG messages to their appropriate
    // handler
    zframe_t *type_frame = zmsg_pop(msg);
    assert(type_frame);
    assert(zframe_streq(type_frame, "DI_PKG"));
    zframe_destroy(&type_frame);

    // get osd_packet from frame data
    zframe_t *data_frame = zmsg_pop(msg);
    assert(data_frame);
    uint16_t *data = (uint16_t*)zframe_data(data_frame);
    size_t data_size_bytes = zframe_size(data_frame);
    size_t data_size_words = data_size_bytes / sizeof(uint16_t);
    assert(data);
    assert(data_size_words >= 3); // 3 header words

    struct osd_packet *p;
    osd_rv = osd_packet_new(&p, data_size_words);
    assert(OSD_SUCCEEDED(osd_rv));
    memcpy(p->data_raw, data, data_size_bytes);

    zframe_destroy(&data_frame);
    zmsg_destroy(&msg);

    *packet = p;

    return OSD_OK;
}

/**
 * Read the system information from the device, as stored in the SCM
 */
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

static osd_result discover_debug_module(struct osd_hostmod_ctx *ctx,
                                        uint16_t module_addr)
{
    assert(module_addr < ctx->modules_len);

    osd_result rv;
    struct osd_module_desc* desc;
    desc = &ctx->modules[module_addr];

    desc->addr = module_addr;

    rv = osd_hostmod_reg_read(ctx, module_addr, REG_BASE_MOD_TYPE, 16,
                          &desc->type, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    rv = osd_hostmod_reg_read(ctx, module_addr, REG_BASE_MOD_VENDOR, 16,
                          &desc->vendor, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    rv = osd_hostmod_reg_read(ctx, module_addr, REG_BASE_MOD_VERSION, 16,
                          &desc->version, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    return OSD_OK;
}

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


#if 0
/**
 * Send out a packet to the device
 *
 * The packet is encoded as Debug Transport Datagram (DTD), as specified in the
 * HIM specification.
 *
 * @param ctx communication library context
 * @param packet debug packet to be transmitted
 * @return OSD_OK if sending was successful, any other value indicates an error
 */
static osd_result osd_hostmod_send_packet(struct osd_hostmod_ctx *ctx,
                                      struct osd_packet *packet)
{
    assert(ctx->device_ctrl_if->write);

#if LOG_TRANSMITTED_PACKETS
    dbg(ctx->log_ctx, "Sending packet to device\n");
    osd_packet_log(packet, ctx->log_ctx);
#endif

    osd_result rv;
    ssize_t size_written;
    osd_dtd dtd;

    rv = osd_packet_get_dtd(packet, &dtd);
    assert(OSD_SUCCEEDED(rv));

    size_written = ctx->device_ctrl_if->write(dtd, osd_dtd_get_size_words(dtd),
                                              0);
    if (size_written < 0) {
        err(ctx->log_ctx, "Unable to write data to device. Return code: %zu\n",
            size_written);
    }
    if ((size_t)size_written != osd_dtd_get_size_words(dtd)) {
        err(ctx->log_ctx,
            "Tried to write %zu words to device, wrote %zu bytes.\n",
            osd_dtd_get_size_words(dtd), size_written);
        return OSD_ERROR_COM;
    }

    return OSD_OK;
}
#endif

static void handle_incoming_di_packet(struct osd_hostmod_ctx *ctx,
                                      struct osd_packet* packet)
{
#if LOG_TRANSMITTED_PACKETS
    dbg(ctx->log_ctx, "Received new packet\n");
    osd_packet_log(packet, ctx->log_ctx);
#endif

    switch (osd_packet_get_type(packet)) {
    case OSD_PACKET_TYPE_REG:
        pthread_mutex_lock(&ctx->reg_access_lock);
        // the previous control request must be handled by now
        assert(ctx->rcv_ctrl_packet == NULL);
        ctx->rcv_ctrl_packet = packet;
        pthread_cond_signal(&ctx->reg_access_complete);
        pthread_mutex_unlock(&ctx->reg_access_lock);
        break;
    case OSD_PACKET_TYPE_EVENT:
        // XXX: forward to appropriate handler
        break;
    case OSD_PACKET_TYPE_PLAIN:
        // PLAIN packets should only be sent out, not received (as of now)
    case OSD_PACKET_TYPE_RES:
        // must be ignored by spec
    default:
        // Should never be reached.
        assert(0);
    }
}

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
    char *action = zmsg_popstr(msg);

    if (!strcmp(action, "DISCONNECT")) {
        // End ctrl_io thread by returning -1, which will terminate zloop
        retval = -1;
        goto free_return;

    } else if (!strcmp(action, "DI_PKG")) {
        printf("got DI_PKG to forward to other socket\n");
        // Forward a osd_packet in the second frame to the host controller as
        // data message
        zframe_t *di_pkg_frame = zmsg_pop(msg);
        assert(di_pkg_frame);

        zmsg_t *data_msg = zmsg_new();
        assert(data_msg);
        rv = zmsg_addstr(data_msg, "D");
        assert(rv == 0);
        rv = zmsg_append(data_msg, &di_pkg_frame);
        assert(rv == 0);

        rv = zmsg_send(&data_msg, ctx->ctrl_socket);
        assert(rv == 0);
        printf("sent out di packet to control socket\n");

    } else {
        assert(0 && "Unknown threadcom message received.");
    }

    retval = 0;
free_return:
    zmsg_destroy(&msg);
    free(action);
    return retval;
}

/**
 * Process incoming messages from the host controller
 */
static int ctrl_io_ext_rcv(zloop_t *loop, zsock_t *reader, void *ctx_void)
{
    struct osd_hostmod_ctrl_io_ctx* ctx = (struct osd_hostmod_ctrl_io_ctx*)ctx_void;
    int rv;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }

    zframe_t *type_frame = zmsg_pop(msg);
    assert(type_frame);
    if (zframe_streq(type_frame, "D")) {
        // forward data messages to the main thread
        zframe_t *di_pkg_frame = zmsg_pop(msg);
        assert(di_pkg_frame);

        zmsg_t *data_msg = zmsg_new();
        assert(data_msg);
        rv = zmsg_addstr(data_msg, "DI_PKG");
        assert(rv == 0);
        rv = zmsg_append(data_msg, &di_pkg_frame);
        assert(rv == 0);

        zmsg_send(&data_msg, ctx->inproc_ctrl_io_socket);

    } else if (zframe_streq(type_frame, "M")) {
        // TODO: handle incoming management messages
        err(ctx->log_ctx, "XXX: management messages are not yet handled by this client.\n");

    } else {
        assert(0 && "Message of unknown type received.");
    }

    zframe_destroy(&type_frame);
    zmsg_destroy(&msg);

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
        err(ctx->log_ctx, "No response received from host controller: %s (%d)\n",
            strerror(errno), errno);
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
        threadcom_send_status(ctrl_io_ctx->inproc_ctrl_io_socket, "CONNECT_FAIL");
        goto free_return;
    }

    zsock_set_rcvtimeo(ctrl_io_ctx->ctrl_socket, ZMQ_RCV_TIMEOUT);

    // Get our DI address
    uint16_t di_addr;
    osd_rv = obtain_diaddr(ctrl_io_ctx, &di_addr);
    if (OSD_FAILED(osd_rv)) {
        threadcom_send_status(ctrl_io_ctx->inproc_ctrl_io_socket, "CONNECT_FAIL");
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
    threadcom_send_data(ctrl_io_ctx->inproc_ctrl_io_socket, "CONNECT_OK",
                        &di_addr, sizeof(uint16_t));

    // start event loop -- takes over thread
    zloop_start(ctrl_zloop);

    // when we reach this point the loop has been terminated either through a
    // signal or a control message
    threadcom_send_status(ctrl_io_ctx->inproc_ctrl_io_socket, "DISCONNECT_OK");

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

/**
 * Create new osd_hostmod instance
 *
 * @param[out] ctx the osd_hostmod_ctx context to be created
 * @param[in] log_ctx the log context to be used. Set to NULL to disable logging
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_free()
 */
API_EXPORT
osd_result osd_hostmod_new(struct osd_hostmod_ctx **ctx,
                           struct osd_log_ctx *log_ctx)
{
    struct osd_hostmod_ctx *c = calloc(1, sizeof(struct osd_hostmod_ctx));
    assert(c);

    pthread_mutex_init(&c->reg_access_lock, 0);
    pthread_cond_init(&c->reg_access_complete, 0);
    c->log_ctx = log_ctx;
    c->is_connected = 0;

    *ctx = c;

    return OSD_OK;
}

/**
 * Get the DI address assigned to this host debug module
 *
 * The address is assigned during the connection, i.e. you need to call
 * osd_hostmod_connect() before calling this function.
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return the address assigned to this debug module
 */
API_EXPORT
uint16_t osd_hostmod_get_addr(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);
    assert(ctx->is_connected);
    return ctx->addr;
}

/**
 * Connect to the host controller
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_disconnect()
 */
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
        return OSD_ERROR_CONNECTION_FAILED;
    }
    zframe_t *status_frame = zmsg_pop(msg);
    if (zframe_streq(status_frame, "CONNECT_OK")) {
        zframe_t *di_addr_frame = zmsg_pop(msg);
        assert(zframe_size(di_addr_frame) == sizeof(uint16_t));
        uint16_t *di_addr = (uint16_t*)zframe_data(di_addr_frame);
        ctx->addr = *di_addr;
        zframe_destroy(&di_addr_frame);

        ctx->is_connected = 1;
    } else if (zframe_streq(status_frame, "CONNECT_FAIL")) {
        ctx->is_connected = 1;
    }
    zframe_destroy(&status_frame);
    zmsg_destroy(&msg);

    if (!ctx->is_connected) {
        return OSD_ERROR_CONNECTION_FAILED;
    }

    dbg(ctx->log_ctx, "Connection established, DI address is %u\n", ctx->addr);

    return OSD_OK;
}

/**
 * Is the connection to the device active?
 *
 * @param ctx the osd_hostmod context object
 * @return 1 if connected, 0 if not connected
 *
 * @see osd_hostmod_connect()
 * @see osd_hostmod_disconnect()
 */
API_EXPORT
int osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);

    return ctx->is_connected;
}

/**
 * Shut down all communication with the device
 *
 * @param ctx the osd_hostmod context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_run()
 */
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);

    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    // signal control I/O thread to shut down
    threadcom_send_status(ctx->inproc_ctrl_io_socket, "DISCONNECT");

    // wait for disconnect to happen
    zmsg_t *msg = zmsg_recv(ctx->inproc_ctrl_io_socket);
    if (!msg) {
        return OSD_ERROR_FAILURE;
    }
    zframe_t *status_frame = zmsg_pop(msg);
    if (!zframe_streq(status_frame, "DISCONNECT_OK")) {
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

/**
 * Free a communication API context object
 *
 * Call osd_hostmod_disconnect() before calling this function.
 *
 * @param ctx the osd_com context object
 */
API_EXPORT
void osd_hostmod_free(struct osd_hostmod_ctx *ctx)
{
    assert(ctx);
    assert(ctx->is_connected == 0);

    pthread_mutex_destroy(&ctx->reg_access_lock);

    free(ctx);
}

/**
 * Read a register of a module in the debug system
 *
 * Unless the flag OSD_COM_WAIT_FOREVER has been set this function waits up to
 * REG_ACCESS_TIMEOUT_NS ns for the register access to complete.
 *
 * @param ctx the osd_com context object
 * @param module_addr the module address to read the register from
 * @param reg_addr the address of the register to read
 * @param reg_size_bit size of the register in bit.
 *                     Supported values: 16, 32, 64 and 128.
 * @param[out] result the result of the register read. Preallocate a variable
 *                    large enough to hold @p reg_size_bit bits.
 * @param flags flags. Set OSD_COM_WAIT_FOREVER to block indefinitely until the
 *              access succeeds.
 * @return OSD_OK on success, any other value indicates an error
 * @return OSD_ERROR_TIMEDOUT if the register read timed out (only if
 *         OSD_COM_WAIT_FOREVER is not set)
 */
API_EXPORT
osd_result osd_hostmod_reg_read(struct osd_hostmod_ctx *ctx,
                                const uint16_t module_addr,
                                const uint16_t reg_addr, const int reg_size_bit,
                                void *result, const int flags)
{
    assert(ctx);
    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    osd_result retval = OSD_ERROR_FAILURE;
    osd_result rv;

    dbg(ctx->log_ctx, "Issuing %d bit read request to register 0x%x of module "
        "0x%x\n", reg_size_bit, reg_addr, module_addr);

    // determine type_sub
    enum osd_packet_type_reg_subtype type_sub;
    switch (reg_size_bit) {
    case 16:
        type_sub = REQ_READ_REG_16;
        break;
    case 32:
        type_sub = REQ_READ_REG_32;
        break;
    case 64:
        type_sub = REQ_READ_REG_64;
        break;
    case 128:
        type_sub = REQ_READ_REG_128;
        break;
    default:
        assert(0); // API calling error
    }

    // assemble request packet
    struct osd_packet *pkg_read_req;
    rv = osd_packet_new(&pkg_read_req,
                        osd_packet_get_data_size_words_from_payload(1));
    if (OSD_FAILED(rv)) {
        retval = rv;
        goto err_free_req;
    }

    osd_packet_set_header(pkg_read_req, module_addr, ctx->addr,
                          OSD_PACKET_TYPE_REG, type_sub);
    pkg_read_req->data.payload[0] = reg_addr;


    // send register read request
    rv = osd_hostmod_send_packet(ctx, pkg_read_req);
    if (OSD_FAILED(rv)) {
        retval = rv;
        goto err_free_req;
    }

    // wait for response
    struct osd_packet *pkg_read_resp;
    osd_hostmod_receive_packet(ctx, &pkg_read_resp);
    assert(osd_packet_get_type(pkg_read_resp) == OSD_PACKET_TYPE_REG);

    // parse response
    enum osd_packet_type_reg_subtype resp_type_sub_exp;
    switch (reg_size_bit) {
    case 16:
        resp_type_sub_exp = RESP_READ_REG_SUCCESS_16;
        break;
    case 32:
        resp_type_sub_exp = RESP_READ_REG_SUCCESS_32;
        break;
    case 64:
        resp_type_sub_exp = RESP_READ_REG_SUCCESS_64;
        break;
    case 128:
        resp_type_sub_exp = RESP_READ_REG_SUCCESS_128;
        break;
    default:
        assert(0 && "API calling error");
    }

    // handle register read error
    if (osd_packet_get_type_sub(pkg_read_resp) == RESP_READ_REG_ERROR) {
        err(ctx->log_ctx, "Device returned RESP_READ_REG_ERROR as register "
            "read response.\n");
        retval = OSD_ERROR_DEVICE_ERROR;
        goto err_free_resp;
    }

    // validate response subtype
    if (osd_packet_get_type_sub(pkg_read_resp) != resp_type_sub_exp) {
        err(ctx->log_ctx, "Expected register response of subtype %d, got %d\n",
            resp_type_sub_exp,
            osd_packet_get_type_sub(pkg_read_resp));
        retval = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    // validate response size
    unsigned int exp_data_size_words = 3 /* header */ +
            reg_size_bit / 16 /* payload */;
    if (pkg_read_resp->data_size_words != exp_data_size_words) {
        err(ctx->log_ctx, "Expected %d 16 bit data words in register read "
            "response, got %d.\n",
            exp_data_size_words,
            pkg_read_resp->data_size_words);
        retval = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    // make result available to caller
    memcpy(result, pkg_read_resp->data.payload, reg_size_bit / 8);


    retval = OSD_OK;

err_free_resp:
    free(pkg_read_resp);

err_free_req:
    free(pkg_read_req);

    return retval;
}
