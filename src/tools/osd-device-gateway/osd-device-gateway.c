/**
 * Open SoC Debug Device Gateway
 */

#define CLI_TOOL_PROGNAME "osd-device-gateway"
#define CLI_TOOL_SHORTDESC "Open SoC Debug device gateway"

#include "../cli-util.h"
#include <czmq.h>
#include <byteswap.h>
#include "../../libosd/include/osd/hostmod.h"
#include <libglip.h>

/**
 * Default GLIP backend to be used when connecting to a device
 */
#define GLIP_DEFAULT_BACKEND "tcp"

/**
 * GLIP library context
 */
struct glip_ctx *glip_ctx;

/**
 * Open SoC Debug communication library context
 */
struct osd_com_ctx *osd_com_ctx;


zsock_t *host_com_sock;
pthread_mutex_t host_com_sock_lock = PTHREAD_MUTEX_INITIALIZER;



// command line arguments
struct arg_str *a_glip_backend;
struct arg_str *a_glip_backend_options;


/**
 * Log handler for GLIP
 */
static void glip_log_handler(struct glip_ctx *ctx, int priority,
                             const char *file, int line, const char *fn,
                             const char *format, va_list args)
{
    cli_vlog(priority, "libglip", format, args);
}

static ssize_t device_read(uint16_t *buf, size_t size_words, int flags)
{
    int rv;
    size_t words_read;
    size_t bytes_read;

    uint16_t *buf_be;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    buf_be = malloc(size_words * sizeof(uint16_t));
    if (!buf_be) {
        return -1;
    }
#else
    buf_be = buf;
#endif

    rv = glip_read_b(glip_ctx, 0,
                     size_words * sizeof(uint16_t), (uint8_t*)buf_be,
                     &bytes_read, 0 /* timeout [ms]; 0 == never */);
    if (rv != 0) {
        return -1;
    }
    words_read = bytes_read / sizeof(uint16_t);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (size_t w = 0; w < words_read; w++) {
        buf[w] = bswap_16(buf_be[w]);
    }
#endif

    return words_read;
}

static ssize_t device_write(uint16_t *buf, size_t size_words, int flags)
{
    size_t bytes_written;
    int rv;

    // GLIP and OSD are big endian, |buf| is in native endianness
    uint16_t *buf_be;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    buf_be = malloc(size_words * sizeof(uint16_t));
    if (!buf_be) {
        return -1;
    }

    for (size_t w = 0; w < size_words; w++) {
        buf_be[w] = bswap_16(buf[w]);
    }
#else
    buf_be = buf;
#endif

    rv = glip_write_b(glip_ctx, 0, size_words * sizeof(uint16_t), (uint8_t*)buf_be,
                      &bytes_written, 0 /* timeout [ms]; 0 == never */);
    if (rv != 0) {
        return -1;
    }

    size_t words_written = bytes_written / sizeof(uint16_t);
    return words_written;
}

/**
 * Initialize GLIP for device communication
 */
static void init_glip(void)
{
    int rv;

    struct glip_option* glip_backend_options;
    size_t glip_backend_options_len;
    rv = glip_parse_option_string(a_glip_backend_options->sval[0],
                                  &glip_backend_options,
                                  &glip_backend_options_len);
    if (rv != 0) {
        fatal("Unable to parse GLIP backend options.\n");
    }

    dbg("Creating GLIP device context for backend %s\n",
        a_glip_backend->sval[0]);
    rv = glip_new(&glip_ctx, a_glip_backend->sval[0],
                  glip_backend_options,
                  glip_backend_options_len,
                  &glip_log_handler);
    if (rv < 0) {
        fatal("Unable to create new GLIP context (rv=%d).\n", rv);
    }

    if (glip_get_fifo_width(glip_ctx) != 2) {
        fatal("FIFO width of GLIP channel must be 16 bit, not %d bit.\n",
              glip_get_fifo_width(glip_ctx) * 8);
    }

    // route log messages to our log handler
    glip_set_log_priority(glip_ctx, cfg.log_level);
}

/**
 * Send a packet received on the host to the device
 *
 * This function is registered as zloop reactor.
 */
static int send_to_device(zloop_t *loop, zsock_t *reader, void *arg)
{
    zmsg_t *msg = zmsg_recv(reader);
    if (!msg) {
        return -1; // process interrupted
    }

    zframe_t *type_frame = zmsg_pop(msg);
    if (zframe_streq(type_frame, "D")) {
        dbg("Forwarding data message to device\n");

        zframe_t *data_frame = zmsg_pop(msg);
        assert(data_frame);
        uint16_t *data = (uint16_t*)zframe_data(data_frame);
        size_t data_size_words = zframe_size(data_frame) / sizeof(uint16_t);
        assert(data);

        size_t size_words_written;

        // data is sent as Debug Transport Datagram (DTD) -- a length-value
        // encoded version of the osd packet.

        // length (in uint16_t words)
        uint16_t dtd_size = data_size_words;
        size_words_written = device_write(&dtd_size, 1, 0);
        assert(size_words_written == 1);

        // value (the packet data)
        size_words_written = device_write(data, data_size_words, 0);
        assert(size_words_written == data_size_words);

    } else if (zframe_streq(type_frame, "M")) {
        // TODO: handle incoming management messages
        err("XXX: management messages are not yet handled by this client.\n");
        goto ret;

    } else {
        err("Message of unknown type received. Ignoring.\n");
        goto ret;
    }

ret:
    zmsg_destroy(&msg);
    return 0;
}

/**
 * Register this tool as gateway for a given subnet
 */
static osd_result osd_hostcom_register_subnet_gw(unsigned int subnet)
{
    zmsg_t *msg;

    msg = zmsg_new();
    zmsg_addstrf(msg, "M");
    zmsg_addstrf(msg, "GW_REGISTER %u", subnet);
    zmsg_send(&msg, host_com_sock);
    zmsg_destroy(&msg);

    // process reply
    msg = zmsg_recv(host_com_sock);
    if (!msg && errno == EINTR) {
        // user pressed CTRL-C (SIGHUP)
        return OSD_ERROR_ABORTED;
    }
    printf("Received message: \n");
    zmsg_print(msg);

    char* msg_type = zmsg_popstr(msg);
    if (strcmp(msg_type, "M") != 0) {
        err("Received invalid response of type %s\n", msg_type);
    }
    char* response = zmsg_popstr(msg);
    if (strcmp(response, "ACK") != 0) {
        err("Received %s when expecting 'ACK'.\n", response);
    }

    dbg("Registered as gateway for subnet %d with host controller\n", subnet);

    return OSD_OK;
}

/**
 * Read data from the device encoded as Debug Transport Datagrams (DTDs)
 */
static void* thread_ctrl_receive(void *unused)
{
    ssize_t rv;

    while (1) {
        // read packet size, which is transmitted as first word in a DTD
        uint16_t pkg_size_words;
        rv = device_read(&pkg_size_words, 1, 0);
        if (rv != 1) {
            err("Unable to receive data from device. Aborting.\n");
            return NULL;
        }

        // read packet data
        uint16_t* pkg_data = malloc(sizeof(uint16_t) * pkg_size_words);
        rv = device_read(pkg_data, pkg_size_words, 0);
        assert(rv == pkg_size_words);

        // forward received packet to host controller
        zmsg_t *msg;
        msg = zmsg_new();
        zmsg_addstrf(msg, "D");
        zmsg_addmem(msg, pkg_data, pkg_size_words * sizeof(uint16_t));

        pthread_mutex_lock(&host_com_sock_lock);
        zmsg_send(&msg, host_com_sock);
        pthread_mutex_unlock(&host_com_sock_lock);

        dbg("Received packet from device\n");
        free(pkg_data);
    }
}

osd_result setup(void)
{
    a_glip_backend = arg_str0(NULL, "glip-backend", "<name>",
                              "GLIP backend name");
    a_glip_backend->sval[0] = GLIP_DEFAULT_BACKEND;
    osd_tool_add_arg(a_glip_backend);

    a_glip_backend_options = arg_str0(NULL, "glip-backend-options",
                                      "<option1=value1,option2=value2,...>",
                                      "GLIP backend options");
    osd_tool_add_arg(a_glip_backend_options);

    return 0;
}

int run(void)
{
    // prepare GLIP for device communication
    init_glip();

    // connect to device
    dbg("Attempting physical connection to device.\n");
    int rv = glip_open(glip_ctx, 1);
    if (rv < 0) {
        fatal("Unable to open connection to device.\n");
    }
    dbg("Physical connection established.\n");


    // initialize communication with host controller
    zsys_init();
    host_com_sock = zsock_new_dealer("tcp://127.0.0.1:9990");


    // register this tool as gateway for subnet 0
    // XXX: make subnet dynamic
    osd_hostcom_register_subnet_gw(0);

    // connect data path between host controller and device
    // device -> host
    pthread_t rcv_thread;
    rv = pthread_create(&rcv_thread, 0,
                        thread_ctrl_receive, NULL);
    if (rv) {
        err("Unable to create thread_ctrl_receive: %d\n", rv);
        return OSD_ERROR_FAILURE;
    }

    // host -> device
    zloop_t* dev_tx_loop = zloop_new();
    //zloop_set_verbose(dev_tx_loop, 1);
    int rc = zloop_reader(dev_tx_loop, host_com_sock, send_to_device, NULL);
    assert(rc == 0);
    zloop_reader_set_tolerant(dev_tx_loop, host_com_sock);
    zloop_start(dev_tx_loop);

    // clean up host -> device path
    zloop_destroy(&dev_tx_loop);

    // clean up device -> host path
    pthread_cancel(rcv_thread);

    // all remaining cleanups
    zsock_destroy(&host_com_sock);

    return 0;
}
