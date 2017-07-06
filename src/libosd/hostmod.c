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
 *   Stefan Wallentowitz <stefan@wallentowitz.de>
 */


#include <osd/osd.h>
#include <osd/hostmod.h>
#include "osd-private.h"
#include "packet.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "hostmod-private.h"

/** Timeout when accessing debug registers (in ns) */
#define REG_ACCESS_TIMEOUT_NS (1*1000*1000*1000) // 1 s

/** Debugging aid: log all sent and received packets */
#define LOG_TRANSMITTED_PACKETS 1

static osd_result osd_hostmod_send_packet(struct osd_hostmod_ctx *ctx,
                                          struct osd_packet *packet)
{
    int rv;
    zmsg_t *msg = zmsg_new();
    assert(msg);

    rv = zmsg_addstr(msg, "D");
    assert(rv == 0);
    dbg(ctx->log_ctx, "siez: %d\n", packet->size_data);
    rv = zmsg_addmem(msg, packet->data_raw, osd_packet_sizeof(packet));
    assert(rv == 0);

    zmsg_print(msg);
    rv = zmsg_send(&msg, ctx->socket);

    if (rv == 0) {
        return OSD_OK;
    } else {
        return OSD_ERROR_COM;
    }

    return OSD_OK;
}

/**
 * Read the system information from the device, as stored in the SCM
 */
static osd_result read_system_info_from_device(struct osd_hostmod_ctx *ctx)
{
    osd_result rv;

    rv = osd_hostmod_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_SYSTEM_VENDOR_ID, 16,
                          &ctx->system_info.vendor_id, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read VENDOR_ID from SCM (rv=%d)\n", rv);
        return rv;
    }
    rv = osd_hostmod_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_SYSTEM_DEVICE_ID, 16,
                          &ctx->system_info.device_id, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read DEVICE_ID from SCM (rv=%d)\n", rv);
        return rv;
    }
    rv = osd_hostmod_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_MAX_PKT_LEN, 16,
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
    rv = osd_hostmod_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_NUM_MOD, 16,
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
 * Get the size of the Debug Transport Datagram in words
 */
size_t osd_dtd_get_size_words(osd_dtd dtd)
{
    if (!dtd) {
        return 0;
    }

    /*
     * The first word in the DTD always contains the size. We then need to add
     * the word containing the size itself.
     */
    return dtd[0] + 1;
}

/**
 * Get the Debug Transport Datagram representation of a packet
 *
 * Note that the ownership of the data in @p dtd remains with the packet, do
 * not free @p dtd.
 *
 * @param[in] packet packet to get the DTD from
 * @param[out] dtd   DTD representation of the packet
 * @return OSD_OK on success, any other value indicates an error
 */
osd_result osd_packet_get_dtd(struct osd_packet *packet, osd_dtd *dtd)
{
    *dtd = &packet->size_data;
    return OSD_OK;
}

/**
 * Make a packet out of a Debug Transport Datagram
 *
 * The ownership of the @p dtd is transferred to the packet, @p dtd may not be
 * used or freed anymore after calling this function.
 */
osd_result osd_dtd_to_packet(osd_dtd dtd, struct osd_packet** packet)
{
    *packet = (struct osd_packet*)dtd;
    return OSD_OK;
}

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

static int rcv_msg(zloop_t *loop, zsock_t *reader, void *ctx_void)
{
    struct osd_hostmod_ctx* ctx = (struct osd_hostmod_ctx*)ctx_void;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process interrupted
    }

    zframe_t *type_frame = zmsg_pop(msg);
    if (zframe_streq(type_frame, "D")) {

        zframe_t *data_frame = zmsg_pop(msg);
        assert(data_frame);
        uint16_t *data = (uint16_t*)zframe_data(data_frame);
        size_t data_size_words = zframe_size(data_frame) / sizeof(uint16_t);
        assert(data);

        struct osd_packet *packet;
        osd_packet_new(&packet, data_size_words);

        handle_incoming_di_packet(ctx, packet);

    } else if (zframe_streq(type_frame, "M")) {
        // TODO: handle incoming management messages
        err(ctx->log_ctx, "XXX: management messages are not yet handled by this client.\n");
        goto ret;

    } else {
        err(ctx->log_ctx, "Message of unknown type received. Ignoring.\n");
        goto ret;
    }

ret:
    zmsg_destroy(&msg);
    return 0;
}
/**
 * Read data from the device encoded as Debug Transport Datagrams (DTDs)
 */
static void* thread_ctrl_receive(void *ctx_void)
{
    struct osd_hostmod_ctx *ctx = (struct osd_hostmod_ctx*) ctx_void;
    ssize_t rv;

    // ingress data path
    zloop_t* rx_zloop = zloop_new();
    zloop_set_verbose(rx_zloop, 1);
    rv = zloop_reader(rx_zloop, ctx->socket, rcv_msg, ctx);
    assert(rv == 0);
    zloop_reader_set_tolerant(rx_zloop, ctx->socket);
    ctx->rx_zloop = rx_zloop;

    // start event loop -- takes over thread
    zloop_start(ctx->rx_zloop);

    return NULL;
}

/**
 * Create new communication API object
 *
 * @param[out] ctx the osd_com context to be created
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

API_EXPORT
uint16_t osd_hostmod_get_addr(struct osd_hostmod_ctx *ctx)
{
    assert(ctx->is_connected);
    return ctx->addr;
}

/**
 * Connect to the host controller
 *
 * @param ctx the osd_com context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_disconnect()
 */
osd_result osd_hostmod_connect(struct osd_hostmod_ctx *ctx,
                               char* host_controller_address)
{
    int rv;
    assert(ctx->is_connected == 0);

    zsock_t *sock = zsock_new_dealer(host_controller_address);
    if (!sock) {
        err(ctx->log_ctx, "Unable to connect to %s\n", host_controller_address);
        return OSD_ERROR_CONNECTION_FAILED;
    }
    ctx->socket = sock;

    // get address for this host module
    // request
    zmsg_t *msg_req = zmsg_new();
    assert(msg_req);

    rv = zmsg_addstr(msg_req, "M");
    assert(rv == 0);
    rv = zmsg_addstr(msg_req, "ADDR_REQUEST");
    assert(rv == 0);
    rv = zmsg_send(&msg_req, ctx->socket);
    if (rv != 0) {
        err(ctx->log_ctx, "Unable to send ADDR_REQUEST request to host controller\n");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    // response
    zmsg_t *msg_resp = zmsg_recv(sock);
    if (!msg_resp) {
        err(ctx->log_ctx, "No response received from host controller\n");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    zframe_t *type_frame = zmsg_pop(msg_resp);
    assert(zframe_streq(type_frame, "M"));

    char* addr_string = zmsg_popstr(msg_resp);
    assert(addr_string);

    char* end;
    int addr = strtol(addr_string, &end, 10);
    assert(!*end);
    assert(addr < UINT16_MAX);
    ctx->addr = (uint16_t) addr;

    // set up thread_ctrl_receive
    rv = pthread_create(&ctx->thread_ctrl_receive, 0,
                        thread_ctrl_receive, (void*)ctx);
    if (rv) {
        err(ctx->log_ctx, "Unable to create thread_ctrl_receive: %d\n", rv);
        return OSD_ERROR_FAILURE;
    }

    ctx->is_connected = 1;

    read_system_info_from_device(ctx);

    return OSD_OK;
}

/**
 * Is the connection to the device active?
 *
 * @param ctx the osd_com context object
 * @return 1 if connected, 0 if not connected
 *
 * @see osd_hostmod_connect()
 * @see osd_hostmod_disconnect()
 */
API_EXPORT
int osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx)
{
    return ctx->is_connected;
}

/**
 * Shut down all communication with the device
 *
 * @param ctx the osd_com context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_run()
 */
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx)
{
    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    ctx->is_connected = 0;

    zloop_destroy(&ctx->rx_zloop);

    void *status;
    pthread_cancel(ctx->thread_ctrl_receive);
    pthread_join(ctx->thread_ctrl_receive, &status);

    zsock_destroy(&ctx->socket);

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
 * @param flags flags. Set OSD_COM_WAIT_FOREVER to block indefinetly until the
 *              access succeeds.
 * @return OSD_OK on success, any other value indicates an error
 * @return OSD_ERROR_TIMEDOUT if the register read timed out (only if
 *         OSD_COM_WAIT_FOREVER is not set)
 */
API_EXPORT
osd_result osd_hostmod_reg_read(struct osd_hostmod_ctx *ctx,
                                const unsigned int module_addr,
                                const uint16_t reg_addr, const int reg_size_bit,
                                void *result, const int flags)
{
    if (!ctx->is_connected) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    int use_timeout;
    struct timespec abs_timeout;
    clock_gettime(CLOCK_REALTIME, &abs_timeout);
    if (flags & OSD_COM_WAIT_FOREVER) {
        use_timeout = 0;
    } else {
        use_timeout = 1;
        timespec_add_ns(&abs_timeout, REG_ACCESS_TIMEOUT_NS);
    }

    osd_result ret = OSD_OK;
    osd_result rv;

    dbg(ctx->log_ctx, "Issuing %d bit read request to register 0x%x of module "
        "0x%x\n", reg_size_bit, reg_addr, module_addr);

    /*
     * XXX: This lock is overly protective. We could use one lock per
     * |module_addr|, not one for the the whole system. Switch to finer grained
     * locking if needed.
     */
    if (use_timeout) {
        int r = pthread_mutex_timedlock(&ctx->reg_access_lock, &abs_timeout);
        if (r == ETIMEDOUT) {
            ret = OSD_ERROR_TIMEDOUT;
            goto err_unlock;
        } else if (r != 0) {
            ret = OSD_ERROR_FAILURE;
            goto err_unlock;
        }
    } else {
        int r = pthread_mutex_lock(&ctx->reg_access_lock);
        if (r != 0) {
            ret = OSD_ERROR_FAILURE;
            goto err_unlock;
        }
    }

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
                        osd_packet_get_size_data_from_payload(1));
    if (OSD_FAILED(rv)) {
        ret = rv;
        goto err_free_req;
    }

    osd_packet_set_header(pkg_read_req, module_addr, ctx->addr,
                          OSD_PACKET_TYPE_REG, type_sub);
    pkg_read_req->data.payload[0] = reg_addr;


    // send register read request
    rv = osd_hostmod_send_packet(ctx, pkg_read_req);
    if (OSD_FAILED(rv)) {
        ret = rv;
        goto err_free_req;
    }

    // wait for response
    if (use_timeout) {
        int r = pthread_cond_timedwait(&ctx->reg_access_complete,
                                       &ctx->reg_access_lock, &abs_timeout);
        if (r == ETIMEDOUT) {
            ret = OSD_ERROR_TIMEDOUT;
            goto err_free_req;
        } else if (r != 0) {
            ret = OSD_ERROR_FAILURE;
            goto err_free_req;
        }
    } else {
        int r = pthread_cond_wait(&ctx->reg_access_complete,
                                  &ctx->reg_access_lock);
        if (r != 0) {
            ret = OSD_ERROR_FAILURE;
            goto err_free_req;
        }
    }

    // parse response
    struct osd_packet *pkg_read_resp = ctx->rcv_ctrl_packet;
    assert(osd_packet_get_type(pkg_read_resp) == OSD_PACKET_TYPE_REG);

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
        assert(0); // API calling error
    }

    // handle register read error
    if (osd_packet_get_type_sub(pkg_read_resp) == RESP_READ_REG_ERROR) {
        err(ctx->log_ctx, "Device returned RESP_READ_REG_ERROR as register "
            "read response.\n");
        ret = OSD_ERROR_DEVICE_ERROR;
        goto err_free_resp;
    }

    // validate response subtype
    if (osd_packet_get_type_sub(pkg_read_resp) != resp_type_sub_exp) {
        err(ctx->log_ctx, "Expected register response of subtype %d, got %d\n",
            resp_type_sub_exp,
            osd_packet_get_type_sub(pkg_read_resp));
        ret = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    // validate response size
    unsigned int exp_size_data_words = 3 /* DP_HEADER_{1-3} */ +
            reg_size_bit / 16 /* payload */;
    if (pkg_read_resp->size_data != exp_size_data_words) {
        err(ctx->log_ctx, "Expected %d 16 bit data words in register read "
            "response, got %d.\n",
            exp_size_data_words,
            pkg_read_resp->size_data);
        ret = OSD_ERROR_DEVICE_INVALID_DATA;
        goto err_free_resp;
    }

    // make result available to caller
    memcpy(result, pkg_read_resp->data.payload, reg_size_bit / 8);


err_free_resp:
    free(ctx->rcv_ctrl_packet);
    ctx->rcv_ctrl_packet = NULL;

err_free_req:
    free(pkg_read_req);

err_unlock:
    pthread_mutex_unlock(&ctx->reg_access_lock);

    return ret;
}
