#include <stdint.h>
#include <pthread.h>

#pragma once

/** Information about the connected system */
struct osd_system_info {
    /** vendor identifier */
    uint16_t vendor_id;
    /** device identifier */
    uint16_t device_id;
    /** maximum number of words in a debug packet supported by the device */
    uint16_t max_pkt_len;
};

/**
 * Host module context
 */
struct osd_hostmod_ctx {
    /** Is the library connected to a device? */
    int is_connected;

    /** Logging context */
    struct osd_log_ctx *log_ctx;

    /** Address assigned to this module in the debug interconnect */
    uint16_t diaddr;

    /** communication socket to the host controller */
    zsock_t *inproc_ctrl_io_socket;

    /** Control data receive thread */
    pthread_t thread_ctrl_io;

    /** system information */
    struct osd_system_info system_info;

    /** list of modules in the system */
    struct osd_module_desc *modules;
    /** number of elements in modules */
    size_t modules_len;
};

/**
 * Private context for the ctrl_io thread
 */
struct osd_hostmod_ctrl_io_ctx {
    /** ZeroMQ address/URL of the host controller */
    char* host_controller_address;

    /** Logging context */
    struct osd_log_ctx *log_ctx;

    /** Control communication socket with the host controller */
    zsock_t *ctrl_socket;
    /** Inprocess communication with the main thread */
    zsock_t *inproc_ctrl_io_socket;
};
