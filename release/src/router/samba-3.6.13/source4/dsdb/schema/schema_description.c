/* 
   Unix SMB/CIFS mplementation.
   Print schema info into string format
   
   Copyright (C) Andrew Bartlett 2006-2008
    
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
#include "librpc/ndr/libndr.h"

#define IF_NULL_FAIL_RET(x) do {     \
		if (!x) {		\
			return NULL;	\
		}			\
	} while (0) 


char *schema_attribute_description(TALLOC_CTX *mem_ctx, 
					  enum dsdb_schema_convert_target target,
					  const char *seperator,
					  const char *oid, 
					  const char *name,
					  const char *equality, 
					  const char *substring, 
					  const char *syntax,
					  bool single_value, bool operational,
					  uint32_t *range_lower,
					  uint32_t *range_upper,
					  const char *property_guid,
					  const char *property_set_guid,
					  bool indexed, bool system_only)
{
	char *schema_entry = talloc_asprintf(mem_ctx, 
					     "(%s%s%s", seperator, oid, seperator);
	
	schema_entry = talloc_asprintf_append(schema_entry, 
					      "NAME '%s'%s", name, seperator);
	IF_NULL_FAIL_RET(schema_entry);
	
	if (equality) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "EQUALITY %s%s", equality, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}
	if (substring) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "SUBSTR %s%s", substring, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (syntax) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "SYNTAX %s%s", syntax, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (single_value) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "SINGLE-VALUE%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}
	
	if (operational) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "NO-USER-MODIFICATION%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (range_lower) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "RANGE-LOWER '%u'%s",
						      *range_lower, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (range_upper) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "RANGE-UPPER '%u'%s",
						      *range_upper, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (property_guid) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "PROPERTY-GUID '%s'%s",
						      property_guid, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (property_set_guid) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "PROPERTY-SET-GUID '%s'%s",
						      property_set_guid, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (indexed) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "INDEXED%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (system_only) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "SYSTEM-ONLY%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	schema_entry = talloc_asprintf_append(schema_entry, 
					      ")");
	return schema_entry;
}

char *schema_attribute_to_description(TALLOC_CTX *mem_ctx, const struct dsdb_attribute *attribute) 
{
	char *schema_description;
	const char *syntax = attribute->syntax->ldap_oid;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NULL;
	}

	schema_description 
		= schema_attribute_description(mem_ctx, 
					       TARGET_AD_SCHEMA_SUBENTRY,
					       " ",
					       attribute->attributeID_oid,
					       attribute->lDAPDisplayName,
					       NULL, NULL, talloc_asprintf(tmp_ctx, "'%s'", syntax),
					       attribute->isSingleValued,
					       attribute->systemOnly,/* TODO: is this correct? */
					       NULL, NULL, NULL, NULL,
					       false, false);
	talloc_free(tmp_ctx);
	return schema_description;
}

char *schema_attribute_to_extendedInfo(TALLOC_CTX *mem_ctx, const struct dsdb_attribute *attribute)
{
	char *schema_description;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NULL;
	}

	schema_description
		= schema_attribute_description(mem_ctx,
					       TARGET_AD_SCHEMA_SUBENTRY,
					       " ",
					       attribute->attributeID_oid,
					       attribute->lDAPDisplayName,
					       NULL, NULL, NULL,
					       false, false,
					       attribute->rangeLower,
					       attribute->rangeUpper,
					       GUID_hexstring(tmp_ctx, &attribute->schemaIDGUID),
					       GUID_hexstring(tmp_ctx, &attribute->attributeSecurityGUID),
					       (attribute->searchFlags & SEARCH_FLAG_ATTINDEX),
					       attribute->systemOnly);
	talloc_free(tmp_ctx);
	return schema_description;
}

#define APPEND_ATTRS(attributes)				\
	do {								\
		unsigned int k;						\
		for (k=0; attributes && attributes[k]; k++) {		\
			const char *attr_name = attributes[k];		\
									\
			schema_entry = talloc_asprintf_append(schema_entry, \
							      "%s ",	\
							      attr_name); \
			IF_NULL_FAIL_RET(schema_entry);			\
			if (attributes[k+1]) {				\
				IF_NULL_FAIL_RET(schema_entry);		\
				if (target == TARGET_OPENLDAP && ((k+1)%5 == 0)) { \
					schema_entry = talloc_asprintf_append(schema_entry, \
									      "$%s ", seperator); \
					IF_NULL_FAIL_RET(schema_entry);	\
				} else {				\
					schema_entry = talloc_asprintf_append(schema_entry, \
									      "$ "); \
				}					\
			}						\
		}							\
	} while (0)
	

/* Print a schema class or dITContentRule as a string.  
 *
 * To print a scheam class, specify objectClassCategory but not auxillary_classes
 * To print a dITContentRule, specify auxillary_classes but set objectClassCategory == -1
 *
 */

char *schema_class_description(TALLOC_CTX *mem_ctx, 
			       enum dsdb_schema_convert_target target,
			       const char *seperator,
			       const char *oid, 
			       const char *name,
			       const char **auxillary_classes,
			       const char *subClassOf,
			       int objectClassCategory,
			       const char **must,
			       const char **may,
			       const char *schemaHexGUID)
{
	char *schema_entry = talloc_asprintf(mem_ctx, 
					     "(%s%s%s", seperator, oid, seperator);
	
	IF_NULL_FAIL_RET(schema_entry);

	schema_entry = talloc_asprintf_append(schema_entry, 
					      "NAME '%s'%s", name, seperator);
	IF_NULL_FAIL_RET(schema_entry);
	
	if (auxillary_classes) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "AUX ( ");
		IF_NULL_FAIL_RET(schema_entry);
		
		APPEND_ATTRS(auxillary_classes);
		
		schema_entry = talloc_asprintf_append(schema_entry, 
						      ")%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (subClassOf && strcasecmp(subClassOf, name) != 0) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "SUP %s%s", subClassOf, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}
	
	switch (objectClassCategory) {
	case -1:
		break;
		/* Dummy case for when used for printing ditContentRules */
	case 0:
		/*
		 * NOTE: this is an type 88 class
		 *       e.g. 2.5.6.6 NAME 'person'
		 *	 but w2k3 gives STRUCTURAL here!
		 */
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "STRUCTURAL%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
		break;
	case 1:
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "STRUCTURAL%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
		break;
	case 2:
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "ABSTRACT%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
		break;
	case 3:
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "AUXILIARY%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
		break;
	}
	
	if (must) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "MUST (%s", target == TARGET_AD_SCHEMA_SUBENTRY ? "" : " ");
		IF_NULL_FAIL_RET(schema_entry);
		
		APPEND_ATTRS(must);
		
		schema_entry = talloc_asprintf_append(schema_entry, 
						      ")%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}
	
	if (may) {
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "MAY (%s", target == TARGET_AD_SCHEMA_SUBENTRY ? "" : " ");
		IF_NULL_FAIL_RET(schema_entry);
		
		APPEND_ATTRS(may);
		
		schema_entry = talloc_asprintf_append(schema_entry, 
						      ")%s", seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	if (schemaHexGUID) {
		schema_entry = talloc_asprintf_append(schema_entry,
						      "CLASS-GUID '%s'%s",
						      schemaHexGUID, seperator);
		IF_NULL_FAIL_RET(schema_entry);
	}

	schema_entry = talloc_asprintf_append(schema_entry, 
					      ")");
	return schema_entry;
}

char *schema_class_to_description(TALLOC_CTX *mem_ctx, const struct dsdb_class *sclass)
{
	char *schema_description;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NULL;
	}
	
	schema_description
		= schema_class_description(mem_ctx, 
					   TARGET_AD_SCHEMA_SUBENTRY,
					   " ",
					   sclass->governsID_oid,
					   sclass->lDAPDisplayName,
					   NULL, 
					   sclass->subClassOf,
					   sclass->objectClassCategory,
					   dsdb_attribute_list(tmp_ctx, 
							       sclass, DSDB_SCHEMA_ALL_MUST),
					   dsdb_attribute_list(tmp_ctx, 
							       sclass, DSDB_SCHEMA_ALL_MAY),
					   NULL);
	talloc_free(tmp_ctx);
	return schema_description;
}

char *schema_class_to_dITContentRule(TALLOC_CTX *mem_ctx, const struct dsdb_class *sclass,
				     const struct dsdb_schema *schema)
{
	unsigned int i;
	char *schema_description;
	const char **aux_class_list = NULL;
	const char **attrs;
	const char **must_attr_list = NULL;
	const char **may_attr_list = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	const struct dsdb_class *aux_class;
	if (!tmp_ctx) {
		return NULL;
	}

	aux_class_list = merge_attr_list(tmp_ctx, aux_class_list, sclass->systemAuxiliaryClass);
	aux_class_list = merge_attr_list(tmp_ctx, aux_class_list, sclass->auxiliaryClass);

	for (i=0; aux_class_list && aux_class_list[i]; i++) {
		aux_class = dsdb_class_by_lDAPDisplayName(schema, aux_class_list[i]);
		
		attrs = dsdb_attribute_list(mem_ctx, aux_class, DSDB_SCHEMA_ALL_MUST);
		must_attr_list = merge_attr_list(mem_ctx, must_attr_list, attrs);

		attrs = dsdb_attribute_list(mem_ctx, aux_class, DSDB_SCHEMA_ALL_MAY);
		may_attr_list = merge_attr_list(mem_ctx, may_attr_list, attrs);
	}

	schema_description
		= schema_class_description(mem_ctx, 
					   TARGET_AD_SCHEMA_SUBENTRY,
					   " ",
					   sclass->governsID_oid,
					   sclass->lDAPDisplayName,
					   (const char **)aux_class_list,
					   NULL, /* Must not specify a
						  * SUP (subclass) in
						  * ditContentRules
						  * per MS-ADTS
						  * 3.1.1.3.1.1.1 */
					   -1, must_attr_list, may_attr_list,
					   NULL);
	talloc_free(tmp_ctx);
	return schema_description;
}

char *schema_class_to_extendedInfo(TALLOC_CTX *mem_ctx, const struct dsdb_class *sclass)
{
	char *schema_description = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return NULL;
	}

	schema_description
		= schema_class_description(mem_ctx,
					   TARGET_AD_SCHEMA_SUBENTRY,
					   " ",
					   sclass->governsID_oid,
					   sclass->lDAPDisplayName,
					   NULL,
					   NULL, /* Must not specify a
						  * SUP (subclass) in
						  * ditContentRules
						  * per MS-ADTS
						  * 3.1.1.3.1.1.1 */
					   -1, NULL, NULL,
					   GUID_hexstring(tmp_ctx, &sclass->schemaIDGUID));
	talloc_free(tmp_ctx);
	return schema_description;
}


