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

/**
 * @file
 * @brief Pid file handling
 */

/**
 * return the pid in a pidfile. return 0 if the process (or pidfile)
 * does not exist 
 */
pid_t pidfile_pid(const char *piddir, const char *name)
{
	int fd;
	char pidstr[20];
	pid_t ret;
	char *pidFile;

	if (asprintf(&pidFile, "%s/%s.pid", piddir, name) < 0) {
		return 0;
	}

	fd = open(pidFile, O_NONBLOCK | O_RDONLY, 0644);

	if (fd == -1) {
		SAFE_FREE(pidFile);
		return 0;
	}

	ZERO_STRUCT(pidstr);

	if (read(fd, pidstr, sizeof(pidstr)-1) <= 0) {
		goto noproc;
	}

	ret = (pid_t)atoi(pidstr);
	
	if (!process_exists_by_pid(ret)) {
		goto noproc;
	}

	if (fcntl_lock(fd,F_SETLK,0,1,F_RDLCK)) {
		/* we could get the lock - it can't be a Samba process */
		goto noproc;
	}

	close(fd);
	SAFE_FREE(pidFile);
	return ret;

 noproc:
	close(fd);
	unlink(pidFile);
	SAFE_FREE(pidFile);
	return 0;
}

/**
 * create a pid file in the pid directory. open it and leave it locked 
 */
void pidfile_create(const char *piddir, const char *name)
{
	int     fd;
	char    buf[20];
	char *pidFile;
	pid_t pid;

	if (asprintf(&pidFile, "%s/%s.pid", piddir, name) < 0) {
		DEBUG(0,("ERROR: Out of memory\n"));
		exit(1);
	}

	pid = pidfile_pid(piddir, name);
	if (pid != 0) {
		DEBUG(0,("ERROR: %s is already running. File %s exists and process id %d is running.\n", 
			 name, pidFile, (int)pid));
		exit(1);
	}

	fd = open(pidFile, O_NONBLOCK | O_CREAT | O_WRONLY | O_EXCL, 0644);
	if (fd == -1) {
		DEBUG(0,("ERROR: can't open %s: Error was %s\n", pidFile, 
			 strerror(errno)));
		exit(1);
	}

	if (fcntl_lock(fd,F_SETLK,0,1,F_WRLCK)==false) {
		DEBUG(0,("ERROR: %s : fcntl lock of file %s failed. Error was %s\n",  
              name, pidFile, strerror(errno)));
		exit(1);
	}

	memset(buf, 0, sizeof(buf));
	slprintf(buf, sizeof(buf) - 1, "%u\n", (unsigned int) getpid());
	if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf)) {
		DEBUG(0,("ERROR: can't write to file %s: %s\n", 
			 pidFile, strerror(errno)));
		exit(1);
	}

	/* Leave pid file open & locked for the duration... */
	SAFE_FREE(pidFile);
}
