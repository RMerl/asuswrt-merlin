/* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - fixed strerr(errno) in gettext calls
 *
 * 2006-06-08 Amit Gud <agud@redhat.com>
 * - Moved code to nfs-utils/support/nfs from util-linux/mount.
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <mntent.h>

#include "fstab.h"
#include "xcommon.h"
#include "nfs_mntent.h"
#include "nfs_paths.h"
#include "nls.h"

#define LOCK_TIMEOUT	10
#define streq(s, t)	(strcmp ((s), (t)) == 0)
#define PROC_MOUNTS		"/proc/mounts"

extern char *progname;
extern int verbose;

/* Information about mtab. ------------------------------------*/
static int have_mtab_info = 0;
static int var_mtab_does_not_exist = 0;
static int var_mtab_is_a_symlink = 0;

static void
get_mtab_info(void) {
	struct stat mtab_stat;

	if (!have_mtab_info) {
		if (lstat(MOUNTED, &mtab_stat))
			var_mtab_does_not_exist = 1;
		else if (S_ISLNK(mtab_stat.st_mode))
			var_mtab_is_a_symlink = 1;
		have_mtab_info = 1;
	}
}

void
reset_mtab_info(void) {
        have_mtab_info = 0;
}

int
mtab_does_not_exist(void) {
	get_mtab_info();
	return var_mtab_does_not_exist;
}

static int
mtab_is_a_symlink(void) {
        get_mtab_info();
        return var_mtab_is_a_symlink;
}

int
mtab_is_writable() {
	int fd;

	/* Should we write to /etc/mtab upon an update?
	   Probably not if it is a symlink to /proc/mounts, since that
	   would create a file /proc/mounts in case the proc filesystem
	   is not mounted. */
	if (mtab_is_a_symlink())
		return 0;

	fd = open(MOUNTED, O_RDWR | O_CREAT, 0644);
	if (fd >= 0) {
		close(fd);
		return 1;
	} else
		return 0;
}

/* Contents of mtab and fstab ---------------------------------*/

struct mntentchn mounttable;
static int got_mtab = 0;
struct mntentchn fstab;
static int got_fstab = 0;

static void read_mounttable(void);
static void read_fstab(void);

static struct mntentchn *
mtab_head(void)
{
	if (!got_mtab)
		read_mounttable();
	return &mounttable;
}

static struct mntentchn *
fstab_head(void)
{
	if (!got_fstab)
		read_fstab();
	return &fstab;
}

#if 0
static void
my_free(const void *s) {
	if (s)
		free((void *) s);
}

static void
discard_mntentchn(struct mntentchn *mc0) {
	struct mntentchn *mc, *mc1;

	for (mc = mc0->nxt; mc && mc != mc0; mc = mc1) {
		mc1 = mc->nxt;
		my_free(mc->m.mnt_fsname);
		my_free(mc->m.mnt_dir);
		my_free(mc->m.mnt_type);
		my_free(mc->m.mnt_opts);
		free(mc);
	}
}
#endif

static void
read_mntentchn(mntFILE *mfp, const char *fnam, struct mntentchn *mc0) {
	struct mntentchn *mc = mc0;
	struct mntent *mnt;

	while ((mnt = nfs_getmntent(mfp)) != NULL) {
		if (!streq(mnt->mnt_type, MNTTYPE_IGNORE)) {
			mc->nxt = (struct mntentchn *) xmalloc(sizeof(*mc));
			mc->nxt->prev = mc;
			mc = mc->nxt;
			mc->m = *mnt;
			mc->nxt = mc0;
		}
	}
	mc0->prev = mc;
	if (ferror(mfp->mntent_fp)) {
		int errsv = errno;
		nfs_error(_("warning: error reading %s: %s"),
		      fnam, strerror (errsv));
		mc0->nxt = mc0->prev = NULL;
	}
	nfs_endmntent(mfp);
}

/*
 * Read /etc/mtab.  If that fails, try /proc/mounts.
 * This produces a linked list. The list head mounttable is a dummy.
 * Return 0 on success.
 */
static void
read_mounttable() {
        mntFILE *mfp;
        const char *fnam;
        struct mntentchn *mc = &mounttable;

        got_mtab = 1;
        mc->nxt = mc->prev = NULL;

        fnam = MOUNTED;
        mfp = nfs_setmntent (fnam, "r");
        if (mfp == NULL || mfp->mntent_fp == NULL) {
                int errsv = errno;
                fnam = PROC_MOUNTS;
                mfp = nfs_setmntent (fnam, "r");
                if (mfp == NULL || mfp->mntent_fp == NULL) {
                        nfs_error(_("warning: can't open %s: %s"),
                              MOUNTED, strerror (errsv));
                        return;
                }
                if (verbose)
                        printf(_("%s: could not open %s; using %s instead\n"),
				progname, MOUNTED, PROC_MOUNTS);
        }
        read_mntentchn(mfp, fnam, mc);
}

static void
read_fstab()
{
	mntFILE *mfp = NULL;
	const char *fnam;
	struct mntentchn *mc = &fstab;

	got_fstab = 1;
	mc->nxt = mc->prev = NULL;

	fnam = _PATH_FSTAB;
	mfp = nfs_setmntent (fnam, "r");
	if (mfp == NULL || mfp->mntent_fp == NULL) {
		int errsv = errno;
		nfs_error(_("warning: can't open %s: %s"),
			  _PATH_FSTAB, strerror (errsv));
		return;
	}
	read_mntentchn(mfp, fnam, mc);
}

/*
 * Given the directory name NAME, and the place MCPREV we found it last time,
 * try to find more occurrences.
 */ 
struct mntentchn *
getmntdirbackward (const char *name, struct mntentchn *mcprev) {
	struct mntentchn *mc, *mc0;

	mc0 = mtab_head();
	if (!mcprev)
		mcprev = mc0;
	for (mc = mcprev->prev; mc && mc != mc0; mc = mc->prev)
		if (streq(mc->m.mnt_dir, name))
			return mc;
	return NULL;
}

/*
 * Given the device name NAME, and the place MCPREV we found it last time,
 * try to find more occurrences.
 */ 
struct mntentchn *
getmntdevbackward (const char *name, struct mntentchn *mcprev) {
	struct mntentchn *mc, *mc0;

	mc0 = mtab_head();
	if (!mcprev)
		mcprev = mc0;
	for (mc = mcprev->prev; mc && mc != mc0; mc = mc->prev)
		if (streq(mc->m.mnt_fsname, name))
			return mc;
	return NULL;
}

/* Find the dir FILE in fstab.  */
struct mntentchn *
getfsfile (const char *file)
{
	struct mntentchn *mc, *mc0;

	mc0 = fstab_head();
	for (mc = mc0->nxt; mc && mc != mc0; mc = mc->nxt)
		if (streq(mc->m.mnt_dir, file))
			return mc;
	return NULL;
}

/* Find the device SPEC in fstab.  */
struct mntentchn *
getfsspec (const char *spec)
{
	struct mntentchn *mc, *mc0;

	mc0 = fstab_head();
	for (mc = mc0->nxt; mc && mc != mc0; mc = mc->nxt)
		if (streq(mc->m.mnt_fsname, spec))
			return mc;
	return NULL;
}

/* Updating mtab ----------------------------------------------*/

/* Flag for already existing lock file. */
static int we_created_lockfile = 0;
static int lockfile_fd = -1;

/* Flag to indicate that signals have been set up. */
static int signals_have_been_setup = 0;

/* Ensure that the lock is released if we are interrupted.  */
extern char *strsignal(int sig);	/* not always in <string.h> */

static void
handler (int sig) {
     die(EX_USER, "%s", strsignal(sig));
}

static void
setlkw_timeout (int sig) {
     /* nothing, fcntl will fail anyway */
}

/* Remove lock file.  */
void
unlock_mtab (void) {
	if (we_created_lockfile) {
		close(lockfile_fd);
		lockfile_fd = -1;
		unlink (MOUNTED_LOCK);
		we_created_lockfile = 0;
	}
}

/* Create the lock file.
   The lock file will be removed if we catch a signal or when we exit. */
/* The old code here used flock on a lock file /etc/mtab~ and deleted
   this lock file afterwards. However, as rgooch remarks, that has a
   race: a second mount may be waiting on the lock and proceed as
   soon as the lock file is deleted by the first mount, and immediately
   afterwards a third mount comes, creates a new /etc/mtab~, applies
   flock to that, and also proceeds, so that the second and third mount
   now both are scribbling in /etc/mtab.
   The new code uses a link() instead of a creat(), where we proceed
   only if it was us that created the lock, and hence we always have
   to delete the lock afterwards. Now the use of flock() is in principle
   superfluous, but avoids an arbitrary sleep(). */

/* Where does the link point to? Obvious choices are mtab and mtab~~.
   HJLu points out that the latter leads to races. Right now we use
   mtab~.<pid> instead. Use 20 as upper bound for the length of %d. */
#define MOUNTLOCK_LINKTARGET		MOUNTED_LOCK "%d"
#define MOUNTLOCK_LINKTARGET_LTH	(sizeof(MOUNTED_LOCK)+20)

void
lock_mtab (void) {
	int tries = 100000, i;
	char linktargetfile[MOUNTLOCK_LINKTARGET_LTH];

	at_die = unlock_mtab;

	if (!signals_have_been_setup) {
		int sig = 0;
		struct sigaction sa;

		sa.sa_handler = handler;
		sa.sa_flags = 0;
		sigfillset (&sa.sa_mask);
  
		while (sigismember (&sa.sa_mask, ++sig) != -1
		       && sig != SIGCHLD) {
			if (sig == SIGALRM)
				sa.sa_handler = setlkw_timeout;
			else
				sa.sa_handler = handler;
			sigaction (sig, &sa, (struct sigaction *) 0);
		}
		signals_have_been_setup = 1;
	}

	sprintf(linktargetfile, MOUNTLOCK_LINKTARGET, getpid ());

	i = open (linktargetfile, O_WRONLY|O_CREAT, 0);
	if (i < 0) {
		int errsv = errno;
		/* linktargetfile does not exist (as a file)
		   and we cannot create it. Read-only filesystem?
		   Too many files open in the system?
		   Filesystem full? */
		die (EX_FILEIO, _("can't create lock file %s: %s "
						  "(use -n flag to override)"),
			 linktargetfile, strerror (errsv));
	}
	close(i);
	
	/* Repeat until it was us who made the link */
	while (!we_created_lockfile) {
		struct flock flock;
		int errsv, j;

		j = link(linktargetfile, MOUNTED_LOCK);
		errsv = errno;

		if (j == 0)
			we_created_lockfile = 1;

		if (j < 0 && errsv != EEXIST) {
			(void) unlink(linktargetfile);
			die (EX_FILEIO, _("can't link lock file %s: %s "
			     "(use -n flag to override)"),
			     MOUNTED_LOCK, strerror (errsv));
		}

		lockfile_fd = open (MOUNTED_LOCK, O_WRONLY);

		if (lockfile_fd < 0) {
			int errsv = errno;
			/* Strange... Maybe the file was just deleted? */
			if (errno == ENOENT && tries-- > 0) {
				if (tries % 200 == 0)
					usleep(30);
				continue;
			}
			(void) unlink(linktargetfile);
			die (EX_FILEIO, _("can't open lock file %s: %s "
			     "(use -n flag to override)"),
			     MOUNTED_LOCK, strerror (errsv));
		}

		flock.l_type = F_WRLCK;
		flock.l_whence = SEEK_SET;
		flock.l_start = 0;
		flock.l_len = 0;

		if (j == 0) {
			/* We made the link. Now claim the lock. */
			if (fcntl (lockfile_fd, F_SETLK, &flock) == -1) {
				if (verbose) {
				    int errsv = errno;
				    nfs_error(_("%s: Can't lock lock file "
						"%s: %s"), progname,
					   	MOUNTED_LOCK,
						strerror (errsv));
				}
				/* proceed anyway */
			}
			(void) unlink(linktargetfile);
		} else {
			static int tries = 0;

			/* Someone else made the link. Wait. */
			alarm(LOCK_TIMEOUT);
			if (fcntl (lockfile_fd, F_SETLKW, &flock) == -1) {
				int errsv = errno;
				(void) unlink(linktargetfile);
				die (EX_FILEIO, _("can't lock lock file %s: %s"),
				     MOUNTED_LOCK, (errno == EINTR) ?
				     _("timed out") : strerror (errsv));
			}
			alarm(0);
			/* Limit the number of iterations - maybe there
			   still is some old /etc/mtab~ */
			++tries;
			if (tries % 200 == 0)
			   usleep(30);
			if (tries > 100000) {
				(void) unlink(linktargetfile);
				close(lockfile_fd);
				die (EX_FILEIO, _("Cannot create link %s\n"
						  "Perhaps there is a stale lock file?\n"),
					 MOUNTED_LOCK);
 			}
			close(lockfile_fd);
		}
	}
}

/*
 * Update the mtab.
 *  Used by umount with null INSTEAD: remove the last DIR entry.
 *  Used by mount upon a remount: update option part,
 *   and complain if a wrong device or type was given.
 *   [Note that often a remount will be a rw remount of /
 *    where there was no entry before, and we'll have to believe
 *    the values given in INSTEAD.]
 */

void
update_mtab (const char *dir, struct mntent *instead)
{
	mntFILE *mfp, *mftmp;
	const char *fnam = MOUNTED;
	struct mntentchn mtabhead;	/* dummy */
	struct mntentchn *mc, *mc0, *absent = NULL;

	if (mtab_does_not_exist() || !mtab_is_writable())
		return;

	lock_mtab();

	/* having locked mtab, read it again */
	mc0 = mc = &mtabhead;
	mc->nxt = mc->prev = NULL;

	mfp = nfs_setmntent(fnam, "r");
	if (mfp == NULL || mfp->mntent_fp == NULL) {
		int errsv = errno;
		nfs_error (_("cannot open %s (%s) - mtab not updated"),
		       fnam, strerror (errsv));
		goto leave;
	}

	read_mntentchn(mfp, fnam, mc);

	/* find last occurrence of dir */
	for (mc = mc0->prev; mc && mc != mc0; mc = mc->prev)
		if (streq(mc->m.mnt_dir, dir))
			break;
	if (mc && mc != mc0) {
		if (instead == NULL) {
			/* An umount - remove entry */
			if (mc && mc != mc0) {
				mc->prev->nxt = mc->nxt;
				mc->nxt->prev = mc->prev;
				free(mc);
			}
		} else {
			/* A remount */
			mc->m.mnt_opts = instead->mnt_opts;
		}
	} else if (instead) {
		/* not found, add a new entry */
		absent = xmalloc(sizeof(*absent));
		absent->m = *instead;
		absent->nxt = mc0;
		absent->prev = mc0->prev;
		mc0->prev = absent;
		if (mc0->nxt == NULL)
			mc0->nxt = absent;
	}

	/* write chain to mtemp */
	mftmp = nfs_setmntent (MOUNTED_TEMP, "w");
	if (mftmp == NULL || mftmp->mntent_fp == NULL) {
		int errsv = errno;
		nfs_error (_("cannot open %s (%s) - mtab not updated"),
		       MOUNTED_TEMP, strerror (errsv));
		goto leave;
	}

	for (mc = mc0->nxt; mc && mc != mc0; mc = mc->nxt) {
		if (nfs_addmntent(mftmp, &(mc->m)) == 1) {
			int errsv = errno;
			die (EX_FILEIO, _("error writing %s: %s"),
			     MOUNTED_TEMP, strerror (errsv));
		}
	}

#if 0
	/* the chain might have strings copied from 'instead',
	 * so we cannot safely free it.
	 * And there is no need anyway because we are going to exit
	 * shortly.  So just don't call discard_mntentchn....
	 */
	discard_mntentchn(mc0);
#endif
	if (fchmod (fileno (mftmp->mntent_fp),
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0) {
		int errsv = errno;
		nfs_error(_("%s: error changing mode of %s: %s"),
				progname, MOUNTED_TEMP, strerror (errsv));
	}
	nfs_endmntent (mftmp);

	{ /*
	   * If mount is setuid and some non-root user mounts sth,
	   * then mtab.tmp might get the group of this user. Copy uid/gid
	   * from the present mtab before renaming.
	   */
	    struct stat sbuf;
	    if (stat (MOUNTED, &sbuf) == 0) {
			if (chown (MOUNTED_TEMP, sbuf.st_uid, sbuf.st_gid) < 0) {
				nfs_error(_("%s: error changing owner of %s: %s"),
					progname, MOUNTED_TEMP, strerror (errno));
			}
		}
	}

	/* rename mtemp to mtab */
	if (rename (MOUNTED_TEMP, MOUNTED) < 0) {
		int errsv = errno;
		nfs_error(_("%s: can't rename %s to %s: %s\n"),
				progname, MOUNTED_TEMP, MOUNTED,
				strerror(errsv));
	}

 leave:
	unlock_mtab();
}
