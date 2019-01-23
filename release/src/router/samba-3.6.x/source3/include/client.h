/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Luke Kenneth Casson Leighton 1996-1998
   Copyright (C) Jeremy Allison 1998

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

#ifndef _CLIENT_H
#define _CLIENT_H

/* the client asks for a smaller buffer to save ram and also to get more
   overlap on the wire. This size gives us a nice read/write size, which
   will be a multiple of the page size on almost any system */
#define CLI_BUFFER_SIZE (0xFFFF)
#define CLI_SAMBA_MAX_LARGE_READX_SIZE (127*1024) /* Works for Samba servers */
#define CLI_SAMBA_MAX_LARGE_WRITEX_SIZE (127*1024) /* Works for Samba servers */
#define CLI_WINDOWS_MAX_LARGE_READX_SIZE ((64*1024)-2) /* Windows servers are broken.... */
#define CLI_WINDOWS_MAX_LARGE_WRITEX_SIZE ((64*1024)-2) /* Windows servers are broken.... */
#define CLI_SAMBA_MAX_POSIX_LARGE_READX_SIZE (0xFFFF00) /* 24-bit len. */
#define CLI_SAMBA_MAX_POSIX_LARGE_WRITEX_SIZE (0xFFFF00) /* 24-bit len. */

/*
 * These definitions depend on smb.h
 */

struct print_job_info {
	uint16 id;
	uint16 priority;
	size_t size;
	fstring user;
	fstring name;
	time_t t;
};

struct cli_state_seqnum {
	struct cli_state_seqnum *prev, *next;
	uint16_t mid;
	uint32_t seqnum;
	bool persistent;
};

struct cli_state {
	/**
	 * A list of subsidiary connections for DFS.
	 */
        struct cli_state *prev, *next;
	int port;
	int fd;
	/* Last read or write error. */
	enum smb_read_errors smb_rw_error;
	uint16 cnum;
	uint16 pid;
	uint16 mid;
	uint16 vuid;
	int protocol;
	int sec_mode;
	int rap_error;
	int privileges;

	char *desthost;

	/* The credentials used to open the cli_state connection. */
	char *domain;
	char *user_name;
	char *password; /* Can be null to force use of zero NTLMSSP session key. */

	/*
	 * The following strings are the
	 * ones returned by the server if
	 * the protocol > NT1.
	 */
	char *server_type;
	char *server_os;
	char *server_domain;

	char *share;
	char *dev;
	struct nmb_name called;
	struct nmb_name calling;
	struct sockaddr_storage dest_ss;

	DATA_BLOB secblob; /* cryptkey or negTokenInit */
	uint32 sesskey;
	int serverzone;
	uint32 servertime;
	int readbraw_supported;
	int writebraw_supported;
	int timeout; /* in milliseconds. */
	size_t max_xmit;
	size_t max_mux;
	char *outbuf;
	struct cli_state_seqnum *seqnum;
	char *inbuf;
	unsigned int bufsize;
	int initialised;
	int win95;
	bool is_samba;
	bool is_guestlogin;
	uint32 capabilities;
	/* What the server offered. */
	uint32_t server_posix_capabilities;
	/* What the client requested. */
	uint32_t requested_posix_capabilities;
	bool dfsroot;

	struct smb_signing_state *signing_state;

	struct smb_trans_enc_state *trans_enc_state; /* Setup if we're encrypting SMB's. */

	/* the session key for this CLI, outside
	   any per-pipe authenticaion */
	DATA_BLOB user_session_key;

	/* The list of pipes currently open on this connection. */
	struct rpc_pipe_client *pipe_list;

	bool use_kerberos;
	bool fallback_after_kerberos;
	bool use_spnego;
	bool use_ccache;
	bool got_kerberos_mechanism; /* Server supports krb5 in SPNEGO. */

	bool use_oplocks; /* should we use oplocks? */
	bool use_level_II_oplocks; /* should we use level II oplocks? */

	/* a oplock break request handler */
	NTSTATUS (*oplock_handler)(struct cli_state *cli, uint16_t fnum, unsigned char level);

	bool force_dos_errors;
	bool case_sensitive; /* False by default. */

	/* Where (if anywhere) this is mounted under DFS. */
	char *dfs_mountpoint;

	struct tevent_queue *outgoing;
	struct tevent_req **pending;
};

struct file_info {
	uint64_t size;
	uint16 mode;
	uid_t uid;
	gid_t gid;
	/* these times are normally kept in GMT */
	struct timespec mtime_ts;
	struct timespec atime_ts;
	struct timespec ctime_ts;
	char *name;
	char short_name[13*3]; /* the *3 is to cope with multi-byte */
};

#define CLI_FULL_CONNECTION_DONT_SPNEGO 0x0001
#define CLI_FULL_CONNECTION_USE_KERBEROS 0x0002
#define CLI_FULL_CONNECTION_ANONYMOUS_FALLBACK 0x0004
#define CLI_FULL_CONNECTION_FALLBACK_AFTER_KERBEROS 0x0008
#define CLI_FULL_CONNECTION_OPLOCKS 0x0010
#define CLI_FULL_CONNECTION_LEVEL_II_OPLOCKS 0x0020
#define CLI_FULL_CONNECTION_USE_CCACHE 0x0040

#endif /* _CLIENT_H */
