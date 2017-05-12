/**
 * Open SoC Debug Communication Library
 *
 * The communication library handles all low-level communication detail between
 * the device and OSD Clients. The device is usually the hardware on which Open
 * SoC Debug is integrated. OSD Clients are libraries or applications using
 * the functionality of the OSD Communication API to interact with a device, or
 * to receive data (e.g. traces) from it.
 *
 * The physical interface (i.e. low-level communication) is not implemented in
 * the communication library. Instead, the API provides hooks to connect
 * different physical communication libraries to it.
 *
 * Every device connects to one communication library. To enable the use case of
 * multiple tools (such as a run-control debugger like GDB and a trace debugger)
 * talking to the same OSD-enabled device, the communication library supports
 * multiple clients.
 */

#include <osd.h>
#include "osd-private.h"
#include <osd-com.h>
#include "osd-com-private.h"

#include <errno.h>

/**
 * Get all available debug modules from the attached devices
 */
static osd_result _osd_com_enumerate_modules(struct osd_com_ctx *ctx)
{
    return OSD_RV_SUCCESS;
}

/**
 * Create new communication API object
 *
 * @return OSD_RV_SUCCESS on success, any other value indicates an error
 */
API_EXPORT
osd_result osd_com_new(struct osd_com_ctx **ctx, struct osd_log_ctx *log_ctx)
{
    struct osd_com_ctx *c = calloc(1, sizeof(struct osd_com_ctx));
    if (!c) {
        return -ENOMEM;
    }

    c->log_ctx = log_ctx;

    *ctx = c;

    return OSD_RV_SUCCESS;
}

/**
 * Free a communication API context object
 */
API_EXPORT
void osd_com_free(struct osd_com_ctx *ctx)
{
    free(ctx);
}

/**
 * Get a list of all Open SoC Debug modules present in the device
 */
API_EXPORT
osd_result osd_com_get_modules(struct osd_com_ctx *ctx,
                               struct osd_module_desc **modules,
                               size_t *modules_len)
{
    if (!ctx->modules) {
        osd_result rv = _osd_com_enumerate_modules(ctx);
        if (OSD_FAILED(rv)) {
            return rv;
        }
    }

    *modules = ctx->modules;
    *modules_len = ctx->modules_len;

    return OSD_RV_SUCCESS;
}
