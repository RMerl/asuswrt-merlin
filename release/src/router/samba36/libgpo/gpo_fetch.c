/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2005-2006
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
#include "system/filesys.h"
#include "../libgpo/gpo.h"
#include "../libgpo/gpo_ini.h"

#if _SAMBA_BUILD_ == 4
#include "param/param.h"
#include "libcli/resolve/resolve.h"
#include <tevent.h>
#include "libcli/libcli.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli_proto.h"
#include "libgpo/ads_convenience.h"
#include "libgpo/gpo_s4.h"
#include "lib/util/util.h"
#else
#include "libgpo/gpo_proto.h"
#include "libsmb/libsmb.h"
#endif

/****************************************************************
 explode the GPO CIFS URI into their components
****************************************************************/

NTSTATUS gpo_explode_filesyspath(TALLOC_CTX *mem_ctx,
                                 const char *cache_dir,
				 const char *file_sys_path,
				 char **server,
				 char **service,
				 char **nt_path,
				 char **unix_path)
{
	char *path = NULL;

	*server = NULL;
	*service = NULL;
	*nt_path = NULL;
	*unix_path = NULL;

	if (!file_sys_path) {
		return NT_STATUS_OK;
	}

	if (!next_token_talloc(mem_ctx, &file_sys_path, server, "\\")) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	NT_STATUS_HAVE_NO_MEMORY(*server);

	if (!next_token_talloc(mem_ctx, &file_sys_path, service, "\\")) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	NT_STATUS_HAVE_NO_MEMORY(*service);

	if ((*nt_path = talloc_asprintf(mem_ctx, "\\%s", file_sys_path))
		== NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	NT_STATUS_HAVE_NO_MEMORY(*nt_path);

	if ((path = talloc_asprintf(mem_ctx,
					"%s/%s",
					cache_dir,
					file_sys_path)) == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
#if _SAMBA_BUILD_ == 4
	path = string_sub_talloc(mem_ctx, path, "\\", "/");
#else
	path = talloc_string_sub(mem_ctx, path, "\\", "/");
#endif
	if (!path) {
		return NT_STATUS_NO_MEMORY;
	}

	*unix_path = talloc_strdup(mem_ctx, path);
	NT_STATUS_HAVE_NO_MEMORY(*unix_path);

	talloc_free(path);
	return NT_STATUS_OK;
}

/****************************************************************
 prepare the local disc storage for "unix_path"
****************************************************************/

static NTSTATUS gpo_prepare_local_store(TALLOC_CTX *mem_ctx,
                                        const char *cache_dir,
					const char *unix_path)
{
	char *current_dir;
	char *tok;

	current_dir = talloc_strdup(mem_ctx, cache_dir);
	NT_STATUS_HAVE_NO_MEMORY(current_dir);

	if ((mkdir(cache_dir, 0644)) < 0 && errno != EEXIST) {
		return NT_STATUS_ACCESS_DENIED;
	}

	while (next_token_talloc(mem_ctx, &unix_path, &tok, "/")) {
		if (strequal(tok, GPO_CACHE_DIR)) {
			break;
		}
	}

	while (next_token_talloc(mem_ctx, &unix_path, &tok, "/")) {
		current_dir = talloc_asprintf_append_buffer(current_dir, "/%s", tok);
		NT_STATUS_HAVE_NO_MEMORY(current_dir);

		if ((mkdir(current_dir, 0644)) < 0 && errno != EEXIST) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	return NT_STATUS_OK;
}

static NTSTATUS gpo_connect_server(ADS_STRUCT *ads, struct loadparm_context *lp_ctx,
                                   const char *server, const char *service, void *ret_cli)
{
	NTSTATUS result;
#if _SAMBA_BUILD_ == 3
	struct cli_state *cli;


	result = cli_full_connection(&cli,
			global_myname(),
			server,
			NULL, 0,
			service, "A:",
			ads->auth.user_name, NULL,
			ads->auth.password,
			CLI_FULL_CONNECTION_USE_KERBEROS |
			CLI_FULL_CONNECTION_FALLBACK_AFTER_KERBEROS,
			Undefined);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("check_refresh_gpo: "
				"failed to connect: %s\n",
				nt_errstr(result)));
		return result;
	}
	*(struct cli_state **) ret_cli = cli;
#else
	struct smbcli_state *cli = NULL;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lp_smbcli_options(lp_ctx, &options);
	lp_smbcli_session_options(lp_ctx, &session_options);

	result = smbcli_full_connection(NULL, &cli,
			server,
			NULL, service,
			NULL /*devtype*/, NULL /* socket options */,
			ads->credentials,
			lp_resolve_context(lp_ctx),
			tevent_context_init(ads),
			&options,
			&session_options,
			lp_iconv_convenience(lp_ctx),
			lp_gensec_settings(ads, lp_ctx));
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("failed to connect: %s\n",
				nt_errstr(result)));
		return result;
	}
	*(struct smbcli_state **) ret_cli = cli;
#endif
	return NT_STATUS_OK;
}

/****************************************************************
 download a full GPO via CIFS
****************************************************************/

NTSTATUS gpo_fetch_files(TALLOC_CTX *mem_ctx,
                         ADS_STRUCT *ads,
                         struct loadparm_context *lp_ctx,
                         const char *cache_dir,
			 struct GROUP_POLICY_OBJECT *gpo)
{
	NTSTATUS result;
	char *server, *service, *nt_path, *unix_path;
	char *nt_ini_path, *unix_ini_path;
#if _SAMBA_BUILD_ == 3
	struct cli_state *cli;
#else
	struct smbcli_state *cli;
#endif


	result = gpo_explode_filesyspath(mem_ctx, cache_dir, gpo->file_sys_path,
					 &server, &service, &nt_path,
					 &unix_path);
	NT_STATUS_NOT_OK_RETURN(result);


	result = gpo_connect_server(ads, lp_ctx, server, service, &cli);
	NT_STATUS_NOT_OK_RETURN(result);


	result = gpo_prepare_local_store(mem_ctx, cache_dir, unix_path);
	NT_STATUS_NOT_OK_RETURN(result);

	unix_ini_path = talloc_asprintf(mem_ctx, "%s/%s", unix_path, GPT_INI);
	nt_ini_path = talloc_asprintf(mem_ctx, "%s\\%s", nt_path, GPT_INI);
	NT_STATUS_HAVE_NO_MEMORY(unix_ini_path);
	NT_STATUS_HAVE_NO_MEMORY(nt_ini_path);

	result = gpo_copy_file(mem_ctx, cli, nt_ini_path, unix_ini_path);
	NT_STATUS_NOT_OK_RETURN(result);

	result = gpo_sync_directories(mem_ctx, cli, nt_path, unix_path);
	NT_STATUS_NOT_OK_RETURN(result);

	return NT_STATUS_OK;
}

/****************************************************************
 get the locally stored gpt.ini version number
****************************************************************/

NTSTATUS gpo_get_sysvol_gpt_version(TALLOC_CTX *mem_ctx,
				    const char *unix_path,
				    uint32_t *sysvol_version,
				    char **display_name)
{
	NTSTATUS status;
	uint32_t version = 0;
	char *local_path = NULL;
	char *name = NULL;

	if (!unix_path) {
		return NT_STATUS_OK;
	}

	local_path = talloc_asprintf(mem_ctx, "%s/%s", unix_path, GPT_INI);
	NT_STATUS_HAVE_NO_MEMORY(local_path);

	status = parse_gpt_ini(mem_ctx, local_path, &version, &name);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("gpo_get_sysvol_gpt_version: "
			"failed to parse ini [%s]: %s\n",
			local_path, nt_errstr(status)));
		return status;
	}

	if (sysvol_version) {
		*sysvol_version = version;
	}

	if (name && *display_name) {
		*display_name = talloc_strdup(mem_ctx, name);
		NT_STATUS_HAVE_NO_MEMORY(*display_name);
	}

	return NT_STATUS_OK;
}
