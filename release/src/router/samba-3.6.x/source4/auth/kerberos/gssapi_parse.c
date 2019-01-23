/* 
   Unix SMB/CIFS implementation.

   simple GSSAPI wrappers

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
#include "../lib/util/asn1.h"
#include "auth/gensec/gensec.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"

/*
  generate a krb5 GSS-API wrapper packet given a ticket
*/
DATA_BLOB gensec_gssapi_gen_krb5_wrap(TALLOC_CTX *mem_ctx, const DATA_BLOB *ticket, const uint8_t tok_id[2])
{
	struct asn1_data *data;
	DATA_BLOB ret;

	data = asn1_init(mem_ctx);
	if (!data || !ticket->data) {
		return data_blob(NULL,0);
	}

	asn1_push_tag(data, ASN1_APPLICATION(0));
	asn1_write_OID(data, GENSEC_OID_KERBEROS5);

	asn1_write(data, tok_id, 2);
	asn1_write(data, ticket->data, ticket->length);
	asn1_pop_tag(data);

	if (data->has_error) {
		DEBUG(1,("Failed to build krb5 wrapper at offset %d\n", (int)data->ofs));
		asn1_free(data);
		return data_blob(NULL,0);
	}

	ret = data_blob_talloc(mem_ctx, data->data, data->length);
	asn1_free(data);

	return ret;
}

/*
  parse a krb5 GSS-API wrapper packet giving a ticket
*/
bool gensec_gssapi_parse_krb5_wrap(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, DATA_BLOB *ticket, uint8_t tok_id[2])
{
	bool ret;
	struct asn1_data *data = asn1_init(mem_ctx);
	int data_remaining;

	if (!data) {
		return false;
	}

	asn1_load(data, *blob);
	asn1_start_tag(data, ASN1_APPLICATION(0));
	asn1_check_OID(data, GENSEC_OID_KERBEROS5);

	data_remaining = asn1_tag_remaining(data);

	if (data_remaining < 3) {
		data->has_error = true;
	} else {
		asn1_read(data, tok_id, 2);
		data_remaining -= 2;
		*ticket = data_blob_talloc(mem_ctx, NULL, data_remaining);
		asn1_read(data, ticket->data, ticket->length);
	}

	asn1_end_tag(data);

	ret = !data->has_error;

	asn1_free(data);

	return ret;
}


/*
  check a GSS-API wrapper packet givin an expected OID
*/
bool gensec_gssapi_check_oid(const DATA_BLOB *blob, const char *oid)
{
	bool ret;
	struct asn1_data *data = asn1_init(NULL);

	if (!data) return false;

	asn1_load(data, *blob);
	asn1_start_tag(data, ASN1_APPLICATION(0));
	asn1_check_OID(data, oid);

	ret = !data->has_error;

	asn1_free(data);

	return ret;
}


