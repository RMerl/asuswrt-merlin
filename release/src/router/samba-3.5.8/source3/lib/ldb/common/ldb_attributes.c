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

#include "includes.h"
#include "ldb/include/includes.h"

/*
  add to the list of ldif handlers for this ldb context
*/
int ldb_set_attrib_handlers(struct ldb_context *ldb, 
			    const struct ldb_attrib_handler *handlers, 
			    unsigned num_handlers)
{
	int i;
	struct ldb_attrib_handler *h;
	h = talloc_realloc(ldb, ldb->schema.attrib_handlers,
			   struct ldb_attrib_handler,
			   ldb->schema.num_attrib_handlers + num_handlers);
	if (h == NULL) {
		ldb_oom(ldb);
		return -1;
	}
	ldb->schema.attrib_handlers = h;
	memcpy(h + ldb->schema.num_attrib_handlers, 
	       handlers, sizeof(*h) * num_handlers);
	for (i=0;i<num_handlers;i++) {
		if (h[ldb->schema.num_attrib_handlers+i].flags & LDB_ATTR_FLAG_ALLOCATED) {
			h[ldb->schema.num_attrib_handlers+i].attr = talloc_strdup(ldb->schema.attrib_handlers,
										  h[ldb->schema.num_attrib_handlers+i].attr);
			if (h[ldb->schema.num_attrib_handlers+i].attr == NULL) {
				ldb_oom(ldb);
				return -1;
			}
		}
	}
	ldb->schema.num_attrib_handlers += num_handlers;
	return 0;
}
			  

/*
  default function for read/write/canonicalise
*/
static int ldb_default_copy(struct ldb_context *ldb, 
			    void *mem_ctx,
			    const struct ldb_val *in, 
			    struct ldb_val *out)
{
	*out = ldb_val_dup(mem_ctx, in);

	if (out->data == NULL && in->data != NULL) {
		return -1;
	}

	return 0;
}

/*
  default function for comparison
*/
static int ldb_default_cmp(struct ldb_context *ldb, 
			    void *mem_ctx,
			   const struct ldb_val *v1, 
			   const struct ldb_val *v2)
{
	if (v1->length != v2->length) {
		return v1->length - v2->length;
	}
	return memcmp(v1->data, v2->data, v1->length);
}

/*
  default handler function pointers
*/
static const struct ldb_attrib_handler ldb_default_attrib_handler = {
	.attr = NULL,
	.ldif_read_fn    = ldb_default_copy,
	.ldif_write_fn   = ldb_default_copy,
	.canonicalise_fn = ldb_default_copy,
	.comparison_fn   = ldb_default_cmp,
};

/*
  return the attribute handlers for a given attribute
*/
const struct ldb_attrib_handler *ldb_attrib_handler(struct ldb_context *ldb,
						    const char *attrib)
{
	int i;
	const struct ldb_attrib_handler *def = &ldb_default_attrib_handler;
	/* TODO: should be replaced with a binary search, with a sort on add */
	for (i=0;i<ldb->schema.num_attrib_handlers;i++) {
		if (strcmp(ldb->schema.attrib_handlers[i].attr, "*") == 0) {
			def = &ldb->schema.attrib_handlers[i];
		}
		if (ldb_attr_cmp(attrib, ldb->schema.attrib_handlers[i].attr) == 0) {
			return &ldb->schema.attrib_handlers[i];
		}
	}
	return def;
}


/*
  add to the list of ldif handlers for this ldb context
*/
void ldb_remove_attrib_handler(struct ldb_context *ldb, const char *attrib)
{
	const struct ldb_attrib_handler *h;
	int i;
	h = ldb_attrib_handler(ldb, attrib);
	if (h == &ldb_default_attrib_handler) {
		return;
	}
	if (h->flags & LDB_ATTR_FLAG_ALLOCATED) {
		talloc_free(discard_const_p(char, h->attr));
	}
	i = h - ldb->schema.attrib_handlers;
	if (i < ldb->schema.num_attrib_handlers - 1) {
		memmove(&ldb->schema.attrib_handlers[i], 
			h+1, sizeof(*h) * (ldb->schema.num_attrib_handlers-(i+1)));
	}
	ldb->schema.num_attrib_handlers--;
}

/*
  setup a attribute handler using a standard syntax
*/
int ldb_set_attrib_handler_syntax(struct ldb_context *ldb, 
				  const char *attr, const char *syntax)
{
	const struct ldb_attrib_handler *h = ldb_attrib_handler_syntax(ldb, syntax);
	struct ldb_attrib_handler h2;
	if (h == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Unknown syntax '%s'\n", syntax);
		return -1;
	}
	h2 = *h;
	h2.attr = attr;
	return ldb_set_attrib_handlers(ldb, &h2, 1);
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
		{ "ncName", LDB_SYNTAX_DN },
		{ "distinguishedName", LDB_SYNTAX_DN },
		{ "cn", LDB_SYNTAX_DIRECTORY_STRING },
		{ "dc", LDB_SYNTAX_DIRECTORY_STRING },
		{ "ou", LDB_SYNTAX_DIRECTORY_STRING },
		{ "objectClass", LDB_SYNTAX_OBJECTCLASS }
	};
	int i;
	for (i=0;i<ARRAY_SIZE(wellknown);i++) {
		if (ldb_set_attrib_handler_syntax(ldb, wellknown[i].attr, 
						  wellknown[i].syntax) != 0) {
			return -1;
		}
	}
	return 0;
}


/*
  return the list of subclasses for a class
*/
const char **ldb_subclass_list(struct ldb_context *ldb, const char *classname)
{
	int i;
	for (i=0;i<ldb->schema.num_classes;i++) {
		if (ldb_attr_cmp(classname, ldb->schema.classes[i].name) == 0) {
			return (const char **)ldb->schema.classes[i].subclasses;
		}
	}
	return NULL;
}


/*
  add a new subclass
*/
static int ldb_subclass_new(struct ldb_context *ldb, const char *classname, const char *subclass)
{
	struct ldb_subclass *s, *c;
	s = talloc_realloc(ldb, ldb->schema.classes, struct ldb_subclass, ldb->schema.num_classes+1);
	if (s == NULL) goto failed;

	ldb->schema.classes = s;
	c = &s[ldb->schema.num_classes];
	c->name = talloc_strdup(s, classname);
	if (c->name == NULL) goto failed;

	c->subclasses = talloc_array(s, char *, 2);
	if (c->subclasses == NULL) goto failed;

	c->subclasses[0] = talloc_strdup(c->subclasses, subclass);
	if (c->subclasses[0] == NULL) goto failed;
	c->subclasses[1] = NULL;

	ldb->schema.num_classes++;

	return 0;
failed:
	ldb_oom(ldb);
	return -1;
}

/*
  add a subclass
*/
int ldb_subclass_add(struct ldb_context *ldb, const char *classname, const char *subclass)
{
	int i, n;
	struct ldb_subclass *c;
	char **s;

	for (i=0;i<ldb->schema.num_classes;i++) {
		if (ldb_attr_cmp(classname, ldb->schema.classes[i].name) == 0) {
			break;
		}
	}
	if (i == ldb->schema.num_classes) {
		return ldb_subclass_new(ldb, classname, subclass);
	}
	c = &ldb->schema.classes[i];
	
	for (n=0;c->subclasses[n];n++) /* noop */;

	s = talloc_realloc(ldb->schema.classes, c->subclasses, char *, n+2);
	if (s == NULL) {
		ldb_oom(ldb);
		return -1;
	}

	c->subclasses = s;
	s[n] = talloc_strdup(s, subclass);
	if (s[n] == NULL) {
		ldb_oom(ldb);
		return -1;
	}
	s[n+1] = NULL;

	return 0;
}

/*
  remove a set of subclasses for a class
*/
void ldb_subclass_remove(struct ldb_context *ldb, const char *classname)
{
	int i;
	struct ldb_subclass *c;

	for (i=0;i<ldb->schema.num_classes;i++) {
		if (ldb_attr_cmp(classname, ldb->schema.classes[i].name) == 0) {
			break;
		}
	}
	if (i == ldb->schema.num_classes) {
		return;
	}

	c = &ldb->schema.classes[i];
	talloc_free(c->name);
	talloc_free(c->subclasses);
	if (ldb->schema.num_classes-(i+1) > 0) {
		memmove(c, c+1, sizeof(*c) * (ldb->schema.num_classes-(i+1)));
	}
	ldb->schema.num_classes--;
	if (ldb->schema.num_classes == 0) {
		talloc_free(ldb->schema.classes);
		ldb->schema.classes = NULL;
	}
}
