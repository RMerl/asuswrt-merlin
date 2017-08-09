/*
   Unix SMB/CIFS implementation.
   run a command as a specified user
   Copyright (C) Andrew Tridgell 1992-1998

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

/* need to move this from here!! need some sleep ... */
struct current_user current_user;

/****************************************************************************
This is a utility function of smbrun().
****************************************************************************/

static int setup_out_fd(void)
{
	int fd;
	TALLOC_CTX *ctx = talloc_stackframe();
	char *path = NULL;

	path = talloc_asprintf(ctx,
				"%s/smb.XXXXXX",
				tmpdir());
	if (!path) {
		TALLOC_FREE(ctx);
		errno = ENOMEM;
		return -1;
	}

	/* now create the file */
	fd = mkstemp(path);

	if (fd == -1) {
		DEBUG(0,("setup_out_fd: Failed to create file %s. (%s)\n",
			path, strerror(errno) ));
		TALLOC_FREE(ctx);
		return -1;
	}

	DEBUG(10,("setup_out_fd: Created tmp file %s\n", path ));

	/* Ensure file only kept around by open fd. */
	unlink(path);
	TALLOC_FREE(ctx);
	return fd;
}

/****************************************************************************
run a command being careful about uid/gid handling and putting the output in
outfd (or discard it if outfd is NULL).
****************************************************************************/

static int smbrun_internal(const char *cmd, int *outfd, bool sanitize)
{
	pid_t pid;
	uid_t uid = current_user.ut.uid;
	gid_t gid = current_user.ut.gid;

	/*
	 * Lose any elevated privileges.
	 */
	drop_effective_capability(KERNEL_OPLOCK_CAPABILITY);
	drop_effective_capability(DMAPI_ACCESS_CAPABILITY);

	/* point our stdout at the file we want output to go into */

	if (outfd && ((*outfd = setup_out_fd()) == -1)) {
		return -1;
	}

	/* in this method we will exec /bin/sh with the correct
	   arguments, after first setting stdout to point at the file */

	/*
	 * We need to temporarily stop CatchChild from eating
	 * SIGCLD signals as it also eats the exit status code. JRA.
	 */

	CatchChildLeaveStatus();
                                   	
	if ((pid=sys_fork()) < 0) {
		DEBUG(0,("smbrun: fork failed with error %s\n", strerror(errno) ));
		CatchChild(); 
		if (outfd) {
			close(*outfd);
			*outfd = -1;
		}
		return errno;
	}

	if (pid) {
		/*
		 * Parent.
		 */
		int status=0;
		pid_t wpid;

		
		/* the parent just waits for the child to exit */
		while((wpid = sys_waitpid(pid,&status,0)) < 0) {
			if(errno == EINTR) {
				errno = 0;
				continue;
			}
			break;
		}

		CatchChild(); 

		if (wpid != pid) {
			DEBUG(2,("waitpid(%d) : %s\n",(int)pid,strerror(errno)));
			if (outfd) {
				close(*outfd);
				*outfd = -1;
			}
			return -1;
		}

		/* Reset the seek pointer. */
		if (outfd) {
			sys_lseek(*outfd, 0, SEEK_SET);
		}

#if defined(WIFEXITED) && defined(WEXITSTATUS)
		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		}
#endif

		return status;
	}
	
	CatchChild(); 
	
	/* we are in the child. we exec /bin/sh to do the work for us. we
	   don't directly exec the command we want because it may be a
	   pipeline or anything else the config file specifies */
	
	/* point our stdout at the file we want output to go into */
	if (outfd) {
		close(1);
		if (dup2(*outfd,1) != 1) {
			DEBUG(2,("Failed to create stdout file descriptor\n"));
			close(*outfd);
			exit(80);
		}
	}

	/* now completely lose our privileges. This is a fairly paranoid
	   way of doing it, but it does work on all systems that I know of */

	become_user_permanently(uid, gid);

	if (!non_root_mode()) {
		if (getuid() != uid || geteuid() != uid ||
		    getgid() != gid || getegid() != gid) {
			/* we failed to lose our privileges - do not execute
			   the command */
			exit(81); /* we can't print stuff at this stage,
				     instead use exit codes for debugging */
		}
	}

#ifndef __INSURE__
	/* close all other file descriptors, leaving only 0, 1 and 2. 0 and
	   2 point to /dev/null from the startup code */
	{
	int fd;
	for (fd=3;fd<256;fd++) close(fd);
	}
#endif

	{
		char *newcmd = NULL;
		if (sanitize) {
			newcmd = escape_shell_string(cmd);
			if (!newcmd)
				exit(82);
		}

		execl("/bin/sh","sh","-c",
		    newcmd ? (const char *)newcmd : cmd, NULL);

		SAFE_FREE(newcmd);
	}
	
	/* not reached */
	exit(83);
	return 1;
}

/****************************************************************************
 Use only in known safe shell calls (printing).
****************************************************************************/

int smbrun_no_sanitize(const char *cmd, int *outfd)
{
	return smbrun_internal(cmd, outfd, False);
}

/****************************************************************************
 By default this now sanitizes shell expansion.
****************************************************************************/

int smbrun(const char *cmd, int *outfd)
{
	return smbrun_internal(cmd, outfd, True);
}

/****************************************************************************
run a command being careful about uid/gid handling and putting the output in
outfd (or discard it if outfd is NULL).
sends the provided secret to the child stdin.
****************************************************************************/

int smbrunsecret(const char *cmd, const char *secret)
{
	pid_t pid;
	uid_t uid = current_user.ut.uid;
	gid_t gid = current_user.ut.gid;
	int ifd[2];
	
	/*
	 * Lose any elevated privileges.
	 */
	drop_effective_capability(KERNEL_OPLOCK_CAPABILITY);
	drop_effective_capability(DMAPI_ACCESS_CAPABILITY);

	/* build up an input pipe */
	if(pipe(ifd)) {
		return -1;
	}

	/* in this method we will exec /bin/sh with the correct
	   arguments, after first setting stdout to point at the file */

	/*
	 * We need to temporarily stop CatchChild from eating
	 * SIGCLD signals as it also eats the exit status code. JRA.
	 */

	CatchChildLeaveStatus();
                                   	
	if ((pid=sys_fork()) < 0) {
		DEBUG(0, ("smbrunsecret: fork failed with error %s\n", strerror(errno)));
		CatchChild(); 
		return errno;
    	}

	if (pid) {
		/*
		 * Parent.
		 */
		int status = 0;
		pid_t wpid;
		size_t towrite;
		ssize_t wrote;
		
		close(ifd[0]);
		/* send the secret */
		towrite = strlen(secret);
		wrote = write(ifd[1], secret, towrite);
		if ( wrote != towrite ) {
		    DEBUG(0,("smbrunsecret: wrote %ld of %lu bytes\n",(long)wrote,(unsigned long)towrite));
		}
		fsync(ifd[1]);
		close(ifd[1]);

		/* the parent just waits for the child to exit */
		while((wpid = sys_waitpid(pid, &status, 0)) < 0) {
			if(errno == EINTR) {
				errno = 0;
				continue;
			}
			break;
		}

		CatchChild(); 

		if (wpid != pid) {
			DEBUG(2, ("waitpid(%d) : %s\n", (int)pid, strerror(errno)));
			return -1;
		}

#if defined(WIFEXITED) && defined(WEXITSTATUS)
		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		}
#endif

		return status;
	}
	
	CatchChild(); 
	
	/* we are in the child. we exec /bin/sh to do the work for us. we
	   don't directly exec the command we want because it may be a
	   pipeline or anything else the config file specifies */
	
	close(ifd[1]);
	close(0);
	if (dup2(ifd[0], 0) != 0) {
		DEBUG(2,("Failed to create stdin file descriptor\n"));
		close(ifd[0]);
		exit(80);
	}

	/* now completely lose our privileges. This is a fairly paranoid
	   way of doing it, but it does work on all systems that I know of */

	become_user_permanently(uid, gid);

	if (!non_root_mode()) {
		if (getuid() != uid || geteuid() != uid ||
		    getgid() != gid || getegid() != gid) {
			/* we failed to lose our privileges - do not execute
			   the command */
			exit(81); /* we can't print stuff at this stage,
				     instead use exit codes for debugging */
		}
	}

#ifndef __INSURE__
	/* close all other file descriptors, leaving only 0, 1 and 2. 0 and
	   2 point to /dev/null from the startup code */
	{
		int fd;
		for (fd = 3; fd < 256; fd++) close(fd);
	}
#endif

	execl("/bin/sh", "sh", "-c", cmd, NULL);  

	/* not reached */
	exit(82);
	return 1;
}
