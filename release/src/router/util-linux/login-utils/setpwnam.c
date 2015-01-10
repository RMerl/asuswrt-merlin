/*
 *  setpwnam.c -- edit an entry in a password database.
 *
 *  (c) 1994 Salvatore Valente <svalente@mit.edu>
 *  This file is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Edited 11/10/96 (DD/MM/YY ;-) by Nicolai Langfeldt (janl@math.uio.no)
 *  to read /etc/passwd directly so that passwd, chsh and chfn can work on
 *  machines that run NIS (né YP).  Changes will not be made to usernames
 *  starting with +.
 *
 *  This file is distributed with no warranty.
 *
 *  Usage:
 *  1) get a struct passwd * from getpwnam().
 *     You should assume a struct passwd has an infinite number of fields, so
 *     you should not try to create one from scratch.
 *  2) edit the fields you want to edit.
 *  3) call setpwnam() with the edited struct passwd.
 *
 *  A _normal user_ program should never directly manipulate etc/passwd but
 *  /use getpwnam() and (family, as well as) setpwnam().
 *
 *  But, setpwnam was made to _edit_ the password file.  For use by chfn,
 *  chsh and passwd.  _I_ _HAVE_ to read and write /etc/passwd directly.  Let
 *  those who say nay be forever silent and think about how getpwnam (and
 *  family) works on a machine running YP.
 *
 *  Added checks for failure of malloc() and removed error reporting to
 *  stderr, this is a library function and should not print on the screen,
 *  but return appropriate error codes.
 *  27-Jan-97  - poe@daimi.aau.dk
 *
 *  Thanks to "two guys named Ian".
 *
 *   $Author: poer $
 *   $Revision: 1.13 $
 *   $Date: 1997/06/23 08:26:29 $
 */

#undef DEBUG

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "setpwnam.h"

static void pw_init(void);

/*
 *  setpwnam () --
 *	takes a struct passwd in which every field is filled in and valid.
 *	If the given username exists in the passwd file, the entry is
 *	replaced with the given entry.
 */
int setpwnam(struct passwd *pwd)
{
	FILE *fp = NULL, *pwf = NULL;
	int x, save_errno, fd, ret;
	int found;
	int oldumask;
	int namelen;
	int buflen = 256;
	int contlen, rc;
	char *linebuf = NULL;

	oldumask = umask(0);	/* Create with exact permissions */

	pw_init();

	/* sanity check */
	for (x = 0; x < 3; x++) {
		if (x > 0)
			sleep(1);
		fd = open(PTMPTMP_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd == -1) {
			umask(oldumask);
			return -1;
		}
		ret = link(PTMPTMP_FILE, PTMP_FILE);
		unlink(PTMPTMP_FILE);
		if (ret == -1)
			close(fd);
		else
			break;
	}
	umask(oldumask);
	if (ret == -1)
		return -1;

	/* ptmp should be owned by root.root or root.wheel */
	if (chown(PTMP_FILE, (uid_t) 0, (gid_t) 0) < 0)
		return -1;

	/* open ptmp for writing and passwd for reading */
	fp = fdopen(fd, "w");
	if (!fp)
		goto fail;

	pwf = fopen(PASSWD_FILE, "r");
	if (!pwf)
		goto fail;

	namelen = strlen(pwd->pw_name);

	linebuf = malloc(buflen);
	if (!linebuf)
		goto fail;

	/* parse the passwd file */
	found = false;

	/* Do you wonder why I don't use getpwent? Read comments at top of
	 * file */
	while (fgets(linebuf, buflen, pwf) != NULL) {
		contlen = strlen(linebuf);
		while (linebuf[contlen - 1] != '\n' && !feof(pwf)) {
			char *tmp;
			/* Extend input buffer if it failed getting the whole line,
			 * so now we double the buffer size */
			buflen *= 2;
			tmp = realloc(linebuf, buflen);
			if (tmp == NULL)
				goto fail;
			linebuf = tmp;
			/* And fill the rest of the buffer */
			if (fgets(&linebuf[contlen], buflen / 2, pwf) == NULL)
				break;
			contlen = strlen(linebuf);
			/* That was a lot of work for nothing. Gimme perl! */
		}

		/* Is this the username we were sent to change? */
		if (!found && linebuf[namelen] == ':' &&
		    !strncmp(linebuf, pwd->pw_name, namelen)) {
			/* Yes! So go forth in the name of the Lord and
			 * change it!  */
			if (putpwent(pwd, fp) < 0)
				goto fail;
			found = true;
			continue;
		}
		/* Nothing in particular happened, copy input to output */
		fputs(linebuf, fp);
	}

	rc = fclose(fp);
	fp = NULL;
	if (rc < 0)
		goto fail;

	close(fd);
	fd = -1;
	fclose(pwf);	/* I don't think I want to know if this failed */
	pwf = NULL;

	if (!found) {
		errno = ENOENT;	/* give me something better */
		goto fail;
	}

	/* we don't care if we can't remove the backup file */
	unlink(PASSWD_FILE ".OLD");
	/* we don't care if we can't create the backup file */
	ignore_result(link(PASSWD_FILE, PASSWD_FILE ".OLD"));
	/* we DO care if we can't rename to the passwd file */
	if (rename(PTMP_FILE, PASSWD_FILE) < 0)
		goto fail;
	/* finally:  success */
	return 0;

 fail:
	save_errno = errno;
	if (fp != NULL)
		fclose(fp);
	if (pwf != NULL)
		fclose(pwf);
	if (fd >= 0)
		close(fd);
	free(linebuf);
	unlink(PTMP_FILE);
	errno = save_errno;
	return -1;
}

/* Set up the limits so that we're not foiled */
static void pw_init(void)
{
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CPU, &rlim);
	setrlimit(RLIMIT_FSIZE, &rlim);
	setrlimit(RLIMIT_STACK, &rlim);
	setrlimit(RLIMIT_DATA, &rlim);
	setrlimit(RLIMIT_RSS, &rlim);

#ifndef DEBUG
	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	setrlimit(RLIMIT_CORE, &rlim);
#endif

	/* Turn off signals. */
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	/* Create with exact permissions. */
	umask(0);
}
