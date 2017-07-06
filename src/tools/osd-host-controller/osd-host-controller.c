/**
 * Open SoC Debug host controller
 */

#define CLI_TOOL_PROGNAME "osd-host-controller"
#define CLI_TOOL_SHORTDESC "Open SoC Debug host controller"

#include "../cli-util.h"
#include <czmq.h>

zsock_t *server_socket;

uint16_t debug_addr_max;

static void mgmt_send_ack(zframe_t* dest)
{
    zmsg_t* msg = zmsg_new();
    zframe_t* dest_new = zframe_dup(dest);
    zmsg_add(msg, dest_new);
    zmsg_addstr(msg, "M");
    zmsg_addstr(msg, "ACK");
    zmsg_send(&msg, server_socket);
    zmsg_destroy(&msg);
}

static void mgmt_addr_release(zframe_t* dest)
{
    // XXX: implement this properly
    return mgmt_send_ack(dest);
}

static void mgmt_addr_request(zframe_t* dest)
{
    debug_addr_max += 100;
    zmsg_t* msg = zmsg_new();
    zframe_t* dest_new = zframe_dup(dest);
    zmsg_add(msg, dest_new);
    zmsg_addstr(msg, "M");
    zmsg_addstrf(msg, "%u", debug_addr_max);
    zmsg_send(&msg, server_socket);
    zmsg_destroy(&msg);

    printf("assigned id to device\n");
}

byte gw_routing_id[5];

static void mgmt_gw_register(zframe_t* dest, char* params)
{
    char* end;
    unsigned int subnet = strtol(params, &end, 10);
    assert(!*end);
    assert(subnet < UINT16_MAX);

    //gw_routing_id = zframe_strhex(dest);
    assert(zframe_size(dest) == 5);
    memcpy(gw_routing_id, zframe_data(dest), 5);

    dbg("Registering gateway %s for subnet %u\n", zframe_strhex(dest), subnet);
    mgmt_send_ack(dest);
}

static void process_mgmt_msg(const zframe_t* src, const zframe_t* payload_frame)
{
    char* request = zframe_strdup(payload_frame);
    dbg("Received management message %s\n", request);

    if (strcmp(request, "ADDR_REQUEST") == 0) {
        mgmt_addr_request(src);
    } else if (strcmp(request, "ADDR_RELEASE") == 0) {
        mgmt_addr_release(src);
    } else if (strncmp(request, "GW_REGISTER", strlen("GW_REGISTER")) == 0) {
        mgmt_gw_register(src, request + strlen("GW_REGISTER") + 1 /* space == parameter separator */);
    } else {
        mgmt_send_ack(src);
    }

    free(request);
}

static void process_data_msg(const zframe_t* src, const zframe_t* payload_frame)
{
    osd_result rv;
    dbg("Received data message\n");

    struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, zframe_size(payload_frame)*sizeof(uint16_t));
    assert(!OSD_FAILED(rv));

    unsigned int dest = osd_packet_get_dest(pkg);
    unsigned int subnet = osd_addr_subnet(dest);

    dbg("packet for subnet %u, forwarding to gw\n", subnet);

    zmsg_t *msg = zmsg_new();
    assert(msg);
    zmsg_addmem(msg, gw_routing_id, 5);
    zmsg_addstr(msg, "D");
    zmsg_add(msg, payload_frame);
    zmsg_send(&msg, server_socket);


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

osd_result setup(void)
{
    // XXX: nothing to do right now
    return OSD_OK;
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
