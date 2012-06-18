/* 
   Unix SMB/CIFS implementation.

   DCERPC client side interface structures

   Copyright (C) 2008 Jelmer Vernooij
   
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

/* This is a public header file that is installed as part of Samba. 
 * If you remove any functions or change their signature, update 
 * the so version number. */

#ifndef __DCERPC_H__
#define __DCERPC_H__

#include "includes.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/epmapper.h"

struct loadparm_context;
struct cli_credentials;

/**
 * Connection to a particular DCE/RPC interface.
 */
struct dcerpc_pipe {
	const struct ndr_interface_table *table;

	/** SMB context used when transport is ncacn_np. */
	struct cli_state *cli;

	/** Samba 3 DCE/RPC client context. */
	struct rpc_pipe_client *rpc_cli;
};

struct rpc_request {
	const struct ndr_interface_call *call;
	prs_struct q_ps;
	uint32_t opnum;
	struct dcerpc_pipe *pipe;
	void *r;
};

enum dcerpc_transport_t {
	NCA_UNKNOWN, NCACN_NP, NCACN_IP_TCP, NCACN_IP_UDP, NCACN_VNS_IPC, 
	NCACN_VNS_SPP, NCACN_AT_DSP, NCADG_AT_DDP, NCALRPC, NCACN_UNIX_STREAM, 
	NCADG_UNIX_DGRAM, NCACN_HTTP, NCADG_IPX, NCACN_SPX, NCACN_INTERNAL };


/** this describes a binding to a particular transport/pipe */
struct dcerpc_binding {
	enum dcerpc_transport_t transport;
	struct ndr_syntax_id object;
	const char *host;
	const char *target_hostname;
	const char *endpoint;
	const char **options;
	uint32_t flags;
	uint32_t assoc_group_id;
};


/* dcerpc pipe flags */
#define DCERPC_DEBUG_PRINT_IN          (1<<0)
#define DCERPC_DEBUG_PRINT_OUT         (1<<1)
#define DCERPC_DEBUG_PRINT_BOTH (DCERPC_DEBUG_PRINT_IN | DCERPC_DEBUG_PRINT_OUT)

#define DCERPC_DEBUG_VALIDATE_IN       (1<<2)
#define DCERPC_DEBUG_VALIDATE_OUT      (1<<3)
#define DCERPC_DEBUG_VALIDATE_BOTH (DCERPC_DEBUG_VALIDATE_IN | DCERPC_DEBUG_VALIDATE_OUT)

#define DCERPC_CONNECT                 (1<<4)
#define DCERPC_SIGN                    (1<<5)
#define DCERPC_SEAL                    (1<<6)

#define DCERPC_PUSH_BIGENDIAN          (1<<7)
#define DCERPC_PULL_BIGENDIAN          (1<<8)

#define DCERPC_SCHANNEL                (1<<9)

/* use a 128 bit session key */
#define DCERPC_SCHANNEL_128            (1<<12)

/* check incoming pad bytes */
#define DCERPC_DEBUG_PAD_CHECK         (1<<13)

/* set LIBNDR_FLAG_REF_ALLOC flag when decoding NDR */
#define DCERPC_NDR_REF_ALLOC           (1<<14)

#define DCERPC_AUTH_OPTIONS    (DCERPC_SEAL|DCERPC_SIGN|DCERPC_SCHANNEL|DCERPC_AUTH_SPNEGO|DCERPC_AUTH_KRB5|DCERPC_AUTH_NTLM)

/* select spnego auth */
#define DCERPC_AUTH_SPNEGO             (1<<15)

/* select krb5 auth */
#define DCERPC_AUTH_KRB5               (1<<16)

#define DCERPC_SMB2                    (1<<17)

/* select NTLM auth */
#define DCERPC_AUTH_NTLM               (1<<18)

/* this triggers the DCERPC_PFC_FLAG_CONC_MPX flag in the bind request */
#define DCERPC_CONCURRENT_MULTIPLEX     (1<<19)

/* this triggers the DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN flag in the bind request */
#define DCERPC_HEADER_SIGNING          (1<<20)

/* use NDR64 transport */
#define DCERPC_NDR64                   (1<<21)


#endif /* __DCERPC_H__ */
