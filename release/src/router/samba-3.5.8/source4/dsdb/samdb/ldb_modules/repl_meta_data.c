/* 
   ldb database library

   Copyright (C) Simo Sorce  2004-2008
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007

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
#include "libcli/security/dom_sid.h"
#include "lib/util/dlinklist.h"

struct replmd_private {
	TALLOC_CTX *la_ctx;
	struct la_entry *la_list;
	uint32_t num_ncs;
	struct nc_entry {
		struct ldb_dn *dn;
		struct GUID guid;
		uint64_t mod_usn;
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

	struct dsdb_extended_replicated_objects *objs;

	/* the controls we pass down */
	struct ldb_control **controls;

	uint32_t index_current;

	struct ldb_message *search_msg;
};


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


static int nc_compare(struct nc_entry *n1, struct nc_entry *n2)
{
	return ldb_dn_compare(n1->dn, n2->dn);
}

/*
  build the list of partition DNs for use by replmd_notify()
 */
static int replmd_load_NCs(struct ldb_module *module)
{
	const char *attrs[] = { "namingContexts", NULL };
	struct ldb_result *res = NULL;
	int i, ret;
	TALLOC_CTX *tmp_ctx;
	struct ldb_context *ldb;
	struct ldb_message_element *el;
	struct replmd_private *replmd_private = 
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);

	if (replmd_private->ncs != NULL) {
		return LDB_SUCCESS;
	}

	ldb = ldb_module_get_ctx(module);
	tmp_ctx = talloc_new(module);

	/* load the list of naming contexts */
	ret = ldb_search(ldb, tmp_ctx, &res, ldb_dn_new(tmp_ctx, ldb, ""), 
			 LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS ||
	    res->count != 1) {
		DEBUG(0,(__location__ ": Failed to load rootDSE\n"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	el = ldb_msg_find_element(res->msgs[0], "namingContexts");
	if (el == NULL) {
		DEBUG(0,(__location__ ": Failed to load namingContexts\n"));
		return LDB_ERR_OPERATIONS_ERROR;		
	}

	replmd_private->num_ncs = el->num_values;
	replmd_private->ncs = talloc_array(replmd_private, struct nc_entry, 
					   replmd_private->num_ncs);
	if (replmd_private->ncs == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; i<replmd_private->num_ncs; i++) {
		replmd_private->ncs[i].dn = 
			ldb_dn_from_ldb_val(replmd_private->ncs, 
					    ldb, &el->values[i]);
		replmd_private->ncs[i].mod_usn = 0;
	}

	talloc_free(res);

	/* now find the GUIDs of each of those DNs */
	for (i=0; i<replmd_private->num_ncs; i++) {
		const char *attrs2[] = { "objectGUID", NULL };
		ret = ldb_search(ldb, tmp_ctx, &res, replmd_private->ncs[i].dn,
				 LDB_SCOPE_BASE, attrs2, NULL);
		if (ret != LDB_SUCCESS ||
		    res->count != 1) {
			/* this happens when the schema is first being
			   setup */
			talloc_free(replmd_private->ncs);
			replmd_private->ncs = NULL;
			replmd_private->num_ncs = 0;
			talloc_free(tmp_ctx);
			return LDB_SUCCESS;
		}
		replmd_private->ncs[i].guid = 
			samdb_result_guid(res->msgs[0], "objectGUID");
		talloc_free(res);
	}	

	/* sort the NCs into order, most to least specific */
	qsort(replmd_private->ncs, replmd_private->num_ncs,
	      sizeof(replmd_private->ncs[0]), QSORT_CAST nc_compare);

	
	talloc_free(tmp_ctx);
	
	return LDB_SUCCESS;
}


/*
 * notify the repl task that a object has changed. The notifies are
 * gathered up in the replmd_private structure then written to the
 * @REPLCHANGED object in each partition during the prepare_commit
 */
static int replmd_notify(struct ldb_module *module, struct ldb_dn *dn, uint64_t uSN)
{
	int ret, i;
	struct replmd_private *replmd_private = 
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);

	ret = replmd_load_NCs(module);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	if (replmd_private->num_ncs == 0) {
		return LDB_SUCCESS;
	}

	for (i=0; i<replmd_private->num_ncs; i++) {
		if (ldb_dn_compare_base(replmd_private->ncs[i].dn, dn) == 0) {
			break;
		}
	}
	if (i == replmd_private->num_ncs) {
		DEBUG(0,(__location__ ": DN not within known NCs '%s'\n", 
			 ldb_dn_get_linearized(dn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (uSN > replmd_private->ncs[i].mod_usn) {
	    replmd_private->ncs[i].mod_usn = uSN;
	}

	return LDB_SUCCESS;
}


/*
 * update a @REPLCHANGED record in each partition if there have been
 * any writes of replicated data in the partition
 */
static int replmd_notify_store(struct ldb_module *module)
{
	int i;
	struct replmd_private *replmd_private = 
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);
	struct ldb_context *ldb = ldb_module_get_ctx(module);

	for (i=0; i<replmd_private->num_ncs; i++) {
		int ret;

		if (replmd_private->ncs[i].mod_usn == 0) {
			/* this partition has not changed in this
			   transaction */
			continue;
		}

		ret = dsdb_save_partition_usn(ldb, replmd_private->ncs[i].dn, 
					      replmd_private->ncs[i].mod_usn);
		if (ret != LDB_SUCCESS) {
			DEBUG(0,(__location__ ": Failed to save partition uSN for %s\n",
				 ldb_dn_get_linearized(replmd_private->ncs[i].dn)));
			return ret;
		}
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
	return ac;
}

/*
  add a time element to a record
*/
static int add_time_element(struct ldb_message *msg, const char *attr, time_t t)
{
	struct ldb_message_element *el;
	char *s;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return LDB_SUCCESS;
	}

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ldb_msg_add_string(msg, attr, s) != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
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
static int add_uint64_element(struct ldb_message *msg, const char *attr, uint64_t v)
{
	struct ldb_message_element *el;

	if (ldb_msg_find_element(msg, attr) != NULL) {
		return LDB_SUCCESS;
	}

	if (ldb_msg_add_fmt(msg, attr, "%llu", (unsigned long long)v) != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
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

	return m1->attid - m2->attid;
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

	ldb_qsort(ctr1->array, ctr1->count, sizeof(struct replPropertyMetaData1),
		  discard_const_p(void, &rdn_sa->attributeID_id), 
		  (ldb_qsort_cmp_fn_t)replmd_replPropertyMetaData1_attid_sort);

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

	return a1->attributeID_id - a2->attributeID_id;
}

static void replmd_ldb_message_sort(struct ldb_message *msg,
				    const struct dsdb_schema *schema)
{
	ldb_qsort(msg->elements, msg->num_elements, sizeof(struct ldb_message_element),
		  discard_const_p(void, schema), (ldb_qsort_cmp_fn_t)replmd_ldb_message_element_attid_sort);
}

static int replmd_op_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;

	ac = talloc_get_type(req->context, struct replmd_replicated_request);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
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

	return ldb_module_done(ac->req, ares->controls,
				ares->response, LDB_SUCCESS);
}

static int replmd_add(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;
	const struct dsdb_schema *schema;
	enum ndr_err_code ndr_err;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	struct GUID guid;
	struct ldb_val guid_value;
	struct replPropertyMetaDataBlob nmd;
	struct ldb_val nmd_value;
	uint64_t seq_num;
	const struct GUID *our_invocation_id;
	time_t t = time(NULL);
	NTTIME now;
	char *time_str;
	int ret;
	uint32_t i, ni=0;

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.add.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_add\n");

	schema = dsdb_get_schema(ldb);
	if (!schema) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "replmd_add: no dsdb_schema loaded");
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac = replmd_ctx_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->schema = schema;

	if (ldb_msg_find_element(req->op.add.message, "objectGUID") != NULL) {
		ldb_debug_set(ldb, LDB_DEBUG_ERROR,
			      "replmd_add: it's not allowed to add an object with objectGUID\n");
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* a new GUID */
	guid = GUID_random();

	/* get our invocationId */
	our_invocation_id = samdb_ntds_invocation_id(ldb);
	if (!our_invocation_id) {
		ldb_debug_set(ldb, LDB_DEBUG_ERROR,
			      "replmd_add: unable to find invocationId\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.add.message);
	if (msg == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* generated times */
	unix_to_nt_time(&now, t);
	time_str = ldb_timestring(msg, t);
	if (!time_str) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* 
	 * remove autogenerated attributes
	 */
	ldb_msg_remove_attr(msg, "whenCreated");
	ldb_msg_remove_attr(msg, "whenChanged");
	ldb_msg_remove_attr(msg, "uSNCreated");
	ldb_msg_remove_attr(msg, "uSNChanged");
	ldb_msg_remove_attr(msg, "replPropertyMetaData");

	if (!ldb_msg_find_element(req->op.add.message, "instanceType")) {
		ret = ldb_msg_add_fmt(msg, "instanceType", "%u", INSTANCE_TYPE_WRITE);
		if (ret != LDB_SUCCESS) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	/*
	 * readd replicated attributes
	 */
	ret = ldb_msg_add_string(msg, "whenCreated", time_str);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
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
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0; i < msg->num_elements; i++) {
		struct ldb_message_element *e = &msg->elements[i];
		struct replPropertyMetaData1 *m = &nmd.ctr.ctr1.array[ni];
		const struct dsdb_attribute *sa;

		if (e->name[0] == '@') continue;

		sa = dsdb_attribute_by_lDAPDisplayName(schema, e->name);
		if (!sa) {
			ldb_debug_set(ldb, LDB_DEBUG_ERROR,
				      "replmd_add: attribute '%s' not defined in schema\n",
				      e->name);
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}

		if ((sa->systemFlags & DS_FLAG_ATTR_NOT_REPLICATED) || (sa->systemFlags & DS_FLAG_ATTR_IS_CONSTRUCTED)) {
			/* if the attribute is not replicated (0x00000001)
			 * or constructed (0x00000004) it has no metadata
			 */
			continue;
		}

		m->attid			= sa->attributeID_id;
		m->version			= 1;
		m->originating_change_time	= now;
		m->originating_invocation_id	= *our_invocation_id;
		m->originating_usn		= seq_num;
		m->local_usn			= seq_num;
		ni++;
	}

	/* fix meta data count */
	nmd.ctr.ctr1.count = ni;

	/*
	 * sort meta data array, and move the rdn attribute entry to the end
	 */
	ret = replmd_replPropertyMetaDataCtr1_sort(&nmd.ctr.ctr1, schema, msg->dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* generated NDR encoded values */
	ndr_err = ndr_push_struct_blob(&guid_value, msg, 
				       NULL,
				       &guid,
				       (ndr_push_flags_fn_t)ndr_push_GUID);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ndr_err = ndr_push_struct_blob(&nmd_value, msg, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &nmd,
				       (ndr_push_flags_fn_t)ndr_push_replPropertyMetaDataBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * add the autogenerated values
	 */
	ret = ldb_msg_add_value(msg, "objectGUID", &guid_value, NULL);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret = ldb_msg_add_string(msg, "whenChanged", time_str);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNCreated", seq_num);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNChanged", seq_num);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret = ldb_msg_add_value(msg, "replPropertyMetaData", &nmd_value, NULL);
	if (ret != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * sort the attributes by attid before storing the object
	 */
	replmd_ldb_message_sort(msg, schema);

	ret = ldb_build_add_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, replmd_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = replmd_notify(module, msg->dn, seq_num);
	if (ret != LDB_SUCCESS) {
		return ret;
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
				      struct replPropertyMetaDataBlob *omd,
				      struct dsdb_schema *schema,
				      uint64_t *seq_num,
				      const struct GUID *our_invocation_id,
				      NTTIME now)
{
	int i;
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

	for (i=0; i<omd->ctr.ctr1.count; i++) {
		if (a->attributeID_id == omd->ctr.ctr1.array[i].attid) break;
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

/*
 * update the replPropertyMetaData object each time we modify an
 * object. This is needed for DRS replication, as the merge on the
 * client is based on this object 
 */
static int replmd_update_rpmd(struct ldb_module *module, 
			      struct ldb_message *msg, uint64_t *seq_num)
{
	const struct ldb_val *omd_value;
	enum ndr_err_code ndr_err;
	struct replPropertyMetaDataBlob omd;
	int i;
	struct dsdb_schema *schema;
	time_t t = time(NULL);
	NTTIME now;
	const struct GUID *our_invocation_id;
	int ret;
	const char *attrs[] = { "replPropertyMetaData" , NULL };
	struct ldb_result *res;
	struct ldb_context *ldb;

	ldb = ldb_module_get_ctx(module);

	our_invocation_id = samdb_ntds_invocation_id(ldb);
	if (!our_invocation_id) {
		/* this happens during an initial vampire while
		   updating the schema */
		DEBUG(5,("No invocationID - skipping replPropertyMetaData update\n"));
		return LDB_SUCCESS;
	}

	unix_to_nt_time(&now, t);

	/* search for the existing replPropertyMetaDataBlob */
	ret = dsdb_search_dn_with_deleted(ldb, msg, &res, msg->dn, attrs);
	if (ret != LDB_SUCCESS || res->count < 1) {
		DEBUG(0,(__location__ ": Object %s failed to find replPropertyMetaData\n",
			 ldb_dn_get_linearized(msg->dn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}
		

	omd_value = ldb_msg_find_ldb_val(res->msgs[0], "replPropertyMetaData");
	if (!omd_value) {
		DEBUG(0,(__location__ ": Object %s does not have a replPropertyMetaData attribute\n",
			 ldb_dn_get_linearized(msg->dn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ndr_err = ndr_pull_struct_blob(omd_value, msg,
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), &omd,
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

	schema = dsdb_get_schema(ldb);

	for (i=0; i<msg->num_elements; i++) {
		ret = replmd_update_rpmd_element(ldb, msg, &msg->elements[i], &omd, schema, seq_num, 
						 our_invocation_id, now);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	/*
	 * replmd_update_rpmd_element has done an update if the
	 * seq_num is set
	 */
	if (*seq_num != 0) {
		struct ldb_val *md_value;
		struct ldb_message_element *el;

		md_value = talloc(msg, struct ldb_val);
		if (md_value == NULL) {
			ldb_oom(ldb);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = replmd_replPropertyMetaDataCtr1_sort(&omd.ctr.ctr1, schema, msg->dn);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ndr_err = ndr_push_struct_blob(md_value, msg, 
					       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
					       &omd,
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

		ret = replmd_notify(module, msg->dn, *seq_num);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		el->num_values = 1;
		el->values = md_value;
	}

	return LDB_SUCCESS;	
}


static int replmd_modify(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ac;
	const struct dsdb_schema *schema;
	struct ldb_request *down_req;
	struct ldb_message *msg;
	int ret;
	time_t t = time(NULL);
	uint64_t seq_num = 0;

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_modify\n");

	schema = dsdb_get_schema(ldb);
	if (!schema) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "replmd_modify: no dsdb_schema loaded");
		DEBUG(0,(__location__ ": %s\n", ldb_errstring(ldb)));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	ac = replmd_ctx_init(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->schema = schema;

	/* we have to copy the message as the caller might have it as a const */
	msg = ldb_msg_copy_shallow(ac, req->op.mod.message);
	if (msg == NULL) {
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* TODO:
	 * - get the whole old object
	 * - if the old object doesn't exist report an error
	 * - give an error when a readonly attribute should
	 *   be modified
	 * - merge the changed into the old object
	 *   if the caller set values to the same value
	 *   ignore the attribute, return success when no
	 *   attribute was changed
	 */

	ret = replmd_update_rpmd(module, msg, &seq_num);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* TODO:
	 * - replace the old object with the newly constructed one
	 */

	ret = ldb_build_mod_req(&down_req, ldb, ac,
				msg,
				req->controls,
				ac, replmd_op_callback,
				req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	talloc_steal(down_req, msg);

	/* we only change whenChanged and uSNChanged if the seq_num
	   has changed */
	if (seq_num != 0) {
		if (add_time_element(msg, "whenChanged", t) != LDB_SUCCESS) {
			talloc_free(ac);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (add_uint64_element(msg, "uSNChanged", seq_num) != LDB_SUCCESS) {
			talloc_free(ac);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	/* go on with the call chain */
	return ldb_next_request(module, down_req);
}


/*
  handle a rename request

  On a rename we need to do an extra ldb_modify which sets the
  whenChanged and uSNChanged attributes
 */
static int replmd_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	int ret, i;
	time_t t = time(NULL);
	uint64_t seq_num = 0;
	struct ldb_message *msg;
	struct replmd_private *replmd_private = 
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);

	/* do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.mod.message->dn)) {
		return ldb_next_request(module, req);
	}

	ldb = ldb_module_get_ctx(module);

	ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_rename\n");

	/* Get a sequence number from the backend */
	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	msg = ldb_msg_new(req);
	if (msg == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->dn = req->op.rename.olddn;

	if (add_time_element(msg, "whenChanged", t) != LDB_SUCCESS) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;

	if (add_uint64_element(msg, "uSNChanged", seq_num) != LDB_SUCCESS) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	msg->elements[1].flags = LDB_FLAG_MOD_REPLACE;

	ret = ldb_modify(ldb, msg);
	talloc_free(msg);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = replmd_load_NCs(module);
	if (ret != 0) {
		return ret;
	}

	/* now update the highest uSNs of the partitions that are
	   affected. Note that two partitions could be changing */
	for (i=0; i<replmd_private->num_ncs; i++) {
		if (ldb_dn_compare_base(replmd_private->ncs[i].dn, 
					req->op.rename.olddn) == 0) {
			break;
		}
	}
	if (i == replmd_private->num_ncs) {
		DEBUG(0,(__location__ ": rename olddn outside tree? %s\n",
			 ldb_dn_get_linearized(req->op.rename.olddn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	replmd_private->ncs[i].mod_usn = seq_num;

	for (i=0; i<replmd_private->num_ncs; i++) {
		if (ldb_dn_compare_base(replmd_private->ncs[i].dn, 
					req->op.rename.newdn) == 0) {
			break;
		}
	}
	if (i == replmd_private->num_ncs) {
		DEBUG(0,(__location__ ": rename newdn outside tree? %s\n",
			 ldb_dn_get_linearized(req->op.rename.newdn)));
		return LDB_ERR_OPERATIONS_ERROR;
	}
	replmd_private->ncs[i].mod_usn = seq_num;
	
	/* go on with the call chain */
	return ldb_next_request(module, req);
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

static int replmd_replicated_apply_next(struct replmd_replicated_request *ar);

static int replmd_replicated_apply_add_callback(struct ldb_request *req,
						struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ar = talloc_get_type(req->context,
					       struct replmd_replicated_request);
	int ret;

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
	ar->index_current++;

	ret = replmd_replicated_apply_next(ar);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ar->req, NULL, NULL, ret);
	}

	return LDB_SUCCESS;
}

static int replmd_replicated_apply_add(struct replmd_replicated_request *ar)
{
	struct ldb_context *ldb;
	struct ldb_request *change_req;
	enum ndr_err_code ndr_err;
	struct ldb_message *msg;
	struct replPropertyMetaDataBlob *md;
	struct ldb_val md_value;
	uint32_t i;
	uint64_t seq_num;
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

	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
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

	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNCreated", seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNChanged", seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	ret = replmd_notify(ar->module, msg->dn, seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	/* remove any message elements that have zero values */
	for (i=0; i<msg->num_elements; i++) {
		if (msg->elements[i].num_values == 0) {
			DEBUG(4,(__location__ ": Removing attribute %s with num_values==0\n",
				 msg->elements[i].name));
			memmove(&msg->elements[i], 
				&msg->elements[i+1], 
				sizeof(msg->elements[i])*(msg->num_elements - (i+1)));
			msg->num_elements--;
			i--;
		}
	}
	
	/*
	 * the meta data array is already sorted by the caller
	 */
	for (i=0; i < md->ctr.ctr1.count; i++) {
		md->ctr.ctr1.array[i].local_usn = seq_num;
	}
	ndr_err = ndr_push_struct_blob(&md_value, msg, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       md,
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
				replmd_replicated_apply_add_callback,
				ar->req);
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	return ldb_next_request(ar->module, change_req);
}

static int replmd_replPropertyMetaData1_conflict_compare(struct replPropertyMetaData1 *m1,
							 struct replPropertyMetaData1 *m2)
{
	int ret;

	if (m1->version != m2->version) {
		return m1->version - m2->version;
	}

	if (m1->originating_change_time != m2->originating_change_time) {
		return m1->originating_change_time - m2->originating_change_time;
	}

	ret = GUID_compare(&m1->originating_invocation_id, &m2->originating_invocation_id);
	if (ret != 0) {
		return ret;
	}

	return m1->originating_usn - m2->originating_usn;
}

static int replmd_replicated_apply_merge_callback(struct ldb_request *req,
						  struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct replmd_replicated_request *ar = talloc_get_type(req->context,
					       struct replmd_replicated_request);
	int ret;

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
	ar->index_current++;

	ret = replmd_replicated_apply_next(ar);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ar->req, NULL, NULL, ret);
	}

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
	uint32_t i,j,ni=0;
	uint32_t removed_attrs = 0;
	uint64_t seq_num;
	int ret;

	ldb = ldb_module_get_ctx(ar->module);
	msg = ar->objs->objects[ar->index_current].msg;
	rmd = ar->objs->objects[ar->index_current].meta_data;
	ZERO_STRUCT(omd);
	omd.version = 1;

	/*
	 * TODO: check repl data is correct after a rename
	 */
	if (ldb_dn_compare(msg->dn, ar->search_msg->dn) != 0) {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "replmd_replicated_request rename %s => %s\n",
			  ldb_dn_get_linearized(ar->search_msg->dn),
			  ldb_dn_get_linearized(msg->dn));
		if (ldb_rename(ldb, ar->search_msg->dn, msg->dn) != LDB_SUCCESS) {
			ldb_debug(ldb, LDB_DEBUG_FATAL, "replmd_replicated_request rename %s => %s failed - %s\n",
				  ldb_dn_get_linearized(ar->search_msg->dn),
				  ldb_dn_get_linearized(msg->dn),
				  ldb_errstring(ldb));
			return replmd_replicated_request_werror(ar, WERR_DS_DRA_DB_ERROR);
		}
	}

	/* find existing meta data */
	omd_value = ldb_msg_find_ldb_val(ar->search_msg, "replPropertyMetaData");
	if (omd_value) {
		ndr_err = ndr_pull_struct_blob(omd_value, ar,
					       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), &omd,
					       (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
			return replmd_replicated_request_werror(ar, ntstatus_to_werror(nt_status));
		}

		if (omd.version != 1) {
			return replmd_replicated_request_werror(ar, WERR_DS_DRA_INTERNAL_ERROR);
		}
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
			int cmp;

			if (rmd->ctr.ctr1.array[i].attid != nmd.ctr.ctr1.array[j].attid) {
				continue;
			}

			cmp = replmd_replPropertyMetaData1_conflict_compare(&rmd->ctr.ctr1.array[i],
									    &nmd.ctr.ctr1.array[j]);
			if (cmp > 0) {
				/* replace the entry */
				nmd.ctr.ctr1.array[j] = rmd->ctr.ctr1.array[i];
				found = true;
				break;
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

	ret = ldb_sequence_number(ldb, LDB_SEQ_NEXT, &seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
	}

	for (i=0; i<ni; i++) {
		nmd.ctr.ctr1.array[i].local_usn = seq_num;
	}

	/* create the meta data value */
	ndr_err = ndr_push_struct_blob(&nmd_value, msg, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &nmd,
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
	ret = samdb_msg_add_uint64(ldb, msg, msg, "uSNChanged", seq_num);
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

	ret = replmd_notify(ar->module, msg->dn, seq_num);
	if (ret != LDB_SUCCESS) {
		return replmd_replicated_request_error(ar, ret);
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
				replmd_replicated_apply_merge_callback,
				ar->req);
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
				   ar->objs->partition_dn,
				   LDB_SCOPE_SUBTREE,
				   filter,
				   NULL,
				   NULL,
				   ar,
				   replmd_replicated_apply_search_callback,
				   ar->req);

	ret = ldb_request_add_control(search_req, LDB_CONTROL_SHOW_DELETED_OID, true, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	

	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

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
	uint32_t i,j,ni=0;
	bool found = false;
	time_t t = time(NULL);
	NTTIME now;
	int ret;

	ldb = ldb_module_get_ctx(ar->module);
	ruv = ar->objs->uptodateness_vector;
	ZERO_STRUCT(ouv);
	ouv.version = 2;
	ZERO_STRUCT(nuv);
	nuv.version = 2;

	unix_to_nt_time(&now, t);

	/*
	 * first create the new replUpToDateVector
	 */
	ouv_value = ldb_msg_find_ldb_val(ar->search_msg, "replUpToDateVector");
	if (ouv_value) {
		ndr_err = ndr_pull_struct_blob(ouv_value, ar,
					       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), &ouv,
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
	qsort(nuv.ctr.ctr2.cursors, nuv.ctr.ctr2.count,
	      sizeof(struct drsuapi_DsReplicaCursor2),
	      (comparison_fn_t)drsuapi_DsReplicaCursor2_compare);

	/*
	 * create the change ldb_message
	 */
	msg = ldb_msg_new(ar);
	if (!msg) return replmd_replicated_request_werror(ar, WERR_NOMEM);
	msg->dn = ar->search_msg->dn;

	ndr_err = ndr_push_struct_blob(&nuv_value, msg, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
				       &nuv,
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
	/* and fix some values... */
	nrf.ctr.ctr1.consecutive_sync_failures		= 0;
	nrf.ctr.ctr1.last_success			= now;
	nrf.ctr.ctr1.last_attempt			= now;
	nrf.ctr.ctr1.result_last_attempt		= WERR_OK;
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

			ndr_err = ndr_pull_struct_blob(&orf_el->values[i], trf, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), trf,
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
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
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
	if (ret != LDB_SUCCESS) return replmd_replicated_request_error(ar, ret);

	return ldb_next_request(ar->module, search_req);
}



static int replmd_extended_replicated_objects(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct dsdb_extended_replicated_objects *objs;
	struct replmd_replicated_request *ar;
	struct ldb_control **ctrls;
	int ret, i;
	struct dsdb_control_current_partition *partition_ctrl;
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

	ar->objs = objs;
	ar->schema = dsdb_get_schema(ldb);
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

	ret = ldb_request_add_control(req, DSDB_CONTROL_REPLICATED_UPDATE_OID, false, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/*
	  add the DSDB_CONTROL_CURRENT_PARTITION_OID control. This
	  tells the partition module which partition this request is
	  directed at. That is important as the partition roots appear
	  twice in the directory, once as mount points in the top
	  level store, and once as the roots of each partition. The
	  replication code wants to operate on the root of the
	  partitions, not the top level mount points
	 */
	partition_ctrl = talloc(req, struct dsdb_control_current_partition);
	if (partition_ctrl == NULL) {
		if (!partition_ctrl) return replmd_replicated_request_werror(ar, WERR_NOMEM);
	}
	partition_ctrl->version = DSDB_CONTROL_CURRENT_PARTITION_VERSION;
	partition_ctrl->dn = objs->partition_dn;

	ret = ldb_request_add_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID, false, partition_ctrl);
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
					   struct la_entry *la_entry)
{					   
	struct drsuapi_DsReplicaLinkedAttribute *la = la_entry->la;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct drsuapi_DsReplicaObjectIdentifier3 target;
	struct ldb_message *msg;
	struct ldb_message_element *ret_el;
	TALLOC_CTX *tmp_ctx = talloc_new(la_entry);
	enum ndr_err_code ndr_err;
	struct ldb_request *mod_req;
	int ret;
	const struct dsdb_attribute *attr;
	struct ldb_dn *target_dn;
	uint64_t seq_num = 0;

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
        attid                    : DRSUAPI_ATTRIBUTE_member (0x1F)             
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
     &target: struct drsuapi_DsReplicaObjectIdentifier3                        
        __ndr_size               : 0x0000007e (126)                            
        __ndr_size_sid           : 0x0000001c (28)                             
        guid                     : 7639e594-db75-4086-b0d4-67890ae46031        
        sid                      : S-1-5-21-2848215498-2472035911-1947525656-19924
        __ndr_size_dn            : 0x00000022 (34)                                
        dn                       : 'CN=UOne,OU=TestOU,DC=vsofs8,DC=com'           
 */
	if (DEBUGLVL(4)) {
		NDR_PRINT_DEBUG(drsuapi_DsReplicaLinkedAttribute, la); 
	}
	
	/* decode the target of the link */
	ndr_err = ndr_pull_struct_blob(la->value.blob, 
				       tmp_ctx, lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
				       &target,
				       (ndr_pull_flags_fn_t)ndr_pull_drsuapi_DsReplicaObjectIdentifier3);
	if (ndr_err != NDR_ERR_SUCCESS) {
		DEBUG(0,("Unable to decode linked_attribute target\n"));
		dump_data(4, la->value.blob->data, la->value.blob->length);			
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	if (DEBUGLVL(4)) {
		NDR_PRINT_DEBUG(drsuapi_DsReplicaObjectIdentifier3, &target);
	}

	/* construct a modify request for this attribute change */
	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
		ldb_oom(ldb);
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = dsdb_find_dn_by_guid(ldb, tmp_ctx, 
				   GUID_string(tmp_ctx, &la->identifier->guid), &msg->dn);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	/* find the attribute being modified */
	attr = dsdb_attribute_by_attributeID_id(dsdb_get_schema(ldb), la->attid);
	if (attr == NULL) {
		DEBUG(0, (__location__ ": Unable to find attributeID 0x%x\n", la->attid));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (la->flags & DRSUAPI_DS_LINKED_ATTRIBUTE_FLAG_ACTIVE) {
		ret = ldb_msg_add_empty(msg, attr->lDAPDisplayName,
					LDB_FLAG_MOD_ADD, &ret_el);
	} else {
		ret = ldb_msg_add_empty(msg, attr->lDAPDisplayName,
					LDB_FLAG_MOD_DELETE, &ret_el);
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	/* we allocate two entries here, in case we need a remove/add
	   pair */
	ret_el->values = talloc_array(msg, struct ldb_val, 2);
	if (!ret_el->values) {
		ldb_oom(ldb);
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ret_el->num_values = 1;

	ret = dsdb_find_dn_by_guid(ldb, tmp_ctx, GUID_string(tmp_ctx, &target.guid), &target_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to map GUID %s to DN\n", GUID_string(tmp_ctx, &target.guid)));
		talloc_free(tmp_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret_el->values[0].data = (uint8_t *)ldb_dn_get_extended_linearized(tmp_ctx, target_dn, 1);
	ret_el->values[0].length = strlen((char *)ret_el->values[0].data);

	ret = replmd_update_rpmd(module, msg, &seq_num);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	/* we only change whenChanged and uSNChanged if the seq_num
	   has changed */
	if (seq_num != 0) {
		time_t t = time(NULL);

		if (add_time_element(msg, "whenChanged", t) != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (add_uint64_element(msg, "uSNChanged", seq_num) != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ret = ldb_build_mod_req(&mod_req, ldb, tmp_ctx,
				msg,
				NULL,
				NULL, 
				ldb_op_default_callback,
				NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}
	talloc_steal(mod_req, msg);

	if (DEBUGLVL(4)) {
		DEBUG(4,("Applying DRS linked attribute change:\n%s\n",
			 ldb_ldif_message_string(ldb, tmp_ctx, LDB_CHANGETYPE_MODIFY, msg)));
	}

	/* Run the new request */
	ret = ldb_next_request(module, mod_req);

	/* we need to wait for this to finish, as we are being called
	   from the synchronous end_transaction hook of this module */
	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(mod_req->handle, LDB_WAIT_ALL);
	}

	if (ret == LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS) {
		/* the link destination exists, we need to update it
		 * by deleting the old one for the same DN then adding
		 * the new one */
		msg->elements = talloc_realloc(msg, msg->elements,
					       struct ldb_message_element,
					       msg->num_elements+1);
		if (msg->elements == NULL) {
			ldb_oom(ldb);
			talloc_free(tmp_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		/* this relies on the backend matching the old entry
		   only by the DN portion of the extended DN */
		msg->elements[1] = msg->elements[0];
		msg->elements[0].flags = LDB_FLAG_MOD_DELETE;
		msg->num_elements++;

		ret = ldb_build_mod_req(&mod_req, ldb, tmp_ctx,
					msg,
					NULL,
					NULL, 
					ldb_op_default_callback,
					NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return ret;
		}

		/* Run the new request */
		ret = ldb_next_request(module, mod_req);
		
		if (ret == LDB_SUCCESS) {
			ret = ldb_wait(mod_req->handle, LDB_WAIT_ALL);
		}
	}

	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "Failed to apply linked attribute change '%s' %s\n",
			  ldb_errstring(ldb),
			  ldb_ldif_message_string(ldb, tmp_ctx, LDB_CHANGETYPE_MODIFY, msg));
		ret = LDB_SUCCESS;
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
	int i;
	struct replmd_private *replmd_private = talloc_get_type(ldb_module_get_private(module),
								struct replmd_private);
	talloc_free(replmd_private->la_ctx);
	replmd_private->la_list = NULL;
	replmd_private->la_ctx = NULL;

	for (i=0; i<replmd_private->num_ncs; i++) {
		replmd_private->ncs[i].mod_usn = 0;
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
	int ret;

	/* walk the list backwards, to do the first entry first, as we
	 * added the entries with DLIST_ADD() which puts them at the
	 * start of the list */
	for (la = replmd_private->la_list; la && la->next; la=la->next) ;

	for (; la; la=prev) {
		prev = la->prev;
		DLIST_REMOVE(replmd_private->la_list, la);
		ret = replmd_process_linked_attribute(module, la);
		if (ret != LDB_SUCCESS) {
			talloc_free(replmd_private->la_ctx);
			replmd_private->la_list = NULL;
			replmd_private->la_ctx = NULL;
			return ret;
		}
	}

	talloc_free(replmd_private->la_ctx);
	replmd_private->la_list = NULL;
	replmd_private->la_ctx = NULL;

	/* possibly change @REPLCHANGED */
	ret = replmd_notify_store(module);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	
	return ldb_next_prepare_commit(module);
}

static int replmd_del_transaction(struct ldb_module *module)
{
	struct replmd_private *replmd_private = 
		talloc_get_type(ldb_module_get_private(module), struct replmd_private);
	talloc_free(replmd_private->la_ctx);
	replmd_private->la_list = NULL;
	replmd_private->la_ctx = NULL;
	return ldb_next_del_trans(module);
}


_PUBLIC_ const struct ldb_module_ops ldb_repl_meta_data_module_ops = {
	.name          = "repl_meta_data",
	.init_context	   = replmd_init,
	.add               = replmd_add,
	.modify            = replmd_modify,
	.rename            = replmd_rename,
	.extended          = replmd_extended,
	.start_transaction = replmd_start_transaction,
	.prepare_commit    = replmd_prepare_commit,
	.del_transaction   = replmd_del_transaction,
};
