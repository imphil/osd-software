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

#include <stdlib.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


struct osd_memory_descriptor {
    uint16_t data_width;
    uint16_t addr_width;
    uint8_t num_regions;
    struct {
        uint64_t base_addr;
        uint64_t size;
    } regions[];
};

/**
 * Get all memories in the system
 */
osd_result osd_memory_list(struct osd_memory_descriptor **memories);

/**
 * Write data to a memory
 */
osd_result osd_memory_write(struct osd_memory_descriptor *mem,
                            const void *data, size_t nbyte,
                            const unsigned int start_addr,
                            int flags);

/**
 * Read data from a memory
 */
osd_result osd_memory_read(struct osd_memory_descriptor *mem, uint8_t** data,
                           size_t nbyte, const unsigned int start_addr,
                           int flags);


#ifdef __cplusplus
}
#endif
