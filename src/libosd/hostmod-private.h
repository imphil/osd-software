#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

#ifndef OSD_HOSTMOD_PRIVATE_H
#define OSD_HOSTMOD_PRIVATE_H

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

    /** communication socket to the host controller */
    zsock_t *inproc_ctrl_io_socket;

    /** Control data I/O thread */
    pthread_t thread_ctrl_io;

    /** Event packet handler function */
    osd_hostmod_event_handler_fn event_handler;

    /** Argument passed to event_handler */
    void* event_handler_arg;
};

/**
 * Private context for the ctrl_io thread
 */
struct osd_hostmod_ctrl_io_ctx {
    /** Control communication socket with the host controller */
    zsock_t *ctrl_socket;

    /** Inprocess communication with the main thread */
    zsock_t *inproc_ctrl_io_socket;

    /** ZeroMQ address/URL of the host controller */
    char* host_controller_address;

    /** Logging context */
    struct osd_log_ctx *log_ctx;

    /** Event packet handler function */
    osd_hostmod_event_handler_fn event_handler;

    /** Argument passed to event_handler */
    void* event_handler_arg;
};

#endif // OSD_HOSTMOD_PRIVATE_H
