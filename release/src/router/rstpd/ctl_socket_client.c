/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#include "ctl_socket.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/poll.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

static int fd = -1;

int ctl_client_init(void)
{
	struct sockaddr_un sa_svr;
	int s;
	TST(strlen(RSTP_SERVER_SOCK_NAME) < sizeof(sa_svr.sun_path), -1);

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		ERROR("Couldn't open unix socket: %m");
		return -1;
	}

	set_socket_address(&sa_svr, RSTP_SERVER_SOCK_NAME);

	struct sockaddr_un sa;
	char tmpname[64];
	sprintf(tmpname, "RSTPCTL_%d", getpid());
	set_socket_address(&sa, tmpname);
	/* We need this bind. The autobind on connect isn't working properly.
	   The server doesn't get a proper sockaddr in recvmsg if we don't do this.
	 */
	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		ERROR("Couldn't bind socket: %m");
		close(s);
		return -1;
	}

	if (connect(s, (struct sockaddr *)&sa_svr, sizeof(sa_svr)) != 0) {
		ERROR("Couldn't connect to server");
		close(s);
		return -1;
	}
	fd = s;

	return 0;
}

void ctl_client_cleanup(void)
{
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
}

int send_ctl_message(int cmd, void *inbuf, int lin, void *outbuf, int *lout,
		     int *res)
{
	struct ctl_msg_hdr mhdr;
	struct msghdr msg;
	struct iovec iov[2];
	int l;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	mhdr.cmd = cmd;
	mhdr.lin = lin;
	mhdr.lout = lout != NULL ? *lout : 0;
	iov[0].iov_base = &mhdr;
	iov[0].iov_len = sizeof(mhdr);
	iov[1].iov_base = (void *)inbuf;
	iov[1].iov_len = lin;

	l = sendmsg(fd, &msg, 0);
	if (l < 0) {
		ERROR("Error sending message to server: %m");
		return -1;
	}
	if (l != sizeof(mhdr) + lin) {
		ERROR("Error sending message to server: Partial write");
		return -1;
	}

	iov[1].iov_base = outbuf;
	iov[1].iov_len = lout != NULL ? *lout : 0;

	{
		struct pollfd pfd;
		int timeout = 5000;	/* 5 s */
		int r;

		pfd.fd = fd;
		pfd.events = POLLIN;
		do {
			r = poll(&pfd, 1, timeout);
			if (r == 0) {
				ERROR
				    ("Error getting message from server: Timeout");
				return -1;
			}
			if (r < 0) {
				ERROR
				    ("Error getting message from server: poll error: %m");
				return -1;
			}
		} while ((pfd.
			  revents & (POLLERR | POLLHUP | POLLNVAL | POLLIN)) ==
			 0);

		l = recvmsg(fd, &msg, 0);
		if (l < 0) {
			ERROR("Error getting message from server: %m");
			return -1;
		}
		if (l < sizeof(mhdr) || l != sizeof(mhdr) + mhdr.lout
		    || mhdr.cmd != cmd) {
			ERROR("Error getting message from server: Bad format");
			return -1;
		}
	}
	if (lout)
		*lout = mhdr.lout;
	if (res)
		*res = mhdr.res;

	return 0;
}
