[SUBSYSTEM::DYNCONFIG]

DYNCONFIG_OBJ_FILES = $(dynconfigsrcdir)/dynconfig.o

# set these to where to find various files
# These can be overridden by command line switches (see samba(8))
# or in smb.conf (see smb.conf(5))
CONFIG4FILE = $(sysconfdir)/smb.conf
pkgconfigdir = $(libdir)/pkgconfig
LMHOSTSFILE4 = $(sysconfdir)/lmhosts

$(dynconfigsrcdir)/dynconfig.o: CFLAGS+=-DCONFIGFILE=\"$(CONFIG4FILE)\" -DBINDIR=\"$(bindir)\" \
	 -DLMHOSTSFILE=\"$(LMHOSTSFILE4)\" \
	 -DLOCKDIR=\"$(lockdir)\" -DPIDDIR=\"$(piddir)\" -DDATADIR=\"$(datadir)\" \
	 -DLOGFILEBASE=\"$(logfilebase)\" \
	 -DCONFIGDIR=\"$(sysconfdir)\" -DNCALRPCDIR=\"$(ncalrpcdir)\" \
	 -DSWATDIR=\"$(swatdir)\" \
	 -DPRIVATE_DIR=\"$(privatedir)\" \
	 -DMODULESDIR=\"$(modulesdir)\" \
	 -DTORTUREDIR=\"$(torturedir)\" \
	 -DSETUPDIR=\"$(setupdir)\" \
	 -DWINBINDD_PRIVILEGED_SOCKET_DIR=\"$(winbindd_privileged_socket_dir)\" \
	 -DWINBINDD_SOCKET_DIR=\"$(winbindd_socket_dir)\" \
	 -DNTP_SIGND_SOCKET_DIR=\"$(ntp_signd_socket_dir)\"

