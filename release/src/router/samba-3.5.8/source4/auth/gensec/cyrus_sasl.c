/* 
   Unix SMB/CIFS implementation.
 
   Connect GENSEC to an external SASL lib

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   
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
#include "auth/auth.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "lib/socket/socket.h"
#include <sasl/sasl.h>

struct gensec_sasl_state {
	sasl_conn_t *conn;
	int step;
};

static NTSTATUS sasl_nt_status(int sasl_ret) 
{
	switch (sasl_ret) {
	case SASL_CONTINUE:
		return NT_STATUS_MORE_PROCESSING_REQUIRED;
	case SASL_NOMEM:
		return NT_STATUS_NO_MEMORY;
	case SASL_BADPARAM:
	case SASL_NOMECH:
		return NT_STATUS_INVALID_PARAMETER;
	case SASL_BADMAC:
		return NT_STATUS_ACCESS_DENIED;
	case SASL_OK:
		return NT_STATUS_OK;
	default:
		return NT_STATUS_UNSUCCESSFUL;
	}
}

static int gensec_sasl_get_user(void *context, int id,
				const char **result, unsigned *len)
{
	struct gensec_security *gensec_security = talloc_get_type(context, struct gensec_security);
	const char *username = cli_credentials_get_username(gensec_get_credentials(gensec_security));
	if (id != SASL_CB_USER && id != SASL_CB_AUTHNAME) {
		return SASL_FAIL;
	}
	
	*result = username;
	return SASL_OK;
}

static int gensec_sasl_get_realm(void *context, int id,
				 const char **availrealms,
				 const char **result)
{
	struct gensec_security *gensec_security = talloc_get_type(context, struct gensec_security);
	const char *realm = cli_credentials_get_realm(gensec_get_credentials(gensec_security));
	int i;
	if (id != SASL_CB_GETREALM) {
		return SASL_FAIL;
	}

	for (i=0; availrealms && availrealms[i]; i++) {
		if (strcasecmp_m(realm, availrealms[i]) == 0) {
			result[i] = availrealms[i];
			return SASL_OK;
		}
	}
	/* None of the realms match, so lets not specify one */
	*result = "";
	return SASL_OK;
}

static int gensec_sasl_get_password(sasl_conn_t *conn, void *context, int id,
			     sasl_secret_t **psecret)
{
	struct gensec_security *gensec_security = talloc_get_type(context, struct gensec_security);
	const char *password = cli_credentials_get_password(gensec_get_credentials(gensec_security));
	
	sasl_secret_t *secret;
	if (!password) {
		*psecret = NULL;
		return SASL_OK;
	}
	secret = talloc_size(gensec_security, sizeof(sasl_secret_t)+strlen(password));
	if (!secret) {
		return SASL_NOMEM;
	}
	secret->len = strlen(password);
	safe_strcpy((char*)secret->data, password, secret->len+1);
	*psecret = secret;
	return SASL_OK;
}

static int gensec_sasl_dispose(struct gensec_sasl_state *gensec_sasl_state)
{
	sasl_dispose(&gensec_sasl_state->conn);
	return SASL_OK;
}

static NTSTATUS gensec_sasl_client_start(struct gensec_security *gensec_security)
{
	struct gensec_sasl_state *gensec_sasl_state;
	const char *service = gensec_get_target_service(gensec_security);
	const char *target_name = gensec_get_target_hostname(gensec_security);
	struct socket_address *local_socket_addr = gensec_get_my_addr(gensec_security);
	struct socket_address *remote_socket_addr = gensec_get_peer_addr(gensec_security);
	char *local_addr = NULL;
	char *remote_addr = NULL;
	int sasl_ret;

	sasl_callback_t *callbacks;

	gensec_sasl_state = talloc(gensec_security, struct gensec_sasl_state);
	if (!gensec_sasl_state) {
		return NT_STATUS_NO_MEMORY;
	}

	callbacks = talloc_array(gensec_sasl_state, sasl_callback_t, 5);
	callbacks[0].id = SASL_CB_USER;
	callbacks[0].proc = gensec_sasl_get_user;
	callbacks[0].context = gensec_security;

	callbacks[1].id =  SASL_CB_AUTHNAME;
	callbacks[1].proc = gensec_sasl_get_user;
	callbacks[1].context = gensec_security;

	callbacks[2].id = SASL_CB_GETREALM;
	callbacks[2].proc = gensec_sasl_get_realm;
	callbacks[2].context = gensec_security;

	callbacks[3].id = SASL_CB_PASS;
	callbacks[3].proc = gensec_sasl_get_password;
	callbacks[3].context = gensec_security;

	callbacks[4].id = SASL_CB_LIST_END;
	callbacks[4].proc = NULL;
	callbacks[4].context = NULL;

	gensec_security->private_data = gensec_sasl_state;

	if (local_socket_addr) {
		local_addr = talloc_asprintf(gensec_sasl_state, 
					     "%s;%d",
					     local_socket_addr->addr, 
					     local_socket_addr->port);
	}

	if (remote_socket_addr) {
		remote_addr = talloc_asprintf(gensec_sasl_state, 
					     "%s;%d",
					     remote_socket_addr->addr, 
					     remote_socket_addr->port);
	}
	gensec_sasl_state->step = 0;

	sasl_ret = sasl_client_new(service,
				   target_name,
				   local_addr, remote_addr, callbacks, 0,
				   &gensec_sasl_state->conn);
	
	if (sasl_ret == SASL_OK || sasl_ret == SASL_CONTINUE) {
		sasl_security_properties_t props;
		talloc_set_destructor(gensec_sasl_state, gensec_sasl_dispose);

		ZERO_STRUCT(props);
		if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
			props.min_ssf = 1;
		}
		if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
			props.min_ssf = 40;
		}
		
		props.max_ssf = UINT_MAX;
		props.maxbufsize = 65536;
		sasl_ret = sasl_setprop(gensec_sasl_state->conn, SASL_SEC_PROPS, &props);
		if (sasl_ret != SASL_OK) {
			return sasl_nt_status(sasl_ret);
		}

	} else {
		DEBUG(1, ("GENSEC SASL: client_new failed: %s\n", sasl_errdetail(gensec_sasl_state->conn)));
	}
	return sasl_nt_status(sasl_ret);
}

static NTSTATUS gensec_sasl_update(struct gensec_security *gensec_security, 
				   TALLOC_CTX *out_mem_ctx, 
				   const DATA_BLOB in, DATA_BLOB *out) 
{
	struct gensec_sasl_state *gensec_sasl_state = talloc_get_type(gensec_security->private_data,
								      struct gensec_sasl_state);
	int sasl_ret;
	const char *out_data;
	unsigned int out_len;

	if (gensec_sasl_state->step == 0) {
		const char *mech;
		sasl_ret = sasl_client_start(gensec_sasl_state->conn, gensec_security->ops->sasl_name, 
					     NULL, &out_data, &out_len, &mech);
	} else {
		sasl_ret = sasl_client_step(gensec_sasl_state->conn,
					    (char*)in.data, in.length, NULL,
					    &out_data, &out_len);
	}
	if (sasl_ret == SASL_OK || sasl_ret == SASL_CONTINUE) {
		*out = data_blob_talloc(out_mem_ctx, out_data, out_len);
	} else {
		DEBUG(1, ("GENSEC SASL: step %d update failed: %s\n", gensec_sasl_state->step, 
			  sasl_errdetail(gensec_sasl_state->conn)));
	}
	gensec_sasl_state->step++;
	return sasl_nt_status(sasl_ret);
}

static NTSTATUS gensec_sasl_unwrap_packets(struct gensec_security *gensec_security, 
					TALLOC_CTX *out_mem_ctx, 
					const DATA_BLOB *in, 
					DATA_BLOB *out,
					size_t *len_processed) 
{
	struct gensec_sasl_state *gensec_sasl_state = talloc_get_type(gensec_security->private_data,
								      struct gensec_sasl_state);
	const char *out_data;
	unsigned int out_len;

	int sasl_ret = sasl_decode(gensec_sasl_state->conn,
				   (char*)in->data, in->length, &out_data,
				   &out_len);
	if (sasl_ret == SASL_OK) {
		*out = data_blob_talloc(out_mem_ctx, out_data, out_len);
		*len_processed = in->length;
	} else {
		DEBUG(1, ("GENSEC SASL: unwrap failed: %s\n", sasl_errdetail(gensec_sasl_state->conn)));
	}
	return sasl_nt_status(sasl_ret);

}

static NTSTATUS gensec_sasl_wrap_packets(struct gensec_security *gensec_security, 
					TALLOC_CTX *out_mem_ctx, 
					const DATA_BLOB *in, 
					DATA_BLOB *out,
					size_t *len_processed) 
{
	struct gensec_sasl_state *gensec_sasl_state = talloc_get_type(gensec_security->private_data,
								      struct gensec_sasl_state);
	const char *out_data;
	unsigned int out_len;

	int sasl_ret = sasl_encode(gensec_sasl_state->conn,
				   (char*)in->data, in->length, &out_data,
				   &out_len);
	if (sasl_ret == SASL_OK) {
		*out = data_blob_talloc(out_mem_ctx, out_data, out_len);
		*len_processed = in->length;
	} else {
		DEBUG(1, ("GENSEC SASL: wrap failed: %s\n", sasl_errdetail(gensec_sasl_state->conn)));
	}
	return sasl_nt_status(sasl_ret);
}

/* Try to figure out what features we actually got on the connection */
static bool gensec_sasl_have_feature(struct gensec_security *gensec_security, 
				     uint32_t feature) 
{
	struct gensec_sasl_state *gensec_sasl_state = talloc_get_type(gensec_security->private_data,
								      struct gensec_sasl_state);
	sasl_ssf_t ssf;
	int sasl_ret = sasl_getprop(gensec_sasl_state->conn, SASL_SSF,
			(const void**)&ssf);
	if (sasl_ret != SASL_OK) {
		return false;
	}
	if (feature & GENSEC_FEATURE_SIGN) {
		if (ssf == 0) {
			return false;
		}
		if (ssf >= 1) {
			return true;
		}
	}
	if (feature & GENSEC_FEATURE_SEAL) {
		if (ssf <= 1) {
			return false;
		}
		if (ssf > 1) {
			return true;
		}
	}
	return false;
}

/* This could in theory work with any SASL mech */
static const struct gensec_security_ops gensec_sasl_security_ops = {
	.name             = "sasl-DIGEST-MD5",
	.sasl_name        = "DIGEST-MD5",
	.client_start     = gensec_sasl_client_start,
	.update 	  = gensec_sasl_update,
	.wrap_packets     = gensec_sasl_wrap_packets,
	.unwrap_packets   = gensec_sasl_unwrap_packets,
	.have_feature     = gensec_sasl_have_feature,
	.enabled          = true,
	.priority         = GENSEC_SASL
};

static int gensec_sasl_log(void *context, 
		    int sasl_log_level,
		    const char *message) 
{
	int dl;
	switch (sasl_log_level) {
	case SASL_LOG_NONE:
		dl = 0;
		break;
	case SASL_LOG_ERR:
		dl = 1;
		break;
	case SASL_LOG_FAIL:
		dl = 2;
		break;
	case SASL_LOG_WARN:
		dl = 3;
		break;
	case SASL_LOG_NOTE:
		dl = 5;
		break;
	case SASL_LOG_DEBUG:
		dl = 10;
		break;
	case SASL_LOG_TRACE:
		dl = 11;
		break;
#if DEBUG_PASSWORD
	case SASL_LOG_PASS:
		dl = 100;
		break;
#endif
	default:
		dl = 0;
		break;
	}
	DEBUG(dl, ("gensec_sasl: %s\n", message));

	return SASL_OK;
}

NTSTATUS gensec_sasl_init(void)
{
	NTSTATUS ret;
	int sasl_ret;
#if 0
	int i;
	const char **sasl_mechs;
#endif
	
	static const sasl_callback_t callbacks[] = {
		{ 
			.id = SASL_CB_LOG,
			.proc = gensec_sasl_log,
			.context = NULL,
		},
		{
			.id = SASL_CB_LIST_END,
			.proc = gensec_sasl_log,
			.context = NULL,
		}
	};
	sasl_ret = sasl_client_init(callbacks);
	
	if (sasl_ret == SASL_NOMECH) {
		/* Nothing to do here */
		return NT_STATUS_OK;
	}

	if (sasl_ret != SASL_OK) {
		return sasl_nt_status(sasl_ret);
	}

	/* For now, we just register DIGEST-MD5 */
#if 1
	ret = gensec_register(&gensec_sasl_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			 gensec_sasl_security_ops.name));
		return ret;
	}
#else
	sasl_mechs = sasl_global_listmech();
	for (i = 0; sasl_mechs && sasl_mechs[i]; i++) {
		const struct gensec_security_ops *oldmech;
		struct gensec_security_ops *newmech;
		oldmech = gensec_security_by_sasl_name(NULL, sasl_mechs[i]);
		if (oldmech) {
			continue;
		}
		newmech = talloc(talloc_autofree_context(), struct gensec_security_ops);
		if (!newmech) {
			return NT_STATUS_NO_MEMORY;
		}
		*newmech = gensec_sasl_security_ops;
		newmech->sasl_name = talloc_strdup(newmech, sasl_mechs[i]);
		newmech->name = talloc_asprintf(newmech, "sasl-%s", sasl_mechs[i]);
		if (!newmech->sasl_name || !newmech->name) {
			return NT_STATUS_NO_MEMORY;
		}

		ret = gensec_register(newmech);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(0,("Failed to register '%s' gensec backend!\n",
				 gensec_sasl_security_ops.name));
			return ret;
		}
	}
#endif
	return NT_STATUS_OK;
}
