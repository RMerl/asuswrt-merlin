/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *
 *  Copyright (C) Guenther Deschner 2006
 *  Copyright (C) Wilco Baan Hofman 2008
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
#include "libcli/libcli.h"
#include "system/filesys.h"


struct sync_context {
	TALLOC_CTX *mem_ctx;
	struct smbcli_state *cli;
	char *remote_path;
	char *local_path;
	char *mask;
	uint16_t attribute;
};

static void gpo_sync_func(struct clilist_file_info *info,
			  const char *mask,
			  void *state);

NTSTATUS gpo_copy_file(TALLOC_CTX *mem_ctx,
		       struct smbcli_state *cli,
		       const char *nt_path,
		       const char *unix_path)
{
	NTSTATUS result;
	int fnum;
	int fd = 0;
	char *data = NULL;
	static int io_bufsize = 64512;
	int read_size = io_bufsize;
	off_t nread = 0;

	if ((fnum = smbcli_open(cli->tree, nt_path, O_RDONLY, DENY_NONE)) == -1) {
		result = NT_STATUS_NO_SUCH_FILE;
		goto out;
	}

	if ((fd = open(unix_path, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		result = map_nt_error_from_unix(errno);
		goto out;
	}

	if ((data = talloc_size(mem_ctx, read_size)) == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto out;
	}

	while (1) {

		int n = smbcli_read(cli->tree, fnum, data, nread, read_size);

		if (n <= 0)
			break;

		if (write(fd, data, n) != n) {
			break;
		}

		nread += n;
	}

	result = NT_STATUS_OK;

 out:
	SAFE_FREE(data);
	if (fnum) {
		smbcli_close(cli->tree, fnum);
	}
	if (fd) {
		close(fd);
	}

	return result;
}

/****************************************************************
 copy dir
****************************************************************/

static NTSTATUS gpo_copy_dir(const char *nt_dir, const char *unix_path)
{
	if ((mkdir(unix_path, 0644)) < 0 && errno != EEXIST) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

/****************************************************************
 sync files
****************************************************************/

static bool gpo_sync_files(struct sync_context *ctx)
{
	DEBUG(3,("calling cli_list with mask: %s\n", ctx->mask));

	if (smbcli_list(ctx->cli->tree,
		     ctx->mask,
		     ctx->attribute,
		     gpo_sync_func,
		     ctx) == -1) {
		DEBUG(1,("listing [%s] failed with error: %s\n",
			ctx->mask, smbcli_errstr(ctx->cli->tree)));
		return false;
	}

	return true;
}

/****************************************************************
 syncronisation call back
****************************************************************/

static void gpo_sync_func(struct clilist_file_info *info,
			  const char *mask,
			  void *state)
{
	NTSTATUS result;
	struct sync_context *ctx;
	char *nt_filename, *unix_filename;
	char *nt_dir, *unix_dir;
	char *old_nt_dir, *old_unix_dir;

	ctx = (struct sync_context *)state;

	if (strequal(info->name, ".") || strequal(info->name, "..")) {
		return;
	}

	DEBUG(5,("gpo_sync_func: got mask: [%s], name: [%s]\n",
		mask, info->name));

	if (info->attrib & FILE_ATTRIBUTE_DIRECTORY) {

		DEBUG(3,("got dir: [%s]\n", info->name));

		nt_dir = talloc_asprintf(ctx->mem_ctx, "%s\\%s",
				ctx->remote_path,
				info->name);

		unix_dir = talloc_asprintf(ctx->mem_ctx, "%s/%s",
				ctx->local_path,
				info->name);

		result = gpo_copy_dir(nt_dir, unix_dir);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(1,("failed to copy dir: %s\n",
				nt_errstr(result)));
		}

		old_nt_dir = ctx->remote_path;
		ctx->remote_path = talloc_strdup(ctx->mem_ctx, nt_dir);

		old_unix_dir = ctx->local_path;
		ctx->local_path = talloc_strdup(ctx->mem_ctx, unix_dir);

		ctx->mask = talloc_asprintf(ctx->mem_ctx,
					"%s\\*",
					nt_dir);
		if (!ctx->local_path || !ctx->mask || !ctx->remote_path) {
			DEBUG(0,("gpo_sync_func: ENOMEM\n"));
			return;
		}
		if (!gpo_sync_files(ctx)) {
			DEBUG(0,("could not sync files\n"));
		}

		ctx->remote_path = old_nt_dir;
		ctx->local_path = old_unix_dir;
		return;
	}

	DEBUG(3,("got file: [%s]\n", info->name));

	nt_filename = talloc_asprintf(ctx->mem_ctx, "%s\\%s",
			ctx->remote_path,
			info->name);

	unix_filename = talloc_asprintf(ctx->mem_ctx, "%s/%s",
			ctx->local_path,
			info->name);

	result = gpo_copy_file(ctx->mem_ctx, ctx->cli,
			       nt_filename, unix_filename);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(1,("failed to copy file: %s\n",
			nt_errstr(result)));
	}
}


/****************************************************************
 list a remote directory and download recursivly
****************************************************************/

NTSTATUS gpo_sync_directories(TALLOC_CTX *mem_ctx,
			      struct smbcli_state *cli,
			      const char *nt_path,
			      const char *local_path)
{
	struct sync_context ctx;

	ctx.mem_ctx 	= mem_ctx;
	ctx.cli 	= cli;
	ctx.remote_path	= discard_const_p(char, nt_path);
	ctx.local_path	= discard_const_p(char, local_path);
	ctx.attribute 	= (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);

	ctx.mask = talloc_asprintf(mem_ctx,
				"%s\\*",
				nt_path);
	if (!ctx.mask) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!gpo_sync_files(&ctx)) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	return NT_STATUS_OK;
}
