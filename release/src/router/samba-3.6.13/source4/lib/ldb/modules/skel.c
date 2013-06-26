/*
   ldb database library

   Copyright (C) Simo Sorce  2004

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
 *  Name: ldb
 *
 *  Component: ldb skel module
 *
 *  Description: example module
 *
 *  Author: Simo Sorce
 */

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_module.h"

struct private_data {

	char *some_private_data;
};

/* search */
static int skel_search(struct ldb_module *module, struct ldb_request *req)
{
	return ldb_next_request(module, req);
}

/* add */
static int skel_add(struct ldb_module *module, struct ldb_request *req){
	return ldb_next_request(module, req);
}

/* modify */
static int skel_modify(struct ldb_module *module, struct ldb_request *req)
{
	return ldb_next_request(module, req);
}

/* delete */
static int skel_delete(struct ldb_module *module, struct ldb_request *req)
{
	return ldb_next_request(module, req);
}

/* rename */
static int skel_rename(struct ldb_module *module, struct ldb_request *req)
{
	return ldb_next_request(module, req);
}

/* start a transaction */
static int skel_start_trans(struct ldb_module *module)
{
	return ldb_next_start_trans(module);
}

/* end a transaction */
static int skel_end_trans(struct ldb_module *module)
{
	return ldb_next_end_trans(module);
}

/* delete a transaction */
static int skel_del_trans(struct ldb_module *module)
{
	return ldb_next_del_trans(module);
}

static int skel_destructor(struct ldb_module *ctx)
{
	struct private_data *data;

	data = talloc_get_type(ldb_module_get_private(ctx), struct private_data);

	/* put your clean-up functions here */
	if (data->some_private_data) talloc_free(data->some_private_data);

	return 0;
}

static int skel_request(struct ldb_module *module, struct ldb_request *req)
{
	return ldb_next_request(module, req);
}

static int skel_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	struct private_data *data;

	ldb = ldb_module_get_ctx(module);

	data = talloc(module, struct private_data);
	if (data == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	data->some_private_data = NULL;
	ldb_module_set_private(module, data);

	talloc_set_destructor (module, skel_destructor);

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_skel_module_ops = {
	.name		   = "skel",
	.init_context	   = skel_init,
	.search            = skel_search,
	.add               = skel_add,
	.modify            = skel_modify,
	.del               = skel_delete,
	.rename            = skel_rename,
	.request      	   = skel_request,
	.start_transaction = skel_start_trans,
	.end_transaction   = skel_end_trans,
	.del_transaction   = skel_del_trans,
};

int ldb_skel_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_skel_module_ops);
}
