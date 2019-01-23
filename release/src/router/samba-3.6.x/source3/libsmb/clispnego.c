/* 
   Unix SMB/CIFS implementation.
   simple kerberos5/SPNEGO routines
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   Copyright (C) Luke Howard     2003
   Copyright (C) Jeremy Allison 2010

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
#include "smb_krb5.h"
#include "../lib/util/asn1.h"

/*
  generate a negTokenInit packet given a list of supported
  OIDs (the mechanisms) a blob, and a principal name string
*/

DATA_BLOB spnego_gen_negTokenInit(TALLOC_CTX *ctx,
				  const char *OIDs[],
				  DATA_BLOB *psecblob,
				  const char *principal)
{
	int i;
	ASN1_DATA *data;
	DATA_BLOB ret;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_push_tag(data,ASN1_APPLICATION(0));
	asn1_write_OID(data,OID_SPNEGO);
	asn1_push_tag(data,ASN1_CONTEXT(0));
	asn1_push_tag(data,ASN1_SEQUENCE(0));

	asn1_push_tag(data,ASN1_CONTEXT(0));
	asn1_push_tag(data,ASN1_SEQUENCE(0));
	for (i=0; OIDs[i]; i++) {
		asn1_write_OID(data,OIDs[i]);
	}
	asn1_pop_tag(data);
	asn1_pop_tag(data);

	if (psecblob && psecblob->length && psecblob->data) {
		asn1_push_tag(data, ASN1_CONTEXT(2));
		asn1_write_OctetString(data,psecblob->data,
			psecblob->length);
		asn1_pop_tag(data);
	}

	if (principal) {
		asn1_push_tag(data, ASN1_CONTEXT(3));
		asn1_push_tag(data, ASN1_SEQUENCE(0));
		asn1_push_tag(data, ASN1_CONTEXT(0));
		asn1_write_GeneralString(data,principal);
		asn1_pop_tag(data);
		asn1_pop_tag(data);
		asn1_pop_tag(data);
	}

	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(1,("Failed to build negTokenInit at offset %d\n", (int)data->ofs));
	}

	ret = data_blob_talloc(ctx, data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  parse a negTokenInit packet giving a GUID, a list of supported
  OIDs (the mechanisms) and a principal name string 
*/
bool spnego_parse_negTokenInit(TALLOC_CTX *ctx,
			       DATA_BLOB blob,
			       char *OIDs[ASN1_MAX_OIDS],
			       char **principal,
			       DATA_BLOB *secblob)
{
	int i;
	bool ret;
	ASN1_DATA *data;

	for (i = 0; i < ASN1_MAX_OIDS; i++) {
		OIDs[i] = NULL;
	}

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_load(data, blob);

	asn1_start_tag(data,ASN1_APPLICATION(0));

	asn1_check_OID(data,OID_SPNEGO);

	/* negTokenInit  [0]  NegTokenInit */
	asn1_start_tag(data,ASN1_CONTEXT(0));
	asn1_start_tag(data,ASN1_SEQUENCE(0));

	/* mechTypes [0] MechTypeList  OPTIONAL */

	/* Not really optional, we depend on this to decide
	 * what mechanisms we have to work with. */

	asn1_start_tag(data,ASN1_CONTEXT(0));
	asn1_start_tag(data,ASN1_SEQUENCE(0));
	for (i=0; asn1_tag_remaining(data) > 0 && i < ASN1_MAX_OIDS-1; i++) {
		asn1_read_OID(data,ctx, &OIDs[i]);
		if (data->has_error) {
			break;
		}
	}
	OIDs[i] = NULL;
	asn1_end_tag(data);
	asn1_end_tag(data);

	if (principal) {
		*principal = NULL;
	}
	if (secblob) {
		*secblob = data_blob_null;
	}

	/*
	  Win7 + Live Sign-in Assistant attaches a mechToken
	  ASN1_CONTEXT(2) to the negTokenInit packet
	  which breaks our negotiation if we just assume
	  the next tag is ASN1_CONTEXT(3).
	*/

	if (asn1_peek_tag(data, ASN1_CONTEXT(1))) {
		uint8 flags;

		/* reqFlags [1] ContextFlags  OPTIONAL */
		asn1_start_tag(data, ASN1_CONTEXT(1));
		asn1_start_tag(data, ASN1_BIT_STRING);
		while (asn1_tag_remaining(data) > 0) {
			asn1_read_uint8(data, &flags);
		}
		asn1_end_tag(data);
		asn1_end_tag(data);
	}

	if (asn1_peek_tag(data, ASN1_CONTEXT(2))) {
		DATA_BLOB sblob = data_blob_null;
		/* mechToken [2] OCTET STRING  OPTIONAL */
		asn1_start_tag(data, ASN1_CONTEXT(2));
		asn1_read_OctetString(data, ctx, &sblob);
		asn1_end_tag(data);
		if (secblob) {
			*secblob = sblob;
		} else {
			data_blob_free(&sblob);
		}
	}

	if (asn1_peek_tag(data, ASN1_CONTEXT(3))) {
		char *princ = NULL;
		/* mechListMIC [3] OCTET STRING  OPTIONAL */
		asn1_start_tag(data, ASN1_CONTEXT(3));
		asn1_start_tag(data, ASN1_SEQUENCE(0));
		asn1_start_tag(data, ASN1_CONTEXT(0));
		asn1_read_GeneralString(data, ctx, &princ);
		asn1_end_tag(data);
		asn1_end_tag(data);
		asn1_end_tag(data);
		if (principal) {
			*principal = princ;
		} else {
			TALLOC_FREE(princ);
		}
	}

	asn1_end_tag(data);
	asn1_end_tag(data);

	asn1_end_tag(data);

	ret = !data->has_error;
	if (data->has_error) {
		int j;
		if (principal) {
			TALLOC_FREE(*principal);
		}
		if (secblob) {
			data_blob_free(secblob);
		}
		for(j = 0; j < i && j < ASN1_MAX_OIDS-1; j++) {
			TALLOC_FREE(OIDs[j]);
		}
	}

	asn1_free(data);
	return ret;
}

/*
  generate a krb5 GSS-API wrapper packet given a ticket
*/
DATA_BLOB spnego_gen_krb5_wrap(TALLOC_CTX *ctx, const DATA_BLOB ticket, const uint8 tok_id[2])
{
	ASN1_DATA *data;
	DATA_BLOB ret;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_push_tag(data, ASN1_APPLICATION(0));
	asn1_write_OID(data, OID_KERBEROS5);

	asn1_write(data, tok_id, 2);
	asn1_write(data, ticket.data, ticket.length);
	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(1,("Failed to build krb5 wrapper at offset %d\n", (int)data->ofs));
	}

	ret = data_blob_talloc(ctx, data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  parse a krb5 GSS-API wrapper packet giving a ticket
*/
bool spnego_parse_krb5_wrap(TALLOC_CTX *ctx, DATA_BLOB blob, DATA_BLOB *ticket, uint8 tok_id[2])
{
	bool ret;
	ASN1_DATA *data;
	int data_remaining;
	*ticket = data_blob_null;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_load(data, blob);
	asn1_start_tag(data, ASN1_APPLICATION(0));
	asn1_check_OID(data, OID_KERBEROS5);

	data_remaining = asn1_tag_remaining(data);

	if (data_remaining < 3) {
		data->has_error = True;
	} else {
		asn1_read(data, tok_id, 2);
		data_remaining -= 2;
		*ticket = data_blob_talloc(ctx, NULL, data_remaining);
		asn1_read(data, ticket->data, ticket->length);
	}

	asn1_end_tag(data);

	ret = !data->has_error;

	if (data->has_error) {
		data_blob_free(ticket);
	}

	asn1_free(data);

	return ret;
}


/* 
   generate a SPNEGO krb5 negTokenInit packet, ready for a EXTENDED_SECURITY
   kerberos session setup
*/
int spnego_gen_krb5_negTokenInit(TALLOC_CTX *ctx,
			    const char *principal, int time_offset,
			    DATA_BLOB *targ,
			    DATA_BLOB *session_key_krb5, uint32 extra_ap_opts,
			    time_t *expire_time)
{
	int retval;
	DATA_BLOB tkt, tkt_wrapped;
	const char *krb_mechs[] = {OID_KERBEROS5_OLD, OID_KERBEROS5, OID_NTLMSSP, NULL};

	/* get a kerberos ticket for the service and extract the session key */
	retval = cli_krb5_get_ticket(ctx, principal, time_offset,
					  &tkt, session_key_krb5,
					  extra_ap_opts, NULL,
					  expire_time, NULL);
	if (retval) {
		return retval;
	}

	/* wrap that up in a nice GSS-API wrapping */
	tkt_wrapped = spnego_gen_krb5_wrap(ctx, tkt, TOK_ID_KRB_AP_REQ);

	/* and wrap that in a shiny SPNEGO wrapper */
	*targ = spnego_gen_negTokenInit(ctx, krb_mechs, &tkt_wrapped, NULL);

	data_blob_free(&tkt_wrapped);
	data_blob_free(&tkt);

	return retval;
}


/*
  parse a spnego NTLMSSP challenge packet giving two security blobs
*/
bool spnego_parse_challenge(TALLOC_CTX *ctx, const DATA_BLOB blob,
			    DATA_BLOB *chal1, DATA_BLOB *chal2)
{
	bool ret;
	ASN1_DATA *data;

	ZERO_STRUCTP(chal1);
	ZERO_STRUCTP(chal2);

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_load(data, blob);
	asn1_start_tag(data,ASN1_CONTEXT(1));
	asn1_start_tag(data,ASN1_SEQUENCE(0));

	asn1_start_tag(data,ASN1_CONTEXT(0));
	asn1_check_enumerated(data,1);
	asn1_end_tag(data);

	asn1_start_tag(data,ASN1_CONTEXT(1));
	asn1_check_OID(data, OID_NTLMSSP);
	asn1_end_tag(data);

	asn1_start_tag(data,ASN1_CONTEXT(2));
	asn1_read_OctetString(data, ctx, chal1);
	asn1_end_tag(data);

	/* the second challenge is optional (XP doesn't send it) */
	if (asn1_tag_remaining(data)) {
		asn1_start_tag(data,ASN1_CONTEXT(3));
		asn1_read_OctetString(data, ctx, chal2);
		asn1_end_tag(data);
	}

	asn1_end_tag(data);
	asn1_end_tag(data);

	ret = !data->has_error;

	if (data->has_error) {
		data_blob_free(chal1);
		data_blob_free(chal2);
	}

	asn1_free(data);
	return ret;
}


/*
 generate a SPNEGO auth packet. This will contain the encrypted passwords
*/
DATA_BLOB spnego_gen_auth(TALLOC_CTX *ctx, DATA_BLOB blob)
{
	ASN1_DATA *data;
	DATA_BLOB ret;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_push_tag(data, ASN1_CONTEXT(1));
	asn1_push_tag(data, ASN1_SEQUENCE(0));
	asn1_push_tag(data, ASN1_CONTEXT(2));
	asn1_write_OctetString(data,blob.data,blob.length);
	asn1_pop_tag(data);
	asn1_pop_tag(data);
	asn1_pop_tag(data);

	ret = data_blob_talloc(ctx, data->data, data->length);

	asn1_free(data);

	return ret;
}

/*
 parse a SPNEGO auth packet. This contains the encrypted passwords
*/
bool spnego_parse_auth_and_mic(TALLOC_CTX *ctx, DATA_BLOB blob,
				DATA_BLOB *auth, DATA_BLOB *signature)
{
	ssize_t len;
	struct spnego_data token;

	len = spnego_read_data(talloc_tos(), blob, &token);
	if (len == -1) {
		DEBUG(3,("spnego_parse_auth: spnego_read_data failed\n"));
		return false;
	}

	if (token.type != SPNEGO_NEG_TOKEN_TARG) {
		DEBUG(3,("spnego_parse_auth: wrong token type: %d\n",
			token.type));
		spnego_free_data(&token);
		return false;
	}

	*auth = data_blob_talloc(ctx,
				 token.negTokenTarg.responseToken.data,
				 token.negTokenTarg.responseToken.length);

	if (!signature) {
		goto done;
	}

	*signature = data_blob_talloc(ctx,
				 token.negTokenTarg.mechListMIC.data,
				 token.negTokenTarg.mechListMIC.length);

done:
	spnego_free_data(&token);

	return true;
}

bool spnego_parse_auth(TALLOC_CTX *ctx, DATA_BLOB blob, DATA_BLOB *auth)
{
	return spnego_parse_auth_and_mic(ctx, blob, auth, NULL);
}

/*
  generate a minimal SPNEGO response packet.  Doesn't contain much.
*/
DATA_BLOB spnego_gen_auth_response_and_mic(TALLOC_CTX *ctx,
					   NTSTATUS nt_status,
					   const char *mechOID,
					   DATA_BLOB *reply,
					   DATA_BLOB *mechlistMIC)
{
	ASN1_DATA *data;
	DATA_BLOB ret;
	uint8 negResult;

	if (NT_STATUS_IS_OK(nt_status)) {
		negResult = SPNEGO_ACCEPT_COMPLETED;
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		negResult = SPNEGO_ACCEPT_INCOMPLETE;
	} else {
		negResult = SPNEGO_REJECT;
	}

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_push_tag(data, ASN1_CONTEXT(1));
	asn1_push_tag(data, ASN1_SEQUENCE(0));
	asn1_push_tag(data, ASN1_CONTEXT(0));
	asn1_write_enumerated(data, negResult);
	asn1_pop_tag(data);

	if (mechOID) {
		asn1_push_tag(data,ASN1_CONTEXT(1));
		asn1_write_OID(data, mechOID);
		asn1_pop_tag(data);
	}

	if (reply && reply->data != NULL) {
		asn1_push_tag(data,ASN1_CONTEXT(2));
		asn1_write_OctetString(data, reply->data, reply->length);
		asn1_pop_tag(data);
	}

	if (mechlistMIC && mechlistMIC->data != NULL) {
		asn1_push_tag(data, ASN1_CONTEXT(3));
		asn1_write_OctetString(data,
					mechlistMIC->data,
					mechlistMIC->length);
		asn1_pop_tag(data);
	}

	asn1_pop_tag(data);
	asn1_pop_tag(data);

	ret = data_blob_talloc(ctx, data->data, data->length);
	asn1_free(data);
	return ret;
}

DATA_BLOB spnego_gen_auth_response(TALLOC_CTX *ctx, DATA_BLOB *reply,
				   NTSTATUS nt_status, const char *mechOID)
{
	return spnego_gen_auth_response_and_mic(ctx, nt_status,
						mechOID, reply, NULL);
}

/*
 parse a SPNEGO auth packet. This contains the encrypted passwords
*/
bool spnego_parse_auth_response(TALLOC_CTX *ctx,
			        DATA_BLOB blob, NTSTATUS nt_status,
				const char *mechOID,
				DATA_BLOB *auth)
{
	ASN1_DATA *data;
	uint8 negResult;

	if (NT_STATUS_IS_OK(nt_status)) {
		negResult = SPNEGO_ACCEPT_COMPLETED;
	} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		negResult = SPNEGO_ACCEPT_INCOMPLETE;
	} else {
		negResult = SPNEGO_REJECT;
	}

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_load(data, blob);
	asn1_start_tag(data, ASN1_CONTEXT(1));
	asn1_start_tag(data, ASN1_SEQUENCE(0));
	asn1_start_tag(data, ASN1_CONTEXT(0));
	asn1_check_enumerated(data, negResult);
	asn1_end_tag(data);

	*auth = data_blob_null;

	if (asn1_tag_remaining(data)) {
		asn1_start_tag(data,ASN1_CONTEXT(1));
		asn1_check_OID(data, mechOID);
		asn1_end_tag(data);

		if (asn1_tag_remaining(data)) {
			asn1_start_tag(data,ASN1_CONTEXT(2));
			asn1_read_OctetString(data, ctx, auth);
			asn1_end_tag(data);
		}
	} else if (negResult == SPNEGO_ACCEPT_INCOMPLETE) {
		data->has_error = 1;
	}

	/* Binding against Win2K DC returns a duplicate of the responseToken in
	 * the optional mechListMIC field. This is a bug in Win2K. We ignore
	 * this field if it exists. Win2K8 may return a proper mechListMIC at
	 * which point we need to implement the integrity checking. */
	if (asn1_tag_remaining(data)) {
		DATA_BLOB mechList = data_blob_null;
		asn1_start_tag(data, ASN1_CONTEXT(3));
		asn1_read_OctetString(data, ctx, &mechList);
		asn1_end_tag(data);
		data_blob_free(&mechList);
		DEBUG(5,("spnego_parse_auth_response received mechListMIC, "
		    "ignoring.\n"));
	}

	asn1_end_tag(data);
	asn1_end_tag(data);

	if (data->has_error) {
		DEBUG(3,("spnego_parse_auth_response failed at %d\n", (int)data->ofs));
		asn1_free(data);
		data_blob_free(auth);
		return False;
	}

	asn1_free(data);
	return True;
}

bool spnego_mech_list_blob(TALLOC_CTX *mem_ctx,
			   char **oid_list, DATA_BLOB *raw_data)
{
	ASN1_DATA *data;
	unsigned int idx;

	if (!oid_list || !oid_list[0] || !raw_data) {
		return false;
	}

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_push_tag(data, ASN1_SEQUENCE(0));
	for (idx = 0; oid_list[idx]; idx++) {
		asn1_write_OID(data, oid_list[idx]);
	}
	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(3, (__location__ " failed at %d\n", (int)data->ofs));
		asn1_free(data);
		return false;
	}

	*raw_data = data_blob_talloc(mem_ctx, data->data, data->length);
	if (!raw_data->data) {
		DEBUG(3, (__location__": data_blob_talloc() failed!\n"));
		asn1_free(data);
		return false;
	}

	asn1_free(data);
	return true;
}
