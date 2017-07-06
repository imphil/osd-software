bin_PROGRAMS = osd-systrace-log

osd_systrace_log_LDADD = ../../libosd/libosd.la

AM_LDFLAGS += \
	${libczmq_LIBS}

AM_CFLAGS += \
	-I$(top_srcdir)/src/libosd/include \
	-include $(top_builddir)/config.h \
	${libczmq_CFLAGS}

osd_systrace_log_SOURCES = \
	../argtable3.c \
	../iniparser.c \
	../dictionary.c \
	osd-systrace-log.c 
