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

#ifndef OSD_HOSTMOD_STMLOGGER_H
#define OSD_HOSTMOD_STMLOGGER_H


#include <osd/osd.h>
#include <osd/hostmod.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libosd-hostmod-stmlogger System Trace Logger
 * @ingroup libosd
 *
 * @{
 */

struct osd_hostmod_stmlogger_ctx;

osd_result osd_hostmod_stmlogger_new(struct osd_hostmod_stmlogger_ctx **ctx,
                                     struct osd_log_ctx *log_ctx,
                                     unsigned int stm_di_addr);
osd_result osd_hostmod_stmlogger_connect(struct osd_hostmod_stmlogger_ctx *ctx, const char* host_controller_address);
osd_result osd_hostmod_stmlogger_disconnect(struct osd_hostmod_stmlogger_ctx *ctx);
void osd_hostmod_stmlogger_free(struct osd_hostmod_stmlogger_ctx **ctx_p);

struct osd_hostmod_ctx * osd_hostmod_stmlogger_get_hostmod_ctx(struct osd_hostmod_stmlogger_ctx *ctx);
osd_result osd_hostmod_stmlogger_tracestart(struct osd_hostmod_stmlogger_ctx *ctx);
osd_result osd_hostmod_stmlogger_tracestop(struct osd_hostmod_stmlogger_ctx *ctx);


/**@}*/ /* end of doxygen group libosd-hostmod-stmlogger */

#ifdef __cplusplus
}
#endif

#endif // OSD_HOSTMOD_STMLOGGER_H
