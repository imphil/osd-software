/**
 * Open SoC Debug host controller
 */

#define CLI_TOOL_PROGNAME "osd-host-controller"
#define CLI_TOOL_SHORTDESC "Open SoC Debug host controller"

#include "../cli-util.h"
#include <czmq.h>

zsock_t *server_socket;

static void mgmt_send_ack(const zframe_t* dest)
{
    zmsg_t* msg = zmsg_new();
    zframe_t* dest_new = zframe_dup(dest);
    zmsg_add(msg, dest_new);
    zmsg_addstr(msg, "M");
    zmsg_addstr(msg, "ACK");
    zmsg_send(&msg, server_socket);
    zmsg_destroy(&msg);
}

static void process_mgmt_msg(const zframe_t* src, const zframe_t* payload_frame)
{
    char* msg = zframe_strdup(payload_frame);
    dbg("Received management message %s\n", msg);
    free(msg);

    mgmt_send_ack(src);
}

static void process_data_msg(const zframe_t* src, const zframe_t* payload_frame)
{
#if 0
    uint16_t* client_addr = zhash_lookup(hash, routing_id);
    if (client_addr != NULL) {
      printf("Client is known to server already with address %u\n", *client_addr);
    } else {
      printf("Client unknown, recording its address.\n");
      uint16_t *value = malloc(sizeof(uint16_t));
      *value = next_addr++;
      zhash_insert(hash, routing_id, value);
    }
#endif
}

void setup(void)
{
    // XXX: nothing to do right now
}

int run(void)
{
    zsys_init();

    server_socket = zsock_new_router("tcp://0.0.0.0:9990");

    // address lookup hash
    zhash_t *hash = zhash_new();

    while (!zsys_interrupted) {
        zmsg_t * msg = zmsg_recv(server_socket);
        if (!msg && errno == EINTR) {
            // user pressed CTRL-C (SIGHUP)
            printf("Exiting server ...\n");
            break;
        }
        printf("Received message: \n");
        zmsg_print(msg);

        zframe_t *src_frame = zmsg_pop(msg);
        char* routing_id = zframe_strhex(src_frame);
        printf("sender: %s\n", routing_id);
        free(routing_id);

        zframe_t* type_frame = zmsg_pop(msg);
        if (zframe_streq(type_frame, "M")) {
            zframe_t* payload_frame = zmsg_pop(msg);
            process_mgmt_msg(src_frame, payload_frame);
        } else if (zframe_streq(type_frame, "D")) {
            zframe_t* payload_frame = zmsg_pop(msg);
            process_data_msg(src_frame, payload_frame);
        } else {
            err("Message of unknown type received. Ignoring.\n");
            goto next_msg;
        }

next_msg:
        zmsg_destroy(&msg);
    }

    zhash_destroy(&hash);
    zsock_destroy(&server_socket);
    return 0;
}
