/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   start/stop nmbd and smbd
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
#include "smb.h"

/* need to wait for daemons to startup */
#define SLEEP_TIME 3

/* startup smbd */
void start_smbd(void)
{
	pstring binfile;

	if (geteuid() != 0) return;

	if (fork()) {
		sleep(SLEEP_TIME);
		return;
	}

	slprintf(binfile, sizeof(pstring) - 1, "%s/smbd", SBINDIR);

	become_daemon();

	execl(binfile, binfile, "-D", NULL);

	exit(0);
}

/* startup nmbd */
void start_nmbd(void)
{
	pstring binfile;

	if (geteuid() != 0) return;

	if (fork()) {
		sleep(SLEEP_TIME);
		return;
	}

	slprintf(binfile, sizeof(pstring) - 1, "%s/nmbd", SBINDIR);
	
	become_daemon();

	execl(binfile, binfile, "-D", NULL);

	exit(0);
}


/* stop smbd */
void stop_smbd(void)
{
	pid_t pid = pidfile_pid("smbd");

	if (geteuid() != 0) return;

	if (pid == 0) return;

	kill(pid, SIGTERM);
}

/* stop nmbd */
void stop_nmbd(void)
{
	pid_t pid = pidfile_pid("nmbd");

	if (geteuid() != 0) return;

	if (pid == 0) return;

	kill(pid, SIGTERM);
}

/* kill a specified process */
void kill_pid(pid_t pid)
{
	if (geteuid() != 0) return;

	if (pid <= 0) return;

	kill(pid, SIGTERM);
	sleep(SLEEP_TIME);
}
