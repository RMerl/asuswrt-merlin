/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007

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
#include "ldb_module.h"

static int sample_add(struct ldb_module *mod, struct ldb_request *req)
{
	struct ldb_control *control;

	ldb_msg_add_fmt(req->op.add.message, "touchedBy", "sample");

	/* check if there's a relax control */
	control = ldb_request_get_control(req, LDB_CONTROL_RELAX_OID);
	if (control == NULL) {
		/* not found go on */
		return ldb_next_request(mod, req);
	} else {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
}

static int sample_modify(struct ldb_module *mod, struct ldb_request *req)
{
	struct ldb_control *control;

	/* check if there's a relax control */
	control = ldb_request_get_control(req, LDB_CONTROL_RELAX_OID);
	if (control == NULL) {
		/* not found go on */
		return ldb_next_request(mod, req);
	} else {
		return LDB_ERR_UNWILLING_TO_PERFORM;
	}
}


static struct ldb_module_ops ldb_sample_module_ops = {
	.name              = "sample",
	.add		   = sample_add,
	.del		   = sample_modify,
	.modify		   = sample_modify,
};

int ldb_sample_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_sample_module_ops);
}
