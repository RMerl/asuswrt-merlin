/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   run a command as a specified user
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

/* need to move this from here!! need some sleep ... */
struct current_user current_user;

extern int DEBUGLEVEL;

/****************************************************************************
This is a utility function of smbrun(). It must be called only from
the child as it may leave the caller in a privileged state.
****************************************************************************/
static BOOL setup_stdout_file(char *outfile,BOOL shared)
{  
  int fd;
  mode_t mode = S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH;
  int flags = O_RDWR|O_CREAT|O_TRUNC|O_EXCL;

  close(1);

  if (shared) {
	/* become root - unprivileged users can't delete these files */
	gain_root_privilege();
	gain_root_group_privilege();
  }

  unlink(outfile);
  /* now create the file */
  fd = sys_open(outfile,flags,mode);

  if (fd == -1) return False;

  if (fd != 1) {
    if (dup2(fd,1) != 0) {
      DEBUG(2,("Failed to create stdout file descriptor\n"));
      close(fd);
      return False;
    }
    close(fd);
  }
  return True;
}


/****************************************************************************
run a command being careful about uid/gid handling and putting the output in
outfile (or discard it if outfile is NULL).

if shared is True then ensure the file will be writeable by all users
but created such that its owned by root. This overcomes a security hole.

if shared is not set then open the file with O_EXCL set
****************************************************************************/
int smbrun(char *cmd,char *outfile,BOOL shared)
{
	int fd;
	pid_t pid;
	uid_t uid = current_user.uid;
	gid_t gid = current_user.gid;

    /*
     * Lose any kernel oplock capabilities we may have.
     */
    set_process_capability(KERNEL_OPLOCK_CAPABILITY, False);
    set_inherited_process_capability(KERNEL_OPLOCK_CAPABILITY, False);

#ifndef HAVE_EXECL
	int ret;
	pstring syscmd;  
	char *path = lp_smbrun();
	
	/* in the old method we use system() to execute smbrun which then
	   executes the command (using system() again!). This involves lots
	   of shell launches and is very slow. It also suffers from a
	   potential security hole */
	if (!file_exist(path,NULL)) {
		DEBUG(0,("SMBRUN ERROR: Can't find %s. Installation problem?\n",path));
		return(1);
	}

	slprintf(syscmd,sizeof(syscmd)-1,"%s %d %d \"(%s 2>&1) > %s\"",
		 path,(int)uid,(int)gid,cmd,
		 outfile?outfile:"/dev/null");
	
	DEBUG(5,("smbrun - running %s ",syscmd));
	ret = system(syscmd);
	DEBUG(5,("gave %d\n",ret));
	return(ret);
#else
	/* in this newer method we will exec /bin/sh with the correct
	   arguments, after first setting stdout to point at the file */

	/*
	 * We need to temporarily stop CatchChild from eating
	 * SIGCLD signals as it also eats the exit status code. JRA.
	 */

	CatchChildLeaveStatus();
                                   	
	if ((pid=fork()) < 0) {
		DEBUG(0,("smbrun: fork failed with error %s\n", strerror(errno) ));
		CatchChild(); 
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
	
	/* point our stdout at the file we want output to go into */
	if (outfile && !setup_stdout_file(outfile,shared)) {
		exit(80);
	}
	
	/* now completely lose our privileges. This is a fairly paranoid
	   way of doing it, but it does work on all systems that I know of */

	become_user_permanently(uid, gid);

	if (getuid() != uid || geteuid() != uid ||
	    getgid() != gid || getegid() != gid) {
		/* we failed to lose our privileges - do not execute
                   the command */
		exit(81); /* we can't print stuff at this stage,
			     instead use exit codes for debugging */
	}
	
	/* close all other file descriptors, leaving only 0, 1 and 2. 0 and
	   2 point to /dev/null from the startup code */
	for (fd=3;fd<256;fd++) close(fd);
	
	execl("/bin/sh","sh","-c",cmd,NULL);  
	
	/* not reached */
	exit(82);
#endif
	return 1;
}
