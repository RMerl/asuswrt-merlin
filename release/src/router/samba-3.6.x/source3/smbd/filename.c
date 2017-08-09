/*
   Unix SMB/CIFS implementation.
   filename handling routines
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1999-2007
   Copyright (C) Ying Chen 2000
   Copyright (C) Volker Lendecke 2007

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

/*
 * New hash table stat cache code added by Ying Chen.
 */

#include "includes.h"
#include "system/filesys.h"
#include "fake_file.h"
#include "smbd/smbd.h"

static NTSTATUS build_stream_path(TALLOC_CTX *mem_ctx,
				  connection_struct *conn,
				  const char *orig_path,
				  struct smb_filename *smb_fname);

/****************************************************************************
 Mangle the 2nd name and check if it is then equal to the first name.
****************************************************************************/

static bool mangled_equal(const char *name1,
			const char *name2,
			const struct share_params *p)
{
	char mname[13];

	if (!name_to_8_3(name2, mname, False, p)) {
		return False;
	}
	return strequal(name1, mname);
}

/****************************************************************************
 Cope with the differing wildcard and non-wildcard error cases.
****************************************************************************/

static NTSTATUS determine_path_error(const char *name,
			bool allow_wcard_last_component)
{
	const char *p;

	if (!allow_wcard_last_component) {
		/* Error code within a pathname. */
		return NT_STATUS_OBJECT_PATH_NOT_FOUND;
	}

	/* We're terminating here so we
	 * can be a little slower and get
	 * the error code right. Windows
	 * treats the last part of the pathname
	 * separately I think, so if the last
	 * component is a wildcard then we treat
	 * this ./ as "end of component" */

	p = strchr(name, '/');

	if (!p && (ms_has_wild(name) || ISDOT(name))) {
		/* Error code at the end of a pathname. */
		return NT_STATUS_OBJECT_NAME_INVALID;
	} else {
		/* Error code within a pathname. */
		return NT_STATUS_OBJECT_PATH_NOT_FOUND;
	}
}

static NTSTATUS check_for_dot_component(const struct smb_filename *smb_fname)
{
	/* Ensure we catch all names with in "/."
	   this is disallowed under Windows and
	   in POSIX they've already been removed. */
	const char *p = strstr(smb_fname->base_name, "/."); /*mb safe*/
	if (p) {
		if (p[2] == '/') {
			/* Error code within a pathname. */
			return NT_STATUS_OBJECT_PATH_NOT_FOUND;
		} else if (p[2] == '\0') {
			/* Error code at the end of a pathname. */
			return NT_STATUS_OBJECT_NAME_INVALID;
		}
	}
	return NT_STATUS_OK;
}

/****************************************************************************
 Optimization for common case where the missing part
 is in the last component and the client already
 sent the correct case.
 Returns NT_STATUS_OK to mean continue the tree walk
 (possibly with modified start pointer).
 Any other NT_STATUS_XXX error means terminate the path
 lookup here.
****************************************************************************/

static NTSTATUS check_parent_exists(TALLOC_CTX *ctx,
				connection_struct *conn,
				bool posix_pathnames,
				const struct smb_filename *smb_fname,
				char **pp_dirpath,
				char **pp_start)
{
	struct smb_filename parent_fname;
	const char *last_component = NULL;
	NTSTATUS status;
	int ret;

	ZERO_STRUCT(parent_fname);
	if (!parent_dirname(ctx, smb_fname->base_name,
				&parent_fname.base_name,
				&last_component)) {
		return NT_STATUS_NO_MEMORY;
	}

	/*
	 * If there was no parent component in
	 * smb_fname->base_name of the parent name
	 * contained a wildcard then don't do this
	 * optimization.
	 */
	if ((smb_fname->base_name == last_component) ||
			ms_has_wild(parent_fname.base_name)) {
		return NT_STATUS_OK;
	}

	if (posix_pathnames) {
		ret = SMB_VFS_LSTAT(conn, &parent_fname);
	} else {
		ret = SMB_VFS_STAT(conn, &parent_fname);
	}

	/* If the parent stat failed, just continue
	   with the normal tree walk. */

	if (ret == -1) {
		return NT_STATUS_OK;
	}

	status = check_for_dot_component(&parent_fname);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Parent exists - set "start" to be the
	 * last compnent to shorten the tree walk. */

	/*
	 * Safe to use CONST_DISCARD
	 * here as last_component points
	 * into our smb_fname->base_name.
	 */
	*pp_start = CONST_DISCARD(char *,last_component);

	/* Update dirpath. */
	TALLOC_FREE(*pp_dirpath);
	*pp_dirpath = talloc_strdup(ctx, parent_fname.base_name);
	if (!*pp_dirpath) {
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5,("check_parent_exists: name "
		"= %s, dirpath = %s, "
		"start = %s\n",
		smb_fname->base_name,
		*pp_dirpath,
		*pp_start));

	return NT_STATUS_OK;
}

/****************************************************************************
This routine is called to convert names from the dos namespace to unix
namespace. It needs to handle any case conversions, mangling, format changes,
streams etc.

We assume that we have already done a chdir() to the right "root" directory
for this service.

The function will return an NTSTATUS error if some part of the name except for
the last part cannot be resolved, else NT_STATUS_OK.

Note NT_STATUS_OK doesn't mean the name exists or is valid, just that we
didn't get any fatal errors that should immediately terminate the calling SMB
processing whilst resolving.

If the UCF_SAVE_LCOMP flag is passed in, then the unmodified last component
of the pathname is set in smb_filename->original_lcomp.

If UCF_ALWAYS_ALLOW_WCARD_LCOMP is passed in, then a MS wildcard was detected
and should be allowed in the last component of the path only.

If the orig_path was a stream, smb_filename->base_name will point to the base
filename, and smb_filename->stream_name will point to the stream name.  If
orig_path was not a stream, then smb_filename->stream_name will be NULL.

On exit from unix_convert, the smb_filename->st stat struct will be populated
if the file exists and was found, if not this stat struct will be filled with
zeros (and this can be detected by checking for nlinks = 0, which can never be
true for any file).
****************************************************************************/

NTSTATUS unix_convert(TALLOC_CTX *ctx,
		      connection_struct *conn,
		      const char *orig_path,
		      struct smb_filename **smb_fname_out,
		      uint32_t ucf_flags)
{
	struct smb_filename *smb_fname = NULL;
	char *start, *end;
	char *dirpath = NULL;
	char *stream = NULL;
	bool component_was_mangled = False;
	bool name_has_wildcard = False;
	bool posix_pathnames = false;
	bool allow_wcard_last_component =
	    (ucf_flags & UCF_ALWAYS_ALLOW_WCARD_LCOMP);
	bool save_last_component = ucf_flags & UCF_SAVE_LCOMP;
	NTSTATUS status;
	int ret = -1;

	*smb_fname_out = NULL;

	smb_fname = talloc_zero(ctx, struct smb_filename);
	if (smb_fname == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (conn->printer) {
		/* we don't ever use the filenames on a printer share as a
			filename - so don't convert them */
		if (!(smb_fname->base_name = talloc_strdup(smb_fname,
							   orig_path))) {
			status = NT_STATUS_NO_MEMORY;
			goto err;
		}
		goto done;
	}

	DEBUG(5, ("unix_convert called on file \"%s\"\n", orig_path));

	/*
	 * Conversion to basic unix format is already done in
	 * check_path_syntax().
	 */

	/*
	 * Names must be relative to the root of the service - any leading /.
	 * and trailing /'s should have been trimmed by check_path_syntax().
	 */

#ifdef DEVELOPER
	SMB_ASSERT(*orig_path != '/');
#endif

	/*
	 * If we trimmed down to a single '\0' character
	 * then we should use the "." directory to avoid
	 * searching the cache, but not if we are in a
	 * printing share.
	 * As we know this is valid we can return true here.
	 */

	if (!*orig_path) {
		if (!(smb_fname->base_name = talloc_strdup(smb_fname, "."))) {
			status = NT_STATUS_NO_MEMORY;
			goto err;
		}
		if (SMB_VFS_STAT(conn, smb_fname) != 0) {
			status = map_nt_error_from_unix(errno);
			goto err;
		}
		DEBUG(5, ("conversion finished \"\" -> %s\n",
			  smb_fname->base_name));
		goto done;
	}

	if (orig_path[0] == '.' && (orig_path[1] == '/' ||
				orig_path[1] == '\0')) {
		/* Start of pathname can't be "." only. */
		if (orig_path[1] == '\0' || orig_path[2] == '\0') {
			status = NT_STATUS_OBJECT_NAME_INVALID;
		} else {
			status =determine_path_error(&orig_path[2],
			    allow_wcard_last_component);
		}
		goto err;
	}

	/* Start with the full orig_path as given by the caller. */
	if (!(smb_fname->base_name = talloc_strdup(smb_fname, orig_path))) {
		DEBUG(0, ("talloc_strdup failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto err;
	}

	/*
	 * Large directory fix normalization. If we're case sensitive, and
	 * the case preserving parameters are set to "no", normalize the case of
	 * the incoming filename from the client WHETHER IT EXISTS OR NOT !
	 * This is in conflict with the current (3.0.20) man page, but is
	 * what people expect from the "large directory howto". I'll update
	 * the man page. Thanks to jht@samba.org for finding this. JRA.
	 */

	if (conn->case_sensitive && !conn->case_preserve &&
			!conn->short_case_preserve) {
		strnorm(smb_fname->base_name, lp_defaultcase(SNUM(conn)));
	}

	/*
	 * Ensure saved_last_component is valid even if file exists.
	 */

	if(save_last_component) {
		end = strrchr_m(smb_fname->base_name, '/');
		if (end) {
			smb_fname->original_lcomp = talloc_strdup(smb_fname,
								  end + 1);
		} else {
			smb_fname->original_lcomp =
			    talloc_strdup(smb_fname, smb_fname->base_name);
		}
		if (smb_fname->original_lcomp == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto err;
		}
	}

	posix_pathnames = (lp_posix_pathnames() ||
				(ucf_flags & UCF_POSIX_PATHNAMES));

	/*
	 * Strip off the stream, and add it back when we're done with the
	 * base_name.
	 */
	if (!posix_pathnames) {
		stream = strchr_m(smb_fname->base_name, ':');

		if (stream != NULL) {
			char *tmp = talloc_strdup(smb_fname, stream);
			if (tmp == NULL) {
				status = NT_STATUS_NO_MEMORY;
				goto err;
			}
			/*
			 * Since this is actually pointing into
			 * smb_fname->base_name this truncates base_name.
			 */
			*stream = '\0';
			stream = tmp;
		}
	}

	start = smb_fname->base_name;

	/*
	 * If we're providing case insensitive semantics or
	 * the underlying filesystem is case insensitive,
	 * then a case-normalized hit in the stat-cache is
	 * authoratitive. JRA.
	 *
	 * Note: We're only checking base_name.  The stream_name will be
	 * added and verified in build_stream_path().
	 */

	if((!conn->case_sensitive || !(conn->fs_capabilities &
				       FILE_CASE_SENSITIVE_SEARCH)) &&
	    stat_cache_lookup(conn, posix_pathnames, &smb_fname->base_name, &dirpath, &start,
			      &smb_fname->st)) {
		goto done;
	}

	/*
	 * Make sure "dirpath" is an allocated string, we use this for
	 * building the directories with talloc_asprintf and free it.
	 */

	if ((dirpath == NULL) && (!(dirpath = talloc_strdup(ctx,"")))) {
		DEBUG(0, ("talloc_strdup failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto err;
	}

	/*
	 * If we have a wildcard we must walk the path to
	 * find where the error is, even if case sensitive
	 * is true.
	 */

	name_has_wildcard = ms_has_wild(smb_fname->base_name);
	if (name_has_wildcard && !allow_wcard_last_component) {
		/* Wildcard not valid anywhere. */
		status = NT_STATUS_OBJECT_NAME_INVALID;
		goto fail;
	}

	DEBUG(5,("unix_convert begin: name = %s, dirpath = %s, start = %s\n",
		 smb_fname->base_name, dirpath, start));

	if (!name_has_wildcard) {
		/*
		 * stat the name - if it exists then we can add the stream back (if
		 * there was one) and be done!
		 */

		if (posix_pathnames) {
			ret = SMB_VFS_LSTAT(conn, smb_fname);
		} else {
			ret = SMB_VFS_STAT(conn, smb_fname);
		}

		if (ret == 0) {
			status = check_for_dot_component(smb_fname);
			if (!NT_STATUS_IS_OK(status)) {
				goto fail;
			}
			/* Add the path (not including the stream) to the cache. */
			stat_cache_add(orig_path, smb_fname->base_name,
				       conn->case_sensitive);
			DEBUG(5,("conversion of base_name finished %s -> %s\n",
				 orig_path, smb_fname->base_name));
			goto done;
		}

		/* Stat failed - ensure we don't use it. */
		SET_STAT_INVALID(smb_fname->st);

		if (errno == ENOENT) {
			/* Optimization when creating a new file - only
			   the last component doesn't exist.
			   NOTE : check_parent_exists() doesn't preserve errno.
			*/
			int saved_errno = errno;
			status = check_parent_exists(ctx,
						conn,
						posix_pathnames,
						smb_fname,
						&dirpath,
						&start);
			errno = saved_errno;
			if (!NT_STATUS_IS_OK(status)) {
				goto fail;
			}
		}

		/*
		 * A special case - if we don't have any wildcards or mangling chars and are case
		 * sensitive or the underlying filesystem is case insensitive then searching
		 * won't help.
		 */

		if ((conn->case_sensitive || !(conn->fs_capabilities &
					FILE_CASE_SENSITIVE_SEARCH)) &&
				!mangle_is_mangled(smb_fname->base_name, conn->params)) {

			status = check_for_dot_component(smb_fname);
			if (!NT_STATUS_IS_OK(status)) {
				goto fail;
			}

			/*
			 * The stat failed. Could be ok as it could be
 			 * a new file.
 			 */

			if (errno == ENOTDIR || errno == ELOOP) {
				status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
				goto fail;
			} else if (errno == ENOENT) {
				/*
				 * Was it a missing last component ?
				 * or a missing intermediate component ?
				 */
				struct smb_filename parent_fname;
				const char *last_component = NULL;

				ZERO_STRUCT(parent_fname);
				if (!parent_dirname(ctx, smb_fname->base_name,
							&parent_fname.base_name,
							&last_component)) {
					status = NT_STATUS_NO_MEMORY;
					goto fail;
				}
				if (posix_pathnames) {
					ret = SMB_VFS_LSTAT(conn, &parent_fname);
				} else {
					ret = SMB_VFS_STAT(conn, &parent_fname);
				}
				if (ret == -1) {
					if (errno == ENOTDIR ||
							errno == ENOENT ||
							errno == ELOOP) {
						status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
						goto fail;
					}
				}

				/*
				 * Missing last component is ok - new file.
				 * Also deal with permission denied elsewhere.
				 * Just drop out to done.
				 */
				goto done;
			}
		}
	} else {
		/*
		 * We have a wildcard in the pathname.
		 *
		 * Optimization for common case where the wildcard
		 * is in the last component and the client already
		 * sent the correct case.
		 * NOTE : check_parent_exists() doesn't preserve errno.
		 */
		int saved_errno = errno;
		status = check_parent_exists(ctx,
					conn,
					posix_pathnames,
					smb_fname,
					&dirpath,
					&start);
		errno = saved_errno;
		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}
	}

	/*
	 * is_mangled() was changed to look at an entire pathname, not
	 * just a component. JRA.
	 */

	if (mangle_is_mangled(start, conn->params)) {
		component_was_mangled = True;
	}

	/*
	 * Now we need to recursively match the name against the real
	 * directory structure.
	 */

	/*
	 * Match each part of the path name separately, trying the names
	 * as is first, then trying to scan the directory for matching names.
	 */

	for (; start ; start = (end?end+1:(char *)NULL)) {
		/*
		 * Pinpoint the end of this section of the filename.
		 */
		/* mb safe. '/' can't be in any encoded char. */
		end = strchr(start, '/');

		/*
		 * Chop the name at this point.
		 */
		if (end) {
			*end = 0;
		}

		if (save_last_component) {
			TALLOC_FREE(smb_fname->original_lcomp);
			smb_fname->original_lcomp = talloc_strdup(smb_fname,
							end ? end + 1 : start);
			if (!smb_fname->original_lcomp) {
				DEBUG(0, ("talloc failed\n"));
				status = NT_STATUS_NO_MEMORY;
				goto err;
			}
		}

		/* The name cannot have a component of "." */

		if (ISDOT(start)) {
			if (!end)  {
				/* Error code at the end of a pathname. */
				status = NT_STATUS_OBJECT_NAME_INVALID;
			} else {
				status = determine_path_error(end+1,
						allow_wcard_last_component);
			}
			goto fail;
		}

		/* The name cannot have a wildcard if it's not
		   the last component. */

		name_has_wildcard = ms_has_wild(start);

		/* Wildcards never valid within a pathname. */
		if (name_has_wildcard && end) {
			status = NT_STATUS_OBJECT_NAME_INVALID;
			goto fail;
		}

		/* Skip the stat call if it's a wildcard end. */
		if (name_has_wildcard) {
			DEBUG(5,("Wildcard %s\n",start));
			goto done;
		}

		/*
		 * Check if the name exists up to this point.
		 */

		if (posix_pathnames) {
			ret = SMB_VFS_LSTAT(conn, smb_fname);
		} else {
			ret = SMB_VFS_STAT(conn, smb_fname);
		}

		if (ret == 0) {
			/*
			 * It exists. it must either be a directory or this must
			 * be the last part of the path for it to be OK.
			 */
			if (end && !S_ISDIR(smb_fname->st.st_ex_mode)) {
				/*
				 * An intermediate part of the name isn't
				 * a directory.
				 */
				DEBUG(5,("Not a dir %s\n",start));
				*end = '/';
				/*
				 * We need to return the fact that the
				 * intermediate name resolution failed. This
				 * is used to return an error of ERRbadpath
				 * rather than ERRbadfile. Some Windows
				 * applications depend on the difference between
				 * these two errors.
				 */
				status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
				goto fail;
			}

		} else {
			char *found_name = NULL;

			/* Stat failed - ensure we don't use it. */
			SET_STAT_INVALID(smb_fname->st);

			/*
			 * Reset errno so we can detect
			 * directory open errors.
			 */
			errno = 0;

			/*
			 * Try to find this part of the path in the directory.
			 */

			if (name_has_wildcard ||
			    (get_real_filename(conn, dirpath, start,
					       talloc_tos(),
					       &found_name) == -1)) {
				char *unmangled;

				if (end) {
					/*
					 * An intermediate part of the name
					 * can't be found.
					 */
					DEBUG(5,("Intermediate not found %s\n",
							start));
					*end = '/';

					/*
					 * We need to return the fact that the
					 * intermediate name resolution failed.
					 * This is used to return an error of
					 * ERRbadpath rather than ERRbadfile.
					 * Some Windows applications depend on
					 * the difference between these two
					 * errors.
					 */

					/*
					 * ENOENT, ENOTDIR and ELOOP all map
					 * to NT_STATUS_OBJECT_PATH_NOT_FOUND
					 * in the filename walk.
					 */

					if (errno == ENOENT ||
							errno == ENOTDIR ||
							errno == ELOOP) {
						status =
						NT_STATUS_OBJECT_PATH_NOT_FOUND;
					}
					else {
						status =
						map_nt_error_from_unix(errno);
					}
					goto fail;
				}

				/*
				 * ENOENT/EACCESS are the only valid errors
				 * here.
				 */
				if (errno == EACCES) {
					if (ucf_flags & UCF_CREATING_FILE) {
						/*
						 * This is the dropbox
						 * behaviour. A dropbox is a
						 * directory with only -wx
						 * permissions, so
						 * get_real_filename fails
						 * with EACCESS, it needs to
						 * list the directory. We
						 * nevertheless want to allow
						 * users creating a file.
						 */
						status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
					} else {
						status = NT_STATUS_ACCESS_DENIED;
					}
					goto fail;
				}

				if ((errno != 0) && (errno != ENOENT)) {
					/*
					 * ENOTDIR and ELOOP both map to
					 * NT_STATUS_OBJECT_PATH_NOT_FOUND
					 * in the filename walk.
					 */
					if (errno == ENOTDIR ||
							errno == ELOOP) {
						status =
						NT_STATUS_OBJECT_PATH_NOT_FOUND;
					} else {
						status =
						map_nt_error_from_unix(errno);
					}
					goto fail;
				}

				/*
				 * Just the last part of the name doesn't exist.
				 * We need to strupper() or strlower() it as
				 * this conversion may be used for file creation
				 * purposes. Fix inspired by
				 * Thomas Neumann <t.neumann@iku-ag.de>.
				 */
				if (!conn->case_preserve ||
				    (mangle_is_8_3(start, False,
						   conn->params) &&
						 !conn->short_case_preserve)) {
					strnorm(start,
						lp_defaultcase(SNUM(conn)));
				}

				/*
				 * check on the mangled stack to see if we can
				 * recover the base of the filename.
				 */

				if (mangle_is_mangled(start, conn->params)
				    && mangle_lookup_name_from_8_3(ctx,
					    		start,
							&unmangled,
							conn->params)) {
					char *tmp;
					size_t start_ofs =
					    start - smb_fname->base_name;

					if (*dirpath != '\0') {
						tmp = talloc_asprintf(
							smb_fname, "%s/%s",
							dirpath, unmangled);
						TALLOC_FREE(unmangled);
					}
					else {
						tmp = unmangled;
					}
					if (tmp == NULL) {
						DEBUG(0, ("talloc failed\n"));
						status = NT_STATUS_NO_MEMORY;
						goto err;
					}
					TALLOC_FREE(smb_fname->base_name);
					smb_fname->base_name = tmp;
					start =
					    smb_fname->base_name + start_ofs;
					end = start + strlen(start);
				}

				DEBUG(5,("New file %s\n",start));
				goto done;
			}


			/*
			 * Restore the rest of the string. If the string was
			 * mangled the size may have changed.
			 */
			if (end) {
				char *tmp;
				size_t start_ofs =
				    start - smb_fname->base_name;

				if (*dirpath != '\0') {
					tmp = talloc_asprintf(smb_fname,
						"%s/%s/%s", dirpath,
						found_name, end+1);
				}
				else {
					tmp = talloc_asprintf(smb_fname,
						"%s/%s", found_name,
						end+1);
				}
				if (tmp == NULL) {
					DEBUG(0, ("talloc_asprintf failed\n"));
					status = NT_STATUS_NO_MEMORY;
					goto err;
				}
				TALLOC_FREE(smb_fname->base_name);
				smb_fname->base_name = tmp;
				start = smb_fname->base_name + start_ofs;
				end = start + strlen(found_name);
				*end = '\0';
			} else {
				char *tmp;
				size_t start_ofs =
				    start - smb_fname->base_name;

				if (*dirpath != '\0') {
					tmp = talloc_asprintf(smb_fname,
						"%s/%s", dirpath,
						found_name);
				} else {
					tmp = talloc_strdup(smb_fname,
						found_name);
				}
				if (tmp == NULL) {
					DEBUG(0, ("talloc failed\n"));
					status = NT_STATUS_NO_MEMORY;
					goto err;
				}
				TALLOC_FREE(smb_fname->base_name);
				smb_fname->base_name = tmp;
				start = smb_fname->base_name + start_ofs;

				/*
				 * We just scanned for, and found the end of
				 * the path. We must return a valid stat struct
				 * if it exists. JRA.
				 */

				if (posix_pathnames) {
					ret = SMB_VFS_LSTAT(conn, smb_fname);
				} else {
					ret = SMB_VFS_STAT(conn, smb_fname);
				}

				if (ret != 0) {
					SET_STAT_INVALID(smb_fname->st);
				}
			}

			TALLOC_FREE(found_name);
		} /* end else */

#ifdef DEVELOPER
		/*
		 * This sucks!
		 * We should never provide different behaviors
		 * depending on DEVELOPER!!!
		 */
		if (VALID_STAT(smb_fname->st)) {
			bool delete_pending;
			uint32_t name_hash;

			status = file_name_hash(conn,
					smb_fname_str_dbg(smb_fname),
					&name_hash);
			if (!NT_STATUS_IS_OK(status)) {
				goto fail;
			}

			get_file_infos(vfs_file_id_from_sbuf(conn,
							     &smb_fname->st),
				       name_hash,
				       &delete_pending, NULL);
			if (delete_pending) {
				status = NT_STATUS_DELETE_PENDING;
				goto fail;
			}
		}
#endif

		/*
		 * Add to the dirpath that we have resolved so far.
		 */

		if (*dirpath != '\0') {
			char *tmp = talloc_asprintf(ctx,
					"%s/%s", dirpath, start);
			if (!tmp) {
				DEBUG(0, ("talloc_asprintf failed\n"));
				status = NT_STATUS_NO_MEMORY;
				goto err;
			}
			TALLOC_FREE(dirpath);
			dirpath = tmp;
		}
		else {
			TALLOC_FREE(dirpath);
			if (!(dirpath = talloc_strdup(ctx,start))) {
				DEBUG(0, ("talloc_strdup failed\n"));
				status = NT_STATUS_NO_MEMORY;
				goto err;
			}
		}

		/*
		 * Cache the dirpath thus far. Don't cache a name with mangled
		 * or wildcard components as this can change the size.
		 */
		if(!component_was_mangled && !name_has_wildcard) {
			stat_cache_add(orig_path, dirpath,
					conn->case_sensitive);
		}

		/*
		 * Restore the / that we wiped out earlier.
		 */
		if (end) {
			*end = '/';
		}
	}

	/*
	 * Cache the full path. Don't cache a name with mangled or wildcard
	 * components as this can change the size.
	 */

	if(!component_was_mangled && !name_has_wildcard) {
		stat_cache_add(orig_path, smb_fname->base_name,
			       conn->case_sensitive);
	}

	/*
	 * The name has been resolved.
	 */

	DEBUG(5,("conversion finished %s -> %s\n", orig_path,
		 smb_fname->base_name));

 done:
	/* Add back the stream if one was stripped off originally. */
	if (stream != NULL) {
		smb_fname->stream_name = stream;

		/* Check path now that the base_name has been converted. */
		status = build_stream_path(ctx, conn, orig_path, smb_fname);
		if (!NT_STATUS_IS_OK(status)) {
			goto fail;
		}
	}
	TALLOC_FREE(dirpath);
	*smb_fname_out = smb_fname;
	return NT_STATUS_OK;
 fail:
	DEBUG(10, ("dirpath = [%s] start = [%s]\n", dirpath, start));
	if (*dirpath != '\0') {
		smb_fname->base_name = talloc_asprintf(smb_fname, "%s/%s",
						       dirpath, start);
	} else {
		smb_fname->base_name = talloc_strdup(smb_fname, start);
	}
	if (!smb_fname->base_name) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto err;
	}

	*smb_fname_out = smb_fname;
	TALLOC_FREE(dirpath);
	return status;
 err:
	TALLOC_FREE(smb_fname);
	return status;
}

/****************************************************************************
 Ensure a path is not vetod.
****************************************************************************/

NTSTATUS check_veto_path(connection_struct *conn, const char *name)
{
	if (IS_VETO_PATH(conn, name))  {
		/* Is it not dot or dot dot. */
		if (!(ISDOT(name) || ISDOTDOT(name))) {
			DEBUG(5,("check_veto_path: file path name %s vetoed\n",
						name));
			return map_nt_error_from_unix(ENOENT);
		}
	}
	return NT_STATUS_OK;
}

/****************************************************************************
 Check a filename - possibly calling check_reduced_name.
 This is called by every routine before it allows an operation on a filename.
 It does any final confirmation necessary to ensure that the filename is
 a valid one for the user to access.
****************************************************************************/

NTSTATUS check_name(connection_struct *conn, const char *name)
{
	NTSTATUS status = check_veto_path(conn, name);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!lp_widelinks(SNUM(conn)) || !lp_symlinks(SNUM(conn))) {
		status = check_reduced_name(conn,name);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(5,("check_name: name %s failed with %s\n",name,
						nt_errstr(status)));
			return status;
		}
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 Check if two filenames are equal.
 This needs to be careful about whether we are case sensitive.
****************************************************************************/

static bool fname_equal(const char *name1, const char *name2,
		bool case_sensitive)
{
	/* Normal filename handling */
	if (case_sensitive) {
		return(strcmp(name1,name2) == 0);
	}

	return(strequal(name1,name2));
}

/****************************************************************************
 Scan a directory to find a filename, matching without case sensitivity.
 If the name looks like a mangled name then try via the mangling functions
****************************************************************************/

static int get_real_filename_full_scan(connection_struct *conn,
				       const char *path, const char *name,
				       bool mangled,
				       TALLOC_CTX *mem_ctx, char **found_name)
{
	struct smb_Dir *cur_dir;
	const char *dname = NULL;
	char *talloced = NULL;
	char *unmangled_name = NULL;
	long curpos;

	/* handle null paths */
	if ((path == NULL) || (*path == 0)) {
		path = ".";
	}

	/* If we have a case-sensitive filesystem, it doesn't do us any
	 * good to search for a name. If a case variation of the name was
	 * there, then the original stat(2) would have found it.
	 */
	if (!mangled && !(conn->fs_capabilities & FILE_CASE_SENSITIVE_SEARCH)) {
		errno = ENOENT;
		return -1;
	}

	/*
	 * The incoming name can be mangled, and if we de-mangle it
	 * here it will not compare correctly against the filename (name2)
	 * read from the directory and then mangled by the name_to_8_3()
	 * call. We need to mangle both names or neither.
	 * (JRA).
	 *
	 * Fix for bug found by Dina Fine. If in case sensitive mode then
	 * the mangle cache is no good (3 letter extension could be wrong
	 * case - so don't demangle in this case - leave as mangled and
	 * allow the mangling of the directory entry read (which is done
	 * case insensitively) to match instead. This will lead to more
	 * false positive matches but we fail completely without it. JRA.
	 */

	if (mangled && !conn->case_sensitive) {
		mangled = !mangle_lookup_name_from_8_3(talloc_tos(), name,
						       &unmangled_name,
						       conn->params);
		if (!mangled) {
			/* Name is now unmangled. */
			name = unmangled_name;
		}
	}

	/* open the directory */
	if (!(cur_dir = OpenDir(talloc_tos(), conn, path, NULL, 0))) {
		DEBUG(3,("scan dir didn't open dir [%s]\n",path));
		TALLOC_FREE(unmangled_name);
		return -1;
	}

	/* now scan for matching names */
	curpos = 0;
	while ((dname = ReadDirName(cur_dir, &curpos, NULL, &talloced))) {

		/* Is it dot or dot dot. */
		if (ISDOT(dname) || ISDOTDOT(dname)) {
			TALLOC_FREE(talloced);
			continue;
		}

		/*
		 * At this point dname is the unmangled name.
		 * name is either mangled or not, depending on the state
		 * of the "mangled" variable. JRA.
		 */

		/*
		 * Check mangled name against mangled name, or unmangled name
		 * against unmangled name.
		 */

		if ((mangled && mangled_equal(name,dname,conn->params)) ||
			fname_equal(name, dname, conn->case_sensitive)) {
			/* we've found the file, change it's name and return */
			*found_name = talloc_strdup(mem_ctx, dname);
			TALLOC_FREE(unmangled_name);
			TALLOC_FREE(cur_dir);
			if (!*found_name) {
				errno = ENOMEM;
				TALLOC_FREE(talloced);
				return -1;
			}
			TALLOC_FREE(talloced);
			return 0;
		}
		TALLOC_FREE(talloced);
	}

	TALLOC_FREE(unmangled_name);
	TALLOC_FREE(cur_dir);
	errno = ENOENT;
	return -1;
}

/****************************************************************************
 Wrapper around the vfs get_real_filename and the full directory scan
 fallback.
****************************************************************************/

int get_real_filename(connection_struct *conn, const char *path,
		      const char *name, TALLOC_CTX *mem_ctx,
		      char **found_name)
{
	int ret;
	bool mangled;

	mangled = mangle_is_mangled(name, conn->params);

	if (mangled) {
		return get_real_filename_full_scan(conn, path, name, mangled,
						   mem_ctx, found_name);
	}

	/* Try the vfs first to take advantage of case-insensitive stat. */
	ret = SMB_VFS_GET_REAL_FILENAME(conn, path, name, mem_ctx, found_name);

	/*
	 * If the case-insensitive stat was successful, or returned an error
	 * other than EOPNOTSUPP then there is no need to fall back on the
	 * full directory scan.
	 */
	if (ret == 0 || (ret == -1 && errno != EOPNOTSUPP)) {
		return ret;
	}

	return get_real_filename_full_scan(conn, path, name, mangled, mem_ctx,
					   found_name);
}

static NTSTATUS build_stream_path(TALLOC_CTX *mem_ctx,
				  connection_struct *conn,
				  const char *orig_path,
				  struct smb_filename *smb_fname)
{
	NTSTATUS status;
	unsigned int i, num_streams = 0;
	struct stream_struct *streams = NULL;

	if (SMB_VFS_STAT(conn, smb_fname) == 0) {
		DEBUG(10, ("'%s' exists\n", smb_fname_str_dbg(smb_fname)));
		return NT_STATUS_OK;
	}

	if (errno != ENOENT) {
		DEBUG(10, ("vfs_stat failed: %s\n", strerror(errno)));
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	/* Fall back to a case-insensitive scan of all streams on the file. */
	status = vfs_streaminfo(conn, NULL, smb_fname->base_name, mem_ctx,
				&num_streams, &streams);

	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		SET_STAT_INVALID(smb_fname->st);
		return NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("vfs_streaminfo failed: %s\n", nt_errstr(status)));
		goto fail;
	}

	for (i=0; i<num_streams; i++) {
		DEBUG(10, ("comparing [%s] and [%s]: ",
			   smb_fname->stream_name, streams[i].name));
		if (fname_equal(smb_fname->stream_name, streams[i].name,
				conn->case_sensitive)) {
			DEBUGADD(10, ("equal\n"));
			break;
		}
		DEBUGADD(10, ("not equal\n"));
	}

	/* Couldn't find the stream. */
	if (i == num_streams) {
		SET_STAT_INVALID(smb_fname->st);
		TALLOC_FREE(streams);
		return NT_STATUS_OK;
	}

	DEBUG(10, ("case insensitive stream. requested: %s, actual: %s\n",
		smb_fname->stream_name, streams[i].name));


	TALLOC_FREE(smb_fname->stream_name);
	smb_fname->stream_name = talloc_strdup(smb_fname, streams[i].name);
	if (smb_fname->stream_name == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	SET_STAT_INVALID(smb_fname->st);

	if (SMB_VFS_STAT(conn, smb_fname) == 0) {
		DEBUG(10, ("'%s' exists\n", smb_fname_str_dbg(smb_fname)));
	}
	status = NT_STATUS_OK;
 fail:
	TALLOC_FREE(streams);
	return status;
}

/**
 * Go through all the steps to validate a filename.
 *
 * @param ctx		talloc_ctx to allocate memory with.
 * @param conn		connection struct for vfs calls.
 * @param dfs_path	Whether this path requires dfs resolution.
 * @param name_in	The unconverted name.
 * @param ucf_flags	flags to pass through to unix_convert().
 *			UCF_ALWAYS_ALLOW_WCARD_LCOMP will be OR'd in if
 *			p_cont_wcard != NULL and is true and
 *			UCF_COND_ALLOW_WCARD_LCOMP.
 * @param p_cont_wcard	If not NULL, will be set to true if the dfs path
 *			resolution detects a wildcard.
 * @param pp_smb_fname	The final converted name will be allocated if the
 *			return is NT_STATUS_OK.
 *
 * @return NT_STATUS_OK if all operations completed succesfully, appropriate
 * 	   error otherwise.
 */
NTSTATUS filename_convert(TALLOC_CTX *ctx,
				connection_struct *conn,
				bool dfs_path,
				const char *name_in,
				uint32_t ucf_flags,
				bool *ppath_contains_wcard,
				struct smb_filename **pp_smb_fname)
{
	NTSTATUS status;
	bool allow_wcards = (ucf_flags & (UCF_COND_ALLOW_WCARD_LCOMP|UCF_ALWAYS_ALLOW_WCARD_LCOMP));
	char *fname = NULL;

	*pp_smb_fname = NULL;

	status = resolve_dfspath_wcard(ctx, conn,
				dfs_path,
				name_in,
				allow_wcards,
				&fname,
				ppath_contains_wcard);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("filename_convert: resolve_dfspath failed "
			"for name %s with %s\n",
			name_in,
			nt_errstr(status) ));
		return status;
	}

	if (is_fake_file_path(name_in)) {
		SMB_STRUCT_STAT st;
		ZERO_STRUCT(st);
		st.st_ex_nlink = 1;
		status = create_synthetic_smb_fname_split(ctx,
							  name_in,
							  &st,
							  pp_smb_fname);
		return status;
	}

	/*
	 * If the caller conditionally allows wildcard lookups, only add the
	 * always allow if the path actually does contain a wildcard.
	 */
	if (ucf_flags & UCF_COND_ALLOW_WCARD_LCOMP &&
	    ppath_contains_wcard != NULL && *ppath_contains_wcard) {
		ucf_flags |= UCF_ALWAYS_ALLOW_WCARD_LCOMP;
	}

	status = unix_convert(ctx, conn, fname, pp_smb_fname, ucf_flags);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10,("filename_convert: unix_convert failed "
			"for name %s with %s\n",
			fname,
			nt_errstr(status) ));
		return status;
	}

	if ((ucf_flags & UCF_UNIX_NAME_LOOKUP) &&
			VALID_STAT((*pp_smb_fname)->st) &&
			S_ISLNK((*pp_smb_fname)->st.st_ex_mode)) {
		return check_veto_path(conn, (*pp_smb_fname)->base_name);
	}

	status = check_name(conn, (*pp_smb_fname)->base_name);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3,("filename_convert: check_name failed "
			"for name %s with %s\n",
			smb_fname_str_dbg(*pp_smb_fname),
			nt_errstr(status) ));
		TALLOC_FREE(*pp_smb_fname);
		return status;
	}

	return status;
}
