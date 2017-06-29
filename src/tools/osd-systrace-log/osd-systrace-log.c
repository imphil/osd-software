/**
 * Open SoC Debug tool: osd-systrace-log
 */

#include <osd/osd.h>
#include <osd/com.h>

#include "../cli-util.h"

#include <stdlib.h>
#include <argp.h>
#include <stdio.h>
#include <unistd.h>

const char *argp_program_bug_address =
  "https://github.com/opensocdebug/software/issues";

/* Program documentation. */
static char doc[] =
  "Open SoC Debug System Trace Logger";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/**
 * Command-line options for this program
 */
static struct argp_option options[] = {
    CLI_COMMON_OPTIONS,
    { 0 }
};

/**
 * Definition of arguments available to this daemon
 */
struct arguments {
    CLI_COMMON_ARGUMENTS
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
 * Set the default values of |arguments|
 *
 * The defaults set in this function can be overwritten later, e.g. by command
 * line parameters.
 */
static void arguments_set_default(void)
{
    arguments.verbose = 0;
    arguments.color_output = isatty(STDOUT_FILENO) || isatty(STDERR_FILENO);
}

int main(int argc, char** argv)
{
    // read program arguments
    arguments_set_default();
    if (argp_parse(&argp, argc, argv, 0, 0, &arguments) != 0) {
        fatal("Unable to parse program arguments.\n");
    }


    return 0;
}
