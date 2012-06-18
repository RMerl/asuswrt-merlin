/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   pidfile handling
   Copyright (C) Andrew Tridgell 1998
   
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


extern int DEBUGLEVEL;

#ifndef O_NONBLOCK
#define O_NONBLOCK
#endif

/* return the pid in a pidfile. return 0 if the process (or pidfile)
   does not exist */
pid_t pidfile_pid(char *name)
{
	int fd;
	char pidstr[20];
	unsigned ret;
	pstring pidFile;

	slprintf(pidFile, sizeof(pidFile)-1, "%s/%s.pid", lp_lockdir(), name);

	fd = sys_open(pidFile, O_NONBLOCK | O_RDWR, 0644);
	if (fd == -1) {
		return 0;
	}

	ZERO_ARRAY(pidstr);

	if (read(fd, pidstr, sizeof(pidstr)-1) <= 0) {
		goto ok;
	}

	ret = atoi(pidstr);
	
	if (!process_exists((pid_t)ret)) {
		goto ok;
	}

	if (fcntl_lock(fd,SMB_F_SETLK,0,1,F_WRLCK)) {
		/* we could get the lock - it can't be a Samba process */
		goto ok;
	}

	close(fd);
	return (pid_t)ret;

 ok:
	close(fd);
	unlink(pidFile);
	return 0;
}

/* create a pid file in the lock directory. open it and leave it locked */
void pidfile_create(char *name)
{
	int     fd;
	char    buf[20];
	pstring pidFile;
	pid_t pid;

	slprintf(pidFile, sizeof(pidFile)-1, "%s/%s.pid", lp_lockdir(), name);

	pid = pidfile_pid(name);
	if (pid != 0) {
		DEBUG(0,("ERROR: %s is already running. File %s exists and process id %d is running.\n", 
			 name, pidFile, (int)pid));
		exit(1);
	}

	fd = sys_open(pidFile, O_NONBLOCK | O_CREAT | O_WRONLY | O_EXCL, 0644);
	if (fd == -1) {
		DEBUG(0,("ERROR: can't open %s: Error was %s\n", pidFile, 
			 strerror(errno)));
		exit(1);
	}

	if (fcntl_lock(fd,SMB_F_SETLK,0,1,F_WRLCK)==False) {
		DEBUG(0,("ERROR: %s : fcntl lock of file %s failed. Error was %s\n",  
              name, pidFile, strerror(errno)));
		exit(1);
	}

	memset(buf, 0, sizeof(buf));
	slprintf(buf, sizeof(buf) - 1, "%u\n", (unsigned int) getpid());
	if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
		DEBUG(0,("ERROR: can't write to file %s: %s\n", 
			 pidFile, strerror(errno)));
		exit(1);
	}
	/* Leave pid file open & locked for the duration... */
}
