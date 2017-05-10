/**
 * Open SoC Debug common functionality (header-only)
 *
 * Data structures and functionality used across the layers of Open SoC Debug.
 */


#include <inttypes.h>

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

#define API_EXPORT __attribute__ ((visibility("default")))

/**
 * Module type identifiers for the standard-defined modules (vendor id 0x00)
 */
enum osd_module_types {
    OSD_MOD_STD_HOST = 0, //!< the host
    OSD_MOD_STD_SCM = 1, //!< System Control Module (SCM)
    OSD_MOD_STD_DEM_UART = 2, //!< Device Emulation Module UART (DEM_UART)
    OSD_MOD_STD_MAM = 3, //!< Memory Access Module (MAM)
    OSD_MOD_STD_STM = 4, //!< System Trace Module (STM)
    OSD_MOD_STD_CTM = 5 //!< Core Trace Module (CTM)
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

#ifdef __cplusplus
}
#endif
