pkginclude_HEADERS = include/osd-com.h
lib_LTLIBRARIES = libosd-com.la

libosd_com_la_SOURCES = osd-com.c

libosd_com_la_LDFLAGS = $(AM_LDFLAGS) $(LTLDFLAGS)
libosd_com_la_CPPFLAGS = $(AM_CPPFLAGS) \
						 -I$(top_srcdir)/src/common/include \
						 -I$(srcdir)/include
						 -include $(top_builddir)/config.h
