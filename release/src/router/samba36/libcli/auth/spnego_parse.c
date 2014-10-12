/*
   Unix SMB/CIFS implementation.

   RFC2478 Compliant SPNEGO implementation

   Copyright (C) Jim McDonough <jmcd@us.ibm.com>   2003

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
#include "../lib/util/asn1.h"

static bool read_negTokenInit(struct asn1_data *asn1, TALLOC_CTX *mem_ctx,
			      struct spnego_negTokenInit *token)
{
	ZERO_STRUCTP(token);

	asn1_start_tag(asn1, ASN1_CONTEXT(0));
	asn1_start_tag(asn1, ASN1_SEQUENCE(0));

	while (!asn1->has_error && 0 < asn1_tag_remaining(asn1)) {
		int i;
		uint8_t context;
		if (!asn1_peek_uint8(asn1, &context)) {
			asn1->has_error = true;
			break;
		}

		switch (context) {
		/* Read mechTypes */
		case ASN1_CONTEXT(0):
			asn1_start_tag(asn1, ASN1_CONTEXT(0));
			asn1_start_tag(asn1, ASN1_SEQUENCE(0));

			token->mechTypes = talloc(NULL, const char *);
			for (i = 0; !asn1->has_error &&
				     0 < asn1_tag_remaining(asn1); i++) {
				char *oid;
				token->mechTypes = talloc_realloc(NULL,
								  token->mechTypes,
								  const char *, i+2);
				asn1_read_OID(asn1, token->mechTypes, &oid);
				token->mechTypes[i] = oid;
			}
			token->mechTypes[i] = NULL;

			asn1_end_tag(asn1);
			asn1_end_tag(asn1);
			break;
		/* Read reqFlags */
		case ASN1_CONTEXT(1):
			asn1_start_tag(asn1, ASN1_CONTEXT(1));
			asn1_read_BitString(asn1, mem_ctx, &token->reqFlags,
					    &token->reqFlagsPadding);
			asn1_end_tag(asn1);
			break;
                /* Read mechToken */
		case ASN1_CONTEXT(2):
			asn1_start_tag(asn1, ASN1_CONTEXT(2));
			asn1_read_OctetString(asn1, mem_ctx, &token->mechToken);
			asn1_end_tag(asn1);
			break;
		/* Read mecListMIC */
		case ASN1_CONTEXT(3):
		{
			uint8_t type_peek;
			asn1_start_tag(asn1, ASN1_CONTEXT(3));
			if (!asn1_peek_uint8(asn1, &type_peek)) {
				asn1->has_error = true;
				break;
			}
			if (type_peek == ASN1_OCTET_STRING) {
				asn1_read_OctetString(asn1, mem_ctx,
						      &token->mechListMIC);
			} else {
				/* RFC 2478 says we have an Octet String here,
				   but W2k sends something different... */
				char *mechListMIC;
				asn1_start_tag(asn1, ASN1_SEQUENCE(0));
				asn1_start_tag(asn1, ASN1_CONTEXT(0));
				asn1_read_GeneralString(asn1, mem_ctx, &mechListMIC);
				asn1_end_tag(asn1);
				asn1_end_tag(asn1);

				token->targetPrincipal = mechListMIC;
			}
			asn1_end_tag(asn1);
			break;
		}
		default:
			asn1->has_error = true;
			break;
		}
	}

	asn1_end_tag(asn1);
	asn1_end_tag(asn1);

	return !asn1->has_error;
}

static bool write_negTokenInit(struct asn1_data *asn1, struct spnego_negTokenInit *token)
{
	asn1_push_tag(asn1, ASN1_CONTEXT(0));
	asn1_push_tag(asn1, ASN1_SEQUENCE(0));

	/* Write mechTypes */
	if (token->mechTypes && *token->mechTypes) {
		int i;

		asn1_push_tag(asn1, ASN1_CONTEXT(0));
		asn1_push_tag(asn1, ASN1_SEQUENCE(0));
		for (i = 0; token->mechTypes[i]; i++) {
			asn1_write_OID(asn1, token->mechTypes[i]);
		}
		asn1_pop_tag(asn1);
		asn1_pop_tag(asn1);
	}

	/* write reqFlags */
	if (token->reqFlags.length > 0) {
		asn1_push_tag(asn1, ASN1_CONTEXT(1));
		asn1_write_BitString(asn1, token->reqFlags.data,
				     token->reqFlags.length,
				     token->reqFlagsPadding);
		asn1_pop_tag(asn1);
	}

	/* write mechToken */
	if (token->mechToken.data) {
		asn1_push_tag(asn1, ASN1_CONTEXT(2));
		asn1_write_OctetString(asn1, token->mechToken.data,
				       token->mechToken.length);
		asn1_pop_tag(asn1);
	}

	/* write mechListMIC */
	if (token->mechListMIC.data) {
		asn1_push_tag(asn1, ASN1_CONTEXT(3));
#if 0
		/* This is what RFC 2478 says ... */
		asn1_write_OctetString(asn1, token->mechListMIC.data,
				       token->mechListMIC.length);
#else
		/* ... but unfortunately this is what Windows
		   sends/expects */
		asn1_push_tag(asn1, ASN1_SEQUENCE(0));
		asn1_push_tag(asn1, ASN1_CONTEXT(0));
		asn1_push_tag(asn1, ASN1_GENERAL_STRING);
		asn1_write(asn1, token->mechListMIC.data,
			   token->mechListMIC.length);
		asn1_pop_tag(asn1);
		asn1_pop_tag(asn1);
		asn1_pop_tag(asn1);
#endif
		asn1_pop_tag(asn1);
	}

	asn1_pop_tag(asn1);
	asn1_pop_tag(asn1);

	return !asn1->has_error;
}

static bool read_negTokenTarg(struct asn1_data *asn1, TALLOC_CTX *mem_ctx,
			      struct spnego_negTokenTarg *token)
{
	ZERO_STRUCTP(token);

	asn1_start_tag(asn1, ASN1_CONTEXT(1));
	asn1_start_tag(asn1, ASN1_SEQUENCE(0));

	while (!asn1->has_error && 0 < asn1_tag_remaining(asn1)) {
		uint8_t context;
		char *oid;
		if (!asn1_peek_uint8(asn1, &context)) {
			asn1->has_error = true;
			break;
		}

		switch (context) {
		case ASN1_CONTEXT(0):
			asn1_start_tag(asn1, ASN1_CONTEXT(0));
			asn1_start_tag(asn1, ASN1_ENUMERATED);
			asn1_read_uint8(asn1, &token->negResult);
			asn1_end_tag(asn1);
			asn1_end_tag(asn1);
			break;
		case ASN1_CONTEXT(1):
			asn1_start_tag(asn1, ASN1_CONTEXT(1));
			asn1_read_OID(asn1, mem_ctx, &oid);
			token->supportedMech = oid;
			asn1_end_tag(asn1);
			break;
		case ASN1_CONTEXT(2):
			asn1_start_tag(asn1, ASN1_CONTEXT(2));
			asn1_read_OctetString(asn1, mem_ctx, &token->responseToken);
			asn1_end_tag(asn1);
			break;
		case ASN1_CONTEXT(3):
			asn1_start_tag(asn1, ASN1_CONTEXT(3));
			asn1_read_OctetString(asn1, mem_ctx, &token->mechListMIC);
			asn1_end_tag(asn1);
			break;
		default:
			asn1->has_error = true;
			break;
		}
	}

	asn1_end_tag(asn1);
	asn1_end_tag(asn1);

	return !asn1->has_error;
}

static bool write_negTokenTarg(struct asn1_data *asn1, struct spnego_negTokenTarg *token)
{
	asn1_push_tag(asn1, ASN1_CONTEXT(1));
	asn1_push_tag(asn1, ASN1_SEQUENCE(0));

	if (token->negResult != SPNEGO_NONE_RESULT) {
		asn1_push_tag(asn1, ASN1_CONTEXT(0));
		asn1_write_enumerated(asn1, token->negResult);
		asn1_pop_tag(asn1);
	}

	if (token->supportedMech) {
		asn1_push_tag(asn1, ASN1_CONTEXT(1));
		asn1_write_OID(asn1, token->supportedMech);
		asn1_pop_tag(asn1);
	}

	if (token->responseToken.data) {
		asn1_push_tag(asn1, ASN1_CONTEXT(2));
		asn1_write_OctetString(asn1, token->responseToken.data,
				       token->responseToken.length);
		asn1_pop_tag(asn1);
	}

	if (token->mechListMIC.data) {
		asn1_push_tag(asn1, ASN1_CONTEXT(3));
		asn1_write_OctetString(asn1, token->mechListMIC.data,
				      token->mechListMIC.length);
		asn1_pop_tag(asn1);
	}

	asn1_pop_tag(asn1);
	asn1_pop_tag(asn1);

	return !asn1->has_error;
}

ssize_t spnego_read_data(TALLOC_CTX *mem_ctx, DATA_BLOB data, struct spnego_data *token)
{
	struct asn1_data *asn1;
	ssize_t ret = -1;
	uint8_t context;

	ZERO_STRUCTP(token);

	if (data.length == 0) {
		return ret;
	}

	asn1 = asn1_init(mem_ctx);
	if (asn1 == NULL) {
		return -1;
	}

	asn1_load(asn1, data);

	if (!asn1_peek_uint8(asn1, &context)) {
		asn1->has_error = true;
	} else {
		switch (context) {
		case ASN1_APPLICATION(0):
			asn1_start_tag(asn1, ASN1_APPLICATION(0));
			asn1_check_OID(asn1, OID_SPNEGO);
			if (read_negTokenInit(asn1, mem_ctx, &token->negTokenInit)) {
				token->type = SPNEGO_NEG_TOKEN_INIT;
			}
			asn1_end_tag(asn1);
			break;
		case ASN1_CONTEXT(1):
			if (read_negTokenTarg(asn1, mem_ctx, &token->negTokenTarg)) {
				token->type = SPNEGO_NEG_TOKEN_TARG;
			}
			break;
		default:
			asn1->has_error = true;
			break;
		}
	}

	if (!asn1->has_error) ret = asn1->ofs;
	asn1_free(asn1);

	return ret;
}

ssize_t spnego_write_data(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, struct spnego_data *spnego)
{
	struct asn1_data *asn1 = asn1_init(mem_ctx);
	ssize_t ret = -1;

	if (asn1 == NULL) {
		return -1;
	}

	switch (spnego->type) {
	case SPNEGO_NEG_TOKEN_INIT:
		asn1_push_tag(asn1, ASN1_APPLICATION(0));
		asn1_write_OID(asn1, OID_SPNEGO);
		write_negTokenInit(asn1, &spnego->negTokenInit);
		asn1_pop_tag(asn1);
		break;
	case SPNEGO_NEG_TOKEN_TARG:
		write_negTokenTarg(asn1, &spnego->negTokenTarg);
		break;
	default:
		asn1->has_error = true;
		break;
	}

	if (!asn1->has_error) {
		*blob = data_blob_talloc(mem_ctx, asn1->data, asn1->length);
		ret = asn1->ofs;
	}
	asn1_free(asn1);

	return ret;
}

bool spnego_free_data(struct spnego_data *spnego)
{
	bool ret = true;

	if (!spnego) goto out;

	switch(spnego->type) {
	case SPNEGO_NEG_TOKEN_INIT:
		if (spnego->negTokenInit.mechTypes) {
			talloc_free(spnego->negTokenInit.mechTypes);
		}
		data_blob_free(&spnego->negTokenInit.reqFlags);
		data_blob_free(&spnego->negTokenInit.mechToken);
		data_blob_free(&spnego->negTokenInit.mechListMIC);
		talloc_free(spnego->negTokenInit.targetPrincipal);
		break;
	case SPNEGO_NEG_TOKEN_TARG:
		if (spnego->negTokenTarg.supportedMech) {
			talloc_free(discard_const(spnego->negTokenTarg.supportedMech));
		}
		data_blob_free(&spnego->negTokenTarg.responseToken);
		data_blob_free(&spnego->negTokenTarg.mechListMIC);
		break;
	default:
		ret = false;
		break;
	}
	ZERO_STRUCTP(spnego);
out:
	return ret;
}

bool spnego_write_mech_types(TALLOC_CTX *mem_ctx,
			     const char **mech_types,
			     DATA_BLOB *blob)
{
	struct asn1_data *asn1 = asn1_init(mem_ctx);

	if (asn1 == NULL) {
		return false;
	}

	/* Write mechTypes */
	if (mech_types && *mech_types) {
		int i;

		asn1_push_tag(asn1, ASN1_SEQUENCE(0));
		for (i = 0; mech_types[i]; i++) {
			asn1_write_OID(asn1, mech_types[i]);
		}
		asn1_pop_tag(asn1);
	}

	if (asn1->has_error) {
		asn1_free(asn1);
		return false;
	}

	*blob = data_blob_talloc(mem_ctx, asn1->data, asn1->length);
	if (blob->length != asn1->length) {
		asn1_free(asn1);
		return false;
	}

	asn1_free(asn1);

	return true;
}
