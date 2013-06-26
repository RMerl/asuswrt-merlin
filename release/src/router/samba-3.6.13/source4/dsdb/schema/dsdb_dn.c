/* 
   Unix SMB/CIFS implementation.
   Samba utility functions

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

#include "includes.h"
#include "dsdb/samdb/samdb.h"
#include <ldb_module.h>
#include "librpc/ndr/libndr.h"
#include "libcli/security/dom_sid.h"

/*
   convert a dsdb_dn to a linked attribute data blob
*/
WERROR dsdb_dn_la_to_blob(struct ldb_context *sam_ctx,
			  const struct dsdb_attribute *schema_attrib,
			  const struct dsdb_schema *schema,
			  TALLOC_CTX *mem_ctx,
			  struct dsdb_dn *dsdb_dn, DATA_BLOB **blob)
{
	struct ldb_val v;
	WERROR werr;
	struct ldb_message_element val_el;
	struct drsuapi_DsReplicaAttribute drs;
	struct dsdb_syntax_ctx syntax_ctx;

	/* use default syntax conversion context */
	dsdb_syntax_ctx_init(&syntax_ctx, sam_ctx, schema);

	/* we need a message_element with just one value in it */
	v = data_blob_string_const(dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 1));

	val_el.name = schema_attrib->lDAPDisplayName;
	val_el.values = &v;
	val_el.num_values = 1;

	werr = schema_attrib->syntax->ldb_to_drsuapi(&syntax_ctx, schema_attrib, &val_el, mem_ctx, &drs);
	W_ERROR_NOT_OK_RETURN(werr);

	if (drs.value_ctr.num_values != 1) {
		DEBUG(1,(__location__ ": Failed to build DRS blob for linked attribute %s\n",
			 schema_attrib->lDAPDisplayName));
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	*blob = drs.value_ctr.values[0].blob;
	return WERR_OK;
}

/*
  convert a data blob to a dsdb_dn
 */
WERROR dsdb_dn_la_from_blob(struct ldb_context *sam_ctx,
			    const struct dsdb_attribute *schema_attrib,
			    const struct dsdb_schema *schema,
			    TALLOC_CTX *mem_ctx,
			    DATA_BLOB *blob,
			    struct dsdb_dn **dsdb_dn)
{
	WERROR werr;
	struct ldb_message_element new_el;
	struct drsuapi_DsReplicaAttribute drs;
	struct drsuapi_DsAttributeValue val;
	struct dsdb_syntax_ctx syntax_ctx;

	/* use default syntax conversion context */
	dsdb_syntax_ctx_init(&syntax_ctx, sam_ctx, schema);

	drs.value_ctr.num_values = 1;
	drs.value_ctr.values = &val;
	val.blob = blob;

	werr = schema_attrib->syntax->drsuapi_to_ldb(&syntax_ctx, schema_attrib, &drs, mem_ctx, &new_el);
	W_ERROR_NOT_OK_RETURN(werr);

	if (new_el.num_values != 1) {
		return WERR_INTERNAL_ERROR;
	}

	*dsdb_dn = dsdb_dn_parse(mem_ctx, sam_ctx, &new_el.values[0], schema_attrib->syntax->ldap_oid);
	if (!*dsdb_dn) {
		return WERR_INTERNAL_ERROR;
	}

	return WERR_OK;
}
