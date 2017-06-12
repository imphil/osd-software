/**
 * Open SoC Debug Communication Daemon
 */

#include <osd/osd.h>
#include <osd/com.h>

#include <libglip.h>
#include <stdlib.h>
#include <argp.h>
#include <stdio.h>
#include <unistd.h>
#include <byteswap.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
 * Default GLIP backend to be used when connecting to a device
 */
#define GLIP_DEFAULT_BACKEND "uart"


static void cli_log(int priority, const char* category,
                    const char *format, ...) __attribute__ ((format (printf, 3, 4)));

#define dbg(arg...) cli_log(LOG_DEBUG, "osd-daemon", ## arg)
#define info(arg...) cli_log(LOG_INFO, "osd-daemon", ## arg)
#define err(arg...) cli_log(LOG_ERR, "osd-daemon", ## arg)
#define fatal(arg...) { cli_log(LOG_CRIT, "osd-daemon", ## arg); exit(1); }

const char *argp_program_bug_address =
  "https://github.com/opensocdebug/software/issues";

/* Program documentation. */
static char doc[] =
  "Open SoC Debug Daemon -- communicate with an Open SoC Debug enabled device";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/**
 * Command-line options for this program
 */
static struct argp_option options[] = {
    {"verbose",  'v', 0,      0,  "Produce verbose output" },
    {"glip-backend",  'b', "BACKEND", 0,
     "Use GLIP backend BACKEND to connect to the device (default: " GLIP_DEFAULT_BACKEND ")" },
    {"glip-backend-options",  'o', "OPTIONS", 0,
     "Comma-separated list of options to pass to the GLIP backend" },
    { 0 }
};

/**
 * Definition of arguments available to this daemon
 */
struct arguments {
    int verbose; //!< verbose output
    int color_output; //!< use colors in the output
    char* glip_backend; //!< name of the GLIP backend to be used
    struct glip_option* glip_backend_options; //!< list of options passed to the GLIP backend
    size_t glip_backend_options_len; //!< number of entries in glip_backend_options
};

/**
 * Argp callback: parse a single option
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    int rv;

    /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
    struct arguments *arguments = state->input;

    switch (key) {
    case 'v':
        arguments->verbose = 1;
        break;
    case 'b':
        arguments->glip_backend = arg;
        break;
    case 'o':
        rv = glip_parse_option_string(arg, &arguments->glip_backend_options,
                                      &arguments->glip_backend_options_len);
        if (rv != 0) {
            info("Unable to parse GLIP backend options.\n");
            return ARGP_ERR_UNKNOWN;
        }
        break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

/**
 * Arguments passed to this daemon
 */
struct arguments arguments;

/**
 * GLIP library context
 */
struct glip_ctx *glip_ctx;

/**
 * Open SoC Debug communication library context
 */
struct osd_com_ctx *osd_com_ctx;

/**
 * Format a log message and print it out to the user
 *
 * All log messages end up in this function.
 */
static void cli_vlog(int priority, const char* category,
                     const char *format, va_list args)
{
    int max_priority;
    if (arguments.verbose) {
        max_priority = LOG_DEBUG;
    } else {
        max_priority = LOG_ERR;
    }

    if (priority > max_priority) {
        return;
    }

    switch (priority) {
    case LOG_DEBUG:
        fprintf(stderr, "[DEBUG] ");
        break;
    case LOG_INFO:
        if (arguments.color_output) fprintf(stderr, ANSI_COLOR_YELLOW);
        fprintf(stderr, "[INFO]  ");
        break;
    case LOG_ERR:
        if (arguments.color_output) fprintf(stderr, ANSI_COLOR_RED);
        fprintf(stderr, "[ERROR] ");
        break;
    case LOG_CRIT:
        if (arguments.color_output) fprintf(stderr, ANSI_COLOR_RED);
        fprintf(stderr, "[FATAL] ");
        break;
    }

    fprintf(stderr, "%s: ", category);
    vfprintf(stderr, format, args);

    if (arguments.color_output) fprintf(stderr, ANSI_COLOR_RESET);
}

/**
 * Log a message from this daemon (printf-style)
 *
 * To log a message, typically use the dbg(), info(), err() and fatal() macros
 * instead of this function.
 *
 * @param priority severity level (see the LOG_* constants from syslog.h)
 * @param category category string
 * @param format printf-style format string
 * @param ... arguments for the format string
 */
static void cli_log(int priority, const char* category,
                    const char *format, ...)
{
    va_list args;

    va_start(args, format);
    cli_vlog(priority, category, format, args);
    va_end(args);
}

/**
 * Log handler for OSD
 */
static void osd_log_handler(struct osd_log_ctx *ctx, int priority,
                            const char *file, int line, const char *fn,
                            const char *format, va_list args)
{
    cli_vlog(priority, "libosd", format, args);
}

/**
 * Log handler for GLIP
 */
static void glip_log_handler(struct glip_ctx *ctx, int priority,
                             const char *file, int line, const char *fn,
                             const char *format, va_list args)
{
    cli_vlog(priority, "libglip", format, args);
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

    if (flags & OSD_COM_NONBLOCK) {
        rv = glip_write(glip_ctx, 0, size_words * sizeof(uint16_t), (uint8_t*)buf_be,
                        &bytes_written);
    } else {
        rv = glip_write_b(glip_ctx, 0, size_words * sizeof(uint16_t), (uint8_t*)buf_be,
                          &bytes_written, 0 /* timeout [ms]; 0 == never */);
    }
    if (rv != 0) {
        return -1;
    }

    size_t words_written = bytes_written / sizeof(uint16_t);
    return words_written;
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

    if (flags & OSD_COM_NONBLOCK) {
        rv = glip_read(glip_ctx, 0,
                       size_words * sizeof(uint16_t), (uint8_t*)buf_be,
                       &bytes_read);
    } else {
        rv = glip_read_b(glip_ctx, 0,
                         size_words * sizeof(uint16_t), (uint8_t*)buf_be,
                         &bytes_read, 0 /* timeout [ms]; 0 == never */);
    }
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

static struct osd_com_device_if device_communication_functions = {
    .write = &device_write,
    .read = &device_read
};

/**
 * Initialize OSD Communication Library
 */
static void init_osd_com(void)
{
    osd_result osd_rv;

    // prepare OSD logging system
    struct osd_log_ctx *osd_log_ctx;
    int log_priority;
    if (arguments.verbose) {
        log_priority = LOG_DEBUG;
    } else {
        log_priority = LOG_ERR;
    }
    osd_rv = osd_log_new(&osd_log_ctx, log_priority, &osd_log_handler);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to create osd_log_ctx (rv=%d).\n", osd_rv);
    }

    // create osd_com_ctx
    osd_rv = osd_com_new(&osd_com_ctx, osd_log_ctx);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to create osd_com_ctx (rv=%d).\n", osd_rv);
    }

    // connect the device read/write functions to the OSD communication API
    // for now we use the same device connection for control and event data
    osd_com_set_device_ctrl_if(osd_com_ctx, &device_communication_functions);
    osd_com_set_device_event_if(osd_com_ctx, &device_communication_functions);
}

/**
 * Initialize GLIP for device communication
 */
static void init_glip(void)
{
    int rv;

    dbg("Creating GLIP device context for backend %s\n",
        arguments.glip_backend);
    rv = glip_new(&glip_ctx, arguments.glip_backend,
                  arguments.glip_backend_options,
                  arguments.glip_backend_options_len,
                  &glip_log_handler);
    if (rv < 0) {
        fatal("Unable to create new GLIP context (rv=%d).\n", rv);
    }

    if (glip_get_fifo_width(glip_ctx) != 2) {
        fatal("FIFO width of GLIP channel must be 16 bit, not %d bit.\n",
              glip_get_fifo_width(glip_ctx) * 8);
    }

    // route log messages to our log handler
    if (arguments.verbose) {
        glip_set_log_priority(glip_ctx, LOG_DEBUG);
    }
}

/**
 * Set the default values of |arguments|
 *
 * The defaults set in this function can be overwritten later, e.g. by command
 * line parameters.
 */
static void arguments_set_default(void)
{
    arguments.verbose = 0;
    arguments.color_output = isatty(STDOUT_FILENO) || isatty(STDERR_FILENO);
    arguments.glip_backend = GLIP_DEFAULT_BACKEND;
    arguments.glip_backend_options = NULL;
    arguments.glip_backend_options_len = 0;
}

/**
 * Print the program version to a stream
 *
 * This function is registered as hook with argp to be invoked when calling the
 * application with --version.
 */
static void print_version(FILE *stream, struct argp_state *state)
{
    const struct osd_version *libosd_version = osd_version_get();
    const struct glip_version *libglip_version = glip_get_version();

    fprintf(stream, "osd-daemon %d.%d.%d%s (using libosd %d.%d.%d%s, "
            "libglip %d.%d.%d%s)\n",
            OSD_VERSION_MAJOR, OSD_VERSION_MINOR, OSD_VERSION_MICRO,
            OSD_VERSION_SUFFIX,
            libosd_version->major, libosd_version->minor, libosd_version->micro,
            libosd_version->suffix,
            libglip_version->major, libglip_version->minor,
            libglip_version->micro, libglip_version->suffix);
}

// register print_version with argp
void (*argp_program_version_hook) (FILE *, struct argp_state *) = print_version;


int main(int argc, char** argv)
{
    // read program arguments
    arguments_set_default();
    if (argp_parse(&argp, argc, argv, 0, 0, &arguments) != 0) {
        fatal("Unable to parse program arguments.\n");
    }

    // prepare GLIP for device communication
    init_glip();

    // initialize OSD communication library
    init_osd_com();

    // connect to device
    dbg("Attempting physical connection to device.\n");
    int rv = glip_open(glip_ctx, 1);
    if (rv < 0) {
        fatal("Unable to open connection to device.\n");
    }
    dbg("Physical connection established.\n");

    struct osd_module_desc *modules;
    size_t modules_len;
    osd_result osd_rv;

    dbg("Connecting to debug system\n");
    osd_rv = osd_com_connect(osd_com_ctx);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to connect to debug system\n");
    }
    dbg("Connection to debug system established.\n");

    dbg("Enumerating modules in debug system\n");
    osd_rv = osd_com_get_modules(osd_com_ctx, &modules, &modules_len);
    if (OSD_FAILED(osd_rv)) {
        fatal("Unable to get a list of debug modules from the device\n")
    }
    dbg("Found %zu debug modules\n", modules_len);

    for (size_t i = 0; i < modules_len; i++) {
        dbg("[0x%04x] %u.%u (v%u)\n", modules[i].addr, modules[i].vendor,
            modules[i].type, modules[i].version);
    }


    return 0;
}
