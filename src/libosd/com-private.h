#include <stdint.h>
#include <pthread.h>

#pragma once

/**
 * Debug Transport Datagram
 */
typedef uint16_t* osd_dtd;

/**
 * Get the size of the Debug Transport Datagram in words
 */
size_t osd_dtd_get_size_words(osd_dtd dtd)
{
    if (!dtd) {
        return 0;
    }

    // the first word in the DTD always contains the size. We then need to add
    // the word containing the size itself.
    return dtd[0] + 1;
}

/**
 * Get the Debug Transport Datagram representation of a packet
 *
 * Note that the ownership of the data in @p dtd remains with the packet, do
 * not free @p dtd.
 */
osd_result osd_packet_get_dtd(struct osd_packet *packet, osd_dtd *dtd)
{
    *dtd = &packet->size_data;
    return OSD_OK;
}

/**
 * Set the data of a packet from a Debug Transport Datagram
 *
 * The ownership of the @p dtd is transferred to the packet, @p dtd may not be
 * used or freed anymore after calling this function.
 */
/*osd_result osd_packet_set_dtd(struct osd_packet* packet, osd_dtd dtd)
{
    &packet->size_data = dtd;
    return OSD_OK;
}*/

struct osd_system_info {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t max_pkt_len;
};

struct osd_com_ctx {
    int is_running;

    /** Logging context */
    struct osd_log_ctx *log_ctx;
    struct osd_com_device_if *device_ctrl_if;
    struct osd_com_device_if *device_event_if;
    enum osd_com_byte_order device_transport_byte_order;
    /** Control data receive thread */
    pthread_t thread_ctrl_receive;

    struct osd_system_info system_info;
    struct osd_module_desc *modules;
    size_t modules_len;

    /** Lock protecting all register accesses */
    pthread_mutex_t reg_access_lock;
    /** Register access response has been received */
    pthread_cond_t reg_access_complete;
    /** Last received control packet */
    struct osd_packet *rcv_ctrl_packet;
};



struct osd_com_client {
};

// register maps
// base register map (common across all debug modules)
#define REG_BASE_MOD_ID          0x0000 /* module id */
#define REG_BASE_MOD_VERSION     0x0001 /* module version */
#define REG_BASE_MOD_VENDOR      0x0002 /* module vendor */
#define REG_BASE_MOD_CS          0x0003 /* control and status */
  #define REG_BASE_MOD_CS_ACTIVE   BIT(0) /* activate/stall module */
#define REG_BASE_MOD_EVENT_DEST  0x0004 /* event destination */
  #define REG_BASE_MOD_EVENT_DEST_ADDR_SHIFT 0
  #define REG_BASE_MOD_EVENT_DEST_ADDR_MASK  ((1 << 10) - 1)

// SCM register map
#define REG_SCM_SYSTEM_VENDOR_ID 0x0200
#define REG_SCM_SYSTEM_DEVICE_ID 0x0201
#define REG_SCM_NUM_MOD          0x0202
#define REG_SCM_MAX_PKT_LEN      0x0203
#define REG_SCM_SYSRST           0x0204
  #define REG_SCM_SYSRST_SYS_RST   BIT(0)
  #define REG_SCM_SYSRST_CPU_RST   BIT(1)
