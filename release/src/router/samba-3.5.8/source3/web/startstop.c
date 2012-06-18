/* 
   Unix SMB/CIFS implementation.
   start/stop nmbd and smbd
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
#include "web/swat_proto.h"
#include "dynconfig.h"


/** Startup smbd from web interface. */
void start_smbd(void)
{
	char *binfile = NULL;

	if (geteuid() != 0) {
		 return;
	}

	if (fork()) {
		return;
	}

	if (asprintf(&binfile, "%s/smbd", get_dyn_SBINDIR()) > 0) {
		become_daemon(true, false);
		execl(binfile, binfile, "-D", NULL);
	}
	exit(0);
}

/* startup nmbd */
void start_nmbd(void)
{
	char *binfile = NULL;

	if (geteuid() != 0) {
		return;
	}

	if (fork()) {
		return;
	}

	if (asprintf(&binfile, "%s/nmbd", get_dyn_SBINDIR()) > 0) {
		become_daemon(true, false);
		execl(binfile, binfile, "-D", NULL);
	}
	exit(0);
}

/** Startup winbindd from web interface. */
void start_winbindd(void)
{
	char *binfile = NULL;

	if (geteuid() != 0) {
		return;
	}

	if (fork()) {
		return;
	}

	if (asprintf(&binfile, "%s/winbindd", get_dyn_SBINDIR()) > 0) {
		become_daemon(true, false);
		execl(binfile, binfile, NULL);
	}
	exit(0);
}


/* stop smbd */
void stop_smbd(void)
{
	pid_t pid = pidfile_pid("smbd");

	if (geteuid() != 0) return;

	if (pid <= 0) return;

	kill(pid, SIGTERM);
}

/* stop nmbd */
void stop_nmbd(void)
{
	pid_t pid = pidfile_pid("nmbd");

	if (geteuid() != 0) return;

	if (pid <= 0) return;

	kill(pid, SIGTERM);
}
#ifdef WITH_WINBIND
/* stop winbindd */
void stop_winbindd(void)
{
	pid_t pid = pidfile_pid("winbindd");

	if (geteuid() != 0) return;

	if (pid <= 0) return;

	kill(pid, SIGTERM);
}
#endif
/* kill a specified process */
void kill_pid(struct server_id pid)
{
	if (geteuid() != 0) return;

	if (procid_to_pid(&pid) <= 0) return;

	kill(procid_to_pid(&pid), SIGTERM);
}
