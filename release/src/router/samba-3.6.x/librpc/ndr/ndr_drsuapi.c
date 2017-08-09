/*
   Unix SMB/CIFS implementation.

   routines for printing some linked list structs in DRSUAPI

   Copyright (C) Stefan (metze) Metzmacher 2005

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
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "../lib/util/asn1.h"
#include "librpc/ndr/ndr_compression.h"
/* We don't need multibyte if we're just comparing to 'ff' */
#undef strncasecmp

void ndr_print_drsuapi_DsReplicaObjectListItem(struct ndr_print *ndr, const char *name,
					       const struct drsuapi_DsReplicaObjectListItem *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsReplicaObjectListItem");
	ndr->depth++;
	ndr_print_ptr(ndr, "next_object", r->next_object);
	ndr_print_drsuapi_DsReplicaObject(ndr, "object", &r->object);
	ndr->depth--;
	if (r->next_object) {
		ndr_print_drsuapi_DsReplicaObjectListItem(ndr, "next_object", r->next_object);
	}
}

void ndr_print_drsuapi_DsReplicaObjectListItemEx(struct ndr_print *ndr, const char *name, const struct drsuapi_DsReplicaObjectListItemEx *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsReplicaObjectListItemEx");
	ndr->depth++;
	ndr_print_ptr(ndr, "next_object", r->next_object);
	ndr_print_drsuapi_DsReplicaObject(ndr, "object", &r->object);
	ndr_print_uint32(ndr, "is_nc_prefix", r->is_nc_prefix);
	ndr_print_ptr(ndr, "parent_object_guid", r->parent_object_guid);
	ndr->depth++;
	if (r->parent_object_guid) {
		ndr_print_GUID(ndr, "parent_object_guid", r->parent_object_guid);
	}
	ndr->depth--;
	ndr_print_ptr(ndr, "meta_data_ctr", r->meta_data_ctr);
	ndr->depth++;
	if (r->meta_data_ctr) {
		ndr_print_drsuapi_DsReplicaMetaDataCtr(ndr, "meta_data_ctr", r->meta_data_ctr);
	}
	ndr->depth--;
	ndr->depth--;
	if (r->next_object) {
		ndr_print_drsuapi_DsReplicaObjectListItemEx(ndr, "next_object", r->next_object);
	}
}

_PUBLIC_ void ndr_print_drsuapi_DsReplicaOID(struct ndr_print *ndr, const char *name, const struct drsuapi_DsReplicaOID *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsReplicaOID");
	ndr->depth++;
	ndr_print_uint32(ndr, "length", r->length);
	ndr->print(ndr, "%-25s: length=%u", "oid", r->length);
	if (r->binary_oid) {
		char *partial_oid = NULL;
		DATA_BLOB oid_blob = data_blob_const(r->binary_oid, r->length);
		char *hex_str = data_blob_hex_string_upper(ndr, &oid_blob);
		ber_read_partial_OID_String(ndr, oid_blob, &partial_oid);
		ndr->depth++;
		ndr->print(ndr, "%-25s: 0x%s (%s)", "binary_oid", hex_str, partial_oid);
		ndr->depth--;
		talloc_free(hex_str);
		talloc_free(partial_oid);
	}
	ndr->depth--;
}

static void _print_drsuapi_DsAttributeValue_attid(struct ndr_print *ndr, const char *name,
						  const struct drsuapi_DsAttributeValue *r)
{
	uint32_t v;

	ndr_print_struct(ndr, name, "drsuapi_DsAttributeValue");
	ndr->depth++;
	v = IVAL(r->blob->data, 0);
	ndr_print_uint32(ndr, "attid", v);
	ndr->depth--;
}

static void _print_drsuapi_DsAttributeValue_str(struct ndr_print *ndr, const char *name,
						const struct drsuapi_DsAttributeValue *r)
{
	char *str;

	ndr_print_struct(ndr, name, "drsuapi_DsAttributeValue");
	ndr->depth++;
	if (!convert_string_talloc(ndr,
	                           CH_UTF16, CH_UNIX,
	                           r->blob->data,
	                           r->blob->length,
	                           (void **)&str, NULL, false)) {
		ndr_print_string(ndr, "string", "INVALID CONVERSION");
	} else {
		ndr_print_string(ndr, "string", str);
		talloc_free(str);
	}
	ndr->depth--;
}

static void _print_drsuapi_DsAttributeValueCtr(struct ndr_print *ndr,
					       const char *name,
					       const struct drsuapi_DsAttributeValueCtr *r,
					       void (*print_val_fn)(struct ndr_print *ndr, const char *name, const struct drsuapi_DsAttributeValue *r))
{
	uint32_t cntr_values_1;
	ndr_print_struct(ndr, name, "drsuapi_DsAttributeValueCtr");
	ndr->depth++;
	ndr_print_uint32(ndr, "num_values", r->num_values);
	ndr_print_ptr(ndr, "values", r->values);
	ndr->depth++;
	if (r->values) {
		ndr->print(ndr, "%s: ARRAY(%d)", "values", (int)r->num_values);
		ndr->depth++;
		for (cntr_values_1=0;cntr_values_1<r->num_values;cntr_values_1++) {
			char *idx_1=NULL;
			if (asprintf(&idx_1, "[%d]", cntr_values_1) != -1) {
				//ndr_print_drsuapi_DsAttributeValue(ndr, "values", &r->values[cntr_values_1]);
				print_val_fn(ndr, "values", &r->values[cntr_values_1]);
				free(idx_1);
			}
		}
		ndr->depth--;
	}
	ndr->depth--;
	ndr->depth--;
}

_PUBLIC_ void ndr_print_drsuapi_DsReplicaAttribute(struct ndr_print *ndr,
						   const char *name,
						   const struct drsuapi_DsReplicaAttribute *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsReplicaAttribute");
	ndr->depth++;
	ndr_print_drsuapi_DsAttributeId(ndr, "attid", r->attid);
	switch (r->attid) {
	case DRSUAPI_ATTID_objectClass:
	case DRSUAPI_ATTID_possSuperiors:
	case DRSUAPI_ATTID_subClassOf:
	case DRSUAPI_ATTID_governsID:
	case DRSUAPI_ATTID_mustContain:
	case DRSUAPI_ATTID_mayContain:
	case DRSUAPI_ATTID_rDNAttId:
	case DRSUAPI_ATTID_attributeID:
	case DRSUAPI_ATTID_attributeSyntax:
	case DRSUAPI_ATTID_auxiliaryClass:
	case DRSUAPI_ATTID_systemPossSuperiors:
	case DRSUAPI_ATTID_systemMayContain:
	case DRSUAPI_ATTID_systemMustContain:
	case DRSUAPI_ATTID_systemAuxiliaryClass:
	case DRSUAPI_ATTID_transportAddressAttribute:
		/* ATTIDs for classSchema and attributeSchema */
		_print_drsuapi_DsAttributeValueCtr(ndr, "value_ctr", &r->value_ctr,
		                                   _print_drsuapi_DsAttributeValue_attid);
		break;
	case DRSUAPI_ATTID_cn:
	case DRSUAPI_ATTID_ou:
	case DRSUAPI_ATTID_description:
	case DRSUAPI_ATTID_displayName:
	case DRSUAPI_ATTID_dMDLocation:
	case DRSUAPI_ATTID_adminDisplayName:
	case DRSUAPI_ATTID_adminDescription:
	case DRSUAPI_ATTID_lDAPDisplayName:
	case DRSUAPI_ATTID_name:
		_print_drsuapi_DsAttributeValueCtr(ndr, "value_ctr", &r->value_ctr,
		                                   _print_drsuapi_DsAttributeValue_str);
		break;
	default:
		_print_drsuapi_DsAttributeValueCtr(ndr, "value_ctr", &r->value_ctr,
		                                   ndr_print_drsuapi_DsAttributeValue);
		break;
	}
	ndr->depth--;
}

enum ndr_err_code ndr_push_drsuapi_DsGetNCChangesMSZIPCtr1(struct ndr_push *ndr, int ndr_flags, const struct drsuapi_DsGetNCChangesMSZIPCtr1 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t decompressed_length = 0;
		uint32_t compressed_length = 0;
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr1TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					decompressed_length = _ndr_ts_compressed->offset;
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
				}
				compressed_length = _ndr_ts->offset;
				talloc_free(_ndr_ts);
			}
		}
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, decompressed_length));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, compressed_length));
		NDR_CHECK(ndr_push_unique_ptr(ndr, r->ts));
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr1TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
				}
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_ts, 4, -1));
			}
		}
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_drsuapi_DsGetNCChangesMSZIPCtr6(struct ndr_push *ndr, int ndr_flags, const struct drsuapi_DsGetNCChangesMSZIPCtr6 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t decompressed_length = 0;
		uint32_t compressed_length = 0;
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr6TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					decompressed_length = _ndr_ts_compressed->offset;
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
				}
				compressed_length = _ndr_ts->offset;
				talloc_free(_ndr_ts);
			}
		}
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, decompressed_length));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, compressed_length));
		NDR_CHECK(ndr_push_unique_ptr(ndr, r->ts));
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr6TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_MSZIP, -1));
				}
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_ts, 4, -1));
			}
		}
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_drsuapi_DsGetNCChangesXPRESSCtr1(struct ndr_push *ndr, int ndr_flags, const struct drsuapi_DsGetNCChangesXPRESSCtr1 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t decompressed_length = 0;
		uint32_t compressed_length = 0;
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr1TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					decompressed_length = _ndr_ts_compressed->offset;
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
				}
				compressed_length = _ndr_ts->offset;
				talloc_free(_ndr_ts);
			}
		}
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, decompressed_length));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, compressed_length));
		NDR_CHECK(ndr_push_unique_ptr(ndr, r->ts));
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr1TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
				}
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_ts, 4, -1));
			}
		}
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_drsuapi_DsGetNCChangesXPRESSCtr6(struct ndr_push *ndr, int ndr_flags, const struct drsuapi_DsGetNCChangesXPRESSCtr6 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t decompressed_length = 0;
		uint32_t compressed_length = 0;
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr6TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					decompressed_length = _ndr_ts_compressed->offset;
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
				}
				compressed_length = _ndr_ts->offset;
				talloc_free(_ndr_ts);
			}
		}
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, decompressed_length));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, compressed_length));
		NDR_CHECK(ndr_push_unique_ptr(ndr, r->ts));
	}
	if (ndr_flags & NDR_BUFFERS) {
		if (r->ts) {
			{
				struct ndr_push *_ndr_ts;
				NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_ts, 4, -1));
				{
					struct ndr_push *_ndr_ts_compressed;
					NDR_CHECK(ndr_push_compression_start(_ndr_ts, &_ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
					NDR_CHECK(ndr_push_drsuapi_DsGetNCChangesCtr6TS(_ndr_ts_compressed, NDR_SCALARS|NDR_BUFFERS, r->ts));
					NDR_CHECK(ndr_push_compression_end(_ndr_ts, _ndr_ts_compressed, NDR_COMPRESSION_XPRESS, -1));
				}
				NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_ts, 4, -1));
			}
		}
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ size_t ndr_size_drsuapi_DsReplicaObjectIdentifier3Binary_without_Binary(const struct drsuapi_DsReplicaObjectIdentifier3Binary *r, int flags)
{
	return ndr_size_struct((const struct drsuapi_DsReplicaObjectIdentifier3 *)r, flags, (ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
}

_PUBLIC_ void ndr_print_drsuapi_SecBufferType(struct ndr_print *ndr, const char *name, enum drsuapi_SecBufferType r)
{
	const char *val = NULL;

	switch (r & 0x00000007) {
		case DRSUAPI_SECBUFFER_EMPTY: val = "DRSUAPI_SECBUFFER_EMPTY"; break;
		case DRSUAPI_SECBUFFER_DATA: val = "DRSUAPI_SECBUFFER_DATA"; break;
		case DRSUAPI_SECBUFFER_TOKEN: val = "DRSUAPI_SECBUFFER_TOKEN"; break;
		case DRSUAPI_SECBUFFER_PKG_PARAMS: val = "DRSUAPI_SECBUFFER_PKG_PARAMS"; break;
		case DRSUAPI_SECBUFFER_MISSING: val = "DRSUAPI_SECBUFFER_MISSING"; break;
		case DRSUAPI_SECBUFFER_EXTRA: val = "DRSUAPI_SECBUFFER_EXTRA"; break;
		case DRSUAPI_SECBUFFER_STREAM_TRAILER: val = "DRSUAPI_SECBUFFER_STREAM_TRAILER"; break;
		case DRSUAPI_SECBUFFER_STREAM_HEADER: val = "DRSUAPI_SECBUFFER_STREAM_HEADER"; break;
	}

	if (r & DRSUAPI_SECBUFFER_READONLY) {
		char *v = talloc_asprintf(ndr, "DRSUAPI_SECBUFFER_READONLY | %s", val);
		ndr_print_enum(ndr, name, "ENUM", v, r);
	} else {
		ndr_print_enum(ndr, name, "ENUM", val, r);
	}
}

_PUBLIC_ void ndr_print_drsuapi_DsAddEntry_AttrErrListItem_V1(struct ndr_print *ndr, const char *name, const struct drsuapi_DsAddEntry_AttrErrListItem_V1 *r)
{
	ndr_print_struct(ndr, name, "drsuapi_DsAddEntry_AttrErrListItem_V1");
	ndr->depth++;
	ndr_print_ptr(ndr, "next", r->next);
	ndr_print_drsuapi_DsAddEntry_AttrErr_V1(ndr, "err_data", &r->err_data);
	ndr->depth--;
	if (r->next) {
		ndr_print_drsuapi_DsAddEntry_AttrErrListItem_V1(ndr, "next", r->next);
	}
}
