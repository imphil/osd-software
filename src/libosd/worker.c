#include "worker.h"

#include <osd/osd.h>
#include <assert.h>
#include "osd-private.h"

/**
 * Handler: Message from main thread received in worker thread
 */
static int thread_inproc_rcv(zloop_t *loop, zsock_t *reader,
                             void *thread_ctx_void)
{
    struct worker_thread_ctx *thread_ctx = thread_ctx_void;
    assert(thread_ctx);

    int retval;
    osd_result rv;

    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process was interrupted, terminate zloop
    }

    zframe_t *type_frame = zmsg_first(msg);
    assert(type_frame);

    char* type_str = (char*) zframe_data(type_frame);
    assert(type_str);

    if (!strcmp(type_str, "I-SHUTDOWN")) {
        // End thread by returning -1, which will terminate zloop
        retval = -1;
        zmsg_destroy(&msg);
        goto free_return;
    } else {
        if (!thread_ctx->cmd_handler_fn) {
            err(thread_ctx->log_ctx, "No handler for inproc message set.");
            zmsg_destroy(&msg);
        }
        rv = thread_ctx->cmd_handler_fn(thread_ctx, type_str, msg);
        if (OSD_FAILED(rv)) {
            err(thread_ctx->log_ctx, "Handler for inproc message failed.");
        }
    }

    retval = 0;
    free_return: return retval;
}

static void* thread_main(void *thread_ctx_void)
{
    struct worker_thread_ctx *thread_ctx = thread_ctx_void;

    int zmq_rv;
    osd_result osd_rv;

    // create new PAIR socket for the communication of the main thread
    thread_ctx->inproc_socket = zsock_new_pair(">inproc://threadcom");
    assert(thread_ctx->inproc_socket);

    // extension point: thread init
    if (thread_ctx->init_fn) {
        osd_rv = thread_ctx->init_fn(thread_ctx);
        if (OSD_FAILED(osd_rv)) {
            worker_send_status(thread_ctx->inproc_socket, "I-THREADINIT-DONE",
                               osd_rv);

            zsock_destroy(&thread_ctx->inproc_socket);
            return NULL;
        }
    }

    // prepare processing loop
    thread_ctx->zloop = zloop_new();
    assert(thread_ctx->zloop);

#ifdef DEBUG
    zloop_set_verbose(thread_ctx->zloop, 1);
#endif

    zmq_rv = zloop_reader(thread_ctx->zloop, thread_ctx->inproc_socket,
                          thread_inproc_rcv, thread_ctx);
    assert(zmq_rv == 0);
    zloop_reader_set_tolerant(thread_ctx->zloop, thread_ctx->inproc_socket);

    // connection successful: inform main thread
    worker_send_status(thread_ctx->inproc_socket, "I-THREADINIT-DONE", OSD_OK);

    // start event loop -- takes over thread
    zmq_rv = zloop_start(thread_ctx->zloop);
    if (zmq_rv != -1) {
        err(thread_ctx->log_ctx, "ZeroMQ zloop did not shut down properly.");
    }

    // extension point: thread destruction
    if (thread_ctx->destroy_fn) {
        thread_ctx->destroy_fn(thread_ctx);
    }

    worker_send_status(thread_ctx->inproc_socket, "I-SHUTDOWN-DONE", OSD_OK);

    assert(thread_ctx->usr == NULL &&
           "You need to free() and NULL the user context in a thread function "
           "to prevent memory leaks.");

    zsock_destroy(&thread_ctx->inproc_socket);

    zloop_destroy(&thread_ctx->zloop);

    free(thread_ctx);
    thread_ctx = NULL;

    return NULL;
}

osd_result worker_new(struct worker_ctx **ctx, struct osd_log_ctx *log_ctx,
                      worker_thread_init_fn thread_init_fn,
                      worker_thread_destroy_fn thread_destroy_fn,
                      worker_cmd_handler_fn cmd_handler_fn,
                      void* thread_ctx_usr)
{
    int rv;

    struct worker_ctx *c = calloc(1, sizeof(struct worker_ctx));
    assert(c);

    c->inproc_socket = zsock_new_pair("@inproc://threadcom");
    assert(c->inproc_socket);

    // To support I/O with timeouts (e.g. reading a register with a timeout)
    // we need the ZeroMQ receive functions to time out as well.
    // If fully blocking behavior is required, manually loop on the zmsg_recv()
    // calls.
    // We need to use a slightly higher timeout for the internal communication
    // than for the external communication: if an external communication fails,
    // the I/O thread must be able to recognize this by the timeout, and then
    // inform the main thread. If both threads follow the same timeout, the
    // I/O thread cannot inform the main thread of timeouts.
    zsock_set_rcvtimeo(c->inproc_socket, 1.5 * ZMQ_RCV_TIMEOUT);

    struct worker_thread_ctx *thread_ctx = calloc(
            1, sizeof(struct worker_thread_ctx));
    assert(thread_ctx);
    thread_ctx->usr = thread_ctx_usr;
    thread_ctx->log_ctx = log_ctx;
    thread_ctx->init_fn = thread_init_fn;
    thread_ctx->destroy_fn = thread_destroy_fn;
    thread_ctx->cmd_handler_fn = cmd_handler_fn;

    rv = pthread_create(&c->thread, 0, thread_main, (void*) thread_ctx);
    assert(rv == 0);

    // wait for thread setup to be completed
    int retval;
    worker_wait_for_status(c->inproc_socket, "I-THREADINIT-DONE", &retval);
    if (OSD_FAILED(retval)) {
        pthread_join(c->thread, NULL);
        zsock_destroy(&c->inproc_socket);
        free(c);
        return rv;
    }

    *ctx = c;

    return OSD_OK;
}

void worker_free(struct worker_ctx **ctx_p)
{
    osd_result osd_rv;

    assert(ctx_p);
    struct worker_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    // shut down thread
    worker_send_status(ctx->inproc_socket, "I-SHUTDOWN", 0);

    // wait for shutdown to happen
    int retvalue;
    osd_rv = worker_wait_for_status(ctx->inproc_socket, "I-SHUTDOWN-DONE",
                                    &retvalue);
    if (OSD_FAILED(osd_rv)) {
        // If the thread shutting down properly by itself, we force a shutdown
        pthread_cancel(ctx->thread);
    }

    // wait until control I/O thread has finished its cleanup
    pthread_join(ctx->thread, NULL);

    zsock_destroy(&ctx->inproc_socket);

    free(ctx);
    *ctx_p = NULL;
}


void worker_send_data(zsock_t *socket, const char* name, const void* data,
                      size_t size)
{
    int zmq_rv;

    assert(socket);

    zmsg_t *msg = zmsg_new();
    assert(msg);
    zmq_rv = zmsg_addstr(msg, name);
    assert(zmq_rv == 0);
    if (data != NULL && size > 0) {
        zmq_rv = zmsg_addmem(msg, data, size);
        assert(zmq_rv == 0);
    }
    zmq_rv = zmsg_send(&msg, socket);
    assert(zmq_rv == 0);
}

void worker_send_status(zsock_t *socket, const char* name, int value)
{
    worker_send_data(socket, name, &value, sizeof(int));
}

osd_result worker_wait_for_status(zsock_t *socket, const char* name,
                                  int *retvalue)
{
    zmsg_t *msg = zmsg_recv(socket);
    if (!msg) {
        if (errno == EAGAIN) {
            return OSD_ERROR_TIMEDOUT;
        } else {
            return OSD_ERROR_FAILURE;
        }
    }

    zframe_t *name_frame = zmsg_pop(msg);
    if (!zframe_streq(name_frame, name)) {
        zframe_destroy(&name_frame);
        zmsg_destroy(&msg);
        return OSD_ERROR_FAILURE;
    }
    zframe_destroy(&name_frame);

    zframe_t *data_frame = zmsg_pop(msg);
    assert(zframe_size(data_frame) == sizeof(int));
    memcpy(retvalue, zframe_data(data_frame), sizeof(int));
    zframe_destroy(&data_frame);

    zmsg_destroy(&msg);

    return OSD_OK;
}
