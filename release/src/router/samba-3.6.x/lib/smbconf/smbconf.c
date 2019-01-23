/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library
 *  Copyright (C) Michael Adam 2007-2008
 *  Copyright (C) Guenther Deschner 2007
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

#include "includes.h"
#include "smbconf_private.h"

/**********************************************************************
 *
 * internal helper functions
 *
 **********************************************************************/

static sbcErr smbconf_global_check(struct smbconf_ctx *ctx)
{
	if (!smbconf_share_exists(ctx, GLOBAL_NAME)) {
		return smbconf_create_share(ctx, GLOBAL_NAME);
	}

	return SBC_ERR_OK;
}


/**********************************************************************
 *
 * The actual libsmbconf API functions that are exported.
 *
 **********************************************************************/

const char *sbcErrorString(sbcErr error)
{
	switch (error) {
		case SBC_ERR_OK:
			return "SBC_ERR_OK";
		case SBC_ERR_NOT_IMPLEMENTED:
			return "SBC_ERR_NOT_IMPLEMENTED";
		case SBC_ERR_NOT_SUPPORTED:
			return "SBC_ERR_NOT_SUPPORTED";
		case SBC_ERR_UNKNOWN_FAILURE:
			return "SBC_ERR_UNKNOWN_FAILURE";
		case SBC_ERR_NOMEM:
			return "SBC_ERR_NOMEM";
		case SBC_ERR_INVALID_PARAM:
			return "SBC_ERR_INVALID_PARAM";
		case SBC_ERR_BADFILE:
			return "SBC_ERR_BADFILE";
		case SBC_ERR_NO_SUCH_SERVICE:
			return "SBC_ERR_NO_SUCH_SERVICE";
		case SBC_ERR_IO_FAILURE:
			return "SBC_ERR_IO_FAILURE";
		case SBC_ERR_CAN_NOT_COMPLETE:
			return "SBC_ERR_CAN_NOT_COMPLETE";
		case SBC_ERR_NO_MORE_ITEMS:
			return "SBC_ERR_NO_MORE_ITEMS";
		case SBC_ERR_FILE_EXISTS:
			return "SBC_ERR_FILE_EXISTS";
		case SBC_ERR_ACCESS_DENIED:
			return "SBC_ERR_ACCESS_DENIED";
	}

	return "unknown sbcErr value";
}


/**
 * Tell whether the backend requires messaging to be set up
 * for the backend to work correctly.
 */
bool smbconf_backend_requires_messaging(struct smbconf_ctx *ctx)
{
	return ctx->ops->requires_messaging(ctx);
}

/**
 * Tell whether the source is writeable.
 */
bool smbconf_is_writeable(struct smbconf_ctx *ctx)
{
	return ctx->ops->is_writeable(ctx);
}

/**
 * Close the configuration.
 */
void smbconf_shutdown(struct smbconf_ctx *ctx)
{
	talloc_free(ctx);
}

/**
 * Detect changes in the configuration.
 * The given csn struct is filled with the current csn.
 * smbconf_changed() can also be used for initial retrieval
 * of the csn.
 */
bool smbconf_changed(struct smbconf_ctx *ctx, struct smbconf_csn *csn,
		     const char *service, const char *param)
{
	struct smbconf_csn old_csn;

	if (csn == NULL) {
		return false;
	}

	old_csn = *csn;

	ctx->ops->get_csn(ctx, csn, service, param);
	return (csn->csn != old_csn.csn);
}

/**
 * Drop the whole configuration (restarting empty).
 */
sbcErr smbconf_drop(struct smbconf_ctx *ctx)
{
	return ctx->ops->drop(ctx);
}

/**
 * Get the whole configuration as lists of strings with counts:
 *
 *  num_shares   : number of shares
 *  share_names  : list of length num_shares of share names
 *  num_params   : list of length num_shares of parameter counts for each share
 *  param_names  : list of lists of parameter names for each share
 *  param_values : list of lists of parameter values for each share
 */
sbcErr smbconf_get_config(struct smbconf_ctx *ctx,
			  TALLOC_CTX *mem_ctx,
			  uint32_t *num_shares,
			  struct smbconf_service ***services)
{
	sbcErr err;
	TALLOC_CTX *tmp_ctx = NULL;
	uint32_t tmp_num_shares;
	char **tmp_share_names;
	struct smbconf_service **tmp_services;
	uint32_t count;

	if ((num_shares == NULL) || (services == NULL)) {
		err = SBC_ERR_INVALID_PARAM;
		goto done;
	}

	tmp_ctx = talloc_stackframe();

	err = smbconf_get_share_names(ctx, tmp_ctx, &tmp_num_shares,
				      &tmp_share_names);
	if (!SBC_ERROR_IS_OK(err)) {
		goto done;
	}

	tmp_services = talloc_array(tmp_ctx, struct smbconf_service *,
				    tmp_num_shares);
	if (tmp_services == NULL) {
		err = SBC_ERR_NOMEM;
		goto done;
	}

	for (count = 0; count < tmp_num_shares; count++) {
		err = smbconf_get_share(ctx, tmp_services,
					tmp_share_names[count],
					&tmp_services[count]);
		if (!SBC_ERROR_IS_OK(err)) {
			goto done;
		}
	}

	err = SBC_ERR_OK;

	*num_shares = tmp_num_shares;
	if (tmp_num_shares > 0) {
		*services = talloc_move(mem_ctx, &tmp_services);
	} else {
		*services = NULL;
	}

done:
	talloc_free(tmp_ctx);
	return err;
}

/**
 * get the list of share names defined in the configuration.
 */
sbcErr smbconf_get_share_names(struct smbconf_ctx *ctx,
			       TALLOC_CTX *mem_ctx,
			       uint32_t *num_shares,
			       char ***share_names)
{
	return ctx->ops->get_share_names(ctx, mem_ctx, num_shares,
					 share_names);
}

/**
 * check if a share/service of a given name exists
 */
bool smbconf_share_exists(struct smbconf_ctx *ctx,
			  const char *servicename)
{
	return ctx->ops->share_exists(ctx, servicename);
}

/**
 * Add a service if it does not already exist.
 */
sbcErr smbconf_create_share(struct smbconf_ctx *ctx,
			    const char *servicename)
{
	if ((servicename != NULL) && smbconf_share_exists(ctx, servicename)) {
		return SBC_ERR_FILE_EXISTS;
	}

	return ctx->ops->create_share(ctx, servicename);
}

/**
 * get a definition of a share (service) from configuration.
 */
sbcErr smbconf_get_share(struct smbconf_ctx *ctx,
			 TALLOC_CTX *mem_ctx,
			 const char *servicename,
			 struct smbconf_service **service)
{
	return ctx->ops->get_share(ctx, mem_ctx, servicename, service);
}

/**
 * delete a service from configuration
 */
sbcErr smbconf_delete_share(struct smbconf_ctx *ctx, const char *servicename)
{
	if (!smbconf_share_exists(ctx, servicename)) {
		return SBC_ERR_NO_SUCH_SERVICE;
	}

	return ctx->ops->delete_share(ctx, servicename);
}

/**
 * set a configuration parameter to the value provided.
 */
sbcErr smbconf_set_parameter(struct smbconf_ctx *ctx,
			     const char *service,
			     const char *param,
			     const char *valstr)
{
	return ctx->ops->set_parameter(ctx, service, param, valstr);
}

/**
 * Set a global parameter
 * (i.e. a parameter in the [global] service).
 *
 * This also creates [global] when it does not exist.
 */
sbcErr smbconf_set_global_parameter(struct smbconf_ctx *ctx,
				    const char *param, const char *val)
{
	sbcErr err;

	err = smbconf_global_check(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}
	err = smbconf_set_parameter(ctx, GLOBAL_NAME, param, val);

	return err;
}

/**
 * get the value of a configuration parameter as a string
 */
sbcErr smbconf_get_parameter(struct smbconf_ctx *ctx,
			     TALLOC_CTX *mem_ctx,
			     const char *service,
			     const char *param,
			     char **valstr)
{
	if (valstr == NULL) {
		return SBC_ERR_INVALID_PARAM;
	}

	return ctx->ops->get_parameter(ctx, mem_ctx, service, param, valstr);
}

/**
 * Get the value of a global parameter.
 *
 * Create [global] if it does not exist.
 */
sbcErr smbconf_get_global_parameter(struct smbconf_ctx *ctx,
				    TALLOC_CTX *mem_ctx,
				    const char *param,
				    char **valstr)
{
	sbcErr err;

	err = smbconf_global_check(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}

	err = smbconf_get_parameter(ctx, mem_ctx, GLOBAL_NAME, param,
				    valstr);

	return err;
}

/**
 * delete a parameter from configuration
 */
sbcErr smbconf_delete_parameter(struct smbconf_ctx *ctx,
				const char *service, const char *param)
{
	return ctx->ops->delete_parameter(ctx, service, param);
}

/**
 * Delete a global parameter.
 *
 * Create [global] if it does not exist.
 */
sbcErr smbconf_delete_global_parameter(struct smbconf_ctx *ctx,
				       const char *param)
{
	sbcErr err;

	err = smbconf_global_check(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}
	err = smbconf_delete_parameter(ctx, GLOBAL_NAME, param);

	return err;
}

sbcErr smbconf_get_includes(struct smbconf_ctx *ctx,
			    TALLOC_CTX *mem_ctx,
			    const char *service,
			    uint32_t *num_includes, char ***includes)
{
	return ctx->ops->get_includes(ctx, mem_ctx, service, num_includes,
				      includes);
}

sbcErr smbconf_get_global_includes(struct smbconf_ctx *ctx,
				   TALLOC_CTX *mem_ctx,
				   uint32_t *num_includes, char ***includes)
{
	sbcErr err;

	err = smbconf_global_check(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}
	err = smbconf_get_includes(ctx, mem_ctx, GLOBAL_NAME,
				    num_includes, includes);

	return err;
}

sbcErr smbconf_set_includes(struct smbconf_ctx *ctx,
			    const char *service,
			    uint32_t num_includes, const char **includes)
{
	return ctx->ops->set_includes(ctx, service, num_includes, includes);
}

sbcErr smbconf_set_global_includes(struct smbconf_ctx *ctx,
				   uint32_t num_includes,
				   const char **includes)
{
	sbcErr err;

	err = smbconf_global_check(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}
	err = smbconf_set_includes(ctx, GLOBAL_NAME,
				   num_includes, includes);

	return err;
}


sbcErr smbconf_delete_includes(struct smbconf_ctx *ctx, const char *service)
{
	return ctx->ops->delete_includes(ctx, service);
}

sbcErr smbconf_delete_global_includes(struct smbconf_ctx *ctx)
{
	sbcErr err;

	err = smbconf_global_check(ctx);
	if (!SBC_ERROR_IS_OK(err)) {
		return err;
	}
	err = smbconf_delete_includes(ctx, GLOBAL_NAME);

	return err;
}

sbcErr smbconf_transaction_start(struct smbconf_ctx *ctx)
{
	return ctx->ops->transaction_start(ctx);
}

sbcErr smbconf_transaction_commit(struct smbconf_ctx *ctx)
{
	return ctx->ops->transaction_commit(ctx);
}

sbcErr smbconf_transaction_cancel(struct smbconf_ctx *ctx)
{
	return ctx->ops->transaction_cancel(ctx);
}
