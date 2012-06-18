/* scsi-idle
 * Copyright (C) 19?? Christer Weinigel
 * Copyright (C) 1999 Trent Piepho <xyzzy@speakeasy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Changelog:
 * August 25, 2002:
 * Daniel Sterling (dan@lost-habit.com):
   added #include <unistd.h>
   removed unused variables
   down variable set when hd is down instead of vice versa
   don't go into daemon mode if timeout is 30 seconds or less
   warn if timeout is less than 60 seconds.... that's still very low--
    I recommend timeouts of at least 20 minutes
   fixed daemon to spin down drive more than just one time (oops!!)
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <scsi/scsi_ioctl.h>

/* Kernel 2.0 and 2.2 differ on how SCSI_DISK_MAJOR works */
#ifdef SCSI_DISK0_MAJOR
#define IS_SCSI_DISK(rdev)	SCSI_DISK_MAJOR(MAJOR(rdev))
#else
#define IS_SCSI_DISK(rdev)	(MAJOR(rdev)==SCSI_DISK_MAJOR)
#endif

#ifndef SD_IOCTL_IDLE
#define SD_IOCTL_IDLE 4746	/* get idle time */
#endif

#define DEBUG 0

char lockfn[64];

void handler()
{
    unlink(lockfn);
    exit(0);
}

int main(int argc, char *argv[])
{
    int fd;
    int down = 0;
    struct stat statbuf;

    if (argc < 2 || argc > 3) {
	fprintf(stderr,
		"usage: %s device [timeout]\n"
		"  where timeout is the time until motor off in seconds\n"
		"  to idle the disk immediately, use a timeout of zero\n",
		argv[0]);
	exit(1);
    }
    if ((fd = open(argv[1], O_RDWR)) < 0) {
	perror(argv[1]);
	exit(1);
    }
    if ((fstat(fd, &statbuf)) < 0) {
	perror(argv[1]);
	close(fd);
	exit(1);
    }
    if (!S_ISBLK(statbuf.st_mode)
	|| !IS_SCSI_DISK(statbuf.st_rdev)) {
	fprintf(stderr, "%s is not a SCSI disk device\n", argv[1]);
	close(fd);
	exit(1);
    }
    if (argc == 2) {
	long last;

	/* Report idle time */

	if ((last = ioctl(fd, SD_IOCTL_IDLE, &last)) < 0) {
	    perror(argv[1]);
	    close(fd);
	    exit(1);
	}
	if (last == 0)
	    printf("%s has not been accessed yet\n", argv[1]);
	else
	    printf("%s has been idle for %i second%s\n", argv[1],
		 (int) last, (last == 1 ? "" : "s"));
    } else {
	long timeout;

	if ((timeout = atol(argv[2])) < 0) {
	    fprintf(stderr, "timeout may not be negative\n");
	    close(fd);
	    exit(1);
	}
	if (timeout <= 30) {
	    if (timeout != 0) fprintf(stderr,
	        "Really low timeouts are a bad, "
	        "bad idea.\nI'll spin down immediately for you. Use a higher "
	        "timeout for daemon mode.");
	    /* Spin down disk immediately */

	    if (ioctl(fd, SCSI_IOCTL_STOP_UNIT) < 0) {
		perror(argv[1]);
		close(fd);
		exit(1);
	    }
	} else {
	    FILE *fp;
	    int dev;
	    long last;
	    long llast = 0;
	    pid_t pid = 0;

	    /* Spin down disk after an idle period */
	    if (timeout < 60)
		fprintf(stderr,
			"Warning: timeouts less than 60 seconds are really "
			"not such a good idea..\n");

	    /* This is ugly, but I guess there is no portable way to do this */
	    dev = ((statbuf.st_rdev >> 4) & 15) + 'a';

	    sprintf(lockfn, "/var/run/scsi-idle.sd%c.pid", dev);

	    if ((fp = fopen(lockfn, "r")) != NULL) {
		fscanf(fp, "%i", &pid);
		fclose(fp);

		kill(pid, SIGTERM);

		unlink(lockfn);
	    }
	    switch (fork()) {
	    case -1:
		perror("fork failed");
		close(fd);
		exit(1);

	    case 0:
		signal(SIGINT, handler);
		signal(SIGTERM, handler);

		if ((fp = fopen(lockfn, "w")) == NULL) {
		    perror(lockfn);
		    close(fd);
		    exit(1);
		}
		fprintf(fp, "%i\n", (int)getpid());
		fclose(fp);

		nice(10);

/* main daemon loop.
 * obtain idle time.
 * have we shut down the drive already?
   * IF yes: has the idle time reset since the last check?
     * IF yes: reset our drive status variable
 * IF the drive is running AND idle time > user-specified timeout, 
   send STOP_UNIT and set our status variable.
 * IF idle time < timeout, sleep until idle time would be > timeout.
 * ELSE sleep for timeout.
 */
		for (;;) {
		    if ((last = ioctl(fd, SD_IOCTL_IDLE, &last)) < 0) {
			perror(argv[1]);
			close(fd);
			exit(1);
		    }
#if DEBUG
		    if (last == 0)
			printf("%s has not been accessed\n", argv[1]);
		    else
			printf("%s has been idle for %d second%s\n", argv[1],
			       last, (last == 1 ? "" : "s"));
#endif
		    if (down == 1 && llast >= last)
		        down = 0;
		    llast = last;
		    if (last >= timeout) {
			if (down == 0 &&
			(ioctl(fd, SCSI_IOCTL_STOP_UNIT) != 0)) {
			    perror(argv[1]);
			    close(fd);
			    exit(1);
			}
			last = 0;
			down = 1;
		    }
#if DEBUG
		    printf("sleeping for %d seconds\n", timeout - last);
#endif
		    sleep(timeout - last);
		}
		/* not reached */

	    default:
		printf("%s: idle daemon started, timeout is %ld seconds\n",
		       argv[1], timeout);
		/* Ok, done */
		break;
	    }
	}
    }

    close(fd);
    exit(0);
}
