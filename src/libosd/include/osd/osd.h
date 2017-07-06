/**
 * Open SoC Debug Library (common parts)
 *
 * Data structures and functionality used across the layers of Open SoC Debug.
 */


#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Standard return type */
typedef int osd_result;

/** Return code: The operation was successful */
#define OSD_OK 0
/** Return code: Generic (unknown) failure */
#define OSD_ERROR_FAILURE -1
/** Return code: debug system returned a failure */
#define OSD_ERROR_DEVICE_ERROR -2
/** Return code: received invalid or malformed data from device */
#define OSD_ERROR_DEVICE_INVALID_DATA -3
/** Return code: failed to communicate with device */
#define OSD_ERROR_COM -4
/** Return code: operation timed out */
#define OSD_ERROR_TIMEDOUT -5
/** Return code: not connected to the device */
#define OSD_ERROR_NOT_CONNECTED -6
/** Return code: not all debug modules have been properly enumerated */
#define OSD_ERROR_ENUMERATION_INCOMPLETE -7
/** Return code: operation aborted */
#define OSD_ERROR_ABORTED -8
/** Return code: connection failed */
#define OSD_ERROR_CONNECTION_FAILED -9
/** Return code: Out of memory */
#define OSD_ERROR_OOM -11


/**
 * Return true if |rv| is an error code
 */
#define OSD_FAILED(rv) ((rv) < 0)
/**
 * Return true if |rv| is a successful return code
 */
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

osd_result osd_log_new(struct osd_log_ctx **ctx,
                       int log_priority,
                       osd_log_fn log_fn);
void osd_log_free(struct osd_log_ctx *ctx);
void osd_log_set_fn(struct osd_log_ctx *ctx, osd_log_fn log_fn);
osd_result osd_log_get_priority(struct osd_log_ctx *ctx);
void osd_log_set_priority(struct osd_log_ctx *ctx, int priority);
void osd_log_set_caller_ctx(struct osd_log_ctx *ctx, void *caller_ctx);
void* osd_log_get_caller_ctx(struct osd_log_ctx *ctx);


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
 * Module type identifiers for the standard-defined modules (vendor id 0x01)
 */
enum osd_module_type {
    MOD_STD_SCM = 1, //!< Subnet Control Module (SCM)
    MOD_STD_DEM_UART = 2, //!< Device Emulation Module UART (DEM_UART)
    MOD_STD_MAM = 3, //!< Memory Access Module (MAM)
    MOD_STD_STM = 4, //!< System Trace Module (STM)
    MOD_STD_CTM = 5 //!< Core Trace Module (CTM)
};

/**
 * A single module instance in the Open SoC Debug system
 */
struct osd_module_desc {
    uint16_t addr;    //!< Module address
    uint16_t vendor;  //!< Module version
    uint16_t type;    //!< Module type
    uint16_t version; //!< Module version
};

const struct osd_version * osd_version_get(void);

/**
 * Number of bits in the address used to describe the subnet
 */
#define OSD_DIADDR_SUBNET_BITS 6
#define OSD_DIADDR_LOCAL_BITS (16 - OSD_DIADDR_SUBNET_BITS)
#define OSD_DIADDR_SUBNET_MAX ((1 << OSD_DIADDR_SUBNET_BITS) - 1)
#define OSD_DIADDR_LOCAL_MAX ((1 << OSD_DIADDR_LOCAL_BITS) - 1)

unsigned int osd_diaddr_subnet(unsigned int diaddr);
unsigned int osd_diaddr_localaddr(unsigned int diaddr);
unsigned int osd_diaddr_build(unsigned int subnet, unsigned int local_diaddr);

#ifdef __cplusplus
}
#endif
