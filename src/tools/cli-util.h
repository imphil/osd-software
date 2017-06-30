#include <osd/osd.h>

#include <stdio.h>
#include <unistd.h>

#include "argtable3.h"
#include "ini.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static const char* DEFAULT_CONFIG_FILE = "/etc/osd/osd.conf";

void cli_log(int priority, const char* category,
             const char *format, ...) __attribute__ ((format (printf, 3, 4)));

#define dbg(arg...) cli_log(LOG_DEBUG, CLI_TOOL_PROGNAME, ## arg)
#define info(arg...) cli_log(LOG_INFO, CLI_TOOL_PROGNAME, ## arg)
#define err(arg...) cli_log(LOG_ERR, CLI_TOOL_PROGNAME, ## arg)
#define fatal(arg...) { cli_log(LOG_CRIT, CLI_TOOL_PROGNAME, ## arg); exit(1); }

int color_output = 0;
int verbose = 0;

/**
 * Format a log message and print it out to the user
 *
 * All log messages end up in this function.
 */
static void cli_vlog(int priority, const char* category,
                     const char *format, va_list args)
{
    int max_priority;
    if (verbose) {
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
        if (color_output) fprintf(stderr, ANSI_COLOR_YELLOW);
        fprintf(stderr, "[INFO]  ");
        break;
    case LOG_ERR:
        if (color_output) fprintf(stderr, ANSI_COLOR_RED);
        fprintf(stderr, "[ERROR] ");
        break;
    case LOG_CRIT:
        if (color_output) fprintf(stderr, ANSI_COLOR_RED);
        fprintf(stderr, "[FATAL] ");
        break;
    }

    fprintf(stderr, "%s: ", category);
    vfprintf(stderr, format, args);

    if (color_output) fprintf(stderr, ANSI_COLOR_RESET);
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

static void print_version()
{
    const struct osd_version *libosd_version = osd_version_get();

    printf("%s %d.%d.%d%s (using libosd %d.%d.%d%s)\n",
           CLI_TOOL_PROGNAME,
           OSD_VERSION_MAJOR, OSD_VERSION_MINOR, OSD_VERSION_MICRO,
           OSD_VERSION_SUFFIX,
           libosd_version->major, libosd_version->minor, libosd_version->micro,
           libosd_version->suffix);
}

// command line arguments
struct arg_lit *a_verbose, *a_help, *a_version;
struct arg_file *a_config_file;
struct arg_end *a_end;

int osd_main();

int main(int argc, char** argv)
{
    color_output = isatty(STDOUT_FILENO) || isatty(STDERR_FILENO);

    // command line argument parsing
    void *argtable[] = {
        a_help    = arg_litn(NULL, "help", 0, 1,
                           "display this help and exit"),
        a_version = arg_litn(NULL, "version", 0, 1,
                           "display version info and exit"),
        a_verbose = arg_litn("v", "verbose", 0, 1, "verbose output"),
        a_config_file = arg_filen("c", "config-file", "<file>", 0, 1,
                               "non-standard configuration file location. "),
        a_end     = arg_end(20),
    };

    int exitcode = 0;

    a_config_file->filename = DEFAULT_CONFIG_FILE;

    int nerrors;
    nerrors = arg_parse(argc,argv,argtable);

    if (a_help->count > 0) {
        printf("Usage: %s", CLI_TOOL_PROGNAME);
        arg_print_syntax(stdout, argtable, "\n");
        printf(CLI_TOOL_SHORTDESC "\n\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = 0;
        goto exit;
    }

    if (a_version->count > 0) {
        print_version();
        exitcode = 0;
        goto exit;
    }

    if (nerrors > 0) {
        /* Display the error details contained in the arg_end struct.*/
        arg_print_errors(stdout, a_end, CLI_TOOL_PROGNAME);
        printf("Try '%s --help' for more information.\n", CLI_TOOL_PROGNAME);
        exitcode = 1;
        goto exit;
    }

    if (a_verbose->count) {
        verbose = 1;
    }

    exitcode = osd_main();

exit:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
