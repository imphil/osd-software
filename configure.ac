# Copyright (c) 2017 by the author(s)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# Author(s):
#   Philipp Wagner <philipp.wagner@tum.de>

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
PKG_CHECK_MODULES([CHECK], [check >= 0.9.10])

# test coverage with gcov and reporting with lcov
AX_CODE_COVERAGE

# ZeroMQ and high-level C wrapper (CZMQ)
# use the libzmq3-dev and libczmq3-dev packages in Debian/Ubuntu
PKG_CHECK_MODULES([libzmq], [libzmq >= 4.1])
PKG_CHECK_MODULES([libczmq], [libczmq >= 3.0])

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

# documentation
AC_ARG_ENABLE([docs],
    AS_HELP_STRING([--enable-docs], [build documentation @<:@default=disabled@:>@]),
    [],
    [enable_docs=no])
AS_IF([test "x$enable_docs" = "xyes"], [
    AC_CHECK_PROG([DOXYGEN], [doxygen], [doxygen])
    AS_IF([ test -z "$DOXYGEN" ], [
			AC_MSG_ERROR([doxygen is required to build the documentation.])
		])
])
AM_CONDITIONAL([BUILD_DOCS], [test "x$enable_docs" = "xyes"])


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

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
        Makefile
        src/Makefile
        src/libosd/Makefile
        src/tools/Makefile
        src/tools/osd-daemon/Makefile
        src/tools/osd-systrace-log/Makefile
        src/tools/osd-host-controller/Makefile
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

ENABLED TOOLS])
AS_IF([test "x$enable_daemon" != "xno"], AC_MSG_RESULT([  osd-daemon]))

AC_MSG_RESULT([
You can now run 'make' to start the build process.
])
