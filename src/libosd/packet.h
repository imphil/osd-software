#pragma once


// debug packet structure
#define DP_HEADER_TYPE_SHIFT     14
#define DP_HEADER_TYPE_MASK      0b11

#define DP_HEADER_TYPE_SUB_SHIFT 10
#define DP_HEADER_TYPE_SUB_MASK  0b1111

#define DP_HEADER_SRC_SHIFT      0
#define DP_HEADER_SRC_MASK       ((1 << 16) - 1)

#define DP_HEADER_DEST_SHIFT     0
#define DP_HEADER_DEST_MASK      ((1 << 16) - 1)

const uint16_t osd_packet_get_size_data_from_payload(const unsigned int size_payload);
osd_result osd_packet_new(struct osd_packet **packet,
                          const unsigned int size_payload);
void osd_packet_free(struct osd_packet *packet);
unsigned int osd_packet_get_dest(const struct osd_packet *packet);
unsigned int osd_packet_get_src(const struct osd_packet *packet);
unsigned int osd_packet_get_type(const struct osd_packet *packet);
unsigned int osd_packet_get_type_sub(const struct osd_packet *packet);
osd_result osd_packet_set_header(struct osd_packet* packet,
                                 const unsigned int dest,
                                 const unsigned int src,
                                 const enum osd_packet_type type,
                                 const unsigned int type_sub);
size_t osd_packet_sizeof(struct osd_packet *packet);
void osd_packet_log(const struct osd_packet *packet,
                    struct osd_log_ctx *log_ctx);

