/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   
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

#ifndef _DCE_RPC_H /* _DCE_RPC_H */
#define _DCE_RPC_H 

#define RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN 	0x20

/* Maximum size of the signing data in a fragment. */
#define RPC_MAX_SIGN_SIZE 0x38 /* 56 */

/* Maximum PDU fragment size. */
/* #define MAX_PDU_FRAG_LEN 0x1630		this is what wnt sets */
#define RPC_MAX_PDU_FRAG_LEN 0x10b8			/* this is what w2k sets */

#define RPC_IFACE_LEN (UUID_SIZE + 4)

/* RPC_HDR - dce rpc header */
typedef struct rpc_hdr_info {
	uint8  major; /* 5 - RPC major version */
	uint8  minor; /* 0 - RPC minor version */
	uint8  pkt_type; /* dcerpc_pkt_type - RPC response packet */
	uint8  flags; /* DCE/RPC flags */
	uint8  pack_type[4]; /* 0x1000 0000 - little-endian packed data representation */
	uint16 frag_len; /* fragment length - data size (bytes) inc header and tail. */
	uint16 auth_len; /* 0 - authentication length  */
	uint32 call_id; /* call identifier.  matches 12th uint32 of incoming RPC data. */
} RPC_HDR;

#define RPC_HEADER_LEN 16

/* RPC_HDR_REQ - ms request rpc header */
typedef struct rpc_hdr_req_info {
	uint32 alloc_hint;   /* allocation hint - data size (bytes) minus header and tail. */
	uint16 context_id;   /* presentation context identifier */
	uint16  opnum;       /* opnum */
} RPC_HDR_REQ;

#define RPC_HDR_REQ_LEN 8

/* RPC_HDR_RESP - ms response rpc header */
typedef struct rpc_hdr_resp_info {
	uint32 alloc_hint;   /* allocation hint - data size (bytes) minus header and tail. */
	uint16 context_id;   /* 0 - presentation context identifier */
	uint8  cancel_count; /* 0 - cancel count */
	uint8  reserved;     /* 0 - reserved. */
} RPC_HDR_RESP;

#define RPC_HDR_RESP_LEN 8

/* RPC_HDR_FAULT - fault rpc header */
typedef struct rpc_hdr_fault_info {
	NTSTATUS status;
	uint32 reserved; /* 0x0000 0000 */
} RPC_HDR_FAULT;

#define RPC_HDR_FAULT_LEN 8

/* this seems to be the same string name depending on the name of the pipe,
 * but is more likely to be linked to the interface name
 * "srvsvc", "\\PIPE\\ntsvcs"
 * "samr", "\\PIPE\\lsass"
 * "wkssvc", "\\PIPE\\wksvcs"
 * "NETLOGON", "\\PIPE\\NETLOGON"
 */
/* RPC_ADDR_STR */
typedef struct rpc_addr_info {
	uint16 len;   /* length of the string including null terminator */
	fstring str; /* the string above in single byte, null terminated form */
} RPC_ADDR_STR;

/* RPC_HDR_BBA - bind acknowledge, and alter context response. */
typedef struct rpc_hdr_bba_info {
	uint16 max_tsize;       /* maximum transmission fragment size (0x1630) */
	uint16 max_rsize;       /* max receive fragment size (0x1630) */
	uint32 assoc_gid;       /* associated group id (0x0) */
} RPC_HDR_BBA;

#define RPC_HDR_BBA_LEN 8

/* RPC_HDR_AUTH */
typedef struct rpc_hdr_auth_info {
	uint8 auth_type; /* See XXX_AUTH_TYPE above. */
	uint8 auth_level; /* See RPC_PIPE_AUTH_XXX_LEVEL above. */
	uint8 auth_pad_len;
	uint8 auth_reserved;
	uint32 auth_context_id;
} RPC_HDR_AUTH;

#define RPC_HDR_AUTH_LEN 8

typedef struct rpc_context {
	uint16 context_id;		/* presentation context identifier. */
	uint8 num_transfer_syntaxes;	/* the number of syntaxes */
	struct ndr_syntax_id abstract;	/* num and vers. of interface client is using */
	struct ndr_syntax_id *transfer;	/* Array of transfer interfaces. */
} RPC_CONTEXT;

/* RPC_BIND_REQ - ms req bind */
typedef struct rpc_bind_req_info {
	RPC_HDR_BBA bba;
	uint8 num_contexts;    /* the number of contexts */
	RPC_CONTEXT *rpc_context;
} RPC_HDR_RB;

/* 
 * The following length is 8 bytes RPC_HDR_BBA_LEN + 
 * 4 bytes size of context count +
 * (context_count * (4 bytes of context_id, size of transfer syntax count + RPC_IFACE_LEN bytes +
 *                    (transfer_syntax_count * RPC_IFACE_LEN bytes)))
 */

#define RPC_HDR_RB_LEN(rpc_hdr_rb) (RPC_HDR_BBA_LEN + 4 + \
	((rpc_hdr_rb)->num_contexts) * (4 + RPC_IFACE_LEN + (((rpc_hdr_rb)->rpc_context->num_transfer_syntaxes)*RPC_IFACE_LEN)))

/* RPC_RESULTS - can only cope with one reason, right now... */
typedef struct rpc_results_info {
	/* uint8[] # 4-byte alignment padding, against SMB header */

	uint8 num_results; /* the number of results (0x01) */

	/* uint8[] # 4-byte alignment padding, against SMB header */

	uint16 result; /* result (0x00 = accept) */
	uint16 reason; /* reason (0x00 = no reason specified) */
} RPC_RESULTS;

/* RPC_HDR_BA */
typedef struct rpc_hdr_ba_info {
	RPC_HDR_BBA bba;

	RPC_ADDR_STR addr    ;  /* the secondary address string, as described earlier */
	RPC_RESULTS  res     ; /* results and reasons */
	struct ndr_syntax_id transfer; /* the transfer syntax from the request */
} RPC_HDR_BA;

#endif /* _DCE_RPC_H */
