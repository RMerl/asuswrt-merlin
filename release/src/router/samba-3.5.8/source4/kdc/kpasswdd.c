/* 
   Unix SMB/CIFS implementation.

   kpasswd Server implementation

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell	2005

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
#include "smbd/service_task.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "system/network.h"
#include "../lib/util/dlinklist.h"
#include "lib/ldb/include/ldb.h"
#include "auth/gensec/gensec.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "auth/auth.h"
#include "dsdb/samdb/samdb.h"
#include "rpc_server/dcerpc_server.h"
#include "rpc_server/samr/proto.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "kdc/kdc.h"

/* TODO: remove all SAMBA4_INTERNAL_HEIMDAL stuff from this file */
#ifdef SAMBA4_INTERNAL_HEIMDAL
#include "heimdal_build/kpasswdd-glue.h"
#endif

/* hold information about one kdc socket */
struct kpasswd_socket {
	struct socket_context *sock;
	struct kdc_server *kdc;
	struct tevent_fd *fde;

	/* a queue of outgoing replies that have been deferred */
	struct kdc_reply *send_queue;
};

/* Return true if there is a valid error packet formed in the error_blob */
static bool kpasswdd_make_error_reply(struct kdc_server *kdc, 
				     TALLOC_CTX *mem_ctx, 
				     uint16_t result_code, 
				     const char *error_string, 
				     DATA_BLOB *error_blob) 
{
	char *error_string_utf8;
	size_t len;
	
	DEBUG(result_code ? 3 : 10, ("kpasswdd: %s\n", error_string));

	if (!push_utf8_talloc(mem_ctx, &error_string_utf8, error_string, &len)) {
		return false;
	}

	*error_blob = data_blob_talloc(mem_ctx, NULL, 2 + len + 1);
	if (!error_blob->data) {
		return false;
	}
	RSSVAL(error_blob->data, 0, result_code);
	memcpy(error_blob->data + 2, error_string_utf8, len + 1);
	return true;
}

/* Return true if there is a valid error packet formed in the error_blob */
static bool kpasswdd_make_unauth_error_reply(struct kdc_server *kdc, 
					    TALLOC_CTX *mem_ctx, 
					    uint16_t result_code, 
					    const char *error_string, 
					    DATA_BLOB *error_blob) 
{
	bool ret;
	int kret;
	DATA_BLOB error_bytes;
	krb5_data k5_error_bytes, k5_error_blob;
	ret = kpasswdd_make_error_reply(kdc, mem_ctx, result_code, error_string, 
				       &error_bytes);
	if (!ret) {
		return false;
	}
	k5_error_bytes.data = error_bytes.data;
	k5_error_bytes.length = error_bytes.length;
	kret = krb5_mk_error(kdc->smb_krb5_context->krb5_context,
			     result_code, NULL, &k5_error_bytes, 
			     NULL, NULL, NULL, NULL, &k5_error_blob);
	if (kret) {
		return false;
	}
	*error_blob = data_blob_talloc(mem_ctx, k5_error_blob.data, k5_error_blob.length);
	krb5_data_free(&k5_error_blob);
	if (!error_blob->data) {
		return false;
	}
	return true;
}

static bool kpasswd_make_pwchange_reply(struct kdc_server *kdc, 
					TALLOC_CTX *mem_ctx, 
					NTSTATUS status, 
					enum samr_RejectReason reject_reason,
					struct samr_DomInfo1 *dominfo,
					DATA_BLOB *error_blob) 
{
	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_ACCESSDENIED,
						"No such user when changing password",
						error_blob);
	}
	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_ACCESSDENIED,
						"Not permitted to change password",
						error_blob);
	}
	if (dominfo && NT_STATUS_EQUAL(status, NT_STATUS_PASSWORD_RESTRICTION)) {
		const char *reject_string;
		switch (reject_reason) {
		case SAMR_REJECT_TOO_SHORT:
			reject_string = talloc_asprintf(mem_ctx, "Password too short, password must be at least %d characters long",
							dominfo->min_password_length);
			break;
		case SAMR_REJECT_COMPLEXITY:
			reject_string = "Password does not meet complexity requirements";
			break;
		case SAMR_REJECT_IN_HISTORY:
			reject_string = "Password is already in password history";
			break;
		case SAMR_REJECT_OTHER:
		default:
			reject_string = talloc_asprintf(mem_ctx, "Password must be at least %d characters long, and cannot match any of your %d previous passwords",
							dominfo->min_password_length, dominfo->password_history_length);
			break;
		}
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_SOFTERROR,
						reject_string,
						error_blob);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						 KRB5_KPASSWD_HARDERROR,
						 talloc_asprintf(mem_ctx, "failed to set password: %s", nt_errstr(status)),
						 error_blob);
		
	}
	return kpasswdd_make_error_reply(kdc, mem_ctx, KRB5_KPASSWD_SUCCESS,
					"Password changed",
					error_blob);
}

/* 
   A user password change
   
   Return true if there is a valid error packet (or sucess) formed in
   the error_blob
*/
static bool kpasswdd_change_password(struct kdc_server *kdc,
				     TALLOC_CTX *mem_ctx, 
				     struct auth_session_info *session_info,
				     const DATA_BLOB *password,
				     DATA_BLOB *reply)
{
	NTSTATUS status;
	enum samr_RejectReason reject_reason;
	struct samr_DomInfo1 *dominfo;
	struct ldb_context *samdb;

	samdb = samdb_connect(mem_ctx, kdc->task->event_ctx, kdc->task->lp_ctx, system_session(mem_ctx, kdc->task->lp_ctx));
	if (!samdb) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_HARDERROR,
						"Failed to open samdb",
						reply);
	}
	
	DEBUG(3, ("Changing password of %s\\%s (%s)\n", 
		  session_info->server_info->domain_name,
		  session_info->server_info->account_name,
		  dom_sid_string(mem_ctx, session_info->security_token->user_sid)));

	/* User password change */
	status = samdb_set_password_sid(samdb, mem_ctx, 
					session_info->security_token->user_sid,
					password, NULL, NULL, 
					true, /* this is a user password change */
					&reject_reason,
					&dominfo);
	return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
					   status, 
					   reject_reason,
					   dominfo, 
					   reply);

}

static bool kpasswd_process_request(struct kdc_server *kdc,
				    TALLOC_CTX *mem_ctx, 
				    struct gensec_security *gensec_security,
				    uint16_t version,
				    DATA_BLOB *input, 
				    DATA_BLOB *reply)
{
	struct auth_session_info *session_info;
	size_t pw_len;

	if (!NT_STATUS_IS_OK(gensec_session_info(gensec_security, 
						 &session_info))) {
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						KRB5_KPASSWD_HARDERROR,
						"gensec_session_info failed!",
						reply);
	}

	switch (version) {
	case KRB5_KPASSWD_VERS_CHANGEPW:
	{
		DATA_BLOB password;
		if (!convert_string_talloc_convenience(mem_ctx, lp_iconv_convenience(kdc->task->lp_ctx), 
					       CH_UTF8, CH_UTF16, 
					       (const char *)input->data, 
					       input->length,
					       (void **)&password.data, &pw_len, false)) {
			return false;
		}
		password.length = pw_len;
	
		return kpasswdd_change_password(kdc, mem_ctx, session_info, 
						&password, reply);
		break;
	}
	case KRB5_KPASSWD_VERS_SETPW:
	{
		NTSTATUS status;
		enum samr_RejectReason reject_reason = SAMR_REJECT_OTHER;
		struct samr_DomInfo1 *dominfo = NULL;
		struct ldb_context *samdb;
		struct ldb_message *msg;
		krb5_context context = kdc->smb_krb5_context->krb5_context;

		ChangePasswdDataMS chpw;
		DATA_BLOB password;

		krb5_principal principal;
		char *set_password_on_princ;
		struct ldb_dn *set_password_on_dn;

		size_t len;
		int ret;

		msg = ldb_msg_new(mem_ctx);
		if (!msg) {
			return false;
		}

		ret = decode_ChangePasswdDataMS(input->data, input->length,
						&chpw, &len);
		if (ret) {
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							KRB5_KPASSWD_MALFORMED,
							"failed to decode password change structure",
							reply);
		}
		
		if (!convert_string_talloc_convenience(mem_ctx, lp_iconv_convenience(kdc->task->lp_ctx), 
					       CH_UTF8, CH_UTF16, 
					       (const char *)chpw.newpasswd.data, 
					       chpw.newpasswd.length,
					       (void **)&password.data, &pw_len, false)) {
			free_ChangePasswdDataMS(&chpw);
			return false;
		}
		
		password.length = pw_len;
	
		if ((chpw.targname && !chpw.targrealm) 
		    || (!chpw.targname && chpw.targrealm)) {
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							KRB5_KPASSWD_MALFORMED,
							"Realm and principal must be both present, or neither present",
							reply);
		}
		if (chpw.targname && chpw.targrealm) {
#ifdef SAMBA4_INTERNAL_HEIMDAL
			if (_krb5_principalname2krb5_principal(kdc->smb_krb5_context->krb5_context,
							       &principal, *chpw.targname, 
							       *chpw.targrealm) != 0) {
				free_ChangePasswdDataMS(&chpw);
				return kpasswdd_make_error_reply(kdc, mem_ctx, 
								KRB5_KPASSWD_MALFORMED,
								"failed to extract principal to set",
								reply);
				
			}
#else /* SAMBA4_INTERNAL_HEIMDAL */
				return kpasswdd_make_error_reply(kdc, mem_ctx,
								KRB5_KPASSWD_BAD_VERSION,
								"Operation Not Implemented",
								reply);
#endif /* SAMBA4_INTERNAL_HEIMDAL */
		} else {
			free_ChangePasswdDataMS(&chpw);
			return kpasswdd_change_password(kdc, mem_ctx, session_info, 
							&password, reply);
		}
		free_ChangePasswdDataMS(&chpw);

		if (krb5_unparse_name(context, principal, &set_password_on_princ) != 0) {
			krb5_free_principal(context, principal);
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							KRB5_KPASSWD_MALFORMED,
							"krb5_unparse_name failed!",
							reply);
		}
		
		krb5_free_principal(context, principal);
		
		samdb = samdb_connect(mem_ctx, kdc->task->event_ctx, kdc->task->lp_ctx, session_info);
		if (!samdb) {
			return kpasswdd_make_error_reply(kdc, mem_ctx, 
							 KRB5_KPASSWD_HARDERROR,
							 "Unable to open database!",
							 reply);
		}

		DEBUG(3, ("%s\\%s (%s) is changing password of %s\n", 
			  session_info->server_info->domain_name,
			  session_info->server_info->account_name,
			  dom_sid_string(mem_ctx, session_info->security_token->user_sid), 
			  set_password_on_princ));
		ret = ldb_transaction_start(samdb);
		if (ret) {
			status = NT_STATUS_TRANSACTION_ABORTED;
			return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
							   status,
							   SAMR_REJECT_OTHER, 
							   NULL, 
							   reply);
		}

		status = crack_user_principal_name(samdb, mem_ctx, 
						   set_password_on_princ, 
						   &set_password_on_dn, NULL);
		free(set_password_on_princ);
		if (!NT_STATUS_IS_OK(status)) {
			ldb_transaction_cancel(samdb);
			return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
							   status,
							   SAMR_REJECT_OTHER, 
							   NULL, 
							   reply);
		}

		msg = ldb_msg_new(mem_ctx);
		if (msg == NULL) {
			ldb_transaction_cancel(samdb);
			status = NT_STATUS_NO_MEMORY;
		} else {
			msg->dn = ldb_dn_copy(msg, set_password_on_dn);
			if (!msg->dn) {
				status = NT_STATUS_NO_MEMORY;
			}
		}

		if (NT_STATUS_IS_OK(status)) {
			/* Admin password set */
			status = samdb_set_password(samdb, mem_ctx,
						    set_password_on_dn, NULL,
						    msg, &password, NULL, NULL, 
						    false, /* this is not a user password change */
						    &reject_reason, &dominfo);
		}

		if (NT_STATUS_IS_OK(status)) {
			/* modify the samdb record */
			ret = samdb_replace(samdb, mem_ctx, msg);
			if (ret != 0) {
				DEBUG(2,("Failed to modify record to set password on %s: %s\n",
					 ldb_dn_get_linearized(msg->dn),
					 ldb_errstring(samdb)));
				status = NT_STATUS_ACCESS_DENIED;
			}
		}
		if (NT_STATUS_IS_OK(status)) {
			ret = ldb_transaction_commit(samdb);
			if (ret != 0) {
				DEBUG(1,("Failed to commit transaction to set password on %s: %s\n",
					 ldb_dn_get_linearized(msg->dn),
					 ldb_errstring(samdb)));
				status = NT_STATUS_TRANSACTION_ABORTED;
			}
		} else {
			ldb_transaction_cancel(samdb);
		}
		return kpasswd_make_pwchange_reply(kdc, mem_ctx, 
						   status,
						   reject_reason, 
						   dominfo, 
						   reply);
	}
	default:
		return kpasswdd_make_error_reply(kdc, mem_ctx, 
						 KRB5_KPASSWD_BAD_VERSION,
						 talloc_asprintf(mem_ctx, 
								 "Protocol version %u not supported", 
								 version),
						 reply);
	}
	return true;
}

bool kpasswdd_process(struct kdc_server *kdc,
		      TALLOC_CTX *mem_ctx, 
		      DATA_BLOB *input, 
		      DATA_BLOB *reply,
		      struct socket_address *peer_addr,
		      struct socket_address *my_addr,
		      int datagram_reply)
{
	bool ret;
	const uint16_t header_len = 6;
	uint16_t len;
	uint16_t ap_req_len;
	uint16_t krb_priv_len;
	uint16_t version;
	NTSTATUS nt_status;
	DATA_BLOB ap_req, krb_priv_req;
	DATA_BLOB krb_priv_rep = data_blob(NULL, 0);
	DATA_BLOB ap_rep = data_blob(NULL, 0);
	DATA_BLOB kpasswd_req, kpasswd_rep;
	struct cli_credentials *server_credentials;
	struct gensec_security *gensec_security;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	char *keytab_name;

	if (!tmp_ctx) {
		return false;
	}

	/* Be parinoid.  We need to ensure we don't just let the
	 * caller lead us into a buffer overflow */
	if (input->length <= header_len) {
		talloc_free(tmp_ctx);
		return false;
	}

	len = RSVAL(input->data, 0);
	if (input->length != len) {
		talloc_free(tmp_ctx);
		return false;
	}

	/* There are two different versions of this protocol so far,
	 * plus others in the standards pipe.  Fortunetly they all
	 * take a very similar framing */
	version = RSVAL(input->data, 2);
	ap_req_len = RSVAL(input->data, 4);
	if ((ap_req_len >= len) || (ap_req_len + header_len) >= len) {
		talloc_free(tmp_ctx);
		return false;
	}
	
	krb_priv_len = len - ap_req_len;
	ap_req = data_blob_const(&input->data[header_len], ap_req_len);
	krb_priv_req = data_blob_const(&input->data[header_len + ap_req_len], krb_priv_len);
	
	server_credentials = cli_credentials_init(tmp_ctx);
	if (!server_credentials) {
		DEBUG(1, ("Failed to init server credentials\n"));
		return false;
	}

	/* We want the credentials subsystem to use the krb5 context
	 * we already have, rather than a new context */	
	cli_credentials_set_krb5_context(server_credentials, kdc->smb_krb5_context);
	cli_credentials_set_conf(server_credentials, kdc->task->lp_ctx);

	keytab_name = talloc_asprintf(server_credentials, "HDB:samba4&%p", kdc->hdb_samba4_context);

	cli_credentials_set_username(server_credentials, "kadmin/changepw", CRED_SPECIFIED);
	ret = cli_credentials_set_keytab_name(server_credentials, kdc->task->event_ctx, kdc->task->lp_ctx, keytab_name, CRED_SPECIFIED);
	if (ret != 0) {
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "Failed to obtain server credentials for kadmin/changepw: %s\n", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}
	
	/* We don't strictly need to call this wrapper, and could call
	 * gensec_server_start directly, as we have no need for NTLM
	 * and we have a PAC, but this ensures that the wrapper can be
	 * safely extended for other helpful things in future */
	nt_status = samba_server_gensec_start(tmp_ctx, kdc->task->event_ctx, 
					      kdc->task->msg_ctx,
					      kdc->task->lp_ctx,
					      server_credentials,
					      "kpasswd", 
					      &gensec_security);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return false;
	}

	/* The kerberos PRIV packets include these addresses.  MIT
	 * clients check that they are present */
#if 0
	/* Skip this part for now, it breaks with a NetAPP filer and
	 * in any case where the client address is behind NAT.  If
	 * older MIT clients need this, we might have to insert more
	 * complex code */

	nt_status = gensec_set_peer_addr(gensec_security, peer_addr);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return false;
	}
#endif

	nt_status = gensec_set_my_addr(gensec_security, my_addr);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return false;
	}

	/* We want the GENSEC wrap calls to generate PRIV tokens */
	gensec_want_feature(gensec_security, GENSEC_FEATURE_SEAL);

	nt_status = gensec_start_mech_by_name(gensec_security, "krb5");
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return false;
	}

	/* Accept the AP-REQ and generate teh AP-REP we need for the reply */
	nt_status = gensec_update(gensec_security, tmp_ctx, ap_req, &ap_rep);
	if (!NT_STATUS_IS_OK(nt_status) && !NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "gensec_update failed: %s", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}

	/* Extract the data from the KRB-PRIV half of the message */
	nt_status = gensec_unwrap(gensec_security, tmp_ctx, &krb_priv_req, &kpasswd_req);
	if (!NT_STATUS_IS_OK(nt_status)) {
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "gensec_unwrap failed: %s", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}

	/* Figure out something to do with it (probably changing a password...) */
	ret = kpasswd_process_request(kdc, tmp_ctx, 
				      gensec_security, 
				      version, 
				      &kpasswd_req, &kpasswd_rep); 
	if (!ret) {
		/* Argh! */
		return false;
	}

	/* And wrap up the reply: This ensures that the error message
	 * or success can be verified by the client */
	nt_status = gensec_wrap(gensec_security, tmp_ctx, 
				&kpasswd_rep, &krb_priv_rep);
	if (!NT_STATUS_IS_OK(nt_status)) {
		ret = kpasswdd_make_unauth_error_reply(kdc, mem_ctx, 
						       KRB5_KPASSWD_HARDERROR,
						       talloc_asprintf(mem_ctx, 
								       "gensec_wrap failed: %s", 
								       nt_errstr(nt_status)),
						       &krb_priv_rep);
		ap_rep.length = 0;
		if (ret) {
			goto reply;
		}
		talloc_free(tmp_ctx);
		return ret;
	}
	
reply:
	*reply = data_blob_talloc(mem_ctx, NULL, krb_priv_rep.length + ap_rep.length + header_len);
	if (!reply->data) {
		return false;
	}

	RSSVAL(reply->data, 0, reply->length);
	RSSVAL(reply->data, 2, 1); /* This is a version 1 reply, MS change/set or otherwise */
	RSSVAL(reply->data, 4, ap_rep.length);
	memcpy(reply->data + header_len, 
	       ap_rep.data, 
	       ap_rep.length);
	memcpy(reply->data + header_len + ap_rep.length, 
	       krb_priv_rep.data, 
	       krb_priv_rep.length);

	talloc_free(tmp_ctx);
	return ret;
}

