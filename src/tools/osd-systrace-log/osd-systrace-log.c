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
 * Open SoC Debug system trace logger
 */

#define CLI_TOOL_PROGNAME "osd-systrace-log"
#define CLI_TOOL_SHORTDESC "Open SoC Debug system trace logger"

#include "../cli-util.h"
#include <czmq.h>
#include <osd/hostmod.h>

zsock_t *server_socket;

static void mgmt_send_ack(const zframe_t* dest)
{
    zmsg_t* msg = zmsg_new();
    zframe_t* dest_new = zframe_dup(dest);
    zmsg_add(msg, dest_new);
    zmsg_addstr(msg, "M");
    zmsg_addstr(msg, "ACK");
    zmsg_send(&msg, server_socket);
    zmsg_destroy(&msg);
}

static void process_mgmt_msg(const zframe_t* src, const zframe_t* payload_frame)
{
    char* msg = zframe_strdup(payload_frame);
    dbg("Received management message %s\n", msg);
    free(msg);

    mgmt_send_ack(src);
}

static void process_data_msg(const zframe_t* src, const zframe_t* payload_frame)
{
#if 0
    uint16_t* client_addr = zhash_lookup(hash, routing_id);
    if (client_addr != NULL) {
      printf("Client is known to server already with address %u\n", *client_addr);
    } else {
      printf("Client unknown, recording its address.\n");
      uint16_t *value = malloc(sizeof(uint16_t));
      *value = next_addr++;
      zhash_insert(hash, routing_id, value);
    }
#endif
}

osd_result setup(void)
{
    // XXX: nothing to do right now
    return OSD_OK;
}

int run(void)
{
    osd_result osd_rv;

    // prepare OSD logging system
    struct osd_log_ctx *osd_log_ctx;
    osd_rv = osd_log_new(&osd_log_ctx, cfg.log_level, &osd_log_handler);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to create osd_log_ctx (rv=%d).\n", osd_rv);
    }

    // create host module
    struct osd_hostmod_ctx *hostmod_ctx;
    osd_rv = osd_hostmod_new(&hostmod_ctx, osd_log_ctx);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to create hostmod_ctx (rv=%d).\n", osd_rv);
    }

    // connect to subnet controller
    osd_rv = osd_hostmod_connect(hostmod_ctx, "tcp://localhost:9990");
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to connect to host controller (rv=%d).\n", osd_rv);
    }

    uint16_t address = osd_hostmod_get_diaddr(hostmod_ctx);
    info("Our address in the debug network is %u\n", address);

    osd_hostmod_disconnect(hostmod_ctx);
    osd_hostmod_free(&hostmod_ctx);

    osd_log_free(&osd_log_ctx);

    return 0;
}
