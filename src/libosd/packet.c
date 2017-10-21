/**
 * Class: A Debug Packet in the OSD
 */

#include <osd/osd.h>
#include <osd/packet.h>
#include "osd-private.h"
#include <assert.h>
#include <errno.h>
#include <string.h>


#define MACROSTR(k) #k

/**
 * Get the data size including all headers for a given payload size
 */
API_EXPORT
const uint16_t osd_packet_get_data_size_words_from_payload(const unsigned int size_payload)
{
    unsigned int s = size_payload + 3 /* dest, src, flags */;
    assert(s <= UINT16_MAX);
    return s;
}

/**
 * Allocate memory for a packet with given data size and zero all data fields
 *
 * The osd_packet.size field is set to the allocated size.
 *
 * @param[out] packet the packet to be allocated
 * @param[in]  data_size_words number of uint16_t words in the packet, including the
 *             header words.
 * @return the allocated packet, or NULL if allocation fails
 */
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

/**
 * Free the memory associated with the packet
 */
API_EXPORT
void osd_packet_free(struct osd_packet *packet)
{
    free(packet);
}

/**
 * Extract the DEST field out of a packet
 */
API_EXPORT
unsigned int osd_packet_get_dest(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.dest >> DP_HEADER_DEST_SHIFT)
           & DP_HEADER_DEST_MASK;
}

/**
 * Extract the SRC field out of a packet
 */
API_EXPORT
unsigned int osd_packet_get_src(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.src >> DP_HEADER_SRC_SHIFT)
           & DP_HEADER_SRC_MASK;
}

/**
 * Extract the TYPE field out of a packet
 */
API_EXPORT
unsigned int osd_packet_get_type(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.flags >> DP_HEADER_TYPE_SHIFT)
           & DP_HEADER_TYPE_MASK;
}

/**
 * Extract the TYPE_SUB field out of a packet
 */
API_EXPORT
unsigned int osd_packet_get_type_sub(const struct osd_packet *packet)
{
    assert((packet->data_size_words >= 3) &&
           "The packet must be large enough for 3 header words.");

    return (packet->data.flags >> DP_HEADER_TYPE_SUB_SHIFT)
           & DP_HEADER_TYPE_SUB_MASK;
}

/**
 * Populate the header of a osd_packet
 *
 * @param packet
 * @param dest     packet destination
 * @param src      packet source
 * @param type     packet type
 * @param type_sub packet subtype
 *
 * @return OSD_OK on success, any other value indicates an error
 */
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

/**
 * Size in bytes of a packet
 */
API_EXPORT
size_t osd_packet_sizeof(struct osd_packet *packet)
{
    return packet->data_size_words * sizeof(uint16_t);
}

/**
 * Write a debug message to the log with the contents of a packet
 */
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

