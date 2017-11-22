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

#ifndef OSD_HOSTCTRL_H
#define OSD_HOSTCTRL_H

#include <osd/osd.h>

#include <czmq.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libosd-hostctrl Host Controller
 * @ingroup libosd
 *
 * @{
 */

struct osd_hostctrl_ctx;

/**
 * Create new host controller
 *
 * The host controller will listen to requests at @p router_addres
 *
 * @param ctx context object
 * @param log_ctx logging context
 * @param router_address ZeroMQ endpoint/URL the host controller will listen on
 * @return OSD_OK if initialization was successful,
 *         any other return code indicates an error
 */
osd_result osd_hostctrl_new(struct osd_hostctrl_ctx **ctx,
                            struct osd_log_ctx *log_ctx,
                            const char* router_address);

/**
 * Start host controller
 */
osd_result osd_hostctrl_start(struct osd_hostctrl_ctx *ctx);

/**
 * Stop host controller
 */
osd_result osd_hostctrl_stop(struct osd_hostctrl_ctx *ctx);
void osd_hostctrl_free(struct osd_hostctrl_ctx **ctx_p);


/**@}*/ /* end of doxygen group libosd-hostctrl */

#ifdef __cplusplus
}
#endif

#endif // OSD_HOSTCTRL_H
