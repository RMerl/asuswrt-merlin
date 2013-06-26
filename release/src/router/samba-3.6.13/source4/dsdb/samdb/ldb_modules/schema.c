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
#include "ldb.h"
#include "ldb_module.h"
#include "librpc/ndr/libndr.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "dsdb/samdb/samdb.h"
#include "util.h"
#include "libcli/security/security.h"


const struct dsdb_class * get_last_structural_class(const struct dsdb_schema *schema,const struct ldb_message_element *element,
						    struct ldb_request *parent)
{
	const struct dsdb_class *last_class = NULL;
	unsigned int i;

	for (i = 0; i < element->num_values; i++){
		const struct dsdb_class *tmp_class = dsdb_class_by_lDAPDisplayName_ldb_val(schema, &element->values[i]);

		if(tmp_class == NULL) {
			continue;
		}

		if(tmp_class->objectClassCategory > 1) {
			continue;
		}

		if (!last_class) {
			last_class = tmp_class;
		} else {
			if (tmp_class->subClass_order > last_class->subClass_order)
				last_class = tmp_class;
		}
	}

	return last_class;
}

int acl_check_access_on_class(struct ldb_module *module,
			      const struct dsdb_schema *schema,
			      TALLOC_CTX *mem_ctx,
			      struct security_descriptor *sd,
			      struct dom_sid *rp_sid,
			      uint32_t access_mask,
			      const char *class_name)
{
	int ret;
	NTSTATUS status;
	uint32_t access_granted;
	struct object_tree *root = NULL;
	struct object_tree *new_node = NULL;
	const struct GUID *guid;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	struct security_token *token = acl_user_token(module);
	if (class_name) {
		guid = class_schemaid_guid_by_lDAPDisplayName(schema, class_name);
		if (!guid) {
			DEBUG(10, ("acl_search: cannot find class %s\n",
				   class_name));
			goto fail;
		}
		if (!insert_in_object_tree(tmp_ctx,
					   guid, access_mask,
					   &root, &new_node)) {
			DEBUG(10, ("acl_search: cannot add to object tree guid\n"));
			goto fail;
		}
	}
	status = sec_access_check_ds(sd, token,
				     access_mask,
				     &access_granted,
				     root,
				     rp_sid);
	if (!NT_STATUS_IS_OK(status)) {
		ret = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}
	else {
		ret = LDB_SUCCESS;
	}
	return ret;
fail:
	return ldb_operr(ldb_module_get_ctx(module));
}

const struct GUID *get_oc_guid_from_message(struct ldb_module *module,
						   const struct dsdb_schema *schema,
						   struct ldb_message *msg)
{
	struct ldb_message_element *oc_el;

	oc_el = ldb_msg_find_element(msg, "objectClass");
	if (!oc_el) {
		return NULL;
	}

	return class_schemaid_guid_by_lDAPDisplayName(schema,
						      (char *)oc_el->values[oc_el->num_values-1].data);
}


