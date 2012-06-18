/*
   ldb database mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006

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

#ifndef __LDB_MAP_H__
#define __LDB_MAP_H__

#include "ldb_module.h"

/* ldb_map is a skeleton LDB module that can be used for any other modules
 * that need to map attributes.
 *
 * The term 'remote' in this header refers to the connection where the 
 * original schema is used on while 'local' means the local connection 
 * that any upper layers will use.
 *
 * All local attributes will have to have a definition. Not all remote 
 * attributes need a definition as LDB is a lot less strict than LDAP 
 * (in other words, sending unknown attributes to an LDAP server hurts us, 
 * while returning too many attributes in ldb_search() doesn't)
 */


/* Name of the internal attribute pointing from the local to the
 * remote part of a record */
#define IS_MAPPED "isMapped"


struct ldb_map_context;

/* convert a local ldb_val to a remote ldb_val */
typedef struct ldb_val (*ldb_map_convert_func) (struct ldb_module *module, void *mem_ctx, const struct ldb_val *val);

#define LDB_MAP_MAX_REMOTE_NAMES 10

/* map from local to remote attribute */
struct ldb_map_attribute {
	const char *local_name; /* local name */

	enum ldb_map_attr_type { 
		MAP_IGNORE, /* Ignore this local attribute. Doesn't exist remotely.  */
		MAP_KEEP,   /* Keep as is. Same name locally and remotely. */
		MAP_RENAME, /* Simply rename the attribute. Name changes, data is the same */
		MAP_CONVERT, /* Rename + convert data */
		MAP_GENERATE /* Use generate function for generating new name/data. 
						Used for generating attributes based on 
						multiple remote attributes. */
	} type;
	
	/* if set, will be called for search expressions that contain this attribute */
	int (*convert_operator)(struct ldb_module *, TALLOC_CTX *ctx, struct ldb_parse_tree **ntree, const struct ldb_parse_tree *otree);

	union { 
		struct {
			const char *remote_name;
		} rename;
		
		struct {
			const char *remote_name;

			/* Convert local to remote data */
			ldb_map_convert_func convert_local;

			/* Convert remote to local data */
			/* an entry can have convert_remote set to NULL, as long as there as an entry with the same local_name 
			 * that is non-NULL before it. */
			ldb_map_convert_func convert_remote;
		} convert;
	
		struct {
			/* Generate the local attribute from remote message */
			struct ldb_message_element *(*generate_local)(struct ldb_module *, TALLOC_CTX *mem_ctx, const char *remote_attr, const struct ldb_message *remote);

			/* Update remote message with information from local message */
			void (*generate_remote)(struct ldb_module *, const char *local_attr, const struct ldb_message *old, struct ldb_message *remote, struct ldb_message *local);

			/* Name(s) for this attribute on the remote server. This is an array since 
			 * one local attribute's data can be split up into several attributes 
			 * remotely */
			const char *remote_names[LDB_MAP_MAX_REMOTE_NAMES];

			/* Names of additional remote attributes
			 * required for the generation.	 NULL
			 * indicates that `local_attr' suffices. */
			/*
#define LDB_MAP_MAX_SELF_ATTRIBUTES 10
			const char *self_attrs[LDB_MAP_MAX_SELF_ATTRIBUTES];
			*/
		} generate;
	} u;
};


#define LDB_MAP_MAX_SUBCLASSES	10
#define LDB_MAP_MAX_MUSTS		10
#define LDB_MAP_MAX_MAYS		50

/* map from local to remote objectClass */
struct ldb_map_objectclass {
	const char *local_name;
	const char *remote_name;
	const char *base_classes[LDB_MAP_MAX_SUBCLASSES];
	const char *musts[LDB_MAP_MAX_MUSTS];
	const char *mays[LDB_MAP_MAX_MAYS];
};


/* private context data */
struct ldb_map_context {
	struct ldb_map_attribute *attribute_maps;
	/* NOTE: Always declare base classes first here */
	const struct ldb_map_objectclass *objectclass_maps;

	/* Remote (often operational) attributes that should be added
	 * to any wildcard search */
	const char * const *wildcard_attributes;

	/* ObjectClass (if any) to be added to remote attributes on add */
	const char *add_objectclass;

	/* struct ldb_context *mapped_ldb; */
	struct ldb_dn *local_base_dn;
	struct ldb_dn *remote_base_dn;
};

/* Global private data */
struct map_private {
	void *caller_private;
	struct ldb_map_context *context;
};

/* Initialize global private data. */
int ldb_map_init(struct ldb_module *module, const struct ldb_map_attribute *attrs, 
		 const struct ldb_map_objectclass *ocls,
		 const char * const *wildcard_attributes,
		 const char *add_objectclass,
		 const char *name);

int map_add(struct ldb_module *module, struct ldb_request *req);
int map_search(struct ldb_module *module, struct ldb_request *req);
int map_rename(struct ldb_module *module, struct ldb_request *req);
int map_delete(struct ldb_module *module, struct ldb_request *req);
int map_modify(struct ldb_module *module, struct ldb_request *req);

#define LDB_MAP_OPS \
	.add		= map_add, \
	.modify		= map_modify, \
	.del		= map_delete, \
	.rename		= map_rename, \
	.search		= map_search,

#endif /* __LDB_MAP_H__ */
