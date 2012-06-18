/* 
   Unix SMB/CIFS implementation.
   diagnosis tools for web admin
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

#ifdef WITH_WINBIND

/* check to see if winbind is running by pinging it */

bool winbindd_running(void)
{
	return winbind_ping();
}	
#endif

/* check to see if nmbd is running on localhost by looking for a __SAMBA__
   response */
bool nmbd_running(void)
{
	struct in_addr loopback_ip;
	int fd, count, flags;
	struct sockaddr_storage *ss_list;
	struct sockaddr_storage ss;

	loopback_ip.s_addr = htonl(INADDR_LOOPBACK);
	in_addr_to_sockaddr_storage(&ss, loopback_ip);

	if ((fd = open_socket_in(SOCK_DGRAM, 0, 3,
				 &ss, True)) != -1) {
		if ((ss_list = name_query(fd, "__SAMBA__", 0, 
					  True, True, &ss,
					  &count, &flags, NULL)) != NULL) {
			SAFE_FREE(ss_list);
			close(fd);
			return True;
		}
		close (fd);
	}

	return False;
}


/* check to see if smbd is running on localhost by trying to open a connection
   then closing it */
bool smbd_running(void)
{
	struct in_addr loopback_ip;
	NTSTATUS status;
	struct cli_state *cli;
	struct sockaddr_storage ss;

	loopback_ip.s_addr = htonl(INADDR_LOOPBACK);
	in_addr_to_sockaddr_storage(&ss, loopback_ip);

	if ((cli = cli_initialise()) == NULL)
		return False;

	status = cli_connect(cli, global_myname(), &ss);
	if (!NT_STATUS_IS_OK(status)) {
		cli_shutdown(cli);
		return False;
	}

	cli_shutdown(cli);
	return True;
}
