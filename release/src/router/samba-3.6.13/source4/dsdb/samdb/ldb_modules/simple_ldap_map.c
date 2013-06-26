/* 
   ldb database module

   LDAP semantics mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006

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

/* 
   This module relies on ldb_map to do all the real work, but performs
   some of the trivial mappings between AD semantics and that provided
   by OpenLDAP and similar servers.
*/

#include "includes.h"
#include <ldb_module.h>
#include "ldb/ldb_map/ldb_map.h"

#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/ndr/libndr.h"
#include "dsdb/samdb/samdb.h"
#include <ldb_handlers.h>

struct entryuuid_private {
	struct ldb_context *ldb;
	struct ldb_dn **base_dns;
};

static struct ldb_val encode_guid(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct GUID guid;
	NTSTATUS status = GUID_from_data_blob(val, &guid);
	struct ldb_val out = data_blob(NULL, 0);

	if (!NT_STATUS_IS_OK(status)) {
		return out;
	}
	status = GUID_to_ndr_blob(&guid, ctx, &out);
	if (!NT_STATUS_IS_OK(status)) {
		return data_blob(NULL, 0);
	}

	return out;
}

static struct ldb_val guid_always_string(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out = data_blob(NULL, 0);
	struct GUID guid;
	NTSTATUS status = GUID_from_data_blob(val, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return out;
	}
	return data_blob_string_const(GUID_string(ctx, &guid));
}

static struct ldb_val encode_ns_guid(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct GUID guid;
	NTSTATUS status = NS_GUID_from_string((char *)val->data, &guid);
	struct ldb_val out = data_blob(NULL, 0);

	if (!NT_STATUS_IS_OK(status)) {
		return out;
	}
	status = GUID_to_ndr_blob(&guid, ctx, &out);
	if (!NT_STATUS_IS_OK(status)) {
		return data_blob(NULL, 0);
	}

	return out;
}

static struct ldb_val guid_ns_string(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out = data_blob(NULL, 0);
	struct GUID guid;
	NTSTATUS status = GUID_from_data_blob(val, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return out;
	}
	return data_blob_string_const(NS_GUID_string(ctx, &guid));
}

/* The backend holds binary sids, so just copy them back */
static struct ldb_val val_copy(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out = data_blob(NULL, 0);
	out = ldb_val_dup(ctx, val);

	return out;
}

/* Ensure we always convert sids into binary, so the backend doesn't have to know about both forms */
static struct ldb_val sid_always_binary(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_val out = data_blob(NULL, 0);
	const struct ldb_schema_attribute *a = ldb_schema_attribute_by_name(ldb, "objectSid");

	if (a->syntax->canonicalise_fn(ldb, ctx, val, &out) != LDB_SUCCESS) {
		return data_blob(NULL, 0);
	}

	return out;
}

/* Ensure we always convert sids into string, so the backend doesn't have to know about both forms */
static struct ldb_val sid_always_string(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_val out = data_blob(NULL, 0);

	if (ldif_comparision_objectSid_isString(val)) {
		if (ldb_handler_copy(ldb, ctx, val, &out) != LDB_SUCCESS) {
			return data_blob(NULL, 0);
		}

	} else {
		if (ldif_write_objectSid(ldb, ctx, val, &out) != LDB_SUCCESS) {
			return data_blob(NULL, 0);
		}
	}
	return out;
}

/* Ensure we always convert objectCategory into a DN */
static struct ldb_val objectCategory_always_dn(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_dn *dn;
	struct ldb_val out = data_blob(NULL, 0);
	const struct ldb_schema_attribute *a = ldb_schema_attribute_by_name(ldb, "objectCategory");

	dn = ldb_dn_from_ldb_val(ctx, ldb, val);
	if (ldb_dn_validate(dn)) {
		talloc_free(dn);
		return val_copy(module, ctx, val);
	}
	talloc_free(dn);

	if (a->syntax->canonicalise_fn(ldb, ctx, val, &out) != LDB_SUCCESS) {
		return data_blob(NULL, 0);
	}

	return out;
}

static struct ldb_val normalise_to_signed32(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	/* We've to use "strtoll" here to have the intended overflows.
	 * Otherwise we may get "LONG_MAX" and the conversion is wrong. */
	int32_t i = (int32_t) strtoll((char *)val->data, NULL, 0);
	out = data_blob_string_const(talloc_asprintf(ctx, "%d", i));
	return out;
}

static struct ldb_val usn_to_entryCSN(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	unsigned long long usn = strtoull((const char *)val->data, NULL, 10);
	time_t t = (usn >> 24);
	out = data_blob_string_const(talloc_asprintf(ctx, "%s#%06x#00#000000", ldb_timestring(ctx, t), (unsigned int)(usn & 0xFFFFFF)));
	return out;
}

static unsigned long long entryCSN_to_usn_int(TALLOC_CTX *ctx, const struct ldb_val *val) 
{
	char *entryCSN = talloc_strndup(ctx, (const char *)val->data, val->length);
	char *mod_per_sec;
	time_t t;
	unsigned long long usn;
	char *p;
	if (!entryCSN) {
		return 0;
	}
	p = strchr(entryCSN, '#');
	if (!p) {
		return 0;
	}
	p[0] = '\0';
	p++;
	mod_per_sec = p;

	p = strchr(p, '#');
	if (!p) {
		return 0;
	}
	p[0] = '\0';
	p++;

	usn = strtol(mod_per_sec, NULL, 16);

	t = ldb_string_to_time(entryCSN);
	
	usn = usn | ((unsigned long long)t <<24);
	return usn;
}

static struct ldb_val entryCSN_to_usn(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	unsigned long long usn = entryCSN_to_usn_int(ctx, val);
	out = data_blob_string_const(talloc_asprintf(ctx, "%lld", usn));
	return out;
}

static struct ldb_val usn_to_timestamp(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	unsigned long long usn = strtoull((const char *)val->data, NULL, 10);
	time_t t = (usn >> 24);
	out = data_blob_string_const(ldb_timestring(ctx, t));
	return out;
}

static struct ldb_val timestamp_to_usn(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	time_t t=0;
	unsigned long long usn;

	ldb_val_to_time(val, &t);
	
	usn = ((unsigned long long)t <<24);

	out = data_blob_string_const(talloc_asprintf(ctx, "%lld", usn));
	return out;
}


static const struct ldb_map_attribute entryuuid_attributes[] = 
{
	/* objectGUID */
	{
		.local_name = "objectGUID",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "entryUUID", 
				.convert_local = guid_always_string,
				.convert_remote = encode_guid,
			},
		},
	},
	/* invocationId */
	{
		.local_name = "invocationId",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "invocationId", 
				.convert_local = guid_always_string,
				.convert_remote = encode_guid,
			},
		},
	},
	/* objectSid */
	{
		.local_name = "objectSid",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "objectSid", 
				.convert_local = sid_always_binary,
				.convert_remote = val_copy,
			},
		},
	},
	/* securityIdentifier */
	{
		.local_name = "securityIdentifier",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "securityIdentifier",
				.convert_local = sid_always_binary,
				.convert_remote = val_copy,
			},
		},
	},
	{
		.local_name = "name",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "rdnValue"
			 }
		}
	},
	{
		.local_name = "whenCreated",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "createTimestamp"
			 }
		}
	},
	{
		.local_name = "whenChanged",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "modifyTimestamp"
			 }
		}
	},
	{
		.local_name = "objectClasses",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "samba4ObjectClasses"
			 }
		}
	},
	{
		.local_name = "dITContentRules",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "samba4DITContentRules"
			 }
		}
	},
	{
		.local_name = "attributeTypes",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "samba4AttributeTypes"
			 }
		}
	},
	{
		.local_name = "objectCategory",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "objectCategory", 
				.convert_local = objectCategory_always_dn,
				.convert_remote = val_copy,
			},
		},
	},
	{
		.local_name = "distinguishedName",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "entryDN"
			 }
		}
	},
	{
		.local_name = "primaryGroupID",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "primaryGroupID",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			}
		}
	},
	{
		.local_name = "groupType",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "groupType",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "userAccountControl",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "userAccountControl",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "sAMAccountType",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "sAMAccountType",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "systemFlags",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "systemFlags",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "usnChanged",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "entryCSN",
				 .convert_local = usn_to_entryCSN,
				 .convert_remote = entryCSN_to_usn
			 },
		},
	},
	{
		.local_name = "usnCreated",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "createTimestamp",
				 .convert_local = usn_to_timestamp,
				 .convert_remote = timestamp_to_usn,
			 },
		},
	},
	{
		.local_name = "*",
		.type = LDB_MAP_KEEP,
	},
	{
		.local_name = NULL,
	}
};

/* This objectClass conflicts with builtin classes on OpenLDAP */
const struct ldb_map_objectclass entryuuid_objectclasses[] =
{
	{
		.local_name = "subSchema",
		.remote_name = "samba4SubSchema"
	},
	{
		.local_name = NULL
	}
};

/* These things do not show up in wildcard searches in OpenLDAP, but
 * we need them to show up in the AD-like view */
static const char * const entryuuid_wildcard_attributes[] = {
	"objectGUID", 
	"whenCreated", 
	"whenChanged",
	"usnCreated",
	"usnChanged",
	"memberOf",
	NULL
};

static const struct ldb_map_attribute nsuniqueid_attributes[] = 
{
	/* objectGUID */
	{
		.local_name = "objectGUID",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "nsuniqueid", 
				.convert_local = guid_ns_string,
				.convert_remote = encode_ns_guid,
			}
		}
	},
	/* objectSid */	
	{
		.local_name = "objectSid",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "sambaSID", 
				.convert_local = sid_always_string,
				.convert_remote = sid_always_binary,
			}
		}
	},
	/* securityIdentifier */
	{
		.local_name = "securityIdentifier",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "securityIdentifier",
				.convert_local = sid_always_binary,
				.convert_remote = val_copy,
			},
		},
	},
	{
		.local_name = "whenCreated",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "createTimestamp"
			 }
		}
	},
	{
		.local_name = "whenChanged",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "modifyTimestamp"
			 }
		}
	},
	{
		.local_name = "objectCategory",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "objectCategory", 
				.convert_local = objectCategory_always_dn,
				.convert_remote = val_copy,
			}
		}
	},
	{
		.local_name = "distinguishedName",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "entryDN"
			 }
		}
	},
	{
		.local_name = "primaryGroupID",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "primaryGroupID",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			}
		}
	},
	{
		.local_name = "groupType",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "sambaGroupType",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "userAccountControl",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "userAccountControl",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "sAMAccountType",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "sAMAccountType",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "systemFlags",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "systemFlags",
				 .convert_local = normalise_to_signed32,
				 .convert_remote = val_copy,
			 }
		}
	},
	{
		.local_name = "usnChanged",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "modifyTimestamp",
				 .convert_local = usn_to_timestamp,
				 .convert_remote = timestamp_to_usn,
			 }
		}
	},
	{
		.local_name = "usnCreated",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "createTimestamp",
				 .convert_local = usn_to_timestamp,
				 .convert_remote = timestamp_to_usn,
			 }
		}
	},
	{
		.local_name = "pwdLastSet",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaPwdLastSet"
			 }
		}
	},
	{
		.local_name = "lastLogon",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaLogonTime"
			 }
		}
	},
	{
		.local_name = "lastLogoff",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaLogoffTime"
			 }
		}
	},
	{
		.local_name = "badPwdCount",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaBadPasswordCount"
			 }
		}
	},
	{
		.local_name = "logonHours",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaLogonHours"
			 }
		}
	},
	{
		.local_name = "homeDrive",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaHomeDrive"
			 }
		}
	},
	{
		.local_name = "scriptPath",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaLogonScript"
			 }
		}
	},
	{
		.local_name = "profilePath",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaProfilePath"
			 }
		}
	},
	{
		.local_name = "userWorkstations",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaUserWorkstations"
			 }
		}
	},
	{
		.local_name = "homeDirectory",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaHomePath"
			 }
		}
	},
	{
		.local_name = "nextRid",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaNextRid"
			 }
		}
	},
	{
		.local_name = "privilegeDisplayName",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				 .remote_name = "sambaPrivName"
			 }
		}
	},
	{
		.local_name = "*",
		.type = LDB_MAP_KEEP,
	},
	{
		.local_name = NULL,
	}
};

/* This objectClass conflicts with builtin classes on FDS */
const struct ldb_map_objectclass nsuniqueid_objectclasses[] =
{
	{
		.local_name = NULL
	}
};

/* These things do not show up in wildcard searches in OpenLDAP, but
 * we need them to show up in the AD-like view */
static const char * const nsuniqueid_wildcard_attributes[] = {
	"objectGUID", 
	"whenCreated", 
	"whenChanged",
	"usnCreated",
	"usnChanged",
	NULL
};

/* the context init function */
static int entryuuid_init(struct ldb_module *module)
{
        int ret;
	ret = ldb_map_init(module, entryuuid_attributes, entryuuid_objectclasses, entryuuid_wildcard_attributes, "samba4Top", NULL);
        if (ret != LDB_SUCCESS)
                return ret;

	return ldb_next_init(module);
}

/* the context init function */
static int nsuniqueid_init(struct ldb_module *module)
{
        int ret;
	ret = ldb_map_init(module, nsuniqueid_attributes, nsuniqueid_objectclasses, nsuniqueid_wildcard_attributes, "extensibleObject", NULL);
        if (ret != LDB_SUCCESS)
                return ret;

	return ldb_next_init(module);
}

static int get_seq_callback(struct ldb_request *req,
			    struct ldb_reply *ares)
{
	unsigned long long *seq = (unsigned long long *)req->context;

	if (!ares) {
		return ldb_request_done(req, LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_request_done(req, ares->error);
	}

	if (ares->type == LDB_REPLY_ENTRY) {
		struct ldb_message_element *el = ldb_msg_find_element(ares->message, "contextCSN");
		if (el) {
			*seq = entryCSN_to_usn_int(ares, &el->values[0]);
		}
	}

	if (ares->type == LDB_REPLY_DONE) {
		return ldb_request_done(req, LDB_SUCCESS);
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int entryuuid_sequence_number(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	int ret;
	struct map_private *map_private;
	struct entryuuid_private *entryuuid_private;
	unsigned long long seq_num = 0;
	struct ldb_request *search_req;

	const struct ldb_control *partition_ctrl;
	const struct dsdb_control_current_partition *partition;
 
	static const char *contextCSN_attr[] = {
		"contextCSN", NULL
	};

	struct ldb_seqnum_request *seq;
	struct ldb_seqnum_result *seqr;
	struct ldb_extended *ext;

	ldb = ldb_module_get_ctx(module);

	seq = talloc_get_type(req->op.extended.data, struct ldb_seqnum_request);

	map_private = talloc_get_type(ldb_module_get_private(module), struct map_private);

	entryuuid_private = talloc_get_type(map_private->caller_private, struct entryuuid_private);

	/* All this to get the DN of the parition, so we can search the right thing */
	partition_ctrl = ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID);
	if (!partition_ctrl) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "entryuuid_sequence_number: no current partition control found!");
		return LDB_ERR_PROTOCOL_ERROR;
	}

	partition = talloc_get_type(partition_ctrl->data,
				    struct dsdb_control_current_partition);
	if ((partition == NULL) || (partition->version != DSDB_CONTROL_CURRENT_PARTITION_VERSION)) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "entryuuid_sequence_number: current partition control with wrong data!");
		return LDB_ERR_PROTOCOL_ERROR;
	}

	ret = ldb_build_search_req(&search_req, ldb, req,
				   partition->dn, LDB_SCOPE_BASE,
				   NULL, contextCSN_attr, NULL,
				   &seq_num, get_seq_callback,
				   NULL);
	LDB_REQ_SET_LOCATION(search_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ldb_next_request(module, search_req);

	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(search_req->handle, LDB_WAIT_ALL);
	}

	talloc_free(search_req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ext = talloc_zero(req, struct ldb_extended);
	if (!ext) {
		return ldb_oom(ldb);
	}
	seqr = talloc_zero(req, struct ldb_seqnum_result);
	if (seqr == NULL) {
		talloc_free(ext);
		return ldb_oom(ldb);
	}
	ext->oid = LDB_EXTENDED_SEQUENCE_NUMBER;
	ext->data = seqr;

	switch (seq->type) {
	case LDB_SEQ_HIGHEST_SEQ:
		seqr->seq_num = seq_num;
		break;
	case LDB_SEQ_NEXT:
		seqr->seq_num = seq_num;
		seqr->seq_num++;
		break;
	case LDB_SEQ_HIGHEST_TIMESTAMP:
	{
		seqr->seq_num = (seq_num >> 24);
		break;
	}
	}
	seqr->flags = 0;
	seqr->flags |= LDB_SEQ_TIMESTAMP_SEQUENCE;
	seqr->flags |= LDB_SEQ_GLOBAL_SEQUENCE;

	/* send request done */
	return ldb_module_done(req, NULL, ext, LDB_SUCCESS);
}

static int entryuuid_extended(struct ldb_module *module, struct ldb_request *req)
{
	if (strcmp(req->op.extended.oid, LDB_EXTENDED_SEQUENCE_NUMBER) == 0) {
		return entryuuid_sequence_number(module, req);
	}

	return ldb_next_request(module, req);
}

static const struct ldb_module_ops ldb_entryuuid_module_ops = {
	.name		   = "entryuuid",
	.init_context	   = entryuuid_init,
	.extended          = entryuuid_extended,
	LDB_MAP_OPS
};

static const struct ldb_module_ops ldb_nsuniqueid_module_ops = {
	.name		   = "nsuniqueid",
	.init_context	   = nsuniqueid_init,
	.extended          = entryuuid_extended,
	LDB_MAP_OPS
};

/*
  initialise the module
 */
_PUBLIC_ int ldb_simple_ldap_map_module_init(const char *version)
{
	int ret;
	LDB_MODULE_CHECK_VERSION(version);
	ret = ldb_register_module(&ldb_entryuuid_module_ops);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	ret = ldb_register_module(&ldb_nsuniqueid_module_ops);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	return LDB_SUCCESS;
}
