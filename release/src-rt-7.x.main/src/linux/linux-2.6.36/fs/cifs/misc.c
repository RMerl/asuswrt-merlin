/*
 *   fs/cifs/misc.c
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2008
 *   Author(s): Steve French (sfrench@us.ibm.com)
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

#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/mempool.h>
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "cifs_debug.h"
#include "smberr.h"
#include "nterr.h"
#include "cifs_unicode.h"

extern mempool_t *cifs_sm_req_poolp;
extern mempool_t *cifs_req_poolp;

/* The xid serves as a useful identifier for each incoming vfs request,
   in a similar way to the mid which is useful to track each sent smb,
   and CurrentXid can also provide a running counter (although it
   will eventually wrap past zero) of the total vfs operations handled
   since the cifs fs was mounted */

unsigned int
_GetXid(void)
{
	unsigned int xid;

	spin_lock(&GlobalMid_Lock);
	GlobalTotalActiveXid++;

	/* keep high water mark for number of simultaneous ops in filesystem */
	if (GlobalTotalActiveXid > GlobalMaxActiveXid)
		GlobalMaxActiveXid = GlobalTotalActiveXid;
	if (GlobalTotalActiveXid > 65000)
		cFYI(1, "warning: more than 65000 requests active");
	xid = GlobalCurrentXid++;
	spin_unlock(&GlobalMid_Lock);
	return xid;
}

void
_FreeXid(unsigned int xid)
{
	spin_lock(&GlobalMid_Lock);
	/* if (GlobalTotalActiveXid == 0)
		BUG(); */
	GlobalTotalActiveXid--;
	spin_unlock(&GlobalMid_Lock);
}

struct cifsSesInfo *
sesInfoAlloc(void)
{
	struct cifsSesInfo *ret_buf;

	ret_buf = kzalloc(sizeof(struct cifsSesInfo), GFP_KERNEL);
	if (ret_buf) {
		atomic_inc(&sesInfoAllocCount);
		ret_buf->status = CifsNew;
		++ret_buf->ses_count;
		INIT_LIST_HEAD(&ret_buf->smb_ses_list);
		INIT_LIST_HEAD(&ret_buf->tcon_list);
		mutex_init(&ret_buf->session_mutex);
	}
	return ret_buf;
}

void
sesInfoFree(struct cifsSesInfo *buf_to_free)
{
	if (buf_to_free == NULL) {
		cFYI(1, "Null buffer passed to sesInfoFree");
		return;
	}

	atomic_dec(&sesInfoAllocCount);
	kfree(buf_to_free->serverOS);
	kfree(buf_to_free->serverDomain);
	kfree(buf_to_free->serverNOS);
	if (buf_to_free->password) {
		memset(buf_to_free->password, 0, strlen(buf_to_free->password));
		kfree(buf_to_free->password);
	}
	kfree(buf_to_free->domainName);
	kfree(buf_to_free);
}

struct cifsTconInfo *
tconInfoAlloc(void)
{
	struct cifsTconInfo *ret_buf;
	ret_buf = kzalloc(sizeof(struct cifsTconInfo), GFP_KERNEL);
	if (ret_buf) {
		atomic_inc(&tconInfoAllocCount);
		ret_buf->tidStatus = CifsNew;
		++ret_buf->tc_count;
		INIT_LIST_HEAD(&ret_buf->openFileList);
		INIT_LIST_HEAD(&ret_buf->tcon_list);
#ifdef CONFIG_CIFS_STATS
		spin_lock_init(&ret_buf->stat_lock);
#endif
	}
	return ret_buf;
}

void
tconInfoFree(struct cifsTconInfo *buf_to_free)
{
	if (buf_to_free == NULL) {
		cFYI(1, "Null buffer passed to tconInfoFree");
		return;
	}
	atomic_dec(&tconInfoAllocCount);
	kfree(buf_to_free->nativeFileSystem);
	if (buf_to_free->password) {
		memset(buf_to_free->password, 0, strlen(buf_to_free->password));
		kfree(buf_to_free->password);
	}
	kfree(buf_to_free);
}

struct smb_hdr *
cifs_buf_get(void)
{
	struct smb_hdr *ret_buf = NULL;

/* We could use negotiated size instead of max_msgsize -
   but it may be more efficient to always alloc same size
   albeit slightly larger than necessary and maxbuffersize
   defaults to this and can not be bigger */
	ret_buf = mempool_alloc(cifs_req_poolp, GFP_NOFS);

	/* clear the first few header bytes */
	/* for most paths, more is cleared in header_assemble */
	if (ret_buf) {
		memset(ret_buf, 0, sizeof(struct smb_hdr) + 3);
		atomic_inc(&bufAllocCount);
#ifdef CONFIG_CIFS_STATS2
		atomic_inc(&totBufAllocCount);
#endif /* CONFIG_CIFS_STATS2 */
	}

	return ret_buf;
}

void
cifs_buf_release(void *buf_to_free)
{
	if (buf_to_free == NULL) {
		/* cFYI(1, "Null buffer passed to cifs_buf_release");*/
		return;
	}
	mempool_free(buf_to_free, cifs_req_poolp);

	atomic_dec(&bufAllocCount);
	return;
}

struct smb_hdr *
cifs_small_buf_get(void)
{
	struct smb_hdr *ret_buf = NULL;

/* We could use negotiated size instead of max_msgsize -
   but it may be more efficient to always alloc same size
   albeit slightly larger than necessary and maxbuffersize
   defaults to this and can not be bigger */
	ret_buf = mempool_alloc(cifs_sm_req_poolp, GFP_NOFS);
	if (ret_buf) {
	/* No need to clear memory here, cleared in header assemble */
	/*	memset(ret_buf, 0, sizeof(struct smb_hdr) + 27);*/
		atomic_inc(&smBufAllocCount);
#ifdef CONFIG_CIFS_STATS2
		atomic_inc(&totSmBufAllocCount);
#endif /* CONFIG_CIFS_STATS2 */

	}
	return ret_buf;
}

void
cifs_small_buf_release(void *buf_to_free)
{

	if (buf_to_free == NULL) {
		cFYI(1, "Null buffer passed to cifs_small_buf_release");
		return;
	}
	mempool_free(buf_to_free, cifs_sm_req_poolp);

	atomic_dec(&smBufAllocCount);
	return;
}

/*
	Find a free multiplex id (SMB mid). Otherwise there could be
	mid collisions which might cause problems, demultiplexing the
	wrong response to this request. Multiplex ids could collide if
	one of a series requests takes much longer than the others, or
	if a very large number of long lived requests (byte range
	locks or FindNotify requests) are pending.  No more than
	64K-1 requests can be outstanding at one time.  If no
	mids are available, return zero.  A future optimization
	could make the combination of mids and uid the key we use
	to demultiplex on (rather than mid alone).
	In addition to the above check, the cifs demultiplex
	code already used the command code as a secondary
	check of the frame and if signing is negotiated the
	response would be discarded if the mid were the same
	but the signature was wrong.  Since the mid is not put in the
	pending queue until later (when it is about to be dispatched)
	we do have to limit the number of outstanding requests
	to somewhat less than 64K-1 although it is hard to imagine
	so many threads being in the vfs at one time.
*/
__u16 GetNextMid(struct TCP_Server_Info *server)
{
	__u16 mid = 0;
	__u16 last_mid;
	int   collision;

	if (server == NULL)
		return mid;

	spin_lock(&GlobalMid_Lock);
	last_mid = server->CurrentMid; /* we do not want to loop forever */
	server->CurrentMid++;
	/* This nested loop looks more expensive than it is.
	In practice the list of pending requests is short,
	fewer than 50, and the mids are likely to be unique
	on the first pass through the loop unless some request
	takes longer than the 64 thousand requests before it
	(and it would also have to have been a request that
	 did not time out) */
	while (server->CurrentMid != last_mid) {
		struct list_head *tmp;
		struct mid_q_entry *mid_entry;

		collision = 0;
		if (server->CurrentMid == 0)
			server->CurrentMid++;

		list_for_each(tmp, &server->pending_mid_q) {
			mid_entry = list_entry(tmp, struct mid_q_entry, qhead);

			if ((mid_entry->mid == server->CurrentMid) &&
			    (mid_entry->midState == MID_REQUEST_SUBMITTED)) {
				/* This mid is in use, try a different one */
				collision = 1;
				break;
			}
		}
		if (collision == 0) {
			mid = server->CurrentMid;
			break;
		}
		server->CurrentMid++;
	}
	spin_unlock(&GlobalMid_Lock);
	return mid;
}

/* NB: MID can not be set if treeCon not passed in, in that
   case it is responsbility of caller to set the mid */
void
header_assemble(struct smb_hdr *buffer, char smb_command /* command */ ,
		const struct cifsTconInfo *treeCon, int word_count
		/* length of fixed section (word count) in two byte units  */)
{
	struct list_head *temp_item;
	struct cifsSesInfo *ses;
	char *temp = (char *) buffer;

	memset(temp, 0, 256); /* bigger than MAX_CIFS_HDR_SIZE */

	buffer->smb_buf_length =
	    (2 * word_count) + sizeof(struct smb_hdr) -
	    4 /*  RFC 1001 length field does not count */  +
	    2 /* for bcc field itself */ ;
	/* Note that this is the only network field that has to be converted
	   to big endian and it is done just before we send it */

	buffer->Protocol[0] = 0xFF;
	buffer->Protocol[1] = 'S';
	buffer->Protocol[2] = 'M';
	buffer->Protocol[3] = 'B';
	buffer->Command = smb_command;
	buffer->Flags = 0x00;	/* case sensitive */
	buffer->Flags2 = SMBFLG2_KNOWS_LONG_NAMES;
	buffer->Pid = cpu_to_le16((__u16)current->tgid);
	buffer->PidHigh = cpu_to_le16((__u16)(current->tgid >> 16));
	if (treeCon) {
		buffer->Tid = treeCon->tid;
		if (treeCon->ses) {
			if (treeCon->ses->capabilities & CAP_UNICODE)
				buffer->Flags2 |= SMBFLG2_UNICODE;
			if (treeCon->ses->capabilities & CAP_STATUS32)
				buffer->Flags2 |= SMBFLG2_ERR_STATUS;

			/* Uid is not converted */
			buffer->Uid = treeCon->ses->Suid;
			buffer->Mid = GetNextMid(treeCon->ses->server);
			if (multiuser_mount != 0) {
		/* For the multiuser case, there are few obvious technically  */
		/* possible mechanisms to match the local linux user (uid)    */
		/* to a valid remote smb user (smb_uid):		      */
		/* 	1) Query Winbind (or other local pam/nss daemon       */
		/* 	  for userid/password/logon_domain or credential      */
		/*      2) Query Winbind for uid to sid to username mapping   */
		/* 	   and see if we have a matching password for existing*/
		/*         session for that user perhas getting password by   */
		/*         adding a new pam_cifs module that stores passwords */
		/*         so that the cifs vfs can get at that for all logged*/
		/*	   on users					      */
		/*	3) (Which is the mechanism we have chosen)	      */
		/*	   Search through sessions to the same server for a   */
		/*	   a match on the uid that was passed in on mount     */
		/*         with the current processes uid (or euid?) and use  */
		/* 	   that smb uid.   If no existing smb session for     */
		/* 	   that uid found, use the default smb session ie     */
		/*         the smb session for the volume mounted which is    */
		/* 	   the same as would be used if the multiuser mount   */
		/* 	   flag were disabled.  */

		/*  BB Add support for establishing new tCon and SMB Session  */
		/*      with userid/password pairs found on the smb session   */
		/*	for other target tcp/ip addresses 		BB    */
				if (current_fsuid() != treeCon->ses->linux_uid) {
					cFYI(1, "Multiuser mode and UID "
						 "did not match tcon uid");
					read_lock(&cifs_tcp_ses_lock);
					list_for_each(temp_item, &treeCon->ses->server->smb_ses_list) {
						ses = list_entry(temp_item, struct cifsSesInfo, smb_ses_list);
						if (ses->linux_uid == current_fsuid()) {
							if (ses->server == treeCon->ses->server) {
								cFYI(1, "found matching uid substitute right smb_uid");
								buffer->Uid = ses->Suid;
								break;
							} else {
				/* BB eventually call cifs_setup_session here */
								cFYI(1, "local UID found but no smb sess with this server exists");
							}
						}
					}
					read_unlock(&cifs_tcp_ses_lock);
				}
			}
		}
		if (treeCon->Flags & SMB_SHARE_IS_IN_DFS)
			buffer->Flags2 |= SMBFLG2_DFS;
		if (treeCon->nocase)
			buffer->Flags  |= SMBFLG_CASELESS;
		if ((treeCon->ses) && (treeCon->ses->server))
			if (treeCon->ses->server->secMode &
			  (SECMODE_SIGN_REQUIRED | SECMODE_SIGN_ENABLED))
				buffer->Flags2 |= SMBFLG2_SECURITY_SIGNATURE;
	}

/*  endian conversion of flags is now done just before sending */
	buffer->WordCount = (char) word_count;
	return;
}

static int
checkSMBhdr(struct smb_hdr *smb, __u16 mid)
{
	/* Make sure that this really is an SMB, that it is a response,
	   and that the message ids match */
	if ((*(__le32 *) smb->Protocol == cpu_to_le32(0x424d53ff)) &&
		(mid == smb->Mid)) {
		if (smb->Flags & SMBFLG_RESPONSE)
			return 0;
		else {
		/* only one valid case where server sends us request */
			if (smb->Command == SMB_COM_LOCKING_ANDX)
				return 0;
			else
				cERROR(1, "Received Request not response");
		}
	} else { /* bad signature or mid */
		if (*(__le32 *) smb->Protocol != cpu_to_le32(0x424d53ff))
			cERROR(1, "Bad protocol string signature header %x",
				*(unsigned int *) smb->Protocol);
		if (mid != smb->Mid)
			cERROR(1, "Mids do not match");
	}
	cERROR(1, "bad smb detected. The Mid=%d", smb->Mid);
	return 1;
}

int
checkSMB(struct smb_hdr *smb, __u16 mid, unsigned int length)
{
	__u32 len = smb->smb_buf_length;
	__u32 clc_len;  /* calculated length */
	cFYI(0, "checkSMB Length: 0x%x, smb_buf_length: 0x%x", length, len);

	if (length < 2 + sizeof(struct smb_hdr)) {
		if ((length >= sizeof(struct smb_hdr) - 1)
			    && (smb->Status.CifsError != 0)) {
			smb->WordCount = 0;
			/* some error cases do not return wct and bcc */
			return 0;
		} else if ((length == sizeof(struct smb_hdr) + 1) &&
				(smb->WordCount == 0)) {
			char *tmp = (char *)smb;
			/* First, check if the part of bcc they sent was zero */
			if (tmp[sizeof(struct smb_hdr)] == 0) {
				/* some servers return only half of bcc
				 * on simple responses (wct, bcc both zero)
				 * in particular have seen this on
				 * ulogoffX and FindClose. This leaves
				 * one byte of bcc potentially unitialized
				 */
				/* zero rest of bcc */
				tmp[sizeof(struct smb_hdr)+1] = 0;
				return 0;
			}
			cERROR(1, "rcvd invalid byte count (bcc)");
		} else {
			cERROR(1, "Length less than smb header size");
		}
		return 1;
	}
	if (len > CIFSMaxBufSize + MAX_CIFS_HDR_SIZE - 4) {
		cERROR(1, "smb length greater than MaxBufSize, mid=%d",
				   smb->Mid);
		return 1;
	}

	if (checkSMBhdr(smb, mid))
		return 1;
	clc_len = smbCalcSize_LE(smb);

	if (4 + len != length) {
		cERROR(1, "Length read does not match RFC1001 length %d",
			   len);
		return 1;
	}

	if (4 + len != clc_len) {
		/* check if bcc wrapped around for large read responses */
		if ((len > 64 * 1024) && (len > clc_len)) {
			/* check if lengths match mod 64K */
			if (((4 + len) & 0xFFFF) == (clc_len & 0xFFFF))
				return 0; /* bcc wrapped */
		}
		cFYI(1, "Calculated size %d vs length %d mismatch for mid %d",
				clc_len, 4 + len, smb->Mid);
		/* Windows XP can return a few bytes too much, presumably
		an illegal pad, at the end of byte range lock responses
		so we allow for that three byte pad, as long as actual
		received length is as long or longer than calculated length */
		/* We have now had to extend this more, since there is a
		case in which it needs to be bigger still to handle a
		malformed response to transact2 findfirst from WinXP when
		access denied is returned and thus bcc and wct are zero
		but server says length is 0x21 bytes too long as if the server
		forget to reset the smb rfc1001 length when it reset the
		wct and bcc to minimum size and drop the t2 parms and data */
		if ((4+len > clc_len) && (len <= clc_len + 512))
			return 0;
		else {
			cERROR(1, "RFC1001 size %d bigger than SMB for Mid=%d",
					len, smb->Mid);
			return 1;
		}
	}
	return 0;
}

bool
is_valid_oplock_break(struct smb_hdr *buf, struct TCP_Server_Info *srv)
{
	struct smb_com_lock_req *pSMB = (struct smb_com_lock_req *)buf;
	struct list_head *tmp, *tmp1, *tmp2;
	struct cifsSesInfo *ses;
	struct cifsTconInfo *tcon;
	struct cifsInodeInfo *pCifsInode;
	struct cifsFileInfo *netfile;

	cFYI(1, "Checking for oplock break or dnotify response");
	if ((pSMB->hdr.Command == SMB_COM_NT_TRANSACT) &&
	   (pSMB->hdr.Flags & SMBFLG_RESPONSE)) {
		struct smb_com_transaction_change_notify_rsp *pSMBr =
			(struct smb_com_transaction_change_notify_rsp *)buf;
		struct file_notify_information *pnotify;
		__u32 data_offset = 0;
		if (pSMBr->ByteCount > sizeof(struct file_notify_information)) {
			data_offset = le32_to_cpu(pSMBr->DataOffset);

			pnotify = (struct file_notify_information *)
				((char *)&pSMBr->hdr.Protocol + data_offset);
			cFYI(1, "dnotify on %s Action: 0x%x",
				 pnotify->FileName, pnotify->Action);
			/*   cifs_dump_mem("Rcvd notify Data: ",buf,
				sizeof(struct smb_hdr)+60); */
			return true;
		}
		if (pSMBr->hdr.Status.CifsError) {
			cFYI(1, "notify err 0x%d",
				pSMBr->hdr.Status.CifsError);
			return true;
		}
		return false;
	}
	if (pSMB->hdr.Command != SMB_COM_LOCKING_ANDX)
		return false;
	if (pSMB->hdr.Flags & SMBFLG_RESPONSE) {
		/* no sense logging error on invalid handle on oplock
		   break - harmless race between close request and oplock
		   break response is expected from time to time writing out
		   large dirty files cached on the client */
		if ((NT_STATUS_INVALID_HANDLE) ==
		   le32_to_cpu(pSMB->hdr.Status.CifsError)) {
			cFYI(1, "invalid handle on oplock break");
			return true;
		} else if (ERRbadfid ==
		   le16_to_cpu(pSMB->hdr.Status.DosError.Error)) {
			return true;
		} else {
			return false; /* on valid oplock brk we get "request" */
		}
	}
	if (pSMB->hdr.WordCount != 8)
		return false;

	cFYI(1, "oplock type 0x%d level 0x%d",
		 pSMB->LockType, pSMB->OplockLevel);
	if (!(pSMB->LockType & LOCKING_ANDX_OPLOCK_RELEASE))
		return false;

	/* look up tcon based on tid & uid */
	read_lock(&cifs_tcp_ses_lock);
	list_for_each(tmp, &srv->smb_ses_list) {
		ses = list_entry(tmp, struct cifsSesInfo, smb_ses_list);
		list_for_each(tmp1, &ses->tcon_list) {
			tcon = list_entry(tmp1, struct cifsTconInfo, tcon_list);
			if (tcon->tid != buf->Tid)
				continue;

			cifs_stats_inc(&tcon->num_oplock_brks);
			read_lock(&GlobalSMBSeslock);
			list_for_each(tmp2, &tcon->openFileList) {
				netfile = list_entry(tmp2, struct cifsFileInfo,
						     tlist);
				if (pSMB->Fid != netfile->netfid)
					continue;

				/*
				 * don't do anything if file is about to be
				 * closed anyway.
				 */
				if (netfile->closePend) {
					read_unlock(&GlobalSMBSeslock);
					read_unlock(&cifs_tcp_ses_lock);
					return true;
				}

				cFYI(1, "file id match, oplock break");
				pCifsInode = CIFS_I(netfile->pInode);
				pCifsInode->clientCanCacheAll = false;
				if (pSMB->OplockLevel == 0)
					pCifsInode->clientCanCacheRead = false;

				/*
				 * cifs_oplock_break_put() can't be called
				 * from here.  Get reference after queueing
				 * succeeded.  cifs_oplock_break() will
				 * synchronize using GlobalSMSSeslock.
				 */
				if (queue_work(system_nrt_wq,
					       &netfile->oplock_break))
					cifs_oplock_break_get(netfile);
				netfile->oplock_break_cancelled = false;

				read_unlock(&GlobalSMBSeslock);
				read_unlock(&cifs_tcp_ses_lock);
				return true;
			}
			read_unlock(&GlobalSMBSeslock);
			read_unlock(&cifs_tcp_ses_lock);
			cFYI(1, "No matching file for oplock break");
			return true;
		}
	}
	read_unlock(&cifs_tcp_ses_lock);
	cFYI(1, "Can not process oplock break for non-existent connection");
	return true;
}

void
dump_smb(struct smb_hdr *smb_buf, int smb_buf_length)
{
	int i, j;
	char debug_line[17];
	unsigned char *buffer;

	if (traceSMB == 0)
		return;

	buffer = (unsigned char *) smb_buf;
	for (i = 0, j = 0; i < smb_buf_length; i++, j++) {
		if (i % 8 == 0) {
			/* have reached the beginning of line */
			printk(KERN_DEBUG "| ");
			j = 0;
		}
		printk("%0#4x ", buffer[i]);
		debug_line[2 * j] = ' ';
		if (isprint(buffer[i]))
			debug_line[1 + (2 * j)] = buffer[i];
		else
			debug_line[1 + (2 * j)] = '_';

		if (i % 8 == 7) {
			/* reached end of line, time to print ascii */
			debug_line[16] = 0;
			printk(" | %s\n", debug_line);
		}
	}
	for (; j < 8; j++) {
		printk("     ");
		debug_line[2 * j] = ' ';
		debug_line[1 + (2 * j)] = ' ';
	}
	printk(" | %s\n", debug_line);
	return;
}

/* Convert 16 bit Unicode pathname to wire format from string in current code
   page.  Conversion may involve remapping up the seven characters that are
   only legal in POSIX-like OS (if they are present in the string). Path
   names are little endian 16 bit Unicode on the wire */
int
cifsConvertToUCS(__le16 *target, const char *source, int maxlen,
		 const struct nls_table *cp, int mapChars)
{
	int i, j, charlen;
	int len_remaining = maxlen;
	char src_char;
	__u16 temp;

	if (!mapChars)
		return cifs_strtoUCS(target, source, PATH_MAX, cp);

	for (i = 0, j = 0; i < maxlen; j++) {
		src_char = source[i];
		switch (src_char) {
			case 0:
				target[j] = 0;
				goto ctoUCS_out;
			case ':':
				target[j] = cpu_to_le16(UNI_COLON);
				break;
			case '*':
				target[j] = cpu_to_le16(UNI_ASTERIK);
				break;
			case '?':
				target[j] = cpu_to_le16(UNI_QUESTION);
				break;
			case '<':
				target[j] = cpu_to_le16(UNI_LESSTHAN);
				break;
			case '>':
				target[j] = cpu_to_le16(UNI_GRTRTHAN);
				break;
			case '|':
				target[j] = cpu_to_le16(UNI_PIPE);
				break;
			/* BB We can not handle remapping slash until
			   all the calls to build_path_from_dentry
			   are modified, as they use slash as separator BB */
			/* case '\\':
				target[j] = cpu_to_le16(UNI_SLASH);
				break;*/
			default:
				charlen = cp->char2uni(source+i,
					len_remaining, &temp);
				/* if no match, use question mark, which
				at least in some cases servers as wild card */
				if (charlen < 1) {
					target[j] = cpu_to_le16(0x003f);
					charlen = 1;
				} else
					target[j] = cpu_to_le16(temp);
				len_remaining -= charlen;
				/* character may take more than one byte in the
				   the source string, but will take exactly two
				   bytes in the target string */
				i += charlen;
				continue;
		}
		i++; /* move to next char in source string */
		len_remaining--;
	}

ctoUCS_out:
	return i;
}

void
cifs_autodisable_serverino(struct cifs_sb_info *cifs_sb)
{
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_SERVER_INUM) {
		cifs_sb->mnt_cifs_flags &= ~CIFS_MOUNT_SERVER_INUM;
		cERROR(1, "Autodisabling the use of server inode numbers on "
			   "%s. This server doesn't seem to support them "
			   "properly. Hardlinks will not be recognized on this "
			   "mount. Consider mounting with the \"noserverino\" "
			   "option to silence this message.",
			   cifs_sb->tcon->treeName);
	}
}
