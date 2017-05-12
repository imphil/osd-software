#include <stdint.h>

#pragma once

struct osd_com_ctx {
    struct osd_log_ctx *log_ctx;
    struct osd_system_info *system_info;
    struct osd_module_desc *modules;
    size_t modules_len;
};


struct osd_com_system_info {
    uint16_t identifier;
    uint16_t max_pkt_len;
};


struct osd_com_client {
};
