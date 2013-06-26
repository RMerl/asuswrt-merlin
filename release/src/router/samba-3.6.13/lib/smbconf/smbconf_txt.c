/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library, text backend
 *  Copyright (C) Michael Adam 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This is a sample implementation of a libsmbconf text backend
 * using the params.c parser.
 *
 * It is read only.
 * Don't expect brilliant performance, since it is not hashing the lists.
 */

#include "includes.h"
#include "smbconf_private.h"
#include "lib/smbconf/smbconf_txt.h"

struct txt_cache {
	uint32_t current_share;
	uint32_t num_shares;
	char **share_names;
	uint32_t *num_params;
	char ***param_names;
	char ***param_values;
};

struct txt_private_data {
	struct txt_cache *cache;
	uint64_t csn;
	bool verbatim;
};

/**********************************************************************
 *
 * helper functions
 *
 **********************************************************************/

/**
 * a convenience helper to cast the private data structure
 */
static struct txt_private_data *pd(struct smbconf_ctx *ctx)
{
	return (struct txt_private_data *)(ctx->data);
}

static bool smbconf_txt_do_section(const char *section, void *private_data)
{
	sbcErr err;
	uint32_t idx;
	struct txt_private_data *tpd = (struct txt_private_data *)private_data;
	struct txt_cache *cache = tpd->cache;

	if (smbconf_find_in_array(section, cache->share_names,
				  cache->num_shares, &idx))
	{
		cache->current_share = idx;
		return true;
	}

	err = smbconf_add_string_to_array(cache, &(cache->share_names),
					  cache->num_shares, section);
	if (!SBC_ERROR_IS_OK(err)) {
		return false;
	}
	cache->current_share = cache->num_shares;
	cache->num_shares++;

	cache->param_names = talloc_realloc(cache,
						  cache->param_names,
						  char **,
						  cache->num_shares);
	if (cache->param_names == NULL) {
		return false;
	}
	cache->param_names[cache->current_share] = NULL;

	cache->param_values = talloc_realloc(cache,
						   cache->param_values,
						   char **,
						   cache->num_shares);
	if (cache->param_values == NULL) {
		return false;
	}
	cache->param_values[cache->current_share] = NULL;

	cache->num_params = talloc_realloc(cache,
						 cache->num_params,
						 uint32_t,
						 cache->num_shares);
	if (cache->num_params == NULL) {
		return false;
	}
	cache->num_params[cache->current_share] = 0;

	return true;
}

static bool smbconf_txt_do_parameter(const char *param_name,
				     const char *param_value,
				     void *private_data)
{
	sbcErr err;
	char **param_names, **param_values;
	uint32_t num_params;
	uint32_t idx;
	struct txt_private_data *tpd = (struct txt_private_data *)private_data;
	struct txt_cache *cache = tpd->cache;

	if (cache->num_shares == 0) {
		/*
		 * not in any share yet,
		 * initialize the "empty" section (NULL):
		 * parameters without a previous [section] are stored here.
		 */
		if (!smbconf_txt_do_section(NULL, private_data)) {
			return false;
		}
	}

	param_names  = cache->param_names[cache->current_share];
	param_values = cache->param_values[cache->current_share];
	num_params   = cache->num_params[cache->current_share];

	if (!(tpd->verbatim) &&
	    smbconf_find_in_array(param_name, param_names, num_params, &idx))
	{
		talloc_free(param_values[idx]);
		param_values[idx] = talloc_strdup(cache, param_value);
		if (param_values[idx] == NULL) {
			return false;
		}
		return true;
	}
	err = smbconf_add_string_to_array(cache,
				&(cache->param_names[cache->current_share]),
				num_params, param_name);
	if (!SBC_ERROR_IS_OK(err)) {
		return false;
	}
	err = smbconf_add_string_to_array(cache,
				&(cache->param_values[cache->current_share]),
				num_params, param_value);
	cache->num_params[cache->current_share]++;
	return SBC_ERROR_IS_OK(err);
}

static void smbconf_txt_flush_cache(struct smbconf_ctx *ctx)
{
	talloc_free(pd(ctx)->cache);
	pd(ctx)->cache = NULL;
}

static sbcErr smbconf_txt_init_cache(struct smbconf_ctx *ctx)
{
	if (pd(ctx)->cache != NULL) {
		smbconf_txt_flush_cache(ctx);
	}

	pd(ctx)->cache = talloc_zero(pd(ctx), struct txt_cache);

	if (pd(ctx)->cache == NULL) {
		return SBC_ERR_NOMEM;
	}

	return SBC_ERR_OK;
}

static sbcErr smbconf_txt_load_file(struct smbconf_ctx *ctx)
{
	sbcErr err;
	uint64_t new_csn;

	if (!file_exist(ctx->path)) {
		return SBC_ERR_BADFILE;
	}

	new_csn = (uint64_t)file_modtime(ctx->path);
	if (new_csn == pd(ctx)->csn) {
		return SBC_ERR_OK;
	}

	err = smbconf_txt_init_cache(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	if (!pm_process(ctx->path, smbconf_txt_do_section,
			smbconf_txt_do_parameter, pd(ctx)))
	{
		return SBC_ERR_CAN_NOT_COMPLETE;
	}

	pd(ctx)->csn = new_csn;

	return SBC_ERR_OK;
}


/**********************************************************************
 *
 * smbconf operations: text backend implementations
 *
 **********************************************************************/

/**
 * initialize the text based smbconf backend
 */
static sbcErr smbconf_txt_init(struct smbconf_ctx *ctx, const char *path)
{
	if (path == NULL) {
		return SBC_ERR_BADFILE;
	}
	ctx->path = talloc_strdup(ctx, path);
	if (ctx->path == NULL) {
		return SBC_ERR_NOMEM;
	}

	ctx->data = talloc_zero(ctx, struct txt_private_data);
	if (ctx->data == NULL) {
		return SBC_ERR_NOMEM;
	}

	pd(ctx)->verbatim = true;

	return SBC_ERR_OK;
}

static int smbconf_txt_shutdown(struct smbconf_ctx *ctx)
{
	return ctx->ops->close_conf(ctx);
}

static bool smbconf_txt_requires_messaging(struct smbconf_ctx *ctx)
{
	return false;
}

static bool smbconf_txt_is_writeable(struct smbconf_ctx *ctx)
{
	/* no write support in this backend yet... */
	return false;
}

static sbcErr smbconf_txt_open(struct smbconf_ctx *ctx)
{
	return smbconf_txt_load_file(ctx);
}

static int smbconf_txt_close(struct smbconf_ctx *ctx)
{
	smbconf_txt_flush_cache(ctx);
	return 0;
}

/**
 * Get the change sequence number of the given service/parameter.
 * service and parameter strings may be NULL.
 */
static void smbconf_txt_get_csn(struct smbconf_ctx *ctx,
				struct smbconf_csn *csn,
				const char *service, const char *param)
{
	if (csn == NULL) {
		return;
	}

	csn->csn = (uint64_t)file_modtime(ctx->path);
}

/**
 * Drop the whole configuration (restarting empty)
 */
static sbcErr smbconf_txt_drop(struct smbconf_ctx *ctx)
{
	return SBC_ERR_NOT_SUPPORTED;
}

/**
 * get the list of share names defined in the configuration.
 */
static sbcErr smbconf_txt_get_share_names(struct smbconf_ctx *ctx,
					  TALLOC_CTX *mem_ctx,
					  uint32_t *num_shares,
					  char ***share_names)
{
	uint32_t count;
	uint32_t added_count = 0;
	TALLOC_CTX *tmp_ctx = NULL;
	sbcErr err = SBC_ERR_OK;
	char **tmp_share_names = NULL;

	if ((num_shares == NULL) || (share_names == NULL)) {
		return SBC_ERR_INVALID_PARAM;
	}

	err = smbconf_txt_load_file(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	tmp_ctx = talloc_stackframe();

	/* make sure "global" is always listed first,
	 * possibly after NULL section */

	if (smbconf_share_exists(ctx, NULL)) {
		err = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
						  0, NULL);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		added_count++;
	}

	if (smbconf_share_exists(ctx, GLOBAL_NAME)) {
		err = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
						   added_count, GLOBAL_NAME);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		added_count++;
	}

	for (count = 0; count < pd(ctx)->cache->num_shares; count++) {
		if (strequal(pd(ctx)->cache->share_names[count], GLOBAL_NAME) ||
		    (pd(ctx)->cache->share_names[count] == NULL))
		{
			continue;
		}

		err = smbconf_add_string_to_array(tmp_ctx, &tmp_share_names,
					added_count,
					pd(ctx)->cache->share_names[count]);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		added_count++;
	}

	*num_shares = added_count;
	if (added_count > 0) {
		*share_names = talloc_move(mem_ctx, &tmp_share_names);
	} else {
		*share_names = NULL;
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * check if a share/service of a given name exists
 */
static bool smbconf_txt_share_exists(struct smbconf_ctx *ctx,
				     const char *servicename)
{
	sbcErr err;

	err = smbconf_txt_load_file(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return false;
	}

	return smbconf_find_in_array(servicename,
				     pd(ctx)->cache->share_names,
				     pd(ctx)->cache->num_shares, NULL);
}

/**
 * Add a service if it does not already exist
 */
static sbcErr smbconf_txt_create_share(struct smbconf_ctx *ctx,
				       const char *servicename)
{
	return SBC_ERR_NOT_SUPPORTED;
}

/**
 * get a definition of a share (service) from configuration.
 */
static sbcErr smbconf_txt_get_share(struct smbconf_ctx *ctx,
				    TALLOC_CTX *mem_ctx,
				    const char *servicename,
				    struct smbconf_service **service)
{
	sbcErr err;
	uint32_t sidx, count;
	bool found;
	TALLOC_CTX *tmp_ctx = NULL;
	struct smbconf_service *tmp_service = NULL;

	err = smbconf_txt_load_file(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	found = smbconf_find_in_array(servicename,
				      pd(ctx)->cache->share_names,
				      pd(ctx)->cache->num_shares,
				      &sidx);
	if (!found) {
		return SBC_ERR_NO_SUCH_SERVICE;
	}

	tmp_ctx = talloc_stackframe();

	tmp_service = talloc_zero(tmp_ctx, struct smbconf_service);
	if (tmp_service == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	if (servicename != NULL) {
		tmp_service->name = talloc_strdup(tmp_service, servicename);
		if (tmp_service->name == NULL) {
			err = SBC_ERR_NOMEM;
			goto done;
		}
	}

	for (count = 0; count < pd(ctx)->cache->num_params[sidx]; count++) {
		err = smbconf_add_string_to_array(tmp_service,
				&(tmp_service->param_names),
				count,
				pd(ctx)->cache->param_names[sidx][count]);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
		err = smbconf_add_string_to_array(tmp_service,
				&(tmp_service->param_values),
				count,
				pd(ctx)->cache->param_values[sidx][count]);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
	}

	tmp_service->num_params = count;
	*service = talloc_move(mem_ctx, &tmp_service);

done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * delete a service from configuration
 */
static sbcErr smbconf_txt_delete_share(struct smbconf_ctx *ctx,
				       const char *servicename)
{
	return SBC_ERR_NOT_SUPPORTED;
}

/**
 * set a configuration parameter to the value provided.
 */
static sbcErr smbconf_txt_set_parameter(struct smbconf_ctx *ctx,
					const char *service,
					const char *param,
					const char *valstr)
{
	return SBC_ERR_NOT_SUPPORTED;
}

/**
 * get the value of a configuration parameter as a string
 */
static sbcErr smbconf_txt_get_parameter(struct smbconf_ctx *ctx,
					TALLOC_CTX *mem_ctx,
					const char *service,
					const char *param,
					char **valstr)
{
	sbcErr err;
	bool found;
	uint32_t share_index, param_index;

	err = smbconf_txt_load_file(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	found = smbconf_find_in_array(service,
				      pd(ctx)->cache->share_names,
				      pd(ctx)->cache->num_shares,
				      &share_index);
	if (!found) {
		return SBC_ERR_NO_SUCH_SERVICE;
	}

	found = smbconf_reverse_find_in_array(param,
				pd(ctx)->cache->param_names[share_index],
				pd(ctx)->cache->num_params[share_index],
				&param_index);
	if (!found) {
		return SBC_ERR_INVALID_PARAM;
	}

	*valstr = talloc_strdup(mem_ctx,
			pd(ctx)->cache->param_values[share_index][param_index]);

	if (*valstr == NULL) {
		return SBC_ERR_NOMEM;
	}

	return SBC_ERR_OK;
}

/**
 * delete a parameter from configuration
 */
static sbcErr smbconf_txt_delete_parameter(struct smbconf_ctx *ctx,
					   const char *service,
					   const char *param)
{
	return SBC_ERR_NOT_SUPPORTED;
}

static sbcErr smbconf_txt_get_includes(struct smbconf_ctx *ctx,
				       TALLOC_CTX *mem_ctx,
				       const char *service,
				       uint32_t *num_includes,
				       char ***includes)
{
	sbcErr err;
	bool found;
	uint32_t sidx, count;
	TALLOC_CTX *tmp_ctx = NULL;
	uint32_t tmp_num_includes = 0;
	char **tmp_includes = NULL;

	err = smbconf_txt_load_file(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	found = smbconf_find_in_array(service,
				      pd(ctx)->cache->share_names,
				      pd(ctx)->cache->num_shares,
				      &sidx);
	if (!found) {
		return SBC_ERR_NO_SUCH_SERVICE;
	}

	tmp_ctx = talloc_stackframe();

	for (count = 0; count < pd(ctx)->cache->num_params[sidx]; count++) {
		if (strequal(pd(ctx)->cache->param_names[sidx][count],
			     "include"))
		{
			err = smbconf_add_string_to_array(tmp_ctx,
				&tmp_includes,
				tmp_num_includes,
				pd(ctx)->cache->param_values[sidx][count]);
			if (!SBC_ERROR_IS_OK(err)) {
				goto done;
			}
			tmp_num_includes++;
		}
	}

	*num_includes = tmp_num_includes;
	if (*num_includes > 0) {
		*includes = talloc_move(mem_ctx, &tmp_includes);
		if (*includes == NULL) {
			err = SBC_ERR_NOMEM;
			goto done;
		}
	} else {
		*includes = NULL;
	}

	err = SBC_ERR_OK;

done:
	talloc_free(tmp_ctx);
	return err;
}

static sbcErr smbconf_txt_set_includes(struct smbconf_ctx *ctx,
				       const char *service,
				       uint32_t num_includes,
				       const char **includes)
{
	return SBC_ERR_NOT_SUPPORTED;
}

static sbcErr smbconf_txt_delete_includes(struct smbconf_ctx *ctx,
					  const char *service)
{
	return SBC_ERR_NOT_SUPPORTED;
}

static sbcErr smbconf_txt_transaction_start(struct smbconf_ctx *ctx)
{
	return SBC_ERR_OK;
}

static sbcErr smbconf_txt_transaction_commit(struct smbconf_ctx *ctx)
{
	return SBC_ERR_OK;
}

static sbcErr smbconf_txt_transaction_cancel(struct smbconf_ctx *ctx)
{
	return SBC_ERR_OK;
}

static struct smbconf_ops smbconf_ops_txt = {
	.init			= smbconf_txt_init,
	.shutdown		= smbconf_txt_shutdown,
	.requires_messaging	= smbconf_txt_requires_messaging,
	.is_writeable		= smbconf_txt_is_writeable,
	.open_conf		= smbconf_txt_open,
	.close_conf		= smbconf_txt_close,
	.get_csn		= smbconf_txt_get_csn,
	.drop			= smbconf_txt_drop,
	.get_share_names	= smbconf_txt_get_share_names,
	.share_exists		= smbconf_txt_share_exists,
	.create_share		= smbconf_txt_create_share,
	.get_share		= smbconf_txt_get_share,
	.delete_share		= smbconf_txt_delete_share,
	.set_parameter		= smbconf_txt_set_parameter,
	.get_parameter		= smbconf_txt_get_parameter,
	.delete_parameter	= smbconf_txt_delete_parameter,
	.get_includes		= smbconf_txt_get_includes,
	.set_includes		= smbconf_txt_set_includes,
	.delete_includes	= smbconf_txt_delete_includes,
	.transaction_start	= smbconf_txt_transaction_start,
	.transaction_commit	= smbconf_txt_transaction_commit,
	.transaction_cancel	= smbconf_txt_transaction_cancel,
};


/**
 * initialize the smbconf text backend
 * the only function that is exported from this module
 */
sbcErr smbconf_init_txt(TALLOC_CTX *mem_ctx,
			struct smbconf_ctx **conf_ctx,
			const char *path)
{
	sbcErr err;

	err = smbconf_init_internal(mem_ctx, conf_ctx, path, &smbconf_ops_txt);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	return smbconf_txt_load_file(*conf_ctx);
}
