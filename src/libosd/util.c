#include <osd/osd.h>
#include "osd-private.h"

static const struct osd_version osd_version_internal = {OSD_VERSION_MAJOR,
    OSD_VERSION_MINOR, OSD_VERSION_MICRO, OSD_VERSION_SUFFIX};

/**
 * Get the version of the library
 *
 * @return the library version information
 */
API_EXPORT
const struct osd_version * osd_version_get(void)
{
    return &osd_version_internal;
}
