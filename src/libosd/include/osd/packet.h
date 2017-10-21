#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup libosd-packet Packet
 * @ingroup libosd
 *
 * @{
 */

/**
 * A packet in the Open SoC Debug system
 */
// We must use zero-length data members (a GCC extension) instead of standard C
// flexible arrays (uint16_t payload[]) as flexible array members are not
// allowed in unions by the C standard.
struct osd_packet {
    uint16_t data_size_words; //!< size of data/data_raw in uint16_t words
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


// debug packet structure
#define DP_HEADER_TYPE_SHIFT     14
#define DP_HEADER_TYPE_MASK      0b11

#define DP_HEADER_TYPE_SUB_SHIFT 10
#define DP_HEADER_TYPE_SUB_MASK  0b1111

#define DP_HEADER_SRC_SHIFT      0
#define DP_HEADER_SRC_MASK       ((1 << 16) - 1)

#define DP_HEADER_DEST_SHIFT     0
#define DP_HEADER_DEST_MASK      ((1 << 16) - 1)

const uint16_t osd_packet_get_data_size_words_from_payload(const unsigned int size_payload);
osd_result osd_packet_new(struct osd_packet **packet,
                          size_t size_data_words);
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

/**@}*/ /* end of doxygen group libosd-packet */

#ifdef __cplusplus
}
#endif
