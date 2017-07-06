pkginclude_HEADERS = include/osd/osd.h \
	include/osd/hostmod.h

lib_LTLIBRARIES = libosd.la

libosd_la_SOURCES = log.c \
	packet.c \
	hostmod.c \
	util.c

libosd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(LTLDFLAGS) \
    -fvisibility=hidden

libosd_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(srcdir)/include \
	-include $(top_builddir)/config.h
