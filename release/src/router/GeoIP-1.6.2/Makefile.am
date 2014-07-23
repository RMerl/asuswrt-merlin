AM_CPPFLAGS = -Wall -ansi

AUTOMAKE_OPTIONS = foreign

SUBDIRS =		\
	libGeoIP	\
	apps		\
	test		\
	man

pkgconfigdir = $(libdir)/pkgconfig/
nodist_pkgconfig_DATA = geoip.pc

EXTRA_DIST = README.md README.OSX READMEwin32static.txt README.MinGW READMEwin32.txt GeoIPWinDLL.patch TODO bootstrap GeoIP.spec GeoIP.spec.in Makefile.netware Makefile.vc Makefile.win32 get_ver.awk geoip.ico geoip.pc.in data/GeoIP.dat
