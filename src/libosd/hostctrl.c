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
#include "osd-private.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

/**
 * Host Controller context
 */
struct osd_hostctrl_ctx {
    struct osd_log_ctx *log_ctx;
    char* host_controller_address;
    unsigned int subnet_addr;
};

static void* thread_io_main(void* ctx_void)
{
    struct osd_hostctrl_ctx *ctx = ctx_void;

    zsock_t *server_socket = zsock_new_router(ctx->host_controller_address);

    // allocate routing lookup tables
    // mods_in_subnet is 1024 * 8B = 8 kB
    mods_in_subnet = calloc(OSD_DIADDR_LOCAL_MAX + 1, sizeof(zframe_t*));
    assert(mods_in_subnet);
    // gateways is 64 * 8B = 1 kB
    gateways = calloc(OSD_DIADDR_SUBNET_MAX + 1, sizeof(zframe_t*));
    assert(gateways);

    while (!zsys_interrupted) {
        zmsg_t * msg = zmsg_recv(server_socket);
        if (!msg && errno == EINTR) {
            // user pressed CTRL-C (SIGHUP)
            printf("Exiting server ...\n");
            break;
        }
        printf("Received message: \n");
        zmsg_print(msg);

        zframe_t *src_frame = zmsg_pop(msg);
        zframe_t* type_frame = zmsg_pop(msg);

        if (zframe_streq(type_frame, "M")) {
            zframe_t* payload_frame = zmsg_pop(msg);
            process_mgmt_msg(src_frame, payload_frame);
        } else if (zframe_streq(type_frame, "D")) {
            zframe_t* payload_frame = zmsg_pop(msg);
            process_data_msg(src_frame, payload_frame);
        } else {
            err("Message of unknown type received. Ignoring.\n");
            goto next_msg;
        }

next_msg:
        zmsg_destroy(&msg);
    }

    free(mods_in_subnet);
    free(gateways);
    zsock_destroy(&server_socket);
    return 0;
}

API_EXPORT
osd_result osd_hostctrl_new(struct osd_hostctrl_ctx **ctx,
                            struct osd_log_ctx *log_ctx)
{
    osd_result rv;

    struct osd_hostctrl_ctx *c = calloc(1, sizeof(struct osd_hostctrl_ctx));
    assert(c);

    c->log_ctx = log_ctx;

    *ctx = c;

    return OSD_OK;
}

API_EXPORT
osd_result osd_hostctrl_start(struct osd_hostctrl_ctx *ctx,
                              const char* host_controller_address)
{

}

API_EXPORT
osd_result osd_hostctrl_stop(struct osd_hostctrl_ctx *ctx)
{

}

API_EXPORT
void osd_hostctrl_free(struct osd_hostctrl_ctx **ctx_p)
{
    assert(ctx_p);
    struct osd_hostctrl_ctx *ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    free(ctx);
    *ctx_p = NULL;
}
