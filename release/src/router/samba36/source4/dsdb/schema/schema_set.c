/*
   Unix SMB/CIFS implementation.
   DSDB schema header

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006-2007
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2008

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
#include "lib/util/dlinklist.h"
#include "dsdb/samdb/samdb.h"
#include <ldb_module.h>
#include "param/param.h"
#include "librpc/ndr/libndr.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "lib/util/tsort.h"

/*
  override the name to attribute handler function
 */
const struct ldb_schema_attribute *dsdb_attribute_handler_override(struct ldb_context *ldb,
								   void *private_data,
								   const char *name)
{
	struct dsdb_schema *schema = talloc_get_type_abort(private_data, struct dsdb_schema);
	const struct dsdb_attribute *a = dsdb_attribute_by_lDAPDisplayName(schema, name);
	if (a == NULL) {
		/* this will fall back to ldb internal handling */
		return NULL;
	}
	return a->ldb_schema_attribute;
}

static int dsdb_schema_set_attributes(struct ldb_context *ldb, struct dsdb_schema *schema, bool write_attributes)
{
	int ret = LDB_SUCCESS;
	struct ldb_result *res;
	struct ldb_result *res_idx;
	struct dsdb_attribute *attr;
	struct ldb_message *mod_msg;
	TALLOC_CTX *mem_ctx;
	struct ldb_message *msg;
	struct ldb_message *msg_idx;

	/* setup our own attribute name to schema handler */
	ldb_schema_attribute_set_override_handler(ldb, dsdb_attribute_handler_override, schema);

	if (!write_attributes) {
		return ret;
	}

	mem_ctx = talloc_new(ldb);
	if (!mem_ctx) {
		return ldb_oom(ldb);
	}

	msg = ldb_msg_new(mem_ctx);
	if (!msg) {
		ldb_oom(ldb);
		goto op_error;
	}
	msg_idx = ldb_msg_new(mem_ctx);
	if (!msg_idx) {
		ldb_oom(ldb);
		goto op_error;
	}
	msg->dn = ldb_dn_new(msg, ldb, "@ATTRIBUTES");
	if (!msg->dn) {
		ldb_oom(ldb);
		goto op_error;
	}
	msg_idx->dn = ldb_dn_new(msg_idx, ldb, "@INDEXLIST");
	if (!msg_idx->dn) {
		ldb_oom(ldb);
		goto op_error;
	}

	ret = ldb_msg_add_string(msg_idx, "@IDXONE", "1");
	if (ret != LDB_SUCCESS) {
		goto op_error;
	}

	for (attr = schema->attributes; attr; attr = attr->next) {
		const char *syntax = attr->syntax->ldb_syntax;

		if (!syntax) {
			syntax = attr->syntax->ldap_oid;
		}

		/*
		 * Write out a rough approximation of the schema
		 * as an @ATTRIBUTES value, for bootstrapping
		 */
		if (strcmp(syntax, LDB_SYNTAX_INTEGER) == 0) {
			ret = ldb_msg_add_string(msg, attr->lDAPDisplayName, "INTEGER");
		} else if (strcmp(syntax, LDB_SYNTAX_DIRECTORY_STRING) == 0) {
			ret = ldb_msg_add_string(msg, attr->lDAPDisplayName, "CASE_INSENSITIVE");
		}
		if (ret != LDB_SUCCESS) {
			break;
		}

		if (attr->searchFlags & SEARCH_FLAG_ATTINDEX) {
			ret = ldb_msg_add_string(msg_idx, "@IDXATTR", attr->lDAPDisplayName);
			if (ret != LDB_SUCCESS) {
				break;
			}
		}
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}

	/*
	 * Try to avoid churning the attributes too much,
	 * we only want to do this if they have changed
	 */
	ret = ldb_search(ldb, mem_ctx, &res, msg->dn, LDB_SCOPE_BASE, NULL,
			 NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = ldb_add(ldb, msg);
	} else if (ret != LDB_SUCCESS) {
	} else if (res->count != 1) {
		ret = ldb_add(ldb, msg);
	} else {
		ret = LDB_SUCCESS;
		/* Annoyingly added to our search results */
		ldb_msg_remove_attr(res->msgs[0], "distinguishedName");

		ret = ldb_msg_difference(ldb, mem_ctx,
		                         res->msgs[0], msg, &mod_msg);
		if (ret != LDB_SUCCESS) {
			goto op_error;
		}
		if (mod_msg->num_elements > 0) {
			ret = dsdb_replace(ldb, mod_msg, 0);
		}
		talloc_free(mod_msg);
	}

	if (ret == LDB_ERR_OPERATIONS_ERROR || ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS || ret == LDB_ERR_INVALID_DN_SYNTAX) {
		/* We might be on a read-only DB or LDAP */
		ret = LDB_SUCCESS;
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}

	/* Now write out the indexes, as found in the schema (if they have changed) */

	ret = ldb_search(ldb, mem_ctx, &res_idx, msg_idx->dn, LDB_SCOPE_BASE,
			 NULL, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = ldb_add(ldb, msg_idx);
	} else if (ret != LDB_SUCCESS) {
	} else if (res_idx->count != 1) {
		ret = ldb_add(ldb, msg_idx);
	} else {
		ret = LDB_SUCCESS;
		/* Annoyingly added to our search results */
		ldb_msg_remove_attr(res_idx->msgs[0], "distinguishedName");

		ret = ldb_msg_difference(ldb, mem_ctx,
		                         res_idx->msgs[0], msg_idx, &mod_msg);
		if (ret != LDB_SUCCESS) {
			goto op_error;
		}
		if (mod_msg->num_elements > 0) {
			ret = dsdb_replace(ldb, mod_msg, 0);
		}
		talloc_free(mod_msg);
	}
	if (ret == LDB_ERR_OPERATIONS_ERROR || ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS || ret == LDB_ERR_INVALID_DN_SYNTAX) {
		/* We might be on a read-only DB */
		ret = LDB_SUCCESS;
	}
	talloc_free(mem_ctx);
	return ret;

op_error:
	talloc_free(mem_ctx);
	return ldb_operr(ldb);
}

static int uint32_cmp(uint32_t c1, uint32_t c2)
{
	if (c1 == c2) return 0;
	return c1 > c2 ? 1 : -1;
}

static int dsdb_compare_class_by_lDAPDisplayName(struct dsdb_class **c1, struct dsdb_class **c2)
{
	return strcasecmp((*c1)->lDAPDisplayName, (*c2)->lDAPDisplayName);
}
static int dsdb_compare_class_by_governsID_id(struct dsdb_class **c1, struct dsdb_class **c2)
{
	return uint32_cmp((*c1)->governsID_id, (*c2)->governsID_id);
}
static int dsdb_compare_class_by_governsID_oid(struct dsdb_class **c1, struct dsdb_class **c2)
{
	return strcasecmp((*c1)->governsID_oid, (*c2)->governsID_oid);
}
static int dsdb_compare_class_by_cn(struct dsdb_class **c1, struct dsdb_class **c2)
{
	return strcasecmp((*c1)->cn, (*c2)->cn);
}

static int dsdb_compare_attribute_by_lDAPDisplayName(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return strcasecmp((*a1)->lDAPDisplayName, (*a2)->lDAPDisplayName);
}
static int dsdb_compare_attribute_by_attributeID_id(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return uint32_cmp((*a1)->attributeID_id, (*a2)->attributeID_id);
}
static int dsdb_compare_attribute_by_msDS_IntId(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return uint32_cmp((*a1)->msDS_IntId, (*a2)->msDS_IntId);
}
static int dsdb_compare_attribute_by_attributeID_oid(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return strcasecmp((*a1)->attributeID_oid, (*a2)->attributeID_oid);
}
static int dsdb_compare_attribute_by_linkID(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return uint32_cmp((*a1)->linkID, (*a2)->linkID);
}

/**
 * Clean up Classes and Attributes accessor arrays
 */
static void dsdb_sorted_accessors_free(struct dsdb_schema *schema)
{
	/* free classes accessors */
	TALLOC_FREE(schema->classes_by_lDAPDisplayName);
	TALLOC_FREE(schema->classes_by_governsID_id);
	TALLOC_FREE(schema->classes_by_governsID_oid);
	TALLOC_FREE(schema->classes_by_cn);
	/* free attribute accessors */
	TALLOC_FREE(schema->attributes_by_lDAPDisplayName);
	TALLOC_FREE(schema->attributes_by_attributeID_id);
	TALLOC_FREE(schema->attributes_by_msDS_IntId);
	TALLOC_FREE(schema->attributes_by_attributeID_oid);
	TALLOC_FREE(schema->attributes_by_linkID);
}

/*
  create the sorted accessor arrays for the schema
 */
int dsdb_setup_sorted_accessors(struct ldb_context *ldb,
				struct dsdb_schema *schema)
{
	struct dsdb_class *cur;
	struct dsdb_attribute *a;
	unsigned int i;
	unsigned int num_int_id;

	/* free all caches */
	dsdb_sorted_accessors_free(schema);

	/* count the classes */
	for (i=0, cur=schema->classes; cur; i++, cur=cur->next) /* noop */ ;
	schema->num_classes = i;

	/* setup classes_by_* */
	schema->classes_by_lDAPDisplayName = talloc_array(schema, struct dsdb_class *, i);
	schema->classes_by_governsID_id    = talloc_array(schema, struct dsdb_class *, i);
	schema->classes_by_governsID_oid   = talloc_array(schema, struct dsdb_class *, i);
	schema->classes_by_cn              = talloc_array(schema, struct dsdb_class *, i);
	if (schema->classes_by_lDAPDisplayName == NULL ||
	    schema->classes_by_governsID_id == NULL ||
	    schema->classes_by_governsID_oid == NULL ||
	    schema->classes_by_cn == NULL) {
		goto failed;
	}

	for (i=0, cur=schema->classes; cur; i++, cur=cur->next) {
		schema->classes_by_lDAPDisplayName[i] = cur;
		schema->classes_by_governsID_id[i]    = cur;
		schema->classes_by_governsID_oid[i]   = cur;
		schema->classes_by_cn[i]              = cur;
	}

	/* sort the arrays */
	TYPESAFE_QSORT(schema->classes_by_lDAPDisplayName, schema->num_classes, dsdb_compare_class_by_lDAPDisplayName);
	TYPESAFE_QSORT(schema->classes_by_governsID_id, schema->num_classes, dsdb_compare_class_by_governsID_id);
	TYPESAFE_QSORT(schema->classes_by_governsID_oid, schema->num_classes, dsdb_compare_class_by_governsID_oid);
	TYPESAFE_QSORT(schema->classes_by_cn, schema->num_classes, dsdb_compare_class_by_cn);

	/* now build the attribute accessor arrays */

	/* count the attributes
	 * and attributes with msDS-IntId set */
	num_int_id = 0;
	for (i=0, a=schema->attributes; a; i++, a=a->next) {
		if (a->msDS_IntId != 0) {
			num_int_id++;
		}
	}
	schema->num_attributes = i;
	schema->num_int_id_attr = num_int_id;

	/* setup attributes_by_* */
	schema->attributes_by_lDAPDisplayName = talloc_array(schema, struct dsdb_attribute *, i);
	schema->attributes_by_attributeID_id    = talloc_array(schema, struct dsdb_attribute *, i);
	schema->attributes_by_msDS_IntId        = talloc_array(schema,
	                                                       struct dsdb_attribute *, num_int_id);
	schema->attributes_by_attributeID_oid   = talloc_array(schema, struct dsdb_attribute *, i);
	schema->attributes_by_linkID              = talloc_array(schema, struct dsdb_attribute *, i);
	if (schema->attributes_by_lDAPDisplayName == NULL ||
	    schema->attributes_by_attributeID_id == NULL ||
	    schema->attributes_by_msDS_IntId == NULL ||
	    schema->attributes_by_attributeID_oid == NULL ||
	    schema->attributes_by_linkID == NULL) {
		goto failed;
	}

	num_int_id = 0;
	for (i=0, a=schema->attributes; a; i++, a=a->next) {
		schema->attributes_by_lDAPDisplayName[i] = a;
		schema->attributes_by_attributeID_id[i]    = a;
		schema->attributes_by_attributeID_oid[i]   = a;
		schema->attributes_by_linkID[i]          = a;
		/* append attr-by-msDS-IntId values */
		if (a->msDS_IntId != 0) {
			schema->attributes_by_msDS_IntId[num_int_id] = a;
			num_int_id++;
		}
	}
	SMB_ASSERT(num_int_id == schema->num_int_id_attr);

	/* sort the arrays */
	TYPESAFE_QSORT(schema->attributes_by_lDAPDisplayName, schema->num_attributes, dsdb_compare_attribute_by_lDAPDisplayName);
	TYPESAFE_QSORT(schema->attributes_by_attributeID_id, schema->num_attributes, dsdb_compare_attribute_by_attributeID_id);
	TYPESAFE_QSORT(schema->attributes_by_msDS_IntId, schema->num_int_id_attr, dsdb_compare_attribute_by_msDS_IntId);
	TYPESAFE_QSORT(schema->attributes_by_attributeID_oid, schema->num_attributes, dsdb_compare_attribute_by_attributeID_oid);
	TYPESAFE_QSORT(schema->attributes_by_linkID, schema->num_attributes, dsdb_compare_attribute_by_linkID);

	return LDB_SUCCESS;

failed:
	dsdb_sorted_accessors_free(schema);
	return ldb_oom(ldb);
}

int dsdb_setup_schema_inversion(struct ldb_context *ldb, struct dsdb_schema *schema)
{
	/* Walk the list of schema classes */

	/*  For each subClassOf, add us to subclasses of the parent */

	/* collect these subclasses into a recursive list of total subclasses, preserving order */

	/* For each subclass under 'top', write the index from it's
	 * order as an integer in the dsdb_class (for sorting
	 * objectClass lists efficiently) */

	/* Walk the list of schema classes */

	/*  Create a 'total possible superiors' on each class */
	return LDB_SUCCESS;
}

/**
 * Attach the schema to an opaque pointer on the ldb,
 * so ldb modules can find it
 */
int dsdb_set_schema(struct ldb_context *ldb, struct dsdb_schema *schema)
{
	struct dsdb_schema *old_schema;
	int ret;

	ret = dsdb_setup_sorted_accessors(ldb, schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = schema_fill_constructed(schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	old_schema = ldb_get_opaque(ldb, "dsdb_schema");

	ret = ldb_set_opaque(ldb, "dsdb_schema", schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Remove the reference to the schema we just overwrote - if there was
	 * none, NULL is harmless here */
	if (old_schema != schema) {
		talloc_unlink(ldb, old_schema);
		talloc_steal(ldb, schema);
	}

	ret = ldb_set_opaque(ldb, "dsdb_use_global_schema", NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Set the new attributes based on the new schema */
	ret = dsdb_schema_set_attributes(ldb, schema, true);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return LDB_SUCCESS;
}

/**
 * Global variable to hold one copy of the schema, used to avoid memory bloat
 */
static struct dsdb_schema *global_schema;

/**
 * Make this ldb use a specified schema, already fully calculated and belonging to another ldb
 */
int dsdb_reference_schema(struct ldb_context *ldb, struct dsdb_schema *schema,
			  bool write_attributes)
{
	int ret;
	struct dsdb_schema *old_schema;
	old_schema = ldb_get_opaque(ldb, "dsdb_schema");
	ret = ldb_set_opaque(ldb, "dsdb_schema", schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Remove the reference to the schema we just overwrote - if there was
	 * none, NULL is harmless here */
	talloc_unlink(ldb, old_schema);

	if (talloc_reference(ldb, schema) == NULL) {
		return ldb_oom(ldb);
	}

	/* Make this ldb use local schema preferably */
	ret = ldb_set_opaque(ldb, "dsdb_use_global_schema", NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = dsdb_schema_set_attributes(ldb, schema, write_attributes);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return LDB_SUCCESS;
}

/**
 * Make this ldb use the 'global' schema, setup to avoid having multiple copies in this process
 */
int dsdb_set_global_schema(struct ldb_context *ldb)
{
	int ret;
	void *use_global_schema = (void *)1;
	if (!global_schema) {
		return LDB_SUCCESS;
	}
	ret = ldb_set_opaque(ldb, "dsdb_use_global_schema", use_global_schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Set the new attributes based on the new schema */
	ret = dsdb_schema_set_attributes(ldb, global_schema, false /* Don't write attributes, it's expensive */);
	if (ret == LDB_SUCCESS) {
		/* Keep a reference to this schema, just in case the original copy is replaced */
		if (talloc_reference(ldb, global_schema) == NULL) {
			return ldb_oom(ldb);
		}
	}

	return ret;
}

bool dsdb_uses_global_schema(struct ldb_context *ldb)
{
	return (ldb_get_opaque(ldb, "dsdb_use_global_schema") != NULL);
}

/**
 * Find the schema object for this ldb
 *
 * If reference_ctx is not NULL, then talloc_reference onto that context
 */

struct dsdb_schema *dsdb_get_schema(struct ldb_context *ldb, TALLOC_CTX *reference_ctx)
{
	const void *p;
	struct dsdb_schema *schema_out;
	struct dsdb_schema *schema_in;
	bool use_global_schema;
	TALLOC_CTX *tmp_ctx = talloc_new(reference_ctx);
	if (!tmp_ctx) {
		return NULL;
	}

	/* see if we have a cached copy */
	use_global_schema = dsdb_uses_global_schema(ldb);
	if (use_global_schema) {
		schema_in = global_schema;
	} else {
		p = ldb_get_opaque(ldb, "dsdb_schema");

		schema_in = talloc_get_type(p, struct dsdb_schema);
		if (!schema_in) {
			talloc_free(tmp_ctx);
			return NULL;
		}
	}

	if (schema_in->refresh_fn && !schema_in->refresh_in_progress) {
		if (!talloc_reference(tmp_ctx, schema_in)) {
			/*
			 * ensure that the schema_in->refresh_in_progress
			 * remains valid for the right amount of time
			 */
			talloc_free(tmp_ctx);
			return NULL;
		}
		schema_in->refresh_in_progress = true;
		/* This may change schema, if it needs to reload it from disk */
		schema_out = schema_in->refresh_fn(schema_in->loaded_from_module,
						   schema_in,
						   use_global_schema);
		schema_in->refresh_in_progress = false;
	} else {
		schema_out = schema_in;
	}

	/* This removes the extra reference above */
	talloc_free(tmp_ctx);
	if (!reference_ctx) {
		return schema_out;
	} else {
		return talloc_reference(reference_ctx, schema_out);
	}
}

/**
 * Make the schema found on this ldb the 'global' schema
 */

void dsdb_make_schema_global(struct ldb_context *ldb, struct dsdb_schema *schema)
{
	if (!schema) {
		return;
	}

	if (global_schema) {
		talloc_unlink(talloc_autofree_context(), global_schema);
	}

	/* we want the schema to be around permanently */
	talloc_reparent(ldb, talloc_autofree_context(), schema);
	global_schema = schema;

	/* This calls the talloc_reference() of the global schema back onto the ldb */
	dsdb_set_global_schema(ldb);
}

/**
 * When loading the schema from LDIF files, we don't get the extended DNs.
 *
 * We need to set these up, so that from the moment we start the provision,
 * the defaultObjectCategory links are set up correctly.
 */
int dsdb_schema_fill_extended_dn(struct ldb_context *ldb, struct dsdb_schema *schema)
{
	struct dsdb_class *cur;
	const struct dsdb_class *target_class;
	for (cur = schema->classes; cur; cur = cur->next) {
		const struct ldb_val *rdn;
		struct ldb_val guid;
		NTSTATUS status;
		struct ldb_dn *dn = ldb_dn_new(NULL, ldb, cur->defaultObjectCategory);

		if (!dn) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		rdn = ldb_dn_get_component_val(dn, 0);
		if (!rdn) {
			talloc_free(dn);
			return LDB_ERR_INVALID_DN_SYNTAX;
		}
		target_class = dsdb_class_by_cn_ldb_val(schema, rdn);
		if (!target_class) {
			talloc_free(dn);
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		status = GUID_to_ndr_blob(&target_class->objectGUID, dn, &guid);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(dn);
			return ldb_operr(ldb);
		}
		ldb_dn_set_extended_component(dn, "GUID", &guid);

		cur->defaultObjectCategory = ldb_dn_get_extended_linearized(cur, dn, 1);
		talloc_free(dn);
	}
	return LDB_SUCCESS;
}

/**
 * Add an element to the schema (attribute or class) from an LDB message
 */
WERROR dsdb_schema_set_el_from_ldb_msg(struct ldb_context *ldb, struct dsdb_schema *schema,
				       struct ldb_message *msg)
{
	if (samdb_find_attribute(ldb, msg,
				 "objectclass", "attributeSchema") != NULL) {
		return dsdb_attribute_from_ldb(ldb, schema, msg);
	} else if (samdb_find_attribute(ldb, msg,
				 "objectclass", "classSchema") != NULL) {
		return dsdb_class_from_ldb(schema, msg);
	}

	/* Don't fail on things not classes or attributes */
	return WERR_OK;
}

/**
 * Rather than read a schema from the LDB itself, read it from an ldif
 * file.  This allows schema to be loaded and used while adding the
 * schema itself to the directory.
 */

WERROR dsdb_set_schema_from_ldif(struct ldb_context *ldb, const char *pf, const char *df)
{
	struct ldb_ldif *ldif;
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx;
	WERROR status;
	int ret;
	struct dsdb_schema *schema;
	const struct ldb_val *prefix_val;
	const struct ldb_val *info_val;
	struct ldb_val info_val_default;


	mem_ctx = talloc_new(ldb);
	if (!mem_ctx) {
		goto nomem;
	}

	schema = dsdb_new_schema(mem_ctx);

	schema->fsmo.we_are_master = true;
	schema->fsmo.master_dn = ldb_dn_new_fmt(schema, ldb, "@PROVISION_SCHEMA_MASTER");
	if (!schema->fsmo.master_dn) {
		goto nomem;
	}

	/*
	 * load the prefixMap attribute from pf
	 */
	ldif = ldb_ldif_read_string(ldb, &pf);
	if (!ldif) {
		status = WERR_INVALID_PARAM;
		goto failed;
	}
	talloc_steal(mem_ctx, ldif);

	ret = ldb_msg_normalize(ldb, mem_ctx, ldif->msg, &msg);
	if (ret != LDB_SUCCESS) {
		goto nomem;
	}
	talloc_free(ldif);

	prefix_val = ldb_msg_find_ldb_val(msg, "prefixMap");
	if (!prefix_val) {
	    	status = WERR_INVALID_PARAM;
		goto failed;
	}

	info_val = ldb_msg_find_ldb_val(msg, "schemaInfo");
	if (!info_val) {
		status = dsdb_schema_info_blob_new(mem_ctx, &info_val_default);
		W_ERROR_NOT_OK_GOTO(status, failed);
		info_val = &info_val_default;
	}

	status = dsdb_load_oid_mappings_ldb(schema, prefix_val, info_val);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("ERROR: dsdb_load_oid_mappings_ldb() failed with %s\n", win_errstr(status)));
		goto failed;
	}

	/* load the attribute and class definitions out of df */
	while ((ldif = ldb_ldif_read_string(ldb, &df))) {
		talloc_steal(mem_ctx, ldif);

		ret = ldb_msg_normalize(ldb, ldif, ldif->msg, &msg);
		if (ret != LDB_SUCCESS) {
			goto nomem;
		}

		status = dsdb_schema_set_el_from_ldb_msg(ldb, schema, msg);
		talloc_free(ldif);
		if (!W_ERROR_IS_OK(status)) {
			goto failed;
		}
	}

	ret = dsdb_set_schema(ldb, schema);
	if (ret != LDB_SUCCESS) {
		status = WERR_FOOBAR;
		goto failed;
	}

	ret = dsdb_schema_fill_extended_dn(ldb, schema);
	if (ret != LDB_SUCCESS) {
		status = WERR_FOOBAR;
		goto failed;
	}

	goto done;

nomem:
	status = WERR_NOMEM;
failed:
done:
	talloc_free(mem_ctx);
	return status;
}
