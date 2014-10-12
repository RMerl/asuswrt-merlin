/* 
   Unix SMB/CIFS implementation.

   RFC2478 Compliant SPNEGO implementation
   
   Copyright (C) Jim McDonough <jmcd@us.ibm.com>      2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Stefan Metzmacher <metze@samba.org>  2004-2008

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
#include "../libcli/auth/spnego.h"
#include "librpc/gen_ndr/ndr_dcerpc.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "param/param.h"

enum spnego_state_position {
	SPNEGO_SERVER_START,
	SPNEGO_CLIENT_START,
	SPNEGO_SERVER_TARG,
	SPNEGO_CLIENT_TARG,
	SPNEGO_FALLBACK,
	SPNEGO_DONE
};

struct spnego_state {
	enum spnego_message_type expected_packet;
	enum spnego_state_position state_position;
	struct gensec_security *sub_sec_security;
	bool no_response_expected;

	const char *neg_oid;

	DATA_BLOB mech_types;
};


static NTSTATUS gensec_spnego_client_start(struct gensec_security *gensec_security)
{
	struct spnego_state *spnego_state;

	spnego_state = talloc(gensec_security, struct spnego_state);
	if (!spnego_state) {
		return NT_STATUS_NO_MEMORY;
	}

	spnego_state->expected_packet = SPNEGO_NEG_TOKEN_INIT;
	spnego_state->state_position = SPNEGO_CLIENT_START;
	spnego_state->sub_sec_security = NULL;
	spnego_state->no_response_expected = false;
	spnego_state->mech_types = data_blob(NULL, 0);

	gensec_security->private_data = spnego_state;
	return NT_STATUS_OK;
}

static NTSTATUS gensec_spnego_server_start(struct gensec_security *gensec_security)
{
	struct spnego_state *spnego_state;

	spnego_state = talloc(gensec_security, struct spnego_state);		
	if (!spnego_state) {
		return NT_STATUS_NO_MEMORY;
	}

	spnego_state->expected_packet = SPNEGO_NEG_TOKEN_INIT;
	spnego_state->state_position = SPNEGO_SERVER_START;
	spnego_state->sub_sec_security = NULL;
	spnego_state->no_response_expected = false;
	spnego_state->mech_types = data_blob(NULL, 0);

	gensec_security->private_data = spnego_state;
	return NT_STATUS_OK;
}

/*
  wrappers for the spnego_*() functions
*/
static NTSTATUS gensec_spnego_unseal_packet(struct gensec_security *gensec_security, 
					    TALLOC_CTX *mem_ctx, 
					    uint8_t *data, size_t length, 
					    const uint8_t *whole_pdu, size_t pdu_length, 
					    const DATA_BLOB *sig)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_unseal_packet(spnego_state->sub_sec_security, 
				    mem_ctx, 
				    data, length, 
				    whole_pdu, pdu_length,
				    sig); 
}

static NTSTATUS gensec_spnego_check_packet(struct gensec_security *gensec_security, 
					   TALLOC_CTX *mem_ctx, 
					   const uint8_t *data, size_t length, 
					   const uint8_t *whole_pdu, size_t pdu_length, 
					   const DATA_BLOB *sig)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_check_packet(spnego_state->sub_sec_security, 
				   mem_ctx, 
				   data, length, 
				   whole_pdu, pdu_length,
				   sig);
}

static NTSTATUS gensec_spnego_seal_packet(struct gensec_security *gensec_security, 
					  TALLOC_CTX *mem_ctx, 
					  uint8_t *data, size_t length, 
					  const uint8_t *whole_pdu, size_t pdu_length, 
					  DATA_BLOB *sig)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_seal_packet(spnego_state->sub_sec_security, 
				  mem_ctx, 
				  data, length, 
				  whole_pdu, pdu_length,
				  sig);
}

static NTSTATUS gensec_spnego_sign_packet(struct gensec_security *gensec_security, 
					  TALLOC_CTX *mem_ctx, 
					  const uint8_t *data, size_t length, 
					  const uint8_t *whole_pdu, size_t pdu_length, 
					  DATA_BLOB *sig)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_sign_packet(spnego_state->sub_sec_security, 
				  mem_ctx, 
				  data, length, 
				  whole_pdu, pdu_length,
				  sig);
}

static NTSTATUS gensec_spnego_wrap(struct gensec_security *gensec_security, 
				   TALLOC_CTX *mem_ctx, 
				   const DATA_BLOB *in, 
				   DATA_BLOB *out)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		DEBUG(1, ("gensec_spnego_wrap: wrong state for wrap\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_wrap(spnego_state->sub_sec_security, 
			   mem_ctx, in, out);
}

static NTSTATUS gensec_spnego_unwrap(struct gensec_security *gensec_security, 
				     TALLOC_CTX *mem_ctx, 
				     const DATA_BLOB *in, 
				     DATA_BLOB *out)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		DEBUG(1, ("gensec_spnego_unwrap: wrong state for unwrap\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_unwrap(spnego_state->sub_sec_security, 
			     mem_ctx, in, out);
}

static NTSTATUS gensec_spnego_wrap_packets(struct gensec_security *gensec_security, 
					   TALLOC_CTX *mem_ctx, 
					   const DATA_BLOB *in, 
					   DATA_BLOB *out,
					   size_t *len_processed) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		DEBUG(1, ("gensec_spnego_wrap: wrong state for wrap\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_wrap_packets(spnego_state->sub_sec_security, 
				   mem_ctx, in, out,
				   len_processed);
}

static NTSTATUS gensec_spnego_packet_full_request(struct gensec_security *gensec_security, 
					     	DATA_BLOB blob, size_t *size)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		DEBUG(1, ("gensec_spnego_unwrap: wrong state for unwrap\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_packet_full_request(spnego_state->sub_sec_security, 
					  blob, size);
}

static NTSTATUS gensec_spnego_unwrap_packets(struct gensec_security *gensec_security, 
					     TALLOC_CTX *mem_ctx, 
					     const DATA_BLOB *in, 
					     DATA_BLOB *out,
					     size_t *len_processed) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		DEBUG(1, ("gensec_spnego_unwrap: wrong state for unwrap\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_unwrap_packets(spnego_state->sub_sec_security, 
				     mem_ctx, in, out,
				     len_processed);
}

static size_t gensec_spnego_sig_size(struct gensec_security *gensec_security, size_t data_size) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return 0;
	}
	
	return gensec_sig_size(spnego_state->sub_sec_security, data_size);
}

static size_t gensec_spnego_max_input_size(struct gensec_security *gensec_security) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return 0;
	}
	
	return gensec_max_input_size(spnego_state->sub_sec_security);
}

static size_t gensec_spnego_max_wrapped_size(struct gensec_security *gensec_security) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (spnego_state->state_position != SPNEGO_DONE 
	    && spnego_state->state_position != SPNEGO_FALLBACK) {
		return 0;
	}
	
	return gensec_max_wrapped_size(spnego_state->sub_sec_security);
}

static NTSTATUS gensec_spnego_session_key(struct gensec_security *gensec_security, 
					  DATA_BLOB *session_key)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;
	if (!spnego_state->sub_sec_security) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_session_key(spnego_state->sub_sec_security, 
				  session_key);
}

static NTSTATUS gensec_spnego_session_info(struct gensec_security *gensec_security,
								      struct auth_session_info **session_info) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;
	if (!spnego_state->sub_sec_security) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	return gensec_session_info(spnego_state->sub_sec_security, 
				   session_info);
}

/** Fallback to another GENSEC mechanism, based on magic strings 
 *
 * This is the 'fallback' case, where we don't get SPNEGO, and have to
 * try all the other options (and hope they all have a magic string
 * they check)
*/

static NTSTATUS gensec_spnego_server_try_fallback(struct gensec_security *gensec_security, 
						  struct spnego_state *spnego_state,
						  TALLOC_CTX *out_mem_ctx, 
						  const DATA_BLOB in, DATA_BLOB *out) 
{
	int i,j;
	struct gensec_security_ops **all_ops
		= gensec_security_mechs(gensec_security, out_mem_ctx);
	for (i=0; all_ops[i]; i++) {
		bool is_spnego;
		NTSTATUS nt_status;

	    	if (gensec_security != NULL && 
				!gensec_security_ops_enabled(all_ops[i], gensec_security))
		    continue;

		if (!all_ops[i]->oid) {
			continue;
		}

		is_spnego = false;
		for (j=0; all_ops[i]->oid[j]; j++) {
			if (strcasecmp(GENSEC_OID_SPNEGO,all_ops[i]->oid[j]) == 0) {
				is_spnego = true;
			}
		}
		if (is_spnego) {
			continue;
		}

		if (!all_ops[i]->magic) {
			continue;
		}

		nt_status = all_ops[i]->magic(gensec_security, &in);
		if (!NT_STATUS_IS_OK(nt_status)) {
			continue;
		}

		spnego_state->state_position = SPNEGO_FALLBACK;

		nt_status = gensec_subcontext_start(spnego_state, 
						    gensec_security, 
						    &spnego_state->sub_sec_security);

		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
		/* select the sub context */
		nt_status = gensec_start_mech_by_ops(spnego_state->sub_sec_security,
						     all_ops[i]);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
		nt_status = gensec_update(spnego_state->sub_sec_security,
					  out_mem_ctx, in, out);
		return nt_status;
	}
	DEBUG(1, ("Failed to parse SPNEGO request\n"));
	return NT_STATUS_INVALID_PARAMETER;
	
}

/* 
   Parse the netTokenInit, either from the client, to the server, or
   from the server to the client.
*/

static NTSTATUS gensec_spnego_parse_negTokenInit(struct gensec_security *gensec_security,
						 struct spnego_state *spnego_state, 
						 TALLOC_CTX *out_mem_ctx, 
						 const char **mechType,
						 const DATA_BLOB unwrapped_in, DATA_BLOB *unwrapped_out) 
{
	int i;
	NTSTATUS nt_status = NT_STATUS_INVALID_PARAMETER;
	DATA_BLOB null_data_blob = data_blob(NULL,0);
	bool ok;

	const struct gensec_security_ops_wrapper *all_sec
		= gensec_security_by_oid_list(gensec_security, 
					      out_mem_ctx, 
					      mechType,
					      GENSEC_OID_SPNEGO);

	ok = spnego_write_mech_types(spnego_state,
				     mechType,
				     &spnego_state->mech_types);
	if (!ok) {
		DEBUG(1, ("SPNEGO: Failed to write mechTypes\n"));
		return NT_STATUS_NO_MEMORY;
	}

	if (spnego_state->state_position == SPNEGO_SERVER_START) {
		uint32_t j;
		for (j=0; mechType && mechType[j]; j++) {
			for (i=0; all_sec && all_sec[i].op; i++) {
				nt_status = gensec_subcontext_start(spnego_state,
								    gensec_security,
								    &spnego_state->sub_sec_security);
				if (!NT_STATUS_IS_OK(nt_status)) {
					return nt_status;
				}
				/* select the sub context */
				nt_status = gensec_start_mech_by_ops(spnego_state->sub_sec_security,
								     all_sec[i].op);
				if (!NT_STATUS_IS_OK(nt_status)) {
					talloc_free(spnego_state->sub_sec_security);
					spnego_state->sub_sec_security = NULL;
					break;
				}

				if (j > 0) {
					/* no optimistic token */
					spnego_state->neg_oid = all_sec[i].oid;
					*unwrapped_out = data_blob_null;
					nt_status = NT_STATUS_MORE_PROCESSING_REQUIRED;
					break;
				}

				nt_status = gensec_update(spnego_state->sub_sec_security,
							  out_mem_ctx, 
							  unwrapped_in,
							  unwrapped_out);
				if (NT_STATUS_EQUAL(nt_status, NT_STATUS_INVALID_PARAMETER) || 
				    NT_STATUS_EQUAL(nt_status, NT_STATUS_CANT_ACCESS_DOMAIN_INFO)) {
					/* Pretend we never started it (lets the first run find some incompatible demand) */
					
					DEBUG(1, ("SPNEGO(%s) NEG_TOKEN_INIT failed to parse contents: %s\n", 
						  spnego_state->sub_sec_security->ops->name, nt_errstr(nt_status)));
					talloc_free(spnego_state->sub_sec_security);
					spnego_state->sub_sec_security = NULL;
					break;
				}

				spnego_state->neg_oid = all_sec[i].oid;
				break;
			}
			if (spnego_state->sub_sec_security) {
				break;
			}
		}

		if (!spnego_state->sub_sec_security) {
			DEBUG(1, ("SPNEGO: Could not find a suitable mechtype in NEG_TOKEN_INIT\n"));
			return NT_STATUS_INVALID_PARAMETER;
		}
	}
	
	/* Having tried any optimistic token from the client (if we
	 * were the server), if we didn't get anywhere, walk our list
	 * in our preference order */
	
	if (!spnego_state->sub_sec_security) {
		for (i=0; all_sec && all_sec[i].op; i++) {
			nt_status = gensec_subcontext_start(spnego_state,
							    gensec_security,
							    &spnego_state->sub_sec_security);
			if (!NT_STATUS_IS_OK(nt_status)) {
				return nt_status;
			}
			/* select the sub context */
			nt_status = gensec_start_mech_by_ops(spnego_state->sub_sec_security,
							     all_sec[i].op);
			if (!NT_STATUS_IS_OK(nt_status)) {
				talloc_free(spnego_state->sub_sec_security);
				spnego_state->sub_sec_security = NULL;
				continue;
			}
			
			spnego_state->neg_oid = all_sec[i].oid;

			/* only get the helping start blob for the first OID */
			nt_status = gensec_update(spnego_state->sub_sec_security,
						  out_mem_ctx, 
						  null_data_blob, 
						  unwrapped_out);

			/* it is likely that a NULL input token will
			 * not be liked by most server mechs, but if
			 * we are in the client, we want the first
			 * update packet to be able to abort the use
			 * of this mech */
			if (spnego_state->state_position != SPNEGO_SERVER_START) {
				if (NT_STATUS_EQUAL(nt_status, NT_STATUS_INVALID_PARAMETER) || 
				    NT_STATUS_EQUAL(nt_status, NT_STATUS_NO_LOGON_SERVERS) ||
				    NT_STATUS_EQUAL(nt_status, NT_STATUS_TIME_DIFFERENCE_AT_DC) ||
				    NT_STATUS_EQUAL(nt_status, NT_STATUS_CANT_ACCESS_DOMAIN_INFO)) {
					/* Pretend we never started it (lets the first run find some incompatible demand) */
					
					DEBUG(1, ("SPNEGO(%s) NEG_TOKEN_INIT failed to parse: %s\n", 
						  spnego_state->sub_sec_security->ops->name, nt_errstr(nt_status)));
					talloc_free(spnego_state->sub_sec_security);
					spnego_state->sub_sec_security = NULL;
					continue;
				}
			}

			break;
		}
	}

	if (spnego_state->sub_sec_security) {
		/* it is likely that a NULL input token will
		 * not be liked by most server mechs, but this
		 * does the right thing in the CIFS client.
		 * just push us along the merry-go-round
		 * again, and hope for better luck next
		 * time */
		
		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_INVALID_PARAMETER)) {
			*unwrapped_out = data_blob(NULL, 0);
			nt_status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		}
		
		if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_INVALID_PARAMETER) 
		    && !NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) 
		    && !NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1, ("SPNEGO(%s) NEG_TOKEN_INIT failed: %s\n", 
				  spnego_state->sub_sec_security->ops->name, nt_errstr(nt_status)));
			talloc_free(spnego_state->sub_sec_security);
			spnego_state->sub_sec_security = NULL;
			
			/* We started the mech correctly, and the
			 * input from the other side was valid.
			 * Return the error (say bad password, invalid
			 * ticket) */
			return nt_status;
		}
	
		
		return nt_status; /* OK, INVALID_PARAMETER ore MORE PROCESSING */
	}

	DEBUG(1, ("SPNEGO: Could not find a suitable mechtype in NEG_TOKEN_INIT\n"));
	/* we could re-negotiate here, but it would only work
	 * if the client or server lied about what it could
	 * support the first time.  Lets keep this code to
	 * reality */

	return nt_status;
}

/** create a negTokenInit 
 *
 * This is the same packet, no matter if the client or server sends it first, but it is always the first packet
*/
static NTSTATUS gensec_spnego_create_negTokenInit(struct gensec_security *gensec_security, 
						  struct spnego_state *spnego_state,
						  TALLOC_CTX *out_mem_ctx, 
						  const DATA_BLOB in, DATA_BLOB *out) 
{
	int i;
	NTSTATUS nt_status = NT_STATUS_INVALID_PARAMETER;
	DATA_BLOB null_data_blob = data_blob(NULL,0);
	const char **mechTypes = NULL;
	DATA_BLOB unwrapped_out = data_blob(NULL, 0);
	const struct gensec_security_ops_wrapper *all_sec;

	mechTypes = gensec_security_oids(gensec_security, 
					 out_mem_ctx, GENSEC_OID_SPNEGO);

	all_sec	= gensec_security_by_oid_list(gensec_security, 
					      out_mem_ctx, 
					      mechTypes,
					      GENSEC_OID_SPNEGO);
	for (i=0; all_sec && all_sec[i].op; i++) {
		struct spnego_data spnego_out;
		const char **send_mech_types;
		bool ok;

		nt_status = gensec_subcontext_start(spnego_state,
						    gensec_security,
						    &spnego_state->sub_sec_security);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
		/* select the sub context */
		nt_status = gensec_start_mech_by_ops(spnego_state->sub_sec_security,
						     all_sec[i].op);
		if (!NT_STATUS_IS_OK(nt_status)) {
			talloc_free(spnego_state->sub_sec_security);
			spnego_state->sub_sec_security = NULL;
			continue;
		}

		/* In the client, try and produce the first (optimistic) packet */
		if (spnego_state->state_position == SPNEGO_CLIENT_START) {
			nt_status = gensec_update(spnego_state->sub_sec_security,
						  out_mem_ctx, 
						  null_data_blob,
						  &unwrapped_out);
			
			if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) 
			    && !NT_STATUS_IS_OK(nt_status)) {
				DEBUG(1, ("SPNEGO(%s) creating NEG_TOKEN_INIT failed: %s\n", 
					  spnego_state->sub_sec_security->ops->name, nt_errstr(nt_status)));
				talloc_free(spnego_state->sub_sec_security);
				spnego_state->sub_sec_security = NULL;
				/* Pretend we never started it (lets the first run find some incompatible demand) */
				
				continue;
			}
		}

		spnego_out.type = SPNEGO_NEG_TOKEN_INIT;

		send_mech_types = gensec_security_oids_from_ops_wrapped(out_mem_ctx,
									&all_sec[i]);

		ok = spnego_write_mech_types(spnego_state,
					     send_mech_types,
					     &spnego_state->mech_types);
		if (!ok) {
			DEBUG(1, ("SPNEGO: Failed to write mechTypes\n"));
			return NT_STATUS_NO_MEMORY;
		}

		/* List the remaining mechs as options */
		spnego_out.negTokenInit.mechTypes = send_mech_types;
		spnego_out.negTokenInit.reqFlags = null_data_blob;
		spnego_out.negTokenInit.reqFlagsPadding = 0;
		
		if (spnego_state->state_position == SPNEGO_SERVER_START) {
			spnego_out.negTokenInit.mechListMIC
				= data_blob_string_const(ADS_IGNORE_PRINCIPAL);
		} else {
			spnego_out.negTokenInit.mechListMIC = null_data_blob;
		}

		spnego_out.negTokenInit.mechToken = unwrapped_out;
		
		if (spnego_write_data(out_mem_ctx, out, &spnego_out) == -1) {
			DEBUG(1, ("Failed to write NEG_TOKEN_INIT\n"));
				return NT_STATUS_INVALID_PARAMETER;
		}
		
		/* set next state */
		spnego_state->neg_oid = all_sec[i].oid;
		
		if (NT_STATUS_IS_OK(nt_status)) {
			spnego_state->no_response_expected = true;
		}

		return NT_STATUS_MORE_PROCESSING_REQUIRED;
	} 
	talloc_free(spnego_state->sub_sec_security);
	spnego_state->sub_sec_security = NULL;

	DEBUG(1, ("Failed to setup SPNEGO negTokenInit request: %s\n", nt_errstr(nt_status)));
	return NT_STATUS_INVALID_PARAMETER;
}


/** create a server negTokenTarg 
 *
 * This is the case, where the client is the first one who sends data
*/

static NTSTATUS gensec_spnego_server_negTokenTarg(struct gensec_security *gensec_security, 
						  struct spnego_state *spnego_state,
						  TALLOC_CTX *out_mem_ctx, 
						  NTSTATUS nt_status,
						  const DATA_BLOB unwrapped_out,
						  DATA_BLOB mech_list_mic,
						  DATA_BLOB *out)
{
	struct spnego_data spnego_out;
	DATA_BLOB null_data_blob = data_blob(NULL, 0);

	/* compose reply */
	spnego_out.type = SPNEGO_NEG_TOKEN_TARG;
	spnego_out.negTokenTarg.responseToken = unwrapped_out;
	spnego_out.negTokenTarg.mechListMIC = null_data_blob;
	spnego_out.negTokenTarg.supportedMech = NULL;

	if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {	
		spnego_out.negTokenTarg.supportedMech = spnego_state->neg_oid;
		spnego_out.negTokenTarg.negResult = SPNEGO_ACCEPT_INCOMPLETE;
		spnego_state->state_position = SPNEGO_SERVER_TARG;
	} else if (NT_STATUS_IS_OK(nt_status)) {
		if (unwrapped_out.data) {
			spnego_out.negTokenTarg.supportedMech = spnego_state->neg_oid;
		}
		spnego_out.negTokenTarg.negResult = SPNEGO_ACCEPT_COMPLETED;
		spnego_out.negTokenTarg.mechListMIC = mech_list_mic;
		spnego_state->state_position = SPNEGO_DONE;
	} else {
		spnego_out.negTokenTarg.negResult = SPNEGO_REJECT;
		DEBUG(2, ("SPNEGO login failed: %s\n", nt_errstr(nt_status)));
		spnego_state->state_position = SPNEGO_DONE;
	}

	if (spnego_write_data(out_mem_ctx, out, &spnego_out) == -1) {
		DEBUG(1, ("Failed to write SPNEGO reply to NEG_TOKEN_TARG\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	spnego_state->expected_packet = SPNEGO_NEG_TOKEN_TARG;

	return nt_status;
}


static NTSTATUS gensec_spnego_update(struct gensec_security *gensec_security, TALLOC_CTX *out_mem_ctx, 
				     const DATA_BLOB in, DATA_BLOB *out) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;
	DATA_BLOB null_data_blob = data_blob(NULL, 0);
	DATA_BLOB mech_list_mic = data_blob(NULL, 0);
	DATA_BLOB unwrapped_out = data_blob(NULL, 0);
	struct spnego_data spnego_out;
	struct spnego_data spnego;

	ssize_t len;

	*out = data_blob(NULL, 0);

	if (!out_mem_ctx) {
		out_mem_ctx = spnego_state;
	}

	/* and switch into the state machine */

	switch (spnego_state->state_position) {
	case SPNEGO_FALLBACK:
		return gensec_update(spnego_state->sub_sec_security,
				     out_mem_ctx, in, out);
	case SPNEGO_SERVER_START:
	{
		NTSTATUS nt_status;
		if (in.length) {

			len = spnego_read_data(gensec_security, in, &spnego);
			if (len == -1) {
				return gensec_spnego_server_try_fallback(gensec_security, spnego_state, 
									 out_mem_ctx, in, out);
			}
			/* client sent NegTargetInit, we send NegTokenTarg */

			/* OK, so it's real SPNEGO, check the packet's the one we expect */
			if (spnego.type != spnego_state->expected_packet) {
				DEBUG(1, ("Invalid SPNEGO request: %d, expected %d\n", spnego.type, 
					  spnego_state->expected_packet));
				dump_data(1, in.data, in.length);
				spnego_free_data(&spnego);
				return NT_STATUS_INVALID_PARAMETER;
			}
			
			nt_status = gensec_spnego_parse_negTokenInit(gensec_security,
								     spnego_state,
								     out_mem_ctx, 
								     spnego.negTokenInit.mechTypes,
								     spnego.negTokenInit.mechToken, 
								     &unwrapped_out);
			
			nt_status = gensec_spnego_server_negTokenTarg(gensec_security,
								      spnego_state,
								      out_mem_ctx,
								      nt_status,
								      unwrapped_out,
								      null_data_blob,
								      out);
			
			spnego_free_data(&spnego);
			
			return nt_status;
		} else {
			nt_status = gensec_spnego_create_negTokenInit(gensec_security, spnego_state, 
								      out_mem_ctx, in, out);
			spnego_state->state_position = SPNEGO_SERVER_START;
			spnego_state->expected_packet = SPNEGO_NEG_TOKEN_INIT;
			return nt_status;
		}
	}
	
	case SPNEGO_CLIENT_START:
	{
		/* The server offers a list of mechanisms */
		
		const char *my_mechs[] = {NULL, NULL};
		NTSTATUS nt_status = NT_STATUS_INVALID_PARAMETER;

		if (!in.length) {
			/* client to produce negTokenInit */
			nt_status = gensec_spnego_create_negTokenInit(gensec_security, spnego_state, 
								 out_mem_ctx, in, out);
			spnego_state->state_position = SPNEGO_CLIENT_TARG;
			spnego_state->expected_packet = SPNEGO_NEG_TOKEN_TARG;
			return nt_status;
		}
		
		len = spnego_read_data(gensec_security, in, &spnego);
		
		if (len == -1) {
			DEBUG(1, ("Invalid SPNEGO request:\n"));
			dump_data(1, in.data, in.length);
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		/* OK, so it's real SPNEGO, check the packet's the one we expect */
		if (spnego.type != spnego_state->expected_packet) {
			DEBUG(1, ("Invalid SPNEGO request: %d, expected %d\n", spnego.type, 
				  spnego_state->expected_packet));
			dump_data(1, in.data, in.length);
			spnego_free_data(&spnego);
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (spnego.negTokenInit.targetPrincipal
		    && strcmp(spnego.negTokenInit.targetPrincipal, ADS_IGNORE_PRINCIPAL) != 0) {
			DEBUG(5, ("Server claims it's principal name is %s\n", spnego.negTokenInit.targetPrincipal));
			if (lpcfg_client_use_spnego_principal(gensec_security->settings->lp_ctx)) {
				gensec_set_target_principal(gensec_security, spnego.negTokenInit.targetPrincipal);
			}
		}

		nt_status = gensec_spnego_parse_negTokenInit(gensec_security,
							     spnego_state,
							     out_mem_ctx, 
							     spnego.negTokenInit.mechTypes,
							     spnego.negTokenInit.mechToken, 
							     &unwrapped_out);

		if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) && !NT_STATUS_IS_OK(nt_status)) {
			spnego_free_data(&spnego);
			return nt_status;
		}

		my_mechs[0] = spnego_state->neg_oid;
		/* compose reply */
		spnego_out.type = SPNEGO_NEG_TOKEN_INIT;
		spnego_out.negTokenInit.mechTypes = my_mechs;
		spnego_out.negTokenInit.reqFlags = null_data_blob;
		spnego_out.negTokenInit.reqFlagsPadding = 0;
		spnego_out.negTokenInit.mechListMIC = null_data_blob;
		spnego_out.negTokenInit.mechToken = unwrapped_out;
		
		if (spnego_write_data(out_mem_ctx, out, &spnego_out) == -1) {
			DEBUG(1, ("Failed to write SPNEGO reply to NEG_TOKEN_INIT\n"));
				return NT_STATUS_INVALID_PARAMETER;
		}
		
		/* set next state */
		spnego_state->expected_packet = SPNEGO_NEG_TOKEN_TARG;
		spnego_state->state_position = SPNEGO_CLIENT_TARG;

		if (NT_STATUS_IS_OK(nt_status)) {
			spnego_state->no_response_expected = true;
		}
		
		spnego_free_data(&spnego);
		return NT_STATUS_MORE_PROCESSING_REQUIRED;
	}
	case SPNEGO_SERVER_TARG:
	{
		NTSTATUS nt_status;
		bool new_spnego = false;

		if (!in.length) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		len = spnego_read_data(gensec_security, in, &spnego);
		
		if (len == -1) {
			DEBUG(1, ("Invalid SPNEGO request:\n"));
			dump_data(1, in.data, in.length);
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		/* OK, so it's real SPNEGO, check the packet's the one we expect */
		if (spnego.type != spnego_state->expected_packet) {
			DEBUG(1, ("Invalid SPNEGO request: %d, expected %d\n", spnego.type, 
				  spnego_state->expected_packet));
			dump_data(1, in.data, in.length);
			spnego_free_data(&spnego);
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (!spnego_state->sub_sec_security) {
			DEBUG(1, ("SPNEGO: Did not setup a mech in NEG_TOKEN_INIT\n"));
			spnego_free_data(&spnego);
			return NT_STATUS_INVALID_PARAMETER;
		}

		nt_status = gensec_update(spnego_state->sub_sec_security,
					  out_mem_ctx, 
					  spnego.negTokenTarg.responseToken,
					  &unwrapped_out);
		if (NT_STATUS_IS_OK(nt_status) && spnego.negTokenTarg.mechListMIC.length > 0) {
			new_spnego = true;
			nt_status = gensec_check_packet(spnego_state->sub_sec_security,
							out_mem_ctx,
							spnego_state->mech_types.data,
							spnego_state->mech_types.length,
							spnego_state->mech_types.data,
							spnego_state->mech_types.length,
							&spnego.negTokenTarg.mechListMIC);
			if (!NT_STATUS_IS_OK(nt_status)) {
				DEBUG(2,("GENSEC SPNEGO: failed to verify mechListMIC: %s\n",
					nt_errstr(nt_status)));
			}
		}
		if (NT_STATUS_IS_OK(nt_status) && new_spnego) {
			nt_status = gensec_sign_packet(spnego_state->sub_sec_security,
						       out_mem_ctx,
						       spnego_state->mech_types.data,
						       spnego_state->mech_types.length,
						       spnego_state->mech_types.data,
						       spnego_state->mech_types.length,
						       &mech_list_mic);
			if (!NT_STATUS_IS_OK(nt_status)) {
				DEBUG(2,("GENSEC SPNEGO: failed to sign mechListMIC: %s\n",
					nt_errstr(nt_status)));
			}
		}

		nt_status = gensec_spnego_server_negTokenTarg(gensec_security,
							      spnego_state,
							      out_mem_ctx, 
							      nt_status,
							      unwrapped_out,
							      mech_list_mic,
							      out);
		
		spnego_free_data(&spnego);
		
		return nt_status;
	}
	case SPNEGO_CLIENT_TARG:
	{
		NTSTATUS nt_status;
		if (!in.length) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		len = spnego_read_data(gensec_security, in, &spnego);
		
		if (len == -1) {
			DEBUG(1, ("Invalid SPNEGO request:\n"));
			dump_data(1, in.data, in.length);
			return NT_STATUS_INVALID_PARAMETER;
		}
		
		/* OK, so it's real SPNEGO, check the packet's the one we expect */
		if (spnego.type != spnego_state->expected_packet) {
			DEBUG(1, ("Invalid SPNEGO request: %d, expected %d\n", spnego.type, 
				  spnego_state->expected_packet));
			dump_data(1, in.data, in.length);
			spnego_free_data(&spnego);
			return NT_STATUS_INVALID_PARAMETER;
		}
	
		if (spnego.negTokenTarg.negResult == SPNEGO_REJECT) {
			spnego_free_data(&spnego);
			return NT_STATUS_ACCESS_DENIED;
		}

		/* Server didn't like our choice of mech, and chose something else */
		if ((spnego.negTokenTarg.negResult == SPNEGO_ACCEPT_INCOMPLETE) &&
		    spnego.negTokenTarg.supportedMech &&
		    strcmp(spnego.negTokenTarg.supportedMech, spnego_state->neg_oid) != 0) {
			DEBUG(3,("GENSEC SPNEGO: client preferred mech (%s) not accepted, server wants: %s\n",
				 gensec_get_name_by_oid(gensec_security, spnego.negTokenTarg.supportedMech), 
				 gensec_get_name_by_oid(gensec_security, spnego_state->neg_oid)));
			
			talloc_free(spnego_state->sub_sec_security);
			nt_status = gensec_subcontext_start(spnego_state,
							    gensec_security,
							    &spnego_state->sub_sec_security);
			if (!NT_STATUS_IS_OK(nt_status)) {
				spnego_free_data(&spnego);
				return nt_status;
			}
			/* select the sub context */
			nt_status = gensec_start_mech_by_oid(spnego_state->sub_sec_security,
							     spnego.negTokenTarg.supportedMech);
			if (!NT_STATUS_IS_OK(nt_status)) {
				spnego_free_data(&spnego);
				return nt_status;
			}

			nt_status = gensec_update(spnego_state->sub_sec_security,
						  out_mem_ctx, 
						  spnego.negTokenTarg.responseToken,
						  &unwrapped_out);
			spnego_state->neg_oid = talloc_strdup(spnego_state, spnego.negTokenTarg.supportedMech);
		} else if (spnego_state->no_response_expected) {
			if (spnego.negTokenTarg.negResult != SPNEGO_ACCEPT_COMPLETED) {
				DEBUG(3,("GENSEC SPNEGO: client GENSEC accepted, but server rejected (bad password?)\n"));
				nt_status = NT_STATUS_INVALID_PARAMETER;
			} else if (spnego.negTokenTarg.responseToken.length) {
				DEBUG(2,("GENSEC SPNEGO: client GENSEC accepted, but server continued negotiation!\n"));
				nt_status = NT_STATUS_INVALID_PARAMETER;
			} else {
				nt_status = NT_STATUS_OK;
			}
			if (NT_STATUS_IS_OK(nt_status) && spnego.negTokenTarg.mechListMIC.length > 0) {
				nt_status = gensec_check_packet(spnego_state->sub_sec_security,
								out_mem_ctx,
								spnego_state->mech_types.data,
								spnego_state->mech_types.length,
								spnego_state->mech_types.data,
								spnego_state->mech_types.length,
								&spnego.negTokenTarg.mechListMIC);
				if (!NT_STATUS_IS_OK(nt_status)) {
					DEBUG(2,("GENSEC SPNEGO: failed to verify mechListMIC: %s\n",
						nt_errstr(nt_status)));
				}
			}
		} else {
			bool new_spnego = false;

			nt_status = gensec_update(spnego_state->sub_sec_security,
						  out_mem_ctx, 
						  spnego.negTokenTarg.responseToken, 
						  &unwrapped_out);

			if (NT_STATUS_IS_OK(nt_status)
			    && spnego.negTokenTarg.negResult != SPNEGO_ACCEPT_COMPLETED) {
				new_spnego = gensec_have_feature(spnego_state->sub_sec_security,
								 GENSEC_FEATURE_NEW_SPNEGO);
			}
			if (NT_STATUS_IS_OK(nt_status) && new_spnego) {
				nt_status = gensec_sign_packet(spnego_state->sub_sec_security,
							       out_mem_ctx,
							       spnego_state->mech_types.data,
							       spnego_state->mech_types.length,
							       spnego_state->mech_types.data,
							       spnego_state->mech_types.length,
							       &mech_list_mic);
				if (!NT_STATUS_IS_OK(nt_status)) {
					DEBUG(2,("GENSEC SPNEGO: failed to sign mechListMIC: %s\n",
						nt_errstr(nt_status)));
				}
			}
			if (NT_STATUS_IS_OK(nt_status)) {
				spnego_state->no_response_expected = true;
			}
		} 
		
		spnego_free_data(&spnego);

		if (!NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)
			&& !NT_STATUS_IS_OK(nt_status)) {
			DEBUG(1, ("SPNEGO(%s) login failed: %s\n", 
				  spnego_state->sub_sec_security->ops->name, 
				  nt_errstr(nt_status)));
			return nt_status;
		}

		if (unwrapped_out.length || mech_list_mic.length) {
			/* compose reply */
			spnego_out.type = SPNEGO_NEG_TOKEN_TARG;
			spnego_out.negTokenTarg.negResult = SPNEGO_NONE_RESULT;
			spnego_out.negTokenTarg.supportedMech = NULL;
			spnego_out.negTokenTarg.responseToken = unwrapped_out;
			spnego_out.negTokenTarg.mechListMIC = mech_list_mic;
			
			if (spnego_write_data(out_mem_ctx, out, &spnego_out) == -1) {
				DEBUG(1, ("Failed to write SPNEGO reply to NEG_TOKEN_TARG\n"));
				return NT_STATUS_INVALID_PARAMETER;
			}
		
			spnego_state->state_position = SPNEGO_CLIENT_TARG;
			nt_status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		} else {

			/* all done - server has accepted, and we agree */
			*out = null_data_blob;

			if (spnego.negTokenTarg.negResult != SPNEGO_ACCEPT_COMPLETED) {
				/* unless of course it did not accept */
				DEBUG(1,("gensec_update ok but not accepted\n"));
				nt_status = NT_STATUS_INVALID_PARAMETER;
			}
		
			spnego_state->state_position = SPNEGO_DONE;
		}

		return nt_status;
	}
	case SPNEGO_DONE:
		/* We should not be called after we are 'done' */
		return NT_STATUS_INVALID_PARAMETER;
	}
	return NT_STATUS_INVALID_PARAMETER;
}

static void gensec_spnego_want_feature(struct gensec_security *gensec_security,
				       uint32_t feature)
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;

	if (!spnego_state || !spnego_state->sub_sec_security) {
		gensec_security->want_features |= feature;
		return;
	}

	gensec_want_feature(spnego_state->sub_sec_security,
			    feature);
}

static bool gensec_spnego_have_feature(struct gensec_security *gensec_security,
				       uint32_t feature) 
{
	struct spnego_state *spnego_state = (struct spnego_state *)gensec_security->private_data;
	if (!spnego_state->sub_sec_security) {
		return false;
	}
	
	return gensec_have_feature(spnego_state->sub_sec_security, 
				   feature);
}

static const char *gensec_spnego_oids[] = { 
	GENSEC_OID_SPNEGO,
	NULL 
};

static const struct gensec_security_ops gensec_spnego_security_ops = {
	.name		  = "spnego",
	.sasl_name	  = "GSS-SPNEGO",
	.auth_type	  = DCERPC_AUTH_TYPE_SPNEGO,
	.oid              = gensec_spnego_oids,
	.client_start     = gensec_spnego_client_start,
	.server_start     = gensec_spnego_server_start,
	.update 	  = gensec_spnego_update,
	.seal_packet	  = gensec_spnego_seal_packet,
	.sign_packet	  = gensec_spnego_sign_packet,
	.sig_size	  = gensec_spnego_sig_size,
	.max_wrapped_size = gensec_spnego_max_wrapped_size,
	.max_input_size	  = gensec_spnego_max_input_size,
	.check_packet	  = gensec_spnego_check_packet,
	.unseal_packet	  = gensec_spnego_unseal_packet,
	.packet_full_request = gensec_spnego_packet_full_request,
	.wrap             = gensec_spnego_wrap,
	.unwrap           = gensec_spnego_unwrap,
	.wrap_packets     = gensec_spnego_wrap_packets,
	.unwrap_packets   = gensec_spnego_unwrap_packets,
	.session_key	  = gensec_spnego_session_key,
	.session_info     = gensec_spnego_session_info,
	.want_feature     = gensec_spnego_want_feature,
	.have_feature     = gensec_spnego_have_feature,
	.enabled          = true,
	.priority         = GENSEC_SPNEGO
};

_PUBLIC_ NTSTATUS gensec_spnego_init(void)
{
	NTSTATUS ret;
	ret = gensec_register(&gensec_spnego_security_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register '%s' gensec backend!\n",
			gensec_spnego_security_ops.name));
		return ret;
	}

	return ret;
}
