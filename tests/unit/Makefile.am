check_PROGRAMS = \
	check_log \
	check_util \
	check_packet \
	check_com

TESTS = $(check_PROGRAMS)

AM_CFLAGS = \
	@CHECK_CFLAGS@ \
	-I$(top_srcdir)/src/libosd/include \
	-include $(top_builddir)/config.h

LDADD = \
	@CHECK_LIBS@ \
	$(top_builddir)/src/libosd/libosd.la



# Include make  targets to generate code coverage reports
CODE_COVERAGE_IGNORE_PATTERN = "/usr/*"
@CODE_COVERAGE_RULES@
