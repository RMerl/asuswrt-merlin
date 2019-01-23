/*
   Unix SMB/CIFS implementation.
   Filename utility functions.
   Copyright (C) Tim Prouty 2009

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

/**
 * XXX: This is temporary and there should be no callers of this outside of
 * this file once smb_filename is plumbed through all path based operations.
 * The one legitimate caller currently is smb_fname_str_dbg(), which this
 * could be made static for.
 */
NTSTATUS get_full_smb_filename(TALLOC_CTX *ctx,
			       const struct smb_filename *smb_fname,
			       char **full_name)
{
	if (smb_fname->stream_name) {
		/* stream_name must always be NULL if there is no stream. */
		SMB_ASSERT(smb_fname->stream_name[0] != '\0');

		*full_name = talloc_asprintf(ctx, "%s%s", smb_fname->base_name,
					     smb_fname->stream_name);
	} else {
		*full_name = talloc_strdup(ctx, smb_fname->base_name);
	}

	if (!*full_name) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

/**
 * There are actually legitimate callers of this such as functions that
 * enumerate streams using the vfs_streaminfo interface and then want to
 * operate on each stream.
 */
NTSTATUS create_synthetic_smb_fname(TALLOC_CTX *ctx, const char *base_name,
				    const char *stream_name,
				    const SMB_STRUCT_STAT *psbuf,
				    struct smb_filename **smb_fname_out)
{
	struct smb_filename smb_fname_loc;

	ZERO_STRUCT(smb_fname_loc);

	/* Setup the base_name/stream_name. */
	smb_fname_loc.base_name = CONST_DISCARD(char *, base_name);
	smb_fname_loc.stream_name = CONST_DISCARD(char *, stream_name);

	/* Copy the psbuf if one was given. */
	if (psbuf)
		smb_fname_loc.st = *psbuf;

	/* Let copy_smb_filename() do the heavy lifting. */
	return copy_smb_filename(ctx, &smb_fname_loc, smb_fname_out);
}

/**
 * XXX: This is temporary and there should be no callers of this once
 * smb_filename is plumbed through all path based operations.
 */
NTSTATUS create_synthetic_smb_fname_split(TALLOC_CTX *ctx,
					  const char *fname,
					  const SMB_STRUCT_STAT *psbuf,
					  struct smb_filename **smb_fname_out)
{
	NTSTATUS status;
	const char *stream_name = NULL;
	char *base_name = NULL;

	if (!lp_posix_pathnames()) {
		stream_name = strchr_m(fname, ':');
	}

	/* Setup the base_name/stream_name. */
	if (stream_name) {
		base_name = talloc_strndup(ctx, fname,
					   PTR_DIFF(stream_name, fname));
	} else {
		base_name = talloc_strdup(ctx, fname);
	}

	if (!base_name) {
		return NT_STATUS_NO_MEMORY;
	}

	status = create_synthetic_smb_fname(ctx, base_name, stream_name, psbuf,
					    smb_fname_out);
	TALLOC_FREE(base_name);
	return status;
}

/**
 * Return a string using the talloc_tos()
 */
const char *smb_fname_str_dbg(const struct smb_filename *smb_fname)
{
	char *fname = NULL;
	NTSTATUS status;

	if (smb_fname == NULL) {
		return "";
	}
	status = get_full_smb_filename(talloc_tos(), smb_fname, &fname);
	if (!NT_STATUS_IS_OK(status)) {
		return "";
	}
	return fname;
}

/**
 * Return a debug string using the talloc_tos().  This can only be called from
 * DEBUG() macros due to the debut_ctx().
 */
const char *fsp_str_dbg(const struct files_struct *fsp)
{
	return smb_fname_str_dbg(fsp->fsp_name);
}

NTSTATUS copy_smb_filename(TALLOC_CTX *ctx,
			   const struct smb_filename *smb_fname_in,
			   struct smb_filename **smb_fname_out)
{
	/* stream_name must always be NULL if there is no stream. */
	if (smb_fname_in->stream_name) {
		SMB_ASSERT(smb_fname_in->stream_name[0] != '\0');
	}

	*smb_fname_out = talloc_zero(ctx, struct smb_filename);
	if (*smb_fname_out == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (smb_fname_in->base_name) {
		(*smb_fname_out)->base_name =
		    talloc_strdup(*smb_fname_out, smb_fname_in->base_name);
		if (!(*smb_fname_out)->base_name)
			goto no_mem_err;
	}

	if (smb_fname_in->stream_name) {
		(*smb_fname_out)->stream_name =
		    talloc_strdup(*smb_fname_out, smb_fname_in->stream_name);
		if (!(*smb_fname_out)->stream_name)
			goto no_mem_err;
	}

	if (smb_fname_in->original_lcomp) {
		(*smb_fname_out)->original_lcomp =
		    talloc_strdup(*smb_fname_out, smb_fname_in->original_lcomp);
		if (!(*smb_fname_out)->original_lcomp)
			goto no_mem_err;
	}

	(*smb_fname_out)->st = smb_fname_in->st;
	return NT_STATUS_OK;

 no_mem_err:
	TALLOC_FREE(*smb_fname_out);
	return NT_STATUS_NO_MEMORY;
}

/****************************************************************************
 Simple check to determine if the filename is a stream.
 ***************************************************************************/
bool is_ntfs_stream_smb_fname(const struct smb_filename *smb_fname)
{
	/* stream_name must always be NULL if there is no stream. */
	if (smb_fname->stream_name) {
		SMB_ASSERT(smb_fname->stream_name[0] != '\0');
	}

	if (lp_posix_pathnames()) {
		return false;
	}

	return smb_fname->stream_name != NULL;
}

/****************************************************************************
 Returns true if the filename's stream == "::$DATA"
 ***************************************************************************/
bool is_ntfs_default_stream_smb_fname(const struct smb_filename *smb_fname)
{
	if (!is_ntfs_stream_smb_fname(smb_fname)) {
		return false;
	}

	return StrCaseCmp(smb_fname->stream_name, "::$DATA") == 0;
}
