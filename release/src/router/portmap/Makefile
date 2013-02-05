
####################################
### Beginning of configurable stuff.

# By default, logfile entries are written to the same file as used for
# sendmail transaction logs. Change the definition of the following macro
# if you disagree. See `man 3 syslog' for examples. Some syslog versions
# do not provide this flexibility.

FACILITY=LOG_DAEMON

# To disable tcp-wrapper style access control, comment out the following
# macro definitions.  Access control can also be turned off by providing
# no access control tables. The local system, since it runs the portmap
# daemon, is always treated as an authorized host.
# By default, access control does not do hostname lookup as there is a risk
# that will require portmap access, hence deadlock.  If you are sure the
# target system will never user NIS for hostname lookup, you can define
# USE_DNS to add hostname tests in hosts.allow/deny.

ifeq ($(NO_TCP_WRAPPER),)
CPPFLAGS += -DHOSTS_ACCESS
WRAP_LIB  = -lwrap
ifdef USE_DNS
CPPFLAGS += -DENABLE_DNS
MAN_SED += -e 's/USE_DNS/yes/'
endif
endif

# Comment out if your RPC library does not allocate privileged ports for
# requests from processes with root privilege, or the new portmap will
# always reject requests to register/unregister services on privileged
# ports. You can find out by running "rpcinfo -p"; if all mountd and NIS
# daemons use a port >= 1024 you should probably disable the next line.

#CPPFLAGS += -DCHECK_PORT

# The portmap daemon runs a uid=1/gid=1 by default.  You can change that
# be defining DAEMON_UID and DAMEON_GID to numbers, or RPCUSER to a
# name, though you must be sure that name lookup will not require use
# of portmap.
ifdef RPCUSER
CPPFLAGS += -DRPCUSER=\"$(RPCUSER)\"
MAN_SED += -e 's/RPCUSER/$(RPCUSER)/'
else
MAN_SED += -e 's/RPCUSER//'
endif
ifdef DAEMON_UID
CPPFLAGS += -DDAEMON_UID=$(DAEMON_UID) -DDAEMON_GID=$(DAEMON_GID)
MAN_SED += -e 's/DAEMON_UID/$(DAEMON_UID)/' -e 's/DAEMON_GID/$(DAEMON_GID)/'
else
MAN_SED += -e 's/DAEMON_UID/1/' -e 's/DAEMON_GID/1/'
endif

# Warning: troublesome feature ahead!! Enable only when you are really
# desperate!!
#
# It is possible to prevent an attacker from manipulating your portmapper
# tables from outside with requests that contain spoofed source addresses.
# The countermeasure is to force all rpc servers to register and
# unregister with the portmapper via the loopback network interface,
# instead of via the primary network interface that every host can talk
# to. For this countermeasure to work it is necessary to uncomment the
# LOOPBACK definition below, and to take the following additional steps:
# 
# (1) Modify the libc library (or librpc if you have one) and replace
# get_myaddress() by a version that selects the loopback address instead
# of the primary network interface address. A suitable version is
# provided in the file get_myaddress.c. This forces rpc servers to send
# all set/unset requests to the loopback address.
# 
# (2) Rebuild all statically-linked rpc servers with the modified
# library.
# 
# (3) Disable IP source routing in the kernel (otherwise an outside
# attacker can still send requests that appear to come from the local
# machine).
# 
# Instead of (1) it may be sufficient to run the rpc servers with a
# preload shared object that implements the alternate get_myaddress()
# behavior (see Makefile.shlib). You still need to disable IP source
# routing, though.
#
# I warned you, you need to be really desperate to do this. It is
# probably much easier to just block port UDP and TCP ports 111 on
# your routers.
#
# CPPFLAGS += -DLOOPBACK_SETUNSET

# When the portmapper cannot find any local interfaces (it will complain
# to the syslog daemon) your system probably has variable-length socket
# address structures (struct sockaddr has a sa_len component; examples:
# AIX 4.1 and 4.4BSD). Uncomment next macro definition in that case.
#
# CPPFLAGS += -DHAS_SA_LEN		# AIX 4.x, BSD 4.4, FreeBSD, NetBSD

# With verbose logging on, HP-UX 9.x and AIX 4.1 leave zombies behind when
# SIGCHLD is not ignored. Enable next macro for a fix.
#
CPPFLAGS += -DIGNORE_SIGCHLD	# AIX 4.x, HP-UX 9.x

# Uncomment the following macro if your system does not have u_long.
#
# CPPFLAGS	+=-Du_long="unsigned long"

#
# LDLIBS	+= -m
# CFLAGS	+= -arch m68k -arch i386 -arch hppa

# Auxiliary libraries that you may have to specify
#
# LDLIBS	+= -lrpc

# Comment out if your compiler talks ANSI and understands const
#
# CPPFLAGS += -Dconst=

### End of configurable stuff.
##############################

CPPFLAGS += -DFACILITY=$(FACILITY)
CFLAGS   ?= -O2
CFLAGS   += -Wall -Wstrict-prototypes $(EXTRACFLAGS)

all:	portmap pmap_dump pmap_set portmap.man

CPPFLAGS += $(HOSTS_ACCESS)
portmap: CFLAGS   += -fpie
portmap: LDLIBS   += $(WRAP_LIB)
#portmap: LDFLAGS  += -pie
portmap: portmap.o pmap_check.o from_local.o

from_local: CPPFLAGS += -DTEST

portmap.man : portmap.8
	sed $(MAN_SED) < portmap.8 > portmap.man

install: all
	install -o root -g root -m 0755 -s portmap ${BASEDIR}/sbin
	install -o root -g root -m 0755 -s pmap_dump ${BASEDIR}/sbin
	install -o root -g root -m 0755 -s pmap_set ${BASEDIR}/sbin
	install -o root -g root -m 0644 portmap.man ${BASEDIR}/usr/share/man/man8/portmap.8
	install -o root -g root -m 0644 pmap_dump.8 ${BASEDIR}/usr/share/man/man8
	install -o root -g root -m 0644 pmap_set.8 ${BASEDIR}/usr/share/man/man8

clean:
	rm -f *.o portmap pmap_dump pmap_set from_local \
	    core portmap.man

-include .depend
.depend: *.c
	$(CC) -MM $(CFLAGS) *.c > .depend

.PHONY: all clean install
