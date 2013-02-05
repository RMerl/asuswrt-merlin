dnl *********** BSD vs. POSIX signal handling **************
AC_DEFUN([AC_BSD_SIGNALS], [
  AC_MSG_CHECKING(for BSD signal semantics)
  AC_CACHE_VAL(knfsd_cv_bsd_signals,
    [AC_TRY_RUN([
	#include <signal.h>
	#include <unistd.h>
	#include <sys/wait.h>

	static int counter = 0;
	static RETSIGTYPE handler(int num) { counter++; }

	int main()
	{
		int	s;
		if ((s = fork()) < 0) return 1;
		if (s != 0) {
			if (wait(&s) < 0) return 1;
			return WIFSIGNALED(s)? 1 : 0;
		}

		signal(SIGHUP, handler);
		kill(getpid(), SIGHUP); kill(getpid(), SIGHUP);
		return (counter == 2)? 0 : 1;
	}
    ], knfsd_cv_bsd_signals=yes, knfsd_cv_bsd_signals=no,
    [
      case "$host_os" in
        *linux*) knfsd_cv_bsd_signals=no;;
        *bsd*)   knfsd_cv_bsd_signals=yes;;
        *)       AC_MSG_ERROR([unable to guess signal semantics for $host_os; please set knfsd_cv_bsd_signals]);;
      esac
    ])]) dnl
    AC_MSG_RESULT($knfsd_cv_bsd_signals)
    test $knfsd_cv_bsd_signals = yes && AC_DEFINE(HAVE_BSD_SIGNALS, 1, [Define this if you want to use BSD signal semantics])
])dnl
