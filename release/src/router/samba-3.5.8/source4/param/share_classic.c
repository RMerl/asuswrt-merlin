/* 
   Unix SMB/CIFS implementation.
   
   Classic file based shares configuration
   
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
#include "param/share.h"
#include "param/param.h"

static NTSTATUS sclassic_init(TALLOC_CTX *mem_ctx, 
			      const struct share_ops *ops, 
			      struct tevent_context *event_ctx,
			      struct loadparm_context *lp_ctx,
			      struct share_context **ctx)
{
	*ctx = talloc(mem_ctx, struct share_context);
	if (!*ctx) {
		DEBUG(0, ("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	(*ctx)->ops = ops;
	(*ctx)->priv_data = lp_ctx;

	return NT_STATUS_OK;
}

static const char *sclassic_string_option(struct share_config *scfg, 
					  const char *opt_name, 
					  const char *defval)
{
	struct loadparm_service *s = talloc_get_type(scfg->opaque, 
						     struct loadparm_service);
	struct loadparm_context *lp_ctx = talloc_get_type(scfg->ctx->priv_data, 
							  struct loadparm_context);
	char *parm, *val;
	const char *ret;

	if (strchr(opt_name, ':')) {
		parm = talloc_strdup(scfg, opt_name);
		if (!parm) {
			return NULL;
		}
		val = strchr(parm, ':');
		*val = '\0';
		val++;

		ret = lp_parm_string(lp_ctx, s, parm, val);
		if (!ret) {
			ret = defval;
		}
		talloc_free(parm);
		return ret;
	}

	if (strcmp(opt_name, SHARE_NAME) == 0) {
		return scfg->name;
	}

	if (strcmp(opt_name, SHARE_PATH) == 0) {
		return lp_pathname(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_COMMENT) == 0) {
		return lp_comment(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_VOLUME) == 0) {
		return volume_label(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_TYPE) == 0) {
		if (lp_print_ok(s, lp_default_service(lp_ctx))) {
			return "PRINTER";
		}
		if (strcmp("NTFS", lp_fstype(s, lp_default_service(lp_ctx))) == 0) {
			return "DISK";
		}
		return lp_fstype(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_PASSWORD) == 0) {
		return defval;
	}

	DEBUG(0,("request for unknown share string option '%s'\n",
		 opt_name));

	return defval;
}

static int sclassic_int_option(struct share_config *scfg, const char *opt_name, int defval)
{
	struct loadparm_service *s = talloc_get_type(scfg->opaque, 
						     struct loadparm_service);
	struct loadparm_context *lp_ctx = talloc_get_type(scfg->ctx->priv_data, 
							  struct loadparm_context);
	char *parm, *val;
	int ret;

	if (strchr(opt_name, ':')) {
		parm = talloc_strdup(scfg, opt_name);
		if (!parm) {
			return -1;
		}
		val = strchr(parm, ':');
		*val = '\0';
		val++;

		ret = lp_parm_int(lp_ctx, s, parm, val, defval);
		if (!ret) {
			ret = defval;
		}
		talloc_free(parm);
		return ret;
	}

	if (strcmp(opt_name, SHARE_CSC_POLICY) == 0) {
		return lp_csc_policy(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_MAX_CONNECTIONS) == 0) {
		return lp_max_connections(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_CREATE_MASK) == 0) {
		return lp_create_mask(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_DIR_MASK) == 0) {
		return lp_dir_mask(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_FORCE_DIR_MODE) == 0) {
		return lp_force_dir_mode(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_FORCE_CREATE_MODE) == 0) {
		return lp_force_create_mode(s, lp_default_service(lp_ctx));
	}


	DEBUG(0,("request for unknown share int option '%s'\n",
		 opt_name));

	return defval;
}

static bool sclassic_bool_option(struct share_config *scfg, const char *opt_name, 
			  bool defval)
{
	struct loadparm_service *s = talloc_get_type(scfg->opaque, 
						     struct loadparm_service);
	struct loadparm_context *lp_ctx = talloc_get_type(scfg->ctx->priv_data, 
							  struct loadparm_context);
	char *parm, *val;
	bool ret;

	if (strchr(opt_name, ':')) {
		parm = talloc_strdup(scfg, opt_name);
		if(!parm) {
			return false;
		}
		val = strchr(parm, ':');
		*val = '\0';
		val++;

		ret = lp_parm_bool(lp_ctx, s, parm, val, defval);
		talloc_free(parm);
		return ret;
	}

	if (strcmp(opt_name, SHARE_AVAILABLE) == 0) {
		return s != NULL;
	}

	if (strcmp(opt_name, SHARE_BROWSEABLE) == 0) {
		return lp_browseable(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_READONLY) == 0) {
		return lp_readonly(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_MAP_SYSTEM) == 0) {
		return lp_map_system(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_MAP_HIDDEN) == 0) {
		return lp_map_hidden(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_MAP_ARCHIVE) == 0) {
		return lp_map_archive(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_STRICT_LOCKING) == 0) {
		return lp_strict_locking(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_OPLOCKS) == 0) {
		return lp_oplocks(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_STRICT_SYNC) == 0) {
		return lp_strict_sync(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_MSDFS_ROOT) == 0) {
		return lp_msdfs_root(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_CI_FILESYSTEM) == 0) {
		return lp_ci_filesystem(s, lp_default_service(lp_ctx));
	}

	DEBUG(0,("request for unknown share bool option '%s'\n",
		 opt_name));

	return defval;
}

static const char **sclassic_string_list_option(TALLOC_CTX *mem_ctx, struct share_config *scfg, const char *opt_name)
{
	struct loadparm_service *s = talloc_get_type(scfg->opaque, 
						     struct loadparm_service);
	struct loadparm_context *lp_ctx = talloc_get_type(scfg->ctx->priv_data, 
							  struct loadparm_context);
	char *parm, *val;
	const char **ret;

	if (strchr(opt_name, ':')) {
		parm = talloc_strdup(scfg, opt_name);
		if (!parm) {
			return NULL;
		}
		val = strchr(parm, ':');
		*val = '\0';
		val++;

		ret = lp_parm_string_list(mem_ctx, lp_ctx, s, parm, val, ",;");
		talloc_free(parm);
		return ret;
	}

	if (strcmp(opt_name, SHARE_HOSTS_ALLOW) == 0) {
		return lp_hostsallow(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_HOSTS_DENY) == 0) {
		return lp_hostsdeny(s, lp_default_service(lp_ctx));
	}

	if (strcmp(opt_name, SHARE_NTVFS_HANDLER) == 0) {
		return lp_ntvfs_handler(s, lp_default_service(lp_ctx));
	}

	DEBUG(0,("request for unknown share list option '%s'\n",
		 opt_name));

	return NULL;
}

static NTSTATUS sclassic_list_all(TALLOC_CTX *mem_ctx,
				  struct share_context *ctx,
				  int *count,
				  const char ***names)
{
	int i;
	int num_services;
	const char **n;
       
	num_services = lp_numservices((struct loadparm_context *)ctx->priv_data);

	n = talloc_array(mem_ctx, const char *, num_services);
	if (!n) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	for (i = 0; i < num_services; i++) {
		n[i] = talloc_strdup(n, lp_servicename(lp_servicebynum((struct loadparm_context *)ctx->priv_data, i)));
		if (!n[i]) {
			DEBUG(0,("ERROR: Out of memory!\n"));
			talloc_free(n);
			return NT_STATUS_NO_MEMORY;
		}
	}

	*names = n;
	*count = num_services;

	return NT_STATUS_OK;
}

static NTSTATUS sclassic_get_config(TALLOC_CTX *mem_ctx,
				    struct share_context *ctx,
				    const char *name,
				    struct share_config **scfg)
{
	struct share_config *s;
	struct loadparm_service *service;

	service = lp_service((struct loadparm_context *)ctx->priv_data, name);

	if (service == NULL) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	s = talloc(mem_ctx, struct share_config);
	if (!s) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	s->name = talloc_strdup(s, lp_servicename(service));
	if (!s->name) {
		DEBUG(0,("ERROR: Out of memory!\n"));
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}

	s->opaque = (void *)service;
	s->ctx = ctx;
	
	*scfg = s;

	return NT_STATUS_OK;
}

static const struct share_ops ops = {
	.name = "classic",
	.init = sclassic_init,
	.string_option = sclassic_string_option,
	.int_option = sclassic_int_option,
	.bool_option = sclassic_bool_option,
	.string_list_option = sclassic_string_list_option,
	.list_all = sclassic_list_all,
	.get_config = sclassic_get_config
};

NTSTATUS share_classic_init(void)
{
	return share_register(&ops);
}

