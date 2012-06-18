/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Largely rewritten by Jeremy Allison		    2005.
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

#include "includes.h"
#include "librpc/gen_ndr/cli_epmapper.h"
#include "../librpc/gen_ndr/ndr_schannel.h"
#include "../libcli/auth/schannel.h"
#include "../libcli/auth/spnego.h"
#include "smb_krb5.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_CLI

static const char *get_pipe_name_from_iface(
	TALLOC_CTX *mem_ctx, const struct ndr_interface_table *interface)
{
	int i;
	const struct ndr_interface_string_array *ep = interface->endpoints;
	char *p;

	for (i=0; i<ep->count; i++) {
		if (strncmp(ep->names[i], "ncacn_np:[\\pipe\\", 16) == 0) {
			break;
		}
	}
	if (i == ep->count) {
		return NULL;
	}

	/*
	 * extract the pipe name without \\pipe from for example
	 * ncacn_np:[\\pipe\\epmapper]
	 */
	p = strchr(ep->names[i]+15, ']');
	if (p == NULL) {
		return "PIPE";
	}
	return talloc_strndup(mem_ctx, ep->names[i]+15, p - ep->names[i] - 15);
}

static const struct ndr_interface_table **interfaces;

bool smb_register_ndr_interface(const struct ndr_interface_table *interface)
{
	int num_interfaces = talloc_array_length(interfaces);
	const struct ndr_interface_table **tmp;
	int i;

	for (i=0; i<num_interfaces; i++) {
		if (ndr_syntax_id_equal(&interfaces[i]->syntax_id,
					&interface->syntax_id)) {
			return true;
		}
	}

	tmp = talloc_realloc(NULL, interfaces,
			     const struct ndr_interface_table *,
			     num_interfaces + 1);
	if (tmp == NULL) {
		DEBUG(1, ("smb_register_ndr_interface: talloc failed\n"));
		return false;
	}
	interfaces = tmp;
	interfaces[num_interfaces] = interface;
	return true;
}

static bool initialize_interfaces(void)
{
	if (!smb_register_ndr_interface(&ndr_table_lsarpc)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_dssetup)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_samr)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_netlogon)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_srvsvc)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_wkssvc)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_winreg)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_spoolss)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_netdfs)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_rpcecho)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_initshutdown)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_svcctl)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_eventlog)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_ntsvcs)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_epmapper)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_drsuapi)) {
		return false;
	}
	return true;
}

const struct ndr_interface_table *get_iface_from_syntax(
	const struct ndr_syntax_id *syntax)
{
	int num_interfaces;
	int i;

	if (interfaces == NULL) {
		if (!initialize_interfaces()) {
			return NULL;
		}
	}
	num_interfaces = talloc_array_length(interfaces);

	for (i=0; i<num_interfaces; i++) {
		if (ndr_syntax_id_equal(&interfaces[i]->syntax_id, syntax)) {
			return interfaces[i];
		}
	}

	return NULL;
}

/****************************************************************************
 Return the pipe name from the interface.
 ****************************************************************************/

const char *get_pipe_name_from_syntax(TALLOC_CTX *mem_ctx,
				      const struct ndr_syntax_id *syntax)
{
	const struct ndr_interface_table *interface;
	char *guid_str;
	const char *result;

	interface = get_iface_from_syntax(syntax);
	if (interface != NULL) {
		result = get_pipe_name_from_iface(mem_ctx, interface);
		if (result != NULL) {
			return result;
		}
	}

	/*
	 * Here we should ask \\epmapper, but for now our code is only
	 * interested in the known pipes mentioned in pipe_names[]
	 */

	guid_str = GUID_string(talloc_tos(), &syntax->uuid);
	if (guid_str == NULL) {
		return NULL;
	}
	result = talloc_asprintf(mem_ctx, "Interface %s.%d", guid_str,
				 (int)syntax->if_version);
	TALLOC_FREE(guid_str);

	if (result == NULL) {
		return "PIPE";
	}
	return result;
}

/********************************************************************
 Map internal value to wire value.
 ********************************************************************/

static int map_pipe_auth_type_to_rpc_auth_type(enum pipe_auth_type auth_type)
{
	switch (auth_type) {

	case PIPE_AUTH_TYPE_NONE:
		return DCERPC_AUTH_TYPE_NONE;

	case PIPE_AUTH_TYPE_NTLMSSP:
		return DCERPC_AUTH_TYPE_NTLMSSP;

	case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
	case PIPE_AUTH_TYPE_SPNEGO_KRB5:
		return DCERPC_AUTH_TYPE_SPNEGO;

	case PIPE_AUTH_TYPE_SCHANNEL:
		return DCERPC_AUTH_TYPE_SCHANNEL;

	case PIPE_AUTH_TYPE_KRB5:
		return DCERPC_AUTH_TYPE_KRB5;

	default:
		DEBUG(0,("map_pipe_auth_type_to_rpc_type: unknown pipe "
			"auth type %u\n",
			(unsigned int)auth_type ));
		break;
	}
	return -1;
}

/********************************************************************
 Pipe description for a DEBUG
 ********************************************************************/
static const char *rpccli_pipe_txt(TALLOC_CTX *mem_ctx,
				   struct rpc_pipe_client *cli)
{
	char *result = talloc_asprintf(mem_ctx, "host %s", cli->desthost);
	if (result == NULL) {
		return "pipe";
	}
	return result;
}

/********************************************************************
 Rpc pipe call id.
 ********************************************************************/

static uint32 get_rpc_call_id(void)
{
	static uint32 call_id = 0;
	return ++call_id;
}

/*
 * Realloc pdu to have a least "size" bytes
 */

static bool rpc_grow_buffer(prs_struct *pdu, size_t size)
{
	size_t extra_size;

	if (prs_data_size(pdu) >= size) {
		return true;
	}

	extra_size = size - prs_data_size(pdu);

	if (!prs_force_grow(pdu, extra_size)) {
		DEBUG(0, ("rpc_grow_buffer: Failed to grow parse struct by "
			  "%d bytes.\n", (int)extra_size));
		return false;
	}

	DEBUG(5, ("rpc_grow_buffer: grew buffer by %d bytes to %u\n",
		  (int)extra_size, prs_data_size(pdu)));
	return true;
}


/*******************************************************************
 Use SMBreadX to get rest of one fragment's worth of rpc data.
 Reads the whole size or give an error message
 ********************************************************************/

struct rpc_read_state {
	struct event_context *ev;
	struct rpc_cli_transport *transport;
	uint8_t *data;
	size_t size;
	size_t num_read;
};

static void rpc_read_done(struct tevent_req *subreq);

static struct tevent_req *rpc_read_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct rpc_cli_transport *transport,
					uint8_t *data, size_t size)
{
	struct tevent_req *req, *subreq;
	struct rpc_read_state *state;

	req = tevent_req_create(mem_ctx, &state, struct rpc_read_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->transport = transport;
	state->data = data;
	state->size = size;
	state->num_read = 0;

	DEBUG(5, ("rpc_read_send: data_to_read: %u\n", (unsigned int)size));

	subreq = transport->read_send(state, ev, (uint8_t *)data, size,
				      transport->priv);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_read_done, req);
	return req;

 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_read_state *state = tevent_req_data(
		req, struct rpc_read_state);
	NTSTATUS status;
	ssize_t received;

	status = state->transport->read_recv(subreq, &received);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->num_read += received;
	if (state->num_read == state->size) {
		tevent_req_done(req);
		return;
	}

	subreq = state->transport->read_send(state, state->ev,
					     state->data + state->num_read,
					     state->size - state->num_read,
					     state->transport->priv);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_read_done, req);
}

static NTSTATUS rpc_read_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

struct rpc_write_state {
	struct event_context *ev;
	struct rpc_cli_transport *transport;
	const uint8_t *data;
	size_t size;
	size_t num_written;
};

static void rpc_write_done(struct tevent_req *subreq);

static struct tevent_req *rpc_write_send(TALLOC_CTX *mem_ctx,
					 struct event_context *ev,
					 struct rpc_cli_transport *transport,
					 const uint8_t *data, size_t size)
{
	struct tevent_req *req, *subreq;
	struct rpc_write_state *state;

	req = tevent_req_create(mem_ctx, &state, struct rpc_write_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->transport = transport;
	state->data = data;
	state->size = size;
	state->num_written = 0;

	DEBUG(5, ("rpc_write_send: data_to_write: %u\n", (unsigned int)size));

	subreq = transport->write_send(state, ev, data, size, transport->priv);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_write_done, req);
	return req;
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_write_state *state = tevent_req_data(
		req, struct rpc_write_state);
	NTSTATUS status;
	ssize_t written;

	status = state->transport->write_recv(subreq, &written);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->num_written += written;

	if (state->num_written == state->size) {
		tevent_req_done(req);
		return;
	}

	subreq = state->transport->write_send(state, state->ev,
					      state->data + state->num_written,
					      state->size - state->num_written,
					      state->transport->priv);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_write_done, req);
}

static NTSTATUS rpc_write_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}


static NTSTATUS parse_rpc_header(struct rpc_pipe_client *cli,
				 struct rpc_hdr_info *prhdr,
				 prs_struct *pdu)
{
	/*
	 * This next call sets the endian bit correctly in current_pdu. We
	 * will propagate this to rbuf later.
	 */

	if(!smb_io_rpc_hdr("rpc_hdr   ", prhdr, pdu, 0)) {
		DEBUG(0, ("get_current_pdu: Failed to unmarshall RPC_HDR.\n"));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	if (prhdr->frag_len > cli->max_recv_frag) {
		DEBUG(0, ("cli_pipe_get_current_pdu: Server sent fraglen %d,"
			  " we only allow %d\n", (int)prhdr->frag_len,
			  (int)cli->max_recv_frag));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Try and get a PDU's worth of data from current_pdu. If not, then read more
 from the wire.
 ****************************************************************************/

struct get_complete_frag_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	struct rpc_hdr_info *prhdr;
	prs_struct *pdu;
};

static void get_complete_frag_got_header(struct tevent_req *subreq);
static void get_complete_frag_got_rest(struct tevent_req *subreq);

static struct tevent_req *get_complete_frag_send(TALLOC_CTX *mem_ctx,
						 struct event_context *ev,
						 struct rpc_pipe_client *cli,
						 struct rpc_hdr_info *prhdr,
						 prs_struct *pdu)
{
	struct tevent_req *req, *subreq;
	struct get_complete_frag_state *state;
	uint32_t pdu_len;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state,
				struct get_complete_frag_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->prhdr = prhdr;
	state->pdu = pdu;

	pdu_len = prs_data_size(pdu);
	if (pdu_len < RPC_HEADER_LEN) {
		if (!rpc_grow_buffer(pdu, RPC_HEADER_LEN)) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		subreq = rpc_read_send(
			state, state->ev,
			state->cli->transport,
			(uint8_t *)(prs_data_p(state->pdu) + pdu_len),
			RPC_HEADER_LEN - pdu_len);
		if (subreq == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		tevent_req_set_callback(subreq, get_complete_frag_got_header,
					req);
		return req;
	}

	status = parse_rpc_header(cli, prhdr, pdu);
	if (!NT_STATUS_IS_OK(status)) {
		goto post_status;
	}

	/*
	 * Ensure we have frag_len bytes of data.
	 */
	if (pdu_len < prhdr->frag_len) {
		if (!rpc_grow_buffer(pdu, prhdr->frag_len)) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		subreq = rpc_read_send(state, state->ev,
				       state->cli->transport,
				       (uint8_t *)(prs_data_p(pdu) + pdu_len),
				       prhdr->frag_len - pdu_len);
		if (subreq == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto post_status;
		}
		tevent_req_set_callback(subreq, get_complete_frag_got_rest,
					req);
		return req;
	}

	status = NT_STATUS_OK;
 post_status:
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, status);
	}
	return tevent_req_post(req, ev);
}

static void get_complete_frag_got_header(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct get_complete_frag_state *state = tevent_req_data(
		req, struct get_complete_frag_state);
	NTSTATUS status;

	status = rpc_read_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	status = parse_rpc_header(state->cli, state->prhdr, state->pdu);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (!rpc_grow_buffer(state->pdu, state->prhdr->frag_len)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	/*
	 * We're here in this piece of code because we've read exactly
	 * RPC_HEADER_LEN bytes into state->pdu.
	 */

	subreq = rpc_read_send(
		state, state->ev, state->cli->transport,
		(uint8_t *)(prs_data_p(state->pdu) + RPC_HEADER_LEN),
		state->prhdr->frag_len - RPC_HEADER_LEN);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, get_complete_frag_got_rest, req);
}

static void get_complete_frag_got_rest(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = rpc_read_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS get_complete_frag_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

/****************************************************************************
 NTLMSSP specific sign/seal.
 Virtually identical to rpc_server/srv_pipe.c:api_pipe_ntlmssp_auth_process.
 In fact I should probably abstract these into identical pieces of code... JRA.
 ****************************************************************************/

static NTSTATUS cli_pipe_verify_ntlmssp(struct rpc_pipe_client *cli, RPC_HDR *prhdr,
				prs_struct *current_pdu,
				uint8 *p_ss_padding_len)
{
	RPC_HDR_AUTH auth_info;
	uint32 save_offset = prs_offset(current_pdu);
	uint32 auth_len = prhdr->auth_len;
	NTLMSSP_STATE *ntlmssp_state = cli->auth->a_u.ntlmssp_state;
	unsigned char *data = NULL;
	size_t data_len;
	unsigned char *full_packet_data = NULL;
	size_t full_packet_data_len;
	DATA_BLOB auth_blob;
	NTSTATUS status;

	if (cli->auth->auth_level == DCERPC_AUTH_LEVEL_NONE
	    || cli->auth->auth_level == DCERPC_AUTH_LEVEL_CONNECT) {
		return NT_STATUS_OK;
	}

	if (!ntlmssp_state) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Ensure there's enough data for an authenticated response. */
	if ((auth_len > RPC_MAX_SIGN_SIZE) ||
			(RPC_HEADER_LEN + RPC_HDR_RESP_LEN + RPC_HDR_AUTH_LEN + auth_len > prhdr->frag_len)) {
		DEBUG(0,("cli_pipe_verify_ntlmssp: auth_len %u is too large.\n",
			(unsigned int)auth_len ));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/*
	 * We need the full packet data + length (minus auth stuff) as well as the packet data + length
	 * after the RPC header.
	 * We need to pass in the full packet (minus auth len) to the NTLMSSP sign and check seal
	 * functions as NTLMv2 checks the rpc headers also.
	 */

	data = (unsigned char *)(prs_data_p(current_pdu) + RPC_HEADER_LEN + RPC_HDR_RESP_LEN);
	data_len = (size_t)(prhdr->frag_len - RPC_HEADER_LEN - RPC_HDR_RESP_LEN - RPC_HDR_AUTH_LEN - auth_len);

	full_packet_data = (unsigned char *)prs_data_p(current_pdu);
	full_packet_data_len = prhdr->frag_len - auth_len;

	/* Pull the auth header and the following data into a blob. */
	if(!prs_set_offset(current_pdu, RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len)) {
		DEBUG(0,("cli_pipe_verify_ntlmssp: cannot move offset to %u.\n",
			(unsigned int)RPC_HEADER_LEN + (unsigned int)RPC_HDR_RESP_LEN + (unsigned int)data_len ));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, current_pdu, 0)) {
		DEBUG(0,("cli_pipe_verify_ntlmssp: failed to unmarshall RPC_HDR_AUTH.\n"));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	auth_blob.data = (unsigned char *)prs_data_p(current_pdu) + prs_offset(current_pdu);
	auth_blob.length = auth_len;

	switch (cli->auth->auth_level) {
		case DCERPC_AUTH_LEVEL_PRIVACY:
			/* Data is encrypted. */
			status = ntlmssp_unseal_packet(ntlmssp_state,
							data, data_len,
							full_packet_data,
							full_packet_data_len,
							&auth_blob);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0,("cli_pipe_verify_ntlmssp: failed to unseal "
					"packet from %s. Error was %s.\n",
					rpccli_pipe_txt(talloc_tos(), cli),
					nt_errstr(status) ));
				return status;
			}
			break;
		case DCERPC_AUTH_LEVEL_INTEGRITY:
			/* Data is signed. */
			status = ntlmssp_check_packet(ntlmssp_state,
							data, data_len,
							full_packet_data,
							full_packet_data_len,
							&auth_blob);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0,("cli_pipe_verify_ntlmssp: check signing failed on "
					"packet from %s. Error was %s.\n",
					rpccli_pipe_txt(talloc_tos(), cli),
					nt_errstr(status) ));
				return status;
			}
			break;
		default:
			DEBUG(0, ("cli_pipe_verify_ntlmssp: unknown internal "
				  "auth level %d\n", cli->auth->auth_level));
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	/*
	 * Return the current pointer to the data offset.
	 */

	if(!prs_set_offset(current_pdu, save_offset)) {
		DEBUG(0,("api_pipe_auth_process: failed to set offset back to %u\n",
			(unsigned int)save_offset ));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/*
	 * Remember the padding length. We must remove it from the real data
	 * stream once the sign/seal is done.
	 */

	*p_ss_padding_len = auth_info.auth_pad_len;

	return NT_STATUS_OK;
}

/****************************************************************************
 schannel specific sign/seal.
 ****************************************************************************/

static NTSTATUS cli_pipe_verify_schannel(struct rpc_pipe_client *cli, RPC_HDR *prhdr,
				prs_struct *current_pdu,
				uint8 *p_ss_padding_len)
{
	RPC_HDR_AUTH auth_info;
	uint32 auth_len = prhdr->auth_len;
	uint32 save_offset = prs_offset(current_pdu);
	struct schannel_state *schannel_auth =
		cli->auth->a_u.schannel_auth;
	uint8_t *data;
	uint32 data_len;
	DATA_BLOB blob;
	NTSTATUS status;

	if (cli->auth->auth_level == DCERPC_AUTH_LEVEL_NONE
	    || cli->auth->auth_level == DCERPC_AUTH_LEVEL_CONNECT) {
		return NT_STATUS_OK;
	}

	if (auth_len < RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN) {
		DEBUG(0,("cli_pipe_verify_schannel: auth_len %u.\n", (unsigned int)auth_len ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!schannel_auth) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Ensure there's enough data for an authenticated response. */
	if ((auth_len > RPC_MAX_SIGN_SIZE) ||
			(RPC_HEADER_LEN + RPC_HDR_RESP_LEN + RPC_HDR_AUTH_LEN + auth_len > prhdr->frag_len)) {
		DEBUG(0,("cli_pipe_verify_schannel: auth_len %u is too large.\n",
			(unsigned int)auth_len ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	data_len = prhdr->frag_len - RPC_HEADER_LEN - RPC_HDR_RESP_LEN - RPC_HDR_AUTH_LEN - auth_len;

	if(!prs_set_offset(current_pdu, RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len)) {
		DEBUG(0,("cli_pipe_verify_schannel: cannot move offset to %u.\n",
			(unsigned int)RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len ));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, current_pdu, 0)) {
		DEBUG(0,("cli_pipe_verify_schannel: failed to unmarshall RPC_HDR_AUTH.\n"));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	if (auth_info.auth_type != DCERPC_AUTH_TYPE_SCHANNEL) {
		DEBUG(0,("cli_pipe_verify_schannel: Invalid auth info %d on schannel\n",
			auth_info.auth_type));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	blob = data_blob_const(prs_data_p(current_pdu) + prs_offset(current_pdu), auth_len);

	if (DEBUGLEVEL >= 10) {
		dump_NL_AUTH_SIGNATURE(talloc_tos(), &blob);
	}

	data = (uint8_t *)prs_data_p(current_pdu)+RPC_HEADER_LEN+RPC_HDR_RESP_LEN;

	switch (cli->auth->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = netsec_incoming_packet(schannel_auth,
						talloc_tos(),
						true,
						data,
						data_len,
						&blob);
		break;
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = netsec_incoming_packet(schannel_auth,
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
		DEBUG(3,("cli_pipe_verify_schannel: failed to decode PDU "
				"Connection to %s (%s).\n",
				rpccli_pipe_txt(talloc_tos(), cli),
				nt_errstr(status)));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * Return the current pointer to the data offset.
	 */

	if(!prs_set_offset(current_pdu, save_offset)) {
		DEBUG(0,("api_pipe_auth_process: failed to set offset back to %u\n",
			(unsigned int)save_offset ));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/*
	 * Remember the padding length. We must remove it from the real data
	 * stream once the sign/seal is done.
	 */

	*p_ss_padding_len = auth_info.auth_pad_len;

	return NT_STATUS_OK;
}

/****************************************************************************
 Do the authentication checks on an incoming pdu. Check sign and unseal etc.
 ****************************************************************************/

static NTSTATUS cli_pipe_validate_rpc_response(struct rpc_pipe_client *cli, RPC_HDR *prhdr,
				prs_struct *current_pdu,
				uint8 *p_ss_padding_len)
{
	NTSTATUS ret = NT_STATUS_OK;

	/* Paranioa checks for auth_len. */
	if (prhdr->auth_len) {
		if (prhdr->auth_len > prhdr->frag_len) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (prhdr->auth_len + (unsigned int)RPC_HDR_AUTH_LEN < prhdr->auth_len ||
				prhdr->auth_len + (unsigned int)RPC_HDR_AUTH_LEN < (unsigned int)RPC_HDR_AUTH_LEN) {
			/* Integer wrap attempt. */
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	/*
	 * Now we have a complete RPC request PDU fragment, try and verify any auth data.
	 */

	switch(cli->auth->auth_type) {
		case PIPE_AUTH_TYPE_NONE:
			if (prhdr->auth_len) {
				DEBUG(3, ("cli_pipe_validate_rpc_response: "
					  "Connection to %s - got non-zero "
					  "auth len %u.\n",
					rpccli_pipe_txt(talloc_tos(), cli),
					(unsigned int)prhdr->auth_len ));
				return NT_STATUS_INVALID_PARAMETER;
			}
			break;

		case PIPE_AUTH_TYPE_NTLMSSP:
		case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
			ret = cli_pipe_verify_ntlmssp(cli, prhdr, current_pdu, p_ss_padding_len);
			if (!NT_STATUS_IS_OK(ret)) {
				return ret;
			}
			break;

		case PIPE_AUTH_TYPE_SCHANNEL:
			ret = cli_pipe_verify_schannel(cli, prhdr, current_pdu, p_ss_padding_len);
			if (!NT_STATUS_IS_OK(ret)) {
				return ret;
			}
			break;

		case PIPE_AUTH_TYPE_KRB5:
		case PIPE_AUTH_TYPE_SPNEGO_KRB5:
		default:
			DEBUG(3, ("cli_pipe_validate_rpc_response: Connection "
				  "to %s - unknown internal auth type %u.\n",
				  rpccli_pipe_txt(talloc_tos(), cli),
				  cli->auth->auth_type ));
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Do basic authentication checks on an incoming pdu.
 ****************************************************************************/

static NTSTATUS cli_pipe_validate_current_pdu(struct rpc_pipe_client *cli, RPC_HDR *prhdr,
			prs_struct *current_pdu,
			uint8 expected_pkt_type,
			char **ppdata,
			uint32 *pdata_len,
			prs_struct *return_data)
{

	NTSTATUS ret = NT_STATUS_OK;
	uint32 current_pdu_len = prs_data_size(current_pdu);

	if (current_pdu_len != prhdr->frag_len) {
		DEBUG(5,("cli_pipe_validate_current_pdu: incorrect pdu length %u, expected %u\n",
			(unsigned int)current_pdu_len, (unsigned int)prhdr->frag_len ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/*
	 * Point the return values at the real data including the RPC
	 * header. Just in case the caller wants it.
	 */
	*ppdata = prs_data_p(current_pdu);
	*pdata_len = current_pdu_len;

	/* Ensure we have the correct type. */
	switch (prhdr->pkt_type) {
		case DCERPC_PKT_ALTER_RESP:
		case DCERPC_PKT_BIND_ACK:

			/* Alter context and bind ack share the same packet definitions. */
			break;


		case DCERPC_PKT_RESPONSE:
		{
			RPC_HDR_RESP rhdr_resp;
			uint8 ss_padding_len = 0;

			if(!smb_io_rpc_hdr_resp("rpc_hdr_resp", &rhdr_resp, current_pdu, 0)) {
				DEBUG(5,("cli_pipe_validate_current_pdu: failed to unmarshal RPC_HDR_RESP.\n"));
				return NT_STATUS_BUFFER_TOO_SMALL;
			}

			/* Here's where we deal with incoming sign/seal. */
			ret = cli_pipe_validate_rpc_response(cli, prhdr,
					current_pdu, &ss_padding_len);
			if (!NT_STATUS_IS_OK(ret)) {
				return ret;
			}

			/* Point the return values at the NDR data. Remember to remove any ss padding. */
			*ppdata = prs_data_p(current_pdu) + RPC_HEADER_LEN + RPC_HDR_RESP_LEN;

			if (current_pdu_len < RPC_HEADER_LEN + RPC_HDR_RESP_LEN + ss_padding_len) {
				return NT_STATUS_BUFFER_TOO_SMALL;
			}

			*pdata_len = current_pdu_len - RPC_HEADER_LEN - RPC_HDR_RESP_LEN - ss_padding_len;

			/* Remember to remove the auth footer. */
			if (prhdr->auth_len) {
				/* We've already done integer wrap tests on auth_len in
					cli_pipe_validate_rpc_response(). */
				if (*pdata_len < RPC_HDR_AUTH_LEN + prhdr->auth_len) {
					return NT_STATUS_BUFFER_TOO_SMALL;
				}
				*pdata_len -= (RPC_HDR_AUTH_LEN + prhdr->auth_len);
			}

			DEBUG(10,("cli_pipe_validate_current_pdu: got pdu len %u, data_len %u, ss_len %u\n",
				current_pdu_len, *pdata_len, ss_padding_len ));

			/*
			 * If this is the first reply, and the allocation hint is reasonably, try and
			 * set up the return_data parse_struct to the correct size.
			 */

			if ((prs_data_size(return_data) == 0) && rhdr_resp.alloc_hint && (rhdr_resp.alloc_hint < 15*1024*1024)) {
				if (!prs_set_buffer_size(return_data, rhdr_resp.alloc_hint)) {
					DEBUG(0,("cli_pipe_validate_current_pdu: reply alloc hint %u "
						"too large to allocate\n",
						(unsigned int)rhdr_resp.alloc_hint ));
					return NT_STATUS_NO_MEMORY;
				}
			}

			break;
		}

		case DCERPC_PKT_BIND_NAK:
			DEBUG(1, ("cli_pipe_validate_current_pdu: Bind NACK "
				  "received from %s!\n",
				  rpccli_pipe_txt(talloc_tos(), cli)));
			/* Use this for now... */
			return NT_STATUS_NETWORK_ACCESS_DENIED;

		case DCERPC_PKT_FAULT:
		{
			RPC_HDR_RESP rhdr_resp;
			RPC_HDR_FAULT fault_resp;

			if(!smb_io_rpc_hdr_resp("rpc_hdr_resp", &rhdr_resp, current_pdu, 0)) {
				DEBUG(5,("cli_pipe_validate_current_pdu: failed to unmarshal RPC_HDR_RESP.\n"));
				return NT_STATUS_BUFFER_TOO_SMALL;
			}

			if(!smb_io_rpc_hdr_fault("fault", &fault_resp, current_pdu, 0)) {
				DEBUG(5,("cli_pipe_validate_current_pdu: failed to unmarshal RPC_HDR_FAULT.\n"));
				return NT_STATUS_BUFFER_TOO_SMALL;
			}

			DEBUG(1, ("cli_pipe_validate_current_pdu: RPC fault "
				  "code %s received from %s!\n",
				dcerpc_errstr(talloc_tos(), NT_STATUS_V(fault_resp.status)),
				rpccli_pipe_txt(talloc_tos(), cli)));
			if (NT_STATUS_IS_OK(fault_resp.status)) {
				return NT_STATUS_UNSUCCESSFUL;
			} else {
				return fault_resp.status;
			}
		}

		default:
			DEBUG(0, ("cli_pipe_validate_current_pdu: unknown packet type %u received "
				"from %s!\n",
				(unsigned int)prhdr->pkt_type,
				rpccli_pipe_txt(talloc_tos(), cli)));
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	if (prhdr->pkt_type != expected_pkt_type) {
		DEBUG(3, ("cli_pipe_validate_current_pdu: Connection to %s "
			  "got an unexpected RPC packet type - %u, not %u\n",
			rpccli_pipe_txt(talloc_tos(), cli),
			prhdr->pkt_type,
			expected_pkt_type));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	/* Do this just before return - we don't want to modify any rpc header
	   data before now as we may have needed to do cryptographic actions on
	   it before. */

	if ((prhdr->pkt_type == DCERPC_PKT_BIND_ACK) && !(prhdr->flags & DCERPC_PFC_FLAG_LAST)) {
		DEBUG(5,("cli_pipe_validate_current_pdu: bug in server (AS/U?), "
			"setting fragment first/last ON.\n"));
		prhdr->flags |= DCERPC_PFC_FLAG_FIRST|DCERPC_PFC_FLAG_LAST;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Ensure we eat the just processed pdu from the current_pdu prs_struct.
 Normally the frag_len and buffer size will match, but on the first trans
 reply there is a theoretical chance that buffer size > frag_len, so we must
 deal with that.
 ****************************************************************************/

static NTSTATUS cli_pipe_reset_current_pdu(struct rpc_pipe_client *cli, RPC_HDR *prhdr, prs_struct *current_pdu)
{
	uint32 current_pdu_len = prs_data_size(current_pdu);

	if (current_pdu_len < prhdr->frag_len) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/* Common case. */
	if (current_pdu_len == (uint32)prhdr->frag_len) {
		prs_mem_free(current_pdu);
		prs_init_empty(current_pdu, prs_get_mem_context(current_pdu), UNMARSHALL);
		/* Make current_pdu dynamic with no memory. */
		prs_give_memory(current_pdu, 0, 0, True);
		return NT_STATUS_OK;
	}

	/*
	 * Oh no ! More data in buffer than we processed in current pdu.
	 * Cheat. Move the data down and shrink the buffer.
	 */

	memcpy(prs_data_p(current_pdu), prs_data_p(current_pdu) + prhdr->frag_len,
			current_pdu_len - prhdr->frag_len);

	/* Remember to set the read offset back to zero. */
	prs_set_offset(current_pdu, 0);

	/* Shrink the buffer. */
	if (!prs_set_buffer_size(current_pdu, current_pdu_len - prhdr->frag_len)) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Call a remote api on an arbitrary pipe.  takes param, data and setup buffers.
****************************************************************************/

struct cli_api_pipe_state {
	struct event_context *ev;
	struct rpc_cli_transport *transport;
	uint8_t *rdata;
	uint32_t rdata_len;
};

static void cli_api_pipe_trans_done(struct tevent_req *subreq);
static void cli_api_pipe_write_done(struct tevent_req *subreq);
static void cli_api_pipe_read_done(struct tevent_req *subreq);

static struct tevent_req *cli_api_pipe_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct rpc_cli_transport *transport,
					    uint8_t *data, size_t data_len,
					    uint32_t max_rdata_len)
{
	struct tevent_req *req, *subreq;
	struct cli_api_pipe_state *state;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct cli_api_pipe_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->transport = transport;

	if (max_rdata_len < RPC_HEADER_LEN) {
		/*
		 * For a RPC reply we always need at least RPC_HEADER_LEN
		 * bytes. We check this here because we will receive
		 * RPC_HEADER_LEN bytes in cli_trans_sock_send_done.
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto post_status;
	}

	if (transport->trans_send != NULL) {
		subreq = transport->trans_send(state, ev, data, data_len,
					       max_rdata_len, transport->priv);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, cli_api_pipe_trans_done, req);
		return req;
	}

	/*
	 * If the transport does not provide a "trans" routine, i.e. for
	 * example the ncacn_ip_tcp transport, do the write/read step here.
	 */

	subreq = rpc_write_send(state, ev, transport, data, data_len);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, cli_api_pipe_write_done, req);
	return req;

	status = NT_STATUS_INVALID_PARAMETER;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void cli_api_pipe_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;

	status = state->transport->trans_recv(subreq, state, &state->rdata,
					      &state->rdata_len);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

static void cli_api_pipe_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;

	status = rpc_write_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	state->rdata = TALLOC_ARRAY(state, uint8_t, RPC_HEADER_LEN);
	if (tevent_req_nomem(state->rdata, req)) {
		return;
	}

	/*
	 * We don't need to use rpc_read_send here, the upper layer will cope
	 * with a short read, transport->trans_send could also return less
	 * than state->max_rdata_len.
	 */
	subreq = state->transport->read_send(state, state->ev, state->rdata,
					     RPC_HEADER_LEN,
					     state->transport->priv);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, cli_api_pipe_read_done, req);
}

static void cli_api_pipe_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;
	ssize_t received;

	status = state->transport->read_recv(subreq, &received);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	state->rdata_len = received;
	tevent_req_done(req);
}

static NTSTATUS cli_api_pipe_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				  uint8_t **prdata, uint32_t *prdata_len)
{
	struct cli_api_pipe_state *state = tevent_req_data(
		req, struct cli_api_pipe_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	*prdata = talloc_move(mem_ctx, &state->rdata);
	*prdata_len = state->rdata_len;
	return NT_STATUS_OK;
}

/****************************************************************************
 Send data on an rpc pipe via trans. The prs_struct data must be the last
 pdu fragment of an NDR data stream.

 Receive response data from an rpc pipe, which may be large...

 Read the first fragment: unfortunately have to use SMBtrans for the first
 bit, then SMBreadX for subsequent bits.

 If first fragment received also wasn't the last fragment, continue
 getting fragments until we _do_ receive the last fragment.

 Request/Response PDU's look like the following...

 |<------------------PDU len----------------------------------------------->|
 |<-HDR_LEN-->|<--REQ LEN------>|.............|<-AUTH_HDRLEN->|<-AUTH_LEN-->|

 +------------+-----------------+-------------+---------------+-------------+
 | RPC HEADER | REQ/RESP HEADER | DATA ...... | AUTH_HDR      | AUTH DATA   |
 +------------+-----------------+-------------+---------------+-------------+

 Where the presence of the AUTH_HDR and AUTH DATA are dependent on the
 signing & sealing being negotiated.

 ****************************************************************************/

struct rpc_api_pipe_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	uint8_t expected_pkt_type;

	prs_struct incoming_frag;
	struct rpc_hdr_info rhdr;

	prs_struct incoming_pdu;	/* Incoming reply */
	uint32_t incoming_pdu_offset;
};

static int rpc_api_pipe_state_destructor(struct rpc_api_pipe_state *state)
{
	prs_mem_free(&state->incoming_frag);
	prs_mem_free(&state->incoming_pdu);
	return 0;
}

static void rpc_api_pipe_trans_done(struct tevent_req *subreq);
static void rpc_api_pipe_got_pdu(struct tevent_req *subreq);

static struct tevent_req *rpc_api_pipe_send(TALLOC_CTX *mem_ctx,
					    struct event_context *ev,
					    struct rpc_pipe_client *cli,
					    prs_struct *data, /* Outgoing PDU */
					    uint8_t expected_pkt_type)
{
	struct tevent_req *req, *subreq;
	struct rpc_api_pipe_state *state;
	uint16_t max_recv_frag;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct rpc_api_pipe_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->expected_pkt_type = expected_pkt_type;
	state->incoming_pdu_offset = 0;

	prs_init_empty(&state->incoming_frag, state, UNMARSHALL);

	prs_init_empty(&state->incoming_pdu, state, UNMARSHALL);
	/* Make incoming_pdu dynamic with no memory. */
	prs_give_memory(&state->incoming_pdu, NULL, 0, true);

	talloc_set_destructor(state, rpc_api_pipe_state_destructor);

	/*
	 * Ensure we're not sending too much.
	 */
	if (prs_offset(data) > cli->max_xmit_frag) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto post_status;
	}

	DEBUG(5,("rpc_api_pipe: %s\n", rpccli_pipe_txt(talloc_tos(), cli)));

	max_recv_frag = cli->max_recv_frag;

#if 0
	max_recv_frag = RPC_HEADER_LEN + 10 + (sys_random() % 32);
#endif

	subreq = cli_api_pipe_send(state, ev, cli->transport,
				   (uint8_t *)prs_data_p(data),
				   prs_offset(data), max_recv_frag);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_api_pipe_trans_done, req);
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_api_pipe_trans_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_state *state = tevent_req_data(
		req, struct rpc_api_pipe_state);
	NTSTATUS status;
	uint8_t *rdata = NULL;
	uint32_t rdata_len = 0;
	char *rdata_copy;

	status = cli_api_pipe_recv(subreq, state, &rdata, &rdata_len);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("cli_api_pipe failed: %s\n", nt_errstr(status)));
		tevent_req_nterror(req, status);
		return;
	}

	if (rdata == NULL) {
		DEBUG(3,("rpc_api_pipe: %s failed to return data.\n",
			 rpccli_pipe_txt(talloc_tos(), state->cli)));
		tevent_req_done(req);
		return;
	}

	/*
	 * Give the memory received from cli_trans as dynamic to the current
	 * pdu. Duplicating it sucks, but prs_struct doesn't know about talloc
	 * :-(
	 */
	rdata_copy = (char *)memdup(rdata, rdata_len);
	TALLOC_FREE(rdata);
	if (tevent_req_nomem(rdata_copy, req)) {
		return;
	}
	prs_give_memory(&state->incoming_frag, rdata_copy, rdata_len, true);

	/* Ensure we have enough data for a pdu. */
	subreq = get_complete_frag_send(state, state->ev, state->cli,
					&state->rhdr, &state->incoming_frag);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_api_pipe_got_pdu, req);
}

static void rpc_api_pipe_got_pdu(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_state *state = tevent_req_data(
		req, struct rpc_api_pipe_state);
	NTSTATUS status;
	char *rdata = NULL;
	uint32_t rdata_len = 0;

	status = get_complete_frag_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(5, ("get_complete_frag failed: %s\n",
			  nt_errstr(status)));
		tevent_req_nterror(req, status);
		return;
	}

	status = cli_pipe_validate_current_pdu(
		state->cli, &state->rhdr, &state->incoming_frag,
		state->expected_pkt_type, &rdata, &rdata_len,
		&state->incoming_pdu);

	DEBUG(10,("rpc_api_pipe: got frag len of %u at offset %u: %s\n",
		  (unsigned)prs_data_size(&state->incoming_frag),
		  (unsigned)state->incoming_pdu_offset,
		  nt_errstr(status)));

	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if ((state->rhdr.flags & DCERPC_PFC_FLAG_FIRST)
	    && (state->rhdr.pack_type[0] == 0)) {
		/*
		 * Set the data type correctly for big-endian data on the
		 * first packet.
		 */
		DEBUG(10,("rpc_api_pipe: On %s PDU data format is "
			  "big-endian.\n",
			  rpccli_pipe_txt(talloc_tos(), state->cli)));
		prs_set_endian_data(&state->incoming_pdu, RPC_BIG_ENDIAN);
	}
	/*
	 * Check endianness on subsequent packets.
	 */
	if (state->incoming_frag.bigendian_data
	    != state->incoming_pdu.bigendian_data) {
		DEBUG(0,("rpc_api_pipe: Error : Endianness changed from %s to "
			 "%s\n",
			 state->incoming_pdu.bigendian_data?"big":"little",
			 state->incoming_frag.bigendian_data?"big":"little"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* Now copy the data portion out of the pdu into rbuf. */
	if (!prs_force_grow(&state->incoming_pdu, rdata_len)) {
		tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
		return;
	}

	memcpy(prs_data_p(&state->incoming_pdu) + state->incoming_pdu_offset,
	       rdata, (size_t)rdata_len);
	state->incoming_pdu_offset += rdata_len;

	status = cli_pipe_reset_current_pdu(state->cli, &state->rhdr,
					    &state->incoming_frag);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (state->rhdr.flags & DCERPC_PFC_FLAG_LAST) {
		DEBUG(10,("rpc_api_pipe: %s returned %u bytes.\n",
			  rpccli_pipe_txt(talloc_tos(), state->cli),
			  (unsigned)prs_data_size(&state->incoming_pdu)));
		tevent_req_done(req);
		return;
	}

	subreq = get_complete_frag_send(state, state->ev, state->cli,
					&state->rhdr, &state->incoming_frag);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, rpc_api_pipe_got_pdu, req);
}

static NTSTATUS rpc_api_pipe_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				  prs_struct *reply_pdu)
{
	struct rpc_api_pipe_state *state = tevent_req_data(
		req, struct rpc_api_pipe_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}

	*reply_pdu = state->incoming_pdu;
	reply_pdu->mem_ctx = mem_ctx;

	/*
	 * Prevent state->incoming_pdu from being freed in
	 * rpc_api_pipe_state_destructor()
	 */
	prs_init_empty(&state->incoming_pdu, state, UNMARSHALL);

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates krb5 auth bind.
 ********************************************************************/

static NTSTATUS create_krb5_auth_bind_req( struct rpc_pipe_client *cli,
						enum dcerpc_AuthLevel auth_level,
						RPC_HDR_AUTH *pauth_out,
						prs_struct *auth_data)
{
#ifdef HAVE_KRB5
	int ret;
	struct kerberos_auth_struct *a = cli->auth->a_u.kerberos_auth;
	DATA_BLOB tkt = data_blob_null;
	DATA_BLOB tkt_wrapped = data_blob_null;

	/* We may change the pad length before marshalling. */
	init_rpc_hdr_auth(pauth_out, DCERPC_AUTH_TYPE_KRB5, (int)auth_level, 0, 1);

	DEBUG(5, ("create_krb5_auth_bind_req: creating a service ticket for principal %s\n",
		a->service_principal ));

	/* Create the ticket for the service principal and return it in a gss-api wrapped blob. */

	ret = cli_krb5_get_ticket(a->service_principal, 0, &tkt,
			&a->session_key, (uint32)AP_OPTS_MUTUAL_REQUIRED, NULL, NULL, NULL);

	if (ret) {
		DEBUG(1,("create_krb5_auth_bind_req: cli_krb5_get_ticket for principal %s "
			"failed with %s\n",
			a->service_principal,
			error_message(ret) ));

		data_blob_free(&tkt);
		prs_mem_free(auth_data);
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* wrap that up in a nice GSS-API wrapping */
	tkt_wrapped = spnego_gen_krb5_wrap(tkt, TOK_ID_KRB_AP_REQ);

	data_blob_free(&tkt);

	/* Auth len in the rpc header doesn't include auth_header. */
	if (!prs_copy_data_in(auth_data, (char *)tkt_wrapped.data, tkt_wrapped.length)) {
		data_blob_free(&tkt_wrapped);
		prs_mem_free(auth_data);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5, ("create_krb5_auth_bind_req: Created krb5 GSS blob :\n"));
	dump_data(5, tkt_wrapped.data, tkt_wrapped.length);

	data_blob_free(&tkt_wrapped);
	return NT_STATUS_OK;
#else
	return NT_STATUS_INVALID_PARAMETER;
#endif
}

/*******************************************************************
 Creates SPNEGO NTLMSSP auth bind.
 ********************************************************************/

static NTSTATUS create_spnego_ntlmssp_auth_rpc_bind_req( struct rpc_pipe_client *cli,
						enum dcerpc_AuthLevel auth_level,
						RPC_HDR_AUTH *pauth_out,
						prs_struct *auth_data)
{
	NTSTATUS nt_status;
	DATA_BLOB null_blob = data_blob_null;
	DATA_BLOB request = data_blob_null;
	DATA_BLOB spnego_msg = data_blob_null;

	/* We may change the pad length before marshalling. */
	init_rpc_hdr_auth(pauth_out, DCERPC_AUTH_TYPE_SPNEGO, (int)auth_level, 0, 1);

	DEBUG(5, ("create_spnego_ntlmssp_auth_rpc_bind_req: Processing NTLMSSP Negotiate\n"));
	nt_status = ntlmssp_update(cli->auth->a_u.ntlmssp_state,
					null_blob,
					&request);

	if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		data_blob_free(&request);
		prs_mem_free(auth_data);
		return nt_status;
	}

	/* Wrap this in SPNEGO. */
	spnego_msg = gen_negTokenInit(OID_NTLMSSP, request);

	data_blob_free(&request);

	/* Auth len in the rpc header doesn't include auth_header. */
	if (!prs_copy_data_in(auth_data, (char *)spnego_msg.data, spnego_msg.length)) {
		data_blob_free(&spnego_msg);
		prs_mem_free(auth_data);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5, ("create_spnego_ntlmssp_auth_rpc_bind_req: NTLMSSP Negotiate:\n"));
	dump_data(5, spnego_msg.data, spnego_msg.length);

	data_blob_free(&spnego_msg);
	return NT_STATUS_OK;
}

/*******************************************************************
 Creates NTLMSSP auth bind.
 ********************************************************************/

static NTSTATUS create_ntlmssp_auth_rpc_bind_req( struct rpc_pipe_client *cli,
						enum dcerpc_AuthLevel auth_level,
						RPC_HDR_AUTH *pauth_out,
						prs_struct *auth_data)
{
	NTSTATUS nt_status;
	DATA_BLOB null_blob = data_blob_null;
	DATA_BLOB request = data_blob_null;

	/* We may change the pad length before marshalling. */
	init_rpc_hdr_auth(pauth_out, DCERPC_AUTH_TYPE_NTLMSSP, (int)auth_level, 0, 1);

	DEBUG(5, ("create_ntlmssp_auth_rpc_bind_req: Processing NTLMSSP Negotiate\n"));
	nt_status = ntlmssp_update(cli->auth->a_u.ntlmssp_state,
					null_blob,
					&request);

	if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		data_blob_free(&request);
		prs_mem_free(auth_data);
		return nt_status;
	}

	/* Auth len in the rpc header doesn't include auth_header. */
	if (!prs_copy_data_in(auth_data, (char *)request.data, request.length)) {
		data_blob_free(&request);
		prs_mem_free(auth_data);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5, ("create_ntlmssp_auth_rpc_bind_req: NTLMSSP Negotiate:\n"));
	dump_data(5, request.data, request.length);

	data_blob_free(&request);
	return NT_STATUS_OK;
}

/*******************************************************************
 Creates schannel auth bind.
 ********************************************************************/

static NTSTATUS create_schannel_auth_rpc_bind_req( struct rpc_pipe_client *cli,
						enum dcerpc_AuthLevel auth_level,
						RPC_HDR_AUTH *pauth_out,
						prs_struct *auth_data)
{
	struct NL_AUTH_MESSAGE r;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	/* We may change the pad length before marshalling. */
	init_rpc_hdr_auth(pauth_out, DCERPC_AUTH_TYPE_SCHANNEL, (int)auth_level, 0, 1);

	/* Use lp_workgroup() if domain not specified */

	if (!cli->auth->domain || !cli->auth->domain[0]) {
		cli->auth->domain = talloc_strdup(cli, lp_workgroup());
		if (cli->auth->domain == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	/*
	 * Now marshall the data into the auth parse_struct.
	 */

	r.MessageType			= NL_NEGOTIATE_REQUEST;
	r.Flags				= NL_FLAG_OEM_NETBIOS_DOMAIN_NAME |
					  NL_FLAG_OEM_NETBIOS_COMPUTER_NAME;
	r.oem_netbios_domain.a		= cli->auth->domain;
	r.oem_netbios_computer.a	= global_myname();

	ndr_err = ndr_push_struct_blob(&blob, talloc_tos(), NULL, &r,
		       (ndr_push_flags_fn_t)ndr_push_NL_AUTH_MESSAGE);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("Failed to marshall NL_AUTH_MESSAGE.\n"));
		prs_mem_free(auth_data);
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(NL_AUTH_MESSAGE, &r);
	}

	if (!prs_copy_data_in(auth_data, (const char *)blob.data, blob.length))
	{
		prs_mem_free(auth_data);
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates the internals of a DCE/RPC bind request or alter context PDU.
 ********************************************************************/

static NTSTATUS create_bind_or_alt_ctx_internal(enum dcerpc_pkt_type pkt_type,
						prs_struct *rpc_out, 
						uint32 rpc_call_id,
						const struct ndr_syntax_id *abstract,
						const struct ndr_syntax_id *transfer,
						RPC_HDR_AUTH *phdr_auth,
						prs_struct *pauth_info)
{
	RPC_HDR hdr;
	RPC_HDR_RB hdr_rb;
	RPC_CONTEXT rpc_ctx;
	uint16 auth_len = prs_offset(pauth_info);
	uint8 ss_padding_len = 0;
	uint16 frag_len = 0;

	/* create the RPC context. */
	init_rpc_context(&rpc_ctx, 0 /* context id */, abstract, transfer);

	/* create the bind request RPC_HDR_RB */
	init_rpc_hdr_rb(&hdr_rb, RPC_MAX_PDU_FRAG_LEN, RPC_MAX_PDU_FRAG_LEN, 0x0, &rpc_ctx);

	/* Start building the frag length. */
	frag_len = RPC_HEADER_LEN + RPC_HDR_RB_LEN(&hdr_rb);

	/* Do we need to pad ? */
	if (auth_len) {
		uint16 data_len = RPC_HEADER_LEN + RPC_HDR_RB_LEN(&hdr_rb);
		if (data_len % 8) {
			ss_padding_len = 8 - (data_len % 8);
			phdr_auth->auth_pad_len = ss_padding_len;
		}
		frag_len += RPC_HDR_AUTH_LEN + auth_len + ss_padding_len;
	}

	/* Create the request RPC_HDR */
	init_rpc_hdr(&hdr, pkt_type, DCERPC_PFC_FLAG_FIRST|DCERPC_PFC_FLAG_LAST, rpc_call_id, frag_len, auth_len);

	/* Marshall the RPC header */
	if(!smb_io_rpc_hdr("hdr"   , &hdr, rpc_out, 0)) {
		DEBUG(0,("create_bind_or_alt_ctx_internal: failed to marshall RPC_HDR.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Marshall the bind request data */
	if(!smb_io_rpc_hdr_rb("", &hdr_rb, rpc_out, 0)) {
		DEBUG(0,("create_bind_or_alt_ctx_internal: failed to marshall RPC_HDR_RB.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * Grow the outgoing buffer to store any auth info.
	 */

	if(auth_len != 0) {
		if (ss_padding_len) {
			char pad[8];
			memset(pad, '\0', 8);
			if (!prs_copy_data_in(rpc_out, pad, ss_padding_len)) {
				DEBUG(0,("create_bind_or_alt_ctx_internal: failed to marshall padding.\n"));
				return NT_STATUS_NO_MEMORY;
			}
		}

		if(!smb_io_rpc_hdr_auth("hdr_auth", phdr_auth, rpc_out, 0)) {
			DEBUG(0,("create_bind_or_alt_ctx_internal: failed to marshall RPC_HDR_AUTH.\n"));
			return NT_STATUS_NO_MEMORY;
		}


		if(!prs_append_prs_data( rpc_out, pauth_info)) {
			DEBUG(0,("create_bind_or_alt_ctx_internal: failed to grow parse struct to add auth.\n"));
			return NT_STATUS_NO_MEMORY;
		}
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates a DCE/RPC bind request.
 ********************************************************************/

static NTSTATUS create_rpc_bind_req(struct rpc_pipe_client *cli,
				prs_struct *rpc_out, 
				uint32 rpc_call_id,
				const struct ndr_syntax_id *abstract,
				const struct ndr_syntax_id *transfer,
				enum pipe_auth_type auth_type,
				enum dcerpc_AuthLevel auth_level)
{
	RPC_HDR_AUTH hdr_auth;
	prs_struct auth_info;
	NTSTATUS ret = NT_STATUS_OK;

	ZERO_STRUCT(hdr_auth);
	if (!prs_init(&auth_info, RPC_HDR_AUTH_LEN, prs_get_mem_context(rpc_out), MARSHALL))
		return NT_STATUS_NO_MEMORY;

	switch (auth_type) {
		case PIPE_AUTH_TYPE_SCHANNEL:
			ret = create_schannel_auth_rpc_bind_req(cli, auth_level, &hdr_auth, &auth_info);
			if (!NT_STATUS_IS_OK(ret)) {
				prs_mem_free(&auth_info);
				return ret;
			}
			break;

		case PIPE_AUTH_TYPE_NTLMSSP:
			ret = create_ntlmssp_auth_rpc_bind_req(cli, auth_level, &hdr_auth, &auth_info);
			if (!NT_STATUS_IS_OK(ret)) {
				prs_mem_free(&auth_info);
				return ret;
			}
			break;

		case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
			ret = create_spnego_ntlmssp_auth_rpc_bind_req(cli, auth_level, &hdr_auth, &auth_info);
			if (!NT_STATUS_IS_OK(ret)) {
				prs_mem_free(&auth_info);
				return ret;
			}
			break;

		case PIPE_AUTH_TYPE_KRB5:
			ret = create_krb5_auth_bind_req(cli, auth_level, &hdr_auth, &auth_info);
			if (!NT_STATUS_IS_OK(ret)) {
				prs_mem_free(&auth_info);
				return ret;
			}
			break;

		case PIPE_AUTH_TYPE_NONE:
			break;

		default:
			/* "Can't" happen. */
			return NT_STATUS_INVALID_INFO_CLASS;
	}

	ret = create_bind_or_alt_ctx_internal(DCERPC_PKT_BIND,
						rpc_out, 
						rpc_call_id,
						abstract,
						transfer,
						&hdr_auth,
						&auth_info);

	prs_mem_free(&auth_info);
	return ret;
}

/*******************************************************************
 Create and add the NTLMSSP sign/seal auth header and data.
 ********************************************************************/

static NTSTATUS add_ntlmssp_auth_footer(struct rpc_pipe_client *cli,
					RPC_HDR *phdr,
					uint32 ss_padding_len,
					prs_struct *outgoing_pdu)
{
	RPC_HDR_AUTH auth_info;
	NTSTATUS status;
	DATA_BLOB auth_blob = data_blob_null;
	uint16 data_and_pad_len = prs_offset(outgoing_pdu) - RPC_HEADER_LEN - RPC_HDR_RESP_LEN;

	if (!cli->auth->a_u.ntlmssp_state) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Init and marshall the auth header. */
	init_rpc_hdr_auth(&auth_info,
			map_pipe_auth_type_to_rpc_auth_type(
				cli->auth->auth_type),
			cli->auth->auth_level,
			ss_padding_len,
			1 /* context id. */);

	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, outgoing_pdu, 0)) {
		DEBUG(0,("add_ntlmssp_auth_footer: failed to marshall RPC_HDR_AUTH.\n"));
		data_blob_free(&auth_blob);
		return NT_STATUS_NO_MEMORY;
	}

	switch (cli->auth->auth_level) {
		case DCERPC_AUTH_LEVEL_PRIVACY:
			/* Data portion is encrypted. */
			status = ntlmssp_seal_packet(cli->auth->a_u.ntlmssp_state,
					(unsigned char *)prs_data_p(outgoing_pdu) + RPC_HEADER_LEN + RPC_HDR_RESP_LEN,
					data_and_pad_len,
					(unsigned char *)prs_data_p(outgoing_pdu),
					(size_t)prs_offset(outgoing_pdu),
					&auth_blob);
			if (!NT_STATUS_IS_OK(status)) {
				data_blob_free(&auth_blob);
				return status;
			}
			break;

		case DCERPC_AUTH_LEVEL_INTEGRITY:
			/* Data is signed. */
			status = ntlmssp_sign_packet(cli->auth->a_u.ntlmssp_state,
					(unsigned char *)prs_data_p(outgoing_pdu) + RPC_HEADER_LEN + RPC_HDR_RESP_LEN,
					data_and_pad_len,
					(unsigned char *)prs_data_p(outgoing_pdu),
					(size_t)prs_offset(outgoing_pdu),
					&auth_blob);
			if (!NT_STATUS_IS_OK(status)) {
				data_blob_free(&auth_blob);
				return status;
			}
			break;

		default:
			/* Can't happen. */
			smb_panic("bad auth level");
			/* Notreached. */
			return NT_STATUS_INVALID_PARAMETER;
	}

	/* Finally marshall the blob. */

	if (!prs_copy_data_in(outgoing_pdu, (const char *)auth_blob.data, NTLMSSP_SIG_SIZE)) {
		DEBUG(0,("add_ntlmssp_auth_footer: failed to add %u bytes auth blob.\n",
			(unsigned int)NTLMSSP_SIG_SIZE));
		data_blob_free(&auth_blob);
		return NT_STATUS_NO_MEMORY;
	}

	data_blob_free(&auth_blob);
	return NT_STATUS_OK;
}

/*******************************************************************
 Create and add the schannel sign/seal auth header and data.
 ********************************************************************/

static NTSTATUS add_schannel_auth_footer(struct rpc_pipe_client *cli,
					RPC_HDR *phdr,
					uint32 ss_padding_len,
					prs_struct *outgoing_pdu)
{
	RPC_HDR_AUTH auth_info;
	struct schannel_state *sas = cli->auth->a_u.schannel_auth;
	char *data_p = prs_data_p(outgoing_pdu) + RPC_HEADER_LEN + RPC_HDR_RESP_LEN;
	size_t data_and_pad_len = prs_offset(outgoing_pdu) - RPC_HEADER_LEN - RPC_HDR_RESP_LEN;
	DATA_BLOB blob;
	NTSTATUS status;

	if (!sas) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Init and marshall the auth header. */
	init_rpc_hdr_auth(&auth_info,
			map_pipe_auth_type_to_rpc_auth_type(cli->auth->auth_type),
			cli->auth->auth_level,
			ss_padding_len,
			1 /* context id. */);

	if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, outgoing_pdu, 0)) {
		DEBUG(0,("add_schannel_auth_footer: failed to marshall RPC_HDR_AUTH.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10,("add_schannel_auth_footer: SCHANNEL seq_num=%d\n",
			sas->seq_num));

	switch (cli->auth->auth_level) {
	case DCERPC_AUTH_LEVEL_PRIVACY:
		status = netsec_outgoing_packet(sas,
						talloc_tos(),
						true,
						(uint8_t *)data_p,
						data_and_pad_len,
						&blob);
		break;
	case DCERPC_AUTH_LEVEL_INTEGRITY:
		status = netsec_outgoing_packet(sas,
						talloc_tos(),
						false,
						(uint8_t *)data_p,
						data_and_pad_len,
						&blob);
		break;
	default:
		status = NT_STATUS_INTERNAL_ERROR;
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("add_schannel_auth_footer: failed to process packet: %s\n",
			nt_errstr(status)));
		return status;
	}

	if (DEBUGLEVEL >= 10) {
		dump_NL_AUTH_SIGNATURE(talloc_tos(), &blob);
	}

	/* Finally marshall the blob. */
	if (!prs_copy_data_in(outgoing_pdu, (const char *)blob.data, blob.length)) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Calculate how much data we're going to send in this packet, also
 work out any sign/seal padding length.
 ********************************************************************/

static uint32 calculate_data_len_tosend(struct rpc_pipe_client *cli,
					uint32 data_left,
					uint16 *p_frag_len,
					uint16 *p_auth_len,
					uint32 *p_ss_padding)
{
	uint32 data_space, data_len;

#if 0
	if ((data_left > 0) && (sys_random() % 2)) {
		data_left = MAX(data_left/2, 1);
	}
#endif

	switch (cli->auth->auth_level) {
		case DCERPC_AUTH_LEVEL_NONE:
		case DCERPC_AUTH_LEVEL_CONNECT:
			data_space = cli->max_xmit_frag - RPC_HEADER_LEN - RPC_HDR_REQ_LEN;
			data_len = MIN(data_space, data_left);
			*p_ss_padding = 0;
			*p_auth_len = 0;
			*p_frag_len = RPC_HEADER_LEN + RPC_HDR_REQ_LEN + data_len;
			return data_len;

		case DCERPC_AUTH_LEVEL_INTEGRITY:
		case DCERPC_AUTH_LEVEL_PRIVACY:
			/* Treat the same for all authenticated rpc requests. */
			switch(cli->auth->auth_type) {
				case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
				case PIPE_AUTH_TYPE_NTLMSSP:
					*p_auth_len = NTLMSSP_SIG_SIZE;
					break;
				case PIPE_AUTH_TYPE_SCHANNEL:
					*p_auth_len = RPC_AUTH_SCHANNEL_SIGN_OR_SEAL_CHK_LEN;
					break;
				default:
					smb_panic("bad auth type");
					break;
			}

			data_space = cli->max_xmit_frag - RPC_HEADER_LEN - RPC_HDR_REQ_LEN -
						RPC_HDR_AUTH_LEN - *p_auth_len;

			data_len = MIN(data_space, data_left);
			*p_ss_padding = 0;
			if (data_len % 8) {
				*p_ss_padding = 8 - (data_len % 8);
			}
			*p_frag_len = RPC_HEADER_LEN + RPC_HDR_REQ_LEN + 		/* Normal headers. */
					data_len + *p_ss_padding + 		/* data plus padding. */
					RPC_HDR_AUTH_LEN + *p_auth_len;		/* Auth header and auth data. */
			return data_len;

		default:
			smb_panic("bad auth level");
			/* Notreached. */
			return 0;
	}
}

/*******************************************************************
 External interface.
 Does an rpc request on a pipe. Incoming data is NDR encoded in in_data.
 Reply is NDR encoded in out_data. Splits the data stream into RPC PDU's
 and deals with signing/sealing details.
 ********************************************************************/

struct rpc_api_pipe_req_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	uint8_t op_num;
	uint32_t call_id;
	prs_struct *req_data;
	uint32_t req_data_sent;
	prs_struct outgoing_frag;
	prs_struct reply_pdu;
};

static int rpc_api_pipe_req_state_destructor(struct rpc_api_pipe_req_state *s)
{
	prs_mem_free(&s->outgoing_frag);
	prs_mem_free(&s->reply_pdu);
	return 0;
}

static void rpc_api_pipe_req_write_done(struct tevent_req *subreq);
static void rpc_api_pipe_req_done(struct tevent_req *subreq);
static NTSTATUS prepare_next_frag(struct rpc_api_pipe_req_state *state,
				  bool *is_last_frag);

struct tevent_req *rpc_api_pipe_req_send(TALLOC_CTX *mem_ctx,
					 struct event_context *ev,
					 struct rpc_pipe_client *cli,
					 uint8_t op_num,
					 prs_struct *req_data)
{
	struct tevent_req *req, *subreq;
	struct rpc_api_pipe_req_state *state;
	NTSTATUS status;
	bool is_last_frag;

	req = tevent_req_create(mem_ctx, &state,
				struct rpc_api_pipe_req_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->op_num = op_num;
	state->req_data = req_data;
	state->req_data_sent = 0;
	state->call_id = get_rpc_call_id();

	if (cli->max_xmit_frag
	    < RPC_HEADER_LEN + RPC_HDR_REQ_LEN + RPC_MAX_SIGN_SIZE) {
		/* Server is screwed up ! */
		status = NT_STATUS_INVALID_PARAMETER;
		goto post_status;
	}

	prs_init_empty(&state->reply_pdu, state, UNMARSHALL);

	if (!prs_init(&state->outgoing_frag, cli->max_xmit_frag,
		      state, MARSHALL)) {
		goto fail;
	}

	talloc_set_destructor(state, rpc_api_pipe_req_state_destructor);

	status = prepare_next_frag(state, &is_last_frag);
	if (!NT_STATUS_IS_OK(status)) {
		goto post_status;
	}

	if (is_last_frag) {
		subreq = rpc_api_pipe_send(state, ev, state->cli,
					   &state->outgoing_frag,
					   DCERPC_PKT_RESPONSE);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_done, req);
	} else {
		subreq = rpc_write_send(
			state, ev, cli->transport,
			(uint8_t *)prs_data_p(&state->outgoing_frag),
			prs_offset(&state->outgoing_frag));
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_write_done,
					req);
	}
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static NTSTATUS prepare_next_frag(struct rpc_api_pipe_req_state *state,
				  bool *is_last_frag)
{
	RPC_HDR hdr;
	RPC_HDR_REQ hdr_req;
	uint32_t data_sent_thistime;
	uint16_t auth_len;
	uint16_t frag_len;
	uint8_t flags = 0;
	uint32_t ss_padding;
	uint32_t data_left;
	char pad[8] = { 0, };
	NTSTATUS status;

	data_left = prs_offset(state->req_data) - state->req_data_sent;

	data_sent_thistime = calculate_data_len_tosend(
		state->cli, data_left, &frag_len, &auth_len, &ss_padding);

	if (state->req_data_sent == 0) {
		flags = DCERPC_PFC_FLAG_FIRST;
	}

	if (data_sent_thistime == data_left) {
		flags |= DCERPC_PFC_FLAG_LAST;
	}

	if (!prs_set_offset(&state->outgoing_frag, 0)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Create and marshall the header and request header. */
	init_rpc_hdr(&hdr, DCERPC_PKT_REQUEST, flags, state->call_id, frag_len,
		     auth_len);

	if (!smb_io_rpc_hdr("hdr    ", &hdr, &state->outgoing_frag, 0)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Create the rpc request RPC_HDR_REQ */
	init_rpc_hdr_req(&hdr_req, prs_offset(state->req_data),
			 state->op_num);

	if (!smb_io_rpc_hdr_req("hdr_req", &hdr_req,
				&state->outgoing_frag, 0)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Copy in the data, plus any ss padding. */
	if (!prs_append_some_prs_data(&state->outgoing_frag,
				      state->req_data, state->req_data_sent,
				      data_sent_thistime)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Copy the sign/seal padding data. */
	if (!prs_copy_data_in(&state->outgoing_frag, pad, ss_padding)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* Generate any auth sign/seal and add the auth footer. */
	switch (state->cli->auth->auth_type) {
	case PIPE_AUTH_TYPE_NONE:
		status = NT_STATUS_OK;
		break;
	case PIPE_AUTH_TYPE_NTLMSSP:
	case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
		status = add_ntlmssp_auth_footer(state->cli, &hdr, ss_padding,
						 &state->outgoing_frag);
		break;
	case PIPE_AUTH_TYPE_SCHANNEL:
		status = add_schannel_auth_footer(state->cli, &hdr, ss_padding,
						  &state->outgoing_frag);
		break;
	default:
		status = NT_STATUS_INVALID_PARAMETER;
		break;
	}

	state->req_data_sent += data_sent_thistime;
	*is_last_frag = ((flags & DCERPC_PFC_FLAG_LAST) != 0);

	return status;
}

static void rpc_api_pipe_req_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_req_state *state = tevent_req_data(
		req, struct rpc_api_pipe_req_state);
	NTSTATUS status;
	bool is_last_frag;

	status = rpc_write_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	status = prepare_next_frag(state, &is_last_frag);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (is_last_frag) {
		subreq = rpc_api_pipe_send(state, state->ev, state->cli,
					   &state->outgoing_frag,
					   DCERPC_PKT_RESPONSE);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_done, req);
	} else {
		subreq = rpc_write_send(
			state, state->ev,
			state->cli->transport,
			(uint8_t *)prs_data_p(&state->outgoing_frag),
			prs_offset(&state->outgoing_frag));
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, rpc_api_pipe_req_write_done,
					req);
	}
}

static void rpc_api_pipe_req_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_api_pipe_req_state *state = tevent_req_data(
		req, struct rpc_api_pipe_req_state);
	NTSTATUS status;

	status = rpc_api_pipe_recv(subreq, state, &state->reply_pdu);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS rpc_api_pipe_req_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			       prs_struct *reply_pdu)
{
	struct rpc_api_pipe_req_state *state = tevent_req_data(
		req, struct rpc_api_pipe_req_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		/*
		 * We always have to initialize to reply pdu, even if there is
		 * none. The rpccli_* caller routines expect this.
		 */
		prs_init_empty(reply_pdu, mem_ctx, UNMARSHALL);
		return status;
	}

	*reply_pdu = state->reply_pdu;
	reply_pdu->mem_ctx = mem_ctx;

	/*
	 * Prevent state->req_pdu from being freed in
	 * rpc_api_pipe_req_state_destructor()
	 */
	prs_init_empty(&state->reply_pdu, state, UNMARSHALL);

	return NT_STATUS_OK;
}

#if 0
/****************************************************************************
 Set the handle state.
****************************************************************************/

static bool rpc_pipe_set_hnd_state(struct rpc_pipe_client *cli,
				   const char *pipe_name, uint16 device_state)
{
	bool state_set = False;
	char param[2];
	uint16 setup[2]; /* only need 2 uint16 setup parameters */
	char *rparam = NULL;
	char *rdata = NULL;
	uint32 rparam_len, rdata_len;

	if (pipe_name == NULL)
		return False;

	DEBUG(5,("Set Handle state Pipe[%x]: %s - device state:%x\n",
		 cli->fnum, pipe_name, device_state));

	/* create parameters: device state */
	SSVAL(param, 0, device_state);

	/* create setup parameters. */
	setup[0] = 0x0001; 
	setup[1] = cli->fnum; /* pipe file handle.  got this from an SMBOpenX. */

	/* send the data on \PIPE\ */
	if (cli_api_pipe(cli->cli, "\\PIPE\\",
	            setup, 2, 0,                /* setup, length, max */
	            param, 2, 0,                /* param, length, max */
	            NULL, 0, 1024,              /* data, length, max */
	            &rparam, &rparam_len,        /* return param, length */
	            &rdata, &rdata_len))         /* return data, length */
	{
		DEBUG(5, ("Set Handle state: return OK\n"));
		state_set = True;
	}

	SAFE_FREE(rparam);
	SAFE_FREE(rdata);

	return state_set;
}
#endif

/****************************************************************************
 Check the rpc bind acknowledge response.
****************************************************************************/

static bool check_bind_response(RPC_HDR_BA *hdr_ba,
				const struct ndr_syntax_id *transfer)
{
	if ( hdr_ba->addr.len == 0) {
		DEBUG(4,("Ignoring length check -- ASU bug (server didn't fill in the pipe name correctly)"));
	}

	/* check the transfer syntax */
	if ((hdr_ba->transfer.if_version != transfer->if_version) ||
	     (memcmp(&hdr_ba->transfer.uuid, &transfer->uuid, sizeof(transfer->uuid)) !=0)) {
		DEBUG(2,("bind_rpc_pipe: transfer syntax differs\n"));
		return False;
	}

	if (hdr_ba->res.num_results != 0x1 || hdr_ba->res.result != 0) {
		DEBUG(2,("bind_rpc_pipe: bind denied results: %d reason: %x\n",
		          hdr_ba->res.num_results, hdr_ba->res.reason));
	}

	DEBUG(5,("check_bind_response: accepted!\n"));
	return True;
}

/*******************************************************************
 Creates a DCE/RPC bind authentication response.
 This is the packet that is sent back to the server once we
 have received a BIND-ACK, to finish the third leg of
 the authentication handshake.
 ********************************************************************/

static NTSTATUS create_rpc_bind_auth3(struct rpc_pipe_client *cli,
				uint32 rpc_call_id,
				enum pipe_auth_type auth_type,
				enum dcerpc_AuthLevel auth_level,
				DATA_BLOB *pauth_blob,
				prs_struct *rpc_out)
{
	RPC_HDR hdr;
	RPC_HDR_AUTH hdr_auth;
	uint32 pad = 0;

	/* Create the request RPC_HDR */
	init_rpc_hdr(&hdr, DCERPC_PKT_AUTH3, DCERPC_PFC_FLAG_FIRST|DCERPC_PFC_FLAG_LAST, rpc_call_id,
		     RPC_HEADER_LEN + 4 /* pad */ + RPC_HDR_AUTH_LEN + pauth_blob->length,
		     pauth_blob->length );

	/* Marshall it. */
	if(!smb_io_rpc_hdr("hdr", &hdr, rpc_out, 0)) {
		DEBUG(0,("create_rpc_bind_auth3: failed to marshall RPC_HDR.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/*
		I'm puzzled about this - seems to violate the DCE RPC auth rules,
		about padding - shouldn't this pad to length 8 ? JRA.
	*/

	/* 4 bytes padding. */
	if (!prs_uint32("pad", rpc_out, 0, &pad)) {
		DEBUG(0,("create_rpc_bind_auth3: failed to marshall 4 byte pad.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/* Create the request RPC_HDR_AUTHA */
	init_rpc_hdr_auth(&hdr_auth,
			map_pipe_auth_type_to_rpc_auth_type(auth_type),
			auth_level, 0, 1);

	if(!smb_io_rpc_hdr_auth("hdr_auth", &hdr_auth, rpc_out, 0)) {
		DEBUG(0,("create_rpc_bind_auth3: failed to marshall RPC_HDR_AUTHA.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * Append the auth data to the outgoing buffer.
	 */

	if(!prs_copy_data_in(rpc_out, (char *)pauth_blob->data, pauth_blob->length)) {
		DEBUG(0,("create_rpc_bind_auth3: failed to marshall auth blob.\n"));
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
 Creates a DCE/RPC bind alter context authentication request which
 may contain a spnego auth blobl
 ********************************************************************/

static NTSTATUS create_rpc_alter_context(uint32 rpc_call_id,
					const struct ndr_syntax_id *abstract,
					const struct ndr_syntax_id *transfer,
					enum dcerpc_AuthLevel auth_level,
					const DATA_BLOB *pauth_blob, /* spnego auth blob already created. */
					prs_struct *rpc_out)
{
	RPC_HDR_AUTH hdr_auth;
	prs_struct auth_info;
	NTSTATUS ret = NT_STATUS_OK;

	ZERO_STRUCT(hdr_auth);
	if (!prs_init(&auth_info, RPC_HDR_AUTH_LEN, prs_get_mem_context(rpc_out), MARSHALL))
		return NT_STATUS_NO_MEMORY;

	/* We may change the pad length before marshalling. */
	init_rpc_hdr_auth(&hdr_auth, DCERPC_AUTH_TYPE_SPNEGO, (int)auth_level, 0, 1);

	if (pauth_blob->length) {
		if (!prs_copy_data_in(&auth_info, (const char *)pauth_blob->data, pauth_blob->length)) {
			prs_mem_free(&auth_info);
			return NT_STATUS_NO_MEMORY;
		}
	}

	ret = create_bind_or_alt_ctx_internal(DCERPC_PKT_ALTER,
						rpc_out, 
						rpc_call_id,
						abstract,
						transfer,
						&hdr_auth,
						&auth_info);
	prs_mem_free(&auth_info);
	return ret;
}

/****************************************************************************
 Do an rpc bind.
****************************************************************************/

struct rpc_pipe_bind_state {
	struct event_context *ev;
	struct rpc_pipe_client *cli;
	prs_struct rpc_out;
	uint32_t rpc_call_id;
};

static int rpc_pipe_bind_state_destructor(struct rpc_pipe_bind_state *state)
{
	prs_mem_free(&state->rpc_out);
	return 0;
}

static void rpc_pipe_bind_step_one_done(struct tevent_req *subreq);
static NTSTATUS rpc_finish_auth3_bind_send(struct tevent_req *req,
					   struct rpc_pipe_bind_state *state,
					   struct rpc_hdr_info *phdr,
					   prs_struct *reply_pdu);
static void rpc_bind_auth3_write_done(struct tevent_req *subreq);
static NTSTATUS rpc_finish_spnego_ntlmssp_bind_send(struct tevent_req *req,
						    struct rpc_pipe_bind_state *state,
						    struct rpc_hdr_info *phdr,
						    prs_struct *reply_pdu);
static void rpc_bind_ntlmssp_api_done(struct tevent_req *subreq);

struct tevent_req *rpc_pipe_bind_send(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct rpc_pipe_client *cli,
				      struct cli_pipe_auth_data *auth)
{
	struct tevent_req *req, *subreq;
	struct rpc_pipe_bind_state *state;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct rpc_pipe_bind_state);
	if (req == NULL) {
		return NULL;
	}

	DEBUG(5,("Bind RPC Pipe: %s auth_type %u, auth_level %u\n",
		rpccli_pipe_txt(talloc_tos(), cli),
		(unsigned int)auth->auth_type,
		(unsigned int)auth->auth_level ));

	state->ev = ev;
	state->cli = cli;
	state->rpc_call_id = get_rpc_call_id();

	prs_init_empty(&state->rpc_out, state, MARSHALL);
	talloc_set_destructor(state, rpc_pipe_bind_state_destructor);

	cli->auth = talloc_move(cli, &auth);

	/* Marshall the outgoing data. */
	status = create_rpc_bind_req(cli, &state->rpc_out,
				     state->rpc_call_id,
				     &cli->abstract_syntax,
				     &cli->transfer_syntax,
				     cli->auth->auth_type,
				     cli->auth->auth_level);

	if (!NT_STATUS_IS_OK(status)) {
		goto post_status;
	}

	subreq = rpc_api_pipe_send(state, ev, cli, &state->rpc_out,
				   DCERPC_PKT_BIND_ACK);
	if (subreq == NULL) {
		goto fail;
	}
	tevent_req_set_callback(subreq, rpc_pipe_bind_step_one_done, req);
	return req;

 post_status:
	tevent_req_nterror(req, status);
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void rpc_pipe_bind_step_one_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_pipe_bind_state *state = tevent_req_data(
		req, struct rpc_pipe_bind_state);
	prs_struct reply_pdu;
	struct rpc_hdr_info hdr;
	struct rpc_hdr_ba_info hdr_ba;
	NTSTATUS status;

	status = rpc_api_pipe_recv(subreq, talloc_tos(), &reply_pdu);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("rpc_pipe_bind: %s bind request returned %s\n",
			  rpccli_pipe_txt(talloc_tos(), state->cli),
			  nt_errstr(status)));
		tevent_req_nterror(req, status);
		return;
	}

	/* Unmarshall the RPC header */
	if (!smb_io_rpc_hdr("hdr", &hdr, &reply_pdu, 0)) {
		DEBUG(0, ("rpc_pipe_bind: failed to unmarshall RPC_HDR.\n"));
		prs_mem_free(&reply_pdu);
		tevent_req_nterror(req, NT_STATUS_BUFFER_TOO_SMALL);
		return;
	}

	if (!smb_io_rpc_hdr_ba("", &hdr_ba, &reply_pdu, 0)) {
		DEBUG(0, ("rpc_pipe_bind: Failed to unmarshall "
			  "RPC_HDR_BA.\n"));
		prs_mem_free(&reply_pdu);
		tevent_req_nterror(req, NT_STATUS_BUFFER_TOO_SMALL);
		return;
	}

	if (!check_bind_response(&hdr_ba, &state->cli->transfer_syntax)) {
		DEBUG(2, ("rpc_pipe_bind: check_bind_response failed.\n"));
		prs_mem_free(&reply_pdu);
		tevent_req_nterror(req, NT_STATUS_BUFFER_TOO_SMALL);
		return;
	}

	state->cli->max_xmit_frag = hdr_ba.bba.max_tsize;
	state->cli->max_recv_frag = hdr_ba.bba.max_rsize;

	/*
	 * For authenticated binds we may need to do 3 or 4 leg binds.
	 */

	switch(state->cli->auth->auth_type) {

	case PIPE_AUTH_TYPE_NONE:
	case PIPE_AUTH_TYPE_SCHANNEL:
		/* Bind complete. */
		prs_mem_free(&reply_pdu);
		tevent_req_done(req);
		break;

	case PIPE_AUTH_TYPE_NTLMSSP:
		/* Need to send AUTH3 packet - no reply. */
		status = rpc_finish_auth3_bind_send(req, state, &hdr,
						    &reply_pdu);
		prs_mem_free(&reply_pdu);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
		}
		break;

	case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
		/* Need to send alter context request and reply. */
		status = rpc_finish_spnego_ntlmssp_bind_send(req, state, &hdr,
							     &reply_pdu);
		prs_mem_free(&reply_pdu);
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_nterror(req, status);
		}
		break;

	case PIPE_AUTH_TYPE_KRB5:
		/* */

	default:
		DEBUG(0,("cli_finish_bind_auth: unknown auth type %u\n",
			 (unsigned int)state->cli->auth->auth_type));
		prs_mem_free(&reply_pdu);
		tevent_req_nterror(req, NT_STATUS_INTERNAL_ERROR);
	}
}

static NTSTATUS rpc_finish_auth3_bind_send(struct tevent_req *req,
					   struct rpc_pipe_bind_state *state,
					   struct rpc_hdr_info *phdr,
					   prs_struct *reply_pdu)
{
	DATA_BLOB server_response = data_blob_null;
	DATA_BLOB client_reply = data_blob_null;
	struct rpc_hdr_auth_info hdr_auth;
	struct tevent_req *subreq;
	NTSTATUS status;

	if ((phdr->auth_len == 0)
	    || (phdr->frag_len < phdr->auth_len + RPC_HDR_AUTH_LEN)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!prs_set_offset(
		    reply_pdu,
		    phdr->frag_len - phdr->auth_len - RPC_HDR_AUTH_LEN)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!smb_io_rpc_hdr_auth("hdr_auth", &hdr_auth, reply_pdu, 0)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* TODO - check auth_type/auth_level match. */

	server_response = data_blob_talloc(talloc_tos(), NULL, phdr->auth_len);
	prs_copy_data_out((char *)server_response.data, reply_pdu,
			  phdr->auth_len);

	status = ntlmssp_update(state->cli->auth->a_u.ntlmssp_state,
				server_response, &client_reply);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpc_finish_auth3_bind: NTLMSSP update using server "
			  "blob failed: %s.\n", nt_errstr(status)));
		return status;
	}

	prs_init_empty(&state->rpc_out, talloc_tos(), MARSHALL);

	status = create_rpc_bind_auth3(state->cli, state->rpc_call_id,
				       state->cli->auth->auth_type,
				       state->cli->auth->auth_level,
				       &client_reply, &state->rpc_out);
	data_blob_free(&client_reply);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	subreq = rpc_write_send(state, state->ev, state->cli->transport,
				(uint8_t *)prs_data_p(&state->rpc_out),
				prs_offset(&state->rpc_out));
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, rpc_bind_auth3_write_done, req);
	return NT_STATUS_OK;
}

static void rpc_bind_auth3_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = rpc_write_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

static NTSTATUS rpc_finish_spnego_ntlmssp_bind_send(struct tevent_req *req,
						    struct rpc_pipe_bind_state *state,
						    struct rpc_hdr_info *phdr,
						    prs_struct *reply_pdu)
{
	DATA_BLOB server_spnego_response = data_blob_null;
	DATA_BLOB server_ntlm_response = data_blob_null;
	DATA_BLOB client_reply = data_blob_null;
	DATA_BLOB tmp_blob = data_blob_null;
	RPC_HDR_AUTH hdr_auth;
	struct tevent_req *subreq;
	NTSTATUS status;

	if ((phdr->auth_len == 0)
	    || (phdr->frag_len < phdr->auth_len + RPC_HDR_AUTH_LEN)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Process the returned NTLMSSP blob first. */
	if (!prs_set_offset(
		    reply_pdu,
		    phdr->frag_len - phdr->auth_len - RPC_HDR_AUTH_LEN)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!smb_io_rpc_hdr_auth("hdr_auth", &hdr_auth, reply_pdu, 0)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	server_spnego_response = data_blob(NULL, phdr->auth_len);
	prs_copy_data_out((char *)server_spnego_response.data,
			  reply_pdu, phdr->auth_len);

	/*
	 * The server might give us back two challenges - tmp_blob is for the
	 * second.
	 */
	if (!spnego_parse_challenge(server_spnego_response,
				    &server_ntlm_response, &tmp_blob)) {
		data_blob_free(&server_spnego_response);
		data_blob_free(&server_ntlm_response);
		data_blob_free(&tmp_blob);
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* We're finished with the server spnego response and the tmp_blob. */
	data_blob_free(&server_spnego_response);
	data_blob_free(&tmp_blob);

	status = ntlmssp_update(state->cli->auth->a_u.ntlmssp_state,
				server_ntlm_response, &client_reply);

	/* Finished with the server_ntlm response */
	data_blob_free(&server_ntlm_response);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpc_finish_spnego_ntlmssp_bind: NTLMSSP update "
			  "using server blob failed.\n"));
		data_blob_free(&client_reply);
		return status;
	}

	/* SPNEGO wrap the client reply. */
	tmp_blob = spnego_gen_auth(client_reply);
	data_blob_free(&client_reply);
	client_reply = tmp_blob;
	tmp_blob = data_blob_null;

	/* Now prepare the alter context pdu. */
	prs_init_empty(&state->rpc_out, state, MARSHALL);

	status = create_rpc_alter_context(state->rpc_call_id,
					  &state->cli->abstract_syntax,
					  &state->cli->transfer_syntax,
					  state->cli->auth->auth_level,
					  &client_reply,
					  &state->rpc_out);
	data_blob_free(&client_reply);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	subreq = rpc_api_pipe_send(state, state->ev, state->cli,
				   &state->rpc_out, DCERPC_PKT_ALTER_RESP);
	if (subreq == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, rpc_bind_ntlmssp_api_done, req);
	return NT_STATUS_OK;
}

static void rpc_bind_ntlmssp_api_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct rpc_pipe_bind_state *state = tevent_req_data(
		req, struct rpc_pipe_bind_state);
	DATA_BLOB server_spnego_response = data_blob_null;
	DATA_BLOB tmp_blob = data_blob_null;
	prs_struct reply_pdu;
	struct rpc_hdr_info hdr;
	struct rpc_hdr_auth_info hdr_auth;
	NTSTATUS status;

	status = rpc_api_pipe_recv(subreq, talloc_tos(), &reply_pdu);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	/* Get the auth blob from the reply. */
	if (!smb_io_rpc_hdr("rpc_hdr   ", &hdr, &reply_pdu, 0)) {
		DEBUG(0, ("rpc_finish_spnego_ntlmssp_bind: Failed to "
			  "unmarshall RPC_HDR.\n"));
		tevent_req_nterror(req, NT_STATUS_BUFFER_TOO_SMALL);
		return;
	}

	if (!prs_set_offset(
		    &reply_pdu,
		    hdr.frag_len - hdr.auth_len - RPC_HDR_AUTH_LEN)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if (!smb_io_rpc_hdr_auth("hdr_auth", &hdr_auth, &reply_pdu, 0)) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	server_spnego_response = data_blob(NULL, hdr.auth_len);
	prs_copy_data_out((char *)server_spnego_response.data, &reply_pdu,
			  hdr.auth_len);

	/* Check we got a valid auth response. */
	if (!spnego_parse_auth_response(server_spnego_response, NT_STATUS_OK,
					OID_NTLMSSP, &tmp_blob)) {
		data_blob_free(&server_spnego_response);
		data_blob_free(&tmp_blob);
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	data_blob_free(&server_spnego_response);
	data_blob_free(&tmp_blob);

	DEBUG(5,("rpc_finish_spnego_ntlmssp_bind: alter context request to "
		 "%s.\n", rpccli_pipe_txt(talloc_tos(), state->cli)));
	tevent_req_done(req);
}

NTSTATUS rpc_pipe_bind_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS rpc_pipe_bind(struct rpc_pipe_client *cli,
		       struct cli_pipe_auth_data *auth)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = rpc_pipe_bind_send(frame, ev, cli, auth);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = rpc_pipe_bind_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

#define RPCCLI_DEFAULT_TIMEOUT 10000 /* 10 seconds. */

unsigned int rpccli_set_timeout(struct rpc_pipe_client *rpc_cli,
				unsigned int timeout)
{
	unsigned int old;

	if (rpc_cli->transport == NULL) {
		return RPCCLI_DEFAULT_TIMEOUT;
	}

	if (rpc_cli->transport->set_timeout == NULL) {
		return RPCCLI_DEFAULT_TIMEOUT;
	}

	old = rpc_cli->transport->set_timeout(rpc_cli->transport->priv, timeout);
	if (old == 0) {
		return RPCCLI_DEFAULT_TIMEOUT;
	}

	return old;
}

bool rpccli_is_connected(struct rpc_pipe_client *rpc_cli)
{
	if (rpc_cli == NULL) {
		return false;
	}

	if (rpc_cli->transport == NULL) {
		return false;
	}

	return rpc_cli->transport->is_connected(rpc_cli->transport->priv);
}

bool rpccli_get_pwd_hash(struct rpc_pipe_client *rpc_cli, uint8_t nt_hash[16])
{
	struct cli_state *cli;

	if ((rpc_cli->auth->auth_type == PIPE_AUTH_TYPE_NTLMSSP)
	    || (rpc_cli->auth->auth_type == PIPE_AUTH_TYPE_SPNEGO_NTLMSSP)) {
		memcpy(nt_hash, rpc_cli->auth->a_u.ntlmssp_state->nt_hash, 16);
		return true;
	}

	cli = rpc_pipe_np_smb_conn(rpc_cli);
	if (cli == NULL) {
		return false;
	}
	E_md4hash(cli->password ? cli->password : "", nt_hash);
	return true;
}

NTSTATUS rpccli_anon_bind_data(TALLOC_CTX *mem_ctx,
			       struct cli_pipe_auth_data **presult)
{
	struct cli_pipe_auth_data *result;

	result = talloc(mem_ctx, struct cli_pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = PIPE_AUTH_TYPE_NONE;
	result->auth_level = DCERPC_AUTH_LEVEL_NONE;

	result->user_name = talloc_strdup(result, "");
	result->domain = talloc_strdup(result, "");
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	*presult = result;
	return NT_STATUS_OK;
}

static int cli_auth_ntlmssp_data_destructor(struct cli_pipe_auth_data *auth)
{
	ntlmssp_end(&auth->a_u.ntlmssp_state);
	return 0;
}

static NTSTATUS rpccli_ntlmssp_bind_data(TALLOC_CTX *mem_ctx,
				  enum pipe_auth_type auth_type,
				  enum dcerpc_AuthLevel auth_level,
				  const char *domain,
				  const char *username,
				  const char *password,
				  struct cli_pipe_auth_data **presult)
{
	struct cli_pipe_auth_data *result;
	NTSTATUS status;

	result = talloc(mem_ctx, struct cli_pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = auth_type;
	result->auth_level = auth_level;

	result->user_name = talloc_strdup(result, username);
	result->domain = talloc_strdup(result, domain);
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	status = ntlmssp_client_start(&result->a_u.ntlmssp_state);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	talloc_set_destructor(result, cli_auth_ntlmssp_data_destructor);

	status = ntlmssp_set_username(result->a_u.ntlmssp_state, username);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = ntlmssp_set_domain(result->a_u.ntlmssp_state, domain);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = ntlmssp_set_password(result->a_u.ntlmssp_state, password);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	/*
	 * Turn off sign+seal to allow selected auth level to turn it back on.
	 */
	result->a_u.ntlmssp_state->neg_flags &=
		~(NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_SEAL);

	if (auth_level == DCERPC_AUTH_LEVEL_INTEGRITY) {
		result->a_u.ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SIGN;
	} else if (auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
		result->a_u.ntlmssp_state->neg_flags
			|= NTLMSSP_NEGOTIATE_SEAL | NTLMSSP_NEGOTIATE_SIGN;
	}

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return status;
}

NTSTATUS rpccli_schannel_bind_data(TALLOC_CTX *mem_ctx, const char *domain,
				   enum dcerpc_AuthLevel auth_level,
				   struct netlogon_creds_CredentialState *creds,
				   struct cli_pipe_auth_data **presult)
{
	struct cli_pipe_auth_data *result;

	result = talloc(mem_ctx, struct cli_pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = PIPE_AUTH_TYPE_SCHANNEL;
	result->auth_level = auth_level;

	result->user_name = talloc_strdup(result, "");
	result->domain = talloc_strdup(result, domain);
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		goto fail;
	}

	result->a_u.schannel_auth = talloc(result, struct schannel_state);
	if (result->a_u.schannel_auth == NULL) {
		goto fail;
	}

	result->a_u.schannel_auth->state = SCHANNEL_STATE_START;
	result->a_u.schannel_auth->seq_num = 0;
	result->a_u.schannel_auth->initiator = true;
	result->a_u.schannel_auth->creds = netlogon_creds_copy(result, creds);

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return NT_STATUS_NO_MEMORY;
}

#ifdef HAVE_KRB5
static int cli_auth_kerberos_data_destructor(struct kerberos_auth_struct *auth)
{
	data_blob_free(&auth->session_key);
	return 0;
}
#endif

static NTSTATUS rpccli_kerberos_bind_data(TALLOC_CTX *mem_ctx,
				   enum dcerpc_AuthLevel auth_level,
				   const char *service_princ,
				   const char *username,
				   const char *password,
				   struct cli_pipe_auth_data **presult)
{
#ifdef HAVE_KRB5
	struct cli_pipe_auth_data *result;

	if ((username != NULL) && (password != NULL)) {
		int ret = kerberos_kinit_password(username, password, 0, NULL);
		if (ret != 0) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	result = talloc(mem_ctx, struct cli_pipe_auth_data);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->auth_type = PIPE_AUTH_TYPE_KRB5;
	result->auth_level = auth_level;

	/*
	 * Username / domain need fixing!
	 */
	result->user_name = talloc_strdup(result, "");
	result->domain = talloc_strdup(result, "");
	if ((result->user_name == NULL) || (result->domain == NULL)) {
		goto fail;
	}

	result->a_u.kerberos_auth = TALLOC_ZERO_P(
		result, struct kerberos_auth_struct);
	if (result->a_u.kerberos_auth == NULL) {
		goto fail;
	}
	talloc_set_destructor(result->a_u.kerberos_auth,
			      cli_auth_kerberos_data_destructor);

	result->a_u.kerberos_auth->service_principal = talloc_strdup(
		result, service_princ);
	if (result->a_u.kerberos_auth->service_principal == NULL) {
		goto fail;
	}

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return NT_STATUS_NO_MEMORY;
#else
	return NT_STATUS_NOT_SUPPORTED;
#endif
}

/**
 * Create an rpc pipe client struct, connecting to a tcp port.
 */
static NTSTATUS rpc_pipe_open_tcp_port(TALLOC_CTX *mem_ctx, const char *host,
				       uint16_t port,
				       const struct ndr_syntax_id *abstract_syntax,
				       struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct sockaddr_storage addr;
	NTSTATUS status;
	int fd;

	result = TALLOC_ZERO_P(mem_ctx, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;
	result->dispatch = cli_do_rpc_ndr;
	result->dispatch_send = cli_do_rpc_ndr_send;
	result->dispatch_recv = cli_do_rpc_ndr_recv;

	result->desthost = talloc_strdup(result, host);
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", result->desthost);
	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	if (!resolve_name(host, &addr, 0, false)) {
		status = NT_STATUS_NOT_FOUND;
		goto fail;
	}

	status = open_socket_out(&addr, port, 60, &fd);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	set_socket_options(fd, lp_socket_options());

	status = rpc_transport_sock_init(result, fd, &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		close(fd);
		goto fail;
	}

	result->transport->transport = NCACN_IP_TCP;

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return status;
}

/**
 * Determine the tcp port on which a dcerpc interface is listening
 * for the ncacn_ip_tcp transport via the endpoint mapper of the
 * target host.
 */
static NTSTATUS rpc_pipe_get_tcp_port(const char *host,
				      const struct ndr_syntax_id *abstract_syntax,
				      uint16_t *pport)
{
	NTSTATUS status;
	struct rpc_pipe_client *epm_pipe = NULL;
	struct cli_pipe_auth_data *auth = NULL;
	struct dcerpc_binding *map_binding = NULL;
	struct dcerpc_binding *res_binding = NULL;
	struct epm_twr_t *map_tower = NULL;
	struct epm_twr_t *res_towers = NULL;
	struct policy_handle *entry_handle = NULL;
	uint32_t num_towers = 0;
	uint32_t max_towers = 1;
	struct epm_twr_p_t towers;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();

	if (pport == NULL) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	}

	/* open the connection to the endpoint mapper */
	status = rpc_pipe_open_tcp_port(tmp_ctx, host, 135,
					&ndr_table_epmapper.syntax_id,
					&epm_pipe);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpccli_anon_bind_data(tmp_ctx, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = rpc_pipe_bind(epm_pipe, auth);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* create tower for asking the epmapper */

	map_binding = TALLOC_ZERO_P(tmp_ctx, struct dcerpc_binding);
	if (map_binding == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	map_binding->transport = NCACN_IP_TCP;
	map_binding->object = *abstract_syntax;
	map_binding->host = host; /* needed? */
	map_binding->endpoint = "0"; /* correct? needed? */

	map_tower = TALLOC_ZERO_P(tmp_ctx, struct epm_twr_t);
	if (map_tower == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	status = dcerpc_binding_build_tower(tmp_ctx, map_binding,
					    &(map_tower->tower));
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* allocate further parameters for the epm_Map call */

	res_towers = TALLOC_ARRAY(tmp_ctx, struct epm_twr_t, max_towers);
	if (res_towers == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	towers.twr = res_towers;

	entry_handle = TALLOC_ZERO_P(tmp_ctx, struct policy_handle);
	if (entry_handle == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* ask the endpoint mapper for the port */

	status = rpccli_epm_Map(epm_pipe,
				tmp_ctx,
				CONST_DISCARD(struct GUID *,
					      &(abstract_syntax->uuid)),
				map_tower,
				entry_handle,
				max_towers,
				&num_towers,
				&towers);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (num_towers != 1) {
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	/* extract the port from the answer */

	status = dcerpc_binding_from_tower(tmp_ctx,
					   &(towers.twr->tower),
					   &res_binding);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	/* are further checks here necessary? */
	if (res_binding->transport != NCACN_IP_TCP) {
		status = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	*pport = (uint16_t)atoi(res_binding->endpoint);

done:
	TALLOC_FREE(tmp_ctx);
	return status;
}

/**
 * Create a rpc pipe client struct, connecting to a host via tcp.
 * The port is determined by asking the endpoint mapper on the given
 * host.
 */
NTSTATUS rpc_pipe_open_tcp(TALLOC_CTX *mem_ctx, const char *host,
			   const struct ndr_syntax_id *abstract_syntax,
			   struct rpc_pipe_client **presult)
{
	NTSTATUS status;
	uint16_t port = 0;

	status = rpc_pipe_get_tcp_port(host, abstract_syntax, &port);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return rpc_pipe_open_tcp_port(mem_ctx, host, port,
					abstract_syntax, presult);
}

/********************************************************************
 Create a rpc pipe client struct, connecting to a unix domain socket
 ********************************************************************/
NTSTATUS rpc_pipe_open_ncalrpc(TALLOC_CTX *mem_ctx, const char *socket_path,
			       const struct ndr_syntax_id *abstract_syntax,
			       struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct sockaddr_un addr;
	NTSTATUS status;
	int fd;

	result = talloc_zero(mem_ctx, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;
	result->dispatch = cli_do_rpc_ndr;
	result->dispatch_send = cli_do_rpc_ndr_send;
	result->dispatch_recv = cli_do_rpc_ndr_recv;

	result->desthost = get_myname(result);
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", result->desthost);
	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	ZERO_STRUCT(addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));

	if (sys_connect(fd, (struct sockaddr *)(void *)&addr) == -1) {
		DEBUG(0, ("connect(%s) failed: %s\n", socket_path,
			  strerror(errno)));
		close(fd);
		return map_nt_error_from_unix(errno);
	}

	status = rpc_transport_sock_init(result, fd, &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		close(fd);
		goto fail;
	}

	result->transport->transport = NCALRPC;

	*presult = result;
	return NT_STATUS_OK;

 fail:
	TALLOC_FREE(result);
	return status;
}

struct rpc_pipe_client_np_ref {
	struct cli_state *cli;
	struct rpc_pipe_client *pipe;
};

static int rpc_pipe_client_np_ref_destructor(struct rpc_pipe_client_np_ref *np_ref)
{
	DLIST_REMOVE(np_ref->cli->pipe_list, np_ref->pipe);
	return 0;
}

/****************************************************************************
 Open a named pipe over SMB to a remote server.
 *
 * CAVEAT CALLER OF THIS FUNCTION:
 *    The returned rpc_pipe_client saves a copy of the cli_state cli pointer,
 *    so be sure that this function is called AFTER any structure (vs pointer)
 *    assignment of the cli.  In particular, libsmbclient does structure
 *    assignments of cli, which invalidates the data in the returned
 *    rpc_pipe_client if this function is called before the structure assignment
 *    of cli.
 * 
 ****************************************************************************/

static NTSTATUS rpc_pipe_open_np(struct cli_state *cli,
				 const struct ndr_syntax_id *abstract_syntax,
				 struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	NTSTATUS status;
	struct rpc_pipe_client_np_ref *np_ref;

	/* sanity check to protect against crashes */

	if ( !cli ) {
		return NT_STATUS_INVALID_HANDLE;
	}

	result = TALLOC_ZERO_P(NULL, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;
	result->dispatch = cli_do_rpc_ndr;
	result->dispatch_send = cli_do_rpc_ndr_send;
	result->dispatch_recv = cli_do_rpc_ndr_recv;
	result->desthost = talloc_strdup(result, cli->desthost);
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", result->desthost);

	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	status = rpc_transport_np_init(result, cli, abstract_syntax,
				       &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(result);
		return status;
	}

	result->transport->transport = NCACN_NP;

	np_ref = talloc(result->transport, struct rpc_pipe_client_np_ref);
	if (np_ref == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}
	np_ref->cli = cli;
	np_ref->pipe = result;

	DLIST_ADD(np_ref->cli->pipe_list, np_ref->pipe);
	talloc_set_destructor(np_ref, rpc_pipe_client_np_ref_destructor);

	*presult = result;
	return NT_STATUS_OK;
}

NTSTATUS rpc_pipe_open_local(TALLOC_CTX *mem_ctx,
			     struct rpc_cli_smbd_conn *conn,
			     const struct ndr_syntax_id *syntax,
			     struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct cli_pipe_auth_data *auth;
	NTSTATUS status;

	result = talloc(mem_ctx, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->abstract_syntax = *syntax;
	result->transfer_syntax = ndr_transfer_syntax;
	result->dispatch = cli_do_rpc_ndr;
	result->dispatch_send = cli_do_rpc_ndr_send;
	result->dispatch_recv = cli_do_rpc_ndr_recv;
	result->max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	result->max_recv_frag = RPC_MAX_PDU_FRAG_LEN;

	result->desthost = talloc_strdup(result, global_myname());
	result->srv_name_slash = talloc_asprintf_strupper_m(
		result, "\\\\%s", global_myname());
	if ((result->desthost == NULL) || (result->srv_name_slash == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	status = rpc_transport_smbd_init(result, conn, syntax,
					 &result->transport);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("rpc_transport_smbd_init failed: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	status = rpccli_anon_bind_data(result, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("rpccli_anon_bind_data failed: %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("rpc_pipe_bind failed: %s\n", nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	result->transport->transport = NCACN_INTERNAL;

	*presult = result;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a pipe to a remote server.
 ****************************************************************************/

static NTSTATUS cli_rpc_pipe_open(struct cli_state *cli,
				  enum dcerpc_transport_t transport,
				  const struct ndr_syntax_id *interface,
				  struct rpc_pipe_client **presult)
{
	switch (transport) {
	case NCACN_IP_TCP:
		return rpc_pipe_open_tcp(NULL, cli->desthost, interface,
					 presult);
	case NCACN_NP:
		return rpc_pipe_open_np(cli, interface, presult);
	default:
		return NT_STATUS_NOT_IMPLEMENTED;
	}
}

/****************************************************************************
 Open a named pipe to an SMB server and bind anonymously.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_noauth_transport(struct cli_state *cli,
					    enum dcerpc_transport_t transport,
					    const struct ndr_syntax_id *interface,
					    struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct cli_pipe_auth_data *auth;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_anon_bind_data(result, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_anon_bind_data returned %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	/*
	 * This is a bit of an abstraction violation due to the fact that an
	 * anonymous bind on an authenticated SMB inherits the user/domain
	 * from the enclosing SMB creds
	 */

	TALLOC_FREE(auth->user_name);
	TALLOC_FREE(auth->domain);

	auth->user_name = talloc_strdup(auth, cli->user_name);
	auth->domain = talloc_strdup(auth, cli->domain);
	auth->user_session_key = data_blob_talloc(auth,
		cli->user_session_key.data,
		cli->user_session_key.length);

	if ((auth->user_name == NULL) || (auth->domain == NULL)) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		int lvl = 0;
		if (ndr_syntax_id_equal(interface,
					&ndr_table_dssetup.syntax_id)) {
			/* non AD domains just don't have this pipe, avoid
			 * level 0 statement in that case - gd */
			lvl = 3;
		}
		DEBUG(lvl, ("cli_rpc_pipe_open_noauth: rpc_pipe_bind for pipe "
			    "%s failed with error %s\n",
			    get_pipe_name_from_syntax(talloc_tos(), interface),
			    nt_errstr(status) ));
		TALLOC_FREE(result);
		return status;
	}

	DEBUG(10,("cli_rpc_pipe_open_noauth: opened pipe %s to machine "
		  "%s and bound anonymously.\n",
		  get_pipe_name_from_syntax(talloc_tos(), interface),
		  cli->desthost));

	*presult = result;
	return NT_STATUS_OK;
}

/****************************************************************************
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_noauth(struct cli_state *cli,
				  const struct ndr_syntax_id *interface,
				  struct rpc_pipe_client **presult)
{
	return cli_rpc_pipe_open_noauth_transport(cli, NCACN_NP,
						  interface, presult);
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using NTLMSSP or SPNEGO NTLMSSP
 ****************************************************************************/

static NTSTATUS cli_rpc_pipe_open_ntlmssp_internal(struct cli_state *cli,
						   const struct ndr_syntax_id *interface,
						   enum dcerpc_transport_t transport,
						   enum pipe_auth_type auth_type,
						   enum dcerpc_AuthLevel auth_level,
						   const char *domain,
						   const char *username,
						   const char *password,
						   struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct cli_pipe_auth_data *auth;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_ntlmssp_bind_data(
		result, auth_type, auth_level, domain, username,
		password, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_ntlmssp_bind_data returned %s\n",
			  nt_errstr(status)));
		goto err;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_open_ntlmssp_internal: cli_rpc_pipe_bind failed with error %s\n",
			nt_errstr(status) ));
		goto err;
	}

	DEBUG(10,("cli_rpc_pipe_open_ntlmssp_internal: opened pipe %s to "
		"machine %s and bound NTLMSSP as user %s\\%s.\n",
		  get_pipe_name_from_syntax(talloc_tos(), interface),
		  cli->desthost, domain, username ));

	*presult = result;
	return NT_STATUS_OK;

  err:

	TALLOC_FREE(result);
	return status;
}

/****************************************************************************
 External interface.
 Open a named pipe to an SMB server and bind using NTLMSSP (bind type 10)
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_ntlmssp(struct cli_state *cli,
				   const struct ndr_syntax_id *interface,
				   enum dcerpc_transport_t transport,
				   enum dcerpc_AuthLevel auth_level,
				   const char *domain,
				   const char *username,
				   const char *password,
				   struct rpc_pipe_client **presult)
{
	return cli_rpc_pipe_open_ntlmssp_internal(cli,
						interface,
						transport,
						PIPE_AUTH_TYPE_NTLMSSP,
						auth_level,
						domain,
						username,
						password,
						presult);
}

/****************************************************************************
 External interface.
 Open a named pipe to an SMB server and bind using spnego NTLMSSP (bind type 9)
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_spnego_ntlmssp(struct cli_state *cli,
					  const struct ndr_syntax_id *interface,
					  enum dcerpc_transport_t transport,
					  enum dcerpc_AuthLevel auth_level,
					  const char *domain,
					  const char *username,
					  const char *password,
					  struct rpc_pipe_client **presult)
{
	return cli_rpc_pipe_open_ntlmssp_internal(cli,
						interface,
						transport,
						PIPE_AUTH_TYPE_SPNEGO_NTLMSSP,
						auth_level,
						domain,
						username,
						password,
						presult);
}

/****************************************************************************
  Get a the schannel session key out of an already opened netlogon pipe.
 ****************************************************************************/
static NTSTATUS get_schannel_session_key_common(struct rpc_pipe_client *netlogon_pipe,
						struct cli_state *cli,
						const char *domain,
						uint32 *pneg_flags)
{
	enum netr_SchannelType sec_chan_type = 0;
	unsigned char machine_pwd[16];
	const char *machine_account;
	NTSTATUS status;

	/* Get the machine account credentials from secrets.tdb. */
	if (!get_trust_pw_hash(domain, machine_pwd, &machine_account,
			       &sec_chan_type))
	{
		DEBUG(0, ("get_schannel_session_key: could not fetch "
			"trust account password for domain '%s'\n",
			domain));
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	status = rpccli_netlogon_setup_creds(netlogon_pipe,
					cli->desthost, /* server name */
					domain,	       /* domain */
					global_myname(), /* client name */
					machine_account, /* machine account name */
					machine_pwd,
					sec_chan_type,
					pneg_flags);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("get_schannel_session_key_common: "
			  "rpccli_netlogon_setup_creds failed with result %s "
			  "to server %s, domain %s, machine account %s.\n",
			  nt_errstr(status), cli->desthost, domain,
			  machine_account ));
		return status;
	}

	if (((*pneg_flags) & NETLOGON_NEG_SCHANNEL) == 0) {
		DEBUG(3, ("get_schannel_session_key: Server %s did not offer schannel\n",
			cli->desthost));
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	return NT_STATUS_OK;;
}

/****************************************************************************
 Open a netlogon pipe and get the schannel session key.
 Now exposed to external callers.
 ****************************************************************************/


NTSTATUS get_schannel_session_key(struct cli_state *cli,
				  const char *domain,
				  uint32 *pneg_flags,
				  struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS status;

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_netlogon.syntax_id,
					  &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = get_schannel_session_key_common(netlogon_pipe, cli, domain,
						 pneg_flags);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(netlogon_pipe);
		return status;
	}

	*presult = netlogon_pipe;
	return NT_STATUS_OK;
}

/****************************************************************************
 External interface.
 Open a named pipe to an SMB server and bind using schannel (bind type 68)
 using session_key. sign and seal.

 The *pdc will be stolen onto this new pipe
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_schannel_with_key(struct cli_state *cli,
					     const struct ndr_syntax_id *interface,
					     enum dcerpc_transport_t transport,
					     enum dcerpc_AuthLevel auth_level,
					     const char *domain,
					     struct netlogon_creds_CredentialState **pdc,
					     struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;
	struct cli_pipe_auth_data *auth;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, transport, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_schannel_bind_data(result, domain, auth_level,
					   *pdc, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_schannel_bind_data returned %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_open_schannel_with_key: "
			  "cli_rpc_pipe_bind failed with error %s\n",
			  nt_errstr(status) ));
		TALLOC_FREE(result);
		return status;
	}

	/*
	 * The credentials on a new netlogon pipe are the ones we are passed
	 * in - copy them over
	 */
	result->dc = netlogon_creds_copy(result, *pdc);
	if (result->dc == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(10,("cli_rpc_pipe_open_schannel_with_key: opened pipe %s to machine %s "
		  "for domain %s and bound using schannel.\n",
		  get_pipe_name_from_syntax(talloc_tos(), interface),
		  cli->desthost, domain ));

	*presult = result;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using schannel (bind type 68).
 Fetch the session key ourselves using a temporary netlogon pipe. This
 version uses an ntlmssp auth bound netlogon pipe to get the key.
 ****************************************************************************/

static NTSTATUS get_schannel_session_key_auth_ntlmssp(struct cli_state *cli,
						      const char *domain,
						      const char *username,
						      const char *password,
						      uint32 *pneg_flags,
						      struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *netlogon_pipe = NULL;
	NTSTATUS status;

	status = cli_rpc_pipe_open_spnego_ntlmssp(
		cli, &ndr_table_netlogon.syntax_id, NCACN_NP,
		DCERPC_AUTH_LEVEL_PRIVACY,
		domain, username, password, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = get_schannel_session_key_common(netlogon_pipe, cli, domain,
						 pneg_flags);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(netlogon_pipe);
		return status;
	}

	*presult = netlogon_pipe;
	return NT_STATUS_OK;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using schannel (bind type 68).
 Fetch the session key ourselves using a temporary netlogon pipe. This version
 uses an ntlmssp bind to get the session key.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_ntlmssp_auth_schannel(struct cli_state *cli,
						 const struct ndr_syntax_id *interface,
						 enum dcerpc_transport_t transport,
						 enum dcerpc_AuthLevel auth_level,
						 const char *domain,
						 const char *username,
						 const char *password,
						 struct rpc_pipe_client **presult)
{
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	struct rpc_pipe_client *result = NULL;
	NTSTATUS status;

	status = get_schannel_session_key_auth_ntlmssp(
		cli, domain, username, password, &neg_flags, &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("cli_rpc_pipe_open_ntlmssp_auth_schannel: failed to get schannel session "
			"key from server %s for domain %s.\n",
			cli->desthost, domain ));
		return status;
	}

	status = cli_rpc_pipe_open_schannel_with_key(
		cli, interface, transport, auth_level, domain, &netlogon_pipe->dc,
		&result);

	/* Now we've bound using the session key we can close the netlog pipe. */
	TALLOC_FREE(netlogon_pipe);

	if (NT_STATUS_IS_OK(status)) {
		*presult = result;
	}
	return status;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using schannel (bind type 68).
 Fetch the session key ourselves using a temporary netlogon pipe.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_schannel(struct cli_state *cli,
				    const struct ndr_syntax_id *interface,
				    enum dcerpc_transport_t transport,
				    enum dcerpc_AuthLevel auth_level,
				    const char *domain,
				    struct rpc_pipe_client **presult)
{
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct rpc_pipe_client *netlogon_pipe = NULL;
	struct rpc_pipe_client *result = NULL;
	NTSTATUS status;

	status = get_schannel_session_key(cli, domain, &neg_flags,
					  &netlogon_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("cli_rpc_pipe_open_schannel: failed to get schannel session "
			"key from server %s for domain %s.\n",
			cli->desthost, domain ));
		return status;
	}

	status = cli_rpc_pipe_open_schannel_with_key(
		cli, interface, transport, auth_level, domain, &netlogon_pipe->dc,
		&result);

	/* Now we've bound using the session key we can close the netlog pipe. */
	TALLOC_FREE(netlogon_pipe);

	if (NT_STATUS_IS_OK(status)) {
		*presult = result;
	}

	return status;
}

/****************************************************************************
 Open a named pipe to an SMB server and bind using krb5 (bind type 16).
 The idea is this can be called with service_princ, username and password all
 NULL so long as the caller has a TGT.
 ****************************************************************************/

NTSTATUS cli_rpc_pipe_open_krb5(struct cli_state *cli,
				const struct ndr_syntax_id *interface,
				enum dcerpc_AuthLevel auth_level,
				const char *service_princ,
				const char *username,
				const char *password,
				struct rpc_pipe_client **presult)
{
#ifdef HAVE_KRB5
	struct rpc_pipe_client *result;
	struct cli_pipe_auth_data *auth;
	NTSTATUS status;

	status = cli_rpc_pipe_open(cli, NCACN_NP, interface, &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = rpccli_kerberos_bind_data(result, auth_level, service_princ,
					   username, password, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("rpccli_kerberos_bind_data returned %s\n",
			  nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	status = rpc_pipe_bind(result, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("cli_rpc_pipe_open_krb5: cli_rpc_pipe_bind failed "
			  "with error %s\n", nt_errstr(status)));
		TALLOC_FREE(result);
		return status;
	}

	*presult = result;
	return NT_STATUS_OK;
#else
	DEBUG(0,("cli_rpc_pipe_open_krb5: kerberos not found at compile time.\n"));
	return NT_STATUS_NOT_IMPLEMENTED;
#endif
}

NTSTATUS cli_get_session_key(TALLOC_CTX *mem_ctx,
			     struct rpc_pipe_client *cli,
			     DATA_BLOB *session_key)
{
	if (!session_key || !cli) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!cli->auth) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	switch (cli->auth->auth_type) {
		case PIPE_AUTH_TYPE_SCHANNEL:
			*session_key = data_blob_talloc(mem_ctx,
				cli->auth->a_u.schannel_auth->creds->session_key, 16);
			break;
		case PIPE_AUTH_TYPE_NTLMSSP:
		case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
			*session_key = data_blob_talloc(mem_ctx,
				cli->auth->a_u.ntlmssp_state->session_key.data,
				cli->auth->a_u.ntlmssp_state->session_key.length);
			break;
		case PIPE_AUTH_TYPE_KRB5:
		case PIPE_AUTH_TYPE_SPNEGO_KRB5:
			*session_key = data_blob_talloc(mem_ctx,
				cli->auth->a_u.kerberos_auth->session_key.data,
				cli->auth->a_u.kerberos_auth->session_key.length);
			break;
		case PIPE_AUTH_TYPE_NONE:
			*session_key = data_blob_talloc(mem_ctx,
				cli->auth->user_session_key.data,
				cli->auth->user_session_key.length);
			break;
		default:
			return NT_STATUS_NO_USER_SESSION_KEY;
	}

	return NT_STATUS_OK;
}
