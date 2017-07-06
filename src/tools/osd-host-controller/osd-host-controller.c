/**
 * Open SoC Debug host controller
 */

#define CLI_TOOL_PROGNAME "osd-host-controller"
#define CLI_TOOL_SHORTDESC "Open SoC Debug host controller"

#include "../cli-util.h"
#include <czmq.h>
#include <osd/packet.h>

zsock_t *server_socket;

uint16_t debug_addr_max;

unsigned int subnet_addr;

// size (in bytes) of the host address (== the zeromq identity frame)
#define HOSTADDR_SIZE 5

zframe_t** mods_in_subnet;
zframe_t** gateways;

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

static void mgmt_send_nack(zframe_t* dest)
{
    zmsg_t* msg = zmsg_new();
    zframe_t* dest_new = zframe_dup(dest);
    zmsg_add(msg, dest_new);
    zmsg_addstr(msg, "M");
    zmsg_addstr(msg, "NACK");
    zmsg_send(&msg, server_socket);
    zmsg_destroy(&msg);
}

/**
 * Get an available address in the local subnet
 */
static osd_result osd_hostctrl_diaddr_find_available(unsigned int *diaddr)
{
    unsigned int localaddr;
    for (localaddr = 1; localaddr <= OSD_DIADDR_LOCAL_MAX; localaddr++) {
        if (mods_in_subnet[localaddr] == NULL) {
            *diaddr = osd_diaddr_build(subnet_addr, localaddr);
            return OSD_OK;
        }
    }

    err("No more DI addresses available in subnet.\n");
    return OSD_ERROR_FAILURE;
}

static osd_result osd_hostctrl_diaddr_register(zframe_t* hostaddr, unsigned int diaddr)
{
    unsigned int localaddr = osd_diaddr_localaddr(diaddr);
    assert(mods_in_subnet[localaddr] == NULL);
    mods_in_subnet[localaddr] = hostaddr;

    dbg("Registered diaddr %u.%u (%u) for host module %s\n",
        osd_diaddr_subnet(diaddr), osd_diaddr_localaddr(diaddr),
        diaddr, zframe_strhex(hostaddr));

    return OSD_OK;
}

/**
 * Assign a new debug interconnect address to a host module in our subnet
 */
static void mgmt_diaddr_request(zframe_t* hostaddr)
{
    osd_result rv;
    unsigned int diaddr;
    rv = osd_hostctrl_diaddr_find_available(&diaddr);
    assert(rv == OSD_OK);

    zframe_t* hostaddr2 = zframe_dup(hostaddr);
    rv = osd_hostctrl_diaddr_register(hostaddr2, diaddr);
    assert(rv == OSD_OK);

    zmsg_t* msg = zmsg_new();
    zframe_t* dest = zframe_dup(hostaddr);
    zmsg_add(msg, dest);
    zmsg_addstr(msg, "M");
    zmsg_addstrf(msg, "%u", diaddr);
    zmsg_send(&msg, server_socket);
}

static void mgmt_diaddr_release(zframe_t* hostaddr)
{
    unsigned int i, localaddr;
    int found = 0;
    for (i = 1; i < OSD_DIADDR_SUBNET_MAX; i++) {
        if (zframe_eq(mods_in_subnet[i], hostaddr)) {
            localaddr = i;
            found = 1;
            break;
        }
    }
    if (!found) {
        err("Trying to release address for host which isn't registered.\n");
        return mgmt_send_nack(hostaddr);
    }

    zframe_destroy(mods_in_subnet[i]);
    mods_in_subnet[i] = NULL;
    dbg("Releasing address %u for host module %s\n", i,
        zframe_strhex(hostaddr));

    return mgmt_send_ack(hostaddr);
}

static void mgmt_gw_register(zframe_t* hostaddr, char* params)
{
    char* end;

    unsigned int subnet = strtol(params, &end, 10);
    assert(!*end);
    assert(subnet <= OSD_DIADDR_SUBNET_MAX);

    if (gateways[subnet] != NULL) {
        err("A gateway for subnet %u is already registered.\n", subnet);
        return mgmt_send_nack(hostaddr);
    }

    gateways[subnet] = zframe_dup(hostaddr);
    dbg("Registered gateway %s for subnet %u\n", zframe_strhex(hostaddr), subnet);
    mgmt_send_ack(hostaddr);
}

static void process_mgmt_msg(const zframe_t* src, const zframe_t* payload_frame)
{
    char* request = zframe_strdup(payload_frame);
    dbg("Received management message %s\n", request);

    if (strcmp(request, "DIADDR_REQUEST") == 0) {
        mgmt_diaddr_request(src);
    } else if (strcmp(request, "DIADDR_RELEASE") == 0) {
        mgmt_diaddr_release(src);
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

    // create packet out of incoming data
    /*struct osd_packet *pkg;
    rv = osd_packet_new(&pkg, zframe_size(payload_frame) / sizeof(uint16_t));
    assert(rv == OSD_OK);

    memcpy(pkg->data_raw, zframe_data(payload_frame), zframe_size(payload_frame));*/

    uint16_t* osd_packet_data = (uint16_t*)zframe_data(payload_frame);

    uint16_t dest_diaddr = osd_packet_data[0];

    //unsigned int dest_diaddr = osd_packet_get_dest(pkg);
    unsigned int dest_diaddr_subnet = osd_diaddr_subnet(dest_diaddr);
    unsigned int dest_diaddr_local = osd_diaddr_localaddr(dest_diaddr);

    dbg("Routing lookup for packet with destination %u.%u (%u). Local subnet is %u\n",
        dest_diaddr_subnet, dest_diaddr_local, dest_diaddr, subnet_addr);

    const zframe_t* dest_hostaddr;
    if (dest_diaddr_subnet == subnet_addr) {
        // routing inside our subnet
        dest_hostaddr = mods_in_subnet[dest_diaddr_local];
        if (dest_hostaddr == NULL) {
            err("No destination module registered for di address %u.%u\n",
                dest_diaddr_subnet, dest_diaddr_local);
            return;
        }
        dbg("Destination address is local, routing directly to destination.\n");
    } else {
        // routing through a gateway
        dest_hostaddr = gateways[dest_diaddr_subnet];
        if (dest_hostaddr == NULL) {
            err("No gateway for subnet %u registered to route di address %u.%u\n",
                dest_diaddr_subnet, dest_diaddr_subnet, dest_diaddr_local);
            return;
        }
        dbg("Destination address is in a different subnet, routing through gateway.\n");
    }

    dbg("Routing data packet to %s\n", zframe_strhex(dest_hostaddr));

    zmsg_t *msg = zmsg_new();
    assert(msg);
    zmsg_add(msg, zframe_dup(dest_hostaddr));
    zmsg_addstr(msg, "D");
    zmsg_add(msg, zframe_dup(payload_frame));
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

    // Our subnet: always 1 for now.
    // XXX: make this dynamic
    subnet_addr = 1;

    // allocate routing lookup tables
    // mods_in_subnet is 1024 * 8B = 8 kB
    mods_in_subnet = calloc(OSD_DIADDR_LOCAL_MAX + 1, sizeof(zframe_t*));
    // gateways is 64 * 8B = 1 kB
    gateways = calloc(OSD_DIADDR_SUBNET_MAX + 1, sizeof(zframe_t*));

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

    free(mods_in_subnet);
    free(gateways);
    zsock_destroy(&server_socket);
    return 0;
}
