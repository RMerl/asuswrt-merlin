/* 
   ldb database library

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

/*
 *  Name: ldb
 *
 *  Component: ad2oLschema
 *
 *  Description: utility to convert an AD schema into the format required by OpenLDAP
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"
#include "system/locale.h"
#include "ldb/tools/cmdline.h"
#include "ldb/tools/convert.h"

struct schema_conv {
	int count;
	int skipped;
	int failures;
};

enum convert_target {
	TARGET_OPENLDAP,
	TARGET_FEDORA_DS
};
	

static void usage(void)
{
	printf("Usage: ad2oLschema <options>\n");
	printf("\nConvert AD-like LDIF to OpenLDAP schema format\n\n");
	printf("Options:\n");
	printf("  -I inputfile     inputfile of mapped OIDs and skipped attributes/ObjectClasses");
	printf("  -H url           LDB or LDAP server to read schmea from\n");
	printf("  -O outputfile    outputfile otherwise STDOUT\n");
	printf("  -o options       pass options like modules to activate\n");
	printf("              e.g: -o modules:timestamps\n");
	printf("\n");
	printf("Converts records from an AD-like LDIF schema into an openLdap formatted schema\n\n");
	exit(1);
}

static int fetch_attrs_schema(struct ldb_context *ldb, struct ldb_dn *schemadn,
			      TALLOC_CTX *mem_ctx, 
			      struct ldb_result **attrs_res)
{
	TALLOC_CTX *local_ctx = talloc_new(mem_ctx);
	int ret;
	const char *attrs[] = {
		"lDAPDisplayName",
		"isSingleValued",
		"attributeID",
		"attributeSyntax",
		"description",		
		NULL
	};

	if (!local_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	/* Downlaod schema */
	ret = ldb_search(ldb, ldb, attrs_res, schemadn, LDB_SCOPE_SUBTREE, 
			 attrs, "objectClass=attributeSchema");
	if (ret != LDB_SUCCESS) {
		printf("Search failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	return ret;
}

static const char *oc_attrs[] = {
	"lDAPDisplayName",
	"mayContain",
	"mustContain",
	"systemMayContain",
	"systemMustContain",
	"objectClassCategory",
	"governsID",
	"description",
	"subClassOf",
	NULL
};

static int fetch_oc_recursive(struct ldb_context *ldb, struct ldb_dn *schemadn, 
			      TALLOC_CTX *mem_ctx, 
			      struct ldb_result *search_from,
			      struct ldb_result *res_list)
{
	int i;
	int ret = 0;
	for (i=0; i < search_from->count; i++) {
		struct ldb_result *res;
		const char *name = ldb_msg_find_attr_as_string(search_from->msgs[i], 
							       "lDAPDisplayname", NULL);

		ret = ldb_search(ldb, ldb, &res, schemadn, LDB_SCOPE_SUBTREE, 
				 oc_attrs, "(&(&(objectClass=classSchema)(subClassOf=%s))(!(lDAPDisplayName=%s)))", 
					       name, name);
		if (ret != LDB_SUCCESS) {
			printf("Search failed: %s\n", ldb_errstring(ldb));
			return ret;
		}
		
		talloc_steal(mem_ctx, res);

		res_list->msgs = talloc_realloc(res_list, res_list->msgs, 
						struct ldb_message *, res_list->count + 2);
		if (!res_list->msgs) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		res_list->msgs[res_list->count] = talloc_move(res_list, 
							      &search_from->msgs[i]);
		res_list->count++;
		res_list->msgs[res_list->count] = NULL;

		if (res->count > 0) {
			ret = fetch_oc_recursive(ldb, schemadn, mem_ctx, res, res_list); 
		}
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return ret;
}

static int fetch_objectclass_schema(struct ldb_context *ldb, struct ldb_dn *schemadn, 
				    TALLOC_CTX *mem_ctx, 
				    struct ldb_result **objectclasses_res)
{
	TALLOC_CTX *local_ctx = talloc_new(mem_ctx);
	struct ldb_result *top_res, *ret_res;
	int ret;
	if (!local_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	/* Downlaod 'top' */
	ret = ldb_search(ldb, ldb, &top_res, schemadn, LDB_SCOPE_SUBTREE, 
			 oc_attrs, "(&(objectClass=classSchema)(lDAPDisplayName=top))");
	if (ret != LDB_SUCCESS) {
		printf("Search failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	talloc_steal(local_ctx, top_res);

	if (top_res->count != 1) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret_res = talloc_zero(local_ctx, struct ldb_result);
	if (!ret_res) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = fetch_oc_recursive(ldb, schemadn, local_ctx, top_res, ret_res); 

	if (ret != LDB_SUCCESS) {
		printf("Search failed: %s\n", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*objectclasses_res = talloc_move(mem_ctx, &ret_res);
	return ret;
}

static struct ldb_dn *find_schema_dn(struct ldb_context *ldb, TALLOC_CTX *mem_ctx) 
{
	const char *rootdse_attrs[] = {"schemaNamingContext", NULL};
	struct ldb_dn *schemadn;
	struct ldb_dn *basedn = ldb_dn_explode(mem_ctx, "");
	struct ldb_result *rootdse_res;
	int ldb_ret;
	if (!basedn) {
		return NULL;
	}
	
	/* Search for rootdse */
	ldb_ret = ldb_search(ldb, ldb, &rootdse_res, basedn, LDB_SCOPE_BASE, rootdse_attrs, NULL);
	if (ldb_ret != LDB_SUCCESS) {
		printf("Search failed: %s\n", ldb_errstring(ldb));
		return NULL;
	}
	
	talloc_steal(mem_ctx, rootdse_res);

	if (rootdse_res->count != 1) {
		printf("Failed to find rootDSE");
		return NULL;
	}
	
	/* Locate schema */
	schemadn = ldb_msg_find_attr_as_dn(mem_ctx, rootdse_res->msgs[0], "schemaNamingContext");
	if (!schemadn) {
		return NULL;
	}

	talloc_free(rootdse_res);
	return schemadn;
}

#define IF_NULL_FAIL_RET(x) do {     \
		if (!x) {		\
			ret.failures++; \
			return ret;	\
		}			\
	} while (0) 


static struct schema_conv process_convert(struct ldb_context *ldb, enum convert_target target, FILE *in, FILE *out) 
{
	/* Read list of attributes to skip, OIDs to map */
	TALLOC_CTX *mem_ctx = talloc_new(ldb);
	char *line;
	const char **attrs_skip = NULL;
	int num_skip = 0;
	struct oid_map {
		char *old_oid;
		char *new_oid;
	} *oid_map = NULL;
	int num_maps = 0;
	struct ldb_result *attrs_res, *objectclasses_res;
	struct ldb_dn *schemadn;
	struct schema_conv ret;

	int ldb_ret, i;

	ret.count = 0;
	ret.skipped = 0;
	ret.failures = 0;

	while ((line = afdgets(fileno(in), mem_ctx, 0))) {
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
			IF_NULL_FAIL_RET(p);
			if (!p) {
				ret.failures = 1;
				return ret;
			}
			p[0] = '\0';
			p++;
			oid_map = talloc_realloc(mem_ctx, oid_map, struct oid_map, num_maps + 2);
			trim_string(line, " ", " ");
			oid_map[num_maps].old_oid = talloc_move(oid_map, &line);
			trim_string(p, " ", " ");
			oid_map[num_maps].new_oid = p;
			num_maps++;
			oid_map[num_maps].old_oid = NULL;
		} else {
			attrs_skip = talloc_realloc(mem_ctx, attrs_skip, const char *, num_skip + 2);
			trim_string(line, " ", " ");
			attrs_skip[num_skip] = talloc_move(attrs_skip, &line);
			num_skip++;
			attrs_skip[num_skip] = NULL;
		}
	}

	schemadn = find_schema_dn(ldb, mem_ctx);
	if (!schemadn) {
		printf("Failed to find schema DN: %s\n", ldb_errstring(ldb));
		ret.failures = 1;
		return ret;
	}
	
	ldb_ret = fetch_attrs_schema(ldb, schemadn, mem_ctx, &attrs_res);
	if (ldb_ret != LDB_SUCCESS) {
		printf("Failed to fetch attribute schema: %s\n", ldb_errstring(ldb));
		ret.failures = 1;
		return ret;
	}
	
	switch (target) {
	case TARGET_OPENLDAP:
		break;
	case TARGET_FEDORA_DS:
		fprintf(out, "dn: cn=schema\n");
		break;
	}

	for (i=0; i < attrs_res->count; i++) {
		struct ldb_message *msg = attrs_res->msgs[i];

		const char *name = ldb_msg_find_attr_as_string(msg, "lDAPDisplayName", NULL);
		const char *description = ldb_msg_find_attr_as_string(msg, "description", NULL);
		const char *oid = ldb_msg_find_attr_as_string(msg, "attributeID", NULL);
		const char *syntax = ldb_msg_find_attr_as_string(msg, "attributeSyntax", NULL);
		BOOL single_value = ldb_msg_find_attr_as_bool(msg, "isSingleValued", False);
		const struct syntax_map *map = find_syntax_map_by_ad_oid(syntax);
		char *schema_entry = NULL;
		int j;

		/* We have been asked to skip some attributes/objectClasses */
		if (attrs_skip && str_list_check_ci(attrs_skip, name)) {
			ret.skipped++;
			continue;
		}

		/* We might have been asked to remap this oid, due to a conflict */
		for (j=0; oid && oid_map[j].old_oid; j++) {
			if (strcmp(oid, oid_map[j].old_oid) == 0) {
				oid =  oid_map[j].new_oid;
				break;
			}
		}
		
		switch (target) {
		case TARGET_OPENLDAP:
			schema_entry = talloc_asprintf(mem_ctx, 
						       "attributetype (\n"
						       "  %s\n", oid);
			break;
		case TARGET_FEDORA_DS:
			schema_entry = talloc_asprintf(mem_ctx, 
						       "attributeTypes: (\n"
						       "  %s\n", oid);
			break;
		}
		IF_NULL_FAIL_RET(schema_entry);

		schema_entry = talloc_asprintf_append(schema_entry, 
						      "  NAME '%s'\n", name);
		IF_NULL_FAIL_RET(schema_entry);

		if (description) {
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  DESC %s\n", description);
			IF_NULL_FAIL_RET(schema_entry);
		}

		if (map) {
			const char *syntax_oid;
			if (map->equality) {
				schema_entry = talloc_asprintf_append(schema_entry, 
								      "  EQUALITY %s\n", map->equality);
				IF_NULL_FAIL_RET(schema_entry);
			}
			if (map->substring) {
				schema_entry = talloc_asprintf_append(schema_entry, 
								      "  SUBSTR %s\n", map->substring);
				IF_NULL_FAIL_RET(schema_entry);
			}
			syntax_oid = map->Standard_OID;
			/* We might have been asked to remap this oid,
			 * due to a conflict, or lack of
			 * implementation */
			for (j=0; syntax_oid && oid_map[j].old_oid; j++) {
				if (strcmp(syntax_oid, oid_map[j].old_oid) == 0) {
					syntax_oid =  oid_map[j].new_oid;
					break;
				}
			}
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  SYNTAX %s\n", syntax_oid);
			IF_NULL_FAIL_RET(schema_entry);
		}

		if (single_value) {
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  SINGLE-VALUE\n");
			IF_NULL_FAIL_RET(schema_entry);
		}
		
		schema_entry = talloc_asprintf_append(schema_entry, 
						      "  )");

		switch (target) {
		case TARGET_OPENLDAP:
			fprintf(out, "%s\n\n", schema_entry);
			break;
		case TARGET_FEDORA_DS:
			fprintf(out, "%s\n", schema_entry);
			break;
		}
		ret.count++;
	}

	ldb_ret = fetch_objectclass_schema(ldb, schemadn, mem_ctx, &objectclasses_res);
	if (ldb_ret != LDB_SUCCESS) {
		printf("Failed to fetch objectClass schema elements: %s\n", ldb_errstring(ldb));
		ret.failures = 1;
		return ret;
	}
	
	for (i=0; i < objectclasses_res->count; i++) {
		struct ldb_message *msg = objectclasses_res->msgs[i];
		const char *name = ldb_msg_find_attr_as_string(msg, "lDAPDisplayName", NULL);
		const char *description = ldb_msg_find_attr_as_string(msg, "description", NULL);
		const char *oid = ldb_msg_find_attr_as_string(msg, "governsID", NULL);
		const char *subClassOf = ldb_msg_find_attr_as_string(msg, "subClassOf", NULL);
		int objectClassCategory = ldb_msg_find_attr_as_int(msg, "objectClassCategory", 0);
		struct ldb_message_element *must = ldb_msg_find_element(msg, "mustContain");
		struct ldb_message_element *sys_must = ldb_msg_find_element(msg, "systemMustContain");
		struct ldb_message_element *may = ldb_msg_find_element(msg, "mayContain");
		struct ldb_message_element *sys_may = ldb_msg_find_element(msg, "systemMayContain");
		char *schema_entry = NULL;
		int j;

		/* We have been asked to skip some attributes/objectClasses */
		if (attrs_skip && str_list_check_ci(attrs_skip, name)) {
			ret.skipped++;
			continue;
		}

		/* We might have been asked to remap this oid, due to a conflict */
		for (j=0; oid_map[j].old_oid; j++) {
			if (strcmp(oid, oid_map[j].old_oid) == 0) {
				oid =  oid_map[j].new_oid;
				break;
			}
		}
		
		switch (target) {
		case TARGET_OPENLDAP:
			schema_entry = talloc_asprintf(mem_ctx, 
						       "objectclass (\n"
						       "  %s\n", oid);
			break;
		case TARGET_FEDORA_DS:
			schema_entry = talloc_asprintf(mem_ctx, 
						       "objectClasses: (\n"
						       "  %s\n", oid);
			break;
		}
		IF_NULL_FAIL_RET(schema_entry);
		if (!schema_entry) {
			ret.failures++;
			break;
		}

		schema_entry = talloc_asprintf_append(schema_entry, 
						      "  NAME '%s'\n", name);
		IF_NULL_FAIL_RET(schema_entry);

		if (!schema_entry) return ret;

		if (description) {
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  DESC %s\n", description);
			IF_NULL_FAIL_RET(schema_entry);
		}

		if (subClassOf) {
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  SUP %s\n", subClassOf);
			IF_NULL_FAIL_RET(schema_entry);
		}
		
		switch (objectClassCategory) {
		case 1:
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  STRUCTURAL\n");
			IF_NULL_FAIL_RET(schema_entry);
			break;
		case 2:
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  ABSTRACT\n");
			IF_NULL_FAIL_RET(schema_entry);
			break;
		case 3:
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  AUXILIARY\n");
			IF_NULL_FAIL_RET(schema_entry);
			break;
		}

#define APPEND_ATTRS(attributes) \
		do {						\
			int k;						\
			for (k=0; attributes && k < attributes->num_values; k++) { \
				schema_entry = talloc_asprintf_append(schema_entry, \
								      " %s", \
								      (const char *)attributes->values[k].data); \
				IF_NULL_FAIL_RET(schema_entry);		\
				if (k != (attributes->num_values - 1)) { \
					schema_entry = talloc_asprintf_append(schema_entry, \
									      " $"); \
					IF_NULL_FAIL_RET(schema_entry);	\
					if (target == TARGET_OPENLDAP && ((k+1)%5 == 0)) { \
						schema_entry = talloc_asprintf_append(schema_entry, \
										      "\n  "); \
						IF_NULL_FAIL_RET(schema_entry);	\
					}				\
				}					\
			}						\
		} while (0)

		if (must || sys_must) {
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  MUST (");
			IF_NULL_FAIL_RET(schema_entry);

			APPEND_ATTRS(must);
			if (must && sys_must) {
				schema_entry = talloc_asprintf_append(schema_entry, \
								      " $"); \
			}
			APPEND_ATTRS(sys_must);
			
			schema_entry = talloc_asprintf_append(schema_entry, 
							      " )\n");
			IF_NULL_FAIL_RET(schema_entry);
		}

		if (may || sys_may) {
			schema_entry = talloc_asprintf_append(schema_entry, 
							      "  MAY (");
			IF_NULL_FAIL_RET(schema_entry);

			APPEND_ATTRS(may);
			if (may && sys_may) {
				schema_entry = talloc_asprintf_append(schema_entry, \
								      " $"); \
			}
			APPEND_ATTRS(sys_may);
			
			schema_entry = talloc_asprintf_append(schema_entry, 
							      " )\n");
			IF_NULL_FAIL_RET(schema_entry);
		}

		schema_entry = talloc_asprintf_append(schema_entry, 
						      "  )");

		switch (target) {
		case TARGET_OPENLDAP:
			fprintf(out, "%s\n\n", schema_entry);
			break;
		case TARGET_FEDORA_DS:
			fprintf(out, "%s\n", schema_entry);
			break;
		}
		ret.count++;
	}

	return ret;
}

 int main(int argc, const char **argv)
{
	TALLOC_CTX *ctx;
	struct ldb_cmdline *options;
	FILE *in = stdin;
	FILE *out = stdout;
	struct ldb_context *ldb;
	struct schema_conv ret;
	const char *target_str;
	enum convert_target target;

	ldb_global_init();

	ctx = talloc_new(NULL);
	ldb = ldb_init(ctx);

	options = ldb_cmdline_process(ldb, argc, argv, usage);

	if (options->input) {
		in = fopen(options->input, "r");
		if (!in) {
			perror(options->input);
			exit(1);
		}
	}
	if (options->output) {
		out = fopen(options->output, "w");
		if (!out) {
			perror(options->output);
			exit(1);
		}
	}

	target_str = lp_parm_string(-1, "convert", "target");

	if (!target_str || strcasecmp(target_str, "openldap") == 0) {
		target = TARGET_OPENLDAP;
	} else if (strcasecmp(target_str, "fedora-ds") == 0) {
		target = TARGET_FEDORA_DS;
	} else {
		printf("Unsupported target: %s\n", target_str);
		exit(1);
	}

	ret = process_convert(ldb, target, in, out);

	fclose(in);
	fclose(out);

	printf("Converted %d records (skipped %d) with %d failures\n", ret.count, ret.skipped, ret.failures);

	return 0;
}
