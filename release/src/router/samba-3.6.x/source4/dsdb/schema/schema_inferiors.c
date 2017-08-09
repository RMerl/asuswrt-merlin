/* 
   Unix SMB/CIFS implementation.

   implement possibleInferiors calculation
   
   Copyright (C) Andrew Tridgell 2009
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009

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
  This module is a C implementation of the logic in the
  dsdb/samdb/ldb_modules/tests/possibleInferiors.py code

  To understand the C code, please see the python code first
 */

#include "includes.h"
#include "dsdb/samdb/samdb.h"


/*
  create the SUPCLASSES() list
 */
static const char **schema_supclasses(const struct dsdb_schema *schema,
				      struct dsdb_class *schema_class)
{
	const char **list;

	if (schema_class->supclasses) {
		return schema_class->supclasses;
	}

	list = const_str_list(str_list_make_empty(schema_class));
	if (list == NULL) {
		DEBUG(0,(__location__ " out of memory\n"));
		return NULL;
	}

	/* Cope with 'top SUP top', i.e. top is subClassOf top */
	if (schema_class->subClassOf &&
	    strcmp(schema_class->lDAPDisplayName, schema_class->subClassOf) == 0) {
		schema_class->supclasses = list;
		return list;
	}

	if (schema_class->subClassOf) {
		const struct dsdb_class *schema_class2 = dsdb_class_by_lDAPDisplayName(schema, schema_class->subClassOf);
		const char **list2;
		list = str_list_add_const(list,	schema_class->subClassOf);

		list2 = schema_supclasses(schema, discard_const_p(struct dsdb_class, schema_class2));
		list = str_list_append_const(list, list2);
	}

	schema_class->supclasses = str_list_unique(list);

	return schema_class->supclasses;
}

/*
  this one is used internally
  matches SUBCLASSES() python function
 */
static const char **schema_subclasses(const struct dsdb_schema *schema,
				      TALLOC_CTX *mem_ctx,
				      const char **oclist)
{
	const char **list = const_str_list(str_list_make_empty(mem_ctx));
	unsigned int i;

	for (i=0; oclist && oclist[i]; i++) {
		const struct dsdb_class *schema_class =	dsdb_class_by_lDAPDisplayName(schema, oclist[i]);
		if (!schema_class) {
			DEBUG(0, ("ERROR: Unable to locate subClass: '%s'\n", oclist[i]));
			continue;
		}
		list = str_list_append_const(list, schema_class->subclasses);
	}
	return list;
}


/* 
   equivalent of the POSSSUPERIORS() python function
 */
static const char **schema_posssuperiors(const struct dsdb_schema *schema,
					 struct dsdb_class *schema_class)
{
	if (schema_class->posssuperiors == NULL) {
		const char **list2 = const_str_list(str_list_make_empty(schema_class));
		const char **list3;
		unsigned int i;

		list2 = str_list_append_const(list2, schema_class->systemPossSuperiors);
		list2 = str_list_append_const(list2, schema_class->possSuperiors);
		list3 = schema_supclasses(schema, schema_class);
		for (i=0; list3 && list3[i]; i++) {
			const struct dsdb_class *class2 = dsdb_class_by_lDAPDisplayName(schema, list3[i]);
			if (!class2) {
				DEBUG(0, ("ERROR: Unable to locate supClass: '%s'\n", list3[i]));
				continue;
			}
			list2 = str_list_append_const(list2, schema_posssuperiors(schema,
				discard_const_p(struct dsdb_class, class2)));
		}
		list2 = str_list_append_const(list2, schema_subclasses(schema, list2, list2));

		schema_class->posssuperiors = str_list_unique(list2);
	}

	return schema_class->posssuperiors;
}

static const char **schema_subclasses_recurse(const struct dsdb_schema *schema,
					      struct dsdb_class *schema_class)
{
	const char **list = str_list_copy_const(schema_class, schema_class->subclasses_direct);
	unsigned int i;
	for (i=0;list && list[i]; i++) {
		const struct dsdb_class *schema_class2 = dsdb_class_by_lDAPDisplayName(schema, list[i]);
		if (schema_class != schema_class2) {
			list = str_list_append_const(list, schema_subclasses_recurse(schema,
				discard_const_p(struct dsdb_class, schema_class2)));
		}
	}
	return list;
}

/* Walk down the subClass tree, setting a higher index as we go down
 * each level.  top is 1, subclasses of top are 2, etc */
void schema_subclasses_order_recurse(const struct dsdb_schema *schema,
				     struct dsdb_class *schema_class,
				     const int order)
{
	const char **list = schema_class->subclasses_direct;
	unsigned int i;
	schema_class->subClass_order = order;
	for (i=0;list && list[i]; i++) {
		const struct dsdb_class *schema_class2 = dsdb_class_by_lDAPDisplayName(schema, list[i]);
		schema_subclasses_order_recurse(schema, discard_const_p(struct dsdb_class, schema_class2), order+1);
	}
	return;
}

static int schema_create_subclasses(const struct dsdb_schema *schema)
{
	struct dsdb_class *schema_class, *top;

	for (schema_class=schema->classes; schema_class; schema_class=schema_class->next) {
		struct dsdb_class *schema_class2 = discard_const_p(struct dsdb_class,
			dsdb_class_by_lDAPDisplayName(schema, schema_class->subClassOf));
		if (schema_class2 == NULL) {
			DEBUG(0,("ERROR: no subClassOf '%s' for '%s'\n",
				 schema_class->subClassOf,
				 schema_class->lDAPDisplayName));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (schema_class2 && schema_class != schema_class2) {
			if (schema_class2->subclasses_direct == NULL) {
				schema_class2->subclasses_direct = const_str_list(str_list_make_empty(schema_class2));
				if (!schema_class2->subclasses_direct) {
					return LDB_ERR_OPERATIONS_ERROR;
				}
			}
			schema_class2->subclasses_direct = str_list_add_const(schema_class2->subclasses_direct,
						schema_class->lDAPDisplayName);
		}
	}

	for (schema_class=schema->classes; schema_class; schema_class=schema_class->next) {
		schema_class->subclasses = str_list_unique(schema_subclasses_recurse(schema, schema_class));

		/* Initialize the subClass order, to ensure we can't have uninitialized sort on the subClass hierarchy */
		schema_class->subClass_order = 0;
	}

	top = discard_const_p(struct dsdb_class, dsdb_class_by_lDAPDisplayName(schema, "top"));
	if (!top) {
		DEBUG(0,("ERROR: no 'top' class in loaded schema\n"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	schema_subclasses_order_recurse(schema, top, 1);
	return LDB_SUCCESS;
}

static void schema_fill_possible_inferiors(const struct dsdb_schema *schema,
					   struct dsdb_class *schema_class)
{
	struct dsdb_class *c2;

	for (c2=schema->classes; c2; c2=c2->next) {
		const char **superiors = schema_posssuperiors(schema, c2);
		if (c2->systemOnly == false 
		    && c2->objectClassCategory != 2 
		    && c2->objectClassCategory != 3
		    && str_list_check(superiors, schema_class->lDAPDisplayName)) {
			if (schema_class->possibleInferiors == NULL) {
				schema_class->possibleInferiors = const_str_list(str_list_make_empty(schema_class));
			}
			schema_class->possibleInferiors = str_list_add_const(schema_class->possibleInferiors,
							c2->lDAPDisplayName);
		}
	}
	schema_class->possibleInferiors = str_list_unique(schema_class->possibleInferiors);
}

static void schema_fill_system_possible_inferiors(const struct dsdb_schema *schema,
						  struct dsdb_class *schema_class)
{
	struct dsdb_class *c2;

	for (c2=schema->classes; c2; c2=c2->next) {
		const char **superiors = schema_posssuperiors(schema, c2);
		if (c2->objectClassCategory != 2
		    && c2->objectClassCategory != 3
		    && str_list_check(superiors, schema_class->lDAPDisplayName)) {
			if (schema_class->systemPossibleInferiors == NULL) {
				schema_class->systemPossibleInferiors = const_str_list(str_list_make_empty(schema_class));
			}
			schema_class->systemPossibleInferiors = str_list_add_const(schema_class->systemPossibleInferiors,
							c2->lDAPDisplayName);
		}
	}
	schema_class->systemPossibleInferiors = str_list_unique(schema_class->systemPossibleInferiors);
}

/*
  fill in a string class name from a governs_ID
 */
static void schema_fill_from_class_one(const struct dsdb_schema *schema,
				       const struct dsdb_class *c,
				       const char **s,
				       const uint32_t id)
{
	if (*s == NULL && id != 0) {
		const struct dsdb_class *c2 =
					dsdb_class_by_governsID_id(schema, id);
		if (c2) {
			*s = c2->lDAPDisplayName;
		}
	}
}

/*
  fill in a list of string class names from a governs_ID list
 */
static void schema_fill_from_class_list(const struct dsdb_schema *schema,
					const struct dsdb_class *c,
					const char ***s,
					const uint32_t *ids)
{
	if (*s == NULL && ids != NULL) {
		unsigned int i;
		for (i=0;ids[i];i++) ;
		*s = talloc_array(c, const char *, i+1);
		for (i=0;ids[i];i++) {
			const struct dsdb_class *c2 =
				dsdb_class_by_governsID_id(schema, ids[i]);
			if (c2) {
				(*s)[i] = c2->lDAPDisplayName;
			} else {
				(*s)[i] = NULL;				
			}
		}
		(*s)[i] = NULL;				
	}
}

/*
  fill in a list of string attribute names from a attributeID list
 */
static void schema_fill_from_attribute_list(const struct dsdb_schema *schema,
					    const struct dsdb_class *c,
					    const char ***s,
					    const uint32_t *ids)
{
	if (*s == NULL && ids != NULL) {
		unsigned int i;
		for (i=0;ids[i];i++) ;
		*s = talloc_array(c, const char *, i+1);
		for (i=0;ids[i];i++) {
			const struct dsdb_attribute *a =
				dsdb_attribute_by_attributeID_id(schema, ids[i]);
			if (a) {
				(*s)[i] = a->lDAPDisplayName;
			} else {
				(*s)[i] = NULL;				
			}
		}
		(*s)[i] = NULL;				
	}
}

/*
  if the schema came from DRS then some attributes will be setup as IDs
 */
static void schema_fill_from_ids(const struct dsdb_schema *schema)
{
	struct dsdb_class *c;
	for (c=schema->classes; c; c=c->next) {
		schema_fill_from_class_one(schema, c, &c->subClassOf, c->subClassOf_id);
		schema_fill_from_attribute_list(schema, c, &c->systemMayContain, c->systemMayContain_ids);
		schema_fill_from_attribute_list(schema, c, &c->systemMustContain, c->systemMustContain_ids);
		schema_fill_from_attribute_list(schema, c, &c->mustContain, c->mustContain_ids);
		schema_fill_from_attribute_list(schema, c, &c->mayContain, c->mayContain_ids);
		schema_fill_from_class_list(schema, c, &c->possSuperiors, c->possSuperiors_ids);
		schema_fill_from_class_list(schema, c, &c->systemPossSuperiors, c->systemPossSuperiors_ids);
		schema_fill_from_class_list(schema, c, &c->systemAuxiliaryClass, c->systemAuxiliaryClass_ids);
		schema_fill_from_class_list(schema, c, &c->auxiliaryClass, c->auxiliaryClass_ids);
	}
}

int schema_fill_constructed(const struct dsdb_schema *schema)
{
	int ret;
	struct dsdb_class *schema_class;

	schema_fill_from_ids(schema);

	ret = schema_create_subclasses(schema);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	for (schema_class=schema->classes; schema_class; schema_class=schema_class->next) {
		schema_fill_possible_inferiors(schema, schema_class);
		schema_fill_system_possible_inferiors(schema, schema_class);
	}

	/* free up our internal cache elements */
	for (schema_class=schema->classes; schema_class; schema_class=schema_class->next) {
		talloc_free(schema_class->supclasses);
		talloc_free(schema_class->subclasses_direct);
		talloc_free(schema_class->subclasses);
		talloc_free(schema_class->posssuperiors);
		schema_class->supclasses = NULL;
		schema_class->subclasses_direct = NULL;
		schema_class->subclasses = NULL;
		schema_class->posssuperiors = NULL;
	}

	return LDB_SUCCESS;
}
