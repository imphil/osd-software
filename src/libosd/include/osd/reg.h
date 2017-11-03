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

#ifndef OSD_REG_H
#define OSD_REG_H

/**
 * @defgroup libosd-reg Register definitions
 * @ingroup libosd
 *
 * @{
 */

// register maps
// base register map (common across all debug modules)
#define OSD_REG_BASE_MOD_VENDOR      0x0000 /* module type */
#define OSD_REG_BASE_MOD_TYPE        0x0001 /* module version */
#define OSD_REG_BASE_MOD_VERSION     0x0002 /* module vendor */
#define OSD_REG_BASE_MOD_CS          0x0003 /* control and status */
  #define OSD_REG_BASE_MOD_CS_ACTIVE   BIT(0) /* activate/stall module */
#define OSD_REG_BASE_MOD_EVENT_DEST  0x0004 /* event destination */
  #define OSD_REG_BASE_MOD_EVENT_DEST_ADDR_SHIFT 0
  #define OSD_REG_BASE_MOD_EVENT_DEST_ADDR_MASK  ((1 << 10) - 1)

// SCM register map
#define OSD_REG_SCM_SYSTEM_VENDOR_ID 0x0200
#define OSD_REG_SCM_SYSTEM_DEVICE_ID 0x0201
#define OSD_REG_SCM_NUM_MOD          0x0202
#define OSD_REG_SCM_MAX_PKT_LEN      0x0203
#define OSD_REG_SCM_SYSRST           0x0204
  #define OSD_REG_SCM_SYSRST_SYS_RST   BIT(0)
  #define OSD_REG_SCM_SYSRST_CPU_RST   BIT(1)

/**@}*/ /* end of doxygen group libosd-reg */

#endif // OSD_REG_H
