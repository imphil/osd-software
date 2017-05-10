bin_PROGRAMS = osd-daemon

osd_daemon_LDADD = ../../com/libosd-com.la

osd_daemon_LDFLAGS = $(AM_LDFLAGS) \
	${libglip_LIBS}

osd_daemon_CFLAGS = $(AM_CFLAGS) \
	-I$(top_srcdir)/src/common/include \
	-I$(top_srcdir)/src/com/include \
	${libglip_CFLAGS}
osd_daemon_SOURCES = osd-daemon.c
