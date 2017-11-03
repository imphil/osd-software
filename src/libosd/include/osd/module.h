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

#ifndef OSD_MODULE_H
#define OSD_MODULE_H

#include <stdint.h>

/**
 * @defgroup libosd-module Module definitions
 * @ingroup libosd
 *
 * @{
 */

/**
 * List of all registered vendor IDs
 *
 * Keep this list in sync with
 * http://opensocdebug.readthedocs.io/en/latest/05_idregistry/index.html
 */
#define OSD_MODULE_VENDOR_LIST \
    LIST_ENTRY(0x0001, OSD, "The Open SoC Debug Project") \
    LIST_ENTRY(0x0002, OPTIMSOC, "The OpTiMSoC Project") \
    LIST_ENTRY(0x0003, LOWRISC, "LowRISC")

/**
 * List of module types as defined in the Open SoC Debug Specification
 *
 * All modules in this list have a vendor id 0x0001
 */
#define OSD_MODULE_TYPE_STD_LIST \
    LIST_ENTRY(0x0001, SCM, "Subnet Control Module") \
    LIST_ENTRY(0x0002, DEM_UART, "Device Emulation Module UART") \
    LIST_ENTRY(0x0003, MAM, "Memory Access Module") \
    LIST_ENTRY(0x0004, STM, "System Trace Module") \
    LIST_ENTRY(0x0005, CTM, "Core Trace Module")

/**
 * Vendor identifiers
 */
enum osd_module_vendor {
#define LIST_ENTRY(vendor_id, shortname, longname) OSD_MODULE_VENDOR_##shortname = vendor_id, //!< longname
    OSD_MODULE_VENDOR_LIST
#undef LIST_ENTRY
};

/**
 * Module type identifiers for the standard-defined modules (vendor id 0x0001)
 */
enum osd_module_type_std {
#define LIST_ENTRY(type_id, shortname, longname) OSD_MODULE_TYPE_STD_##shortname = type_id, //!< longname
    OSD_MODULE_TYPE_STD_LIST
#undef LIST_ENTRY
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

const char* osd_module_get_type_std_short_name(unsigned int type_id);
const char* osd_module_get_type_std_long_name(unsigned int type_id);

/**@}*/ /* end of doxygen group libosd-module */

#endif // OSD_MODULE_H
