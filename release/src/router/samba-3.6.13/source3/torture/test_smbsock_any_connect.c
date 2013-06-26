/*
   Unix SMB/CIFS implementation.
   Test the smb_any_connect functionality
   Copyright (C) Volker Lendecke 2010

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
#include "torture/proto.h"

bool run_smb_any_connect(int dummy)
{
	int fd;
	NTSTATUS status;
	struct sockaddr_storage addrs[5];
	size_t chosen_index;
	uint16_t port;

	interpret_string_addr(&addrs[0], "192.168.99.5", 0);
	interpret_string_addr(&addrs[1], "192.168.99.6", 0);
	interpret_string_addr(&addrs[2], "192.168.99.7", 0);
	interpret_string_addr(&addrs[3], "192.168.99.8", 0);
	interpret_string_addr(&addrs[4], "192.168.99.9", 0);

	status = smbsock_any_connect(addrs, NULL, NULL, NULL, NULL,
				     ARRAY_SIZE(addrs), 0, 0,
				     &fd, &chosen_index, &port);

	d_printf("smbsock_any_connect returned %s (fd %d)\n",
		 nt_errstr(status), NT_STATUS_IS_OK(status) ? fd : -1);
	if (NT_STATUS_IS_OK(status)) {
		close(fd);
	}
	return true;
}
