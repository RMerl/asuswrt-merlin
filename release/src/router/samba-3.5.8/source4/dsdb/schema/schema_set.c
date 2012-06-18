/* 
   Unix SMB/CIFS mplementation.
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
#include "lib/ldb/include/ldb_module.h"
#include "param/param.h"

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
		return LDB_ERR_OPERATIONS_ERROR;
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
	msg_idx->dn = ldb_dn_new(msg, ldb, "@INDEXLIST");
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

		/* Write out a rough approximation of the schema as an @ATTRIBUTES value, for bootstrapping */
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

	/* Try to avoid churning the attributes too much - we only want to do this if they have changed */
	ret = ldb_search(ldb, mem_ctx, &res, msg->dn, LDB_SCOPE_BASE, NULL, "dn=%s", ldb_dn_get_linearized(msg->dn));
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = ldb_add(ldb, msg);
	} else if (ret != LDB_SUCCESS) {
	} else if (res->count != 1) {
		ret = ldb_add(ldb, msg);
	} else {
		ret = LDB_SUCCESS;
		/* Annoyingly added to our search results */
		ldb_msg_remove_attr(res->msgs[0], "distinguishedName");
		
		mod_msg = ldb_msg_diff(ldb, res->msgs[0], msg);
		if (mod_msg->num_elements > 0) {
			ret = samdb_replace(ldb, mem_ctx, mod_msg);
		}
	}

	if (ret == LDB_ERR_OPERATIONS_ERROR || ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS) {
		/* We might be on a read-only DB */
		ret = LDB_SUCCESS;
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}

	/* Now write out the indexs, as found in the schema (if they have changed) */

	ret = ldb_search(ldb, mem_ctx, &res_idx, msg_idx->dn, LDB_SCOPE_BASE, NULL, "dn=%s", ldb_dn_get_linearized(msg_idx->dn));
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ret = ldb_add(ldb, msg_idx);
	} else if (ret != LDB_SUCCESS) {
	} else if (res_idx->count != 1) {
		ret = ldb_add(ldb, msg_idx);
	} else {
		ret = LDB_SUCCESS;
		/* Annoyingly added to our search results */
		ldb_msg_remove_attr(res_idx->msgs[0], "distinguishedName");

		mod_msg = ldb_msg_diff(ldb, res_idx->msgs[0], msg_idx);
		if (mod_msg->num_elements > 0) {
			ret = samdb_replace(ldb, mem_ctx, mod_msg);
		}
	}
	if (ret == LDB_ERR_OPERATIONS_ERROR || ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS) {
		/* We might be on a read-only DB */
		ret = LDB_SUCCESS;
	}
	talloc_free(mem_ctx);
	return ret;

op_error:
	talloc_free(mem_ctx);
	return LDB_ERR_OPERATIONS_ERROR;
}

static int dsdb_compare_class_by_lDAPDisplayName(struct dsdb_class **c1, struct dsdb_class **c2)
{
	return strcasecmp((*c1)->lDAPDisplayName, (*c2)->lDAPDisplayName);
}
static int dsdb_compare_class_by_governsID_id(struct dsdb_class **c1, struct dsdb_class **c2)
{
	return (*c1)->governsID_id - (*c2)->governsID_id;
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
	return (*a1)->attributeID_id - (*a2)->attributeID_id;
}
static int dsdb_compare_attribute_by_attributeID_oid(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return strcasecmp((*a1)->attributeID_oid, (*a2)->attributeID_oid);
}
static int dsdb_compare_attribute_by_linkID(struct dsdb_attribute **a1, struct dsdb_attribute **a2)
{
	return (*a1)->linkID - (*a2)->linkID;
}

/*
  create the sorted accessor arrays for the schema
 */
static int dsdb_setup_sorted_accessors(struct ldb_context *ldb,
				       struct dsdb_schema *schema)
{
	struct dsdb_class *cur;
	struct dsdb_attribute *a;
	uint32_t i;

	talloc_free(schema->classes_by_lDAPDisplayName);
	talloc_free(schema->classes_by_governsID_id);
	talloc_free(schema->classes_by_governsID_oid);
	talloc_free(schema->classes_by_cn);

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
	qsort(schema->classes_by_lDAPDisplayName, schema->num_classes, 
	      sizeof(struct dsdb_class *), QSORT_CAST dsdb_compare_class_by_lDAPDisplayName);
	qsort(schema->classes_by_governsID_id, schema->num_classes, 
	      sizeof(struct dsdb_class *), QSORT_CAST dsdb_compare_class_by_governsID_id);
	qsort(schema->classes_by_governsID_oid, schema->num_classes, 
	      sizeof(struct dsdb_class *), QSORT_CAST dsdb_compare_class_by_governsID_oid);
	qsort(schema->classes_by_cn, schema->num_classes, 
	      sizeof(struct dsdb_class *), QSORT_CAST dsdb_compare_class_by_cn);

	/* now build the attribute accessor arrays */
	talloc_free(schema->attributes_by_lDAPDisplayName);
	talloc_free(schema->attributes_by_attributeID_id);
	talloc_free(schema->attributes_by_attributeID_oid);
	talloc_free(schema->attributes_by_linkID);

	/* count the attributes */
	for (i=0, a=schema->attributes; a; i++, a=a->next) /* noop */ ;
	schema->num_attributes = i;

	/* setup attributes_by_* */
	schema->attributes_by_lDAPDisplayName = talloc_array(schema, struct dsdb_attribute *, i);
	schema->attributes_by_attributeID_id    = talloc_array(schema, struct dsdb_attribute *, i);
	schema->attributes_by_attributeID_oid   = talloc_array(schema, struct dsdb_attribute *, i);
	schema->attributes_by_linkID              = talloc_array(schema, struct dsdb_attribute *, i);
	if (schema->attributes_by_lDAPDisplayName == NULL ||
	    schema->attributes_by_attributeID_id == NULL ||
	    schema->attributes_by_attributeID_oid == NULL ||
	    schema->attributes_by_linkID == NULL) {
		goto failed;
	}

	for (i=0, a=schema->attributes; a; i++, a=a->next) {
		schema->attributes_by_lDAPDisplayName[i] = a;
		schema->attributes_by_attributeID_id[i]    = a;
		schema->attributes_by_attributeID_oid[i]   = a;
		schema->attributes_by_linkID[i]          = a;
	}

	/* sort the arrays */
	qsort(schema->attributes_by_lDAPDisplayName, schema->num_attributes, 
	      sizeof(struct dsdb_attribute *), QSORT_CAST dsdb_compare_attribute_by_lDAPDisplayName);
	qsort(schema->attributes_by_attributeID_id, schema->num_attributes, 
	      sizeof(struct dsdb_attribute *), QSORT_CAST dsdb_compare_attribute_by_attributeID_id);
	qsort(schema->attributes_by_attributeID_oid, schema->num_attributes, 
	      sizeof(struct dsdb_attribute *), QSORT_CAST dsdb_compare_attribute_by_attributeID_oid);
	qsort(schema->attributes_by_linkID, schema->num_attributes, 
	      sizeof(struct dsdb_attribute *), QSORT_CAST dsdb_compare_attribute_by_linkID);

	return LDB_SUCCESS;

failed:
	schema->classes_by_lDAPDisplayName = NULL;
	schema->classes_by_governsID_id = NULL;
	schema->classes_by_governsID_oid = NULL;
	schema->classes_by_cn = NULL;
	schema->attributes_by_lDAPDisplayName = NULL;
	schema->attributes_by_attributeID_id = NULL;
	schema->attributes_by_attributeID_oid = NULL;
	schema->attributes_by_linkID = NULL;
	ldb_oom(ldb);
	return LDB_ERR_OPERATIONS_ERROR;
}

int dsdb_setup_schema_inversion(struct ldb_context *ldb, struct dsdb_schema *schema)
{
	/* Walk the list of schema classes */

	/*  For each subClassOf, add us to subclasses of the parent */

	/* collect these subclasses into a recursive list of total subclasses, preserving order */

	/* For each subclass under 'top', write the index from it's
	 * order as an integer in the dsdb_class (for sorting
	 * objectClass lists efficiently) */

	/* Walk the list of scheam classes */
	
	/*  Create a 'total possible superiors' on each class */
	return LDB_SUCCESS;
}

/**
 * Attach the schema to an opaque pointer on the ldb, so ldb modules
 * can find it 
 */

int dsdb_set_schema(struct ldb_context *ldb, struct dsdb_schema *schema)
{
	int ret;

	ret = dsdb_setup_sorted_accessors(ldb, schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	schema_fill_constructed(schema);

	ret = ldb_set_opaque(ldb, "dsdb_schema", schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Set the new attributes based on the new schema */
	ret = dsdb_schema_set_attributes(ldb, schema, true);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	talloc_steal(ldb, schema);

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
	ret = ldb_set_opaque(ldb, "dsdb_schema", schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Set the new attributes based on the new schema */
	ret = dsdb_schema_set_attributes(ldb, schema, write_attributes);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* Keep a reference to this schema, just incase the original copy is replaced */
	if (talloc_reference(ldb, schema) == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

/**
 * Make this ldb use the 'global' schema, setup to avoid having multiple copies in this process
 */
int dsdb_set_global_schema(struct ldb_context *ldb)
{
	if (!global_schema) {
		return LDB_SUCCESS;
	}

	return dsdb_reference_schema(ldb, global_schema, false /* Don't write attributes, it's expensive */);
}

/**
 * Find the schema object for this ldb
 */

struct dsdb_schema *dsdb_get_schema(struct ldb_context *ldb)
{
	const void *p;
	struct dsdb_schema *schema;

	/* see if we have a cached copy */
	p = ldb_get_opaque(ldb, "dsdb_schema");
	if (!p) {
		return NULL;
	}

	schema = talloc_get_type(p, struct dsdb_schema);
	if (!schema) {
		return NULL;
	}

	return schema;
}

/**
 * Make the schema found on this ldb the 'global' schema
 */

void dsdb_make_schema_global(struct ldb_context *ldb)
{
	struct dsdb_schema *schema = dsdb_get_schema(ldb);
	if (!schema) {
		return;
	}

	if (global_schema) {
		talloc_unlink(talloc_autofree_context(), global_schema);
	}

	/* we want the schema to be around permanently */
	talloc_reparent(talloc_parent(schema), talloc_autofree_context(), schema);

	global_schema = schema;

	dsdb_set_global_schema(ldb);
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

	schema = dsdb_new_schema(mem_ctx, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")));

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

	msg = ldb_msg_canonicalize(ldb, ldif->msg);
	if (!msg) {
		goto nomem;
	}
	talloc_steal(mem_ctx, msg);
	talloc_free(ldif);

	prefix_val = ldb_msg_find_ldb_val(msg, "prefixMap");
	if (!prefix_val) {
	    	status = WERR_INVALID_PARAM;
		goto failed;
	}

	info_val = ldb_msg_find_ldb_val(msg, "schemaInfo");
	if (!info_val) {
		info_val_default = strhex_to_data_blob(mem_ctx, "FF0000000000000000000000000000000000000000");
		if (!info_val_default.data) {
			goto nomem;
		}
		info_val = &info_val_default;
	}

	status = dsdb_load_oid_mappings_ldb(schema, prefix_val, info_val);
	if (!W_ERROR_IS_OK(status)) {
		goto failed;
	}

	/*
	 * load the attribute and class definitions outof df
	 */
	while ((ldif = ldb_ldif_read_string(ldb, &df))) {
		bool is_sa;
		bool is_sc;

		talloc_steal(mem_ctx, ldif);

		msg = ldb_msg_canonicalize(ldb, ldif->msg);
		if (!msg) {
			goto nomem;
		}

		talloc_steal(mem_ctx, msg);
		talloc_free(ldif);

		is_sa = ldb_msg_check_string_attribute(msg, "objectClass", "attributeSchema");
		is_sc = ldb_msg_check_string_attribute(msg, "objectClass", "classSchema");

		if (is_sa) {
			struct dsdb_attribute *sa;

			sa = talloc_zero(schema, struct dsdb_attribute);
			if (!sa) {
				goto nomem;
			}

			status = dsdb_attribute_from_ldb(ldb, schema, msg, sa, sa);
			if (!W_ERROR_IS_OK(status)) {
				goto failed;
			}

			DLIST_ADD(schema->attributes, sa);
		} else if (is_sc) {
			struct dsdb_class *sc;

			sc = talloc_zero(schema, struct dsdb_class);
			if (!sc) {
				goto nomem;
			}

			status = dsdb_class_from_ldb(schema, msg, sc, sc);
			if (!W_ERROR_IS_OK(status)) {
				goto failed;
			}

			DLIST_ADD(schema->classes, sc);
		}
	}

	ret = dsdb_set_schema(ldb, schema);
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
