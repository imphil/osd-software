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
#include <osd/hostmod_stmlogger.h>

// command line arguments
struct arg_int *a_stm_diaddr;
struct arg_str *a_hostctrl_ep;

osd_result setup(void)
{
    a_hostctrl_ep = arg_str0("c", "hostctrl", "<URL>", "ZeroMQ endpoint of the host controller (default: "DEFAULT_HOSTCTRL_EP")");
    a_hostctrl_ep->sval[0] = DEFAULT_HOSTCTRL_EP;
    osd_tool_add_arg(a_hostctrl_ep);

    a_stm_diaddr = arg_int1("a", "diaddr", "<diaddr>",
                              "DI address of the STM module");
    osd_tool_add_arg(a_stm_diaddr);

    return OSD_OK;
}

int run(void)
{
    int prog_ret = -1;
    osd_result osd_rv;

    struct osd_log_ctx *osd_log_ctx;
    osd_rv = osd_log_new(&osd_log_ctx, cfg.log_level, &osd_log_handler);
    assert(OSD_SUCCEEDED(osd_rv));

    struct osd_hostmod_stmlogger_ctx *hostmod_stmlogger_ctx;
    osd_rv = osd_hostmod_stmlogger_new(&hostmod_stmlogger_ctx, osd_log_ctx,
                                       a_hostctrl_ep->sval[0],
                                       a_stm_diaddr->ival[0]);
    assert(OSD_SUCCEEDED(osd_rv));

    osd_rv = osd_hostmod_stmlogger_connect(hostmod_stmlogger_ctx);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to connect to host controller at %s (rv=%d).\n",
              a_hostctrl_ep->sval[0], osd_rv);
        prog_ret = -1;
        goto free_return;
    }

    prog_ret = 0;


free_return:
    osd_hostmod_stmlogger_disconnect(hostmod_stmlogger_ctx);
    osd_hostmod_stmlogger_free(&hostmod_stmlogger_ctx);

    osd_log_free(&osd_log_ctx);

    return prog_ret;
}
