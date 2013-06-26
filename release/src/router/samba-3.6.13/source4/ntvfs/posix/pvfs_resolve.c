/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - filename resolution

   Copyright (C) Andrew Tridgell 2004

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
  this is the core code for converting a filename from the format as
  given by a client to a posix filename, including any case-matching
  required, and checks for legal characters
*/


#include "includes.h"
#include "vfs_posix.h"
#include "system/dir.h"
#include "param/param.h"

/**
  compare two filename components. This is where the name mangling hook will go
*/
static int component_compare(struct pvfs_state *pvfs, const char *comp, const char *name)
{
	int ret;

	ret = strcasecmp_m(comp, name);

	if (ret != 0) {
		char *shortname = pvfs_short_name_component(pvfs, name);
		if (shortname) {
			ret = strcasecmp_m(comp, shortname);
			talloc_free(shortname);
		}
	}

	return ret;
}

/*
  search for a filename in a case insensitive fashion

  TODO: add a cache for previously resolved case-insensitive names
  TODO: add mangled name support
*/
static NTSTATUS pvfs_case_search(struct pvfs_state *pvfs,
				 struct pvfs_filename *name,
				 unsigned int flags)
{
	/* break into a series of components */
	int num_components;
	char **components;
	char *p, *partial_name;
	int i;

	/* break up the full name info pathname components */
	num_components=2;
	p = name->full_name + strlen(pvfs->base_directory) + 1;

	for (;*p;p++) {
		if (*p == '/') {
			num_components++;
		}
	}

	components = talloc_array(name, char *, num_components);
	p = name->full_name + strlen(pvfs->base_directory);
	*p++ = 0;

	components[0] = name->full_name;

	for (i=1;i<num_components;i++) {
		components[i] = p;
		p = strchr(p, '/');
		if (p) *p++ = 0;
		if (pvfs_is_reserved_name(pvfs, components[i])) {
			return NT_STATUS_ACCESS_DENIED;
		}
	}

	partial_name = talloc_strdup(name, components[0]);
	if (!partial_name) {
		return NT_STATUS_NO_MEMORY;
	}

	/* for each component, check if it exists as-is, and if not then
	   do a directory scan */
	for (i=1;i<num_components;i++) {
		char *test_name;
		DIR *dir;
		struct dirent *de;
		char *long_component;

		/* possibly remap from the short name cache */
		long_component = pvfs_mangled_lookup(pvfs, name, components[i]);
		if (long_component) {
			components[i] = long_component;
		}

		test_name = talloc_asprintf(name, "%s/%s", partial_name, components[i]);
		if (!test_name) {
			return NT_STATUS_NO_MEMORY;
		}

		/* check if this component exists as-is */
		if (stat(test_name, &name->st) == 0) {
			if (i<num_components-1 && !S_ISDIR(name->st.st_mode)) {
				return NT_STATUS_OBJECT_PATH_NOT_FOUND;
			}
			talloc_free(partial_name);
			partial_name = test_name;
			if (i == num_components - 1) {
				name->exists = true;
			}
			continue;
		}

		/* the filesystem might be case insensitive, in which
		   case a search is pointless unless the name is
		   mangled */
		if ((pvfs->flags & PVFS_FLAG_CI_FILESYSTEM) &&
		    !pvfs_is_mangled_component(pvfs, components[i])) {
			if (i < num_components-1) {
				return NT_STATUS_OBJECT_PATH_NOT_FOUND;
			}
			partial_name = test_name;
			continue;
		}
		
		dir = opendir(partial_name);
		if (!dir) {
			return pvfs_map_errno(pvfs, errno);
		}

		while ((de = readdir(dir))) {
			if (component_compare(pvfs, components[i], de->d_name) == 0) {
				break;
			}
		}

		if (!de) {
			if (i < num_components-1) {
				closedir(dir);
				return NT_STATUS_OBJECT_PATH_NOT_FOUND;
			}
		} else {
			components[i] = talloc_strdup(name, de->d_name);
		}
		test_name = talloc_asprintf(name, "%s/%s", partial_name, components[i]);
		talloc_free(partial_name);
		partial_name = test_name;

		closedir(dir);
	}

	if (!name->exists) {
		if (stat(partial_name, &name->st) == 0) {
			name->exists = true;
		}
	}

	talloc_free(name->full_name);
	name->full_name = partial_name;

	if (name->exists) {
		return pvfs_fill_dos_info(pvfs, name, flags, -1);
	}

	return NT_STATUS_OK;
}

/*
  parse a alternate data stream name
*/
static NTSTATUS parse_stream_name(struct pvfs_filename *name,
				  const char *s)
{
	char *p, *stream_name;
	if (s[1] == '\0') {
		return NT_STATUS_OBJECT_NAME_INVALID;
	}
	name->stream_name = stream_name = talloc_strdup(name, s+1);
	if (name->stream_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	p = stream_name;

	while (*p) {
		size_t c_size;
		codepoint_t c = next_codepoint(p, &c_size);

		switch (c) {
		case '/':
		case '\\':
			return NT_STATUS_OBJECT_NAME_INVALID;
		case ':':
			*p= 0;
			p++;
			if (*p == '\0') {
				return NT_STATUS_OBJECT_NAME_INVALID;
			}
			if (strcasecmp_m(p, "$DATA") != 0) {
				if (strchr_m(p, ':')) {
					return NT_STATUS_OBJECT_NAME_INVALID;
				}
				return NT_STATUS_INVALID_PARAMETER;
			}
			c_size = 0;
			p--;
			break;
		}

		p += c_size;
	}

	if (strcmp(name->stream_name, "") == 0) {
		/*
		 * we don't set stream_name to NULL, here
		 * as this would be wrong for directories
		 *
		 * pvfs_fill_dos_info() will set it to NULL
		 * if it's not a directory.
		 */
		name->stream_id = 0;
	} else {
		name->stream_id = pvfs_name_hash(name->stream_name, 
						 strlen(name->stream_name));
	}
						 
	return NT_STATUS_OK;	
}


/*
  convert a CIFS pathname to a unix pathname. Note that this does NOT
  take into account case insensitivity, and in fact does not access
  the filesystem at all. It is merely a reformatting and charset
  checking routine.

  errors are returned if the filename is illegal given the flags
*/
static NTSTATUS pvfs_unix_path(struct pvfs_state *pvfs, const char *cifs_name,
			       unsigned int flags, struct pvfs_filename *name)
{
	char *ret, *p, *p_start;
	NTSTATUS status;

	name->original_name = talloc_strdup(name, cifs_name);

	/* remove any :$DATA */
	p = strrchr(name->original_name, ':');
	if (p && strcasecmp_m(p, ":$DATA") == 0) {
		if (p > name->original_name && p[-1] == ':') {
			p--;
		}
		*p = 0;
	}

	name->stream_name = NULL;
	name->stream_id = 0;
	name->has_wildcard = false;

	while (*cifs_name == '\\') {
		cifs_name++;
	}

	if (*cifs_name == 0) {
		name->full_name = talloc_asprintf(name, "%s/.", pvfs->base_directory);
		if (name->full_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		return NT_STATUS_OK;
	}

	ret = talloc_asprintf(name, "%s/%s", pvfs->base_directory, cifs_name);
	if (ret == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	p = ret + strlen(pvfs->base_directory) + 1;

	/* now do an in-place conversion of '\' to '/', checking
	   for legal characters */
	p_start = p;

	while (*p) {
		size_t c_size;
		codepoint_t c = next_codepoint(p, &c_size);

		if (c <= 0x1F) {
			return NT_STATUS_OBJECT_NAME_INVALID;
		}

		switch (c) {
		case '\\':
			if (name->has_wildcard) {
				/* wildcards are only allowed in the last part
				   of a name */
				return NT_STATUS_OBJECT_NAME_INVALID;
			}
			if (p > p_start && (p[1] == '\\' || p[1] == '\0')) {
				/* see if it is definately a "\\" or
				 * a trailing "\". If it is then fail here,
				 * and let the next layer up try again after
				 * pvfs_reduce_name() if it wants to. This is
				 * much more efficient on average than always
				 * scanning for these separately
				 */
				return NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
			} else {
				*p = '/';
			}
			break;
		case ':':
			if (!(flags & PVFS_RESOLVE_STREAMS)) {
				return NT_STATUS_OBJECT_NAME_INVALID;
			}
			if (name->has_wildcard) {
				return NT_STATUS_OBJECT_NAME_INVALID;
			}
			status = parse_stream_name(name, p);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
			*p-- = 0;
			break;
		case '*':
		case '>':
		case '<':
		case '?':
		case '"':
			if (!(flags & PVFS_RESOLVE_WILDCARD)) {
				return NT_STATUS_OBJECT_NAME_INVALID;
			}
			name->has_wildcard = true;
			break;
		case '/':
		case '|':
			return NT_STATUS_OBJECT_NAME_INVALID;
		case '.':
			/* see if it is definately a .. or
			   . component. If it is then fail here, and
			   let the next layer up try again after
			   pvfs_reduce_name() if it wants to. This is
			   much more efficient on average than always
			   scanning for these separately */
			if (p[1] == '.' && 
			    (p[2] == 0 || p[2] == '\\') &&
			    (p == p_start || p[-1] == '/')) {
				return NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
			}
			if ((p[1] == 0 || p[1] == '\\') &&
			    (p == p_start || p[-1] == '/')) {
				return NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
			}
			break;
		}

		p += c_size;
	}

	name->full_name = ret;

	return NT_STATUS_OK;
}


/*
  reduce a name that contains .. components or repeated \ separators
  return NULL if it can't be reduced
*/
static NTSTATUS pvfs_reduce_name(TALLOC_CTX *mem_ctx, 
				 const char **fname, unsigned int flags)
{
	codepoint_t c;
	size_t c_size, len;
	int i, num_components, err_count;
	char **components;
	char *p, *s, *ret;

	s = talloc_strdup(mem_ctx, *fname);
	if (s == NULL) return NT_STATUS_NO_MEMORY;

	for (num_components=1, p=s; *p; p += c_size) {
		c = next_codepoint(p, &c_size);
		if (c == '\\') num_components++;
	}

	components = talloc_array(s, char *, num_components+1);
	if (components == NULL) {
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}

	components[0] = s;
	for (i=0, p=s; *p; p += c_size) {
		c = next_codepoint(p, &c_size);
		if (c == '\\') {
			*p = 0;
			components[++i] = p+1;
		}
	}
	components[i+1] = NULL;

	/*
	  rather bizarre!

	  '.' components are not allowed, but the rules for what error
	  code to give don't seem to make sense. This is a close
	  approximation.
	*/
	for (err_count=i=0;components[i];i++) {
		if (strcmp(components[i], "") == 0) {
			continue;
		}
		if (ISDOT(components[i]) || err_count) {
			err_count++;
		}
	}
	if (err_count) {
		if (flags & PVFS_RESOLVE_WILDCARD) err_count--;

		if (err_count==1) {
			return NT_STATUS_OBJECT_NAME_INVALID;
		} else {
			return NT_STATUS_OBJECT_PATH_NOT_FOUND;
		}
	}

	/* remove any null components */
	for (i=0;components[i];i++) {
		if (strcmp(components[i], "") == 0) {
			memmove(&components[i], &components[i+1], 
				sizeof(char *)*(num_components-i));
			i--;
			continue;
		}
		if (ISDOTDOT(components[i])) {
			if (i < 1) return NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
			memmove(&components[i-1], &components[i+1], 
				sizeof(char *)*(num_components-i));
			i -= 2;
			continue;
		}
	}

	if (components[0] == NULL) {
		talloc_free(s);
		*fname = talloc_strdup(mem_ctx, "\\");
		return NT_STATUS_OK;
	}

	for (len=i=0;components[i];i++) {
		len += strlen(components[i]) + 1;
	}

	/* rebuild the name */
	ret = talloc_array(mem_ctx, char, len+1);
	if (ret == NULL) {
		talloc_free(s);
		return NT_STATUS_NO_MEMORY;
	}

	for (len=0,i=0;components[i];i++) {
		size_t len1 = strlen(components[i]);
		ret[len] = '\\';
		memcpy(ret+len+1, components[i], len1);
		len += len1 + 1;
	}	
	ret[len] = 0;

	talloc_set_name_const(ret, ret);

	talloc_free(s);

	*fname = ret;
	
	return NT_STATUS_OK;
}


/*
  resolve a name from relative client format to a struct pvfs_filename
  the memory for the filename is made as a talloc child of 'name'

  flags include:
     PVFS_RESOLVE_NO_WILDCARD = wildcards are considered illegal characters
     PVFS_RESOLVE_STREAMS     = stream names are allowed

     TODO: ../ collapsing, and outside share checking
*/
NTSTATUS pvfs_resolve_name(struct pvfs_state *pvfs, 
			   struct ntvfs_request *req,
			   const char *cifs_name,
			   unsigned int flags, struct pvfs_filename **name)
{
	NTSTATUS status;

	*name = talloc(req, struct pvfs_filename);
	if (*name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	(*name)->exists = false;
	(*name)->stream_exists = false;

	if (!(pvfs->fs_attribs & FS_ATTR_NAMED_STREAMS)) {
		flags &= ~PVFS_RESOLVE_STREAMS;
	}

	/* SMB2 doesn't allow a leading slash */
	if (req->ctx->protocol == PROTOCOL_SMB2 &&
	    *cifs_name == '\\') {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* do the basic conversion to a unix formatted path,
	   also checking for allowable characters */
	status = pvfs_unix_path(pvfs, cifs_name, flags, *name);

	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_PATH_SYNTAX_BAD)) {
		/* it might contain .. components which need to be reduced */
		status = pvfs_reduce_name(*name, &cifs_name, flags);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
		status = pvfs_unix_path(pvfs, cifs_name, flags, *name);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* if it has a wildcard then no point doing a stat() of the
	   full name. Instead We need check if the directory exists 
	 */
	if ((*name)->has_wildcard) {
		const char *p;
		char *dir_name, *saved_name;
		p = strrchr((*name)->full_name, '/');
		if (p == NULL) {
			/* root directory wildcard is OK */
			return NT_STATUS_OK;
		}
		dir_name = talloc_strndup(*name, (*name)->full_name, (p-(*name)->full_name));
		if (stat(dir_name, &(*name)->st) == 0) {
			talloc_free(dir_name);
			return NT_STATUS_OK;
		}
		/* we need to search for a matching name */
		saved_name = (*name)->full_name;
		(*name)->full_name = dir_name;
		status = pvfs_case_search(pvfs, *name, flags);
		if (!NT_STATUS_IS_OK(status)) {
			/* the directory doesn't exist */
			(*name)->full_name = saved_name;
			return status;
		}
		/* it does exist, but might need a case change */
		if (dir_name != (*name)->full_name) {
			(*name)->full_name = talloc_asprintf(*name, "%s%s",
							     (*name)->full_name, p);
			NT_STATUS_HAVE_NO_MEMORY((*name)->full_name);
		} else {
			(*name)->full_name = saved_name;
			talloc_free(dir_name);
		}
		return NT_STATUS_OK;
	}

	/* if we can stat() the full name now then we are done */
	if (stat((*name)->full_name, &(*name)->st) == 0) {
		(*name)->exists = true;
		return pvfs_fill_dos_info(pvfs, *name, flags, -1);
	}

	/* search for a matching filename */
	status = pvfs_case_search(pvfs, *name, flags);

	return status;
}


/*
  do a partial resolve, returning a pvfs_filename structure given a
  base path and a relative component. It is an error if the file does
  not exist. No case-insensitive matching is done.

  this is used in places like directory searching where we need a pvfs_filename
  to pass to a function, but already know the unix base directory and component
*/
NTSTATUS pvfs_resolve_partial(struct pvfs_state *pvfs, TALLOC_CTX *mem_ctx,
			      const char *unix_dir, const char *fname,
			      unsigned int flags, struct pvfs_filename **name)
{
	NTSTATUS status;

	*name = talloc(mem_ctx, struct pvfs_filename);
	if (*name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	(*name)->full_name = talloc_asprintf(*name, "%s/%s", unix_dir, fname);
	if ((*name)->full_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (stat((*name)->full_name, &(*name)->st) == -1) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	(*name)->exists = true;
	(*name)->stream_exists = true;
	(*name)->has_wildcard = false;
	(*name)->original_name = talloc_strdup(*name, fname);
	(*name)->stream_name = NULL;
	(*name)->stream_id = 0;

	status = pvfs_fill_dos_info(pvfs, *name, flags, -1);

	return status;
}


/*
  fill in the pvfs_filename info for an open file, given the current
  info for a (possibly) non-open file. This is used by places that need
  to update the pvfs_filename stat information, and by pvfs_open()
*/
NTSTATUS pvfs_resolve_name_fd(struct pvfs_state *pvfs, int fd,
			      struct pvfs_filename *name, unsigned int flags)
{
	dev_t device = (dev_t)0;
	ino_t inode = 0;

	if (name->exists) {
		device = name->st.st_dev;
		inode = name->st.st_ino;
	}

	if (fd == -1) {
		if (stat(name->full_name, &name->st) == -1) {
			return NT_STATUS_INVALID_HANDLE;
		}
	} else {
		if (fstat(fd, &name->st) == -1) {
			return NT_STATUS_INVALID_HANDLE;
		}
	}

	if (name->exists &&
	    (device != name->st.st_dev || inode != name->st.st_ino)) {
		/* the file we are looking at has changed! this could
		 be someone trying to exploit a race
		 condition. Certainly we don't want to continue
		 operating on this file */
		DEBUG(0,("pvfs: WARNING: file '%s' changed during resolve - failing\n",
			 name->full_name));
		return NT_STATUS_UNEXPECTED_IO_ERROR;
	}

	name->exists = true;
	
	return pvfs_fill_dos_info(pvfs, name, flags, fd);
}

/*
  fill in the pvfs_filename info for an open file, given the current
  info for a (possibly) non-open file. This is used by places that need
  to update the pvfs_filename stat information, and the path
  after a possible rename on a different handle.
*/
NTSTATUS pvfs_resolve_name_handle(struct pvfs_state *pvfs,
				  struct pvfs_file_handle *h)
{
	NTSTATUS status;

	if (h->have_opendb_entry) {
		struct odb_lock *lck;
		char *name = NULL;

		lck = odb_lock(h, h->pvfs->odb_context, &h->odb_locking_key);
		if (lck == NULL) {
			DEBUG(0,("%s: failed to lock file '%s' in opendb\n",
				 __FUNCTION__, h->name->full_name));
			/* we were supposed to do a blocking lock, so something
			   is badly wrong! */
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		status = odb_get_path(lck, (const char **) &name);
		if (NT_STATUS_IS_OK(status)) {
			/*
			 * This relies an the fact that
			 * renames of open files are only
			 * allowed by setpathinfo() and setfileinfo()
			 * and there're only renames within the same
			 * directory supported
			 */
			if (strcmp(h->name->full_name, name) != 0) {
				const char *orig_dir;
				const char *new_file;
				char *new_orig;
				char *delim;

				delim = strrchr(name, '/');
				if (!delim) {
					talloc_free(lck);
					return NT_STATUS_INTERNAL_ERROR;
				}

				new_file = delim + 1;
				delim = strrchr(h->name->original_name, '\\');
				if (delim) {
					delim[0] = '\0';
					orig_dir = h->name->original_name;
					new_orig = talloc_asprintf(h->name, "%s\\%s",
								   orig_dir, new_file);
					if (!new_orig) {
						talloc_free(lck);
						return NT_STATUS_NO_MEMORY;
					}
				} else {
					new_orig = talloc_strdup(h->name, new_file);
					if (!new_orig) {
						talloc_free(lck);
						return NT_STATUS_NO_MEMORY;
					}
				}

				talloc_free(h->name->original_name);
				talloc_free(h->name->full_name);
				h->name->full_name = talloc_steal(h->name, name);
				h->name->original_name = new_orig;
			}
		}

		talloc_free(lck);
	}

	/*
	 * TODO: pass PVFS_RESOLVE_NO_OPENDB and get
	 *       the write time from odb_lock() above.
	 */
	status = pvfs_resolve_name_fd(pvfs, h->fd, h->name, 0);
	NT_STATUS_NOT_OK_RETURN(status);

	if (!null_nttime(h->write_time.close_time)) {
		h->name->dos.write_time = h->write_time.close_time;
	}

	return NT_STATUS_OK;
}


/*
  resolve the parent of a given name
*/
NTSTATUS pvfs_resolve_parent(struct pvfs_state *pvfs, TALLOC_CTX *mem_ctx,
			     const struct pvfs_filename *child,
			     struct pvfs_filename **name)
{
	NTSTATUS status;
	char *p;

	*name = talloc(mem_ctx, struct pvfs_filename);
	if (*name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	(*name)->full_name = talloc_strdup(*name, child->full_name);
	if ((*name)->full_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	p = strrchr_m((*name)->full_name, '/');
	if (p == NULL) {
		return NT_STATUS_OBJECT_PATH_SYNTAX_BAD;
	}

	/* this handles the root directory */
	if (p == (*name)->full_name) {
		p[1] = 0;
	} else {
		p[0] = 0;
	}

	if (stat((*name)->full_name, &(*name)->st) == -1) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	(*name)->exists = true;
	(*name)->stream_exists = true;
	(*name)->has_wildcard = false;
	/* we can't get the correct 'original_name', but for the purposes
	   of this call this is close enough */
	(*name)->original_name = talloc_strdup(*name, child->original_name);
	if ((*name)->original_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	(*name)->stream_name = NULL;
	(*name)->stream_id = 0;

	status = pvfs_fill_dos_info(pvfs, *name, PVFS_RESOLVE_NO_OPENDB, -1);

	return status;
}
