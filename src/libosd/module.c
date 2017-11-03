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

#include <osd/osd.h>
#include <osd/module.h>
#include "osd-private.h"

static const char *OSD_MODULE_TYPE_STD_shortnames[] = {
#define LIST_ENTRY(type_id, shortname, longname) [type_id] = #shortname,
    OSD_MODULE_TYPE_STD_LIST
#undef LIST_ENTRY
};

#define LIST_ENTRY(type_id, shortname, longname) [type_id] = longname,
static const char *OSD_MODULE_TYPE_STD_longnames[] = { OSD_MODULE_TYPE_STD_LIST };
#undef LIST_ENTRY

API_EXPORT
const char* osd_module_get_type_std_short_name(unsigned int type_id)
{
    return OSD_MODULE_TYPE_STD_shortnames[type_id];
}

API_EXPORT
const char* osd_module_get_type_std_long_name(unsigned int type_id)
{
    return OSD_MODULE_TYPE_STD_longnames[type_id];
}
