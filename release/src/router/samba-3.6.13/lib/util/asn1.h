/* 
   Unix SMB/CIFS implementation.   
   simple ASN1 code
   Copyright (C) Andrew Tridgell 2001
   
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

#ifndef _ASN_1_H
#define _ASN_1_H

struct nesting {
	off_t start;
	size_t taglen; /* for parsing */
	struct nesting *next;
};

struct asn1_data {
	uint8_t *data;
	size_t length;
	off_t ofs;
	struct nesting *nesting;
	bool has_error;
};

typedef struct asn1_data ASN1_DATA;

#define ASN1_APPLICATION(x) ((x)+0x60)
#define ASN1_APPLICATION_SIMPLE(x) ((x)+0x40)
#define ASN1_SEQUENCE(x) ((x)+0x30)
#define ASN1_CONTEXT(x) ((x)+0xa0)
#define ASN1_CONTEXT_SIMPLE(x) ((x)+0x80)
#define ASN1_GENERAL_STRING 0x1b
#define ASN1_OCTET_STRING 0x4
#define ASN1_OID 0x6
#define ASN1_BOOLEAN 0x1
#define ASN1_INTEGER 0x2
#define ASN1_BIT_STRING 0x3
#define ASN1_ENUMERATED 0xa
#define ASN1_SET 0x31

#define ASN1_MAX_OIDS 20

struct asn1_data *asn1_init(TALLOC_CTX *mem_ctx);
void asn1_free(struct asn1_data *data);
bool asn1_write(struct asn1_data *data, const void *p, int len);
bool asn1_write_uint8(struct asn1_data *data, uint8_t v);
bool asn1_push_tag(struct asn1_data *data, uint8_t tag);
bool asn1_pop_tag(struct asn1_data *data);
bool asn1_write_implicit_Integer(struct asn1_data *data, int i);
bool asn1_write_Integer(struct asn1_data *data, int i);
bool asn1_write_BitString(struct asn1_data *data, const void *p, size_t length, uint8_t padding);
bool ber_write_OID_String(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char *OID);
bool ber_write_partial_OID_String(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char *partial_oid);
bool asn1_write_OID(struct asn1_data *data, const char *OID);
bool asn1_write_OctetString(struct asn1_data *data, const void *p, size_t length);
bool asn1_write_LDAPString(struct asn1_data *data, const char *s);
bool asn1_write_DATA_BLOB_LDAPString(struct asn1_data *data, const DATA_BLOB *s);
bool asn1_write_GeneralString(struct asn1_data *data, const char *s);
bool asn1_write_ContextSimple(struct asn1_data *data, uint8_t num, DATA_BLOB *blob);
bool asn1_write_BOOLEAN(struct asn1_data *data, bool v);
bool asn1_read_BOOLEAN(struct asn1_data *data, bool *v);
bool asn1_check_BOOLEAN(struct asn1_data *data, bool v);
bool asn1_write_BOOLEAN_context(struct asn1_data *data, bool v, int context);
bool asn1_read_BOOLEAN_context(struct asn1_data *data, bool *v, int context);
bool asn1_load(struct asn1_data *data, DATA_BLOB blob);
bool asn1_peek(struct asn1_data *data, void *p, int len);
bool asn1_read(struct asn1_data *data, void *p, int len);
bool asn1_read_uint8(struct asn1_data *data, uint8_t *v);
bool asn1_peek_uint8(struct asn1_data *data, uint8_t *v);
bool asn1_peek_tag_needed_size(struct asn1_data *data, uint8_t tag, size_t *size);
bool asn1_peek_tag(struct asn1_data *data, uint8_t tag);
bool asn1_start_tag(struct asn1_data *data, uint8_t tag);
bool asn1_end_tag(struct asn1_data *data);
int asn1_tag_remaining(struct asn1_data *data);
bool ber_read_OID_String(TALLOC_CTX *mem_ctx, DATA_BLOB blob, char **OID);
bool ber_read_partial_OID_String(TALLOC_CTX *mem_ctx, DATA_BLOB blob, char **partial_oid);
bool asn1_read_OID(struct asn1_data *data, TALLOC_CTX *mem_ctx, char **OID);
bool asn1_check_OID(struct asn1_data *data, const char *OID);
bool asn1_read_LDAPString(struct asn1_data *data, TALLOC_CTX *mem_ctx, char **s);
bool asn1_read_GeneralString(struct asn1_data *data, TALLOC_CTX *mem_ctx, char **s);
bool asn1_read_OctetString(struct asn1_data *data, TALLOC_CTX *mem_ctx, DATA_BLOB *blob);
bool asn1_read_ContextSimple(struct asn1_data *data, uint8_t num, DATA_BLOB *blob);
bool asn1_read_implicit_Integer(struct asn1_data *data, int *i);
bool asn1_read_Integer(struct asn1_data *data, int *i);
bool asn1_read_BitString(struct asn1_data *data, TALLOC_CTX *mem_ctx, DATA_BLOB *blob, uint8_t *padding);
bool asn1_read_enumerated(struct asn1_data *data, int *v);
bool asn1_check_enumerated(struct asn1_data *data, int v);
bool asn1_write_enumerated(struct asn1_data *data, uint8_t v);
bool asn1_blob(const struct asn1_data *asn1, DATA_BLOB *blob);
void asn1_load_nocopy(struct asn1_data *data, uint8_t *buf, size_t len);
NTSTATUS asn1_full_tag(DATA_BLOB blob, uint8_t tag, size_t *packet_size);
NTSTATUS asn1_peek_full_tag(DATA_BLOB blob, uint8_t tag, size_t *packet_size);

#endif /* _ASN_1_H */
