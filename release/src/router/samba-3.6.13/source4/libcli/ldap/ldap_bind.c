/* 
   Unix SMB/CIFS mplementation.

   LDAP bind calls
   
   Copyright (C) Andrew Tridgell  2005
   Copyright (C) Volker Lendecke  2004
    
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
#include "libcli/ldap/libcli_ldap.h"
#include "libcli/ldap/ldap_proto.h"
#include "libcli/ldap/ldap_client.h"
#include "lib/tls/tls.h"
#include "auth/gensec/gensec.h"
#include "auth/credentials/credentials.h"
#include "lib/stream/packet.h"
#include "param/param.h"

struct ldap_simple_creds {
	const char *dn;
	const char *pw;
};

_PUBLIC_ NTSTATUS ldap_rebind(struct ldap_connection *conn)
{
	NTSTATUS status;
	struct ldap_simple_creds *creds;

	switch (conn->bind.type) {
	case LDAP_BIND_SASL:
		status = ldap_bind_sasl(conn, (struct cli_credentials *)conn->bind.creds,
					conn->lp_ctx);
		break;
		
	case LDAP_BIND_SIMPLE:
		creds = (struct ldap_simple_creds *)conn->bind.creds;

		if (creds == NULL) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		status = ldap_bind_simple(conn, creds->dn, creds->pw);
		break;

	default:
		return NT_STATUS_UNSUCCESSFUL;
	}

	return status;
}


static struct ldap_message *new_ldap_simple_bind_msg(struct ldap_connection *conn, 
						     const char *dn, const char *pw)
{
	struct ldap_message *res;

	res = new_ldap_message(conn);
	if (!res) {
		return NULL;
	}

	res->type = LDAP_TAG_BindRequest;
	res->r.BindRequest.version = 3;
	res->r.BindRequest.dn = talloc_strdup(res, dn);
	res->r.BindRequest.mechanism = LDAP_AUTH_MECH_SIMPLE;
	res->r.BindRequest.creds.password = talloc_strdup(res, pw);
	res->controls = NULL;

	return res;
}


/*
  perform a simple username/password bind
*/
_PUBLIC_ NTSTATUS ldap_bind_simple(struct ldap_connection *conn, 
			  const char *userdn, const char *password)
{
	struct ldap_request *req;
	struct ldap_message *msg;
	const char *dn, *pw;
	NTSTATUS status;

	if (conn == NULL) {
		return NT_STATUS_INVALID_CONNECTION;
	}

	if (userdn) {
		dn = userdn;
	} else {
		if (conn->auth_dn) {
			dn = conn->auth_dn;
		} else {
			dn = "";
		}
	}

	if (password) {
		pw = password;
	} else {
		if (conn->simple_pw) {
			pw = conn->simple_pw;
		} else {
			pw = "";
		}
	}

	msg = new_ldap_simple_bind_msg(conn, dn, pw);
	NT_STATUS_HAVE_NO_MEMORY(msg);

	/* send the request */
	req = ldap_request_send(conn, msg);
	talloc_free(msg);
	NT_STATUS_HAVE_NO_MEMORY(req);

	/* wait for replies */
	status = ldap_request_wait(req);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return status;
	}

	/* check its a valid reply */
	msg = req->replies[0];
	if (msg->type != LDAP_TAG_BindResponse) {
		talloc_free(req);
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	status = ldap_check_response(conn, &msg->r.BindResponse.response);

	talloc_free(req);

	if (NT_STATUS_IS_OK(status)) {
		struct ldap_simple_creds *creds = talloc(conn, struct ldap_simple_creds);
		if (creds == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		creds->dn = talloc_strdup(creds, dn);
		creds->pw = talloc_strdup(creds, pw);
		if (creds->dn == NULL || creds->pw == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		conn->bind.type = LDAP_BIND_SIMPLE;
		conn->bind.creds = creds;
	}

	return status;
}


static struct ldap_message *new_ldap_sasl_bind_msg(struct ldap_connection *conn, 
						   const char *sasl_mechanism, 
						   DATA_BLOB *secblob)
{
	struct ldap_message *res;

	res = new_ldap_message(conn);
	if (!res) {
		return NULL;
	}

	res->type = LDAP_TAG_BindRequest;
	res->r.BindRequest.version = 3;
	res->r.BindRequest.dn = "";
	res->r.BindRequest.mechanism = LDAP_AUTH_MECH_SASL;
	res->r.BindRequest.creds.SASL.mechanism = talloc_strdup(res, sasl_mechanism);
	if (secblob) {
		res->r.BindRequest.creds.SASL.secblob = talloc(res, DATA_BLOB);
		if (!res->r.BindRequest.creds.SASL.secblob) {
			talloc_free(res);
			return NULL;
		}
		*res->r.BindRequest.creds.SASL.secblob = *secblob;
	} else {
		res->r.BindRequest.creds.SASL.secblob = NULL;
	}
	res->controls = NULL;

	return res;
}


/*
  perform a sasl bind using the given credentials
*/
_PUBLIC_ NTSTATUS ldap_bind_sasl(struct ldap_connection *conn,
			struct cli_credentials *creds,
			struct loadparm_context *lp_ctx)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = NULL;

	DATA_BLOB input = data_blob(NULL, 0);
	DATA_BLOB output = data_blob(NULL, 0);

	struct ldap_message **sasl_mechs_msgs;
	struct ldap_SearchResEntry *search;
	int count, i;

	const char **sasl_names;
	uint32_t old_gensec_features;
	static const char *supported_sasl_mech_attrs[] = {
		"supportedSASLMechanisms", 
		NULL 
	};

	gensec_init(lp_ctx);

	status = gensec_client_start(conn, &conn->gensec,
				     conn->event.event_ctx, 
				     lpcfg_gensec_settings(conn, lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to start GENSEC engine (%s)\n", nt_errstr(status)));
		goto failed;
	}

	/* require Kerberos SIGN/SEAL only if we don't use SSL
	 * Windows seem not to like double encryption */
	old_gensec_features = cli_credentials_get_gensec_features(creds);
	if (tls_enabled(conn->sock)) {
		cli_credentials_set_gensec_features(creds, old_gensec_features & ~(GENSEC_FEATURE_SIGN|GENSEC_FEATURE_SEAL));
	}

	/* this call also sets the gensec_want_features */
	status = gensec_set_credentials(conn->gensec, creds);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to set GENSEC creds: %s\n", 
			  nt_errstr(status)));
		goto failed;
	}

	/* reset the original gensec_features (on the credentials
	 * context, so we don't tatoo it ) */
	cli_credentials_set_gensec_features(creds, old_gensec_features);

	if (conn->host) {
		status = gensec_set_target_hostname(conn->gensec, conn->host);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to set GENSEC target hostname: %s\n", 
				  nt_errstr(status)));
			goto failed;
		}
	}

	status = gensec_set_target_service(conn->gensec, "ldap");
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to set GENSEC target service: %s\n", 
			  nt_errstr(status)));
		goto failed;
	}

	status = ildap_search(conn, "", LDAP_SEARCH_SCOPE_BASE, "", supported_sasl_mech_attrs, 
			      false, NULL, NULL, &sasl_mechs_msgs);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Failed to inquire of target's available sasl mechs in rootdse search: %s\n", 
			  nt_errstr(status)));
		goto failed;
	}
	
	count = ildap_count_entries(conn, sasl_mechs_msgs);
	if (count != 1) {
		DEBUG(1, ("Failed to inquire of target's available sasl mechs in rootdse search: wrong number of replies: %d\n",
			  count));
		goto failed;
	}

	tmp_ctx = talloc_new(conn);
	if (tmp_ctx == NULL) goto failed;

	search = &sasl_mechs_msgs[0]->r.SearchResultEntry;
	if (search->num_attributes != 1) {
		DEBUG(1, ("Failed to inquire of target's available sasl mechs in rootdse search: wrong number of attributes: %d != 1\n",
			  search->num_attributes));
		goto failed;
	}

	sasl_names = talloc_array(tmp_ctx, const char *, search->attributes[0].num_values + 1);
	if (!sasl_names) {
		DEBUG(1, ("talloc_arry(char *, %d) failed\n",
			  count));
		goto failed;
	}
		
	for (i=0; i<search->attributes[0].num_values; i++) {
		sasl_names[i] = (const char *)search->attributes[0].values[i].data;
	}
	sasl_names[i] = NULL;
	
	status = gensec_start_mech_by_sasl_list(conn->gensec, sasl_names);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("None of the %d proposed SASL mechs were acceptable: %s\n",
			  count, nt_errstr(status)));
		goto failed;
	}

	while (1) {
		NTSTATUS gensec_status;
		struct ldap_message *response;
		struct ldap_message *msg;
		struct ldap_request *req;
		int result = LDAP_OTHER;
	
		status = gensec_update(conn->gensec, tmp_ctx,
				       input,
				       &output);
		/* The status value here, from GENSEC is vital to the security
		 * of the system.  Even if the other end accepts, if GENSEC
		 * claims 'MORE_PROCESSING_REQUIRED' then you must keep
		 * feeding it blobs, or else the remote host/attacker might
		 * avoid mutal authentication requirements.
		 *
		 * Likewise, you must not feed GENSEC too much (after the OK),
		 * it doesn't like that either
		 */

		gensec_status = status;

		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED) && 
		    !NT_STATUS_IS_OK(status)) {
			break;
		}
		if (NT_STATUS_IS_OK(status) && output.length == 0) {
			break;
		}

		/* Perhaps we should make gensec_start_mech_by_sasl_list() return the name we got? */
		msg = new_ldap_sasl_bind_msg(tmp_ctx, conn->gensec->ops->sasl_name, (output.data?&output:NULL));
		if (msg == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}

		req = ldap_request_send(conn, msg);
		if (req == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto failed;
		}
		talloc_reparent(conn, tmp_ctx, req);

		status = ldap_result_n(req, 0, &response);
		if (!NT_STATUS_IS_OK(status)) {
			goto failed;
		}
		
		if (response->type != LDAP_TAG_BindResponse) {
			status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
			goto failed;
		}

		result = response->r.BindResponse.response.resultcode;

		if (result != LDAP_SUCCESS && result != LDAP_SASL_BIND_IN_PROGRESS) {
			status = ldap_check_response(conn, 
						     &response->r.BindResponse.response);
			break;
		}

		/* This is where we check if GENSEC wanted to be fed more data */
		if (!NT_STATUS_EQUAL(gensec_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			break;
		}
		if (response->r.BindResponse.SASL.secblob) {
			input = *response->r.BindResponse.SASL.secblob;
		} else {
			input = data_blob(NULL, 0);
		}
	}

	talloc_free(tmp_ctx);

	if (NT_STATUS_IS_OK(status)) {
		struct socket_context *sasl_socket;
		status = gensec_socket_init(conn->gensec, 
					    conn,
					    conn->sock,
					    conn->event.event_ctx, 
					    ldap_read_io_handler,
					    conn,
					    &sasl_socket);
		if (!NT_STATUS_IS_OK(status)) goto failed;

		conn->sock = sasl_socket;
		packet_set_socket(conn->packet, conn->sock);

		conn->bind.type = LDAP_BIND_SASL;
		conn->bind.creds = creds;
	}

	return status;

failed:
	talloc_free(tmp_ctx);
	talloc_free(conn->gensec);
	conn->gensec = NULL;
	return status;
}
