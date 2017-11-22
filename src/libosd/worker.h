#ifndef WORKER_H
#define WORKER_H

#include <czmq.h>
#include <osd/osd.h>

/**
 * Reactive In-Process Worker with ZeroMQ Communication
 *
 * This helper class provides a reactive worker based on the CZMQ zloop. It
 * handles the setup and teardown of the worker thread and provides means to
 * communicate with the thread in a safe and easy manner.
 */

/**
 * Worker context object (to be used on main thread)
 */
struct worker_ctx {
    /** Worker thread */
    pthread_t thread;

    /** In-process socket for communication with the worker thread */
    zsock_t *inproc_socket;
};

// forward declaration for typedefs below
struct worker_thread_ctx;

typedef osd_result (*worker_thread_init_fn)(
        struct worker_thread_ctx* /* thread_ctx */);
typedef osd_result (*worker_thread_destroy_fn)(
        struct worker_thread_ctx* /* thread_ctx */);

/**
 * Handle a message received in the worker thread from the main thread
 *
 * @param thread_ctx the thread context
 * @param type_str message type
 * @param inproc_msg the whole message. The ownership of the message is passed
 *                   on to the handler function, which is responsible for
 *                   destroying it after use.
 */
typedef osd_result (*worker_cmd_handler_fn)(
        struct worker_thread_ctx* /* thread_ctx */, const char* /* type_str */,
        zmsg_t* /* inproc_msg */);

/**
 * Worker context object (to be used in the worker thread)
 */
struct worker_thread_ctx {
    /** Event processing zloop */
    zloop_t* zloop;

    /** In-process socket for communication with main thread */
    zsock_t *inproc_socket;

    /** Logging context */
    struct osd_log_ctx *log_ctx;

    /** User-specific extensions to the structure */
    void *usr;

    worker_thread_init_fn init_fn;
    worker_thread_init_fn destroy_fn;
    worker_cmd_handler_fn cmd_handler_fn;
};

/**
 * Initialize the worker
 *
 * @param ctx the context object
 * @param log_ctx the log context
 * @param thread_init_fn extension point: function called during initialization
 *                       of the worker thread.
 * @param thread_destroy_fn extension point: function called during destruction
 *                          of the worker thread.
 * @param cmd_handler_fn extension point: handle a custom message sent to the
 *                       worker thread using worker_send_data() or
 *                       worker_send_status().
 * @param thread_ctx_user user data passed to the worker thread. The ownership
 *                        of this pointer is passed on to the worker. The
 *                        user data must be freed and set to NULL in the
 *                        thread_destroy_fn.
 */
osd_result worker_new(struct worker_ctx **ctx, struct osd_log_ctx *log_ctx,
                      worker_thread_init_fn thread_init_fn,
                      worker_thread_destroy_fn thread_destroy_fn,
                      worker_cmd_handler_fn cmd_handler_fn,
                      void* thread_ctx_usr);

/**
 * Free all resources
 */
void worker_free(struct worker_ctx **ctx_p);

/**
 * Send a data message to another thread over a ZeroMQ socket
 *
 * @param socket ZeroMQ socket to send the status message to.
 * @param name name identifying the message
 * @param data data to be sent
 * @param size size of @p data (bytes)
 *
 * @see worker_send_status()
 */
void worker_send_data(zsock_t *socket, const char* name, const void* data,
                      size_t size);

/**
 * Send a status message to another thread over a ZeroMQ socket
 *
 * @param socket ZeroMQ socket to send the status message to.
 * @param name name identifying the message
 * @param value status value
 *
 * @see worker_send_data()
 */
void worker_send_status(zsock_t *socket, const char* name, int value);

/**
 * Wait for a status message of a given name and return its value
 *
 * @return OSD_ERROR_FAILURE if an unexpected error happened,
 *         OSD_ERROR_TIMEOUT if the wait timeout was exceeded
 *         OSD_OK if operation was successful.
 */
osd_result worker_wait_for_status(zsock_t *socket, const char* name,
                                  int *retvalue);

#endif // WORKER_H
