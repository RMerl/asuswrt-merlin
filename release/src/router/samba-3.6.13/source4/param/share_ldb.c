/* 
   Unix SMB/CIFS implementation.
   
   LDB based shares configuration
   
   Copyright (C) Simo Sorce	2006
   
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
#include <ldb.h>
#include <ldb_errors.h>
#include "auth/auth.h"
#include "ldb_wrap.h"
#include "param/share.h"
#include "param/param.h"

static NTSTATUS sldb_init(TALLOC_CTX *mem_ctx, const struct share_ops *ops, 
			  struct tevent_context *ev_ctx,
			  struct loadparm_context *lp_ctx,
			  struct share_context **ctx)
{
	struct ldb_context *sdb;

	*ctx = talloc(mem_ctx, struct share_context);
	if (!*ctx) {
		DEBUG(0, ("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}
	
	sdb = ldb_wrap_connect(*ctx, ev_ctx, lp_ctx,
			       private_path(*ctx, lp_ctx, "share.ldb"),
			       system_session(lp_ctx),
			       NULL, 0);

	if (!sdb) {
		talloc_free(*ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	(*ctx)->ops = ops;
	(*ctx)->priv_data = (void *)sdb;

	return NT_STATUS_OK;
}

static const char *sldb_string_option(struct share_config *scfg, const char *opt_name, const char *defval)
{
	struct ldb_message *msg;
	struct ldb_message_element *el;

	if (scfg == NULL) return defval;

	msg = talloc_get_type(scfg->opaque, struct ldb_message);

	if (strchr(opt_name, ':')) {
		char *name, *p;

		name = talloc_strdup(scfg, opt_name);
		if (!name) {
			return NULL;
		}
		p = strchr(name, ':');
		*p = '-';

		el = ldb_msg_find_element(msg, name);
	} else {
		el = ldb_msg_find_element(msg, opt_name);
	}

	if (el == NULL) {
		return defval;
	}

	return (const char *)(el->values[0].data);
}

static int sldb_int_option(struct share_config *scfg, const char *opt_name, int defval)
{
	const char *val;
	int ret;

       	val = sldb_string_option(scfg, opt_name, NULL);
	if (val == NULL) return defval;

	errno = 0;
	ret = (int)strtol(val, NULL, 10);
	if (errno) return -1;

	return ret;
}

static bool sldb_bool_option(struct share_config *scfg, const char *opt_name, bool defval)
{
	const char *val;

       	val = sldb_string_option(scfg, opt_name, NULL);
	if (val == NULL) return defval;

	if (strcasecmp(val, "true") == 0) return true;

	return false;
}

static const char **sldb_string_list_option(TALLOC_CTX *mem_ctx, struct share_config *scfg, const char *opt_name)
{
	struct ldb_message *msg;
	struct ldb_message_element *el;
	const char **list;
	int i;

	if (scfg == NULL) return NULL;

	msg = talloc_get_type(scfg->opaque, struct ldb_message);

	if (strchr(opt_name, ':')) {
		char *name, *p;

		name = talloc_strdup(scfg, opt_name);
		if (!name) {
			return NULL;
		}
		p = strchr(name, ':');
		*p = '-';

		el = ldb_msg_find_element(msg, name);
	} else {
		el = ldb_msg_find_element(msg, opt_name);
	}

	if (el == NULL) {
		return NULL;
	}

	list = talloc_array(mem_ctx, const char *, el->num_values + 1);
	if (!list) return NULL;

	for (i = 0; i < el->num_values; i++) {
		list[i] = (const char *)(el->values[i].data);
	}
	list[i] = NULL;

	return list;
}

static NTSTATUS sldb_list_all(TALLOC_CTX *mem_ctx,
				 struct share_context *ctx,
				 int *count,
				 const char ***names)
{
	int ret, i, j;
	const char **n;
	struct ldb_context *ldb;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ldb = talloc_get_type(ctx->priv_data, struct ldb_context);

	ret = ldb_search(ldb, tmp_ctx, &res, ldb_dn_new(tmp_ctx, ldb, "CN=SHARES"),
			 LDB_SCOPE_SUBTREE, NULL, "(name=*)");
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	n = talloc_array(mem_ctx, const char *, res->count);
	if (!n) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0, j = 0; i < res->count; i++) {
		n[j] = talloc_strdup(n, ldb_msg_find_attr_as_string(res->msgs[i], "name", NULL));
		if (!n[j]) {
			DEBUG(0,("WARNING: Malformed share object in share database\n!"));
			continue;
		}
		j++;
	}

	*names = n;
	*count = j;
	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

static NTSTATUS sldb_get_config(TALLOC_CTX *mem_ctx,
			 struct share_context *ctx,
			 const char *name,
			 struct share_config **scfg)
{
	int ret;
	struct share_config *s;
	struct ldb_context *ldb;
	struct ldb_result *res;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ldb = talloc_get_type(ctx->priv_data, struct ldb_context);

	ret = ldb_search(ldb, tmp_ctx, &res,
				 ldb_dn_new(tmp_ctx, ldb, "CN=SHARES"), LDB_SCOPE_SUBTREE, NULL,
				 "(name=%s)", name);
	if (ret != LDB_SUCCESS || res->count > 1) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	} else if (res->count != 1) {
		talloc_free(tmp_ctx);
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	s = talloc(tmp_ctx, struct share_config);
	if (!s) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	s->name = talloc_strdup(s, ldb_msg_find_attr_as_string(res->msgs[0], "name", NULL));
	if (!s->name) {
		DEBUG(0,("ERROR: Invalid share object!\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	s->opaque = talloc_steal(s, res->msgs[0]);
	if (!s->opaque) {
		DEBUG(0,("ERROR: Invalid share object!\n"));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	s->ctx = ctx;

	*scfg = talloc_steal(mem_ctx, s);

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

#define SHARE_ADD_STRING(name, value) do { \
	err = ldb_msg_add_string(msg, name, value); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add string share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} } while(0)

#define SHARE_ADD_INT(name, value) do { \
	err = ldb_msg_add_fmt(msg, name, "%d", value); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add integer share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} } while(0)

#define SHARE_ADD_BLOB(name, value) do { \
	err = ldb_msg_add_value(msg, name, value, NULL); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add blob share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} } while(0)

static NTSTATUS sldb_create(struct share_context *ctx, const char *name, struct share_info *info, int count)
{
	struct ldb_context *ldb;
	struct ldb_message *msg;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS ret;
	int err, i, j;

	for (i = 0, j = 0; i < count && j != 0x03; i++) {
		if (strcasecmp(info[i].name, SHARE_TYPE) == 0) j |= 0x02;
		if (strcasecmp(info[i].name, SHARE_PATH) == 0) j |= 0x01;
		if (strcasecmp(info[i].name, SHARE_NAME) == 0) {
			if (strcasecmp(name, (char *)info[i].value) != 0) {
				return NT_STATUS_INVALID_PARAMETER;
			}
		}
	}
	if (!name || j != 0x03) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	tmp_ctx = talloc_new(NULL);
	if (!tmp_ctx) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ldb = talloc_get_type(ctx->priv_data, struct ldb_context);

	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* TODO: escape info->name */
	msg->dn = ldb_dn_new_fmt(tmp_ctx, ldb, "CN=%s,CN=SHARES", name);
	if (!msg->dn) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	SHARE_ADD_STRING("objectClass", "top");
	SHARE_ADD_STRING("objectClass", "share");
	SHARE_ADD_STRING("cn", name);
	SHARE_ADD_STRING(SHARE_NAME, name);

	for (i = 0; i < count; i++) {
		if (strcasecmp(info[i].name, SHARE_NAME) == 0) continue;

		switch (info[i].type) {
		case SHARE_INFO_STRING:
			SHARE_ADD_STRING(info[i].name, (char *)info[i].value);
			break;
		case SHARE_INFO_INT:
			SHARE_ADD_INT(info[i].name, *((int *)info[i].value));
			break;
		case SHARE_INFO_BLOB:
			SHARE_ADD_BLOB(info[i].name, (DATA_BLOB *)info[i].value);
			break;
		default:
			DEBUG(2,("ERROR: Invalid share info type for %s\n", info[i].name));
			ret = NT_STATUS_INVALID_PARAMETER;
			goto done;
		}
	}

	/* TODO: Security Descriptor */

	SHARE_ADD_STRING(SHARE_AVAILABLE, "true");
	SHARE_ADD_STRING(SHARE_BROWSEABLE, "true");
	SHARE_ADD_STRING(SHARE_READONLY, "false");
	SHARE_ADD_STRING(SHARE_NTVFS_HANDLER, "unixuid");
	SHARE_ADD_STRING(SHARE_NTVFS_HANDLER, "posix");

	err = ldb_add(ldb, msg);
	if (err != LDB_SUCCESS) {
		DEBUG(2,("ERROR: unable to add share %s to share.ldb\n"
			 "       err=%d [%s]\n", name, err, ldb_errstring(ldb)));
		if (err == LDB_ERR_NO_SUCH_OBJECT) {
			ret = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		} else if (err == LDB_ERR_ENTRY_ALREADY_EXISTS) {
			ret = NT_STATUS_OBJECT_NAME_COLLISION;
		} else {
			ret = NT_STATUS_UNSUCCESSFUL;
		}
		goto done;
	}

	ret = NT_STATUS_OK;
done:
	talloc_free(tmp_ctx);
	return ret;
}

#define SHARE_MOD_STRING(name, value) do { \
	err = ldb_msg_add_empty(msg, name, LDB_FLAG_MOD_REPLACE, NULL); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add string share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} \
	err = ldb_msg_add_string(msg, name, value); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add string share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} } while(0)

#define SHARE_MOD_INT(name, value) do { \
	err = ldb_msg_add_empty(msg, name, LDB_FLAG_MOD_REPLACE, NULL); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add string share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} \
	err = ldb_msg_add_fmt(msg, name, "%d", value); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add integer share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} } while(0)

#define SHARE_MOD_BLOB(name, value) do { \
	err = ldb_msg_add_empty(msg, name, LDB_FLAG_MOD_REPLACE, NULL); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add string share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} \
	err = ldb_msg_add_value(msg, name, value, NULL); \
	if (err != LDB_SUCCESS) { \
		DEBUG(2,("ERROR: unable to add blob share option %s to ldb msg\n", name)); \
		ret = NT_STATUS_UNSUCCESSFUL; \
		goto done; \
	} } while(0)

static NTSTATUS sldb_set(struct share_context *ctx, const char *name, struct share_info *info, int count)
{
	struct ldb_context *ldb;
	struct ldb_message *msg;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS ret;
	bool do_rename = false;
	char *newname;
	int err, i;

	if (!name) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	
	tmp_ctx = talloc_new(NULL);
	if (!tmp_ctx) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ldb = talloc_get_type(ctx->priv_data, struct ldb_context);

	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	/* TODO: escape name */
	msg->dn = ldb_dn_new_fmt(tmp_ctx, ldb, "CN=%s,CN=SHARES", name);
	if (!msg->dn) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i = 0; i < count; i++) {
		if (strcasecmp(info[i].name, SHARE_NAME) == 0) {
			if (strcasecmp(name, (char *)info[i].value) != 0) {
				do_rename = true;
				newname = (char *)info[i].value;
				SHARE_MOD_STRING("cn", (char *)info[i].value);
			}
		}

		switch (info[i].type) {
		case SHARE_INFO_STRING:
			SHARE_MOD_STRING(info[i].name, (char *)info[i].value);
			break;
		case SHARE_INFO_INT:
			SHARE_MOD_INT(info[i].name, *((int *)info[i].value));
			break;
		case SHARE_INFO_BLOB:
			SHARE_MOD_BLOB(info[i].name, (DATA_BLOB *)info[i].value);
			break;
		default:
			DEBUG(2,("ERROR: Invalid share info type for %s\n", info[i].name));
			ret = NT_STATUS_INVALID_PARAMETER;
			goto done;
		}
	}

	if (do_rename) {
		struct ldb_dn *olddn, *newdn;

		olddn = msg->dn;

		/* TODO: escape newname */
		newdn = ldb_dn_new_fmt(tmp_ctx, ldb, "CN=%s,CN=SHARES", newname);
		if (!newdn) {
			DEBUG(0,("ERROR: Out of memory!\n"));
			ret = NT_STATUS_NO_MEMORY;
			goto done;
		}

		err = ldb_rename(ldb, olddn, newdn);
		if (err != LDB_SUCCESS) {
			DEBUG(2,("ERROR: unable to rename share %s (to %s)\n"
				 "       err=%d [%s]\n", name, newname, err, ldb_errstring(ldb)));
			if (err == LDB_ERR_NO_SUCH_OBJECT) {
				ret = NT_STATUS_OBJECT_NAME_COLLISION;
			} else {
				ret = NT_STATUS_UNSUCCESSFUL;
			}
			goto done;
		}

		msg->dn = newdn;
	}

	err = ldb_modify(ldb, msg);
	if (err != LDB_SUCCESS) {
		DEBUG(2,("ERROR: unable to add share %s to share.ldb\n"
			 "       err=%d [%s]\n", name, err, ldb_errstring(ldb)));
		if (err == LDB_ERR_NO_SUCH_OBJECT) {
			ret = NT_STATUS_OBJECT_NAME_COLLISION;
		} else {
			ret = NT_STATUS_UNSUCCESSFUL;
		}
		goto done;
	}

	ret = NT_STATUS_OK;
done:
	talloc_free(tmp_ctx);
	return ret;
}

static NTSTATUS sldb_remove(struct share_context *ctx, const char *name)
{
	struct ldb_context *ldb;
	struct ldb_dn *dn;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS ret;
	int err;

	tmp_ctx = talloc_new(NULL);
	if (!tmp_ctx) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ldb = talloc_get_type(ctx->priv_data, struct ldb_context);

	dn = ldb_dn_new_fmt(tmp_ctx, ldb, "CN=%s,CN=SHARES", name);
	if (!dn) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		ret = NT_STATUS_NO_MEMORY;
		goto done;
	}

	err = ldb_delete(ldb, dn);
	if (err != LDB_SUCCESS) {
		DEBUG(2,("ERROR: unable to remove share %s from share.ldb\n"
			 "       err=%d [%s]\n", name, err, ldb_errstring(ldb)));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto done;
	}

	ret = NT_STATUS_OK;
done:
	talloc_free(tmp_ctx);
	return ret;
}

static const struct share_ops ops = {
	.name = "ldb",
	.init = sldb_init,
	.string_option = sldb_string_option,
	.int_option = sldb_int_option,
	.bool_option = sldb_bool_option,
	.string_list_option = sldb_string_list_option,
	.list_all = sldb_list_all,
	.get_config = sldb_get_config,
	.create = sldb_create,
	.set = sldb_set,
	.remove = sldb_remove
};

NTSTATUS share_ldb_init(void)
{
	return share_register(&ops);
}
