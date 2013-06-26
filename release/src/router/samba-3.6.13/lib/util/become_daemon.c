/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2002
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) James J Myers 2003
   
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
#include "system/locale.h"

/*******************************************************************
 Close the low 3 fd's and open dev/null in their place.
********************************************************************/

_PUBLIC_ void close_low_fds(bool stderr_too)
{
#ifndef VALGRIND
	int fd;
	int i;

	close(0);
	close(1);

	if (stderr_too)
		close(2);

	/* try and use up these file descriptors, so silly
		library routines writing to stdout etc won't cause havoc */
	for (i=0;i<3;i++) {
		if (i == 2 && !stderr_too)
			continue;

		fd = open("/dev/null",O_RDWR,0);
		if (fd < 0)
			fd = open("/dev/null",O_WRONLY,0);
		if (fd < 0) {
			DEBUG(0,("Can't open /dev/null\n"));
			return;
		}
		if (fd != i) {
			DEBUG(0,("Didn't get file descriptor %d\n",i));
			return;
		}
	}
#endif
}

/****************************************************************************
 Become a daemon, discarding the controlling terminal.
****************************************************************************/

_PUBLIC_ void become_daemon(bool do_fork, bool no_process_group, bool log_stdout)
{
	if (do_fork) {
		if (sys_fork()) {
			_exit(0);
		}
	}

	/* detach from the terminal */
#ifdef HAVE_SETSID
	if (!no_process_group) setsid();
#elif defined(TIOCNOTTY)
	if (!no_process_group) {
		int i = sys_open("/dev/tty", O_RDWR, 0);
		if (i != -1) {
			ioctl(i, (int) TIOCNOTTY, (char *)0);
			close(i);
		}
	}
#endif /* HAVE_SETSID */

	if (!log_stdout) {
		/* Close fd's 0,1,2. Needed if started by rsh */
		close_low_fds(false);  /* Don't close stderr, let the debug system
					  attach it to the logfile */
	}
}
