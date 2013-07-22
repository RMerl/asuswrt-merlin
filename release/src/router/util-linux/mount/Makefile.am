include $(top_srcdir)/config/include-Makefile.am

EXTRA_DIST = README.mount

bin_PROGRAMS = mount umount
sbin_PROGRAMS = losetup swapon
dist_man_MANS = fstab.5 mount.8 swapoff.8 swapon.8 umount.8 losetup.8

# generic sources for all programs (mount, umount, losetup)
srcs_common = sundries.c xmalloc.c $(top_srcdir)/lib/canonicalize.c sundries.h xmalloc.h

# generic header for mount and umount
hdrs_mount = fstab.h mount_mntent.h mount_constants.h \
	lomount.h getusername.h loop.h

# generic sources for mount and umount
srcs_mount = fstab.c mount_mntent.c getusername.c lomount.c devname.c devname.h \
	$(srcs_common) $(hdrs_mount) $(top_srcdir)/lib/env.c \
	$(top_srcdir)/lib/linux_version.c $(top_srcdir)/lib/blkdev.c \
	$(top_srcdir)/lib/fsprobe.c $(top_srcdir)/lib/mangle.c

# generic flags for all programs (except losetup)
# -- note that pkg-config autoconf macros (pkg.m4) does not differentiate
#    between CFLAGS and CPPFLAGS, we follow this behaviour and use CFLAGS only.
ldadd_common =
ldadd_static =
cflags_common = $(AM_CFLAGS)
ldflags_static = -all-static

mount_SOURCES = mount.c $(srcs_mount) $(top_srcdir)/lib/setproctitle.c \
		$(top_srcdir)/lib/strutils.c
mount_CFLAGS = $(SUID_CFLAGS) $(cflags_common)
mount_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)
mount_LDADD = $(ldadd_common)

umount_SOURCES = umount.c $(srcs_mount) $(top_srcdir)/lib/strutils.c
umount_CFLAGS = $(SUID_CFLAGS) $(cflags_common)
umount_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)
umount_LDADD = $(ldadd_common)

swapon_SOURCES = swapon.c swap_constants.h $(top_srcdir)/lib/linux_version.c \
		$(top_srcdir)/lib/blkdev.c $(top_srcdir)/lib/fsprobe.c \
		$(top_srcdir)/lib/canonicalize.c $(top_srcdir)/lib/mangle.c
swapon_CFLAGS = $(cflags_common)
swapon_LDADD = $(ldadd_common)

losetup_SOURCES = lomount.c $(srcs_common) loop.h lomount.h \
		$(top_srcdir)/lib/strutils.c
losetup_CPPFLAGS = -DMAIN $(AM_CPPFLAGS)

mount_static_LDADD =

if HAVE_STATIC_MOUNT
bin_PROGRAMS += mount.static
mount_static_SOURCES = $(mount_SOURCES)
mount_static_CFLAGS = $(cflags_common)
mount_static_LDFLAGS = $(ldflags_static)
mount_static_LDADD += $(ldadd_static)
endif

if HAVE_STATIC_UMOUNT
bin_PROGRAMS += umount.static
umount_static_SOURCES = $(umount_SOURCES)
umount_static_CFLAGS = $(cflags_common)
umount_static_LDFLAGS = $(ldflags_static)
umount_static_LDADD = $(ldadd_static)
endif

if HAVE_STATIC_LOSETUP
bin_PROGRAMS += losetup.static
losetup_static_SOURCES = $(losetup_SOURCES)
losetup_static_LDFLAGS = $(ldflags_static)
losetup_static_CPPFLAGS = -DMAIN $(AM_CPPFLAGS)
endif

if BUILD_LIBBLKID
ldadd_common += $(ul_libblkid_la)
ldadd_static += $(ul_libblkid_la)
cflags_common += -I$(ul_libblkid_incdir)
endif

if HAVE_SELINUX
mount_LDADD += $(SELINUX_LIBS)
mount_static_LDADD += $(SELINUX_LIBS_STATIC)
endif

if BUILD_LIBMOUNT_MOUNT
mount_LDADD += $(ul_libmount_la)
mount_CFLAGS += $(AM_CFLAGS) -I$(ul_libmount_incdir)
umount_LDADD += $(ul_libmount_la)
umount_CFLAGS += $(AM_CFLAGS) -I$(ul_libmount_incdir)
if HAVE_STATIC_MOUNT
mount_static_LDADD += $(ul_libmount_la)
mount_static_CFLAGS += $(AM_CFLAGS) -I$(ul_libmount_incdir)
endif
if HAVE_STATIC_UMOUNT
umount_static_LDADD += $(ul_libmount_la)
umount_static_CFLAGS += $(AM_CFLAGS) -I$(ul_libmount_incdir)
endif
endif

if !BUILD_LIBMOUNT_MOUNT
noinst_PROGRAMS = mtab_lock_test
mtab_lock_test_SOURCES = fstab.c $(srcs_common) $(hdrs_mount)
mtab_lock_test_CPPFLAGS = -DMAIN_TEST_MTABLOCK $(AM_CPPFLAGS)
endif

install-exec-hook:
	cd $(DESTDIR)$(sbindir) && ln -sf swapon swapoff
if MAKEINSTALL_DO_SETUID
	chmod 4755 $(DESTDIR)$(bindir)/mount
	chmod 4755 $(DESTDIR)$(bindir)/umount
endif

