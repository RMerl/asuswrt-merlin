# udhcp makefile

prefix=/usr
SBINDIR=/sbin
USRSBINDIR=${prefix}/sbin
USRBINDIR=${prefix}/bin
USRSHAREDIR=${prefix}/share
INSTALLDIR = $(ROOTDIR)/romfs

# Uncomment this to get a shared binary. Call as udhcpd for the server,
# and udhcpc for the client
# COMBINED_BINARY=1

# Uncomment this for extra output and to compile with debugging symbols
# DEBUG=1

# Uncomment this to output messages to syslog, otherwise, messages go to stdout
#CFLAGS += -DSYSLOG

#CROSS_COMPILE=arm-uclibc-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc

ifeq ($(ROOTDIR),)	# Broadcom Platform
CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include
#LDFLAGS += -L$(TOP)/netconf -L$(PLATFORMDIR)/install/netconf/usr/lib -lnetconf
#LDFLAGS = -L$(TOP)/shared -lshared
else			# Ralink Platform
SHDIR = $(ROOTDIR)/user/shared
CFLAGS += -I$(SHDIR)/include -I$(ROOTDIR)/linux-2.6.21.x/include
#LDFLAGS += -lshared -L$(SHDIR)
endif

INSTALL = install

VER := 0.9.8-asus

OBJS_SHARED = options.o socket.o packet.o pidfile.o uptime.o
DHCPD_OBJS = dhcpd.o arpping.o files.o leases.o serverpacket.o
DHCPC_OBJS = dhcpc.o clientpacket.o script.o

# Comment this to disable compilation
# BOOT_PROGRAMS = udhcpc
DAEMONS = udhcpd
COMMANDS = dumpleases

ifdef COMBINED_BINARY
EXEC1 = udhcpd
OBJS1 = $(DHCPD_OBJS) $(DHCPC_OBJS) $(OBJS_SHARED) frontend.o
CFLAGS += -DCOMBINED_BINARY
else
EXEC1 = $(DAEMONS)
OBJS1 = $(DHCPD_OBJS) $(OBJS_SHARED)

EXEC2 = $(BOOT_PROGRAMS)
OBJS2 = $(DHCPC_OBJS) $(OBJS_SHARED)
endif

EXEC3 = $(COMMANDS)
OBJS3 = dumpleases.o

ifdef SYSLOG
CFLAGS += -DSYSLOG
endif

CFLAGS += -W -Wall -Wstrict-prototypes -DVERSION='"$(VER)"'

ifdef DEBUG
CFLAGS += -g -DDEBUG
STRIP=true
else
CFLAGS += -O2 -fomit-frame-pointer
STRIP=$(CROSS_COMPILE)strip
endif

all: $(EXEC1) $(EXEC2) $(EXEC3)
	$(STRIP) --remove-section=.note --remove-section=.comment $(EXEC1) $(EXEC2) $(EXEC3)

$(OBJS1) $(OBJS2) $(OBJS3): *.h Makefile
$(EXEC1) $(EXEC2) $(EXEC3): Makefile

.c.o:
	$(CC) -c $(CFLAGS) $<

$(EXEC1): $(OBJS1)
	$(LD) $(LDFLAGS) $(OBJS1) -o $(EXEC1)

$(EXEC2): $(OBJS2)
	$(LD) $(LDFLAGS) $(OBJS2) -o $(EXEC2)

$(EXEC3): $(OBJS3)
	$(LD) $(LDFLAGS) $(OBJS3) -o $(EXEC3)


install: all

	#$(INSTALL) $(DAEMONS) $(USRSBINDIR)
	#$(INSTALL) $(COMMANDS) $(USRBINDIR)
ifdef COMBINED_BINARY
	#ln -sf $(USRSBINDIR)/$(DAEMONS) $(SBINDIR)/$(BOOT_PROGRAMS)
else
	#$(INSTALL) $(BOOT_PROGRAMS) $(SBINDIR)
endif
	#mkdir -p $(USRSHAREDIR)/udhcpc
	#for name in bound deconfig renew script ; do \
	#	$(INSTALL) samples/sample.$$name \
	#		$(USRSHAREDIR)/udhcpc/default.$$name ; \
	#done
	#mkdir -p $(USRSHAREDIR)/man/man1
	#$(INSTALL) dumpleases.1 $(USRSHAREDIR)/man/man1
	#mkdir -p $(USRSHAREDIR)/man/man5
	#$(INSTALL) udhcpd.conf.5 $(USRSHAREDIR)/man/man5
	#mkdir -p $(USRSHAREDIR)/man/man8
	#$(INSTALL) udhcpc.8 udhcpd.8 $(USRSHAREDIR)/man/man8

	$(INSTALL) -d $(INSTALLDIR)/usr/sbin
ifdef COMBINED_BINARY
	$(INSTALL) udhcpd $(INSTALLDIR)/usr/sbin
	cd $(INSTALLDIR)/usr/sbin && ln -sf udhcpd udhcpc
else
ifneq ($(EXEC1),)
	$(INSTALL) $(EXEC1) $(INSTALLDIR)/usr/sbin
endif
ifneq ($(EXEC2),)
	$(INSTALL) $(EXEC2) $(INSTALLDIR)/usr/sbin
endif
endif

romfs: all
ifdef COMBINED_BINARY
	$(ROMFSINST) /usr/sbin/udhcpd
	cd $(INSTALLDIR)/usr/sbin && ln -sf udhcpd udhcpc
else
ifneq ($(EXEC1),)
	$(ROMFSINST) /usr/sbin/$(EXEC1)
endif
ifneq ($(EXEC2),)
	$(ROMFSINST) /usr/sbin/$(EXEC2)
endif
endif

clean:
	-rm -f udhcpd udhcpc dumpleases *.o core
