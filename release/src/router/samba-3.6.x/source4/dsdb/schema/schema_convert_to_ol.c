/* 
   schema conversion routines

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
#include "ldb.h"
#include "dsdb/samdb/samdb.h"
#include "system/locale.h"

#define SEPERATOR "\n  "

struct attr_map {
	char *old_attr;
	char *new_attr;
};

struct oid_map {
	char *old_oid;
	char *new_oid;
};

static char *print_schema_recursive(char *append_to_string, struct dsdb_schema *schema, const char *print_class,
				    enum dsdb_schema_convert_target target, 
				    const char **attrs_skip, const struct attr_map *attr_map, const struct oid_map *oid_map) 
{
	char *out = append_to_string;
	const struct dsdb_class *objectclass;
	objectclass = dsdb_class_by_lDAPDisplayName(schema, print_class);
	if (!objectclass) {
		DEBUG(0, ("Cannot find class %s in schema\n", print_class));
		return NULL;
	}

	do {
		TALLOC_CTX *mem_ctx = talloc_new(append_to_string);
		const char *name = objectclass->lDAPDisplayName;
		const char *oid = objectclass->governsID_oid;
		const char *subClassOf = objectclass->subClassOf;
		int objectClassCategory = objectclass->objectClassCategory;
		const char **must;
		const char **may;
		char *schema_entry = NULL;
		struct ldb_val objectclass_name_as_ldb_val = data_blob_string_const(objectclass->lDAPDisplayName);
		struct ldb_message_element objectclass_name_as_el = {
			.name = "objectClass",
			.num_values = 1,
			.values = &objectclass_name_as_ldb_val
		};
		unsigned int j;
		unsigned int attr_idx;
		
		if (!mem_ctx) {
			DEBUG(0, ("Failed to create new talloc context\n"));
			return NULL;
		}

		/* We have been asked to skip some attributes/objectClasses */
		if (attrs_skip && str_list_check_ci(attrs_skip, name)) {
			continue;
		}

		/* We might have been asked to remap this oid, due to a conflict */
		for (j=0; oid_map && oid_map[j].old_oid; j++) {
			if (strcasecmp(oid, oid_map[j].old_oid) == 0) {
				oid =  oid_map[j].new_oid;
				break;
			}
		}
		
		/* We might have been asked to remap this name, due to a conflict */
		for (j=0; name && attr_map && attr_map[j].old_attr; j++) {
			if (strcasecmp(name, attr_map[j].old_attr) == 0) {
				name =  attr_map[j].new_attr;
				break;
			}
		}
		
		/* We might have been asked to remap this subClassOf, due to a conflict */
		for (j=0; subClassOf && attr_map && attr_map[j].old_attr; j++) {
			if (strcasecmp(subClassOf, attr_map[j].old_attr) == 0) {
				subClassOf =  attr_map[j].new_attr;
				break;
			}
		}
		
		may = dsdb_full_attribute_list(mem_ctx, schema, &objectclass_name_as_el, DSDB_SCHEMA_ALL_MAY);

		for (j=0; may && may[j]; j++) {
			/* We might have been asked to remap this name, due to a conflict */ 
			for (attr_idx=0; attr_map && attr_map[attr_idx].old_attr; attr_idx++) { 
				if (strcasecmp(may[j], attr_map[attr_idx].old_attr) == 0) { 
					may[j] =  attr_map[attr_idx].new_attr; 
					break;				
				}					
			}						
		}

		must = dsdb_full_attribute_list(mem_ctx, schema, &objectclass_name_as_el, DSDB_SCHEMA_ALL_MUST);

		for (j=0; must && must[j]; j++) {
			/* We might have been asked to remap this name, due to a conflict */ 
			for (attr_idx=0; attr_map && attr_map[attr_idx].old_attr; attr_idx++) { 
				if (strcasecmp(must[j], attr_map[attr_idx].old_attr) == 0) { 
					must[j] =  attr_map[attr_idx].new_attr; 
					break;				
				}					
			}						
		}

		schema_entry = schema_class_description(mem_ctx, target, 
							SEPERATOR,
							oid, 
							name,
							NULL, 
							subClassOf,
							objectClassCategory,
							must,
							may,
							NULL);
		if (schema_entry == NULL) {
			DEBUG(0, ("failed to generate schema description for %s\n", name));
			return NULL;
		}

		switch (target) {
		case TARGET_OPENLDAP:
			out = talloc_asprintf_append(out, "objectclass %s\n\n", schema_entry);
			break;
		case TARGET_FEDORA_DS:
			out = talloc_asprintf_append(out, "objectClasses: %s\n", schema_entry);
			break;
		}
		talloc_free(mem_ctx);
	} while (0);

	
	for (objectclass=schema->classes; objectclass; objectclass = objectclass->next) {
		if (ldb_attr_cmp(objectclass->subClassOf, print_class) == 0 
		    && ldb_attr_cmp(objectclass->lDAPDisplayName, print_class) != 0) {
			out = print_schema_recursive(out, schema, objectclass->lDAPDisplayName, 
						     target, attrs_skip, attr_map, oid_map);
		}
	}
	return out;
}

/* Routine to linearise our internal schema into the format that
   OpenLDAP and Fedora DS use for their backend.  

   The 'mappings' are of a format like:

#Standard OpenLDAP attributes
labeledURI
#The memberOf plugin provides this attribute
memberOf
#These conflict with OpenLDAP builtins
attributeTypes:samba4AttributeTypes
2.5.21.5:1.3.6.1.4.1.7165.4.255.7

*/


char *dsdb_convert_schema_to_openldap(struct ldb_context *ldb, char *target_str, const char *mappings) 
{
	/* Read list of attributes to skip, OIDs to map */
	TALLOC_CTX *mem_ctx = talloc_new(ldb);
	char *line;
	char *out;
	const char **attrs_skip = NULL;
	unsigned int num_skip = 0;
	struct oid_map *oid_map = NULL;
	unsigned int num_oid_maps = 0;
	struct attr_map *attr_map = NULL;
	unsigned int num_attr_maps = 0;
	struct dsdb_attribute *attribute;
	struct dsdb_schema *schema;
	enum dsdb_schema_convert_target target;

	char *next_line = talloc_strdup(mem_ctx, mappings);

	if (!target_str || strcasecmp(target_str, "openldap") == 0) {
		target = TARGET_OPENLDAP;
	} else if (strcasecmp(target_str, "fedora-ds") == 0) {
		target = TARGET_FEDORA_DS;
	} else {
		DEBUG(0, ("Invalid target type for schema conversion %s\n", target_str));
		return NULL;
	}

	/* The mappings are line-seperated, and specify details such as OIDs to skip etc */
	while (1) {
		line = next_line;
		next_line = strchr(line, '\n');
		if (!next_line) {
			break;
		}
		next_line[0] = '\0';
		next_line++;

		/* Blank Line */
		if (line[0] == '\0') {
			continue;
		}
		/* Comment */
		if (line[0] == '#') {
			continue;
		}

		if (isdigit(line[0])) {
			char *p = strchr(line, ':');
			if (!p) {
				DEBUG(0, ("schema mapping file line has OID but no OID to map to: %s\n", line));
				return NULL;
			}
			p[0] = '\0';
			p++;
			oid_map = talloc_realloc(mem_ctx, oid_map, struct oid_map, num_oid_maps + 2);
			trim_string(line, " ", " ");
			oid_map[num_oid_maps].old_oid = talloc_strdup(oid_map, line);
			trim_string(p, " ", " ");
			oid_map[num_oid_maps].new_oid = p;
			num_oid_maps++;
			oid_map[num_oid_maps].old_oid = NULL;
		} else {
			char *p = strchr(line, ':');
			if (p) {
				/* remap attribute/objectClass */
				p[0] = '\0';
				p++;
				attr_map = talloc_realloc(mem_ctx, attr_map, struct attr_map, num_attr_maps + 2);
				trim_string(line, " ", " ");
				attr_map[num_attr_maps].old_attr = talloc_strdup(attr_map, line);
				trim_string(p, " ", " ");
				attr_map[num_attr_maps].new_attr = p;
				num_attr_maps++;
				attr_map[num_attr_maps].old_attr = NULL;
			} else {
				/* skip attribute/objectClass */
				attrs_skip = talloc_realloc(mem_ctx, attrs_skip, const char *, num_skip + 2);
				trim_string(line, " ", " ");
				attrs_skip[num_skip] = talloc_strdup(attrs_skip, line);
				num_skip++;
				attrs_skip[num_skip] = NULL;
			}
		}
	}

	schema = dsdb_get_schema(ldb, mem_ctx);
	if (!schema) {
		DEBUG(0, ("No schema on ldb to convert!\n"));
		return NULL;
	}

	switch (target) {
	case TARGET_OPENLDAP:
		out = talloc_strdup(mem_ctx, "");
		break;
	case TARGET_FEDORA_DS:
		out = talloc_strdup(mem_ctx, "dn: cn=schema\n");
		break;
	}

	for (attribute=schema->attributes; attribute; attribute = attribute->next) {
		const char *name = attribute->lDAPDisplayName;
		const char *oid = attribute->attributeID_oid;
		const char *syntax = attribute->attributeSyntax_oid;
		const char *equality = NULL, *substring = NULL;
		bool single_value = attribute->isSingleValued;

		char *schema_entry = NULL;
		unsigned int j;

		/* We have been asked to skip some attributes/objectClasses */
		if (attrs_skip && str_list_check_ci(attrs_skip, name)) {
			continue;
		}

		/* We might have been asked to remap this oid, due to a conflict */
		for (j=0; oid && oid_map && oid_map[j].old_oid; j++) {
			if (strcasecmp(oid, oid_map[j].old_oid) == 0) {
				oid =  oid_map[j].new_oid;
				break;
			}
		}
		
		if (attribute->syntax) {
			/* We might have been asked to remap this oid,
			 * due to a conflict, or lack of
			 * implementation */
			syntax = attribute->syntax->ldap_oid;
			/* We might have been asked to remap this oid, due to a conflict */
			for (j=0; syntax && oid_map && oid_map[j].old_oid; j++) {
				if (strcasecmp(syntax, oid_map[j].old_oid) == 0) {
					syntax =  oid_map[j].new_oid;
					break;
				}
			}
			
			equality = attribute->syntax->equality;
			substring = attribute->syntax->substring;
		}

		/* We might have been asked to remap this name, due to a conflict */
		for (j=0; name && attr_map && attr_map[j].old_attr; j++) {
			if (strcasecmp(name, attr_map[j].old_attr) == 0) {
				name =  attr_map[j].new_attr;
				break;
			}
		}
		
		schema_entry = schema_attribute_description(mem_ctx, 
							    target, 
							    SEPERATOR, 
							    oid, 
							    name, 
							    equality, 
							    substring, 
							    syntax, 
							    single_value, 
							    false,
							    NULL, NULL,
							    NULL, NULL,
							    false, false);

		if (schema_entry == NULL) {
			DEBUG(0, ("failed to generate attribute description for %s\n", name));
			return NULL;
		}

		switch (target) {
		case TARGET_OPENLDAP:
			out = talloc_asprintf_append(out, "attributetype %s\n\n", schema_entry);
			break;
		case TARGET_FEDORA_DS:
			out = talloc_asprintf_append(out, "attributeTypes: %s\n", schema_entry);
			break;
		}
	}

	out = print_schema_recursive(out, schema, "top", target, attrs_skip, attr_map, oid_map);

	return out;
}

