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

const char *_ldb_nss_gr_attrs[] = {
	"cn",
	"userPassword",
	"gidNumber",
	NULL
};

const char *_ldb_nss_mem_attrs[] = {
	"uid",
	NULL
};

#define _NSS_LDB_ENOMEM(amem) \
	do { \
		if ( ! amem) { \
			errno = ENOMEM; \
			talloc_free(memctx); \
			return NSS_STATUS_UNAVAIL; \
		} \
	} while(0)

/* This setgrent, getgrent, endgrent is not very efficient */

NSS_STATUS _nss_ldb_setgrent(void)
{
	int ret;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	_ldb_nss_ctx->gr_cur = 0;
	if (_ldb_nss_ctx->gr_res != NULL) {
		talloc_free(_ldb_nss_ctx->gr_res);
		_ldb_nss_ctx->gr_res = NULL;
	}

	ret = ldb_search(_ldb_nss_ctx->ldb,
			 _ldb_nss_ctx->ldb,
			 &_ldb_nss_ctx->gr_res,
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 _ldb_nss_gr_attrs,
			 _LDB_NSS_GRENT_FILTER);
	if (ret != LDB_SUCCESS) {
		return NSS_STATUS_UNAVAIL;
	}

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _nss_ldb_endgrent(void)
{
	int ret;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	_ldb_nss_ctx->gr_cur = 0;
	if (_ldb_nss_ctx->gr_res) {
		talloc_free(_ldb_nss_ctx->gr_res);
		_ldb_nss_ctx->gr_res = NULL;
	}

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _nss_ldb_getgrent_r(struct group *result_buf, char *buffer, size_t buflen, int *errnop)
{
	int ret;
	struct ldb_result *res;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	*errnop = 0;

	if (_ldb_nss_ctx->gr_cur >= _ldb_nss_ctx->gr_res->count) {
		/* already returned all entries */
		return NSS_STATUS_NOTFOUND;
	}

	res = talloc_zero(_ldb_nss_ctx->gr_res, struct ldb_result);
	if ( ! res) {
		errno = *errnop = ENOMEM;
		_ldb_nss_ctx->gr_cur++; /* skip this entry */
		return NSS_STATUS_UNAVAIL;
	}

	ret = _ldb_nss_group_request(&res,
				_ldb_nss_ctx->gr_res->msgs[_ldb_nss_ctx->gr_cur]->dn, 
				_ldb_nss_mem_attrs,
				"member");

	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno;
		talloc_free(res);
		_ldb_nss_ctx->gr_cur++; /* skip this entry */
		return ret;
	}

	ret = _ldb_nss_fill_group(result_buf,
				buffer,
				buflen,
				errnop,
				_ldb_nss_ctx->gr_res->msgs[_ldb_nss_ctx->gr_cur],
				res);

	talloc_free(res);

	if (ret != NSS_STATUS_SUCCESS) {
		if (ret != NSS_STATUS_TRYAGAIN) {
			_ldb_nss_ctx->gr_cur++; /* skip this entry */
		}
		return ret;
	}

	/* this entry is ok, increment counter to nex entry */
	_ldb_nss_ctx->gr_cur++;

	return NSS_STATUS_SUCCESS;
}

NSS_STATUS _nss_ldb_getgrnam_r(const char *name, struct group *result_buf, char *buffer, size_t buflen, int *errnop)
{
	int ret;
	char *filter;
	TALLOC_CTX *ctx;
	struct ldb_result *gr_res;
	struct ldb_result *mem_res;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	ctx = talloc_new(_ldb_nss_ctx->ldb);
	if ( ! ctx) {
		*errnop = errno = ENOMEM;
		return NSS_STATUS_UNAVAIL;
	}

	/* build the filter for this uid */
	filter = talloc_asprintf(ctx, _LDB_NSS_GRNAM_FILTER, name);
	if (filter == NULL) {
		/* this is a fatal error */
		*errnop = errno = ENOMEM;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* search the entry */
	ret = ldb_search(_ldb_nss_ctx->ldb,
			 _ldb_nss_ctx->ldb,
			 &gr_res,
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 _ldb_nss_gr_attrs,
			 filter);
	if (ret != LDB_SUCCESS) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	talloc_steal(ctx, gr_res);

	/* if none found return */
	if (gr_res->count == 0) {
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_NOTFOUND;
		goto done;
	}

	if (gr_res->count != 1) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	mem_res = talloc_zero(ctx, struct ldb_result);
	if ( ! mem_res) {
		errno = *errnop = ENOMEM;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	ret = _ldb_nss_group_request(&mem_res,
					gr_res->msgs[0]->dn,
					_ldb_nss_mem_attrs,
					"member");

	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno;
		goto done;
	}

	ret = _ldb_nss_fill_group(result_buf,
				buffer,
				buflen,
				errnop,
				gr_res->msgs[0],
				mem_res);

	if (ret != NSS_STATUS_SUCCESS) {
		goto done;
	}

	ret = NSS_STATUS_SUCCESS;
done:
	talloc_free(ctx);
	return ret;
}

NSS_STATUS _nss_ldb_getgrgid_r(gid_t gid, struct group *result_buf, char *buffer, size_t buflen, int *errnop)
{
	int ret;
	char *filter;
	TALLOC_CTX *ctx;
	struct ldb_result *gr_res;
	struct ldb_result *mem_res;

	if (gid == 0) { /* we don't serve root gid by policy */
		*errnop = errno = ENOENT;
		return NSS_STATUS_NOTFOUND;
	}

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	ctx = talloc_new(_ldb_nss_ctx->ldb);
	if ( ! ctx) {
		*errnop = errno = ENOMEM;
		return NSS_STATUS_UNAVAIL;
	}

	/* build the filter for this uid */
	filter = talloc_asprintf(ctx, _LDB_NSS_GRGID_FILTER, gid);
	if (filter == NULL) {
		/* this is a fatal error */
		*errnop = errno = ENOMEM;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* search the entry */
	ret = ldb_search(_ldb_nss_ctx->ldb,
			 _ldb_nss_ctx->ldb,
			 &gr_res,
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 _ldb_nss_gr_attrs,
			 filter);
	if (ret != LDB_SUCCESS) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	talloc_steal(ctx, gr_res);

	/* if none found return */
	if (gr_res->count == 0) {
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_NOTFOUND;
		goto done;
	}

	if (gr_res->count != 1) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	mem_res = talloc_zero(ctx, struct ldb_result);
	if ( ! mem_res) {
		errno = *errnop = ENOMEM;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	ret = _ldb_nss_group_request(&mem_res,
					gr_res->msgs[0]->dn,
					_ldb_nss_mem_attrs,
					"member");

	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno;
		goto done;
	}

	ret = _ldb_nss_fill_group(result_buf,
				buffer,
				buflen,
				errnop,
				gr_res->msgs[0],
				mem_res);

	if (ret != NSS_STATUS_SUCCESS) {
		goto done;
	}

	ret = NSS_STATUS_SUCCESS;
done:
	talloc_free(ctx);
	return ret;
}

NSS_STATUS _nss_ldb_initgroups_dyn(const char *user, gid_t group, long int *start, long int *size, gid_t **groups, long int limit, int *errnop)
{
	int ret;
	char *filter;
	const char * attrs[] = { "uidNumber", "gidNumber", NULL };
	struct ldb_result *uid_res;
	struct ldb_result *mem_res;

	ret = _ldb_nss_init();
	if (ret != NSS_STATUS_SUCCESS) {
		return ret;
	}

	mem_res = talloc_zero(_ldb_nss_ctx, struct ldb_result);
	if ( ! mem_res) {
		errno = *errnop = ENOMEM;
		return NSS_STATUS_UNAVAIL;
	}

	/* build the filter for this name */
	filter = talloc_asprintf(mem_res, _LDB_NSS_PWNAM_FILTER, user);
	if (filter == NULL) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	/* search the entry */
	ret = ldb_search(_ldb_nss_ctx->ldb,
			 _ldb_nss_ctx->ldb,
			 &uid_res,
			 _ldb_nss_ctx->base,
			 LDB_SCOPE_SUBTREE,
			 attrs,
			 filter);
	if (ret != LDB_SUCCESS) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	talloc_steal(mem_res, uid_res);

	/* if none found return */
	if (uid_res->count == 0) {
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_NOTFOUND;
		goto done;
	}

	if (uid_res->count != 1) {
		/* this is a fatal error */
		*errnop = errno = ENOENT;
		ret = NSS_STATUS_UNAVAIL;
		goto done;
	}

	ret = _ldb_nss_group_request(&mem_res,
					uid_res->msgs[0]->dn,
					attrs,
					"memberOf");

	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno;
		goto done;
	}

	ret = _ldb_nss_fill_initgr(group,
				limit,
				start,
				size,
				groups,
				errnop,
				mem_res);

	if (ret != NSS_STATUS_SUCCESS) {
		goto done;
	}

	ret = NSS_STATUS_SUCCESS;

done:
	talloc_free(mem_res);
	return ret;
}
