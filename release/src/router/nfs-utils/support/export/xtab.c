/*
 * support/export/xtab.c
 *
 * Interface to the xtab file.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "xmalloc.h"
#include "nfslib.h"
#include "exportfs.h"
#include "xio.h"
#include "xlog.h"

static void cond_rename(char *newfile, char *oldfile);

static int
xtab_read(char *xtab, char *lockfn, int is_export)
{
    /* is_export == 0  => reading /proc/fs/nfs/exports - we know these things are exported to kernel
     * is_export == 1  => reading /var/lib/nfs/etab - these things are allowed to be exported
     * is_export == 2  => reading /var/lib/nfs/xtab - these things might be known to kernel
     */
	struct exportent	*xp;
	nfs_export		*exp;
	int			lockid;

	if ((lockid = xflock(lockfn, "r")) < 0)
		return 0;
	setexportent(xtab, "r");
	while ((xp = getexportent(is_export==0, 0)) != NULL) {
		if (!(exp = export_lookup(xp->e_hostname, xp->e_path, is_export != 1)) &&
		    !(exp = export_create(xp, is_export!=1))) {
			continue;
		}
		switch (is_export) {
		case 0:
			exp->m_exported = 1;
			break;
		case 1:
			exp->m_xtabent = 1;
			exp->m_mayexport = 1;
			break;
		case 2:
			exp->m_exported = -1;/* may be exported */
			break;
		}
	}
	endexportent();
	xfunlock(lockid);

	return 0;
}

int
xtab_mount_read(void)
{
	int fd;
	if ((fd=open(_PATH_PROC_EXPORTS, O_RDONLY))>=0) {
		close(fd);
		return xtab_read(_PATH_PROC_EXPORTS,
				 _PATH_PROC_EXPORTS, 0);
	} else if ((fd=open(_PATH_PROC_EXPORTS_ALT, O_RDONLY) >= 0)) {
		close(fd);
		return xtab_read(_PATH_PROC_EXPORTS_ALT,
				 _PATH_PROC_EXPORTS_ALT, 0);
	} else
		return xtab_read(_PATH_XTAB, _PATH_XTABLCK, 2);
}

int
xtab_export_read(void)
{
	return xtab_read(_PATH_ETAB, _PATH_ETABLCK, 1);
}

/*
 * mountd now keeps an open fd for the etab at all times to make sure that the
 * inode number changes when the xtab_export_write is done. If you change the
 * routine below such that the files are edited in place, then you'll need to
 * fix the auth_reload logic as well...
 */
static int
xtab_write(char *xtab, char *xtabtmp, char *lockfn, int is_export)
{
	struct exportent	xe;
	nfs_export		*exp;
	int			lockid, i;

	if ((lockid = xflock(lockfn, "w")) < 0) {
		xlog(L_ERROR, "can't lock %s for writing", xtab);
		return 0;
	}
	setexportent(xtabtmp, "w");

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (exp = exportlist[i].p_head; exp; exp = exp->m_next) {
			if (is_export && !exp->m_xtabent)
				continue;
			if (!is_export && ! exp->m_exported)
				continue;

			/* write out the export entry using the FQDN */
			xe = exp->m_export;
			xe.e_hostname = exp->m_client->m_hostname;
			putexportent(&xe);
		}
	}
	endexportent();

	cond_rename(xtabtmp, xtab);

	xfunlock(lockid);

	return 1;
}

int
xtab_export_write()
{
	return xtab_write(_PATH_ETAB, _PATH_ETABTMP, _PATH_ETABLCK, 1);
}

int
xtab_mount_write()
{
	return xtab_write(_PATH_XTAB, _PATH_XTABTMP, _PATH_XTABLCK, 0);
}

void
xtab_append(nfs_export *exp)
{
	struct exportent xe;
	int		lockid;

	if ((lockid = xflock(_PATH_XTABLCK, "w")) < 0)
		return;
	setexportent(_PATH_XTAB, "a");
	xe = exp->m_export;
	xe.e_hostname = exp->m_client->m_hostname;
	putexportent(&xe);
	endexportent();
	xfunlock(lockid);
	exp->m_xtabent = 1;
}

/*
 * rename newfile onto oldfile unless
 * they are identical
 */
static void cond_rename(char *newfile, char *oldfile)
{
	int nfd, ofd;
	char nbuf[4096], obuf[4096];
	int ncnt, ocnt;

	nfd = open(newfile, 0);
	if (nfd < 0)
		return;
	ofd = open(oldfile, 0);
	if (ofd < 0) {
		close(nfd);
		rename(newfile, oldfile);
		return;
	}

	do {
		ncnt = read(nfd, nbuf, sizeof(nbuf));
		if (ncnt < 0)
			break;
		ocnt = read(ofd, obuf, sizeof(obuf));
		if (ocnt < 0)
			break;
		if (ncnt != ocnt)
			break;
		if (ncnt == 0) {
			close(nfd);
			close(ofd);
			unlink(newfile);
			return;
		}
	} while (memcmp(obuf, nbuf, ncnt) == 0);

	/* some mis-match */
	close(nfd);
	close(ofd);
	rename(newfile, oldfile);
	return;
}
