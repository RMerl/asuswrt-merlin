/*
   ldb database mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006

   * NOTICE: this module is NOT released under the GNU LGPL license as
   * other ldb code. This module is release under the GNU GPL v2 or
   * later license.

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
#include "ldb/include/includes.h"

#include "ldb/modules/ldb_map.h"
#include "ldb/modules/ldb_map_private.h"


/* Mapping message elements
 * ======================== */

/* Map a message element into the remote partition. */
static struct ldb_message_element *ldb_msg_el_map_local(struct ldb_module *module, void *mem_ctx, const struct ldb_map_attribute *map, const struct ldb_message_element *old)
{
	struct ldb_message_element *el;
	int i;

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

	/* Unknown attribute: ignore */
	if (map == NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_WARNING, "ldb_map: "
			  "Not mapping attribute '%s': no mapping found\n",
			  old->name);
		goto local;
	}

	switch (map->type) {
	case MAP_IGNORE:
		goto local;

	case MAP_CONVERT:
		if (map->u.convert.convert_local == NULL) {
			ldb_debug(module->ldb, LDB_DEBUG_WARNING, "ldb_map: "
				  "Not mapping attribute '%s': "
				  "'convert_local' not set\n",
				  map->local_name);
			goto local;
		}
		/* fall through */
	case MAP_KEEP:
	case MAP_RENAME:
		el = ldb_msg_el_map_local(module, remote, map, old);
		break;

	case MAP_GENERATE:
		if (map->u.generate.generate_remote == NULL) {
			ldb_debug(module->ldb, LDB_DEBUG_WARNING, "ldb_map: "
				  "Not mapping attribute '%s': "
				  "'generate_remote' not set\n",
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
static BOOL ldb_msg_check_remote(struct ldb_module *module, const struct ldb_message *msg)
{
	const struct ldb_map_context *data = map_get_context(module);
	BOOL ret;
	int i;

	for (i = 0; i < msg->num_elements; i++) {
		ret = map_attr_check_remote(data, msg->elements[i].name);
		if (ret) {
			return ret;
		}
	}

	return False;
}

/* Split message elements that stay in the local partition from those
 * that are mapped into the remote partition. */
static int ldb_msg_partition(struct ldb_module *module, struct ldb_message *local, struct ldb_message *remote, const struct ldb_message *msg)
{
	/* const char * const names[]; */
	int i, ret;

	for (i = 0; i < msg->num_elements; i++) {
		/* Skip 'IS_MAPPED' */
		if (ldb_attr_cmp(msg->elements[i].name, IS_MAPPED) == 0) {
			ldb_debug(module->ldb, LDB_DEBUG_WARNING, "ldb_map: "
				  "Skipping attribute '%s'\n",
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


/* Inbound requests: add, modify, rename, delete
 * ============================================= */

/* Add the remote record. */
int map_add_do_remote(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->remote_req);

	ac->step = MAP_ADD_REMOTE;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_remote_request(ac->module, ac->remote_req);
}

/* Add the local record. */
int map_add_do_local(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->local_req);

	ac->step = MAP_ADD_LOCAL;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_request(ac->module, ac->local_req);
}

/* Add a record. */
int map_add(struct ldb_module *module, struct ldb_request *req)
{
	const struct ldb_message *msg = req->op.add.message;
	struct ldb_handle *h;
	struct map_context *ac;
	struct ldb_message *local, *remote;
	const char *dn;

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
	h = map_init_handle(req, module);
	if (h == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct map_context);

	/* Prepare the local operation */
	ac->local_req = talloc(ac, struct ldb_request);
	if (ac->local_req == NULL) {
		goto oom;
	}

	*(ac->local_req) = *req;	/* copy the request */

	ac->local_req->context = NULL;
	ac->local_req->callback = NULL;

	/* Prepare the remote operation */
	ac->remote_req = talloc(ac, struct ldb_request);
	if (ac->remote_req == NULL) {
		goto oom;
	}

	*(ac->remote_req) = *req;	/* copy the request */

	ac->remote_req->context = NULL;
	ac->remote_req->callback = NULL;

	/* Prepare the local message */
	local = ldb_msg_new(ac->local_req);
	if (local == NULL) {
		goto oom;
	}
	local->dn = msg->dn;

	/* Prepare the remote message */
	remote = ldb_msg_new(ac->remote_req);
	if (remote == NULL) {
		goto oom;
	}
	remote->dn = ldb_dn_map_local(ac->module, remote, msg->dn);

	/* Split local from remote message */
	ldb_msg_partition(module, local, remote, msg);
	ac->local_req->op.add.message = local;
	ac->remote_req->op.add.message = remote;

	if ((local->num_elements == 0) || (!map_check_local_db(ac->module))) {
		/* No local data or db, just run the remote request */
		talloc_free(ac->local_req);
		req->handle = h;	/* return our own handle to deal with this call */
		return map_add_do_remote(h);
	}

	/* Store remote DN in 'IS_MAPPED' */
	/* TODO: use GUIDs here instead */
	dn = ldb_dn_linearize(local, remote->dn);
	if (ldb_msg_add_string(local, IS_MAPPED, dn) != 0) {
		goto failed;
	}

	req->handle = h;		/* return our own handle to deal with this call */
	return map_add_do_local(h);

oom:
	map_oom(module);
failed:
	talloc_free(h);
	return LDB_ERR_OPERATIONS_ERROR;
}

/* Modify the remote record. */
int map_modify_do_remote(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->remote_req);

	ac->step = MAP_MODIFY_REMOTE;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_remote_request(ac->module, ac->remote_req);
}

/* Modify the local record. */
int map_modify_do_local(struct ldb_handle *handle)
{
	struct map_context *ac;
	struct ldb_message *msg;
	char *dn;

	ac = talloc_get_type(handle->private_data, struct map_context);

	if (ac->local_dn == NULL) {
		/* No local record present, add it instead */
		msg = discard_const_p(struct ldb_message, ac->local_req->op.mod.message);

		/* Add local 'IS_MAPPED' */
		/* TODO: use GUIDs here instead */
		dn = ldb_dn_linearize(msg, ac->remote_req->op.mod.message->dn);
		if (ldb_msg_add_empty(msg, IS_MAPPED, LDB_FLAG_MOD_ADD, NULL) != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		if (ldb_msg_add_string(msg, IS_MAPPED, dn) != 0) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* Turn request into 'add' */
		ac->local_req->operation = LDB_ADD;
		ac->local_req->op.add.message = msg;
		/* TODO: Could I just leave msg in there?  I think so,
		 *	 but it looks clearer this way. */
	}

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->local_req);

	ac->step = MAP_MODIFY_LOCAL;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_request(ac->module, ac->local_req);
}

/* Modify a record. */
int map_modify(struct ldb_module *module, struct ldb_request *req)
{
	const struct ldb_message *msg = req->op.mod.message;
	struct ldb_handle *h;
	struct map_context *ac;
	struct ldb_message *local, *remote;

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
	h = map_init_handle(req, module);
	if (h == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct map_context);

	/* Prepare the local operation */
	ac->local_req = talloc(ac, struct ldb_request);
	if (ac->local_req == NULL) {
		goto oom;
	}

	*(ac->local_req) = *req;	/* copy the request */

	ac->local_req->context = NULL;
	ac->local_req->callback = NULL;

	/* Prepare the remote operation */
	ac->remote_req = talloc(ac, struct ldb_request);
	if (ac->remote_req == NULL) {
		goto oom;
	}

	*(ac->remote_req) = *req;	/* copy the request */

	ac->remote_req->context = NULL;
	ac->remote_req->callback = NULL;

	/* Prepare the local message */
	local = ldb_msg_new(ac->local_req);
	if (local == NULL) {
		goto oom;
	}
	local->dn = msg->dn;

	/* Prepare the remote message */
	remote = ldb_msg_new(ac->remote_req);
	if (remote == NULL) {
		goto oom;
	}
	remote->dn = ldb_dn_map_local(ac->module, remote, msg->dn);

	/* Split local from remote message */
	ldb_msg_partition(module, local, remote, msg);
	ac->local_req->op.mod.message = local;
	ac->remote_req->op.mod.message = remote;

	if ((local->num_elements == 0) || (!map_check_local_db(ac->module))) {
		/* No local data or db, just run the remote request */
		talloc_free(ac->local_req);
		req->handle = h;	/* return our own handle to deal with this call */
		return map_modify_do_remote(h);
	}

	/* prepare the search operation */
	ac->search_req = map_search_self_req(ac, msg->dn);
	if (ac->search_req == NULL) {
		goto failed;
	}

	ac->step = MAP_SEARCH_SELF_MODIFY;

	req->handle = h;		/* return our own handle to deal with this call */
	return ldb_next_request(module, ac->search_req);

oom:
	map_oom(module);
failed:
	talloc_free(h);
	return LDB_ERR_OPERATIONS_ERROR;
}

/* Delete the remote record. */
int map_delete_do_remote(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->remote_req);

	ac->step = MAP_DELETE_REMOTE;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_remote_request(ac->module, ac->remote_req);
}

/* Delete the local record. */
int map_delete_do_local(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	/* No local record, continue remotely */
	if (ac->local_dn == NULL) {
		return map_delete_do_remote(handle);
	}

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->local_req);

	ac->step = MAP_DELETE_LOCAL;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_request(ac->module, ac->local_req);
}

/* Delete a record. */
int map_delete(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_handle *h;
	struct map_context *ac;

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.del.dn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested (perhaps no DN mapping specified), skip to next module */
	if (!ldb_dn_check_local(module, req->op.del.dn)) {
		return ldb_next_request(module, req);
	}

	/* Prepare context and handle */
	h = map_init_handle(req, module);
	if (h == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct map_context);

	/* Prepare the local operation */
	ac->local_req = talloc(ac, struct ldb_request);
	if (ac->local_req == NULL) {
		goto oom;
	}

	*(ac->local_req) = *req;	/* copy the request */
	ac->local_req->op.del.dn = req->op.del.dn;

	ac->local_req->context = NULL;
	ac->local_req->callback = NULL;

	/* Prepare the remote operation */
	ac->remote_req = talloc(ac, struct ldb_request);
	if (ac->remote_req == NULL) {
		goto oom;
	}

	*(ac->remote_req) = *req;	/* copy the request */
	ac->remote_req->op.del.dn = ldb_dn_map_local(module, ac->remote_req, req->op.del.dn);

	/* No local db, just run the remote request */
	if (!map_check_local_db(ac->module)) {
		req->handle = h;	/* return our own handle to deal with this call */
		return map_delete_do_remote(h);
	}

	ac->remote_req->context = NULL;
	ac->remote_req->callback = NULL;

	/* Prepare the search operation */
	ac->search_req = map_search_self_req(ac, req->op.del.dn);
	if (ac->search_req == NULL) {
		goto failed;
	}

	req->handle = h;		/* return our own handle to deal with this call */

	ac->step = MAP_SEARCH_SELF_DELETE;

	return ldb_next_request(module, ac->search_req);

oom:
	map_oom(module);
failed:
	talloc_free(h);
	return LDB_ERR_OPERATIONS_ERROR;
}

/* Rename the remote record. */
int map_rename_do_remote(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->remote_req);

	ac->step = MAP_RENAME_REMOTE;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_remote_request(ac->module, ac->remote_req);
}

/* Update the local 'IS_MAPPED' attribute. */
int map_rename_do_fixup(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->down_req);

	ac->step = MAP_RENAME_FIXUP;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_request(ac->module, ac->down_req);
}

/* Rename the local record. */
int map_rename_do_local(struct ldb_handle *handle)
{
	struct map_context *ac;

	ac = talloc_get_type(handle->private_data, struct map_context);

	/* No local record, continue remotely */
	if (ac->local_dn == NULL) {
		return map_rename_do_remote(handle);
	}

	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, ac->local_req);

	ac->step = MAP_RENAME_LOCAL;

	handle->state = LDB_ASYNC_INIT;
	handle->status = LDB_SUCCESS;

	return ldb_next_request(ac->module, ac->local_req);
}

/* Rename a record. */
int map_rename(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_handle *h;
	struct map_context *ac;

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.rename.olddn)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested (perhaps no DN mapping specified), skip to next module */
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
	h = map_init_handle(req, module);
	if (h == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct map_context);

	/* Prepare the local operation */
	ac->local_req = talloc(ac, struct ldb_request);
	if (ac->local_req == NULL) {
		goto oom;
	}

	*(ac->local_req) = *req;	/* copy the request */
	ac->local_req->op.rename.olddn = req->op.rename.olddn;
	ac->local_req->op.rename.newdn = req->op.rename.newdn;

	ac->local_req->context = NULL;
	ac->local_req->callback = NULL;

	/* Prepare the remote operation */
	ac->remote_req = talloc(ac, struct ldb_request);
	if (ac->remote_req == NULL) {
		goto oom;
	}

	*(ac->remote_req) = *req;	/* copy the request */
	ac->remote_req->op.rename.olddn = ldb_dn_map_local(module, ac->remote_req, req->op.rename.olddn);
	ac->remote_req->op.rename.newdn = ldb_dn_map_local(module, ac->remote_req, req->op.rename.newdn);

	ac->remote_req->context = NULL;
	ac->remote_req->callback = NULL;

	/* No local db, just run the remote request */
	if (!map_check_local_db(ac->module)) {
		req->handle = h;	/* return our own handle to deal with this call */
		return map_rename_do_remote(h);
	}

	/* Prepare the fixup operation */
	/* TODO: use GUIDs here instead -- or skip it when GUIDs are used. */
	ac->down_req = map_build_fixup_req(ac, req->op.rename.newdn, ac->remote_req->op.rename.newdn);
	if (ac->down_req == NULL) {
		goto failed;
	}

	/* Prepare the search operation */
	ac->search_req = map_search_self_req(ac, req->op.rename.olddn);
	if (ac->search_req == NULL) {
		goto failed;
	}

	req->handle = h;		/* return our own handle to deal with this call */

	ac->step = MAP_SEARCH_SELF_RENAME;

	return ldb_next_request(module, ac->search_req);

oom:
	map_oom(module);
failed:
	talloc_free(h);
	return LDB_ERR_OPERATIONS_ERROR;
}
