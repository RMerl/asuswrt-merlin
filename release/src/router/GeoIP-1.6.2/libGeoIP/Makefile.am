lib_LTLIBRARIES = libGeoIP.la

EXTRA_DIST = Makefile.vc GeoIP_internal.h pread.c pread.h 

AM_CPPFLAGS = -DGEOIPDATADIR=\"$(pkgdatadir)\" -Wall

libGeoIP_la_SOURCES = GeoIP.c GeoIP_deprecated.c GeoIPCity.c regionName.c timeZone.c
include_HEADERS = GeoIP.h GeoIPCity.h

libGeoIP_la_LDFLAGS = -no-undefined -version-info @GEOIP_VERSION_INFO@

GeoIP.lo GeoIP.o: GeoIP.c GeoIP_deprecated.c GeoIP.h

GeoIPCity.lo GeoIPCity.o: GeoIPCity.c GeoIP.h

regionName.lo regionName.o: regionName.c

timeZone.lo timeZone.o: timeZone.c

