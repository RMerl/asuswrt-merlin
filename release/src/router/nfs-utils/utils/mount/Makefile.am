## Process this file with automake to produce Makefile.in

# These binaries go in /sbin (not /usr/sbin), and that cannot be
# overriden at config time.
sbindir = /sbin

man8_MANS	= mount.nfs.man umount.nfs.man
man5_MANS	= nfs.man

sbin_PROGRAMS	= mount.nfs
EXTRA_DIST = nfsmount.x $(man8_MANS) $(man5_MANS)
mount_nfs_SOURCES = mount.c error.c network.c fstab.c token.c \
		    parse_opt.c parse_dev.c \
		    nfsmount.c nfs4mount.c stropts.c\
		    nfsumount.c \
		    mount_constants.h error.h network.h fstab.h token.h \
		    parse_opt.h parse_dev.h \
		    nfs4_mount.h nfs_mount4.h stropts.h version.h

mount_nfs_LDADD = ../../support/nfs/libnfs.a \
		  ../../support/export/libexport.a

MAINTAINERCLEANFILES = Makefile.in

install-exec-hook:
	(cd $(DESTDIR)$(sbindir) && \
	  ln -sf mount.nfs mount.nfs4 && \
	  ln -sf mount.nfs umount.nfs && \
	  ln -sf mount.nfs umount.nfs4 && \
	  chmod 4511 mount.nfs )
uninstall-hook:
	(cd $(DESTDIR)$(sbindir) && \
	    rm -f mount.nfs4 umount.nfs umount.nfs4)


install-man-links:
	(cd $(DESTDIR)$(man8dir) && \
	  for m in $(man8_MANS) $(dist_man8_MANS) $(nodist_man8_MANS); do \
	    inst=`echo $$m | sed -e 's/man$$/8/'`; \
	    rm -f $$inst ; \
	  done)
	(cd $(DESTDIR)$(man5dir) && \
	  for m in $(man5_MANS) $(dist_man5_MANS) $(nodist_man5_MANS); do \
	    inst=`echo $$m | sed -e 's/man$$/5/'`; \
	    rm -f $$inst ; \
	  done)

uninstall-man-links:
	(cd $(DESTDIR)$(man8dir) && \
	  for m in $(man8_MANS) $(dist_man8_MANS) $(nodist_man8_MANS); do \
	    inst=`echo $$m | sed -e 's/man$$/8/'`; \
	    rm -f $$inst ; \
	  done)
	(cd $(DESTDIR)$(man5dir) && \
	  for m in $(man5_MANS) $(dist_man5_MANS) $(nodist_man5_MANS); do \
	    inst=`echo $$m | sed -e 's/man$$/5/'`; \
	    rm -f $$inst ; \
	  done)

