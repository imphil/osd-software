#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define CLI_COMMON_OPTIONS \
    {"verbose",  'v', 0,      0,  "Produce verbose output" }

#define CLI_COMMON_ARGUMENTS \
    int verbose; \
    int color_output;


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


void cli_log(int priority, const char* category,
             const char *format, ...) __attribute__ ((format (printf, 3, 4)));

#define dbg(arg...) cli_log(LOG_DEBUG, "osd-daemon", ## arg)
#define info(arg...) cli_log(LOG_INFO, "osd-daemon", ## arg)
#define err(arg...) cli_log(LOG_ERR, "osd-daemon", ## arg)
#define fatal(arg...) { cli_log(LOG_CRIT, "osd-daemon", ## arg); exit(1); }

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
void cli_log(int priority, const char* category, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    cli_vlog(priority, category, format, args);
    va_end(args);
}

/**
 * Log handler for OSD
 */
void osd_log_handler(struct osd_log_ctx *ctx, int priority, const char *file,
                     int line, const char *fn, const char *format, va_list args)
{
    cli_vlog(priority, "libosd", format, args);
}
