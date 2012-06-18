
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison                    1999.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"

extern int DEBUGLEVEL;


/*******************************************************************
interface/version dce/rpc pipe identification
********************************************************************/

#define TRANS_SYNT_V2                       \
{                                           \
	{                                   \
		0x8a885d04, 0x1ceb, 0x11c9, \
		{ 0x9f, 0xe8, 0x08, 0x00,   \
		0x2b, 0x10, 0x48, 0x60 }    \
	}, 0x02                             \
}

#define SYNT_NETLOGON_V2                    \
{                                           \
	{                                   \
		0x8a885d04, 0x1ceb, 0x11c9, \
		{ 0x9f, 0xe8, 0x08, 0x00,   \
		0x2b, 0x10, 0x48, 0x60 }    \
	}, 0x02                             \
}

#define SYNT_WKSSVC_V1                      \
{                                           \
	{                                   \
		0x6bffd098, 0xa112, 0x3610, \
		{ 0x98, 0x33, 0x46, 0xc3,   \
		0xf8, 0x7e, 0x34, 0x5a }    \
	}, 0x01                             \
}

#define SYNT_SRVSVC_V3                      \
{                                           \
	{                                   \
		0x4b324fc8, 0x1670, 0x01d3, \
		{ 0x12, 0x78, 0x5a, 0x47,   \
		0xbf, 0x6e, 0xe1, 0x88 }    \
	}, 0x03                             \
}

#define SYNT_LSARPC_V0                      \
{                                           \
	{                                   \
		0x12345778, 0x1234, 0xabcd, \
		{ 0xef, 0x00, 0x01, 0x23,   \
		0x45, 0x67, 0x89, 0xab }    \
	}, 0x00                             \
}

#define SYNT_SAMR_V1                        \
{                                           \
	{                                   \
		0x12345778, 0x1234, 0xabcd, \
		{ 0xef, 0x00, 0x01, 0x23,   \
		0x45, 0x67, 0x89, 0xac }    \
	}, 0x01                             \
}

#define SYNT_NETLOGON_V1                    \
{                                           \
	{                                   \
		0x12345678, 0x1234, 0xabcd, \
		{ 0xef, 0x00, 0x01, 0x23,   \
		0x45, 0x67, 0xcf, 0xfb }    \
	}, 0x01                             \
}

#define SYNT_WINREG_V1                      \
{                                           \
	{                                   \
		0x338cd001, 0x2244, 0x31f1, \
		{ 0xaa, 0xaa, 0x90, 0x00,   \
		0x38, 0x00, 0x10, 0x03 }    \
	}, 0x01                             \
}

#define SYNT_NONE_V0                        \
{                                           \
	{                                   \
		0x0, 0x0, 0x0,              \
		{ 0x00, 0x00, 0x00, 0x00,   \
		0x00, 0x00, 0x00, 0x00 }    \
	}, 0x00                             \
}

/* pipe string names */
#define PIPE_SRVSVC   "\\PIPE\\srvsvc"
#define PIPE_SAMR     "\\PIPE\\samr"
#define PIPE_WINREG   "\\PIPE\\winreg"
#define PIPE_WKSSVC   "\\PIPE\\wkssvc"
#define PIPE_NETLOGON "\\PIPE\\NETLOGON"
#define PIPE_NTLSA    "\\PIPE\\ntlsa"
#define PIPE_NTSVCS   "\\PIPE\\ntsvcs"
#define PIPE_LSASS    "\\PIPE\\lsass"
#define PIPE_LSARPC   "\\PIPE\\lsarpc"

struct pipe_id_info pipe_names [] =
{
	/* client pipe , abstract syntax , server pipe   , transfer syntax */
	{ PIPE_LSARPC  , SYNT_LSARPC_V0  , PIPE_LSASS    , TRANS_SYNT_V2 },
	{ PIPE_SAMR    , SYNT_SAMR_V1    , PIPE_LSASS    , TRANS_SYNT_V2 },
	{ PIPE_NETLOGON, SYNT_NETLOGON_V1, PIPE_LSASS    , TRANS_SYNT_V2 },
	{ PIPE_SRVSVC  , SYNT_SRVSVC_V3  , PIPE_NTSVCS   , TRANS_SYNT_V2 },
	{ PIPE_WKSSVC  , SYNT_WKSSVC_V1  , PIPE_NTSVCS   , TRANS_SYNT_V2 },
	{ PIPE_WINREG  , SYNT_WINREG_V1  , PIPE_WINREG   , TRANS_SYNT_V2 },
	{ NULL         , SYNT_NONE_V0    , NULL          , SYNT_NONE_V0  }
};

/*******************************************************************
 Inits an RPC_HDR structure.
********************************************************************/

void init_rpc_hdr(RPC_HDR *hdr, enum RPC_PKT_TYPE pkt_type, uint8 flags,
				uint32 call_id, int data_len, int auth_len)
{
	hdr->major        = 5;               /* RPC version 5 */
	hdr->minor        = 0;               /* minor version 0 */
	hdr->pkt_type     = pkt_type;        /* RPC packet type */
	hdr->flags        = flags;           /* dce/rpc flags */
	hdr->pack_type[0] = 0x10;            /* little-endian data representation */
	hdr->pack_type[1] = 0;               /* packed data representation */
	hdr->pack_type[2] = 0;               /* packed data representation */
	hdr->pack_type[3] = 0;               /* packed data representation */
	hdr->frag_len     = data_len;        /* fragment length, fill in later */
	hdr->auth_len     = auth_len;        /* authentication length */
	hdr->call_id      = call_id;         /* call identifier - match incoming RPC */
}

/*******************************************************************
 Reads or writes an RPC_HDR structure.
********************************************************************/

BOOL smb_io_rpc_hdr(char *desc,  RPC_HDR *rpc, prs_struct *ps, int depth)
{
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr");
	depth++;

	if(!prs_uint8 ("major     ", ps, depth, &rpc->major))
		return False;

	if(!prs_uint8 ("minor     ", ps, depth, &rpc->minor))
		return False;
	if(!prs_uint8 ("pkt_type  ", ps, depth, &rpc->pkt_type))
		return False;
	if(!prs_uint8 ("flags     ", ps, depth, &rpc->flags))
		return False;
	if(!prs_uint8("pack_type0", ps, depth, &rpc->pack_type[0]))
		return False;
	if(!prs_uint8("pack_type1", ps, depth, &rpc->pack_type[1]))
		return False;
	if(!prs_uint8("pack_type2", ps, depth, &rpc->pack_type[2]))
		return False;
	if(!prs_uint8("pack_type3", ps, depth, &rpc->pack_type[3]))
		return False;

	/*
	 * If reading and pack_type[0] == 0 then the data is in big-endian
	 * format. Set the flag in the prs_struct to specify reverse-endainness.
	 */

	if (ps->io && rpc->pack_type[0] == 0) {
		DEBUG(10,("smb_io_rpc_hdr: PDU data format is big-endian. Setting flag.\n"));
		prs_set_bigendian_data(ps);
	}

	if(!prs_uint16("frag_len  ", ps, depth, &rpc->frag_len))
		return False;
	if(!prs_uint16("auth_len  ", ps, depth, &rpc->auth_len))
		return False;
	if(!prs_uint32("call_id   ", ps, depth, &rpc->call_id))
		return False;
	return True;
}

/*******************************************************************
 Reads or writes an RPC_IFACE structure.
********************************************************************/

static BOOL smb_io_rpc_iface(char *desc, RPC_IFACE *ifc, prs_struct *ps, int depth)
{
	if (ifc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_iface");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32 ("data   ", ps, depth, &ifc->uuid.time_low))
		return False;
	if(!prs_uint16 ("data   ", ps, depth, &ifc->uuid.time_mid))
		return False;
	if(!prs_uint16 ("data   ", ps, depth, &ifc->uuid.time_hi_and_version))
		return False;

	if(!prs_uint8s (False, "data   ", ps, depth, ifc->uuid.remaining, sizeof(ifc->uuid.remaining)))
		return False;
	if(!prs_uint32 (       "version", ps, depth, &(ifc->version)))
		return False;

	return True;
}

/*******************************************************************
 Inits an RPC_ADDR_STR structure.
********************************************************************/

static void init_rpc_addr_str(RPC_ADDR_STR *str, char *name)
{
	str->len = strlen(name) + 1;
	fstrcpy(str->str, name);
}

/*******************************************************************
 Reads or writes an RPC_ADDR_STR structure.
********************************************************************/

static BOOL smb_io_rpc_addr_str(char *desc,  RPC_ADDR_STR *str, prs_struct *ps, int depth)
{
	if (str == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_addr_str");
	depth++;
	if(!prs_align(ps))
		return False;

	if(!prs_uint16 (      "len", ps, depth, &str->len))
		return False;
	if(!prs_uint8s (True, "str", ps, depth, (uchar*)str->str, MIN(str->len, sizeof(str->str)) ))
		return False;
	return True;
}

/*******************************************************************
 Inits an RPC_HDR_BBA structure.
********************************************************************/

static void init_rpc_hdr_bba(RPC_HDR_BBA *bba, uint16 max_tsize, uint16 max_rsize, uint32 assoc_gid)
{
	bba->max_tsize = max_tsize; /* maximum transmission fragment size (0x1630) */
	bba->max_rsize = max_rsize; /* max receive fragment size (0x1630) */   
	bba->assoc_gid = assoc_gid; /* associated group id (0x0) */ 
}

/*******************************************************************
 Reads or writes an RPC_HDR_BBA structure.
********************************************************************/

static BOOL smb_io_rpc_hdr_bba(char *desc,  RPC_HDR_BBA *rpc, prs_struct *ps, int depth)
{
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_bba");
	depth++;

	if(!prs_uint16("max_tsize", ps, depth, &rpc->max_tsize))
		return False;
	if(!prs_uint16("max_rsize", ps, depth, &rpc->max_rsize))
		return False;
	if(!prs_uint32("assoc_gid", ps, depth, &rpc->assoc_gid))
		return False;
	return True;
}

/*******************************************************************
 Inits an RPC_HDR_RB structure.
********************************************************************/

void init_rpc_hdr_rb(RPC_HDR_RB *rpc, 
				uint16 max_tsize, uint16 max_rsize, uint32 assoc_gid,
				uint32 num_elements, uint16 context_id, uint8 num_syntaxes,
				RPC_IFACE *abstract, RPC_IFACE *transfer)
{
	init_rpc_hdr_bba(&rpc->bba, max_tsize, max_rsize, assoc_gid);

	rpc->num_elements = num_elements ; /* the number of elements (0x1) */
	rpc->context_id   = context_id   ; /* presentation context identifier (0x0) */
	rpc->num_syntaxes = num_syntaxes ; /* the number of syntaxes (has always been 1?)(0x1) */

	/* num and vers. of interface client is using */
	rpc->abstract = *abstract;

	/* num and vers. of interface to use for replies */
	rpc->transfer = *transfer;
}

/*******************************************************************
 Reads or writes an RPC_HDR_RB structure.
********************************************************************/

BOOL smb_io_rpc_hdr_rb(char *desc, RPC_HDR_RB *rpc, prs_struct *ps, int depth)
{
	RPC_HDR_RB rpc2;
	int i;
	
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_rb");
	depth++;

	if(!smb_io_rpc_hdr_bba("", &rpc->bba, ps, depth))
		return False;

	if(!prs_uint32("num_elements", ps, depth, &rpc->num_elements))
		return False;
	if(!prs_uint16("context_id  ", ps, depth, &rpc->context_id ))
		return False;
	if(!prs_uint8 ("num_syntaxes", ps, depth, &rpc->num_syntaxes))
		return False;

	if(!smb_io_rpc_iface("", &rpc->abstract, ps, depth))
		return False;
	if(!smb_io_rpc_iface("", &rpc->transfer, ps, depth))
		return False;
		
	/* just chew through extra context id's for now */
	
	for ( i=1; i<rpc->num_elements; i++ ) {
		if(!prs_uint16("context_id  ", ps, depth, &rpc2.context_id ))
			return False;
		if(!prs_uint8 ("num_syntaxes", ps, depth, &rpc2.num_syntaxes))
			return False;

		if(!smb_io_rpc_iface("", &rpc2.abstract, ps, depth))
			return False;
		if(!smb_io_rpc_iface("", &rpc2.transfer, ps, depth))
			return False;
	}	

	return True;
}

/*******************************************************************
 Inits an RPC_RESULTS structure.

 lkclXXXX only one reason at the moment!
********************************************************************/

static void init_rpc_results(RPC_RESULTS *res, 
				uint8 num_results, uint16 result, uint16 reason)
{
	res->num_results = num_results; /* the number of results (0x01) */
	res->result      = result     ;  /* result (0x00 = accept) */
	res->reason      = reason     ;  /* reason (0x00 = no reason specified) */
}

/*******************************************************************
 Reads or writes an RPC_RESULTS structure.

 lkclXXXX only one reason at the moment!
********************************************************************/

static BOOL smb_io_rpc_results(char *desc, RPC_RESULTS *res, prs_struct *ps, int depth)
{
	if (res == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_results");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint8 ("num_results", ps, depth, &res->num_results))	
		return False;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint16("result     ", ps, depth, &res->result))
		return False;
	if(!prs_uint16("reason     ", ps, depth, &res->reason))
		return False;
	return True;
}

/*******************************************************************
 Init an RPC_HDR_BA structure.

 lkclXXXX only one reason at the moment!

********************************************************************/

void init_rpc_hdr_ba(RPC_HDR_BA *rpc, 
				uint16 max_tsize, uint16 max_rsize, uint32 assoc_gid,
				char *pipe_addr,
				uint8 num_results, uint16 result, uint16 reason,
				RPC_IFACE *transfer)
{
	init_rpc_hdr_bba (&rpc->bba, max_tsize, max_rsize, assoc_gid);
	init_rpc_addr_str(&rpc->addr, pipe_addr);
	init_rpc_results (&rpc->res, num_results, result, reason);

	/* the transfer syntax from the request */
	memcpy(&rpc->transfer, transfer, sizeof(rpc->transfer));
}

/*******************************************************************
 Reads or writes an RPC_HDR_BA structure.
********************************************************************/

BOOL smb_io_rpc_hdr_ba(char *desc, RPC_HDR_BA *rpc, prs_struct *ps, int depth)
{
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_ba");
	depth++;

	if(!smb_io_rpc_hdr_bba("", &rpc->bba, ps, depth))
		return False;
	if(!smb_io_rpc_addr_str("", &rpc->addr, ps, depth))
		return False;
	if(!smb_io_rpc_results("", &rpc->res, ps, depth))
		return False;
	if(!smb_io_rpc_iface("", &rpc->transfer, ps, depth))
		return False;
	return True;
}

/*******************************************************************
 Init an RPC_HDR_REQ structure.
********************************************************************/

void init_rpc_hdr_req(RPC_HDR_REQ *hdr, uint32 alloc_hint, uint16 opnum)
{
	hdr->alloc_hint   = alloc_hint; /* allocation hint */
	hdr->context_id   = 0;         /* presentation context identifier */
	hdr->opnum        = opnum;     /* opnum */
}

/*******************************************************************
 Reads or writes an RPC_HDR_REQ structure.
********************************************************************/

BOOL smb_io_rpc_hdr_req(char *desc, RPC_HDR_REQ *rpc, prs_struct *ps, int depth)
{
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_req");
	depth++;

	if(!prs_uint32("alloc_hint", ps, depth, &rpc->alloc_hint))
		return False;
	if(!prs_uint16("context_id", ps, depth, &rpc->context_id))
		return False;
	if(!prs_uint16("opnum     ", ps, depth, &rpc->opnum))
		return False;
	return True;
}

/*******************************************************************
 Reads or writes an RPC_HDR_RESP structure.
********************************************************************/

BOOL smb_io_rpc_hdr_resp(char *desc, RPC_HDR_RESP *rpc, prs_struct *ps, int depth)
{
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_resp");
	depth++;

	if(!prs_uint32("alloc_hint", ps, depth, &rpc->alloc_hint))
		return False;
	if(!prs_uint16("context_id", ps, depth, &rpc->context_id))
		return False;
	if(!prs_uint8 ("cancel_ct ", ps, depth, &rpc->cancel_count))
		return False;
	if(!prs_uint8 ("reserved  ", ps, depth, &rpc->reserved))
		return False;
	return True;
}

/*******************************************************************
 Reads or writes an RPC_HDR_FAULT structure.
********************************************************************/

BOOL smb_io_rpc_hdr_fault(char *desc, RPC_HDR_FAULT *rpc, prs_struct *ps, int depth)
{
	if (rpc == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_fault");
	depth++;

	if(!prs_uint32("status  ", ps, depth, &rpc->status))
		return False;
	if(!prs_uint32("reserved", ps, depth, &rpc->reserved))
		return False;

    return True;
}

/*******************************************************************
 Init an RPC_HDR_AUTHA structure.
********************************************************************/

void init_rpc_hdr_autha(RPC_HDR_AUTHA *rai,
				uint16 max_tsize, uint16 max_rsize,
				uint8 auth_type, uint8 auth_level,
				uint8 stub_type_len)
{
	rai->max_tsize = max_tsize; /* maximum transmission fragment size (0x1630) */
	rai->max_rsize = max_rsize; /* max receive fragment size (0x1630) */   

	rai->auth_type     = auth_type; /* nt lm ssp 0x0a */
	rai->auth_level    = auth_level; /* 0x06 */
	rai->stub_type_len = stub_type_len; /* 0x00 */
	rai->padding       = 0; /* padding 0x00 */

	rai->unknown       = 0x0014a0c0; /* non-zero pointer to something */
}

/*******************************************************************
 Reads or writes an RPC_HDR_AUTHA structure.
********************************************************************/

BOOL smb_io_rpc_hdr_autha(char *desc, RPC_HDR_AUTHA *rai, prs_struct *ps, int depth)
{
	if (rai == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_autha");
	depth++;

	if(!prs_uint16("max_tsize    ", ps, depth, &rai->max_tsize))
		return False;
	if(!prs_uint16("max_rsize    ", ps, depth, &rai->max_rsize))
		return False;

	if(!prs_uint8 ("auth_type    ", ps, depth, &rai->auth_type)) /* 0x0a nt lm ssp */
		return False;
	if(!prs_uint8 ("auth_level   ", ps, depth, &rai->auth_level)) /* 0x06 */
		return False;
	if(!prs_uint8 ("stub_type_len", ps, depth, &rai->stub_type_len))
		return False;
	if(!prs_uint8 ("padding      ", ps, depth, &rai->padding))
		return False;

	if(!prs_uint32("unknown      ", ps, depth, &rai->unknown)) /* 0x0014a0c0 */
		return False;

	return True;
}

/*******************************************************************
 Checks an RPC_HDR_AUTH structure.
********************************************************************/

BOOL rpc_hdr_auth_chk(RPC_HDR_AUTH *rai)
{
	return (rai->auth_type == NTLMSSP_AUTH_TYPE && rai->auth_level == NTLMSSP_AUTH_LEVEL);
}

/*******************************************************************
 Inits an RPC_HDR_AUTH structure.
********************************************************************/

void init_rpc_hdr_auth(RPC_HDR_AUTH *rai,
				uint8 auth_type, uint8 auth_level,
				uint8 stub_type_len,
				uint32 ptr)
{
	rai->auth_type     = auth_type; /* nt lm ssp 0x0a */
	rai->auth_level    = auth_level; /* 0x06 */
	rai->stub_type_len = stub_type_len; /* 0x00 */
	rai->padding       = 0; /* padding 0x00 */

	rai->unknown       = ptr; /* non-zero pointer to something */
}

/*******************************************************************
 Reads or writes an RPC_HDR_AUTH structure.
********************************************************************/

BOOL smb_io_rpc_hdr_auth(char *desc, RPC_HDR_AUTH *rai, prs_struct *ps, int depth)
{
	if (rai == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_hdr_auth");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint8 ("auth_type    ", ps, depth, &rai->auth_type)) /* 0x0a nt lm ssp */
		return False;
	if(!prs_uint8 ("auth_level   ", ps, depth, &rai->auth_level)) /* 0x06 */
		return False;
	if(!prs_uint8 ("stub_type_len", ps, depth, &rai->stub_type_len))
		return False;
	if(!prs_uint8 ("padding      ", ps, depth, &rai->padding))
		return False;

	if(!prs_uint32("unknown      ", ps, depth, &rai->unknown)) /* 0x0014a0c0 */
		return False;

	return True;
}

/*******************************************************************
 Checks an RPC_AUTH_VERIFIER structure.
********************************************************************/

BOOL rpc_auth_verifier_chk(RPC_AUTH_VERIFIER *rav,
				char *signature, uint32 msg_type)
{
	return (strequal(rav->signature, signature) && rav->msg_type == msg_type);
}

/*******************************************************************
 Inits an RPC_AUTH_VERIFIER structure.
********************************************************************/

void init_rpc_auth_verifier(RPC_AUTH_VERIFIER *rav,
				char *signature, uint32 msg_type)
{
	fstrcpy(rav->signature, signature); /* "NTLMSSP" */
	rav->msg_type = msg_type; /* NTLMSSP_MESSAGE_TYPE */
}

/*******************************************************************
 Reads or writes an RPC_AUTH_VERIFIER structure.
********************************************************************/

BOOL smb_io_rpc_auth_verifier(char *desc, RPC_AUTH_VERIFIER *rav, prs_struct *ps, int depth)
{
	if (rav == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_auth_verifier");
	depth++;

	/* "NTLMSSP" */
	if(!prs_string("signature", ps, depth, rav->signature, strlen("NTLMSSP"),
			sizeof(rav->signature)))
		return False;
	if(!prs_uint32("msg_type ", ps, depth, &rav->msg_type)) /* NTLMSSP_MESSAGE_TYPE */
		return False;

	return True;
}

/*******************************************************************
 Inits an RPC_AUTH_NTLMSSP_NEG structure.
********************************************************************/

void init_rpc_auth_ntlmssp_neg(RPC_AUTH_NTLMSSP_NEG *neg,
				uint32 neg_flgs,
				fstring myname, fstring domain)
{
	int len_myname = strlen(myname);
	int len_domain = strlen(domain);

	neg->neg_flgs = neg_flgs ; /* 0x00b2b3 */

	init_str_hdr(&neg->hdr_domain, len_domain, len_domain, 0x20 + len_myname); 
	init_str_hdr(&neg->hdr_myname, len_myname, len_myname, 0x20); 

	fstrcpy(neg->myname, myname);
	fstrcpy(neg->domain, domain);
}

/*******************************************************************
 Reads or writes an RPC_AUTH_NTLMSSP_NEG structure.

 *** lkclXXXX HACK ALERT! ***
********************************************************************/

BOOL smb_io_rpc_auth_ntlmssp_neg(char *desc, RPC_AUTH_NTLMSSP_NEG *neg, prs_struct *ps, int depth)
{
	uint32 start_offset = prs_offset(ps);
	if (neg == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_auth_ntlmssp_neg");
	depth++;

	if(!prs_uint32("neg_flgs ", ps, depth, &neg->neg_flgs))
		return False;

	if (ps->io) {
		uint32 old_offset;
		uint32 old_neg_flags = neg->neg_flgs;

		/* reading */

		ZERO_STRUCTP(neg);

		neg->neg_flgs = old_neg_flags;

		if(!smb_io_strhdr("hdr_domain", &neg->hdr_domain, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_myname", &neg->hdr_myname, ps, depth))
			return False;

		old_offset = prs_offset(ps);

		if(!prs_set_offset(ps, neg->hdr_myname.buffer + start_offset - 12))
			return False;

		if(!prs_uint8s(True, "myname", ps, depth, (uint8*)neg->myname, 
				MIN(neg->hdr_myname.str_str_len, sizeof(neg->myname))))
			return False;

		old_offset += neg->hdr_myname.str_str_len;

		if(!prs_set_offset(ps, neg->hdr_domain.buffer + start_offset - 12))
			return False;

		if(!prs_uint8s(True, "domain", ps, depth, (uint8*)neg->domain, 
			MIN(neg->hdr_domain.str_str_len, sizeof(neg->domain  ))))
			return False;

		old_offset += neg->hdr_domain  .str_str_len;

		if(!prs_set_offset(ps, old_offset))
			return False;
	} else {
		/* writing */
		if(!smb_io_strhdr("hdr_domain", &neg->hdr_domain, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_myname", &neg->hdr_myname, ps, depth))
			return False;

		if(!prs_uint8s(True, "myname", ps, depth, (uint8*)neg->myname, 
					MIN(neg->hdr_myname.str_str_len, sizeof(neg->myname))))
			return False;
		if(!prs_uint8s(True, "domain", ps, depth, (uint8*)neg->domain, 
					MIN(neg->hdr_domain.str_str_len, sizeof(neg->domain  ))))
			return False;
	}

	return True;
}

/*******************************************************************
creates an RPC_AUTH_NTLMSSP_CHAL structure.
********************************************************************/

void init_rpc_auth_ntlmssp_chal(RPC_AUTH_NTLMSSP_CHAL *chl,
				uint32 neg_flags,
				uint8 challenge[8])
{
	chl->unknown_1 = 0x0; 
	chl->unknown_2 = 0x00000028;
	chl->neg_flags = neg_flags; /* 0x0082b1 */

	memcpy(chl->challenge, challenge, sizeof(chl->challenge)); 
	memset((char *)chl->reserved , '\0', sizeof(chl->reserved)); 
}

/*******************************************************************
 Reads or writes an RPC_AUTH_NTLMSSP_CHAL structure.
********************************************************************/

BOOL smb_io_rpc_auth_ntlmssp_chal(char *desc, RPC_AUTH_NTLMSSP_CHAL *chl, prs_struct *ps, int depth)
{
	if (chl == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_auth_ntlmssp_chal");
	depth++;

	if(!prs_uint32("unknown_1", ps, depth, &chl->unknown_1)) /* 0x0000 0000 */
		return False;
	if(!prs_uint32("unknown_2", ps, depth, &chl->unknown_2)) /* 0x0000 b2b3 */
		return False;
	if(!prs_uint32("neg_flags", ps, depth, &chl->neg_flags)) /* 0x0000 82b1 */
		return False;

	if(!prs_uint8s (False, "challenge", ps, depth, chl->challenge, sizeof(chl->challenge)))
		return False;
	if(!prs_uint8s (False, "reserved ", ps, depth, chl->reserved , sizeof(chl->reserved )))
		return False;

	return True;
}

/*******************************************************************
 Inits an RPC_AUTH_NTLMSSP_RESP structure.

 *** lkclXXXX FUDGE!  HAVE TO MANUALLY SPECIFY OFFSET HERE (0x1c bytes) ***
 *** lkclXXXX the actual offset is at the start of the auth verifier    ***
********************************************************************/

void init_rpc_auth_ntlmssp_resp(RPC_AUTH_NTLMSSP_RESP *rsp,
				uchar lm_resp[24], uchar nt_resp[24],
				char *domain, char *user, char *wks,
				uint32 neg_flags)
{
	uint32 offset;
	int dom_len = strlen(domain);
	int wks_len = strlen(wks);
	int usr_len = strlen(user);
	int lm_len  = (lm_resp != NULL) ? 24 : 0;
	int nt_len  = (nt_resp != NULL) ? 24 : 0;

	DEBUG(5,("make_rpc_auth_ntlmssp_resp\n"));

#ifdef DEBUG_PASSWORD
	DEBUG(100,("lm_resp\n"));
	dump_data(100, (char *)lm_resp, 24);
	DEBUG(100,("nt_resp\n"));
	dump_data(100, (char *)nt_resp, 24);
#endif

	DEBUG(6,("dom: %s user: %s wks: %s neg_flgs: 0x%x\n",
	          domain, user, wks, neg_flags));

	offset = 0x40;

	if (IS_BITS_SET_ALL(neg_flags, NTLMSSP_NEGOTIATE_UNICODE))
	{
		dom_len *= 2;
		wks_len *= 2;
		usr_len *= 2;
	}

	init_str_hdr(&rsp->hdr_domain, dom_len, dom_len, offset);
	offset += dom_len;

	init_str_hdr(&rsp->hdr_usr, usr_len, usr_len, offset);
	offset += usr_len;

	init_str_hdr(&rsp->hdr_wks, wks_len, wks_len, offset);
	offset += wks_len;

	init_str_hdr(&rsp->hdr_lm_resp, lm_len, lm_len, offset);
	offset += lm_len;

	init_str_hdr(&rsp->hdr_nt_resp, nt_len, nt_len, offset);
	offset += nt_len;

	init_str_hdr(&rsp->hdr_sess_key, 0, 0, offset);

	rsp->neg_flags = neg_flags;

	memcpy(rsp->lm_resp, lm_resp, 24);
	memcpy(rsp->nt_resp, nt_resp, 24);

	if (IS_BITS_SET_ALL(neg_flags, NTLMSSP_NEGOTIATE_UNICODE)) {
		dos_struni2(rsp->domain, domain, sizeof(rsp->domain));
		dos_struni2(rsp->user, user, sizeof(rsp->user));
		dos_struni2(rsp->wks, wks, sizeof(rsp->wks));
	} else {
		fstrcpy(rsp->domain, domain);
		fstrcpy(rsp->user, user);
		fstrcpy(rsp->wks, wks);
	}
	rsp->sess_key[0] = 0;
}

/*******************************************************************
 Reads or writes an RPC_AUTH_NTLMSSP_RESP structure.

 *** lkclXXXX FUDGE!  HAVE TO MANUALLY SPECIFY OFFSET HERE (0x1c bytes) ***
 *** lkclXXXX the actual offset is at the start of the auth verifier    ***
********************************************************************/

BOOL smb_io_rpc_auth_ntlmssp_resp(char *desc, RPC_AUTH_NTLMSSP_RESP *rsp, prs_struct *ps, int depth)
{
	if (rsp == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_auth_ntlmssp_resp");
	depth++;

	if (ps->io) {
		uint32 old_offset;

		/* reading */

		ZERO_STRUCTP(rsp);

		if(!smb_io_strhdr("hdr_lm_resp ", &rsp->hdr_lm_resp, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_nt_resp ", &rsp->hdr_nt_resp, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_domain  ", &rsp->hdr_domain, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_user    ", &rsp->hdr_usr, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_wks     ", &rsp->hdr_wks, ps, depth)) 
			return False;
		if(!smb_io_strhdr("hdr_sess_key", &rsp->hdr_sess_key, ps, depth))
			return False;

		if(!prs_uint32("neg_flags", ps, depth, &rsp->neg_flags)) /* 0x0000 82b1 */
			return False;

		old_offset = prs_offset(ps);

		if(!prs_set_offset(ps, rsp->hdr_domain.buffer + 0xc))
			return False;

		if(!prs_uint8s(True , "domain  ", ps, depth, (uint8*)rsp->domain,
				MIN(rsp->hdr_domain.str_str_len, sizeof(rsp->domain))))
			return False;

		old_offset += rsp->hdr_domain.str_str_len;

		if(!prs_set_offset(ps, rsp->hdr_usr.buffer + 0xc))
			return False;

		if(!prs_uint8s(True , "user    ", ps, depth, (uint8*)rsp->user,
				MIN(rsp->hdr_usr.str_str_len, sizeof(rsp->user))))
			return False;

		old_offset += rsp->hdr_usr.str_str_len;

		if(!prs_set_offset(ps, rsp->hdr_wks.buffer + 0xc))
			return False;

		if(!prs_uint8s(True, "wks     ", ps, depth, (uint8*)rsp->wks,
				MIN(rsp->hdr_wks.str_str_len, sizeof(rsp->wks))))
			return False;

		old_offset += rsp->hdr_wks.str_str_len;

		if(!prs_set_offset(ps, rsp->hdr_lm_resp.buffer + 0xc))
			return False;

		if(!prs_uint8s(False, "lm_resp ", ps, depth, (uint8*)rsp->lm_resp,
				MIN(rsp->hdr_lm_resp.str_str_len, sizeof(rsp->lm_resp ))))
			return False;

		old_offset += rsp->hdr_lm_resp.str_str_len;

		if(!prs_set_offset(ps, rsp->hdr_nt_resp.buffer + 0xc))
			return False;

		if(!prs_uint8s(False, "nt_resp ", ps, depth, (uint8*)rsp->nt_resp,
				MIN(rsp->hdr_nt_resp.str_str_len, sizeof(rsp->nt_resp ))))
			return False;

		old_offset += rsp->hdr_nt_resp.str_str_len;

		if (rsp->hdr_sess_key.str_str_len != 0) {

			if(!prs_set_offset(ps, rsp->hdr_sess_key.buffer + 0x10))
				return False;

			old_offset += rsp->hdr_sess_key.str_str_len;

			if(!prs_uint8s(False, "sess_key", ps, depth, (uint8*)rsp->sess_key,
					MIN(rsp->hdr_sess_key.str_str_len, sizeof(rsp->sess_key))))
				return False;
		}

		if(!prs_set_offset(ps, old_offset))
			return False;
	} else {
		/* writing */
		if(!smb_io_strhdr("hdr_lm_resp ", &rsp->hdr_lm_resp, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_nt_resp ", &rsp->hdr_nt_resp, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_domain  ", &rsp->hdr_domain, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_user    ", &rsp->hdr_usr, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_wks     ", &rsp->hdr_wks, ps, depth))
			return False;
		if(!smb_io_strhdr("hdr_sess_key", &rsp->hdr_sess_key, ps, depth))
			return False;

		if(!prs_uint32("neg_flags", ps, depth, &rsp->neg_flags)) /* 0x0000 82b1 */
			return False;

		if(!prs_uint8s(True , "domain  ", ps, depth, (uint8*)rsp->domain,
				MIN(rsp->hdr_domain.str_str_len, sizeof(rsp->domain))))
			return False;

		if(!prs_uint8s(True , "user    ", ps, depth, (uint8*)rsp->user,
				MIN(rsp->hdr_usr.str_str_len, sizeof(rsp->user))))
			return False;

		if(!prs_uint8s(True , "wks     ", ps, depth, (uint8*)rsp->wks,
				MIN(rsp->hdr_wks.str_str_len, sizeof(rsp->wks))))
			return False;
		if(!prs_uint8s(False, "lm_resp ", ps, depth, (uint8*)rsp->lm_resp,
				MIN(rsp->hdr_lm_resp .str_str_len, sizeof(rsp->lm_resp))))
			return False;
		if(!prs_uint8s(False, "nt_resp ", ps, depth, (uint8*)rsp->nt_resp,
				MIN(rsp->hdr_nt_resp .str_str_len, sizeof(rsp->nt_resp ))))
			return False;
		if(!prs_uint8s(False, "sess_key", ps, depth, (uint8*)rsp->sess_key,
				MIN(rsp->hdr_sess_key.str_str_len, sizeof(rsp->sess_key))))
			return False;
	}

	return True;
}

/*******************************************************************
 Checks an RPC_AUTH_NTLMSSP_CHK structure.
********************************************************************/

BOOL rpc_auth_ntlmssp_chk(RPC_AUTH_NTLMSSP_CHK *chk, uint32 crc32, uint32 seq_num)
{
	if (chk == NULL)
		return False;

	if (chk->crc32 != crc32 ||
	    chk->ver   != NTLMSSP_SIGN_VERSION ||
	    chk->seq_num != seq_num)
	{
		DEBUG(5,("verify failed - crc %x ver %x seq %d\n",
			crc32, NTLMSSP_SIGN_VERSION, seq_num));
		DEBUG(5,("verify expect - crc %x ver %x seq %d\n",
			chk->crc32, chk->ver, chk->seq_num));
		return False;
	}
	return True;
}

/*******************************************************************
 Inits an RPC_AUTH_NTLMSSP_CHK structure.
********************************************************************/

void init_rpc_auth_ntlmssp_chk(RPC_AUTH_NTLMSSP_CHK *chk,
				uint32 ver, uint32 crc32, uint32 seq_num)
{
	chk->ver      = ver;
	chk->reserved = 0x0;
	chk->crc32    = crc32;
	chk->seq_num  = seq_num;
}

/*******************************************************************
 Reads or writes an RPC_AUTH_NTLMSSP_CHK structure.
********************************************************************/

BOOL smb_io_rpc_auth_ntlmssp_chk(char *desc, RPC_AUTH_NTLMSSP_CHK *chk, prs_struct *ps, int depth)
{
	if (chk == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_rpc_auth_ntlmssp_chk");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("ver     ", ps, depth, &chk->ver))
		return False;
	if(!prs_uint32("reserved", ps, depth, &chk->reserved))
		return False;
	if(!prs_uint32("crc32   ", ps, depth, &chk->crc32))
		return False;
	if(!prs_uint32("seq_num ", ps, depth, &chk->seq_num))
		return False;

	return True;
}
