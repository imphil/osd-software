/**
 * Open SoC Debug common functionality (header-only)
 *
 * Data structures and functionality used across the layers of Open SoC Debug.
 */


#include <inttypes.h>
#include <stdarg.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Standard return type
 */
typedef int osd_result;

#define OSD_RV_SUCCESS 0

#define OSD_FAILED(rv) ((rv) < 0)
#define OSD_SUCCEEDED(rv) ((rv) >= 0)


/**
 * Opaque logging context
 */
struct osd_log_ctx;

/**
 * Logging function template
 *
 * Implement a function with this signature and pass it to set_log_fn()
 * if you want to implement custom logging.
 */
typedef void (*osd_log_fn)(struct osd_log_ctx *ctx,
                           int priority, const char *file,
                           int line, const char *fn,
                           const char *format, va_list args);

osd_result osd_log_new(struct osd_log_ctx **ctx);
void osd_log_free(struct osd_log_ctx *ctx);
void osd_set_log_fn(struct osd_log_ctx *ctx, osd_log_fn log_fn);
osd_result osd_get_log_priority(struct osd_log_ctx *ctx);
void osd_set_log_priority(struct osd_log_ctx *ctx, int priority);


/**
 * API version
 */
struct osd_version {
    /** major version */
    const uint16_t major;

    /** minor version */
    const uint16_t minor;

    /** micro version */
    const uint16_t micro;

    /**
     * suffix string, e.g. for release candidates ("-rc4") and development
     * versions ("-dev")
     */
    const char *suffix;
};

/**
 * Module type identifiers for the standard-defined modules (vendor id 0x00)
 */
enum osd_module_types {
    MOD_STD_HOST = 0, //!< the host
    MOD_STD_SCM = 1, //!< System Control Module (SCM)
    MOD_STD_DEM_UART = 2, //!< Device Emulation Module UART (DEM_UART)
    MOD_STD_MAM = 3, //!< Memory Access Module (MAM)
    MOD_STD_STM = 4, //!< System Trace Module (STM)
    MOD_STD_CTM = 5 //!< Core Trace Module (CTM)
};

/**
 * A single module instance in the Open SoC Debug system
 */
struct osd_module_desc {
    uint16_t addr;    //!< Address in the Debug Interconnect
    uint16_t type;    //!< Module type
    uint16_t vendor;  //!< Module version
    uint16_t version; //!< Module version
};

const struct osd_version * osd_get_version(void);

#ifdef __cplusplus
}
#endif
