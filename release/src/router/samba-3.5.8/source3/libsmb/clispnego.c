/* 
   Unix SMB/CIFS implementation.
   simple kerberos5/SPNEGO routines
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   Copyright (C) Luke Howard     2003

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

/*
  generate a negTokenInit packet given a GUID, a list of supported
  OIDs (the mechanisms) and a principal name string 
*/
DATA_BLOB spnego_gen_negTokenInit(char guid[16], 
				  const char *OIDs[], 
				  const char *principal)
{
	int i;
	ASN1_DATA *data;
	DATA_BLOB ret;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_write(data, guid, 16);
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

	asn1_push_tag(data, ASN1_CONTEXT(3));
	asn1_push_tag(data, ASN1_SEQUENCE(0));
	asn1_push_tag(data, ASN1_CONTEXT(0));
	asn1_write_GeneralString(data,principal);
	asn1_pop_tag(data);
	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(1,("Failed to build negTokenInit at offset %d\n", (int)data->ofs));
	}

	ret = data_blob(data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  Generate a negTokenInit as used by the client side ... It has a mechType
  (OID), and a mechToken (a security blob) ... 

  Really, we need to break out the NTLMSSP stuff as well, because it could be
  raw in the packets!
*/
DATA_BLOB gen_negTokenInit(const char *OID, DATA_BLOB blob)
{
	ASN1_DATA *data;
	DATA_BLOB ret;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_push_tag(data, ASN1_APPLICATION(0));
	asn1_write_OID(data,OID_SPNEGO);
	asn1_push_tag(data, ASN1_CONTEXT(0));
	asn1_push_tag(data, ASN1_SEQUENCE(0));

	asn1_push_tag(data, ASN1_CONTEXT(0));
	asn1_push_tag(data, ASN1_SEQUENCE(0));
	asn1_write_OID(data, OID);
	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_push_tag(data, ASN1_CONTEXT(2));
	asn1_write_OctetString(data,blob.data,blob.length);
	asn1_pop_tag(data);

	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(1,("Failed to build negTokenInit at offset %d\n", (int)data->ofs));
	}

	ret = data_blob(data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  parse a negTokenInit packet giving a GUID, a list of supported
  OIDs (the mechanisms) and a principal name string 
*/
bool spnego_parse_negTokenInit(DATA_BLOB blob,
			       char *OIDs[ASN1_MAX_OIDS],
			       char **principal)
{
	int i;
	bool ret;
	ASN1_DATA *data;

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
		const char *oid_str = NULL;
		asn1_read_OID(data,talloc_autofree_context(),&oid_str);
		OIDs[i] = CONST_DISCARD(char *, oid_str);
	}
	OIDs[i] = NULL;
	asn1_end_tag(data);
	asn1_end_tag(data);

	*principal = NULL;

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
		/* mechToken [2] OCTET STRING  OPTIONAL */
		DATA_BLOB token;
		asn1_start_tag(data, ASN1_CONTEXT(2));
		asn1_read_OctetString(data, talloc_autofree_context(),
			&token);
		asn1_end_tag(data);
		/* Throw away the token - not used. */
		data_blob_free(&token);
	}

	if (asn1_peek_tag(data, ASN1_CONTEXT(3))) {
		/* mechListMIC [3] OCTET STRING  OPTIONAL */
		asn1_start_tag(data, ASN1_CONTEXT(3));
		asn1_start_tag(data, ASN1_SEQUENCE(0));
		asn1_start_tag(data, ASN1_CONTEXT(0));
		asn1_read_GeneralString(data,talloc_autofree_context(),
			principal);
		asn1_end_tag(data);
		asn1_end_tag(data);
		asn1_end_tag(data);
	}

	asn1_end_tag(data);
	asn1_end_tag(data);

	asn1_end_tag(data);

	ret = !data->has_error;
	if (data->has_error) {
		int j;
		TALLOC_FREE(*principal);
		for(j = 0; j < i && j < ASN1_MAX_OIDS-1; j++) {
			TALLOC_FREE(OIDs[j]);
		}
	}

	asn1_free(data);
	return ret;
}

/*
  generate a negTokenTarg packet given a list of OIDs and a security blob
*/
DATA_BLOB gen_negTokenTarg(const char *OIDs[], DATA_BLOB blob)
{
	int i;
	ASN1_DATA *data;
	DATA_BLOB ret;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return data_blob_null;
	}

	asn1_push_tag(data, ASN1_APPLICATION(0));
	asn1_write_OID(data,OID_SPNEGO);
	asn1_push_tag(data, ASN1_CONTEXT(0));
	asn1_push_tag(data, ASN1_SEQUENCE(0));

	asn1_push_tag(data, ASN1_CONTEXT(0));
	asn1_push_tag(data, ASN1_SEQUENCE(0));
	for (i=0; OIDs[i]; i++) {
		asn1_write_OID(data,OIDs[i]);
	}
	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_push_tag(data, ASN1_CONTEXT(2));
	asn1_write_OctetString(data,blob.data,blob.length);
	asn1_pop_tag(data);

	asn1_pop_tag(data);
	asn1_pop_tag(data);

	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(1,("Failed to build negTokenTarg at offset %d\n", (int)data->ofs));
	}

	ret = data_blob(data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  parse a negTokenTarg packet giving a list of OIDs and a security blob
*/
bool parse_negTokenTarg(DATA_BLOB blob, char *OIDs[ASN1_MAX_OIDS], DATA_BLOB *secblob)
{
	int i;
	ASN1_DATA *data;

	data = asn1_init(talloc_tos());
	if (data == NULL) {
		return false;
	}

	asn1_load(data, blob);
	asn1_start_tag(data, ASN1_APPLICATION(0));
	asn1_check_OID(data,OID_SPNEGO);
	asn1_start_tag(data, ASN1_CONTEXT(0));
	asn1_start_tag(data, ASN1_SEQUENCE(0));

	asn1_start_tag(data, ASN1_CONTEXT(0));
	asn1_start_tag(data, ASN1_SEQUENCE(0));
	for (i=0; asn1_tag_remaining(data) > 0 && i < ASN1_MAX_OIDS-1; i++) {
		const char *oid_str = NULL;
		asn1_read_OID(data,talloc_autofree_context(),&oid_str);
		OIDs[i] = CONST_DISCARD(char *, oid_str);
	}
	OIDs[i] = NULL;
	asn1_end_tag(data);
	asn1_end_tag(data);

	/* Skip any optional req_flags that are sent per RFC 4178 */
	if (asn1_peek_tag(data, ASN1_CONTEXT(1))) {
		uint8 flags;

		asn1_start_tag(data, ASN1_CONTEXT(1));
		asn1_start_tag(data, ASN1_BIT_STRING);
		while (asn1_tag_remaining(data) > 0)
			asn1_read_uint8(data, &flags);
		asn1_end_tag(data);
		asn1_end_tag(data);
	}

	asn1_start_tag(data, ASN1_CONTEXT(2));
	asn1_read_OctetString(data,talloc_autofree_context(),secblob);
	asn1_end_tag(data);

	asn1_end_tag(data);
	asn1_end_tag(data);

	asn1_end_tag(data);

	if (data->has_error) {
		int j;
		data_blob_free(secblob);
		for(j = 0; j < i && j < ASN1_MAX_OIDS-1; j++) {
			TALLOC_FREE(OIDs[j]);
		}
		DEBUG(1,("Failed to parse negTokenTarg at offset %d\n", (int)data->ofs));
		asn1_free(data);
		return False;
	}

	asn1_free(data);
	return True;
}

/*
  generate a krb5 GSS-API wrapper packet given a ticket
*/
DATA_BLOB spnego_gen_krb5_wrap(const DATA_BLOB ticket, const uint8 tok_id[2])
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

	ret = data_blob(data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  parse a krb5 GSS-API wrapper packet giving a ticket
*/
bool spnego_parse_krb5_wrap(DATA_BLOB blob, DATA_BLOB *ticket, uint8 tok_id[2])
{
	bool ret;
	ASN1_DATA *data;
	int data_remaining;

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
		*ticket = data_blob(NULL, data_remaining);
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
   generate a SPNEGO negTokenTarg packet, ready for a EXTENDED_SECURITY
   kerberos session setup 
*/
int spnego_gen_negTokenTarg(const char *principal, int time_offset, 
			    DATA_BLOB *targ, 
			    DATA_BLOB *session_key_krb5, uint32 extra_ap_opts,
			    time_t *expire_time)
{
	int retval;
	DATA_BLOB tkt, tkt_wrapped;
	const char *krb_mechs[] = {OID_KERBEROS5_OLD, OID_KERBEROS5, OID_NTLMSSP, NULL};

	/* get a kerberos ticket for the service and extract the session key */
	retval = cli_krb5_get_ticket(principal, time_offset,
					&tkt, session_key_krb5, extra_ap_opts, NULL, 
					expire_time, NULL);

	if (retval)
		return retval;

	/* wrap that up in a nice GSS-API wrapping */
	tkt_wrapped = spnego_gen_krb5_wrap(tkt, TOK_ID_KRB_AP_REQ);

	/* and wrap that in a shiny SPNEGO wrapper */
	*targ = gen_negTokenTarg(krb_mechs, tkt_wrapped);

	data_blob_free(&tkt_wrapped);
	data_blob_free(&tkt);

	return retval;
}


/*
  parse a spnego NTLMSSP challenge packet giving two security blobs
*/
bool spnego_parse_challenge(const DATA_BLOB blob,
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
	asn1_read_OctetString(data, talloc_autofree_context(), chal1);
	asn1_end_tag(data);

	/* the second challenge is optional (XP doesn't send it) */
	if (asn1_tag_remaining(data)) {
		asn1_start_tag(data,ASN1_CONTEXT(3));
		asn1_read_OctetString(data, talloc_autofree_context(), chal2);
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
DATA_BLOB spnego_gen_auth(DATA_BLOB blob)
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

	ret = data_blob(data->data, data->length);

	asn1_free(data);

	return ret;
}

/*
 parse a SPNEGO auth packet. This contains the encrypted passwords
*/
bool spnego_parse_auth(DATA_BLOB blob, DATA_BLOB *auth)
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

	*auth = data_blob_talloc(talloc_tos(),
				 token.negTokenTarg.responseToken.data,
				 token.negTokenTarg.responseToken.length);
	spnego_free_data(&token);

	return true;
}

/*
  generate a minimal SPNEGO response packet.  Doesn't contain much.
*/
DATA_BLOB spnego_gen_auth_response(DATA_BLOB *reply, NTSTATUS nt_status,
				   const char *mechOID)
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

	asn1_pop_tag(data);
	asn1_pop_tag(data);

	ret = data_blob(data->data, data->length);
	asn1_free(data);
	return ret;
}

/*
 parse a SPNEGO auth packet. This contains the encrypted passwords
*/
bool spnego_parse_auth_response(DATA_BLOB blob, NTSTATUS nt_status,
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
			asn1_read_OctetString(data, talloc_autofree_context(), auth);
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
		asn1_read_OctetString(data, talloc_autofree_context(), &mechList);
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
