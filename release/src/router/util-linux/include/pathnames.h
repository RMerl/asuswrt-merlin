/*
 * Vaguely based on
 *	@(#)pathnames.h	5.3 (Berkeley) 5/9/89
 * This code is in the public domain.
 */
#ifndef PATHNAMES_H
#define PATHNAMES_H

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifndef __STDC__
# error "we need an ANSI compiler"
#endif

/* used by kernel in /proc (e.g. /proc/swaps) for deleted files */
#define PATH_DELETED_SUFFIX	"\\040(deleted)"
#define PATH_DELETED_SUFFIX_SZ	(sizeof(PATH_DELETED_SUFFIX) - 1)

/* DEFPATHs from <paths.h> don't include /usr/local */
#undef _PATH_DEFPATH
#define	_PATH_DEFPATH	        "/usr/local/bin:/bin:/usr/bin"

#undef _PATH_DEFPATH_ROOT
#define	_PATH_DEFPATH_ROOT	"/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin"

#define _PATH_SECURETTY		"/etc/securetty"
#define _PATH_WTMPLOCK		"/etc/wtmplock"

#define	_PATH_HUSHLOGIN		".hushlogin"
#define	_PATH_HUSHLOGINS	"/etc/hushlogins"

#ifndef _PATH_MAILDIR
#define	_PATH_MAILDIR		"/var/spool/mail"
#endif
#define	_PATH_MOTDFILE		"/etc/motd"
#define	_PATH_NOLOGIN		"/etc/nologin"

#define _PATH_LOGIN		"/bin/login"
#define _PATH_INITTAB		"/etc/inittab"
#define _PATH_RC		"/etc/rc"
#define _PATH_REBOOT		"/sbin/reboot"
#define _PATH_SHUTDOWN		"/sbin/shutdown"
#define _PATH_SINGLE		"/etc/singleboot"
#define _PATH_SHUTDOWN_CONF	"/etc/shutdown.conf"

#define _PATH_SECURE		"/etc/securesingle"
#define _PATH_USERTTY           "/etc/usertty"

/* used in login-utils/shutdown.c */

/* used in login-utils/setpwnam.h and login-utils/islocal.c */
#define _PATH_PASSWD            "/etc/passwd"

/* used in login-utils/newgrp */
#define _PATH_GSHADOW		"/etc/gshadow"

/* used in login-utils/setpwnam.h */
#define _PATH_PTMP              "/etc/ptmp"
#define _PATH_PTMPTMP           "/etc/ptmptmp"
#define _PATH_GROUP             "/etc/group"
#define _PATH_GTMP              "/etc/gtmp"
#define _PATH_GTMPTMP           "/etc/gtmptmp"
#define _PATH_SHADOW_PASSWD     "/etc/shadow"
#define _PATH_SHADOW_PTMP       "/etc/sptmp"
#define _PATH_SHADOW_PTMPTMP    "/etc/sptmptmp"
#define _PATH_SHADOW_GROUP      "/etc/gshadow"
#define _PATH_SHADOW_GTMP       "/etc/sgtmp"
#define _PATH_SHADOW_GTMPTMP    "/etc/sgtmptmp"

/* used in term-utils/agetty.c */
#define _PATH_ISSUE		"/etc/issue"

#define _PATH_LOGINDEFS		"/etc/login.defs"

/* used in misc-utils/look.c */
#define _PATH_WORDS             "/usr/share/dict/words"
#define _PATH_WORDS_ALT         "/usr/share/dict/web2"

/* mount paths */
#define _PATH_UMOUNT		"/bin/umount"

#define _PATH_FILESYSTEMS	"/etc/filesystems"
#define _PATH_PROC_SWAPS	"/proc/swaps"
#define _PATH_PROC_FILESYSTEMS	"/proc/filesystems"
#define _PATH_PROC_MOUNTS	"/proc/mounts"
#define _PATH_PROC_PARTITIONS	"/proc/partitions"
#define _PATH_PROC_DEVICES	"/proc/devices"
#define _PATH_PROC_MOUNTINFO	"/proc/self/mountinfo"

#define _PATH_SYS_BLOCK		"/sys/block"
#define _PATH_SYS_DEVBLOCK	"/sys/dev/block"

#ifndef _PATH_MOUNTED
# ifdef MOUNTED					/* deprecated */
#  define _PATH_MOUNTED		MOUNTED
# else
#  define _PATH_MOUNTED		"/etc/mtab"
# endif
#endif

#ifndef _PATH_MNTTAB
# ifdef MNTTAB					/* deprecated */
#  define _PATH_MNTTAB		MNTTAB
# else
#  define _PATH_MNTTAB		"/etc/fstab"
# endif
#endif

#define _PATH_MNTTAB_DIR	_PATH_MNTTAB ".d"

#define _PATH_MOUNTED_LOCK	_PATH_MOUNTED "~"
#define _PATH_MOUNTED_TMP	_PATH_MOUNTED ".tmp"

#ifndef _PATH_DEV
  /*
   * The tailing '/' in _PATH_DEV is there for compatibility with libc.
   */
# define _PATH_DEV		"/dev/"
#endif

#define _PATH_DEV_LOOP		"/dev/loop"
#define _PATH_DEV_LOOPCTL	"/dev/loop-control"
#define _PATH_DEV_TTY		"/dev/tty"


/* udev paths */
#define _PATH_DEV_BYLABEL	"/dev/disk/by-label"
#define _PATH_DEV_BYUUID	"/dev/disk/by-uuid"
#define _PATH_DEV_BYID		"/dev/disk/by-id"
#define _PATH_DEV_BYPATH	"/dev/disk/by-path"

/* hwclock paths */
#define _PATH_ADJPATH		"/etc/adjtime"
#define _PATH_LASTDATE		"/var/lib/lastdate"
#ifdef __ia64__
# define _PATH_RTC_DEV		"/dev/efirtc"
#else
# define _PATH_RTC_DEV		"/dev/rtc"
#endif

#ifndef _PATH_BTMP
#define _PATH_BTMP		"/var/log/btmp"
#endif

#endif /* PATHNAMES_H */

