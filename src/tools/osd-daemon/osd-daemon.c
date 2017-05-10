/**
 * Open SoC Debug Communication Daemon
 */

#include <osd.h>
#include <osd-com.h>
#include <libglip.h>
#include <stdlib.h>
#include <argp.h>
#include <stdlib.h>

const char *argp_program_version =
  "osd-daemon 1.0";
const char *argp_program_bug_address =
  "<opensocdebug@lists.librecores.org>";

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
     "Use GLIP backend BACKEND to connect to the device" },
    {"glip-backend-options",  'o', "OPTIONS", 0,
     "Comma-separated list of options to pass to the GLIP backend" },
    { 0 }
};

/**
 * Definition of arguments available to this daemon
 */
struct arguments {
    int verbose; //!< verbose output
    char* glip_backend; //!< name of the GLIP backend to be used
    struct glip_option* glip_backend_options; //!< list of options passed to the GLIP backend
    size_t glip_backend_options_len; //!< number of entries in glip_backend_options
};

/**
 * Argp callback: parse a single option
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
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
        glip_parse_option_string(arg, &arguments->glip_backend_options,
                                 &arguments->glip_backend_options_len);
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
 * Initialize OSD Communication Library
 */
static int init_osd_com(void)
{
    osd_result osd_rv;
    osd_rv = osd_com_new(&osd_com_ctx);
    if (OSD_FAILED(osd_rv)) {
        printf("Unable to connect\n");
        return -1;
    }

    return 0;
}

/**
 * Initialize GLIP for device communication
 */
static int init_glip(void)
{
    int rv;

    rv = glip_new(&glip_ctx, arguments.glip_backend,
                  arguments.glip_backend_options,
                  arguments.glip_backend_options_len);
    if (rv < 0) {
        return rv;
    }

    return 0;
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
    arguments.glip_backend = "uart";
    arguments.glip_backend_options = NULL;
    arguments.glip_backend_options_len = 0;
}

int main(int argc, char** argv)
{
    // read program arguments
    arguments_set_default();
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // Initialize GLIP for device communication
    init_glip();

    // Initialize OSD communication library
    init_osd_com();

    return 0;
}
