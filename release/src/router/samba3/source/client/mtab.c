/*
 * mtab locking routines for use with mount.cifs and umount.cifs
 * Copyright (C) 2008 Jeff Layton (jlayton@samba.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * This code was copied from the util-linux-ng sources and modified:
 *
 * git://git.kernel.org/pub/scm/utils/util-linux-ng/util-linux-ng.git
 *
 * ...specifically from mount/fstab.c. That file has no explicit license. The
 * "default" license for anything in that tree is apparently GPLv2+, so I
 * believe we're OK to copy it here.
 *
 * Jeff Layton <jlayton@samba.org> 
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <mntent.h>
#include <stdlib.h>
#include <signal.h>
#include "mount.h"


/* Updating mtab ----------------------------------------------*/

/* Flag for already existing lock file. */
static int we_created_lockfile = 0;
static int lockfile_fd = -1;

/* Flag to indicate that signals have been set up. */
static int signals_have_been_setup = 0;

static void
handler (int sig) {
     exit(EX_USER);
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
		unlink (_PATH_MOUNTED_LOCK);
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
#define MOUNTLOCK_LINKTARGET		_PATH_MOUNTED_LOCK "%d"
#define MOUNTLOCK_LINKTARGET_LTH	(sizeof(_PATH_MOUNTED_LOCK)+20)

/*
 * The original mount locking code has used sleep(1) between attempts and
 * maximal number of attemps has been 5.
 *
 * There was very small number of attempts and extremely long waiting (1s)
 * that is useless on machines with large number of concurret mount processes.
 *
 * Now we wait few thousand microseconds between attempts and we have global
 * time limit (30s) rather than limit for number of attempts. The advantage
 * is that this method also counts time which we spend in fcntl(F_SETLKW) and
 * number of attempts is not so much restricted.
 *
 * -- kzak@redhat.com [2007-Mar-2007]
 */

/* maximum seconds between first and last attempt */
#define MOUNTLOCK_MAXTIME		30

/* sleep time (in microseconds, max=999999) between attempts */
#define MOUNTLOCK_WAITTIME		5000

int
lock_mtab (void) {
	int i;
	struct timespec waittime;
	struct timeval maxtime;
	char linktargetfile[MOUNTLOCK_LINKTARGET_LTH];

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

	i = open (linktargetfile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	if (i < 0) {
		/* linktargetfile does not exist (as a file)
		   and we cannot create it. Read-only filesystem?
		   Too many files open in the system?
		   Filesystem full? */
		return EX_FILEIO;
	}
	close(i);

	gettimeofday(&maxtime, NULL);
	maxtime.tv_sec += MOUNTLOCK_MAXTIME;

	waittime.tv_sec = 0;
	waittime.tv_nsec = (1000 * MOUNTLOCK_WAITTIME);

	/* Repeat until it was us who made the link */
	while (!we_created_lockfile) {
		struct timeval now;
		struct flock flock;
		int errsv, j;

		j = link(linktargetfile, _PATH_MOUNTED_LOCK);
		errsv = errno;

		if (j == 0)
			we_created_lockfile = 1;

		if (j < 0 && errsv != EEXIST) {
			(void) unlink(linktargetfile);
			return EX_FILEIO;
		}

		lockfile_fd = open (_PATH_MOUNTED_LOCK, O_WRONLY);

		if (lockfile_fd < 0) {
			/* Strange... Maybe the file was just deleted? */
			gettimeofday(&now, NULL);
			if (errno == ENOENT && now.tv_sec < maxtime.tv_sec) {
				we_created_lockfile = 0;
				continue;
			}
			(void) unlink(linktargetfile);
			return EX_FILEIO;
		}

		flock.l_type = F_WRLCK;
		flock.l_whence = SEEK_SET;
		flock.l_start = 0;
		flock.l_len = 0;

		if (j == 0) {
			/* We made the link. Now claim the lock. If we can't
			 * get it, continue anyway
			 */
			fcntl (lockfile_fd, F_SETLK, &flock);
			(void) unlink(linktargetfile);
		} else {
			/* Someone else made the link. Wait. */
			gettimeofday(&now, NULL);
			if (now.tv_sec < maxtime.tv_sec) {
				alarm(maxtime.tv_sec - now.tv_sec);
				if (fcntl (lockfile_fd, F_SETLKW, &flock) == -1) {
					(void) unlink(linktargetfile);
					return EX_FILEIO;
				}
				alarm(0);
				nanosleep(&waittime, NULL);
			} else {
				(void) unlink(linktargetfile);
				return EX_FILEIO;
			}
			close(lockfile_fd);
		}
	}
	return 0;
}

