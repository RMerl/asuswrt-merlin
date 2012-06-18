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

#ifndef __LIBSMBCONF_PRIVATE_H__
#define __LIBSMBCONF_PRIVATE_H__

#ifndef GLOBAL_NAME
#define GLOBAL_NAME "global"
#endif

#include "lib/smbconf/smbconf.h"

struct smbconf_ops {
	WERROR (*init)(struct smbconf_ctx *ctx, const char *path);
	int (*shutdown)(struct smbconf_ctx *ctx);
	bool (*requires_messaging)(struct smbconf_ctx *ctx);
	bool (*is_writeable)(struct smbconf_ctx *ctx);
	WERROR (*open_conf)(struct smbconf_ctx *ctx);
	int (*close_conf)(struct smbconf_ctx *ctx);
	void (*get_csn)(struct smbconf_ctx *ctx, struct smbconf_csn *csn,
			const char *service, const char *param);
	WERROR (*drop)(struct smbconf_ctx *ctx);
	WERROR (*get_share_names)(struct smbconf_ctx *ctx,
				  TALLOC_CTX *mem_ctx,
				  uint32_t *num_shares,
				  char ***share_names);
	bool (*share_exists)(struct smbconf_ctx *ctx, const char *service);
	WERROR (*create_share)(struct smbconf_ctx *ctx, const char *service);
	WERROR (*get_share)(struct smbconf_ctx *ctx,
			    TALLOC_CTX *mem_ctx,
			    const char *servicename,
			    struct smbconf_service **service);
	WERROR (*delete_share)(struct smbconf_ctx *ctx,
				    const char *servicename);
	WERROR (*set_parameter)(struct smbconf_ctx *ctx,
				const char *service,
				const char *param,
				const char *valstr);
	WERROR (*get_parameter)(struct smbconf_ctx *ctx,
				TALLOC_CTX *mem_ctx,
				const char *service,
				const char *param,
				char **valstr);
	WERROR (*delete_parameter)(struct smbconf_ctx *ctx,
				   const char *service, const char *param);
	WERROR (*get_includes)(struct smbconf_ctx *ctx,
			       TALLOC_CTX *mem_ctx,
			       const char *service,
			       uint32_t *num_includes, char ***includes);
	WERROR (*set_includes)(struct smbconf_ctx *ctx,
			       const char *service,
			       uint32_t num_includes, const char **includes);
	WERROR (*delete_includes)(struct smbconf_ctx *ctx,
				  const char *service);
	WERROR (*transaction_start)(struct smbconf_ctx *ctx);
	WERROR (*transaction_commit)(struct smbconf_ctx *ctx);
	WERROR (*transaction_cancel)(struct smbconf_ctx *ctx);
};

struct smbconf_ctx {
	const char *path;
	struct smbconf_ops *ops;
	void *data; /* private data for use in backends */
};

WERROR smbconf_init_internal(TALLOC_CTX *mem_ctx, struct smbconf_ctx **conf_ctx,
			     const char *path, struct smbconf_ops *ops);

WERROR smbconf_add_string_to_array(TALLOC_CTX *mem_ctx,
				   char ***array,
				   uint32_t count,
				   const char *string);

bool smbconf_find_in_array(const char *string, char **list,
			   uint32_t num_entries, uint32_t *entry);

bool smbconf_reverse_find_in_array(const char *string, char **list,
				   uint32_t num_entries, uint32_t *entry);

#endif
