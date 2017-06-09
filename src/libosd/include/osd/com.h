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

#include <stdlib.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Request non-blocking operation
 */
#define OSD_COM_NONBLOCK 0x400

enum osd_com_byte_order{
    OSD_COM_BIG_ENDIAN,
    OSD_COM_LITTLE_ENDIAN
};

/**
 * Opaque context object
 *
 * This object contains all state information. Create and initialize a new
 * object with osd_com_new() and delete it with osd_com_free().
 */
struct osd_com_ctx;

/**
 * Function template: write data to the device
 *
 * @see struct osd_com_device_if
 */
typedef ssize_t (*osd_com_device_write_fn)(uint16_t *buf, size_t size, int flags);

/**
 * Function template: read data from the device
 *
 * @see struct osd_com_device_if
 */
typedef ssize_t (*osd_com_device_read_fn)(uint16_t *buf, size_t size, int flags);


/**
 * Read/write interface of a device
 */
struct osd_com_device_if {
    osd_com_device_write_fn write;
    osd_com_device_read_fn read;
};

#if 0
/**
 * Client talking to the osd-com library
 */
struct osd_com_client;
#endif

osd_result osd_com_new(struct osd_com_ctx **ctx, struct osd_log_ctx *log_ctx);

void osd_com_free(struct osd_com_ctx *ctx);

osd_result osd_com_get_modules(struct osd_com_ctx *ctx,
                               struct osd_module_desc **modules,
                               size_t *modules_len);

osd_result osd_com_set_device_ctrl_if(struct osd_com_ctx *ctx,
                                      struct osd_com_device_if *event_if);
osd_result osd_com_set_device_event_if(struct osd_com_ctx *ctx,
                                       struct osd_com_device_if *event_if);

osd_result osd_com_connect(struct osd_com_ctx *ctx);
osd_result osd_com_disconnect(struct osd_com_ctx *ctx);

osd_result osd_com_reg_read(struct osd_com_ctx *ctx,
                            const unsigned int module_addr,
                            const uint16_t reg_addr,
                            const int reg_size_bit, void *result,
                            const int flags);

#if 0

/**
 * Connect to the device
 */
osd_result osd_com_connect_to_device(struct osd_com_ctx ctx);


// API to the higher layers

// client management
/**
 * Connect a client to the library
 */
osd_result struct osd_com_client_connect(struct osd_com_ctx *ctx, struct osd_com_client* client);

/**
 * Disconnect a client from the library
 */
osd_result struct osd_com_client_disconnect(struct osd_com_ctx *ctx, struct osd_com_client *client);


/**
 * Give a client exclusive access to a OSD module
 *
 * XXX: this is only for write access, isn't it?
 */
osd_result osd_com_claim_module(struct *osd_com_ctx, struct osd_com_client client, osd_mod_desc_t *module);

/**
 * Register a client as receiver for trace events
 *
 * XXX: this is only for 1:n event broadcasts?
 */
osd_result osd_com_register_event_receiver(struct osd_com_ctx *ctx, struct osd_com_client client, unsigned int module_id);

/**
 * Release a previously claimed module
 *
 * @see osd_com_claim_module()
 */
osd_result osd_com_release_module(struct osd_com_ctx *ctx, unsigned int module_id)



#endif


#ifdef __cplusplus
}
#endif
