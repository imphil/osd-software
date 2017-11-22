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
#include <osd/hostctrl.h>
#include <osd/packet.h>
#include "osd-private.h"
#include "worker.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>


/**
 * Host Controller context
 */
struct osd_hostctrl_ctx {
    /** Logging context */
    struct osd_log_ctx *log_ctx;

    /** DI subnet address */
    unsigned int subnet_addr;

    /** I/O worker context */
    struct worker_ctx *ioworker_ctx;

    /** Is the router running? */
    bool is_running;
};

struct iothread_usr_ctx {
    /** Host controller router socket */
    zsock_t *router_socket;

    /** ZeroMQ address/URL this host controller is bound to */
    char* router_address;

    /** Our DI subnet address */
    unsigned int subnet_addr;

    /** Debug modules registered in this subnet */
    zframe_t** mods_in_subnet;

    /** Gateways registered in this subnet */
    zframe_t** gateways;
};

/**
 * Get an available address in the local subnet
 */
static osd_result get_available_diaddr(struct worker_thread_ctx *thread_ctx,
                                       unsigned int *diaddr)
{
    assert(thread_ctx);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    unsigned int localaddr;
    for (localaddr = 1; localaddr <= OSD_DIADDR_LOCAL_MAX; localaddr++) {
        if (usrctx->mods_in_subnet[localaddr] == NULL) {
            *diaddr = osd_diaddr_build(usrctx->subnet_addr, localaddr);
            return OSD_OK;
        }
    }

    return OSD_ERROR_FAILURE;
}

/**
 * Register a host address for a given DI address
 */
static osd_result register_diaddr(struct worker_thread_ctx *thread_ctx,
                                  zframe_t* hostaddr, unsigned int diaddr)
{
    assert(thread_ctx);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    unsigned int localaddr = osd_diaddr_localaddr(diaddr);
    if (usrctx->mods_in_subnet[localaddr] != NULL) {
        return OSD_ERROR_FAILURE;
    }
    usrctx->mods_in_subnet[localaddr] = hostaddr;

#ifdef DEBUG
    char* hostaddr_str = zframe_strhex((zframe_t*)hostaddr);
    dbg(thread_ctx->log_ctx, "Registered diaddr %u.%u (%u) for host module %s",
        osd_diaddr_subnet(diaddr), osd_diaddr_localaddr(diaddr),
        diaddr, hostaddr_str);
    free(hostaddr_str);
#endif

    return OSD_OK;
}

static void mgmt_send_ack(struct worker_thread_ctx *thread_ctx, zframe_t* dest)
{
    assert(thread_ctx);
    assert(dest);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    zmsg_t* msg = zmsg_new();
    zmsg_add(msg, dest);
    zmsg_addstr(msg, "M");
    zmsg_addstr(msg, "ACK");
    zmsg_send(&msg, usrctx->router_socket);
}

static void mgmt_send_nack(struct worker_thread_ctx *thread_ctx, zframe_t* dest)
{
    assert(thread_ctx);
    assert(dest);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    zmsg_t* msg = zmsg_new();
    zmsg_add(msg, dest);
    zmsg_addstr(msg, "M");
    zmsg_addstr(msg, "NACK");
    zmsg_send(&msg, usrctx->router_socket);
}

/**
 * Assign a new DI address to a host module in our subnet
 */
static void mgmt_diaddr_request(struct worker_thread_ctx *thread_ctx,
                                zframe_t* hostaddr)
{
    assert(thread_ctx);
    assert(hostaddr);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result rv;
    unsigned int diaddr;
    rv = get_available_diaddr(thread_ctx, &diaddr);
    // XXX: Return error to host module instead of failing hard
    assert(OSD_SUCCEEDED(rv));

    rv = register_diaddr(thread_ctx, zframe_dup(hostaddr), diaddr);
    assert(OSD_SUCCEEDED(rv));

    zmsg_t* msg = zmsg_new();
    zmsg_add(msg, hostaddr);
    zmsg_addstr(msg, "M");
    zmsg_addstrf(msg, "%u", diaddr);
    zmsg_send(&msg, usrctx->router_socket);
}

static void mgmt_diaddr_release(struct worker_thread_ctx *thread_ctx,
                                zframe_t* hostaddr)
{
    assert(thread_ctx);
    assert(hostaddr);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    unsigned int i, localaddr;
    int found = 0;
    for (i = 1; i < OSD_DIADDR_SUBNET_MAX; i++) {
        if (zframe_eq(usrctx->mods_in_subnet[i], hostaddr)) {
            localaddr = i;
            found = 1;
            break;
        }
    }
    if (!found) {
        err(thread_ctx->log_ctx, "Trying to release address for host which "
            "isn't registered.");
        return mgmt_send_nack(thread_ctx, hostaddr);
    }

    zframe_destroy(&usrctx->mods_in_subnet[localaddr]);

#ifdef DEBUG
    char* hostaddr_str = zframe_strhex((zframe_t*)hostaddr);
    dbg(thread_ctx->log_ctx, "Releasing address %u for host module %s\n",
        localaddr, hostaddr_str);
    free(hostaddr_str);
#endif

    return mgmt_send_ack(thread_ctx, hostaddr);
}

static void mgmt_gw_register(struct worker_thread_ctx *thread_ctx,
                             zframe_t* hostaddr, const char* params)
{
    assert(thread_ctx);
    assert(hostaddr);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    char* end;

    unsigned int subnet = strtol(params, &end, 10);
    assert(!*end);
    assert(subnet <= OSD_DIADDR_SUBNET_MAX);

    if (usrctx->gateways[subnet] != NULL) {
        err(thread_ctx->log_ctx, "A gateway for subnet %u is already "
            "registered.", subnet);
        return mgmt_send_nack(thread_ctx, hostaddr);
    }

    usrctx->gateways[subnet] = hostaddr;

#ifdef DEBUG
    char* hostaddr_str = zframe_strhex((zframe_t*)hostaddr);
    dbg(thread_ctx->log_ctx, "Registered gateway %s for subnet %u", hostaddr_str, subnet);
    free(hostaddr_str);
#endif

    mgmt_send_ack(thread_ctx, hostaddr);
}

/**
 * Process an incoming management message (from the host modules)
 */
static void process_mgmt_msg(struct worker_thread_ctx *thread_ctx,
                             zframe_t* src, zframe_t* payload_frame)
{
    assert(thread_ctx);
    assert(src);
    assert(payload_frame);

    char* request = (char*)zframe_data(payload_frame);
    dbg(thread_ctx->log_ctx, "Received management message %s", request);

    if (!strcmp(request, "DIADDR_REQUEST")) {
        mgmt_diaddr_request(thread_ctx, src);
    } else if (!strcmp(request, "DIADDR_RELEASE")) {
        mgmt_diaddr_release(thread_ctx, src);
    } else if (!strncmp(request, "GW_REGISTER", strlen("GW_REGISTER"))) {
        mgmt_gw_register(thread_ctx, src, request + strlen("GW_REGISTER "));
    } else {
        mgmt_send_ack(thread_ctx, src);
    }

    // The ownership of |src| is passed to the called handler functions and must
    // be freed there.

    free(payload_frame);
}

/**
 * Route a DI data message to its destination
 */
static void process_data_msg(struct worker_thread_ctx *thread_ctx,
                             zframe_t* src, zframe_t* payload_frame)
{
    assert(thread_ctx);
    assert(src);
    assert(payload_frame);

    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result rv;

    struct osd_packet *pkg = NULL;
    rv = osd_packet_new_from_zframe(&pkg, payload_frame);
    if (OSD_FAILED(rv)) {
        err(thread_ctx->log_ctx, "Dropping invalid data packet (%d)", rv);
        goto free_return;
    }

    unsigned int dest_diaddr_subnet = osd_diaddr_subnet(osd_packet_get_dest(pkg));
    unsigned int dest_diaddr_local = osd_diaddr_localaddr(osd_packet_get_dest(pkg));

    dbg(thread_ctx->log_ctx,
        "Routing lookup for packet with destination %u.%u. Local subnet is %u.",
        dest_diaddr_subnet, dest_diaddr_local, usrctx->subnet_addr);

    const zframe_t* dest_hostaddr;
    if (dest_diaddr_subnet == usrctx->subnet_addr) {
        // routing inside our subnet
        dest_hostaddr = usrctx->mods_in_subnet[dest_diaddr_local];
        if (dest_hostaddr == NULL) {
            err(thread_ctx->log_ctx, "No destination module registered for "
                "DI address %u.%u", dest_diaddr_subnet, dest_diaddr_local);
            goto free_return;
        }
        dbg(thread_ctx->log_ctx,
            "Destination address is local, routing directly to destination.");
    } else {
        // routing through a gateway
        dest_hostaddr = usrctx->gateways[dest_diaddr_subnet];
        if (dest_hostaddr == NULL) {
            err(thread_ctx->log_ctx,
                "No gateway for subnet %u registered to route di address %u.%u",
                dest_diaddr_subnet, dest_diaddr_subnet, dest_diaddr_local);
            goto free_return;
        }
        dbg(thread_ctx->log_ctx, "Destination address is in a different "
            "subnet, routing through gateway.");
    }

#ifdef DEBUG
    char* dest_hostaddr_str = zframe_strhex((zframe_t*)dest_hostaddr);
    dbg(thread_ctx->log_ctx, "Routing data packet to %s", dest_hostaddr_str);
    free(dest_hostaddr_str);
#endif

    zmsg_t *msg = zmsg_new();
    assert(msg);
    zmsg_add(msg, zframe_dup(dest_hostaddr));
    zmsg_addstr(msg, "D");
    zmsg_add(msg, payload_frame);
    zmsg_send(&msg, usrctx->router_socket);

free_return:
    osd_packet_free(&pkg);
    zframe_destroy(&payload_frame);
}

/**
 * Process incoming messages
 *
 * @return 0 if the message was processed, -1 if @p loop should be terminated
 */
static int iothread_handle_ext_msg(zloop_t *loop, zsock_t *reader,
                                   void *thread_ctx_void)
{
    struct worker_thread_ctx* thread_ctx = (struct worker_thread_ctx*)thread_ctx_void;
    assert(thread_ctx);

    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }

    zframe_t *src_frame = zmsg_pop(msg);
    zframe_t *type_frame = zmsg_pop(msg);
    char* type_str = (char*)zframe_data(type_frame);

    if (type_str[0] == 'M') {
        zframe_t* payload_frame = zmsg_pop(msg);
        process_mgmt_msg(thread_ctx, src_frame, payload_frame);
    } else if (type_str[0] == 'D') {
        zframe_t* payload_frame = zmsg_pop(msg);
        process_data_msg(thread_ctx, src_frame, payload_frame);
    } else {
        err(thread_ctx->log_ctx, "Ignoring message of unknown type '%s'.",
            type_str);
        zframe_destroy(&src_frame);
    }

    zframe_destroy(&type_frame);
    zmsg_destroy(&msg);

    return 0;
}

/**
 * Start host controller router function in I/O thread
 *
 * This function is called by the worker as response to a I-START message.
 * It create a new ZeroMQ ROUTER socket acting as host controller and registers
 * an event handler function if new packages are received. After all startup
 * tasks are done a I-START-DONE message is sent to the main thread.
 */
static void iothread_router_start(struct worker_thread_ctx *thread_ctx)
{
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result retval;

    // create new ROUTER socket for host controller
    usrctx->router_socket = zsock_new_router(usrctx->router_address);
    if (!usrctx->router_socket) {
        err(thread_ctx->log_ctx, "Unable to bind to %s",
            usrctx->router_address);
        retval = OSD_ERROR_CONNECTION_FAILED;
        goto free_return;
    }
    zsock_set_rcvtimeo(usrctx->router_socket, ZMQ_RCV_TIMEOUT);

    // register event handler for incoming messages
    int zmq_rv;
    zmq_rv = zloop_reader(thread_ctx->zloop, usrctx->router_socket,
                          iothread_handle_ext_msg, thread_ctx);
    assert(zmq_rv == 0);
    zloop_reader_set_tolerant(thread_ctx->zloop, usrctx->router_socket);

free_return:
    worker_send_status(thread_ctx->inproc_socket, "I-START-DONE", retval);
}

/**
 * Stop the host controller router function in the I/O thread
 */
static void iothread_router_stop(struct worker_thread_ctx *thread_ctx)
{
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result retval;

    zloop_reader_end(thread_ctx->zloop, usrctx->router_socket);
    zsock_destroy(&usrctx->router_socket);

    retval = OSD_OK;

    worker_send_status(thread_ctx->inproc_socket, "I-STOP-DONE", retval);
}

static osd_result iothread_handle_inproc_msg(struct worker_thread_ctx *thread_ctx,
                                             const char *name, zmsg_t *msg)
{
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    if (!strcmp(name, "I-START")) {
        iothread_router_start(thread_ctx);

    } else if (!strcmp(name, "I-STOP")) {
        iothread_router_stop(thread_ctx);

    } else {
        assert(0 && "Received unknown message from main thread.");
    }

    return OSD_OK;
}

static osd_result iothread_destroy(struct worker_thread_ctx *thread_ctx)
{
    assert(thread_ctx);
    struct iothread_usr_ctx *usrctx = thread_ctx->usr;
    assert(usrctx);

    free(usrctx->router_address);
    free(usrctx->mods_in_subnet);
    free(usrctx->gateways);
    free(usrctx);
    thread_ctx->usr = NULL;

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostctrl_new(struct osd_hostctrl_ctx **ctx,
                            struct osd_log_ctx *log_ctx,
                            const char* router_address)
{
    osd_result rv;

    struct osd_hostctrl_ctx *c = calloc(1, sizeof(struct osd_hostctrl_ctx));
    assert(c);

    c->log_ctx = log_ctx;
    c->is_running = false;

    // prepare custom data passed to I/O thread
    struct iothread_usr_ctx *iothread_usr_data =
            calloc(1, sizeof(struct iothread_usr_ctx));
    assert(iothread_usr_data);

    iothread_usr_data->router_address = strdup(router_address);

    // Our subnet: always 1 for now.
    // XXX: make this dynamic
    iothread_usr_data->subnet_addr = 1;

    // allocate routing lookup tables
    // mods_in_subnet is 1024 * 8B = 8 kB
    iothread_usr_data->mods_in_subnet =
            calloc(OSD_DIADDR_LOCAL_MAX + 1, sizeof(zframe_t*));
    assert(iothread_usr_data->mods_in_subnet);
    // gateways is 64 * 8B = 1 kB
    iothread_usr_data->gateways =
            calloc(OSD_DIADDR_SUBNET_MAX + 1, sizeof(zframe_t*));
    assert(iothread_usr_data->gateways);

    rv = worker_new(&c->ioworker_ctx, log_ctx, NULL, iothread_destroy,
                    iothread_handle_inproc_msg, iothread_usr_data);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    *ctx = c;

    return OSD_OK;
}

API_EXPORT
void osd_hostctrl_free(struct osd_hostctrl_ctx **ctx_p)
{
    assert(ctx_p);
    struct osd_hostctrl_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    assert(!ctx->is_running);

    worker_free(&ctx->ioworker_ctx);

    free(ctx);
    *ctx_p = NULL;
}


API_EXPORT
osd_result osd_hostctrl_start(struct osd_hostctrl_ctx *ctx)
{
    osd_result rv;
    assert(ctx);
    assert(!ctx->is_running);

    worker_send_status(ctx->ioworker_ctx->inproc_socket, "I-START", 0);
    int retval;
    rv = worker_wait_for_status(ctx->ioworker_ctx->inproc_socket,
                                "I-START-DONE", &retval);
    if (OSD_FAILED(rv) || retval == -1) {
        err(ctx->log_ctx, "Unable to start router functionality.");
        return OSD_ERROR_CONNECTION_FAILED;
    }

    ctx->is_running = true;

    dbg(ctx->log_ctx, "Host controller started, accepting connections.");

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostctrl_stop(struct osd_hostctrl_ctx *ctx)
{

    osd_result rv;

    assert(ctx);

    if (!ctx->is_running) {
        return OSD_ERROR_NOT_CONNECTED;
    }

    worker_send_status(ctx->ioworker_ctx->inproc_socket, "I-STOP", 0);
    osd_result retval;
    rv = worker_wait_for_status(ctx->ioworker_ctx->inproc_socket, "I-STOP-DONE",
                                &retval);
    if (OSD_FAILED(rv)) {
        return rv;
    }
    if (OSD_FAILED(retval)) {
        return retval;
    }

    ctx->is_running = false;

    return OSD_OK;
}
