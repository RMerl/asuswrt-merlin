/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2005

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
  register handlers for specific attributes and objectclass relationships

  this allows a backend to store its schema information in any format
  it likes (or to not have any schema information at all) while keeping the 
  message matching logic generic
*/

#include "ldb_private.h"
#include "ldb_handlers.h"

/*
  add a attribute to the ldb_schema

  if flags contains LDB_ATTR_FLAG_ALLOCATED
  the attribute name string will be copied using
  talloc_strdup(), otherwise it needs to be a static const
  string at least with a lifetime longer than the ldb struct!
  
  the ldb_schema_syntax structure should be a pointer
  to a static const struct or at least it needs to be
  a struct with a longer lifetime than the ldb context!

*/
int ldb_schema_attribute_add_with_syntax(struct ldb_context *ldb, 
					 const char *attribute,
					 unsigned flags,
					 const struct ldb_schema_syntax *syntax)
{
	unsigned int i, n;
	struct ldb_schema_attribute *a;

	if (!syntax) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	n = ldb->schema.num_attributes + 1;

	a = talloc_realloc(ldb, ldb->schema.attributes,
			   struct ldb_schema_attribute, n);
	if (a == NULL) {
		ldb_oom(ldb);
		return -1;
	}
	ldb->schema.attributes = a;

	for (i = 0; i < ldb->schema.num_attributes; i++) {
		int cmp = ldb_attr_cmp(attribute, a[i].name);
		if (cmp == 0) {
			/* silently ignore attempts to overwrite fixed attributes */
			if (a[i].flags & LDB_ATTR_FLAG_FIXED) {
				return 0;
			}
			if (a[i].flags & LDB_ATTR_FLAG_ALLOCATED) {
				talloc_free(discard_const_p(char, a[i].name));
			}
			/* To cancel out increment below */
			ldb->schema.num_attributes--;
			break;
		} else if (cmp < 0) {
			memmove(a+i+1, a+i, sizeof(*a) * (ldb->schema.num_attributes-i));
			break;
		}
	}
	ldb->schema.num_attributes++;

	a[i].name	= attribute;
	a[i].flags	= flags;
	a[i].syntax	= syntax;

	if (a[i].flags & LDB_ATTR_FLAG_ALLOCATED) {
		a[i].name = talloc_strdup(a, a[i].name);
		if (a[i].name == NULL) {
			ldb_oom(ldb);
			return -1;
		}
	}

	return 0;
}

static const struct ldb_schema_syntax ldb_syntax_default = {
	.name            = LDB_SYNTAX_OCTET_STRING,
	.ldif_read_fn    = ldb_handler_copy,
	.ldif_write_fn   = ldb_handler_copy,
	.canonicalise_fn = ldb_handler_copy,
	.comparison_fn   = ldb_comparison_binary
};

static const struct ldb_schema_attribute ldb_attribute_default = {
	.name	= NULL,
	.flags	= 0,
	.syntax	= &ldb_syntax_default
};

/*
  return the attribute handlers for a given attribute
*/
static const struct ldb_schema_attribute *ldb_schema_attribute_by_name_internal(
	struct ldb_context *ldb,
	const char *name)
{
	/* for binary search we need signed variables */
	unsigned int i, e, b = 0;
	int r;
	const struct ldb_schema_attribute *def = &ldb_attribute_default;

	/* as handlers are sorted, '*' must be the first if present */
	if (strcmp(ldb->schema.attributes[0].name, "*") == 0) {
		def = &ldb->schema.attributes[0];
		b = 1;
	}

	/* do a binary search on the array */
	e = ldb->schema.num_attributes - 1;

	while ((b <= e) && (e != (unsigned int) -1)) {
		i = (b + e) / 2;

		r = ldb_attr_cmp(name, ldb->schema.attributes[i].name);
		if (r == 0) {
			return &ldb->schema.attributes[i];
		}
		if (r < 0) {
			e = i - 1;
		} else {
			b = i + 1;
		}
	}

	return def;
}

/*
  return the attribute handlers for a given attribute
*/
const struct ldb_schema_attribute *ldb_schema_attribute_by_name(struct ldb_context *ldb,
								const char *name)
{
	if (ldb->schema.attribute_handler_override) {
		const struct ldb_schema_attribute *ret = 
			ldb->schema.attribute_handler_override(ldb, 
							       ldb->schema.attribute_handler_override_private,
							       name);
		if (ret) {
			return ret;
		}
	}

	return ldb_schema_attribute_by_name_internal(ldb, name);
}


/*
  add to the list of ldif handlers for this ldb context
*/
void ldb_schema_attribute_remove(struct ldb_context *ldb, const char *name)
{
	const struct ldb_schema_attribute *a;
	ptrdiff_t i;

	a = ldb_schema_attribute_by_name_internal(ldb, name);
	if (a == NULL || a->name == NULL) {
		return;
	}

	/* FIXED attributes are never removed */
	if (a->flags & LDB_ATTR_FLAG_FIXED) {
		return;
	}

	if (a->flags & LDB_ATTR_FLAG_ALLOCATED) {
		talloc_free(discard_const_p(char, a->name));
	}

	i = a - ldb->schema.attributes;
	if (i < ldb->schema.num_attributes - 1) {
		memmove(&ldb->schema.attributes[i], 
			a+1, sizeof(*a) * (ldb->schema.num_attributes-(i+1)));
	}

	ldb->schema.num_attributes--;
}

/*
  setup a attribute handler using a standard syntax
*/
int ldb_schema_attribute_add(struct ldb_context *ldb,
			     const char *attribute,
			     unsigned flags,
			     const char *syntax)
{
	const struct ldb_schema_syntax *s = ldb_standard_syntax_by_name(ldb, syntax);
	return ldb_schema_attribute_add_with_syntax(ldb, attribute, flags, s);
}

/*
  setup the attribute handles for well known attributes
*/
int ldb_setup_wellknown_attributes(struct ldb_context *ldb)
{
	const struct {
		const char *attr;
		const char *syntax;
	} wellknown[] = {
		{ "dn", LDB_SYNTAX_DN },
		{ "distinguishedName", LDB_SYNTAX_DN },
		{ "cn", LDB_SYNTAX_DIRECTORY_STRING },
		{ "dc", LDB_SYNTAX_DIRECTORY_STRING },
		{ "ou", LDB_SYNTAX_DIRECTORY_STRING },
		{ "objectClass", LDB_SYNTAX_OBJECTCLASS }
	};
	unsigned int i;
	int ret;

	for (i=0;i<ARRAY_SIZE(wellknown);i++) {
		ret = ldb_schema_attribute_add(ldb, wellknown[i].attr, 0,
					       wellknown[i].syntax);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}


/*
  add a extended dn syntax to the ldb_schema
*/
int ldb_dn_extended_add_syntax(struct ldb_context *ldb, 
			       unsigned flags,
			       const struct ldb_dn_extended_syntax *syntax)
{
	unsigned int n;
	struct ldb_dn_extended_syntax *a;

	if (!syntax) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	n = ldb->schema.num_dn_extended_syntax + 1;

	a = talloc_realloc(ldb, ldb->schema.dn_extended_syntax,
			   struct ldb_dn_extended_syntax, n);

	if (!a) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	a[ldb->schema.num_dn_extended_syntax] = *syntax;
	ldb->schema.dn_extended_syntax = a;

	ldb->schema.num_dn_extended_syntax = n;

	return LDB_SUCCESS;
}

/*
  return the extended dn syntax for a given name
*/
const struct ldb_dn_extended_syntax *ldb_dn_extended_syntax_by_name(struct ldb_context *ldb,
								    const char *name)
{
	unsigned int i;
	for (i=0; i < ldb->schema.num_dn_extended_syntax; i++) {
		if (ldb_attr_cmp(ldb->schema.dn_extended_syntax[i].name, name) == 0) {
			return &ldb->schema.dn_extended_syntax[i];
		}
	}
	return NULL;
}

/*
  set an attribute handler override function - used to delegate schema handling
  to external code
 */
void ldb_schema_attribute_set_override_handler(struct ldb_context *ldb,
					       ldb_attribute_handler_override_fn_t override,
					       void *private_data)
{
	ldb->schema.attribute_handler_override_private = private_data;
	ldb->schema.attribute_handler_override = override;
}
