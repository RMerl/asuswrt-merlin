/* 
   Unix SMB/CIFS implementation.
   process incoming packets - main loop
   Copyright (C) Andrew Tridgell 1992-2005
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan Metzmacher 2004-2005
   
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
#include "system/time.h"
#include "smbd/service_stream.h"
#include "smb_server/smb_server.h"
#include "system/filesys.h"
#include "param/param.h"


/*
  send an oplock break request to a client
*/
NTSTATUS smbsrv_send_oplock_break(void *p, struct ntvfs_handle *ntvfs, uint8_t level)
{
	struct smbsrv_tcon *tcon = talloc_get_type(p, struct smbsrv_tcon);
	struct smbsrv_request *req;

	req = smbsrv_init_request(tcon->smb_conn);
	NT_STATUS_HAVE_NO_MEMORY(req);

	smbsrv_setup_reply(req, 8, 0);

	SCVAL(req->out.hdr,HDR_COM,SMBlockingX);
	SSVAL(req->out.hdr,HDR_TID,tcon->tid);
	SSVAL(req->out.hdr,HDR_PID,0xFFFF);
	SSVAL(req->out.hdr,HDR_UID,0);
	SSVAL(req->out.hdr,HDR_MID,0xFFFF);
	SCVAL(req->out.hdr,HDR_FLG,0);
	SSVAL(req->out.hdr,HDR_FLG2,0);

	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	smbsrv_push_fnum(req->out.vwv, VWV(2), ntvfs);
	SCVAL(req->out.vwv, VWV(3), LOCKING_ANDX_OPLOCK_RELEASE);
	SCVAL(req->out.vwv, VWV(3)+1, level);
	SIVAL(req->out.vwv, VWV(4), 0);
	SSVAL(req->out.vwv, VWV(6), 0);
	SSVAL(req->out.vwv, VWV(7), 0);

	smbsrv_send_reply(req);
	return NT_STATUS_OK;
}

static void switch_message(int type, struct smbsrv_request *req);

/*
  These flags determine some of the permissions required to do an operation 
*/
#define NEED_SESS		(1<<0)
#define NEED_TCON		(1<<1)
#define SIGNING_NO_REPLY	(1<<2)
/* does VWV(0) of the request hold chaining information */
#define AND_X			(1<<3)
/* The 64Kb question: are requests > 64K valid? */
#define LARGE_REQUEST	(1<<4)

/* 
   define a list of possible SMB messages and their corresponding
   functions. Any message that has a NULL function is unimplemented -
   please feel free to contribute implementations!
*/
static const struct smb_message_struct
{
	const char *name;
	void (*fn)(struct smbsrv_request *);
#define message_flags(type) smb_messages[(type) & 0xff].flags
	int flags;
}
 smb_messages[256] = {
/* 0x00 */ { "SMBmkdir",	smbsrv_reply_mkdir,		NEED_SESS|NEED_TCON },
/* 0x01 */ { "SMBrmdir",	smbsrv_reply_rmdir,		NEED_SESS|NEED_TCON },
/* 0x02 */ { "SMBopen",		smbsrv_reply_open,		NEED_SESS|NEED_TCON },
/* 0x03 */ { "SMBcreate",	smbsrv_reply_mknew,		NEED_SESS|NEED_TCON },
/* 0x04 */ { "SMBclose",	smbsrv_reply_close,		NEED_SESS|NEED_TCON },
/* 0x05 */ { "SMBflush",	smbsrv_reply_flush,		NEED_SESS|NEED_TCON },
/* 0x06 */ { "SMBunlink",	smbsrv_reply_unlink,		NEED_SESS|NEED_TCON },
/* 0x07 */ { "SMBmv",		smbsrv_reply_mv,		NEED_SESS|NEED_TCON },
/* 0x08 */ { "SMBgetatr",	smbsrv_reply_getatr,		NEED_SESS|NEED_TCON },
/* 0x09 */ { "SMBsetatr",	smbsrv_reply_setatr,		NEED_SESS|NEED_TCON },
/* 0x0a */ { "SMBread",		smbsrv_reply_read,		NEED_SESS|NEED_TCON },
/* 0x0b */ { "SMBwrite",	smbsrv_reply_write,		NEED_SESS|NEED_TCON },
/* 0x0c */ { "SMBlock",		smbsrv_reply_lock,		NEED_SESS|NEED_TCON },
/* 0x0d */ { "SMBunlock",	smbsrv_reply_unlock,		NEED_SESS|NEED_TCON },
/* 0x0e */ { "SMBctemp",	smbsrv_reply_ctemp,		NEED_SESS|NEED_TCON },
/* 0x0f */ { "SMBmknew",	smbsrv_reply_mknew,		NEED_SESS|NEED_TCON }, 
/* 0x10 */ { "SMBchkpth",	smbsrv_reply_chkpth,		NEED_SESS|NEED_TCON },
/* 0x11 */ { "SMBexit",		smbsrv_reply_exit,		NEED_SESS },
/* 0x12 */ { "SMBlseek",	smbsrv_reply_lseek,		NEED_SESS|NEED_TCON },
/* 0x13 */ { "SMBlockread",	smbsrv_reply_lockread,		NEED_SESS|NEED_TCON },
/* 0x14 */ { "SMBwriteunlock",	smbsrv_reply_writeunlock,	NEED_SESS|NEED_TCON },
/* 0x15 */ { NULL, NULL, 0 },
/* 0x16 */ { NULL, NULL, 0 },
/* 0x17 */ { NULL, NULL, 0 },
/* 0x18 */ { NULL, NULL, 0 },
/* 0x19 */ { NULL, NULL, 0 },
/* 0x1a */ { "SMBreadbraw",	smbsrv_reply_readbraw,		NEED_SESS|NEED_TCON },
/* 0x1b */ { "SMBreadBmpx",	smbsrv_reply_readbmpx,		NEED_SESS|NEED_TCON },
/* 0x1c */ { "SMBreadBs",	NULL,				0 },
/* 0x1d */ { "SMBwritebraw",	smbsrv_reply_writebraw,		NEED_SESS|NEED_TCON },
/* 0x1e */ { "SMBwriteBmpx",	smbsrv_reply_writebmpx,		NEED_SESS|NEED_TCON },
/* 0x1f */ { "SMBwriteBs",	smbsrv_reply_writebs,		NEED_SESS|NEED_TCON },
/* 0x20 */ { "SMBwritec",	NULL,				0 },
/* 0x21 */ { NULL, NULL, 0 },
/* 0x22 */ { "SMBsetattrE",	smbsrv_reply_setattrE,		NEED_SESS|NEED_TCON },
/* 0x23 */ { "SMBgetattrE",	smbsrv_reply_getattrE,		NEED_SESS|NEED_TCON },
/* 0x24 */ { "SMBlockingX",	smbsrv_reply_lockingX,		NEED_SESS|NEED_TCON|AND_X },
/* 0x25 */ { "SMBtrans",	smbsrv_reply_trans,		NEED_SESS|NEED_TCON },
/* 0x26 */ { "SMBtranss",	smbsrv_reply_transs,		NEED_SESS|NEED_TCON },
/* 0x27 */ { "SMBioctl",	smbsrv_reply_ioctl,		NEED_SESS|NEED_TCON },
/* 0x28 */ { "SMBioctls",	NULL,				NEED_SESS|NEED_TCON },
/* 0x29 */ { "SMBcopy",		smbsrv_reply_copy,		NEED_SESS|NEED_TCON },
/* 0x2a */ { "SMBmove",		NULL,				NEED_SESS|NEED_TCON },
/* 0x2b */ { "SMBecho",		smbsrv_reply_echo,		0 },
/* 0x2c */ { "SMBwriteclose",	smbsrv_reply_writeclose,	NEED_SESS|NEED_TCON },
/* 0x2d */ { "SMBopenX",	smbsrv_reply_open_and_X,	NEED_SESS|NEED_TCON|AND_X },
/* 0x2e */ { "SMBreadX",	smbsrv_reply_read_and_X,	NEED_SESS|NEED_TCON|AND_X },
/* 0x2f */ { "SMBwriteX",	smbsrv_reply_write_and_X,	NEED_SESS|NEED_TCON|AND_X|LARGE_REQUEST},
/* 0x30 */ { NULL, NULL, 0 },
/* 0x31 */ { NULL, NULL, 0 },
/* 0x32 */ { "SMBtrans2",	smbsrv_reply_trans2,		NEED_SESS|NEED_TCON },
/* 0x33 */ { "SMBtranss2",	smbsrv_reply_transs2,		NEED_SESS|NEED_TCON },
/* 0x34 */ { "SMBfindclose",	smbsrv_reply_findclose,		NEED_SESS|NEED_TCON },
/* 0x35 */ { "SMBfindnclose",	smbsrv_reply_findnclose,	NEED_SESS|NEED_TCON },
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
/* 0x70 */ { "SMBtcon",		smbsrv_reply_tcon,		NEED_SESS },
/* 0x71 */ { "SMBtdis",		smbsrv_reply_tdis,		NEED_TCON },
/* 0x72 */ { "SMBnegprot",	smbsrv_reply_negprot,		0 },
/* 0x73 */ { "SMBsesssetupX",	smbsrv_reply_sesssetup,		AND_X },
/* 0x74 */ { "SMBulogoffX",	smbsrv_reply_ulogoffX,		NEED_SESS|AND_X }, /* ulogoff doesn't give a valid TID */
/* 0x75 */ { "SMBtconX",	smbsrv_reply_tcon_and_X,	NEED_SESS|AND_X },
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
/* 0x80 */ { "SMBdskattr",	smbsrv_reply_dskattr,		NEED_SESS|NEED_TCON },
/* 0x81 */ { "SMBsearch",	smbsrv_reply_search,		NEED_SESS|NEED_TCON },
/* 0x82 */ { "SMBffirst",	smbsrv_reply_search,		NEED_SESS|NEED_TCON },
/* 0x83 */ { "SMBfunique",	smbsrv_reply_search,		NEED_SESS|NEED_TCON },
/* 0x84 */ { "SMBfclose",	smbsrv_reply_fclose,		NEED_SESS|NEED_TCON },
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
/* 0xa0 */ { "SMBnttrans",	smbsrv_reply_nttrans,		NEED_SESS|NEED_TCON|LARGE_REQUEST },
/* 0xa1 */ { "SMBnttranss",	smbsrv_reply_nttranss,		NEED_SESS|NEED_TCON },
/* 0xa2 */ { "SMBntcreateX",	smbsrv_reply_ntcreate_and_X,	NEED_SESS|NEED_TCON|AND_X },
/* 0xa3 */ { NULL, NULL, 0 },
/* 0xa4 */ { "SMBntcancel",	smbsrv_reply_ntcancel,		NEED_SESS|NEED_TCON|SIGNING_NO_REPLY },
/* 0xa5 */ { "SMBntrename",	smbsrv_reply_ntrename,		NEED_SESS|NEED_TCON },
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
/* 0xc0 */ { "SMBsplopen",	smbsrv_reply_printopen,		NEED_SESS|NEED_TCON },
/* 0xc1 */ { "SMBsplwr",	smbsrv_reply_printwrite,	NEED_SESS|NEED_TCON },
/* 0xc2 */ { "SMBsplclose",	smbsrv_reply_printclose,	NEED_SESS|NEED_TCON },
/* 0xc3 */ { "SMBsplretq",	smbsrv_reply_printqueue,	NEED_SESS|NEED_TCON },
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
/* 0xd0 */ { "SMBsends",	NULL,				0 },
/* 0xd1 */ { "SMBsendb",	NULL,				0 },
/* 0xd2 */ { "SMBfwdname",	NULL,				0 },
/* 0xd3 */ { "SMBcancelf",	NULL,				0 },
/* 0xd4 */ { "SMBgetmac",	NULL,				0 },
/* 0xd5 */ { "SMBsendstrt",	NULL,				0 },
/* 0xd6 */ { "SMBsendend",	NULL,				0 },
/* 0xd7 */ { "SMBsendtxt",	NULL,				0 },
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

/****************************************************************************
receive a SMB request header from the wire, forming a request_context
from the result
****************************************************************************/
NTSTATUS smbsrv_recv_smb_request(void *private_data, DATA_BLOB blob)
{
	struct smbsrv_connection *smb_conn = talloc_get_type(private_data, struct smbsrv_connection);
	struct smbsrv_request *req;
	struct timeval cur_time = timeval_current();
	uint8_t command;

	smb_conn->statistics.last_request_time = cur_time;

	/* see if its a special NBT packet */
	if (CVAL(blob.data, 0) != 0) {
		req = smbsrv_init_request(smb_conn);
		NT_STATUS_HAVE_NO_MEMORY(req);

		ZERO_STRUCT(req->in);

		req->in.buffer = talloc_steal(req, blob.data);
		req->in.size = blob.length;
		req->request_time = cur_time;

		smbsrv_reply_special(req);
		return NT_STATUS_OK;
	}

	if ((NBT_HDR_SIZE + MIN_SMB_SIZE) > blob.length) {
		DEBUG(2,("Invalid SMB packet: length %ld\n", (long)blob.length));
		smbsrv_terminate_connection(smb_conn, "Invalid SMB packet");
		return NT_STATUS_OK;
	}

	/* Make sure this is an SMB packet */
	if (IVAL(blob.data, NBT_HDR_SIZE) != SMB_MAGIC) {
		DEBUG(2,("Non-SMB packet of length %ld. Terminating connection\n",
			 (long)blob.length));
		smbsrv_terminate_connection(smb_conn, "Non-SMB packet");
		return NT_STATUS_OK;
	}

	req = smbsrv_init_request(smb_conn);
	NT_STATUS_HAVE_NO_MEMORY(req);

	req->in.buffer = talloc_steal(req, blob.data);
	req->in.size = blob.length;
	req->request_time = cur_time;
	req->chained_fnum = -1;
	req->in.allocated = req->in.size;
	req->in.hdr = req->in.buffer + NBT_HDR_SIZE;
	req->in.vwv = req->in.hdr + HDR_VWV;
	req->in.wct = CVAL(req->in.hdr, HDR_WCT);

	command = CVAL(req->in.hdr, HDR_COM);

	if (req->in.vwv + VWV(req->in.wct) <= req->in.buffer + req->in.size) {
		req->in.data = req->in.vwv + VWV(req->in.wct) + 2;
		req->in.data_size = SVAL(req->in.vwv, VWV(req->in.wct));

		/* special handling for oversize calls. Windows seems
		   to take the maximum of the BCC value and the
		   computed buffer size. This handles oversized writeX
		   calls, and possibly oversized SMBtrans calls */
		if ((message_flags(command) & LARGE_REQUEST) &&
		    ( !(message_flags(command) & AND_X) ||
		      (req->in.wct < 1 || SVAL(req->in.vwv, VWV(0)) == SMB_CHAIN_NONE)) &&
		    req->in.data_size < req->in.size - PTR_DIFF(req->in.data,req->in.buffer)) {
			req->in.data_size = req->in.size - PTR_DIFF(req->in.data,req->in.buffer);
		}
	}

	if (NBT_HDR_SIZE + MIN_SMB_SIZE + 2*req->in.wct > req->in.size) {
		DEBUG(2,("Invalid SMB word count %d\n", req->in.wct));
		smbsrv_terminate_connection(req->smb_conn, "Invalid SMB packet");
		return NT_STATUS_OK;
	}
 
	if (NBT_HDR_SIZE + MIN_SMB_SIZE + 2*req->in.wct + req->in.data_size > req->in.size) {
		DEBUG(2,("Invalid SMB buffer length count %d\n", 
			 (int)req->in.data_size));
		smbsrv_terminate_connection(req->smb_conn, "Invalid SMB packet");
		return NT_STATUS_OK;
	}

	req->flags2	= SVAL(req->in.hdr, HDR_FLG2);

	/* fix the bufinfo */
	smbsrv_setup_bufinfo(req);

	if (!smbsrv_signing_check_incoming(req)) {
		smbsrv_send_error(req, NT_STATUS_ACCESS_DENIED);
		return NT_STATUS_OK;
	}

	command = CVAL(req->in.hdr, HDR_COM);
	switch_message(command, req);
	return NT_STATUS_OK;
}

/****************************************************************************
return a string containing the function name of a SMB command
****************************************************************************/
static const char *smb_fn_name(uint8_t type)
{
	const char *unknown_name = "SMBunknown";

	if (smb_messages[type].name == NULL)
		return unknown_name;

	return smb_messages[type].name;
}


/****************************************************************************
 Do a switch on the message type and call the specific reply function for this 
message. Unlike earlier versions of Samba the reply functions are responsible
for sending the reply themselves, rather than returning a size to this function
The reply functions may also choose to delay the processing by pushing the message
onto the message queue
****************************************************************************/
static void switch_message(int type, struct smbsrv_request *req)
{
	int flags;
	struct smbsrv_connection *smb_conn = req->smb_conn;
	NTSTATUS status;

	type &= 0xff;

	errno = 0;

	if (smb_messages[type].fn == NULL) {
		DEBUG(0,("Unknown message type %d!\n",type));
		smbsrv_reply_unknown(req);
		return;
	}

	flags = smb_messages[type].flags;

	req->tcon = smbsrv_smb_tcon_find(smb_conn, SVAL(req->in.hdr,HDR_TID), req->request_time);

	if (!req->session) {
		/* setup the user context for this request if it
		   hasn't already been initialised (to cope with SMB
		   chaining) */

		/* In share mode security we must ignore the vuid. */
		if (smb_conn->config.security == SEC_SHARE) {
			if (req->tcon) {
				req->session = req->tcon->sec_share.session;
			}
 		} else {
			req->session = smbsrv_session_find(req->smb_conn, SVAL(req->in.hdr,HDR_UID), req->request_time);
		}
	}

	DEBUG(5,("switch message %s (task_id %u)\n",
		 smb_fn_name(type), (unsigned)req->smb_conn->connection->server_id.id));

	/* this must be called before we do any reply */
	if (flags & SIGNING_NO_REPLY) {
		smbsrv_signing_no_reply(req);
	}

	/* see if the vuid is valid */
	if ((flags & NEED_SESS) && !req->session) {
		status = NT_STATUS_DOS(ERRSRV, ERRbaduid);
		/* amazingly, the error code depends on the command */
		switch (type) {
		case SMBntcreateX:
		case SMBntcancel:
		case SMBulogoffX:
			break;
		default:
			if (req->smb_conn->config.nt_status_support &&
			    req->smb_conn->negotiate.client_caps & CAP_STATUS32) {
				status = NT_STATUS_INVALID_HANDLE;
			}
			break;
		}
		/* 
		 * TODO:
		 * don't know how to handle smb signing for this case 
		 * so just skip the reply
		 */
		if ((flags & SIGNING_NO_REPLY) &&
		    (req->smb_conn->signing.signing_state != SMB_SIGNING_ENGINE_OFF)) {
			DEBUG(1,("SKIP ERROR REPLY: %s %s because of unknown smb signing case\n",
				smb_fn_name(type), nt_errstr(status)));
			talloc_free(req);
			return;
		}
		smbsrv_send_error(req, status);
		return;
	}

	/* does this protocol need a valid tree connection? */
	if ((flags & NEED_TCON) && !req->tcon) {
		status = NT_STATUS_DOS(ERRSRV, ERRinvnid);
		/* amazingly, the error code depends on the command */
		switch (type) {
		case SMBntcreateX:
		case SMBntcancel:
		case SMBtdis:
			break;
		default:
			if (req->smb_conn->config.nt_status_support &&
			    req->smb_conn->negotiate.client_caps & CAP_STATUS32) {
				status = NT_STATUS_INVALID_HANDLE;
			}
			break;
		}
		/* 
		 * TODO:
		 * don't know how to handle smb signing for this case 
		 * so just skip the reply
		 */
		if ((flags & SIGNING_NO_REPLY) &&
		    (req->smb_conn->signing.signing_state != SMB_SIGNING_ENGINE_OFF)) {
			DEBUG(1,("SKIP ERROR REPLY: %s %s because of unknown smb signing case\n",
				smb_fn_name(type), nt_errstr(status)));
			talloc_free(req);
			return;
		}
		smbsrv_send_error(req, status);
		return;
	}

	smb_messages[type].fn(req);
}

/*
  we call this when first first part of a possibly chained request has been completed
  and we need to call the 2nd part, if any
*/
void smbsrv_chain_reply(struct smbsrv_request *req)
{
	uint16_t chain_cmd, chain_offset;
	uint8_t *vwv, *data;
	uint16_t wct;
	uint16_t data_size;

	if (req->in.wct < 2 || req->out.wct < 2) {
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror));
		return;
	}

	chain_cmd    = CVAL(req->in.vwv, VWV(0));
	chain_offset = SVAL(req->in.vwv, VWV(1));

	if (chain_cmd == SMB_CHAIN_NONE) {
		/* end of chain */
		SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
		SSVAL(req->out.vwv, VWV(1), 0);
		smbsrv_send_reply(req);
		return;
	}

	if (chain_offset + req->in.hdr >= req->in.buffer + req->in.size) {
		goto error;
	}

	wct = CVAL(req->in.hdr, chain_offset);
	vwv = req->in.hdr + chain_offset + 1;

	if (vwv + VWV(wct) + 2 > req->in.buffer + req->in.size) {
		goto error;
	}

	data_size = SVAL(vwv, VWV(wct));
	data = vwv + VWV(wct) + 2;

	if (data + data_size > req->in.buffer + req->in.size) {
		goto error;
	}

	/* all seems legit */
	req->in.vwv = vwv;
	req->in.wct = wct;
	req->in.data = data;
	req->in.data_size = data_size;
	req->in.ptr = data;

	/* fix the bufinfo */
	smbsrv_setup_bufinfo(req);

	req->chain_count++;

	SSVAL(req->out.vwv, VWV(0), chain_cmd);
	SSVAL(req->out.vwv, VWV(1), req->out.size - NBT_HDR_SIZE);

	/* cleanup somestuff for the next request */
	talloc_free(req->ntvfs);
	req->ntvfs = NULL;
	talloc_free(req->io_ptr);
	req->io_ptr = NULL;

	switch_message(chain_cmd, req);
	return;

error:
	SSVAL(req->out.vwv, VWV(0), SMB_CHAIN_NONE);
	SSVAL(req->out.vwv, VWV(1), 0);
	smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror));
}

/*
 * init the SMB protocol related stuff
 */
NTSTATUS smbsrv_init_smb_connection(struct smbsrv_connection *smb_conn, struct loadparm_context *lp_ctx)
{
	NTSTATUS status;

	/* now initialise a few default values associated with this smb socket */
	smb_conn->negotiate.max_send = 0xFFFF;

	/* this is the size that w2k uses, and it appears to be important for
	   good performance */
	smb_conn->negotiate.max_recv = lpcfg_max_xmit(lp_ctx);

	smb_conn->negotiate.zone_offset = get_time_zone(time(NULL));

	smb_conn->config.security = lpcfg_security(lp_ctx);
	smb_conn->config.nt_status_support = lpcfg_nt_status_support(lp_ctx);

	status = smbsrv_init_sessions(smb_conn, UINT16_MAX);
	NT_STATUS_NOT_OK_RETURN(status);

	status = smbsrv_smb_init_tcons(smb_conn);
	NT_STATUS_NOT_OK_RETURN(status);

	smbsrv_init_signing(smb_conn);

	return NT_STATUS_OK;
}
