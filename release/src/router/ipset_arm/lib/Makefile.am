include $(top_srcdir)/Make_global.am

IPSET_SETTYPE_LIST = \
	ipset_bitmap_ip.c \
	ipset_bitmap_ipmac.c \
	ipset_bitmap_port.c \
	ipset_hash_ip.c \
	ipset_hash_ipport.c \
	ipset_hash_ipmark.c \
	ipset_hash_ipportip.c \
	ipset_hash_ipportnet.c \
	ipset_hash_net.c \
	ipset_hash_netnet.c \
	ipset_hash_netportnet.c \
	ipset_hash_netport.c \
	ipset_hash_netiface.c \
	ipset_hash_ipmac.c \
	ipset_hash_mac.c \
	ipset_list_set.c

AM_CFLAGS += ${libmnl_CFLAGS}

lib_LTLIBRARIES = libipset.la

include $(top_srcdir)/lib/Make_extra.am

libipset_la_LDFLAGS = -Wl,--version-script=$(top_srcdir)/lib/libipset.map -version-info $(LIBVERSION)
libipset_la_LIBADD  = ${libmnl_LIBS} $(IPSET_SETTYPE_STATIC_OBJECTS) $(LIBADD_DLOPEN)
libipset_la_SOURCES = \
	data.c \
	errcode.c \
	icmp.c \
	icmpv6.c \
	mnl.c \
	parse.c \
	print.c \
	session.c \
	types.c \
	ui.c \
	types_init.c

EXTRA_libipset_la_SOURCES = \
	debug.c

EXTRA_DIST = $(IPSET_SETTYPE_LIST) libipset.map

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libipset.pc

sparse-check: $(libipset_la_SOURCES:.c=.d)

%.d: %.c
	$(IPSET_AM_V_CHECK)\
	$(SPARSE) $(SPARSE_FLAGS) $(AM_CFLAGS) $(AM_CPPFLAGS) $< || :
