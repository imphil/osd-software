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

#ifndef OSD_HOSTMOD_H
#define OSD_HOSTMOD_H


#include <osd/osd.h>
#include <osd/module.h>

#include <stdlib.h>
#include <czmq.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libosd-hostmod Host
 * @ingroup libosd
 *
 * @{
 */

/** Flag: fully blocking operation (i.e. wait forever) */
#define OSD_HOSTMOD_BLOCKING 1

/**
 * Opaque context object
 *
 * This object contains all state information. Create and initialize a new
 * object with osd_hostmod_new() and delete it with osd_hostmod_free().
 */
struct osd_hostmod_ctx;

/**
 * Create new osd_hostmod instance
 *
 * @param[out] ctx the osd_hostmod_ctx context to be created
 * @param[in] log_ctx the log context to be used. Set to NULL to disable logging
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_free()
 */
osd_result osd_hostmod_new(struct osd_hostmod_ctx **ctx,
                           struct osd_log_ctx *log_ctx);

/**
 * Free and NULL a communication API context object
 *
 * Call osd_hostmod_disconnect() before calling this function.
 *
 * @param ctx the osd_com context object
 */
void osd_hostmod_free(struct osd_hostmod_ctx **ctx);

osd_result osd_hostmod_get_modules(struct osd_hostmod_ctx *ctx,
                               struct osd_module_desc **modules,
                               size_t *modules_len);

/**
 * Connect to the host controller
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_disconnect()
 */
osd_result osd_hostmod_connect(struct osd_hostmod_ctx *ctx,
                               const char* host_controller_address);

/**
 * Shut down all communication with the device
 *
 * @param ctx the osd_hostmod context object
 * @return OSD_OK on success, any other value indicates an error
 *
 * @see osd_hostmod_run()
 */
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx);

/**
 * Is the connection to the device active?
 *
 * @param ctx the osd_hostmod context object
 * @return 1 if connected, 0 if not connected
 *
 * @see osd_hostmod_connect()
 * @see osd_hostmod_disconnect()
 */
int osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx);

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
 * @param flags flags. Set OSD_HOSTMOD_BLOCKING to block indefinitely until the
 *              access succeeds.
 * @return OSD_OK on success, any other value indicates an error
 * @return OSD_ERROR_TIMEDOUT if the register read timed out (only if
 *         OSD_HOSTMOD_BLOCKING is not set)
 */
osd_result osd_hostmod_reg_read(struct osd_hostmod_ctx *ctx,
                                const uint16_t module_addr,
                                const uint16_t reg_addr,
                                const int reg_size_bit, void *result,
                                const int flags);

/**
 * Get the DI address assigned to this host debug module
 *
 * The address is assigned during the connection, i.e. you need to call
 * osd_hostmod_connect() before calling this function.
 *
 * @param ctx the osd_hostmod_ctx context object
 * @return the address assigned to this debug module
 */
uint16_t osd_hostmod_get_diaddr(struct osd_hostmod_ctx *ctx);

/**
 * Get the description fields of a debug module (type, vendor, version)
 */
osd_result osd_hostmod_describe_module(struct osd_hostmod_ctx *ctx,
                                       uint16_t di_addr,
                                       struct osd_module_desc *desc);


/**@}*/ /* end of doxygen group libosd-hostmod */

#ifdef __cplusplus
}
#endif

#endif // OSD_HOSTMOD_H
