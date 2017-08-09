/*
   ldb database mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006
   Copyright (C) Simo Sorce <idra@samba.org> 2008

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

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_map.h"
#include "ldb_map_private.h"


/* Mapping message elements
 * ======================== */

/* Map a message element into the remote partition. */
static struct ldb_message_element *ldb_msg_el_map_local(struct ldb_module *module, void *mem_ctx, const struct ldb_map_attribute *map, const struct ldb_message_element *old)
{
	struct ldb_message_element *el;
	unsigned int i;

	el = talloc_zero(mem_ctx, struct ldb_message_element);
	if (el == NULL) {
		map_oom(module);
		return NULL;
	}

	el->num_values = old->num_values;
	el->values = talloc_array(el, struct ldb_val, el->num_values);
	if (el->values == NULL) {
		talloc_free(el);
		map_oom(module);
		return NULL;
	}

	el->name = map_attr_map_local(el, map, old->name);

	for (i = 0; i < el->num_values; i++) {
		el->values[i] = ldb_val_map_local(module, el->values, map, &old->values[i]);
	}

	return el;
}

/* Add a message element either to a local or to a remote message,
 * depending on whether it goes into the local or remote partition. */
static int ldb_msg_el_partition(struct ldb_module *module, struct ldb_message *local, struct ldb_message *remote, const struct ldb_message *msg, const char *attr_name, /* const char * const names[], */ const struct ldb_message_element *old)
{
	const struct ldb_map_context *data = map_get_context(module);
	const struct ldb_map_attribute *map = map_attr_find_local(data, attr_name);
	struct ldb_message_element *el=NULL;
	struct ldb_context *ldb = ldb_module_get_ctx(module);

	/* Unknown attribute: ignore */
	if (map == NULL) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb_map: "
			  "Not mapping attribute '%s': no mapping found",
			  old->name);
		goto local;
	}

	switch (map->type) {
	case LDB_MAP_IGNORE:
		goto local;

	case LDB_MAP_CONVERT:
		if (map->u.convert.convert_local == NULL) {
			ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb_map: "
				  "Not mapping attribute '%s': "
				  "'convert_local' not set",
				  map->local_name);
			goto local;
		}
		/* fall through */
	case LDB_MAP_KEEP:
	case LDB_MAP_RENAME:
		el = ldb_msg_el_map_local(module, remote, map, old);
		break;

	case LDB_MAP_GENERATE:
		if (map->u.generate.generate_remote == NULL) {
			ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb_map: "
				  "Not mapping attribute '%s': "
				  "'generate_remote' not set",
				  map->local_name);
			goto local;
		}

		/* TODO: if this attr requires context:
		 *	 make sure all context attrs are mappable (in 'names')
		 *	 make sure all context attrs have already been mapped?
		 *	 maybe postpone generation until they have been mapped?
		 */

		map->u.generate.generate_remote(module, map->local_name, msg, remote, local);
		return 0;
	}

	if (el == NULL) {
		return -1;
	}

	return ldb_msg_add(remote, el, old->flags);

local:
	el = talloc(local, struct ldb_message_element);
	if (el == NULL) {
		map_oom(module);
		return -1;
	}

	*el = *old;			/* copy the old element */

	return ldb_msg_add(local, el, old->flags);
}

/* Mapping messages
 * ================ */

/* Check whether a message will be (partially) mapped into the remote partition. */
static bool ldb_msg_check_remote(struct ldb_module *module, const struct ldb_message *msg)
{
	const struct ldb_map_context *data = map_get_context(module);
	bool ret;
	unsigned int i;

	for (i = 0; i < msg->num_elements; i++) {
		ret = map_attr_check_remote(data, msg->elements[i].name);
		if (ret) {
			return ret;
		}
	}

	return false;
}

/* Split message elements that stay in the local partition from those
 * that are mapped into the remote partition. */
static int ldb_msg_partition(struct ldb_module *module, struct ldb_message *local, struct ldb_message *remote, const struct ldb_message *msg)
{
	/* const char * const names[]; */
	struct ldb_context *ldb;
	unsigned int i;
	int ret;

	ldb = ldb_module_get_ctx(module);

	for (i = 0; i < msg->num_elements; i++) {
		/* Skip 'IS_MAPPED' */
		if (ldb_attr_cmp(msg->elements[i].name, IS_MAPPED) == 0) {
			ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb_map: "
				  "Skipping attribute '%s'",
				  msg->elements[i].name);
			continue;
		}

		ret = ldb_msg_el_partition(module, local, remote, msg, msg->elements[i].name, &msg->elements[i]);
		if (ret) {
			return ret;
		}
	}

	return 0;
}


static int map_add_do_local(struct map_context *ac);
static int map_modify_do_local(struct map_context *ac);
static int map_delete_do_local(struct map_context *ac);
static int map_rename_do_local(struct map_context *ac);
static int map_rename_do_fixup(struct map_context *ac);
static int map_rename_local_callback(struct ldb_request *req,
				     struct ldb_reply *ares);


/*****************************************************************************
 * COMMON INBOUND functions
*****************************************************************************/

/* Store the DN of a single search result in context. */
static int map_search_self_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct map_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	/* We are interested only in the single reply */
	switch(ares->type) {
	case LDB_REPLY_ENTRY:
		/* We have already found a remote DN */
		if (ac->local_dn) {
			ldb_set_errstring(ldb,
					  "Too many results!");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		/* Store local DN */
		ac->local_dn = talloc_steal(ac, ares->message->dn);
		break;

	case LDB_REPLY_DONE:

		switch (ac->req->operation) {
		case LDB_MODIFY:
			ret = map_modify_do_local(ac);
			break;
		case LDB_DELETE:
			ret = map_delete_do_local(ac);
			break;
		case LDB_RENAME:
			ret = map_rename_do_local(ac);
			break;
		default:
			/* if we get here we have definitely a problem */
			ret = LDB_ERR_OPERATIONS_ERROR;
		}
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

	default:
		/* ignore referrals */
		break;
	}

	talloc_free(ares);
	return LDB_SUCCESS;
}

/* Build a request to search the local record by its DN. */
static int map_search_self_req(struct ldb_request **req,
				struct map_context *ac,
				struct ldb_dn *dn)
{
	/* attrs[] is returned from this function in
	 * ac->search_req->op.search.attrs, so it must be static, as
	 * otherwise the compiler can put it on the stack */
	static const char * const attrs[] = { IS_MAPPED, NULL };
	struct ldb_parse_tree *tree;

	/* Limit search to records with 'IS_MAPPED' present */
	tree = ldb_parse_tree(ac, "(" IS_MAPPED "=*)");
	if (tree == NULL) {
		map_oom(ac->module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*req = map_search_base_req(ac, dn, attrs, tree,
				   ac, map_search_self_callback);
	if (*req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

static int map_op_local_callback(struct ldb_request *req,
				 struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct map_context);
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
		ldb_set_errstring(ldb, "Invalid reply type!");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	/* Do the remote request. */
	ret = ldb_next_remote_request(ac->module, ac->remote_req);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	return LDB_SUCCESS;
}

static int map_op_remote_callback(struct ldb_request *req,
				  struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct map_context *ac;

	ac = talloc_get_type(req->context, struct map_context);
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
		ldb_set_errstring(ldb, "Invalid reply type!");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
}


/*****************************************************************************
 * ADD operations
*****************************************************************************/


/* Add a record. */
int ldb_map_add(struct ldb_module *module, struct ldb_request *req)
{
	const struct ldb_message *msg = req->op.add.message;
	struct ldb_context *ldb;
	struct map_context *ac;
	struct ldb_message *remote_msg;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(msg->dn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested (perhaps no DN mapping specified), skip to next module */
	if (!ldb_dn_check_local(module, msg->dn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping needed, fail */
	if (!ldb_msg_check_remote(module, msg)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare context and handle */
	ac = map_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}


	/* Prepare the local message */
	ac->local_msg = ldb_msg_new(ac);
	if (ac->local_msg == NULL) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->local_msg->dn = msg->dn;

	/* Prepare the remote message */
	remote_msg = ldb_msg_new(ac);
	if (remote_msg == NULL) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	remote_msg->dn = ldb_dn_map_local(ac->module, remote_msg, msg->dn);

	/* Split local from remote message */
	ldb_msg_partition(module, ac->local_msg, remote_msg, msg);

	/* Prepare the remote operation */
	ret = ldb_build_add_req(&ac->remote_req, ldb,
				ac, remote_msg,
				req->controls,
				ac, map_op_remote_callback,
				req);
	LDB_REQ_SET_LOCATION(ac->remote_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if ((ac->local_msg->num_elements == 0) ||
	    ( ! map_check_local_db(ac->module))) {
		/* No local data or db, just run the remote request */
		return ldb_next_remote_request(ac->module, ac->remote_req);
	}

	/* Store remote DN in 'IS_MAPPED' */
	/* TODO: use GUIDs here instead */
	ret = ldb_msg_add_linearized_dn(ac->local_msg, IS_MAPPED,
					remote_msg->dn);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return map_add_do_local(ac);
}

/* Add the local record. */
static int map_add_do_local(struct map_context *ac)
{
	struct ldb_request *local_req;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	/* Prepare the local operation */
	ret = ldb_build_add_req(&local_req, ldb, ac,
				ac->local_msg,
				ac->req->controls,
				ac,
				map_op_local_callback,
				ac->req);
	LDB_REQ_SET_LOCATION(local_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return ldb_next_request(ac->module, local_req);
}

/*****************************************************************************
 * MODIFY operations
*****************************************************************************/

/* Modify a record. */
int ldb_map_modify(struct ldb_module *module, struct ldb_request *req)
{
	const struct ldb_message *msg = req->op.mod.message;
	struct ldb_request *search_req;
	struct ldb_message *remote_msg;
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(msg->dn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested (perhaps no DN mapping specified), skip to next module */
	if (!ldb_dn_check_local(module, msg->dn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping needed, skip to next module */
	/* TODO: What if the remote part exists, the local doesn't,
	 *	 and this request wants to modify local data and thus
	 *	 add the local record? */
	if (!ldb_msg_check_remote(module, msg)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare context and handle */
	ac = map_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare the local message */
	ac->local_msg = ldb_msg_new(ac);
	if (ac->local_msg == NULL) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->local_msg->dn = msg->dn;

	/* Prepare the remote message */
	remote_msg = ldb_msg_new(ac->remote_req);
	if (remote_msg == NULL) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	remote_msg->dn = ldb_dn_map_local(ac->module, remote_msg, msg->dn);

	/* Split local from remote message */
	ldb_msg_partition(module, ac->local_msg, remote_msg, msg);

	/* Prepare the remote operation */
	ret = ldb_build_mod_req(&ac->remote_req, ldb,
				ac, remote_msg,
				req->controls,
				ac, map_op_remote_callback,
				req);
	LDB_REQ_SET_LOCATION(ac->remote_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if ((ac->local_msg->num_elements == 0) ||
	    ( ! map_check_local_db(ac->module))) {
		/* No local data or db, just run the remote request */
		return ldb_next_remote_request(ac->module, ac->remote_req);
	}

	/* prepare the search operation */
	ret = map_search_self_req(&search_req, ac, msg->dn);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(module, search_req);
}

/* Modify the local record. */
static int map_modify_do_local(struct map_context *ac)
{
	struct ldb_request *local_req;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	if (ac->local_dn == NULL) {
		/* No local record present, add it instead */
		/* Add local 'IS_MAPPED' */
		/* TODO: use GUIDs here instead */
		if (ldb_msg_add_empty(ac->local_msg, IS_MAPPED,
					LDB_FLAG_MOD_ADD, NULL) != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ret = ldb_msg_add_linearized_dn(ac->local_msg, IS_MAPPED,
						ac->remote_req->op.mod.message->dn);
		if (ret != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* Prepare the local operation */
		ret = ldb_build_add_req(&local_req, ldb, ac,
					ac->local_msg,
					ac->req->controls,
					ac,
					map_op_local_callback,
					ac->req);
		LDB_REQ_SET_LOCATION(local_req);
		if (ret != LDB_SUCCESS) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	} else {
		/* Prepare the local operation */
		ret = ldb_build_mod_req(&local_req, ldb, ac,
					ac->local_msg,
					ac->req->controls,
					ac,
					map_op_local_callback,
					ac->req);
		LDB_REQ_SET_LOCATION(local_req);
		if (ret != LDB_SUCCESS) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	return ldb_next_request(ac->module, local_req);
}

/*****************************************************************************
 * DELETE operations
*****************************************************************************/

/* Delete a record. */
int ldb_map_delete(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_request *search_req;
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.del.dn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested (perhaps no DN mapping specified).
	 * Skip to next module */
	if (!ldb_dn_check_local(module, req->op.del.dn)) {
		return ldb_next_request(module, req);
	}

	/* Prepare context and handle */
	ac = map_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare the remote operation */
	ret = ldb_build_del_req(&ac->remote_req, ldb, ac,
				   ldb_dn_map_local(module, ac, req->op.del.dn),
				   req->controls,
				   ac,
				   map_op_remote_callback,
				   req);
	LDB_REQ_SET_LOCATION(ac->remote_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* No local db, just run the remote request */
	if (!map_check_local_db(ac->module)) {
		/* Do the remote request. */
		return ldb_next_remote_request(ac->module, ac->remote_req);
	}

	/* Prepare the search operation */
	ret = map_search_self_req(&search_req, ac, req->op.del.dn);
	if (ret != LDB_SUCCESS) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(module, search_req);
}

/* Delete the local record. */
static int map_delete_do_local(struct map_context *ac)
{
	struct ldb_request *local_req;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	/* No local record, continue remotely */
	if (ac->local_dn == NULL) {
		/* Do the remote request. */
		return ldb_next_remote_request(ac->module, ac->remote_req);
	}

	/* Prepare the local operation */
	ret = ldb_build_del_req(&local_req, ldb, ac,
				   ac->req->op.del.dn,
				   ac->req->controls,
				   ac,
				   map_op_local_callback,
				   ac->req);
	LDB_REQ_SET_LOCATION(local_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return ldb_next_request(ac->module, local_req);
}

/*****************************************************************************
 * RENAME operations
*****************************************************************************/

/* Rename a record. */
int ldb_map_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_request *search_req;
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.rename.olddn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested (perhaps no DN mapping specified).
	 * Skip to next module */
	if ((!ldb_dn_check_local(module, req->op.rename.olddn)) &&
	    (!ldb_dn_check_local(module, req->op.rename.newdn))) {
		return ldb_next_request(module, req);
	}

	/* Rename into/out of the mapped partition requested, bail out */
	if (!ldb_dn_check_local(module, req->op.rename.olddn) ||
	    !ldb_dn_check_local(module, req->op.rename.newdn)) {
		return LDB_ERR_AFFECTS_MULTIPLE_DSAS;
	}

	/* Prepare context and handle */
	ac = map_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare the remote operation */
	ret = ldb_build_rename_req(&ac->remote_req, ldb, ac,
				   ldb_dn_map_local(module, ac, req->op.rename.olddn),
				   ldb_dn_map_local(module, ac, req->op.rename.newdn),
				   req->controls,
				   ac, map_op_remote_callback,
				   req);
	LDB_REQ_SET_LOCATION(ac->remote_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* No local db, just run the remote request */
	if (!map_check_local_db(ac->module)) {
		/* Do the remote request. */
		return ldb_next_remote_request(ac->module, ac->remote_req);
	}

	/* Prepare the search operation */
	ret = map_search_self_req(&search_req, ac, req->op.rename.olddn);
	if (ret != LDB_SUCCESS) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(module, search_req);
}

/* Rename the local record. */
static int map_rename_do_local(struct map_context *ac)
{
	struct ldb_request *local_req;
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(ac->module);

	/* No local record, continue remotely */
	if (ac->local_dn == NULL) {
		/* Do the remote request. */
		return ldb_next_remote_request(ac->module, ac->remote_req);
	}

	/* Prepare the local operation */
	ret = ldb_build_rename_req(&local_req, ldb, ac,
				   ac->req->op.rename.olddn,
				   ac->req->op.rename.newdn,
				   ac->req->controls,
				   ac,
				   map_rename_local_callback,
				   ac->req);
	LDB_REQ_SET_LOCATION(local_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(ac->module, local_req);
}

static int map_rename_local_callback(struct ldb_request *req,
				     struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct map_context);
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
		ldb_set_errstring(ldb, "Invalid reply type!");
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	/* proceed with next step */
	ret = map_rename_do_fixup(ac);
	if (ret != LDB_SUCCESS) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}

	return LDB_SUCCESS;
}

/* Update the local 'IS_MAPPED' attribute. */
static int map_rename_do_fixup(struct map_context *ac)
{
	struct ldb_request *local_req;

	/* Prepare the fixup operation */
	/* TODO: use GUIDs here instead -- or skip it when GUIDs are used. */
	local_req = map_build_fixup_req(ac,
					ac->req->op.rename.newdn,
					ac->remote_req->op.rename.newdn,
					ac,
					map_op_local_callback);
	if (local_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(ac->module, local_req);
}
