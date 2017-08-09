/*
 *  Unix SMB/CIFS implementation.
 *  libsmbconf - Samba configuration library
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

#ifndef __LIBSMBCONF_H__
#define __LIBSMBCONF_H__

/**
 * @defgroup libsmbconf The smbconf API
 *
 * libsmbconf is a library to read or, based on the backend, modify the Samba
 * configuration.
 *
 * @{
 */

/**
 * @brief Status codes returned from smbconf functions
 */
enum _sbcErrType {
	SBC_ERR_OK = 0,          /**< Successful completion **/
	SBC_ERR_NOT_IMPLEMENTED, /**< Function not implemented **/
	SBC_ERR_NOT_SUPPORTED,   /**< Function not supported **/
	SBC_ERR_UNKNOWN_FAILURE, /**< General failure **/
	SBC_ERR_NOMEM,           /**< Memory allocation error **/
	SBC_ERR_INVALID_PARAM,   /**< An Invalid parameter was supplied **/
	SBC_ERR_BADFILE,         /**< A bad file was supplied **/
	SBC_ERR_NO_SUCH_SERVICE, /**< There is no such service provided **/
	SBC_ERR_IO_FAILURE,      /**< There was an IO error **/
	SBC_ERR_CAN_NOT_COMPLETE,/**< Can not complete action **/
	SBC_ERR_NO_MORE_ITEMS,   /**< No more items left **/
	SBC_ERR_FILE_EXISTS,     /**< File already exists **/
	SBC_ERR_ACCESS_DENIED,   /**< Access has been denied **/
};

typedef enum _sbcErrType sbcErr;

#define SBC_ERROR_IS_OK(x) ((x) == SBC_ERR_OK)
#define SBC_ERROR_EQUAL(x,y) ((x) == (y))

struct smbconf_ctx;

/* the change sequence number */
struct smbconf_csn {
	uint64_t csn;
};

/** Information about a service */
struct smbconf_service {
	char *name;          /**< The name of the share */
	uint32_t num_params; /**< List of length num_shares of parameter counts for each share */
	char **param_names;  /**< List of lists of parameter names for each share */
	char **param_values; /**< List of lists of parameter values for each share */
};

/*
 * The smbconf API functions
 */

/**
 * @brief Translate an error value into a string
 *
 * @param error
 *
 * @return a pointer to a static string
 **/
const char *sbcErrorString(sbcErr error);

/**
 * @brief Check if the backend requires messaging to be set up.
 *
 * Tell whether the backend requires messaging to be set up
 * for the backend to work correctly.
 *
 * @param[in] ctx       The smbconf context to check.
 *
 * @return              True if needed, false if not.
 */
bool smbconf_backend_requires_messaging(struct smbconf_ctx *ctx);

/**
 * @brief Tell whether the source is writeable.
 *
 * @param[in] ctx       The smbconf context to check.
 *
 * @return              True if it is writeable, false if not.
 */
bool smbconf_is_writeable(struct smbconf_ctx *ctx);

/**
 * @brief Close the configuration.
 *
 * @param[in] ctx       The smbconf context to close.
 */
void smbconf_shutdown(struct smbconf_ctx *ctx);

/**
 * @brief Detect changes in the configuration.
 *
 * Get the change sequence number of the given service/parameter. Service and
 * parameter strings may be NULL.
 *
 * The given change sequence number (csn) struct is filled with the current
 * csn. smbconf_changed() can also be used for initial retrieval of the csn.
 *
 * @param[in] ctx       The smbconf context to check for changes.
 *
 * @param[inout] csn    The smbconf csn to be filled.
 *
 * @param[in] service   The service name to check or NULL.
 *
 * @param[in] param     The param to check or NULL.
 *
 * @return              True if it has been changed, false if not.
 */
bool smbconf_changed(struct smbconf_ctx *ctx, struct smbconf_csn *csn,
		     const char *service, const char *param);

/**
 * @brief Drop the whole configuration (restarting empty).
 *
 * @param[in] ctx       The smbconf context to drop the config.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_drop(struct smbconf_ctx *ctx);

/**
 * @brief Get the whole configuration as lists of strings with counts.
 *
 * @param[in] ctx       The smbconf context to get the lists from.
 *
 * @param[in] mem_ctx   The memory context to use.
 *
 * @param[in] num_shares A pointer to store the number of shares.
 *
 * @param[out] services  A pointer to store the services.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 *
 * @see smbconf_service
 */
sbcErr smbconf_get_config(struct smbconf_ctx *ctx,
			  TALLOC_CTX *mem_ctx,
			  uint32_t *num_shares,
			  struct smbconf_service ***services);

/**
 * @brief Get the list of share names defined in the configuration.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] mem_ctx   The memory context to use.
 *
 * @param[in] num_shares A pointer to store the number of shares.
 *
 * @param[in] share_names A pointer to store the share names.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_get_share_names(struct smbconf_ctx *ctx,
			       TALLOC_CTX *mem_ctx,
			       uint32_t *num_shares,
			       char ***share_names);

/**
 * @brief Check if a share/service of a given name exists.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] servicename The service name to check if it exists.
 *
 * @return              True if it exists, false if not.
 */
bool smbconf_share_exists(struct smbconf_ctx *ctx, const char *servicename);

/**
 * @brief Add a service if it does not already exist.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] servicename The name of the service to add.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_create_share(struct smbconf_ctx *ctx, const char *servicename);

/**
 * @brief Get a definition of a share (service) from configuration.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] mem_ctx   A memory context to allocate the result.
 *
 * @param[in] servicename The service name to get the information from.
 *
 * @param[out] service  A pointer to store the service information about the
 *                      share.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 *
 * @see smbconf_service
 */
sbcErr smbconf_get_share(struct smbconf_ctx *ctx,
			 TALLOC_CTX *mem_ctx,
			 const char *servicename,
			 struct smbconf_service **service);

/**
 * @brief Delete a service from configuration.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] servicename The service name to delete.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_delete_share(struct smbconf_ctx *ctx,
			    const char *servicename);

/**
 * @brief Set a configuration parameter to the value provided.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] service   The service name to set the parameter.
 *
 * @param[in] param     The name of the parameter to set.
 *
 * @param[in] valstr    The value to set.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_set_parameter(struct smbconf_ctx *ctx,
			     const char *service,
			     const char *param,
			     const char *valstr);

/**
 * @brief Set a global configuration parameter to the value provided.
 *
 * This adds a paramet in the [global] service. It also creates [global] if it
 * does't exist.
 *
 * @param[in] ctx       The smbconf context to use.
 *
 * @param[in] param     The name of the parameter to set.
 *
 * @param[in] val       The value to set.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_set_global_parameter(struct smbconf_ctx *ctx,
				    const char *param, const char *val);

/**
 * @brief Get the value of a configuration parameter as a string.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  mem_ctx  The memory context to allocate the string on.
 *
 * @param[in]  service  The name of the service where to find the parameter.
 *
 * @param[in]  param    The parameter to get.
 *
 * @param[out] valstr   A pointer to store the value as a string.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_get_parameter(struct smbconf_ctx *ctx,
			     TALLOC_CTX *mem_ctx,
			     const char *service,
			     const char *param,
			     char **valstr);

/**
 * @brief Get the value of a global configuration parameter as a string.
 *
 * It also creates [global] if it does't exist.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  mem_ctx  The memory context to allocate the string on.
 *
 * @param[in]  param    The parameter to get.
 *
 * @param[out] valstr   A pointer to store the value as a string.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_get_global_parameter(struct smbconf_ctx *ctx,
				    TALLOC_CTX *mem_ctx,
				    const char *param,
				    char **valstr);

/**
 * @brief Delete a parameter from the configuration.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in] service   The service where the parameter can be found.
 *
 * @param[in] param     The name of the parameter to delete.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_delete_parameter(struct smbconf_ctx *ctx,
				const char *service, const char *param);

/**
 * @brief Delete a global parameter from the configuration.
 *
 * It also creates [global] if it does't exist.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in] param     The name of the parameter to delete.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_delete_global_parameter(struct smbconf_ctx *ctx,
				       const char *param);

/**
 * @brief Get the list of names of included files.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  mem_ctx  The memory context to allocate the names.
 *
 * @param[in]  service  The service name to get the include files.
 *
 * @param[out] num_includes A pointer to store the number of included files.
 *
 * @param[out] includes A pointer to store the paths of the included files.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_get_includes(struct smbconf_ctx *ctx,
			    TALLOC_CTX *mem_ctx,
			    const char *service,
			    uint32_t *num_includes, char ***includes);

/**
 * @brief Get the list of globally included files.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  mem_ctx  The memory context to allocate the names.
 *
 * @param[out] num_includes A pointer to store the number of included files.
 *
 * @param[out] includes A pointer to store the paths of the included files.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_get_global_includes(struct smbconf_ctx *ctx,
				   TALLOC_CTX *mem_ctx,
				   uint32_t *num_includes, char ***includes);

/**
 * @brief Set a list of config files to include on the given service.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  service  The service to add includes.
 *
 * @param[in]  num_includes The number of includes to set.
 *
 * @param[in]  includes A list of paths to include.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_set_includes(struct smbconf_ctx *ctx,
			    const char *service,
			    uint32_t num_includes, const char **includes);

/**
 * @brief Set a list of config files to include globally.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  num_includes The number of includes to set.
 *
 * @param[in]  includes A list of paths to include.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_set_global_includes(struct smbconf_ctx *ctx,
				   uint32_t num_includes,
				   const char **includes);

/**
 * @brief Delete include parameter on the given service.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @param[in]  service  The name of the service to delete the includes from.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_delete_includes(struct smbconf_ctx *ctx, const char *service);

/**
 * @brief Delete include parameter from the global service.
 *
 * @param[in]  ctx      The smbconf context to use.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_delete_global_includes(struct smbconf_ctx *ctx);

/**
 * @brief Start a transaction on the configuration backend.
 *
 * This is to speed up writes to the registry based backend.
 *
 * @param[in] ctx       The smbconf context to start the transaction.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 */
sbcErr smbconf_transaction_start(struct smbconf_ctx *ctx);

/**
 * @brief Commit a transaction on the configuration backend.
 *
 * This is to speed up writes to the registry based backend.
 *
 * @param[in] ctx       The smbconf context to commit the transaction.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 *
 * @see smbconf_transaction_start()
 */
sbcErr smbconf_transaction_commit(struct smbconf_ctx *ctx);

/**
 * @brief Cancel a transaction on the configuration backend.
 *
 * @param[in] ctx       The smbconf context to cancel the transaction.
 *
 * @return              SBC_ERR_OK on success, a corresponding sbcErr if an
 *                      error occured.
 *
 * @see smbconf_transaction_start()
 */
sbcErr smbconf_transaction_cancel(struct smbconf_ctx *ctx);

/* @} ******************************************************************/

#endif /*  _LIBSMBCONF_H_  */
