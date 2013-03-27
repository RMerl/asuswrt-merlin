dnl Process this file with autoconf to create configure.  Use autoreconf.
AC_PREREQ(2.57)
AC_INIT([strace],[4.6])
AC_CONFIG_SRCDIR([strace.c])
AC_CONFIG_AUX_DIR([.])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([foreign check-news dist-xz no-dist-gzip silent-rules])
AM_MAINTAINER_MODE
AC_CANONICAL_HOST

AC_MSG_CHECKING([for supported operating system])
case "$host_os" in
*linux*)
	opsys=linux
	AC_DEFINE([LINUX], 1, [Define for the Linux operating system.])
	;;
sunos4*)
	opsys=sunos4
	AC_DEFINE([SUNOS4], 1, [Define for the SunOS 4.x operating system.])
	;;
solaris2* | sysv[[45]]* | irix[[56]]*)
	opsys=svr4
	AC_DEFINE([SVR4], 1, [Define for the System V release 4 operating
system or a derivative like Solaris 2.x or Irix 5.x.])
	case "$host_os" in
	sysv4.2uw*)
		AC_DEFINE(UNIXWARE, 2, [Define for UnixWare systems.])
		;;
	sysv5*)
		AC_DEFINE(UNIXWARE, 7, [Define for UnixWare systems.])
		;;
	esac
	;;
freebsd*)
	opsys=freebsd
	AC_DEFINE([FREEBSD], 1, [Define for the FreeBSD operating system.])
	;;
*)
	AC_MSG_RESULT([NO!])
	AC_MSG_ERROR([operating system $host_os is not supported by strace])
	;;
esac
AC_MSG_RESULT($opsys)

AC_MSG_CHECKING([for supported architecture])
case "$host_cpu" in
bfin)
	arch=bfin
	AC_DEFINE([BFIN], 1, [Define for the Blackfin architecture.])
	;;
i[[3456]]86|pentium)
	arch=i386
	AC_DEFINE([I386], 1, [Define for the i386 architecture.])
	;;
ia64)
	arch=ia64
	AC_DEFINE([IA64], 1, [Define for the IA64 architecture.])
	;;
m68k)
	arch=m68k
	AC_DEFINE([M68K], 1, [Define for the m68k architecture.])
	;;
sparc64*)
	arch=sparc64
	AC_DEFINE([SPARC64], 1, [Define for the SPARC64 architecture.])
	;;
sparc*)
	arch=sparc
	AC_DEFINE([SPARC], 1, [Define for the SPARC architecture.])
	;;
mips*)
	arch=mips
	AC_DEFINE([MIPS], 1, [Define for the MIPS architecture.])
	;;
alpha*)
	arch=alpha
	AC_DEFINE([ALPHA], 1, [Define for the Alpha architecture.])
	;;
powerpc*)
	arch=powerpc
	AC_DEFINE([POWERPC], 1, [Define for the PowerPC architecture.])
	if test $host_cpu = powerpc64; then
		AC_DEFINE([POWERPC64], 1, [Define for the PowerPC64 architecture.])
	fi
	;;
arm*)
	arch=arm
	AC_DEFINE([ARM], 1, [Define for the ARM architecture.])
	;;
avr32*)
	arch=avr32
	AC_DEFINE([AVR32], 1, [Define for the AVR32 architecture.])
	;;
s390)
	arch=s390
	AC_DEFINE([S390], 1, [Define for the S390 architecture.])
	;;
s390x)
	arch=s390x
	AC_DEFINE([S390X], 1, [Define for the S390x architecture.])
	;;
hppa*|parisc*)
	arch=hppa
	AC_DEFINE([HPPA], 1, [Define for the HPPA architecture.])
	;;
sh64*)
	arch=sh64
	AC_DEFINE([SH64], 1, [Define for the SH64 architecture.])
	;;
sh*)
	arch=sh
	AC_DEFINE([SH], 1, [Define for the SH architecture.])
	;;
x86?64*)
	arch=x86_64
	AC_DEFINE([X86_64], 1, [Define for the AMD x86-64 architecture.])
	;;
cris|crisv10)
	arch=crisv10
	AC_DEFINE([CRISV10], 1, [Define for the CRISv10 architecture.])
	;;
crisv32)
	arch=crisv32
	AC_DEFINE([CRISV32], 1, [Define for the CRISv32 architecture.])
	;;
tile*)
	arch=tile
	AC_DEFINE([TILE], 1, [Define for the Tile architecture])
	;;
microblaze*)
	arch=microblaze
	AC_DEFINE([MICROBLAZE], 1, [Define for the MicroBlaze architecture.])
	;;
*)
	AC_MSG_RESULT([NO!])
	AC_MSG_ERROR([architecture $host_cpu is not supported by strace])
	;;
esac
AC_MSG_RESULT($arch)

AC_SUBST(opsys)
AC_SUBST(arch)

AM_CONDITIONAL([LINUX], [test x$opsys = xlinux])
AM_CONDITIONAL([I386], [test x$arch = xi386])
AM_CONDITIONAL([X86_64], [test x$arch = xx86_64])
AM_CONDITIONAL([SUNOS4], [test x$opsys = xsunos4])
AM_CONDITIONAL([SVR4], [test x$opsys = xsvr4])
AM_CONDITIONAL([FREEBSD], [test x$opsys = xfreebsd])

AC_PROG_CC
AC_GNU_SOURCE

AC_INCLUDEDIR

if test "x$opsys" = "xsunos4" && test "x$arch" = "xsparc"
then
	AC_MSG_CHECKING(for valid machine include directory)
	if test -d "$includedir/sun4"
	then
		rm -f machine
		ln -s $includedir/sun4 machine
		AC_MSG_RESULT(yes)
		AC_DEFINE(SUNOS4_KERNEL_ARCH_KLUDGE, 1, [
Define if you are have a SPARC with SUNOS4 and your want a version
of strace that will work on sun4, sun4c and sun4m kernel architectures.
Only useful if you have a symbolic link from machine to /usr/include/sun4
in the compilation directory.])
	else
		AC_MSG_RESULT(no)
	fi
fi

gl_WARN_ADD([-Wall])
gl_WARN_ADD([-Wwrite-strings])
if test "x$opsys" = "xsunos4"
then
	# SunOS 4.x header files don't declare int functions.
	gl_WARN_ADD([-Wno-implicit])
fi
AC_SUBST([WARN_CFLAGS])

AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL
AC_C_CONST
AC_C_BIGENDIAN
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_DIRENT
AC_HEADER_STAT
AC_CHECK_MEMBERS([struct stat.st_blksize,
		  struct stat.st_blocks,
		  struct stat.st_aclcnt,
		  struct stat.st_flags,
		  struct stat.st_fstype,
		  struct stat.st_gen,
		  struct stat.st_level,
		  struct stat.st_rdev])
AC_STAT64
AC_STATFS64

AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_GETGROUPS
AC_HEADER_MAJOR
AC_CHECK_TYPES([sig_atomic_t, siginfo_t],,, [#include <signal.h>])
AC_CHECK_MEMBERS([struct sockaddr_in6.sin6_scope_id],,,
[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>])
AC_CHECK_TYPES([long long])
AC_LITTLE_ENDIAN_LONG_LONG
AC_OFF_T_IS_LONG_LONG
AC_RLIM_T_IS_LONG_LONG
AC_CHECK_TYPES([struct opthdr],,, [#include <sys/socket.h>])
AC_CHECK_TYPES([struct t_opthdr],,, [#include <sys/tiuser.h>])

if test x$opsys != xlinux; then
AC_CHECK_LIB(nsl, main)
fi

AC_CHECK_FUNCS([ \
	fork \
	getdents \
	if_indextoname \
	inet_ntop \
	mctl \
	prctl \
	pread \
	sendmsg \
	sigaction \
	strerror \
	strsignal \
	sys_siglist \
	_sys_siglist \
])
AC_CHECK_HEADERS([ \
	inttypes.h \
	ioctls.h \
	libaio.h \
	mqueue.h \
	poll.h \
	stropts.h \
	termio.h \
	sys/acl.h \
	sys/aio.h \
	sys/asynch.h \
	sys/conf.h \
	sys/door.h \
	sys/epoll.h \
	sys/filio.h \
	sys/ioctl.h \
	sys/nscsys.h \
	sys/poll.h \
	sys/ptrace.h \
	sys/reg.h \
	sys/stream.h \
	sys/sysconfig.h \
	sys/tihdr.h \
	sys/tiuser.h \
	sys/uio.h \
	sys/vfs.h \
	asm/cachectl.h \
	asm/sysmips.h \
	linux/capability.h \
	linux/ptrace.h \
	linux/utsname.h \
	netinet/sctp.h \
], [], [])
AC_CHECK_HEADERS([linux/icmp.h linux/in6.h linux/netlink.h linux/if_packet.h],
                 [], [], [#include <stddef.h>
#include <sys/socket.h>
#include <asm/types.h>])
AC_CHECK_HEADERS([asm/sigcontext.h], [], [], [#include <signal.h>])
AC_CHECK_TYPES([struct sigcontext_struct,
		struct sigcontext],,, [#include <signal.h>])
AC_CHECK_HEADERS([netinet/tcp.h netinet/udp.h],,, [#include <netinet/in.h>])

AC_MP_PROCFS
AC_POLLABLE_PROCFS

AC_CHECK_MEMBERS([struct msghdr.msg_control],,, [#include <sys/socket.h>])
AC_STRUCT_PR_SYSCALL

AC_CHECK_MEMBERS([struct T_conn_res.QUEUE_ptr,
                  struct T_conn_res.ACCEPTOR_id],,,
[#include <sys/stream.h>
#include <sys/tihdr.h>])

AC_CHECK_TYPES([struct __old_kernel_stat],,, [#include <asm/stat.h>])

AC_CHECK_TYPES([struct pt_all_user_regs, struct ia64_fpreg],,,
	       [#include <sys/ptrace.h>])

AC_CHECK_TYPES([struct user_desc],,, [#include <asm/ldt.h>])

AC_CHECK_MEMBERS([struct dqblk.dqb_curblocks],,, [#include <sys/quota.h>])

AC_CHECK_MEMBERS([struct sigcontext.sc_hi2],,, [#include <signal.h>
#ifdef HAVE_ASM_SIGCONTEXT_H
# include <asm/sigcontext.h>
#endif])


AC_CHECK_DECLS([sys_errlist])
AC_CHECK_DECLS([sys_siglist, _sys_siglist],,, [#include <signal.h>])
AC_CHECK_DECLS([\
		PTRACE_SETOPTIONS, \
		PTRACE_GETEVENTMSG, \
		PTRACE_GETSIGINFO, \
		PTRACE_O_TRACEFORK, \
		PTRACE_O_TRACEVFORK, \
		PTRACE_O_TRACECLONE, \
		PTRACE_EVENT_FORK, \
		PTRACE_EVENT_VFORK, \
		PTRACE_EVENT_CLONE],,, [#include <sys/ptrace.h>])

AC_PATH_PROG([PERL], [perl])

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT
