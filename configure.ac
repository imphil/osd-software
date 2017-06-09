# Open SoC Debug version information
# Update this on every release. For API changes also update the library
# version below!
m4_define([OSD_VERSION_MAJOR_], [0])
m4_define([OSD_VERSION_MINOR_], [1])
m4_define([OSD_VERSION_MICRO_], [0])
m4_define([OSD_VERSION_SUFFIX_], [-dev])

AC_PREREQ(2.60)
AC_INIT([opensocdebug],
        [OSD_VERSION_MAJOR_[.]OSD_VERSION_MINOR_[.]OSD_VERSION_MICRO_[]OSD_VERSION_SUFFIX_],
        [opensocdebug@lists.librecores.org],
        [opensocdebug],
        [http://www.opensocdebug.org])

# Library versioning
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# http://sourceware.org/autobook/autobook/autobook_91.html
OSD_CURRENT=0
OSD_REVISION=1
OSD_AGE=0
LTLDFLAGS="-version-info ${OSD_CURRENT}:${OSD_REVISION}:${OSD_AGE}"
AC_SUBST(LTLDFLAGS)


# define macros in config.h with the version information
AC_DEFINE([OSD_VERSION_MAJOR], [OSD_VERSION_MAJOR_], "OSD major version")
AC_DEFINE([OSD_VERSION_MINOR], [OSD_VERSION_MINOR_], "OSD minor version")
AC_DEFINE([OSD_VERSION_MICRO], [OSD_VERSION_MICRO_], "OSD micro version")
AC_DEFINE([OSD_VERSION_SUFFIX], ["OSD_VERSION_SUFFIX_"], "OSD version suffix")


AC_CONFIG_SRCDIR([src/libosd/include/osd/osd.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([check-news foreign 1.11 -Wall -Wno-portability silent-rules \
                  tar-pax no-dist-gzip dist-xz subdir-objects])

AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])
LT_INIT([disable-static pic-only])
AC_PREFIX_DEFAULT([/usr])

AC_PROG_SED
AC_PROG_MKDIR_P

# check unit testing framework
PKG_CHECK_MODULES([CHECK], [check >= 0.10.0])

# test coverage with gcov and reporting with lcov
AX_CODE_COVERAGE

AC_ARG_ENABLE([logging],
    AS_HELP_STRING([--disable-logging], [disable system logging @<:@default=enabled@:>@]),
    [],
    [enable_logging=yes])
AS_IF([test "x$enable_logging" = "xyes"], [
    AC_DEFINE(LOGGING, [1], [System logging.])
])

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [enable debug messages @<:@default=disabled@:>@]),
    [],
    [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
    AC_DEFINE(DEBUG, [1], [Debug messages.])
])


# osd-daemon
AC_ARG_ENABLE([daemon],
    AS_HELP_STRING([--disable-daemon], [don't build osd-daemon]))

AS_IF([test "x$enable_daemon" != "xno"], [
    # GLIP is required to build the daemon
    PKG_CHECK_MODULES([libglip], [libglip >= 0.1])
])
AM_CONDITIONAL([ENABLE_DAEMON], [test "x$enable_daemon" != "xno"])

# check for libreadline
#AC_CHECK_LIB([readline], [readline], [
#    READLINE_LIBS=-lreadline
#    AC_DEFINE([HAVE_LIBREADLINE], [1], [Define to 1 if you have the `readline' library (-lreadline).])
#])
#AC_SUBST(READLINE_LIBS)

#AC_CHECK_HEADER([readline/readline.h],,AC_MSG_ERROR([Unable to find readline.h. You may need to install the readline development package.]))

# check for libelf
#AC_CHECK_LIB([elf], [elf_version], [
#    LIBELF_LIBS=-lelf
#    AC_DEFINE([HAVE_LIBELF], [1], [Define to 1 if you have the `elfutils' library (-lelf).])
#])
#AC_SUBST(LIBELF_LIBS)

#AC_CHECK_HEADER([elfutils/version.h],,AC_MSG_ERROR([Unable to find elfutils/version.h. You may need to install the libelf-dev development package.]))

#AC_ARG_ENABLE([python-bindings],
#    AS_HELP_STRING([--enable-python-bindings], [enable python bindings (default n)]),
#    [enable_python=$enableval],
#    [enable_python=no])

#AM_CONDITIONAL([HAVE_SWIG], [test "$SWIG"])
#AS_IF([test "x$enable_python" = "xyes"], [
#    AC_CHECK_PROG([SWIG], [swig], yes)
#    AS_IF([test "x$SWIG" != "xyes"],
#          [AC_MSG_ERROR([Unable to find swig. Needed for python bindings.])])
#    AM_PATH_PYTHON2([2.5.0])
#    AC_DEFINE(ENABLE_PYTHON, [1], [Define to use python bindings])
#])
#AM_CONDITIONAL([ENABLE_PYTHON], [test "x$enable_python" != "xno"])

AM_CFLAGS="-Wall \
    -Wmissing-declarations -Wmissing-prototypes \
    -Wnested-externs -Wpointer-arith \
    -Wsign-compare -Wchar-subscripts \
    -Wstrict-prototypes -Wshadow \
    -Wformat-security -Wtype-limits \
    -ffunction-sections \
    -fdata-sections \
    -pthread \
    $CODE_COVERAGE_CFLAGS"
AC_SUBST([AM_CFLAGS])

AM_LDFLAGS="-Wl,--gc-sections \
    -Wl,--as-needed \
    -pthread \
    -lrt \
    $CODE_COVERAGE_LIBS"
AC_SUBST(AM_LDFLAGS)

dnl ----- DOCUMENTATION -----
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(ON)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(osd-software, doc/doxygen.cfg, apidoc)


AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
        Makefile
        src/Makefile
        src/libosd/Makefile
        src/tools/Makefile
        src/tools/osd-daemon/Makefile
        tests/Makefile
        tests/unit/Makefile
        doc/Makefile
])

AC_OUTPUT
AC_MSG_RESULT([
Configuration summary for $PACKAGE $VERSION

PATHS
  prefix:                 ${prefix}
  libdir:                 ${libdir}
  includedir:             ${includedir}
  datarootdir:            ${datarootdir}
  datadir:                ${datadir}

COMPILER OPTIONS
  compiler:               ${CC}
  cflags:                 ${CFLAGS}
  ldflags:                ${LDFLAGS}

ENABLED TOOLS
  osd-daemon              ${enable_daemon}
  
Now run
make
to start the build process.
])
