#ifndef MOCK_HOST_CONTROLLER_H
#define MOCK_HOST_CONTROLLER_H

#include <check.h>

#include <osd/osd.h>
#include <osd/packet.h>
#include <czmq.h>

void mock_host_controller_setup(void);
void mock_host_controller_teardown(void);

osd_result mock_host_controller_queue_event_packet(const struct osd_packet *pkg);
void mock_host_controller_expect_reg_write(unsigned int src,
                                           unsigned int dest,
                                           unsigned int reg_addr,
                                           uint16_t exp_write_value);
void mock_host_controller_expect_reg_read(unsigned int src,
                                          unsigned int dest,
                                          unsigned int reg_addr,
                                          uint16_t ret_value);
void mock_host_controller_expect_mgmt_req(const char* cmd, const char* resp);
void mock_host_controller_expect_diaddr_req(unsigned int diaddr);
void mock_host_controller_expect_data_req(struct osd_packet *req, struct osd_packet *resp);
void mock_host_controller_wait_for_event_tx();
#endif // MOCK_HOST_CONTROLLER_H
