## Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\" -DMULTIPATH_NUM=@MULTIPATH_NUM@
INSTALL_SDATA=@INSTALL@ -m 600

LIBCAP = @LIBCAP@

ipforward = @IPFORWARD@
if_method = @IF_METHOD@
rt_method = @RT_METHOD@
rtread_method = @RTREAD_METHOD@
kernel_method = @KERNEL_METHOD@
ioctl_method = @IOCTL_METHOD@

otherobj = $(ioctl_method) $(ipforward) $(if_method) \
	$(rt_method) $(rtread_method) $(kernel_method)

if HAVE_NETLINK
othersrc = zebra_fpm_netlink.c
endif

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

sbin_PROGRAMS = zebra

noinst_PROGRAMS = testzebra

zebra_SOURCES = \
	zserv.c main.c interface.c connected.c zebra_rib.c zebra_routemap.c \
	redistribute.c debug.c rtadv.c zebra_snmp.c zebra_vty.c \
	irdp_main.c irdp_interface.c irdp_packet.c router-id.c zebra_fpm.c \
	$(othersrc)

testzebra_SOURCES = test_main.c zebra_rib.c interface.c connected.c debug.c \
	zebra_vty.c \
	kernel_null.c  redistribute_null.c ioctl_null.c misc_null.c

noinst_HEADERS = \
	connected.h ioctl.h rib.h rt.h zserv.h redistribute.h debug.h rtadv.h \
	interface.h ipforward.h irdp.h router-id.h kernel_socket.h \
	rt_netlink.h zebra_fpm.h zebra_fpm_private.h

zebra_LDADD = $(otherobj) ../lib/libzebra.la $(LIBCAP)

testzebra_LDADD = ../lib/libzebra.la $(LIBCAP)

zebra_DEPENDENCIES = $(otherobj)

EXTRA_DIST = if_ioctl.c if_ioctl_solaris.c if_netlink.c \
        if_sysctl.c ipforward_proc.c \
	ipforward_solaris.c ipforward_sysctl.c rt_netlink.c \
	rt_socket.c rtread_netlink.c rtread_sysctl.c \
	rtread_getmsg.c kernel_socket.c kernel_netlink.c \
	ioctl.c ioctl_solaris.c \
	GNOME-SMI GNOME-PRODUCT-ZEBRA-MIB

client : client_main.o ../lib/libzebra.la
	$(CC) -g -o client client_main.o ../liblzebra.la $(LIBS) $(LIB_IPV6)

quaggaconfdir = $(sysconfdir)

examplesdir = $(exampledir)
dist_examples_DATA = zebra.conf.sample
