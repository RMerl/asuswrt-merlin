/* 
   Partitions ldb module

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
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
 *  Component: ldb partitions module
 *
 *  Description: Implement LDAP partitions
 *
 *  Author: Andrew Bartlett
 *  Author: Stefan Metzmacher
 */

#include "includes.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_errors.h"
#include "lib/ldb/include/ldb_module.h"
#include "lib/ldb/include/ldb_private.h"
#include "dsdb/samdb/samdb.h"

struct dsdb_partition {
	struct ldb_module *module;
	struct dsdb_control_current_partition *ctrl;
};

struct partition_private_data {
	struct dsdb_partition **partitions;
	struct ldb_dn **replicate;
};

struct part_request {
	struct ldb_module *module;
	struct ldb_request *req;
};

struct partition_context {
	struct ldb_module *module;
	struct ldb_request *req;
	bool got_success;

	struct part_request *part_req;
	int num_requests;
	int finished_requests;
};

static struct partition_context *partition_init_ctx(struct ldb_module *module, struct ldb_request *req)
{
	struct partition_context *ac;

	ac = talloc_zero(req, struct partition_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb_module_get_ctx(module), "Out of Memory");
		return NULL;
	}

	ac->module = module;
	ac->req = req;

	return ac;
}

#define PARTITION_FIND_OP_NOERROR(module, op) do { \
        while (module && module->ops->op == NULL) module = module->next; \
} while (0)

#define PARTITION_FIND_OP(module, op) do { \
	PARTITION_FIND_OP_NOERROR(module, op); \
        if (module == NULL) { \
                ldb_asprintf_errstring(ldb_module_get_ctx(module), \
			"Unable to find backend operation for " #op ); \
                return LDB_ERR_OPERATIONS_ERROR; \
        } \
} while (0)

/*
 *    helper functions to call the next module in chain
 *    */

static int partition_request(struct ldb_module *module, struct ldb_request *request)
{
	int ret;
	switch (request->operation) {
	case LDB_SEARCH:
		PARTITION_FIND_OP(module, search);
		ret = module->ops->search(module, request);
		break;
	case LDB_ADD:
		PARTITION_FIND_OP(module, add);
		ret = module->ops->add(module, request);
		break;
	case LDB_MODIFY:
		PARTITION_FIND_OP(module, modify);
		ret = module->ops->modify(module, request);
		break;
	case LDB_DELETE:
		PARTITION_FIND_OP(module, del);
		ret = module->ops->del(module, request);
		break;
	case LDB_RENAME:
		PARTITION_FIND_OP(module, rename);
		ret = module->ops->rename(module, request);
		break;
	case LDB_EXTENDED:
		PARTITION_FIND_OP(module, extended);
		ret = module->ops->extended(module, request);
		break;
	default:
		PARTITION_FIND_OP(module, request);
		ret = module->ops->request(module, request);
		break;
	}
	if (ret == LDB_SUCCESS) {
		return ret;
	}
	if (!ldb_errstring(ldb_module_get_ctx(module))) {
		/* Set a default error string, to place the blame somewhere */
		ldb_asprintf_errstring(ldb_module_get_ctx(module),
					"error in module %s: %s (%d)",
					module->ops->name,
					ldb_strerror(ret), ret);
	}
	return ret;
}

static struct dsdb_partition *find_partition(struct partition_private_data *data,
					     struct ldb_dn *dn,
					     struct ldb_request *req)
{
	int i;
	struct ldb_control *partition_ctrl;

	/* see if the request has the partition DN specified in a
	 * control. The repl_meta_data module can specify this to
	 * ensure that replication happens to the right partition
	 */
	partition_ctrl = ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID);
	if (partition_ctrl) {
		const struct dsdb_control_current_partition *partition;
		partition = talloc_get_type(partition_ctrl->data,
					    struct dsdb_control_current_partition);
		if (partition != NULL) {
			dn = partition->dn;
		}
	}

	if (dn == NULL) {
		return NULL;
	}

	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialisation) */
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		if (ldb_dn_compare_base(data->partitions[i]->ctrl->dn, dn) == 0) {
			return data->partitions[i];
		}
	}

	return NULL;
}

/**
 * fire the caller's callback for every entry, but only send 'done' once.
 */
static int partition_req_callback(struct ldb_request *req,
				  struct ldb_reply *ares)
{
	struct partition_context *ac;
	struct ldb_module *module;
	struct ldb_request *nreq;
	int ret, i;
	struct partition_private_data *data;

	ac = talloc_get_type(req->context, struct partition_context);
	data = talloc_get_type(ac->module->private_data, struct partition_private_data);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	if (ares->error != LDB_SUCCESS && !ac->got_success) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_REFERRAL:
		/* ignore referrals for now */
		break;

	case LDB_REPLY_ENTRY:
		if (ac->req->operation != LDB_SEARCH) {
			ldb_set_errstring(ldb_module_get_ctx(ac->module),
				"partition_req_callback:"
				" Unsupported reply type for this request");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		for (i=0; data && data->partitions && data->partitions[i]; i++) {
			if (ldb_dn_compare(ares->message->dn, data->partitions[i]->ctrl->dn) == 0) {
				struct ldb_control *part_control;
				/* this is a partition root message - make
				   sure it isn't one of our fake root
				   entries from a parent partition */
				part_control = ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID);
				if (part_control && part_control->data != data->partitions[i]->ctrl) {
					DEBUG(6,(__location__ ": Discarding partition mount object %s\n",
						 ldb_dn_get_linearized(ares->message->dn)));
					talloc_free(ares);
					return LDB_SUCCESS;
				}
			}
		}
		
		return ldb_module_send_entry(ac->req, ares->message, ares->controls);

	case LDB_REPLY_DONE:
		if (ares->error == LDB_SUCCESS) {
			ac->got_success = true;
		}
		if (ac->req->operation == LDB_EXTENDED) {
			/* FIXME: check for ares->response, replmd does not fill it ! */
			if (ares->response) {
				if (strcmp(ares->response->oid, LDB_EXTENDED_START_TLS_OID) != 0) {
					ldb_set_errstring(ldb_module_get_ctx(ac->module),
							  "partition_req_callback:"
							  " Unknown extended reply, "
							  "only supports START_TLS");
					talloc_free(ares);
					return ldb_module_done(ac->req, NULL, NULL,
								LDB_ERR_OPERATIONS_ERROR);
				}
			}
		}

		ac->finished_requests++;
		if (ac->finished_requests == ac->num_requests) {
			/* this was the last one, call callback */
			return ldb_module_done(ac->req, ares->controls,
					       ares->response, 
					       ac->got_success?LDB_SUCCESS:ares->error);
		}

		/* not the last, now call the next one */
		module = ac->part_req[ac->finished_requests].module;
		nreq = ac->part_req[ac->finished_requests].req;

		ret = partition_request(module, nreq);
		if (ret != LDB_SUCCESS) {
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}

		break;
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

static int partition_prep_request(struct partition_context *ac,
				  struct dsdb_partition *partition)
{
	int ret;
	struct ldb_request *req;

	ac->part_req = talloc_realloc(ac, ac->part_req,
					struct part_request,
					ac->num_requests + 1);
	if (ac->part_req == NULL) {
		ldb_oom(ldb_module_get_ctx(ac->module));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	switch (ac->req->operation) {
	case LDB_SEARCH:
		ret = ldb_build_search_req_ex(&req, ldb_module_get_ctx(ac->module),
					ac->part_req,
					ac->req->op.search.base,
					ac->req->op.search.scope,
					ac->req->op.search.tree,
					ac->req->op.search.attrs,
					ac->req->controls,
					ac, partition_req_callback,
					ac->req);
		break;
	case LDB_ADD:
		ret = ldb_build_add_req(&req, ldb_module_get_ctx(ac->module), ac->part_req,
					ac->req->op.add.message,
					ac->req->controls,
					ac, partition_req_callback,
					ac->req);
		break;
	case LDB_MODIFY:
		ret = ldb_build_mod_req(&req, ldb_module_get_ctx(ac->module), ac->part_req,
					ac->req->op.mod.message,
					ac->req->controls,
					ac, partition_req_callback,
					ac->req);
		break;
	case LDB_DELETE:
		ret = ldb_build_del_req(&req, ldb_module_get_ctx(ac->module), ac->part_req,
					ac->req->op.del.dn,
					ac->req->controls,
					ac, partition_req_callback,
					ac->req);
		break;
	case LDB_RENAME:
		ret = ldb_build_rename_req(&req, ldb_module_get_ctx(ac->module), ac->part_req,
					ac->req->op.rename.olddn,
					ac->req->op.rename.newdn,
					ac->req->controls,
					ac, partition_req_callback,
					ac->req);
		break;
	case LDB_EXTENDED:
		ret = ldb_build_extended_req(&req, ldb_module_get_ctx(ac->module),
					ac->part_req,
					ac->req->op.extended.oid,
					ac->req->op.extended.data,
					ac->req->controls,
					ac, partition_req_callback,
					ac->req);
		break;
	default:
		ldb_set_errstring(ldb_module_get_ctx(ac->module),
				  "Unsupported request type!");
		ret = LDB_ERR_UNWILLING_TO_PERFORM;
	}

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ac->part_req[ac->num_requests].req = req;

	if (ac->req->controls) {
		req->controls = talloc_memdup(req, ac->req->controls,
					talloc_get_size(ac->req->controls));
		if (req->controls == NULL) {
			ldb_oom(ldb_module_get_ctx(ac->module));
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	if (partition) {
		ac->part_req[ac->num_requests].module = partition->module;

		if (!ldb_request_get_control(req, DSDB_CONTROL_CURRENT_PARTITION_OID)) {
			ret = ldb_request_add_control(req,
						      DSDB_CONTROL_CURRENT_PARTITION_OID,
						      false, partition->ctrl);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}

		if (req->operation == LDB_SEARCH) {
			/* If the search is for 'more' than this partition,
			 * then change the basedn, so a remote LDAP server
			 * doesn't object */
			if (ldb_dn_compare_base(partition->ctrl->dn,
						req->op.search.base) != 0) {
				req->op.search.base = partition->ctrl->dn;
			}
		}

	} else {
		/* make sure you put the NEXT module here, or
		 * partition_request() will simply loop forever on itself */
		ac->part_req[ac->num_requests].module = ac->module->next;
	}

	ac->num_requests++;

	return LDB_SUCCESS;
}

static int partition_call_first(struct partition_context *ac)
{
	return partition_request(ac->part_req[0].module, ac->part_req[0].req);
}

/**
 * Send a request down to all the partitions
 */
static int partition_send_all(struct ldb_module *module, 
			      struct partition_context *ac, 
			      struct ldb_request *req) 
{
	int i;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	int ret = partition_prep_request(ac, NULL);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		ret = partition_prep_request(ac, data->partitions[i]);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	/* fire the first one */
	return partition_call_first(ac);
}

/**
 * Figure out which backend a request needs to be aimed at.  Some
 * requests must be replicated to all backends
 */
static int partition_replicate(struct ldb_module *module, struct ldb_request *req, struct ldb_dn *dn) 
{
	struct partition_context *ac;
	unsigned i;
	int ret;
	struct dsdb_partition *partition;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	if (!data || !data->partitions) {
		return ldb_next_request(module, req);
	}
	
	if (req->operation != LDB_SEARCH) {
		/* Is this a special DN, we need to replicate to every backend? */
		for (i=0; data->replicate && data->replicate[i]; i++) {
			if (ldb_dn_compare(data->replicate[i], 
					   dn) == 0) {
				
				ac = partition_init_ctx(module, req);
				if (!ac) {
					return LDB_ERR_OPERATIONS_ERROR;
				}
				
				return partition_send_all(module, ac, req);
			}
		}
	}

	/* Otherwise, we need to find the partition to fire it to */

	/* Find partition */
	partition = find_partition(data, dn, req);
	if (!partition) {
		/*
		 * if we haven't found a matching partition
		 * pass the request to the main ldb
		 *
		 * TODO: we should maybe return an error here
		 *       if it's not a special dn
		 */

		return ldb_next_request(module, req);
	}

	ac = partition_init_ctx(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we need to add a control but we never touch the original request */
	ret = partition_prep_request(ac, partition);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* fire the first one */
	return partition_call_first(ac);
}

/* search */
static int partition_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_control **saved_controls;
	/* Find backend */
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);

	/* issue request */

	/* (later) consider if we should be searching multiple
	 * partitions (for 'invisible' partition behaviour */

	struct ldb_control *search_control = ldb_request_get_control(req, LDB_CONTROL_SEARCH_OPTIONS_OID);
	struct ldb_control *domain_scope_control = ldb_request_get_control(req, LDB_CONTROL_DOMAIN_SCOPE_OID);
	
	struct ldb_search_options_control *search_options = NULL;
	struct dsdb_partition *p;
	
	p = find_partition(data, NULL, req);
	if (p != NULL) {
		/* the caller specified what partition they want the
		 * search - just pass it on
		 */
		return ldb_next_request(p->module, req);		
	}


	if (search_control) {
		search_options = talloc_get_type(search_control->data, struct ldb_search_options_control);
	}

	/* Remove the domain_scope control, so we don't confuse a backend server */
	if (domain_scope_control && !save_controls(domain_scope_control, req, &saved_controls)) {
		ldb_oom(ldb_module_get_ctx(module));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/*
	 * for now pass down the LDB_CONTROL_SEARCH_OPTIONS_OID control
	 * down as uncritical to make windows 2008 dcpromo happy.
	 */
	if (search_control) {
		search_control->critical = 0;
	}

	/* TODO:
	   Generate referrals (look for a partition under this DN) if we don't have the above control specified
	*/
	
	if (search_options && (search_options->search_options & LDB_SEARCH_OPTION_PHANTOM_ROOT)) {
		int ret, i;
		struct partition_context *ac;
		if ((search_options->search_options & ~LDB_SEARCH_OPTION_PHANTOM_ROOT) == 0) {
			/* We have processed this flag, so we are done with this control now */

			/* Remove search control, so we don't confuse a backend server */
			if (search_control && !save_controls(search_control, req, &saved_controls)) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
		}
		ac = partition_init_ctx(module, req);
		if (!ac) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* Search from the base DN */
		if (!req->op.search.base || ldb_dn_is_null(req->op.search.base)) {
			return partition_send_all(module, ac, req);
		}
		for (i=0; data && data->partitions && data->partitions[i]; i++) {
			bool match = false, stop = false;
			/* Find all partitions under the search base 
			   
			   we match if:

			      1) the DN we are looking for exactly matches the partition
		             or
			      2) the DN we are looking for is a parent of the partition and it isn't
                                 a scope base search
                             or
			      3) the DN we are looking for is a child of the partition
			 */
			if (ldb_dn_compare(data->partitions[i]->ctrl->dn, req->op.search.base) == 0) {
				match = true;
				if (req->op.search.scope == LDB_SCOPE_BASE) {
					stop = true;
				}
			}
			if (!match && 
			    (ldb_dn_compare_base(req->op.search.base, data->partitions[i]->ctrl->dn) == 0 &&
			     req->op.search.scope != LDB_SCOPE_BASE)) {
				match = true;
			}
			if (!match &&
			    ldb_dn_compare_base(data->partitions[i]->ctrl->dn, req->op.search.base) == 0) {
				match = true;
				stop = true; /* note that this relies on partition ordering */
			}
			if (match) {
				ret = partition_prep_request(ac, data->partitions[i]);
				if (ret != LDB_SUCCESS) {
					return ret;
				}
			}
			if (stop) break;
		}

		/* Perhaps we didn't match any partitions.  Try the main partition, only */
		if (ac->num_requests == 0) {
			talloc_free(ac);
			return ldb_next_request(module, req);
		}

		/* fire the first one */
		return partition_call_first(ac);

	} else {
		/* Handle this like all other requests */
		if (search_control && (search_options->search_options & ~LDB_SEARCH_OPTION_PHANTOM_ROOT) == 0) {
			/* We have processed this flag, so we are done with this control now */

			/* Remove search control, so we don't confuse a backend server */
			if (search_control && !save_controls(search_control, req, &saved_controls)) {
				ldb_oom(ldb_module_get_ctx(module));
				return LDB_ERR_OPERATIONS_ERROR;
			}
		}

		return partition_replicate(module, req, req->op.search.base);
	}
}

/* add */
static int partition_add(struct ldb_module *module, struct ldb_request *req)
{
	return partition_replicate(module, req, req->op.add.message->dn);
}

/* modify */
static int partition_modify(struct ldb_module *module, struct ldb_request *req)
{
	return partition_replicate(module, req, req->op.mod.message->dn);
}

/* delete */
static int partition_delete(struct ldb_module *module, struct ldb_request *req)
{
	return partition_replicate(module, req, req->op.del.dn);
}

/* rename */
static int partition_rename(struct ldb_module *module, struct ldb_request *req)
{
	/* Find backend */
	struct dsdb_partition *backend, *backend2;
	
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);

	/* Skip the lot if 'data' isn't here yet (initialisation) */
	if (!data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	backend = find_partition(data, req->op.rename.olddn, req);
	backend2 = find_partition(data, req->op.rename.newdn, req);

	if ((backend && !backend2) || (!backend && backend2)) {
		return LDB_ERR_AFFECTS_MULTIPLE_DSAS;
	}

	if (backend != backend2) {
		ldb_asprintf_errstring(ldb_module_get_ctx(module), 
				       "Cannot rename from %s in %s to %s in %s: %s",
				       ldb_dn_get_linearized(req->op.rename.olddn),
				       ldb_dn_get_linearized(backend->ctrl->dn),
				       ldb_dn_get_linearized(req->op.rename.newdn),
				       ldb_dn_get_linearized(backend2->ctrl->dn),
				       ldb_strerror(LDB_ERR_AFFECTS_MULTIPLE_DSAS));
		return LDB_ERR_AFFECTS_MULTIPLE_DSAS;
	}

	return partition_replicate(module, req, req->op.rename.olddn);
}

/* start a transaction */
static int partition_start_trans(struct ldb_module *module)
{
	int i, ret;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	/* Look at base DN */
	/* Figure out which partition it is under */
	/* Skip the lot if 'data' isn't here yet (initialization) */
	ret = ldb_next_start_trans(module);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next = data->partitions[i]->module;
		PARTITION_FIND_OP(next, start_transaction);

		ret = next->ops->start_transaction(next);
		if (ret != LDB_SUCCESS) {
			/* Back it out, if it fails on one */
			for (i--; i >= 0; i--) {
				next = data->partitions[i]->module;
				PARTITION_FIND_OP(next, del_transaction);

				next->ops->del_transaction(next);
			}
			ldb_next_del_trans(module);
			return ret;
		}
	}
	return LDB_SUCCESS;
}

/* prepare for a commit */
static int partition_prepare_commit(struct ldb_module *module)
{
	int i;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);

	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next_prepare = data->partitions[i]->module;
		int ret;

		PARTITION_FIND_OP_NOERROR(next_prepare, prepare_commit);
		if (next_prepare == NULL) {
			continue;
		}

		ret = next_prepare->ops->prepare_commit(next_prepare);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return ldb_next_prepare_commit(module);
}


/* end a transaction */
static int partition_end_trans(struct ldb_module *module)
{
	int i;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next_end = data->partitions[i]->module;
		int ret;

		PARTITION_FIND_OP(next_end, end_transaction);

		ret = next_end->ops->end_transaction(next_end);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return ldb_next_end_trans(module);
}

/* delete a transaction */
static int partition_del_trans(struct ldb_module *module)
{
	int i, ret, final_ret = LDB_SUCCESS;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	for (i=0; data && data->partitions && data->partitions[i]; i++) {
		struct ldb_module *next = data->partitions[i]->module;
		PARTITION_FIND_OP(next, del_transaction);

		ret = next->ops->del_transaction(next);
		if (ret != LDB_SUCCESS) {
			final_ret = ret;
		}
	}	

	ret = ldb_next_del_trans(module);
	if (ret != LDB_SUCCESS) {
		final_ret = ret;
	}
	return final_ret;
}


/* FIXME: This function is still semi-async */
static int partition_sequence_number(struct ldb_module *module, struct ldb_request *req)
{
	int i, ret;
	uint64_t seq_number = 0;
	uint64_t timestamp_sequence = 0;
	uint64_t timestamp = 0;
	struct partition_private_data *data = talloc_get_type(module->private_data, 
							      struct partition_private_data);
	struct ldb_seqnum_request *seq;
	struct ldb_seqnum_result *seqr;
	struct ldb_request *treq;
	struct ldb_seqnum_request *tseq;
	struct ldb_seqnum_result *tseqr;
	struct ldb_extended *ext;
	struct ldb_result *res;
	struct dsdb_partition *p;

	p = find_partition(data, NULL, req);
	if (p != NULL) {
		/* the caller specified what partition they want the
		 * sequence number operation on - just pass it on
		 */
		return ldb_next_request(p->module, req);		
	}

	seq = talloc_get_type(req->op.extended.data, struct ldb_seqnum_request);

	switch (seq->type) {
	case LDB_SEQ_NEXT:
	case LDB_SEQ_HIGHEST_SEQ:
		res = talloc_zero(req, struct ldb_result);
		if (res == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		tseq = talloc_zero(res, struct ldb_seqnum_request);
		if (tseq == NULL) {
			talloc_free(res);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		tseq->type = seq->type;

		ret = ldb_build_extended_req(&treq, ldb_module_get_ctx(module), res,
					     LDB_EXTENDED_SEQUENCE_NUMBER,
					     tseq,
					     NULL,
					     res,
					     ldb_extended_default_callback,
					     NULL);
	       	ret = ldb_next_request(module, treq);
		if (ret == LDB_SUCCESS) {
			ret = ldb_wait(treq->handle, LDB_WAIT_ALL);
		}
		if (ret != LDB_SUCCESS) {
			talloc_free(res);
			return ret;
		}
		seqr = talloc_get_type(res->extended->data,
					struct ldb_seqnum_result);
		if (seqr->flags & LDB_SEQ_TIMESTAMP_SEQUENCE) {
			timestamp_sequence = seqr->seq_num;
		} else {
			seq_number += seqr->seq_num;
		}
		talloc_free(res);

		/* Skip the lot if 'data' isn't here yet (initialisation) */
		for (i=0; data && data->partitions && data->partitions[i]; i++) {

			res = talloc_zero(req, struct ldb_result);
			if (res == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			tseq = talloc_zero(res, struct ldb_seqnum_request);
			if (tseq == NULL) {
				talloc_free(res);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			tseq->type = seq->type;

			ret = ldb_build_extended_req(&treq, ldb_module_get_ctx(module), res,
						     LDB_EXTENDED_SEQUENCE_NUMBER,
						     tseq,
						     NULL,
						     res,
						     ldb_extended_default_callback,
						     NULL);
			if (ret != LDB_SUCCESS) {
				talloc_free(res);
				return ret;
			}

			if (!ldb_request_get_control(treq, DSDB_CONTROL_CURRENT_PARTITION_OID)) {
				ret = ldb_request_add_control(treq,
							      DSDB_CONTROL_CURRENT_PARTITION_OID,
							      false, data->partitions[i]->ctrl);
				if (ret != LDB_SUCCESS) {
					talloc_free(res);
					return ret;
				}
			}

			ret = partition_request(data->partitions[i]->module, treq);
			if (ret != LDB_SUCCESS) {
				talloc_free(res);
				return ret;
			}
			ret = ldb_wait(treq->handle, LDB_WAIT_ALL);
			if (ret != LDB_SUCCESS) {
				talloc_free(res);
				return ret;
			}
			tseqr = talloc_get_type(res->extended->data,
						struct ldb_seqnum_result);
			if (tseqr->flags & LDB_SEQ_TIMESTAMP_SEQUENCE) {
				timestamp_sequence = MAX(timestamp_sequence,
							 tseqr->seq_num);
			} else {
				seq_number += tseqr->seq_num;
			}
			talloc_free(res);
		}
		/* fall through */
	case LDB_SEQ_HIGHEST_TIMESTAMP:

		res = talloc_zero(req, struct ldb_result);
		if (res == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		tseq = talloc_zero(res, struct ldb_seqnum_request);
		if (tseq == NULL) {
			talloc_free(res);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		tseq->type = LDB_SEQ_HIGHEST_TIMESTAMP;

		ret = ldb_build_extended_req(&treq, ldb_module_get_ctx(module), res,
					     LDB_EXTENDED_SEQUENCE_NUMBER,
					     tseq,
					     NULL,
					     res,
					     ldb_extended_default_callback,
					     NULL);
		if (ret != LDB_SUCCESS) {
			talloc_free(res);
			return ret;
		}

		ret = ldb_next_request(module, treq);
		if (ret != LDB_SUCCESS) {
			talloc_free(res);
			return ret;
		}
		ret = ldb_wait(treq->handle, LDB_WAIT_ALL);
		if (ret != LDB_SUCCESS) {
			talloc_free(res);
			return ret;
		}

		tseqr = talloc_get_type(res->extended->data,
					   struct ldb_seqnum_result);
		timestamp = tseqr->seq_num;

		talloc_free(res);

		/* Skip the lot if 'data' isn't here yet (initialisation) */
		for (i=0; data && data->partitions && data->partitions[i]; i++) {

			res = talloc_zero(req, struct ldb_result);
			if (res == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}

			tseq = talloc_zero(res, struct ldb_seqnum_request);
			if (tseq == NULL) {
				talloc_free(res);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			tseq->type = LDB_SEQ_HIGHEST_TIMESTAMP;

			ret = ldb_build_extended_req(&treq, ldb_module_get_ctx(module), res,
						     LDB_EXTENDED_SEQUENCE_NUMBER,
						     tseq,
						     NULL,
						     res,
						     ldb_extended_default_callback,
						     NULL);
			if (ret != LDB_SUCCESS) {
				talloc_free(res);
				return ret;
			}

			if (!ldb_request_get_control(treq, DSDB_CONTROL_CURRENT_PARTITION_OID)) {
				ret = ldb_request_add_control(treq,
							      DSDB_CONTROL_CURRENT_PARTITION_OID,
							      false, data->partitions[i]->ctrl);
				if (ret != LDB_SUCCESS) {
					talloc_free(res);
					return ret;
				}
			}

			ret = partition_request(data->partitions[i]->module, treq);
			if (ret != LDB_SUCCESS) {
				talloc_free(res);
				return ret;
			}
			ret = ldb_wait(treq->handle, LDB_WAIT_ALL);
			if (ret != LDB_SUCCESS) {
				talloc_free(res);
				return ret;
			}

			tseqr = talloc_get_type(res->extended->data,
						  struct ldb_seqnum_result);
			timestamp = MAX(timestamp, tseqr->seq_num);

			talloc_free(res);
		}

		break;
	}

	ext = talloc_zero(req, struct ldb_extended);
	if (!ext) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	seqr = talloc_zero(ext, struct ldb_seqnum_result);
	if (seqr == NULL) {
		talloc_free(ext);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ext->oid = LDB_EXTENDED_SEQUENCE_NUMBER;
	ext->data = seqr;

	switch (seq->type) {
	case LDB_SEQ_NEXT:
	case LDB_SEQ_HIGHEST_SEQ:

		/* Has someone above set a timebase sequence? */
		if (timestamp_sequence) {
			seqr->seq_num = (((unsigned long long)timestamp << 24) | (seq_number & 0xFFFFFF));
		} else {
			seqr->seq_num = seq_number;
		}

		if (timestamp_sequence > seqr->seq_num) {
			seqr->seq_num = timestamp_sequence;
			seqr->flags |= LDB_SEQ_TIMESTAMP_SEQUENCE;
		}

		seqr->flags |= LDB_SEQ_GLOBAL_SEQUENCE;
		break;
	case LDB_SEQ_HIGHEST_TIMESTAMP:
		seqr->seq_num = timestamp;
		break;
	}

	if (seq->type == LDB_SEQ_NEXT) {
		seqr->seq_num++;
	}

	/* send request done */
	return ldb_module_done(req, NULL, ext, LDB_SUCCESS);
}

static int partition_extended_schema_update_now(struct ldb_module *module, struct ldb_request *req)
{
	struct dsdb_partition *partition;
	struct partition_private_data *data;
	struct ldb_dn *schema_dn;
	struct partition_context *ac;
	int ret;

	schema_dn = talloc_get_type(req->op.extended.data, struct ldb_dn);
	if (!schema_dn) {
		ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_FATAL, "partition_extended: invalid extended data\n");
		return LDB_ERR_PROTOCOL_ERROR;
	}

	data = talloc_get_type(module->private_data, struct partition_private_data);
	if (!data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	
	partition = find_partition( data, schema_dn, req);
	if (!partition) {
		return ldb_next_request(module, req);
	}

	ac = partition_init_ctx(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we need to add a control but we never touch the original request */
	ret = partition_prep_request(ac, partition);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	/* fire the first one */
	ret =  partition_call_first(ac);

	if (ret != LDB_SUCCESS){
		return ret;
	}

	return ldb_request_done(req, ret);
}


/* extended */
static int partition_extended(struct ldb_module *module, struct ldb_request *req)
{
	struct partition_private_data *data;
	struct partition_context *ac;

	data = talloc_get_type(module->private_data, struct partition_private_data);
	if (!data || !data->partitions) {
		return ldb_next_request(module, req);
	}

	if (strcmp(req->op.extended.oid, LDB_EXTENDED_SEQUENCE_NUMBER) == 0) {
		return partition_sequence_number(module, req);
	}

	/* forward schemaUpdateNow operation to schema_fsmo module*/
	if (strcmp(req->op.extended.oid, DSDB_EXTENDED_SCHEMA_UPDATE_NOW_OID) == 0) {
		return partition_extended_schema_update_now( module, req );
	}	

	/* 
	 * as the extended operation has no dn
	 * we need to send it to all partitions
	 */

	ac = partition_init_ctx(module, req);
	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return partition_send_all(module, ac, req);
}

static int partition_sort_compare(const void *v1, const void *v2)
{
	const struct dsdb_partition *p1;
	const struct dsdb_partition *p2;

	p1 = *((struct dsdb_partition * const*)v1);
	p2 = *((struct dsdb_partition * const*)v2);

	return ldb_dn_compare(p1->ctrl->dn, p2->ctrl->dn);
}

static int partition_init(struct ldb_module *module)
{
	int ret, i;
	TALLOC_CTX *mem_ctx = talloc_new(module);
	const char *attrs[] = { "partition", "replicateEntries", "modules", NULL };
	struct ldb_result *res;
	struct ldb_message *msg;
	struct ldb_message_element *partition_attributes;
	struct ldb_message_element *replicate_attributes;
	struct ldb_message_element *modules_attributes;

	struct partition_private_data *data;

	if (!mem_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data = talloc(mem_ctx, struct partition_private_data);
	if (data == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_search(ldb_module_get_ctx(module), mem_ctx, &res,
			 ldb_dn_new(mem_ctx, ldb_module_get_ctx(module), "@PARTITION"),
			 LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(mem_ctx);
		return ret;
	}
	if (res->count == 0) {
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	}

	if (res->count > 1) {
		talloc_free(mem_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	msg = res->msgs[0];

	partition_attributes = ldb_msg_find_element(msg, "partition");
	if (!partition_attributes) {
		ldb_set_errstring(ldb_module_get_ctx(module), "partition_init: no partitions specified");
		talloc_free(mem_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	data->partitions = talloc_array(data, struct dsdb_partition *, partition_attributes->num_values + 1);
	if (!data->partitions) {
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	for (i=0; i < partition_attributes->num_values; i++) {
		char *base = talloc_strdup(data->partitions, (char *)partition_attributes->values[i].data);
		char *p = strchr(base, ':');
		const char *backend;

		if (!p) {
			ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						"partition_init: "
						"invalid form for partition record (missing ':'): %s", base);
			talloc_free(mem_ctx);
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		p[0] = '\0';
		p++;
		if (!p[0]) {
			ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						"partition_init: "
						"invalid form for partition record (missing backend database): %s", base);
			talloc_free(mem_ctx);
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		data->partitions[i] = talloc(data->partitions, struct dsdb_partition);
		if (!data->partitions[i]) {
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		data->partitions[i]->ctrl = talloc(data->partitions[i], struct dsdb_control_current_partition);
		if (!data->partitions[i]->ctrl) {
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		data->partitions[i]->ctrl->version = DSDB_CONTROL_CURRENT_PARTITION_VERSION;
		data->partitions[i]->ctrl->dn = ldb_dn_new(data->partitions[i], ldb_module_get_ctx(module), base);
		if (!data->partitions[i]->ctrl->dn) {
			ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						"partition_init: invalid DN in partition record: %s", base);
			talloc_free(mem_ctx);
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		backend = samdb_relative_path(ldb_module_get_ctx(module), 
								   data->partitions[i], 
								   p);
		if (!backend) {
			ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						"partition_init: unable to determine an relative path for partition: %s", base);
			talloc_free(mem_ctx);			
		}
		ret = ldb_connect_backend(ldb_module_get_ctx(module), backend, NULL, &data->partitions[i]->module);
		if (ret != LDB_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}
	}
	data->partitions[i] = NULL;

	/* sort these into order, most to least specific */
	qsort(data->partitions, partition_attributes->num_values,
	      sizeof(*data->partitions), partition_sort_compare);

	for (i=0; data->partitions[i]; i++) {
		struct ldb_request *req;
		req = talloc_zero(mem_ctx, struct ldb_request);
		if (req == NULL) {
			ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_ERROR, "partition: Out of memory!\n");
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		req->operation = LDB_REQ_REGISTER_PARTITION;
		req->op.reg_partition.dn = data->partitions[i]->ctrl->dn;
		req->callback = ldb_op_default_callback;

		ldb_set_timeout(ldb_module_get_ctx(module), req, 0);

		req->handle = ldb_handle_new(req, ldb_module_get_ctx(module));
		if (req->handle == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		
		ret = ldb_request(ldb_module_get_ctx(module), req);
		if (ret == LDB_SUCCESS) {
			ret = ldb_wait(req->handle, LDB_WAIT_ALL);
		}
		if (ret != LDB_SUCCESS) {
			ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_ERROR, "partition: Unable to register partition with rootdse!\n");
			talloc_free(mem_ctx);
			return LDB_ERR_OTHER;
		}
		talloc_free(req);
	}

	replicate_attributes = ldb_msg_find_element(msg, "replicateEntries");
	if (!replicate_attributes) {
		data->replicate = NULL;
	} else {
		data->replicate = talloc_array(data, struct ldb_dn *, replicate_attributes->num_values + 1);
		if (!data->replicate) {
			talloc_free(mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		for (i=0; i < replicate_attributes->num_values; i++) {
			data->replicate[i] = ldb_dn_from_ldb_val(data->replicate, ldb_module_get_ctx(module), &replicate_attributes->values[i]);
			if (!ldb_dn_validate(data->replicate[i])) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
							"partition_init: "
							"invalid DN in partition replicate record: %s", 
							replicate_attributes->values[i].data);
				talloc_free(mem_ctx);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
		}
		data->replicate[i] = NULL;
	}

	/* Make the private data available to any searches the modules may trigger in initialisation */
	module->private_data = data;
	talloc_steal(module, data);
	
	modules_attributes = ldb_msg_find_element(msg, "modules");
	if (modules_attributes) {
		for (i=0; i < modules_attributes->num_values; i++) {
			struct ldb_dn *base_dn;
			int partition_idx;
			struct dsdb_partition *partition = NULL;
			const char **modules = NULL;

			char *base = talloc_strdup(data->partitions, (char *)modules_attributes->values[i].data);
			char *p = strchr(base, ':');
			if (!p) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
							"partition_init: "
							"invalid form for partition module record (missing ':'): %s", base);
				talloc_free(mem_ctx);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
			p[0] = '\0';
			p++;
			if (!p[0]) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
							"partition_init: "
							"invalid form for partition module record (missing backend database): %s", base);
				talloc_free(mem_ctx);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}

			modules = ldb_modules_list_from_string(ldb_module_get_ctx(module), mem_ctx,
							       p);
			
			base_dn = ldb_dn_new(mem_ctx, ldb_module_get_ctx(module), base);
			if (!ldb_dn_validate(base_dn)) {
				talloc_free(mem_ctx);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			
			for (partition_idx = 0; data->partitions[partition_idx]; partition_idx++) {
				if (ldb_dn_compare(data->partitions[partition_idx]->ctrl->dn, base_dn) == 0) {
					partition = data->partitions[partition_idx];
					break;
				}
			}
			
			if (!partition) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
							"partition_init: "
							"invalid form for partition module record (no such partition): %s", base);
				talloc_free(mem_ctx);
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
			
			ret = ldb_load_modules_list(ldb_module_get_ctx(module), modules, partition->module, &partition->module);
			if (ret != LDB_SUCCESS) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						       "partition_init: "
						       "loading backend for %s failed: %s", 
						       base, ldb_errstring(ldb_module_get_ctx(module)));
				talloc_free(mem_ctx);
				return ret;
			}
			ret = ldb_init_module_chain(ldb_module_get_ctx(module), partition->module);
			if (ret != LDB_SUCCESS) {
				ldb_asprintf_errstring(ldb_module_get_ctx(module), 
						       "partition_init: "
						       "initialising backend for %s failed: %s", 
						       base, ldb_errstring(ldb_module_get_ctx(module)));
				talloc_free(mem_ctx);
				return ret;
			}
		}
	}

	ret = ldb_mod_register_control(module, LDB_CONTROL_DOMAIN_SCOPE_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_ERROR,
			"partition: Unable to register control with rootdse!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_mod_register_control(module, LDB_CONTROL_SEARCH_OPTIONS_OID);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb_module_get_ctx(module), LDB_DEBUG_ERROR,
			"partition: Unable to register control with rootdse!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	talloc_free(mem_ctx);
	return ldb_next_init(module);
}

_PUBLIC_ const struct ldb_module_ops ldb_partition_module_ops = {
	.name		   = "partition",
	.init_context	   = partition_init,
	.search            = partition_search,
	.add               = partition_add,
	.modify            = partition_modify,
	.del               = partition_delete,
	.rename            = partition_rename,
	.extended          = partition_extended,
	.start_transaction = partition_start_trans,
	.prepare_commit    = partition_prepare_commit,
	.end_transaction   = partition_end_trans,
	.del_transaction   = partition_del_trans,
};
