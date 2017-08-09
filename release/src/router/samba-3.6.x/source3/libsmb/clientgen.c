/* 
   Unix SMB/CIFS implementation.
   SMB client generic functions
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Jeremy Allison 2007.
   
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
#include "libsmb/libsmb.h"
#include "../lib/util/tevent_ntstatus.h"
#include "smb_signing.h"
#include "async_smb.h"

/*******************************************************************
 Setup the word count and byte count for a client smb message.
********************************************************************/

int cli_set_message(char *buf,int num_words,int num_bytes,bool zero)
{
	if (zero && (num_words || num_bytes)) {
		memset(buf + smb_size,'\0',num_words*2 + num_bytes);
	}
	SCVAL(buf,smb_wct,num_words);
	SSVAL(buf,smb_vwv + num_words*SIZEOFWORD,num_bytes);
	smb_setlen(buf,smb_size + num_words*2 + num_bytes - 4);
	return (smb_size + num_words*2 + num_bytes);
}

/****************************************************************************
 Change the timeout (in milliseconds).
****************************************************************************/

unsigned int cli_set_timeout(struct cli_state *cli, unsigned int timeout)
{
	unsigned int old_timeout = cli->timeout;
	cli->timeout = timeout;
	return old_timeout;
}

/****************************************************************************
 Change the port number used to call on.
****************************************************************************/

void cli_set_port(struct cli_state *cli, int port)
{
	cli->port = port;
}

/****************************************************************************
 convenience routine to find if we negotiated ucs2
****************************************************************************/

bool cli_ucs2(struct cli_state *cli)
{
	return ((cli->capabilities & CAP_UNICODE) != 0);
}


/****************************************************************************
 Read an smb from a fd ignoring all keepalive packets.
 The timeout is in milliseconds

 This is exactly the same as receive_smb except that it never returns
 a session keepalive packet (just as receive_smb used to do).
 receive_smb was changed to return keepalives as the oplock processing means this call
 should never go into a blocking read.
****************************************************************************/

static ssize_t client_receive_smb(struct cli_state *cli, size_t maxlen)
{
	size_t len;

	for(;;) {
		NTSTATUS status;

		set_smb_read_error(&cli->smb_rw_error, SMB_READ_OK);

		status = receive_smb_raw(cli->fd, cli->inbuf, cli->bufsize,
					cli->timeout, maxlen, &len);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10,("client_receive_smb failed\n"));
			show_msg(cli->inbuf);

			if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
				set_smb_read_error(&cli->smb_rw_error,
						   SMB_READ_EOF);
				return -1;
			}

			if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
				set_smb_read_error(&cli->smb_rw_error,
						   SMB_READ_TIMEOUT);
				return -1;
			}

			set_smb_read_error(&cli->smb_rw_error, SMB_READ_ERROR);
			return -1;
		}

		/*
		 * I don't believe len can be < 0 with NT_STATUS_OK
		 * returned above, but this check doesn't hurt. JRA.
		 */

		if ((ssize_t)len < 0) {
			return len;
		}

		/* Ignore session keepalive packets. */
		if(CVAL(cli->inbuf,0) != SMBkeepalive) {
			break;
		}
	}

	if (cli_encryption_on(cli)) {
		NTSTATUS status = cli_decrypt_message(cli);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("SMB decryption failed on incoming packet! Error %s\n",
				nt_errstr(status)));
			cli->smb_rw_error = SMB_READ_BAD_DECRYPT;
			return -1;
		}
	}

	show_msg(cli->inbuf);
	return len;
}

static bool cli_state_set_seqnum(struct cli_state *cli, uint16_t mid, uint32_t seqnum)
{
	struct cli_state_seqnum *c;

	for (c = cli->seqnum; c; c = c->next) {
		if (c->mid == mid) {
			c->seqnum = seqnum;
			return true;
		}
	}

	c = talloc_zero(cli, struct cli_state_seqnum);
	if (!c) {
		return false;
	}

	c->mid = mid;
	c->seqnum = seqnum;
	c->persistent = false;
	DLIST_ADD_END(cli->seqnum, c, struct cli_state_seqnum *);

	return true;
}

bool cli_state_seqnum_persistent(struct cli_state *cli,
				 uint16_t mid)
{
	struct cli_state_seqnum *c;

	for (c = cli->seqnum; c; c = c->next) {
		if (c->mid == mid) {
			c->persistent = true;
			return true;
		}
	}

	return false;
}

bool cli_state_seqnum_remove(struct cli_state *cli,
			     uint16_t mid)
{
	struct cli_state_seqnum *c;

	for (c = cli->seqnum; c; c = c->next) {
		if (c->mid == mid) {
			DLIST_REMOVE(cli->seqnum, c);
			TALLOC_FREE(c);
			return true;
		}
	}

	return false;
}

static uint32_t cli_state_get_seqnum(struct cli_state *cli, uint16_t mid)
{
	struct cli_state_seqnum *c;

	for (c = cli->seqnum; c; c = c->next) {
		if (c->mid == mid) {
			uint32_t seqnum = c->seqnum;
			if (!c->persistent) {
				DLIST_REMOVE(cli->seqnum, c);
				TALLOC_FREE(c);
			}
			return seqnum;
		}
	}

	return 0;
}

/****************************************************************************
 Recv an smb.
****************************************************************************/

bool cli_receive_smb(struct cli_state *cli)
{
	ssize_t len;
	uint16_t mid;
	uint32_t seqnum;

	/* fd == -1 causes segfaults -- Tom (tom@ninja.nl) */
	if (cli->fd == -1)
		return false; 

 again:
	len = client_receive_smb(cli, 0);
	
	if (len > 0) {
		/* it might be an oplock break request */
		if (!(CVAL(cli->inbuf, smb_flg) & FLAG_REPLY) &&
		    CVAL(cli->inbuf,smb_com) == SMBlockingX &&
		    SVAL(cli->inbuf,smb_vwv6) == 0 &&
		    SVAL(cli->inbuf,smb_vwv7) == 0) {
			if (cli->oplock_handler) {
				int fnum = SVAL(cli->inbuf,smb_vwv2);
				unsigned char level = CVAL(cli->inbuf,smb_vwv3+1);
				if (!NT_STATUS_IS_OK(cli->oplock_handler(cli, fnum, level))) {
					return false;
				}
			}
			/* try to prevent loops */
			SCVAL(cli->inbuf,smb_com,0xFF);
			goto again;
		}
	}

	/* If the server is not responding, note that now */
	if (len < 0) {
		/*
		 * only log if the connection should still be open and not when
		 * the connection was closed due to a dropped ip message
		 */
		if (cli->fd != -1) {
			char addr[INET6_ADDRSTRLEN];
			print_sockaddr(addr, sizeof(addr), &cli->dest_ss);
			DEBUG(0, ("Receiving SMB: Server %s stopped responding\n",
				addr));
			close(cli->fd);
			cli->fd = -1;
		}
		return false;
	}

	mid = SVAL(cli->inbuf,smb_mid);
	seqnum = cli_state_get_seqnum(cli, mid);

	if (!cli_check_sign_mac(cli, cli->inbuf, seqnum+1)) {
		/*
		 * If we get a signature failure in sessionsetup, then
		 * the server sometimes just reflects the sent signature
		 * back to us. Detect this and allow the upper layer to
		 * retrieve the correct Windows error message.
		 */
		if (CVAL(cli->outbuf,smb_com) == SMBsesssetupX &&
			(smb_len(cli->inbuf) > (smb_ss_field + 8 - 4)) &&
			(SVAL(cli->inbuf,smb_flg2) & FLAGS2_SMB_SECURITY_SIGNATURES) &&
			memcmp(&cli->outbuf[smb_ss_field],&cli->inbuf[smb_ss_field],8) == 0 &&
			cli_is_error(cli)) {

			/*
			 * Reflected signature on login error. 
			 * Set bad sig but don't close fd.
			 */
			cli->smb_rw_error = SMB_READ_BAD_SIG;
			return true;
		}

		DEBUG(0, ("SMB Signature verification failed on incoming packet!\n"));
		cli->smb_rw_error = SMB_READ_BAD_SIG;
		close(cli->fd);
		cli->fd = -1;
		return false;
	};
	return true;
}

static ssize_t write_socket(int fd, const char *buf, size_t len)
{
        ssize_t ret=0;

        DEBUG(6,("write_socket(%d,%d)\n",fd,(int)len));
        ret = write_data(fd,buf,len);

        DEBUG(6,("write_socket(%d,%d) wrote %d\n",fd,(int)len,(int)ret));
        if(ret <= 0)
                DEBUG(0,("write_socket: Error writing %d bytes to socket %d: ERRNO = %s\n",
                        (int)len, fd, strerror(errno) ));

        return(ret);
}

/****************************************************************************
 Send an smb to a fd.
****************************************************************************/

bool cli_send_smb(struct cli_state *cli)
{
	size_t len;
	size_t nwritten=0;
	ssize_t ret;
	char *buf_out = cli->outbuf;
	bool enc_on = cli_encryption_on(cli);
	uint32_t seqnum;

	/* fd == -1 causes segfaults -- Tom (tom@ninja.nl) */
	if (cli->fd == -1)
		return false;

	cli_calculate_sign_mac(cli, cli->outbuf, &seqnum);

	if (!cli_state_set_seqnum(cli, cli->mid, seqnum)) {
		DEBUG(0,("Failed to store mid[%u]/seqnum[%u]\n",
			(unsigned int)cli->mid,
			(unsigned int)seqnum));
		return false;
	}

	if (enc_on) {
		NTSTATUS status = cli_encrypt_message(cli, cli->outbuf,
						      &buf_out);
		if (!NT_STATUS_IS_OK(status)) {
			close(cli->fd);
			cli->fd = -1;
			cli->smb_rw_error = SMB_WRITE_ERROR;
			DEBUG(0,("Error in encrypting client message. Error %s\n",
				nt_errstr(status) ));
			return false;
		}
	}

	len = smb_len(buf_out) + 4;

	while (nwritten < len) {
		ret = write_socket(cli->fd,buf_out+nwritten,len - nwritten);
		if (ret <= 0) {
			if (enc_on) {
				cli_free_enc_buffer(cli, buf_out);
			}
			close(cli->fd);
			cli->fd = -1;
			cli->smb_rw_error = SMB_WRITE_ERROR;
			DEBUG(0,("Error writing %d bytes to client. %d (%s)\n",
				(int)len,(int)ret, strerror(errno) ));
			return false;
		}
		nwritten += ret;
	}

	if (enc_on) {
		cli_free_enc_buffer(cli, buf_out);
	}

	/* Increment the mid so we can tell between responses. */
	cli->mid++;
	if (!cli->mid)
		cli->mid++;
	return true;
}

/****************************************************************************
 Setup basics in a outgoing packet.
****************************************************************************/

void cli_setup_packet_buf(struct cli_state *cli, char *buf)
{
	uint16 flags2;
	cli->rap_error = 0;
	SIVAL(buf,smb_rcls,0);
	SSVAL(buf,smb_pid,cli->pid);
	memset(buf+smb_pidhigh, 0, 12);
	SSVAL(buf,smb_uid,cli->vuid);
	SSVAL(buf,smb_mid,cli->mid);

	if (cli->protocol <= PROTOCOL_CORE) {
		return;
	}

	if (cli->case_sensitive) {
		SCVAL(buf,smb_flg,0x0);
	} else {
		/* Default setting, case insensitive. */
		SCVAL(buf,smb_flg,0x8);
	}
	flags2 = FLAGS2_LONG_PATH_COMPONENTS;
	if (cli->capabilities & CAP_UNICODE)
		flags2 |= FLAGS2_UNICODE_STRINGS;
	if ((cli->capabilities & CAP_DFS) && cli->dfsroot)
		flags2 |= FLAGS2_DFS_PATHNAMES;
	if (cli->capabilities & CAP_STATUS32)
		flags2 |= FLAGS2_32_BIT_ERROR_CODES;
	if (cli->use_spnego)
		flags2 |= FLAGS2_EXTENDED_SECURITY;
	SSVAL(buf,smb_flg2, flags2);
}

void cli_setup_packet(struct cli_state *cli)
{
	cli_setup_packet_buf(cli, cli->outbuf);
}

/****************************************************************************
 Setup the bcc length of the packet from a pointer to the end of the data.
****************************************************************************/

void cli_setup_bcc(struct cli_state *cli, void *p)
{
	set_message_bcc(cli->outbuf, PTR_DIFF(p, smb_buf(cli->outbuf)));
}

/****************************************************************************
 Initialize Domain, user or password.
****************************************************************************/

NTSTATUS cli_set_domain(struct cli_state *cli, const char *domain)
{
	TALLOC_FREE(cli->domain);
	cli->domain = talloc_strdup(cli, domain ? domain : "");
	if (cli->domain == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_set_username(struct cli_state *cli, const char *username)
{
	TALLOC_FREE(cli->user_name);
	cli->user_name = talloc_strdup(cli, username ? username : "");
	if (cli->user_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

NTSTATUS cli_set_password(struct cli_state *cli, const char *password)
{
	TALLOC_FREE(cli->password);

	/* Password can be NULL. */
	if (password) {
		cli->password = talloc_strdup(cli, password);
		if (cli->password == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		/* Use zero NTLMSSP hashes and session key. */
		cli->password = NULL;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Initialise credentials of a client structure.
****************************************************************************/

NTSTATUS cli_init_creds(struct cli_state *cli, const char *username, const char *domain, const char *password)
{
	NTSTATUS status = cli_set_username(cli, username);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	status = cli_set_domain(cli, domain);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	DEBUG(10,("cli_init_creds: user %s domain %s\n", cli->user_name, cli->domain));

	return cli_set_password(cli, password);
}

/****************************************************************************
 Initialise a client structure. Always returns a talloc'ed struct.
 Set the signing state (used from the command line).
****************************************************************************/

struct cli_state *cli_initialise_ex(int signing_state)
{
	struct cli_state *cli = NULL;
	bool allow_smb_signing = false;
	bool mandatory_signing = false;

	/* Check the effective uid - make sure we are not setuid */
	if (is_setuid_root()) {
		DEBUG(0,("libsmb based programs must *NOT* be setuid root.\n"));
		return NULL;
	}

	cli = TALLOC_ZERO_P(NULL, struct cli_state);
	if (!cli) {
		return NULL;
	}

	cli->dfs_mountpoint = talloc_strdup(cli, "");
	if (!cli->dfs_mountpoint) {
		goto error;
	}
	cli->port = 0;
	cli->fd = -1;
	cli->cnum = -1;
	cli->pid = (uint16)sys_getpid();
	cli->mid = 1;
	cli->vuid = UID_FIELD_INVALID;
	cli->protocol = PROTOCOL_NT1;
	cli->timeout = 20000; /* Timeout is in milliseconds. */
	cli->bufsize = CLI_BUFFER_SIZE+4;
	cli->max_xmit = cli->bufsize;
	cli->outbuf = (char *)SMB_MALLOC(cli->bufsize+SAFETY_MARGIN);
	cli->seqnum = 0;
	cli->inbuf = (char *)SMB_MALLOC(cli->bufsize+SAFETY_MARGIN);
	cli->oplock_handler = cli_oplock_ack;
	cli->case_sensitive = false;
	cli->smb_rw_error = SMB_READ_OK;

	cli->use_spnego = lp_client_use_spnego();

	cli->capabilities = CAP_UNICODE | CAP_STATUS32 | CAP_DFS;

	/* Set the CLI_FORCE_DOSERR environment variable to test
	   client routines using DOS errors instead of STATUS32
	   ones.  This intended only as a temporary hack. */	
	if (getenv("CLI_FORCE_DOSERR"))
		cli->force_dos_errors = true;

	if (lp_client_signing()) {
		allow_smb_signing = true;
	}

	if (lp_client_signing() == Required) {
		mandatory_signing = true;
	}

	if (signing_state != Undefined) {
		allow_smb_signing = true;
	}

	if (signing_state == false) {
		allow_smb_signing = false;
		mandatory_signing = false;
	}

	if (signing_state == Required) {
		mandatory_signing = true;
	}

	if (!cli->outbuf || !cli->inbuf)
                goto error;

	memset(cli->outbuf, 0, cli->bufsize);
	memset(cli->inbuf, 0, cli->bufsize);


#if defined(DEVELOPER)
	/* just because we over-allocate, doesn't mean it's right to use it */
	clobber_region(__FUNCTION__, __LINE__, cli->outbuf+cli->bufsize, SAFETY_MARGIN);
	clobber_region(__FUNCTION__, __LINE__, cli->inbuf+cli->bufsize, SAFETY_MARGIN);
#endif

	/* initialise signing */
	cli->signing_state = smb_signing_init(cli,
					      allow_smb_signing,
					      mandatory_signing);
	if (!cli->signing_state) {
		goto error;
	}

	cli->outgoing = tevent_queue_create(cli, "cli_outgoing");
	if (cli->outgoing == NULL) {
		goto error;
	}
	cli->pending = NULL;

	cli->initialised = 1;

	return cli;

        /* Clean up after malloc() error */

 error:

        SAFE_FREE(cli->inbuf);
        SAFE_FREE(cli->outbuf);
	TALLOC_FREE(cli);
        return NULL;
}

struct cli_state *cli_initialise(void)
{
	return cli_initialise_ex(Undefined);
}

/****************************************************************************
 Close all pipes open on this session.
****************************************************************************/

void cli_nt_pipes_close(struct cli_state *cli)
{
	while (cli->pipe_list != NULL) {
		/*
		 * No TALLOC_FREE here!
		 */
		talloc_free(cli->pipe_list);
	}
}

/****************************************************************************
 Shutdown a client structure.
****************************************************************************/

static void _cli_shutdown(struct cli_state *cli)
{
	cli_nt_pipes_close(cli);

	/*
	 * tell our peer to free his resources.  Wihtout this, when an
	 * application attempts to do a graceful shutdown and calls
	 * smbc_free_context() to clean up all connections, some connections
	 * can remain active on the peer end, until some (long) timeout period
	 * later.  This tree disconnect forces the peer to clean up, since the
	 * connection will be going away.
	 *
	 * Also, do not do tree disconnect when cli->smb_rw_error is SMB_DO_NOT_DO_TDIS
	 * the only user for this so far is smbmount which passes opened connection
	 * down to kernel's smbfs module.
	 */
	if ( (cli->cnum != (uint16)-1) && (cli->smb_rw_error != SMB_DO_NOT_DO_TDIS ) ) {
		cli_tdis(cli);
	}
        
	SAFE_FREE(cli->outbuf);
	SAFE_FREE(cli->inbuf);

	data_blob_free(&cli->secblob);
	data_blob_free(&cli->user_session_key);

	if (cli->fd != -1) {
		close(cli->fd);
	}
	cli->fd = -1;
	cli->smb_rw_error = SMB_READ_OK;

	/*
	 * Need to free pending first, they remove themselves
	 */
	while (cli->pending) {
		talloc_free(cli->pending[0]);
	}
	TALLOC_FREE(cli);
}

void cli_shutdown(struct cli_state *cli)
{
	struct cli_state *cli_head;
	if (cli == NULL) {
		return;
	}
	DLIST_HEAD(cli, cli_head);
	if (cli_head == cli) {
		/*
		 * head of a DFS list, shutdown all subsidiary DFS
		 * connections.
		 */
		struct cli_state *p, *next;

		for (p = cli_head->next; p; p = next) {
			next = p->next;
			DLIST_REMOVE(cli_head, p);
			_cli_shutdown(p);
		}
	} else {
		DLIST_REMOVE(cli_head, cli);
	}

	_cli_shutdown(cli);
}

/****************************************************************************
 Set socket options on a open connection.
****************************************************************************/

void cli_sockopt(struct cli_state *cli, const char *options)
{
	set_socket_options(cli->fd, options);
}

/****************************************************************************
 Set the PID to use for smb messages. Return the old pid.
****************************************************************************/

uint16 cli_setpid(struct cli_state *cli, uint16 pid)
{
	uint16 ret = cli->pid;
	cli->pid = pid;
	return ret;
}

/****************************************************************************
 Set the case sensitivity flag on the packets. Returns old state.
****************************************************************************/

bool cli_set_case_sensitive(struct cli_state *cli, bool case_sensitive)
{
	bool ret = cli->case_sensitive;
	cli->case_sensitive = case_sensitive;
	return ret;
}

struct cli_echo_state {
	uint16_t vwv[1];
	DATA_BLOB data;
	int num_echos;
};

static void cli_echo_done(struct tevent_req *subreq);

struct tevent_req *cli_echo_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct cli_state *cli, uint16_t num_echos,
				 DATA_BLOB data)
{
	struct tevent_req *req, *subreq;
	struct cli_echo_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_echo_state);
	if (req == NULL) {
		return NULL;
	}
	SSVAL(state->vwv, 0, num_echos);
	state->data = data;
	state->num_echos = num_echos;

	subreq = cli_smb_send(state, ev, cli, SMBecho, 0, 1, state->vwv,
			      data.length, data.data);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, cli_echo_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void cli_echo_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_echo_state *state = tevent_req_data(
		req, struct cli_echo_state);
	NTSTATUS status;
	uint32_t num_bytes;
	uint8_t *bytes;
	uint8_t *inbuf;

	status = cli_smb_recv(subreq, state, &inbuf, 0, NULL, NULL,
			      &num_bytes, &bytes);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	if ((num_bytes != state->data.length)
	    || (memcmp(bytes, state->data.data, num_bytes) != 0)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	state->num_echos -=1;
	if (state->num_echos == 0) {
		tevent_req_done(req);
		return;
	}

	if (!cli_smb_req_set_pending(subreq)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}
}

/**
 * Get the result out from an echo request
 * @param[in] req	The async_req from cli_echo_send
 * @retval Did the server reply correctly?
 */

NTSTATUS cli_echo_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

/**
 * @brief Send/Receive SMBEcho requests
 * @param[in] mem_ctx	The memory context to put the async_req on
 * @param[in] ev	The event context that will call us back
 * @param[in] cli	The connection to send the echo to
 * @param[in] num_echos	How many times do we want to get the reply?
 * @param[in] data	The data we want to get back
 * @retval Did the server reply correctly?
 */

NTSTATUS cli_echo(struct cli_state *cli, uint16_t num_echos, DATA_BLOB data)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_echo_send(frame, ev, cli, num_echos, data);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_echo_recv(req);
 fail:
	TALLOC_FREE(frame);
	if (!NT_STATUS_IS_OK(status)) {
		cli_set_error(cli, status);
	}
	return status;
}

/**
 * Is the SMB command able to hold an AND_X successor
 * @param[in] cmd	The SMB command in question
 * @retval Can we add a chained request after "cmd"?
 */
bool is_andx_req(uint8_t cmd)
{
	switch (cmd) {
	case SMBtconX:
	case SMBlockingX:
	case SMBopenX:
	case SMBreadX:
	case SMBwriteX:
	case SMBsesssetupX:
	case SMBulogoffX:
	case SMBntcreateX:
		return true;
		break;
	default:
		break;
	}

	return false;
}

NTSTATUS cli_smb(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		 uint8_t smb_command, uint8_t additional_flags,
		 uint8_t wct, uint16_t *vwv,
		 uint32_t num_bytes, const uint8_t *bytes,
		 struct tevent_req **result_parent,
		 uint8_t min_wct, uint8_t *pwct, uint16_t **pvwv,
		 uint32_t *pnum_bytes, uint8_t **pbytes)
{
        struct tevent_context *ev;
        struct tevent_req *req = NULL;
        NTSTATUS status = NT_STATUS_NO_MEMORY;

        if (cli_has_async_calls(cli)) {
                return NT_STATUS_INVALID_PARAMETER;
        }
        ev = tevent_context_init(mem_ctx);
        if (ev == NULL) {
                goto fail;
        }
        req = cli_smb_send(mem_ctx, ev, cli, smb_command, additional_flags,
			   wct, vwv, num_bytes, bytes);
        if (req == NULL) {
                goto fail;
        }
        if (!tevent_req_poll_ntstatus(req, ev, &status)) {
                goto fail;
        }
        status = cli_smb_recv(req, NULL, NULL, min_wct, pwct, pvwv,
			      pnum_bytes, pbytes);
fail:
        TALLOC_FREE(ev);
	if (NT_STATUS_IS_OK(status) && (result_parent != NULL)) {
		*result_parent = req;
	}
        return status;
}
