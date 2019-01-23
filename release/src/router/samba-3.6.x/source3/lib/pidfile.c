/* this code is broken - there is a race condition with the unlink (tridge) */

/* 
   Unix SMB/CIFS implementation.
   pidfile handling
   Copyright (C) Andrew Tridgell 1998
   
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
#include "system/filesys.h"

#ifndef O_NONBLOCK
#define O_NONBLOCK
#endif

static char *pidFile_name = NULL;

/* return the pid in a pidfile. return 0 if the process (or pidfile)
   does not exist */
pid_t pidfile_pid(const char *program_name)
{
	int fd;
	char pidstr[20];
	pid_t pid;
	unsigned int ret;
	char *name;
	const char *short_configfile;
	char * pidFile;

	/* Add a suffix to the program name if this is a process with a
	 * none default configuration file name. */
	if (strcmp( CONFIGFILE, get_dyn_CONFIGFILE()) == 0) {
		name = SMB_STRDUP(program_name);
	} else {
		short_configfile = strrchr( get_dyn_CONFIGFILE(), '/');
		if (short_configfile == NULL) {
			/* conf file in current directory */
			short_configfile = get_dyn_CONFIGFILE();
		} else {
			/* full/relative path provided */
			short_configfile++;
		}
		if (asprintf(&name, "%s-%s", program_name,
				short_configfile) == -1) {
			smb_panic("asprintf failed");
		}
	}

	if (asprintf(&pidFile, "%s/%s.pid", lp_piddir(), name) == -1) {
		SAFE_FREE(name);
		return 0;
	}

	SAFE_FREE(name);

	fd = sys_open(pidFile, O_NONBLOCK | O_RDONLY, 0644);
	if (fd == -1) {
		SAFE_FREE(pidFile);
		return 0;
	}

	ZERO_ARRAY(pidstr);

	if (read(fd, pidstr, sizeof(pidstr)-1) <= 0) {
		goto noproc;
	}

	ret = atoi(pidstr);

	if (ret == 0) {
		/* Obviously we had some garbage in the pidfile... */
		DEBUG(1, ("Could not parse contents of pidfile %s\n",
			  pidFile));
		goto noproc;
	}
	
	pid = (pid_t)ret;
	if (!process_exists_by_pid(pid)) {
		goto noproc;
	}

	if (fcntl_lock(fd,SMB_F_SETLK,0,1,F_RDLCK)) {
		/* we could get the lock - it can't be a Samba process */
		goto noproc;
	}

	SAFE_FREE(pidFile);
	close(fd);
	return (pid_t)ret;

 noproc:
	close(fd);
	unlink(pidFile);
	SAFE_FREE(pidFile);
	return 0;
}

/* create a pid file in the pid directory. open it and leave it locked */
void pidfile_create(const char *program_name)
{
	int     fd;
	char    buf[20];
	const char    *short_configfile;
	char *name;
	pid_t pid;

	/* Add a suffix to the program name if this is a process with a
	 * none default configuration file name. */
	if (strcmp( CONFIGFILE, get_dyn_CONFIGFILE()) == 0) {
		name = SMB_STRDUP(program_name);
	} else {
		short_configfile = strrchr( get_dyn_CONFIGFILE(), '/');
		if (short_configfile == NULL) {
			/* conf file in current directory */
			short_configfile = get_dyn_CONFIGFILE();
		} else {
			/* full/relative path provided */
			short_configfile++;
		}
		if (asprintf(&name, "%s-%s", program_name,
			     short_configfile) == -1) {
			smb_panic("asprintf failed");
		}
	}

	if (asprintf(&pidFile_name, "%s/%s.pid", lp_piddir(), name) == -1) {
		smb_panic("asprintf failed");
	}

	pid = pidfile_pid(name);
	if (pid != 0) {
		DEBUG(0,("ERROR: %s is already running. File %s exists and process id %d is running.\n", 
			 name, pidFile_name, (int)pid));
		exit(1);
	}

	fd = sys_open(pidFile_name, O_NONBLOCK | O_CREAT | O_WRONLY | O_EXCL,
		      0644);
	if (fd == -1) {
		DEBUG(0,("ERROR: can't open %s: Error was %s\n", pidFile_name,
			 strerror(errno)));
		exit(1);
	}

	if (fcntl_lock(fd,SMB_F_SETLK,0,1,F_WRLCK)==False) {
		DEBUG(0,("ERROR: %s : fcntl lock of file %s failed. Error was %s\n",  
			 name, pidFile_name, strerror(errno)));
		exit(1);
	}

	memset(buf, 0, sizeof(buf));
	slprintf(buf, sizeof(buf) - 1, "%u\n", (unsigned int) sys_getpid());
	if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {
		DEBUG(0,("ERROR: can't write to file %s: %s\n", 
			 pidFile_name, strerror(errno)));
		exit(1);
	}
	/* Leave pid file open & locked for the duration... */
	SAFE_FREE(name);

	/* set the close on exec so that we don't leak the fd */
	fcntl(fd, F_SETFD, FD_CLOEXEC);
}

void pidfile_unlink(void)
{
	if (pidFile_name == NULL) {
		return;
	}
	unlink(pidFile_name);
	SAFE_FREE(pidFile_name);
}
