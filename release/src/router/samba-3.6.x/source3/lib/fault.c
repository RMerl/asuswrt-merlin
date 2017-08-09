/* 
   Unix SMB/CIFS implementation.
   Critical Fault handling
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Tim Prouty 2009
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif


#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

static void (*cont_fn)(void *);
static char *corepath;

/*******************************************************************
report a fault
********************************************************************/
static void fault_report(int sig)
{
	static int counter;

	if (counter) _exit(1);

	counter++;

	DEBUGSEP(0);
	DEBUG(0,("INTERNAL ERROR: Signal %d in pid %d (%s)",sig,(int)sys_getpid(),samba_version_string()));
	DEBUG(0,("\nPlease read the Trouble-Shooting section of the Samba3-HOWTO\n"));
	DEBUG(0,("\nFrom: http://www.samba.org/samba/docs/Samba3-HOWTO.pdf\n"));
	DEBUGSEP(0);
  
	smb_panic("internal error");

	if (cont_fn) {
		cont_fn(NULL);
#ifdef SIGSEGV
		CatchSignal(SIGSEGV, SIG_DFL);
#endif
#ifdef SIGBUS
		CatchSignal(SIGBUS, SIG_DFL);
#endif
#ifdef SIGABRT
		CatchSignal(SIGABRT, SIG_DFL);
#endif
		return; /* this should cause a core dump */
	}
	exit(1);
}

/****************************************************************************
catch serious errors
****************************************************************************/
static void sig_fault(int sig)
{
	fault_report(sig);
}

/*******************************************************************
setup our fault handlers
********************************************************************/
void fault_setup(void (*fn)(void *))
{
	cont_fn = fn;

#ifdef SIGSEGV
	CatchSignal(SIGSEGV, sig_fault);
#endif
#ifdef SIGBUS
	CatchSignal(SIGBUS, sig_fault);
#endif
#ifdef SIGABRT
	CatchSignal(SIGABRT, sig_fault);
#endif
}

/**
 * Build up the default corepath as "<logbase>/cores/<progname>"
 */
static char *get_default_corepath(const char *logbase, const char *progname)
{
	char *tmp_corepath;

	/* Setup core dir in logbase. */
	tmp_corepath = talloc_asprintf(NULL, "%s/cores", logbase);
	if (!tmp_corepath)
		return NULL;

	if ((mkdir(tmp_corepath, 0700) == -1) && errno != EEXIST)
		goto err_out;

	if (chmod(tmp_corepath, 0700) == -1)
		goto err_out;

	talloc_free(tmp_corepath);

	/* Setup progname-specific core subdir */
	tmp_corepath = talloc_asprintf(NULL, "%s/cores/%s", logbase, progname);
	if (!tmp_corepath)
		return NULL;

	if (mkdir(tmp_corepath, 0700) == -1 && errno != EEXIST)
		goto err_out;

	if (chown(tmp_corepath, getuid(), getgid()) == -1)
		goto err_out;

	if (chmod(tmp_corepath, 0700) == -1)
		goto err_out;

	return tmp_corepath;

 err_out:
	talloc_free(tmp_corepath);
	return NULL;
}

/**
 * Get the FreeBSD corepath.
 *
 * On FreeBSD the current working directory is ignored when creating a core
 * file.  Instead the core directory is controlled via sysctl.  This consults
 * the value of "kern.corefile" so the correct corepath can be printed out
 * before dump_core() calls abort.
 */
#if (defined(FREEBSD) && defined(HAVE_SYSCTLBYNAME))
static char *get_freebsd_corepath(void)
{
	char *tmp_corepath = NULL;
	char *end = NULL;
	size_t len = 128;
	int ret;

	/* Loop with increasing sizes so we don't allocate too much. */
	do {
		if (len > 1024)  {
			goto err_out;
		}

		tmp_corepath = (char *)talloc_realloc(NULL, tmp_corepath,
						      char, len);
		if (!tmp_corepath) {
			return NULL;
		}

		ret = sysctlbyname("kern.corefile", tmp_corepath, &len, NULL,
				   0);
		if (ret == -1) {
			if (errno != ENOMEM) {
				DEBUG(0, ("sysctlbyname failed getting "
					  "kern.corefile %s\n",
					  strerror(errno)));
				goto err_out;
			}

			/* Not a large enough array, try a bigger one. */
			len = len << 1;
		}
	} while (ret == -1);

	/* Strip off the common filename expansion */
	if ((end = strrchr_m(tmp_corepath, '/'))) {
		*end = '\0';
	}

	return tmp_corepath;

 err_out:
	if (tmp_corepath) {
		talloc_free(tmp_corepath);
	}
	return NULL;
}
#endif

#if defined(HAVE_SYS_KERNEL_PROC_CORE_PATTERN)

/**
 * Get the Linux corepath.
 *
 * On Linux the contents of /proc/sys/kernel/core_pattern indicates the
 * location of the core path.
 */
static char *get_linux_corepath(void)
{
	char *end;
	int fd;
	char *result;

	fd = open("/proc/sys/kernel/core_pattern", O_RDONLY, 0);
	if (fd == -1) {
		return NULL;
	}

	result = afdgets(fd, NULL, 0);
	close(fd);

	if (result == NULL) {
		return NULL;
	}

	if (result[0] != '/') {
		/*
		 * No absolute path, use the default (cwd)
		 */
		TALLOC_FREE(result);
		return NULL;
	}
	/* Strip off the common filename expansion */

	end = strrchr_m(result, '/');

	if ((end != result) /* this would be the only / */
	    && (end != NULL)) {
		*end = '\0';
	}
	return result;
}
#endif


/**
 * Try getting system-specific corepath if one exists.
 *
 * If the system doesn't define a corepath, then the default is used.
 */
static char *get_corepath(const char *logbase, const char *progname)
{
#if (defined(FREEBSD) && defined(HAVE_SYSCTLBYNAME))
	char *tmp_corepath = NULL;
	tmp_corepath = get_freebsd_corepath();

	/* If this has been set correctly, we're done. */
	if (tmp_corepath) {
		return tmp_corepath;
	}
#endif

#if defined(HAVE_SYS_KERNEL_PROC_CORE_PATTERN)
	char *tmp_corepath = NULL;
	tmp_corepath = get_linux_corepath();

	/* If this has been set correctly, we're done. */
	if (tmp_corepath) {
		return tmp_corepath;
	}
#endif

	/* Fall back to the default. */
	return get_default_corepath(logbase, progname);
}

/*******************************************************************
make all the preparations to safely dump a core file
********************************************************************/

void dump_core_setup(const char *progname)
{
	char *logbase = NULL;
	char *end = NULL;

	if (lp_logfile() && *lp_logfile()) {
		if (asprintf(&logbase, "%s", lp_logfile()) < 0) {
			return;
		}
		if ((end = strrchr_m(logbase, '/'))) {
			*end = '\0';
		}
	} else {
		/* We will end up here if the log file is given on the command
		 * line by the -l option but the "log file" option is not set
		 * in smb.conf.
		 */
		if (asprintf(&logbase, "%s", get_dyn_LOGFILEBASE()) < 0) {
			return;
		}
	}

	SMB_ASSERT(progname != NULL);

	corepath = get_corepath(logbase, progname);
	if (!corepath) {
		DEBUG(0, ("Unable to setup corepath for %s: %s\n", progname,
			  strerror(errno)));
		goto out;
	}


#ifdef HAVE_GETRLIMIT
#ifdef RLIMIT_CORE
	{
		struct rlimit rlp;
		getrlimit(RLIMIT_CORE, &rlp);
		rlp.rlim_cur = MAX(16*1024*1024,rlp.rlim_cur);
		setrlimit(RLIMIT_CORE, &rlp);
		getrlimit(RLIMIT_CORE, &rlp);
		DEBUG(3,("Maximum core file size limits now %d(soft) %d(hard)\n",
			 (int)rlp.rlim_cur,(int)rlp.rlim_max));
	}
#endif
#endif

	/* FIXME: if we have a core-plus-pid facility, configurably set
	 * this up here.
	 */
 out:
	SAFE_FREE(logbase);
}

 void dump_core(void)
{
	static bool called;

	if (called) {
		DEBUG(0, ("dump_core() called recursive\n"));
		exit(1);
	}
	called = true;

	/* Note that even if core dumping has been disabled, we still set up
	 * the core path. This is to handle the case where core dumping is
	 * turned on in smb.conf and the relevant daemon is not restarted.
	 */
	if (!lp_enable_core_files()) {
		DEBUG(0, ("Exiting on internal error (core file administratively disabled)\n"));
		exit(1);
	}

#if DUMP_CORE
	/* If we're running as non root we might not be able to dump the core
	 * file to the corepath.  There must not be an unbecome_root() before
	 * we call abort(). */
	if (geteuid() != sec_initial_uid()) {
		become_root();
	}

	if (corepath == NULL) {
		DEBUG(0, ("Can not dump core: corepath not set up\n"));
		exit(1);
	}

	if (*corepath != '\0') {
		/* The chdir might fail if we dump core before we finish
		 * processing the config file.
		 */
		if (chdir(corepath) != 0) {
			DEBUG(0, ("unable to change to %s\n", corepath));
			DEBUGADD(0, ("refusing to dump core\n"));
			exit(1);
		}

		DEBUG(0,("dumping core in %s\n", corepath));
	}

	umask(~(0700));
	dbgflush();

#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE)
	/* On Linux we lose the ability to dump core when we change our user
	 * ID. We know how to dump core safely, so let's make sure we have our
	 * dumpable flag set.
	 */
	prctl(PR_SET_DUMPABLE, 1);
#endif

	/* Ensure we don't have a signal handler for abort. */
#ifdef SIGABRT
	CatchSignal(SIGABRT, SIG_DFL);
#endif

	abort();

#else /* DUMP_CORE */
	exit(1);
#endif /* DUMP_CORE */
}

