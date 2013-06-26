/* 
   Unix SMB/CIFS implementation.
 
   Generic Authentication Interface

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2006
   
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
#include "system/network.h"
#include "lib/events/events.h"
#include "lib/socket/socket.h"
#include "lib/tsocket/tsocket.h"
#include "../lib/util/tevent_ntstatus.h"
#include "librpc/rpc/dcerpc.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "auth/system_session_proto.h"
#include "param/param.h"
#include "lib/util/tsort.h"

/* the list of currently registered GENSEC backends */
static struct gensec_security_ops **generic_security_ops;
static int gensec_num_backends;

/* Return all the registered mechs.  Don't modify the return pointer,
 * but you may talloc_reference it if convient */
_PUBLIC_ struct gensec_security_ops **gensec_security_all(void)
{
	return generic_security_ops;
}

bool gensec_security_ops_enabled(struct gensec_security_ops *ops, struct gensec_security *security)
{
	return lpcfg_parm_bool(security->settings->lp_ctx, NULL, "gensec", ops->name, ops->enabled);
}

/* Sometimes we want to force only kerberos, sometimes we want to
 * force it's avoidance.  The old list could be either
 * gensec_security_all(), or from cli_credentials_gensec_list() (ie,
 * an existing list we have trimmed down) */

_PUBLIC_ struct gensec_security_ops **gensec_use_kerberos_mechs(TALLOC_CTX *mem_ctx, 
						       struct gensec_security_ops **old_gensec_list, 
						       struct cli_credentials *creds)
{
	struct gensec_security_ops **new_gensec_list;
	int i, j, num_mechs_in;
	enum credentials_use_kerberos use_kerberos = CRED_AUTO_USE_KERBEROS;

	if (creds) {
		use_kerberos = cli_credentials_get_kerberos_state(creds);
	}

	if (use_kerberos == CRED_AUTO_USE_KERBEROS) {
		if (!talloc_reference(mem_ctx, old_gensec_list)) {
			return NULL;
		}
		return old_gensec_list;
	}

	for (num_mechs_in=0; old_gensec_list && old_gensec_list[num_mechs_in]; num_mechs_in++) {
		/* noop */
	}

	new_gensec_list = talloc_array(mem_ctx, struct gensec_security_ops *, num_mechs_in + 1);
	if (!new_gensec_list) {
		return NULL;
	}

	j = 0;
	for (i=0; old_gensec_list && old_gensec_list[i]; i++) {
		int oid_idx;

		for (oid_idx = 0; old_gensec_list[i]->oid && old_gensec_list[i]->oid[oid_idx]; oid_idx++) {
			if (strcmp(old_gensec_list[i]->oid[oid_idx], GENSEC_OID_SPNEGO) == 0) {
				new_gensec_list[j] = old_gensec_list[i];
				j++;
				break;
			}
		}
		switch (use_kerberos) {
		case CRED_DONT_USE_KERBEROS:
			if (old_gensec_list[i]->kerberos == false) {
				new_gensec_list[j] = old_gensec_list[i];
				j++;
			}
			break;
		case CRED_MUST_USE_KERBEROS:
			if (old_gensec_list[i]->kerberos == true) {
				new_gensec_list[j] = old_gensec_list[i];
				j++;
			}
			break;
		default:
			/* Can't happen or invalid parameter */
			return NULL;
		}
	}
	new_gensec_list[j] = NULL; 
	
	return new_gensec_list;
}

struct gensec_security_ops **gensec_security_mechs(struct gensec_security *gensec_security,
						   TALLOC_CTX *mem_ctx) 
{
	struct gensec_security_ops **backends;
	backends = gensec_security_all();
	if (!gensec_security) {
		if (!talloc_reference(mem_ctx, backends)) {
			return NULL;
		}
		return backends;
	} else {
		struct cli_credentials *creds = gensec_get_credentials(gensec_security);
		if (!creds) {
			if (!talloc_reference(mem_ctx, backends)) {
				return NULL;
			}
			return backends;
		}
		return gensec_use_kerberos_mechs(mem_ctx, backends, creds);
	}
}

static const struct gensec_security_ops *gensec_security_by_authtype(struct gensec_security *gensec_security,
								     uint8_t auth_type)
{
	int i;
	struct gensec_security_ops **backends;
	const struct gensec_security_ops *backend;
	TALLOC_CTX *mem_ctx = talloc_new(gensec_security);
	if (!mem_ctx) {
		return NULL;
	}
	backends = gensec_security_mechs(gensec_security, mem_ctx);
	for (i=0; backends && backends[i]; i++) {
	    	if (!gensec_security_ops_enabled(backends[i], gensec_security))
				continue;
		if (backends[i]->auth_type == auth_type) {
			backend = backends[i];
			talloc_free(mem_ctx);
			return backend;
		}
	}
	talloc_free(mem_ctx);

	return NULL;
}

const struct gensec_security_ops *gensec_security_by_oid(struct gensec_security *gensec_security,
							 const char *oid_string)
{
	int i, j;
	struct gensec_security_ops **backends;
	const struct gensec_security_ops *backend;
	TALLOC_CTX *mem_ctx = talloc_new(gensec_security);
	if (!mem_ctx) {
		return NULL;
	}
	backends = gensec_security_mechs(gensec_security, mem_ctx);
	for (i=0; backends && backends[i]; i++) {
	    	if (gensec_security != NULL && 
				!gensec_security_ops_enabled(backends[i], 
											 gensec_security))
		    continue;
		if (backends[i]->oid) {
			for (j=0; backends[i]->oid[j]; j++) { 
				if (backends[i]->oid[j] &&
				    (strcmp(backends[i]->oid[j], oid_string) == 0)) {
					backend = backends[i];
					talloc_free(mem_ctx);
					return backend;
				}
			}
		}
	}
	talloc_free(mem_ctx);

	return NULL;
}

const struct gensec_security_ops *gensec_security_by_sasl_name(struct gensec_security *gensec_security,
							       const char *sasl_name)
{
	int i;
	struct gensec_security_ops **backends;
	const struct gensec_security_ops *backend;
	TALLOC_CTX *mem_ctx = talloc_new(gensec_security);
	if (!mem_ctx) {
		return NULL;
	}
	backends = gensec_security_mechs(gensec_security, mem_ctx);
	for (i=0; backends && backends[i]; i++) {
	    	if (!gensec_security_ops_enabled(backends[i], gensec_security))
		    continue;
		if (backends[i]->sasl_name 
		    && (strcmp(backends[i]->sasl_name, sasl_name) == 0)) {
			backend = backends[i];
			talloc_free(mem_ctx);
			return backend;
		}
	}
	talloc_free(mem_ctx);

	return NULL;
}

static const struct gensec_security_ops *gensec_security_by_name(struct gensec_security *gensec_security,
								 const char *name)
{
	int i;
	struct gensec_security_ops **backends;
	const struct gensec_security_ops *backend;
	TALLOC_CTX *mem_ctx = talloc_new(gensec_security);
	if (!mem_ctx) {
		return NULL;
	}
	backends = gensec_security_mechs(gensec_security, mem_ctx);
	for (i=0; backends && backends[i]; i++) {
	    	if (gensec_security != NULL && 
				!gensec_security_ops_enabled(backends[i], gensec_security))
		    continue;
		if (backends[i]->name 
		    && (strcmp(backends[i]->name, name) == 0)) {
			backend = backends[i];
			talloc_free(mem_ctx);
			return backend;
		}
	}
	talloc_free(mem_ctx);
	return NULL;
}

/**
 * Return a unique list of security subsystems from those specified in
 * the list of SASL names.   
 *
 * Use the list of enabled GENSEC mechanisms from the credentials
 * attached to the gensec_security, and return in our preferred order.
 */

const struct gensec_security_ops **gensec_security_by_sasl_list(struct gensec_security *gensec_security,
								TALLOC_CTX *mem_ctx, 
								const char **sasl_names)
{
	const struct gensec_security_ops **backends_out;
	struct gensec_security_ops **backends;
	int i, k, sasl_idx;
	int num_backends_out = 0;

	if (!sasl_names) {
		return NULL;
	}

	backends = gensec_security_mechs(gensec_security, mem_ctx);

	backends_out = talloc_array(mem_ctx, const struct gensec_security_ops *, 1);
	if (!backends_out) {
		return NULL;
	}
	backends_out[0] = NULL;

	/* Find backends in our preferred order, by walking our list,
	 * then looking in the supplied list */
	for (i=0; backends && backends[i]; i++) {
	    	if (gensec_security != NULL &&
				!gensec_security_ops_enabled(backends[i], gensec_security))
		    continue;
		for (sasl_idx = 0; sasl_names[sasl_idx]; sasl_idx++) {
			if (!backends[i]->sasl_name ||
			    !(strcmp(backends[i]->sasl_name, 
				     sasl_names[sasl_idx]) == 0)) {
				continue;
			}
			
			for (k=0; backends_out[k]; k++) {
				if (backends_out[k] == backends[i]) {
					break;
				}
			}
			
			if (k < num_backends_out) {
				/* already in there */
				continue;
			}
			
			backends_out = talloc_realloc(mem_ctx, backends_out, 
						      const struct gensec_security_ops *, 
						      num_backends_out + 2);
			if (!backends_out) {
				return NULL;
			}
			
			backends_out[num_backends_out] = backends[i];
			num_backends_out++;
			backends_out[num_backends_out] = NULL;
		}
	}
	return backends_out;
}

/**
 * Return a unique list of security subsystems from those specified in
 * the OID list.  That is, where two OIDs refer to the same module,
 * return that module only once. 
 *
 * Use the list of enabled GENSEC mechanisms from the credentials
 * attached to the gensec_security, and return in our preferred order.
 */

const struct gensec_security_ops_wrapper *gensec_security_by_oid_list(struct gensec_security *gensec_security,
								      TALLOC_CTX *mem_ctx, 
								      const char **oid_strings,
								      const char *skip)
{
	struct gensec_security_ops_wrapper *backends_out;
	struct gensec_security_ops **backends;
	int i, j, k, oid_idx;
	int num_backends_out = 0;

	if (!oid_strings) {
		return NULL;
	}

	backends = gensec_security_mechs(gensec_security, gensec_security);

	backends_out = talloc_array(mem_ctx, struct gensec_security_ops_wrapper, 1);
	if (!backends_out) {
		return NULL;
	}
	backends_out[0].op = NULL;
	backends_out[0].oid = NULL;

	/* Find backends in our preferred order, by walking our list,
	 * then looking in the supplied list */
	for (i=0; backends && backends[i]; i++) {
	    	if (gensec_security != NULL && 
				!gensec_security_ops_enabled(backends[i], gensec_security))
		    continue;
		if (!backends[i]->oid) {
			continue;
		}
		for (oid_idx = 0; oid_strings[oid_idx]; oid_idx++) {
			if (strcmp(oid_strings[oid_idx], skip) == 0) {
				continue;
			}

			for (j=0; backends[i]->oid[j]; j++) { 
				if (!backends[i]->oid[j] ||
				    !(strcmp(backends[i]->oid[j], 
					    oid_strings[oid_idx]) == 0)) {
					continue;
				}
				
				for (k=0; backends_out[k].op; k++) {
					if (backends_out[k].op == backends[i]) {
						break;
					}
				}
				
				if (k < num_backends_out) {
					/* already in there */
					continue;
				}

				backends_out = talloc_realloc(mem_ctx, backends_out, 
							      struct gensec_security_ops_wrapper, 
							      num_backends_out + 2);
				if (!backends_out) {
					return NULL;
				}
				
				backends_out[num_backends_out].op = backends[i];
				backends_out[num_backends_out].oid = backends[i]->oid[j];
				num_backends_out++;
				backends_out[num_backends_out].op = NULL;
				backends_out[num_backends_out].oid = NULL;
			}
		}
	}
	return backends_out;
}

/**
 * Return OIDS from the security subsystems listed
 */

const char **gensec_security_oids_from_ops(struct gensec_security *gensec_security,
										   TALLOC_CTX *mem_ctx, 
					   struct gensec_security_ops **ops,				   
					   const char *skip) 
{
	int i;
	int j = 0;
	int k;
	const char **oid_list;
	if (!ops) {
		return NULL;
	}
	oid_list = talloc_array(mem_ctx, const char *, 1);
	if (!oid_list) {
		return NULL;
	}
	
	for (i=0; ops && ops[i]; i++) {
		if (gensec_security != NULL && 
			!gensec_security_ops_enabled(ops[i], gensec_security)) {
			continue;
		}
		if (!ops[i]->oid) {
			continue;
		}
		
		for (k = 0; ops[i]->oid[k]; k++) {
			if (skip && strcmp(skip, ops[i]->oid[k])==0) {
			} else {
				oid_list = talloc_realloc(mem_ctx, oid_list, const char *, j + 2);
				if (!oid_list) {
					return NULL;
				}
				oid_list[j] = ops[i]->oid[k];
				j++;
			}
		}
	}
	oid_list[j] = NULL;
	return oid_list;
}


/**
 * Return OIDS from the security subsystems listed
 */

const char **gensec_security_oids_from_ops_wrapped(TALLOC_CTX *mem_ctx, 
						   const struct gensec_security_ops_wrapper *wops)
{
	int i;
	int j = 0;
	int k;
	const char **oid_list;
	if (!wops) {
		return NULL;
	}
	oid_list = talloc_array(mem_ctx, const char *, 1);
	if (!oid_list) {
		return NULL;
	}
	
	for (i=0; wops[i].op; i++) {
		if (!wops[i].op->oid) {
			continue;
		}
		
		for (k = 0; wops[i].op->oid[k]; k++) {
			oid_list = talloc_realloc(mem_ctx, oid_list, const char *, j + 2);
			if (!oid_list) {
				return NULL;
			}
			oid_list[j] = wops[i].op->oid[k];
			j++;
		}
	}
	oid_list[j] = NULL;
	return oid_list;
}


/**
 * Return all the security subsystems currently enabled on a GENSEC context.
 * 
 * This is taken from a list attached to the cli_credentials, and
 * skips the OID in 'skip'.  (Typically the SPNEGO OID)
 * 
 */

const char **gensec_security_oids(struct gensec_security *gensec_security, 
				  TALLOC_CTX *mem_ctx, 
				  const char *skip) 
{
	struct gensec_security_ops **ops
		= gensec_security_mechs(gensec_security, mem_ctx);
	return gensec_security_oids_from_ops(gensec_security, mem_ctx, ops, skip);
}



/**
  Start the GENSEC system, returning a context pointer.
  @param mem_ctx The parent TALLOC memory context.
  @param gensec_security Returned GENSEC context pointer.
  @note  The mem_ctx is only a parent and may be NULL.
  @note, the auth context is moved to be a referenced pointer of the
  @ gensec_security return 
*/
static NTSTATUS gensec_start(TALLOC_CTX *mem_ctx, 
			     struct tevent_context *ev,
			     struct gensec_settings *settings,
 			     struct auth_context *auth_context,
			     struct gensec_security **gensec_security)
{
	if (ev == NULL) {
		DEBUG(0, ("No event context available!\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	(*gensec_security) = talloc_zero(mem_ctx, struct gensec_security);
	NT_STATUS_HAVE_NO_MEMORY(*gensec_security);

	(*gensec_security)->event_ctx = ev;
	SMB_ASSERT(settings->lp_ctx != NULL);
	(*gensec_security)->settings = talloc_reference(*gensec_security, settings);

	/* We need to reference this, not steal, as the caller may be
	 * python, which won't like it if we steal it's object away
	 * from it */
	(*gensec_security)->auth_context = talloc_reference(*gensec_security, auth_context);

	return NT_STATUS_OK;
}

/** 
 * Start a GENSEC subcontext, with a copy of the properties of the parent
 * @param mem_ctx The parent TALLOC memory context.
 * @param parent The parent GENSEC context 
 * @param gensec_security Returned GENSEC context pointer.
 * @note Used by SPNEGO in particular, for the actual implementation mechanism
 */

_PUBLIC_ NTSTATUS gensec_subcontext_start(TALLOC_CTX *mem_ctx, 
				 struct gensec_security *parent, 
				 struct gensec_security **gensec_security)
{
	(*gensec_security) = talloc_zero(mem_ctx, struct gensec_security);
	NT_STATUS_HAVE_NO_MEMORY(*gensec_security);

	(**gensec_security) = *parent;
	(*gensec_security)->ops = NULL;
	(*gensec_security)->private_data = NULL;

	(*gensec_security)->subcontext = true;
	(*gensec_security)->want_features = parent->want_features;
	(*gensec_security)->event_ctx = parent->event_ctx;
	(*gensec_security)->auth_context = talloc_reference(*gensec_security, parent->auth_context);
	(*gensec_security)->settings = talloc_reference(*gensec_security, parent->settings);
	(*gensec_security)->auth_context = talloc_reference(*gensec_security, parent->auth_context);

	return NT_STATUS_OK;
}

/**
  Start the GENSEC system, in client mode, returning a context pointer.
  @param mem_ctx The parent TALLOC memory context.
  @param gensec_security Returned GENSEC context pointer.
  @note  The mem_ctx is only a parent and may be NULL.
*/
_PUBLIC_ NTSTATUS gensec_client_start(TALLOC_CTX *mem_ctx, 
			     struct gensec_security **gensec_security,
			     struct tevent_context *ev,
			     struct gensec_settings *settings)
{
	NTSTATUS status;

	if (settings == NULL) {
		DEBUG(0,("gensec_client_start: no settings given!\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = gensec_start(mem_ctx, ev, settings, NULL, gensec_security);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	(*gensec_security)->gensec_role = GENSEC_CLIENT;

	return status;
}



/**
  Start the GENSEC system, in server mode, returning a context pointer.
  @param mem_ctx The parent TALLOC memory context.
  @param gensec_security Returned GENSEC context pointer.
  @note  The mem_ctx is only a parent and may be NULL.
*/
_PUBLIC_ NTSTATUS gensec_server_start(TALLOC_CTX *mem_ctx, 
				      struct tevent_context *ev,
				      struct gensec_settings *settings,
				      struct auth_context *auth_context,
				      struct gensec_security **gensec_security)
{
	NTSTATUS status;

	if (!ev) {
		DEBUG(0,("gensec_server_start: no event context given!\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (!settings) {
		DEBUG(0,("gensec_server_start: no settings given!\n"));
		return NT_STATUS_INTERNAL_ERROR;
	}

	status = gensec_start(mem_ctx, ev, settings, auth_context, gensec_security);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	(*gensec_security)->gensec_role = GENSEC_SERVER;

	return status;
}

static NTSTATUS gensec_start_mech(struct gensec_security *gensec_security) 
{
	NTSTATUS status;
	DEBUG(5, ("Starting GENSEC %smechanism %s\n", 
		  gensec_security->subcontext ? "sub" : "", 
		  gensec_security->ops->name));
	switch (gensec_security->gensec_role) {
	case GENSEC_CLIENT:
		if (gensec_security->ops->client_start) {
			status = gensec_security->ops->client_start(gensec_security);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(2, ("Failed to start GENSEC client mech %s: %s\n",
					  gensec_security->ops->name, nt_errstr(status))); 
			}
			return status;
		}
		break;
	case GENSEC_SERVER:
		if (gensec_security->ops->server_start) {
			status = gensec_security->ops->server_start(gensec_security);
			if (!NT_STATUS_IS_OK(status)) {
				DEBUG(1, ("Failed to start GENSEC server mech %s: %s\n",
					  gensec_security->ops->name, nt_errstr(status))); 
			}
			return status;
		}
		break;
	}
	return NT_STATUS_INVALID_PARAMETER;
}

/** 
 * Start a GENSEC sub-mechanism by DCERPC allocated 'auth type' number 
 * @param gensec_security GENSEC context pointer.
 * @param auth_type DCERPC auth type
 * @param auth_level DCERPC auth level 
 */

_PUBLIC_ NTSTATUS gensec_start_mech_by_authtype(struct gensec_security *gensec_security, 
				       uint8_t auth_type, uint8_t auth_level) 
{
	gensec_security->ops = gensec_security_by_authtype(gensec_security, auth_type);
	if (!gensec_security->ops) {
		DEBUG(3, ("Could not find GENSEC backend for auth_type=%d\n", (int)auth_type));
		return NT_STATUS_INVALID_PARAMETER;
	}
	gensec_want_feature(gensec_security, GENSEC_FEATURE_DCE_STYLE);
	gensec_want_feature(gensec_security, GENSEC_FEATURE_ASYNC_REPLIES);
	if (auth_level == DCERPC_AUTH_LEVEL_INTEGRITY) {
		gensec_want_feature(gensec_security, GENSEC_FEATURE_SIGN);
	} else if (auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
		gensec_want_feature(gensec_security, GENSEC_FEATURE_SIGN);
		gensec_want_feature(gensec_security, GENSEC_FEATURE_SEAL);
	} else if (auth_level == DCERPC_AUTH_LEVEL_CONNECT) {
		/* Default features */
	} else {
		DEBUG(2,("auth_level %d not supported in DCE/RPC authentication\n", 
			 auth_level));
		return NT_STATUS_INVALID_PARAMETER;
	}

	return gensec_start_mech(gensec_security);
}

_PUBLIC_ const char *gensec_get_name_by_authtype(struct gensec_security *gensec_security, uint8_t authtype) 
{
	const struct gensec_security_ops *ops;
	ops = gensec_security_by_authtype(gensec_security, authtype);
	if (ops) {
		return ops->name;
	}
	return NULL;
}
	

_PUBLIC_ const char *gensec_get_name_by_oid(struct gensec_security *gensec_security,
											const char *oid_string) 
{
	const struct gensec_security_ops *ops;
	ops = gensec_security_by_oid(gensec_security, oid_string);
	if (ops) {
		return ops->name;
	}
	return oid_string;
}

/**
 * Start a GENSEC sub-mechanism with a specified mechansim structure, used in SPNEGO
 *
 */

NTSTATUS gensec_start_mech_by_ops(struct gensec_security *gensec_security, 
				  const struct gensec_security_ops *ops) 
{
	gensec_security->ops = ops;
	return gensec_start_mech(gensec_security);
}

/** 
 * Start a GENSEC sub-mechanism by OID, used in SPNEGO
 *
 * @note This should also be used when you wish to just start NLTMSSP (for example), as it uses a
 *       well-known #define to hook it in.
 */

_PUBLIC_ NTSTATUS gensec_start_mech_by_oid(struct gensec_security *gensec_security, 
				  const char *mech_oid) 
{
	SMB_ASSERT(gensec_security != NULL);

	gensec_security->ops = gensec_security_by_oid(gensec_security, mech_oid);
	if (!gensec_security->ops) {
		DEBUG(3, ("Could not find GENSEC backend for oid=%s\n", mech_oid));
		return NT_STATUS_INVALID_PARAMETER;
	}
	return gensec_start_mech(gensec_security);
}

/** 
 * Start a GENSEC sub-mechanism by a well know SASL name
 *
 */

_PUBLIC_ NTSTATUS gensec_start_mech_by_sasl_name(struct gensec_security *gensec_security, 
					const char *sasl_name) 
{
	gensec_security->ops = gensec_security_by_sasl_name(gensec_security, sasl_name);
	if (!gensec_security->ops) {
		DEBUG(3, ("Could not find GENSEC backend for sasl_name=%s\n", sasl_name));
		return NT_STATUS_INVALID_PARAMETER;
	}
	return gensec_start_mech(gensec_security);
}

/** 
 * Start a GENSEC sub-mechanism with the preferred option from a SASL name list
 *
 */

_PUBLIC_ NTSTATUS gensec_start_mech_by_sasl_list(struct gensec_security *gensec_security, 
						 const char **sasl_names) 
{
	NTSTATUS nt_status = NT_STATUS_INVALID_PARAMETER;
	TALLOC_CTX *mem_ctx = talloc_new(gensec_security);
	const struct gensec_security_ops **ops;
	int i;
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}
	ops = gensec_security_by_sasl_list(gensec_security, mem_ctx, sasl_names);
	if (!ops || !*ops) {
		DEBUG(3, ("Could not find GENSEC backend for any of sasl_name = %s\n", 
			  str_list_join(mem_ctx, 
					sasl_names, ' ')));
		talloc_free(mem_ctx);
		return NT_STATUS_INVALID_PARAMETER;
	}
	for (i=0; ops[i]; i++) {
		nt_status = gensec_start_mech_by_ops(gensec_security, ops[i]);
		if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_INVALID_PARAMETER)) {
			break;
		}
	}
	talloc_free(mem_ctx);
	return nt_status;
}

/** 
 * Start a GENSEC sub-mechanism by an internal name
 *
 */

_PUBLIC_ NTSTATUS gensec_start_mech_by_name(struct gensec_security *gensec_security, 
					const char *name) 
{
	gensec_security->ops = gensec_security_by_name(gensec_security, name);
	if (!gensec_security->ops) {
		DEBUG(3, ("Could not find GENSEC backend for name=%s\n", name));
		return NT_STATUS_INVALID_PARAMETER;
	}
	return gensec_start_mech(gensec_security);
}

/*
  wrappers for the gensec function pointers
*/
_PUBLIC_ NTSTATUS gensec_unseal_packet(struct gensec_security *gensec_security, 
			      TALLOC_CTX *mem_ctx, 
			      uint8_t *data, size_t length, 
			      const uint8_t *whole_pdu, size_t pdu_length, 
			      const DATA_BLOB *sig)
{
	if (!gensec_security->ops->unseal_packet) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_security->ops->unseal_packet(gensec_security, mem_ctx, 
						   data, length, 
						   whole_pdu, pdu_length, 
						   sig);
}

_PUBLIC_ NTSTATUS gensec_check_packet(struct gensec_security *gensec_security, 
			     TALLOC_CTX *mem_ctx, 
			     const uint8_t *data, size_t length, 
			     const uint8_t *whole_pdu, size_t pdu_length, 
			     const DATA_BLOB *sig)
{
	if (!gensec_security->ops->check_packet) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_security->ops->check_packet(gensec_security, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

_PUBLIC_ NTSTATUS gensec_seal_packet(struct gensec_security *gensec_security, 
			    TALLOC_CTX *mem_ctx, 
			    uint8_t *data, size_t length, 
			    const uint8_t *whole_pdu, size_t pdu_length, 
			    DATA_BLOB *sig)
{
	if (!gensec_security->ops->seal_packet) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_security->ops->seal_packet(gensec_security, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

_PUBLIC_ NTSTATUS gensec_sign_packet(struct gensec_security *gensec_security, 
			    TALLOC_CTX *mem_ctx, 
			    const uint8_t *data, size_t length, 
			    const uint8_t *whole_pdu, size_t pdu_length, 
			    DATA_BLOB *sig)
{
	if (!gensec_security->ops->sign_packet) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_security->ops->sign_packet(gensec_security, mem_ctx, data, length, whole_pdu, pdu_length, sig);
}

_PUBLIC_ size_t gensec_sig_size(struct gensec_security *gensec_security, size_t data_size) 
{
	if (!gensec_security->ops->sig_size) {
		return 0;
	}
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SIGN)) {
		return 0;
	}
	
	return gensec_security->ops->sig_size(gensec_security, data_size);
}

size_t gensec_max_wrapped_size(struct gensec_security *gensec_security) 
{
	if (!gensec_security->ops->max_wrapped_size) {
		return (1 << 17);
	}
	
	return gensec_security->ops->max_wrapped_size(gensec_security);
}

size_t gensec_max_input_size(struct gensec_security *gensec_security) 
{
	if (!gensec_security->ops->max_input_size) {
		return (1 << 17) - gensec_sig_size(gensec_security, 1 << 17);
	}
	
	return gensec_security->ops->max_input_size(gensec_security);
}

_PUBLIC_ NTSTATUS gensec_wrap(struct gensec_security *gensec_security, 
		     TALLOC_CTX *mem_ctx, 
		     const DATA_BLOB *in, 
		     DATA_BLOB *out) 
{
	if (!gensec_security->ops->wrap) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return gensec_security->ops->wrap(gensec_security, mem_ctx, in, out);
}

_PUBLIC_ NTSTATUS gensec_unwrap(struct gensec_security *gensec_security, 
		       TALLOC_CTX *mem_ctx, 
		       const DATA_BLOB *in, 
		       DATA_BLOB *out) 
{
	if (!gensec_security->ops->unwrap) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return gensec_security->ops->unwrap(gensec_security, mem_ctx, in, out);
}

_PUBLIC_ NTSTATUS gensec_session_key(struct gensec_security *gensec_security, 
			    DATA_BLOB *session_key)
{
	if (!gensec_security->ops->session_key) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	if (!gensec_have_feature(gensec_security, GENSEC_FEATURE_SESSION_KEY)) {
		return NT_STATUS_NO_USER_SESSION_KEY;
	}
	
	return gensec_security->ops->session_key(gensec_security, session_key);
}

/** 
 * Return the credentials of a logged on user, including session keys
 * etc.
 *
 * Only valid after a successful authentication
 *
 * May only be called once per authentication.
 *
 */

_PUBLIC_ NTSTATUS gensec_session_info(struct gensec_security *gensec_security, 
			     struct auth_session_info **session_info)
{
	if (!gensec_security->ops->session_info) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}
	return gensec_security->ops->session_info(gensec_security, session_info);
}

/**
 * Next state function for the GENSEC state machine
 * 
 * @param gensec_security GENSEC State
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent, 
 *                or NT_STATUS_OK if the user is authenticated. 
 */

_PUBLIC_ NTSTATUS gensec_update(struct gensec_security *gensec_security, TALLOC_CTX *out_mem_ctx, 
		       const DATA_BLOB in, DATA_BLOB *out) 
{
	return gensec_security->ops->update(gensec_security, out_mem_ctx, in, out);
}

struct gensec_update_state {
	struct tevent_immediate *im;
	struct gensec_security *gensec_security;
	DATA_BLOB in;
	DATA_BLOB out;
};

static void gensec_update_async_trigger(struct tevent_context *ctx,
					struct tevent_immediate *im,
					void *private_data);
/**
 * Next state function for the GENSEC state machine async version
 *
 * @param mem_ctx The memory context for the request
 * @param ev The event context for the request
 * @param gensec_security GENSEC State
 * @param in The request, as a DATA_BLOB
 *
 * @return The request handle or NULL on no memory failure
 */

_PUBLIC_ struct tevent_req *gensec_update_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct gensec_security *gensec_security,
					       const DATA_BLOB in)
{
	struct tevent_req *req;
	struct gensec_update_state *state = NULL;

	req = tevent_req_create(mem_ctx, &state,
				struct gensec_update_state);
	if (req == NULL) {
		return NULL;
	}

	state->gensec_security		= gensec_security;
	state->in			= in;
	state->out			= data_blob(NULL, 0);
	state->im			= tevent_create_immediate(state);
	if (tevent_req_nomem(state->im, req)) {
		return tevent_req_post(req, ev);
	}

	tevent_schedule_immediate(state->im, ev,
				  gensec_update_async_trigger,
				  req);

	return req;
}

static void gensec_update_async_trigger(struct tevent_context *ctx,
					struct tevent_immediate *im,
					void *private_data)
{
	struct tevent_req *req =
		talloc_get_type_abort(private_data, struct tevent_req);
	struct gensec_update_state *state =
		tevent_req_data(req, struct gensec_update_state);
	NTSTATUS status;

	status = gensec_update(state->gensec_security, state,
			       state->in, &state->out);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	tevent_req_done(req);
}

/**
 * Next state function for the GENSEC state machine
 * 
 * @param req request state
 * @param out_mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param out The reply, as an talloc()ed DATA_BLOB, on *out_mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent, 
 *                or NT_STATUS_OK if the user is authenticated. 
 */
_PUBLIC_ NTSTATUS gensec_update_recv(struct tevent_req *req,
				     TALLOC_CTX *out_mem_ctx,
				     DATA_BLOB *out)
{
	struct gensec_update_state *state =
		tevent_req_data(req, struct gensec_update_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			tevent_req_received(req);
			return status;
		}
	} else {
		status = NT_STATUS_OK;
	}

	*out = state->out;
	talloc_steal(out_mem_ctx, out->data);

	tevent_req_received(req);
	return status;
}

/** 
 * Set the requirement for a certain feature on the connection
 *
 */

_PUBLIC_ void gensec_want_feature(struct gensec_security *gensec_security,
			 uint32_t feature) 
{
	if (!gensec_security->ops || !gensec_security->ops->want_feature) {
		gensec_security->want_features |= feature;
		return;
	}
	gensec_security->ops->want_feature(gensec_security, feature);
}

/** 
 * Check the requirement for a certain feature on the connection
 *
 */

_PUBLIC_ bool gensec_have_feature(struct gensec_security *gensec_security,
			 uint32_t feature) 
{
	if (!gensec_security->ops->have_feature) {
		return false;
	}
	
	/* We might 'have' features that we don't 'want', because the
	 * other end demanded them, or we can't neotiate them off */
	return gensec_security->ops->have_feature(gensec_security, feature);
}

/** 
 * Associate a credentials structure with a GENSEC context - talloc_reference()s it to the context 
 *
 */

_PUBLIC_ NTSTATUS gensec_set_credentials(struct gensec_security *gensec_security, struct cli_credentials *credentials) 
{
	gensec_security->credentials = talloc_reference(gensec_security, credentials);
	NT_STATUS_HAVE_NO_MEMORY(gensec_security->credentials);
	gensec_want_feature(gensec_security, cli_credentials_get_gensec_features(gensec_security->credentials));
	return NT_STATUS_OK;
}

/** 
 * Return the credentials structure associated with a GENSEC context
 *
 */

_PUBLIC_ struct cli_credentials *gensec_get_credentials(struct gensec_security *gensec_security) 
{
	if (!gensec_security) {
		return NULL;
	}
	return gensec_security->credentials;
}

/** 
 * Set the target service (such as 'http' or 'host') on a GENSEC context - ensures it is talloc()ed 
 *
 */

_PUBLIC_ NTSTATUS gensec_set_target_service(struct gensec_security *gensec_security, const char *service) 
{
	gensec_security->target.service = talloc_strdup(gensec_security, service);
	if (!gensec_security->target.service) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

_PUBLIC_ const char *gensec_get_target_service(struct gensec_security *gensec_security) 
{
	if (gensec_security->target.service) {
		return gensec_security->target.service;
	}

	return "host";
}

/** 
 * Set the target hostname (suitable for kerberos resolutation) on a GENSEC context - ensures it is talloc()ed 
 *
 */

_PUBLIC_ NTSTATUS gensec_set_target_hostname(struct gensec_security *gensec_security, const char *hostname) 
{
	gensec_security->target.hostname = talloc_strdup(gensec_security, hostname);
	if (hostname && !gensec_security->target.hostname) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

_PUBLIC_ const char *gensec_get_target_hostname(struct gensec_security *gensec_security) 
{
	/* We allow the target hostname to be overriden for testing purposes */
	if (gensec_security->settings->target_hostname) {
		return gensec_security->settings->target_hostname;
	}

	if (gensec_security->target.hostname) {
		return gensec_security->target.hostname;
	}

	/* We could add use the 'set sockaddr' call, and do a reverse
	 * lookup, but this would be both insecure (compromising the
	 * way kerberos works) and add DNS timeouts */
	return NULL;
}

/**
 * Set (and talloc_reference) local and peer socket addresses onto a socket
 * context on the GENSEC context.
 *
 * This is so that kerberos can include these addresses in
 * cryptographic tokens, to avoid certain attacks.
 */

/**
 * @brief Set the local gensec address.
 *
 * @param  gensec_security   The gensec security context to use.
 *
 * @param  remote       The local address to set.
 *
 * @return              On success NT_STATUS_OK is returned or an NT_STATUS
 *                      error.
 */
_PUBLIC_ NTSTATUS gensec_set_local_address(struct gensec_security *gensec_security,
		const struct tsocket_address *local)
{
	TALLOC_FREE(gensec_security->local_addr);

	if (local == NULL) {
		return NT_STATUS_OK;
	}

	gensec_security->local_addr = tsocket_address_copy(local, gensec_security);
	if (gensec_security->local_addr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/**
 * @brief Set the remote gensec address.
 *
 * @param  gensec_security   The gensec security context to use.
 *
 * @param  remote       The remote address to set.
 *
 * @return              On success NT_STATUS_OK is returned or an NT_STATUS
 *                      error.
 */
_PUBLIC_ NTSTATUS gensec_set_remote_address(struct gensec_security *gensec_security,
		const struct tsocket_address *remote)
{
	TALLOC_FREE(gensec_security->remote_addr);

	if (remote == NULL) {
		return NT_STATUS_OK;
	}

	gensec_security->remote_addr = tsocket_address_copy(remote, gensec_security);
	if (gensec_security->remote_addr == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/**
 * @brief Get the local address from a gensec security context.
 *
 * @param  gensec_security   The security context to get the address from.
 *
 * @return              The address as tsocket_address which could be NULL if
 *                      no address is set.
 */
_PUBLIC_ const struct tsocket_address *gensec_get_local_address(struct gensec_security *gensec_security)
{
	if (gensec_security == NULL) {
		return NULL;
	}
	return gensec_security->local_addr;
}

/**
 * @brief Get the remote address from a gensec security context.
 *
 * @param  gensec_security   The security context to get the address from.
 *
 * @return              The address as tsocket_address which could be NULL if
 *                      no address is set.
 */
_PUBLIC_ const struct tsocket_address *gensec_get_remote_address(struct gensec_security *gensec_security)
{
	if (gensec_security == NULL) {
		return NULL;
	}
	return gensec_security->remote_addr;
}

/** 
 * Set the target principal (assuming it it known, say from the SPNEGO reply)
 *  - ensures it is talloc()ed 
 *
 */

_PUBLIC_ NTSTATUS gensec_set_target_principal(struct gensec_security *gensec_security, const char *principal)
{
	gensec_security->target.principal = talloc_strdup(gensec_security, principal);
	if (!gensec_security->target.principal) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

const char *gensec_get_target_principal(struct gensec_security *gensec_security) 
{
	if (gensec_security->target.principal) {
		return gensec_security->target.principal;
	}

	return NULL;
}

NTSTATUS gensec_generate_session_info(TALLOC_CTX *mem_ctx,
				      struct gensec_security *gensec_security,
				      struct auth_user_info_dc *user_info_dc,
				      struct auth_session_info **session_info)
{
	NTSTATUS nt_status;
	uint32_t flags = AUTH_SESSION_INFO_DEFAULT_GROUPS;
	if (user_info_dc->info->authenticated) {
		flags |= AUTH_SESSION_INFO_AUTHENTICATED;
	}
	if (gensec_security->auth_context) {
		nt_status = gensec_security->auth_context->generate_session_info(mem_ctx, gensec_security->auth_context,
										 user_info_dc,
										 flags,
										 session_info);
	} else {
		flags |= AUTH_SESSION_INFO_SIMPLE_PRIVILEGES;
		nt_status = auth_generate_session_info(mem_ctx,
						       NULL,
						       NULL,
						       user_info_dc, flags,
						       session_info);
	}
	return nt_status;
}

/*
  register a GENSEC backend. 

  The 'name' can be later used by other backends to find the operations
  structure for this backend.
*/
NTSTATUS gensec_register(const struct gensec_security_ops *ops)
{
	if (gensec_security_by_name(NULL, ops->name) != NULL) {
		/* its already registered! */
		DEBUG(0,("GENSEC backend '%s' already registered\n", 
			 ops->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	generic_security_ops = talloc_realloc(talloc_autofree_context(), 
					      generic_security_ops, 
					      struct gensec_security_ops *, 
					      gensec_num_backends+2);
	if (!generic_security_ops) {
		return NT_STATUS_NO_MEMORY;
	}

	generic_security_ops[gensec_num_backends] = discard_const_p(struct gensec_security_ops, ops);
	gensec_num_backends++;
	generic_security_ops[gensec_num_backends] = NULL;

	DEBUG(3,("GENSEC backend '%s' registered\n", 
		 ops->name));

	return NT_STATUS_OK;
}

/*
  return the GENSEC interface version, and the size of some critical types
  This can be used by backends to either detect compilation errors, or provide
  multiple implementations for different smbd compilation options in one module
*/
const struct gensec_critical_sizes *gensec_interface_version(void)
{
	static const struct gensec_critical_sizes critical_sizes = {
		GENSEC_INTERFACE_VERSION,
		sizeof(struct gensec_security_ops),
		sizeof(struct gensec_security),
	};

	return &critical_sizes;
}

static int sort_gensec(struct gensec_security_ops **gs1, struct gensec_security_ops **gs2) {
	return (*gs2)->priority - (*gs1)->priority;
}

int gensec_setting_int(struct gensec_settings *settings, const char *mechanism, const char *name, int default_value)
{
	return lpcfg_parm_int(settings->lp_ctx, NULL, mechanism, name, default_value);
}

bool gensec_setting_bool(struct gensec_settings *settings, const char *mechanism, const char *name, bool default_value)
{
	return lpcfg_parm_bool(settings->lp_ctx, NULL, mechanism, name, default_value);
}

/*
  initialise the GENSEC subsystem
*/
_PUBLIC_ NTSTATUS gensec_init(struct loadparm_context *lp_ctx)
{
	static bool initialized = false;
#define _MODULE_PROTO(init) extern NTSTATUS init(void);
	STATIC_gensec_MODULES_PROTO;
	init_module_fn static_init[] = { STATIC_gensec_MODULES };
	init_module_fn *shared_init;

	if (initialized) return NT_STATUS_OK;
	initialized = true;
	
	shared_init = load_samba_modules(NULL, lp_ctx, "gensec");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	TYPESAFE_QSORT(generic_security_ops, gensec_num_backends, sort_gensec);
	
	return NT_STATUS_OK;
}
