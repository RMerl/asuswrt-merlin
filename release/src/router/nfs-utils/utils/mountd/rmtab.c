/*
 * utils/mountd/rmtab.c
 *
 * Manage the rmtab file for mountd.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "xmalloc.h"
#include "misc.h"
#include "exportfs.h"
#include "xio.h"
#include "mountd.h"
#include "ha-callout.h"

#include <limits.h> /* PATH_MAX */

extern int reverse_resolve;

/* If new path is a link do not destroy it but place the
 * file where the link points.
 */

static int 
slink_safe_rename(const char * oldpath, const char * newpath)
{
	int r;
	struct stat s;
	char slink_path[PATH_MAX];
	const char *real_newpath = newpath;

	if ((lstat(newpath, &s) == 0) && S_ISLNK(s.st_mode)) {
		/* New path is a symbolic link, do not destroy but follow */
		if ((r = readlink(newpath, slink_path, PATH_MAX - 1)) == -1)
			return -1;
		slink_path[r] = '\0';
		real_newpath = slink_path;
	}

	return rename(oldpath, real_newpath);
}

void
mountlist_add(char *host, const char *path)
{
	struct rmtabent	xe;
	struct rmtabent	*rep;
	int		lockid;
	long		pos;

	if ((lockid = xflock(_PATH_RMTABLCK, "a")) < 0)
		return;
	setrmtabent("r+");
	while ((rep = getrmtabent(1, &pos)) != NULL) {
		if (strcmp (rep->r_client,
			    host) == 0
		    && strcmp(rep->r_path, path) == 0) {
			rep->r_count++;
			/* PRC: do the HA callout: */
			ha_callout("mount", rep->r_client, rep->r_path, rep->r_count);
			putrmtabent(rep, &pos);
			endrmtabent();
			xfunlock(lockid);
			return;
		}
	}
	endrmtabent();
	strncpy(xe.r_client, host,
		sizeof (xe.r_client) - 1);
	xe.r_client [sizeof (xe.r_client) - 1] = '\0';
	strncpy(xe.r_path, path, sizeof (xe.r_path) - 1);
	xe.r_path [sizeof (xe.r_path) - 1] = '\0';
	xe.r_count = 1;
	if (setrmtabent("a")) {
		/* PRC: do the HA callout: */
		ha_callout("mount", xe.r_client, xe.r_path, xe.r_count);
		putrmtabent(&xe, NULL);
		endrmtabent();
	}
	xfunlock(lockid);
}

void
mountlist_del(char *hname, const char *path)
{
	struct rmtabent	*rep;
	FILE		*fp;
	int		lockid;
	int		match;

	if ((lockid = xflock(_PATH_RMTABLCK, "w")) < 0)
		return;
	if (!setrmtabent("r")) {
		xfunlock(lockid);
		return;
	}
	if (!(fp = fsetrmtabent(_PATH_RMTABTMP, "w"))) {
		endrmtabent();
		xfunlock(lockid);
		return;
	}
	while ((rep = getrmtabent(1, NULL)) != NULL) {
		match = !strcmp (rep->r_client, hname)
			&& !strcmp(rep->r_path, path);
		if (match) {
			rep->r_count--;
			/* PRC: do the HA callout: */
			ha_callout("unmount", rep->r_client, rep->r_path, rep->r_count);
		}
		if (!match || rep->r_count)
			fputrmtabent(fp, rep, NULL);
	}
	if (slink_safe_rename(_PATH_RMTABTMP, _PATH_RMTAB) < 0) {
		xlog(L_ERROR, "couldn't rename %s to %s",
				_PATH_RMTABTMP, _PATH_RMTAB);
	}
	endrmtabent();	/* close & unlink */
	fendrmtabent(fp);
	xfunlock(lockid);
}

void
mountlist_del_all(struct sockaddr_in *sin)
{
	struct in_addr	addr = sin->sin_addr;
	struct hostent	*hp;
	struct rmtabent	*rep;
	nfs_export	*exp;
	FILE		*fp;
	int		lockid;

	if ((lockid = xflock(_PATH_RMTABLCK, "w")) < 0)
		return;
	if (!(hp = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET))) {
		xlog(L_ERROR, "can't get hostname of %s", inet_ntoa(addr));
		xfunlock(lockid);
		return;
	}
	else
		hp = hostent_dup (hp);

	if (!setrmtabent("r")) {
		xfunlock(lockid);
		free (hp);
		return;
	}
	if (!(fp = fsetrmtabent(_PATH_RMTABTMP, "w"))) {
		endrmtabent();
		xfunlock(lockid);
		free (hp);
		return;
	}
	while ((rep = getrmtabent(1, NULL)) != NULL) {
		if (strcmp(rep->r_client, hp->h_name) == 0 &&
		    (exp = auth_authenticate("umountall", sin, rep->r_path)))
			continue;
		fputrmtabent(fp, rep, NULL);
	}
	if (slink_safe_rename(_PATH_RMTABTMP, _PATH_RMTAB) < 0) {
		xlog(L_ERROR, "couldn't rename %s to %s",
				_PATH_RMTABTMP, _PATH_RMTAB);
	}
	endrmtabent();	/* close & unlink */
	fendrmtabent(fp);
	xfunlock(lockid);
	free (hp);
}

mountlist
mountlist_list(void)
{
	static mountlist	mlist = NULL;
	static time_t		last_mtime = 0;
	mountlist		m;
	struct rmtabent		*rep;
	struct stat		stb;
	int			lockid;
	struct in_addr		addr;
	struct hostent		*he;

	if ((lockid = xflock(_PATH_RMTABLCK, "r")) < 0)
		return NULL;
	if (stat(_PATH_RMTAB, &stb) < 0) {
		xlog(L_ERROR, "can't stat %s", _PATH_RMTAB);
		return NULL;
	}
	if (stb.st_mtime != last_mtime) {
		while (mlist) {
			mlist = (m = mlist)->ml_next;
			xfree(m->ml_hostname);
			xfree(m->ml_directory);
			xfree(m);
		}
		last_mtime = stb.st_mtime;

		setrmtabent("r");
		while ((rep = getrmtabent(1, NULL)) != NULL) {
			m = (mountlist) xmalloc(sizeof(*m));

			if (reverse_resolve &&
			   inet_aton((const char *) rep->r_client, &addr) &&
			   (he = gethostbyaddr(&addr, sizeof(addr), AF_INET)))
				m->ml_hostname = xstrdup(he->h_name);
			else
				m->ml_hostname = xstrdup(rep->r_client);

 			m->ml_directory = xstrdup(rep->r_path);
			m->ml_next = mlist;
			mlist = m;
		}
		endrmtabent();
	}
	xfunlock(lockid);

	return mlist;
}
