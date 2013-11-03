/* 
   Unix SMB/CIFS implementation.
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
   
   modified for netatalk dgautheron@magic.fr
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
/* #define __USE_GNU 1 */
#include <unistd.h>

#include <errno.h>
#include <sys/wait.h>
#include <sys/param.h>  
#include <string.h>

/* FIXME */
#ifdef linux
#ifndef USE_SETRESUID
#define USE_SETRESUID 1
#endif
#else
#ifndef USE_SETEUID
#define USE_SETEUID 1
#endif
#endif

#include <atalk/logger.h>

/**************************************************************************n
 Find a suitable temporary directory. The result should be copied immediately
  as it may be overwritten by a subsequent call.
  ****************************************************************************/
   
static const char *tmpdir(void)
{
    char *p;

    if ((p = getenv("TMPDIR")))
        return p;
    return "/tmp";
}

/****************************************************************************
This is a utility function of afprun().
****************************************************************************/

static int setup_out_fd(void)
{  
	int fd;
	char path[MAXPATHLEN +1];

	snprintf(path, sizeof(path)-1, "%s/afp.XXXXXX", tmpdir());

	/* now create the file */
	fd = mkstemp(path);

	if (fd == -1) {
		LOG(log_error, logtype_afpd, "setup_out_fd: Failed to create file %s. (%s)",path, strerror(errno) );
		return -1;
	}

	/* Ensure file only kept around by open fd. */
	unlink(path);
	return fd;
}

/****************************************************************************
 Gain root privilege before doing something.
 We want to end up with ruid==euid==0
****************************************************************************/
static void gain_root_privilege(void)
{
        seteuid(0);
}
 
/****************************************************************************
 Ensure our real and effective groups are zero.
 we want to end up with rgid==egid==0
****************************************************************************/
static void gain_root_group_privilege(void)
{
        setegid(0);
}

/****************************************************************************
 Become the specified uid and gid - permanently !
 there should be no way back if possible
****************************************************************************/
static void become_user_permanently(uid_t uid, gid_t gid)
{
    /*
     * First - gain root privilege. We do this to ensure
     * we can lose it again.
     */
 
    gain_root_privilege();
    gain_root_group_privilege();
 
#if USE_SETRESUID
    setresgid(gid,gid,gid);
    setgid(gid);
    setresuid(uid,uid,uid);
    setuid(uid);
#endif
 
#if USE_SETREUID
    setregid(gid,gid);
    setgid(gid);
    setreuid(uid,uid);
    setuid(uid);
#endif
 
#if USE_SETEUID
    setegid(gid);
    setgid(gid);
    setuid(uid);
    seteuid(uid);
    setuid(uid);
#endif
 
#if USE_SETUIDX
    setgidx(ID_REAL, gid);
    setgidx(ID_EFFECTIVE, gid);
    setgid(gid);
    setuidx(ID_REAL, uid);
    setuidx(ID_EFFECTIVE, uid);
    setuid(uid);
#endif
}

/****************************************************************************
run a command being careful about uid/gid handling and putting the output in
outfd (or discard it if outfd is NULL).
****************************************************************************/

int afprun(int root, char *cmd, int *outfd)
{
    pid_t pid;
    uid_t uid = geteuid();
    gid_t gid = getegid();
	
    /* point our stdout at the file we want output to go into */
    if (outfd && ((*outfd = setup_out_fd()) == -1)) {
        return -1;
    }
    LOG(log_debug, logtype_afpd, "running %s as user %d", cmd, root?0:uid);
    /* in this method we will exec /bin/sh with the correct
       arguments, after first setting stdout to point at the file */

    if ((pid=fork()) < 0) {
        LOG(log_error, logtype_afpd, "afprun: fork failed with error %s", strerror(errno) );
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
	while((wpid = waitpid(pid,&status,0)) < 0) {
	    if (errno == EINTR) {
	        errno = 0;
		continue;
	    }
	    break;
	}
	if (wpid != pid) {
	    LOG(log_error, logtype_afpd, "waitpid(%d) : %s",(int)pid, strerror(errno) );
	    if (outfd) {
	        close(*outfd);
	        *outfd = -1;
	    }
	    return -1;
	}
	/* Reset the seek pointer. */
	if (outfd) {
	    lseek(*outfd, 0, SEEK_SET);
	}

#if defined(WIFEXITED) && defined(WEXITSTATUS)
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
#endif
        return status;
    }
    
    /* we are in the child. we exec /bin/sh to do the work for us. we
       don't directly exec the command we want because it may be a
       pipeline or anything else the config file specifies */

    /* point our stdout at the file we want output to go into */
    if (outfd) {
        close(1);
	if (dup2(*outfd,1) != 1) {
	    LOG(log_error, logtype_afpd, "Failed to create stdout file descriptor");
	    close(*outfd);
	    exit(80);
	}
    }
    
    if (chdir("/") < 0) {
        LOG(log_error, logtype_afpd, "afprun: can't change directory to \"/\" %s", strerror(errno) );
        exit(83);
    }

    /* now completely lose our privileges. This is a fairly paranoid
       way of doing it, but it does work on all systems that I know of */
    if (root) {
    	become_user_permanently(0, 0);
    	uid = gid = 0;
    }
    else {
    	become_user_permanently(uid, gid);
    }
    if (getuid() != uid || geteuid() != uid || getgid() != gid || getegid() != gid) {
        /* we failed to lose our privileges - do not execute the command */
	exit(81); /* we can't print stuff at this stage, instead use exit codes for debugging */
    }
    
    /* close all other file descriptors, leaving only 0, 1 and 2. 0 and
       2 point to /dev/null from the startup code */
    {
	int fd;
	for (fd=3;fd<256;fd++) close(fd);
    }

    execl("/bin/sh","sh","-c",cmd,NULL);  
    /* not reached */
    exit(82);
    return 1;
}
