## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\" -DMULTIPATH_NUM=@MULTIPATH_NUM@
INSTALL_SDATA=@INSTALL@ -m 600

LIB_IPV6 = @LIB_IPV6@

ipforward = @IPFORWARD@
if_method = @IF_METHOD@
if_proc = @IF_PROC@
rt_method = @RT_METHOD@
rtread_method = @RTREAD_METHOD@
kernel_method = @KERNEL_METHOD@
other_method = @OTHER_METHOD@

otherobj = $(ipforward) $(if_method) $(if_proc) $(rt_method) \
	$(rtread_method) $(kernel_method) $(other_method)

sbin_PROGRAMS = zebra

zebra_SOURCES = \
	zserv.c main.c interface.c connected.c ioctl.c zebra_rib.c \
	redistribute.c debug.c rtadv.c zebra_snmp.c zebra_vty.c \
	irdp_main.c irdp_interface.c irdp_packet.c

noinst_HEADERS = \
	connected.h ioctl.h rib.h rt.h zserv.h redistribute.h debug.h rtadv.h \
	interface.h ipforward.h irdp.h

zebra_LDADD = ../lib/libzebra.a $(otherobj) $(LIB_IPV6)

zebra_DEPENDENCIES = $(otherobj)

sysconf_DATA = zebra.conf.sample

EXTRA_DIST = $(sysconf_DATA) if_ioctl.c if_netlink.c if_proc.c if_sysctl.c \
	ipforward_aix.c ipforward_ews.c ipforward_proc.c \
	ipforward_solaris.c ipforward_sysctl.c rt_ioctl.c rt_netlink.c \
	rt_socket.c rtread_netlink.c rtread_proc.c rtread_sysctl.c \
	rtread_getmsg.c kernel_socket.c kernel_netlink.c mtu_kvm.c \
	GNOME-SMI GNOME-PRODUCT-ZEBRA-MIB

#client : client_main.o ../lib/libzebra.a
#	$(CC) -g -o client client_main.o ../lib/libzebra.a $(LIBS) $(LIB_IPV6)

install-sysconfDATA: $(sysconf_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	@list='$(sysconf_DATA)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_SDATA) $(srcdir)/$$p $(DESTDIR)$(sysconfdir)/$$p"; \
	    $(INSTALL_SDATA) $(srcdir)/$$p $(DESTDIR)$(sysconfdir)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_SDATA) $$p $(DESTDIR)$(sysconfdir)/$$p"; \
	    $(INSTALL_SDATA) $$p $(DESTDIR)$(sysconfdir)/$$p; \
	  fi; fi; \
	done
