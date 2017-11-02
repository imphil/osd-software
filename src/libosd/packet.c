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
#include <osd/packet.h>
#include "osd-private.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

#define MACROSTR(k) #k


API_EXPORT
const uint16_t osd_packet_get_data_size_words_from_payload(const unsigned int size_payload)
{
    unsigned int s = size_payload + 3 /* dest, src, flags */;
    assert(s <= UINT16_MAX);
    return s;
}

API_EXPORT
osd_result osd_packet_new(struct osd_packet **packet, size_t data_size_words)
{
    ssize_t size = sizeof(uint16_t) * 1            // osd_packet.data_size_words
                   + sizeof(uint16_t) * data_size_words; // osd_packet.data
    struct osd_packet *pkg = calloc(1, size);
    assert(pkg);

    pkg->data_size_words = data_size_words;

    *packet = pkg;

    return OSD_OK;
}

API_EXPORT
void osd_packet_free(struct osd_packet **packet_p)
{
    assert(packet_p);
    struct osd_packet *packet = *packet_p;

    free(packet);
    *packet_p = NULL;
}

API_EXPORT
unsigned int osd_packet_get_dest(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.dest >> DP_HEADER_DEST_SHIFT)
           & DP_HEADER_DEST_MASK;
}

API_EXPORT
unsigned int osd_packet_get_src(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.src >> DP_HEADER_SRC_SHIFT)
           & DP_HEADER_SRC_MASK;
}

API_EXPORT
unsigned int osd_packet_get_type(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.flags >> DP_HEADER_TYPE_SHIFT)
           & DP_HEADER_TYPE_MASK;
}

API_EXPORT
unsigned int osd_packet_get_type_sub(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.flags >> DP_HEADER_TYPE_SUB_SHIFT)
           & DP_HEADER_TYPE_SUB_MASK;
}

API_EXPORT
osd_result osd_packet_set_header(struct osd_packet* packet,
                                 const unsigned int dest,
                                 const unsigned int src,
                                 const enum osd_packet_type type,
                                 const unsigned int type_sub)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    packet->data.dest = 0x0000;
    packet->data.src = 0x0000;
    packet->data.flags = 0x0000;

    // DEST
    assert((dest & DP_HEADER_DEST_MASK) == dest); // overflow detection
    packet->data.dest |= (dest & DP_HEADER_DEST_MASK)
            << DP_HEADER_DEST_SHIFT;

    // SRC
    assert((src & DP_HEADER_SRC_MASK) == src);
    packet->data.src |= (src & DP_HEADER_SRC_MASK)
            << DP_HEADER_SRC_SHIFT;

    // FLAGS.TYPE
    assert((type & DP_HEADER_TYPE_MASK) == type);
    packet->data.flags |= (type & DP_HEADER_TYPE_MASK)
            << DP_HEADER_TYPE_SHIFT;

    // FLAGS.TYPE_SUB
    assert((type_sub & DP_HEADER_TYPE_SUB_MASK) == type_sub);
    packet->data.flags |= (type_sub & DP_HEADER_TYPE_SUB_MASK)
            << DP_HEADER_TYPE_SUB_SHIFT;

    return OSD_OK;
}

API_EXPORT
size_t osd_packet_sizeof(struct osd_packet *packet)
{
    return packet->data_size_words * sizeof(uint16_t);
}

API_EXPORT
void osd_packet_log(const struct osd_packet *packet,
                    struct osd_log_ctx *log_ctx)
{
    static char* osd_packet_type_name[] = {
        MACROSTR(OSD_PACKET_TYPE_REG),
        MACROSTR(OSD_PACKET_TYPE_PLAIN),
        MACROSTR(OSD_PACKET_TYPE_EVENT),
        MACROSTR(OSD_PACKET_TYPE_RES),
    };

    dbg(log_ctx, "Packet of %u data words:\n", packet->data_size_words);
    if (packet->data_size_words >= 3) {
        dbg(log_ctx, "DEST = %u, SRC = %u, TYPE = %u (%s), TYPE_SUB = %u\n",
            osd_packet_get_dest(packet),
            osd_packet_get_src(packet),
            osd_packet_get_type(packet),
            osd_packet_type_name[osd_packet_get_type(packet)],
            osd_packet_get_type_sub(packet));
    }
    dbg(log_ctx, "Packet data (including header):\n");
    for (uint16_t i = 0; i < packet->data_size_words; i++) {
        dbg(log_ctx, "  0x%04x\n", packet->data_raw[i]);
    }
}

