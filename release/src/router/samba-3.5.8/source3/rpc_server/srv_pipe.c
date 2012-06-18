/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Almost completely rewritten by (C) Jeremy Allison 2005.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*  this module apparently provides an implementation of DCE/RPC over a
 *  named pipe (IPC$ connection using SMBtrans).  details of DCE/RPC
 *  documentation are available (in on-line form) from the X-Open group.
 *
 *  this module should provide a level of abstraction between SMB
 *  and DCE/RPC, while minimising the amount of mallocs, unnecessary
 *  data copies, and network traffic.
 *
 */

#include "includes.h"
#include "../librpc/gen_ndr/ndr_schannel.h"
#include "../libcli/auth/schannel.h"
#include "../libcli/auth/spnego.h"

extern struct current_user current_user;

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

static void free_pipe_ntlmssp_auth_data(struct pipe_auth_data *auth)
{
	AUTH_NTLMSSP_STATE *a = auth->a_u.auth_ntlmssp_state;

	if (a) {
		auth_ntlmssp_end(&a);
	}
	auth->a_u.auth_ntlmssp_state = NULL;
}

static DATA_BLOB generic_session_key(void)
{
	return data_blob("SystemLibraryDTC", 16);
}

/*******************************************************************
 Generate the next PDU to be returned from the data in p->rdata. 
 Handle NTLMSSP.
 ********************************************************************/

static bool create_next_pdu_ntlmssp(pipes_struct *p)
{
	RPC_HDR_RESP hdr_resp;
	uint32 ss_padding_len = 0;
	uint32 data_space_available;
	uint32 data_len_left;
	uint32 data_len;
	NTSTATUS status;
	DATA_BLOB auth_blob;
	RPC_HDR_AUTH auth_info;
	uint8 auth_type, auth_level;
	AUTH_NTLMSSP_STATE *a = p->auth.a_u.auth_ntlmssp_state;

	/*
	 * If we're in the fault state, keep returning fault PDU's until
	 * the pipe gets closed. JRA.
	 */

	if(p->fault_state) {
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		return True;
	}

	memset((char *)&hdr_resp, '\0', sizeof(hdr_resp));

	/* Change the incoming request header to a response. */
	p->hdr.pkt_type = DCERPC_PKT_RESPONSE;

	/* Set up rpc header flags. */
	if (p->out_data.data_sent_length == 0) {
		p->hdr.flags = DCERPC_PFC_FLAG_FIRST;
	} else {
		p->hdr.flags = 0;
	}

	/*
	 * Work out how much we can fit in a single PDU.
	 */

	data_len_left = prs_offset(&p->out_data.rdata) - p->out_data.data_sent_length;

	/*
	 * Ensure there really is data left to send.
	 */

	if(!data_len_left) {
		DEBUG(0,("create_next_pdu_ntlmssp: no data left to send !\n"));
		return False;
	}

	data_space_available = RPC_MAX_PDU_FRAG_LEN - RPC_HEADER_LEN
		- RPC_HDR_RESP_LEN - RPC_HDR_AUTH_LEN - NTLMSSP_SIG_SIZE;

	/*
	 * The amount we send is the minimum of the available
	 * space and the amount left to send.
	 */

	data_len = MIN(data_len_left, data_space_available);

	/*
	 * Set up the alloc hint. This should be the data left to
	 * send.
	 */

	hdr_resp.alloc_hint = data_len_left;

	/*
	 * Work out if this PDU will be the last.
	 */

	if(p->out_data.data_sent_length + data_len >= prs_offset(&p->out_data.rdata)) {
		p->hdr.flags |= DCERPC_PFC_FLAG_LAST;
		if (data_len_left % 8) {
			ss_padding_len = 8 - (data_len_left % 8);
			DEBUG(10,("create_next_pdu_ntlmssp: adding sign/seal padding of %u\n",
				ss_padding_len ));
		}
	}

	/*
	 * Set up the header lengths.
	 */

	p->hdr.frag_len = RPC_HEADER_LEN + RPC_HDR_RESP_LEN +
			data_len + ss_padding_len +
			RPC_HDR_AUTH_LEN + NTLMSSP_SIG_SIZE;
	p->hdr.auth_len = NTLMSSP_SIG_SIZE;


	/*
	 * Init the parse struct to point at the outgoing
	 * data.
	 */

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/* Store the header in the data stream. */
	if(!smb_io_rpc_hdr("hdr", &p->hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("create_next_pdu_ntlmssp: failed to marshall RPC_HDR.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!smb_io_rpc_hdr_resp("resp", &hdr_resp, &p->out_data.frag, 0)) {
		DEBUG(0,("create_next_pdu_ntlmssp: failed to marshall RPC_HDR_RESP.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/* Copy the data into the PDU. */

	if(!prs_append_some_prs_data(&p->out_data.frag, &p->out_data.rdata,
				     p->out_data.data_sent_length, data_len)) {
		DEBUG(0,("create_next_pdu_ntlmssp: failed to copy %u bytes of data.\n", (unsigned int)data_len));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/* Copy the sign/seal padding data. */
	if (ss_padding_len) {
		char pad[8];

		memset(pad, '\0', 8);
		if (!prs_copy_data_in(&p->out_data.frag, pad,
				      ss_padding_len)) {
			DEBUG(0,("create_next_pdu_ntlmssp: failed to add %u bytes of pad data.\n",
					(unsigned int)ss_padding_len));
			prs_mem_free(&p->out_data.frag);
			return False;
		}
	}


	/* Now write out the auth header and null blob. */
	if (p->auth.auth_type == PIPE_AUTH_TYPE_NTLMSSP) {
		auth_type = DCERPC_AUTH_TYPE_NTLMSSP;
	} else {
		auth_type = DCERPC_AUTH_TYPE_SPNEGO;
	}
	if (p->auth.auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
		auth_level = DCERPC_AUTH_LEVEL_PRIVACY;
	} else {
		auth_level = DCERPC_AUTH_LEVEL_INTEGRITY;
	}

	init_rpc_hdr_auth(&auth_info, auth_type, auth_level, ss_padding_len, 1 /* context id. */);
	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, &p->out_data.frag,
				0)) {
		DEBUG(0,("create_next_pdu_ntlmssp: failed to marshall RPC_HDR_AUTH.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/* Generate the sign blob. */

	switch (p->auth.auth_level) {
		case DCERPC_AUTH_LEVEL_PRIVACY:
			/* Data portion is encrypted. */
			status = ntlmssp_seal_packet(
				a->ntlmssp_state,
				(uint8_t *)prs_data_p(&p->out_data.frag)
				+ RPC_HEADER_LEN + RPC_HDR_RESP_LEN,
				data_len + ss_padding_len,
				(unsigned char *)prs_data_p(&p->out_data.frag),
				(size_t)prs_offset(&p->out_data.frag),
				&auth_blob);
			if (!NT_STATUS_IS_OK(status)) {
				data_blob_free(&auth_blob);
				prs_mem_free(&p->out_data.frag);
				return False;
			}
			break;
		case DCERPC_AUTH_LEVEL_INTEGRITY:
			/* Data is signed. */
			status = ntlmssp_sign_packet(
				a->ntlmssp_state,
				(unsigned char *)prs_data_p(&p->out_data.frag)
				+ RPC_HEADER_LEN + RPC_HDR_RESP_LEN,
				data_len + ss_padding_len,
				(unsigned char *)prs_data_p(&p->out_data.frag),
				(size_t)prs_offset(&p->out_data.frag),
				&auth_blob);
			if (!NT_STATUS_IS_OK(status)) {
				data_blob_free(&auth_blob);
				prs_mem_free(&p->out_data.frag);
				return False;
			}
			break;
		default:
			prs_mem_free(&p->out_data.frag);
			return False;
	}

	/* Append the auth blob. */
	if (!prs_copy_data_in(&p->out_data.frag, (char *)auth_blob.data,
			      NTLMSSP_SIG_SIZE)) {
		DEBUG(0,("create_next_pdu_ntlmssp: failed to add %u bytes auth blob.\n",
				(unsigned int)NTLMSSP_SIG_SIZE));
		data_blob_free(&auth_blob);
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	data_blob_free(&auth_blob);

	/*
	 * Setup the counts for this PDU.
	 */

	p->out_data.data_sent_length += data_len;
	p->out_data.current_pdu_sent = 0;

	return True;
}

/*******************************************************************
 Generate the next PDU to be returned from the data in p->rdata. 
 Return an schannel authenticated fragment.
 ********************************************************************/

static bool create_next_pdu_schannel(pipes_struct *p)
{
	RPC_HDR_RESP hdr_resp;
	uint32 ss_padding_len = 0;
	uint32 data_len;
	uint32 data_space_available;
	uint32 data_len_left;
	uint32 data_pos;
	NTSTATUS status;

	/*
	 * If we're in the fault state, keep returning fault PDU's until
	 * the pipe gets closed. JRA.
	 */

	if(p->fault_state) {
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		return True;
	}

	memset((char *)&hdr_resp, '\0', sizeof(hdr_resp));

	/* Change the incoming request header to a response. */
	p->hdr.pkt_type = DCERPC_PKT_RESPONSE;

	/* Set up rpc header flags. */
	if (p->out_data.data_sent_length == 0) {
		p->hdr.flags = DCERPC_PFC_FLAG_FIRST;
	} else {
		p->hdr.flags = 0;
	}

	/*
	 * Work out how much we can fit in a single PDU.
	 */

	data_len_left = prs_offset(&p->out_data.rdata) - p->out_data.data_sent_length;

	/*
	 * Ensure there really is data left to send.
	 */

	if(!data_len_left) {
		DEBUG(0,("create_next_pdu_schannel: no data left to send !\n"));
		return False;
	}

	data_space_available = RPC_MAX_PDU_FRAG_LEN - RPC_HEADER_LEN
		- RPC_HDR_RESP_LEN - RPC_HDR_AUTH_LEN
		- RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN;

	/*
	 * The amount we send is the minimum of the available
	 * space and the amount left to send.
	 */

	data_len = MIN(data_len_left, data_space_available);

	/*
	 * Set up the alloc hint. This should be the data left to
	 * send.
	 */

	hdr_resp.alloc_hint = data_len_left;

	/*
	 * Work out if this PDU will be the last.
	 */

	if(p->out_data.data_sent_length + data_len >= prs_offset(&p->out_data.rdata)) {
		p->hdr.flags |= DCERPC_PFC_FLAG_LAST;
		if (data_len_left % 8) {
			ss_padding_len = 8 - (data_len_left % 8);
			DEBUG(10,("create_next_pdu_schannel: adding sign/seal padding of %u\n",
				ss_padding_len ));
		}
	}

	p->hdr.frag_len = RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len + ss_padding_len +
				RPC_HDR_AUTH_LEN + RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN;
	p->hdr.auth_len = RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN;

	/*
	 * Init the parse struct to point at the outgoing
	 * data.
	 */

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/* Store the header in the data stream. */
	if(!smb_io_rpc_hdr("hdr", &p->hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("create_next_pdu_schannel: failed to marshall RPC_HDR.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!smb_io_rpc_hdr_resp("resp", &hdr_resp, &p->out_data.frag, 0)) {
		DEBUG(0,("create_next_pdu_schannel: failed to marshall RPC_HDR_RESP.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/* Store the current offset. */
	data_pos = prs_offset(&p->out_data.frag);

	/* Copy the data into the PDU. */

	if(!prs_append_some_prs_data(&p->out_data.frag, &p->out_data.rdata,
				     p->out_data.data_sent_length, data_len)) {
		DEBUG(0,("create_next_pdu_schannel: failed to copy %u bytes of data.\n", (unsigned int)data_len));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/* Copy the sign/seal padding data. */
	if (ss_padding_len) {
		char pad[8];
		memset(pad, '\0', 8);
		if (!prs_copy_data_in(&p->out_data.frag, pad,
				      ss_padding_len)) {
			DEBUG(0,("create_next_pdu_schannel: failed to add %u bytes of pad data.\n", (unsigned int)ss_padding_len));
			prs_mem_free(&p->out_data.frag);
			return False;
		}
	}

	{
		/*
		 * Schannel processing.
		 */
		RPC_HDR_AUTH auth_info;
		DATA_BLOB blob;
		uint8_t *data;

		/* Check it's the type of reply we were expecting to decode */

		init_rpc_hdr_auth(&auth_info,
				DCERPC_AUTH_TYPE_SCHANNEL,
				p->auth.auth_level == DCERPC_AUTH_LEVEL_PRIVACY ?
					DCERPC_AUTH_LEVEL_PRIVACY : DCERPC_AUTH_LEVEL_INTEGRITY,
				ss_padding_len, 1);

		if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info,
					&p->out_data.frag, 0)) {
			DEBUG(0,("create_next_pdu_schannel: failed to marshall RPC_HDR_AUTH.\n"));
			prs_mem_free(&p->out_data.frag);
			return False;
		}

		data = (uint8_t *)prs_data_p(&p->out_data.frag) + data_pos;

		switch (p->auth.auth_level) {
		case DCERPC_AUTH_LEVEL_PRIVACY:
			status = netsec_outgoing_packet(p->auth.a_u.schannel_auth,
							talloc_tos(),
							true,
							data,
							data_len + ss_padding_len,
							&blob);
			break;
		case DCERPC_AUTH_LEVEL_INTEGRITY:
			status = netsec_outgoing_packet(p->auth.a_u.schannel_auth,
							talloc_tos(),
							false,
							data,
							data_len + ss_padding_len,
							&blob);
			break;
		default:
			status = NT_STATUS_INTERNAL_ERROR;
			break;
		}

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("create_next_pdu_schannel: failed to process packet: %s\n",
				nt_errstr(status)));
			prs_mem_free(&p->out_data.frag);
			return false;
		}

		/* Finally marshall the blob. */

		if (DEBUGLEVEL >= 10) {
			dump_NL_AUTH_SIGNATURE(talloc_tos(), &blob);
		}

		if (!prs_copy_data_in(&p->out_data.frag, (const char *)blob.data, blob.length)) {
			prs_mem_free(&p->out_data.frag);
			return false;
		}
	}

	/*
	 * Setup the counts for this PDU.
	 */

	p->out_data.data_sent_length += data_len;
	p->out_data.current_pdu_sent = 0;

	return True;
}

/*******************************************************************
 Generate the next PDU to be returned from the data in p->rdata. 
 No authentication done.
********************************************************************/

static bool create_next_pdu_noauth(pipes_struct *p)
{
	RPC_HDR_RESP hdr_resp;
	uint32 data_len;
	uint32 data_space_available;
	uint32 data_len_left;

	/*
	 * If we're in the fault state, keep returning fault PDU's until
	 * the pipe gets closed. JRA.
	 */

	if(p->fault_state) {
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		return True;
	}

	memset((char *)&hdr_resp, '\0', sizeof(hdr_resp));

	/* Change the incoming request header to a response. */
	p->hdr.pkt_type = DCERPC_PKT_RESPONSE;

	/* Set up rpc header flags. */
	if (p->out_data.data_sent_length == 0) {
		p->hdr.flags = DCERPC_PFC_FLAG_FIRST;
	} else {
		p->hdr.flags = 0;
	}

	/*
	 * Work out how much we can fit in a single PDU.
	 */

	data_len_left = prs_offset(&p->out_data.rdata) - p->out_data.data_sent_length;

	/*
	 * Ensure there really is data left to send.
	 */

	if(!data_len_left) {
		DEBUG(0,("create_next_pdu_noath: no data left to send !\n"));
		return False;
	}

	data_space_available = RPC_MAX_PDU_FRAG_LEN - RPC_HEADER_LEN
		- RPC_HDR_RESP_LEN;

	/*
	 * The amount we send is the minimum of the available
	 * space and the amount left to send.
	 */

	data_len = MIN(data_len_left, data_space_available);

	/*
	 * Set up the alloc hint. This should be the data left to
	 * send.
	 */

	hdr_resp.alloc_hint = data_len_left;

	/*
	 * Work out if this PDU will be the last.
	 */

	if(p->out_data.data_sent_length + data_len >= prs_offset(&p->out_data.rdata)) {
		p->hdr.flags |= DCERPC_PFC_FLAG_LAST;
	}

	/*
	 * Set up the header lengths.
	 */

	p->hdr.frag_len = RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len;
	p->hdr.auth_len = 0;

	/*
	 * Init the parse struct to point at the outgoing
	 * data.
	 */

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/* Store the header in the data stream. */
	if(!smb_io_rpc_hdr("hdr", &p->hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("create_next_pdu_noath: failed to marshall RPC_HDR.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!smb_io_rpc_hdr_resp("resp", &hdr_resp, &p->out_data.frag, 0)) {
		DEBUG(0,("create_next_pdu_noath: failed to marshall RPC_HDR_RESP.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/* Copy the data into the PDU. */

	if(!prs_append_some_prs_data(&p->out_data.frag, &p->out_data.rdata,
				     p->out_data.data_sent_length, data_len)) {
		DEBUG(0,("create_next_pdu_noauth: failed to copy %u bytes of data.\n", (unsigned int)data_len));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/*
	 * Setup the counts for this PDU.
	 */

	p->out_data.data_sent_length += data_len;
	p->out_data.current_pdu_sent = 0;

	return True;
}

/*******************************************************************
 Generate the next PDU to be returned from the data in p->rdata. 
********************************************************************/

bool create_next_pdu(pipes_struct *p)
{
	switch(p->auth.auth_level) {
		case DCERPC_AUTH_LEVEL_NONE:
		case DCERPC_AUTH_LEVEL_CONNECT:
			/* This is incorrect for auth level connect. Fixme. JRA */
			return create_next_pdu_noauth(p);

		default:
			switch(p->auth.auth_type) {
				case PIPE_AUTH_TYPE_NTLMSSP:
				case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
					return create_next_pdu_ntlmssp(p);
				case PIPE_AUTH_TYPE_SCHANNEL:
					return create_next_pdu_schannel(p);
				default:
					break;
			}
	}

	DEBUG(0,("create_next_pdu: invalid internal auth level %u / type %u",
			(unsigned int)p->auth.auth_level,
			(unsigned int)p->auth.auth_type));
	return False;
}

/*******************************************************************
 Process an NTLMSSP authentication response.
 If this function succeeds, the user has been authenticated
 and their domain, name and calling workstation stored in
 the pipe struct.
*******************************************************************/

static bool pipe_ntlmssp_verify_final(pipes_struct *p, DATA_BLOB *p_resp_blob)
{
	DATA_BLOB session_key, reply;
	NTSTATUS status;
	AUTH_NTLMSSP_STATE *a = p->auth.a_u.auth_ntlmssp_state;
	bool ret;

	DEBUG(5,("pipe_ntlmssp_verify_final: pipe %s checking user details\n",
		 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));

	ZERO_STRUCT(reply);

	/* this has to be done as root in order to verify the password */
	become_root();
	status = auth_ntlmssp_update(a, *p_resp_blob, &reply);
	unbecome_root();

	/* Don't generate a reply. */
	data_blob_free(&reply);

	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	/* Finally - if the pipe negotiated integrity (sign) or privacy (seal)
	   ensure the underlying NTLMSSP flags are also set. If not we should
	   refuse the bind. */

	if (p->auth.auth_level == DCERPC_AUTH_LEVEL_INTEGRITY) {
		if (!(a->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SIGN)) {
			DEBUG(0,("pipe_ntlmssp_verify_final: pipe %s : packet integrity requested "
				"but client declined signing.\n",
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			return False;
		}
	}
	if (p->auth.auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
		if (!(a->ntlmssp_state->neg_flags & NTLMSSP_NEGOTIATE_SEAL)) {
			DEBUG(0,("pipe_ntlmssp_verify_final: pipe %s : packet privacy requested "
				"but client declined sealing.\n",
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			return False;
		}
	}

	DEBUG(5, ("pipe_ntlmssp_verify_final: OK: user: %s domain: %s "
		  "workstation: %s\n", a->ntlmssp_state->user,
		  a->ntlmssp_state->domain, a->ntlmssp_state->workstation));

	if (a->server_info->ptok == NULL) {
		DEBUG(1,("Error: Authmodule failed to provide nt_user_token\n"));
		return False;
	}

	TALLOC_FREE(p->server_info);

	p->server_info = copy_serverinfo(p, a->server_info);
	if (p->server_info == NULL) {
		DEBUG(0, ("copy_serverinfo failed\n"));
		return false;
	}

	/*
	 * We're an authenticated bind over smb, so the session key needs to
	 * be set to "SystemLibraryDTC". Weird, but this is what Windows
	 * does. See the RPC-SAMBA3SESSIONKEY.
	 */

	session_key = generic_session_key();
	if (session_key.data == NULL) {
		return False;
	}

	ret = server_info_set_session_key(p->server_info, session_key);

	data_blob_free(&session_key);

	return True;
}

/*******************************************************************
 The switch table for the pipe names and the functions to handle them.
*******************************************************************/

struct rpc_table {
	struct {
		const char *clnt;
		const char *srv;
	} pipe;
	struct ndr_syntax_id rpc_interface;
	const struct api_struct *cmds;
	int n_cmds;
};

static struct rpc_table *rpc_lookup;
static int rpc_lookup_size;

/*******************************************************************
 This is the "stage3" NTLMSSP response after a bind request and reply.
*******************************************************************/

bool api_pipe_bind_auth3(pipes_struct *p, prs_struct *rpc_in_p)
{
	RPC_HDR_AUTH auth_info;
	uint32 pad = 0;
	DATA_BLOB blob;

	ZERO_STRUCT(blob);

	DEBUG(5,("api_pipe_bind_auth3: decode request. %d\n", __LINE__));

	if (p->hdr.auth_len == 0) {
		DEBUG(0,("api_pipe_bind_auth3: No auth field sent !\n"));
		goto err;
	}

	/* 4 bytes padding. */
	if (!prs_uint32("pad", rpc_in_p, 0, &pad)) {
		DEBUG(0,("api_pipe_bind_auth3: unmarshall of 4 byte pad failed.\n"));
		goto err;
	}

	/*
	 * Decode the authentication verifier response.
	 */

	if(!smb_io_rpc_hdr_auth("", &auth_info, rpc_in_p, 0)) {
		DEBUG(0,("api_pipe_bind_auth3: unmarshall of RPC_HDR_AUTH failed.\n"));
		goto err;
	}

	if (auth_info.auth_type != DCERPC_AUTH_TYPE_NTLMSSP) {
		DEBUG(0,("api_pipe_bind_auth3: incorrect auth type (%u).\n",
			(unsigned int)auth_info.auth_type ));
		return False;
	}

	blob = data_blob(NULL,p->hdr.auth_len);

	if (!prs_copy_data_out((char *)blob.data, rpc_in_p, p->hdr.auth_len)) {
		DEBUG(0,("api_pipe_bind_auth3: Failed to pull %u bytes - the response blob.\n",
			(unsigned int)p->hdr.auth_len ));
		goto err;
	}

	/*
	 * The following call actually checks the challenge/response data.
	 * for correctness against the given DOMAIN\user name.
	 */

	if (!pipe_ntlmssp_verify_final(p, &blob)) {
		goto err;
	}

	data_blob_free(&blob);

	p->pipe_bound = True;

	return True;

 err:

	data_blob_free(&blob);
	free_pipe_ntlmssp_auth_data(&p->auth);
	p->auth.a_u.auth_ntlmssp_state = NULL;

	return False;
}

/*******************************************************************
 Marshall a bind_nak pdu.
*******************************************************************/

static bool setup_bind_nak(pipes_struct *p)
{
	RPC_HDR nak_hdr;
	uint16 zero = 0;

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&p->out_data.rdata);

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/*
	 * Initialize a bind_nak header.
	 */

	init_rpc_hdr(&nak_hdr, DCERPC_PKT_BIND_NAK, DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST,
		p->hdr.call_id, RPC_HEADER_LEN + sizeof(uint16), 0);

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &nak_hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("setup_bind_nak: marshalling of RPC_HDR failed.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	/*
	 * Now add the reject reason.
	 */

	if(!prs_uint16("reject code", &p->out_data.frag, 0, &zero)) {
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	if (p->auth.auth_data_free_func) {
		(*p->auth.auth_data_free_func)(&p->auth);
	}
	p->auth.auth_level = DCERPC_AUTH_LEVEL_NONE;
	p->auth.auth_type = PIPE_AUTH_TYPE_NONE;
	p->pipe_bound = False;

	return True;
}

/*******************************************************************
 Marshall a fault pdu.
*******************************************************************/

bool setup_fault_pdu(pipes_struct *p, NTSTATUS status)
{
	RPC_HDR fault_hdr;
	RPC_HDR_RESP hdr_resp;
	RPC_HDR_FAULT fault_resp;

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&p->out_data.rdata);

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/*
	 * Initialize a fault header.
	 */

	init_rpc_hdr(&fault_hdr, DCERPC_PKT_FAULT, DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST | DCERPC_PFC_FLAG_DID_NOT_EXECUTE,
            p->hdr.call_id, RPC_HEADER_LEN + RPC_HDR_RESP_LEN + RPC_HDR_FAULT_LEN, 0);

	/*
	 * Initialize the HDR_RESP and FAULT parts of the PDU.
	 */

	memset((char *)&hdr_resp, '\0', sizeof(hdr_resp));

	fault_resp.status = status;
	fault_resp.reserved = 0;

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &fault_hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("setup_fault_pdu: marshalling of RPC_HDR failed.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!smb_io_rpc_hdr_resp("resp", &hdr_resp, &p->out_data.frag, 0)) {
		DEBUG(0,("setup_fault_pdu: failed to marshall RPC_HDR_RESP.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!smb_io_rpc_hdr_fault("fault", &fault_resp, &p->out_data.frag, 0)) {
		DEBUG(0,("setup_fault_pdu: failed to marshall RPC_HDR_FAULT.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	return True;
}

#if 0
/*******************************************************************
 Marshall a cancel_ack pdu.
 We should probably check the auth-verifier here.
*******************************************************************/

bool setup_cancel_ack_reply(pipes_struct *p, prs_struct *rpc_in_p)
{
	prs_struct outgoing_pdu;
	RPC_HDR ack_reply_hdr;

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&p->out_data.rdata);

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	prs_init_empty( &outgoing_pdu, p->mem_ctx, MARSHALL);
	prs_give_memory( &outgoing_pdu, (char *)p->out_data.current_pdu, sizeof(p->out_data.current_pdu), False);

	/*
	 * Initialize a cancel_ack header.
	 */

	init_rpc_hdr(&ack_reply_hdr, DCERPC_PKT_CANCEL_ACK, DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST,
			p->hdr.call_id, RPC_HEADER_LEN, 0);

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &ack_reply_hdr, &outgoing_pdu, 0)) {
		DEBUG(0,("setup_cancel_ack_reply: marshalling of RPC_HDR failed.\n"));
		prs_mem_free(&outgoing_pdu);
		return False;
	}

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_len = prs_offset(&outgoing_pdu);
	p->out_data.current_pdu_sent = 0;

	prs_mem_free(&outgoing_pdu);
	return True;
}
#endif

/*******************************************************************
 Ensure a bind request has the correct abstract & transfer interface.
 Used to reject unknown binds from Win2k.
*******************************************************************/

static bool check_bind_req(struct pipes_struct *p,
			   struct ndr_syntax_id* abstract,
			   struct ndr_syntax_id* transfer,
			   uint32 context_id)
{
	int i=0;
	struct pipe_rpc_fns *context_fns;

	DEBUG(3,("check_bind_req for %s\n",
		 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));

	/* we have to check all now since win2k introduced a new UUID on the lsaprpc pipe */

	for (i=0; i<rpc_lookup_size; i++) {
		DEBUGADD(10, ("checking %s\n", rpc_lookup[i].pipe.clnt));
		if (ndr_syntax_id_equal(
			    abstract, &rpc_lookup[i].rpc_interface)
		    && ndr_syntax_id_equal(
			    transfer, &ndr_transfer_syntax)) {
			break;
		}
	}

	if (i == rpc_lookup_size) {
		return false;
	}

	context_fns = SMB_MALLOC_P(struct pipe_rpc_fns);
	if (context_fns == NULL) {
		DEBUG(0,("check_bind_req: malloc() failed!\n"));
		return False;
	}

	context_fns->cmds = rpc_lookup[i].cmds;
	context_fns->n_cmds = rpc_lookup[i].n_cmds;
	context_fns->context_id = context_id;

	/* add to the list of open contexts */

	DLIST_ADD( p->contexts, context_fns );

	return True;
}

/*******************************************************************
 Register commands to an RPC pipe
*******************************************************************/

NTSTATUS rpc_srv_register(int version, const char *clnt, const char *srv,
			  const struct ndr_interface_table *iface,
			  const struct api_struct *cmds, int size)
{
        struct rpc_table *rpc_entry;

	if (!clnt || !srv || !cmds) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (version != SMB_RPC_INTERFACE_VERSION) {
		DEBUG(0,("Can't register rpc commands!\n"
			 "You tried to register a rpc module with SMB_RPC_INTERFACE_VERSION %d"
			 ", while this version of samba uses version %d!\n", 
			 version,SMB_RPC_INTERFACE_VERSION));
		return NT_STATUS_OBJECT_TYPE_MISMATCH;
	}

	/* TODO: 
	 *
	 * we still need to make sure that don't register the same commands twice!!!
	 * 
	 * --metze
	 */

        /* We use a temporary variable because this call can fail and 
           rpc_lookup will still be valid afterwards.  It could then succeed if
           called again later */
	rpc_lookup_size++;
        rpc_entry = SMB_REALLOC_ARRAY_KEEP_OLD_ON_ERROR(rpc_lookup, struct rpc_table, rpc_lookup_size);
        if (NULL == rpc_entry) {
                rpc_lookup_size--;
                DEBUG(0, ("rpc_pipe_register_commands: memory allocation failed\n"));
                return NT_STATUS_NO_MEMORY;
        } else {
                rpc_lookup = rpc_entry;
        }

        rpc_entry = rpc_lookup + (rpc_lookup_size - 1);
        ZERO_STRUCTP(rpc_entry);
        rpc_entry->pipe.clnt = SMB_STRDUP(clnt);
        rpc_entry->pipe.srv = SMB_STRDUP(srv);
	rpc_entry->rpc_interface = iface->syntax_id;
        rpc_entry->cmds = cmds;
        rpc_entry->n_cmds = size;

        return NT_STATUS_OK;
}

/**
 * Is a named pipe known?
 * @param[in] cli_filename	The pipe name requested by the client
 * @result			Do we want to serve this?
 */
bool is_known_pipename(const char *cli_filename, struct ndr_syntax_id *syntax)
{
	const char *pipename = cli_filename;
	int i;
	NTSTATUS status;

	if (strnequal(pipename, "\\PIPE\\", 6)) {
		pipename += 5;
	}

	if (*pipename == '\\') {
		pipename += 1;
	}

	if (lp_disable_spoolss() && strequal(pipename, "spoolss")) {
		DEBUG(10, ("refusing spoolss access\n"));
		return false;
	}

	for (i=0; i<rpc_lookup_size; i++) {
		if (strequal(pipename, rpc_lookup[i].pipe.clnt)) {
			*syntax = rpc_lookup[i].rpc_interface;
			return true;
		}
	}

	status = smb_probe_module("rpc", pipename);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("is_known_pipename: %s unknown\n", cli_filename));
		return false;
	}
	DEBUG(10, ("is_known_pipename: %s loaded dynamically\n", pipename));

	/*
	 * Scan the list again for the interface id
	 */

	for (i=0; i<rpc_lookup_size; i++) {
		if (strequal(pipename, rpc_lookup[i].pipe.clnt)) {
			*syntax = rpc_lookup[i].rpc_interface;
			return true;
		}
	}

	DEBUG(10, ("is_known_pipename: pipe %s did not register itself!\n",
		   pipename));

	return false;
}

/*******************************************************************
 Handle a SPNEGO krb5 bind auth.
*******************************************************************/

static bool pipe_spnego_auth_bind_kerberos(pipes_struct *p, prs_struct *rpc_in_p, RPC_HDR_AUTH *pauth_info,
		DATA_BLOB *psecblob, prs_struct *pout_auth)
{
	return False;
}

/*******************************************************************
 Handle the first part of a SPNEGO bind auth.
*******************************************************************/

static bool pipe_spnego_auth_bind_negotiate(pipes_struct *p, prs_struct *rpc_in_p,
					RPC_HDR_AUTH *pauth_info, prs_struct *pout_auth)
{
	DATA_BLOB blob;
	DATA_BLOB secblob;
	DATA_BLOB response;
	DATA_BLOB chal;
	char *OIDs[ASN1_MAX_OIDS];
        int i;
	NTSTATUS status;
        bool got_kerberos_mechanism = false;
	AUTH_NTLMSSP_STATE *a = NULL;
	RPC_HDR_AUTH auth_info;

	ZERO_STRUCT(secblob);
	ZERO_STRUCT(chal);
	ZERO_STRUCT(response);

	/* Grab the SPNEGO blob. */
	blob = data_blob(NULL,p->hdr.auth_len);

	if (!prs_copy_data_out((char *)blob.data, rpc_in_p, p->hdr.auth_len)) {
		DEBUG(0,("pipe_spnego_auth_bind_negotiate: Failed to pull %u bytes - the SPNEGO auth header.\n",
			(unsigned int)p->hdr.auth_len ));
		goto err;
	}

	if (blob.data[0] != ASN1_APPLICATION(0)) {
		goto err;
	}

	/* parse out the OIDs and the first sec blob */
	if (!parse_negTokenTarg(blob, OIDs, &secblob) ||
			OIDs[0] == NULL) {
		DEBUG(0,("pipe_spnego_auth_bind_negotiate: Failed to parse the security blob.\n"));
		goto err;
        }

	if (strcmp(OID_KERBEROS5, OIDs[0]) == 0 || strcmp(OID_KERBEROS5_OLD, OIDs[0]) == 0) {
		got_kerberos_mechanism = true;
	}

	for (i=0;OIDs[i];i++) {
		DEBUG(3,("pipe_spnego_auth_bind_negotiate: Got OID %s\n", OIDs[i]));
		TALLOC_FREE(OIDs[i]);
	}
	DEBUG(3,("pipe_spnego_auth_bind_negotiate: Got secblob of size %lu\n", (unsigned long)secblob.length));

	if ( got_kerberos_mechanism && ((lp_security()==SEC_ADS) || USE_KERBEROS_KEYTAB) ) {
		bool ret = pipe_spnego_auth_bind_kerberos(p, rpc_in_p, pauth_info, &secblob, pout_auth);
		data_blob_free(&secblob);
		data_blob_free(&blob);
		return ret;
	}

	if (p->auth.auth_type == PIPE_AUTH_TYPE_SPNEGO_NTLMSSP && p->auth.a_u.auth_ntlmssp_state) {
		/* Free any previous auth type. */
		free_pipe_ntlmssp_auth_data(&p->auth);
	}

	if (!got_kerberos_mechanism) {
		/* Initialize the NTLM engine. */
		status = auth_ntlmssp_start(&a);
		if (!NT_STATUS_IS_OK(status)) {
			goto err;
		}

		/*
		 * Pass the first security blob of data to it.
		 * This can return an error or NT_STATUS_MORE_PROCESSING_REQUIRED
		 * which means we need another packet to complete the bind.
		 */

		status = auth_ntlmssp_update(a, secblob, &chal);

		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DEBUG(3,("pipe_spnego_auth_bind_negotiate: auth_ntlmssp_update failed.\n"));
			goto err;
		}

		/* Generate the response blob we need for step 2 of the bind. */
		response = spnego_gen_auth_response(&chal, status, OID_NTLMSSP);
	} else {
		/*
		 * SPNEGO negotiate down to NTLMSSP. The subsequent
		 * code to process follow-up packets is not complete
		 * yet. JRA.
		 */
		response = spnego_gen_auth_response(NULL,
					NT_STATUS_MORE_PROCESSING_REQUIRED,
					OID_NTLMSSP);
	}

	/* Copy the blob into the pout_auth parse struct */
	init_rpc_hdr_auth(&auth_info, DCERPC_AUTH_TYPE_SPNEGO, pauth_info->auth_level, RPC_HDR_AUTH_LEN, 1);
	if(!smb_io_rpc_hdr_auth("", &auth_info, pout_auth, 0)) {
		DEBUG(0,("pipe_spnego_auth_bind_negotiate: marshalling of RPC_HDR_AUTH failed.\n"));
		goto err;
	}

	if (!prs_copy_data_in(pout_auth, (char *)response.data, response.length)) {
		DEBUG(0,("pipe_spnego_auth_bind_negotiate: marshalling of data blob failed.\n"));
		goto err;
	}

	p->auth.a_u.auth_ntlmssp_state = a;
	p->auth.auth_data_free_func = &free_pipe_ntlmssp_auth_data;
	p->auth.auth_type = PIPE_AUTH_TYPE_SPNEGO_NTLMSSP;

	data_blob_free(&blob);
	data_blob_free(&secblob);
	data_blob_free(&chal);
	data_blob_free(&response);

	/* We can't set pipe_bound True yet - we need an RPC_ALTER_CONTEXT response packet... */
	return True;

 err:

	data_blob_free(&blob);
	data_blob_free(&secblob);
	data_blob_free(&chal);
	data_blob_free(&response);

	p->auth.a_u.auth_ntlmssp_state = NULL;

	return False;
}

/*******************************************************************
 Handle the second part of a SPNEGO bind auth.
*******************************************************************/

static bool pipe_spnego_auth_bind_continue(pipes_struct *p, prs_struct *rpc_in_p,
					RPC_HDR_AUTH *pauth_info, prs_struct *pout_auth)
{
	RPC_HDR_AUTH auth_info;
	DATA_BLOB spnego_blob;
	DATA_BLOB auth_blob;
	DATA_BLOB auth_reply;
	DATA_BLOB response;
	AUTH_NTLMSSP_STATE *a = p->auth.a_u.auth_ntlmssp_state;

	ZERO_STRUCT(spnego_blob);
	ZERO_STRUCT(auth_blob);
	ZERO_STRUCT(auth_reply);
	ZERO_STRUCT(response);

	/*
	 * NB. If we've negotiated down from krb5 to NTLMSSP we'll currently
	 * fail here as 'a' == NULL.
	 */
	if (p->auth.auth_type != PIPE_AUTH_TYPE_SPNEGO_NTLMSSP || !a) {
		DEBUG(0,("pipe_spnego_auth_bind_continue: not in NTLMSSP auth state.\n"));
		goto err;
	}

	/* Grab the SPNEGO blob. */
	spnego_blob = data_blob(NULL,p->hdr.auth_len);

	if (!prs_copy_data_out((char *)spnego_blob.data, rpc_in_p, p->hdr.auth_len)) {
		DEBUG(0,("pipe_spnego_auth_bind_continue: Failed to pull %u bytes - the SPNEGO auth header.\n",
			(unsigned int)p->hdr.auth_len ));
		goto err;
	}

	if (spnego_blob.data[0] != ASN1_CONTEXT(1)) {
		DEBUG(0,("pipe_spnego_auth_bind_continue: invalid SPNEGO blob type.\n"));
		goto err;
	}

	if (!spnego_parse_auth(spnego_blob, &auth_blob)) {
		DEBUG(0,("pipe_spnego_auth_bind_continue: invalid SPNEGO blob.\n"));
		goto err;
	}

	/*
	 * The following call actually checks the challenge/response data.
	 * for correctness against the given DOMAIN\user name.
	 */

	if (!pipe_ntlmssp_verify_final(p, &auth_blob)) {
		goto err;
	}

	data_blob_free(&spnego_blob);
	data_blob_free(&auth_blob);

	/* Generate the spnego "accept completed" blob - no incoming data. */
	response = spnego_gen_auth_response(&auth_reply, NT_STATUS_OK, OID_NTLMSSP);

	/* Copy the blob into the pout_auth parse struct */
	init_rpc_hdr_auth(&auth_info, DCERPC_AUTH_TYPE_SPNEGO, pauth_info->auth_level, RPC_HDR_AUTH_LEN, 1);
	if(!smb_io_rpc_hdr_auth("", &auth_info, pout_auth, 0)) {
		DEBUG(0,("pipe_spnego_auth_bind_continue: marshalling of RPC_HDR_AUTH failed.\n"));
		goto err;
	}

	if (!prs_copy_data_in(pout_auth, (char *)response.data, response.length)) {
		DEBUG(0,("pipe_spnego_auth_bind_continue: marshalling of data blob failed.\n"));
		goto err;
	}

	data_blob_free(&auth_reply);
	data_blob_free(&response);

	p->pipe_bound = True;

	return True;

 err:

	data_blob_free(&spnego_blob);
	data_blob_free(&auth_blob);
	data_blob_free(&auth_reply);
	data_blob_free(&response);

	free_pipe_ntlmssp_auth_data(&p->auth);
	p->auth.a_u.auth_ntlmssp_state = NULL;

	return False;
}

/*******************************************************************
 Handle an schannel bind auth.
*******************************************************************/

static bool pipe_schannel_auth_bind(pipes_struct *p, prs_struct *rpc_in_p,
					RPC_HDR_AUTH *pauth_info, prs_struct *pout_auth)
{
	RPC_HDR_AUTH auth_info;
	struct NL_AUTH_MESSAGE neg;
	struct NL_AUTH_MESSAGE reply;
	bool ret;
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	DATA_BLOB session_key;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	blob = data_blob_const(prs_data_p(rpc_in_p) + prs_offset(rpc_in_p),
			       prs_data_size(rpc_in_p));

	ndr_err = ndr_pull_struct_blob(&blob, talloc_tos(), NULL, &neg,
			       (ndr_pull_flags_fn_t)ndr_pull_NL_AUTH_MESSAGE);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("pipe_schannel_auth_bind: Could not unmarshal SCHANNEL auth neg\n"));
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(NL_AUTH_MESSAGE, &neg);
	}

	if (!(neg.Flags & NL_FLAG_OEM_NETBIOS_COMPUTER_NAME)) {
		DEBUG(0,("pipe_schannel_auth_bind: Did not receive netbios computer name\n"));
		return false;
	}

	/*
	 * The neg.oem_netbios_computer.a key here must match the remote computer name
	 * given in the DOM_CLNT_SRV.uni_comp_name used on all netlogon pipe
	 * operations that use credentials.
	 */

	become_root();
	status = schannel_fetch_session_key(p,
					    neg.oem_netbios_computer.a,
					    &creds);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("pipe_schannel_auth_bind: Attempt to bind using schannel without successful serverauth2\n"));
		return False;
	}

	p->auth.a_u.schannel_auth = talloc(p, struct schannel_state);
	if (!p->auth.a_u.schannel_auth) {
		TALLOC_FREE(creds);
		return False;
	}

	p->auth.a_u.schannel_auth->state = SCHANNEL_STATE_START;
	p->auth.a_u.schannel_auth->seq_num = 0;
	p->auth.a_u.schannel_auth->initiator = false;
	p->auth.a_u.schannel_auth->creds = creds;

	/*
	 * JRA. Should we also copy the schannel session key into the pipe session key p->session_key
	 * here ? We do that for NTLMSSP, but the session key is already set up from the vuser
	 * struct of the person who opened the pipe. I need to test this further. JRA.
	 *
	 * VL. As we are mapping this to guest set the generic key
	 * "SystemLibraryDTC" key here. It's a bit difficult to test against
	 * W2k3, as it does not allow schannel binds against SAMR and LSA
	 * anymore.
	 */

	session_key = generic_session_key();
	if (session_key.data == NULL) {
		DEBUG(0, ("pipe_schannel_auth_bind: Could not alloc session"
			  " key\n"));
		return false;
	}

	ret = server_info_set_session_key(p->server_info, session_key);

	data_blob_free(&session_key);

	if (!ret) {
		DEBUG(0, ("server_info_set_session_key failed\n"));
		return false;
	}

	init_rpc_hdr_auth(&auth_info, DCERPC_AUTH_TYPE_SCHANNEL, pauth_info->auth_level, RPC_HDR_AUTH_LEN, 1);
	if(!smb_io_rpc_hdr_auth("", &auth_info, pout_auth, 0)) {
		DEBUG(0,("pipe_schannel_auth_bind: marshalling of RPC_HDR_AUTH failed.\n"));
		return False;
	}

	/*** SCHANNEL verifier ***/

	reply.MessageType			= NL_NEGOTIATE_RESPONSE;
	reply.Flags				= 0;
	reply.Buffer.dummy			= 5; /* ??? actually I don't think
						      * this has any meaning
						      * here - gd */

	ndr_err = ndr_push_struct_blob(&blob, talloc_tos(), NULL, &reply,
		       (ndr_push_flags_fn_t)ndr_push_NL_AUTH_MESSAGE);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("Failed to marshall NL_AUTH_MESSAGE.\n"));
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(NL_AUTH_MESSAGE, &reply);
	}

	if (!prs_copy_data_in(pout_auth, (const char *)blob.data, blob.length)) {
		return false;
	}

	DEBUG(10,("pipe_schannel_auth_bind: schannel auth: domain [%s] myname [%s]\n",
		neg.oem_netbios_domain.a, neg.oem_netbios_computer.a));

	/* We're finished with this bind - no more packets. */
	p->auth.auth_data_free_func = NULL;
	p->auth.auth_type = PIPE_AUTH_TYPE_SCHANNEL;

	p->pipe_bound = True;

	return True;
}

/*******************************************************************
 Handle an NTLMSSP bind auth.
*******************************************************************/

static bool pipe_ntlmssp_auth_bind(pipes_struct *p, prs_struct *rpc_in_p,
					RPC_HDR_AUTH *pauth_info, prs_struct *pout_auth)
{
	RPC_HDR_AUTH auth_info;
        DATA_BLOB blob;
	DATA_BLOB response;
        NTSTATUS status;
	AUTH_NTLMSSP_STATE *a = NULL;

	ZERO_STRUCT(blob);
	ZERO_STRUCT(response);

	/* Grab the NTLMSSP blob. */
	blob = data_blob(NULL,p->hdr.auth_len);

	if (!prs_copy_data_out((char *)blob.data, rpc_in_p, p->hdr.auth_len)) {
		DEBUG(0,("pipe_ntlmssp_auth_bind: Failed to pull %u bytes - the NTLM auth header.\n",
			(unsigned int)p->hdr.auth_len ));
		goto err;
	}

	if (strncmp((char *)blob.data, "NTLMSSP", 7) != 0) {
		DEBUG(0,("pipe_ntlmssp_auth_bind: Failed to read NTLMSSP in blob\n"));
                goto err;
        }

	/* We have an NTLMSSP blob. */
	status = auth_ntlmssp_start(&a);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("pipe_ntlmssp_auth_bind: auth_ntlmssp_start failed: %s\n",
			nt_errstr(status) ));
		goto err;
	}

	status = auth_ntlmssp_update(a, blob, &response);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		DEBUG(0,("pipe_ntlmssp_auth_bind: auth_ntlmssp_update failed: %s\n",
			nt_errstr(status) ));
		goto err;
	}

	data_blob_free(&blob);

	/* Copy the blob into the pout_auth parse struct */
	init_rpc_hdr_auth(&auth_info, DCERPC_AUTH_TYPE_NTLMSSP, pauth_info->auth_level, RPC_HDR_AUTH_LEN, 1);
	if(!smb_io_rpc_hdr_auth("", &auth_info, pout_auth, 0)) {
		DEBUG(0,("pipe_ntlmssp_auth_bind: marshalling of RPC_HDR_AUTH failed.\n"));
		goto err;
	}

	if (!prs_copy_data_in(pout_auth, (char *)response.data, response.length)) {
		DEBUG(0,("pipe_ntlmssp_auth_bind: marshalling of data blob failed.\n"));
		goto err;
	}

	p->auth.a_u.auth_ntlmssp_state = a;
	p->auth.auth_data_free_func = &free_pipe_ntlmssp_auth_data;
	p->auth.auth_type = PIPE_AUTH_TYPE_NTLMSSP;

	data_blob_free(&blob);
	data_blob_free(&response);

	DEBUG(10,("pipe_ntlmssp_auth_bind: NTLMSSP auth started\n"));

	/* We can't set pipe_bound True yet - we need an DCERPC_PKT_AUTH3 response packet... */
	return True;

  err:

	data_blob_free(&blob);
	data_blob_free(&response);

	free_pipe_ntlmssp_auth_data(&p->auth);
	p->auth.a_u.auth_ntlmssp_state = NULL;
	return False;
}

/*******************************************************************
 Respond to a pipe bind request.
*******************************************************************/

bool api_pipe_bind_req(pipes_struct *p, prs_struct *rpc_in_p)
{
	RPC_HDR_BA hdr_ba;
	RPC_HDR_RB hdr_rb;
	RPC_HDR_AUTH auth_info;
	uint16 assoc_gid;
	fstring ack_pipe_name;
	prs_struct out_hdr_ba;
	prs_struct out_auth;
	int i = 0;
	int auth_len = 0;
	unsigned int auth_type = DCERPC_AUTH_TYPE_NONE;

	/* No rebinds on a bound pipe - use alter context. */
	if (p->pipe_bound) {
		DEBUG(2,("api_pipe_bind_req: rejecting bind request on bound "
			 "pipe %s.\n",
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
		return setup_bind_nak(p);
	}

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/* 
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	/*
	 * Setup the memory to marshall the ba header, and the
	 * auth footers.
	 */

	if(!prs_init(&out_hdr_ba, 1024, p->mem_ctx, MARSHALL)) {
		DEBUG(0,("api_pipe_bind_req: malloc out_hdr_ba failed.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!prs_init(&out_auth, 1024, p->mem_ctx, MARSHALL)) {
		DEBUG(0,("api_pipe_bind_req: malloc out_auth failed.\n"));
		prs_mem_free(&p->out_data.frag);
		prs_mem_free(&out_hdr_ba);
		return False;
	}

	DEBUG(5,("api_pipe_bind_req: decode request. %d\n", __LINE__));

	ZERO_STRUCT(hdr_rb);

	/* decode the bind request */

	if(!smb_io_rpc_hdr_rb("", &hdr_rb, rpc_in_p, 0))  {
		DEBUG(0,("api_pipe_bind_req: unable to unmarshall RPC_HDR_RB "
			 "struct.\n"));
		goto err_exit;
	}

	if (hdr_rb.num_contexts == 0) {
		DEBUG(0, ("api_pipe_bind_req: no rpc contexts around\n"));
		goto err_exit;
	}

	/*
	 * Try and find the correct pipe name to ensure
	 * that this is a pipe name we support.
	 */

	for (i = 0; i < rpc_lookup_size; i++) {
		if (ndr_syntax_id_equal(&rpc_lookup[i].rpc_interface,
					&hdr_rb.rpc_context[0].abstract)) {
			DEBUG(3, ("api_pipe_bind_req: \\PIPE\\%s -> \\PIPE\\%s\n",
				rpc_lookup[i].pipe.clnt, rpc_lookup[i].pipe.srv));
			break;
		}
	}

	if (i == rpc_lookup_size) {
		NTSTATUS status;

		status = smb_probe_module(
			"rpc", get_pipe_name_from_syntax(
				talloc_tos(),
				&hdr_rb.rpc_context[0].abstract));

		if (NT_STATUS_IS_ERR(status)) {
                       DEBUG(3,("api_pipe_bind_req: Unknown pipe name %s in bind request.\n",
                                get_pipe_name_from_syntax(
					talloc_tos(),
					&hdr_rb.rpc_context[0].abstract)));
			prs_mem_free(&p->out_data.frag);
			prs_mem_free(&out_hdr_ba);
			prs_mem_free(&out_auth);

			return setup_bind_nak(p);
                }

                for (i = 0; i < rpc_lookup_size; i++) {
                       if (strequal(rpc_lookup[i].pipe.clnt,
				    get_pipe_name_from_syntax(talloc_tos(),
							      &p->syntax))) {
                               DEBUG(3, ("api_pipe_bind_req: \\PIPE\\%s -> \\PIPE\\%s\n",
                                         rpc_lookup[i].pipe.clnt, rpc_lookup[i].pipe.srv));
                               break;
                       }
                }

		if (i == rpc_lookup_size) {
			DEBUG(0, ("module %s doesn't provide functions for "
				  "pipe %s!\n",
				  get_pipe_name_from_syntax(talloc_tos(),
							    &p->syntax),
				  get_pipe_name_from_syntax(talloc_tos(),
							    &p->syntax)));
			goto err_exit;
		}
	}

	/* name has to be \PIPE\xxxxx */
	fstrcpy(ack_pipe_name, "\\PIPE\\");
	fstrcat(ack_pipe_name, rpc_lookup[i].pipe.srv);

	DEBUG(5,("api_pipe_bind_req: make response. %d\n", __LINE__));

	/*
	 * Check if this is an authenticated bind request.
	 */

	if (p->hdr.auth_len) {
		/* 
		 * Decode the authentication verifier.
		 */

		if(!smb_io_rpc_hdr_auth("", &auth_info, rpc_in_p, 0)) {
			DEBUG(0,("api_pipe_bind_req: unable to unmarshall RPC_HDR_AUTH struct.\n"));
			goto err_exit;
		}

		auth_type = auth_info.auth_type;

		/* Work out if we have to sign or seal etc. */
		switch (auth_info.auth_level) {
			case DCERPC_AUTH_LEVEL_INTEGRITY:
				p->auth.auth_level = DCERPC_AUTH_LEVEL_INTEGRITY;
				break;
			case DCERPC_AUTH_LEVEL_PRIVACY:
				p->auth.auth_level = DCERPC_AUTH_LEVEL_PRIVACY;
				break;
			default:
				DEBUG(0,("api_pipe_bind_req: unexpected auth level (%u).\n",
					(unsigned int)auth_info.auth_level ));
				goto err_exit;
		}
	} else {
		ZERO_STRUCT(auth_info);
	}

	assoc_gid = hdr_rb.bba.assoc_gid ? hdr_rb.bba.assoc_gid : 0x53f0;

	switch(auth_type) {
		case DCERPC_AUTH_TYPE_NTLMSSP:
			if (!pipe_ntlmssp_auth_bind(p, rpc_in_p, &auth_info, &out_auth)) {
				goto err_exit;
			}
			assoc_gid = 0x7a77;
			break;

		case DCERPC_AUTH_TYPE_SCHANNEL:
			if (!pipe_schannel_auth_bind(p, rpc_in_p, &auth_info, &out_auth)) {
				goto err_exit;
			}
			break;

		case DCERPC_AUTH_TYPE_SPNEGO:
			if (!pipe_spnego_auth_bind_negotiate(p, rpc_in_p, &auth_info, &out_auth)) {
				goto err_exit;
			}
			break;

		case DCERPC_AUTH_TYPE_NONE:
			/* Unauthenticated bind request. */
			/* We're finished - no more packets. */
			p->auth.auth_type = PIPE_AUTH_TYPE_NONE;
			/* We must set the pipe auth_level here also. */
			p->auth.auth_level = DCERPC_AUTH_LEVEL_NONE;
			p->pipe_bound = True;
			/* The session key was initialized from the SMB
			 * session in make_internal_rpc_pipe_p */
			break;

		default:
			DEBUG(0,("api_pipe_bind_req: unknown auth type %x requested.\n", auth_type ));
			goto err_exit;
	}

	/*
	 * Create the bind response struct.
	 */

	/* If the requested abstract synt uuid doesn't match our client pipe,
		reject the bind_ack & set the transfer interface synt to all 0's,
		ver 0 (observed when NT5 attempts to bind to abstract interfaces
		unknown to NT4)
		Needed when adding entries to a DACL from NT5 - SK */

	if(check_bind_req(p, &hdr_rb.rpc_context[0].abstract, &hdr_rb.rpc_context[0].transfer[0],
				hdr_rb.rpc_context[0].context_id )) {
		init_rpc_hdr_ba(&hdr_ba,
	                RPC_MAX_PDU_FRAG_LEN,
	                RPC_MAX_PDU_FRAG_LEN,
	                assoc_gid,
	                ack_pipe_name,
	                0x1, 0x0, 0x0,
	                &hdr_rb.rpc_context[0].transfer[0]);
	} else {
		/* Rejection reason: abstract syntax not supported */
		init_rpc_hdr_ba(&hdr_ba, RPC_MAX_PDU_FRAG_LEN,
					RPC_MAX_PDU_FRAG_LEN, assoc_gid,
					ack_pipe_name, 0x1, 0x2, 0x1,
					&null_ndr_syntax_id);
		p->pipe_bound = False;
	}

	/*
	 * and marshall it.
	 */

	if(!smb_io_rpc_hdr_ba("", &hdr_ba, &out_hdr_ba, 0)) {
		DEBUG(0,("api_pipe_bind_req: marshalling of RPC_HDR_BA failed.\n"));
		goto err_exit;
	}

	/*
	 * Create the header, now we know the length.
	 */

	if (prs_offset(&out_auth)) {
		auth_len = prs_offset(&out_auth) - RPC_HDR_AUTH_LEN;
	}

	init_rpc_hdr(&p->hdr, DCERPC_PKT_BIND_ACK, DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST,
			p->hdr.call_id,
			RPC_HEADER_LEN + prs_offset(&out_hdr_ba) + prs_offset(&out_auth),
			auth_len);

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &p->hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("api_pipe_bind_req: marshalling of RPC_HDR failed.\n"));
		goto err_exit;
	}

	/*
	 * Now add the RPC_HDR_BA and any auth needed.
	 */

	if(!prs_append_prs_data(&p->out_data.frag, &out_hdr_ba)) {
		DEBUG(0,("api_pipe_bind_req: append of RPC_HDR_BA failed.\n"));
		goto err_exit;
	}

	if (auth_len && !prs_append_prs_data( &p->out_data.frag, &out_auth)) {
		DEBUG(0,("api_pipe_bind_req: append of auth info failed.\n"));
		goto err_exit;
	}

	/*
	 * Setup the lengths for the initial reply.
	 */

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	prs_mem_free(&out_hdr_ba);
	prs_mem_free(&out_auth);

	return True;

  err_exit:

	prs_mem_free(&p->out_data.frag);
	prs_mem_free(&out_hdr_ba);
	prs_mem_free(&out_auth);
	return setup_bind_nak(p);
}

/****************************************************************************
 Deal with an alter context call. Can be third part of 3 leg auth request for
 SPNEGO calls.
****************************************************************************/

bool api_pipe_alter_context(pipes_struct *p, prs_struct *rpc_in_p)
{
	RPC_HDR_BA hdr_ba;
	RPC_HDR_RB hdr_rb;
	RPC_HDR_AUTH auth_info;
	uint16 assoc_gid;
	fstring ack_pipe_name;
	prs_struct out_hdr_ba;
	prs_struct out_auth;
	int auth_len = 0;

	prs_init_empty(&p->out_data.frag, p->mem_ctx, MARSHALL);

	/* 
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	/*
	 * Setup the memory to marshall the ba header, and the
	 * auth footers.
	 */

	if(!prs_init(&out_hdr_ba, 1024, p->mem_ctx, MARSHALL)) {
		DEBUG(0,("api_pipe_alter_context: malloc out_hdr_ba failed.\n"));
		prs_mem_free(&p->out_data.frag);
		return False;
	}

	if(!prs_init(&out_auth, 1024, p->mem_ctx, MARSHALL)) {
		DEBUG(0,("api_pipe_alter_context: malloc out_auth failed.\n"));
		prs_mem_free(&p->out_data.frag);
		prs_mem_free(&out_hdr_ba);
		return False;
	}

	ZERO_STRUCT(hdr_rb);

	DEBUG(5,("api_pipe_alter_context: decode request. %d\n", __LINE__));

	/* decode the alter context request */
	if(!smb_io_rpc_hdr_rb("", &hdr_rb, rpc_in_p, 0))  {
		DEBUG(0,("api_pipe_alter_context: unable to unmarshall RPC_HDR_RB struct.\n"));
		goto err_exit;
	}

	/* secondary address CAN be NULL
	 * as the specs say it's ignored.
	 * It MUST be NULL to have the spoolss working.
	 */
	fstrcpy(ack_pipe_name,"");

	DEBUG(5,("api_pipe_alter_context: make response. %d\n", __LINE__));

	/*
	 * Check if this is an authenticated alter context request.
	 */

	if (p->hdr.auth_len != 0) {
		/* 
		 * Decode the authentication verifier.
		 */

		if(!smb_io_rpc_hdr_auth("", &auth_info, rpc_in_p, 0)) {
			DEBUG(0,("api_pipe_alter_context: unable to unmarshall RPC_HDR_AUTH struct.\n"));
			goto err_exit;
		}

		/*
		 * Currently only the SPNEGO auth type uses the alter ctx
		 * response in place of the NTLMSSP auth3 type.
		 */

		if (auth_info.auth_type == DCERPC_AUTH_TYPE_SPNEGO) {
			/* We can only finish if the pipe is unbound. */
			if (!p->pipe_bound) {
				if (!pipe_spnego_auth_bind_continue(p, rpc_in_p, &auth_info, &out_auth)) {
					goto err_exit;
				}
			} else {
				goto err_exit;
			}
		}
	} else {
		ZERO_STRUCT(auth_info);
	}

	assoc_gid = hdr_rb.bba.assoc_gid ? hdr_rb.bba.assoc_gid : 0x53f0;

	/*
	 * Create the bind response struct.
	 */

	/* If the requested abstract synt uuid doesn't match our client pipe,
		reject the bind_ack & set the transfer interface synt to all 0's,
		ver 0 (observed when NT5 attempts to bind to abstract interfaces
		unknown to NT4)
		Needed when adding entries to a DACL from NT5 - SK */

	if(check_bind_req(p, &hdr_rb.rpc_context[0].abstract, &hdr_rb.rpc_context[0].transfer[0],
				hdr_rb.rpc_context[0].context_id )) {
		init_rpc_hdr_ba(&hdr_ba,
	                RPC_MAX_PDU_FRAG_LEN,
	                RPC_MAX_PDU_FRAG_LEN,
	                assoc_gid,
	                ack_pipe_name,
	                0x1, 0x0, 0x0,
	                &hdr_rb.rpc_context[0].transfer[0]);
	} else {
		/* Rejection reason: abstract syntax not supported */
		init_rpc_hdr_ba(&hdr_ba, RPC_MAX_PDU_FRAG_LEN,
					RPC_MAX_PDU_FRAG_LEN, assoc_gid,
					ack_pipe_name, 0x1, 0x2, 0x1,
					&null_ndr_syntax_id);
		p->pipe_bound = False;
	}

	/*
	 * and marshall it.
	 */

	if(!smb_io_rpc_hdr_ba("", &hdr_ba, &out_hdr_ba, 0)) {
		DEBUG(0,("api_pipe_alter_context: marshalling of RPC_HDR_BA failed.\n"));
		goto err_exit;
	}

	/*
	 * Create the header, now we know the length.
	 */

	if (prs_offset(&out_auth)) {
		auth_len = prs_offset(&out_auth) - RPC_HDR_AUTH_LEN;
	}

	init_rpc_hdr(&p->hdr, DCERPC_PKT_ALTER_RESP, DCERPC_PFC_FLAG_FIRST | DCERPC_PFC_FLAG_LAST,
			p->hdr.call_id,
			RPC_HEADER_LEN + prs_offset(&out_hdr_ba) + prs_offset(&out_auth),
			auth_len);

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &p->hdr, &p->out_data.frag, 0)) {
		DEBUG(0,("api_pipe_alter_context: marshalling of RPC_HDR failed.\n"));
		goto err_exit;
	}

	/*
	 * Now add the RPC_HDR_BA and any auth needed.
	 */

	if(!prs_append_prs_data(&p->out_data.frag, &out_hdr_ba)) {
		DEBUG(0,("api_pipe_alter_context: append of RPC_HDR_BA failed.\n"));
		goto err_exit;
	}

	if (auth_len && !prs_append_prs_data(&p->out_data.frag, &out_auth)) {
		DEBUG(0,("api_pipe_alter_context: append of auth info failed.\n"));
		goto err_exit;
	}

	/*
	 * Setup the lengths for the initial reply.
	 */

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	prs_mem_free(&out_hdr_ba);
	prs_mem_free(&out_auth);

	return True;

  err_exit:

	prs_mem_free(&p->out_data.frag);
	prs_mem_free(&out_hdr_ba);
	prs_mem_free(&out_auth);
	return setup_bind_nak(p);
}

/****************************************************************************
 Deal with NTLMSSP sign & seal processing on an RPC request.
****************************************************************************/

bool api_pipe_ntlmssp_auth_process(pipes_struct *p, prs_struct *rpc_in,
					uint32 *p_ss_padding_len, NTSTATUS *pstatus)
{
	RPC_HDR_AUTH auth_info;
	uint32 auth_len = p->hdr.auth_len;
	uint32 save_offset = prs_offset(rpc_in);
	AUTH_NTLMSSP_STATE *a = p->auth.a_u.auth_ntlmssp_state;
	unsigned char *data = NULL;
	size_t data_len;
	unsigned char *full_packet_data = NULL;
	size_t full_packet_data_len;
	DATA_BLOB auth_blob;

	*pstatus = NT_STATUS_OK;

	if (p->auth.auth_level == DCERPC_AUTH_LEVEL_NONE || p->auth.auth_level == DCERPC_AUTH_LEVEL_CONNECT) {
		return True;
	}

	if (!a) {
		*pstatus = NT_STATUS_INVALID_PARAMETER;
		return False;
	}

	/* Ensure there's enough data for an authenticated request. */
	if ((auth_len > RPC_MAX_SIGN_SIZE) ||
			(RPC_HEADER_LEN + RPC_HDR_REQ_LEN + RPC_HDR_AUTH_LEN + auth_len > p->hdr.frag_len)) {
		DEBUG(0,("api_pipe_ntlmssp_auth_process: auth_len %u is too large.\n",
			(unsigned int)auth_len ));
		*pstatus = NT_STATUS_INVALID_PARAMETER;
		return False;
	}

	/*
	 * We need the full packet data + length (minus auth stuff) as well as the packet data + length
	 * after the RPC header. 
 	 * We need to pass in the full packet (minus auth len) to the NTLMSSP sign and check seal
	 * functions as NTLMv2 checks the rpc headers also.
	 */

	data = (unsigned char *)(prs_data_p(rpc_in) + RPC_HDR_REQ_LEN);
	data_len = (size_t)(p->hdr.frag_len - RPC_HEADER_LEN - RPC_HDR_REQ_LEN - RPC_HDR_AUTH_LEN - auth_len);

	full_packet_data = p->in_data.current_in_pdu;
	full_packet_data_len = p->hdr.frag_len - auth_len;

	/* Pull the auth header and the following data into a blob. */
	if(!prs_set_offset(rpc_in, RPC_HDR_REQ_LEN + data_len)) {
		DEBUG(0,("api_pipe_ntlmssp_auth_process: cannot move offset to %u.\n",
			(unsigned int)RPC_HDR_REQ_LEN + (unsigned int)data_len ));
		*pstatus = NT_STATUS_INVALID_PARAMETER;
		return False;
	}

	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, rpc_in, 0)) {
		DEBUG(0,("api_pipe_ntlmssp_auth_process: failed to unmarshall RPC_HDR_AUTH.\n"));
		*pstatus = NT_STATUS_INVALID_PARAMETER;
		return False;
	}

	auth_blob.data = (unsigned char *)prs_data_p(rpc_in) + prs_offset(rpc_in);
	auth_blob.length = auth_len;

	switch (p->auth.auth_level) {
		case DCERPC_AUTH_LEVEL_PRIVACY:
			/* Data is encrypted. */
			*pstatus = ntlmssp_unseal_packet(a->ntlmssp_state,
							data, data_len,
							full_packet_data,
							full_packet_data_len,
							&auth_blob);
			if (!NT_STATUS_IS_OK(*pstatus)) {
				return False;
			}
			break;
		case DCERPC_AUTH_LEVEL_INTEGRITY:
			/* Data is signed. */
			*pstatus = ntlmssp_check_packet(a->ntlmssp_state,
							data, data_len,
							full_packet_data,
							full_packet_data_len,
							&auth_blob);
			if (!NT_STATUS_IS_OK(*pstatus)) {
				return False;
			}
			break;
		default:
			*pstatus = NT_STATUS_INVALID_PARAMETER;
			return False;
	}

	/*
	 * Return the current pointer to the data offset.
	 */

	if(!prs_set_offset(rpc_in, save_offset)) {
		DEBUG(0,("api_pipe_auth_process: failed to set offset back to %u\n",
			(unsigned int)save_offset ));
		*pstatus = NT_STATUS_INVALID_PARAMETER;
		return False;
	}

	/*
	 * Remember the padding length. We must remove it from the real data
	 * stream once the sign/seal is done.
	 */

	*p_ss_padding_len = auth_info.auth_pad_len;

	return True;
}

/****************************************************************************
 Deal with schannel processing on an RPC request.
****************************************************************************/

bool api_pipe_schannel_process(pipes_struct *p, prs_struct *rpc_in, uint32 *p_ss_padding_len)
{
	uint32 data_len;
	uint32 auth_len;
	uint32 save_offset = prs_offset(rpc_in);
	RPC_HDR_AUTH auth_info;
	DATA_BLOB blob;
	NTSTATUS status;
	uint8_t *data;

	auth_len = p->hdr.auth_len;

	if (auth_len < RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN ||
			auth_len > RPC_HEADER_LEN +
					RPC_HDR_REQ_LEN +
					RPC_HDR_AUTH_LEN +
					auth_len) {
		DEBUG(0,("Incorrect auth_len %u.\n", (unsigned int)auth_len ));
		return False;
	}

	/*
	 * The following is that length of the data we must verify or unseal.
	 * This doesn't include the RPC headers or the auth_len or the RPC_HDR_AUTH_LEN
	 * preceeding the auth_data.
	 */

	if (p->hdr.frag_len < RPC_HEADER_LEN + RPC_HDR_REQ_LEN + RPC_HDR_AUTH_LEN + auth_len) {
		DEBUG(0,("Incorrect frag %u, auth %u.\n",
			(unsigned int)p->hdr.frag_len,
			(unsigned int)auth_len ));
		return False;
	}

	data_len = p->hdr.frag_len - RPC_HEADER_LEN - RPC_HDR_REQ_LEN - 
		RPC_HDR_AUTH_LEN - auth_len;

	DEBUG(5,("data %d auth %d\n", data_len, auth_len));

	if(!prs_set_offset(rpc_in, RPC_HDR_REQ_LEN + data_len)) {
		DEBUG(0,("cannot move offset to %u.\n",
			 (unsigned int)RPC_HDR_REQ_LEN + data_len ));
		return False;
	}

	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, rpc_in, 0)) {
		DEBUG(0,("failed to unmarshall RPC_HDR_AUTH.\n"));
		return False;
	}

	if (auth_info.auth_type != DCERPC_AUTH_TYPE_SCHANNEL) {
		DEBUG(0,("Invalid auth info %d on schannel\n",
			 auth_info.auth_type));
		return False;
	}

	blob = data_blob_const(prs_data_p(rpc_in) + prs_offset(rpc_in), auth_len);

	if (DEBUGLEVEL >= 10) {
		dump_NL_AUTH_SIGNATURE(talloc_tos(), &blob);
	}

	data = (uint8_t *)prs_data_p(rpc_in)+RPC_HDR_REQ_LEN;

	switch (auth_info.auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = netsec_incoming_packet(p->auth.a_u.schannel_auth,
						talloc_tos(),
						true,
						data,
						data_len,
						&blob);
		break;
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = netsec_incoming_packet(p->auth.a_u.schannel_auth,
						talloc_tos(),
						false,
						data,
						data_len,
						&blob);
		break;
	default:
		status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("failed to unseal packet: %s\n", nt_errstr(status)));
		return false;
	}

	/*
	 * Return the current pointer to the data offset.
	 */

	if(!prs_set_offset(rpc_in, save_offset)) {
		DEBUG(0,("failed to set offset back to %u\n",
			 (unsigned int)save_offset ));
		return False;
	}

	/*
	 * Remember the padding length. We must remove it from the real data
	 * stream once the sign/seal is done.
	 */

	*p_ss_padding_len = auth_info.auth_pad_len;

	return True;
}

/****************************************************************************
 Find the set of RPC functions associated with this context_id
****************************************************************************/

static PIPE_RPC_FNS* find_pipe_fns_by_context( PIPE_RPC_FNS *list, uint32 context_id )
{
	PIPE_RPC_FNS *fns = NULL;

	if ( !list ) {
		DEBUG(0,("find_pipe_fns_by_context: ERROR!  No context list for pipe!\n"));
		return NULL;
	}

	for (fns=list; fns; fns=fns->next ) {
		if ( fns->context_id == context_id )
			return fns;
	}
	return NULL;
}

/****************************************************************************
 Memory cleanup.
****************************************************************************/

void free_pipe_rpc_context( PIPE_RPC_FNS *list )
{
	PIPE_RPC_FNS *tmp = list;
	PIPE_RPC_FNS *tmp2;

	while (tmp) {
		tmp2 = tmp->next;
		SAFE_FREE(tmp);
		tmp = tmp2;
	}

	return;	
}

static bool api_rpcTNP(pipes_struct *p,
		       const struct api_struct *api_rpc_cmds, int n_cmds);

/****************************************************************************
 Find the correct RPC function to call for this request.
 If the pipe is authenticated then become the correct UNIX user
 before doing the call.
****************************************************************************/

bool api_pipe_request(pipes_struct *p)
{
	bool ret = False;
	bool changed_user = False;
	PIPE_RPC_FNS *pipe_fns;

	if (p->pipe_bound &&
			((p->auth.auth_type == PIPE_AUTH_TYPE_NTLMSSP) ||
			 (p->auth.auth_type == PIPE_AUTH_TYPE_SPNEGO_NTLMSSP))) {
		if(!become_authenticated_pipe_user(p)) {
			prs_mem_free(&p->out_data.rdata);
			return False;
		}
		changed_user = True;
	}

	DEBUG(5, ("Requested \\PIPE\\%s\n",
		  get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));

	/* get the set of RPC functions for this context */

	pipe_fns = find_pipe_fns_by_context(p->contexts, p->hdr_req.context_id);

	if ( pipe_fns ) {
		TALLOC_CTX *frame = talloc_stackframe();
		ret = api_rpcTNP(p, pipe_fns->cmds, pipe_fns->n_cmds);
		TALLOC_FREE(frame);
	}
	else {
		DEBUG(0,("api_pipe_request: No rpc function table associated with context [%d] on pipe [%s]\n",
			p->hdr_req.context_id,
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
	}

	if (changed_user) {
		unbecome_authenticated_pipe_user();
	}

	return ret;
}

/*******************************************************************
 Calls the underlying RPC function for a named pipe.
 ********************************************************************/

static bool api_rpcTNP(pipes_struct *p,
		       const struct api_struct *api_rpc_cmds, int n_cmds)
{
	int fn_num;
	uint32 offset1, offset2;

	/* interpret the command */
	DEBUG(4,("api_rpcTNP: %s op 0x%x - ",
		 get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
		 p->hdr_req.opnum));

	if (DEBUGLEVEL >= 50) {
		fstring name;
		slprintf(name, sizeof(name)-1, "in_%s",
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax));
		prs_dump(name, p->hdr_req.opnum, &p->in_data.data);
	}

	for (fn_num = 0; fn_num < n_cmds; fn_num++) {
		if (api_rpc_cmds[fn_num].opnum == p->hdr_req.opnum && api_rpc_cmds[fn_num].fn != NULL) {
			DEBUG(3,("api_rpcTNP: rpc command: %s\n", api_rpc_cmds[fn_num].name));
			break;
		}
	}

	if (fn_num == n_cmds) {
		/*
		 * For an unknown RPC just return a fault PDU but
		 * return True to allow RPC's on the pipe to continue
		 * and not put the pipe into fault state. JRA.
		 */
		DEBUG(4, ("unknown\n"));
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		return True;
	}

	offset1 = prs_offset(&p->out_data.rdata);

        DEBUG(6, ("api_rpc_cmds[%d].fn == %p\n", 
                fn_num, api_rpc_cmds[fn_num].fn));
	/* do the actual command */
	if(!api_rpc_cmds[fn_num].fn(p)) {
		DEBUG(0,("api_rpcTNP: %s: %s failed.\n",
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
			 api_rpc_cmds[fn_num].name));
		prs_mem_free(&p->out_data.rdata);
		return False;
	}

	if (p->bad_handle_fault_state) {
		DEBUG(4,("api_rpcTNP: bad handle fault return.\n"));
		p->bad_handle_fault_state = False;
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_CONTEXT_MISMATCH));
		return True;
	}

	if (p->rng_fault_state) {
		DEBUG(4, ("api_rpcTNP: rng fault return\n"));
		p->rng_fault_state = False;
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		return True;
	}

	offset2 = prs_offset(&p->out_data.rdata);
	prs_set_offset(&p->out_data.rdata, offset1);
	if (DEBUGLEVEL >= 50) {
		fstring name;
		slprintf(name, sizeof(name)-1, "out_%s",
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax));
		prs_dump(name, p->hdr_req.opnum, &p->out_data.rdata);
	}
	prs_set_offset(&p->out_data.rdata, offset2);

	DEBUG(5,("api_rpcTNP: called %s successfully\n",
		 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));

	/* Check for buffer underflow in rpc parsing */

	if ((DEBUGLEVEL >= 10) && 
	    (prs_offset(&p->in_data.data) != prs_data_size(&p->in_data.data))) {
		size_t data_len = prs_data_size(&p->in_data.data) - prs_offset(&p->in_data.data);
		char *data = (char *)SMB_MALLOC(data_len);

		DEBUG(10, ("api_rpcTNP: rpc input buffer underflow (parse error?)\n"));
		if (data) {
			prs_uint8s(False, "", &p->in_data.data, 0, (unsigned char *)data, (uint32)data_len);
			SAFE_FREE(data);
		}

	}

	return True;
}
