AM_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir) $(all_includes)

if LINUX_API
OS_SPECIFIC = driver_name
OS_SPECIFIC_XFAIL = driver_name
endif

noinst_PROGRAMS = testlibusb descriptor_test id_test find_hubs find_mice \
		get_resolution hub_strings $(OS_SPECIFIC)

testlibusb_LDADD = $(top_builddir)/libusb.la @OSLIBS@

descriptor_test_SOURCES = descriptor_test.cpp
descriptor_test_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

id_test_SOURCES = id_test.cpp
id_test_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

find_hubs_SOURCES = find_hubs.cpp
find_hubs_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

find_mice_SOURCES = find_mice.cpp
find_mice_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

get_resolution_SOURCES = get_resolution.cpp
get_resolution_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

hub_strings_SOURCES = hub_strings.cpp
hub_strings_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

driver_name_SOURCES = driver_name.cpp
driver_name_LDADD = $(top_builddir)/libusbpp.la @OSLIBS@

TESTS = testlibusb descriptor_test id_test find_hubs find_mice \
		get_resolution hub_strings $(OS_SPECIFIC)
XFAIL_TESTS = get_resolution hub_strings $(OS_SPECIFIC_XFAIL)

