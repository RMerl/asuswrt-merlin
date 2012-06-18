/*
 * traffic-analyzer VFS module. Measure the smb traffic users create
 * on the net.
 *
 * Copyright (C) Holger Hetterich, 2008
 * Copyright (C) Jeremy Allison, 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

/* abstraction for the send_over_network function */

enum sock_type {INTERNET_SOCKET = 0, UNIX_DOMAIN_SOCKET};

#define LOCAL_PATHNAME "/var/tmp/stadsocket"

static int vfs_smb_traffic_analyzer_debug_level = DBGC_VFS;

static enum sock_type smb_traffic_analyzer_connMode(vfs_handle_struct *handle)
{
	connection_struct *conn = handle->conn;
        const char *Mode;
        Mode=lp_parm_const_string(SNUM(conn), "smb_traffic_analyzer","mode", \
			"internet_socket");
	if (strstr(Mode,"unix_domain_socket")) {
		return UNIX_DOMAIN_SOCKET;
	} else {
		return INTERNET_SOCKET;
	}
}


/* Connect to an internet socket */

static int smb_traffic_analyzer_connect_inet_socket(vfs_handle_struct *handle,
					const char *name, uint16_t port)
{
	/* Create a streaming Socket */
	int sockfd = -1;
	struct addrinfo hints;
	struct addrinfo *ailist = NULL;
	struct addrinfo *res = NULL;
	int ret;

	ZERO_STRUCT(hints);
	/* By default make sure it supports TCP. */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

	ret = getaddrinfo(name,
			NULL,
			&hints,
			&ailist);

        if (ret) {
		DEBUG(3,("smb_traffic_analyzer_connect_inet_socket: "
			"getaddrinfo failed for name %s [%s]\n",
                        name,
                        gai_strerror(ret) ));
		return -1;
        }

	DEBUG(3,("smb_traffic_analyzer: Internet socket mode. Hostname: %s,"
		"Port: %i\n", name, port));

	for (res = ailist; res; res = res->ai_next) {
		struct sockaddr_storage ss;
		NTSTATUS status;

		if (!res->ai_addr || res->ai_addrlen == 0) {
			continue;
		}

		ZERO_STRUCT(ss);
		memcpy(&ss, res->ai_addr, res->ai_addrlen);

		status = open_socket_out(&ss, port, 10000, &sockfd);
		if (NT_STATUS_IS_OK(status)) {
			break;
		}
	}

	if (ailist) {
		freeaddrinfo(ailist);
	}

        if (sockfd == -1) {
		DEBUG(1, ("smb_traffic_analyzer: unable to create "
			"socket, error is %s",
			strerror(errno)));
		return -1;
	}

	return sockfd;
}

/* Connect to a unix domain socket */

static int smb_traffic_analyzer_connect_unix_socket(vfs_handle_struct *handle,
						const char *name)
{
	/* Create the socket to stad */
	int len, sock;
	struct sockaddr_un remote;

	DEBUG(7, ("smb_traffic_analyzer_connect_unix_socket: "
			"Unix domain socket mode. Using %s\n",
			name ));

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		DEBUG(1, ("smb_traffic_analyzer_connect_unix_socket: "
			"Couldn't create socket, "
			"make sure stad is running!\n"));
		return -1;
	}
	remote.sun_family = AF_UNIX;
	strlcpy(remote.sun_path, name,
		    sizeof(remote.sun_path));
	len=strlen(remote.sun_path) + sizeof(remote.sun_family);
	if (connect(sock, (struct sockaddr *)&remote, len) == -1 ) {
		DEBUG(1, ("smb_traffic_analyzer_connect_unix_socket: "
			"Could not connect to "
			"socket, make sure\nstad is running!\n"));
		close(sock);
		return -1;
	}
	return sock;
}

/* Private data allowing shared connection sockets. */

struct refcounted_sock {
	struct refcounted_sock *next, *prev;
	char *name;
	uint16_t port;
	int sock;
	unsigned int ref_count;
};

/* Send data over a socket */

static void smb_traffic_analyzer_send_data(vfs_handle_struct *handle,
					ssize_t result,
					const char *file_name,
					bool Write)
{
	struct refcounted_sock *rf_sock = NULL;
	struct timeval tv;
	time_t tv_sec;
	struct tm *tm = NULL;
	int seconds;
	char *str = NULL;
	char *username = NULL;
	const char *anon_prefix = NULL;
	const char *total_anonymization = NULL;
	size_t len;

	SMB_VFS_HANDLE_GET_DATA(handle, rf_sock, struct refcounted_sock, return);

	if (rf_sock == NULL || rf_sock->sock == -1) {
		DEBUG(1, ("smb_traffic_analyzer_send_data: socket is "
			"closed\n"));
		return;
	}

	GetTimeOfDay(&tv);
	tv_sec = convert_timespec_to_time_t(convert_timeval_to_timespec(tv));
	tm = localtime(&tv_sec);
	if (!tm) {
		return;
	}
	seconds=(float) (tv.tv_usec / 1000);

	/* check if anonymization is required */

	total_anonymization=lp_parm_const_string(SNUM(handle->conn),"smb_traffic_analyzer",
					"total_anonymization", NULL);

	anon_prefix=lp_parm_const_string(SNUM(handle->conn),"smb_traffic_analyzer",\
					"anonymize_prefix", NULL );
	if (anon_prefix!=NULL) {
		if (total_anonymization!=NULL) {
			username = talloc_asprintf(talloc_tos(),
				"%s",
				anon_prefix);
		} else {
			username = talloc_asprintf(talloc_tos(),
				"%s%i",
				anon_prefix,
				str_checksum(
					handle->conn->server_info->sanitized_username )	); 
		}

	} else {
		username = handle->conn->server_info->sanitized_username;
	}

	if (!username) {
		return;
	}

	str = talloc_asprintf(talloc_tos(),
			"V1,%u,\"%s\",\"%s\",\"%c\",\"%s\",\"%s\","
			"\"%04d-%02d-%02d %02d:%02d:%02d.%03d\"\n",
			(unsigned int)result,
			username,
			pdb_get_domain(handle->conn->server_info->sam_account),
			Write ? 'W' : 'R',
			handle->conn->connectpath,
			file_name,
			tm->tm_year+1900,
			tm->tm_mon+1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			(int)seconds);

	if (!str) {
		return;
	}

	len = strlen(str);

	DEBUG(10, ("smb_traffic_analyzer_send_data_socket: sending %s\n",
			str));
	if (write_data(rf_sock->sock, str, len) != len) {
		DEBUG(1, ("smb_traffic_analyzer_send_data_socket: "
			"error sending data to socket!\n"));
		return ;
	}
}

static struct refcounted_sock *sock_list;

static void smb_traffic_analyzer_free_data(void **pptr)
{
	struct refcounted_sock *rf_sock = *(struct refcounted_sock **)pptr;
	if (rf_sock == NULL) {
		return;
	}
	rf_sock->ref_count--;
	if (rf_sock->ref_count != 0) {
		return;
	}
	if (rf_sock->sock != -1) {
		close(rf_sock->sock);
	}
	DLIST_REMOVE(sock_list, rf_sock);
	TALLOC_FREE(rf_sock);
}

static int smb_traffic_analyzer_connect(struct vfs_handle_struct *handle,
                         const char *service,
                         const char *user)
{
	connection_struct *conn = handle->conn;
	enum sock_type st = smb_traffic_analyzer_connMode(handle);
	struct refcounted_sock *rf_sock = NULL;
	const char *name = (st == UNIX_DOMAIN_SOCKET) ? LOCAL_PATHNAME :
				lp_parm_const_string(SNUM(conn),
					"smb_traffic_analyzer",
				"host", "localhost");
	uint16_t port = (st == UNIX_DOMAIN_SOCKET) ? 0 :
				atoi( lp_parm_const_string(SNUM(conn),
				"smb_traffic_analyzer", "port", "9430"));
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	/* Are we already connected ? */
	for (rf_sock = sock_list; rf_sock; rf_sock = rf_sock->next) {
		if (port == rf_sock->port &&
				(strcmp(name, rf_sock->name) == 0)) {
			break;
		}
	}

	/* If we're connected already, just increase the
 	 * reference count. */
	if (rf_sock) {
		rf_sock->ref_count++;
	} else {
		/* New connection. */
		rf_sock = TALLOC_ZERO_P(NULL, struct refcounted_sock);
		if (rf_sock == NULL) {
			SMB_VFS_NEXT_DISCONNECT(handle);
			errno = ENOMEM;
			return -1;
		}
		rf_sock->name = talloc_strdup(rf_sock, name);
		if (rf_sock->name == NULL) {
			SMB_VFS_NEXT_DISCONNECT(handle);
			TALLOC_FREE(rf_sock);
			errno = ENOMEM;
			return -1;
		}
		rf_sock->port = port;
		rf_sock->ref_count = 1;

		if (st == UNIX_DOMAIN_SOCKET) {
			rf_sock->sock = smb_traffic_analyzer_connect_unix_socket(handle,
							name);
		} else {

			rf_sock->sock = smb_traffic_analyzer_connect_inet_socket(handle,
							name,
							port);
		}
		if (rf_sock->sock == -1) {
			SMB_VFS_NEXT_DISCONNECT(handle);
			TALLOC_FREE(rf_sock);
			return -1;
		}
		DLIST_ADD(sock_list, rf_sock);
	}

	/* Store the private data. */
	SMB_VFS_HANDLE_SET_DATA(handle, rf_sock, smb_traffic_analyzer_free_data,
				struct refcounted_sock, return -1);
	return 0;
}

/* VFS Functions: write, read, pread, pwrite for now */

static ssize_t smb_traffic_analyzer_read(vfs_handle_struct *handle, \
				files_struct *fsp, void *data, size_t n)
{
	ssize_t result;

	result = SMB_VFS_NEXT_READ(handle, fsp, data, n);
	DEBUG(10, ("smb_traffic_analyzer_read: READ: %s\n", fsp_str_dbg(fsp)));

	smb_traffic_analyzer_send_data(handle,
			result,
			fsp->fsp_name->base_name,
			false);
	return result;
}


static ssize_t smb_traffic_analyzer_pread(vfs_handle_struct *handle, \
		files_struct *fsp, void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t result;

	result = SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);

	DEBUG(10, ("smb_traffic_analyzer_pread: PREAD: %s\n",
		   fsp_str_dbg(fsp)));

	smb_traffic_analyzer_send_data(handle,
			result,
			fsp->fsp_name->base_name,
			false);

	return result;
}

static ssize_t smb_traffic_analyzer_write(vfs_handle_struct *handle, \
			files_struct *fsp, const void *data, size_t n)
{
	ssize_t result;

	result = SMB_VFS_NEXT_WRITE(handle, fsp, data, n);

	DEBUG(10, ("smb_traffic_analyzer_write: WRITE: %s\n",
		   fsp_str_dbg(fsp)));

	smb_traffic_analyzer_send_data(handle,
			result,
			fsp->fsp_name->base_name,
			true);
	return result;
}

static ssize_t smb_traffic_analyzer_pwrite(vfs_handle_struct *handle, \
	     files_struct *fsp, const void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t result;

	result = SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);

	DEBUG(10, ("smb_traffic_analyzer_pwrite: PWRITE: %s\n", fsp_str_dbg(fsp)));

	smb_traffic_analyzer_send_data(handle,
			result,
			fsp->fsp_name->base_name,
			true);
	return result;
}

static struct vfs_fn_pointers vfs_smb_traffic_analyzer_fns = {
        .connect_fn = smb_traffic_analyzer_connect,
	.vfs_read = smb_traffic_analyzer_read,
	.pread = smb_traffic_analyzer_pread,
	.write = smb_traffic_analyzer_write,
	.pwrite = smb_traffic_analyzer_pwrite,
};

/* Module initialization */

NTSTATUS vfs_smb_traffic_analyzer_init(void)
{
	NTSTATUS ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
					"smb_traffic_analyzer",
					&vfs_smb_traffic_analyzer_fns);

	if (!NT_STATUS_IS_OK(ret)) {
		return ret;
	}

	vfs_smb_traffic_analyzer_debug_level =
		debug_add_class("smb_traffic_analyzer");

	if (vfs_smb_traffic_analyzer_debug_level == -1) {
		vfs_smb_traffic_analyzer_debug_level = DBGC_VFS;
		DEBUG(1, ("smb_traffic_analyzer_init: Couldn't register custom"
			 "debugging class!\n"));
	} else {
		DEBUG(3, ("smb_traffic_analyzer_init: Debug class number of"
			"'smb_traffic_analyzer': %d\n", \
			vfs_smb_traffic_analyzer_debug_level));
	}

	return ret;
}
