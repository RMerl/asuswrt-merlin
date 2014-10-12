/*
   ldb database library

   Copyright (C) Simo Sorce  2004-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
   Copyright (C) Matthieu Patou <mat@samba.org> 2010

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
 *  Name: ldb
 *
 *  Component: ldb repl_meta_data module
 *
 *  Description: - add a unique objectGUID onto every new record,
 *               - handle whenCreated, whenChanged timestamps
 *               - handle uSNCreated, uSNChanged numbers
 *               - handle replPropertyMetaData attribute
 *
 *  Author: Simo Sorce
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/proto.h"
#include "../libds/common/flags.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"
#include "libcli/security/security.h"
#include "lib/util/dlinklist.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "lib/util/binsearch.h"
#include "lib/util/tsort.h"

struct replmd_private {
	TALLOC_CTX *la_ctx;
	struct la_entry *la_list;
	TALLOC_CTX *bl_ctx;
	struct la_backlink *la_backlinks;
	struct nc_entry {
		struct nc_entry *prev, *next;
		struct ldb_dn *dn;
		uint64_t mod_usn;
		uint64_t mod_usn_urgent;
	} *ncs;
};

struct la_entry {
	struct la_entry *next, *prev;
	struct drsuapi_DsReplicaLinkedAttribute *la;
};

struct replmd_replicated_request {
	struct ldb_module *module;
	struct ldb_request *req;

	const struct dsdb_schema *schema;

	/* the controls we pass down */
	struct ldb_control **controls;

	/* details for the mode where we apply a bunch of inbound replication meessages */
	bool apply_mode;
	uint32_t index_current;
	struct dsdb_extended_replicated_objects *objs;

	struct ldb_message *search_msg;

	uint64_t seq_num;
	bool is_urgent;
};

enum urgent_situation {
	REPL_URGENT_ON_CREATE = 1,
	REPL_URGENT_ON_UPDATE = 2,
	REPL_URGENT_ON_DELETE = 4
};


static const struct {
	const char *update_name;
	enum urgent_situation repl_situation;
} urgent_objects[] = {
		{"nTDSDSA", (REPL_URGENT_ON_CREATE | REPL_URGENT_ON_DELETE)},
		{"crossRef", (REPL_URGENT_ON_CREATE | REPL_URGENT_ON_DELETE)},
		{"attributeSchema", (REPL_URGENT_ON_CREATE | REPL_URGENT_ON_UPDATE)},
		{"classSchema", (REPL_URGENT_ON_CREATE | REPL_URGENT_ON_UPDATE)},
		{"secret", (REPL_URGENT_ON_CREATE | REPL_URGENT_ON_UPDATE)},
		{"rIDManager", (REPL_URGENT_ON_CREATE | REPL_URGENT_ON_UPDATE)},
		{NULL, 0}
};

/* Attributes looked for when updating or deleting, to check for a urgent replication needed */
static const char *urgent_attrs[] = {
		"lockoutTime",
		"pwdLastSet",
		"userAccountControl",
		NULL
};


static bool replmd_check_urgent_objectclass(const struct ldb_message_element *objectclass_el,
					enum urgent_situation situation)
{
	unsigned int i, j;
	for (i=0; urgent_objects[i].update_name; i++) {

		if ((situation & urgent_objects[i].repl_situation) == 0) {
			continue;
		}

		for (j=0; j<objectclass_el->num_values; j++) {
			const struct ldb_val *v = &objectclass_el->values[j];
			if (ldb_attr_cmp((const char *)v->data, urgent_objects[i].update_name) == 0) {
				return true;
			}
		}
	}
	return false;
}

static bool replmd_check_urgent_attribute(const struct ldb_message_element *el)
{
	if (ldb_attr_in_list(urgent_attrs, el->name)) {
		return true;
	}
	return false;
}


static int replmd_replicated_apply_next(struct replmd_replicated_request *ar);

/*
  initialise the module
  allocate the private structure and build the list
  of partition DNs for use by replmd_notify()
 */
static int replmd_init(struct ldb_module *module)
{
	struct replmd_private *replmd_private;
	struct ldb_context *ldb = ldb_module_get_ctx(module);

	replmd_private = talloc_zero(module, struct replmd_private);
	if (replmd_private == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ldb_module_set_private(module, replmd_private);

	return ldb_next_init(module);
}

/*
  cleanup our per-transaction contexts
 */
static void replmd_txn_cleanup(struct replmd_private *replmd_private)
{
	talloc_free(replmd_private->la_ctx);
	replmd_private->la_list = NULL;
	replmd_private->la_ctx = NULL;

	talloc_free(replmd_private->bl_ctx);
	replmd_private->la_backlinks = NULL;
	replmd_private->bl_ctx = NULL;
}


struct la_backlink {
	struct la_backlink *next, *prev;
	const char *attr_name;
	struct GUID forward_guid, target_guid;
	bool active;
};

/*
  process a backlinks we accumulated during a transaction, adding and
  deleting the backlinks from the target objects
 */
static int replmd_process_backlink(struct ldb_module *module, struct la_backlink *bl, struct ldb_request *parent)
{
	struct ldb_dn *target_dn, *source_dn;
	int ret;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_message *msg;
	TALLOC_CTX *tmp_ctx = talloc_new(bl);
	char *dn_string;

	/*
	  - find DN of target
	  - find DN of source
	  - construct ldb_message
              - either an add or a delete
	 */
	ret = dsdb_module_dn_by_guid(module, tmp_ctx, &bl->target_guid, &target_dn, parent);
	if (ret != LDB_SUCCESS) {
		DEBUG(2,(__location__ ": WARNING: Failed to find target DN for linked attribute with GUID %s\n",
			 GUID_string(bl, &bl->target_guid)));
		return LDB_SUCCESS;
	}

	ret = dsdb_module_dn_by_guid(module, tmp_ctx, &bl->forward_guid, &source_dn, parent);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, "Failed to find source DN for linked attribute with GUID %s\n",
				       GUID_string(bl, &bl->forward_guid));
		talloc_free(tmp_ctx);
		return ret;
	}

	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		ldb_module_oom(module);
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* construct a ldb_message for adding/deleting the backlink */
	msg->dn = target_dn;
	dn_string = ldb_dn_get_extended_linearized(tmp_ctx, source_dn, 1);
	if (!dn_string) {
		ldb_module_oom(module);
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret = ldb_msg_add_steal_string(msg, bl->attr_name, dn_string);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	msg->elements[0].flags = bl->active?LDB_FLAG_MOD_ADD:LDB_FLAG_MOD_DELETE;

	/* a backlink should never be single valued. Unfortunately the
	   exchange schema has a attribute
	   msExchBridgeheadedLocalConnectorsDNBL which is single
	   valued and a backlink. We need to cope with that by
	   ignoring the single value flag */
	msg->elements[0].flags |= LDB_FLAG_INTERNAL_DISABLE_SINGLE_VALUE_CHECK;

	ret = dsdb_module_modify(module, msg, DSDB_FLAG_NEXT_MODULE, parent);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, "Failed to %s backlink from %s to %s - %s",
				       bl->active?"add":"remove",
				       ldb_dn_get_linearized(source_dn),
				       ldb_dn_get_linearized(target_dn),
				       ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}
	talloc_free(tmp_ctx);
	return ret;
}

/*
  add a backlink to the list of backlinks to add/delete in the prepare
  commit
 */
static int replmd_add_backlink(struct ldb_module *module, const struct dsdb_schema *schema,
			       struct GUID *forward_guid, struct GUID *target_guid,
			       bool active, const struct dsdb_attribute *schema_attr, bool immediate)
{
	const struct dsdb_attribute *target_attr;
	struct la_backlink *bl;
	struct replmd_private *replmd_private =
		talloc_get_type_abort(ldb_module_get_private(module), struct replmd_private);

	target_attr = dsdb_attribute_by_linkID(schema, schema_attr->linkID ^ 1);
	if (!target_attr) {
		/*
		 * windows 2003 has a broken schema where the
		 * definition of msDS-IsDomainFor is missing (which is
		 * supposed to be the backlink of the
		 * msDS-HasDomainNCs attribute
		 */
		return LDB_SUCCESS;
	}

	/* see if its already in the list */
	for (bl=replmd_private->la_backlinks; bl; bl=bl->next) {
		if (GUID_equal(forward_guid, &bl->forward_guid) &&
		    GUID_equal(target_guid, &bl->target_guid) &&
		    (target_attr->lDAPDisplayName == bl->attr_name ||
		     strcmp(target_attr->lDAPDisplayName, bl->attr_name) == 0)) {
			break;
		}
	}

	if (bl) {
		/* we found an existing one */
		if (bl->active == active) {
			return LDB_SUCCESS;
		}
		DLIST_REMOVE(replmd_private->la_backlinks, bl);
		talloc_free(bl);
		return LDB_SUCCESS;
	}

	if (replmd_private->bl_ctx == NULL) {
		replmd_private->bl_ctx = talloc_new(replmd_private);
		if (replmd_private->bl_ctx == NULL) {
			ldb_module_oom(module);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	/* its a new one */
	bl = talloc(replmd_private->bl_ctx, struct la_backlink);
	if (bl == NULL) {
		ldb_module_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Ensure the schema does not go away before the bl->attr_name is used */
	if (!talloc_reference(bl, schema)) {
		talloc_free(bl);
		ldb_module_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	bl->attr_name = target_attr->lDAPDisplayName;
	bl->forward_guid = *forward_guid;
	bl->target_guid = *target_guid;
	bl->active = active;

	/* the caller may ask for this backlink to be processed
	   immediately */
	if (immediate) {
		int ret = replmd_process_backlink(module, bl, NULL);
		talloc_free(bl);
		return ret;
	}

	DLIST_ADD(replmd_private->la_backlinks, bl);

	return LDB_SUCCESS;
}


/*
 * Callback for most write operations in this module:
 *
 * notify the repl task that a object has changed. The notifies are
 * gathered up in the replmd_private structure then written to the
 * @REPLCHANGED object in each partition during the prepare_commit
 */
static int replmd_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	int ret;
	struct replmd_replicated_request *ac =
		talloc_get_type_abort(req->context, struct replmd_replicated_request);
	struct replmd_private *replmd_private =
		talloc_get_type_abort(ldb_module_get_private(ac->module), struct replmd_private);
	struct nc_entry *modified_partition;
	struct ldb_control *partition_ctrl;
	const struct dsdb_control_current_partition *partition;

	struct ldb_control **controls;

	partition_ctrl = ldb_reply_get_control(ares, DSDB_CONTROL_CURRENT_PARTITION_OID);

	controls = ares->controls;
	if (ldb_request_get_control(ac->req,
				    DSDB_CONTROL_CURRENT_PARTITION_OID) == NULL) {
		/*
		 * Remove the current partition control from what we pass up
		 * the chain if it hasn't been requested manually.
		 */
		controls = ldb_controls_except_specified(ares->controls, ares,
							 partition_ctrl);
	}

	if (ares->error != LDB_SUCCESS) {
		DEBUG(0,("%s failure. Error is: %s\n", __FUNCTION__, ldb_strerror(ares->error)));
		return ldb_module_done(ac->req, controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb_module_get_ctx(ac->module), "Invalid reply type for notify\n!");
		return ldb_module_done(ac->req, NULL,
				       NULL, LDB_ERR_OPERATIONS_ERROR);
	}

	if (!partition_ctrl) {
		ldb_set_errstring(ldb_module_get_ctx(ac->module),"No partition control on reply");
		return ldb_module_done(ac->req, NULL,
				       NULL, LDB_ERR_OPERATIONS_ERROR);
	}

	partition = talloc_get_type_abort(partition_ctrl->data,
				    struct dsdb_control_current_partition);

	if (ac->seq_num > 0) {
		for (modified_partition = replmd_private->ncs; modified_partition;
		     modified_partition = modified_partition->next) {
			if (ldb_dn_compare(modified_partition->dn, partition->dn) == 0) {
				break;
			}
		}

		if (modified_partition == NULL) {
			modified_partition = talloc_zero(replmd_private, struct nc_entry);
			if (!modified_partition) {
				ldb_oom(ldb_module_get_ctx(ac->module));
				return ldb_module_done(ac->req, NULL,
						       NULL, LDB_ERR_OPERATIONS_ERROR);
			}
			modified_partition->dn = ldb_dn_copy(modified_partition, partition->dn);
			if (!modified_partition->dn) {
				ldb_oom(ldb_module_get_ctx(ac->module));
				return ldb_module_done(ac->req, NULL,
						       NULL, LDB_ERR_OPERATIONS_ERROR);
			}
			DLIST_ADD(replmd_private->ncs, modified_partition);
		}

		if (ac->seq_num > modified_partition->mod_usn) {
			modified_partition->mod_usn = ac->seq_num;
			if (ac->is_urgent) {
				modified_partition->mod_usn_urgent = ac->seq_num;
			}
		}
	}

	if (ac->apply_mode) {
		talloc_free(ares);
		ac->index_current++;

		ret = replmd_replicated_apply_next(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
		return ret;
	} else {
		/* free the partition control container here, for the
		 * common path.  Other cases will have it cleaned up
		 * eventually with the ares */
		talloc_free(partition_ctrl);
		return ldb_module_done(ac->req, controls,
				       ares->response, LDB_SUCCESS);
	}
}


/*
 * update a @REPLCHANGED record in each partition if there have been
 * any writes of replicated data in the partition
 */
static int replmd_notify_store(struct ldb_module *module, struct ldb_request *parent)
{
	struct replmd_private *replmd_private =
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);

	while (replmd_private->ncs) {
		int ret;
		struct nc_entry *modified_partition = replmd_private->ncs;

		ret = dsdb_module_save_partition_usn(module, modified_partition->dn,
						     modified_partition->mod_usn,
						     modified_partition->mod_usn_urgent, parent);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to save partition uSN for %s\n",
				 ldb_dn_get_linearized(modified_partition->dn)));
			return ret;
		}
		DLIST_REMOVE(replmd_private->ncs, modified_partition);
		talloc_free(modified_partition);
	}

	return LDB_SUCCESS;
}


/*
  created a replmd_replicated_request context
 */
static struct replmd_replicated_request *replmd_ctx_init(struct ldb_module *module,
							 struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;

	ldb = ldb_module_get_ctx(module);

	ac = talloc_zero(req, struct replmd_replicated_request);
	if (ac == NULL) {
		ldb_oom(ldb);
		return NULL;
	}

	ac->module = module;
	ac->req	= req;

	ac->schema = dsdb_get_schema(ldb, ac);
	if (!ac->schema) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "replmd_modify: no dsdb_schema loaded");
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return NULL;
	}

	return ac;
}

/*
  add a time element to a record
*/
static int add_time_element(struct ldb_message *msg, const char *attr, time_t t)
{
	struct ldb_message_element *el;
	char *s;
	int ret;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return LDB_SUCCESS;
	}

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_msg_add_string(msg, attr, s);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}

/*
  add a uint64_t element to a record
*/
static int add_uint64_element(struct ldb_context *ldb, struct ldb_message *msg,
			      const char *attr, uint64_t v)
{
	struct ldb_message_element *el;
	int ret;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return LDB_SUCCESS;
	}

	ret = samdb_msg_add_uint64(ldb, msg, msg, attr, v);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	el = ldb_msg_find_element(msg, attr);
	/* always set as replace. This works because on add ops, the flag
	   is ignored */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}

static int replmd_replPropertyMetaData1_attid_sort(const struct replPropertyMetaData1 *m1,
						   const struct replPropertyMetaData1 *m2,
						   const uint32_t *rdn_attid)
{
	if (m1->attid == m2->attid) {
		return 0;
	}

	/*
	 * the rdn attribute should be at the end!
	 * so we need to return a value greater than zero
	 * which means m1 is greater than m2
	 */
	if (m1->attid == *rdn_attid) {
		return 1;
	}

	/*
	 * the rdn attribute should be at the end!
	 * so we need to return a value less than zero
	 * which means m2 is greater than m1
	 */
	if (m2->attid == *rdn_attid) {
		return -1;
	}

	return m1->attid > m2->attid ? 1 : -1;
}

static int replmd_replPropertyMetaDataCtr1_sort(struct replPropertyMetaDataCtr1 *ctr1,
						const struct dsdb_schema *schema,
						struct ldb_dn *dn)
{
	const char *rdn_name;
	const struct dsdb_attribute *rdn_sa;

	rdn_name = ldb_dn_get_rdn_name(dn);
	if (!rdn_name) {
		DEBUG(0,(__location__ ": No rDN for %s?\n", ldb_dn_get_linearized(dn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	rdn_sa = dsdb_attribute_by_lDAPDisplayName(schema, rdn_name);
	if (rdn_sa == NULL) {
		DEBUG(0,(__location__ ": No sa found for rDN %s for %s\n", rdn_name, ldb_dn_get_linearized(dn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	DEBUG(6,("Sorting rpmd with attid exception %u rDN=%s DN=%s\n",
		 rdn_sa->attributeID_id, rdn_name, ldb_dn_get_linearized(dn)));

	LDB_TYPESAFE_QSORT(ctr1->array, ctr1->count, &rdn_sa->attributeID_id, replmd_replPropertyMetaData1_attid_sort);

	return LDB_SUCCESS;
}

static int replmd_ldb_message_element_attid_sort(const struct ldb_message_element *e1,
						 const struct ldb_message_element *e2,
						 const struct dsdb_schema *schema)
{
	const struct dsdb_attribute *a1;
	const struct dsdb_attribute *a2;

	/*
	 * TODO: make this faster by caching the dsdb_attribute pointer
	 *       on the ldb_messag_element
	 */

	a1 = dsdb_attribute_by_lDAPDisplayName(schema, e1->name);
	a2 = dsdb_attribute_by_lDAPDisplayName(schema, e2->name);

	/*
	 * TODO: remove this check, we should rely on e1 and e2 having valid attribute names
	 *       in the schema
	 */
	if (!a1 || !a2) {
		return strcasecmp(e1->name, e2->name);
	}
	if (a1->attributeID_id == a2->attributeID_id) {
		return 0;
	}
	return a1->attributeID_id > a2->attributeID_id ? 1 : -1;
}

static void replmd_ldb_message_sort(struct ldb_message *msg,
				    const struct dsdb_schema *schema)
{
	LDB_TYPESAFE_QSORT(msg->elements, msg->num_elements, schema, replmd_ldb_message_element_attid_sort);
}

static int replmd_build_la_val(TALLOC_CTX *mem_ctx, struct ldb_val *v, struct dsdb_dn *dsdb_dn,
			       const struct GUID *invocation_id, uint64_t seq_num,
			       uint64_t local_usn, NTTIME nttime, uint32_t version, bool deleted);


/*
  fix up linked attributes in replmd_add.
  This involves setting up the right meta-data in extended DN
  components, and creating backlinks to the object
 */
static int replmd_add_fix_la(struct ldb_module *module, struct ldb_message_element *el,
			     uint64_t seq_num, const struct GUID *invocationId, time_t t,
			     struct GUID *guid, const struct dsdb_attribute *sa, struct ldb_request *parent)
{
	unsigned int i;
	TALLOC_CTX *tmp_ctx = talloc_new(el->values);
	struct ldb_context *ldb = ldb_module_get_ctx(module);

	/* We will take a reference to the schema in replmd_add_backlink */
	const struct dsdb_schema *schema = dsdb_get_schema(ldb, NULL);
	NTTIME now;

	unix_to_nt_time(&now, t);

	for (i=0; i<el->num_values; i++) {
		struct ldb_val *v = &el->values[i];
		struct dsdb_dn *dsdb_dn = dsdb_dn_parse(tmp_ctx, ldb, v, sa->syntax->ldap_oid);
		struct GUID target_guid;
		NTSTATUS status;
		int ret;

		/* note that the DN already has the extended
		   components from the extended_dn_store module */
		status = dsdb_get_extended_dn_guid(dsdb_dn->dn, &target_guid, "GUID");
		if (!NT_STATUS_IS_OK(status) || GUID_all_zero(&target_guid)) {
			ret = dsdb_module_guid_by_dn(module, dsdb_dn->dn, &target_guid, parent);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
			ret = dsdb_set_extended_dn_guid(dsdb_dn->dn, &target_guid, "GUID");
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
		}

		ret = replmd_build_la_val(el->values, v, dsdb_dn, invocationId,
					  seq_num, seq_num, now, 0, false);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		ret = replmd_add_backlink(module, schema, guid, &target_guid, true, sa, false);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}


/*
  intercept add requests
 */
static int replmd_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
        struct ldb_control *control;
	struct replmd_replicated_request *ac;
	enum ndr_err_code ndr_err;
	struct ldb_request *down_req;
	struct ldb_message *msg;
        const DATA_BLOB *guid_blob;
	struct GUID guid;
	struct replPropertyMetaDataBlob nmd;
	struct ldb_val nmd_value;
	const struct GUID *our_invocation_id;
	time_t t = time(NULL);
	NTTIME now;
	char *time_str;
	int ret;
	unsigned int i;
	unsigned int functional_level;
	uint32_t ni=0;
	bool allow_add_guid = false;
	bool remove_current_guid = false;
	bool is_urgent = false;
	struct ldb_message_element *objectclass_el;

        /* check if there's a show relax control (used by provision to say 'I know what I'm doing') */
        control = ldb_request_get_control(req, LDB_CONTROL_RELAX_OID);
	if (control) {
		allow_add_guid = true;
	}

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_add\n");

	guid_blob = ldb_msg_find_ldb_val(req->op.add.message, "objectGUID");
	if (guid_blob != NULL) {
		if (!allow_add_guid) {
			ldb_set_errstring(ldb,
					  "replmd_add: it's not allowed to add an object with objectGUID!");
			return LDB_ERR_UNWILLING_TO_PERFORM;
		} else {
			NTSTATUS status = GUID_from_data_blob(guid_blob,&guid);
			if (!NT_STATUS_IS_OK(status)) {
				ldb_set_errstring(ldb,
						  "replmd_add: Unable to parse the 'objectGUID' as a GUID!");
				return LDB_ERR_UNWILLING_TO_PERFORM;
			}
			/* we remove this attribute as it can be a string and
			 * will not be treated correctly and then we will re-add
			 * it later on in the good format */
			remove_current_guid = true;
		}
	} else {
		/* a new GUID */
		guid = GUID_random();
	}

	ac = replmd_ctx_init(module, req);
	if (ac == NULL) {
		return ldb_module_oom(module);
	}

	functional_level = dsdb_functional_level(ldb);

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &ac->seq_num);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* get our invocationId */
	our_invocation_id = samdb_ntds_invocation_id(ldb);
	if (!our_invocation_id) {
		ldb_debug_set(ldb, LDB_DEBUG_ERROR,
			      "replmd_add: unable to find invocationId\n");
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.add.message);
	if (msg == NULL) {
		ldb_oom(ldb);
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* generated times */
	unix_to_nt_time(&now, t);
	time_str = ldb_timestring(msg, t);
	if (!time_str) {
		ldb_oom(ldb);
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	if (remove_current_guid) {
		ldb_msg_remove_attr(msg,"objectGUID");
	}

	/*
	 * remove autogenerated attributes
	 */
	ldb_msg_remove_attr(msg, "whenCreated");
	ldb_msg_remove_attr(msg, "whenChanged");
	ldb_msg_remove_attr(msg, "uSNCreated");
	ldb_msg_remove_attr(msg, "uSNChanged");
	ldb_msg_remove_attr(msg, "replPropertyMetaData");

	/*
	 * readd replicated attributes
	 */
	ret = ldb_msg_add_string(msg, "whenCreated", time_str);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		talloc_free(ac);
		return ret;
	}

	/* build the replication meta_data */
	ZERO_STRUCT(nmd);
	nmd.version		= 1;
	nmd.ctr.ctr1.count	= msg->num_elements;
	nmd.ctr.ctr1.array	= talloc_array(msg,
					       struct replPropertyMetaData1,
					       nmd.ctr.ctr1.count);
	if (!nmd.ctr.ctr1.array) {
		ldb_oom(ldb);
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; i < msg->num_elements; i++) {
		struct ldb_message_element *e = &msg->elements[i];
		struct replPropertyMetaData1 *m = &nmd.ctr.ctr1.array[ni];
		const struct dsdb_attribute *sa;

		if (e->name[0] == '@') continue;

		sa = dsdb_attribute_by_lDAPDisplayName(ac->schema, e->name);
		if (!sa) {
			ldb_debug_set(ldb, LDB_DEBUG_ERROR,
				      "replmd_add: attribute '%s' not defined in schema\n",
				      e->name);
			talloc_free(ac);
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}

		if ((sa->systemFlags & DS_FLAG_ATTR_NOT_REPLICATED) || (sa->systemFlags & DS_FLAG_ATTR_IS_CONSTRUCTED)) {
			/* if the attribute is not replicated (0x00000001)
			 * or constructed (0x00000004) it has no metadata
			 */
			continue;
		}

		if (sa->linkID != 0 && functional_level > DS_DOMAIN_FUNCTION_2000) {
			ret = replmd_add_fix_la(module, e, ac->seq_num, our_invocation_id, t, &guid, sa, req);
			if (ret != LDB_SUCCESS) {
				talloc_free(ac);
				return ret;
			}
			/* linked attributes are not stored in
			   replPropertyMetaData in FL above w2k */
			continue;
		}

		m->attid			= sa->attributeID_id;
		m->version			= 1;
		m->originating_change_time	= now;
		m->originating_invocation_id	= *our_invocation_id;
		m->originating_usn		= ac->seq_num;
		m->local_usn			= ac->seq_num;
		ni++;
	}

	/* fix meta data count */
	nmd.ctr.ctr1.count = ni;

	/*
	 * sort meta data array, and move the rdn attribute entry to the end
	 */
	ret = replmd_replPropertyMetaDataCtr1_sort(&nmd.ctr.ctr1, ac->schema, msg->dn);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* generated NDR encoded values */
	ndr_err = ndr_push_struct_blob(&nmd_value, msg,
				       &nmd,
				       (ndr_push_flags_fn_t)ndr_push_replPropertyMetaDataBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		ldb_oom(ldb);
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * add the autogenerated values
	 */
	ret = dsdb_msg_add_guid(msg, &guid, "objectGUID");
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		talloc_free(ac);
		return ret;
	}
	ret = ldb_msg_add_string(msg, "whenChanged", time_str);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		talloc_free(ac);
		return ret;
	}
	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNCreated", ac->seq_num);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		talloc_free(ac);
		return ret;
	}
	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNChanged", ac->seq_num);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		talloc_free(ac);
		return ret;
	}
	ret = ldb_msg_add_value(msg, "replPropertyMetaData", &nmd_value, NULL);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		talloc_free(ac);
		return ret;
	}

	/*
	 * sort the attributes by attid before storing the object
	 */
	replmd_ldb_message_sort(msg, ac->schema);

	objectclass_el = ldb_msg_find_element(msg, "objectClass");
	is_urgent = replmd_check_urgent_objectclass(objectclass_el,
							REPL_URGENT_ON_CREATE);

	ac->is_urgent = is_urgent;
	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, replmd_op_callback,
				req);

	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* current partition control is needed by "replmd_op_callback" */
	if (ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID) == NULL) {
		ret = ldb_request_add_control(down_req,
					      DSDB_CONTROL_CURRENT_PARTITION_OID,
					      false, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}
	}

	if (functional_level == DS_DOMAIN_FUNCTION_2000) {
		ret = ldb_request_add_control(down_req, DSDB_CONTROL_APPLY_LINKS, false, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}
	}

	/* mark the control done */
	if (control) {
		control->critical = 0;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}


/*
 * update the replPropertyMetaData for one element
 */
static int replmd_update_rpmd_element(struct ldb_context *ldb,
				      struct ldb_message *msg,
				      struct ldb_message_element *el,
				      struct ldb_message_element *old_el,
				      struct replPropertyMetaDataBlob *omd,
				      const struct dsdb_schema *schema,
				      uint64_t *seq_num,
				      const struct GUID *our_invocation_id,
				      NTTIME now)
{
	uint32_t i;
	const struct dsdb_attribute *a;
	struct replPropertyMetaData1 *md1;

	a = dsdb_attribute_by_lDAPDisplayName(schema, el->name);
	if (a == NULL) {
		DEBUG(0,(__location__ ": Unable to find attribute %s in schema\n",
			 el->name));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if ((a->systemFlags & DS_FLAG_ATTR_NOT_REPLICATED) || (a->systemFlags & DS_FLAG_ATTR_IS_CONSTRUCTED)) {
		return LDB_SUCCESS;
	}

	/* if the attribute's value haven't changed then return LDB_SUCCESS	*/
	if (old_el != NULL && ldb_msg_element_compare(el, old_el) == 0) {
		return LDB_SUCCESS;
	}

	for (i=0; i<omd->ctr.ctr1.count; i++) {
		if (a->attributeID_id == omd->ctr.ctr1.array[i].attid) break;
	}

	if (a->linkID != 0 && dsdb_functional_level(ldb) > DS_DOMAIN_FUNCTION_2000) {
		/* linked attributes are not stored in
		   replPropertyMetaData in FL above w2k, but we do
		   raise the seqnum for the object  */
		if (*seq_num == 0 &&
		    ldb_sequence_number(ldb, LDB_SEQ_NEXT, seq_num) != LDB_SUCCESS) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		return LDB_SUCCESS;
	}

	if (i == omd->ctr.ctr1.count) {
		/* we need to add a new one */
		omd->ctr.ctr1.array = talloc_realloc(msg, omd->ctr.ctr1.array,
						     struct replPropertyMetaData1, omd->ctr.ctr1.count+1);
		if (omd->ctr.ctr1.array == NULL) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		omd->ctr.ctr1.count++;
		ZERO_STRUCT(omd->ctr.ctr1.array[i]);
	}

	/* Get a new sequence number from the backend. We only do this
	 * if we have a change that requires a new
	 * replPropertyMetaData element
	 */
	if (*seq_num == 0) {
		int ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, seq_num);
		if (ret != LDB_SUCCESS) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	md1 = &omd->ctr.ctr1.array[i];
	md1->version++;
	md1->attid                     = a->attributeID_id;
	md1->originating_change_time   = now;
	md1->originating_invocation_id = *our_invocation_id;
	md1->originating_usn           = *seq_num;
	md1->local_usn                 = *seq_num;

	return LDB_SUCCESS;
}

static uint64_t find_max_local_usn(struct replPropertyMetaDataBlob omd)
{
	uint32_t count = omd.ctr.ctr1.count;
	uint64_t max = 0;
	uint32_t i;
	for (i=0; i < count; i++) {
		struct replPropertyMetaData1 m = omd.ctr.ctr1.array[i];
		if (max < m.local_usn) {
			max = m.local_usn;
		}
	}
	return max;
}

/*
 * update the replPropertyMetaData object each time we modify an
 * object. This is needed for DRS replication, as the merge on the
 * client is based on this object
 */
static int replmd_update_rpmd(struct ldb_module *module,
			      const struct dsdb_schema *schema,
			      struct ldb_request *req,
			      struct ldb_message *msg, uint64_t *seq_num,
			      time_t t,
			      bool *is_urgent)
{
	const struct ldb_val *omd_value;
	enum ndr_err_code ndr_err;
	struct replPropertyMetaDataBlob omd;
	unsigned int i;
	NTTIME now;
	const struct GUID *our_invocation_id;
	int ret;
	const char *attrs[] = { "replPropertyMetaData", "*", NULL };
	const char *attrs2[] = { "uSNChanged", "objectClass", NULL };
	struct ldb_result *res;
	struct ldb_context *ldb;
	struct ldb_message_element *objectclass_el;
	enum urgent_situation situation;
	bool rodc, rmd_is_provided;

	ldb = ldb_module_get_ctx(module);

	our_invocation_id = samdb_ntds_invocation_id(ldb);
	if (!our_invocation_id) {
		/* this happens during an initial vampire while
		   updating the schema */
		DEBUG(5,("No invocationID - skipping replPropertyMetaData update\n"));
		return LDB_SUCCESS;
	}

	unix_to_nt_time(&now, t);

	if (ldb_request_get_control(req, DSDB_CONTROL_CHANGEREPLMETADATA_OID)) {
		rmd_is_provided = true;
	} else {
		rmd_is_provided = false;
	}

	/* if isDeleted is present and is TRUE, then we consider we are deleting,
	 * otherwise we consider we are updating */
	if (ldb_msg_check_string_attribute(msg, "isDeleted", "TRUE")) {
		situation = REPL_URGENT_ON_DELETE;
	} else {
		situation = REPL_URGENT_ON_UPDATE;
	}

	if (rmd_is_provided) {
		/* In this case the change_replmetadata control was supplied */
		/* We check that it's the only attribute that is provided
		 * (it's a rare case so it's better to keep the code simplier)
		 * We also check that the highest local_usn is bigger than
		 * uSNChanged. */
		uint64_t db_seq;
		if( msg->num_elements != 1 ||
			strncmp(msg->elements[0].name,
				"replPropertyMetaData", 20) ) {
			DEBUG(0,(__location__ ": changereplmetada control called without "\
				"a specified replPropertyMetaData attribute or with others\n"));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (situation == REPL_URGENT_ON_DELETE) {
			DEBUG(0,(__location__ ": changereplmetada control can't be called when deleting an object\n"));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		omd_value = ldb_msg_find_ldb_val(msg, "replPropertyMetaData");
		if (!omd_value) {
			DEBUG(0,(__location__ ": replPropertyMetaData was not specified for Object %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ndr_err = ndr_pull_struct_blob(omd_value, msg, &omd,
					       (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0,(__location__ ": Failed to parse replPropertyMetaData for %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}
		*seq_num = find_max_local_usn(omd);

		ret = dsdb_module_search_dn(module, msg, &res, msg->dn, attrs2,
					    DSDB_FLAG_NEXT_MODULE |
					    DSDB_SEARCH_SHOW_RECYCLED |
					    DSDB_SEARCH_SHOW_EXTENDED_DN |
					    DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT |
					    DSDB_SEARCH_REVEAL_INTERNALS, req);

		if (ret != LDB_SUCCESS || res->count != 1) {
			DEBUG(0,(__location__ ": Object %s failed to find uSNChanged\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		objectclass_el = ldb_msg_find_element(res->msgs[0], "objectClass");
		if (is_urgent && replmd_check_urgent_objectclass(objectclass_el,
								situation)) {
			*is_urgent = true;
		}

		db_seq = ldb_msg_find_attr_as_uint64(res->msgs[0], "uSNChanged", 0);
		if (*seq_num <= db_seq) {
			DEBUG(0,(__location__ ": changereplmetada control provided but max(local_usn)"\
					      " is less or equal to uSNChanged (max = %lld uSNChanged = %lld)\n",
				 (long long)*seq_num, (long long)db_seq));
			return LDB_ERR_OPERATIONS_ERROR;
		}

	} else {
		/* search for the existing replPropertyMetaDataBlob. We need
		 * to use REVEAL and ask for DNs in storage format to support
		 * the check for values being the same in
		 * replmd_update_rpmd_element()
		 */
		ret = dsdb_module_search_dn(module, msg, &res, msg->dn, attrs,
					    DSDB_FLAG_NEXT_MODULE |
					    DSDB_SEARCH_SHOW_RECYCLED |
					    DSDB_SEARCH_SHOW_EXTENDED_DN |
					    DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT |
					    DSDB_SEARCH_REVEAL_INTERNALS, req);
		if (ret != LDB_SUCCESS || res->count != 1) {
			DEBUG(0,(__location__ ": Object %s failed to find replPropertyMetaData\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		objectclass_el = ldb_msg_find_element(res->msgs[0], "objectClass");
		if (is_urgent && replmd_check_urgent_objectclass(objectclass_el,
								situation)) {
			*is_urgent = true;
		}

		omd_value = ldb_msg_find_ldb_val(res->msgs[0], "replPropertyMetaData");
		if (!omd_value) {
			DEBUG(0,(__location__ ": Object %s does not have a replPropertyMetaData attribute\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ndr_err = ndr_pull_struct_blob(omd_value, msg, &omd,
					       (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0,(__location__ ": Failed to parse replPropertyMetaData for %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (omd.version != 1) {
			DEBUG(0,(__location__ ": bad version %u in replPropertyMetaData for %s\n",
				 omd.version, ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		for (i=0; i<msg->num_elements; i++) {
			struct ldb_message_element *old_el;
			old_el = ldb_msg_find_element(res->msgs[0], msg->elements[i].name);
			ret = replmd_update_rpmd_element(ldb, msg, &msg->elements[i], old_el, &omd, schema, seq_num,
							 our_invocation_id, now);
			if (ret != LDB_SUCCESS) {
				return ret;
			}

			if (is_urgent && !*is_urgent && (situation == REPL_URGENT_ON_UPDATE)) {
				*is_urgent = replmd_check_urgent_attribute(&msg->elements[i]);
			}

		}
	}
	/*
	 * replmd_update_rpmd_element has done an update if the
	 * seq_num is set
	 */
	if (*seq_num != 0) {
		struct ldb_val *md_value;
		struct ldb_message_element *el;

		/*if we are RODC and this is a DRSR update then its ok*/
		if (!ldb_request_get_control(req, DSDB_CONTROL_REPLICATED_UPDATE_OID)) {
			ret = samdb_rodc(ldb, &rodc);
			if (ret != LDB_SUCCESS) {
				DEBUG(4, (__location__ ": unable to tell if we are an RODC\n"));
			} else if (rodc) {
				ldb_asprintf_errstring(ldb, "RODC modify is forbidden\n");
				return LDB_ERR_REFERRAL;
			}
		}

		md_value = talloc(msg, struct ldb_val);
		if (md_value == NULL) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = replmd_replPropertyMetaDataCtr1_sort(&omd.ctr.ctr1, schema, msg->dn);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ndr_err = ndr_push_struct_blob(md_value, msg, &omd,
					       (ndr_push_flags_fn_t)ndr_push_replPropertyMetaDataBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0,(__location__ ": Failed to marshall replPropertyMetaData for %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_msg_add_empty(msg, "replPropertyMetaData", LDB_FLAG_MOD_REPLACE, &el);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to add updated replPropertyMetaData %s\n",
				 ldb_dn_get_linearized(msg->dn)));
			return ret;
		}

		el->num_values = 1;
		el->values = md_value;
	}

	return LDB_SUCCESS;
}

struct parsed_dn {
	struct dsdb_dn *dsdb_dn;
	struct GUID *guid;
	struct ldb_val *v;
};

static int parsed_dn_compare(struct parsed_dn *pdn1, struct parsed_dn *pdn2)
{
	return GUID_compare(pdn1->guid, pdn2->guid);
}

static struct parsed_dn *parsed_dn_find(struct parsed_dn *pdn,
					unsigned int count, struct GUID *guid,
					struct ldb_dn *dn)
{
	struct parsed_dn *ret;
	unsigned int i;
	if (dn && GUID_all_zero(guid)) {
		/* when updating a link using DRS, we sometimes get a
		   NULL GUID. We then need to try and match by DN */
		for (i=0; i<count; i++) {
			if (ldb_dn_compare(pdn[i].dsdb_dn->dn, dn) == 0) {
				dsdb_get_extended_dn_guid(pdn[i].dsdb_dn->dn, guid, "GUID");
				return &pdn[i];
			}
		}
		return NULL;
	}
	BINARY_ARRAY_SEARCH(pdn, count, guid, guid, GUID_compare, ret);
	return ret;
}

/*
  get a series of message element values as an array of DNs and GUIDs
  the result is sorted by GUID
 */
static int get_parsed_dns(struct ldb_module *module, TALLOC_CTX *mem_ctx,
			  struct ldb_message_element *el, struct parsed_dn **pdn,
			  const char *ldap_oid, struct ldb_request *parent)
{
	unsigned int i;
	struct ldb_context *ldb = ldb_module_get_ctx(module);

	if (el == NULL) {
		*pdn = NULL;
		return LDB_SUCCESS;
	}

	(*pdn) = talloc_array(mem_ctx, struct parsed_dn, el->num_values);
	if (!*pdn) {
		ldb_module_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; i<el->num_values; i++) {
		struct ldb_val *v = &el->values[i];
		NTSTATUS status;
		struct ldb_dn *dn;
		struct parsed_dn *p;

		p = &(*pdn)[i];

		p->dsdb_dn = dsdb_dn_parse(*pdn, ldb, v, ldap_oid);
		if (p->dsdb_dn == NULL) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}

		dn = p->dsdb_dn->dn;

		p->guid = talloc(*pdn, struct GUID);
		if (p->guid == NULL) {
			ldb_module_oom(module);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		status = dsdb_get_extended_dn_guid(dn, p->guid, "GUID");
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			/* we got a DN without a GUID - go find the GUID */
			int ret = dsdb_module_guid_by_dn(module, dn, p->guid, parent);
			if (ret != LDB_SUCCESS) {
				ldb_asprintf_errstring(ldb, "Unable to find GUID for DN %s\n",
						       ldb_dn_get_linearized(dn));
				return ret;
			}
			ret = dsdb_set_extended_dn_guid(dn, p->guid, "GUID");
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		} else if (!NT_STATUS_IS_OK(status)) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* keep a pointer to the original ldb_val */
		p->v = v;
	}

	TYPESAFE_QSORT(*pdn, el->num_values, parsed_dn_compare);

	return LDB_SUCCESS;
}

/*
  build a new extended DN, including all meta data fields

  RMD_FLAGS           = DSDB_RMD_FLAG_* bits
  RMD_ADDTIME         = originating_add_time
  RMD_INVOCID         = originating_invocation_id
  RMD_CHANGETIME      = originating_change_time
  RMD_ORIGINATING_USN = originating_usn
  RMD_LOCAL_USN       = local_usn
  RMD_VERSION         = version
 */
static int replmd_build_la_val(TALLOC_CTX *mem_ctx, struct ldb_val *v, struct dsdb_dn *dsdb_dn,
			       const struct GUID *invocation_id, uint64_t seq_num,
			       uint64_t local_usn, NTTIME nttime, uint32_t version, bool deleted)
{
	struct ldb_dn *dn = dsdb_dn->dn;
	const char *tstring, *usn_string, *flags_string;
	struct ldb_val tval;
	struct ldb_val iid;
	struct ldb_val usnv, local_usnv;
	struct ldb_val vers, flagsv;
	NTSTATUS status;
	int ret;
	const char *dnstring;
	char *vstring;
	uint32_t rmd_flags = deleted?DSDB_RMD_FLAG_DELETED:0;

	tstring = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)nttime);
	if (!tstring) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	tval = data_blob_string_const(tstring);

	usn_string = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)seq_num);
	if (!usn_string) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	usnv = data_blob_string_const(usn_string);

	usn_string = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)local_usn);
	if (!usn_string) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	local_usnv = data_blob_string_const(usn_string);

	vstring = talloc_asprintf(mem_ctx, "%lu", (unsigned long)version);
	if (!vstring) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	vers = data_blob_string_const(vstring);

	status = GUID_to_ndr_blob(invocation_id, dn, &iid);
	if (!NT_STATUS_IS_OK(status)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	flags_string = talloc_asprintf(mem_ctx, "%u", rmd_flags);
	if (!flags_string) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	flagsv = data_blob_string_const(flags_string);

	ret = ldb_dn_set_extended_component(dn, "RMD_FLAGS", &flagsv);
	if (ret != LDB_SUCCESS) return ret;
	ret = ldb_dn_set_extended_component(dn, "RMD_ADDTIME", &tval);
	if (ret != LDB_SUCCESS) return ret;
	ret = ldb_dn_set_extended_component(dn, "RMD_INVOCID", &iid);
	if (ret != LDB_SUCCESS) return ret;
	ret = ldb_dn_set_extended_component(dn, "RMD_CHANGETIME", &tval);
	if (ret != LDB_SUCCESS) return ret;
	ret = ldb_dn_set_extended_component(dn, "RMD_LOCAL_USN", &local_usnv);
	if (ret != LDB_SUCCESS) return ret;
	ret = ldb_dn_set_extended_component(dn, "RMD_ORIGINATING_USN", &usnv);
	if (ret != LDB_SUCCESS) return ret;
	ret = ldb_dn_set_extended_component(dn, "RMD_VERSION", &vers);
	if (ret != LDB_SUCCESS) return ret;

	dnstring = dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 1);
	if (dnstring == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	*v = data_blob_string_const(dnstring);

	return LDB_SUCCESS;
}

static int replmd_update_la_val(TALLOC_CTX *mem_ctx, struct ldb_val *v, struct dsdb_dn *dsdb_dn,
				struct dsdb_dn *old_dsdb_dn, const struct GUID *invocation_id,
				uint64_t seq_num, uint64_t local_usn, NTTIME nttime,
				uint32_t version, bool deleted);

/*
  check if any links need upgrading from w2k format

  The parent_ctx is the ldb_message_element which contains the values array that dns[i].v points at, and which should be used for allocating any new value.
 */
static int replmd_check_upgrade_links(struct parsed_dn *dns, uint32_t count, struct ldb_message_element *parent_ctx, const struct GUID *invocation_id)
{
	uint32_t i;
	for (i=0; i<count; i++) {
		NTSTATUS status;
		uint32_t version;
		int ret;

		status = dsdb_get_extended_dn_uint32(dns[i].dsdb_dn->dn, &version, "RMD_VERSION");
		if (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			continue;
		}

		/* it's an old one that needs upgrading */
		ret = replmd_update_la_val(parent_ctx->values, dns[i].v, dns[i].dsdb_dn, dns[i].dsdb_dn, invocation_id,
					   1, 1, 0, 0, false);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}
	return LDB_SUCCESS;
}

/*
  update an extended DN, including all meta data fields

  see replmd_build_la_val for value names
 */
static int replmd_update_la_val(TALLOC_CTX *mem_ctx, struct ldb_val *v, struct dsdb_dn *dsdb_dn,
				struct dsdb_dn *old_dsdb_dn, const struct GUID *invocation_id,
				uint64_t seq_num, uint64_t local_usn, NTTIME nttime,
				uint32_t version, bool deleted)
{
	struct ldb_dn *dn = dsdb_dn->dn;
	const char *tstring, *usn_string, *flags_string;
	struct ldb_val tval;
	struct ldb_val iid;
	struct ldb_val usnv, local_usnv;
	struct ldb_val vers, flagsv;
	const struct ldb_val *old_addtime;
	uint32_t old_version;
	NTSTATUS status;
	int ret;
	const char *dnstring;
	char *vstring;
	uint32_t rmd_flags = deleted?DSDB_RMD_FLAG_DELETED:0;

	tstring = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)nttime);
	if (!tstring) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	tval = data_blob_string_const(tstring);

	usn_string = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)seq_num);
	if (!usn_string) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	usnv = data_blob_string_const(usn_string);

	usn_string = talloc_asprintf(mem_ctx, "%llu", (unsigned long long)local_usn);
	if (!usn_string) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	local_usnv = data_blob_string_const(usn_string);

	status = GUID_to_ndr_blob(invocation_id, dn, &iid);
	if (!NT_STATUS_IS_OK(status)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	flags_string = talloc_asprintf(mem_ctx, "%u", rmd_flags);
	if (!flags_string) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	flagsv = data_blob_string_const(flags_string);

	ret = ldb_dn_set_extended_component(dn, "RMD_FLAGS", &flagsv);
	if (ret != LDB_SUCCESS) return ret;

	/* get the ADDTIME from the original */
	old_addtime = ldb_dn_get_extended_component(old_dsdb_dn->dn, "RMD_ADDTIME");
	if (old_addtime == NULL) {
		old_addtime = &tval;
	}
	if (dsdb_dn != old_dsdb_dn) {
		ret = ldb_dn_set_extended_component(dn, "RMD_ADDTIME", old_addtime);
		if (ret != LDB_SUCCESS) return ret;
	}

	/* use our invocation id */
	ret = ldb_dn_set_extended_component(dn, "RMD_INVOCID", &iid);
	if (ret != LDB_SUCCESS) return ret;

	/* changetime is the current time */
	ret = ldb_dn_set_extended_component(dn, "RMD_CHANGETIME", &tval);
	if (ret != LDB_SUCCESS) return ret;

	/* update the USN */
	ret = ldb_dn_set_extended_component(dn, "RMD_ORIGINATING_USN", &usnv);
	if (ret != LDB_SUCCESS) return ret;

	ret = ldb_dn_set_extended_component(dn, "RMD_LOCAL_USN", &local_usnv);
	if (ret != LDB_SUCCESS) return ret;

	/* increase the version by 1 */
	status = dsdb_get_extended_dn_uint32(old_dsdb_dn->dn, &old_version, "RMD_VERSION");
	if (NT_STATUS_IS_OK(status) && old_version >= version) {
		version = old_version+1;
	}
	vstring = talloc_asprintf(dn, "%lu", (unsigned long)version);
	vers = data_blob_string_const(vstring);
	ret = ldb_dn_set_extended_component(dn, "RMD_VERSION", &vers);
	if (ret != LDB_SUCCESS) return ret;

	dnstring = dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 1);
	if (dnstring == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	*v = data_blob_string_const(dnstring);

	return LDB_SUCCESS;
}

/*
  handle adding a linked attribute
 */
static int replmd_modify_la_add(struct ldb_module *module,
				const struct dsdb_schema *schema,
				struct ldb_message *msg,
				struct ldb_message_element *el,
				struct ldb_message_element *old_el,
				const struct dsdb_attribute *schema_attr,
				uint64_t seq_num,
				time_t t,
				struct GUID *msg_guid,
				struct ldb_request *parent)
{
	unsigned int i;
	struct parsed_dn *dns, *old_dns;
	TALLOC_CTX *tmp_ctx = talloc_new(msg);
	int ret;
	struct ldb_val *new_values = NULL;
	unsigned int num_new_values = 0;
	unsigned old_num_values = old_el?old_el->num_values:0;
	const struct GUID *invocation_id;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	NTTIME now;

	unix_to_nt_time(&now, t);

	ret = get_parsed_dns(module, tmp_ctx, el, &dns, schema_attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = get_parsed_dns(module, tmp_ctx, old_el, &old_dns, schema_attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	invocation_id = samdb_ntds_invocation_id(ldb);
	if (!invocation_id) {
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = replmd_check_upgrade_links(old_dns, old_num_values, old_el, invocation_id);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	/* for each new value, see if it exists already with the same GUID */
	for (i=0; i<el->num_values; i++) {
		struct parsed_dn *p = parsed_dn_find(old_dns, old_num_values, dns[i].guid, NULL);
		if (p == NULL) {
			/* this is a new linked attribute value */
			new_values = talloc_realloc(tmp_ctx, new_values, struct ldb_val, num_new_values+1);
			if (new_values == NULL) {
				ldb_module_oom(module);
				talloc_free(tmp_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ret = replmd_build_la_val(new_values, &new_values[num_new_values], dns[i].dsdb_dn,
						  invocation_id, seq_num, seq_num, now, 0, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
			num_new_values++;
		} else {
			/* this is only allowed if the GUID was
			   previously deleted. */
			uint32_t rmd_flags = dsdb_dn_rmd_flags(p->dsdb_dn->dn);

			if (!(rmd_flags & DSDB_RMD_FLAG_DELETED)) {
				ldb_asprintf_errstring(ldb, "Attribute %s already exists for target GUID %s",
						       el->name, GUID_string(tmp_ctx, p->guid));
				talloc_free(tmp_ctx);
				return LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
			}
			ret = replmd_update_la_val(old_el->values, p->v, dns[i].dsdb_dn, p->dsdb_dn,
						   invocation_id, seq_num, seq_num, now, 0, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
		}

		ret = replmd_add_backlink(module, schema, msg_guid, dns[i].guid, true, schema_attr, true);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	/* add the new ones on to the end of the old values, constructing a new el->values */
	el->values = talloc_realloc(msg->elements, old_el?old_el->values:NULL,
				    struct ldb_val,
				    old_num_values+num_new_values);
	if (el->values == NULL) {
		ldb_module_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	memcpy(&el->values[old_num_values], new_values, num_new_values*sizeof(struct ldb_val));
	el->num_values = old_num_values + num_new_values;

	talloc_steal(msg->elements, el->values);
	talloc_steal(el->values, new_values);

	talloc_free(tmp_ctx);

	/* we now tell the backend to replace all existing values
	   with the one we have constructed */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}


/*
  handle deleting all active linked attributes
 */
static int replmd_modify_la_delete(struct ldb_module *module,
				   const struct dsdb_schema *schema,
				   struct ldb_message *msg,
				   struct ldb_message_element *el,
				   struct ldb_message_element *old_el,
				   const struct dsdb_attribute *schema_attr,
				   uint64_t seq_num,
				   time_t t,
				   struct GUID *msg_guid,
				   struct ldb_request *parent)
{
	unsigned int i;
	struct parsed_dn *dns, *old_dns;
	TALLOC_CTX *tmp_ctx = talloc_new(msg);
	int ret;
	const struct GUID *invocation_id;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	NTTIME now;

	unix_to_nt_time(&now, t);

	/* check if there is nothing to delete */
	if ((!old_el || old_el->num_values == 0) &&
	    el->num_values == 0) {
		return LDB_SUCCESS;
	}

	if (!old_el || old_el->num_values == 0) {
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}

	ret = get_parsed_dns(module, tmp_ctx, el, &dns, schema_attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = get_parsed_dns(module, tmp_ctx, old_el, &old_dns, schema_attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	invocation_id = samdb_ntds_invocation_id(ldb);
	if (!invocation_id) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = replmd_check_upgrade_links(old_dns, old_el->num_values, old_el, invocation_id);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	el->values = NULL;

	/* see if we are being asked to delete any links that
	   don't exist or are already deleted */
	for (i=0; i<el->num_values; i++) {
		struct parsed_dn *p = &dns[i];
		struct parsed_dn *p2;
		uint32_t rmd_flags;

		p2 = parsed_dn_find(old_dns, old_el->num_values, p->guid, NULL);
		if (!p2) {
			ldb_asprintf_errstring(ldb, "Attribute %s doesn't exist for target GUID %s",
					       el->name, GUID_string(tmp_ctx, p->guid));
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}
		rmd_flags = dsdb_dn_rmd_flags(p2->dsdb_dn->dn);
		if (rmd_flags & DSDB_RMD_FLAG_DELETED) {
			ldb_asprintf_errstring(ldb, "Attribute %s already deleted for target GUID %s",
					       el->name, GUID_string(tmp_ctx, p->guid));
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}
	}

	/* for each new value, see if it exists already with the same GUID
	   if it is not already deleted and matches the delete list then delete it
	*/
	for (i=0; i<old_el->num_values; i++) {
		struct parsed_dn *p = &old_dns[i];
		uint32_t rmd_flags;

		if (el->num_values && parsed_dn_find(dns, el->num_values, p->guid, NULL) == NULL) {
			continue;
		}

		rmd_flags = dsdb_dn_rmd_flags(p->dsdb_dn->dn);
		if (rmd_flags & DSDB_RMD_FLAG_DELETED) continue;

		ret = replmd_update_la_val(old_el->values, p->v, p->dsdb_dn, p->dsdb_dn,
					   invocation_id, seq_num, seq_num, now, 0, true);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		ret = replmd_add_backlink(module, schema, msg_guid, old_dns[i].guid, false, schema_attr, true);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	el->values = talloc_steal(msg->elements, old_el->values);
	el->num_values = old_el->num_values;

	talloc_free(tmp_ctx);

	/* we now tell the backend to replace all existing values
	   with the one we have constructed */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}

/*
  handle replacing a linked attribute
 */
static int replmd_modify_la_replace(struct ldb_module *module,
				    const struct dsdb_schema *schema,
				    struct ldb_message *msg,
				    struct ldb_message_element *el,
				    struct ldb_message_element *old_el,
				    const struct dsdb_attribute *schema_attr,
				    uint64_t seq_num,
				    time_t t,
				    struct GUID *msg_guid,
				    struct ldb_request *parent)
{
	unsigned int i;
	struct parsed_dn *dns, *old_dns;
	TALLOC_CTX *tmp_ctx = talloc_new(msg);
	int ret;
	const struct GUID *invocation_id;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_val *new_values = NULL;
	unsigned int num_new_values = 0;
	unsigned int old_num_values = old_el?old_el->num_values:0;
	NTTIME now;

	unix_to_nt_time(&now, t);

	/* check if there is nothing to replace */
	if ((!old_el || old_el->num_values == 0) &&
	    el->num_values == 0) {
		return LDB_SUCCESS;
	}

	ret = get_parsed_dns(module, tmp_ctx, el, &dns, schema_attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = get_parsed_dns(module, tmp_ctx, old_el, &old_dns, schema_attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	invocation_id = samdb_ntds_invocation_id(ldb);
	if (!invocation_id) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = replmd_check_upgrade_links(old_dns, old_num_values, old_el, invocation_id);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	/* mark all the old ones as deleted */
	for (i=0; i<old_num_values; i++) {
		struct parsed_dn *old_p = &old_dns[i];
		struct parsed_dn *p;
		uint32_t rmd_flags = dsdb_dn_rmd_flags(old_p->dsdb_dn->dn);

		if (rmd_flags & DSDB_RMD_FLAG_DELETED) continue;

		ret = replmd_add_backlink(module, schema, msg_guid, old_dns[i].guid, false, schema_attr, false);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		p = parsed_dn_find(dns, el->num_values, old_p->guid, NULL);
		if (p) {
			/* we don't delete it if we are re-adding it */
			continue;
		}

		ret = replmd_update_la_val(old_el->values, old_p->v, old_p->dsdb_dn, old_p->dsdb_dn,
					   invocation_id, seq_num, seq_num, now, 0, true);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	/* for each new value, either update its meta-data, or add it
	 * to old_el
	*/
	for (i=0; i<el->num_values; i++) {
		struct parsed_dn *p = &dns[i], *old_p;

		if (old_dns &&
		    (old_p = parsed_dn_find(old_dns,
					    old_num_values, p->guid, NULL)) != NULL) {
			/* update in place */
			ret = replmd_update_la_val(old_el->values, old_p->v, old_p->dsdb_dn,
						   old_p->dsdb_dn, invocation_id,
						   seq_num, seq_num, now, 0, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
		} else {
			/* add a new one */
			new_values = talloc_realloc(tmp_ctx, new_values, struct ldb_val,
						    num_new_values+1);
			if (new_values == NULL) {
				ldb_module_oom(module);
				talloc_free(tmp_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ret = replmd_build_la_val(new_values, &new_values[num_new_values], dns[i].dsdb_dn,
						  invocation_id, seq_num, seq_num, now, 0, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
			num_new_values++;
		}

		ret = replmd_add_backlink(module, schema, msg_guid, dns[i].guid, true, schema_attr, false);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	/* add the new values to the end of old_el */
	if (num_new_values != 0) {
		el->values = talloc_realloc(msg->elements, old_el?old_el->values:NULL,
					    struct ldb_val, old_num_values+num_new_values);
		if (el->values == NULL) {
			ldb_module_oom(module);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		memcpy(&el->values[old_num_values], &new_values[0],
		       sizeof(struct ldb_val)*num_new_values);
		el->num_values = old_num_values + num_new_values;
		talloc_steal(msg->elements, new_values);
	} else {
		el->values = old_el->values;
		el->num_values = old_el->num_values;
		talloc_steal(msg->elements, el->values);
	}

	talloc_free(tmp_ctx);

	/* we now tell the backend to replace all existing values
	   with the one we have constructed */
	el->flags = LDB_FLAG_MOD_REPLACE;

	return LDB_SUCCESS;
}


/*
  handle linked attributes in modify requests
 */
static int replmd_modify_handle_linked_attribs(struct ldb_module *module,
					       struct ldb_message *msg,
					       uint64_t seq_num, time_t t,
					       struct ldb_request *parent)
{
	struct ldb_result *res;
	unsigned int i;
	int ret;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_message *old_msg;

	const struct dsdb_schema *schema;
	struct GUID old_guid;

	if (seq_num == 0) {
		/* there the replmd_update_rpmd code has already
		 * checked and saw that there are no linked
		 * attributes */
		return LDB_SUCCESS;
	}

	if (dsdb_functional_level(ldb) == DS_DOMAIN_FUNCTION_2000) {
		/* don't do anything special for linked attributes */
		return LDB_SUCCESS;
	}

	ret = dsdb_module_search_dn(module, msg, &res, msg->dn, NULL,
	                            DSDB_FLAG_NEXT_MODULE |
	                            DSDB_SEARCH_SHOW_RECYCLED |
				    DSDB_SEARCH_REVEAL_INTERNALS |
				    DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT,
				    parent);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	schema = dsdb_get_schema(ldb, res);
	if (!schema) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	old_msg = res->msgs[0];

	old_guid = samdb_result_guid(old_msg, "objectGUID");

	for (i=0; i<msg->num_elements; i++) {
		struct ldb_message_element *el = &msg->elements[i];
		struct ldb_message_element *old_el, *new_el;
		const struct dsdb_attribute *schema_attr
			= dsdb_attribute_by_lDAPDisplayName(schema, el->name);
		if (!schema_attr) {
			ldb_asprintf_errstring(ldb,
					       "%s: attribute %s is not a valid attribute in schema",
					       __FUNCTION__, el->name);
			return LDB_ERR_OBJECT_CLASS_VIOLATION;
		}
		if (schema_attr->linkID == 0) {
			continue;
		}
		if ((schema_attr->linkID & 1) == 1) {
			/* Odd is for the target.  Illegal to modify */
			ldb_asprintf_errstring(ldb,
					       "attribute %s must not be modified directly, it is a linked attribute", el->name);
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
		old_el = ldb_msg_find_element(old_msg, el->name);
		switch (el->flags & LDB_FLAG_MOD_MASK) {
		case LDB_FLAG_MOD_REPLACE:
			ret = replmd_modify_la_replace(module, schema, msg, el, old_el, schema_attr, seq_num, t, &old_guid, parent);
			break;
		case LDB_FLAG_MOD_DELETE:
			ret = replmd_modify_la_delete(module, schema, msg, el, old_el, schema_attr, seq_num, t, &old_guid, parent);
			break;
		case LDB_FLAG_MOD_ADD:
			ret = replmd_modify_la_add(module, schema, msg, el, old_el, schema_attr, seq_num, t, &old_guid, parent);
			break;
		default:
			ldb_asprintf_errstring(ldb,
					       "invalid flags 0x%x for %s linked attribute",
					       el->flags, el->name);
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		if (old_el) {
			ldb_msg_remove_attr(old_msg, el->name);
		}
		ldb_msg_add_empty(old_msg, el->name, 0, &new_el);
		new_el->num_values = el->num_values;
		new_el->values = talloc_steal(msg->elements, el->values);

		/* TODO: this relises a bit too heavily on the exact
		   behaviour of ldb_msg_find_element and
		   ldb_msg_remove_element */
		old_el = ldb_msg_find_element(msg, el->name);
		if (old_el != el) {
			ldb_msg_remove_element(msg, old_el);
			i--;
		}
	}

	talloc_free(res);
	return ret;
}



static int replmd_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	time_t t = time(NULL);
	int ret;
	bool is_urgent = false;
	struct loadparm_context *lp_ctx;
	char *referral;
	unsigned int functional_level;
	const DATA_BLOB *guid_blob;

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_modify\n");

	guid_blob = ldb_msg_find_ldb_val(req->op.mod.message, "objectGUID");
	if ( guid_blob != NULL ) {
		ldb_set_errstring(ldb,
				  "replmd_modify: it's not allowed to change the objectGUID!");
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac = replmd_ctx_init(module, req);
	if (ac == NULL) {
		return ldb_module_oom(module);
	}

	functional_level = dsdb_functional_level(ldb);

	lp_ctx = talloc_get_type(ldb_get_opaque(ldb, "loadparm"),
				 struct loadparm_context);

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		ldb_oom(ldb);
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb_msg_remove_attr(msg, "whenChanged");
	ldb_msg_remove_attr(msg, "uSNChanged");

	ret = replmd_update_rpmd(module, ac->schema, req, msg, &ac->seq_num, t, &is_urgent);
	if (ret == LDB_ERR_REFERRAL) {
		referral = talloc_asprintf(req,
					   "ldap://%s/%s",
					   lpcfg_dnsdomain(lp_ctx),
					   ldb_dn_get_linearized(msg->dn));
		ret = ldb_module_send_referral(req, referral);
		talloc_free(ac);
		return ldb_module_done(req, NULL, NULL, ret);
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	ret = replmd_modify_handle_linked_attribs(module, msg, ac->seq_num, t, req);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* TODO:
	 * - replace the old object with the newly constructed one
	 */

	ac->is_urgent = is_urgent;

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, replmd_op_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* current partition control is needed by "replmd_op_callback" */
	if (ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID) == NULL) {
		ret = ldb_request_add_control(down_req,
					      DSDB_CONTROL_CURRENT_PARTITION_OID,
					      false, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}
	}

	/* If we are in functional level 2000, then
	 * replmd_modify_handle_linked_attribs will have done
	 * nothing */
	if (functional_level == DS_DOMAIN_FUNCTION_2000) {
		ret = ldb_request_add_control(down_req, DSDB_CONTROL_APPLY_LINKS, false, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}
	}

	talloc_steal(down_req, msg);

	/* we only change whenChanged and uSNChanged if the seq_num
	   has changed */
	if (ac->seq_num != 0) {
		ret = add_time_element(msg, "whenChanged", t);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}

		ret = add_uint64_element(ldb, msg, "uSNChanged", ac->seq_num);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

static int replmd_rename_callback(struct ldb_request *req, struct ldb_reply *ares);

/*
  handle a rename request

  On a rename we need to do an extra ldb_modify which sets the
  whenChanged and uSNChanged attributes.  We do this in a callback after the success.
 */
static int replmd_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;
	int ret;
	struct ldb_request *down_req;

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_rename\n");

	ac = replmd_ctx_init(module, req);
	if (ac == NULL) {
		return ldb_module_oom(module);
	}

	ret = ldb_build_rename_req(&down_req, ldb, ac,
				   ac->req->op.rename.olddn,
				   ac->req->op.rename.newdn,
				   ac->req->controls,
				   ac, replmd_rename_callback,
				   ac->req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}

/* After the rename is compleated, update the whenchanged etc */
static int replmd_rename_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	time_t t = time(NULL);
	int ret;

	ac = talloc_get_type(req->context, struct replmd_replicated_request);
	ldb = ldb_module_get_ctx(ac->module);

	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb,
				  "invalid ldb_reply_type in callback");
		talloc_free(ares);
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &ac->seq_num);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* TODO:
	 * - replace the old object with the newly constructed one
	 */

	msg = ldb_msg_new(ac);
	if (msg == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->dn = ac->req->op.rename.newdn;

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, replmd_op_callback,
				req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* current partition control is needed by "replmd_op_callback" */
	if (ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID) == NULL) {
		ret = ldb_request_add_control(down_req,
					      DSDB_CONTROL_CURRENT_PARTITION_OID,
					      false, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(ac);
			return ret;
		}
	}

	talloc_steal(down_req, msg);

	ret = add_time_element(msg, "whenChanged", t);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	ret = add_uint64_element(ldb, msg, "uSNChanged", ac->seq_num);
	if (ret != LDB_SUCCESS) {
		talloc_free(ac);
		return ret;
	}

	/* go on with the call chain - do the modify after the rename */
	return ldb_next_request(ac->module, down_req);
}

/*
   remove links from objects that point at this object when an object
   is deleted
 */
static int replmd_delete_remove_link(struct ldb_module *module,
				     const struct dsdb_schema *schema,
				     struct ldb_dn *dn,
				     struct ldb_message_element *el,
				     const struct dsdb_attribute *sa,
				     struct ldb_request *parent)
{
	unsigned int i;
	TALLOC_CTX *tmp_ctx = talloc_new(module);
	struct ldb_context *ldb = ldb_module_get_ctx(module);

	for (i=0; i<el->num_values; i++) {
		struct dsdb_dn *dsdb_dn;
		NTSTATUS status;
		int ret;
		struct GUID guid2;
		struct ldb_message *msg;
		const struct dsdb_attribute *target_attr;
		struct ldb_message_element *el2;
		struct ldb_val dn_val;

		if (dsdb_dn_is_deleted_val(&el->values[i])) {
			continue;
		}

		dsdb_dn = dsdb_dn_parse(tmp_ctx, ldb, &el->values[i], sa->syntax->ldap_oid);
		if (!dsdb_dn) {
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		status = dsdb_get_extended_dn_guid(dsdb_dn->dn, &guid2, "GUID");
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* remove the link */
		msg = ldb_msg_new(tmp_ctx);
		if (!msg) {
			ldb_module_oom(module);
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}


		msg->dn = dsdb_dn->dn;

		target_attr = dsdb_attribute_by_linkID(schema, sa->linkID ^ 1);
		if (target_attr == NULL) {
			continue;
		}

		ret = ldb_msg_add_empty(msg, target_attr->lDAPDisplayName, LDB_FLAG_MOD_DELETE, &el2);
		if (ret != LDB_SUCCESS) {
			ldb_module_oom(module);
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		dn_val = data_blob_string_const(ldb_dn_get_linearized(dn));
		el2->values = &dn_val;
		el2->num_values = 1;

		ret = dsdb_module_modify(module, msg, DSDB_FLAG_OWN_MODULE, parent);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
	}
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}


/*
  handle update of replication meta data for deletion of objects

  This also handles the mapping of delete to a rename operation
  to allow deletes to be replicated.
 */
static int replmd_delete(struct ldb_module *module, struct ldb_request *req)
{
	int ret = LDB_ERR_OTHER;
	bool retb, disallow_move_on_delete;
	struct ldb_dn *old_dn, *new_dn;
	const char *rdn_name;
	const struct ldb_val *rdn_value, *new_rdn_value;
	struct GUID guid;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	const struct dsdb_schema *schema;
	struct ldb_message *msg, *old_msg;
	struct ldb_message_element *el;
	TALLOC_CTX *tmp_ctx;
	struct ldb_result *res, *parent_res;
	const char *preserved_attrs[] = {
		/* yes, this really is a hard coded list. See MS-ADTS
		   section 3.1.1.5.5.1.1 */
		"nTSecurityDescriptor", "attributeID", "attributeSyntax", "dNReferenceUpdate", "dNSHostName",
		"flatName", "governsID", "groupType", "instanceType", "lDAPDisplayName", "legacyExchangeDN",
		"isDeleted", "isRecycled", "lastKnownParent", "msDS-LastKnownRDN", "mS-DS-CreatorSID",
		"mSMQOwnerID", "nCName", "objectClass", "distinguishedName", "objectGUID", "objectSid",
		"oMSyntax", "proxiedObjectName", "name", "replPropertyMetaData", "sAMAccountName",
		"securityIdentifier", "sIDHistory", "subClassOf", "systemFlags", "trustPartner", "trustDirection",
		"trustType", "trustAttributes", "userAccountControl", "uSNChanged", "uSNCreated", "whenCreated",
		"whenChanged", NULL};
	unsigned int i, el_count = 0;
	enum deletion_state { OBJECT_NOT_DELETED=1, OBJECT_DELETED=2, OBJECT_RECYCLED=3,
						OBJECT_TOMBSTONE=4, OBJECT_REMOVED=5 };
	enum deletion_state deletion_state, next_deletion_state;
	bool enabled;

	if (ldb_dn_is_special(req->op.del.dn)) {
		return ldb_next_request(module, req);
	}

	tmp_ctx = talloc_new(ldb);
	if (!tmp_ctx) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	schema = dsdb_get_schema(ldb, tmp_ctx);
	if (!schema) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	old_dn = ldb_dn_copy(tmp_ctx, req->op.del.dn);

	/* we need the complete msg off disk, so we can work out which
	   attributes need to be removed */
	ret = dsdb_module_search_dn(module, tmp_ctx, &res, old_dn, NULL,
	                            DSDB_FLAG_NEXT_MODULE |
	                            DSDB_SEARCH_SHOW_RECYCLED |
				    DSDB_SEARCH_REVEAL_INTERNALS |
				    DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT, req);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	old_msg = res->msgs[0];


	ret = dsdb_recyclebin_enabled(module, &enabled);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	if (ldb_msg_check_string_attribute(old_msg, "isDeleted", "TRUE")) {
		if (!enabled) {
			deletion_state = OBJECT_TOMBSTONE;
			next_deletion_state = OBJECT_REMOVED;
		} else if (ldb_msg_check_string_attribute(old_msg, "isRecycled", "TRUE")) {
			deletion_state = OBJECT_RECYCLED;
			next_deletion_state = OBJECT_REMOVED;
		} else {
			deletion_state = OBJECT_DELETED;
			next_deletion_state = OBJECT_RECYCLED;
		}
	} else {
		deletion_state = OBJECT_NOT_DELETED;
		if (enabled) {
			next_deletion_state = OBJECT_DELETED;
		} else {
			next_deletion_state = OBJECT_TOMBSTONE;
		}
	}

	if (next_deletion_state == OBJECT_REMOVED) {
		struct auth_session_info *session_info =
				(struct auth_session_info *)ldb_get_opaque(ldb, "sessionInfo");
		if (security_session_user_level(session_info, NULL) != SECURITY_SYSTEM) {
			ldb_asprintf_errstring(ldb, "Refusing to delete deleted object %s",
					ldb_dn_get_linearized(old_msg->dn));
			return LDB_ERR_UNWILLING_TO_PERFORM;
		}

		/* it is already deleted - really remove it this time */
		talloc_free(tmp_ctx);
		return ldb_next_request(module, req);
	}

	rdn_name = ldb_dn_get_rdn_name(old_dn);
	rdn_value = ldb_dn_get_rdn_val(old_dn);
	if ((rdn_name == NULL) || (rdn_value == NULL)) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		ldb_module_oom(module);
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->dn = old_dn;

	if (deletion_state == OBJECT_NOT_DELETED){
		/* consider the SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE flag */
		disallow_move_on_delete =
			(ldb_msg_find_attr_as_int(old_msg, "systemFlags", 0)
				& SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE);

		/* work out where we will be renaming this object to */
		if (!disallow_move_on_delete) {
			ret = dsdb_get_deleted_objects_dn(ldb, tmp_ctx, old_dn,
							  &new_dn);
			if (ret != LDB_SUCCESS) {
				/* this is probably an attempted delete on a partition
				 * that doesn't allow delete operations, such as the
				 * schema partition */
				ldb_asprintf_errstring(ldb, "No Deleted Objects container for DN %s",
							   ldb_dn_get_linearized(old_dn));
				talloc_free(tmp_ctx);
				return LDB_ERR_UNWILLING_TO_PERFORM;
			}
		} else {
			new_dn = ldb_dn_get_parent(tmp_ctx, old_dn);
			if (new_dn == NULL) {
				ldb_module_oom(module);
				talloc_free(tmp_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
		}

		/* get the objects GUID from the search we just did */
		guid = samdb_result_guid(old_msg, "objectGUID");

		/* Add a formatted child */
		retb = ldb_dn_add_child_fmt(new_dn, "%s=%s\\0ADEL:%s",
						rdn_name,
						ldb_dn_escape_value(tmp_ctx, *rdn_value),
						GUID_string(tmp_ctx, &guid));
		if (!retb) {
			DEBUG(0,(__location__ ": Unable to add a formatted child to dn: %s",
					ldb_dn_get_linearized(new_dn)));
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_msg_add_string(msg, "isDeleted", "TRUE");
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to add isDeleted string to the msg\n"));
			ldb_module_oom(module);
			talloc_free(tmp_ctx);
			return ret;
		}
		msg->elements[el_count++].flags = LDB_FLAG_MOD_REPLACE;
	}

	/*
	  now we need to modify the object in the following ways:

	  - add isDeleted=TRUE
	  - update rDN and name, with new rDN
	  - remove linked attributes
	  - remove objectCategory and sAMAccountType
	  - remove attribs not on the preserved list
	     - preserved if in above list, or is rDN
	  - remove all linked attribs from this object
	  - remove all links from other objects to this object
	  - add lastKnownParent
	  - update replPropertyMetaData?

	  see MS-ADTS "Tombstone Requirements" section 3.1.1.5.5.1.1
	 */

	/* we need the storage form of the parent GUID */
	ret = dsdb_module_search_dn(module, tmp_ctx, &parent_res,
				    ldb_dn_get_parent(tmp_ctx, old_dn), NULL,
				    DSDB_FLAG_NEXT_MODULE |
				    DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT |
				    DSDB_SEARCH_REVEAL_INTERNALS|
				    DSDB_SEARCH_SHOW_RECYCLED, req);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	if (deletion_state == OBJECT_NOT_DELETED){
		ret = ldb_msg_add_steal_string(msg, "lastKnownParent",
						   ldb_dn_get_extended_linearized(tmp_ctx, parent_res->msgs[0]->dn, 1));
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to add lastKnownParent string to the msg\n"));
			ldb_module_oom(module);
			talloc_free(tmp_ctx);
			return ret;
		}
		msg->elements[el_count++].flags = LDB_FLAG_MOD_ADD;
	}

	switch (next_deletion_state){

	case OBJECT_DELETED:

		ret = ldb_msg_add_value(msg, "msDS-LastKnownRDN", rdn_value, NULL);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to add msDS-LastKnownRDN string to the msg\n"));
			ldb_module_oom(module);
			talloc_free(tmp_ctx);
			return ret;
		}
		msg->elements[el_count++].flags = LDB_FLAG_MOD_ADD;

		ret = ldb_msg_add_empty(msg, "objectCategory", LDB_FLAG_MOD_DELETE, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			ldb_module_oom(module);
			return ret;
		}

		ret = ldb_msg_add_empty(msg, "sAMAccountType", LDB_FLAG_MOD_DELETE, NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			ldb_module_oom(module);
			return ret;
		}

		break;

	case OBJECT_RECYCLED:
	case OBJECT_TOMBSTONE:

		/* we also mark it as recycled, meaning this object can't be
		   recovered (we are stripping its attributes) */
		if (dsdb_functional_level(ldb) >= DS_DOMAIN_FUNCTION_2008_R2) {
			ret = ldb_msg_add_string(msg, "isRecycled", "TRUE");
			if (ret != LDB_SUCCESS) {
				DEBUG(0,(__location__ ": Failed to add isRecycled string to the msg\n"));
				ldb_module_oom(module);
				talloc_free(tmp_ctx);
				return ret;
			}
			msg->elements[el_count++].flags = LDB_FLAG_MOD_ADD;
		}

		/* work out which of the old attributes we will be removing */
		for (i=0; i<old_msg->num_elements; i++) {
			const struct dsdb_attribute *sa;
			el = &old_msg->elements[i];
			sa = dsdb_attribute_by_lDAPDisplayName(schema, el->name);
			if (!sa) {
				talloc_free(tmp_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			if (ldb_attr_cmp(el->name, rdn_name) == 0) {
				/* don't remove the rDN */
				continue;
			}
			if (sa->linkID && sa->linkID & 1) {
				ret = replmd_delete_remove_link(module, schema, old_dn, el, sa, req);
				if (ret != LDB_SUCCESS) {
					talloc_free(tmp_ctx);
					return LDB_ERR_OPERATIONS_ERROR;
				}
				continue;
			}
			if (!sa->linkID && ldb_attr_in_list(preserved_attrs, el->name)) {
				continue;
			}
			ret = ldb_msg_add_empty(msg, el->name, LDB_FLAG_MOD_DELETE, &el);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				ldb_module_oom(module);
				return ret;
			}
		}
		break;

	default:
		break;
	}

	if (deletion_state == OBJECT_NOT_DELETED) {
		const struct dsdb_attribute *sa;

		/* work out what the new rdn value is, for updating the
		   rDN and name fields */
		new_rdn_value = ldb_dn_get_rdn_val(new_dn);
		if (new_rdn_value == NULL) {
			talloc_free(tmp_ctx);
			return ldb_operr(ldb);
		}

		sa = dsdb_attribute_by_lDAPDisplayName(schema, rdn_name);
		if (!sa) {
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_msg_add_value(msg, sa->lDAPDisplayName, new_rdn_value,
					&el);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}
		el->flags = LDB_FLAG_MOD_REPLACE;

		el = ldb_msg_find_element(old_msg, "name");
		if (el) {
			ret = ldb_msg_add_value(msg, "name", new_rdn_value, &el);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
			el->flags = LDB_FLAG_MOD_REPLACE;
		}
	}

	ret = dsdb_module_modify(module, msg, DSDB_FLAG_OWN_MODULE, req);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, "replmd_delete: Failed to modify object %s in delete - %s",
				       ldb_dn_get_linearized(old_dn), ldb_errstring(ldb));
		talloc_free(tmp_ctx);
		return ret;
	}

	if (deletion_state == OBJECT_NOT_DELETED) {
		/* now rename onto the new DN */
		ret = dsdb_module_rename(module, old_dn, new_dn, DSDB_FLAG_NEXT_MODULE, req);
		if (ret != LDB_SUCCESS){
			DEBUG(0,(__location__ ": Failed to rename object from '%s' to '%s' - %s\n",
				 ldb_dn_get_linearized(old_dn),
				 ldb_dn_get_linearized(new_dn),
				 ldb_errstring(ldb)));
			talloc_free(tmp_ctx);
			return ret;
		}
	}

	talloc_free(tmp_ctx);

	return ldb_module_done(req, NULL, NULL, LDB_SUCCESS);
}



static int replmd_replicated_request_error(struct replmd_replicated_request *ar, int ret)
{
	return ret;
}

static int replmd_replicated_request_werror(struct replmd_replicated_request *ar, WERROR status)
{
	int ret = LDB_ERR_OTHER;
	/* TODO: do some error mapping */
	return ret;
}

static int replmd_replicated_apply_add(struct replmd_replicated_request *ar)
{
	struct ldb_context *ldb;
	struct ldb_request *change_req;
	enum ndr_err_code ndr_err;
	struct ldb_message *msg;
	struct replPropertyMetaDataBlob *md;
	struct ldb_val md_value;
	unsigned int i;
	int ret;

	/*
	 * TODO: check if the parent object exist
	 */

	/*
	 * TODO: handle the conflict case where an object with the
	 *       same name exist
	 */

	ldb = ldb_module_get_ctx(ar->module);
	msg = ar->objs->objects[ar->index_current].msg;
	md = ar->objs->objects[ar->index_current].meta_data;

	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &ar->seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = ldb_msg_add_value(msg, "objectGUID", &ar->objs->objects[ar->index_current].guid_value, NULL);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = ldb_msg_add_string(msg, "whenChanged", ar->objs->objects[ar->index_current].when_changed);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNCreated", ar->seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNChanged", ar->seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	/* remove any message elements that have zero values */
	for (i=0; i<msg->num_elements; i++) {
		struct ldb_message_element *el = &msg->elements[i];

		if (el->num_values == 0) {
			DEBUG(4,(__location__ ": Removing attribute %s with num_values==0\n",
				 el->name));
			memmove(el, el+1, sizeof(*el)*(msg->num_elements - (i+1)));
			msg->num_elements--;
			i--;
			continue;
		}
	}

	/*
	 * the meta data array is already sorted by the caller
	 */
	for (i=0; i < md->ctr.ctr1.count; i++) {
		md->ctr.ctr1.array[i].local_usn = ar->seq_num;
	}
	ndr_err = ndr_push_struct_blob(&md_value, msg, md,
				       (ndr_push_flags_fn_t)ndr_push_replPropertyMetaDataBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
	}
	ret = ldb_msg_add_value(msg, "replPropertyMetaData", &md_value, NULL);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	replmd_ldb_message_sort(msg, ar->schema);

	if (DEBUGLVL(4)) {
		char *s = ldb_ldif_message_string(ldb, ar, LDB_CHANGETYPE_ADD, msg);
		DEBUG(4, ("DRS replication add message:\n%s\n", s));
		talloc_free(s);
	}

	ret = ldb_build_add_req(&change_req,
				ldb,
				ar,
				msg,
				ar->controls,
				ar,
				replmd_op_callback,
				ar->req);
	LDB_REQ_SET_LOCATION(change_req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	/* current partition control needed by "repmd_op_callback" */
	ret = ldb_request_add_control(change_req,
				      DSDB_CONTROL_CURRENT_PARTITION_OID,
				      false, NULL);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	return ldb_next_request(ar->module, change_req);
}

/*
   return true if an update is newer than an existing entry
   see section 5.11 of MS-ADTS
*/
static bool replmd_update_is_newer(const struct GUID *current_invocation_id,
				   const struct GUID *update_invocation_id,
				   uint32_t current_version,
				   uint32_t update_version,
				   NTTIME current_change_time,
				   NTTIME update_change_time)
{
	if (update_version != current_version) {
		return update_version > current_version;
	}
	if (update_change_time != current_change_time) {
		return update_change_time > current_change_time;
	}
	return GUID_compare(update_invocation_id, current_invocation_id) > 0;
}

static bool replmd_replPropertyMetaData1_is_newer(struct replPropertyMetaData1 *cur_m,
						  struct replPropertyMetaData1 *new_m)
{
	return replmd_update_is_newer(&cur_m->originating_invocation_id,
				      &new_m->originating_invocation_id,
				      cur_m->version,
				      new_m->version,
				      cur_m->originating_change_time,
				      new_m->originating_change_time);
}

static struct replPropertyMetaData1 *
replmd_replPropertyMetaData1_find_attid(struct replPropertyMetaDataBlob *md_blob,
                                        enum drsuapi_DsAttributeId attid)
{
	uint32_t i;
	struct replPropertyMetaDataCtr1 *rpmd_ctr = &md_blob->ctr.ctr1;

	for (i = 0; i < rpmd_ctr->count; i++) {
		if (rpmd_ctr->array[i].attid == attid) {
			return &rpmd_ctr->array[i];
		}
	}
	return NULL;
}


/*
  handle renames that come in over DRS replication
 */
static int replmd_replicated_handle_rename(struct replmd_replicated_request *ar,
					   struct ldb_message *msg,
					   struct replPropertyMetaDataBlob *rmd,
					   struct replPropertyMetaDataBlob *omd,
					   struct ldb_request *parent)
{
	struct replPropertyMetaData1 *md_remote;
	struct replPropertyMetaData1 *md_local;

	if (ldb_dn_compare(msg->dn, ar->search_msg->dn) == 0) {
		/* no rename */
		return LDB_SUCCESS;
	}

	/* now we need to check for double renames. We could have a
	 * local rename pending which our replication partner hasn't
	 * received yet. We choose which one wins by looking at the
	 * attribute stamps on the two objects, the newer one wins
	 */
	md_remote = replmd_replPropertyMetaData1_find_attid(rmd, DRSUAPI_ATTID_name);
	md_local  = replmd_replPropertyMetaData1_find_attid(omd, DRSUAPI_ATTID_name);
	/* if there is no name attribute then we have to assume the
	   object we've received is in fact newer */
	if (!md_remote || !md_local ||
	    replmd_replPropertyMetaData1_is_newer(md_local, md_remote)) {
		DEBUG(4,("replmd_replicated_request rename %s => %s\n",
			 ldb_dn_get_linearized(ar->search_msg->dn),
			 ldb_dn_get_linearized(msg->dn)));
		/* pass rename to the next module
		 * so it doesn't appear as an originating update */
		return dsdb_module_rename(ar->module,
					  ar->search_msg->dn, msg->dn,
					  DSDB_FLAG_NEXT_MODULE | DSDB_MODIFY_RELAX, parent);
	}

	/* we're going to keep our old object */
	DEBUG(4,(__location__ ": Keeping object %s and rejecting older rename to %s\n",
		 ldb_dn_get_linearized(ar->search_msg->dn),
		 ldb_dn_get_linearized(msg->dn)));
	return LDB_SUCCESS;
}


static int replmd_replicated_apply_merge(struct replmd_replicated_request *ar)
{
	struct ldb_context *ldb;
	struct ldb_request *change_req;
	enum ndr_err_code ndr_err;
	struct ldb_message *msg;
	struct replPropertyMetaDataBlob *rmd;
	struct replPropertyMetaDataBlob omd;
	const struct ldb_val *omd_value;
	struct replPropertyMetaDataBlob nmd;
	struct ldb_val nmd_value;
	unsigned int i;
	uint32_t j,ni=0;
	unsigned int removed_attrs = 0;
	int ret;

	ldb = ldb_module_get_ctx(ar->module);
	msg = ar->objs->objects[ar->index_current].msg;
	rmd = ar->objs->objects[ar->index_current].meta_data;
	ZERO_STRUCT(omd);
	omd.version = 1;

	/* find existing meta data */
	omd_value = ldb_msg_find_ldb_val(ar->search_msg, "replPropertyMetaData");
	if (omd_value) {
		ndr_err = ndr_pull_struct_blob(omd_value, ar, &omd,
					       (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
			return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
		}

		if (omd.version != 1) {
			return replmd_replicated_request_werror(ar, WERR_DS_DRA_INTERNAL_ERROR);
		}
	}

	/* handle renames that come in over DRS */
	ret = replmd_replicated_handle_rename(ar, msg, rmd, &omd, ar->req);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_FATAL,
			  "replmd_replicated_request rename %s => %s failed - %s\n",
			  ldb_dn_get_linearized(ar->search_msg->dn),
			  ldb_dn_get_linearized(msg->dn),
			  ldb_errstring(ldb));
		return replmd_replicated_request_werror(ar, WERR_DS_DRA_DB_ERROR);
	}

	ZERO_STRUCT(nmd);
	nmd.version = 1;
	nmd.ctr.ctr1.count = omd.ctr.ctr1.count + rmd->ctr.ctr1.count;
	nmd.ctr.ctr1.array = talloc_array(ar,
					  struct replPropertyMetaData1,
					  nmd.ctr.ctr1.count);
	if (!nmd.ctr.ctr1.array) return replmd_replicated_request_werror(ar, WERR_NOMEM);

	/* first copy the old meta data */
	for (i=0; i < omd.ctr.ctr1.count; i++) {
		nmd.ctr.ctr1.array[ni]	= omd.ctr.ctr1.array[i];
		ni++;
	}

	/* now merge in the new meta data */
	for (i=0; i < rmd->ctr.ctr1.count; i++) {
		bool found = false;

		for (j=0; j < ni; j++) {
			bool cmp;

			if (rmd->ctr.ctr1.array[i].attid != nmd.ctr.ctr1.array[j].attid) {
				continue;
			}

			cmp = replmd_replPropertyMetaData1_is_newer(&nmd.ctr.ctr1.array[j],
								    &rmd->ctr.ctr1.array[i]);
			if (cmp) {
				/* replace the entry */
				nmd.ctr.ctr1.array[j] = rmd->ctr.ctr1.array[i];
				found = true;
				break;
			}

			if (rmd->ctr.ctr1.array[i].attid != DRSUAPI_ATTID_instanceType) {
				DEBUG(3,("Discarding older DRS attribute update to %s on %s from %s\n",
					 msg->elements[i-removed_attrs].name,
					 ldb_dn_get_linearized(msg->dn),
					 GUID_string(ar, &rmd->ctr.ctr1.array[i].originating_invocation_id)));
			}

			/* we don't want to apply this change so remove the attribute */
			ldb_msg_remove_element(msg, &msg->elements[i-removed_attrs]);
			removed_attrs++;

			found = true;
			break;
		}

		if (found) continue;

		nmd.ctr.ctr1.array[ni] = rmd->ctr.ctr1.array[i];
		ni++;
	}

	/*
	 * finally correct the size of the meta_data array
	 */
	nmd.ctr.ctr1.count = ni;

	/*
	 * the rdn attribute (the alias for the name attribute),
	 * 'cn' for most objects is the last entry in the meta data array
	 * we have stored
	 *
	 * sort the new meta data array
	 */
	ret = replmd_replPropertyMetaDataCtr1_sort(&nmd.ctr.ctr1, ar->schema, msg->dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/*
	 * check if some replicated attributes left, otherwise skip the ldb_modify() call
	 */
	if (msg->num_elements == 0) {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_replicated_apply_merge[%u]: skip replace\n",
			  ar->index_current);

		ar->index_current++;
		return replmd_replicated_apply_next(ar);
	}

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_replicated_apply_merge[%u]: replace %u attributes\n",
		  ar->index_current, msg->num_elements);

	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &ar->seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	for (i=0; i<ni; i++) {
		nmd.ctr.ctr1.array[i].local_usn = ar->seq_num;
	}

	/* create the meta data value */
	ndr_err = ndr_push_struct_blob(&nmd_value, msg, &nmd,
				       (ndr_push_flags_fn_t)ndr_push_replPropertyMetaDataBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
	}

	/*
	 * when we know that we'll modify the record, add the whenChanged, uSNChanged
	 * and replPopertyMetaData attributes
	 */
	ret = ldb_msg_add_string(msg, "whenChanged", ar->objs->objects[ar->index_current].when_changed);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}
	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNChanged", ar->seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}
	ret = ldb_msg_add_value(msg, "replPropertyMetaData", &nmd_value, NULL);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	replmd_ldb_message_sort(msg, ar->schema);

	/* we want to replace the old values */
	for (i=0; i < msg->num_elements; i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	if (DEBUGLVL(4)) {
		char *s = ldb_ldif_message_string(ldb, ar, LDB_CHANGETYPE_MODIFY, msg);
		DEBUG(4, ("DRS replication modify message:\n%s\n", s));
		talloc_free(s);
	}

	ret = ldb_build_mod_req(&change_req,
				ldb,
				ar,
				msg,
				ar->controls,
				ar,
				replmd_op_callback,
				ar->req);
	LDB_REQ_SET_LOCATION(change_req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	/* current partition control needed by "repmd_op_callback" */
	ret = ldb_request_add_control(change_req,
				      DSDB_CONTROL_CURRENT_PARTITION_OID,
				      false, NULL);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	return ldb_next_request(ar->module, change_req);
}

static int replmd_replicated_apply_search_callback(struct ldb_request *req,
						   struct ldb_reply *ares)
{
	struct replmd_replicated_request *ar = talloc_get_type(req->context,
					       struct replmd_replicated_request);
	int ret;

	if (!ares) {
		return ldb_module_done(ar->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS &&
	    ares->error != LDB_ERR_NO_SUCH_OBJECT) {
		return ldb_module_done(ar->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		ar->search_msg = talloc_steal(ar, ares->message);
		break;

	case LDB_REPLY_REFERRAL:
		/* we ignore referrals */
		break;

	case LDB_REPLY_DONE:
		if (ar->search_msg != NULL) {
			ret = replmd_replicated_apply_merge(ar);
		} else {
			ret = replmd_replicated_apply_add(ar);
		}
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ar->req, NULL, NULL, ret);
		}
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int replmd_replicated_uptodate_vector(struct replmd_replicated_request *ar);

static int replmd_replicated_apply_next(struct replmd_replicated_request *ar)
{
	struct ldb_context *ldb;
	int ret;
	char *tmp_str;
	char *filter;
	struct ldb_request *search_req;
	struct ldb_search_options_control *options;

	if (ar->index_current >= ar->objs->num_objects) {
		/* done with it, go to next stage */
		return replmd_replicated_uptodate_vector(ar);
	}

	ldb = ldb_module_get_ctx(ar->module);
	ar->search_msg = NULL;

	tmp_str = ldb_binary_encode(ar, ar->objs->objects[ar->index_current].guid_value);
	if (!tmp_str) return replmd_replicated_request_werror(ar, WERR_NOMEM);

	filter = talloc_asprintf(ar, "(objectGUID=%s)", tmp_str);
	if (!filter) return replmd_replicated_request_werror(ar, WERR_NOMEM);
	talloc_free(tmp_str);

	ret = ldb_build_search_req(&search_req,
				   ldb,
				   ar,
				   NULL,
				   LDB_SCOPE_SUBTREE,
				   filter,
				   NULL,
				   NULL,
				   ar,
				   replmd_replicated_apply_search_callback,
				   ar->req);
	LDB_REQ_SET_LOCATION(search_req);

	ret = ldb_request_add_control(search_req, LDB_CONTROL_SHOW_RECYCLED_OID,
				      true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* we need to cope with cross-partition links, so search for
	   the GUID over all partitions */
	options = talloc(search_req, struct ldb_search_options_control);
	if (options == NULL) {
		DEBUG(0, (__location__ ": out of memory\n"));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	options->search_options = LDB_SEARCH_OPTION_PHANTOM_ROOT;

	ret = ldb_request_add_control(search_req,
				      LDB_CONTROL_SEARCH_OPTIONS_OID,
				      true, options);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_request(ar->module, search_req);
}

static int replmd_replicated_uptodate_modify_callback(struct ldb_request *req,
						      struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ar = talloc_get_type(req->context,
					       struct replmd_replicated_request);
	ldb = ldb_module_get_ctx(ar->module);

	if (!ares) {
		return ldb_module_done(ar->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ar->req, ares->controls,
					ares->response, ares->error);
	}

	if (ares->type != LDB_REPLY_DONE) {
		ldb_set_errstring(ldb, "Invalid reply type\n!");
		return ldb_module_done(ar->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	talloc_free(ares);

	return ldb_module_done(ar->req, NULL, NULL, LDB_SUCCESS);
}

static int replmd_replicated_uptodate_modify(struct replmd_replicated_request *ar)
{
	struct ldb_context *ldb;
	struct ldb_request *change_req;
	enum ndr_err_code ndr_err;
	struct ldb_message *msg;
	struct replUpToDateVectorBlob ouv;
	const struct ldb_val *ouv_value;
	const struct drsuapi_DsReplicaCursor2CtrEx *ruv;
	struct replUpToDateVectorBlob nuv;
	struct ldb_val nuv_value;
	struct ldb_message_element *nuv_el = NULL;
	const struct GUID *our_invocation_id;
	struct ldb_message_element *orf_el = NULL;
	struct repsFromToBlob nrf;
	struct ldb_val *nrf_value = NULL;
	struct ldb_message_element *nrf_el = NULL;
	unsigned int i;
	uint32_t j,ni=0;
	bool found = false;
	time_t t = time(NULL);
	NTTIME now;
	int ret;
	uint32_t instanceType;

	ldb = ldb_module_get_ctx(ar->module);
	ruv = ar->objs->uptodateness_vector;
	ZERO_STRUCT(ouv);
	ouv.version = 2;
	ZERO_STRUCT(nuv);
	nuv.version = 2;

	unix_to_nt_time(&now, t);

	instanceType = ldb_msg_find_attr_as_uint(ar->search_msg, "instanceType", 0);
	if (! (instanceType & INSTANCE_TYPE_IS_NC_HEAD)) {
		DEBUG(4,(__location__ ": Skipping UDV and repsFrom update as not NC root: %s\n",
			 ldb_dn_get_linearized(ar->search_msg->dn)));
		return ldb_module_done(ar->req, NULL, NULL, LDB_SUCCESS);
	}

	/*
	 * first create the new replUpToDateVector
	 */
	ouv_value = ldb_msg_find_ldb_val(ar->search_msg, "replUpToDateVector");
	if (ouv_value) {
		ndr_err = ndr_pull_struct_blob(ouv_value, ar, &ouv,
					       (ndr_pull_flags_fn_t)ndr_pull_replUpToDateVectorBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
			return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
		}

		if (ouv.version != 2) {
			return replmd_replicated_request_werror(ar, WERR_DS_DRA_INTERNAL_ERROR);
		}
	}

	/*
	 * the new uptodateness vector will at least
	 * contain 1 entry, one for the source_dsa
	 *
	 * plus optional values from our old vector and the one from the source_dsa
	 */
	nuv.ctr.ctr2.count = 1 + ouv.ctr.ctr2.count;
	if (ruv) nuv.ctr.ctr2.count += ruv->count;
	nuv.ctr.ctr2.cursors = talloc_array(ar,
					    struct drsuapi_DsReplicaCursor2,
					    nuv.ctr.ctr2.count);
	if (!nuv.ctr.ctr2.cursors) return replmd_replicated_request_werror(ar, WERR_NOMEM);

	/* first copy the old vector */
	for (i=0; i < ouv.ctr.ctr2.count; i++) {
		nuv.ctr.ctr2.cursors[ni] = ouv.ctr.ctr2.cursors[i];
		ni++;
	}

	/* get our invocation_id if we have one already attached to the ldb */
	our_invocation_id = samdb_ntds_invocation_id(ldb);

	/* merge in the source_dsa vector is available */
	for (i=0; (ruv && i < ruv->count); i++) {
		found = false;

		if (our_invocation_id &&
		    GUID_equal(&ruv->cursors[i].source_dsa_invocation_id,
			       our_invocation_id)) {
			continue;
		}

		for (j=0; j < ni; j++) {
			if (!GUID_equal(&ruv->cursors[i].source_dsa_invocation_id,
					&nuv.ctr.ctr2.cursors[j].source_dsa_invocation_id)) {
				continue;
			}

			found = true;

			/*
			 * we update only the highest_usn and not the latest_sync_success time,
			 * because the last success stands for direct replication
			 */
			if (ruv->cursors[i].highest_usn > nuv.ctr.ctr2.cursors[j].highest_usn) {
				nuv.ctr.ctr2.cursors[j].highest_usn = ruv->cursors[i].highest_usn;
			}
			break;
		}

		if (found) continue;

		/* if it's not there yet, add it */
		nuv.ctr.ctr2.cursors[ni] = ruv->cursors[i];
		ni++;
	}

	/*
	 * merge in the current highwatermark for the source_dsa
	 */
	found = false;
	for (j=0; j < ni; j++) {
		if (!GUID_equal(&ar->objs->source_dsa->source_dsa_invocation_id,
				&nuv.ctr.ctr2.cursors[j].source_dsa_invocation_id)) {
			continue;
		}

		found = true;

		/*
		 * here we update the highest_usn and last_sync_success time
		 * because we're directly replicating from the source_dsa
		 *
		 * and use the tmp_highest_usn because this is what we have just applied
		 * to our ldb
		 */
		nuv.ctr.ctr2.cursors[j].highest_usn		= ar->objs->source_dsa->highwatermark.tmp_highest_usn;
		nuv.ctr.ctr2.cursors[j].last_sync_success	= now;
		break;
	}
	if (!found) {
		/*
		 * here we update the highest_usn and last_sync_success time
		 * because we're directly replicating from the source_dsa
		 *
		 * and use the tmp_highest_usn because this is what we have just applied
		 * to our ldb
		 */
		nuv.ctr.ctr2.cursors[ni].source_dsa_invocation_id= ar->objs->source_dsa->source_dsa_invocation_id;
		nuv.ctr.ctr2.cursors[ni].highest_usn		= ar->objs->source_dsa->highwatermark.tmp_highest_usn;
		nuv.ctr.ctr2.cursors[ni].last_sync_success	= now;
		ni++;
	}

	/*
	 * finally correct the size of the cursors array
	 */
	nuv.ctr.ctr2.count = ni;

	/*
	 * sort the cursors
	 */
	TYPESAFE_QSORT(nuv.ctr.ctr2.cursors, nuv.ctr.ctr2.count, drsuapi_DsReplicaCursor2_compare);

	/*
	 * create the change ldb_message
	 */
	msg = ldb_msg_new(ar);
	if (!msg) return replmd_replicated_request_werror(ar, WERR_NOMEM);
	msg->dn = ar->search_msg->dn;

	ndr_err = ndr_push_struct_blob(&nuv_value, msg, &nuv,
				       (ndr_push_flags_fn_t)ndr_push_replUpToDateVectorBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
	}
	ret = ldb_msg_add_value(msg, "replUpToDateVector", &nuv_value, &nuv_el);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}
	nuv_el->flags = LDB_FLAG_MOD_REPLACE;

	/*
	 * now create the new repsFrom value from the given repsFromTo1 structure
	 */
	ZERO_STRUCT(nrf);
	nrf.version					= 1;
	nrf.ctr.ctr1					= *ar->objs->source_dsa;
	nrf.ctr.ctr1.highwatermark.highest_usn		= nrf.ctr.ctr1.highwatermark.tmp_highest_usn;

	/*
	 * first see if we already have a repsFrom value for the current source dsa
	 * if so we'll later replace this value
	 */
	orf_el = ldb_msg_find_element(ar->search_msg, "repsFrom");
	if (orf_el) {
		for (i=0; i < orf_el->num_values; i++) {
			struct repsFromToBlob *trf;

			trf = talloc(ar, struct repsFromToBlob);
			if (!trf) return replmd_replicated_request_werror(ar, WERR_NOMEM);

			ndr_err = ndr_pull_struct_blob(&orf_el->values[i], trf, trf,
						       (ndr_pull_flags_fn_t)ndr_pull_repsFromToBlob);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
				return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
			}

			if (trf->version != 1) {
				return replmd_replicated_request_werror(ar, WERR_DS_DRA_INTERNAL_ERROR);
			}

			/*
			 * we compare the source dsa objectGUID not the invocation_id
			 * because we want only one repsFrom value per source dsa
			 * and when the invocation_id of the source dsa has changed we don't need
			 * the old repsFrom with the old invocation_id
			 */
			if (!GUID_equal(&trf->ctr.ctr1.source_dsa_obj_guid,
					&ar->objs->source_dsa->source_dsa_obj_guid)) {
				talloc_free(trf);
				continue;
			}

			talloc_free(trf);
			nrf_value = &orf_el->values[i];
			break;
		}

		/*
		 * copy over all old values to the new ldb_message
		 */
		ret = ldb_msg_add_empty(msg, "repsFrom", 0, &nrf_el);
		if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);
		*nrf_el = *orf_el;
	}

	/*
	 * if we haven't found an old repsFrom value for the current source dsa
	 * we'll add a new value
	 */
	if (!nrf_value) {
		struct ldb_val zero_value;
		ZERO_STRUCT(zero_value);
		ret = ldb_msg_add_value(msg, "repsFrom", &zero_value, &nrf_el);
		if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

		nrf_value = &nrf_el->values[nrf_el->num_values - 1];
	}

	/* we now fill the value which is already attached to ldb_message */
	ndr_err = ndr_push_struct_blob(nrf_value, msg,
				       &nrf,
				       (ndr_push_flags_fn_t)ndr_push_repsFromToBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
	}

	/*
	 * the ldb_message_element for the attribute, has all the old values and the new one
	 * so we'll replace the whole attribute with all values
	 */
	nrf_el->flags = LDB_FLAG_MOD_REPLACE;

	if (DEBUGLVL(4)) {
		char *s = ldb_ldif_message_string(ldb, ar, LDB_CHANGETYPE_MODIFY, msg);
		DEBUG(4, ("DRS replication uptodate modify message:\n%s\n", s));
		talloc_free(s);
	}

	/* prepare the ldb_modify() request */
	ret = ldb_build_mod_req(&change_req,
				ldb,
				ar,
				msg,
				ar->controls,
				ar,
				replmd_replicated_uptodate_modify_callback,
				ar->req);
	LDB_REQ_SET_LOCATION(change_req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	return ldb_next_request(ar->module, change_req);
}

static int replmd_replicated_uptodate_search_callback(struct ldb_request *req,
						      struct ldb_reply *ares)
{
	struct replmd_replicated_request *ar = talloc_get_type(req->context,
					       struct replmd_replicated_request);
	int ret;

	if (!ares) {
		return ldb_module_done(ar->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS &&
	    ares->error != LDB_ERR_NO_SUCH_OBJECT) {
		return ldb_module_done(ar->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		ar->search_msg = talloc_steal(ar, ares->message);
		break;

	case LDB_REPLY_REFERRAL:
		/* we ignore referrals */
		break;

	case LDB_REPLY_DONE:
		if (ar->search_msg == NULL) {
			ret = replmd_replicated_request_werror(ar, WERR_DS_DRA_INTERNAL_ERROR);
		} else {
			ret = replmd_replicated_uptodate_modify(ar);
		}
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ar->req, NULL, NULL, ret);
		}
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}


static int replmd_replicated_uptodate_vector(struct replmd_replicated_request *ar)
{
	struct ldb_context *ldb;
	int ret;
	static const char *attrs[] = {
		"replUpToDateVector",
		"repsFrom",
		"instanceType",
		NULL
	};
	struct ldb_request *search_req;

	ldb = ldb_module_get_ctx(ar->module);
	ar->search_msg = NULL;

	ret = ldb_build_search_req(&search_req,
				   ldb,
				   ar,
				   ar->objs->partition_dn,
				   LDB_SCOPE_BASE,
				   "(objectClass=*)",
				   attrs,
				   NULL,
				   ar,
				   replmd_replicated_uptodate_search_callback,
				   ar->req);
	LDB_REQ_SET_LOCATION(search_req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	return ldb_next_request(ar->module, search_req);
}



static int replmd_extended_replicated_objects(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct dsdb_extended_replicated_objects *objs;
	struct replmd_replicated_request *ar;
	struct ldb_control **ctrls;
	int ret;
	uint32_t i;
	struct replmd_private *replmd_private =
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_extended_replicated_objects\n");

	objs = talloc_get_type(req->op.extended.data, struct dsdb_extended_replicated_objects);
	if (!objs) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "replmd_extended_replicated_objects: invalid extended data\n");
		return LDB_ERR_PROTOCOL_ERROR;
	}

	if (objs->version != DSDB_EXTENDED_REPLICATED_OBJECTS_VERSION) {
		ldb_debug(ldb, LDB_DEBUG_FATAL, "replmd_extended_replicated_objects: extended data invalid version [%u != %u]\n",
			  objs->version, DSDB_EXTENDED_REPLICATED_OBJECTS_VERSION);
		return LDB_ERR_PROTOCOL_ERROR;
	}

	ar = replmd_ctx_init(module, req);
	if (!ar)
		return LDB_ERR_OPERATIONS_ERROR;

	/* Set the flags to have the replmd_op_callback run over the full set of objects */
	ar->apply_mode = true;
	ar->objs = objs;
	ar->schema = dsdb_get_schema(ldb, ar);
	if (!ar->schema) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL, "replmd_ctx_init: no loaded schema found\n");
		talloc_free(ar);
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ctrls = req->controls;

	if (req->controls) {
		req->controls = talloc_memdup(ar, req->controls,
					      talloc_get_size(req->controls));
		if (!req->controls) return replmd_replicated_request_werror(ar, WERR_NOMEM);
	}

	/* This allows layers further down to know if a change came in over replication */
	ret = ldb_request_add_control(req, DSDB_CONTROL_REPLICATED_UPDATE_OID, false, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* If this change contained linked attributes in the body
	 * (rather than in the links section) we need to update
	 * backlinks in linked_attributes */
	ret = ldb_request_add_control(req, DSDB_CONTROL_APPLY_LINKS, false, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ar->controls = req->controls;
	req->controls = ctrls;

	DEBUG(4,("linked_attributes_count=%u\n", objs->linked_attributes_count));

	/* save away the linked attributes for the end of the
	   transaction */
	for (i=0; i<ar->objs->linked_attributes_count; i++) {
		struct la_entry *la_entry;

		if (replmd_private->la_ctx == NULL) {
			replmd_private->la_ctx = talloc_new(replmd_private);
		}
		la_entry = talloc(replmd_private->la_ctx, struct la_entry);
		if (la_entry == NULL) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		la_entry->la = talloc(la_entry, struct drsuapi_DsReplicaLinkedAttribute);
		if (la_entry->la == NULL) {
			talloc_free(la_entry);
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		*la_entry->la = ar->objs->linked_attributes[i];

		/* we need to steal the non-scalars so they stay
		   around until the end of the transaction */
		talloc_steal(la_entry->la, la_entry->la->identifier);
		talloc_steal(la_entry->la, la_entry->la->value.blob);

		DLIST_ADD(replmd_private->la_list, la_entry);
	}

	return replmd_replicated_apply_next(ar);
}

/*
  process one linked attribute structure
 */
static int replmd_process_linked_attribute(struct ldb_module *module,
					   struct la_entry *la_entry,
					   struct ldb_request *parent)
{
	struct drsuapi_DsReplicaLinkedAttribute *la = la_entry->la;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_message *msg;
	TALLOC_CTX *tmp_ctx = talloc_new(la_entry);
	const struct dsdb_schema *schema = dsdb_get_schema(ldb, tmp_ctx);
	int ret;
	const struct dsdb_attribute *attr;
	struct dsdb_dn *dsdb_dn;
	uint64_t seq_num = 0;
	struct ldb_message_element *old_el;
	WERROR status;
	time_t t = time(NULL);
	struct ldb_result *res;
	const char *attrs[2];
	struct parsed_dn *pdn_list, *pdn;
	struct GUID guid = GUID_zero();
	NTSTATUS ntstatus;
	bool active = (la->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE)?true:false;
	const struct GUID *our_invocation_id;

/*
linked_attributes[0]:
     &objs->linked_attributes[i]: struct drsuapi_DsReplicaLinkedAttribute
        identifier               : *
            identifier: struct drsuapi_DsReplicaObjectIdentifier
                __ndr_size               : 0x0000003a (58)
                __ndr_size_sid           : 0x00000000 (0)
                guid                     : 8e95b6a9-13dd-4158-89db-3220a5be5cc7
                sid                      : S-0-0
                __ndr_size_dn            : 0x00000000 (0)
                dn                       : ''
        attid                    : DRSUAPI_ATTID_member (0x1F)
        value: struct drsuapi_DsAttributeValue
            __ndr_size               : 0x0000007e (126)
            blob                     : *
                blob                     : DATA_BLOB length=126
        flags                    : 0x00000001 (1)
               1: DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE
        originating_add_time     : Wed Sep  2 22:20:01 2009 EST
        meta_data: struct drsuapi_DsReplicaMetaData
            version                  : 0x00000015 (21)
            originating_change_time  : Wed Sep  2 23:39:07 2009 EST
            originating_invocation_id: 794640f3-18cf-40ee-a211-a93992b67a64
            originating_usn          : 0x000000000001e19c (123292)

(for cases where the link is to a normal DN)
     &target: struct drsuapi_DsReplicaObjectIdentifier3
        __ndr_size               : 0x0000007e (126)
        __ndr_size_sid           : 0x0000001c (28)
        guid                     : 7639e594-db75-4086-b0d4-67890ae46031
        sid                      : S-1-5-21-2848215498-2472035911-1947525656-19924
        __ndr_size_dn            : 0x00000022 (34)
        dn                       : 'CN=UOne,OU=TestOU,DC=vsofs8,DC=com'
 */

	/* find the attribute being modified */
	attr = dsdb_attribute_by_attributeID_id(schema, la->attid);
	if (attr == NULL) {
		DEBUG(0, (__location__ ": Unable to find attributeID 0x%x\n", la->attid));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	attrs[0] = attr->lDAPDisplayName;
	attrs[1] = NULL;

	/* get the existing message from the db for the object with
	   this GUID, returning attribute being modified. We will then
	   use this msg as the basis for a modify call */
	ret = dsdb_module_search(module, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE, attrs,
	                         DSDB_FLAG_NEXT_MODULE |
				 DSDB_SEARCH_SEARCH_ALL_PARTITIONS |
				 DSDB_SEARCH_SHOW_RECYCLED |
				 DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT |
				 DSDB_SEARCH_REVEAL_INTERNALS,
				 parent,
				 "objectGUID=%s", GUID_string(tmp_ctx, &la->identifier->guid));
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	if (res->count != 1) {
		ldb_asprintf_errstring(ldb, "DRS linked attribute for GUID %s - DN not found",
				       GUID_string(tmp_ctx, &la->identifier->guid));
		talloc_free(tmp_ctx);
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	msg = res->msgs[0];

	if (msg->num_elements == 0) {
		ret = ldb_msg_add_empty(msg, attr->lDAPDisplayName, LDB_FLAG_MOD_REPLACE, &old_el);
		if (ret != LDB_SUCCESS) {
			ldb_module_oom(module);
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	} else {
		old_el = &msg->elements[0];
		old_el->flags = LDB_FLAG_MOD_REPLACE;
	}

	/* parse the existing links */
	ret = get_parsed_dns(module, tmp_ctx, old_el, &pdn_list, attr->syntax->ldap_oid, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	/* get our invocationId */
	our_invocation_id = samdb_ntds_invocation_id(ldb);
	if (!our_invocation_id) {
		ldb_debug_set(ldb, LDB_DEBUG_ERROR, __location__ ": unable to find invocationId\n");
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = replmd_check_upgrade_links(pdn_list, old_el->num_values, old_el, our_invocation_id);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	status = dsdb_dn_la_from_blob(ldb, attr, schema, tmp_ctx, la->value.blob, &dsdb_dn);
	if (!W_ERROR_IS_OK(status)) {
		ldb_asprintf_errstring(ldb, "Failed to parsed linked attribute blob for %s on %s - %s\n",
				       old_el->name, ldb_dn_get_linearized(msg->dn), win_errstr(status));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ntstatus = dsdb_get_extended_dn_guid(dsdb_dn->dn, &guid, "GUID");
	if (!NT_STATUS_IS_OK(ntstatus) && active) {
		ldb_asprintf_errstring(ldb, "Failed to find GUID in linked attribute blob for %s on %s from %s",
				       old_el->name,
				       ldb_dn_get_linearized(dsdb_dn->dn),
				       ldb_dn_get_linearized(msg->dn));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* re-resolve the DN by GUID, as the DRS server may give us an
	   old DN value */
	ret = dsdb_module_dn_by_guid(module, dsdb_dn, &guid, &dsdb_dn->dn, parent);
	if (ret != LDB_SUCCESS) {
		DEBUG(2,(__location__ ": WARNING: Failed to re-resolve GUID %s - using %s",
			 GUID_string(tmp_ctx, &guid),
			 ldb_dn_get_linearized(dsdb_dn->dn)));
	}

	/* see if this link already exists */
	pdn = parsed_dn_find(pdn_list, old_el->num_values, &guid, dsdb_dn->dn);
	if (pdn != NULL) {
		/* see if this update is newer than what we have already */
		struct GUID invocation_id = GUID_zero();
		uint32_t version = 0;
		uint32_t originating_usn = 0;
		NTTIME change_time = 0;
		uint32_t rmd_flags = dsdb_dn_rmd_flags(pdn->dsdb_dn->dn);

		dsdb_get_extended_dn_guid(pdn->dsdb_dn->dn, &invocation_id, "RMD_INVOCID");
		dsdb_get_extended_dn_uint32(pdn->dsdb_dn->dn, &version, "RMD_VERSION");
		dsdb_get_extended_dn_uint32(pdn->dsdb_dn->dn, &originating_usn, "RMD_ORIGINATING_USN");
		dsdb_get_extended_dn_nttime(pdn->dsdb_dn->dn, &change_time, "RMD_CHANGETIME");

		if (!replmd_update_is_newer(&invocation_id,
					    &la->meta_data.originating_invocation_id,
					    version,
					    la->meta_data.version,
					    change_time,
					    la->meta_data.originating_change_time)) {
			DEBUG(3,("Discarding older DRS linked attribute update to %s on %s from %s\n",
				 old_el->name, ldb_dn_get_linearized(msg->dn),
				 GUID_string(tmp_ctx, &la->meta_data.originating_invocation_id)));
			talloc_free(tmp_ctx);
			return LDB_SUCCESS;
		}

		/* get a seq_num for this change */
		ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		if (!(rmd_flags & DSDB_RMD_FLAG_DELETED)) {
			/* remove the existing backlink */
			ret = replmd_add_backlink(module, schema, &la->identifier->guid, &guid, false, attr, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
		}

		ret = replmd_update_la_val(tmp_ctx, pdn->v, dsdb_dn, pdn->dsdb_dn,
					   &la->meta_data.originating_invocation_id,
					   la->meta_data.originating_usn, seq_num,
					   la->meta_data.originating_change_time,
					   la->meta_data.version,
					   !active);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		if (active) {
			/* add the new backlink */
			ret = replmd_add_backlink(module, schema, &la->identifier->guid, &guid, true, attr, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
		}
	} else {
		/* get a seq_num for this change */
		ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		old_el->values = talloc_realloc(msg->elements, old_el->values,
						struct ldb_val, old_el->num_values+1);
		if (!old_el->values) {
			ldb_module_oom(module);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		old_el->num_values++;

		ret = replmd_build_la_val(tmp_ctx, &old_el->values[old_el->num_values-1], dsdb_dn,
					  &la->meta_data.originating_invocation_id,
					  la->meta_data.originating_usn, seq_num,
					  la->meta_data.originating_change_time,
					  la->meta_data.version,
					  (la->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE)?false:true);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		if (active) {
			ret = replmd_add_backlink(module, schema, &la->identifier->guid, &guid,
						  true, attr, false);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return ret;
			}
		}
	}

	/* we only change whenChanged and uSNChanged if the seq_num
	   has changed */
	if (add_time_element(msg, "whenChanged", t) != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	if (add_uint64_element(ldb, msg, "uSNChanged",
			       seq_num) != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	old_el = ldb_msg_find_element(msg, attr->lDAPDisplayName);
	if (old_el == NULL) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	ret = dsdb_check_single_valued_link(attr, old_el);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	old_el->flags |= LDB_FLAG_INTERNAL_DISABLE_SINGLE_VALUE_CHECK;

	ret = dsdb_module_modify(module, msg, DSDB_FLAG_NEXT_MODULE, parent);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "Failed to apply linked attribute change '%s'\n%s\n",
			  ldb_errstring(ldb),
			  ldb_ldif_message_string(ldb, tmp_ctx, LDB_CHANGETYPE_MODIFY, msg));
		talloc_free(tmp_ctx);
		return ret;
	}

	talloc_free(tmp_ctx);

	return ret;
}

static int replmd_extended(struct ldb_module *module, struct ldb_request *req)
{
	if (strcmp(req->op.extended.oid, DSDB_EXTENDED_REPLICATED_OBJECTS_OID) == 0) {
		return replmd_extended_replicated_objects(module, req);
	}

	return ldb_next_request(module, req);
}


/*
  we hook into the transaction operations to allow us to
  perform the linked attribute updates at the end of the whole
  transaction. This allows a forward linked attribute to be created
  before the object is created. During a vampire, w2k8 sends us linked
  attributes before the objects they are part of.
 */
static int replmd_start_transaction(struct ldb_module *module)
{
	/* create our private structure for this transaction */
	struct replmd_private *replmd_private = talloc_get_type(ldb_module_get_private(module),
								struct replmd_private);
	replmd_txn_cleanup(replmd_private);

	/* free any leftover mod_usn records from cancelled
	   transactions */
	while (replmd_private->ncs) {
		struct nc_entry *e = replmd_private->ncs;
		DLIST_REMOVE(replmd_private->ncs, e);
		talloc_free(e);
	}

	return ldb_next_start_trans(module);
}

/*
  on prepare commit we loop over our queued la_context structures and
  apply each of them
 */
static int replmd_prepare_commit(struct ldb_module *module)
{
	struct replmd_private *replmd_private =
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);
	struct la_entry *la, *prev;
	struct la_backlink *bl;
	int ret;

	/* walk the list backwards, to do the first entry first, as we
	 * added the entries with DLIST_ADD() which puts them at the
	 * start of the list */
	for (la = DLIST_TAIL(replmd_private->la_list); la; la=prev) {
		prev = DLIST_PREV(la);
		DLIST_REMOVE(replmd_private->la_list, la);
		ret = replmd_process_linked_attribute(module, la, NULL);
		if (ret != LDB_SUCCESS) {
			replmd_txn_cleanup(replmd_private);
			return ret;
		}
	}

	/* process our backlink list, creating and deleting backlinks
	   as necessary */
	for (bl=replmd_private->la_backlinks; bl; bl=bl->next) {
		ret = replmd_process_backlink(module, bl, NULL);
		if (ret != LDB_SUCCESS) {
			replmd_txn_cleanup(replmd_private);
			return ret;
		}
	}

	replmd_txn_cleanup(replmd_private);

	/* possibly change @REPLCHANGED */
	ret = replmd_notify_store(module, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ldb_next_prepare_commit(module);
}

static int replmd_del_transaction(struct ldb_module *module)
{
	struct replmd_private *replmd_private =
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);
	replmd_txn_cleanup(replmd_private);

	return ldb_next_del_trans(module);
}


static const struct ldb_module_ops ldb_repl_meta_data_module_ops = {
	.name          = "repl_meta_data",
	.init_context	   = replmd_init,
	.add               = replmd_add,
	.modify            = replmd_modify,
	.rename            = replmd_rename,
	.del	           = replmd_delete,
	.extended          = replmd_extended,
	.start_transaction = replmd_start_transaction,
	.prepare_commit    = replmd_prepare_commit,
	.del_transaction   = replmd_del_transaction,
};

int ldb_repl_meta_data_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_repl_meta_data_module_ops);
}
