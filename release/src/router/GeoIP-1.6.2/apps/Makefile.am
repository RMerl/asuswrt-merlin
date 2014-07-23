AM_CPPFLAGS = -I$(top_srcdir)/libGeoIP -DSYSCONFDIR=\"$(sysconfdir)\" -Wall

DEPS = $(top_builddir)/libGeoIP/libGeoIP.la
LDADDS = $(top_builddir)/libGeoIP/libGeoIP.la

bin_PROGRAMS = geoiplookup geoiplookup6

geoiplookup_SOURCES = geoiplookup.c
geoiplookup_LDFLAGS =
geoiplookup_DEPENDENCIES = $(top_builddir)/libGeoIP/libGeoIP.la
geoiplookup_LDADD = $(top_builddir)/libGeoIP/libGeoIP.la

geoiplookup6_SOURCES = geoiplookup6.c
geoiplookup6_LDFLAGS =
geoiplookup6_DEPENDENCIES = $(top_builddir)/libGeoIP/libGeoIP.la
geoiplookup6_LDADD = $(top_builddir)/libGeoIP/libGeoIP.la

EXTRA_DIST = Makefile.vc
