/* 
   LDB nsswitch module

   Copyright (C) Simo Sorce 2006
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ldb-nss.h"

struct _ldb_nss_context *_ldb_nss_ctx = NULL;

NSS_STATUS _ldb_nss_init(void)
{
	int ret;

	pid_t mypid = getpid();

	if (_ldb_nss_ctx != NULL) {
		if (_ldb_nss_ctx->pid == mypid) {
			/* already initialized */
			return NSS_STATUS_SUCCESS;
		} else {
			/* we are in a forked child now, reinitialize */
			talloc_free(_ldb_nss_ctx);
			_ldb_nss_ctx = NULL;
		}
	}
		
	_ldb_nss_ctx = talloc_named(NULL, 0, "_ldb_nss_ctx(%u)", mypid);
	if (_ldb_nss_ctx == NULL) {
		return NSS_STATUS_UNAVAIL;
	}

	_ldb_nss_ctx->pid = mypid;

	_ldb_nss_ctx->ldb = ldb_init(_ldb_nss_ctx, NULL);
	if (_ldb_nss_ctx->ldb == NULL) {
		goto failed;
	}

	ret = ldb_connect(_ldb_nss_ctx->ldb, _LDB_NSS_URL, LDB_FLG_RDONLY, NULL);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	_ldb_nss_ctx->base = ldb_dn_new(_ldb_nss_ctx, _ldb_nss_ctx->ldb, _LDB_NSS_BASEDN);
	if ( ! ldb_dn_validate(_ldb_nss_ctx->base)) {
		goto failed;
	}

	_ldb_nss_ctx->pw_cur = 0;
	_ldb_nss_ctx->pw_res = NULL;
	_ldb_nss_ctx->gr_cur = 0;
	_ldb_nss_ctx->gr_res = NULL;

	return NSS_STATUS_SUCCESS;

failed:
	/* talloc_free(_ldb_nss_ctx); */
	_ldb_nss_ctx = NULL;
	return NSS_STATUS_UNAVAIL;
}

NSS_STATUS _ldb_nss_fill_passwd(struct passwd *result,
				char *buffer,
				int buflen,
				int *errnop,
				struct ldb_message *msg)
{
	int len;
	int bufpos;
	const char *tmp;

	bufpos = 0;

	/* get username */
	tmp = ldb_msg_find_attr_as_string(msg, "uid", NULL);
	if (tmp == NULL) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		return NSS_STATUS_UNAVAIL;
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->pw_name = &buffer[bufpos];
	bufpos += len;

	/* get userPassword */
	tmp = ldb_msg_find_attr_as_string(msg, "userPassword", NULL);
	if (tmp == NULL) {
		tmp = "LDB";
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->pw_passwd = &buffer[bufpos];
	bufpos += len;

	/* this backend never serves an uid 0 user */
	result->pw_uid = ldb_msg_find_attr_as_int(msg, "uidNumber", 0);
	if (result->pw_uid == 0) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		return NSS_STATUS_UNAVAIL;
	}

	result->pw_gid = ldb_msg_find_attr_as_int(msg, "gidNumber", 0);
	if (result->pw_gid == 0) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		return NSS_STATUS_UNAVAIL;
	}

	/* get gecos */
	tmp = ldb_msg_find_attr_as_string(msg, "gecos", NULL);
	if (tmp == NULL) {
		tmp = "";
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->pw_gecos = &buffer[bufpos];
	bufpos += len;

	/* get homeDirectory */
	tmp = ldb_msg_find_attr_as_string(msg, "homeDirectory", NULL);
	if (tmp == NULL) {
		tmp = "";
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->pw_dir = &buffer[bufpos];
	bufpos += len;

	/* get shell */
	tmp = ldb_msg_find_attr_as_string(msg, "loginShell", NULL);
	if (tmp == NULL) {
		tmp = "";
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->pw_shell = &buffer[bufpos];
	bufpos += len;

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _ldb_nss_fill_group(struct group *result,
				char *buffer,
				int buflen,
				int *errnop,
				struct ldb_message *group,
				struct ldb_result *members)
{
	const char *tmp;
	size_t len;
	size_t bufpos;
	size_t lsize;
	unsigned int i;

	bufpos = 0;

	/* get group name */
	tmp = ldb_msg_find_attr_as_string(group, "cn", NULL);
	if (tmp == NULL) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		return NSS_STATUS_UNAVAIL;
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->gr_name = &buffer[bufpos];
	bufpos += len;

	/* get userPassword */
	tmp = ldb_msg_find_attr_as_string(group, "userPassword", NULL);
	if (tmp == NULL) {
		tmp = "LDB";
	}
	len = strlen(tmp)+1;
	if (bufpos + len > buflen) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	}
	memcpy(&buffer[bufpos], tmp, len);
	result->gr_passwd = &buffer[bufpos];
	bufpos += len;

	result->gr_gid = ldb_msg_find_attr_as_int(group, "gidNumber", 0);
	if (result->gr_gid == 0) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		return NSS_STATUS_UNAVAIL;
	}

	/* check if there is enough memory for the list of pointers */
	lsize = (members->count + 1) * sizeof(char *);

	/* align buffer on pointer boundary */
	bufpos += (sizeof(char*) - ((unsigned long)(buffer) % sizeof(char*)));
	if ((buflen - bufpos) < lsize) {
		/* buffer too small */
		*errnop = errno = EAGAIN;
		return NSS_STATUS_TRYAGAIN;
	} 

	result->gr_mem = (char **)&buffer[bufpos];
	bufpos += lsize;

	for (i = 0; i < members->count; i++) {
		tmp = ldb_msg_find_attr_as_string(members->msgs[i], "uid", NULL);
		if (tmp == NULL) {
			/* this is a fatal error */
			*errnop = errno = ENOENT;
			return NSS_STATUS_UNAVAIL;
		}
		len = strlen(tmp)+1;
		if (bufpos + len > buflen) {
			/* buffer too small */
			*errnop = errno = EAGAIN;
			return NSS_STATUS_TRYAGAIN;
		}
		memcpy(&buffer[bufpos], tmp, len);
		result->gr_mem[i] = &buffer[bufpos];
		bufpos += len;
	}

	result->gr_mem[i] = NULL;

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _ldb_nss_fill_initgr(gid_t group,
				long int limit,
				long int *start,
				long int *size,
				gid_t **groups,
				int *errnop,
				struct ldb_result *grlist)
{
	NSS_STATUS ret;
	unsigned int i;

	for (i = 0; i < grlist->count; i++) {

		if (limit && (*start > limit)) {
			/* TODO: warn no all groups were reported */
			*errnop = 0;
			ret = NSS_STATUS_SUCCESS;
			goto done;
		}

		if (*start == *size) {
			/* buffer full, enlarge it */
			long int gs;
			gid_t *gm;

			gs = (*size) + 32;
			if (limit && (gs > limit)) {
				gs = limit;
			}

			gm = (gid_t *)realloc((*groups), gs * sizeof(gid_t));
			if ( ! gm) {
				*errnop = ENOMEM;
				ret = NSS_STATUS_UNAVAIL;
				goto done;
			}

			*groups = gm;
			*size = gs;
		}

		(*groups)[*start] = ldb_msg_find_attr_as_int(grlist->msgs[i], "gidNumber", 0);
		if ((*groups)[*start] == 0 || (*groups)[*start] == group) {
			/* skip root group or primary group */
			continue;
		}
		(*start)++;

	}

	*errnop = 0;
	ret = NSS_STATUS_SUCCESS;
done:
	return ret;
}

#define _LDB_NSS_ALLOC_CHECK(mem) do { if (!mem) { errno = ENOMEM; return NSS_STATUS_UNAVAIL; } } while(0)

NSS_STATUS _ldb_nss_group_request(struct ldb_result **_res,
					struct ldb_dn *group_dn,
					const char * const *attrs,
					const char *mattr)
{
	struct ldb_control **ctrls;
	struct ldb_control *ctrl;
	struct ldb_asq_control *asqc;
	struct ldb_request *req;
	int ret;
	struct ldb_result *res = *_res;

	ctrls = talloc_array(res, struct ldb_control *, 2);
	_LDB_NSS_ALLOC_CHECK(ctrls);

	ctrl = talloc(ctrls, struct ldb_control);
	_LDB_NSS_ALLOC_CHECK(ctrl);

	asqc = talloc(ctrl, struct ldb_asq_control);
	_LDB_NSS_ALLOC_CHECK(asqc);

	asqc->source_attribute = talloc_strdup(asqc, mattr);
	_LDB_NSS_ALLOC_CHECK(asqc->source_attribute);

	asqc->request = 1;
	asqc->src_attr_len = strlen(asqc->source_attribute);
	ctrl->oid = LDB_CONTROL_ASQ_OID;
	ctrl->critical = 1;
	ctrl->data = asqc;
	ctrls[0] = ctrl;
	ctrls[1] = NULL;

	ret = ldb_build_search_req(
				&req,
				_ldb_nss_ctx->ldb,
				res,
				group_dn,
				LDB_SCOPE_BASE,
				"(objectClass=*)",
				attrs,
				ctrls,
				res,
				ldb_search_default_callback);
	
	if (ret != LDB_SUCCESS) {
		errno = ENOENT;
		return NSS_STATUS_UNAVAIL;
	}

	ldb_set_timeout(_ldb_nss_ctx->ldb, req, 0);

	ret = ldb_request(_ldb_nss_ctx->ldb, req);

	if (ret == LDB_SUCCESS) {
		ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	} else {
		talloc_free(req);
		return NSS_STATUS_UNAVAIL;
	}

	talloc_free(req);
	return NSS_STATUS_SUCCESS;
}

