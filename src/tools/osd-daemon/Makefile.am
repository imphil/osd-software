bin_PROGRAMS = osd-daemon

osd_daemon_LDADD = ../../libosd/libosd.la

osd_daemon_LDFLAGS = $(AM_LDFLAGS) \
	${libglip_LIBS}

osd_daemon_CFLAGS = $(AM_CFLAGS) \
	${libglip_CFLAGS}
osd_daemon_SOURCES = osd-daemon.c
