/* 
 *  Unix SMB/CIFS implementation.
 *  Provide a connection to GPFS specific features
 *  Copyright (C) Volker Lendecke 2005
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

#ifdef HAVE_GPFS

#include "gpfs_gpl.h"

static void *libgpfs_handle = NULL;

static int (*gpfs_set_share_fn)(int fd, unsigned int allow, unsigned int deny);
static int (*gpfs_set_lease_fn)(int fd, unsigned int leaseType);
static int (*gpfs_getacl_fn)(char *pathname, int flags, void *acl);
static int (*gpfs_putacl_fn)(char *pathname, int flags, void *acl);


BOOL set_gpfs_sharemode(files_struct *fsp, uint32 access_mask,
			uint32 share_access)
{
	unsigned int allow = GPFS_SHARE_NONE;
	unsigned int deny = GPFS_DENY_NONE;
	int result;

	if (gpfs_set_share_fn == NULL) {
		return False;
	}

	if ((fsp == NULL) || (fsp->fh == NULL) || (fsp->fh->fd < 0)) {
		/* No real file, don't disturb */
		return True;
	}

	allow |= (access_mask & (FILE_WRITE_DATA|FILE_APPEND_DATA|
				 DELETE_ACCESS)) ? GPFS_SHARE_WRITE : 0;
	allow |= (access_mask & (FILE_READ_DATA|FILE_EXECUTE)) ?
		GPFS_SHARE_READ : 0;

	if (allow == GPFS_SHARE_NONE) {
		DEBUG(10, ("special case am=no_access:%x\n",access_mask));
	}
	else {	
		deny |= (share_access & FILE_SHARE_WRITE) ?
			0 : GPFS_DENY_WRITE;
		deny |= (share_access & (FILE_SHARE_READ)) ?
			0 : GPFS_DENY_READ;
	}
	DEBUG(10, ("am=%x, allow=%d, sa=%x, deny=%d\n",
		   access_mask, allow, share_access, deny));

	result = gpfs_set_share_fn(fsp->fh->fd, allow, deny);
	if (result != 0) {
		if (errno == ENOSYS) {
			DEBUG(5, ("VFS module vfs_gpfs loaded, but no gpfs "
				  "support has been compiled into Samba. Allowing access\n"));
			return True;
		} else {
			DEBUG(10, ("gpfs_set_share failed: %s\n",
				   strerror(errno)));
		}
	}

	return (result == 0);
}

int set_gpfs_lease(int fd, int leasetype)
{
	int gpfs_type = GPFS_LEASE_NONE;

	if (gpfs_set_lease_fn == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (leasetype == F_RDLCK) {
		gpfs_type = GPFS_LEASE_READ;
	}
	if (leasetype == F_WRLCK) {
		gpfs_type = GPFS_LEASE_WRITE;
	}
	return gpfs_set_lease_fn(fd, gpfs_type);
}

int smbd_gpfs_getacl(char *pathname, int flags, void *acl)
{
	if (gpfs_getacl_fn == NULL) {
		errno = ENOSYS;
		return -1;
	}

	return gpfs_getacl_fn(pathname, flags, acl);
}

int smbd_gpfs_putacl(char *pathname, int flags, void *acl)
{
	if (gpfs_putacl_fn == NULL) {
		errno = ENOSYS;
		return -1;
	}

	return gpfs_putacl_fn(pathname, flags, acl);
}

void init_gpfs(void)
{
	if (libgpfs_handle != NULL) {
		return;
	}

	libgpfs_handle = sys_dlopen("libgpfs_gpl.so", RTLD_LAZY);

	if (libgpfs_handle == NULL) {
		DEBUG(10, ("sys_dlopen for libgpfs_gpl failed: %s\n",
			   strerror(errno)));
		return;
	}

	DEBUG(10, ("libgpfs_gpl.so loaded\n"));

	gpfs_set_share_fn = sys_dlsym(libgpfs_handle, "gpfs_set_share");
	if (gpfs_set_share_fn == NULL) {
		DEBUG(3, ("libgpfs_gpl.so does not contain the symbol "
			  "'gpfs_set_share'\n"));
		sys_dlclose(libgpfs_handle);

		/* leave libgpfs_handle != NULL around, no point
		   in trying twice */
		gpfs_set_share_fn = NULL;
		gpfs_set_lease_fn = NULL;
		gpfs_getacl_fn = NULL;
		gpfs_putacl_fn = NULL;
		return;
	}

	gpfs_set_lease_fn = sys_dlsym(libgpfs_handle, "gpfs_set_lease");
	if (gpfs_set_lease_fn == NULL) {
		DEBUG(3, ("libgpfs_gpl.so does not contain the symbol "
			  "'gpfs_set_lease'\n"));
		sys_dlclose(libgpfs_handle);

		/* leave libgpfs_handle != NULL around, no point
		   in trying twice */
		gpfs_set_share_fn = NULL;
		gpfs_set_lease_fn = NULL;
		gpfs_getacl_fn = NULL;
		gpfs_putacl_fn = NULL;
		return;
	}

	gpfs_getacl_fn = sys_dlsym(libgpfs_handle, "gpfs_getacl");
	if (gpfs_getacl_fn == NULL) {
		DEBUG(3, ("libgpfs_gpl.so does not contain the symbol "
			  "'gpfs_getacl'\n"));
		sys_dlclose(libgpfs_handle);

		/* leave libgpfs_handle != NULL around, no point
		   in trying twice */
		gpfs_set_share_fn = NULL;
		gpfs_set_lease_fn = NULL;
		gpfs_getacl_fn = NULL;
		gpfs_putacl_fn = NULL;
		return;
	}

	gpfs_putacl_fn = sys_dlsym(libgpfs_handle, "gpfs_putacl");
	if (gpfs_putacl_fn == NULL) {
		DEBUG(3, ("libgpfs_gpl.so does not contain the symbol "
			  "'gpfs_putacl'\n"));
		sys_dlclose(libgpfs_handle);

		/* leave libgpfs_handle != NULL around, no point
		   in trying twice */
		gpfs_set_share_fn = NULL;
		gpfs_set_lease_fn = NULL;
		gpfs_getacl_fn = NULL;
		gpfs_putacl_fn = NULL;
		return;
	}

}

#else

int set_gpfs_lease(int snum, int leasetype)
{
	DEBUG(0, ("'VFS module smbgpfs loaded, without gpfs support compiled\n"));

	/* We need to indicate that no GPFS is around by returning ENOSYS, so
	 * that the normal linux kernel oplock code is called. */
	errno = ENOSYS;
	return -1;
}

BOOL set_gpfs_sharemode(files_struct *fsp, uint32 access_mask,
			uint32 share_access)
{
	DEBUG(0, ("VFS module - smbgpfs.so loaded, without gpfs support compiled\n"));
	/* Don't disturb but complain */
	return True;
}

int smbd_gpfs_getacl(char *pathname, int flags, void *acl)
{
	errno = ENOSYS;
	return -1;
}

int smbd_gpfs_putacl(char *pathname, int flags, void *acl)
{
	errno = ENOSYS;
	return -1;
}

void init_gpfs(void)
{
	return;
}

#endif /* HAVE_GPFS */
