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

/**
 * @defgroup libosd-hostmod Host
 * @ingroup libosd
 *
 * @{
 */

#include <osd/osd.h>

#include <stdlib.h>
#include <czmq.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define OSD_COM_WAIT_FOREVER 1


/**
 * Opaque context object
 *
 * This object contains all state information. Create and initialize a new
 * object with osd_hostmod_new() and delete it with osd_hostmod_free().
 */
struct osd_hostmod_ctx;

osd_result osd_hostmod_new(struct osd_hostmod_ctx **ctx, struct osd_log_ctx *log_ctx);

void osd_hostmod_free(struct osd_hostmod_ctx *ctx);

osd_result osd_hostmod_get_modules(struct osd_hostmod_ctx *ctx,
                               struct osd_module_desc **modules,
                               size_t *modules_len);

osd_result osd_hostmod_connect(struct osd_hostmod_ctx *ctx,
                               char* host_controller_address);
osd_result osd_hostmod_disconnect(struct osd_hostmod_ctx *ctx);
int osd_hostmod_is_connected(struct osd_hostmod_ctx *ctx);

osd_result osd_hostmod_reg_read(struct osd_hostmod_ctx *ctx,
                                const uint16_t module_addr,
                                const uint16_t reg_addr,
                                const int reg_size_bit, void *result,
                                const int flags);

uint16_t osd_hostmod_get_addr(struct osd_hostmod_ctx *ctx);

#ifdef __cplusplus
}
#endif


/**@}*/ /* end of doxygen group libosd-hostmod */

