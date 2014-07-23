check_PROGRAMS = test-geoip

noinst_PROGRAMS = benchmark test-geoip-region test-geoip-city test-geoip-org test-geoip-domain test-geoip-asnum test-geoip-isp test-geoip-netspeed

EXTRA_PROGRAMS = benchmark \
		 test-geoip-region \
		 test-geoip-city \
		 test-geoip-org \
                 test-geoip-domain \
		 test-geoip-asnum \
		 test-geoip-isp \
		 test-geoip-netspeed 

LDADD = $(top_builddir)/libGeoIP/libGeoIP.la
AM_CPPFLAGS = -I$(top_srcdir)/libGeoIP -DSRCDIR=\"$(top_srcdir)\" -Wall

test_geoip_SOURCES = test-geoip.c

test_geoip_region_SOURCES = test-geoip-region.c

test_geoip_org_SOURCES = test-geoip-org.c

test_geoip_domain_SOURCES = test-geoip-domain.c

test_geoip_isp_SOURCES = test-geoip-isp.c

test_geoip_asnum_SOURCES = test-geoip-asnum.c

test_geoip_netspeed_SOURCES = test-geoip-netspeed.c

test_geoip_city_SOURCES = test-geoip-city.c

benchmark_SOURCES = benchmark.c

EXTRA_DIST = Makefile.vc city_test.txt country_test.txt country_test2.txt country_test_name.txt region_test.txt 
TESTS = test-geoip
