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
#include <osd/module.h>
#include <osd/hostmod_stmlogger.h>
#include <osd/reg.h>
#include "osd-private.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

/**
 * STM Logger context
 */
struct osd_hostmod_stmlogger_ctx {
    struct osd_hostmod_ctx *hostmod_ctx;
    struct osd_log_ctx *log_ctx;
    unsigned int stm_di_addr;
};


API_EXPORT
osd_result osd_hostmod_stmlogger_new(struct osd_hostmod_stmlogger_ctx **ctx,
                                     struct osd_log_ctx *log_ctx,
                                     unsigned int stm_di_addr)
{
    osd_result rv;

    struct osd_hostmod_stmlogger_ctx *c = calloc(1, sizeof(struct osd_hostmod_stmlogger_ctx));
    assert(c);

    c->log_ctx = log_ctx;
    c->stm_di_addr = stm_di_addr;

    struct osd_hostmod_ctx *hostmod_ctx;
    rv = osd_hostmod_new(&hostmod_ctx, log_ctx);
    assert(OSD_SUCCEEDED(rv));
    c->hostmod_ctx = hostmod_ctx;

    *ctx = c;

    return OSD_OK;
}

static bool is_stm_module(struct osd_hostmod_stmlogger_ctx *ctx)
{
    osd_result rv;

    struct osd_module_desc desc;

    rv = osd_hostmod_describe_module(ctx->hostmod_ctx, ctx->stm_di_addr, &desc);
    if (OSD_FAILED(rv)) {
        err(ctx->log_ctx, "Unable to check if module %u is a STM. "
            "Assuming it is not.\n", ctx->stm_di_addr);
        return false;
    }

    if (desc.vendor != OSD_MODULE_VENDOR_OSD ||
        desc.type != OSD_MODULE_TYPE_STD_STM ||
        desc.version != 0) {
        return false;
    }

    return true;
}

/**
 * Start tracing
 *
 * Instruct the STM module to start sending traces to us.
 */
API_EXPORT
osd_result osd_hostmod_stmlogger_tracestart(struct osd_hostmod_stmlogger_ctx *ctx)
{
    osd_result rv;
    if (!is_stm_module(ctx)) {
        err(ctx->log_ctx, "Unable to start tracing: module %u is no STM.\n",
            ctx->stm_di_addr);
    }

    uint16_t event_dest = osd_hostmod_get_diaddr(ctx->hostmod_ctx);
    rv = osd_hostmod_reg_write(ctx->hostmod_ctx, ctx->stm_di_addr,
                               OSD_REG_BASE_MOD_EVENT_DEST, 16, &event_dest, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    uint16_t cs = OSD_REG_BASE_MOD_CS_ACTIVE;
    rv = osd_hostmod_reg_write(ctx->hostmod_ctx, ctx->stm_di_addr,
                               OSD_REG_BASE_MOD_CS, 16, &cs, 0);
    if (OSD_FAILED(rv)) {
        return rv;
    }

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostmod_stmlogger_tracestop(struct osd_hostmod_stmlogger_ctx *ctx)
{
    uint16_t cs = 0;
    osd_hostmod_reg_write(ctx->hostmod_ctx, ctx->stm_di_addr,
                          OSD_REG_BASE_MOD_CS, 16, &cs, 0);

    return OSD_OK;
}

static osd_result handle_event_pkg(void* arg, struct osd_packet *pkg)
{
    osd_packet_dump(pkg, stdout);
    fflush(stdout);

    osd_packet_free(&pkg);

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostmod_stmlogger_connect(struct osd_hostmod_stmlogger_ctx *ctx,
                                         const char* host_controller_address)
{
    osd_result rv;

    rv = osd_hostmod_register_event_handler(ctx->hostmod_ctx, handle_event_pkg, NULL);
    assert(OSD_SUCCEEDED(rv));

    return osd_hostmod_connect(ctx->hostmod_ctx, host_controller_address);
}

API_EXPORT
osd_result osd_hostmod_stmlogger_disconnect(struct osd_hostmod_stmlogger_ctx *ctx)
{
    return osd_hostmod_disconnect(ctx->hostmod_ctx);
}

API_EXPORT
void osd_hostmod_stmlogger_free(struct osd_hostmod_stmlogger_ctx **ctx_p)
{
    assert(ctx_p);
    struct osd_hostmod_stmlogger_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    osd_hostmod_free(&ctx->hostmod_ctx);

    free(ctx);
    *ctx_p = NULL;
}

API_EXPORT
struct osd_hostmod_ctx * osd_hostmod_stmlogger_get_hostmod_ctx(struct osd_hostmod_stmlogger_ctx *ctx)
{
    return ctx->hostmod_ctx;
}
