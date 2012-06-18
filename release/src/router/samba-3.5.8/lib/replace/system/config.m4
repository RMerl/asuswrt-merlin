# filesys
AC_HEADER_DIRENT 
AC_CHECK_HEADERS(fcntl.h sys/fcntl.h sys/resource.h sys/ioctl.h sys/mode.h sys/filio.h sys/fs/s5param.h sys/filsys.h)
AC_CHECK_HEADERS(sys/acl.h acl/libacl.h)

# select
AC_CHECK_HEADERS(sys/select.h)

# time
AC_CHECK_HEADERS(sys/time.h utime.h)
AC_HEADER_TIME
AC_CHECK_FUNCS(utime utimes)

# wait
AC_HEADER_SYS_WAIT

# capability
AC_CHECK_HEADERS(sys/capability.h)

case "$host_os" in
*linux*)
AC_CACHE_CHECK([for broken RedHat 7.2 system header files],libreplace_cv_BROKEN_REDHAT_7_SYSTEM_HEADERS,[
AC_TRY_COMPILE([
	#ifdef HAVE_SYS_VFS_H
	#include <sys/vfs.h>
	#endif
	#ifdef HAVE_SYS_CAPABILITY_H
	#include <sys/capability.h>
	#endif
	],[
	int i;
	],
	libreplace_cv_BROKEN_REDHAT_7_SYSTEM_HEADERS=no,
	libreplace_cv_BROKEN_REDHAT_7_SYSTEM_HEADERS=yes
)])
if test x"$libreplace_cv_BROKEN_REDHAT_7_SYSTEM_HEADERS" = x"yes"; then
	AC_DEFINE(BROKEN_REDHAT_7_SYSTEM_HEADERS,1,[Broken RedHat 7.2 system header files])
fi

AC_CACHE_CHECK([for broken RHEL5 sys/capability.h],libreplace_cv_BROKEN_RHEL5_SYS_CAP_HEADER,[
AC_TRY_COMPILE([
	#ifdef HAVE_SYS_CAPABILITY_H
	#include <sys/capability.h>
	#endif
	#include <linux/types.h>
	],[
	__s8 i;
	],
	libreplace_cv_BROKEN_RHEL5_SYS_CAP_HEADER=no,
	libreplace_cv_BROKEN_RHEL5_SYS_CAP_HEADER=yes
)])
if test x"$libreplace_cv_BROKEN_RHEL5_SYS_CAP_HEADER" = x"yes"; then
	AC_DEFINE(BROKEN_RHEL5_SYS_CAP_HEADER,1,[Broken RHEL5 sys/capability.h])
fi
;;
esac

# passwd
AC_CHECK_HEADERS(grp.h sys/id.h compat.h shadow.h sys/priv.h pwd.h sys/security.h)
AC_CHECK_FUNCS(getpwnam_r getpwuid_r getpwent_r)
AC_HAVE_DECL(getpwent_r, [
	#include <unistd.h>
	#include <pwd.h>
	])
AC_VERIFY_C_PROTOTYPE([struct passwd *getpwent_r(struct passwd *src, char *buf, int buflen)],
	[
	#ifndef HAVE_GETPWENT_R_DECL
	#error missing getpwent_r prototype
	#endif
	return NULL;
	],[
	AC_DEFINE(SOLARIS_GETPWENT_R, 1, [getpwent_r solaris function prototype])
	],[],[
	#include <unistd.h>
	#include <pwd.h>
	])
AC_VERIFY_C_PROTOTYPE([struct passwd *getpwent_r(struct passwd *src, char *buf, size_t buflen)],
	[
	#ifndef HAVE_GETPWENT_R_DECL
	#error missing getpwent_r prototype
	#endif
	return NULL;
	],[
	AC_DEFINE(SOLARIS_GETPWENT_R, 1, [getpwent_r irix (similar to solaris) function prototype])
	],[],[
	#include <unistd.h>
	#include <pwd.h>
	])
AC_CHECK_FUNCS(getgrnam_r getgrgid_r getgrent_r)
AC_HAVE_DECL(getgrent_r, [
	#include <unistd.h>
	#include <grp.h>
	])
AC_VERIFY_C_PROTOTYPE([struct group *getgrent_r(struct group *src, char *buf, int buflen)],
	[
	#ifndef HAVE_GETGRENT_R_DECL
	#error missing getgrent_r prototype
	#endif
	return NULL;
	],[
	AC_DEFINE(SOLARIS_GETGRENT_R, 1, [getgrent_r solaris function prototype])
	],[],[
	#include <unistd.h>
	#include <grp.h>
	])

AC_VERIFY_C_PROTOTYPE([struct group *getgrent_r(struct group *src, char *buf, size_t buflen)],
	[
	#ifndef HAVE_GETGRENT_R_DECL
	#error missing getgrent_r prototype
	#endif
	return NULL;
	],[
	AC_DEFINE(SOLARIS_GETGRENT_R, 1, [getgrent_r irix (similar to solaris)  function prototype])
	],[],[
	#include <unistd.h>
	#include <grp.h>
	])
AC_CHECK_FUNCS(getgrouplist)

# locale
AC_CHECK_HEADERS(ctype.h locale.h langinfo.h)

# glob
AC_CHECK_HEADERS(fnmatch.h)

# shmem
AC_CHECK_HEADERS(sys/ipc.h sys/mman.h sys/shm.h )

# terminal
AC_CHECK_HEADERS(termios.h termio.h sys/termio.h )
