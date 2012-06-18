/* 
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2006
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

struct sync_context {
	TALLOC_CTX *mem_ctx;
	struct cli_state *cli;
	char *remote_path;
	char *local_path;
	pstring mask;
	uint16 attribute;
};

static void gpo_sync_func(const char *mnt,
			   file_info *info,
			   const char *mask,
			   void *state);

NTSTATUS gpo_copy_file(TALLOC_CTX *mem_ctx,
		       struct cli_state *cli,
		       const char *nt_path,
		       const char *unix_path)
{
	NTSTATUS result;
	int fnum;
	int fd = 0;
	char *data = NULL;
	static int io_bufsize = 64512;
	int read_size = io_bufsize;
	off_t start = 0;
	off_t nread = 0;

	if ((fnum = cli_open(cli, nt_path, O_RDONLY, DENY_NONE)) == -1) {
		result = NT_STATUS_NO_SUCH_FILE;
		goto out;
	}

	if ((fd = sys_open(unix_path, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		result = map_nt_error_from_unix(errno);
		goto out;
	}
	 
	if ((data = (char *)SMB_MALLOC(read_size)) == NULL) {
		result = NT_STATUS_NO_MEMORY;
		goto out;
	}

	while (1) {

		int n = cli_read(cli, fnum, data, nread + start, read_size);

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
		cli_close(cli, fnum);
	}
	if (fd) {
		close(fd);
	}

	return result;
}

/****************************************************************
 copy dir
****************************************************************/

static NTSTATUS gpo_copy_dir(const char *unix_path)
{
	if ((mkdir(unix_path, 0644)) < 0 && errno != EEXIST) {
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_OK;
}

/****************************************************************
 sync files
****************************************************************/

static BOOL gpo_sync_files(struct sync_context *ctx)
{
	DEBUG(3,("calling cli_list with mask: %s\n", ctx->mask));

	if (cli_list(ctx->cli, ctx->mask, ctx->attribute, gpo_sync_func, ctx) == -1) {
		DEBUG(1,("listing [%s] failed with error: %s\n", 
			ctx->mask, cli_errstr(ctx->cli)));
		return False;
	}

	return True;
}

/****************************************************************
 syncronisation call back
****************************************************************/

static void gpo_sync_func(const char *mnt,
			  file_info *info,
			  const char *mask,
			  void *state)
{
	NTSTATUS result;
	struct sync_context *ctx;
	fstring nt_filename, unix_filename;
	fstring nt_dir, unix_dir;
	char *old_nt_dir, *old_unix_dir;

	ctx = (struct sync_context *)state;

	if (strequal(info->name, ".") || strequal(info->name, "..")) {
		return;
	}

	DEBUG(5,("gpo_sync_func: got mask: [%s], name: [%s]\n", 
		mask, info->name));

	if (info->mode & aDIR) {

		DEBUG(3,("got dir: [%s]\n", info->name));

		fstrcpy(nt_dir, ctx->remote_path);
		fstrcat(nt_dir, "\\");
		fstrcat(nt_dir, info->name);

		fstrcpy(unix_dir, ctx->local_path);
		fstrcat(unix_dir, "/");
		fstrcat(unix_dir, info->name);

		result = gpo_copy_dir(unix_dir);
		if (!NT_STATUS_IS_OK(result)) {
			DEBUG(1,("failed to copy dir: %s\n", nt_errstr(result)));
		}

		old_nt_dir = ctx->remote_path;
		ctx->remote_path = nt_dir;
		
		old_unix_dir = ctx->local_path;
		ctx->local_path = talloc_strdup(ctx->mem_ctx, unix_dir);

		pstrcpy(ctx->mask, nt_dir);
		pstrcat(ctx->mask, "\\*");

		if (!gpo_sync_files(ctx)) {
			DEBUG(0,("could not sync files\n"));
		}

		ctx->remote_path = old_nt_dir;
		ctx->local_path = old_unix_dir;
		return;
	}

	DEBUG(3,("got file: [%s]\n", info->name));

	fstrcpy(nt_filename, ctx->remote_path);
	fstrcat(nt_filename, "\\");
	fstrcat(nt_filename, info->name);

	fstrcpy(unix_filename, ctx->local_path);
	fstrcat(unix_filename, "/");
	fstrcat(unix_filename, info->name);

	result = gpo_copy_file(ctx->mem_ctx, ctx->cli, nt_filename, unix_filename);
	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(1,("failed to copy file: %s\n", nt_errstr(result)));
	}
}


/****************************************************************
 list a remote directory and download recursivly
****************************************************************/

NTSTATUS gpo_sync_directories(TALLOC_CTX *mem_ctx, 
			      struct cli_state *cli, 
			      const char *nt_path, 
			      const char *local_path)
{
	struct sync_context ctx;

	ctx.mem_ctx 	= mem_ctx;
	ctx.cli 	= cli;
	ctx.remote_path	= CONST_DISCARD(char *, nt_path);
	ctx.local_path	= CONST_DISCARD(char *, local_path);
	ctx.attribute 	= (aSYSTEM | aHIDDEN | aDIR);

	pstrcpy(ctx.mask, nt_path);
	pstrcat(ctx.mask, "\\*");

	if (!gpo_sync_files(&ctx)) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	return NT_STATUS_OK;
}
