/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Updated Thu Oct 12 09:56:55 1995 by faith@cs.unc.edu with security
 * patches from Zefram <A.Main@dcs.warwick.ac.uk>
 *
 * Updated Thu Nov  9 21:58:53 1995 by Martin Schulze
 * <joey@finlandia.infodrom.north.de>.  Support for vigr.
 *
 * Martin Schulze's patches adapted to Util-Linux by Nicolai Langfeldt.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - fixed strerr(errno) in gettext calls
 */

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "setpwnam.h"
#include "strutils.h"
#include "xalloc.h"

#ifdef HAVE_LIBSELINUX
# include <selinux/selinux.h>
#endif

#define FILENAMELEN 67

enum {
	VIPW,
	VIGR
};
int program;
char orig_file[FILENAMELEN];	/* original file /etc/passwd or /etc/group */
char tmp_file[FILENAMELEN];	/* tmp file */
char tmptmp_file[FILENAMELEN];	/* very tmp file */

void pw_error __P((char *, int, int));

static void copyfile(int from, int to)
{
	int nr, nw, off;
	char buf[8 * 1024];

	while ((nr = read(from, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; nr -= nw, off += nw)
			if ((nw = write(to, buf + off, nr)) < 0)
				pw_error(tmp_file, 1, 1);

	if (nr < 0)
		pw_error(orig_file, 1, 1);
}

static void pw_init(void)
{
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU, &rlim);
	(void)setrlimit(RLIMIT_FSIZE, &rlim);
	(void)setrlimit(RLIMIT_STACK, &rlim);
	(void)setrlimit(RLIMIT_DATA, &rlim);
	(void)setrlimit(RLIMIT_RSS, &rlim);

	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rlim);

	/* Turn off signals. */
	(void)signal(SIGALRM, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);
	(void)signal(SIGTTOU, SIG_IGN);

	/* Create with exact permissions. */
	(void)umask(0);
}

static int pw_lock(void)
{
	int lockfd, fd, ret;

	/*
	 * If the password file doesn't exist, the system is hosed.  Might as
	 * well try to build one.  Set the close-on-exec bit so that users
	 * can't get at the encrypted passwords while editing.  Open should
	 * allow flock'ing the file; see 4.4BSD.  XXX
	 */
#if 0
	/* flock()ing is superfluous here, with the ptmp/ptmptmp system. */
	if (flock(lockfd, LOCK_EX | LOCK_NB)) {
		if (program == VIPW)
			err(EXIT_FAILURE, _("cannot lock password file"));
		else
			err(EXIT_FAILURE, _("cannot lock group file"));
	}
#endif

	if ((fd = open(tmptmp_file, O_WRONLY | O_CREAT, 0600)) == -1)
		err(EXIT_FAILURE, _("%s: open failed"), tmptmp_file);

	ret = link(tmptmp_file, tmp_file);
	(void)unlink(tmptmp_file);
	if (ret == -1) {
		if (errno == EEXIST)
			errx(EXIT_FAILURE,
			     _("the %s file is busy (%s present)"),
			     program == VIPW ? "password" : "group", tmp_file);
		else
			err(EXIT_FAILURE, _("can't link %s"), tmp_file);
	}

	lockfd = open(orig_file, O_RDONLY, 0);

	if (lockfd < 0) {
		warn("%s", orig_file);
		unlink(tmp_file);
		exit(EXIT_FAILURE);
	}

	copyfile(lockfd, fd);
	(void)close(lockfd);
	(void)close(fd);
	return 1;
}

static void pw_unlock(void)
{
	char tmp[FILENAMELEN + 4];

	sprintf(tmp, "%s%s", orig_file, ".OLD");
	unlink(tmp);

	if (link(orig_file, tmp))
		warn(_("%s: create a link to %s failed"), orig_file, tmp);

#ifdef HAVE_LIBSELINUX
	if (is_selinux_enabled() > 0) {
		security_context_t passwd_context = NULL;
		int ret = 0;
		if (getfilecon(orig_file, &passwd_context) < 0) {
			warnx(_("Can't get context for %s"), orig_file);
			pw_error(orig_file, 1, 1);
		}
		ret = setfilecon(tmp_file, passwd_context);
		freecon(passwd_context);
		if (ret != 0) {
			warnx(_("Can't set context for %s"), tmp_file);
			pw_error(tmp_file, 1, 1);
		}
	}
#endif

	if (rename(tmp_file, orig_file) == -1) {
		int errsv = errno;
		errx(EXIT_FAILURE,
		     ("can't unlock %s: %s (your changes are still in %s)"),
		     orig_file, strerror(errsv), tmp_file);
	}
	unlink(tmp_file);
}

static void pw_edit(int notsetuid)
{
	int pstat;
	pid_t pid;
	char *p, *editor, *tk;

	editor = getenv("EDITOR");
	editor = xstrdup(editor ? editor : _PATH_VI);

	tk = strtok(editor, " \t");
	if (tk && (p = strrchr(tk, '/')) != NULL)
		++p;
	else
		p = editor;

	pid = fork();
	if (pid < 0)
		err(EXIT_FAILURE, _("fork failed"));

	if (!pid) {
		if (notsetuid) {
			(void)setgid(getgid());
			(void)setuid(getuid());
		}
		execlp(editor, p, tmp_file, NULL);
		/* Shouldn't get here */
		_exit(EXIT_FAILURE);
	}
	for (;;) {
		pid = waitpid(pid, &pstat, WUNTRACED);
		if (WIFSTOPPED(pstat)) {
			/* the editor suspended, so suspend us as well */
			kill(getpid(), SIGSTOP);
			kill(pid, SIGCONT);
		} else {
			break;
		}
	}
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		pw_error(editor, 1, 1);

	free(editor);
}

void pw_error(char *name, int err, int eval)
{
	if (err) {
		if (name)
			warn("%s: ", name);
		else
			warn(NULL);
	}
	warnx(_("%s unchanged"), orig_file);
	unlink(tmp_file);
	exit(eval);
}

static void edit_file(int is_shadow)
{
	struct stat begin, end;

	pw_init();
	pw_lock();

	if (stat(tmp_file, &begin))
		pw_error(tmp_file, 1, 1);

	pw_edit(0);

	if (stat(tmp_file, &end))
		pw_error(tmp_file, 1, 1);
	if (begin.st_mtime == end.st_mtime) {
		warnx(_("no changes made"));
		pw_error((char *)NULL, 0, 0);
	}
	/* see pw_lock() where we create the file with mode 600 */
	if (!is_shadow)
		chmod(tmp_file, 0644);
	else
		chmod(tmp_file, 0400);
	pw_unlock();
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	memset(tmp_file, '\0', FILENAMELEN);
	if (!strcmp(program_invocation_short_name, "vigr")) {
		program = VIGR;
		xstrncpy(orig_file, GROUP_FILE, sizeof(orig_file));
		xstrncpy(tmp_file, GTMP_FILE, sizeof(tmp_file));
		xstrncpy(tmptmp_file, GTMPTMP_FILE, sizeof(tmptmp_file));
	} else {
		program = VIPW;
		xstrncpy(orig_file, PASSWD_FILE, sizeof(orig_file));
		xstrncpy(tmp_file, PTMP_FILE, sizeof(tmp_file));
		xstrncpy(tmptmp_file, PTMPTMP_FILE, sizeof(tmptmp_file));
	}

	if ((argc > 1) &&
	    (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version"))) {
		printf(UTIL_LINUX_VERSION);
		exit(EXIT_SUCCESS);
	}

	edit_file(0);

	if (program == VIGR) {
		strncpy(orig_file, SGROUP_FILE, FILENAMELEN - 1);
		strncpy(tmp_file, SGTMP_FILE, FILENAMELEN - 1);
		strncpy(tmptmp_file, SGTMPTMP_FILE, FILENAMELEN - 1);
	} else {
		strncpy(orig_file, SHADOW_FILE, FILENAMELEN - 1);
		strncpy(tmp_file, SPTMP_FILE, FILENAMELEN - 1);
		strncpy(tmptmp_file, SPTMPTMP_FILE, FILENAMELEN - 1);
	}

	if (access(orig_file, F_OK) == 0) {
		char response[80];

		printf((program == VIGR)
		       ? _("You are using shadow groups on this system.\n")
		       : _("You are using shadow passwords on this system.\n"));
		printf(_("Would you like to edit %s now [y/n]? "), orig_file);

		/* EOF means no */
		if (fgets(response, sizeof(response), stdin)) {
			if (response[0] == 'y' || response[0] == 'Y')
				edit_file(1);
		}
	}
	exit(EXIT_SUCCESS);
}
