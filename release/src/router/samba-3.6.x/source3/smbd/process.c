/* 
   Unix SMB/CIFS implementation.
   process incoming packets - main loop
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Volker Lendecke 2005-2007

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
#include "../lib/tsocket/tsocket.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "librpc/gen_ndr/netlogon.h"
#include "../lib/async_req/async_sock.h"
#include "ctdbd_conn.h"
#include "../lib/util/select.h"
#include "printing/pcap.h"
#include "system/select.h"
#include "passdb.h"
#include "auth.h"
#include "messages.h"
#include "smbprofile.h"
#include "rpc_server/spoolss/srv_spoolss_nt.h"
#include "libsmb/libsmb.h"

extern bool global_machine_password_needs_changing;

static void construct_reply_common(struct smb_request *req, const char *inbuf,
				   char *outbuf);
static struct pending_message_list *get_deferred_open_message_smb(uint64_t mid);

static bool smbd_lock_socket_internal(struct smbd_server_connection *sconn)
{
	bool ok;

	if (sconn->smb1.echo_handler.socket_lock_fd == -1) {
		return true;
	}

	sconn->smb1.echo_handler.ref_count++;

	if (sconn->smb1.echo_handler.ref_count > 1) {
		return true;
	}

	DEBUG(10,("pid[%d] wait for socket lock\n", (int)sys_getpid()));

	do {
		ok = fcntl_lock(
			sconn->smb1.echo_handler.socket_lock_fd,
			SMB_F_SETLKW, 0, 0, F_WRLCK);
	} while (!ok && (errno == EINTR));

	if (!ok) {
		DEBUG(1, ("fcntl_lock failed: %s\n", strerror(errno)));
		return false;
	}

	DEBUG(10,("pid[%d] got for socket lock\n", (int)sys_getpid()));

	return true;
}

void smbd_lock_socket(struct smbd_server_connection *sconn)
{
	if (!smbd_lock_socket_internal(sconn)) {
		exit_server_cleanly("failed to lock socket");
	}
}

static bool smbd_unlock_socket_internal(struct smbd_server_connection *sconn)
{
	bool ok;

	if (sconn->smb1.echo_handler.socket_lock_fd == -1) {
		return true;
	}

	sconn->smb1.echo_handler.ref_count--;

	if (sconn->smb1.echo_handler.ref_count > 0) {
		return true;
	}

	do {
		ok = fcntl_lock(
			sconn->smb1.echo_handler.socket_lock_fd,
			SMB_F_SETLKW, 0, 0, F_UNLCK);
	} while (!ok && (errno == EINTR));

	if (!ok) {
		DEBUG(1, ("fcntl_lock failed: %s\n", strerror(errno)));
		return false;
	}

	DEBUG(10,("pid[%d] unlocked socket\n", (int)sys_getpid()));

	return true;
}

void smbd_unlock_socket(struct smbd_server_connection *sconn)
{
	if (!smbd_unlock_socket_internal(sconn)) {
		exit_server_cleanly("failed to unlock socket");
	}
}

/* Accessor function for smb_read_error for smbd functions. */

/****************************************************************************
 Send an smb to a fd.
****************************************************************************/

bool srv_send_smb(struct smbd_server_connection *sconn, char *buffer,
		  bool do_signing, uint32_t seqnum,
		  bool do_encrypt,
		  struct smb_perfcount_data *pcd)
{
	size_t len = 0;
	size_t nwritten=0;
	ssize_t ret;
	char *buf_out = buffer;

	smbd_lock_socket(sconn);

	if (do_signing) {
		/* Sign the outgoing packet if required. */
		srv_calculate_sign_mac(sconn, buf_out, seqnum);
	}

	if (do_encrypt) {
		NTSTATUS status = srv_encrypt_buffer(buffer, &buf_out);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("send_smb: SMB encryption failed "
				"on outgoing packet! Error %s\n",
				nt_errstr(status) ));
			goto out;
		}
	}

	len = smb_len_large(buf_out) + 4;

	ret = write_data(sconn->sock, buf_out+nwritten, len - nwritten);
	if (ret <= 0) {

		char addr[INET6_ADDRSTRLEN];
		/*
		 * Try and give an error message saying what
		 * client failed.
		 */
		DEBUG(1,("pid[%d] Error writing %d bytes to client %s. %d. (%s)\n",
			 (int)sys_getpid(), (int)len,
			 get_peer_addr(sconn->sock, addr, sizeof(addr)),
			 (int)ret, strerror(errno) ));

		srv_free_enc_buffer(buf_out);
		goto out;
	}

	SMB_PERFCOUNT_SET_MSGLEN_OUT(pcd, len);
	srv_free_enc_buffer(buf_out);
out:
	SMB_PERFCOUNT_END(pcd);

	smbd_unlock_socket(sconn);
	return true;
}

/*******************************************************************
 Setup the word count and byte count for a smb message.
********************************************************************/

int srv_set_message(char *buf,
                        int num_words,
                        int num_bytes,
                        bool zero)
{
	if (zero && (num_words || num_bytes)) {
		memset(buf + smb_size,'\0',num_words*2 + num_bytes);
	}
	SCVAL(buf,smb_wct,num_words);
	SSVAL(buf,smb_vwv + num_words*SIZEOFWORD,num_bytes);
	smb_setlen(buf,(smb_size + num_words*2 + num_bytes - 4));
	return (smb_size + num_words*2 + num_bytes);
}

static bool valid_smb_header(const uint8_t *inbuf)
{
	if (is_encrypted_packet(inbuf)) {
		return true;
	}
	/*
	 * This used to be (strncmp(smb_base(inbuf),"\377SMB",4) == 0)
	 * but it just looks weird to call strncmp for this one.
	 */
	return (IVAL(smb_base(inbuf), 0) == 0x424D53FF);
}

/* Socket functions for smbd packet processing. */

static bool valid_packet_size(size_t len)
{
	/*
	 * A WRITEX with CAP_LARGE_WRITEX can be 64k worth of data plus 65 bytes
	 * of header. Don't print the error if this fits.... JRA.
	 */

	if (len > (BUFFER_SIZE + LARGE_WRITEX_HDR_SIZE)) {
		DEBUG(0,("Invalid packet length! (%lu bytes).\n",
					(unsigned long)len));
		return false;
	}
	return true;
}

static NTSTATUS read_packet_remainder(int fd, char *buffer,
				      unsigned int timeout, ssize_t len)
{
	NTSTATUS status;

	if (len <= 0) {
		return NT_STATUS_OK;
	}

	status = read_fd_with_timeout(fd, buffer, len, len, timeout, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		char addr[INET6_ADDRSTRLEN];
		DEBUG(0, ("read_fd_with_timeout failed for client %s read "
			  "error = %s.\n",
			  get_peer_addr(fd, addr, sizeof(addr)),
			  nt_errstr(status)));
	}
	return status;
}

/****************************************************************************
 Attempt a zerocopy writeX read. We know here that len > smb_size-4
****************************************************************************/

/*
 * Unfortunately, earlier versions of smbclient/libsmbclient
 * don't send this "standard" writeX header. I've fixed this
 * for 3.2 but we'll use the old method with earlier versions.
 * Windows and CIFSFS at least use this standard size. Not
 * sure about MacOSX.
 */

#define STANDARD_WRITE_AND_X_HEADER_SIZE (smb_size - 4 + /* basic header */ \
				(2*14) + /* word count (including bcc) */ \
				1 /* pad byte */)

static NTSTATUS receive_smb_raw_talloc_partial_read(TALLOC_CTX *mem_ctx,
						    const char lenbuf[4],
						    struct smbd_server_connection *sconn,
						    int sock,
						    char **buffer,
						    unsigned int timeout,
						    size_t *p_unread,
						    size_t *len_ret)
{
	/* Size of a WRITEX call (+4 byte len). */
	char writeX_header[4 + STANDARD_WRITE_AND_X_HEADER_SIZE];
	ssize_t len = smb_len_large(lenbuf); /* Could be a UNIX large writeX. */
	ssize_t toread;
	NTSTATUS status;

	memcpy(writeX_header, lenbuf, 4);

	status = read_fd_with_timeout(
		sock, writeX_header + 4,
		STANDARD_WRITE_AND_X_HEADER_SIZE,
		STANDARD_WRITE_AND_X_HEADER_SIZE,
		timeout, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("read_fd_with_timeout failed for client %s read "
			  "error = %s.\n", sconn->client_id.addr,
			  nt_errstr(status)));
		return status;
	}

	/*
	 * Ok - now try and see if this is a possible
	 * valid writeX call.
	 */

	if (is_valid_writeX_buffer(sconn, (uint8_t *)writeX_header)) {
		/*
		 * If the data offset is beyond what
		 * we've read, drain the extra bytes.
		 */
		uint16_t doff = SVAL(writeX_header,smb_vwv11);
		ssize_t newlen;

		if (doff > STANDARD_WRITE_AND_X_HEADER_SIZE) {
			size_t drain = doff - STANDARD_WRITE_AND_X_HEADER_SIZE;
			if (drain_socket(sock, drain) != drain) {
	                        smb_panic("receive_smb_raw_talloc_partial_read:"
					" failed to drain pending bytes");
	                }
		} else {
			doff = STANDARD_WRITE_AND_X_HEADER_SIZE;
		}

		/* Spoof down the length and null out the bcc. */
		set_message_bcc(writeX_header, 0);
		newlen = smb_len(writeX_header);

		/* Copy the header we've written. */

		*buffer = (char *)TALLOC_MEMDUP(mem_ctx,
				writeX_header,
				sizeof(writeX_header));

		if (*buffer == NULL) {
			DEBUG(0, ("Could not allocate inbuf of length %d\n",
				  (int)sizeof(writeX_header)));
			return NT_STATUS_NO_MEMORY;
		}

		/* Work out the remaining bytes. */
		*p_unread = len - STANDARD_WRITE_AND_X_HEADER_SIZE;
		*len_ret = newlen + 4;
		return NT_STATUS_OK;
	}

	if (!valid_packet_size(len)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * Not a valid writeX call. Just do the standard
	 * talloc and return.
	 */

	*buffer = TALLOC_ARRAY(mem_ctx, char, len+4);

	if (*buffer == NULL) {
		DEBUG(0, ("Could not allocate inbuf of length %d\n",
			  (int)len+4));
		return NT_STATUS_NO_MEMORY;
	}

	/* Copy in what we already read. */
	memcpy(*buffer,
		writeX_header,
		4 + STANDARD_WRITE_AND_X_HEADER_SIZE);
	toread = len - STANDARD_WRITE_AND_X_HEADER_SIZE;

	if(toread > 0) {
		status = read_packet_remainder(
			sock,
			(*buffer) + 4 + STANDARD_WRITE_AND_X_HEADER_SIZE,
			timeout, toread);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10, ("receive_smb_raw_talloc_partial_read: %s\n",
				   nt_errstr(status)));
			return status;
		}
	}

	*len_ret = len + 4;
	return NT_STATUS_OK;
}

static NTSTATUS receive_smb_raw_talloc(TALLOC_CTX *mem_ctx,
				       struct smbd_server_connection *sconn,
				       int sock,
				       char **buffer, unsigned int timeout,
				       size_t *p_unread, size_t *plen)
{
	char lenbuf[4];
	size_t len;
	int min_recv_size = lp_min_receive_file_size();
	NTSTATUS status;

	*p_unread = 0;

	status = read_smb_length_return_keepalive(sock, lenbuf, timeout,
						  &len);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (CVAL(lenbuf,0) == 0 && min_recv_size &&
	    (smb_len_large(lenbuf) > /* Could be a UNIX large writeX. */
		(min_recv_size + STANDARD_WRITE_AND_X_HEADER_SIZE)) &&
	    !srv_is_signing_active(sconn) &&
	    sconn->smb1.echo_handler.trusted_fde == NULL) {

		return receive_smb_raw_talloc_partial_read(
			mem_ctx, lenbuf, sconn, sock, buffer, timeout,
			p_unread, plen);
	}

	if (!valid_packet_size(len)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * The +4 here can't wrap, we've checked the length above already.
	 */

	*buffer = TALLOC_ARRAY(mem_ctx, char, len+4);

	if (*buffer == NULL) {
		DEBUG(0, ("Could not allocate inbuf of length %d\n",
			  (int)len+4));
		return NT_STATUS_NO_MEMORY;
	}

	memcpy(*buffer, lenbuf, sizeof(lenbuf));

	status = read_packet_remainder(sock, (*buffer)+4, timeout, len);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*plen = len + 4;
	return NT_STATUS_OK;
}

static NTSTATUS receive_smb_talloc(TALLOC_CTX *mem_ctx,
				   struct smbd_server_connection *sconn,
				   int sock,
				   char **buffer, unsigned int timeout,
				   size_t *p_unread, bool *p_encrypted,
				   size_t *p_len,
				   uint32_t *seqnum,
				   bool trusted_channel)
{
	size_t len = 0;
	NTSTATUS status;

	*p_encrypted = false;

	status = receive_smb_raw_talloc(mem_ctx, sconn, sock, buffer, timeout,
					p_unread, &len);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)?5:1,
		      ("receive_smb_raw_talloc failed for client %s "
		       "read error = %s.\n",
		       sconn->client_id.addr, nt_errstr(status)));
		return status;
	}

	if (is_encrypted_packet((uint8_t *)*buffer)) {
		status = srv_decrypt_buffer(*buffer);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("receive_smb_talloc: SMB decryption failed on "
				"incoming packet! Error %s\n",
				nt_errstr(status) ));
			return status;
		}
		*p_encrypted = true;
	}

	/* Check the incoming SMB signature. */
	if (!srv_check_sign_mac(sconn, *buffer, seqnum, trusted_channel)) {
		DEBUG(0, ("receive_smb: SMB Signature verification failed on "
			  "incoming packet!\n"));
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	*p_len = len;
	return NT_STATUS_OK;
}

/*
 * Initialize a struct smb_request from an inbuf
 */

static bool init_smb_request(struct smb_request *req,
			     struct smbd_server_connection *sconn,
			     const uint8 *inbuf,
			     size_t unread_bytes, bool encrypted,
			     uint32_t seqnum)
{
	size_t req_size = smb_len(inbuf) + 4;
	/* Ensure we have at least smb_size bytes. */
	if (req_size < smb_size) {
		DEBUG(0,("init_smb_request: invalid request size %u\n",
			(unsigned int)req_size ));
		return false;
	}
	req->cmd    = CVAL(inbuf, smb_com);
	req->flags2 = SVAL(inbuf, smb_flg2);
	req->smbpid = SVAL(inbuf, smb_pid);
	req->mid    = (uint64_t)SVAL(inbuf, smb_mid);
	req->seqnum = seqnum;
	req->vuid   = SVAL(inbuf, smb_uid);
	req->tid    = SVAL(inbuf, smb_tid);
	req->wct    = CVAL(inbuf, smb_wct);
	req->vwv    = (uint16_t *)(inbuf+smb_vwv);
	req->buflen = smb_buflen(inbuf);
	req->buf    = (const uint8_t *)smb_buf(inbuf);
	req->unread_bytes = unread_bytes;
	req->encrypted = encrypted;
	req->sconn = sconn;
	req->conn = conn_find(sconn,req->tid);
	req->chain_fsp = NULL;
	req->chain_outbuf = NULL;
	req->done = false;
	req->smb2req = NULL;
	smb_init_perfcount_data(&req->pcd);

	/* Ensure we have at least wct words and 2 bytes of bcc. */
	if (smb_size + req->wct*2 > req_size) {
		DEBUG(0,("init_smb_request: invalid wct number %u (size %u)\n",
			(unsigned int)req->wct,
			(unsigned int)req_size));
		return false;
	}
	/* Ensure bcc is correct. */
	if (((uint8 *)smb_buf(inbuf)) + req->buflen > inbuf + req_size) {
		DEBUG(0,("init_smb_request: invalid bcc number %u "
			"(wct = %u, size %u)\n",
			(unsigned int)req->buflen,
			(unsigned int)req->wct,
			(unsigned int)req_size));
		return false;
	}

	req->outbuf = NULL;
	return true;
}

static void process_smb(struct smbd_server_connection *conn,
			uint8_t *inbuf, size_t nread, size_t unread_bytes,
			uint32_t seqnum, bool encrypted,
			struct smb_perfcount_data *deferred_pcd);

static void smbd_deferred_open_timer(struct event_context *ev,
				     struct timed_event *te,
				     struct timeval _tval,
				     void *private_data)
{
	struct pending_message_list *msg = talloc_get_type(private_data,
					   struct pending_message_list);
	TALLOC_CTX *mem_ctx = talloc_tos();
	uint64_t mid = (uint64_t)SVAL(msg->buf.data,smb_mid);
	uint8_t *inbuf;

	inbuf = (uint8_t *)talloc_memdup(mem_ctx, msg->buf.data,
					 msg->buf.length);
	if (inbuf == NULL) {
		exit_server("smbd_deferred_open_timer: talloc failed\n");
		return;
	}

	/* We leave this message on the queue so the open code can
	   know this is a retry. */
	DEBUG(5,("smbd_deferred_open_timer: trigger mid %llu.\n",
		(unsigned long long)mid ));

	/* Mark the message as processed so this is not
	 * re-processed in error. */
	msg->processed = true;

	process_smb(smbd_server_conn, inbuf,
		    msg->buf.length, 0,
		    msg->seqnum, msg->encrypted, &msg->pcd);

	/* If it's still there and was processed, remove it. */
	msg = get_deferred_open_message_smb(mid);
	if (msg && msg->processed) {
		remove_deferred_open_message_smb(mid);
	}
}

/****************************************************************************
 Function to push a message onto the tail of a linked list of smb messages ready
 for processing.
****************************************************************************/

static bool push_queued_message(struct smb_request *req,
				struct timeval request_time,
				struct timeval end_time,
				char *private_data, size_t private_len)
{
	int msg_len = smb_len(req->inbuf) + 4;
	struct pending_message_list *msg;

	msg = TALLOC_ZERO_P(NULL, struct pending_message_list);

	if(msg == NULL) {
		DEBUG(0,("push_message: malloc fail (1)\n"));
		return False;
	}

	msg->buf = data_blob_talloc(msg, req->inbuf, msg_len);
	if(msg->buf.data == NULL) {
		DEBUG(0,("push_message: malloc fail (2)\n"));
		TALLOC_FREE(msg);
		return False;
	}

	msg->request_time = request_time;
	msg->seqnum = req->seqnum;
	msg->encrypted = req->encrypted;
	msg->processed = false;
	SMB_PERFCOUNT_DEFER_OP(&req->pcd, &msg->pcd);

	if (private_data) {
		msg->private_data = data_blob_talloc(msg, private_data,
						     private_len);
		if (msg->private_data.data == NULL) {
			DEBUG(0,("push_message: malloc fail (3)\n"));
			TALLOC_FREE(msg);
			return False;
		}
	}

	msg->te = event_add_timed(smbd_event_context(),
				  msg,
				  end_time,
				  smbd_deferred_open_timer,
				  msg);
	if (!msg->te) {
		DEBUG(0,("push_message: event_add_timed failed\n"));
		TALLOC_FREE(msg);
		return false;
	}

	DLIST_ADD_END(deferred_open_queue, msg, struct pending_message_list *);

	DEBUG(10,("push_message: pushed message length %u on "
		  "deferred_open_queue\n", (unsigned int)msg_len));

	return True;
}

/****************************************************************************
 Function to delete a sharing violation open message by mid.
****************************************************************************/

void remove_deferred_open_message_smb(uint64_t mid)
{
	struct pending_message_list *pml;

	if (smbd_server_conn->using_smb2) {
		remove_deferred_open_message_smb2(smbd_server_conn, mid);
		return;
	}

	for (pml = deferred_open_queue; pml; pml = pml->next) {
		if (mid == (uint64_t)SVAL(pml->buf.data,smb_mid)) {
			DEBUG(10,("remove_deferred_open_message_smb: "
				  "deleting mid %llu len %u\n",
				  (unsigned long long)mid,
				  (unsigned int)pml->buf.length ));
			DLIST_REMOVE(deferred_open_queue, pml);
			TALLOC_FREE(pml);
			return;
		}
	}
}

/****************************************************************************
 Move a sharing violation open retry message to the front of the list and
 schedule it for immediate processing.
****************************************************************************/

void schedule_deferred_open_message_smb(uint64_t mid)
{
	struct pending_message_list *pml;
	int i = 0;

	if (smbd_server_conn->using_smb2) {
		schedule_deferred_open_message_smb2(smbd_server_conn, mid);
		return;
	}

	for (pml = deferred_open_queue; pml; pml = pml->next) {
		uint64_t msg_mid = (uint64_t)SVAL(pml->buf.data,smb_mid);

		DEBUG(10,("schedule_deferred_open_message_smb: [%d] "
			"msg_mid = %llu\n",
			i++,
			(unsigned long long)msg_mid ));

		if (mid == msg_mid) {
			struct timed_event *te;

			if (pml->processed) {
				/* A processed message should not be
				 * rescheduled. */
				DEBUG(0,("schedule_deferred_open_message_smb: LOGIC ERROR "
					"message mid %llu was already processed\n",
					(unsigned long long)msg_mid ));
				continue;
			}

			DEBUG(10,("schedule_deferred_open_message_smb: "
				"scheduling mid %llu\n",
				(unsigned long long)mid ));

			te = event_add_timed(smbd_event_context(),
					     pml,
					     timeval_zero(),
					     smbd_deferred_open_timer,
					     pml);
			if (!te) {
				DEBUG(10,("schedule_deferred_open_message_smb: "
					"event_add_timed() failed, "
					"skipping mid %llu\n",
					(unsigned long long)msg_mid ));
			}

			TALLOC_FREE(pml->te);
			pml->te = te;
			DLIST_PROMOTE(deferred_open_queue, pml);
			return;
		}
	}

	DEBUG(10,("schedule_deferred_open_message_smb: failed to "
		"find message mid %llu\n",
		(unsigned long long)mid ));
}

/****************************************************************************
 Return true if this mid is on the deferred queue and was not yet processed.
****************************************************************************/

bool open_was_deferred(uint64_t mid)
{
	struct pending_message_list *pml;

	if (smbd_server_conn->using_smb2) {
		return open_was_deferred_smb2(smbd_server_conn, mid);
	}

	for (pml = deferred_open_queue; pml; pml = pml->next) {
		if (((uint64_t)SVAL(pml->buf.data,smb_mid)) == mid && !pml->processed) {
			return True;
		}
	}
	return False;
}

/****************************************************************************
 Return the message queued by this mid.
****************************************************************************/

static struct pending_message_list *get_deferred_open_message_smb(uint64_t mid)
{
	struct pending_message_list *pml;

	for (pml = deferred_open_queue; pml; pml = pml->next) {
		if (((uint64_t)SVAL(pml->buf.data,smb_mid)) == mid) {
			return pml;
		}
	}
	return NULL;
}

/****************************************************************************
 Get the state data queued by this mid.
****************************************************************************/

bool get_deferred_open_message_state(struct smb_request *smbreq,
				struct timeval *p_request_time,
				void **pp_state)
{
	struct pending_message_list *pml;

	if (smbd_server_conn->using_smb2) {
		return get_deferred_open_message_state_smb2(smbreq->smb2req,
					p_request_time,
					pp_state);
	}

	pml = get_deferred_open_message_smb(smbreq->mid);
	if (!pml) {
		return false;
	}
	if (p_request_time) {
		*p_request_time = pml->request_time;
	}
	if (pp_state) {
		*pp_state = (void *)pml->private_data.data;
	}
	return true;
}

/****************************************************************************
 Function to push a deferred open smb message onto a linked list of local smb
 messages ready for processing.
****************************************************************************/

bool push_deferred_open_message_smb(struct smb_request *req,
			       struct timeval request_time,
			       struct timeval timeout,
			       struct file_id id,
			       char *private_data, size_t priv_len)
{
	struct timeval end_time;

	if (req->smb2req) {
		return push_deferred_open_message_smb2(req->smb2req,
						request_time,
						timeout,
						id,
						private_data,
						priv_len);
	}

	if (req->unread_bytes) {
		DEBUG(0,("push_deferred_open_message_smb: logic error ! "
			"unread_bytes = %u\n",
			(unsigned int)req->unread_bytes ));
		smb_panic("push_deferred_open_message_smb: "
			"logic error unread_bytes != 0" );
	}

	end_time = timeval_sum(&request_time, &timeout);

	DEBUG(10,("push_deferred_open_message_smb: pushing message "
		"len %u mid %llu timeout time [%u.%06u]\n",
		(unsigned int) smb_len(req->inbuf)+4,
		(unsigned long long)req->mid,
		(unsigned int)end_time.tv_sec,
		(unsigned int)end_time.tv_usec));

	return push_queued_message(req, request_time, end_time,
				   private_data, priv_len);
}

struct idle_event {
	struct timed_event *te;
	struct timeval interval;
	char *name;
	bool (*handler)(const struct timeval *now, void *private_data);
	void *private_data;
};

static void smbd_idle_event_handler(struct event_context *ctx,
				    struct timed_event *te,
				    struct timeval now,
				    void *private_data)
{
	struct idle_event *event =
		talloc_get_type_abort(private_data, struct idle_event);

	TALLOC_FREE(event->te);

	DEBUG(10,("smbd_idle_event_handler: %s %p called\n",
		  event->name, event->te));

	if (!event->handler(&now, event->private_data)) {
		DEBUG(10,("smbd_idle_event_handler: %s %p stopped\n",
			  event->name, event->te));
		/* Don't repeat, delete ourselves */
		TALLOC_FREE(event);
		return;
	}

	DEBUG(10,("smbd_idle_event_handler: %s %p rescheduled\n",
		  event->name, event->te));

	event->te = event_add_timed(ctx, event,
				    timeval_sum(&now, &event->interval),
				    smbd_idle_event_handler, event);

	/* We can't do much but fail here. */
	SMB_ASSERT(event->te != NULL);
}

struct idle_event *event_add_idle(struct event_context *event_ctx,
				  TALLOC_CTX *mem_ctx,
				  struct timeval interval,
				  const char *name,
				  bool (*handler)(const struct timeval *now,
						  void *private_data),
				  void *private_data)
{
	struct idle_event *result;
	struct timeval now = timeval_current();

	result = TALLOC_P(mem_ctx, struct idle_event);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->interval = interval;
	result->handler = handler;
	result->private_data = private_data;

	if (!(result->name = talloc_asprintf(result, "idle_evt(%s)", name))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	result->te = event_add_timed(event_ctx, result,
				     timeval_sum(&now, &interval),
				     smbd_idle_event_handler, result);
	if (result->te == NULL) {
		DEBUG(0, ("event_add_timed failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	DEBUG(10,("event_add_idle: %s %p\n", result->name, result->te));
	return result;
}

static NTSTATUS smbd_server_connection_loop_once(struct smbd_server_connection *conn)
{
	int timeout;
	int num_pfds = 0;
	int ret;
	bool retry;

	timeout = SMBD_SELECT_TIMEOUT * 1000;

	/*
	 * Are there any timed events waiting ? If so, ensure we don't
	 * select for longer than it would take to wait for them.
	 */

	event_add_to_poll_args(smbd_event_context(), conn,
			       &conn->pfds, &num_pfds, &timeout);

	/* Process a signal and timed events now... */
	if (run_events_poll(smbd_event_context(), 0, NULL, 0)) {
		return NT_STATUS_RETRY;
	}

	{
		int sav;
		START_PROFILE(smbd_idle);

		ret = sys_poll(conn->pfds, num_pfds, timeout);
		sav = errno;

		END_PROFILE(smbd_idle);
		errno = sav;
	}

	if (ret == -1) {
		if (errno == EINTR) {
			return NT_STATUS_RETRY;
		}
		return map_nt_error_from_unix(errno);
	}

	retry = run_events_poll(smbd_event_context(), ret, conn->pfds,
				num_pfds);
	if (retry) {
		return NT_STATUS_RETRY;
	}

	/* Did we timeout ? */
	if (ret == 0) {
		return NT_STATUS_RETRY;
	}

	/* should not be reached */
	return NT_STATUS_INTERNAL_ERROR;
}

/*
 * Only allow 5 outstanding trans requests. We're allocating memory, so
 * prevent a DoS.
 */

NTSTATUS allow_new_trans(struct trans_state *list, uint64_t mid)
{
	int count = 0;
	for (; list != NULL; list = list->next) {

		if (list->mid == mid) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		count += 1;
	}
	if (count > 5) {
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	return NT_STATUS_OK;
}

/*
These flags determine some of the permissions required to do an operation 

Note that I don't set NEED_WRITE on some write operations because they
are used by some brain-dead clients when printing, and I don't want to
force write permissions on print services.
*/
#define AS_USER (1<<0)
#define NEED_WRITE (1<<1) /* Must be paired with AS_USER */
#define TIME_INIT (1<<2)
#define CAN_IPC (1<<3) /* Must be paired with AS_USER */
#define AS_GUEST (1<<5) /* Must *NOT* be paired with AS_USER */
#define DO_CHDIR (1<<6)

/* 
   define a list of possible SMB messages and their corresponding
   functions. Any message that has a NULL function is unimplemented -
   please feel free to contribute implementations!
*/
static const struct smb_message_struct {
	const char *name;
	void (*fn)(struct smb_request *req);
	int flags;
} smb_messages[256] = {

/* 0x00 */ { "SMBmkdir",reply_mkdir,AS_USER | NEED_WRITE},
/* 0x01 */ { "SMBrmdir",reply_rmdir,AS_USER | NEED_WRITE},
/* 0x02 */ { "SMBopen",reply_open,AS_USER },
/* 0x03 */ { "SMBcreate",reply_mknew,AS_USER},
/* 0x04 */ { "SMBclose",reply_close,AS_USER | CAN_IPC },
/* 0x05 */ { "SMBflush",reply_flush,AS_USER},
/* 0x06 */ { "SMBunlink",reply_unlink,AS_USER | NEED_WRITE },
/* 0x07 */ { "SMBmv",reply_mv,AS_USER | NEED_WRITE },
/* 0x08 */ { "SMBgetatr",reply_getatr,AS_USER},
/* 0x09 */ { "SMBsetatr",reply_setatr,AS_USER | NEED_WRITE},
/* 0x0a */ { "SMBread",reply_read,AS_USER},
/* 0x0b */ { "SMBwrite",reply_write,AS_USER | CAN_IPC },
/* 0x0c */ { "SMBlock",reply_lock,AS_USER},
/* 0x0d */ { "SMBunlock",reply_unlock,AS_USER},
/* 0x0e */ { "SMBctemp",reply_ctemp,AS_USER },
/* 0x0f */ { "SMBmknew",reply_mknew,AS_USER},
/* 0x10 */ { "SMBcheckpath",reply_checkpath,AS_USER},
/* 0x11 */ { "SMBexit",reply_exit,DO_CHDIR},
/* 0x12 */ { "SMBlseek",reply_lseek,AS_USER},
/* 0x13 */ { "SMBlockread",reply_lockread,AS_USER},
/* 0x14 */ { "SMBwriteunlock",reply_writeunlock,AS_USER},
/* 0x15 */ { NULL, NULL, 0 },
/* 0x16 */ { NULL, NULL, 0 },
/* 0x17 */ { NULL, NULL, 0 },
/* 0x18 */ { NULL, NULL, 0 },
/* 0x19 */ { NULL, NULL, 0 },
/* 0x1a */ { "SMBreadbraw",reply_readbraw,AS_USER},
/* 0x1b */ { "SMBreadBmpx",reply_readbmpx,AS_USER},
/* 0x1c */ { "SMBreadBs",reply_readbs,AS_USER },
/* 0x1d */ { "SMBwritebraw",reply_writebraw,AS_USER},
/* 0x1e */ { "SMBwriteBmpx",reply_writebmpx,AS_USER},
/* 0x1f */ { "SMBwriteBs",reply_writebs,AS_USER},
/* 0x20 */ { "SMBwritec", NULL,0},
/* 0x21 */ { NULL, NULL, 0 },
/* 0x22 */ { "SMBsetattrE",reply_setattrE,AS_USER | NEED_WRITE },
/* 0x23 */ { "SMBgetattrE",reply_getattrE,AS_USER },
/* 0x24 */ { "SMBlockingX",reply_lockingX,AS_USER },
/* 0x25 */ { "SMBtrans",reply_trans,AS_USER | CAN_IPC },
/* 0x26 */ { "SMBtranss",reply_transs,AS_USER | CAN_IPC},
/* 0x27 */ { "SMBioctl",reply_ioctl,0},
/* 0x28 */ { "SMBioctls", NULL,AS_USER},
/* 0x29 */ { "SMBcopy",reply_copy,AS_USER | NEED_WRITE },
/* 0x2a */ { "SMBmove", NULL,AS_USER | NEED_WRITE },
/* 0x2b */ { "SMBecho",reply_echo,0},
/* 0x2c */ { "SMBwriteclose",reply_writeclose,AS_USER},
/* 0x2d */ { "SMBopenX",reply_open_and_X,AS_USER | CAN_IPC },
/* 0x2e */ { "SMBreadX",reply_read_and_X,AS_USER | CAN_IPC },
/* 0x2f */ { "SMBwriteX",reply_write_and_X,AS_USER | CAN_IPC },
/* 0x30 */ { NULL, NULL, 0 },
/* 0x31 */ { NULL, NULL, 0 },
/* 0x32 */ { "SMBtrans2",reply_trans2, AS_USER | CAN_IPC },
/* 0x33 */ { "SMBtranss2",reply_transs2, AS_USER | CAN_IPC },
/* 0x34 */ { "SMBfindclose",reply_findclose,AS_USER},
/* 0x35 */ { "SMBfindnclose",reply_findnclose,AS_USER},
/* 0x36 */ { NULL, NULL, 0 },
/* 0x37 */ { NULL, NULL, 0 },
/* 0x38 */ { NULL, NULL, 0 },
/* 0x39 */ { NULL, NULL, 0 },
/* 0x3a */ { NULL, NULL, 0 },
/* 0x3b */ { NULL, NULL, 0 },
/* 0x3c */ { NULL, NULL, 0 },
/* 0x3d */ { NULL, NULL, 0 },
/* 0x3e */ { NULL, NULL, 0 },
/* 0x3f */ { NULL, NULL, 0 },
/* 0x40 */ { NULL, NULL, 0 },
/* 0x41 */ { NULL, NULL, 0 },
/* 0x42 */ { NULL, NULL, 0 },
/* 0x43 */ { NULL, NULL, 0 },
/* 0x44 */ { NULL, NULL, 0 },
/* 0x45 */ { NULL, NULL, 0 },
/* 0x46 */ { NULL, NULL, 0 },
/* 0x47 */ { NULL, NULL, 0 },
/* 0x48 */ { NULL, NULL, 0 },
/* 0x49 */ { NULL, NULL, 0 },
/* 0x4a */ { NULL, NULL, 0 },
/* 0x4b */ { NULL, NULL, 0 },
/* 0x4c */ { NULL, NULL, 0 },
/* 0x4d */ { NULL, NULL, 0 },
/* 0x4e */ { NULL, NULL, 0 },
/* 0x4f */ { NULL, NULL, 0 },
/* 0x50 */ { NULL, NULL, 0 },
/* 0x51 */ { NULL, NULL, 0 },
/* 0x52 */ { NULL, NULL, 0 },
/* 0x53 */ { NULL, NULL, 0 },
/* 0x54 */ { NULL, NULL, 0 },
/* 0x55 */ { NULL, NULL, 0 },
/* 0x56 */ { NULL, NULL, 0 },
/* 0x57 */ { NULL, NULL, 0 },
/* 0x58 */ { NULL, NULL, 0 },
/* 0x59 */ { NULL, NULL, 0 },
/* 0x5a */ { NULL, NULL, 0 },
/* 0x5b */ { NULL, NULL, 0 },
/* 0x5c */ { NULL, NULL, 0 },
/* 0x5d */ { NULL, NULL, 0 },
/* 0x5e */ { NULL, NULL, 0 },
/* 0x5f */ { NULL, NULL, 0 },
/* 0x60 */ { NULL, NULL, 0 },
/* 0x61 */ { NULL, NULL, 0 },
/* 0x62 */ { NULL, NULL, 0 },
/* 0x63 */ { NULL, NULL, 0 },
/* 0x64 */ { NULL, NULL, 0 },
/* 0x65 */ { NULL, NULL, 0 },
/* 0x66 */ { NULL, NULL, 0 },
/* 0x67 */ { NULL, NULL, 0 },
/* 0x68 */ { NULL, NULL, 0 },
/* 0x69 */ { NULL, NULL, 0 },
/* 0x6a */ { NULL, NULL, 0 },
/* 0x6b */ { NULL, NULL, 0 },
/* 0x6c */ { NULL, NULL, 0 },
/* 0x6d */ { NULL, NULL, 0 },
/* 0x6e */ { NULL, NULL, 0 },
/* 0x6f */ { NULL, NULL, 0 },
/* 0x70 */ { "SMBtcon",reply_tcon,0},
/* 0x71 */ { "SMBtdis",reply_tdis,DO_CHDIR},
/* 0x72 */ { "SMBnegprot",reply_negprot,0},
/* 0x73 */ { "SMBsesssetupX",reply_sesssetup_and_X,0},
/* 0x74 */ { "SMBulogoffX",reply_ulogoffX, 0}, /* ulogoff doesn't give a valid TID */
/* 0x75 */ { "SMBtconX",reply_tcon_and_X,0},
/* 0x76 */ { NULL, NULL, 0 },
/* 0x77 */ { NULL, NULL, 0 },
/* 0x78 */ { NULL, NULL, 0 },
/* 0x79 */ { NULL, NULL, 0 },
/* 0x7a */ { NULL, NULL, 0 },
/* 0x7b */ { NULL, NULL, 0 },
/* 0x7c */ { NULL, NULL, 0 },
/* 0x7d */ { NULL, NULL, 0 },
/* 0x7e */ { NULL, NULL, 0 },
/* 0x7f */ { NULL, NULL, 0 },
/* 0x80 */ { "SMBdskattr",reply_dskattr,AS_USER},
/* 0x81 */ { "SMBsearch",reply_search,AS_USER},
/* 0x82 */ { "SMBffirst",reply_search,AS_USER},
/* 0x83 */ { "SMBfunique",reply_search,AS_USER},
/* 0x84 */ { "SMBfclose",reply_fclose,AS_USER},
/* 0x85 */ { NULL, NULL, 0 },
/* 0x86 */ { NULL, NULL, 0 },
/* 0x87 */ { NULL, NULL, 0 },
/* 0x88 */ { NULL, NULL, 0 },
/* 0x89 */ { NULL, NULL, 0 },
/* 0x8a */ { NULL, NULL, 0 },
/* 0x8b */ { NULL, NULL, 0 },
/* 0x8c */ { NULL, NULL, 0 },
/* 0x8d */ { NULL, NULL, 0 },
/* 0x8e */ { NULL, NULL, 0 },
/* 0x8f */ { NULL, NULL, 0 },
/* 0x90 */ { NULL, NULL, 0 },
/* 0x91 */ { NULL, NULL, 0 },
/* 0x92 */ { NULL, NULL, 0 },
/* 0x93 */ { NULL, NULL, 0 },
/* 0x94 */ { NULL, NULL, 0 },
/* 0x95 */ { NULL, NULL, 0 },
/* 0x96 */ { NULL, NULL, 0 },
/* 0x97 */ { NULL, NULL, 0 },
/* 0x98 */ { NULL, NULL, 0 },
/* 0x99 */ { NULL, NULL, 0 },
/* 0x9a */ { NULL, NULL, 0 },
/* 0x9b */ { NULL, NULL, 0 },
/* 0x9c */ { NULL, NULL, 0 },
/* 0x9d */ { NULL, NULL, 0 },
/* 0x9e */ { NULL, NULL, 0 },
/* 0x9f */ { NULL, NULL, 0 },
/* 0xa0 */ { "SMBnttrans",reply_nttrans, AS_USER | CAN_IPC },
/* 0xa1 */ { "SMBnttranss",reply_nttranss, AS_USER | CAN_IPC },
/* 0xa2 */ { "SMBntcreateX",reply_ntcreate_and_X, AS_USER | CAN_IPC },
/* 0xa3 */ { NULL, NULL, 0 },
/* 0xa4 */ { "SMBntcancel",reply_ntcancel, 0 },
/* 0xa5 */ { "SMBntrename",reply_ntrename, AS_USER | NEED_WRITE },
/* 0xa6 */ { NULL, NULL, 0 },
/* 0xa7 */ { NULL, NULL, 0 },
/* 0xa8 */ { NULL, NULL, 0 },
/* 0xa9 */ { NULL, NULL, 0 },
/* 0xaa */ { NULL, NULL, 0 },
/* 0xab */ { NULL, NULL, 0 },
/* 0xac */ { NULL, NULL, 0 },
/* 0xad */ { NULL, NULL, 0 },
/* 0xae */ { NULL, NULL, 0 },
/* 0xaf */ { NULL, NULL, 0 },
/* 0xb0 */ { NULL, NULL, 0 },
/* 0xb1 */ { NULL, NULL, 0 },
/* 0xb2 */ { NULL, NULL, 0 },
/* 0xb3 */ { NULL, NULL, 0 },
/* 0xb4 */ { NULL, NULL, 0 },
/* 0xb5 */ { NULL, NULL, 0 },
/* 0xb6 */ { NULL, NULL, 0 },
/* 0xb7 */ { NULL, NULL, 0 },
/* 0xb8 */ { NULL, NULL, 0 },
/* 0xb9 */ { NULL, NULL, 0 },
/* 0xba */ { NULL, NULL, 0 },
/* 0xbb */ { NULL, NULL, 0 },
/* 0xbc */ { NULL, NULL, 0 },
/* 0xbd */ { NULL, NULL, 0 },
/* 0xbe */ { NULL, NULL, 0 },
/* 0xbf */ { NULL, NULL, 0 },
/* 0xc0 */ { "SMBsplopen",reply_printopen,AS_USER},
/* 0xc1 */ { "SMBsplwr",reply_printwrite,AS_USER},
/* 0xc2 */ { "SMBsplclose",reply_printclose,AS_USER},
/* 0xc3 */ { "SMBsplretq",reply_printqueue,AS_USER},
/* 0xc4 */ { NULL, NULL, 0 },
/* 0xc5 */ { NULL, NULL, 0 },
/* 0xc6 */ { NULL, NULL, 0 },
/* 0xc7 */ { NULL, NULL, 0 },
/* 0xc8 */ { NULL, NULL, 0 },
/* 0xc9 */ { NULL, NULL, 0 },
/* 0xca */ { NULL, NULL, 0 },
/* 0xcb */ { NULL, NULL, 0 },
/* 0xcc */ { NULL, NULL, 0 },
/* 0xcd */ { NULL, NULL, 0 },
/* 0xce */ { NULL, NULL, 0 },
/* 0xcf */ { NULL, NULL, 0 },
/* 0xd0 */ { "SMBsends",reply_sends,AS_GUEST},
/* 0xd1 */ { "SMBsendb", NULL,AS_GUEST},
/* 0xd2 */ { "SMBfwdname", NULL,AS_GUEST},
/* 0xd3 */ { "SMBcancelf", NULL,AS_GUEST},
/* 0xd4 */ { "SMBgetmac", NULL,AS_GUEST},
/* 0xd5 */ { "SMBsendstrt",reply_sendstrt,AS_GUEST},
/* 0xd6 */ { "SMBsendend",reply_sendend,AS_GUEST},
/* 0xd7 */ { "SMBsendtxt",reply_sendtxt,AS_GUEST},
/* 0xd8 */ { NULL, NULL, 0 },
/* 0xd9 */ { NULL, NULL, 0 },
/* 0xda */ { NULL, NULL, 0 },
/* 0xdb */ { NULL, NULL, 0 },
/* 0xdc */ { NULL, NULL, 0 },
/* 0xdd */ { NULL, NULL, 0 },
/* 0xde */ { NULL, NULL, 0 },
/* 0xdf */ { NULL, NULL, 0 },
/* 0xe0 */ { NULL, NULL, 0 },
/* 0xe1 */ { NULL, NULL, 0 },
/* 0xe2 */ { NULL, NULL, 0 },
/* 0xe3 */ { NULL, NULL, 0 },
/* 0xe4 */ { NULL, NULL, 0 },
/* 0xe5 */ { NULL, NULL, 0 },
/* 0xe6 */ { NULL, NULL, 0 },
/* 0xe7 */ { NULL, NULL, 0 },
/* 0xe8 */ { NULL, NULL, 0 },
/* 0xe9 */ { NULL, NULL, 0 },
/* 0xea */ { NULL, NULL, 0 },
/* 0xeb */ { NULL, NULL, 0 },
/* 0xec */ { NULL, NULL, 0 },
/* 0xed */ { NULL, NULL, 0 },
/* 0xee */ { NULL, NULL, 0 },
/* 0xef */ { NULL, NULL, 0 },
/* 0xf0 */ { NULL, NULL, 0 },
/* 0xf1 */ { NULL, NULL, 0 },
/* 0xf2 */ { NULL, NULL, 0 },
/* 0xf3 */ { NULL, NULL, 0 },
/* 0xf4 */ { NULL, NULL, 0 },
/* 0xf5 */ { NULL, NULL, 0 },
/* 0xf6 */ { NULL, NULL, 0 },
/* 0xf7 */ { NULL, NULL, 0 },
/* 0xf8 */ { NULL, NULL, 0 },
/* 0xf9 */ { NULL, NULL, 0 },
/* 0xfa */ { NULL, NULL, 0 },
/* 0xfb */ { NULL, NULL, 0 },
/* 0xfc */ { NULL, NULL, 0 },
/* 0xfd */ { NULL, NULL, 0 },
/* 0xfe */ { NULL, NULL, 0 },
/* 0xff */ { NULL, NULL, 0 }

};

/*******************************************************************
 allocate and initialize a reply packet
********************************************************************/

static bool create_outbuf(TALLOC_CTX *mem_ctx, struct smb_request *req,
			  const char *inbuf, char **outbuf, uint8_t num_words,
			  uint32_t num_bytes)
{
	/*
         * Protect against integer wrap
         */
	if ((num_bytes > 0xffffff)
	    || ((num_bytes + smb_size + num_words*2) > 0xffffff)) {
		char *msg;
		if (asprintf(&msg, "num_bytes too large: %u",
			     (unsigned)num_bytes) == -1) {
			msg = CONST_DISCARD(char *, "num_bytes too large");
		}
		smb_panic(msg);
	}

	*outbuf = TALLOC_ARRAY(mem_ctx, char,
			       smb_size + num_words*2 + num_bytes);
	if (*outbuf == NULL) {
		return false;
	}

	construct_reply_common(req, inbuf, *outbuf);
	srv_set_message(*outbuf, num_words, num_bytes, false);
	/*
	 * Zero out the word area, the caller has to take care of the bcc area
	 * himself
	 */
	if (num_words != 0) {
		memset(*outbuf + smb_vwv0, 0, num_words*2);
	}

	return true;
}

void reply_outbuf(struct smb_request *req, uint8 num_words, uint32 num_bytes)
{
	char *outbuf;
	if (!create_outbuf(req, req, (char *)req->inbuf, &outbuf, num_words,
			   num_bytes)) {
		smb_panic("could not allocate output buffer\n");
	}
	req->outbuf = (uint8_t *)outbuf;
}


/*******************************************************************
 Dump a packet to a file.
********************************************************************/

static void smb_dump(const char *name, int type, const char *data, ssize_t len)
{
	int fd, i;
	char *fname = NULL;
	if (DEBUGLEVEL < 50) {
		return;
	}

	if (len < 4) len = smb_len(data)+4;
	for (i=1;i<100;i++) {
		if (asprintf(&fname, "/tmp/%s.%d.%s", name, i,
			     type ? "req" : "resp") == -1) {
			return;
		}
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL, 0644);
		if (fd != -1 || errno != EEXIST) break;
	}
	if (fd != -1) {
		ssize_t ret = write(fd, data, len);
		if (ret != len)
			DEBUG(0,("smb_dump: problem: write returned %d\n", (int)ret ));
		close(fd);
		DEBUG(0,("created %s len %lu\n", fname, (unsigned long)len));
	}
	SAFE_FREE(fname);
}

/****************************************************************************
 Prepare everything for calling the actual request function, and potentially
 call the request function via the "new" interface.

 Return False if the "legacy" function needs to be called, everything is
 prepared.

 Return True if we're done.

 I know this API sucks, but it is the one with the least code change I could
 find.
****************************************************************************/

static connection_struct *switch_message(uint8 type, struct smb_request *req, int size)
{
	int flags;
	uint16 session_tag;
	connection_struct *conn = NULL;
	struct smbd_server_connection *sconn = req->sconn;

	errno = 0;

	/* Make sure this is an SMB packet. smb_size contains NetBIOS header
	 * so subtract 4 from it. */
	if ((size < (smb_size - 4)) ||
	    !valid_smb_header(req->inbuf)) {
		DEBUG(2,("Non-SMB packet of length %d. Terminating server\n",
			 smb_len(req->inbuf)));
		exit_server_cleanly("Non-SMB packet");
	}

	if (smb_messages[type].fn == NULL) {
		DEBUG(0,("Unknown message type %d!\n",type));
		smb_dump("Unknown", 1, (char *)req->inbuf, size);
		reply_unknown_new(req, type);
		return NULL;
	}

	flags = smb_messages[type].flags;

	/* In share mode security we must ignore the vuid. */
	session_tag = (lp_security() == SEC_SHARE)
		? UID_FIELD_INVALID : req->vuid;
	conn = req->conn;

	DEBUG(3,("switch message %s (pid %d) conn 0x%lx\n", smb_fn_name(type),
		 (int)sys_getpid(), (unsigned long)conn));

	smb_dump(smb_fn_name(type), 1, (char *)req->inbuf, size);

	/* Ensure this value is replaced in the incoming packet. */
	SSVAL(req->inbuf,smb_uid,session_tag);

	/*
	 * Ensure the correct username is in current_user_info.  This is a
	 * really ugly bugfix for problems with multiple session_setup_and_X's
	 * being done and allowing %U and %G substitutions to work correctly.
	 * There is a reason this code is done here, don't move it unless you
	 * know what you're doing... :-).
	 * JRA.
	 */

	if (session_tag != sconn->smb1.sessions.last_session_tag) {
		user_struct *vuser = NULL;

		sconn->smb1.sessions.last_session_tag = session_tag;
		if(session_tag != UID_FIELD_INVALID) {
			vuser = get_valid_user_struct(sconn, session_tag);
			if (vuser) {
				set_current_user_info(
					vuser->session_info->sanitized_username,
					vuser->session_info->unix_name,
					vuser->session_info->info3->base.domain.string);
			}
		}
	}

	/* Does this call need to be run as the connected user? */
	if (flags & AS_USER) {

		/* Does this call need a valid tree connection? */
		if (!conn) {
			/*
			 * Amazingly, the error code depends on the command
			 * (from Samba4).
			 */
			if (type == SMBntcreateX) {
				reply_nterror(req, NT_STATUS_INVALID_HANDLE);
			} else {
				reply_nterror(req, NT_STATUS_NETWORK_NAME_DELETED);
			}
			return NULL;
		}

		if (!change_to_user(conn,session_tag)) {
			DEBUG(0, ("Error: Could not change to user. Removing "
				"deferred open, mid=%llu.\n",
				(unsigned long long)req->mid));
			reply_force_doserror(req, ERRSRV, ERRbaduid);
			return conn;
		}

		/* All NEED_WRITE and CAN_IPC flags must also have AS_USER. */

		/* Does it need write permission? */
		if ((flags & NEED_WRITE) && !CAN_WRITE(conn)) {
			reply_nterror(req, NT_STATUS_MEDIA_WRITE_PROTECTED);
			return conn;
		}

		/* IPC services are limited */
		if (IS_IPC(conn) && !(flags & CAN_IPC)) {
			reply_nterror(req, NT_STATUS_ACCESS_DENIED);
			return conn;
		}
	} else {
		/* This call needs to be run as root */
		change_to_root_user();
	}

	/* load service specific parameters */
	if (conn) {
		if (req->encrypted) {
			conn->encrypted_tid = true;
			/* encrypted required from now on. */
			conn->encrypt_level = Required;
		} else if (ENCRYPTION_REQUIRED(conn)) {
			if (req->cmd != SMBtrans2 && req->cmd != SMBtranss2) {
				exit_server_cleanly("encryption required "
					"on connection");
				return conn;
			}
		}

		if (!set_current_service(conn,SVAL(req->inbuf,smb_flg),
					 (flags & (AS_USER|DO_CHDIR)
					  ?True:False))) {
			reply_nterror(req, NT_STATUS_ACCESS_DENIED);
			return conn;
		}
		conn->num_smb_operations++;
	}

	/* does this protocol need to be run as guest? */
	if ((flags & AS_GUEST)
	    && (!change_to_guest() ||
		!allow_access(lp_hostsdeny(-1), lp_hostsallow(-1),
			      sconn->client_id.name,
			      sconn->client_id.addr))) {
		reply_nterror(req, NT_STATUS_ACCESS_DENIED);
		return conn;
	}

	smb_messages[type].fn(req);
	return req->conn;
}

/****************************************************************************
 Construct a reply to the incoming packet.
****************************************************************************/

static void construct_reply(struct smbd_server_connection *sconn,
			    char *inbuf, int size, size_t unread_bytes,
			    uint32_t seqnum, bool encrypted,
			    struct smb_perfcount_data *deferred_pcd)
{
	connection_struct *conn;
	struct smb_request *req;

	if (!(req = talloc(talloc_tos(), struct smb_request))) {
		smb_panic("could not allocate smb_request");
	}

	if (!init_smb_request(req, sconn, (uint8 *)inbuf, unread_bytes,
			      encrypted, seqnum)) {
		exit_server_cleanly("Invalid SMB request");
	}

	req->inbuf  = (uint8_t *)talloc_move(req, &inbuf);

	/* we popped this message off the queue - keep original perf data */
	if (deferred_pcd)
		req->pcd = *deferred_pcd;
	else {
		SMB_PERFCOUNT_START(&req->pcd);
		SMB_PERFCOUNT_SET_OP(&req->pcd, req->cmd);
		SMB_PERFCOUNT_SET_MSGLEN_IN(&req->pcd, size);
	}

	conn = switch_message(req->cmd, req, size);

	if (req->unread_bytes) {
		/* writeX failed. drain socket. */
		if (drain_socket(req->sconn->sock, req->unread_bytes) !=
				req->unread_bytes) {
			smb_panic("failed to drain pending bytes");
		}
		req->unread_bytes = 0;
	}

	if (req->done) {
		TALLOC_FREE(req);
		return;
	}

	if (req->outbuf == NULL) {
		return;
	}

	if (CVAL(req->outbuf,0) == 0) {
		show_msg((char *)req->outbuf);
	}

	if (!srv_send_smb(req->sconn,
			(char *)req->outbuf,
			true, req->seqnum+1,
			IS_CONN_ENCRYPTED(conn)||req->encrypted,
			&req->pcd)) {
		exit_server_cleanly("construct_reply: srv_send_smb failed.");
	}

	TALLOC_FREE(req);

	return;
}

/****************************************************************************
 Process an smb from the client
****************************************************************************/
static void process_smb(struct smbd_server_connection *sconn,
			uint8_t *inbuf, size_t nread, size_t unread_bytes,
			uint32_t seqnum, bool encrypted,
			struct smb_perfcount_data *deferred_pcd)
{
	int msg_type = CVAL(inbuf,0);

	DO_PROFILE_INC(smb_count);

	DEBUG( 6, ( "got message type 0x%x of len 0x%x\n", msg_type,
		    smb_len(inbuf) ) );
	DEBUG(3, ("Transaction %d of length %d (%u toread)\n",
		  sconn->trans_num, (int)nread, (unsigned int)unread_bytes));

	if (msg_type != 0) {
		/*
		 * NetBIOS session request, keepalive, etc.
		 */
		reply_special(sconn, (char *)inbuf, nread);
		goto done;
	}

	if (sconn->using_smb2) {
		/* At this point we're not really using smb2,
		 * we make the decision here.. */
		if (smbd_is_smb2_header(inbuf, nread)) {
			smbd_smb2_first_negprot(sconn, inbuf, nread);
			return;
		} else if (nread >= smb_size && valid_smb_header(inbuf)
				&& CVAL(inbuf, smb_com) != 0x72) {
			/* This is a non-negprot SMB1 packet.
			   Disable SMB2 from now on. */
			sconn->using_smb2 = false;
		}
	}

	show_msg((char *)inbuf);

	construct_reply(sconn, (char *)inbuf, nread, unread_bytes, seqnum,
			encrypted, deferred_pcd);
	sconn->trans_num++;

done:
	sconn->num_requests++;

	/* The timeout_processing function isn't run nearly
	   often enough to implement 'max log size' without
	   overrunning the size of the file by many megabytes.
	   This is especially true if we are running at debug
	   level 10.  Checking every 50 SMBs is a nice
	   tradeoff of performance vs log file size overrun. */

	if ((sconn->num_requests % 50) == 0 &&
	    need_to_check_log_size()) {
		change_to_root_user();
		check_log_size();
	}
}

/****************************************************************************
 Return a string containing the function name of a SMB command.
****************************************************************************/

const char *smb_fn_name(int type)
{
	const char *unknown_name = "SMBunknown";

	if (smb_messages[type].name == NULL)
		return(unknown_name);

	return(smb_messages[type].name);
}

/****************************************************************************
 Helper functions for contruct_reply.
****************************************************************************/

void add_to_common_flags2(uint32 v)
{
	common_flags2 |= v;
}

void remove_from_common_flags2(uint32 v)
{
	common_flags2 &= ~v;
}

static void construct_reply_common(struct smb_request *req, const char *inbuf,
				   char *outbuf)
{
	srv_set_message(outbuf,0,0,false);

	SCVAL(outbuf, smb_com, req->cmd);
	SIVAL(outbuf,smb_rcls,0);
	SCVAL(outbuf,smb_flg, FLAG_REPLY | (CVAL(inbuf,smb_flg) & FLAG_CASELESS_PATHNAMES)); 
	SSVAL(outbuf,smb_flg2,
		(SVAL(inbuf,smb_flg2) & FLAGS2_UNICODE_STRINGS) |
		common_flags2);
	memset(outbuf+smb_pidhigh,'\0',(smb_tid-smb_pidhigh));

	SSVAL(outbuf,smb_tid,SVAL(inbuf,smb_tid));
	SSVAL(outbuf,smb_pid,SVAL(inbuf,smb_pid));
	SSVAL(outbuf,smb_uid,SVAL(inbuf,smb_uid));
	SSVAL(outbuf,smb_mid,SVAL(inbuf,smb_mid));
}

void construct_reply_common_req(struct smb_request *req, char *outbuf)
{
	construct_reply_common(req, (char *)req->inbuf, outbuf);
}

/*
 * How many bytes have we already accumulated up to the current wct field
 * offset?
 */

size_t req_wct_ofs(struct smb_request *req)
{
	size_t buf_size;

	if (req->chain_outbuf == NULL) {
		return smb_wct - 4;
	}
	buf_size = talloc_get_size(req->chain_outbuf);
	if ((buf_size % 4) != 0) {
		buf_size += (4 - (buf_size % 4));
	}
	return buf_size - 4;
}

/*
 * Hack around reply_nterror & friends not being aware of chained requests,
 * generating illegal (i.e. wct==0) chain replies.
 */

static void fixup_chain_error_packet(struct smb_request *req)
{
	uint8_t *outbuf = req->outbuf;
	req->outbuf = NULL;
	reply_outbuf(req, 2, 0);
	memcpy(req->outbuf, outbuf, smb_wct);
	TALLOC_FREE(outbuf);
	SCVAL(req->outbuf, smb_vwv0, 0xff);
}

/**
 * @brief Find the smb_cmd offset of the last command pushed
 * @param[in] buf	The buffer we're building up
 * @retval		Where can we put our next andx cmd?
 *
 * While chaining requests, the "next" request we're looking at needs to put
 * its SMB_Command before the data the previous request already built up added
 * to the chain. Find the offset to the place where we have to put our cmd.
 */

static bool find_andx_cmd_ofs(uint8_t *buf, size_t *pofs)
{
	uint8_t cmd;
	size_t ofs;

	cmd = CVAL(buf, smb_com);

	SMB_ASSERT(is_andx_req(cmd));

	ofs = smb_vwv0;

	while (CVAL(buf, ofs) != 0xff) {

		if (!is_andx_req(CVAL(buf, ofs))) {
			return false;
		}

		/*
		 * ofs is from start of smb header, so add the 4 length
		 * bytes. The next cmd is right after the wct field.
		 */
		ofs = SVAL(buf, ofs+2) + 4 + 1;

		SMB_ASSERT(ofs+4 < talloc_get_size(buf));
	}

	*pofs = ofs;
	return true;
}

/**
 * @brief Do the smb chaining at a buffer level
 * @param[in] poutbuf		Pointer to the talloc'ed buffer to be modified
 * @param[in] smb_command	The command that we want to issue
 * @param[in] wct		How many words?
 * @param[in] vwv		The words, already in network order
 * @param[in] bytes_alignment	How shall we align "bytes"?
 * @param[in] num_bytes		How many bytes?
 * @param[in] bytes		The data the request ships
 *
 * smb_splice_chain() adds the vwv and bytes to the request already present in
 * *poutbuf.
 */

static bool smb_splice_chain(uint8_t **poutbuf, uint8_t smb_command,
			     uint8_t wct, const uint16_t *vwv,
			     size_t bytes_alignment,
			     uint32_t num_bytes, const uint8_t *bytes)
{
	uint8_t *outbuf;
	size_t old_size, new_size;
	size_t ofs;
	size_t chain_padding = 0;
	size_t bytes_padding = 0;
	bool first_request;

	old_size = talloc_get_size(*poutbuf);

	/*
	 * old_size == smb_wct means we're pushing the first request in for
	 * libsmb/
	 */

	first_request = (old_size == smb_wct);

	if (!first_request && ((old_size % 4) != 0)) {
		/*
		 * Align the wct field of subsequent requests to a 4-byte
		 * boundary
		 */
		chain_padding = 4 - (old_size % 4);
	}

	/*
	 * After the old request comes the new wct field (1 byte), the vwv's
	 * and the num_bytes field. After at we might need to align the bytes
	 * given to us to "bytes_alignment", increasing the num_bytes value.
	 */

	new_size = old_size + chain_padding + 1 + wct * sizeof(uint16_t) + 2;

	if ((bytes_alignment != 0) && ((new_size % bytes_alignment) != 0)) {
		bytes_padding = bytes_alignment - (new_size % bytes_alignment);
	}

	new_size += bytes_padding + num_bytes;

	if ((smb_command != SMBwriteX) && (new_size > 0xffff)) {
		DEBUG(1, ("splice_chain: %u bytes won't fit\n",
			  (unsigned)new_size));
		return false;
	}

	outbuf = TALLOC_REALLOC_ARRAY(NULL, *poutbuf, uint8_t, new_size);
	if (outbuf == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return false;
	}
	*poutbuf = outbuf;

	if (first_request) {
		SCVAL(outbuf, smb_com, smb_command);
	} else {
		size_t andx_cmd_ofs;

		if (!find_andx_cmd_ofs(outbuf, &andx_cmd_ofs)) {
			DEBUG(1, ("invalid command chain\n"));
			*poutbuf = TALLOC_REALLOC_ARRAY(
				NULL, *poutbuf, uint8_t, old_size);
			return false;
		}

		if (chain_padding != 0) {
			memset(outbuf + old_size, 0, chain_padding);
			old_size += chain_padding;
		}

		SCVAL(outbuf, andx_cmd_ofs, smb_command);
		SSVAL(outbuf, andx_cmd_ofs + 2, old_size - 4);
	}

	ofs = old_size;

	/*
	 * Push the chained request:
	 *
	 * wct field
	 */

	SCVAL(outbuf, ofs, wct);
	ofs += 1;

	/*
	 * vwv array
	 */

	memcpy(outbuf + ofs, vwv, sizeof(uint16_t) * wct);
	ofs += sizeof(uint16_t) * wct;

	/*
	 * bcc (byte count)
	 */

	SSVAL(outbuf, ofs, num_bytes + bytes_padding);
	ofs += sizeof(uint16_t);

	/*
	 * padding
	 */

	if (bytes_padding != 0) {
		memset(outbuf + ofs, 0, bytes_padding);
		ofs += bytes_padding;
	}

	/*
	 * The bytes field
	 */

	memcpy(outbuf + ofs, bytes, num_bytes);

	return true;
}

/****************************************************************************
 Construct a chained reply and add it to the already made reply
****************************************************************************/

void chain_reply(struct smb_request *req)
{
	size_t smblen = smb_len(req->inbuf);
	size_t already_used, length_needed;
	uint8_t chain_cmd;
	uint32_t chain_offset;	/* uint32_t to avoid overflow */

	uint8_t wct;
	uint16_t *vwv;
	uint16_t buflen;
	uint8_t *buf;

	if (IVAL(req->outbuf, smb_rcls) != 0) {
		fixup_chain_error_packet(req);
	}

	/*
	 * Any of the AndX requests and replies have at least a wct of
	 * 2. vwv[0] is the next command, vwv[1] is the offset from the
	 * beginning of the SMB header to the next wct field.
	 *
	 * None of the AndX requests put anything valuable in vwv[0] and [1],
	 * so we can overwrite it here to form the chain.
	 */

	if ((req->wct < 2) || (CVAL(req->outbuf, smb_wct) < 2)) {
		if (req->chain_outbuf == NULL) {
			req->chain_outbuf = TALLOC_REALLOC_ARRAY(
				req, req->outbuf, uint8_t,
				smb_len(req->outbuf) + 4);
			if (req->chain_outbuf == NULL) {
				smb_panic("talloc failed");
			}
		}
		req->outbuf = NULL;
		goto error;
	}

	/*
	 * Here we assume that this is the end of the chain. For that we need
	 * to set "next command" to 0xff and the offset to 0. If we later find
	 * more commands in the chain, this will be overwritten again.
	 */

	SCVAL(req->outbuf, smb_vwv0, 0xff);
	SCVAL(req->outbuf, smb_vwv0+1, 0);
	SSVAL(req->outbuf, smb_vwv1, 0);

	if (req->chain_outbuf == NULL) {
		/*
		 * In req->chain_outbuf we collect all the replies. Start the
		 * chain by copying in the first reply.
		 *
		 * We do the realloc because later on we depend on
		 * talloc_get_size to determine the length of
		 * chain_outbuf. The reply_xxx routines might have
		 * over-allocated (reply_pipe_read_and_X used to be such an
		 * example).
		 */
		req->chain_outbuf = TALLOC_REALLOC_ARRAY(
			req, req->outbuf, uint8_t,
			smb_len_large(req->outbuf) + 4);
		if (req->chain_outbuf == NULL) {
			smb_panic("talloc failed");
		}
		req->outbuf = NULL;
	} else {
		/*
		 * Update smb headers where subsequent chained commands
		 * may have updated them.
		 */
		SSVAL(req->chain_outbuf, smb_tid, SVAL(req->outbuf, smb_tid));
		SSVAL(req->chain_outbuf, smb_uid, SVAL(req->outbuf, smb_uid));

		if (!smb_splice_chain(&req->chain_outbuf,
				      CVAL(req->outbuf, smb_com),
				      CVAL(req->outbuf, smb_wct),
				      (uint16_t *)(req->outbuf + smb_vwv),
				      0, smb_buflen(req->outbuf),
				      (uint8_t *)smb_buf(req->outbuf))) {
			goto error;
		}
		TALLOC_FREE(req->outbuf);
	}

	/*
	 * We use the old request's vwv field to grab the next chained command
	 * and offset into the chained fields.
	 */

	chain_cmd = CVAL(req->vwv+0, 0);
	chain_offset = SVAL(req->vwv+1, 0);

	if (chain_cmd == 0xff) {
		/*
		 * End of chain, no more requests from the client. So ship the
		 * replies.
		 */
		smb_setlen((char *)(req->chain_outbuf),
			   talloc_get_size(req->chain_outbuf) - 4);

		if (!srv_send_smb(req->sconn, (char *)req->chain_outbuf,
				  true, req->seqnum+1,
				  IS_CONN_ENCRYPTED(req->conn)
				  ||req->encrypted,
				  &req->pcd)) {
			exit_server_cleanly("chain_reply: srv_send_smb "
					    "failed.");
		}
		TALLOC_FREE(req->chain_outbuf);
		req->done = true;
		return;
	}

	/* add a new perfcounter for this element of chain */
	SMB_PERFCOUNT_ADD(&req->pcd);
	SMB_PERFCOUNT_SET_OP(&req->pcd, chain_cmd);
	SMB_PERFCOUNT_SET_MSGLEN_IN(&req->pcd, smblen);

	/*
	 * Check if the client tries to fool us. The chain offset
	 * needs to point beyond the current request in the chain, it
	 * needs to strictly grow. Otherwise we might be tricked into
	 * an endless loop always processing the same request over and
	 * over again. We used to assume that vwv and the byte buffer
	 * array in a chain are always attached, but OS/2 the
	 * Write&X/Read&X chain puts the Read&X vwv array right behind
	 * the Write&X vwv chain. The Write&X bcc array is put behind
	 * the Read&X vwv array. So now we check whether the chain
	 * offset points strictly behind the previous vwv
	 * array. req->buf points right after the vwv array of the
	 * previous request. See
	 * https://bugzilla.samba.org/show_bug.cgi?id=8360 for more
	 * information.
	 */

	already_used = PTR_DIFF(req->buf, smb_base(req->inbuf));
	if (chain_offset <= already_used) {
		goto error;
	}

	/*
	 * Next check: Make sure the chain offset does not point beyond the
	 * overall smb request length.
	 */

	length_needed = chain_offset+1;	/* wct */
	if (length_needed > smblen) {
		goto error;
	}

	/*
	 * Now comes the pointer magic. Goal here is to set up req->vwv and
	 * req->buf correctly again to be able to call the subsequent
	 * switch_message(). The chain offset (the former vwv[1]) points at
	 * the new wct field.
	 */

	wct = CVAL(smb_base(req->inbuf), chain_offset);

	/*
	 * Next consistency check: Make the new vwv array fits in the overall
	 * smb request.
	 */

	length_needed += (wct+1)*sizeof(uint16_t); /* vwv+buflen */
	if (length_needed > smblen) {
		goto error;
	}
	vwv = (uint16_t *)(smb_base(req->inbuf) + chain_offset + 1);

	/*
	 * Now grab the new byte buffer....
	 */

	buflen = SVAL(vwv+wct, 0);

	/*
	 * .. and check that it fits.
	 */

	length_needed += buflen;
	if (length_needed > smblen) {
		goto error;
	}
	buf = (uint8_t *)(vwv+wct+1);

	req->cmd = chain_cmd;
	req->wct = wct;
	req->vwv = vwv;
	req->buflen = buflen;
	req->buf = buf;

	switch_message(chain_cmd, req, smblen);

	if (req->outbuf == NULL) {
		/*
		 * This happens if the chained command has suspended itself or
		 * if it has called srv_send_smb() itself.
		 */
		return;
	}

	/*
	 * We end up here if the chained command was not itself chained or
	 * suspended, but for example a close() command. We now need to splice
	 * the chained commands' outbuf into the already built up chain_outbuf
	 * and ship the result.
	 */
	goto done;

 error:
	/*
	 * We end up here if there's any error in the chain syntax. Report a
	 * DOS error, just like Windows does.
	 */
	reply_force_doserror(req, ERRSRV, ERRerror);
	fixup_chain_error_packet(req);

 done:
	/*
	 * This scary statement intends to set the
	 * FLAGS2_32_BIT_ERROR_CODES flg2 field in req->chain_outbuf
	 * to the value req->outbuf carries
	 */
	SSVAL(req->chain_outbuf, smb_flg2,
	      (SVAL(req->chain_outbuf, smb_flg2) & ~FLAGS2_32_BIT_ERROR_CODES)
	      | (SVAL(req->outbuf, smb_flg2) & FLAGS2_32_BIT_ERROR_CODES));

	/*
	 * Transfer the error codes from the subrequest to the main one
	 */
	SSVAL(req->chain_outbuf, smb_rcls, SVAL(req->outbuf, smb_rcls));
	SSVAL(req->chain_outbuf, smb_err, SVAL(req->outbuf, smb_err));

	if (!smb_splice_chain(&req->chain_outbuf,
			      CVAL(req->outbuf, smb_com),
			      CVAL(req->outbuf, smb_wct),
			      (uint16_t *)(req->outbuf + smb_vwv),
			      0, smb_buflen(req->outbuf),
			      (uint8_t *)smb_buf(req->outbuf))) {
		exit_server_cleanly("chain_reply: smb_splice_chain failed\n");
	}
	TALLOC_FREE(req->outbuf);

	smb_setlen((char *)(req->chain_outbuf),
		   talloc_get_size(req->chain_outbuf) - 4);

	show_msg((char *)(req->chain_outbuf));

	if (!srv_send_smb(req->sconn, (char *)req->chain_outbuf,
			  true, req->seqnum+1,
			  IS_CONN_ENCRYPTED(req->conn)||req->encrypted,
			  &req->pcd)) {
		exit_server_cleanly("chain_reply: srv_send_smb failed.");
	}
	TALLOC_FREE(req->chain_outbuf);
	req->done = true;
}

/****************************************************************************
 Check if services need reloading.
****************************************************************************/

static void check_reload(struct smbd_server_connection *sconn, time_t t)
{

	if (last_smb_conf_reload_time == 0) {
		last_smb_conf_reload_time = t;
	}

	if (t >= last_smb_conf_reload_time+SMBD_RELOAD_CHECK) {
		reload_services(sconn->msg_ctx, sconn->sock, True);
		last_smb_conf_reload_time = t;
	}
}

static bool fd_is_readable(int fd)
{
	int ret, revents;

	ret = poll_one_fd(fd, POLLIN|POLLHUP, 0, &revents);

	return ((ret > 0) && ((revents & (POLLIN|POLLHUP|POLLERR)) != 0));

}

static void smbd_server_connection_write_handler(struct smbd_server_connection *conn)
{
	/* TODO: make write nonblocking */
}

static void smbd_server_connection_read_handler(
	struct smbd_server_connection *conn, int fd)
{
	uint8_t *inbuf = NULL;
	size_t inbuf_len = 0;
	size_t unread_bytes = 0;
	bool encrypted = false;
	TALLOC_CTX *mem_ctx = talloc_tos();
	NTSTATUS status;
	uint32_t seqnum;

	bool from_client = (conn->sock == fd);

	if (from_client) {
		smbd_lock_socket(conn);

		if (lp_async_smb_echo_handler() && !fd_is_readable(fd)) {
			DEBUG(10,("the echo listener was faster\n"));
			smbd_unlock_socket(conn);
			return;
		}

		/* TODO: make this completely nonblocking */
		status = receive_smb_talloc(mem_ctx, conn, fd,
					    (char **)(void *)&inbuf,
					    0, /* timeout */
					    &unread_bytes,
					    &encrypted,
					    &inbuf_len, &seqnum,
					    false /* trusted channel */);
		smbd_unlock_socket(conn);
	} else {
		/* TODO: make this completely nonblocking */
		status = receive_smb_talloc(mem_ctx, conn, fd,
					    (char **)(void *)&inbuf,
					    0, /* timeout */
					    &unread_bytes,
					    &encrypted,
					    &inbuf_len, &seqnum,
					    true /* trusted channel */);
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_RETRY)) {
		goto process;
	}
	if (NT_STATUS_IS_ERR(status)) {
		exit_server_cleanly("failed to receive smb request");
	}
	if (!NT_STATUS_IS_OK(status)) {
		return;
	}

process:
	process_smb(conn, inbuf, inbuf_len, unread_bytes,
		    seqnum, encrypted, NULL);
}

static void smbd_server_connection_handler(struct event_context *ev,
					   struct fd_event *fde,
					   uint16_t flags,
					   void *private_data)
{
	struct smbd_server_connection *conn = talloc_get_type(private_data,
					      struct smbd_server_connection);

	if (flags & EVENT_FD_WRITE) {
		smbd_server_connection_write_handler(conn);
		return;
	}
	if (flags & EVENT_FD_READ) {
		smbd_server_connection_read_handler(conn, conn->sock);
		return;
	}
}

static void smbd_server_echo_handler(struct event_context *ev,
				     struct fd_event *fde,
				     uint16_t flags,
				     void *private_data)
{
	struct smbd_server_connection *conn = talloc_get_type(private_data,
					      struct smbd_server_connection);

	if (flags & EVENT_FD_WRITE) {
		smbd_server_connection_write_handler(conn);
		return;
	}
	if (flags & EVENT_FD_READ) {
		smbd_server_connection_read_handler(
			conn, conn->smb1.echo_handler.trusted_fd);
		return;
	}
}

/****************************************************************************
received when we should release a specific IP
****************************************************************************/
static void release_ip(const char *ip, void *priv)
{
	const char *addr = (const char *)priv;
	const char *p = addr;

	if (strncmp("::ffff:", addr, 7) == 0) {
		p = addr + 7;
	}

	DEBUG(10, ("Got release IP message for %s, "
		   "our address is %s\n", ip, p));

	if ((strcmp(p, ip) == 0) || ((p != addr) && strcmp(addr, ip) == 0)) {
		/* we can't afford to do a clean exit - that involves
		   database writes, which would potentially mean we
		   are still running after the failover has finished -
		   we have to get rid of this process ID straight
		   away */
		DEBUG(0,("Got release IP message for our IP %s - exiting immediately\n",
			ip));
		/* note we must exit with non-zero status so the unclean handler gets
		   called in the parent, so that the brl database is tickled */
		_exit(1);
	}
}

static void msg_release_ip(struct messaging_context *msg_ctx, void *private_data,
			   uint32_t msg_type, struct server_id server_id, DATA_BLOB *data)
{
	struct smbd_server_connection *sconn = talloc_get_type_abort(
		private_data, struct smbd_server_connection);

	release_ip((char *)data->data, sconn->client_id.addr);
}

#ifdef CLUSTER_SUPPORT
static int client_get_tcp_info(int sock, struct sockaddr_storage *server,
			       struct sockaddr_storage *client)
{
	socklen_t length;
	length = sizeof(*server);
	if (getsockname(sock, (struct sockaddr *)server, &length) != 0) {
		return -1;
	}
	length = sizeof(*client);
	if (getpeername(sock, (struct sockaddr *)client, &length) != 0) {
		return -1;
	}
	return 0;
}
#endif

/*
 * Send keepalive packets to our client
 */
static bool keepalive_fn(const struct timeval *now, void *private_data)
{
	struct smbd_server_connection *sconn = smbd_server_conn;
	bool ret;

	if (sconn->using_smb2) {
		/* Don't do keepalives on an SMB2 connection. */
		return false;
	}

	smbd_lock_socket(smbd_server_conn);
	ret = send_keepalive(sconn->sock);
	smbd_unlock_socket(smbd_server_conn);

	if (!ret) {
		char addr[INET6_ADDRSTRLEN];
		/*
		 * Try and give an error message saying what
		 * client failed.
		 */
		DEBUG(0, ("send_keepalive failed for client %s. "
			  "Error %s - exiting\n",
			  get_peer_addr(sconn->sock, addr, sizeof(addr)),
			  strerror(errno)));
		return False;
	}
	return True;
}

/*
 * Do the recurring check if we're idle
 */
static bool deadtime_fn(const struct timeval *now, void *private_data)
{
	struct smbd_server_connection *sconn =
		(struct smbd_server_connection *)private_data;

	if ((conn_num_open(sconn) == 0)
	    || (conn_idle_all(sconn, now->tv_sec))) {
		DEBUG( 2, ( "Closing idle connection\n" ) );
		messaging_send(sconn->msg_ctx,
			       messaging_server_id(sconn->msg_ctx),
			       MSG_SHUTDOWN, &data_blob_null);
		return False;
	}

	return True;
}

/*
 * Do the recurring log file and smb.conf reload checks.
 */

static bool housekeeping_fn(const struct timeval *now, void *private_data)
{
	struct smbd_server_connection *sconn = talloc_get_type_abort(
		private_data, struct smbd_server_connection);

	DEBUG(5, ("housekeeping\n"));

	change_to_root_user();

#ifdef PRINTER_SUPPORT
	/* update printer queue caches if necessary */
	update_monitored_printq_cache(sconn->msg_ctx);
#endif

	/* check if we need to reload services */
	check_reload(sconn, time_mono(NULL));

#ifdef NETLOGON_SUPPORT
	/* Change machine password if neccessary. */
	attempt_machine_password_change();
#endif

        /*
	 * Force a log file check.
	 */
	force_check_log_size();
	check_log_size();
	return true;
}

static int create_unlink_tmp(const char *dir)
{
	char *fname;
	int fd;

	fname = talloc_asprintf(talloc_tos(), "%s/listenerlock_XXXXXX", dir);
	if (fname == NULL) {
		errno = ENOMEM;
		return -1;
	}
	fd = mkstemp(fname);
	if (fd == -1) {
		TALLOC_FREE(fname);
		return -1;
	}
	if (unlink(fname) == -1) {
		int sys_errno = errno;
		close(fd);
		TALLOC_FREE(fname);
		errno = sys_errno;
		return -1;
	}
	TALLOC_FREE(fname);
	return fd;
}

struct smbd_echo_state {
	struct tevent_context *ev;
	struct iovec *pending;
	struct smbd_server_connection *sconn;
	int parent_pipe;

	struct tevent_fd *parent_fde;

	struct tevent_fd *read_fde;
	struct tevent_req *write_req;
};

static void smbd_echo_writer_done(struct tevent_req *req);

static void smbd_echo_activate_writer(struct smbd_echo_state *state)
{
	int num_pending;

	if (state->write_req != NULL) {
		return;
	}

	num_pending = talloc_array_length(state->pending);
	if (num_pending == 0) {
		return;
	}

	state->write_req = writev_send(state, state->ev, NULL,
				       state->parent_pipe, false,
				       state->pending, num_pending);
	if (state->write_req == NULL) {
		DEBUG(1, ("writev_send failed\n"));
		exit(1);
	}

	talloc_steal(state->write_req, state->pending);
	state->pending = NULL;

	tevent_req_set_callback(state->write_req, smbd_echo_writer_done,
				state);
}

static void smbd_echo_writer_done(struct tevent_req *req)
{
	struct smbd_echo_state *state = tevent_req_callback_data(
		req, struct smbd_echo_state);
	ssize_t written;
	int err;

	written = writev_recv(req, &err);
	TALLOC_FREE(req);
	state->write_req = NULL;
	if (written == -1) {
		DEBUG(1, ("writev to parent failed: %s\n", strerror(err)));
		exit(1);
	}
	DEBUG(10,("echo_handler[%d]: forwarded pdu to main\n", (int)sys_getpid()));
	smbd_echo_activate_writer(state);
}

static bool smbd_echo_reply(uint8_t *inbuf, size_t inbuf_len,
			    uint32_t seqnum)
{
	struct smb_request req;
	uint16_t num_replies;
	size_t out_len;
	char *outbuf;
	bool ok;

	if ((inbuf_len == 4) && (CVAL(inbuf, 0) == SMBkeepalive)) {
		DEBUG(10, ("Got netbios keepalive\n"));
		/*
		 * Just swallow it
		 */
		return true;
	}

	if (inbuf_len < smb_size) {
		DEBUG(10, ("Got short packet: %d bytes\n", (int)inbuf_len));
		return false;
	}
	if (!valid_smb_header(inbuf)) {
		DEBUG(10, ("Got invalid SMB header\n"));
		return false;
	}

	if (!init_smb_request(&req, smbd_server_conn, inbuf, 0, false,
			      seqnum)) {
		return false;
	}
	req.inbuf = inbuf;

	DEBUG(10, ("smbecho handler got cmd %d (%s)\n", (int)req.cmd,
		   smb_messages[req.cmd].name
		   ? smb_messages[req.cmd].name : "unknown"));

	if (req.cmd != SMBecho) {
		return false;
	}
	if (req.wct < 1) {
		return false;
	}

	num_replies = SVAL(req.vwv+0, 0);
	if (num_replies != 1) {
		/* Not a Windows "Hey, you're still there?" request */
		return false;
	}

	if (!create_outbuf(talloc_tos(), &req, (char *)req.inbuf, &outbuf,
			   1, req.buflen)) {
		DEBUG(10, ("create_outbuf failed\n"));
		return false;
	}
	req.outbuf = (uint8_t *)outbuf;

	SSVAL(req.outbuf, smb_vwv0, num_replies);

	if (req.buflen > 0) {
		memcpy(smb_buf(req.outbuf), req.buf, req.buflen);
	}

	out_len = smb_len(req.outbuf) + 4;

	ok = srv_send_smb(req.sconn,
			  (char *)outbuf,
			  true, seqnum+1,
			  false, &req.pcd);
	TALLOC_FREE(outbuf);
	if (!ok) {
		exit(1);
	}

	return true;
}

static void smbd_echo_exit(struct tevent_context *ev,
			   struct tevent_fd *fde, uint16_t flags,
			   void *private_data)
{
	DEBUG(2, ("smbd_echo_exit: lost connection to parent\n"));
	exit(0);
}

static void smbd_echo_reader(struct tevent_context *ev,
			     struct tevent_fd *fde, uint16_t flags,
			     void *private_data)
{
	struct smbd_echo_state *state = talloc_get_type_abort(
		private_data, struct smbd_echo_state);
	struct smbd_server_connection *sconn = state->sconn;
	size_t unread, num_pending;
	NTSTATUS status;
	struct iovec *tmp;
	size_t iov_len;
	uint32_t seqnum = 0;
	bool reply;
	bool ok;
	bool encrypted = false;

	smb_msleep(1000);

	ok = smbd_lock_socket_internal(sconn);
	if (!ok) {
		DEBUG(0, ("%s: failed to lock socket\n",
			__location__));
		exit(1);
	}

	if (!fd_is_readable(sconn->sock)) {
		DEBUG(10,("echo_handler[%d] the parent smbd was faster\n",
			  (int)sys_getpid()));
		ok = smbd_unlock_socket_internal(sconn);
		if (!ok) {
			DEBUG(1, ("%s: failed to unlock socket in\n",
				__location__));
			exit(1);
		}
		return;
	}

	num_pending = talloc_array_length(state->pending);
	tmp = talloc_realloc(state, state->pending, struct iovec,
			     num_pending+1);
	if (tmp == NULL) {
		DEBUG(1, ("talloc_realloc failed\n"));
		exit(1);
	}
	state->pending = tmp;

	DEBUG(10,("echo_handler[%d]: reading pdu\n", (int)sys_getpid()));

	status = receive_smb_talloc(state->pending, sconn, sconn->sock,
				    (char **)(void *)&state->pending[num_pending].iov_base,
				    0 /* timeout */,
				    &unread,
				    &encrypted,
				    &iov_len,
				    &seqnum,
				    false /* trusted_channel*/);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("echo_handler[%d]: receive_smb_raw_talloc failed: %s\n",
			  (int)sys_getpid(), nt_errstr(status)));
		exit(1);
	}
	state->pending[num_pending].iov_len = iov_len;

	ok = smbd_unlock_socket_internal(sconn);
	if (!ok) {
		DEBUG(1, ("%s: failed to unlock socket in\n",
			__location__));
		exit(1);
	}

	reply = smbd_echo_reply((uint8_t *)state->pending[num_pending].iov_base,
				state->pending[num_pending].iov_len,
				seqnum);
	if (reply) {
		DEBUG(10,("echo_handler[%d]: replied to client\n", (int)sys_getpid()));
		/* no check, shrinking by some bytes does not fail */
		state->pending = talloc_realloc(state, state->pending,
						struct iovec,
						num_pending);
		return;
	}

	if (state->pending[num_pending].iov_len >= smb_size) {
		/*
		 * place the seqnum in the packet so that the main process
		 * can reply with signing
		 */
		SIVAL((uint8_t *)state->pending[num_pending].iov_base,
		      smb_ss_field, seqnum);
		SIVAL((uint8_t *)state->pending[num_pending].iov_base,
		      smb_ss_field+4, NT_STATUS_V(NT_STATUS_OK));
	}

	DEBUG(10,("echo_handler[%d]: forward to main\n", (int)sys_getpid()));
	smbd_echo_activate_writer(state);
}

static void smbd_echo_loop(struct smbd_server_connection *sconn,
			   int parent_pipe)
{
	struct smbd_echo_state *state;

	state = talloc_zero(sconn, struct smbd_echo_state);
	if (state == NULL) {
		DEBUG(1, ("talloc failed\n"));
		return;
	}
	state->sconn = sconn;
	state->parent_pipe = parent_pipe;
	state->ev = s3_tevent_context_init(state);
	if (state->ev == NULL) {
		DEBUG(1, ("tevent_context_init failed\n"));
		TALLOC_FREE(state);
		return;
	}
	state->parent_fde = tevent_add_fd(state->ev, state, parent_pipe,
					TEVENT_FD_READ, smbd_echo_exit,
					state);
	if (state->parent_fde == NULL) {
		DEBUG(1, ("tevent_add_fd failed\n"));
		TALLOC_FREE(state);
		return;
	}
	state->read_fde = tevent_add_fd(state->ev, state, sconn->sock,
					TEVENT_FD_READ, smbd_echo_reader,
					state);
	if (state->read_fde == NULL) {
		DEBUG(1, ("tevent_add_fd failed\n"));
		TALLOC_FREE(state);
		return;
	}

	while (true) {
		if (tevent_loop_once(state->ev) == -1) {
			DEBUG(1, ("tevent_loop_once failed: %s\n",
				  strerror(errno)));
			break;
		}
	}
	TALLOC_FREE(state);
}

/*
 * Handle SMBecho requests in a forked child process
 */
bool fork_echo_handler(struct smbd_server_connection *sconn)
{
	int listener_pipe[2];
	int res;
	pid_t child;

	res = pipe(listener_pipe);
	if (res == -1) {
		DEBUG(1, ("pipe() failed: %s\n", strerror(errno)));
		return false;
	}
	sconn->smb1.echo_handler.socket_lock_fd = create_unlink_tmp(lp_lockdir());
	if (sconn->smb1.echo_handler.socket_lock_fd == -1) {
		DEBUG(1, ("Could not create lock fd: %s\n", strerror(errno)));
		goto fail;
	}

	child = sys_fork();
	if (child == 0) {
		NTSTATUS status;

		close(listener_pipe[0]);
		set_blocking(listener_pipe[1], false);

		status = reinit_after_fork(sconn->msg_ctx,
					   smbd_event_context(),
					   procid_self(), false);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("reinit_after_fork failed: %s\n",
				  nt_errstr(status)));
			exit(1);
		}
		smbd_echo_loop(sconn, listener_pipe[1]);
		exit(0);
	}
	close(listener_pipe[1]);
	listener_pipe[1] = -1;
	sconn->smb1.echo_handler.trusted_fd = listener_pipe[0];

	DEBUG(10,("fork_echo_handler: main[%d] echo_child[%d]\n", (int)sys_getpid(), child));

	/*
	 * Without smb signing this is the same as the normal smbd
	 * listener. This needs to change once signing comes in.
	 */
	sconn->smb1.echo_handler.trusted_fde = event_add_fd(smbd_event_context(),
					sconn,
					sconn->smb1.echo_handler.trusted_fd,
					EVENT_FD_READ,
					smbd_server_echo_handler,
					sconn);
	if (sconn->smb1.echo_handler.trusted_fde == NULL) {
		DEBUG(1, ("event_add_fd failed\n"));
		goto fail;
	}

	return true;

fail:
	if (listener_pipe[0] != -1) {
		close(listener_pipe[0]);
	}
	if (listener_pipe[1] != -1) {
		close(listener_pipe[1]);
	}
	sconn->smb1.echo_handler.trusted_fd = -1;
	if (sconn->smb1.echo_handler.socket_lock_fd != -1) {
		close(sconn->smb1.echo_handler.socket_lock_fd);
	}
	sconn->smb1.echo_handler.trusted_fd = -1;
	sconn->smb1.echo_handler.socket_lock_fd = -1;
	return false;
}

#if CLUSTER_SUPPORT

static NTSTATUS smbd_register_ips(struct smbd_server_connection *sconn,
				  struct sockaddr_storage *srv,
				  struct sockaddr_storage *clnt)
{
	struct ctdbd_connection *cconn;
	char tmp_addr[INET6_ADDRSTRLEN];
	char *addr;

	cconn = messaging_ctdbd_connection();
	if (cconn == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	client_socket_addr(sconn->sock, tmp_addr, sizeof(tmp_addr));
	addr = talloc_strdup(cconn, tmp_addr);
	if (addr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	return ctdbd_register_ips(cconn, srv, clnt, release_ip, addr);
}

#endif

/****************************************************************************
 Process commands from the client
****************************************************************************/

void smbd_process(struct smbd_server_connection *sconn)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct sockaddr_storage ss;
	struct sockaddr *sa = NULL;
	socklen_t sa_socklen;
	struct tsocket_address *local_address = NULL;
	struct tsocket_address *remote_address = NULL;
	const char *remaddr = NULL;
	int ret;

	if (lp_maxprotocol() == PROTOCOL_SMB2) {
		/*
		 * We're not making the decision here,
		 * we're just allowing the client
		 * to decide between SMB1 and SMB2
		 * with the first negprot
		 * packet.
		 */
		sconn->using_smb2 = true;
	}

	/* Ensure child is set to blocking mode */
	set_blocking(sconn->sock,True);

	set_socket_options(sconn->sock, "SO_KEEPALIVE");
	set_socket_options(sconn->sock, lp_socket_options());

	sa = (struct sockaddr *)(void *)&ss;
	sa_socklen = sizeof(ss);
	ret = getpeername(sconn->sock, sa, &sa_socklen);
	if (ret != 0) {
		int level = (errno == ENOTCONN)?2:0;
		DEBUG(level,("getpeername() failed - %s\n", strerror(errno)));
		exit_server_cleanly("getpeername() failed.\n");
	}
	ret = tsocket_address_bsd_from_sockaddr(sconn,
						sa, sa_socklen,
						&remote_address);
	if (ret != 0) {
		DEBUG(0,("%s: tsocket_address_bsd_from_sockaddr remote failed - %s\n",
			__location__, strerror(errno)));
		exit_server_cleanly("tsocket_address_bsd_from_sockaddr remote failed.\n");
	}

	sa = (struct sockaddr *)(void *)&ss;
	sa_socklen = sizeof(ss);
	ret = getsockname(sconn->sock, sa, &sa_socklen);
	if (ret != 0) {
		int level = (errno == ENOTCONN)?2:0;
		DEBUG(level,("getsockname() failed - %s\n", strerror(errno)));
		exit_server_cleanly("getsockname() failed.\n");
	}
	ret = tsocket_address_bsd_from_sockaddr(sconn,
						sa, sa_socklen,
						&local_address);
	if (ret != 0) {
		DEBUG(0,("%s: tsocket_address_bsd_from_sockaddr remote failed - %s\n",
			__location__, strerror(errno)));
		exit_server_cleanly("tsocket_address_bsd_from_sockaddr remote failed.\n");
	}

	sconn->local_address = local_address;
	sconn->remote_address = remote_address;

	if (tsocket_address_is_inet(remote_address, "ip")) {
		remaddr = tsocket_address_inet_addr_string(
				sconn->remote_address,
				talloc_tos());
		if (remaddr == NULL) {

		}
	} else {
		remaddr = "0.0.0.0";
	}

	/* this is needed so that we get decent entries
	   in smbstatus for port 445 connects */
	set_remote_machine_name(remaddr, false);
	reload_services(sconn->msg_ctx, sconn->sock, true);

	/*
	 * Before the first packet, check the global hosts allow/ hosts deny
	 * parameters before doing any parsing of packets passed to us by the
	 * client. This prevents attacks on our parsing code from hosts not in
	 * the hosts allow list.
	 */

	if (!allow_access(lp_hostsdeny(-1), lp_hostsallow(-1),
			  sconn->client_id.name,
			  sconn->client_id.addr)) {
		/*
		 * send a negative session response "not listening on calling
		 * name"
		 */
		unsigned char buf[5] = {0x83, 0, 0, 1, 0x81};
		DEBUG( 1, ("Connection denied from %s to %s\n",
			   tsocket_address_string(remote_address, talloc_tos()),
			   tsocket_address_string(local_address, talloc_tos())));
		(void)srv_send_smb(sconn,(char *)buf, false,
				   0, false, NULL);
		exit_server_cleanly("connection denied");
	}

	DEBUG(10, ("Connection allowed from %s to %s\n",
		   tsocket_address_string(remote_address, talloc_tos()),
		   tsocket_address_string(local_address, talloc_tos())));

	init_modules();

	smb_perfcount_init();

	if (!init_account_policy()) {
		exit_server("Could not open account policy tdb.\n");
	}

	if (*lp_rootdir()) {
		if (chroot(lp_rootdir()) != 0) {
			DEBUG(0,("Failed to change root to %s\n", lp_rootdir()));
			exit_server("Failed to chroot()");
		}
		if (chdir("/") == -1) {
			DEBUG(0,("Failed to chdir to / on chroot to %s\n", lp_rootdir()));
			exit_server("Failed to chroot()");
		}
		DEBUG(0,("Changed root to %s\n", lp_rootdir()));
	}

	if (!srv_init_signing(sconn)) {
		exit_server("Failed to init smb_signing");
	}

	/* Setup oplocks */
	if (!init_oplocks(sconn->msg_ctx))
		exit_server("Failed to init oplocks");

	/* register our message handlers */
	messaging_register(sconn->msg_ctx, NULL,
			   MSG_SMB_FORCE_TDIS, msg_force_tdis);
	messaging_register(sconn->msg_ctx, sconn,
			   MSG_SMB_RELEASE_IP, msg_release_ip);
	messaging_register(sconn->msg_ctx, NULL,
			   MSG_SMB_CLOSE_FILE, msg_close_file);

	/*
	 * Use the default MSG_DEBUG handler to avoid rebroadcasting
	 * MSGs to all child processes
	 */
	messaging_deregister(sconn->msg_ctx,
			     MSG_DEBUG, NULL);
	messaging_register(sconn->msg_ctx, NULL,
			   MSG_DEBUG, debug_message);

	if ((lp_keepalive() != 0)
	    && !(event_add_idle(smbd_event_context(), NULL,
				timeval_set(lp_keepalive(), 0),
				"keepalive", keepalive_fn,
				NULL))) {
		DEBUG(0, ("Could not add keepalive event\n"));
		exit(1);
	}

	if (!(event_add_idle(smbd_event_context(), NULL,
			     timeval_set(IDLE_CLOSED_TIMEOUT, 0),
			     "deadtime", deadtime_fn, sconn))) {
		DEBUG(0, ("Could not add deadtime event\n"));
		exit(1);
	}

	if (!(event_add_idle(smbd_event_context(), NULL,
			     timeval_set(SMBD_HOUSEKEEPING_INTERVAL, 0),
			     "housekeeping", housekeeping_fn, sconn))) {
		DEBUG(0, ("Could not add housekeeping event\n"));
		exit(1);
	}

#ifdef CLUSTER_SUPPORT

	if (lp_clustering()) {
		/*
		 * We need to tell ctdb about our client's TCP
		 * connection, so that for failover ctdbd can send
		 * tickle acks, triggering a reconnection by the
		 * client.
		 */

		struct sockaddr_storage srv, clnt;

		if (client_get_tcp_info(sconn->sock, &srv, &clnt) == 0) {
			NTSTATUS status;
			status = smbd_register_ips(sconn, &srv, &clnt);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("ctdbd_register_ips failed: %s\n",
					  nt_errstr(status)));
			}
		} else
		{
			DEBUG(0,("Unable to get tcp info for "
				 "CTDB_CONTROL_TCP_CLIENT: %s\n",
				 strerror(errno)));
		}
	}

#endif

	sconn->nbt.got_session = false;

	sconn->smb1.negprot.max_recv = MIN(lp_maxxmit(),BUFFER_SIZE);

	sconn->smb1.sessions.done_sesssetup = false;
	sconn->smb1.sessions.max_send = BUFFER_SIZE;
	sconn->smb1.sessions.last_session_tag = UID_FIELD_INVALID;
	/* users from session setup */
	sconn->smb1.sessions.session_userlist = NULL;
	/* workgroup from session setup. */
	sconn->smb1.sessions.session_workgroup = NULL;
	/* this holds info on user ids that are already validated for this VC */
	sconn->smb1.sessions.validated_users = NULL;
	sconn->smb1.sessions.next_vuid = VUID_OFFSET;
	sconn->smb1.sessions.num_validated_vuids = 0;

	conn_init(sconn);
	if (!init_dptrs(sconn)) {
		exit_server("init_dptrs() failed");
	}

	sconn->smb1.fde = event_add_fd(smbd_event_context(),
						  sconn,
						  sconn->sock,
						  EVENT_FD_READ,
						  smbd_server_connection_handler,
						  sconn);
	if (!sconn->smb1.fde) {
		exit_server("failed to create smbd_server_connection fde");
	}

	TALLOC_FREE(frame);

	while (True) {
		NTSTATUS status;

		frame = talloc_stackframe_pool(8192);

		errno = 0;

		status = smbd_server_connection_loop_once(sconn);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_RETRY) &&
		    !NT_STATUS_IS_OK(status)) {
			DEBUG(3, ("smbd_server_connection_loop_once failed: %s,"
				  " exiting\n", nt_errstr(status)));
			break;
		}

		TALLOC_FREE(frame);
	}

	exit_server_cleanly(NULL);
}

bool req_is_in_chain(struct smb_request *req)
{
	if (req->vwv != (uint16_t *)(req->inbuf+smb_vwv)) {
		/*
		 * We're right now handling a subsequent request, so we must
		 * be in a chain
		 */
		return true;
	}

	if (!is_andx_req(req->cmd)) {
		return false;
	}

	if (req->wct < 2) {
		/*
		 * Okay, an illegal request, but definitely not chained :-)
		 */
		return false;
	}

	return (CVAL(req->vwv+0, 0) != 0xFF);
}
