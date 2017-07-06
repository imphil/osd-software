#include <stdint.h>
#include <pthread.h>

#pragma once

/** Information about the connected system */
struct osd_system_info {
    /** vendor identifier */
    uint16_t vendor_id;
    /** device identifier */
    uint16_t device_id;
    /** maximum number of words in a debug packet supported by the device */
    uint16_t max_pkt_len;
};

/**
 * Communication context
 */
struct osd_hostmod_ctx {
    /** Is the library connected to a device? */
    int is_connected;

    /** Logging context */
    struct osd_log_ctx *log_ctx;

    uint16_t addr;

    /** communication socket to the host controller */
    zsock_t *socket;

    /** receive zloop */
    zloop_t *rx_zloop;

    /** Control data receive thread */
    pthread_t thread_ctrl_receive;
    /** Lock protecting all register accesses */
    pthread_mutex_t reg_access_lock;
    /** Register access response has been received */
    pthread_cond_t reg_access_complete;
    /** Last received control packet */
    struct osd_packet *rcv_ctrl_packet;

    /** system information */
    struct osd_system_info system_info;

    /** list of modules in the system */
    struct osd_module_desc *modules;
    /** number of elements in modules */
    size_t modules_len;
};


/**
 * Debug Transport Datagram
 */
typedef uint16_t* osd_dtd;

size_t osd_dtd_get_size_words(osd_dtd dtd);
osd_result osd_packet_get_dtd(struct osd_packet *packet, osd_dtd *dtd);
osd_result osd_dtd_to_packet(osd_dtd dtd, struct osd_packet** packet);

// register maps
// base register map (common across all debug modules)
#define REG_BASE_MOD_VENDOR      0x0000 /* module type */
#define REG_BASE_MOD_TYPE        0x0001 /* module version */
#define REG_BASE_MOD_VERSION     0x0002 /* module vendor */
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
