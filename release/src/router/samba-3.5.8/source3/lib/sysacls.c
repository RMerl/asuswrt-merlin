/* 
   Unix SMB/CIFS implementation.
   Samba system utilities for ACL support.
   Copyright (C) Jeremy Allison 2000.
   Copyright (C) Volker Lendecke 2006
   Copyright (C) Michael Adam 2006,2008
   
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

#undef  DBGC_CLASS
#define DBGC_CLASS DBGC_ACLS

/*
 * Note that while this code implements sufficient functionality
 * to support the sys_acl_* interfaces it does not provide all
 * of the semantics of the POSIX ACL interfaces.
 *
 * In particular, an ACL entry descriptor (SMB_ACL_ENTRY_T) returned
 * from a call to sys_acl_get_entry() should not be assumed to be
 * valid after calling any of the following functions, which may
 * reorder the entries in the ACL.
 *
 *	sys_acl_valid()
 *	sys_acl_set_file()
 *	sys_acl_set_fd()
 */

int sys_acl_get_entry(SMB_ACL_T acl_d, int entry_id, SMB_ACL_ENTRY_T *entry_p)
{
	if (entry_id != SMB_ACL_FIRST_ENTRY && entry_id != SMB_ACL_NEXT_ENTRY) {
		errno = EINVAL;
		return -1;
	}

	if (entry_p == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (entry_id == SMB_ACL_FIRST_ENTRY) {
		acl_d->next = 0;
	}

	if (acl_d->next < 0) {
		errno = EINVAL;
		return -1;
	}

	if (acl_d->next >= acl_d->count) {
		return 0;
	}

	*entry_p = &acl_d->acl[acl_d->next++];

	return 1;
}

int sys_acl_get_tag_type(SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T *type_p)
{
	*type_p = entry_d->a_type;

	return 0;
}

int sys_acl_get_permset(SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T *permset_p)
{
	*permset_p = &entry_d->a_perm;

	return 0;
}

void *sys_acl_get_qualifier(SMB_ACL_ENTRY_T entry_d)
{
	if (entry_d->a_type == SMB_ACL_USER) {
		return &entry_d->uid;
		}

	if (entry_d->a_type == SMB_ACL_GROUP) {
		return &entry_d->gid;
	}

	errno = EINVAL;
		return NULL;
}

int sys_acl_clear_perms(SMB_ACL_PERMSET_T permset_d)
{
	*permset_d = 0;

	return 0;
}

int sys_acl_add_perm(SMB_ACL_PERMSET_T permset_d, SMB_ACL_PERM_T perm)
{
	if (perm != SMB_ACL_READ && perm != SMB_ACL_WRITE
	    && perm != SMB_ACL_EXECUTE) {
		errno = EINVAL;
		return -1;
		}
 
	if (permset_d == NULL) {
		errno = EINVAL;
		return -1;
	}

	*permset_d |= perm;
 
	return 0;
}

int sys_acl_get_perm(SMB_ACL_PERMSET_T permset_d, SMB_ACL_PERM_T perm)
{
	return *permset_d & perm;
}

char *sys_acl_to_text(SMB_ACL_T acl_d, ssize_t *len_p)
{
	int	i;
	int	len, maxlen;
	char	*text;

	/*
	 * use an initial estimate of 20 bytes per ACL entry
	 * when allocating memory for the text representation
	 * of the ACL
	 */
	len	= 0;
	maxlen	= 20 * acl_d->count;
	if ((text = (char *)SMB_MALLOC(maxlen)) == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	for (i = 0; i < acl_d->count; i++) {
		struct smb_acl_entry *ap = &acl_d->acl[i];
		struct group	*gr;
		char		tagbuf[12];
		char		idbuf[12];
		const char	*tag;
		const char	*id	= "";
		char		perms[4];
		int		nbytes;

		switch (ap->a_type) {
			/*
			 * for debugging purposes it's probably more
			 * useful to dump unknown tag types rather
			 * than just returning an error
			 */
			default:
				slprintf(tagbuf, sizeof(tagbuf)-1, "0x%x",
					 ap->a_type);
				tag = tagbuf;
				break;
 
			case SMB_ACL_USER:
				id = uidtoname(ap->uid);
			case SMB_ACL_USER_OBJ:
				tag = "user";
				break;

			case SMB_ACL_GROUP:
				if ((gr = getgrgid(ap->gid)) == NULL) {
					slprintf(idbuf, sizeof(idbuf)-1, "%ld",
						(long)ap->gid);
					id = idbuf;
				} else {
					id = gr->gr_name;
				}
			case SMB_ACL_GROUP_OBJ:
				tag = "group";
				break;

			case SMB_ACL_OTHER:
				tag = "other";
				break;

			case SMB_ACL_MASK:
				tag = "mask";
				break;

		}

		perms[0] = (ap->a_perm & SMB_ACL_READ) ? 'r' : '-';
		perms[1] = (ap->a_perm & SMB_ACL_WRITE) ? 'w' : '-';
		perms[2] = (ap->a_perm & SMB_ACL_EXECUTE) ? 'x' : '-';
		perms[3] = '\0';

		/*          <tag>      :  <qualifier>   :  rwx \n  \0 */
		nbytes = strlen(tag) + 1 + strlen(id) + 1 + 3 + 1 + 1;

		/*
		 * If this entry would overflow the buffer
		 * allocate enough additional memory for this
		 * entry and an estimate of another 20 bytes
		 * for each entry still to be processed
		 */
		if ((len + nbytes) > maxlen) {
			maxlen += nbytes + 20 * (acl_d->count - i);
			if ((text = (char *)SMB_REALLOC(text, maxlen)) == NULL) {
			errno = ENOMEM;
				return NULL;
		}
	}

		slprintf(&text[len], nbytes-1, "%s:%s:%s\n", tag, id, perms);
		len += nbytes - 1;
	}

	if (len_p)
		*len_p = len;

	return text;
}

SMB_ACL_T sys_acl_init(int count)
{
	SMB_ACL_T	a;

	if (count < 0) {
		errno = EINVAL;
		return NULL;
	}

	/*
	 * note that since the definition of the structure pointed
	 * to by the SMB_ACL_T includes the first element of the
	 * acl[] array, this actually allocates an ACL with room
	 * for (count+1) entries
	 */
	if ((a = (struct smb_acl_t *)SMB_MALLOC(
		     sizeof(struct smb_acl_t) +
		     count * sizeof(struct smb_acl_entry))) == NULL) {
		errno = ENOMEM;
		return NULL;
	}
 
	a->size = count + 1;
	a->count = 0;
	a->next = -1;

	return a;
}

int sys_acl_create_entry(SMB_ACL_T *acl_p, SMB_ACL_ENTRY_T *entry_p)
{
	SMB_ACL_T	acl_d;
	SMB_ACL_ENTRY_T	entry_d;

	if (acl_p == NULL || entry_p == NULL || (acl_d = *acl_p) == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (acl_d->count >= acl_d->size) {
		errno = ENOSPC;
		return -1;
	}

	entry_d		= &acl_d->acl[acl_d->count++];
	entry_d->a_type	= SMB_ACL_TAG_INVALID;
	entry_d->uid	= -1;
	entry_d->gid	= -1;
	entry_d->a_perm	= 0;
	*entry_p	= entry_d;

	return 0;
}

int sys_acl_set_tag_type(SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T tag_type)
{
	switch (tag_type) {
		case SMB_ACL_USER:
		case SMB_ACL_USER_OBJ:
		case SMB_ACL_GROUP:
		case SMB_ACL_GROUP_OBJ:
		case SMB_ACL_OTHER:
		case SMB_ACL_MASK:
			entry_d->a_type = tag_type;
			break;
		default:
			errno = EINVAL;
			return -1;
		}

	return 0;
}

int sys_acl_set_qualifier(SMB_ACL_ENTRY_T entry_d, void *qual_p)
{
	if (entry_d->a_type == SMB_ACL_USER) {
		entry_d->uid = *((uid_t *)qual_p);
		return 0;
		}
	if (entry_d->a_type == SMB_ACL_GROUP) {
		entry_d->gid = *((gid_t *)qual_p);
		return 0;
	}

	errno = EINVAL;
	return -1;
}

int sys_acl_set_permset(SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T permset_d)
{
	if (*permset_d & ~(SMB_ACL_READ|SMB_ACL_WRITE|SMB_ACL_EXECUTE)) {
		errno = EINVAL;
		return -1;
	}

	entry_d->a_perm = *permset_d;
 
	return 0;
}

int sys_acl_free_text(char *text)
{
	SAFE_FREE(text);
	return 0;
}

int sys_acl_free_acl(SMB_ACL_T acl_d) 
{
	SAFE_FREE(acl_d);
	return 0;
}

int sys_acl_free_qualifier(void *qual, SMB_ACL_TAG_T tagtype)
{
	return 0;
}

int sys_acl_valid(SMB_ACL_T acl_d)
{
	errno = EINVAL;
	return -1;
}

/*
 * acl_get_file, acl_get_fd, acl_set_file, acl_set_fd and
 * sys_acl_delete_def_file are to be redirected to the default
 * statically-bound acl vfs module, but they are replacable.
 */
 
#if defined(HAVE_POSIX_ACLS)
 
SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle, 
			   const char *path_p, SMB_ACL_TYPE_T type)
{
	return posixacl_sys_acl_get_file(handle, path_p, type);
}
 
SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return posixacl_sys_acl_get_fd(handle, fsp);
}
 
int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
	return posixacl_sys_acl_set_file(handle, name, type, acl_d);
}
 
int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
	return posixacl_sys_acl_set_fd(handle, fsp, acl_d);
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
	return posixacl_sys_acl_delete_def_file(handle, path);
}

#elif defined(HAVE_AIX_ACLS)

SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle,
			   const char *path_p, SMB_ACL_TYPE_T type)
{
	return aixacl_sys_acl_get_file(handle, path_p, type);
}

SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return aixacl_sys_acl_get_fd(handle, fsp);
}

int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
	return aixacl_sys_acl_set_file(handle, name, type, acl_d);
}

int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
	return aixacl_sys_acl_set_fd(handle, fsp, acl_d);
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
	return aixacl_sys_acl_delete_def_file(handle, path);
}

#elif defined(HAVE_TRU64_ACLS)
 
SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle,
			   const char *path_p, SMB_ACL_TYPE_T type)
{
	return tru64acl_sys_acl_get_file(handle, path_p, type);
}

SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return tru64acl_sys_acl_get_fd(handle, fsp);
}

int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
	return tru64acl_sys_acl_set_file(handle, name, type, acl_d);
}

int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
	return tru64acl_sys_acl_set_fd(handle, fsp, acl_d);
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
	return tru64acl_sys_acl_delete_def_file(handle, path);
}

#elif defined(HAVE_SOLARIS_ACLS) || defined(HAVE_UNIXWARE_ACLS)

SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle,
			   const char *path_p, SMB_ACL_TYPE_T type)
{
	return solarisacl_sys_acl_get_file(handle, path_p, type);
}

SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return solarisacl_sys_acl_get_fd(handle, fsp);
}

int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
	return solarisacl_sys_acl_set_file(handle, name, type, acl_d);
}

int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
	return solarisacl_sys_acl_set_fd(handle, fsp, acl_d);
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
	return solarisacl_sys_acl_delete_def_file(handle, path);
}

#elif defined(HAVE_HPUX_ACLS)

SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle,
			   const char *path_p, SMB_ACL_TYPE_T type)
{
	return hpuxacl_sys_acl_get_file(handle, path_p, type);
}

SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return hpuxacl_sys_acl_get_fd(handle, fsp);
}

int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
	return hpuxacl_sys_acl_set_file(handle, name, type, acl_d);
}

int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
	return hpuxacl_sys_acl_set_fd(handle, fsp, acl_d);
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
	return hpuxacl_sys_acl_delete_def_file(handle, path);
}

#elif defined(HAVE_IRIX_ACLS)

SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle,
			   const char *path_p, SMB_ACL_TYPE_T type)
{
	return irixacl_sys_acl_get_file(handle, path_p, type);
}

SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
	return irixacl_sys_acl_get_fd(handle, fsp);
}

int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
	return irixacl_sys_acl_set_file(handle, name, type, acl_d);
}

int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
	return irixacl_sys_acl_set_fd(handle, fsp, acl_d);
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
	return irixacl_sys_acl_delete_def_file(handle, path);
}

#else /* No ACLs. */

SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle,
			   const char *path_p, SMB_ACL_TYPE_T type)
{
#ifdef ENOTSUP
	errno = ENOTSUP;
#else
	errno = ENOSYS;
#endif
	return NULL;
}

SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp)
{
#ifdef ENOTSUP
	errno = ENOTSUP;
#else
	errno = ENOSYS;
#endif
	return NULL;
}

int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d)
{
#ifdef ENOTSUP
	errno = ENOTSUP;
#else
	errno = ENOSYS;
#endif
	return -1;
}

int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d)
{
#ifdef ENOTSUP
	errno = ENOTSUP;
#else
	errno = ENOSYS;
#endif
	return -1;
}

int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path)
{
#ifdef ENOTSUP
	errno = ENOTSUP;
#else
	errno = ENOSYS;
#endif
	return -1;
}

#endif

/************************************************************************
 Deliberately outside the ACL defines. Return 1 if this is a "no acls"
 errno, 0 if not.
************************************************************************/

int no_acl_syscall_error(int err)
{
#if defined(ENOSYS)
	if (err == ENOSYS) {
		return 1;
	}
#endif
#if defined(ENOTSUP)
	if (err == ENOTSUP) {
		return 1;
	}
#endif
	return 0;
}
