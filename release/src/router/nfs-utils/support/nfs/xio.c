/*
 * support/nfs/xio.c
 * 
 * Simple I/O functions for the parsing of /etc/exports and /etc/nfsclients.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "xmalloc.h"
#include "xlog.h"
#include "xio.h"

XFILE *
xfopen(char *fname, char *type)
{
	XFILE	*xfp;
	FILE	*fp;

	if (!(fp = fopen(fname, type)))
		return NULL;
	xfp = (XFILE *) xmalloc(sizeof(*xfp));
	xfp->x_fp = fp;
	xfp->x_line = 1;

	return xfp;
}

void
xfclose(XFILE *xfp)
{
	fclose(xfp->x_fp);
	xfree(xfp);
}

static void
doalarm(int sig)
{
	return;
}

int
xflock(char *fname, char *type)
{
	struct sigaction sa, oldsa;
	int		readonly = !strcmp(type, "r");
	struct flock	fl = { readonly? F_RDLCK : F_WRLCK, SEEK_SET, 0, 0, 0 };
	int		fd;

	if (readonly)
		fd = open(fname, (O_RDONLY|O_CREAT), 0600);
	else
		fd = open(fname, (O_RDWR|O_CREAT), 0600);
	if (fd < 0) {
		xlog(L_WARNING, "could not open %s for locking: errno %d (%s)",
				fname, errno, strerror(errno));
		return -1;
	}

	sa.sa_handler = doalarm;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, &oldsa);
	alarm(10);
	if (fcntl(fd, F_SETLKW, &fl) < 0) {
		alarm(0);
		xlog(L_WARNING, "failed to lock %s: errno %d (%s)",
				fname, errno, strerror(errno));
		close(fd);
		fd = 0;
	} else {
		alarm(0);
	}
	sigaction(SIGALRM, &oldsa, NULL);

	return fd;
}

void
xfunlock(int fd)
{
	close(fd);
}

#define isoctal(x) (isdigit(x) && ((x)<'8'))
int
xgettok(XFILE *xfp, char sepa, char *tok, int len)
{
	int	i = 0;
	int	c = 0;
	int 	quoted=0;

	while (i < len && (c = xgetc(xfp)) != EOF &&
	       (quoted || (c != sepa && !isspace(c)))) {
		if (c == '"') {
			quoted = !quoted;
			continue;
		}
		tok[i++] = c;
		if (i >= 4 &&
		    tok[i-4] == '\\' &&
		    isoctal(tok[i-3]) &&
		    isoctal(tok[i-2]) &&
		    isoctal(tok[i-1]) &&
		    ((tok[i]=0),
		     (c = strtol(tok+i-3,NULL, 8)) < 256)) {
			i -= 4;
			tok[i++] = c;
		}
	}	
	if (c == '\n')
		xungetc(c, xfp);
	if (!i)
		return 0;
	if (i >= len || (sepa && c != sepa))
		return -1;
	tok[i] = '\0';
	return 1;
}

int
xgetc(XFILE *xfp)
{
	int	c = getc(xfp->x_fp);

	if (c == EOF)
		return c;
	if (c == '\\') {
		if ((c = getc(xfp->x_fp)) != '\n') {
			ungetc(c, xfp->x_fp);
			return '\\';
		}
		xfp->x_line++;
		while ((c = getc(xfp->x_fp)) == ' ' || c == '\t');
		ungetc(c, xfp->x_fp);
		return ' ';
	}
	if (c == '\n')
		xfp->x_line++;
	return c;
}

void
xungetc(int c, XFILE *xfp)
{
	if (c == EOF)
		return;

	ungetc(c, xfp->x_fp);
	if (c == '\n')
		xfp->x_line--;
}

void
xskip(XFILE *xfp, char *str)
{
	int	c;

	while ((c = xgetc(xfp)) != EOF) {
		if (c == '#')
			c = xskipcomment(xfp);
		if (strchr(str, c) == NULL)
			break;
	}
	xungetc(c, xfp);
}

char
xskipcomment(XFILE *xfp)
{
	int	c;

	while ((c = getc(xfp->x_fp)) != EOF && c != '\n');
	return c;
}
