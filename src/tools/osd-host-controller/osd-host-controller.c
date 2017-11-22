/**
 * Open SoC Debug host controller
 */

#define CLI_TOOL_PROGNAME "osd-host-controller"
#define CLI_TOOL_SHORTDESC "Open SoC Debug host controller"

#include "../cli-util.h"
#include <osd/hostctrl.h>
#include <osd/packet.h>

#include <unistd.h>

#define DEFAULT_HOSTCTRL_BIND_EP "tcp://0.0.0.0:9537"

osd_result setup(void)
{
    // XXX: nothing to do right now
    return OSD_OK;
}

int run(void)
{
    osd_result rv;
    int exitcode;

    zsys_init();

    struct osd_log_ctx *osd_log_ctx;
    rv = osd_log_new(&osd_log_ctx, cfg.log_level, &osd_log_handler);
    assert(OSD_SUCCEEDED(rv));

    struct osd_hostctrl_ctx *hostctrl_ctx;
    rv = osd_hostctrl_new(&hostctrl_ctx, osd_log_ctx, DEFAULT_HOSTCTRL_BIND_EP);
    if (OSD_FAILED(rv)) {
        fatal("Unable to initialize host controller (%d)", rv);
        exitcode = 1;
        goto free_return;
    }

    rv = osd_hostctrl_start(hostctrl_ctx);
    if (OSD_FAILED(rv)) {
        fatal("Unable to start host controller (%d)", rv);
        exitcode = 1;
        goto free_return;
    }

    info("Host controller up and running");
    while (!zsys_interrupted) {
        pause();
    }
    info("Shutting shutdown signal, cleaning up.");

    rv = osd_hostctrl_stop(hostctrl_ctx);
    if (OSD_FAILED(rv)) {
        fatal("Unable to stop host controller (%d)", rv);
        exitcode = 1;
        goto free_return;
    }

    osd_hostctrl_free(&hostctrl_ctx);

    exitcode = 0;
free_return:
    osd_hostctrl_free(&hostctrl_ctx);
    return exitcode;
}
