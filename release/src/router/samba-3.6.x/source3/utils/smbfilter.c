/* 
   Unix SMB/CIFS implementation.
   SMB filter/socket plugin
   Copyright (C) Andrew Tridgell 1999

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
#include "system/select.h"
#include "../lib/util/select.h"
#include "libsmb/nmblib.h"

#define SECURITY_MASK 0
#define SECURITY_SET  0

/* this forces non-unicode */
#define CAPABILITY_MASK 0
#define CAPABILITY_SET  0

/* and non-unicode for the client too */
#define CLI_CAPABILITY_MASK 0
#define CLI_CAPABILITY_SET  0

static char *netbiosname;
static char packet[BUFFER_SIZE];

static void save_file(const char *fname, void *ppacket, size_t length)
{
	int fd;
	fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		perror(fname);
		return;
	}
	if (write(fd, ppacket, length) != length) {
		fprintf(stderr,"Failed to write %s\n", fname);
		close(fd);
		return;
	}
	close(fd);
	printf("Wrote %ld bytes to %s\n", (unsigned long)length, fname);
}

static void filter_reply(char *buf)
{
	int msg_type = CVAL(buf,0);
	int type = CVAL(buf,smb_com);
	unsigned x;

	if (msg_type) return;

	switch (type) {

	case SMBnegprot:
		/* force the security bits */
		x = CVAL(buf, smb_vwv1);
		x = (x | SECURITY_SET) & ~SECURITY_MASK;
		SCVAL(buf, smb_vwv1, x);

		/* force the capabilities */
		x = IVAL(buf,smb_vwv9+1);
		x = (x | CAPABILITY_SET) & ~CAPABILITY_MASK;
		SIVAL(buf, smb_vwv9+1, x);
		break;

	}
}

static void filter_request(char *buf, size_t buf_len)
{
	int msg_type = CVAL(buf,0);
	int type = CVAL(buf,smb_com);
	unsigned x;
	fstring name1,name2;
	int name_len1, name_len2;
	int name_type1, name_type2;

	if (msg_type) {
		/* it's a netbios special */
		switch (msg_type)
		case 0x81:
			/* session request */
			/* inbuf_size is guaranteed to be at least 4. */
			name_len1 = name_len((unsigned char *)(buf+4),
					buf_len - 4);
			if (name_len1 <= 0 || name_len1 > buf_len - 4) {
				DEBUG(0,("Invalid name length in session request\n"));
				return;
			}
			name_len2 = name_len((unsigned char *)(buf+4+name_len1),
					buf_len - 4 - name_len1);
			if (name_len2 <= 0 || name_len2 > buf_len - 4 - name_len1) {
				DEBUG(0,("Invalid name length in session request\n"));
				return;
			}

			name_type1 = name_extract((unsigned char *)buf,
					buf_len,(unsigned int)4,name1);
			name_type2 = name_extract((unsigned char *)buf,
					buf_len,(unsigned int)(4 + name_len1),name2);

			if (name_type1 == -1 || name_type2 == -1) {
				DEBUG(0,("Invalid name type in session request\n"));
				return;
			}

			d_printf("sesion_request: %s -> %s\n",
				 name1, name2);
			if (netbiosname) {
				char *mangled = name_mangle(
					talloc_tos(), netbiosname, 0x20);
				if (mangled != NULL) {
					/* replace the destination netbios
					 * name */
					memcpy(buf+4, mangled,
					       name_len((unsigned char *)mangled,
							talloc_get_size(mangled)));
					TALLOC_FREE(mangled);
				}
			}
		return;
	}

	/* it's an ordinary SMB request */
	switch (type) {
	case SMBsesssetupX:
		/* force the client capabilities */
		x = IVAL(buf,smb_vwv11);
		d_printf("SMBsesssetupX cap=0x%08x\n", x);
		d_printf("pwlen=%d/%d\n", SVAL(buf, smb_vwv7), SVAL(buf, smb_vwv8));
		system("mv sessionsetup.dat sessionsetup1.dat");
		save_file("sessionsetup.dat", smb_buf(buf), SVAL(buf, smb_vwv7));
		x = (x | CLI_CAPABILITY_SET) & ~CLI_CAPABILITY_MASK;
		SIVAL(buf, smb_vwv11, x);
		break;
	}
}

/****************************************************************************
 Send an smb to a fd.
****************************************************************************/

static bool send_smb(int fd, char *buffer)
{
	size_t len;
	size_t nwritten=0;
	ssize_t ret;

        len = smb_len(buffer) + 4;

	while (nwritten < len) {
		ret = write_data(fd,buffer+nwritten,len - nwritten);
		if (ret <= 0) {
			DEBUG(0,("Error writing %d bytes to client. %d. (%s)\n",
				(int)len,(int)ret, strerror(errno) ));
			return false;
		}
		nwritten += ret;
	}

	return true;
}

static void filter_child(int c, struct sockaddr_storage *dest_ss)
{
	NTSTATUS status;
	int s = -1;

	/* we have a connection from a new client, now connect to the server */
	status = open_socket_out(dest_ss, 445, LONG_CONNECT_TIMEOUT, &s);

	if (s == -1) {
		char addr[INET6_ADDRSTRLEN];
		if (dest_ss) {
			print_sockaddr(addr, sizeof(addr), dest_ss);
		}

		d_printf("Unable to connect to %s (%s)\n",
			 dest_ss?addr:"NULL",strerror(errno));
		exit(1);
	}

	while (c != -1 || s != -1) {
		struct pollfd fds[2];
		int num_fds, ret;

		memset(fds, 0, sizeof(struct pollfd) * 2);
		fds[0].fd = -1;
		fds[1].fd = -1;
		num_fds = 0;

		if (s != -1) {
			fds[num_fds].fd = s;
			fds[num_fds].events = POLLIN|POLLHUP;
			num_fds += 1;
		}
		if (c != -1) {
			fds[num_fds].fd = c;
			fds[num_fds].events = POLLIN|POLLHUP;
			num_fds += 1;
		}

		ret = sys_poll_intr(fds, num_fds, -1);
		if (ret <= 0) {
			continue;
		}

		/*
		 * find c in fds and see if it's readable
		 */
		if ((c != -1) &&
		    (((fds[0].fd == c)
		      && (fds[0].revents & (POLLIN|POLLHUP|POLLERR))) ||
		     ((fds[1].fd == c)
		      && (fds[1].revents & (POLLIN|POLLHUP|POLLERR))))) {
			size_t len;
			if (!NT_STATUS_IS_OK(receive_smb_raw(
							c, packet, sizeof(packet),
							0, 0, &len))) {
				d_printf("client closed connection\n");
				exit(0);
			}
			filter_request(packet, len);
			if (!send_smb(s, packet)) {
				d_printf("server is dead\n");
				exit(1);
			}			
		}

		/*
		 * find s in fds and see if it's readable
		 */
		if ((s != -1) &&
		    (((fds[0].fd == s)
		      && (fds[0].revents & (POLLIN|POLLHUP|POLLERR))) ||
		     ((fds[1].fd == s)
		      && (fds[1].revents & (POLLIN|POLLHUP|POLLERR))))) {
			size_t len;
			if (!NT_STATUS_IS_OK(receive_smb_raw(
							s, packet, sizeof(packet),
							0, 0, &len))) {
				d_printf("server closed connection\n");
				exit(0);
			}
			filter_reply(packet);
			if (!send_smb(c, packet)) {
				d_printf("client is dead\n");
				exit(1);
			}			
		}
	}
	d_printf("Connection closed\n");
	exit(0);
}


static void start_filter(char *desthost)
{
	int s, c;
	struct sockaddr_storage dest_ss;
	struct sockaddr_storage my_ss;

	CatchChild();

	/* start listening on port 445 locally */

	zero_sockaddr(&my_ss);
	s = open_socket_in(SOCK_STREAM, 445, 0, &my_ss, True);

	if (s == -1) {
		d_printf("bind failed\n");
		exit(1);
	}

	if (listen(s, 5) == -1) {
		d_printf("listen failed\n");
	}

	if (!resolve_name(desthost, &dest_ss, 0x20, false)) {
		d_printf("Unable to resolve host %s\n", desthost);
		exit(1);
	}

	while (1) {
		int num, revents;
		struct sockaddr_storage ss;
		socklen_t in_addrlen = sizeof(ss);

		num = poll_intr_one_fd(s, POLLIN|POLLHUP, -1, &revents);
		if ((num > 0) && (revents & (POLLIN|POLLHUP|POLLERR))) {
			c = accept(s, (struct sockaddr *)&ss, &in_addrlen);
			if (c != -1) {
				if (fork() == 0) {
					close(s);
					filter_child(c, &dest_ss);
					exit(0);
				} else {
					close(c);
				}
			}
		}
	}
}


int main(int argc, char *argv[])
{
	char *desthost;
	const char *configfile;
	TALLOC_CTX *frame = talloc_stackframe();

	load_case_tables();

	setup_logging(argv[0], DEBUG_STDOUT);

	configfile = get_dyn_CONFIGFILE();

	if (argc < 2) {
		fprintf(stderr,"smbfilter <desthost> <netbiosname>\n");
		exit(1);
	}

	desthost = argv[1];
	if (argc > 2) {
		netbiosname = argv[2];
	}

	if (!lp_load(configfile,True,False,False,True)) {
		d_printf("Unable to load config file\n");
	}

	start_filter(desthost);
	TALLOC_FREE(frame);
	return 0;
}
