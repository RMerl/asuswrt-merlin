include $(top_srcdir)/config/include-Makefile.am

bin_PROGRAMS =
sbin_PROGRAMS =
usrbin_exec_PROGRAMS = flock ipcrm ipcs ipcmk renice setsid
usrsbin_exec_PROGRAMS = readprofile

dist_man_MANS = flock.1 ipcrm.1 ipcs.1 ipcmk.1 renice.1 setsid.1 \
		readprofile.1

if LINUX
bin_PROGRAMS += dmesg
sbin_PROGRAMS += ctrlaltdel fsfreeze fstrim
usrbin_exec_PROGRAMS += cytune setarch
usrsbin_exec_PROGRAMS += ldattach tunelp rtcwake

dist_man_MANS += dmesg.1 ctrlaltdel.8 cytune.8 setarch.8 \
		ldattach.8 tunelp.8 rtcwake.8 fsfreeze.8 fstrim.8

if HAVE_CPU_SET_T
usrbin_exec_PROGRAMS += lscpu
lscpu_SOURCES = lscpu.c $(top_srcdir)/lib/cpuset.c $(top_srcdir)/lib/strutils.c
dist_man_MANS += lscpu.1
endif

endif

cytune_SOURCES = cytune.c cyclades.h
tunelp_SOURCES = tunelp.c lp.h
fstrim_SOURCES = fstrim.c $(top_srcdir)/lib/strutils.c
rtcwake_SOURCES = rtcwake.c $(top_srcdir)/lib/strutils.c
dmesg_SOURCES = dmesg.c $(top_srcdir)/lib/strutils.c

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

