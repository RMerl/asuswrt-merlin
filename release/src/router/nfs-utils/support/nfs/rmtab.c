/*
 * support/nfs/rmtab.c
 *
 * Handling for rmtab.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "nfslib.h"

static FILE	*rmfp = NULL;

int
setrmtabent(char *type)
{
	if (rmfp)
		fclose(rmfp);
	rmfp = fsetrmtabent(_PATH_RMTAB, type);
	return (rmfp != NULL);
}

FILE *
fsetrmtabent(char *fname, char *type)
{
	int	readonly = !strcmp(type, "r");
	FILE	*fp;

	if (!fname)
		return NULL;
	if ((fp = fopen(fname, type)) == NULL) {
		xlog(L_ERROR, "can't open %s for %sing", fname,
				readonly ? "read" : "writ");
		return NULL;
	}
	return fp;
}

struct rmtabent *
getrmtabent(int log, long *pos)
{
	return fgetrmtabent(rmfp, log, pos);
}

struct rmtabent *
fgetrmtabent(FILE *fp, int log, long *pos)
{
	static struct rmtabent	re;
	char	buf[2048], *count, *host, *path;

	errno = 0;
	if (!fp)
		return NULL;
	do {
		if (pos)
			*pos = ftell (fp);
		if (fgets(buf, sizeof(buf)-1, fp) == NULL)
			return NULL;
		host = buf;
		if ((path = strchr(host, '\n')) != NULL)
			*path = '\0';
		if (!(path = strchr(host, ':'))) {
			if (log)
				xlog(L_ERROR, "malformed entry in rmtab file");
			errno = EINVAL;
			return NULL;
		}
		*path++ = '\0';
		count = strchr(path, ':');
		if (count) {
			*count++ = '\0';
			re.r_count = strtol (count, NULL, 0);
		}
		else
			re.r_count = 1;
	} while (0);
	strncpy(re.r_client, host, sizeof (re.r_client) - 1);
	re.r_client[sizeof (re.r_client) - 1] = '\0';
	strncpy(re.r_path, path, sizeof (re.r_path) - 1);
	re.r_path[sizeof (re.r_path) - 1] = '\0';
	return &re;
}

void
putrmtabent(struct rmtabent *rep, long *pos)
{
	fputrmtabent(rmfp, rep, pos);
}

void
fputrmtabent(FILE *fp, struct rmtabent *rep, long *pos)
{
	if (!fp || (pos && fseek (fp, *pos, SEEK_SET) != 0))
		return;
	fprintf(fp, "%s:%s:0x%.8x\n", rep->r_client, rep->r_path,
		rep->r_count);
}

void
endrmtabent(void)
{
	fendrmtabent(rmfp);
	rmfp = NULL;
}

void
fendrmtabent(FILE *fp)
{
	if (fp) {
		/* If it was written to, we really want
		 * to flush to disk before returning
		 */
		fflush(fp);
		fdatasync(fileno(fp));
		fclose(fp);
	}
}

void
rewindrmtabent(void)
{
	if (rmfp)
		rewind(rmfp);
}

void
frewindrmtabent(FILE *fp)
{
	if (fp)
		rewind (fp);
}
