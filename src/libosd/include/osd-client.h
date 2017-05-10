


//////////////////////////

/*
 * XXX: how to encapsulate these high-level API things?
 * make it a header-only library, one for each module?
 * make it one large library with everything in it, and people can use what they
 * need and let the compiler remove unnecessary stuff if it's not being used?
 *
 * Two goals:
 * - most tools need multiple osd modules to work (e.g. gdb needs mam and CDM,
 *   and possible trace)
 * - Yet it's helpful to have all decoding/interfacing with one debug module
 *   in one place.
 *
 *
 * Idea: provide IDL-like descriptions of trace events, and auto-generate all
 * other accessor methods on the fly by reading the CTF.
 */


// MAM
osd_mam_read()
osd_mam_write()


