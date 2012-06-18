/* 
   Unix SMB/CIFS Implementation.
   LDAP protocol helper functions for SAMBA
   Copyright (C) Volker Lendecke 2004
    
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

#ifndef _LIBCLI_LDAP_MESSAGE_H_
#define _LIBCLI_LDAP_MESSAGE_H_

#include "../libcli/ldap/ldap_errors.h"
#include "lib/ldb/include/ldb.h"

enum ldap_request_tag {
	LDAP_TAG_BindRequest = 0,
	LDAP_TAG_BindResponse = 1,
	LDAP_TAG_UnbindRequest = 2,
	LDAP_TAG_SearchRequest = 3,
	LDAP_TAG_SearchResultEntry = 4,
	LDAP_TAG_SearchResultDone = 5,
	LDAP_TAG_ModifyRequest = 6,
	LDAP_TAG_ModifyResponse = 7,
	LDAP_TAG_AddRequest = 8,
	LDAP_TAG_AddResponse = 9,
	LDAP_TAG_DelRequest = 10,
	LDAP_TAG_DelResponse = 11,
	LDAP_TAG_ModifyDNRequest = 12,
	LDAP_TAG_ModifyDNResponse = 13,
	LDAP_TAG_CompareRequest = 14,
	LDAP_TAG_CompareResponse = 15,
	LDAP_TAG_AbandonRequest = 16,
	LDAP_TAG_SearchResultReference = 19,
	LDAP_TAG_ExtendedRequest = 23,
	LDAP_TAG_ExtendedResponse = 24
};

enum ldap_auth_mechanism {
	LDAP_AUTH_MECH_SIMPLE = 0,
	LDAP_AUTH_MECH_SASL = 3
};

struct ldap_Result {
	int resultcode;
	const char *dn;
	const char *errormessage;
	const char *referral;
};

struct ldap_BindRequest {
	int version;
	const char *dn;
	enum ldap_auth_mechanism mechanism;
	union {
		const char *password;
		struct {
			const char *mechanism;
			DATA_BLOB *secblob;/* optional */
		} SASL;
	} creds;
};

struct ldap_BindResponse {
	struct ldap_Result response;
	union {
		DATA_BLOB *secblob;/* optional */
	} SASL;
};

struct ldap_UnbindRequest {
	uint8_t __dummy;
};

enum ldap_scope {
	LDAP_SEARCH_SCOPE_BASE = 0,
	LDAP_SEARCH_SCOPE_SINGLE = 1,
	LDAP_SEARCH_SCOPE_SUB = 2
};

enum ldap_deref {
	LDAP_DEREFERENCE_NEVER = 0,
	LDAP_DEREFERENCE_IN_SEARCHING = 1,
	LDAP_DEREFERENCE_FINDING_BASE = 2,
	LDAP_DEREFERENCE_ALWAYS
};

struct ldap_SearchRequest {
	const char *basedn;
	enum ldap_scope scope;
	enum ldap_deref deref;
	uint32_t timelimit;
	uint32_t sizelimit;
	bool attributesonly;
	struct ldb_parse_tree *tree;
	int num_attributes;
	const char * const *attributes;
};

struct ldap_SearchResEntry {
	const char *dn;
	int num_attributes;
	struct ldb_message_element *attributes;
};

struct ldap_SearchResRef {
	const char *referral;
};

enum ldap_modify_type {
	LDAP_MODIFY_NONE = -1,
	LDAP_MODIFY_ADD = 0,
	LDAP_MODIFY_DELETE = 1,
	LDAP_MODIFY_REPLACE = 2
};

struct ldap_mod {
	enum ldap_modify_type type;
	struct ldb_message_element attrib;
};

struct ldap_ModifyRequest {
	const char *dn;
	int num_mods;
	struct ldap_mod *mods;
};

struct ldap_AddRequest {
	const char *dn;
	int num_attributes;
	struct ldb_message_element *attributes;
};

struct ldap_DelRequest {
	const char *dn;
};

struct ldap_ModifyDNRequest {
	const char *dn;
	const char *newrdn;
	bool deleteolddn;
	const char *newsuperior;/* optional */
};

struct ldap_CompareRequest {
	const char *dn;
	const char *attribute;
	DATA_BLOB value;
};

struct ldap_AbandonRequest {
	int messageid;
};

struct ldap_ExtendedRequest {
	const char *oid;
	DATA_BLOB *value;/* optional */
};

struct ldap_ExtendedResponse {
	struct ldap_Result response;
	const char *oid;/* optional */
	DATA_BLOB *value;/* optional */
};

union ldap_Request {
	struct ldap_Result 		GeneralResult;
	struct ldap_BindRequest 	BindRequest;
	struct ldap_BindResponse 	BindResponse;
	struct ldap_UnbindRequest 	UnbindRequest;
	struct ldap_SearchRequest 	SearchRequest;
	struct ldap_SearchResEntry 	SearchResultEntry;
	struct ldap_Result 		SearchResultDone;
	struct ldap_SearchResRef 	SearchResultReference;
	struct ldap_ModifyRequest 	ModifyRequest;
	struct ldap_Result 		ModifyResponse;
	struct ldap_AddRequest 		AddRequest;
	struct ldap_Result 		AddResponse;
	struct ldap_DelRequest 		DelRequest;
	struct ldap_Result 		DelResponse;
	struct ldap_ModifyDNRequest 	ModifyDNRequest;
	struct ldap_Result 		ModifyDNResponse;
	struct ldap_CompareRequest 	CompareRequest;
	struct ldap_Result 		CompareResponse;
	struct ldap_AbandonRequest 	AbandonRequest;
	struct ldap_ExtendedRequest 	ExtendedRequest;
	struct ldap_ExtendedResponse 	ExtendedResponse;
};


struct ldap_message {
	int                     messageid;
	enum ldap_request_tag   type;
	union ldap_Request      r;
	struct ldb_control    **controls;
	bool                   *controls_decoded;
};

struct ldap_control_handler {
	const char *oid;
	bool (*decode)(void *mem_ctx, DATA_BLOB in, void *_out);
	bool (*encode)(void *mem_ctx, void *in, DATA_BLOB *out);
};

struct asn1_data;

struct ldap_message *new_ldap_message(TALLOC_CTX *mem_ctx);
NTSTATUS ldap_decode(struct asn1_data *data,
		     const struct ldap_control_handler *control_handlers,
		     struct ldap_message *msg);
bool ldap_encode(struct ldap_message *msg,
		 const struct ldap_control_handler *control_handlers,
		 DATA_BLOB *result, TALLOC_CTX *mem_ctx);
NTSTATUS ldap_full_packet(void *private_data, DATA_BLOB blob, size_t *packet_size);

bool asn1_read_OctetString_talloc(TALLOC_CTX *mem_ctx,
				  struct asn1_data *data,
				  const char **result);

void ldap_decode_attribs_bare(TALLOC_CTX *mem_ctx, struct asn1_data *data,
			      struct ldb_message_element **attributes,
			      int *num_attributes);

#endif 
