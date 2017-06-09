/**
 * Open SoC Debug Communication Library
 *
 * The communication library handles all low-level communication detail between
 * the device and OSD Clients. The device is usually the hardware on which Open
 * SoC Debug is integrated. OSD Clients are libraries or applications using
 * the functionality of the OSD Communication API to interact with a device, or
 * to receive data (e.g. traces) from it.
 *
 * The physical interface (i.e. low-level communication) is not implemented in
 * the communication library. Instead, the API provides hooks to connect
 * different physical communication libraries to it.
 *
 * Every device connects to one communication library. To enable the use case of
 * multiple tools (such as a run-control debugger like GDB and a trace debugger)
 * talking to the same OSD-enabled device, the communication library supports
 * multiple clients.
 */

#include <osd/osd.h>
#include <osd/com.h>

#include "osd-private.h"
#include "com-private.h"
#include "packet.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

/* Debugging aid: log all sent and received packets */
#define LOG_TRANSMITTED_PACKETS 1

/**
 * Read the system information from the device, as stored in the SCM
 */
static osd_result read_system_info_from_device(struct osd_com_ctx *ctx)
{
    osd_result rv;

    rv = osd_com_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_SYSTEM_VENDOR_ID, 16,
                          &ctx->system_info.vendor_id, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read VENDOR_ID from SCM\n");
        return OSD_ERROR_FAILURE;
    }
    rv = osd_com_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_SYSTEM_DEVICE_ID, 16,
                          &ctx->system_info.device_id, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read DEVICE_ID from SCM\n");
        return OSD_ERROR_FAILURE;
    }
    rv = osd_com_reg_read(ctx, OSD_MOD_ADDR_SCM, REG_SCM_MAX_PKT_LEN, 16,
                          &ctx->system_info.max_pkt_len, 0);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to read MAX_PKT_LEN from SCM\n");
        return OSD_ERROR_FAILURE;
    }


    dbg(ctx->log_ctx, "Got system information: VENDOR_ID = %u, DEVICE_ID = %u, "
        "MAX_PKT_LEN = %u\n\n", ctx->system_info.vendor_id,
        ctx->system_info.device_id, ctx->system_info.max_pkt_len);

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
static osd_result osd_com_send_packet(struct osd_com_ctx *ctx,
                                      struct osd_packet *packet)
{
    assert(ctx->device_ctrl_if->write);

#if LOG_TRANSMITTED_PACKETS
    dbg(ctx->log_ctx, "Sending packet to device\n");
    osd_packet_log(packet, ctx->log_ctx);
#endif

    ssize_t size_written;
    /*
     * To send the packet to the device it is encoded as Debug Transport
     * Datagram (DTD), a length-value encoded version of the Debug Packet.
     * The layout of struct osd_packet makes this encoding easy, as the length
     * of the packet is stored just before the debug packet itself.
     */
    size_t dtd_size_words = 1 /* length */ + packet->size_data;
    uint16_t *dtd = (uint16_t*)&packet->size_data;

    size_written = ctx->device_ctrl_if->write(dtd, dtd_size_words, 0);
    if (size_written < 0) {
        err(ctx->log_ctx, "Unable to write data to device. Return code: %zu\n",
            size_written);
    }
    if ((size_t)size_written != dtd_size_words) {
        err(ctx->log_ctx,
            "Tried to write %zu words to device, wrote %zu bytes.\n",
            dtd_size_words, size_written);
        return OSD_ERROR_COM;
    }

    return OSD_OK;
}

/**
 * Read data from the device encoded as Debug Transport Datagrams (DTDs)
 */
static void* thread_ctrl_receive(void *ctx_void)
{
    struct osd_com_ctx *ctx = (struct osd_com_ctx*) ctx_void;
    ssize_t rv;

    assert(ctx->device_ctrl_if->read);

    while (1) {
        // read packet size, which is transmitted as first word in a DTD
        uint16_t pkg_size_words;
        rv = ctx->device_ctrl_if->read(&pkg_size_words, 1, 0);
        if (rv != 1) {
            err(ctx->log_ctx, "Unable to receive data from device. "
                "Aborting.\n");
            return NULL;
        }

        // allocate buffer for the to-be-received packet
        assert(pkg_size_words >= 2);
        // we have two header words (DP_HEADER_1 and DP_HEADER_2, everything
        // else is payload
        unsigned int pacload_size = pkg_size_words - 2;
        struct osd_packet *packet;
        osd_packet_new(&packet, pacload_size);


        // read packet of |pkg_size| words from device
        rv = ctx->device_ctrl_if->read(packet->data_raw, pkg_size_words, 0);
        if (rv < 0) {
            // XXX: Figure out suitable handling of this error case
            err(ctx->log_ctx, "Error while receiving data from device. "
                    "Aborting.\n");
            assert(0);
        }
        if ((size_t)rv != pkg_size_words) {
            // XXX: Figure out suitable handling of this error case
            err(ctx->log_ctx, "Received too little data from device. This "
                "should not have happened. Aborting.");
            assert(0);
        }

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
}

/**
 * Create new communication API object
 *
 * @param[out] ctx the osd_com context to be created
 * @param[in] log_ctx the log context to be used. Set to NULL to disable logging
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_com_free()
 */
API_EXPORT
osd_result osd_com_new(struct osd_com_ctx **ctx, struct osd_log_ctx *log_ctx)
{
    struct osd_com_ctx *c = calloc(1, sizeof(struct osd_com_ctx));
    assert(c);

    pthread_mutex_init(&c->reg_access_lock, 0);
    pthread_cond_init(&c->reg_access_complete, 0);
    c->log_ctx = log_ctx;
    c->is_running = 0;

    *ctx = c;

    return OSD_OK;
}

/**
 * Connect to the device
 *
 * Before calling this function, make sure that the setup is complete:
 * - Use osd_com_set_device_ctrl_if() to connect the control interface to the
 *   device.
 * - Use osd_com_set_device_event_if() to connect the event interface to the
 *   device.
 *
 * @param ctx the osd_com context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_com_disconnect()
 */
osd_result osd_com_connect(struct osd_com_ctx *ctx)
{
    assert(ctx->is_running == 0);

    // set up thread_ctrl_receive
    int rv;
    rv = pthread_create(&ctx->thread_ctrl_receive, 0,
                        thread_ctrl_receive, (void*)ctx);
    if (rv) {
        err(ctx->log_ctx, "Unable to create thread_ctrl_receive: %d\n", rv);
        return OSD_ERROR_FAILURE;
    }

    ctx->is_running = 1;

    // retrieve system information
    osd_result osd_rv;
    osd_rv = read_system_info_from_device(ctx);
    if (OSD_FAILED(osd_rv)) {
        return osd_rv;
    }

    return OSD_OK;
}

/**
 * Shut down all communication with the device
 *
 * @param ctx the osd_com context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_com_run()
 */
osd_result osd_com_disconnect(struct osd_com_ctx *ctx)
{
    ctx->is_running = 0;

    void *status;
    pthread_cancel(ctx->thread_ctrl_receive);
    pthread_join(ctx->thread_ctrl_receive, &status);

    return OSD_OK;
}

/**
 * Free a communication API context object
 *
 * @param ctx the osd_com context object
 */
API_EXPORT
void osd_com_free(struct osd_com_ctx *ctx)
{
    assert(ctx->is_running == 0);

    pthread_mutex_destroy(&ctx->reg_access_lock);

    free(ctx);
}

/**
 * Get a list of all Open SoC Debug modules present in the device
 *
 * @param ctx the osd_com context object
 * @param module all modules available in the system
 * @param modules_len number of modules available in the system
 *
 * @return OSD_OK on success, any other value indicates an error
 */
API_EXPORT
osd_result osd_com_get_modules(struct osd_com_ctx *ctx,
                               struct osd_module_desc **modules,
                               size_t *modules_len)
{
    return OSD_OK;
#if 0
    if (!ctx->modules) {
        osd_result rv = enumerate_modules(ctx);
        if (OSD_FAILED(rv)) {
            return rv;
        }
    }

    *modules = ctx->modules;
    *modules_len = ctx->modules_len;

    return OSD_OK;
#endif
}

/**
 * Set the descriptor to communicate with the control interface of the target
 * device
 *
 * The byte order of all sent and received data is expected to be in the form
 * set in osd_com_set_device_transport_byte_order().
 *
 * @param ctx the osd_com context object
 * @param ctrl_if device descriptor
 *
 * @return OSD_OK on success, any other value indicates an error
 */
API_EXPORT
osd_result osd_com_set_device_ctrl_if(struct osd_com_ctx *ctx,
                                      struct osd_com_device_if *ctrl_if)
{
    ctx->device_ctrl_if = ctrl_if;
    return OSD_OK;
}

/**
 * Set the descriptor to communicate with the event interface of the target
 * device
 *
 * The byte order of all sent and received data is expected to be in the form
 * set in osd_com_set_device_transport_byte_order().
 *
 * @param ctx the osd_com context object
 * @param ctrl_if device descriptor
 *
 * @return OSD_OK on success, any other value indicates an error
 */
API_EXPORT
osd_result osd_com_set_device_event_if(struct osd_com_ctx *ctx,
                                       struct osd_com_device_if *event_if)
{
    ctx->device_event_if = event_if;
    return OSD_OK;
}

/**
 * Read a register of a module in the debug system
 *
 * This function blocks until the register read has completed.
 */
API_EXPORT
osd_result osd_com_reg_read(struct osd_com_ctx *ctx,
                            const unsigned int module_addr,
                            const uint16_t reg_addr, const int reg_size_bit,
                            void *result, const int flags)
{
    assert(ctx->is_running);

    osd_result ret = OSD_OK;
    osd_result rv;

    dbg(ctx->log_ctx, "Issuing %d bit read request to register 0x%x of module "
        "0x%x\n", reg_size_bit, reg_addr, module_addr);

    /*
     * XXX: This lock is overly protective. We could use one lock per
     * |module_addr|, not one for the the whole system. Switch to finer grained
     * locking if needed.
     */
    pthread_mutex_lock(&ctx->reg_access_lock);

    // |flags| is currently unused
    assert(flags == 0);

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
    rv = osd_packet_new(&pkg_read_req, 1);
    if (OSD_FAILED(rv)) {
        ret = rv;
        goto err_free_req;
    }

    osd_packet_set_header(pkg_read_req, module_addr, OSD_MOD_ADDR_HIM,
                          OSD_PACKET_TYPE_REG, type_sub);
    pkg_read_req->data.payload[0] = reg_addr;


    // send register read request
    rv = osd_com_send_packet(ctx, pkg_read_req);
    if (OSD_FAILED(rv)) {
        ret = rv;
        goto err_free_req;
    }

    // wait for response
    pthread_cond_wait(&ctx->reg_access_complete, &ctx->reg_access_lock);

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
    unsigned int exp_size_data_words = 2 /* DP_HEADER_1 and DP_HEADER_2 */ +
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

    pthread_mutex_unlock(&ctx->reg_access_lock);

    return ret;
}
