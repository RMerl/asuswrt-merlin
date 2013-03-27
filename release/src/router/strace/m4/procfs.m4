dnl ### A macro to determine if we have a "MP" type procfs
AC_DEFUN([AC_MP_PROCFS],
[AC_MSG_CHECKING(for MP procfs)
AC_CACHE_VAL(ac_cv_mp_procfs,
[AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <signal.h>
#include <sys/procfs.h>

main()
{
	int pid;
	char proc[32];
	FILE *ctl;
	FILE *status;
	int cmd;
	struct pstatus pstatus;

	if ((pid = fork()) == 0) {
		pause();
		exit(0);
	}
	sprintf(proc, "/proc/%d/ctl", pid);
	if ((ctl = fopen(proc, "w")) == NULL)
		goto fail;
	sprintf(proc, "/proc/%d/status", pid);
	if ((status = fopen (proc, "r")) == NULL)
		goto fail;
	cmd = PCSTOP;
	if (write (fileno (ctl), &cmd, sizeof cmd) < 0)
		goto fail;
	if (read (fileno (status), &pstatus, sizeof pstatus) < 0)
		goto fail;
	kill(pid, SIGKILL);
	exit(0);
fail:
	kill(pid, SIGKILL);
	exit(1);
}
]])],[ac_cv_mp_procfs=yes],[ac_cv_mp_procfs=no],[
# Guess or punt.
case "$host_os" in
svr4.2*|svr5*)
	ac_cv_mp_procfs=yes
	;;
*)
	ac_cv_mp_procfs=no
	;;
esac
])])
AC_MSG_RESULT($ac_cv_mp_procfs)
if test "$ac_cv_mp_procfs" = yes
then
	AC_DEFINE([HAVE_MP_PROCFS], 1,
[Define if you have a SVR4 MP type procfs.
I.E. /dev/xxx/ctl, /dev/xxx/status.
Also implies that you have the pr_lwp member in prstatus.])
fi
])

dnl ### A macro to determine if procfs is pollable.
AC_DEFUN([AC_POLLABLE_PROCFS],
[AC_MSG_CHECKING(for pollable procfs)
AC_CACHE_VAL(ac_cv_pollable_procfs,
[AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <signal.h>
#include <sys/procfs.h>
#include <sys/stropts.h>
#include <poll.h>

#ifdef HAVE_MP_PROCFS
#define PIOCSTOP	PCSTOP
#define POLLWANT	POLLWRNORM
#define PROC		"/proc/%d/ctl"
#define PROC_MODE	"w"
int IOCTL (int fd, int cmd, int arg) {
	return write (fd, &cmd, sizeof cmd);
}
#else
#define POLLWANT	POLLPRI
#define	PROC		"/proc/%d"
#define PROC_MODE	"r+"
#define IOCTL		ioctl
#endif

main()
{
	int pid;
	char proc[32];
	FILE *pfp;
	struct pollfd pfd;

	if ((pid = fork()) == 0) {
		pause();
		exit(0);
	}
	sprintf(proc, PROC, pid);
	if ((pfp = fopen(proc, PROC_MODE)) == NULL)
		goto fail;
	if (IOCTL(fileno(pfp), PIOCSTOP, NULL) < 0)
		goto fail;
	pfd.fd = fileno(pfp);
	pfd.events = POLLWANT;
	if (poll(&pfd, 1, 0) < 0)
		goto fail;
	if (!(pfd.revents & POLLWANT))
		goto fail;
	kill(pid, SIGKILL);
	exit(0);
fail:
	kill(pid, SIGKILL);
	exit(1);
}
]])],[ac_cv_pollable_procfs=yes],[ac_cv_pollable_procfs=no],[
# Guess or punt.
case "$host_os" in
solaris2*|irix5*|svr4.2uw*|svr5*)
	ac_cv_pollable_procfs=yes
	;;
*)
	ac_cv_pollable_procfs=no
	;;
esac
])])
AC_MSG_RESULT($ac_cv_pollable_procfs)
if test "$ac_cv_pollable_procfs" = yes
then
	AC_DEFINE([HAVE_POLLABLE_PROCFS], 1,
[Define if you have SVR4 and the poll system call works on /proc files.])
fi
])

dnl ### A macro to determine if the prstatus structure has a pr_syscall member.
AC_DEFUN([AC_STRUCT_PR_SYSCALL],
[AC_MSG_CHECKING(for pr_syscall in struct prstatus)
AC_CACHE_VAL(ac_cv_struct_pr_syscall,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/procfs.h>]], [[#ifdef HAVE_MP_PROCFS
pstatus_t s;
s.pr_lwp.pr_syscall
#else
prstatus_t s;
s.pr_syscall
#endif]])],[ac_cv_struct_pr_syscall=yes],[ac_cv_struct_pr_syscall=no])])
AC_MSG_RESULT($ac_cv_struct_pr_syscall)
if test "$ac_cv_struct_pr_syscall" = yes
then
	AC_DEFINE([HAVE_PR_SYSCALL], 1,
[Define if the prstatus structure in sys/procfs.h has a pr_syscall member.])
fi
])
