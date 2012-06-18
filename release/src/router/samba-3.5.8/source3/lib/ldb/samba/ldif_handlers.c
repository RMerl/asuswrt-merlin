/* 
   ldb database library - ldif handlers for Samba

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Andrew Bartlett 2006
     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "ldb/include/includes.h"

#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "dsdb/samdb/samdb.h"
#include "libcli/security/security.h"

/*
  convert a ldif formatted objectSid to a NDR formatted blob
*/
static int ldif_read_objectSid(struct ldb_context *ldb, void *mem_ctx,
			       const struct ldb_val *in, struct ldb_val *out)
{
	struct dom_sid *sid;
	NTSTATUS status;
	sid = dom_sid_parse_talloc(mem_ctx, (const char *)in->data);
	if (sid == NULL) {
		return -1;
	}
	status = ndr_push_struct_blob(out, mem_ctx, sid, 
				      (ndr_push_flags_fn_t)ndr_push_dom_sid);
	talloc_free(sid);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}
	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted objectSid
*/
static int ldif_write_objectSid(struct ldb_context *ldb, void *mem_ctx,
				const struct ldb_val *in, struct ldb_val *out)
{
	struct dom_sid *sid;
	NTSTATUS status;
	sid = talloc(mem_ctx, struct dom_sid);
	if (sid == NULL) {
		return -1;
	}
	status = ndr_pull_struct_blob(in, sid, sid, 
				      (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(sid);
		return -1;
	}
	out->data = (uint8_t *)dom_sid_string(mem_ctx, sid);
	talloc_free(sid);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((const char *)out->data);
	return 0;
}

static BOOL ldb_comparision_objectSid_isString(const struct ldb_val *v)
{
	if (v->length < 3) {
		return False;
	}

	if (strncmp("S-", (const char *)v->data, 2) != 0) return False;
	
	return True;
}

/*
  compare two objectSids
*/
static int ldb_comparison_objectSid(struct ldb_context *ldb, void *mem_ctx,
				    const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (ldb_comparision_objectSid_isString(v1) && ldb_comparision_objectSid_isString(v2)) {
		return strcmp((const char *)v1->data, (const char *)v2->data);
	} else if (ldb_comparision_objectSid_isString(v1)
		   && !ldb_comparision_objectSid_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectSid(ldb, mem_ctx, v1, &v) != 0) {
			return -1;
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, &v, v2);
		talloc_free(v.data);
		return ret;
	} else if (!ldb_comparision_objectSid_isString(v1)
		   && ldb_comparision_objectSid_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectSid(ldb, mem_ctx, v2, &v) != 0) {
			return -1;
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, v1, &v);
		talloc_free(v.data);
		return ret;
	}
	return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
}

/*
  canonicalise a objectSid
*/
static int ldb_canonicalise_objectSid(struct ldb_context *ldb, void *mem_ctx,
				      const struct ldb_val *in, struct ldb_val *out)
{
	if (ldb_comparision_objectSid_isString(in)) {
		return ldif_read_objectSid(ldb, mem_ctx, in, out);
	}
	return ldb_handler_copy(ldb, mem_ctx, in, out);
}

/*
  convert a ldif formatted objectGUID to a NDR formatted blob
*/
static int ldif_read_objectGUID(struct ldb_context *ldb, void *mem_ctx,
			        const struct ldb_val *in, struct ldb_val *out)
{
	struct GUID guid;
	NTSTATUS status;

	status = GUID_from_string((const char *)in->data, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	status = ndr_push_struct_blob(out, mem_ctx, &guid,
				      (ndr_push_flags_fn_t)ndr_push_GUID);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}
	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted objectGUID
*/
static int ldif_write_objectGUID(struct ldb_context *ldb, void *mem_ctx,
				 const struct ldb_val *in, struct ldb_val *out)
{
	struct GUID guid;
	NTSTATUS status;
	status = ndr_pull_struct_blob(in, mem_ctx, &guid,
				      (ndr_pull_flags_fn_t)ndr_pull_GUID);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}
	out->data = (uint8_t *)GUID_string(mem_ctx, &guid);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((const char *)out->data);
	return 0;
}

static BOOL ldb_comparision_objectGUID_isString(const struct ldb_val *v)
{
	struct GUID guid;
	NTSTATUS status;

	if (v->length < 33) return False;

	/* see if the input if null-terninated (safety check for the below) */
	if (v->data[v->length] != '\0') return False;

	status = GUID_from_string((const char *)v->data, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return False;
	}

	return True;
}

/*
  compare two objectGUIDs
*/
static int ldb_comparison_objectGUID(struct ldb_context *ldb, void *mem_ctx,
				     const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (ldb_comparision_objectGUID_isString(v1) && ldb_comparision_objectGUID_isString(v2)) {
		return strcmp((const char *)v1->data, (const char *)v2->data);
	} else if (ldb_comparision_objectGUID_isString(v1)
		   && !ldb_comparision_objectGUID_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectGUID(ldb, mem_ctx, v1, &v) != 0) {
			return -1;
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, &v, v2);
		talloc_free(v.data);
		return ret;
	} else if (!ldb_comparision_objectGUID_isString(v1)
		   && ldb_comparision_objectGUID_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectGUID(ldb, mem_ctx, v2, &v) != 0) {
			return -1;
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, v1, &v);
		talloc_free(v.data);
		return ret;
	}
	return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
}

/*
  canonicalise a objectGUID
*/
static int ldb_canonicalise_objectGUID(struct ldb_context *ldb, void *mem_ctx,
				       const struct ldb_val *in, struct ldb_val *out)
{
	if (ldb_comparision_objectGUID_isString(in)) {
		return ldif_read_objectGUID(ldb, mem_ctx, in, out);
	}
	return ldb_handler_copy(ldb, mem_ctx, in, out);
}


/*
  convert a ldif (SDDL) formatted ntSecurityDescriptor to a NDR formatted blob
*/
static int ldif_read_ntSecurityDescriptor(struct ldb_context *ldb, void *mem_ctx,
					  const struct ldb_val *in, struct ldb_val *out)
{
	struct security_descriptor *sd;
	NTSTATUS status;

	sd = sddl_decode(mem_ctx, (const char *)in->data, NULL);
	if (sd == NULL) {
		return -1;
	}
	status = ndr_push_struct_blob(out, mem_ctx, sd, 
				      (ndr_push_flags_fn_t)ndr_push_security_descriptor);
	talloc_free(sd);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}
	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted ntSecurityDescriptor (SDDL format)
*/
static int ldif_write_ntSecurityDescriptor(struct ldb_context *ldb, void *mem_ctx,
					   const struct ldb_val *in, struct ldb_val *out)
{
	struct security_descriptor *sd;
	NTSTATUS status;

	sd = talloc(mem_ctx, struct security_descriptor);
	if (sd == NULL) {
		return -1;
	}
	status = ndr_pull_struct_blob(in, sd, sd, 
				      (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(sd);
		return -1;
	}
	out->data = (uint8_t *)sddl_encode(mem_ctx, sd, NULL);
	talloc_free(sd);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((const char *)out->data);
	return 0;
}

/* 
   canonicolise an objectCategory.  We use the short form as the cannoical form:
   cn=Person,cn=Schema,cn=Configuration,<basedn> becomes 'person'
*/

static int ldif_canonicalise_objectCategory(struct ldb_context *ldb, void *mem_ctx,
					    const struct ldb_val *in, struct ldb_val *out)
{
	struct ldb_dn *dn1 = NULL;
	char *oc1, *oc2;

	dn1 = ldb_dn_explode(mem_ctx, (char *)in->data);
	if (dn1 == NULL) {
		oc1 = talloc_strndup(mem_ctx, (char *)in->data, in->length);
	} else if (ldb_dn_get_comp_num(dn1) >= 1 && strcasecmp(ldb_dn_get_rdn_name(dn1), "cn") == 0) {
		const struct ldb_val *val = ldb_dn_get_rdn_val(dn1);
		oc1 = talloc_strndup(mem_ctx, (char *)val->data, val->length);
	} else {
		return -1;
	}

	oc2 = ldb_casefold(ldb, mem_ctx, oc1);
	out->data = (void *)oc2;
	out->length = strlen(oc2);
	talloc_free(oc1);
	talloc_free(dn1);
	return 0;
}

static int ldif_comparison_objectCategory(struct ldb_context *ldb, void *mem_ctx,
					  const struct ldb_val *v1,
					  const struct ldb_val *v2)
{
	struct ldb_dn *dn1 = NULL, *dn2 = NULL;
	const char *oc1, *oc2;

	dn1 = ldb_dn_explode(mem_ctx, (char *)v1->data);
	if (dn1 == NULL) {
		oc1 = talloc_strndup(mem_ctx, (char *)v1->data, v1->length);
	} else if (ldb_dn_get_comp_num(dn1) >= 1 && strcasecmp(ldb_dn_get_rdn_name(dn1), "cn") == 0) {
		const struct ldb_val *val = ldb_dn_get_rdn_val(dn1);
		oc1 = talloc_strndup(mem_ctx, (char *)val->data, val->length);
	} else {
		oc1 = NULL;
	}

	dn2 = ldb_dn_explode(mem_ctx, (char *)v2->data);
	if (dn2 == NULL) {
		oc2 = talloc_strndup(mem_ctx, (char *)v2->data, v2->length);
	} else if (ldb_dn_get_comp_num(dn2) >= 2 && strcasecmp(ldb_dn_get_rdn_name(dn2), "cn") == 0) {
		const struct ldb_val *val = ldb_dn_get_rdn_val(dn2);
		oc2 = talloc_strndup(mem_ctx, (char *)val->data, val->length);
	} else {
		oc2 = NULL;
	}

	oc1 = ldb_casefold(ldb, mem_ctx, oc1);
	oc2 = ldb_casefold(ldb, mem_ctx, oc2);
	if (!oc1 && oc2) {
		return -1;
	} 
	if (oc1 && !oc2) {
		return 1;
	}	
	if (!oc1 && !oc2) {
		return -1;
	}

	return strcmp(oc1, oc2);
}

static const struct ldb_attrib_handler samba_handlers[] = {
	{ 
		.attr            = "objectSid",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectSid,
		.ldif_write_fn   = ldif_write_objectSid,
		.canonicalise_fn = ldb_canonicalise_objectSid,
		.comparison_fn   = ldb_comparison_objectSid
	},
	{ 
		.attr            = "securityIdentifier",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectSid,
		.ldif_write_fn   = ldif_write_objectSid,
		.canonicalise_fn = ldb_canonicalise_objectSid,
		.comparison_fn   = ldb_comparison_objectSid
	},
	{ 
		.attr            = "ntSecurityDescriptor",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_ntSecurityDescriptor,
		.ldif_write_fn   = ldif_write_ntSecurityDescriptor,
		.canonicalise_fn = ldb_handler_copy,
		.comparison_fn   = ldb_comparison_binary
	},
	{ 
		.attr            = "objectGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "invocationId",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "schemaIDGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "attributeSecurityGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "parentGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "siteGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "pKTGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "fRSVersionGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "fRSReplicaSetGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "netbootGUID",
		.flags           = 0,
		.ldif_read_fn    = ldif_read_objectGUID,
		.ldif_write_fn   = ldif_write_objectGUID,
		.canonicalise_fn = ldb_canonicalise_objectGUID,
		.comparison_fn   = ldb_comparison_objectGUID
	},
	{ 
		.attr            = "objectCategory",
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldif_canonicalise_objectCategory,
		.comparison_fn   = ldif_comparison_objectCategory,
	}
};

/*
  register the samba ldif handlers
*/
int ldb_register_samba_handlers(struct ldb_context *ldb)
{
	return ldb_set_attrib_handlers(ldb, samba_handlers, ARRAY_SIZE(samba_handlers));
}
