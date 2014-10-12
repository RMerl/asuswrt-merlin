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
#include "dsdb/samdb/samdb.h"
#include "lib/util/binsearch.h"
#include "lib/util/tsort.h"

static const char **dsdb_full_attribute_list_internal(TALLOC_CTX *mem_ctx, 
						      const struct dsdb_schema *schema, 
						      const char **class_list,
						      enum dsdb_attr_list_query query);

static int uint32_cmp(uint32_t c1, uint32_t c2) 
{
	if (c1 == c2) return 0;
	return c1 > c2 ? 1 : -1;
}

static int strcasecmp_with_ldb_val(const struct ldb_val *target, const char *str)
{
	int ret = strncasecmp((const char *)target->data, str, target->length);
	if (ret == 0) {
		size_t len = strlen(str);
		if (target->length > len) {
			if (target->data[len] == 0) {
				return 0;
			}
			return 1;
		}
		return (target->length - len);
	}
	return ret;
}

const struct dsdb_attribute *dsdb_attribute_by_attributeID_id(const struct dsdb_schema *schema,
							      uint32_t id)
{
	struct dsdb_attribute *c;

	/*
	 * 0xFFFFFFFF is used as value when no mapping table is available,
	 * so don't try to match with it
	 */
	if (id == 0xFFFFFFFF) return NULL;

	/* check for msDS-IntId type attribute */
	if (dsdb_pfm_get_attid_type(id) == DSDB_ATTID_TYPE_INTID) {
		BINARY_ARRAY_SEARCH_P(schema->attributes_by_msDS_IntId,
				      schema->num_int_id_attr, msDS_IntId, id, uint32_cmp, c);
		return c;
	}

	BINARY_ARRAY_SEARCH_P(schema->attributes_by_attributeID_id,
			      schema->num_attributes, attributeID_id, id, uint32_cmp, c);
	return c;
}

const struct dsdb_attribute *dsdb_attribute_by_attributeID_oid(const struct dsdb_schema *schema,
							       const char *oid)
{
	struct dsdb_attribute *c;

	if (!oid) return NULL;

	BINARY_ARRAY_SEARCH_P(schema->attributes_by_attributeID_oid,
			      schema->num_attributes, attributeID_oid, oid, strcasecmp, c);
	return c;
}

const struct dsdb_attribute *dsdb_attribute_by_lDAPDisplayName(const struct dsdb_schema *schema,
							       const char *name)
{
	struct dsdb_attribute *c;

	if (!name) return NULL;

	BINARY_ARRAY_SEARCH_P(schema->attributes_by_lDAPDisplayName,
			      schema->num_attributes, lDAPDisplayName, name, strcasecmp, c);
	return c;
}

const struct dsdb_attribute *dsdb_attribute_by_lDAPDisplayName_ldb_val(const struct dsdb_schema *schema,
								       const struct ldb_val *name)
{
	struct dsdb_attribute *a;

	if (!name) return NULL;

	BINARY_ARRAY_SEARCH_P(schema->attributes_by_lDAPDisplayName,
			      schema->num_attributes, lDAPDisplayName, name, strcasecmp_with_ldb_val, a);
	return a;
}

const struct dsdb_attribute *dsdb_attribute_by_linkID(const struct dsdb_schema *schema,
						      int linkID)
{
	struct dsdb_attribute *c;

	BINARY_ARRAY_SEARCH_P(schema->attributes_by_linkID,
			      schema->num_attributes, linkID, linkID, uint32_cmp, c);
	return c;
}

const struct dsdb_class *dsdb_class_by_governsID_id(const struct dsdb_schema *schema,
						    uint32_t id)
{
	struct dsdb_class *c;

	/*
	 * 0xFFFFFFFF is used as value when no mapping table is available,
	 * so don't try to match with it
	 */
	if (id == 0xFFFFFFFF) return NULL;

	BINARY_ARRAY_SEARCH_P(schema->classes_by_governsID_id,
			      schema->num_classes, governsID_id, id, uint32_cmp, c);
	return c;
}

const struct dsdb_class *dsdb_class_by_governsID_oid(const struct dsdb_schema *schema,
						     const char *oid)
{
	struct dsdb_class *c;
	if (!oid) return NULL;
	BINARY_ARRAY_SEARCH_P(schema->classes_by_governsID_oid,
			      schema->num_classes, governsID_oid, oid, strcasecmp, c);
	return c;
}

const struct dsdb_class *dsdb_class_by_lDAPDisplayName(const struct dsdb_schema *schema,
						       const char *name)
{
	struct dsdb_class *c;
	if (!name) return NULL;
	BINARY_ARRAY_SEARCH_P(schema->classes_by_lDAPDisplayName,
			      schema->num_classes, lDAPDisplayName, name, strcasecmp, c);
	return c;
}

const struct dsdb_class *dsdb_class_by_lDAPDisplayName_ldb_val(const struct dsdb_schema *schema,
							       const struct ldb_val *name)
{
	struct dsdb_class *c;
	if (!name) return NULL;
	BINARY_ARRAY_SEARCH_P(schema->classes_by_lDAPDisplayName,
			      schema->num_classes, lDAPDisplayName, name, strcasecmp_with_ldb_val, c);
	return c;
}

const struct dsdb_class *dsdb_class_by_cn(const struct dsdb_schema *schema,
					  const char *cn)
{
	struct dsdb_class *c;
	if (!cn) return NULL;
	BINARY_ARRAY_SEARCH_P(schema->classes_by_cn,
			      schema->num_classes, cn, cn, strcasecmp, c);
	return c;
}

const struct dsdb_class *dsdb_class_by_cn_ldb_val(const struct dsdb_schema *schema,
						  const struct ldb_val *cn)
{
	struct dsdb_class *c;
	if (!cn) return NULL;
	BINARY_ARRAY_SEARCH_P(schema->classes_by_cn,
			      schema->num_classes, cn, cn, strcasecmp_with_ldb_val, c);
	return c;
}

const char *dsdb_lDAPDisplayName_by_id(const struct dsdb_schema *schema,
				       uint32_t id)
{
	const struct dsdb_attribute *a;
	const struct dsdb_class *c;

	a = dsdb_attribute_by_attributeID_id(schema, id);
	if (a) {
		return a->lDAPDisplayName;
	}

	c = dsdb_class_by_governsID_id(schema, id);
	if (c) {
		return c->lDAPDisplayName;
	}

	return NULL;
}

/** 
    Return a list of linked attributes, in lDAPDisplayName format.

    This may be used to determine if a modification would require
    backlinks to be updated, for example
*/

WERROR dsdb_linked_attribute_lDAPDisplayName_list(const struct dsdb_schema *schema, TALLOC_CTX *mem_ctx, const char ***attr_list_ret)
{
	const char **attr_list = NULL;
	struct dsdb_attribute *cur;
	unsigned int i = 0;
	for (cur = schema->attributes; cur; cur = cur->next) {
		if (cur->linkID == 0) continue;
		
		attr_list = talloc_realloc(mem_ctx, attr_list, const char *, i+2);
		if (!attr_list) {
			return WERR_NOMEM;
		}
		attr_list[i] = cur->lDAPDisplayName;
		i++;
	}
	attr_list[i] = NULL;
	*attr_list_ret = attr_list;
	return WERR_OK;
}

const char **merge_attr_list(TALLOC_CTX *mem_ctx, 
		       const char **attrs, const char * const*new_attrs) 
{
	const char **ret_attrs;
	unsigned int i;
	size_t new_len, orig_len = str_list_length(attrs);
	if (!new_attrs) {
		return attrs;
	}

	ret_attrs = talloc_realloc(mem_ctx, 
				   attrs, const char *, orig_len + str_list_length(new_attrs) + 1);
	if (ret_attrs) {
		for (i=0; i < str_list_length(new_attrs); i++) {
			ret_attrs[orig_len + i] = new_attrs[i];
		}
		new_len = orig_len + str_list_length(new_attrs);

		ret_attrs[new_len] = NULL;
	}

	return ret_attrs;
}

/*
  Return a merged list of the attributes of exactly one class (not
  considering subclasses, auxillary classes etc)
*/

const char **dsdb_attribute_list(TALLOC_CTX *mem_ctx, const struct dsdb_class *sclass, enum dsdb_attr_list_query query)
{
	const char **attr_list = NULL;
	switch (query) {
	case DSDB_SCHEMA_ALL_MAY:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->mayContain);
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->systemMayContain);
		break;
		
	case DSDB_SCHEMA_ALL_MUST:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->mustContain);
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->systemMustContain);
		break;
		
	case DSDB_SCHEMA_SYS_MAY:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->systemMayContain);
		break;
		
	case DSDB_SCHEMA_SYS_MUST:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->systemMustContain);
		break;
		
	case DSDB_SCHEMA_MAY:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->mayContain);
		break;
		
	case DSDB_SCHEMA_MUST:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->mustContain);
		break;
		
	case DSDB_SCHEMA_ALL:
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->mayContain);
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->systemMayContain);
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->mustContain);
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass->systemMustContain);
		break;
	}
	return attr_list;
}

static const char **attribute_list_from_class(TALLOC_CTX *mem_ctx,
					      const struct dsdb_schema *schema, 
					      const struct dsdb_class *sclass,
					      enum dsdb_attr_list_query query) 
{
	const char **this_class_list;
	const char **system_recursive_list;
	const char **recursive_list;
	const char **attr_list;

	this_class_list = dsdb_attribute_list(mem_ctx, sclass, query);
	
	recursive_list = dsdb_full_attribute_list_internal(mem_ctx, schema, 
							   sclass->systemAuxiliaryClass,
							   query);
	
	system_recursive_list = dsdb_full_attribute_list_internal(mem_ctx, schema, 
								  sclass->auxiliaryClass,
								  query);
	
	attr_list = this_class_list;
	attr_list = merge_attr_list(mem_ctx, attr_list, recursive_list);
	attr_list = merge_attr_list(mem_ctx, attr_list, system_recursive_list);
	return attr_list;
}

/* Return a full attribute list for a given class list (as a ldb_message_element)

   Via attribute_list_from_class() this calls itself when recursing on auxiliary classes
 */
static const char **dsdb_full_attribute_list_internal(TALLOC_CTX *mem_ctx, 
						      const struct dsdb_schema *schema, 
						      const char **class_list,
						      enum dsdb_attr_list_query query)
{
	unsigned int i;
	const char **attr_list = NULL;

	for (i=0; class_list && class_list[i]; i++) {
		const char **sclass_list
			= attribute_list_from_class(mem_ctx, schema,
						    dsdb_class_by_lDAPDisplayName(schema, class_list[i]),
						    query);

		attr_list = merge_attr_list(mem_ctx, attr_list, sclass_list);
	}
	return attr_list;
}

/* Return a full attribute list for a given class list (as a ldb_message_element)

   Using the ldb_message_element ensures we do length-limited
   comparisons, rather than casting the possibly-unterminated string

   Via attribute_list_from_class() this calls 
   dsdb_full_attribute_list_internal() when recursing on auxiliary classes
 */
static const char **dsdb_full_attribute_list_internal_el(TALLOC_CTX *mem_ctx, 
							 const struct dsdb_schema *schema, 
							 const struct ldb_message_element *el,
							 enum dsdb_attr_list_query query)
{
	unsigned int i;
	const char **attr_list = NULL;

	for (i=0; i < el->num_values; i++) {
		const char **sclass_list
			= attribute_list_from_class(mem_ctx, schema,
						    dsdb_class_by_lDAPDisplayName_ldb_val(schema, &el->values[i]),
						    query);
		
		attr_list = merge_attr_list(mem_ctx, attr_list, sclass_list);
	}
	return attr_list;
}

static int qsort_string(const char **s1, const char **s2)
{
	return strcasecmp(*s1, *s2);
}

/* Helper function to remove duplicates from the attribute list to be returned */
static const char **dedup_attr_list(const char **attr_list) 
{
	size_t new_len = str_list_length(attr_list);
	/* Remove duplicates */
	if (new_len > 1) {
		size_t i;
		TYPESAFE_QSORT(attr_list, new_len, qsort_string);
		
		for (i=1; i < new_len; i++) {
			const char **val1 = &attr_list[i-1];
			const char **val2 = &attr_list[i];
			if (ldb_attr_cmp(*val1, *val2) == 0) {
				memmove(val1, val2, (new_len - i) * sizeof( *attr_list)); 
				attr_list[new_len-1] = NULL;
				new_len--;
				i--;
			}
		}
	}
	return attr_list;
}

/* Return a full attribute list for a given class list (as a ldb_message_element)

   Using the ldb_message_element ensures we do length-limited
   comparisons, rather than casting the possibly-unterminated string

   The result contains only unique values
 */
const char **dsdb_full_attribute_list(TALLOC_CTX *mem_ctx, 
				      const struct dsdb_schema *schema, 
				      const struct ldb_message_element *class_list,
				      enum dsdb_attr_list_query query)
{
	const char **attr_list = dsdb_full_attribute_list_internal_el(mem_ctx, schema, class_list, query);
	return dedup_attr_list(attr_list);
}

/* Return the schemaIDGUID of a class */

const struct GUID *class_schemaid_guid_by_lDAPDisplayName(const struct dsdb_schema *schema,
                                                          const char *name)
{
        const struct dsdb_class *object_class = dsdb_class_by_lDAPDisplayName(schema, name);
        if (!object_class)
                return NULL;

        return &object_class->schemaIDGUID;
}

const struct GUID *attribute_schemaid_guid_by_lDAPDisplayName(const struct dsdb_schema *schema,
							      const char *name)
{
        const struct dsdb_attribute *attr = dsdb_attribute_by_lDAPDisplayName(schema, name);
        if (!attr)
                return NULL;

        return &attr->schemaIDGUID;
}
