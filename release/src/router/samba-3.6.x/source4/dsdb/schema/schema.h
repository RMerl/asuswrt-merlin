/* 
   Unix SMB/CIFS mplementation.
   DSDB schema header
   
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006
    
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

#ifndef _DSDB_SCHEMA_H
#define _DSDB_SCHEMA_H

#include "prefixmap.h"

struct dsdb_attribute;
struct dsdb_class;
struct dsdb_schema;
struct dsdb_dn;

struct dsdb_syntax_ctx {
	struct ldb_context *ldb;
	const struct dsdb_schema *schema;

	/* set when converting objects under Schema NC */
	bool is_schema_nc;

	/* remote prefixMap to be used for drsuapi_to_ldb conversions */
	const struct dsdb_schema_prefixmap *pfm_remote;
};


struct dsdb_syntax {
	const char *name;
	const char *ldap_oid;
	uint32_t oMSyntax;
	struct ldb_val oMObjectClass;
	const char *attributeSyntax_oid;
	const char *equality;
	const char *substring;
	const char *comment;
	const char *ldb_syntax;

	WERROR (*drsuapi_to_ldb)(const struct dsdb_syntax_ctx *ctx,
				 const struct dsdb_attribute *attr,
				 const struct drsuapi_DsReplicaAttribute *in,
				 TALLOC_CTX *mem_ctx,
				 struct ldb_message_element *out);
	WERROR (*ldb_to_drsuapi)(const struct dsdb_syntax_ctx *ctx,
				 const struct dsdb_attribute *attr,
				 const struct ldb_message_element *in,
				 TALLOC_CTX *mem_ctx,
				 struct drsuapi_DsReplicaAttribute *out);
	WERROR (*validate_ldb)(const struct dsdb_syntax_ctx *ctx,
			       const struct dsdb_attribute *attr,
			       const struct ldb_message_element *in);
};

struct dsdb_attribute {
	struct dsdb_attribute *prev, *next;

	const char *cn;
	const char *lDAPDisplayName;
	const char *attributeID_oid;
	uint32_t attributeID_id;
	struct GUID schemaIDGUID;
	uint32_t mAPIID;
	uint32_t msDS_IntId;

	struct GUID attributeSecurityGUID;
	struct GUID objectGUID;

	uint32_t searchFlags;
	uint32_t systemFlags;
	bool isMemberOfPartialAttributeSet;
	uint32_t linkID;

	const char *attributeSyntax_oid;
	uint32_t attributeSyntax_id;
	uint32_t oMSyntax;
	struct ldb_val oMObjectClass;

	bool isSingleValued;
	uint32_t *rangeLower;
	uint32_t *rangeUpper;
	bool extendedCharsAllowed;

	uint32_t schemaFlagsEx;
	struct ldb_val msDs_Schema_Extensions;

	bool showInAdvancedViewOnly;
	const char *adminDisplayName;
	const char *adminDescription;
	const char *classDisplayName;
	bool isEphemeral;
	bool isDefunct;
	bool systemOnly;

	/* internal stuff */
	const struct dsdb_syntax *syntax;
	const struct ldb_schema_attribute *ldb_schema_attribute;
};

struct dsdb_class {
	struct dsdb_class *prev, *next;

	const char *cn;
	const char *lDAPDisplayName;
	const char *governsID_oid;
	uint32_t governsID_id;
	struct GUID schemaIDGUID;
	struct GUID objectGUID;

	uint32_t objectClassCategory;
	const char *rDNAttID;
	const char *defaultObjectCategory;

	const char *subClassOf;

	const char **systemAuxiliaryClass;
	const char **systemPossSuperiors;
	const char **systemMustContain;
	const char **systemMayContain;

	const char **auxiliaryClass;
	const char **possSuperiors;
	const char **mustContain;
	const char **mayContain;
	const char **possibleInferiors;
	const char **systemPossibleInferiors;

	const char *defaultSecurityDescriptor;

	uint32_t schemaFlagsEx;
	struct ldb_val msDs_Schema_Extensions;

	bool showInAdvancedViewOnly;
	const char *adminDisplayName;
	const char *adminDescription;
	const char *classDisplayName;
	bool defaultHidingValue;
	bool isDefunct;
	bool systemOnly;

	const char **supclasses;
	const char **subclasses;
	const char **subclasses_direct;
	const char **posssuperiors;
	uint32_t subClassOf_id;
	uint32_t *systemAuxiliaryClass_ids;
	uint32_t *auxiliaryClass_ids;
	uint32_t *systemMayContain_ids;
	uint32_t *systemMustContain_ids;
	uint32_t *possSuperiors_ids;
	uint32_t *mustContain_ids;
	uint32_t *mayContain_ids;
	uint32_t *systemPossSuperiors_ids;

	/* An ordered index showing how this subClass fits into the
	 * subClass tree.  that is, an objectclass that is not
	 * subClassOf anything is 0 (just in case), and top is 1, and
	 * subClasses of top are 2, subclasses of those classes are
	 * 3 */ 
	uint32_t subClass_order;
};

/**
 * data stored in schemaInfo attribute
 */
struct dsdb_schema_info {
	uint32_t 	revision;
	struct GUID	invocation_id;
};


struct dsdb_schema {
	struct ldb_dn *base_dn;

	struct dsdb_schema_prefixmap *prefixmap;

	/* 
	 * the last element of the prefix mapping table isn't a oid,
	 * it starts with 0xFF and has 21 bytes and is maybe a schema
	 * version number
	 *
	 * this is the content of the schemaInfo attribute of the
	 * Schema-Partition head object.
	 */
	const char *schema_info;

	/* We can also tell the schema version from the USN on the partition */
	uint64_t loaded_usn;

	struct dsdb_attribute *attributes;
	struct dsdb_class *classes;

	/* lists of classes sorted by various attributes, for faster
	   access */
	uint32_t num_classes;
	struct dsdb_class **classes_by_lDAPDisplayName;
	struct dsdb_class **classes_by_governsID_id;
	struct dsdb_class **classes_by_governsID_oid;
	struct dsdb_class **classes_by_cn;

	/* lists of attributes sorted by various fields */
	uint32_t num_attributes;
	struct dsdb_attribute **attributes_by_lDAPDisplayName;
	struct dsdb_attribute **attributes_by_attributeID_id;
	struct dsdb_attribute **attributes_by_attributeID_oid;
	struct dsdb_attribute **attributes_by_linkID;
	uint32_t num_int_id_attr;
	struct dsdb_attribute **attributes_by_msDS_IntId;

	struct {
		bool we_are_master;
		struct ldb_dn *master_dn;
	} fsmo;

	/* Was this schema loaded from ldb (if so, then we will reload it when we detect a change in ldb) */
	struct ldb_module *loaded_from_module;
	struct dsdb_schema *(*refresh_fn)(struct ldb_module *module, struct dsdb_schema *schema, bool is_global_schema);
	bool refresh_in_progress;
	/* an 'opaque' sequence number that the reload function may also wish to use */
	uint64_t reload_seq_number;

	/* Should the syntax handlers in this case handle all incoming OIDs automatically, assigning them as an OID if no text name is known? */
	bool relax_OID_conversions;
};

enum dsdb_attr_list_query {
	DSDB_SCHEMA_ALL_MAY,
	DSDB_SCHEMA_ALL_MUST,
	DSDB_SCHEMA_SYS_MAY,
	DSDB_SCHEMA_SYS_MUST,
	DSDB_SCHEMA_MAY,
	DSDB_SCHEMA_MUST,
	DSDB_SCHEMA_ALL
};

enum dsdb_schema_convert_target {
	TARGET_OPENLDAP,
	TARGET_FEDORA_DS,
	TARGET_AD_SCHEMA_SUBENTRY
};

#include "dsdb/schema/proto.h"

#endif /* _DSDB_SCHEMA_H */
