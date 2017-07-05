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
 * A packet in the Open SoC Debug system
 */
// We must use zero-length data members (a GCC extension) instead of standard C
// flexible arrays (uint16_t payload[]) as flexible array members are not
// allowed in unions by the C standard.
struct osd_packet {
    uint16_t size_data; //!< size of data (or data_raw) in uint16_t words
    union {
        struct {
            uint16_t dest;       //!< packet destination address
            uint16_t src;        //!< packet source address
            uint16_t flags;      //!< packet flags
            uint16_t payload[0]; //!< (size_data - 3) words of payload
        } data;

        uint16_t data_raw[0];    //!< size_data words of data
    };
};

/**
 * Packet types
 */
enum osd_packet_type {
    OSD_PACKET_TYPE_REG = 0,   //< Register access
    OSD_PACKET_TYPE_PLAIN = 1, //< Plain (unspecified content)
    OSD_PACKET_TYPE_EVENT = 2, //< Debug Event
    OSD_PACKET_TYPE_RES = 3    //< Reserved (will be discarded)
};

/**
 * Values of the TYPE_SUB field in if TYPE == OSD_PACKET_TYPE_REG
 */
enum osd_packet_type_reg_subtype {
    REQ_READ_REG_16 = 0b0000,           //< 16 bit register read request
    REQ_READ_REG_32 = 0b0001,           //< 32 bit register read request
    REQ_READ_REG_64 = 0b0010,           //< 64 bit register read request
    REQ_READ_REG_128 = 0b0011,          //< 128 bit register read request
    RESP_READ_REG_SUCCESS_16 = 0b1000,  //< 16 bit register read response
    RESP_READ_REG_SUCCESS_32 = 0b1001,  //< 32 bit register read response
    RESP_READ_REG_SUCCESS_64 = 0b1010,  //< 64 bit register read response
    RESP_READ_REG_SUCCESS_128 = 0b1011, //< 128 bit register read response
    RESP_READ_REG_ERROR = 0b1100,       //< register read failure
    REQ_WRITE_REG_16 = 0b0100,          //< 16 bit register write request
    REQ_WRITE_REG_32 = 0b0101,          //< 32 bit register write request
    REQ_WRITE_REG_64 = 0b0110,          //< 64 bit register write request
    REQ_WRITE_REG_128 = 0b0111,         //< 128 bit register write request
    RESP_WRITE_REG_SUCCESS = 0b1110,    //< the preceding write request was
                                        //  successful
    RESP_WRITE_REG_ERROR = 0b1111       //< the preceding write request failed
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

// Module addresses defined by the OSD spec
#define OSD_MOD_ADDR_HIM (17 | (1<<10))  /* subnet *///< Address of the Host Interface Module
#define OSD_MOD_ADDR_SCM 0 //< Address of the System Control Module

const struct osd_version * osd_version_get(void);

#ifdef __cplusplus
}
#endif
