/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Almost completely rewritten by (C) Jeremy Allison 2005 - 2010
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
#include "system/filesys.h"
#include "srv_pipe_internal.h"
#include "../librpc/gen_ndr/ndr_schannel.h"
#include "../libcli/auth/schannel.h"
#include "../libcli/auth/spnego.h"
#include "dcesrv_ntlmssp.h"
#include "dcesrv_gssapi.h"
#include "dcesrv_spnego.h"
#include "rpc_server.h"
#include "rpc_dce.h"
#include "smbd/smbd.h"
#include "auth.h"
#include "ntdomain.h"
#include "rpc_server/srv_pipe.h"
#include "../librpc/gen_ndr/ndr_dcerpc.h"
#include "../librpc/ndr/ndr_dcerpc.h"
#include "../librpc/gen_ndr/ndr_samr.h"
#include "../librpc/gen_ndr/ndr_lsa.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../librpc/gen_ndr/ndr_epmapper.h"
#include "../librpc/gen_ndr/ndr_echo.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/**
 * Dump everything from the start of the end up of the provided data
 * into a file, but only at debug level >= 50
 **/
static void dump_pdu_region(const char *name, int v,
			    DATA_BLOB *data, size_t start, size_t end)
{
	int fd, i;
	char *fname = NULL;
	ssize_t sz;

	if (DEBUGLEVEL < 50) return;

	if (start > data->length || end > data->length || start > end) return;

	for (i = 1; i < 100; i++) {
		if (v != -1) {
			fname = talloc_asprintf(talloc_tos(),
						"/tmp/%s_%d.%d.prs",
						name, v, i);
		} else {
			fname = talloc_asprintf(talloc_tos(),
						"/tmp/%s_%d.prs",
						name, i);
		}
		if (!fname) {
			return;
		}
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL, 0644);
		if (fd != -1 || errno != EEXIST) break;
	}
	if (fd != -1) {
		sz = write(fd, data->data + start, end - start);
		i = close(fd);
		if ((sz != end - start) || (i != 0) ) {
			DEBUG(0, ("Error writing/closing %s: %ld!=%ld %d\n",
				  fname, (unsigned long)sz,
				  (unsigned long)end - start, i));
		} else {
			DEBUG(0,("created %s\n", fname));
		}
	}
	TALLOC_FREE(fname);
}

static DATA_BLOB generic_session_key(void)
{
	return data_blob_const("SystemLibraryDTC", 16);
}

/*******************************************************************
 Generate the next PDU to be returned from the data.
********************************************************************/

static NTSTATUS create_next_packet(TALLOC_CTX *mem_ctx,
				   struct pipe_auth_data *auth,
				   uint32_t call_id,
				   DATA_BLOB *rdata,
				   size_t data_sent_length,
				   DATA_BLOB *frag,
				   size_t *pdu_size)
{
	union dcerpc_payload u;
	uint8_t pfc_flags;
	size_t data_left;
	size_t data_to_send;
	size_t frag_len;
	size_t pad_len = 0;
	size_t auth_len = 0;
	NTSTATUS status;

	ZERO_STRUCT(u.response);

	/* Set up rpc packet pfc flags. */
	if (data_sent_length == 0) {
		pfc_flags = DCERPC_PFC_FLAG_FIRST;
	} else {
		pfc_flags = 0;
	}

	/* Work out how much we can fit in a single PDU. */
	data_left = rdata->length - data_sent_length;

	/* Ensure there really is data left to send. */
	if (!data_left) {
		DEBUG(0, ("No data left to send !\n"));
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	status = dcerpc_guess_sizes(auth,
				    DCERPC_RESPONSE_LENGTH,
				    data_left,
				    RPC_MAX_PDU_FRAG_LEN,
				    SERVER_NDR_PADDING_SIZE,
				    &data_to_send, &frag_len,
				    &auth_len, &pad_len);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Set up the alloc hint. This should be the data left to send. */
	u.response.alloc_hint = data_left;

	/* Work out if this PDU will be the last. */
	if (data_sent_length + data_to_send >= rdata->length) {
		pfc_flags |= DCERPC_PFC_FLAG_LAST;
	}

	/* Prepare data to be NDR encoded. */
	u.response.stub_and_verifier =
		data_blob_const(rdata->data + data_sent_length, data_to_send);

	/* Store the packet in the data stream. */
	status = dcerpc_push_ncacn_packet(mem_ctx, DCERPC_PKT_RESPONSE,
					  pfc_flags, auth_len, call_id,
					  &u, frag);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to marshall RPC Packet.\n"));
		return status;
	}

	if (auth_len) {
		/* Set the proper length on the pdu, including padding.
		 * Only needed if an auth trailer will be appended. */
		dcerpc_set_frag_length(frag, frag->length
						+ pad_len
						+ DCERPC_AUTH_TRAILER_LENGTH
						+ auth_len);
	}

	if (auth_len) {
		status = dcerpc_add_auth_footer(auth, pad_len, frag);
		if (!NT_STATUS_IS_OK(status)) {
			data_blob_free(frag);
			return status;
		}
	}

	*pdu_size = data_to_send;
	return NT_STATUS_OK;
}

/*******************************************************************
 Generate the next PDU to be returned from the data in p->rdata. 
********************************************************************/

bool create_next_pdu(struct pipes_struct *p)
{
	size_t pdu_size = 0;
	NTSTATUS status;

	/*
	 * If we're in the fault state, keep returning fault PDU's until
	 * the pipe gets closed. JRA.
	 */
	if (p->fault_state) {
		setup_fault_pdu(p, NT_STATUS(p->fault_state));
		return true;
	}

	status = create_next_packet(p->mem_ctx, &p->auth,
				    p->call_id, &p->out_data.rdata,
				    p->out_data.data_sent_length,
				    &p->out_data.frag, &pdu_size);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to create packet with error %s, "
			  "(auth level %u / type %u)\n",
			  nt_errstr(status),
			  (unsigned int)p->auth.auth_level,
			  (unsigned int)p->auth.auth_type));
		return false;
	}

	/* Setup the counts for this PDU. */
	p->out_data.data_sent_length += pdu_size;
	p->out_data.current_pdu_sent = 0;
	return true;
}


static bool pipe_init_outgoing_data(struct pipes_struct *p);

/*******************************************************************
 Marshall a bind_nak pdu.
*******************************************************************/

static bool setup_bind_nak(struct pipes_struct *p, struct ncacn_packet *pkt)
{
	NTSTATUS status;
	union dcerpc_payload u;

	/* Free any memory in the current return data buffer. */
	pipe_init_outgoing_data(p);

	/*
	 * Initialize a bind_nak header.
	 */

	ZERO_STRUCT(u);

	u.bind_nak.reject_reason  = 0;

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	status = dcerpc_push_ncacn_packet(p->mem_ctx,
					  DCERPC_PKT_BIND_NAK,
					  DCERPC_PFC_FLAG_FIRST |
						DCERPC_PFC_FLAG_LAST,
					  0,
					  pkt->call_id,
					  &u,
					  &p->out_data.frag);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	set_incoming_fault(p);
	TALLOC_FREE(p->auth.auth_ctx);
	p->auth.auth_level = DCERPC_AUTH_LEVEL_NONE;
	p->auth.auth_type = DCERPC_AUTH_TYPE_NONE;
	p->pipe_bound = False;
	p->allow_bind = false;
	p->allow_alter = false;
	p->allow_auth3 = false;

	return True;
}

/*******************************************************************
 Marshall a fault pdu.
*******************************************************************/

bool setup_fault_pdu(struct pipes_struct *p, NTSTATUS fault_status)
{
	NTSTATUS status;
	union dcerpc_payload u;

	/* Free any memory in the current return data buffer. */
	pipe_init_outgoing_data(p);

	/*
	 * Initialize a fault header.
	 */

	ZERO_STRUCT(u);

	u.fault.status		= NT_STATUS_V(fault_status);
	u.fault._pad		= data_blob_talloc_zero(p->mem_ctx, 4);

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	status = dcerpc_push_ncacn_packet(p->mem_ctx,
					  DCERPC_PKT_FAULT,
					  DCERPC_PFC_FLAG_FIRST |
					   DCERPC_PFC_FLAG_LAST |
					   DCERPC_PFC_FLAG_DID_NOT_EXECUTE,
					  0,
					  p->call_id,
					  &u,
					  &p->out_data.frag);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	return True;
}

/*******************************************************************
 Ensure a bind request has the correct abstract & transfer interface.
 Used to reject unknown binds from Win2k.
*******************************************************************/

static bool check_bind_req(struct pipes_struct *p,
			   struct ndr_syntax_id* abstract,
			   struct ndr_syntax_id* transfer,
			   uint32 context_id)
{
	struct pipe_rpc_fns *context_fns;
	const char *interface_name = NULL;
	bool ok;

	DEBUG(3,("check_bind_req for %s\n",
		 get_pipe_name_from_syntax(talloc_tos(), abstract)));

	ok = ndr_syntax_id_equal(transfer, &ndr_transfer_syntax);
	if (!ok) {
		DEBUG(1,("check_bind_req unknown transfer syntax for "
			 "%s context_id=%u\n",
			 get_pipe_name_from_syntax(talloc_tos(), abstract),
			 (unsigned)context_id));
		return false;
	}

	for (context_fns = p->contexts;
	     context_fns != NULL;
	     context_fns = context_fns->next)
	{
		if (context_fns->context_id != context_id) {
			continue;
		}

		ok = ndr_syntax_id_equal(&context_fns->syntax,
					 abstract);
		if (ok) {
			return true;
		}

		DEBUG(1,("check_bind_req: changing abstract syntax for "
			 "%s context_id=%u into %s not supported\n",
			 get_pipe_name_from_syntax(talloc_tos(), &context_fns->syntax),
			 (unsigned)context_id,
			 get_pipe_name_from_syntax(talloc_tos(), abstract)));
		return false;
	}

	/* we have to check all now since win2k introduced a new UUID on the lsaprpc pipe */
	if (!rpc_srv_pipe_exists_by_id(abstract)) {
		return false;
	}

	DEBUG(3, ("check_bind_req: %s -> %s rpc service\n",
		  rpc_srv_get_pipe_cli_name(abstract),
		  rpc_srv_get_pipe_srv_name(abstract)));

	context_fns = SMB_MALLOC_P(struct pipe_rpc_fns);
	if (context_fns == NULL) {
		DEBUG(0,("check_bind_req: malloc() failed!\n"));
		return False;
	}

	interface_name = get_pipe_name_from_syntax(talloc_tos(),
						   abstract);

	SMB_ASSERT(interface_name != NULL);

	context_fns->next = context_fns->prev = NULL;
	context_fns->n_cmds = rpc_srv_get_pipe_num_cmds(abstract);
	context_fns->cmds = rpc_srv_get_pipe_cmds(abstract);
	context_fns->context_id = context_id;
	context_fns->syntax = *abstract;

	context_fns->allow_connect = lp_allow_dcerpc_auth_level_connect();
#ifdef SAMR_SUPPORT
	/*
	 * for the samr and the lsarpc interfaces we don't allow "connect"
	 * auth_level by default.
	 */
	ok = ndr_syntax_id_equal(abstract, &ndr_table_samr.syntax_id);
	if (ok) {
		context_fns->allow_connect = false;
	}
#endif
#ifdef LSA_SUPPORT
	ok = ndr_syntax_id_equal(abstract, &ndr_table_lsarpc.syntax_id);
	if (ok) {
		context_fns->allow_connect = false;
	}
#endif
#ifdef NETLOGON_SUPPORT
	ok = ndr_syntax_id_equal(abstract, &ndr_table_netlogon.syntax_id);
	if (ok) {
		context_fns->allow_connect = false;
	}
#endif
	/*
	 * for the epmapper and echo interfaces we allow "connect"
	 * auth_level by default.
	 */
	ok = ndr_syntax_id_equal(abstract, &ndr_table_epmapper.syntax_id);
	if (ok) {
		context_fns->allow_connect = true;
	}
#ifdef DEVELOPER
	ok = ndr_syntax_id_equal(abstract, &ndr_table_rpcecho.syntax_id);
	if (ok) {
		context_fns->allow_connect = true;
	}
#endif
	/*
	 * every interface can be modified to allow "connect" auth_level by
	 * using a parametric option like:
	 * allow dcerpc auth level connect:<interface>
	 * e.g.
	 * allow dcerpc auth level connect:samr = yes
	 */
	context_fns->allow_connect = lp_parm_bool(-1,
		"allow dcerpc auth level connect",
		interface_name, context_fns->allow_connect);

	/* add to the list of open contexts */

	DLIST_ADD( p->contexts, context_fns );

	return True;
}

/**
 * Is a named pipe known?
 * @param[in] cli_filename	The pipe name requested by the client
 * @result			Do we want to serve this?
 */
bool is_known_pipename(const char *cli_filename, struct ndr_syntax_id *syntax)
{
	const char *pipename = cli_filename;
	NTSTATUS status;

	if (strnequal(pipename, "\\PIPE\\", 6)) {
		pipename += 5;
	}

	if (*pipename == '\\') {
		pipename += 1;
	}

	if (strchr(pipename, '/')) {
		DEBUG(1, ("Refusing open on pipe %s\n", pipename));
		return false;
	}

	if (lp_disable_spoolss() && strequal(pipename, "spoolss")) {
		DEBUG(10, ("refusing spoolss access\n"));
		return false;
	}

	if (rpc_srv_get_pipe_interface_by_cli_name(pipename, syntax)) {
		return true;
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
	if (rpc_srv_get_pipe_interface_by_cli_name(pipename, syntax)) {
		return true;
	}

	DEBUG(10, ("is_known_pipename: pipe %s did not register itself!\n",
		   pipename));

	return false;
}

/*******************************************************************
 Handle the first part of a SPNEGO bind auth.
*******************************************************************/

static bool pipe_spnego_auth_bind(struct pipes_struct *p,
				  TALLOC_CTX *mem_ctx,
				  struct dcerpc_auth *auth_info,
				  DATA_BLOB *response)
{
	struct spnego_context *spnego_ctx;
	NTSTATUS status;

	status = spnego_server_auth_start(p,
					  (auth_info->auth_level ==
						DCERPC_AUTH_LEVEL_INTEGRITY),
					  (auth_info->auth_level ==
						DCERPC_AUTH_LEVEL_PRIVACY),
					  true,
					  &auth_info->credentials,
					  response,
					  &spnego_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed SPNEGO negotiate (%s)\n",
			  nt_errstr(status)));
		return false;
	}

	/* Make sure data is bound to the memctx, to be freed the caller */
	talloc_steal(mem_ctx, response->data);

	p->auth.auth_ctx = spnego_ctx;
	p->auth.auth_type = DCERPC_AUTH_TYPE_SPNEGO;
	p->auth.auth_context_id = auth_info->auth_context_id;

	DEBUG(10, ("SPNEGO auth started\n"));

	return true;
}

/*******************************************************************
 Handle an schannel bind auth.
*******************************************************************/

static bool pipe_schannel_auth_bind(struct pipes_struct *p,
				    TALLOC_CTX *mem_ctx,
				    struct dcerpc_auth *auth_info,
				    DATA_BLOB *response)
{
	struct NL_AUTH_MESSAGE neg;
	struct NL_AUTH_MESSAGE reply;
	bool ret;
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	enum ndr_err_code ndr_err;
	struct schannel_state *schannel_auth;

	ndr_err = ndr_pull_struct_blob(
			&auth_info->credentials, mem_ctx, &neg,
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
	status = schannel_get_creds_state(p, lp_private_dir(),
					    neg.oem_netbios_computer.a, &creds);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("pipe_schannel_auth_bind: Attempt to bind using schannel without successful serverauth2\n"));
		return False;
	}

	schannel_auth = talloc(p, struct schannel_state);
	if (!schannel_auth) {
		TALLOC_FREE(creds);
		return False;
	}

	schannel_auth->state = SCHANNEL_STATE_START;
	schannel_auth->seq_num = 0;
	schannel_auth->initiator = false;
	schannel_auth->creds = creds;

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

	ret = session_info_set_session_key(p->session_info, generic_session_key());

	if (!ret) {
		DEBUG(0, ("session_info_set_session_key failed\n"));
		return false;
	}

	/*** SCHANNEL verifier ***/

	reply.MessageType			= NL_NEGOTIATE_RESPONSE;
	reply.Flags				= 0;
	reply.Buffer.dummy			= 5; /* ??? actually I don't think
						      * this has any meaning
						      * here - gd */

	ndr_err = ndr_push_struct_blob(response, mem_ctx, &reply,
		       (ndr_push_flags_fn_t)ndr_push_NL_AUTH_MESSAGE);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("Failed to marshall NL_AUTH_MESSAGE.\n"));
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(NL_AUTH_MESSAGE, &reply);
	}

	DEBUG(10,("pipe_schannel_auth_bind: schannel auth: domain [%s] myname [%s]\n",
		neg.oem_netbios_domain.a, neg.oem_netbios_computer.a));

	/* We're finished with this bind - no more packets. */
	p->auth.auth_ctx = schannel_auth;
	p->auth.auth_type = DCERPC_AUTH_TYPE_SCHANNEL;
	p->auth.auth_context_id = auth_info->auth_context_id;

	p->pipe_bound = True;

	return True;
}

/*******************************************************************
 Handle an NTLMSSP bind auth.
*******************************************************************/

static bool pipe_ntlmssp_auth_bind(struct pipes_struct *p,
				   TALLOC_CTX *mem_ctx,
				   struct dcerpc_auth *auth_info,
				   DATA_BLOB *response)
{
	struct auth_ntlmssp_state *ntlmssp_state = NULL;
        NTSTATUS status;

	if (strncmp((char *)auth_info->credentials.data, "NTLMSSP", 7) != 0) {
		DEBUG(0, ("Failed to read NTLMSSP in blob\n"));
                return false;
        }

	/* We have an NTLMSSP blob. */
	status = ntlmssp_server_auth_start(p,
					   (auth_info->auth_level ==
						DCERPC_AUTH_LEVEL_INTEGRITY),
					   (auth_info->auth_level ==
						DCERPC_AUTH_LEVEL_PRIVACY),
					   true,
					   &auth_info->credentials,
					   response,
					   &ntlmssp_state);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		DEBUG(0, (__location__ ": auth_ntlmssp_start failed: %s\n",
			  nt_errstr(status)));
		return false;
	}

	/* Make sure data is bound to the memctx, to be freed the caller */
	talloc_steal(mem_ctx, response->data);

	p->auth.auth_ctx = ntlmssp_state;
	p->auth.auth_type = DCERPC_AUTH_TYPE_NTLMSSP;
	p->auth.auth_context_id = auth_info->auth_context_id;

	DEBUG(10, (__location__ ": NTLMSSP auth started\n"));

	return true;
}

/*******************************************************************
 Process an NTLMSSP authentication response.
 If this function succeeds, the user has been authenticated
 and their domain, name and calling workstation stored in
 the pipe struct.
*******************************************************************/

static bool pipe_ntlmssp_verify_final(TALLOC_CTX *mem_ctx,
				struct auth_ntlmssp_state *ntlmssp_ctx,
				enum dcerpc_AuthLevel auth_level,
				struct client_address *client_id,
				struct ndr_syntax_id *syntax,
				struct auth_serversupplied_info **session_info)
{
	NTSTATUS status;
	bool ret;

	DEBUG(5, (__location__ ": pipe %s checking user details\n",
		 get_pipe_name_from_syntax(talloc_tos(), syntax)));

	/* Finally - if the pipe negotiated integrity (sign) or privacy (seal)
	   ensure the underlying NTLMSSP flags are also set. If not we should
	   refuse the bind. */

	status = ntlmssp_server_check_flags(ntlmssp_ctx,
					    (auth_level ==
						DCERPC_AUTH_LEVEL_INTEGRITY),
					    (auth_level ==
						DCERPC_AUTH_LEVEL_PRIVACY));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, (__location__ ": Client failed to negotatie proper "
			  "security for pipe %s\n",
			  get_pipe_name_from_syntax(talloc_tos(), syntax)));
		return false;
	}

	TALLOC_FREE(*session_info);

	status = ntlmssp_server_get_user_info(ntlmssp_ctx,
						mem_ctx, session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, (__location__ ": failed to obtain the server info "
			  "for authenticated user: %s\n", nt_errstr(status)));
		return false;
	}

	if ((*session_info)->security_token == NULL) {
		DEBUG(1, ("Auth module failed to provide nt_user_token\n"));
		return false;
	}

	/*
	 * We're an authenticated bind over smb, so the session key needs to
	 * be set to "SystemLibraryDTC". Weird, but this is what Windows
	 * does. See the RPC-SAMBA3SESSIONKEY.
	 */

	ret = session_info_set_session_key((*session_info), generic_session_key());
	if (!ret) {
		DEBUG(0, ("Failed to set session key!\n"));
		return false;
	}

	return true;
}

/*******************************************************************
 Handle a GSSAPI bind auth.
*******************************************************************/

static bool pipe_gssapi_auth_bind(struct pipes_struct *p,
				  TALLOC_CTX *mem_ctx,
				  struct dcerpc_auth *auth_info,
				  DATA_BLOB *response)
{
        NTSTATUS status;
	struct gse_context *gse_ctx = NULL;

	status = gssapi_server_auth_start(p,
					  (auth_info->auth_level ==
						DCERPC_AUTH_LEVEL_INTEGRITY),
					  (auth_info->auth_level ==
						DCERPC_AUTH_LEVEL_PRIVACY),
					  true,
					  &auth_info->credentials,
					  response,
					  &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to init dcerpc gssapi server (%s)\n",
			  nt_errstr(status)));
		goto err;
	}

	/* Make sure data is bound to the memctx, to be freed the caller */
	talloc_steal(mem_ctx, response->data);

	p->auth.auth_ctx = gse_ctx;
	p->auth.auth_type = DCERPC_AUTH_TYPE_KRB5;

	DEBUG(10, ("KRB5 auth started\n"));

	return true;

err:
	TALLOC_FREE(gse_ctx);
	return false;
}

static NTSTATUS pipe_gssapi_verify_final(TALLOC_CTX *mem_ctx,
					 struct gse_context *gse_ctx,
					 struct client_address *client_id,
					 struct auth_serversupplied_info **session_info)
{
	NTSTATUS status;
	bool bret;

	/* Finally - if the pipe negotiated integrity (sign) or privacy (seal)
	   ensure the underlying flags are also set. If not we should
	   refuse the bind. */

	status = gssapi_server_check_flags(gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Requested Security Layers not honored!\n"));
		return status;
	}

	status = gssapi_server_get_user_info(gse_ctx, mem_ctx,
					     client_id, session_info);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, (__location__ ": failed to obtain the server info "
			  "for authenticated user: %s\n", nt_errstr(status)));
		return status;
	}

	if ((*session_info)->security_token == NULL) {
		status = create_local_token(*session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to create local user token (%s)\n",
				  nt_errstr(status)));
			status = NT_STATUS_ACCESS_DENIED;
			return status;
		}
	}

	/* TODO: this is what the ntlmssp code does with the session_key, check
	 * it is ok with gssapi too */
	/*
	 * We're an authenticated bind over smb, so the session key needs to
	 * be set to "SystemLibraryDTC". Weird, but this is what Windows
	 * does. See the RPC-SAMBA3SESSIONKEY.
	 */

	bret = session_info_set_session_key((*session_info), generic_session_key());
	if (!bret) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

static NTSTATUS pipe_auth_verify_final(struct pipes_struct *p)
{
	enum spnego_mech auth_type;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	void *mech_ctx;
	NTSTATUS status;

	if (p->auth.auth_type == DCERPC_AUTH_TYPE_NONE) {
		p->pipe_bound = true;
		return NT_STATUS_OK;
	}

	switch (p->auth.auth_type) {
	case DCERPC_AUTH_TYPE_NTLMSSP:
		ntlmssp_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						    struct auth_ntlmssp_state);
		if (!pipe_ntlmssp_verify_final(p, ntlmssp_ctx,
						p->auth.auth_level,
						p->client_id, &p->syntax,
						&p->session_info)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		break;
	case DCERPC_AUTH_TYPE_KRB5:
		gse_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						struct gse_context);
		status = pipe_gssapi_verify_final(p, gse_ctx,
						  p->client_id,
						  &p->session_info);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("gssapi bind failed with: %s",
				  nt_errstr(status)));
			return status;
		}
		break;
	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						   struct spnego_context);
		status = spnego_get_negotiated_mech(spnego_ctx,
						    &auth_type, &mech_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Bad SPNEGO state (%s)\n",
				  nt_errstr(status)));
			return status;
		}
		switch(auth_type) {
		case SPNEGO_KRB5:
			gse_ctx = talloc_get_type_abort(mech_ctx,
							struct gse_context);
			status = pipe_gssapi_verify_final(p, gse_ctx,
							  p->client_id,
							  &p->session_info);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(1, ("gssapi bind failed with: %s",
					  nt_errstr(status)));
				return status;
			}
			break;
		case SPNEGO_NTLMSSP:
			ntlmssp_ctx = talloc_get_type_abort(mech_ctx,
						struct auth_ntlmssp_state);
			if (!pipe_ntlmssp_verify_final(p, ntlmssp_ctx,
							p->auth.auth_level,
							p->client_id,
							&p->syntax,
							&p->session_info)) {
				return NT_STATUS_ACCESS_DENIED;
			}
			break;
		default:
			DEBUG(0, (__location__ ": incorrect spnego type "
				  "(%d).\n", auth_type));
			return NT_STATUS_ACCESS_DENIED;
		}
		break;
	default:
		DEBUG(0, (__location__ ": incorrect auth type (%u).\n",
			  (unsigned int)p->auth.auth_type));
		return NT_STATUS_ACCESS_DENIED;
	}

	p->pipe_bound = true;

	return NT_STATUS_OK;
}

/*******************************************************************
 Respond to a pipe bind request.
*******************************************************************/

static bool api_pipe_bind_req(struct pipes_struct *p,
				struct ncacn_packet *pkt)
{
	struct dcerpc_auth auth_info;
	uint16 assoc_gid;
	unsigned int auth_type = DCERPC_AUTH_TYPE_NONE;
	NTSTATUS status;
	struct ndr_syntax_id id;
	union dcerpc_payload u;
	struct dcerpc_ack_ctx bind_ack_ctx;
	DATA_BLOB auth_resp = data_blob_null;
	DATA_BLOB auth_blob = data_blob_null;

	if (!p->allow_bind) {
		DEBUG(2,("Pipe not in allow bind state\n"));
		return setup_bind_nak(p, pkt);
	}
	p->allow_bind = false;

	status = dcerpc_verify_ncacn_packet_header(pkt,
			DCERPC_PKT_BIND,
			pkt->u.bind.auth_info.length,
			0, /* required flags */
			DCERPC_PFC_FLAG_FIRST |
			DCERPC_PFC_FLAG_LAST |
			DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN |
			0x08 | /* this is not defined, but should be ignored */
			DCERPC_PFC_FLAG_CONC_MPX |
			DCERPC_PFC_FLAG_DID_NOT_EXECUTE |
			DCERPC_PFC_FLAG_MAYBE |
			DCERPC_PFC_FLAG_OBJECT_UUID);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("api_pipe_bind_req: invalid pdu: %s\n",
			  nt_errstr(status)));
		goto err_exit;
	}

	if (pkt->u.bind.num_contexts == 0) {
		DEBUG(1, ("api_pipe_bind_req: no rpc contexts around\n"));
		goto err_exit;
	}

	if (pkt->u.bind.ctx_list[0].num_transfer_syntaxes == 0) {
		DEBUG(1, ("api_pipe_bind_req: no transfer syntaxes around\n"));
		goto err_exit;
	}

	/*
	 * Try and find the correct pipe name to ensure
	 * that this is a pipe name we support.
	 */
	id = pkt->u.bind.ctx_list[0].abstract_syntax;
	if (rpc_srv_pipe_exists_by_id(&id)) {
		DEBUG(3, ("api_pipe_bind_req: \\PIPE\\%s -> \\PIPE\\%s\n",
			rpc_srv_get_pipe_cli_name(&id),
			rpc_srv_get_pipe_srv_name(&id)));
	} else {
		status = smb_probe_module(
			"rpc", get_pipe_name_from_syntax(
				talloc_tos(),
				&pkt->u.bind.ctx_list[0].abstract_syntax));

		if (NT_STATUS_IS_ERR(status)) {
                       DEBUG(3,("api_pipe_bind_req: Unknown pipe name %s in bind request.\n",
                                get_pipe_name_from_syntax(
					talloc_tos(),
					&pkt->u.bind.ctx_list[0].abstract_syntax)));

			return setup_bind_nak(p, pkt);
		}

		if (rpc_srv_get_pipe_interface_by_cli_name(
				get_pipe_name_from_syntax(talloc_tos(),
							  &p->syntax),
				&id)) {
			DEBUG(3, ("api_pipe_bind_req: \\PIPE\\%s -> \\PIPE\\%s\n",
				rpc_srv_get_pipe_cli_name(&id),
				rpc_srv_get_pipe_srv_name(&id)));
		} else {
			DEBUG(0, ("module %s doesn't provide functions for "
				  "pipe %s!\n",
				  get_pipe_name_from_syntax(talloc_tos(),
							    &p->syntax),
				  get_pipe_name_from_syntax(talloc_tos(),
							    &p->syntax)));
			return setup_bind_nak(p, pkt);
		}
	}

	DEBUG(5,("api_pipe_bind_req: make response. %d\n", __LINE__));

	if (pkt->u.bind.assoc_group_id != 0) {
		assoc_gid = pkt->u.bind.assoc_group_id;
	} else {
		assoc_gid = 0x53f0;
	}

	/*
	 * Create the bind response struct.
	 */

	/* If the requested abstract synt uuid doesn't match our client pipe,
		reject the bind_ack & set the transfer interface synt to all 0's,
		ver 0 (observed when NT5 attempts to bind to abstract interfaces
		unknown to NT4)
		Needed when adding entries to a DACL from NT5 - SK */

	if (check_bind_req(p,
			&pkt->u.bind.ctx_list[0].abstract_syntax,
			&pkt->u.bind.ctx_list[0].transfer_syntaxes[0],
			pkt->u.bind.ctx_list[0].context_id)) {

		bind_ack_ctx.result = 0;
		bind_ack_ctx.reason = 0;
		bind_ack_ctx.syntax = pkt->u.bind.ctx_list[0].transfer_syntaxes[0];
	} else {
		p->pipe_bound = False;
		/* Rejection reason: abstract syntax not supported */
		bind_ack_ctx.result = DCERPC_BIND_PROVIDER_REJECT;
		bind_ack_ctx.reason = DCERPC_BIND_REASON_ASYNTAX;
		bind_ack_ctx.syntax = null_ndr_syntax_id;
	}

	/*
	 * Check if this is an authenticated bind request.
	 */
	if (pkt->auth_length) {
		/*
		 * Decode the authentication verifier.
		 */
		status = dcerpc_pull_auth_trailer(pkt, pkt,
						  &pkt->u.bind.auth_info,
						  &auth_info, NULL, true);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Unable to unmarshall dcerpc_auth.\n"));
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
		case DCERPC_AUTH_LEVEL_CONNECT:
			p->auth.auth_level = DCERPC_AUTH_LEVEL_CONNECT;
			break;
		default:
			DEBUG(0, ("Unexpected auth level (%u).\n",
				(unsigned int)auth_info.auth_level ));
			goto err_exit;
		}

		switch (auth_type) {
		case DCERPC_AUTH_TYPE_NTLMSSP:
			if (!pipe_ntlmssp_auth_bind(p, pkt,
						&auth_info, &auth_resp)) {
				goto err_exit;
			}
			assoc_gid = 0x7a77;
			break;

		case DCERPC_AUTH_TYPE_SCHANNEL:
			if (!pipe_schannel_auth_bind(p, pkt,
						&auth_info, &auth_resp)) {
				goto err_exit;
			}
			break;

		case DCERPC_AUTH_TYPE_SPNEGO:
			if (!pipe_spnego_auth_bind(p, pkt,
						&auth_info, &auth_resp)) {
				goto err_exit;
			}
			break;

		case DCERPC_AUTH_TYPE_KRB5:
			if (!pipe_gssapi_auth_bind(p, pkt,
						&auth_info, &auth_resp)) {
				goto err_exit;
			}
			break;

		case DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM:
			if (p->transport == NCALRPC && p->ncalrpc_as_system) {
				TALLOC_FREE(p->session_info);

				status = make_session_info_system(p,
								  &p->session_info);
				if (!NT_STATUS_IS_OK(status)) {
					goto err_exit;
				}

				auth_resp = data_blob_talloc(pkt,
							     "NCALRPC_AUTH_OK",
							     15);

				p->auth.auth_type = DCERPC_AUTH_TYPE_NCALRPC_AS_SYSTEM;
				p->pipe_bound = true;
			} else {
				goto err_exit;
			}
			break;

		case DCERPC_AUTH_TYPE_NONE:
			break;

		default:
			DEBUG(0, ("Unknown auth type %x requested.\n", auth_type));
			goto err_exit;
		}
	}

	if (auth_type == DCERPC_AUTH_TYPE_NONE) {
		/* Unauthenticated bind request. */
		/* We're finished - no more packets. */
		p->auth.auth_type = DCERPC_AUTH_TYPE_NONE;
		/* We must set the pipe auth_level here also. */
		p->auth.auth_level = DCERPC_AUTH_LEVEL_NONE;
		p->pipe_bound = True;
		/* The session key was initialized from the SMB
		 * session in make_internal_rpc_pipe_p */
		p->auth.auth_context_id = 0;
	}

	ZERO_STRUCT(u.bind_ack);
	u.bind_ack.max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	u.bind_ack.max_recv_frag = RPC_MAX_PDU_FRAG_LEN;
	u.bind_ack.assoc_group_id = assoc_gid;

	/* name has to be \PIPE\xxxxx */
	u.bind_ack.secondary_address =
			talloc_asprintf(pkt, "\\PIPE\\%s",
					rpc_srv_get_pipe_srv_name(&id));
	if (!u.bind_ack.secondary_address) {
		DEBUG(0, ("Out of memory!\n"));
		goto err_exit;
	}
	u.bind_ack.secondary_address_size =
				strlen(u.bind_ack.secondary_address) + 1;

	u.bind_ack.num_results = 1;
	u.bind_ack.ctx_list = &bind_ack_ctx;

	/* NOTE: We leave the auth_info empty so we can calculate the padding
	 * later and then append the auth_info --simo */

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	status = dcerpc_push_ncacn_packet(p->mem_ctx,
					  DCERPC_PKT_BIND_ACK,
					  DCERPC_PFC_FLAG_FIRST |
						DCERPC_PFC_FLAG_LAST,
					  auth_resp.length,
					  pkt->call_id,
					  &u,
					  &p->out_data.frag);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to marshall bind_ack packet. (%s)\n",
			  nt_errstr(status)));
		goto err_exit;
	}

	if (auth_resp.length) {
		status = dcerpc_push_dcerpc_auth(pkt,
						 auth_type,
						 auth_info.auth_level,
						 0, /* pad_len */
						 p->auth.auth_context_id,
						 &auth_resp,
						 &auth_blob);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Marshalling of dcerpc_auth failed.\n"));
			goto err_exit;
		}
	}

	/* Now that we have the auth len store it into the right place in
	 * the dcerpc header */
	dcerpc_set_frag_length(&p->out_data.frag,
				p->out_data.frag.length + auth_blob.length);

	if (auth_blob.length) {

		if (!data_blob_append(p->mem_ctx, &p->out_data.frag,
					auth_blob.data, auth_blob.length)) {
			DEBUG(0, ("Append of auth info failed.\n"));
			goto err_exit;
		}
	}

	/*
	 * Setup the lengths for the initial reply.
	 */

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	TALLOC_FREE(auth_blob.data);

	if (bind_ack_ctx.result == 0) {
		p->allow_alter = true;
		p->allow_auth3 = true;
		if (p->auth.auth_type == DCERPC_AUTH_TYPE_NONE) {
			status = pipe_auth_verify_final(p);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("pipe_auth_verify_final failed: %s\n",
					  nt_errstr(status)));
				goto err_exit;
			}
		}
	} else {
		goto err_exit;
	}

	return True;

  err_exit:

	data_blob_free(&p->out_data.frag);
	TALLOC_FREE(auth_blob.data);
	return setup_bind_nak(p, pkt);
}

/*******************************************************************
 This is the "stage3" response after a bind request and reply.
*******************************************************************/

bool api_pipe_bind_auth3(struct pipes_struct *p, struct ncacn_packet *pkt)
{
	struct dcerpc_auth auth_info;
	DATA_BLOB response = data_blob_null;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;
	NTSTATUS status;

	DEBUG(5, ("api_pipe_bind_auth3: decode request. %d\n", __LINE__));

	if (!p->allow_auth3) {
		DEBUG(1, ("Pipe not in allow auth3 state.\n"));
		goto err;
	}

	status = dcerpc_verify_ncacn_packet_header(pkt,
			DCERPC_PKT_AUTH3,
			pkt->u.auth3.auth_info.length,
			0, /* required flags */
			DCERPC_PFC_FLAG_FIRST |
			DCERPC_PFC_FLAG_LAST |
			DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN |
			0x08 | /* this is not defined, but should be ignored */
			DCERPC_PFC_FLAG_CONC_MPX |
			DCERPC_PFC_FLAG_DID_NOT_EXECUTE |
			DCERPC_PFC_FLAG_MAYBE |
			DCERPC_PFC_FLAG_OBJECT_UUID);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("api_pipe_bind_auth3: invalid pdu: %s\n",
			  nt_errstr(status)));
		goto err;
	}

	/* We can only finish if the pipe is unbound for now */
	if (p->pipe_bound) {
		DEBUG(0, (__location__ ": Pipe already bound, "
			  "AUTH3 not supported!\n"));
		goto err;
	}

	if (pkt->auth_length == 0) {
		DEBUG(1, ("No auth field sent for auth3 request!\n"));
		goto err;
	}

	/*
	 * Decode the authentication verifier response.
	 */

	status = dcerpc_pull_auth_trailer(pkt, pkt,
					  &pkt->u.auth3.auth_info,
					  &auth_info, NULL, true);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to unmarshall dcerpc_auth.\n"));
		goto err;
	}

	/* We must NEVER look at auth_info->auth_pad_len here,
	 * as old Samba client code gets it wrong and sends it
	 * as zero. JRA.
	 */

	if (auth_info.auth_type != p->auth.auth_type) {
		DEBUG(0, ("Auth type mismatch! Client sent %d, "
			  "but auth was started as type %d!\n",
			  auth_info.auth_type, p->auth.auth_type));
		goto err;
	}

	if (auth_info.auth_level != p->auth.auth_level) {
		DEBUG(1, ("Auth level mismatch! Client sent %d, "
			  "but auth was started as level %d!\n",
			  auth_info.auth_level, p->auth.auth_level));
		goto err;
	}

	if (auth_info.auth_context_id != p->auth.auth_context_id) {
		DEBUG(0, ("Auth context id mismatch! Client sent %u, "
			  "but auth was started as level %u!\n",
			  (unsigned)auth_info.auth_context_id,
			  (unsigned)p->auth.auth_context_id));
		goto err;
	}

	switch (auth_info.auth_type) {
	case DCERPC_AUTH_TYPE_NTLMSSP:
		ntlmssp_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						    struct auth_ntlmssp_state);
		status = ntlmssp_server_step(ntlmssp_ctx,
					     pkt, &auth_info.credentials,
					     &response);
		break;
	case DCERPC_AUTH_TYPE_KRB5:
		gse_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						struct gse_context);
		status = gssapi_server_step(gse_ctx,
					    pkt, &auth_info.credentials,
					    &response);
		break;
	case DCERPC_AUTH_TYPE_SPNEGO:
		spnego_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						   struct spnego_context);
		status = spnego_server_step(spnego_ctx,
					    pkt, &auth_info.credentials,
					    &response);
		break;
	default:
		DEBUG(0, (__location__ ": incorrect auth type (%u).\n",
			  (unsigned int)auth_info.auth_type));
		return false;
	}

	if (NT_STATUS_EQUAL(status,
			    NT_STATUS_MORE_PROCESSING_REQUIRED) ||
	    response.length) {
		DEBUG(0, (__location__ ": This was supposed to be the final "
			  "leg, but crypto machinery claims a response is "
			  "needed, aborting auth!\n"));
		data_blob_free(&response);
		goto err;
	}
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Auth failed (%s)\n", nt_errstr(status)));
		goto err;
	}

	/* Now verify auth was indeed successful and extract server info */
	status = pipe_auth_verify_final(p);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Auth Verify failed (%s)\n", nt_errstr(status)));
		goto err;
	}

	return true;

err:
	p->pipe_bound = false;
	p->allow_bind = false;
	p->allow_alter = false;
	p->allow_auth3 = false;

	TALLOC_FREE(p->auth.auth_ctx);
	return false;
}

/****************************************************************************
 Deal with an alter context call. Can be third part of 3 leg auth request for
 SPNEGO calls.
****************************************************************************/

static bool api_pipe_alter_context(struct pipes_struct *p,
					struct ncacn_packet *pkt)
{
	struct dcerpc_auth auth_info;
	uint16 assoc_gid;
	NTSTATUS status;
	union dcerpc_payload u;
	struct dcerpc_ack_ctx alter_ack_ctx;
	DATA_BLOB auth_resp = data_blob_null;
	DATA_BLOB auth_blob = data_blob_null;
	int pad_len = 0;
	struct auth_ntlmssp_state *ntlmssp_ctx;
	struct spnego_context *spnego_ctx;
	struct gse_context *gse_ctx;

	DEBUG(5,("api_pipe_alter_context: make response. %d\n", __LINE__));

	if (!p->allow_alter) {
		DEBUG(1, ("Pipe not in allow alter state.\n"));
		goto err_exit;
	}

	status = dcerpc_verify_ncacn_packet_header(pkt,
			DCERPC_PKT_ALTER,
			pkt->u.alter.auth_info.length,
			0, /* required flags */
			DCERPC_PFC_FLAG_FIRST |
			DCERPC_PFC_FLAG_LAST |
			DCERPC_PFC_FLAG_SUPPORT_HEADER_SIGN |
			0x08 | /* this is not defined, but should be ignored */
			DCERPC_PFC_FLAG_CONC_MPX |
			DCERPC_PFC_FLAG_DID_NOT_EXECUTE |
			DCERPC_PFC_FLAG_MAYBE |
			DCERPC_PFC_FLAG_OBJECT_UUID);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("api_pipe_alter_context: invalid pdu: %s\n",
			  nt_errstr(status)));
		goto err_exit;
	}

	if (pkt->u.alter.num_contexts == 0) {
		DEBUG(1, ("api_pipe_alter_context: no rpc contexts around\n"));
		goto err_exit;
	}

	if (pkt->u.alter.ctx_list[0].num_transfer_syntaxes == 0) {
		DEBUG(1, ("api_pipe_alter_context: no transfer syntaxes around\n"));
		goto err_exit;
	}

	if (pkt->u.alter.assoc_group_id != 0) {
		assoc_gid = pkt->u.alter.assoc_group_id;
	} else {
		assoc_gid = 0x53f0;
	}

	/*
	 * Create the bind response struct.
	 */

	/* If the requested abstract synt uuid doesn't match our client pipe,
		reject the alter_ack & set the transfer interface synt to all 0's,
		ver 0 (observed when NT5 attempts to bind to abstract interfaces
		unknown to NT4)
		Needed when adding entries to a DACL from NT5 - SK */

	if (check_bind_req(p,
			&pkt->u.alter.ctx_list[0].abstract_syntax,
			&pkt->u.alter.ctx_list[0].transfer_syntaxes[0],
			pkt->u.alter.ctx_list[0].context_id)) {

		alter_ack_ctx.result = 0;
		alter_ack_ctx.reason = 0;
		alter_ack_ctx.syntax = pkt->u.alter.ctx_list[0].transfer_syntaxes[0];
	} else {
		/* Rejection reason: abstract syntax not supported */
		alter_ack_ctx.result = DCERPC_BIND_PROVIDER_REJECT;
		alter_ack_ctx.reason = DCERPC_BIND_REASON_ASYNTAX;
		alter_ack_ctx.syntax = null_ndr_syntax_id;
	}

	/*
	 * Check if this is an authenticated alter context request.
	 */
	if (pkt->auth_length) {
		/* We can only finish if the pipe is unbound for now */
		if (p->pipe_bound) {
			DEBUG(0, (__location__ ": Pipe already bound, "
				  "Altering Context not yet supported!\n"));
			goto err_exit;
		}

		status = dcerpc_pull_auth_trailer(pkt, pkt,
						  &pkt->u.alter.auth_info,
						  &auth_info, NULL, true);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Unable to unmarshall dcerpc_auth.\n"));
			goto err_exit;
		}

		if (auth_info.auth_type != p->auth.auth_type) {
			DEBUG(0, ("Auth type mismatch! Client sent %d, "
				  "but auth was started as type %d!\n",
				  auth_info.auth_type, p->auth.auth_type));
			goto err_exit;
		}

		if (auth_info.auth_level != p->auth.auth_level) {
			DEBUG(0, ("Auth level mismatch! Client sent %d, "
				  "but auth was started as level %d!\n",
				  auth_info.auth_level, p->auth.auth_level));
			goto err_exit;
		}

		if (auth_info.auth_context_id != p->auth.auth_context_id) {
			DEBUG(0, ("Auth context id mismatch! Client sent %u, "
				  "but auth was started as level %u!\n",
				  (unsigned)auth_info.auth_context_id,
				  (unsigned)p->auth.auth_context_id));
			goto err_exit;
		}

		switch (auth_info.auth_type) {
		case DCERPC_AUTH_TYPE_SPNEGO:
			spnego_ctx = talloc_get_type_abort(p->auth.auth_ctx,
							struct spnego_context);
			status = spnego_server_step(spnego_ctx,
						    pkt,
						    &auth_info.credentials,
						    &auth_resp);
			break;

		case DCERPC_AUTH_TYPE_KRB5:
			gse_ctx = talloc_get_type_abort(p->auth.auth_ctx,
							struct gse_context);
			status = gssapi_server_step(gse_ctx,
						    pkt,
						    &auth_info.credentials,
						    &auth_resp);
			break;
		case DCERPC_AUTH_TYPE_NTLMSSP:
			ntlmssp_ctx = talloc_get_type_abort(p->auth.auth_ctx,
						    struct auth_ntlmssp_state);
			status = ntlmssp_server_step(ntlmssp_ctx,
						     pkt,
						     &auth_info.credentials,
						     &auth_resp);
			break;

		default:
			DEBUG(3, (__location__ ": Usupported auth type (%d) "
				  "in alter-context call\n",
				  auth_info.auth_type));
			goto err_exit;
		}

		if (NT_STATUS_IS_OK(status)) {
			/* third leg of auth, verify auth info */
			status = pipe_auth_verify_final(p);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(0, ("Auth Verify failed (%s)\n",
					  nt_errstr(status)));
				goto err_exit;
			}
		} else if (NT_STATUS_EQUAL(status,
					NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DEBUG(10, ("More auth legs required.\n"));
		} else {
			DEBUG(0, ("Auth step returned an error (%s)\n",
				  nt_errstr(status)));
			goto err_exit;
		}
	}

	ZERO_STRUCT(u.alter_resp);
	u.alter_resp.max_xmit_frag = RPC_MAX_PDU_FRAG_LEN;
	u.alter_resp.max_recv_frag = RPC_MAX_PDU_FRAG_LEN;
	u.alter_resp.assoc_group_id = assoc_gid;

	/* secondary address CAN be NULL
	 * as the specs say it's ignored.
	 * It MUST be NULL to have the spoolss working.
	 */
	u.alter_resp.secondary_address = "";
	u.alter_resp.secondary_address_size = 1;

	u.alter_resp.num_results = 1;
	u.alter_resp.ctx_list = &alter_ack_ctx;

	/* NOTE: We leave the auth_info empty so we can calculate the padding
	 * later and then append the auth_info --simo */

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	status = dcerpc_push_ncacn_packet(p->mem_ctx,
					  DCERPC_PKT_ALTER_RESP,
					  DCERPC_PFC_FLAG_FIRST |
						DCERPC_PFC_FLAG_LAST,
					  auth_resp.length,
					  pkt->call_id,
					  &u,
					  &p->out_data.frag);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to marshall alter_resp packet. (%s)\n",
			  nt_errstr(status)));
		goto err_exit;
	}

	if (auth_resp.length) {

		/* Work out any padding needed before the auth footer. */
		pad_len = p->out_data.frag.length % SERVER_NDR_PADDING_SIZE;
		if (pad_len) {
			pad_len = SERVER_NDR_PADDING_SIZE - pad_len;
			DEBUG(10, ("auth pad_len = %u\n",
				   (unsigned int)pad_len));
		}

		status = dcerpc_push_dcerpc_auth(pkt,
						 auth_info.auth_type,
						 auth_info.auth_level,
						 pad_len,
						 p->auth.auth_context_id,
						 &auth_resp,
						 &auth_blob);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Marshalling of dcerpc_auth failed.\n"));
			goto err_exit;
		}
	}

	/* Now that we have the auth len store it into the right place in
	 * the dcerpc header */
	dcerpc_set_frag_length(&p->out_data.frag,
				p->out_data.frag.length +
					pad_len + auth_blob.length);

	if (auth_resp.length) {
		if (pad_len) {
			char pad[SERVER_NDR_PADDING_SIZE];
			memset(pad, '\0', SERVER_NDR_PADDING_SIZE);
			if (!data_blob_append(p->mem_ctx,
						&p->out_data.frag,
						pad, pad_len)) {
				DEBUG(0, ("api_pipe_bind_req: failed to add "
					  "%u bytes of pad data.\n",
					  (unsigned int)pad_len));
				goto err_exit;
			}
		}

		if (!data_blob_append(p->mem_ctx, &p->out_data.frag,
					auth_blob.data, auth_blob.length)) {
			DEBUG(0, ("Append of auth info failed.\n"));
			goto err_exit;
		}
	}

	/*
	 * Setup the lengths for the initial reply.
	 */

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_sent = 0;

	TALLOC_FREE(auth_blob.data);
	return True;

  err_exit:

	data_blob_free(&p->out_data.frag);
	TALLOC_FREE(auth_blob.data);
	return setup_bind_nak(p, pkt);
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

static bool api_rpcTNP(struct pipes_struct *p, struct ncacn_packet *pkt,
		       const struct api_struct *api_rpc_cmds, int n_cmds,
		       const struct ndr_syntax_id *syntax);

static bool srv_pipe_check_verification_trailer(struct pipes_struct *p,
						struct ncacn_packet *pkt,
						struct pipe_rpc_fns *pipe_fns)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct dcerpc_sec_verification_trailer *vt = NULL;
	const uint32_t bitmask1 = 0;
	const struct dcerpc_sec_vt_pcontext pcontext = {
		.abstract_syntax = pipe_fns->syntax,
		.transfer_syntax = ndr_transfer_syntax,
	};
	const struct dcerpc_sec_vt_header2 header2 =
	       dcerpc_sec_vt_header2_from_ncacn_packet(pkt);
	struct ndr_pull *ndr;
	enum ndr_err_code ndr_err;
	bool ret = false;

	ndr = ndr_pull_init_blob(&p->in_data.data, frame);
	if (ndr == NULL) {
		goto done;
	}

	ndr_err = ndr_pop_dcerpc_sec_verification_trailer(ndr, frame, &vt);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		goto done;
	}

	ret = dcerpc_sec_verification_trailer_check(vt, &bitmask1,
						    &pcontext, &header2);
done:
	TALLOC_FREE(frame);
	return ret;
}

/****************************************************************************
 Find the correct RPC function to call for this request.
 If the pipe is authenticated then become the correct UNIX user
 before doing the call.
****************************************************************************/

static bool api_pipe_request(struct pipes_struct *p,
				struct ncacn_packet *pkt)
{
	TALLOC_CTX *frame = talloc_stackframe();
	bool ret = False;
	PIPE_RPC_FNS *pipe_fns;
	const char *interface_name = NULL;

	if (!p->pipe_bound) {
		DEBUG(1, ("Pipe not bound!\n"));
		data_blob_free(&p->out_data.rdata);
		TALLOC_FREE(frame);
		return false;
	}

	/* get the set of RPC functions for this context */

	pipe_fns = find_pipe_fns_by_context(p->contexts,
					    pkt->u.request.context_id);
	if (pipe_fns == NULL) {
		DEBUG(0, ("No rpc function table associated with context "
			  "[%d]\n",
			  pkt->u.request.context_id));
		data_blob_free(&p->out_data.rdata);
		TALLOC_FREE(frame);
		return false;
	}

	interface_name = get_pipe_name_from_syntax(talloc_tos(),
						   &pipe_fns->syntax);

	SMB_ASSERT(interface_name != NULL);

	DEBUG(5, ("Requested \\PIPE\\%s\n",
		  interface_name));

	switch (p->auth.auth_level) {
	case DCERPC_AUTH_LEVEL_NONE:
	case DCERPC_AUTH_LEVEL_INTEGRITY:
	case DCERPC_AUTH_LEVEL_PRIVACY:
		break;
	default:
		if (!pipe_fns->allow_connect) {
			DEBUG(1, ("%s: restrict auth_level_connect access "
				  "to [%s] with auth[type=0x%x,level=0x%x] "
				  "on [%s] from [%s]\n",
				  __func__, interface_name,
				  p->auth.auth_type,
				  p->auth.auth_level,
				  derpc_transport_string_by_transport(p->transport),
				  p->client_id->name));

			setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_ACCESS_DENIED));
			TALLOC_FREE(frame);
			return true;
		}
		break;
	}

	if (!srv_pipe_check_verification_trailer(p, pkt, pipe_fns)) {
		DEBUG(1, ("srv_pipe_check_verification_trailer: failed\n"));
		set_incoming_fault(p);
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_ACCESS_DENIED));
		data_blob_free(&p->out_data.rdata);
		TALLOC_FREE(frame);
		return true;
	}

	if (!become_authenticated_pipe_user(p->session_info)) {
		DEBUG(1, ("Failed to become pipe user!\n"));
		data_blob_free(&p->out_data.rdata);
		TALLOC_FREE(frame);
		return false;
	}

	ret = api_rpcTNP(p, pkt, pipe_fns->cmds, pipe_fns->n_cmds,
			 &pipe_fns->syntax);
	unbecome_authenticated_pipe_user();

	TALLOC_FREE(frame);
	return ret;
}

/*******************************************************************
 Calls the underlying RPC function for a named pipe.
 ********************************************************************/

static bool api_rpcTNP(struct pipes_struct *p, struct ncacn_packet *pkt,
		       const struct api_struct *api_rpc_cmds, int n_cmds,
		       const struct ndr_syntax_id *syntax)
{
	int fn_num;
	uint32_t offset1;

	/* interpret the command */
	DEBUG(4,("api_rpcTNP: %s op 0x%x - ",
		 get_pipe_name_from_syntax(talloc_tos(), syntax),
		 pkt->u.request.opnum));

	if (DEBUGLEVEL >= 50) {
		fstring name;
		slprintf(name, sizeof(name)-1, "in_%s",
			 get_pipe_name_from_syntax(talloc_tos(), syntax));
		dump_pdu_region(name, pkt->u.request.opnum,
				&p->in_data.data, 0,
				p->in_data.data.length);
	}

	for (fn_num = 0; fn_num < n_cmds; fn_num++) {
		if (api_rpc_cmds[fn_num].opnum == pkt->u.request.opnum &&
		    api_rpc_cmds[fn_num].fn != NULL) {
			DEBUG(3, ("api_rpcTNP: rpc command: %s\n",
				  api_rpc_cmds[fn_num].name));
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

	offset1 = p->out_data.rdata.length;

        DEBUG(6, ("api_rpc_cmds[%d].fn == %p\n", 
                fn_num, api_rpc_cmds[fn_num].fn));
	/* do the actual command */
	if(!api_rpc_cmds[fn_num].fn(p)) {
		DEBUG(0,("api_rpcTNP: %s: %s failed.\n",
			 get_pipe_name_from_syntax(talloc_tos(), syntax),
			 api_rpc_cmds[fn_num].name));
		data_blob_free(&p->out_data.rdata);
		return False;
	}

	if (p->fault_state) {
		DEBUG(4,("api_rpcTNP: fault(%d) return.\n", p->fault_state));
		setup_fault_pdu(p, NT_STATUS(p->fault_state));
		p->fault_state = 0;
		return true;
	}

	if (DEBUGLEVEL >= 50) {
		fstring name;
		slprintf(name, sizeof(name)-1, "out_%s",
			 get_pipe_name_from_syntax(talloc_tos(), syntax));
		dump_pdu_region(name, pkt->u.request.opnum,
				&p->out_data.rdata, offset1,
				p->out_data.rdata.length);
	}

	DEBUG(5,("api_rpcTNP: called %s successfully\n",
		 get_pipe_name_from_syntax(talloc_tos(), syntax)));

	/* Check for buffer underflow in rpc parsing */
	if ((DEBUGLEVEL >= 10) &&
	    (pkt->frag_length < p->in_data.data.length)) {
		DEBUG(10, ("api_rpcTNP: rpc input buffer underflow (parse error?)\n"));
		dump_data(10, p->in_data.data.data + pkt->frag_length,
			      p->in_data.data.length - pkt->frag_length);
	}

	return True;
}

/****************************************************************************
 Initialise an outgoing packet.
****************************************************************************/

static bool pipe_init_outgoing_data(struct pipes_struct *p)
{
	output_data *o_data = &p->out_data;

	/* Reset the offset counters. */
	o_data->data_sent_length = 0;
	o_data->current_pdu_sent = 0;

	data_blob_free(&o_data->frag);

	/* Free any memory in the current return data buffer. */
	data_blob_free(&o_data->rdata);

	return True;
}

/****************************************************************************
 Sets the fault state on incoming packets.
****************************************************************************/

void set_incoming_fault(struct pipes_struct *p)
{
	data_blob_free(&p->in_data.data);
	p->in_data.pdu_needed_len = 0;
	p->in_data.pdu.length = 0;
	p->fault_state = DCERPC_NCA_S_PROTO_ERROR;

	p->allow_alter = false;
	p->allow_auth3 = false;
	p->pipe_bound = false;

	DEBUG(10, ("Setting fault state\n"));
}

static NTSTATUS dcesrv_auth_request(struct pipe_auth_data *auth,
				    struct ncacn_packet *pkt,
				    DATA_BLOB *raw_pkt)
{
	NTSTATUS status;
	size_t hdr_size = DCERPC_REQUEST_LENGTH;

	DEBUG(10, ("Checking request auth.\n"));

	if (pkt->pfc_flags & DCERPC_PFC_FLAG_OBJECT_UUID) {
		hdr_size += 16;
	}

	/* in case of sealing this function will unseal the data in place */
	status = dcerpc_check_auth(auth, pkt,
				   &pkt->u.request.stub_and_verifier,
				   hdr_size, raw_pkt);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Processes a request pdu. This will do auth processing if needed, and
 appends the data into the complete stream if the LAST flag is not set.
****************************************************************************/

static bool process_request_pdu(struct pipes_struct *p, struct ncacn_packet *pkt)
{
	NTSTATUS status;
	DATA_BLOB data;

	if (!p->pipe_bound) {
		DEBUG(0,("process_request_pdu: rpc request with no bind.\n"));
		set_incoming_fault(p);
		return False;
	}

	/*
	 * We don't ignore DCERPC_PFC_FLAG_PENDING_CANCEL.
	 * TODO: we can reject it with DCERPC_FAULT_NO_CALL_ACTIVE later.
	 */
	status = dcerpc_verify_ncacn_packet_header(pkt,
			DCERPC_PKT_REQUEST,
			pkt->u.request.stub_and_verifier.length,
			0, /* required_flags */
			DCERPC_PFC_FLAG_FIRST |
			DCERPC_PFC_FLAG_LAST |
			0x08 | /* this is not defined, but should be ignored */
			DCERPC_PFC_FLAG_CONC_MPX |
			DCERPC_PFC_FLAG_DID_NOT_EXECUTE |
			DCERPC_PFC_FLAG_MAYBE |
			DCERPC_PFC_FLAG_OBJECT_UUID);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("process_request_pdu: invalid pdu: %s\n",
			  nt_errstr(status)));
		set_incoming_fault(p);
		return false;
	}

	/* Store the opnum */
	p->opnum = pkt->u.request.opnum;

	status = dcesrv_auth_request(&p->auth, pkt, &p->in_data.pdu);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to check packet auth. (%s)\n",
			  nt_errstr(status)));
		set_incoming_fault(p);
		return false;
	}

	data = pkt->u.request.stub_and_verifier;

	/*
	 * Check the data length doesn't go over the 15Mb limit.
	 * increased after observing a bug in the Windows NT 4.0 SP6a
	 * spoolsv.exe when the response to a GETPRINTERDRIVER2 RPC
	 * will not fit in the initial buffer of size 0x1068   --jerry 22/01/2002
	 */

	if (p->in_data.data.length + data.length > MAX_RPC_DATA_SIZE) {
		DEBUG(0, ("process_request_pdu: "
			  "rpc data buffer too large (%u) + (%u)\n",
			  (unsigned int)p->in_data.data.length,
			  (unsigned int)data.length));
		set_incoming_fault(p);
		return False;
	}

	/*
	 * Append the data portion into the buffer and return.
	 */

	if (data.length) {
		if (!data_blob_append(p->mem_ctx, &p->in_data.data,
					  data.data, data.length)) {
			DEBUG(0, ("Unable to append data size %u "
				  "to parse buffer of size %u.\n",
				  (unsigned int)data.length,
				  (unsigned int)p->in_data.data.length));
			set_incoming_fault(p);
			return False;
		}
	}

	if (pkt->pfc_flags & DCERPC_PFC_FLAG_LAST) {
		bool ret = False;
		/*
		 * Ok - we finally have a complete RPC stream.
		 * Call the rpc command to process it.
		 */

		/*
		 * Process the complete data stream here.
		 */
		if (pipe_init_outgoing_data(p)) {
			ret = api_pipe_request(p, pkt);
		}

		return ret;
	}

	return True;
}

/****************************************************************************
 Processes a finished PDU stored in p->in_data.pdu.
****************************************************************************/

void process_complete_pdu(struct pipes_struct *p)
{
	struct ncacn_packet *pkt = NULL;
	NTSTATUS status;
	bool reply = False;

	if(p->fault_state) {
		DEBUG(10,("process_complete_pdu: pipe %s in fault state.\n",
			  get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
		goto done;
	}

	pkt = talloc(p->mem_ctx, struct ncacn_packet);
	if (!pkt) {
		DEBUG(0, ("Out of memory!\n"));
		goto done;
	}

	/*
	 * Ensure we're using the corrent endianness for both the
	 * RPC header flags and the raw data we will be reading from.
	 */
	if (dcerpc_get_endian_flag(&p->in_data.pdu) & DCERPC_DREP_LE) {
		p->endian = RPC_LITTLE_ENDIAN;
	} else {
		p->endian = RPC_BIG_ENDIAN;
	}
	DEBUG(10, ("PDU is in %s Endian format!\n", p->endian?"Big":"Little"));

	status = dcerpc_pull_ncacn_packet(pkt, &p->in_data.pdu,
					  pkt, p->endian);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to unmarshal rpc packet: %s!\n",
			  nt_errstr(status)));
		goto done;
	}

	/* Store the call_id */
	p->call_id = pkt->call_id;

	DEBUG(10, ("Processing packet type %d\n", (int)pkt->ptype));

	switch (pkt->ptype) {
	case DCERPC_PKT_REQUEST:
		reply = process_request_pdu(p, pkt);
		break;

	case DCERPC_PKT_PING: /* CL request - ignore... */
		DEBUG(0, ("process_complete_pdu: Error. "
			  "Connectionless packet type %d received on "
			  "pipe %s.\n", (int)pkt->ptype,
			 get_pipe_name_from_syntax(talloc_tos(),
						   &p->syntax)));
		break;

	case DCERPC_PKT_RESPONSE: /* No responses here. */
		DEBUG(0, ("process_complete_pdu: Error. "
			  "DCERPC_PKT_RESPONSE received from client "
			  "on pipe %s.\n",
			 get_pipe_name_from_syntax(talloc_tos(),
						   &p->syntax)));
		break;

	case DCERPC_PKT_FAULT:
	case DCERPC_PKT_WORKING:
		/* CL request - reply to a ping when a call in process. */
	case DCERPC_PKT_NOCALL:
		/* CL - server reply to a ping call. */
	case DCERPC_PKT_REJECT:
	case DCERPC_PKT_ACK:
	case DCERPC_PKT_CL_CANCEL:
	case DCERPC_PKT_FACK:
	case DCERPC_PKT_CANCEL_ACK:
		DEBUG(0, ("process_complete_pdu: Error. "
			  "Connectionless packet type %u received on "
			  "pipe %s.\n", (unsigned int)pkt->ptype,
			 get_pipe_name_from_syntax(talloc_tos(),
						   &p->syntax)));
		break;

	case DCERPC_PKT_BIND:
		/*
		 * We assume that a pipe bind is only in one pdu.
		 */
		if (pipe_init_outgoing_data(p)) {
			reply = api_pipe_bind_req(p, pkt);
		}
		break;

	case DCERPC_PKT_BIND_ACK:
	case DCERPC_PKT_BIND_NAK:
		DEBUG(0, ("process_complete_pdu: Error. "
			  "DCERPC_PKT_BINDACK/DCERPC_PKT_BINDNACK "
			  "packet type %u received on pipe %s.\n",
			  (unsigned int)pkt->ptype,
			 get_pipe_name_from_syntax(talloc_tos(),
						   &p->syntax)));
		break;


	case DCERPC_PKT_ALTER:
		/*
		 * We assume that a pipe bind is only in one pdu.
		 */
		if (pipe_init_outgoing_data(p)) {
			reply = api_pipe_alter_context(p, pkt);
		}
		break;

	case DCERPC_PKT_ALTER_RESP:
		DEBUG(0, ("process_complete_pdu: Error. "
			  "DCERPC_PKT_ALTER_RESP on pipe %s: "
			  "Should only be server -> client.\n",
			 get_pipe_name_from_syntax(talloc_tos(),
						   &p->syntax)));
		break;

	case DCERPC_PKT_AUTH3:
		/*
		 * The third packet in an auth exchange.
		 */
		if (pipe_init_outgoing_data(p)) {
			reply = api_pipe_bind_auth3(p, pkt);
		}
		break;

	case DCERPC_PKT_SHUTDOWN:
		DEBUG(0, ("process_complete_pdu: Error. "
			  "DCERPC_PKT_SHUTDOWN on pipe %s: "
			  "Should only be server -> client.\n",
			 get_pipe_name_from_syntax(talloc_tos(),
						   &p->syntax)));
		break;

	case DCERPC_PKT_CO_CANCEL:
		/* For now just free all client data and continue
		 * processing. */
		DEBUG(3,("process_complete_pdu: DCERPC_PKT_CO_CANCEL."
			 " Abandoning rpc call.\n"));
		/* As we never do asynchronous RPC serving, we can
		 * never cancel a call (as far as I know).
		 * If we ever did we'd have to send a cancel_ack reply.
		 * For now, just free all client data and continue
		 * processing. */
		reply = True;
		break;

#if 0
		/* Enable this if we're doing async rpc. */
		/* We must check the outstanding callid matches. */
		if (pipe_init_outgoing_data(p)) {
			/* Send a cancel_ack PDU reply. */
			/* We should probably check the auth-verifier here. */
			reply = setup_cancel_ack_reply(p, pkt);
		}
		break;
#endif

	case DCERPC_PKT_ORPHANED:
		/* We should probably check the auth-verifier here.
		 * For now just free all client data and continue
		 * processing. */
		DEBUG(3, ("process_complete_pdu: DCERPC_PKT_ORPHANED."
			  " Abandoning rpc call.\n"));
		reply = True;
		break;

	default:
		DEBUG(0, ("process_complete_pdu: "
			  "Unknown rpc type = %u received.\n",
			  (unsigned int)pkt->ptype));
		break;
	}

done:
	if (!reply) {
		DEBUG(3,("process_complete_pdu: DCE/RPC fault sent on "
			 "pipe %s\n", get_pipe_name_from_syntax(talloc_tos(),
								&p->syntax)));
		set_incoming_fault(p);
		setup_fault_pdu(p, NT_STATUS(DCERPC_NCA_S_PROTO_ERROR));
		TALLOC_FREE(pkt);
	} else {
		/*
		 * Reset the lengths. We're ready for a new pdu.
		 */
		TALLOC_FREE(p->in_data.pdu.data);
		p->in_data.pdu_needed_len = 0;
		p->in_data.pdu.length = 0;
	}

	TALLOC_FREE(pkt);
}

