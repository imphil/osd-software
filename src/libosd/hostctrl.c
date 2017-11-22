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
#include "osd-private.h"

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

    /** In-process communication helper */
    struct inprochelper_ctx *inprochelper_ctx;
};

struct thread_ctx_usr {
    /** Host controller router socket */
    zsock_t *router_socket;

    /** ZeroMQ address/URL this host controller is bound to */
    const char* router_address;
};

/**
 * Process incoming messages
 *
 * @return 0 if the message was processed, -1 if @p loop should be terminated
 */
static int process_incoming_msg(zloop_t *loop, zsock_t *reader, void *thread_ctx_void)
{
    struct inprochelper_thread_ctx* thread_ctx = (struct inprochelper_thread_ctx*)thread_ctx_void;
    assert(thread_ctx);

    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    int rv;
    osd_result osd_rv;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }



    // XXX: INSERT ROUTING HERE


    return 0;
}

/**
 * Start host controller router function in I/O thread
 *
 * This function is called by the inprochelper as response to a I-START message.
 * It create a new ZeroMQ ROUTER socket acting as host controller and registers
 * an event handler function if new packages are received. After all startup
 * tasks are done a I-START-DONE message is sent to the main thread.
 */
static void router_start(struct inprochelper_thread_ctx *thread_ctx)
{
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    osd_result retval;
    osd_result osd_rv;

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
                          process_incoming_msg, thread_ctx);
    assert(zmq_rv == 0);
    zloop_reader_set_tolerant(thread_ctx->zloop, usrctx->router_socket);

free_return:
    inprochelper_send_status(thread_ctx->inproc_socket, "I-START-DONE",
                             retval);
}

/**
 * Stop the host controller router function in the I/O thread
 */
static void router_stop(struct inprochelper_thread_ctx *thread_ctx)
{

}

static osd_result iothread_handle_inproc_request(struct inprochelper_thread_ctx *thread_ctx,
                                                 const char *name, zmsg_t *msg)
{
    struct thread_ctx_usr *usrctx = thread_ctx->usr;
    assert(usrctx);

    int rv;

    if (!strcmp(name, "I-START")) {
        router_start(thread_ctx);

    } else if (!strcmp(name, "I-STOP")) {
        router_stop(thread_ctx);

    } else {
        assert(0 && "Received unknown message from main thread.");
    }

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

    // prepare custom data passed to I/O thread
    struct thread_ctx_usr *iothread_usr_data = calloc(1, sizeof(struct thread_ctx_usr));
    assert(iothread_usr_data);

    iothread_usr_data->router_address = strdup(router_address);

    rv = inprochelper_new(&c->inprochelper_ctx, log_ctx, NULL, NULL,
                          iothread_handle_inproc_request, iothread_usr_data);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    *ctx = c;

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostctrl_start(struct osd_hostctrl_ctx *ctx,
                              const char* host_controller_address)
{

}

API_EXPORT
osd_result osd_hostctrl_stop(struct osd_hostctrl_ctx *ctx)
{

}

API_EXPORT
void osd_hostctrl_free(struct osd_hostctrl_ctx **ctx_p)
{
    assert(ctx_p);
    struct osd_hostctrl_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    free(ctx);
    *ctx_p = NULL;
}
