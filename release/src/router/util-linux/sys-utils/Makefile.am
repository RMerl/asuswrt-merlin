include $(top_srcdir)/config/include-Makefile.am

bin_PROGRAMS =
sbin_PROGRAMS =
usrbin_exec_PROGRAMS = flock ipcrm ipcs ipcmk renice setsid
usrsbin_exec_PROGRAMS = readprofile

dist_man_MANS = flock.1 ipcrm.1 ipcs.1 ipcmk.1 renice.1 setsid.1 \
		readprofile.8

if LINUX
#
# Linux-only utils with no another dependencies. All another dependencies have
# to be resolved in configure.ac end exported to makefiles by BUILD_*.
#
bin_PROGRAMS += dmesg
sbin_PROGRAMS += ctrlaltdel fsfreeze fstrim
usrbin_exec_PROGRAMS += cytune setarch
usrsbin_exec_PROGRAMS += ldattach tunelp rtcwake

dist_man_MANS += dmesg.1 ctrlaltdel.8 cytune.8 setarch.8 \
		ldattach.8 tunelp.8 rtcwake.8 fsfreeze.8 fstrim.8
endif # LINUX


if BUILD_LOSETUP
sbin_PROGRAMS += losetup
dist_man_MANS += losetup.8

losetup_SOURCES = losetup.c \
		$(top_srcdir)/lib/linux_version.c \
		$(top_srcdir)/lib/at.c \
		$(top_srcdir)/lib/sysfs.c \
		$(top_srcdir)/lib/loopdev.c \
		$(top_srcdir)/lib/canonicalize.c \
		$(top_srcdir)/lib/xgetpass.c \
		$(top_srcdir)/lib/strutils.c

if HAVE_STATIC_LOSETUP
bin_PROGRAMS += losetup.static
losetup_static_SOURCES = $(losetup_SOURCES)
losetup_static_LDFLAGS = -all-static
endif
endif # BUILD_LOSETUP


if BUILD_PRLIMIT
usrbin_exec_PROGRAMS += prlimit
dist_man_MANS += prlimit.1
prlimit_SOURCES = prlimit.c $(top_srcdir)/lib/strutils.c \
			$(top_srcdir)/lib/mbsalign.c \
			$(top_srcdir)/lib/tt.c
endif


if BUILD_NEW_MOUNT
# 
# The original (stable) mount is in mount/ directory
# -- temporary we share some man pages
#
bin_PROGRAMS += mount umount
dist_man_MANS += mount.8 ../mount/fstab.5 ../mount/umount.8

mount_SOURCES = mount.c \
		$(top_srcdir)/lib/env.c \
		$(top_srcdir)/lib/xgetpass.c \
		$(top_srcdir)/lib/strutils.c

mount_LDADD = $(ul_libmount_la) $(SELINUX_LIBS)
mount_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS) -I$(ul_libmount_incdir)
mount_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)

umount_SOURCES = umount.c $(top_srcdir)/lib/env.c
umount_LDADD = $(ul_libmount_la)
umount_CFLAGS = $(AM_CFLAGS) $(SUID_CFLAGS) -I$(ul_libmount_incdir)
umount_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)

if HAVE_STATIC_MOUNT
bin_PROGRAMS += mount.static
mount_static_SOURCES = $(mount_SOURCES)
mount_static_CFLAGS = $(mount_CFLAGS)
mount_static_LDFLAGS = $(mount_LDFLAGS) -all-static
mount_static_LDADD = $(mount_LDADD) $(SELINUX_LIBS_STATIC)
endif

if HAVE_STATIC_UMOUNT
bin_PROGRAMS += umount.static
umount_static_SOURCES = $(umount_SOURCES)
umount_static_CFLAGS = $(umount_CFLAGS)
umount_static_LDFLAGS = $(umount_LDFLAGS) -all-static
umount_static_LDADD = $(umount_LDADD)
endif
endif # BUILD_NEW_MOUNT


if BUILD_SWAPON
sbin_PROGRAMS += swapon
dist_man_MANS += swapoff.8 swapon.8

swapon_SOURCES = swapon.c \
		$(top_srcdir)/lib/linux_version.c \
		$(top_srcdir)/lib/blkdev.c \
		$(top_srcdir)/lib/fsprobe.c \
		$(top_srcdir)/lib/canonicalize.c \
		$(top_srcdir)/lib/mangle.c

swapon_CFLAGS = $(AM_CFLAGS) -I$(ul_libblkid_incdir)
swapon_LDADD = $(ul_libblkid_la)
endif

if BUILD_LSCPU
usrbin_exec_PROGRAMS += lscpu
lscpu_SOURCES = lscpu.c $(top_srcdir)/lib/cpuset.c \
			$(top_srcdir)/lib/strutils.c \
			$(top_srcdir)/lib/mbsalign.c \
			$(top_srcdir)/lib/tt.c \
			$(top_srcdir)/lib/path.c
dist_man_MANS += lscpu.1
endif

if BUILD_CHCPU
sbin_PROGRAMS += chcpu
chcpu_SOURCES = chcpu.c $(top_srcdir)/lib/cpuset.c \
			$(top_srcdir)/lib/path.c
dist_man_MANS += chcpu.8
endif

cytune_SOURCES = cytune.c cyclades.h $(top_srcdir)/lib/strutils.c
tunelp_SOURCES = tunelp.c lp.h
fstrim_SOURCES = fstrim.c $(top_srcdir)/lib/strutils.c
rtcwake_SOURCES = rtcwake.c $(top_srcdir)/lib/strutils.c
dmesg_SOURCES = dmesg.c $(top_srcdir)/lib/strutils.c
ipcmk_SOURCES = ipcmk.c $(top_srcdir)/lib/strutils.c
ipcrm_SOURCES = ipcrm.c  $(top_srcdir)/lib/strutils.c
flock_SOURCES = flock.c $(top_srcdir)/lib/strutils.c
ldattach_SOURCES = ldattach.c $(top_srcdir)/lib/strutils.c

if BUILD_MOUNTPOINT
bin_PROGRAMS += mountpoint
mountpoint_LDADD = $(ul_libmount_la)
mountpoint_CFLAGS = $(AM_CFLAGS) -I$(ul_libmount_incdir)
dist_man_MANS += mountpoint.1
endif

if BUILD_FALLOCATE
usrbin_exec_PROGRAMS += fallocate
fallocate_SOURCES = fallocate.c $(top_srcdir)/lib/strutils.c
dist_man_MANS += fallocate.1
endif

if BUILD_PIVOT_ROOT
sbin_PROGRAMS += pivot_root
dist_man_MANS += pivot_root.8
endif

if BUILD_SWITCH_ROOT
sbin_PROGRAMS += switch_root
dist_man_MANS += switch_root.8
endif

if BUILD_UNSHARE
usrbin_exec_PROGRAMS += unshare
dist_man_MANS += unshare.1
endif

if BUILD_ARCH
bin_PROGRAMS += arch
dist_man_MANS += arch.1
endif

SETARCH_LINKS = linux32 linux64

if ARCH_S390
SETARCH_LINKS += s390 s390x
endif
if ARCH_I86
SETARCH_LINKS += i386
endif
if ARCH_86_64
SETARCH_LINKS += i386 x86_64
endif
if ARCH_PPC
SETARCH_LINKS += ppc ppc64 ppc32
endif
if ARCH_SPARC
SETARCH_LINKS += sparc sparc64 sparc32 sparc32bash
endif
if ARCH_MIPS
SETARCH_LINKS += mips mips64 mips32
endif
if ARCH_IA64
SETARCH_LINKS += i386 ia64
endif
if ARCH_HPPA
SETARCH_LINKS += parisc parisc64 parisc32
endif

SETARCH_MAN_LINKS = $(SETARCH_LINKS:=.8)
man_MANS = $(RDEV_MAN_LINKS) $(SETARCH_MAN_LINKS)
CLEANFILES = $(man_MANS)

$(SETARCH_MAN_LINKS):
	$(AM_V_GEN)echo ".so man8/setarch.8" > $@

install-exec-hook:
if BUILD_SWAPON
	cd $(DESTDIR)$(sbindir) && ln -sf swapon swapoff
endif
if BUILD_NEW_MOUNT
if MAKEINSTALL_DO_SETUID
	chmod 4755 $(DESTDIR)$(bindir)/mount
	chmod 4755 $(DESTDIR)$(bindir)/umount
endif
endif
	for I in $(SETARCH_LINKS); do \
		cd $(DESTDIR)$(usrbin_execdir) && ln -sf setarch $$I ; \
	done

uninstall-hook:
	for I in $(RDEV_LINKS); do \
		rm -f $(DESTDIR)$(usrbin_execdir)/$$I ; \
	done
	for I in $(SETARCH_LINKS); do \
		rm -f $(DESTDIR)$(usrbin_execdir)/$$I ; \
	done

