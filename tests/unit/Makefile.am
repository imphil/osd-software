check_PROGRAMS = \
	check_log \
	check_util \
	check_packet \
	check_hostmod \
	check_hostmod_stmlogger

check_hostmod_SOURCES = \
	check_hostmod.c \
	mock_host_controller.c

check_hostmod_stmlogger_SOURCES = \
	check_hostmod_stmlogger.c \
	mock_host_controller.c

TESTS = $(check_PROGRAMS)

AM_CFLAGS = \
	@CHECK_CFLAGS@ \
	-I$(top_srcdir)/src/libosd/include \
	-include $(top_builddir)/config.h

LDADD = \
	@CHECK_LIBS@ \
	$(top_builddir)/src/libosd/libosd.la



# Include make targets to generate code coverage reports
CODE_COVERAGE_IGNORE_PATTERN = "/usr/*"
@CODE_COVERAGE_RULES@

# Include make targets to generate check targets with valgrind (check-valgrind)
@VALGRIND_CHECK_RULES@
VALGRIND_memcheck_FLAGS = --leak-check=full
#VALGRIND_SUPPRESSIONS_FILES = my-project.supp
#EXTRA_DIST = my-project.supp
