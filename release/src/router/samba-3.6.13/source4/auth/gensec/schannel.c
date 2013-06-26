/*
   Unix SMB/CIFS implementation.

   dcerpc schannel operations

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005

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
#include "librpc/gen_ndr/ndr_schannel.h"
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "../libcli/auth/schannel.h"
#include "librpc/rpc/dcerpc.h"
#include "param/param.h"

static size_t schannel_sig_size(struct gensec_security *gensec_security, size_t data_size)
{
	struct schannel_state *state = (struct schannel_state *)gensec_security->private_data;
	uint32_t sig_size;

	sig_size = netsec_outgoing_sig_size(state);

	return sig_size;
}

static NTSTATUS schannel_session_key(struct gensec_security *gensec_security,
					    DATA_BLOB *session_key)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS schannel_update(struct gensec_security *gensec_security, TALLOC_CTX *out_mem_ctx,
				       const DATA_BLOB in, DATA_BLOB *out)
{
	struct schannel_state *state = (struct schannel_state *)gensec_security->private_data;
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct NL_AUTH_MESSAGE bind_schannel;
	struct NL_AUTH_MESSAGE bind_schannel_ack;
	struct netlogon_creds_CredentialState *creds;
	const char *workstation;
	const char *domain;

	*out = data_blob(NULL, 0);

	switch (gensec_security->gensec_role) {
	case GENSEC_CLIENT:
		if (state->state != SCHANNEL_STATE_START) {
			/* we could parse the bind ack, but we don't know what it is yet */
			return NT_STATUS_OK;
		}

		state->creds = talloc_reference(state, cli_credentials_get_netlogon_creds(gensec_security->credentials));

		bind_schannel.MessageType = NL_NEGOTIATE_REQUEST;
#if 0
		/* to support this we'd need to have access to the full domain name */
		/* 0x17, 23 */
		bind_schannel.Flags = NL_FLAG_OEM_NETBIOS_DOMAIN_NAME |
				      NL_FLAG_OEM_NETBIOS_COMPUTER_NAME |
				      NL_FLAG_UTF8_DNS_DOMAIN_NAME |
				      NL_FLAG_UTF8_NETBIOS_COMPUTER_NAME;
		bind_schannel.oem_netbios_domain.a = cli_credentials_get_domain(gensec_security->credentials);
		bind_schannel.oem_netbios_computer.a = cli_credentials_get_workstation(gensec_security->credentials);
		bind_schannel.utf8_dns_domain = cli_credentials_get_realm(gensec_security->credentials);
		/* w2k3 refuses us if we use the full DNS workstation?
		 why? perhaps because we don't fill in the dNSHostName
		 attribute in the machine account? */
		bind_schannel.utf8_netbios_computer = cli_credentials_get_workstation(gensec_security->credentials);
#else
		bind_schannel.Flags = NL_FLAG_OEM_NETBIOS_DOMAIN_NAME |
				      NL_FLAG_OEM_NETBIOS_COMPUTER_NAME;
		bind_schannel.oem_netbios_domain.a = cli_credentials_get_domain(gensec_security->credentials);
		bind_schannel.oem_netbios_computer.a = cli_credentials_get_workstation(gensec_security->credentials);
#endif

		ndr_err = ndr_push_struct_blob(out, out_mem_ctx, &bind_schannel,
					       (ndr_push_flags_fn_t)ndr_push_NL_AUTH_MESSAGE);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(3, ("Could not create schannel bind: %s\n",
				  nt_errstr(status)));
			return status;
		}

		state->state = SCHANNEL_STATE_UPDATE_1;

		return NT_STATUS_MORE_PROCESSING_REQUIRED;
	case GENSEC_SERVER:

		if (state->state != SCHANNEL_STATE_START) {
			/* no third leg on this protocol */
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* parse the schannel startup blob */
		ndr_err = ndr_pull_struct_blob(&in, out_mem_ctx, &bind_schannel,
			(ndr_pull_flags_fn_t)ndr_pull_NL_AUTH_MESSAGE);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(3, ("Could not parse incoming schannel bind: %s\n",
				  nt_errstr(status)));
			return status;
		}

		if (bind_schannel.Flags & NL_FLAG_OEM_NETBIOS_DOMAIN_NAME) {
			domain = bind_schannel.oem_netbios_domain.a;
			if (strcasecmp_m(domain, lpcfg_workgroup(gensec_security->settings->lp_ctx)) != 0) {
				DEBUG(3, ("Request for schannel to incorrect domain: %s != our domain %s\n",
					  domain, lpcfg_workgroup(gensec_security->settings->lp_ctx)));
				return NT_STATUS_LOGON_FAILURE;
			}
		} else if (bind_schannel.Flags & NL_FLAG_UTF8_DNS_DOMAIN_NAME) {
			domain = bind_schannel.utf8_dns_domain.u;
			if (strcasecmp_m(domain, lpcfg_dnsdomain(gensec_security->settings->lp_ctx)) != 0) {
				DEBUG(3, ("Request for schannel to incorrect domain: %s != our domain %s\n",
					  domain, lpcfg_dnsdomain(gensec_security->settings->lp_ctx)));
				return NT_STATUS_LOGON_FAILURE;
			}
		} else {
			DEBUG(3, ("Request for schannel to without domain\n"));
			return NT_STATUS_LOGON_FAILURE;
		}

		if (bind_schannel.Flags & NL_FLAG_OEM_NETBIOS_COMPUTER_NAME) {
			workstation = bind_schannel.oem_netbios_computer.a;
		} else if (bind_schannel.Flags & NL_FLAG_UTF8_NETBIOS_COMPUTER_NAME) {
			workstation = bind_schannel.utf8_netbios_computer.u;
		} else {
			DEBUG(3, ("Request for schannel to without netbios workstation\n"));
			return NT_STATUS_LOGON_FAILURE;
		}

		status = schannel_get_creds_state(out_mem_ctx,
						  lpcfg_private_dir(gensec_security->settings->lp_ctx),
						  workstation, &creds);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(3, ("Could not find session key for attempted schannel connection from %s: %s\n",
				  workstation, nt_errstr(status)));
			if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
				return NT_STATUS_LOGON_FAILURE;
			}
			return status;
		}

		state->creds = talloc_steal(state, creds);

		bind_schannel_ack.MessageType = NL_NEGOTIATE_RESPONSE;
		bind_schannel_ack.Flags = 0;
		bind_schannel_ack.Buffer.dummy = 0x6c0000; /* actually I think
							    * this does not have
							    * any meaning here
							    * - gd */

		ndr_err = ndr_push_struct_blob(out, out_mem_ctx, &bind_schannel_ack,
					       (ndr_push_flags_fn_t)ndr_push_NL_AUTH_MESSAGE);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			DEBUG(3, ("Could not return schannel bind ack for client %s: %s\n",
				  workstation, nt_errstr(status)));
			return status;
		}

		state->state = SCHANNEL_STATE_UPDATE_1;

		return NT_STATUS_OK;
	}
	return NT_STATUS_INVALID_PARAMETER;
}

/**
 * Return the struct netlogon_creds_CredentialState.
 *
 * Make sure not to call this unless gensec is using schannel...
 */

/* TODO: make this non-public */

_PUBLIC_ NTSTATUS dcerpc_schannel_creds(struct gensec_security *gensec_security,
					TALLOC_CTX *mem_ctx,
					struct netlogon_creds_CredentialState **creds)
{
	struct schannel_state *state = talloc_get_type(gensec_security->private_data, struct schannel_state);

	*creds = talloc_reference(mem_ctx, state->creds);
	if (!*creds) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}


/**
 * Returns anonymous credentials for schannel, matching Win2k3.
 *
 */

static NTSTATUS schannel_session_info(struct gensec_security *gensec_security,
					 struct auth_session_info **_session_info)
{
	struct schannel_state *state = talloc_get_type(gensec_security->private_data, struct schannel_state);
	return auth_anonymous_session_info(state, gensec_security->settings->lp_ctx, _session_info);
}

static NTSTATUS schannel_start(struct gensec_security *gensec_security)
{
	struct schannel_state *state;

	state = talloc(gensec_security, struct schannel_state);
	if (!state) {
		return NT_STATUS_NO_MEMORY;
	}

	state->state = SCHANNEL_STATE_START;
	state->seq_num = 0;
	gensec_security->private_data = state;

	return NT_STATUS_OK;
}

static NTSTATUS schannel_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS status;
	struct schannel_state *state;

	status = schannel_start(gensec_security);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	state = (struct schannel_state *)gensec_security->private_data;
	state->initiator = false;

	return NT_STATUS_OK;
}

static NTSTATUS schannel_client_start(struct gensec_security *gensec_security)
{
	NTSTATUS status;
	struct schannel_state *state;

	status = schannel_start(gensec_security);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	state = (struct schannel_state *)gensec_security->private_data;
	state->initiator = true;

	return NT_STATUS_OK;
}


static bool schannel_have_feature(struct gensec_security *gensec_security,
					 uint32_t feature)
{
	if (feature & (GENSEC_FEATURE_SIGN |
		       GENSEC_FEATURE_SEAL)) {
		return true;
	}
	if (feature & GENSEC_FEATURE_DCE_STYLE) {
		return true;
	}
	if (feature & GENSEC_FEATURE_ASYNC_REPLIES) {
		return true;
	}
	return false;
}

/*
  unseal a packet
*/
static NTSTATUS schannel_unseal_packet(struct gensec_security *gensec_security,
				       TALLOC_CTX *mem_ctx,
				       uint8_t *data, size_t length,
				       const uint8_t *whole_pdu, size_t pdu_length,
				       const DATA_BLOB *sig)
{
	struct schannel_state *state =
		talloc_get_type(gensec_security->private_data,
				struct schannel_state);

	return netsec_incoming_packet(state, mem_ctx, true,
				      discard_const_p(uint8_t, data),
				      length, sig);
}

/*
  check the signature on a packet
*/
static NTSTATUS schannel_check_packet(struct gensec_security *gensec_security,
				      TALLOC_CTX *mem_ctx,
				      const uint8_t *data, size_t length,
				      const uint8_t *whole_pdu, size_t pdu_length,
				      const DATA_BLOB *sig)
{
	struct schannel_state *state =
		talloc_get_type(gensec_security->private_data,
				struct schannel_state);

	return netsec_incoming_packet(state, mem_ctx, false,
				      discard_const_p(uint8_t, data),
				      length, sig);
}
/*
  seal a packet
*/
static NTSTATUS schannel_seal_packet(struct gensec_security *gensec_security,
				     TALLOC_CTX *mem_ctx,
				     uint8_t *data, size_t length,
				     const uint8_t *whole_pdu, size_t pdu_length,
				     DATA_BLOB *sig)
{
	struct schannel_state *state =
		talloc_get_type(gensec_security->private_data,
				struct schannel_state);

	return netsec_outgoing_packet(state, mem_ctx, true,
				      data, length, sig);
}

/*
  sign a packet
*/
static NTSTATUS schannel_sign_packet(struct gensec_security *gensec_security,
				     TALLOC_CTX *mem_ctx,
				     const uint8_t *data, size_t length,
				     const uint8_t *whole_pdu, size_t pdu_length,
				     DATA_BLOB *sig)
{
	struct schannel_state *state =
		talloc_get_type(gensec_security->private_data,
				struct schannel_state);

	return netsec_outgoing_packet(state, mem_ctx, false,
				      discard_const_p(uint8_t, data),
				      length, sig);
}

static const struct gensec_security_ops gensec_schannel_security_ops = {
	.name		= "schannel",
	.auth_type	= DCERPC_AUTH_TYPE_SCHANNEL,
	.client_start   = schannel_client_start,
	.server_start   = schannel_server_start,
	.update 	= schannel_update,
	.seal_packet 	= schannel_seal_packet,
	.sign_packet   	= schannel_sign_packet,
	.check_packet	= schannel_check_packet,
	.unseal_packet 	= schannel_unseal_packet,
	.session_key	= schannel_session_key,
	.session_info	= schannel_session_info,
	.sig_size	= schannel_sig_size,
	.have_feature   = schannel_have_feature,
	.enabled        = true,
	.priority       = GENSEC_SCHANNEL
};

_PUBLIC_ NTSTATUS gensec_schannel_init(void)
{
	NTSTATUS ret;
	ret = gensec_register(&gensec_schannel_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_schannel_security_ops.name));
		return ret;
	}

	return ret;
}
