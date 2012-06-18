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

extern struct _ldb_nss_context *_ldb_nss_ctx;

const char *_ldb_nss_pw_attrs[] = {
	"uid",
	"userPassword",
	"uidNumber",
	"gidNumber",
	"gecos",
	"homeDirectory",
	"loginShell",
	NULL
};

NSS_STATUS _nss_ldb_setpwent(void)
{
	int ret;
	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	_ldb_nss_ctx->pw_cur = 0;
	if (_ldb_nss_ctx->pw_res != NULL) {
		talloc_free(_ldb_nss_ctx->pw_res);
		_ldb_nss_ctx->pw_res = NULL;
	}

	ret = ldb_search(_ldb_nss_ctx->ldb, _ldb_nss_ctx->ldb, 
					 &_ldb_nss_ctx->pw_res,
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 _ldb_nss_pw_attrs,
			 _LDB_NSS_PWENT_FILTER);
	if (ret != LDB_SUCCESS) {
		return NSS_STATUS_UNAVAIL;
	}

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _nss_ldb_endpwent(void)
{
	int ret;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	_ldb_nss_ctx->pw_cur = 0;
	if (_ldb_nss_ctx->pw_res) {
		talloc_free(_ldb_nss_ctx->pw_res);
		_ldb_nss_ctx->pw_res = NULL;
	}

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _nss_ldb_getpwent_r(struct passwd *result_buf,
				char *buffer,
				int buflen,
				int *errnop)
{
	int ret;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	*errnop = 0;

	if (_ldb_nss_ctx->pw_cur >= _ldb_nss_ctx->pw_res->count) {
		/* already returned all entries */
		return NSS_STATUS_NOTFOUND;
	}

	ret = _ldb_nss_fill_passwd(result_buf,
				   buffer,
				   buflen,
				   errnop,
				   _ldb_nss_ctx->pw_res->msgs[_ldb_nss_ctx->pw_cur]);
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	_ldb_nss_ctx->pw_cur++;

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _nss_ldb_getpwuid_r(uid_t uid, struct passwd *result_buf, char *buffer, size_t buflen, int *errnop)
{
	int ret;
	struct ldb_result *res;

	if (uid == 0) { /* we don't serve root uid by policy */
		*errnop = errno = ENOENT;
		return NSS_STATUS_NOTFOUND;
	}

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	/* search the entry */
	ret = ldb_search(_ldb_nss_ctx->ldb, _ldb_nss_ctx->ldb, &res, 
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 _ldb_nss_pw_attrs,
			 _LDB_NSS_PWUID_FILTER, uid);
	if (ret != LDB_SUCCESS) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* if none found return */
	if (res->count == 0) {
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_NOTFOUND;
		goto done;
	}

	if (res->count != 1) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* fill in the passwd struct */
	ret = _ldb_nss_fill_passwd(result_buf,
				   buffer,
				   buflen,
				   errnop,
				   res->msgs[0]);

done:
	talloc_free(res);
	return ret;
}

NSS_STATUS _nss_ldb_getpwnam_r(const char *name, struct passwd *result_buf, char *buffer, size_t buflen, int *errnop)
{
	int ret;
	struct ldb_result *res;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	/* search the entry */
	ret = ldb_search(_ldb_nss_ctx->ldb, _ldb_nss_ctx->ldb, &res,
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 _ldb_nss_pw_attrs,
			 _LDB_NSS_PWNAM_FILTER, name);
	if (ret != LDB_SUCCESS) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* if none found return */
	if (res->count == 0) {
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_NOTFOUND;
		goto done;
	}

	if (res->count != 1) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* fill in the passwd struct */
	ret = _ldb_nss_fill_passwd(result_buf,
				   buffer,
				   buflen,
				   errnop,
				   res->msgs[0]);

done:
	talloc_free(res);
	return ret;
}

