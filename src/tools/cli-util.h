/* Copyright (c) 2017 by the author(s)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ============================================================================
 *
 * Author(s):
 *   Philipp Wagner <philipp.wagner@tum.de>
 */

#include <osd/osd.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "argtable3.h"
#include "iniparser.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static const char* DEFAULT_CONFIG_FILE = "/etc/osd/osd.conf";

/**
 * Default ZeroMQ endpoint of the host controller
 */
#define DEFAULT_HOSTCTRL_EP "tcp://localhost:9537"

void cli_log(int priority, const char* category,
             const char *format, ...) __attribute__ ((format (printf, 3, 4)));

#define dbg(arg...) cli_log(LOG_DEBUG, CLI_TOOL_PROGNAME, ## arg)
#define info(arg...) cli_log(LOG_INFO, CLI_TOOL_PROGNAME, ## arg)
#define err(arg...) cli_log(LOG_ERR, CLI_TOOL_PROGNAME, ## arg)
#define fatal(arg...) cli_log(LOG_CRIT, CLI_TOOL_PROGNAME, ## arg);

struct config {
    int log_level;
    int color_output;
};

struct config cfg = {
    .log_level = LOG_ERR,
    .color_output = 0
};

// command line arguments common across all tools
struct arg_lit *a_verbose, *a_help, *a_version;
struct arg_file *a_config_file;
struct arg_end *a_end;
void** argtable = NULL;
int argtable_len = 0;


/**
 * Format a log message and print it out to the user
 *
 * All log messages end up in this function.
 */
static void cli_vlog(int priority, const char* category,
                     const char *format, va_list args)
{
    if (priority > cfg.log_level) {
        return;
    }

    switch (priority) {
    case LOG_DEBUG:
        fprintf(stderr, "[DEBUG] ");
        break;
    case LOG_INFO:
        if (cfg.color_output) fprintf(stderr, ANSI_COLOR_YELLOW);
        fprintf(stderr, "[INFO]  ");
        break;
    case LOG_ERR:
        if (cfg.color_output) fprintf(stderr, ANSI_COLOR_RED);
        fprintf(stderr, "[ERROR] ");
        break;
    case LOG_CRIT:
        if (cfg.color_output) fprintf(stderr, ANSI_COLOR_RED);
        fprintf(stderr, "[FATAL] ");
        break;
    }

    fprintf(stderr, "%s: ", category);
    vfprintf(stderr, format, args);

    if (cfg.color_output) fprintf(stderr, ANSI_COLOR_RESET);
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

static void cfg_set_runtime_defaults(void)
{
    cfg.color_output = isatty(STDOUT_FILENO) || isatty(STDERR_FILENO);
}

static void cfg_update_with_cli_args(void)
{
    cfg.log_level += a_verbose->count;
    if (cfg.log_level > LOG_DEBUG) {
        cfg.log_level = LOG_DEBUG;
    }
}

static int cfg_parser_error_cb(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    cli_vlog(LOG_INFO, CLI_TOOL_PROGNAME, format, argptr);
    va_end(argptr);
    return 0;
}

static osd_result cfg_parse_config_file(const char* filename)
{
    dbg("Reading configuration from file %s\n", filename);

    dictionary* ini;
    ini = iniparser_load(filename);
    if (ini == NULL) {
        info("Unable to parse configuration file %s\n", filename);
        return OSD_ERROR_FAILURE;
    }

    // general.log_level
    const char* str = iniparser_getstring(ini, "general:log_level", NULL);
    if (str != NULL) {
        if (strcasecmp(str, "emerg") == 0) {
            cfg.log_level = LOG_EMERG;
        } else if (strcasecmp(str, "alert") == 0) {
            cfg.log_level = LOG_ALERT;
        } else if (strcasecmp(str, "crit") == 0) {
            cfg.log_level = LOG_CRIT;
        } else if (strcasecmp(str, "err") == 0) {
            cfg.log_level = LOG_CRIT;
        } else if (strcasecmp(str, "warning") == 0) {
            cfg.log_level = LOG_WARNING;
        } else if (strcasecmp(str, "notice") == 0) {
            cfg.log_level = LOG_NOTICE;
        } else if (strcasecmp(str, "info") == 0) {
            cfg.log_level = LOG_INFO;
        } else if (strcasecmp(str, "debug") == 0) {
            cfg.log_level = LOG_DEBUG;
        } else {
            err("Invalid value '%s' for configuration key general.log_level\n",
                str);
        }
    }

    // general.color_output
    cfg.color_output = iniparser_getboolean(ini, "general:color_output",
                                            cfg.color_output);

    iniparser_freedict(ini);

    return OSD_OK;
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

static void print_version(void)
{
    const struct osd_version *libosd_version = osd_version_get();

    printf("%s %d.%d.%d%s (using libosd %d.%d.%d%s)\n",
           CLI_TOOL_PROGNAME,
           OSD_VERSION_MAJOR, OSD_VERSION_MINOR, OSD_VERSION_MICRO,
           OSD_VERSION_SUFFIX,
           libosd_version->major, libosd_version->minor, libosd_version->micro,
           libosd_version->suffix);
}

osd_result osd_tool_add_arg(void* arg);

/**
 * Add command line argument to the tool
 *
 * @param arg a argtable3 argument
 * @return OSD_OK on success, any other value indicates an error
 */
osd_result osd_tool_add_arg(void* arg)
{
    if (argtable == NULL) {
        argtable_len = 1;
        argtable = malloc(sizeof(void*));
        assert(argtable);
    } else {
        argtable_len++;
        argtable = realloc(argtable, (argtable_len) * sizeof(void*));
        assert(argtable);
    }
    argtable[argtable_len - 1] = arg;
    return OSD_OK;
}

osd_result setup(void);
int run(void);

int main(int argc, char** argv)
{
    int exitcode = 0;

    iniparser_set_error_callback(cfg_parser_error_cb);

    cfg_set_runtime_defaults();

    // command line argument parsing
    a_help = arg_litn(NULL, "help", 0, 1,
                      "display this help and exit");
    osd_tool_add_arg(a_help);
    a_version = arg_litn(NULL, "version", 0, 1,
                         "display version info and exit");
    osd_tool_add_arg(a_version);
    a_config_file = arg_filen("c", "config-file", "<file>", 0, 1,
                              "non-standard configuration file location. ");
    osd_tool_add_arg(a_config_file);
    a_verbose = arg_litn("v", NULL, 0, 10,
                         "increase verbosity (can be used multiple times)");
    osd_tool_add_arg(a_verbose);

    // setup tool and allow it to add its own arguments to the argtable
    setup();

    a_end = arg_end(20 /* max. number of errors */);
    osd_tool_add_arg(a_end);

    // parse arguments
    a_config_file->filename[0] = DEFAULT_CONFIG_FILE;

    int nerrors;
    nerrors = arg_parse(argc, argv, argtable);

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
        arg_print_errors(stdout, a_end, CLI_TOOL_PROGNAME);
        printf("Try '%s --help' for more information.\n", CLI_TOOL_PROGNAME);
        exitcode = 1;
        goto exit;
    }

    /*
     * Update built-in default values already with command line values
     * This allows users to increase the log level on the command line to see
     * errors when parsing the configuration file.
     */
    cfg_update_with_cli_args();

    cfg_parse_config_file(a_config_file->filename[0]);

    /*
     * Override/modify values from the configuration file with command line
     * arguments.
     */
    cfg_update_with_cli_args();

    exitcode = run();

exit:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
