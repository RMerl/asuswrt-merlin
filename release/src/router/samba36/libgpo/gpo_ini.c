/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Support
 *  Copyright (C) Guenther Deschner 2007
 *  Copyright (C) Wilco Baan Hofman 2009
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
#include "gpo.h"
#include "gpo_ini.h"
#include "system/filesys.h"


static bool change_section(const char *section, void *ctx_ptr)
{
	struct gp_inifile_context *ctx = (struct gp_inifile_context *) ctx_ptr;

	if (ctx->current_section) {
		talloc_free(ctx->current_section);
	}
	ctx->current_section = talloc_strdup(ctx, section);
	return true;
}

/****************************************************************
****************************************************************/

static bool store_keyval_pair(const char *key, const char *value, void *ctx_ptr)
{
	struct gp_inifile_context *ctx = (struct gp_inifile_context *) ctx_ptr;
	ctx->data = talloc_realloc(ctx, ctx->data, struct keyval_pair *, ctx->keyval_count+1);
	ctx->data[ctx->keyval_count] = talloc_zero(ctx, struct keyval_pair);
	ctx->data[ctx->keyval_count]->key = talloc_asprintf(ctx, "%s:%s", ctx->current_section, key);
	ctx->data[ctx->keyval_count]->val = talloc_strdup(ctx, value);
	ctx->keyval_count++;
	return true;
}

/****************************************************************
****************************************************************/

static NTSTATUS convert_file_from_ucs2(TALLOC_CTX *mem_ctx,
				       const char *filename_in,
				       char **filename_out)
{
	int tmp_fd = -1;
	uint8_t *data_in = NULL;
	uint8_t *data_out = NULL;
	char *tmp_name = NULL;
	NTSTATUS status;
	size_t n = 0;
	size_t converted_size;

	if (!filename_out) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	data_in = (uint8_t *)file_load(filename_in, &n, 0, NULL);
	if (!data_in) {
		status = NT_STATUS_NO_SUCH_FILE;
		goto out;
	}

	tmp_name = talloc_asprintf(mem_ctx, "%s/convert_file_from_ucs2.XXXXXX",
		tmpdir());
	if (!tmp_name) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	tmp_fd = mkstemp(tmp_name);
	if (tmp_fd == -1) {
		status = NT_STATUS_ACCESS_DENIED;
		goto out;
	}

	if (!convert_string_talloc(mem_ctx, CH_UTF16LE, CH_UNIX, data_in, n,
				   (void *)&data_out, &converted_size, false))
	{
		status = NT_STATUS_INVALID_BUFFER_SIZE;
		goto out;
	}

	/* skip utf8 BOM */
	DEBUG(11,("convert_file_from_ucs2: "
	       "data_out[0]: 0x%x, data_out[1]: 0x%x, data_out[2]: 0x%x\n",
		data_out[0], data_out[1], data_out[2]));

	if ((data_out[0] == 0xef) && (data_out[1] == 0xbb) &&
	    (data_out[2] == 0xbf)) {
		DEBUG(11,("convert_file_from_ucs2: "
			 "%s skipping utf8 BOM\n", tmp_name));
		data_out += 3;
		converted_size -= 3;
	}

	if (write(tmp_fd, data_out, converted_size) != converted_size) {
		status = map_nt_error_from_unix(errno);
		goto out;
	}

	*filename_out = tmp_name;

	status = NT_STATUS_OK;

 out:
	if (tmp_fd != -1) {
		close(tmp_fd);
	}

	talloc_free(data_in);

	return status;
}

/****************************************************************
****************************************************************/

NTSTATUS gp_inifile_getstring(struct gp_inifile_context *ctx, const char *key, char **ret)
{
	int i;

	for (i = 0; i < ctx->keyval_count; i++) {
		if (strcmp(ctx->data[i]->key, key) == 0) {
			*ret = ctx->data[i]->val;
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_NOT_FOUND;
}

/****************************************************************
****************************************************************/

NTSTATUS gp_inifile_getint(struct gp_inifile_context *ctx, const char *key, int *ret)
{
	char *value;
	NTSTATUS result;

	result = gp_inifile_getstring(ctx,key, &value);
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	*ret = (int)strtol(value, NULL, 10);
	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS gp_inifile_init_context(TALLOC_CTX *mem_ctx,
				 uint32_t flags,
				 const char *unix_path,
				 const char *suffix,
				 struct gp_inifile_context **ctx_ret)
{
	struct gp_inifile_context *ctx = NULL;
	NTSTATUS status;
	int rv;
	char *tmp_filename = NULL;
	const char *ini_filename = NULL;

	if (!unix_path || !ctx_ret) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ctx = talloc_zero(mem_ctx, struct gp_inifile_context);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = gp_find_file(mem_ctx, flags, unix_path, suffix,
			      &ini_filename);

	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	status = convert_file_from_ucs2(mem_ctx, ini_filename,
					&tmp_filename);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	rv = pm_process(tmp_filename, change_section, store_keyval_pair, ctx);
	if (!rv) {
		return NT_STATUS_NO_SUCH_FILE;
	}


	ctx->generated_filename = tmp_filename;
	ctx->mem_ctx = mem_ctx;

	*ctx_ret = ctx;

	return NT_STATUS_OK;

 failed:

	DEBUG(1,("gp_inifile_init_context failed: %s\n",
		nt_errstr(status)));

	talloc_free(ctx);

	return status;
}

/****************************************************************
 parse the local gpt.ini file
****************************************************************/

#define GPT_INI_SECTION_GENERAL "General"
#define GPT_INI_PARAMETER_VERSION "Version"
#define GPT_INI_PARAMETER_DISPLAYNAME "displayName"

NTSTATUS parse_gpt_ini(TALLOC_CTX *mem_ctx,
		       const char *filename,
		       uint32_t *version,
		       char **display_name)
{
	NTSTATUS result;
	int rv;
	int v = 0;
	char *name = NULL;
	struct gp_inifile_context *ctx;

	if (!filename) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ctx = talloc_zero(mem_ctx, struct gp_inifile_context);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	rv = pm_process(filename, change_section, store_keyval_pair, ctx);
	if (!rv) {
		return NT_STATUS_NO_SUCH_FILE;
	}


	result = gp_inifile_getstring(ctx, GPT_INI_SECTION_GENERAL
			":"GPT_INI_PARAMETER_DISPLAYNAME, &name);
	if (!NT_STATUS_IS_OK(result)) {
		/* the default domain policy and the default domain controller
		 * policy never have a displayname in their gpt.ini file */
		DEBUG(10,("parse_gpt_ini: no name in %s\n", filename));
	}

	if (name && display_name) {
		*display_name = talloc_strdup(ctx, name);
		if (*display_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	result = gp_inifile_getint(ctx, GPT_INI_SECTION_GENERAL
			":"GPT_INI_PARAMETER_VERSION, &v);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(10,("parse_gpt_ini: no version\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (version) {
		*version = v;
	}

	talloc_free(ctx);

	return NT_STATUS_OK;
}
