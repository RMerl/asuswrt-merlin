/*
 *   fs/cifs/transport.c
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2008
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *   Jeremy Allison (jra@samba.org) 2006.
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/wait.h>
#include <linux/net.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <linux/mempool.h>
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "cifs_debug.h"

extern mempool_t *cifs_mid_poolp;

static void
wake_up_task(struct mid_q_entry *mid)
{
	wake_up_process(mid->callback_data);
}

struct mid_q_entry *
AllocMidQEntry(const struct smb_hdr *smb_buffer, struct TCP_Server_Info *server)
{
	struct mid_q_entry *temp;

	if (server == NULL) {
		cERROR(1, "Null TCP session in AllocMidQEntry");
		return NULL;
	}

	temp = mempool_alloc(cifs_mid_poolp, GFP_NOFS);
	if (temp == NULL)
		return temp;
	else {
		memset(temp, 0, sizeof(struct mid_q_entry));
		temp->mid = smb_buffer->Mid;	/* always LE */
		temp->pid = current->pid;
		temp->command = smb_buffer->Command;
		cFYI(1, "For smb_command %d", temp->command);
	/*	do_gettimeofday(&temp->when_sent);*/ /* easier to use jiffies */
		/* when mid allocated can be before when sent */
		temp->when_alloc = jiffies;

		/*
		 * The default is for the mid to be synchronous, so the
		 * default callback just wakes up the current task.
		 */
		temp->callback = wake_up_task;
		temp->callback_data = current;
	}

	atomic_inc(&midCount);
	temp->midState = MID_REQUEST_ALLOCATED;
	return temp;
}

void
DeleteMidQEntry(struct mid_q_entry *midEntry)
{
#ifdef CONFIG_CIFS_STATS2
	unsigned long now;
#endif
	midEntry->midState = MID_FREE;
	atomic_dec(&midCount);
	if (midEntry->largeBuf)
		cifs_buf_release(midEntry->resp_buf);
	else
		cifs_small_buf_release(midEntry->resp_buf);
#ifdef CONFIG_CIFS_STATS2
	now = jiffies;
	/* commands taking longer than one second are indications that
	   something is wrong, unless it is quite a slow link or server */
	if ((now - midEntry->when_alloc) > HZ) {
		if ((cifsFYI & CIFS_TIMER) &&
		   (midEntry->command != SMB_COM_LOCKING_ANDX)) {
			printk(KERN_DEBUG " CIFS slow rsp: cmd %d mid %d",
			       midEntry->command, midEntry->mid);
			printk(" A: 0x%lx S: 0x%lx R: 0x%lx\n",
			       now - midEntry->when_alloc,
			       now - midEntry->when_sent,
			       now - midEntry->when_received);
		}
	}
#endif
	mempool_free(midEntry, cifs_mid_poolp);
}

static void
delete_mid(struct mid_q_entry *mid)
{
	spin_lock(&GlobalMid_Lock);
	list_del(&mid->qhead);
	spin_unlock(&GlobalMid_Lock);

	DeleteMidQEntry(mid);
}

static int
smb_sendv(struct TCP_Server_Info *server, struct kvec *iov, int n_vec)
{
	int rc = 0;
	int i = 0;
	struct msghdr smb_msg;
	struct smb_hdr *smb_buffer = iov[0].iov_base;
	unsigned int len = iov[0].iov_len;
	unsigned int total_len;
	int first_vec = 0;
	unsigned int smb_buf_length = smb_buffer->smb_buf_length;
	struct socket *ssocket = server->ssocket;

	if (ssocket == NULL)
		return -ENOTSOCK; /* BB eventually add reconnect code here */

	smb_msg.msg_name = (struct sockaddr *) &server->dstaddr;
	smb_msg.msg_namelen = sizeof(struct sockaddr);
	smb_msg.msg_control = NULL;
	smb_msg.msg_controllen = 0;
	if (server->noblocksnd)
		smb_msg.msg_flags = MSG_DONTWAIT + MSG_NOSIGNAL;
	else
		smb_msg.msg_flags = MSG_NOSIGNAL;

	/* smb header is converted in header_assemble. bcc and rest of SMB word
	   area, and byte area if necessary, is converted to littleendian in
	   cifssmb.c and RFC1001 len is converted to bigendian in smb_send
	   Flags2 is converted in SendReceive */


	total_len = 0;
	for (i = 0; i < n_vec; i++)
		total_len += iov[i].iov_len;

	smb_buffer->smb_buf_length = cpu_to_be32(smb_buffer->smb_buf_length);
	cFYI(1, "Sending smb:  total_len %d", total_len);
	dump_smb(smb_buffer, len);

	i = 0;
	while (total_len) {
		rc = kernel_sendmsg(ssocket, &smb_msg, &iov[first_vec],
				    n_vec - first_vec, total_len);
		if ((rc == -ENOSPC) || (rc == -EAGAIN)) {
			i++;
			/* if blocking send we try 3 times, since each can block
			   for 5 seconds. For nonblocking  we have to try more
			   but wait increasing amounts of time allowing time for
			   socket to clear.  The overall time we wait in either
			   case to send on the socket is about 15 seconds.
			   Similarly we wait for 15 seconds for
			   a response from the server in SendReceive[2]
			   for the server to send a response back for
			   most types of requests (except SMB Write
			   past end of file which can be slow, and
			   blocking lock operations). NFS waits slightly longer
			   than CIFS, but this can make it take longer for
			   nonresponsive servers to be detected and 15 seconds
			   is more than enough time for modern networks to
			   send a packet.  In most cases if we fail to send
			   after the retries we will kill the socket and
			   reconnect which may clear the network problem.
			*/
			if ((i >= 14) || (!server->noblocksnd && (i > 2))) {
				cERROR(1, "sends on sock %p stuck for 15 seconds",
				    ssocket);
				rc = -EAGAIN;
				break;
			}
			msleep(1 << i);
			continue;
		}
		if (rc < 0)
			break;

		if (rc == total_len) {
			total_len = 0;
			break;
		} else if (rc > total_len) {
			cERROR(1, "sent %d requested %d", rc, total_len);
			break;
		}
		if (rc == 0) {
			/* should never happen, letting socket clear before
			   retrying is our only obvious option here */
			cERROR(1, "tcp sent no data");
			msleep(500);
			continue;
		}
		total_len -= rc;
		/* the line below resets i */
		for (i = first_vec; i < n_vec; i++) {
			if (iov[i].iov_len) {
				if (rc > iov[i].iov_len) {
					rc -= iov[i].iov_len;
					iov[i].iov_len = 0;
				} else {
					iov[i].iov_base += rc;
					iov[i].iov_len -= rc;
					first_vec = i;
					break;
				}
			}
		}
		i = 0; /* in case we get ENOSPC on the next send */
	}

	if ((total_len > 0) && (total_len != smb_buf_length + 4)) {
		cFYI(1, "partial send (%d remaining), terminating session",
			total_len);
		/* If we have only sent part of an SMB then the next SMB
		   could be taken as the remainder of this one.  We need
		   to kill the socket so the server throws away the partial
		   SMB */
		server->tcpStatus = CifsNeedReconnect;
	}

	if (rc < 0 && rc != -EINTR)
		cERROR(1, "Error %d sending data on socket to server", rc);
	else
		rc = 0;

	/* Don't want to modify the buffer as a
	   side effect of this call. */
	smb_buffer->smb_buf_length = smb_buf_length;

	return rc;
}

int
smb_send(struct TCP_Server_Info *server, struct smb_hdr *smb_buffer,
	 unsigned int smb_buf_length)
{
	struct kvec iov;

	iov.iov_base = smb_buffer;
	iov.iov_len = smb_buf_length + 4;

	return smb_sendv(server, &iov, 1);
}

static int wait_for_free_request(struct TCP_Server_Info *server,
				 const int long_op)
{
	if (long_op == CIFS_ASYNC_OP) {
		/* oplock breaks must not be held up */
		atomic_inc(&server->inFlight);
		return 0;
	}

	spin_lock(&GlobalMid_Lock);
	while (1) {
		if (atomic_read(&server->inFlight) >= cifs_max_pending) {
			spin_unlock(&GlobalMid_Lock);
#ifdef CONFIG_CIFS_STATS2
			atomic_inc(&server->num_waiters);
#endif
			wait_event(server->request_q,
				   atomic_read(&server->inFlight)
				     < cifs_max_pending);
#ifdef CONFIG_CIFS_STATS2
			atomic_dec(&server->num_waiters);
#endif
			spin_lock(&GlobalMid_Lock);
		} else {
			if (server->tcpStatus == CifsExiting) {
				spin_unlock(&GlobalMid_Lock);
				return -ENOENT;
			}

			/* can not count locking commands against total
			   as they are allowed to block on server */

			/* update # of requests on the wire to server */
			if (long_op != CIFS_BLOCKING_OP)
				atomic_inc(&server->inFlight);
			spin_unlock(&GlobalMid_Lock);
			break;
		}
	}
	return 0;
}

static int allocate_mid(struct cifsSesInfo *ses, struct smb_hdr *in_buf,
			struct mid_q_entry **ppmidQ)
{
	if (ses->server->tcpStatus == CifsExiting) {
		return -ENOENT;
	}

	if (ses->server->tcpStatus == CifsNeedReconnect) {
		cFYI(1, "tcp session dead - return to caller to retry");
		return -EAGAIN;
	}

	if (ses->status != CifsGood) {
		/* check if SMB session is bad because we are setting it up */
		if ((in_buf->Command != SMB_COM_SESSION_SETUP_ANDX) &&
			(in_buf->Command != SMB_COM_NEGOTIATE))
			return -EAGAIN;
		/* else ok - we are setting up session */
	}
	*ppmidQ = AllocMidQEntry(in_buf, ses->server);
	if (*ppmidQ == NULL)
		return -ENOMEM;
	spin_lock(&GlobalMid_Lock);
	list_add_tail(&(*ppmidQ)->qhead, &ses->server->pending_mid_q);
	spin_unlock(&GlobalMid_Lock);
	return 0;
}

static int
wait_for_response(struct TCP_Server_Info *server, struct mid_q_entry *midQ)
{
	int error;

	error = wait_event_killable(server->response_q,
				    midQ->midState != MID_REQUEST_SUBMITTED);
	if (error < 0)
		return -ERESTARTSYS;

	return 0;
}


/*
 * Send a SMB request and set the callback function in the mid to handle
 * the result. Caller is responsible for dealing with timeouts.
 */
int
cifs_call_async(struct TCP_Server_Info *server, struct smb_hdr *in_buf,
		mid_callback_t *callback, void *cbdata)
{
	int rc;
	struct mid_q_entry *mid;

	rc = wait_for_free_request(server, CIFS_ASYNC_OP);
	if (rc)
		return rc;

	/* enable signing if server requires it */
	if (server->secMode & (SECMODE_SIGN_REQUIRED | SECMODE_SIGN_ENABLED))
		in_buf->Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	mutex_lock(&server->srv_mutex);
	mid = AllocMidQEntry(in_buf, server);
	if (mid == NULL) {
		mutex_unlock(&server->srv_mutex);
		return -ENOMEM;
	}

	/* put it on the pending_mid_q */
	spin_lock(&GlobalMid_Lock);
	list_add_tail(&mid->qhead, &server->pending_mid_q);
	spin_unlock(&GlobalMid_Lock);

	rc = cifs_sign_smb(in_buf, server, &mid->sequence_number);
	if (rc) {
		mutex_unlock(&server->srv_mutex);
		goto out_err;
	}

	mid->callback = callback;
	mid->callback_data = cbdata;
	mid->midState = MID_REQUEST_SUBMITTED;
#ifdef CONFIG_CIFS_STATS2
	atomic_inc(&server->inSend);
#endif
	rc = smb_send(server, in_buf, in_buf->smb_buf_length);
#ifdef CONFIG_CIFS_STATS2
	atomic_dec(&server->inSend);
	mid->when_sent = jiffies;
#endif
	mutex_unlock(&server->srv_mutex);
	if (rc)
		goto out_err;

	return rc;
out_err:
	delete_mid(mid);
	atomic_dec(&server->inFlight);
	wake_up(&server->request_q);
	return rc;
}

/*
 *
 * Send an SMB Request.  No response info (other than return code)
 * needs to be parsed.
 *
 * flags indicate the type of request buffer and how long to wait
 * and whether to log NT STATUS code (error) before mapping it to POSIX error
 *
 */
int
SendReceiveNoRsp(const unsigned int xid, struct cifsSesInfo *ses,
		struct smb_hdr *in_buf, int flags)
{
	int rc;
	struct kvec iov[1];
	int resp_buf_type;

	iov[0].iov_base = (char *)in_buf;
	iov[0].iov_len = in_buf->smb_buf_length + 4;
	flags |= CIFS_NO_RESP;
	rc = SendReceive2(xid, ses, iov, 1, &resp_buf_type, flags);
	cFYI(DBG2, "SendRcvNoRsp flags %d rc %d", flags, rc);

	return rc;
}

static int
sync_mid_result(struct mid_q_entry *mid, struct TCP_Server_Info *server)
{
	int rc = 0;

	cFYI(1, "%s: cmd=%d mid=%d state=%d", __func__, mid->command,
		mid->mid, mid->midState);

	spin_lock(&GlobalMid_Lock);
	/* ensure that it's no longer on the pending_mid_q */
	list_del_init(&mid->qhead);

	switch (mid->midState) {
	case MID_RESPONSE_RECEIVED:
		spin_unlock(&GlobalMid_Lock);
		return rc;
	case MID_REQUEST_SUBMITTED:
		/* socket is going down, reject all calls */
		if (server->tcpStatus == CifsExiting) {
			cERROR(1, "%s: canceling mid=%d cmd=0x%x state=%d",
			       __func__, mid->mid, mid->command, mid->midState);
			rc = -EHOSTDOWN;
			break;
		}
	case MID_RETRY_NEEDED:
		rc = -EAGAIN;
		break;
	case MID_RESPONSE_MALFORMED:
		rc = -EIO;
		break;
	default:
		cERROR(1, "%s: invalid mid state mid=%d state=%d", __func__,
			mid->mid, mid->midState);
		rc = -EIO;
	}
	spin_unlock(&GlobalMid_Lock);

	DeleteMidQEntry(mid);
	return rc;
}

/*
 * An NT cancel request header looks just like the original request except:
 *
 * The Command is SMB_COM_NT_CANCEL
 * The WordCount is zeroed out
 * The ByteCount is zeroed out
 *
 * This function mangles an existing request buffer into a
 * SMB_COM_NT_CANCEL request and then sends it.
 */
static int
send_nt_cancel(struct TCP_Server_Info *server, struct smb_hdr *in_buf,
		struct mid_q_entry *mid)
{
	int rc = 0;

	/* -4 for RFC1001 length and +2 for BCC field */
	in_buf->smb_buf_length = sizeof(struct smb_hdr) - 4  + 2;
	in_buf->Command = SMB_COM_NT_CANCEL;
	in_buf->WordCount = 0;
	put_bcc_le(0, in_buf);

	mutex_lock(&server->srv_mutex);
	rc = cifs_sign_smb(in_buf, server, &mid->sequence_number);
	if (rc) {
		mutex_unlock(&server->srv_mutex);
		return rc;
	}
	rc = smb_send(server, in_buf, in_buf->smb_buf_length);
	mutex_unlock(&server->srv_mutex);

	cFYI(1, "issued NT_CANCEL for mid %u, rc = %d",
		in_buf->Mid, rc);

	return rc;
}

int
SendReceive2(const unsigned int xid, struct cifsSesInfo *ses,
	     struct kvec *iov, int n_vec, int *pRespBufType /* ret */,
	     const int flags)
{
	int rc = 0;
	int long_op;
	unsigned int receive_len;
	struct mid_q_entry *midQ;
	struct smb_hdr *in_buf = iov[0].iov_base;

	long_op = flags & CIFS_TIMEOUT_MASK;

	*pRespBufType = CIFS_NO_BUFFER;  /* no response buf yet */

	if ((ses == NULL) || (ses->server == NULL)) {
		cifs_small_buf_release(in_buf);
		cERROR(1, "Null session");
		return -EIO;
	}

	if (ses->server->tcpStatus == CifsExiting) {
		cifs_small_buf_release(in_buf);
		return -ENOENT;
	}

	/* Ensure that we do not send more than 50 overlapping requests
	   to the same server. We may make this configurable later or
	   use ses->maxReq */

	rc = wait_for_free_request(ses->server, long_op);
	if (rc) {
		cifs_small_buf_release(in_buf);
		return rc;
	}

	/* make sure that we sign in the same order that we send on this socket
	   and avoid races inside tcp sendmsg code that could cause corruption
	   of smb data */

	mutex_lock(&ses->server->srv_mutex);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		cifs_small_buf_release(in_buf);
		/* Update # of requests on wire to server */
		atomic_dec(&ses->server->inFlight);
		wake_up(&ses->server->request_q);
		return rc;
	}
	rc = cifs_sign_smb2(iov, n_vec, ses->server, &midQ->sequence_number);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		cifs_small_buf_release(in_buf);
		goto out;
	}

	midQ->midState = MID_REQUEST_SUBMITTED;
#ifdef CONFIG_CIFS_STATS2
	atomic_inc(&ses->server->inSend);
#endif
	rc = smb_sendv(ses->server, iov, n_vec);
#ifdef CONFIG_CIFS_STATS2
	atomic_dec(&ses->server->inSend);
	midQ->when_sent = jiffies;
#endif

	mutex_unlock(&ses->server->srv_mutex);

	if (rc < 0) {
		cifs_small_buf_release(in_buf);
		goto out;
	}

	if (long_op == CIFS_ASYNC_OP) {
		cifs_small_buf_release(in_buf);
		goto out;
	}

	rc = wait_for_response(ses->server, midQ);
	if (rc != 0) {
		send_nt_cancel(ses->server, in_buf, midQ);
		spin_lock(&GlobalMid_Lock);
		if (midQ->midState == MID_REQUEST_SUBMITTED) {
			midQ->callback = DeleteMidQEntry;
			spin_unlock(&GlobalMid_Lock);
			cifs_small_buf_release(in_buf);
			atomic_dec(&ses->server->inFlight);
			wake_up(&ses->server->request_q);
			return rc;
		}
		spin_unlock(&GlobalMid_Lock);
	}

	cifs_small_buf_release(in_buf);

	rc = sync_mid_result(midQ, ses->server);
	if (rc != 0) {
		atomic_dec(&ses->server->inFlight);
		wake_up(&ses->server->request_q);
		return rc;
	}

	receive_len = midQ->resp_buf->smb_buf_length;

	if (receive_len > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE) {
		cERROR(1, "Frame too large received.  Length: %d  Xid: %d",
			receive_len, xid);
		rc = -EIO;
		goto out;
	}

	/* rcvd frame is ok */

	if (midQ->resp_buf &&
	    (midQ->midState == MID_RESPONSE_RECEIVED)) {

		iov[0].iov_base = (char *)midQ->resp_buf;
		if (midQ->largeBuf)
			*pRespBufType = CIFS_LARGE_BUFFER;
		else
			*pRespBufType = CIFS_SMALL_BUFFER;
		iov[0].iov_len = receive_len + 4;

		dump_smb(midQ->resp_buf, 80);
		/* convert the length into a more usable form */
		if ((receive_len > 24) &&
		    (ses->server->secMode & (SECMODE_SIGN_REQUIRED |
					     SECMODE_SIGN_ENABLED))) {
			rc = cifs_verify_signature(midQ->resp_buf,
						ses->server,
						midQ->sequence_number+1);
			if (rc) {
				cERROR(1, "Unexpected SMB signature");
				/* BB FIXME add code to kill session */
			}
		}

		/* BB special case reconnect tid and uid here? */
		rc = map_smb_to_linux_error(midQ->resp_buf,
					    flags & CIFS_LOG_ERROR);

		/* convert ByteCount if necessary */
		if (receive_len >= sizeof(struct smb_hdr) - 4
		    /* do not count RFC1001 header */  +
		    (2 * midQ->resp_buf->WordCount) + 2 /* bcc */ )
			put_bcc(get_bcc_le(midQ->resp_buf), midQ->resp_buf);
		if ((flags & CIFS_NO_RESP) == 0)
			midQ->resp_buf = NULL;  /* mark it so buf will
						   not be freed by
						   delete_mid */
	} else {
		rc = -EIO;
		cFYI(1, "Bad MID state?");
	}

out:
	delete_mid(midQ);
	atomic_dec(&ses->server->inFlight);
	wake_up(&ses->server->request_q);

	return rc;
}

int
SendReceive(const unsigned int xid, struct cifsSesInfo *ses,
	    struct smb_hdr *in_buf, struct smb_hdr *out_buf,
	    int *pbytes_returned, const int long_op)
{
	int rc = 0;
	unsigned int receive_len;
	struct mid_q_entry *midQ;

	if (ses == NULL) {
		cERROR(1, "Null smb session");
		return -EIO;
	}
	if (ses->server == NULL) {
		cERROR(1, "Null tcp session");
		return -EIO;
	}

	if (ses->server->tcpStatus == CifsExiting)
		return -ENOENT;

	/* Ensure that we do not send more than 50 overlapping requests
	   to the same server. We may make this configurable later or
	   use ses->maxReq */

	if (in_buf->smb_buf_length > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE - 4) {
		cERROR(1, "Illegal length, greater than maximum frame, %d",
			   in_buf->smb_buf_length);
		return -EIO;
	}

	rc = wait_for_free_request(ses->server, long_op);
	if (rc)
		return rc;

	/* make sure that we sign in the same order that we send on this socket
	   and avoid races inside tcp sendmsg code that could cause corruption
	   of smb data */

	mutex_lock(&ses->server->srv_mutex);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		/* Update # of requests on wire to server */
		atomic_dec(&ses->server->inFlight);
		wake_up(&ses->server->request_q);
		return rc;
	}

	rc = cifs_sign_smb(in_buf, ses->server, &midQ->sequence_number);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		goto out;
	}

	midQ->midState = MID_REQUEST_SUBMITTED;
#ifdef CONFIG_CIFS_STATS2
	atomic_inc(&ses->server->inSend);
#endif
	rc = smb_send(ses->server, in_buf, in_buf->smb_buf_length);
#ifdef CONFIG_CIFS_STATS2
	atomic_dec(&ses->server->inSend);
	midQ->when_sent = jiffies;
#endif
	mutex_unlock(&ses->server->srv_mutex);

	if (rc < 0)
		goto out;

	if (long_op == CIFS_ASYNC_OP)
		goto out;

	rc = wait_for_response(ses->server, midQ);
	if (rc != 0) {
		send_nt_cancel(ses->server, in_buf, midQ);
		spin_lock(&GlobalMid_Lock);
		if (midQ->midState == MID_REQUEST_SUBMITTED) {
			/* no longer considered to be "in-flight" */
			midQ->callback = DeleteMidQEntry;
			spin_unlock(&GlobalMid_Lock);
			atomic_dec(&ses->server->inFlight);
			wake_up(&ses->server->request_q);
			return rc;
		}
		spin_unlock(&GlobalMid_Lock);
	}

	rc = sync_mid_result(midQ, ses->server);
	if (rc != 0) {
		atomic_dec(&ses->server->inFlight);
		wake_up(&ses->server->request_q);
		return rc;
	}

	receive_len = midQ->resp_buf->smb_buf_length;

	if (receive_len > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE) {
		cERROR(1, "Frame too large received.  Length: %d  Xid: %d",
			receive_len, xid);
		rc = -EIO;
		goto out;
	}

	/* rcvd frame is ok */

	if (midQ->resp_buf && out_buf
	    && (midQ->midState == MID_RESPONSE_RECEIVED)) {
		out_buf->smb_buf_length = receive_len;
		memcpy((char *)out_buf + 4,
		       (char *)midQ->resp_buf + 4,
		       receive_len);

		dump_smb(out_buf, 92);
		/* convert the length into a more usable form */
		if ((receive_len > 24) &&
		    (ses->server->secMode & (SECMODE_SIGN_REQUIRED |
					     SECMODE_SIGN_ENABLED))) {
			rc = cifs_verify_signature(out_buf,
						ses->server,
						midQ->sequence_number+1);
			if (rc) {
				cERROR(1, "Unexpected SMB signature");
				/* BB FIXME add code to kill session */
			}
		}

		*pbytes_returned = out_buf->smb_buf_length;

		/* BB special case reconnect tid and uid here? */
		rc = map_smb_to_linux_error(out_buf, 0 /* no log */ );

		/* convert ByteCount if necessary */
		if (receive_len >= sizeof(struct smb_hdr) - 4
		    /* do not count RFC1001 header */  +
		    (2 * out_buf->WordCount) + 2 /* bcc */ )
			put_bcc(get_bcc_le(midQ->resp_buf), midQ->resp_buf);
	} else {
		rc = -EIO;
		cERROR(1, "Bad MID state?");
	}

out:
	delete_mid(midQ);
	atomic_dec(&ses->server->inFlight);
	wake_up(&ses->server->request_q);

	return rc;
}

/* We send a LOCKINGX_CANCEL_LOCK to cause the Windows
   blocking lock to return. */

static int
send_lock_cancel(const unsigned int xid, struct cifsTconInfo *tcon,
			struct smb_hdr *in_buf,
			struct smb_hdr *out_buf)
{
	int bytes_returned;
	struct cifsSesInfo *ses = tcon->ses;
	LOCK_REQ *pSMB = (LOCK_REQ *)in_buf;

	/* We just modify the current in_buf to change
	   the type of lock from LOCKING_ANDX_SHARED_LOCK
	   or LOCKING_ANDX_EXCLUSIVE_LOCK to
	   LOCKING_ANDX_CANCEL_LOCK. */

	pSMB->LockType = LOCKING_ANDX_CANCEL_LOCK|LOCKING_ANDX_LARGE_FILES;
	pSMB->Timeout = 0;
	pSMB->hdr.Mid = GetNextMid(ses->server);

	return SendReceive(xid, ses, in_buf, out_buf,
			&bytes_returned, 0);
}

int
SendReceiveBlockingLock(const unsigned int xid, struct cifsTconInfo *tcon,
	    struct smb_hdr *in_buf, struct smb_hdr *out_buf,
	    int *pbytes_returned)
{
	int rc = 0;
	int rstart = 0;
	unsigned int receive_len;
	struct mid_q_entry *midQ;
	struct cifsSesInfo *ses;

	if (tcon == NULL || tcon->ses == NULL) {
		cERROR(1, "Null smb session");
		return -EIO;
	}
	ses = tcon->ses;

	if (ses->server == NULL) {
		cERROR(1, "Null tcp session");
		return -EIO;
	}

	if (ses->server->tcpStatus == CifsExiting)
		return -ENOENT;

	/* Ensure that we do not send more than 50 overlapping requests
	   to the same server. We may make this configurable later or
	   use ses->maxReq */

	if (in_buf->smb_buf_length > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE - 4) {
		cERROR(1, "Illegal length, greater than maximum frame, %d",
			   in_buf->smb_buf_length);
		return -EIO;
	}

	rc = wait_for_free_request(ses->server, CIFS_BLOCKING_OP);
	if (rc)
		return rc;

	/* make sure that we sign in the same order that we send on this socket
	   and avoid races inside tcp sendmsg code that could cause corruption
	   of smb data */

	mutex_lock(&ses->server->srv_mutex);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		return rc;
	}

	rc = cifs_sign_smb(in_buf, ses->server, &midQ->sequence_number);
	if (rc) {
		delete_mid(midQ);
		mutex_unlock(&ses->server->srv_mutex);
		return rc;
	}

	midQ->midState = MID_REQUEST_SUBMITTED;
#ifdef CONFIG_CIFS_STATS2
	atomic_inc(&ses->server->inSend);
#endif
	rc = smb_send(ses->server, in_buf, in_buf->smb_buf_length);
#ifdef CONFIG_CIFS_STATS2
	atomic_dec(&ses->server->inSend);
	midQ->when_sent = jiffies;
#endif
	mutex_unlock(&ses->server->srv_mutex);

	if (rc < 0) {
		delete_mid(midQ);
		return rc;
	}

	/* Wait for a reply - allow signals to interrupt. */
	rc = wait_event_interruptible(ses->server->response_q,
		(!(midQ->midState == MID_REQUEST_SUBMITTED)) ||
		((ses->server->tcpStatus != CifsGood) &&
		 (ses->server->tcpStatus != CifsNew)));

	/* Were we interrupted by a signal ? */
	if ((rc == -ERESTARTSYS) &&
		(midQ->midState == MID_REQUEST_SUBMITTED) &&
		((ses->server->tcpStatus == CifsGood) ||
		 (ses->server->tcpStatus == CifsNew))) {

		if (in_buf->Command == SMB_COM_TRANSACTION2) {
			/* POSIX lock. We send a NT_CANCEL SMB to cause the
			   blocking lock to return. */
			rc = send_nt_cancel(ses->server, in_buf, midQ);
			if (rc) {
				delete_mid(midQ);
				return rc;
			}
		} else {
			/* Windows lock. We send a LOCKINGX_CANCEL_LOCK
			   to cause the blocking lock to return. */

			rc = send_lock_cancel(xid, tcon, in_buf, out_buf);

			/* If we get -ENOLCK back the lock may have
			   already been removed. Don't exit in this case. */
			if (rc && rc != -ENOLCK) {
				delete_mid(midQ);
				return rc;
			}
		}

		rc = wait_for_response(ses->server, midQ);
		if (rc) {
			send_nt_cancel(ses->server, in_buf, midQ);
			spin_lock(&GlobalMid_Lock);
			if (midQ->midState == MID_REQUEST_SUBMITTED) {
				/* no longer considered to be "in-flight" */
				midQ->callback = DeleteMidQEntry;
				spin_unlock(&GlobalMid_Lock);
				return rc;
			}
			spin_unlock(&GlobalMid_Lock);
		}

		/* We got the response - restart system call. */
		rstart = 1;
	}

	rc = sync_mid_result(midQ, ses->server);
	if (rc != 0)
		return rc;

	receive_len = midQ->resp_buf->smb_buf_length;
	if (receive_len > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE) {
		cERROR(1, "Frame too large received.  Length: %d  Xid: %d",
			receive_len, xid);
		rc = -EIO;
		goto out;
	}

	/* rcvd frame is ok */

	if ((out_buf == NULL) || (midQ->midState != MID_RESPONSE_RECEIVED)) {
		rc = -EIO;
		cERROR(1, "Bad MID state?");
		goto out;
	}

	out_buf->smb_buf_length = receive_len;
	memcpy((char *)out_buf + 4,
	       (char *)midQ->resp_buf + 4,
	       receive_len);

	dump_smb(out_buf, 92);
	/* convert the length into a more usable form */
	if ((receive_len > 24) &&
	    (ses->server->secMode & (SECMODE_SIGN_REQUIRED |
				     SECMODE_SIGN_ENABLED))) {
		rc = cifs_verify_signature(out_buf,
					   ses->server,
					   midQ->sequence_number+1);
		if (rc) {
			cERROR(1, "Unexpected SMB signature");
			/* BB FIXME add code to kill session */
		}
	}

	*pbytes_returned = out_buf->smb_buf_length;

	/* BB special case reconnect tid and uid here? */
	rc = map_smb_to_linux_error(out_buf, 0 /* no log */ );

	/* convert ByteCount if necessary */
	if (receive_len >= sizeof(struct smb_hdr) - 4
	    /* do not count RFC1001 header */  +
	    (2 * out_buf->WordCount) + 2 /* bcc */ )
		put_bcc(get_bcc_le(out_buf), out_buf);

out:
	delete_mid(midQ);
	if (rstart && rc == -EACCES)
		return -ERESTARTSYS;
	return rc;
}
