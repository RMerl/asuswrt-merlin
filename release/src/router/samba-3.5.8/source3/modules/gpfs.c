/* 
 *  Unix SMB/CIFS implementation.
 *  Provide a connection to GPFS specific features
 *  Copyright (C) Volker Lendecke 2005
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

#ifdef HAVE_GPFS

#include "gpfs_gpl.h"
#include "vfs_gpfs.h"

static bool gpfs_share_modes;
static bool gpfs_leases;
static bool gpfs_getrealfilename;
static bool gpfs_winattr;

static int (*gpfs_set_share_fn)(int fd, unsigned int allow, unsigned int deny);
static int (*gpfs_set_lease_fn)(int fd, unsigned int leaseType);
static int (*gpfs_getacl_fn)(char *pathname, int flags, void *acl);
static int (*gpfs_putacl_fn)(char *pathname, int flags, void *acl);
static int (*gpfs_get_realfilename_path_fn)(char *pathname, char *filenamep,
					    int *buflen);
static int (*gpfs_set_winattrs_path_fn)(char *pathname, int flags, struct gpfs_winattr *attrs);
static int (*gpfs_get_winattrs_path_fn)(char *pathname, struct gpfs_winattr *attrs);
static int (*gpfs_get_winattrs_fn)(int fd, struct gpfs_winattr *attrs);


bool set_gpfs_sharemode(files_struct *fsp, uint32 access_mask,
			uint32 share_access)
{
	unsigned int allow = GPFS_SHARE_NONE;
	unsigned int deny = GPFS_DENY_NONE;
	int result;

	if (!gpfs_share_modes) {
		return True;
	}

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

	if (!gpfs_leases) {
		return True;
	}

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

	/* we unconditionally set CAP_LEASE, rather than looking for
	   -1/EACCES as there is a bug in some versions of
	   libgpfs_gpl.so which results in a leaked fd on /dev/ss0
	   each time we try this with the wrong capabilities set
	*/
	linux_set_lease_capability();
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

int smbd_gpfs_get_realfilename_path(char *pathname, char *filenamep,
				    int *buflen)
{
	if ((!gpfs_getrealfilename)
	    || (gpfs_get_realfilename_path_fn == NULL)) {
		errno = ENOSYS;
		return -1;
	}

	return gpfs_get_realfilename_path_fn(pathname, filenamep, buflen);
}

int get_gpfs_winattrs(char *pathname,struct gpfs_winattr *attrs)
{

        if ((!gpfs_winattr) || (gpfs_get_winattrs_path_fn == NULL)) {
                errno = ENOSYS;
                return -1;
        }
        DEBUG(10, ("gpfs_get_winattrs_path:open call %s\n",pathname));
        return gpfs_get_winattrs_path_fn(pathname, attrs);
}

int smbd_fget_gpfs_winattrs(int fd, struct gpfs_winattr *attrs)
{

        if ((!gpfs_winattr) || (gpfs_get_winattrs_fn == NULL)) {
                errno = ENOSYS;
                return -1;
        }
        DEBUG(10, ("gpfs_get_winattrs_path:open call %d\n", fd));
        return gpfs_get_winattrs_fn(fd, attrs);
}

int set_gpfs_winattrs(char *pathname,int flags,struct gpfs_winattr *attrs)
{
        if ((!gpfs_winattr) || (gpfs_set_winattrs_path_fn == NULL)) {
                errno = ENOSYS;
                return -1;
        }

        DEBUG(10, ("gpfs_set_winattrs_path:open call %s\n",pathname));
        return gpfs_set_winattrs_path_fn(pathname,flags, attrs);
}

static bool init_gpfs_function_lib(void *plibhandle_pointer,
				   const char *libname,
				   void *pfn_pointer, const char *fn_name)
{
	bool did_open_here = false;
	void **libhandle_pointer = (void **)plibhandle_pointer;
	void **fn_pointer = (void **)pfn_pointer;

	DEBUG(10, ("trying to load name %s from %s\n",
		   fn_name, libname));

	if (*libhandle_pointer == NULL) {
		*libhandle_pointer = dlopen(libname, RTLD_LAZY);
		did_open_here = true;
	}
	if (*libhandle_pointer == NULL) {
		DEBUG(10, ("Could not open lib %s\n", libname));
		return false;
	}

	*fn_pointer = dlsym(*libhandle_pointer, fn_name);
	if (*fn_pointer == NULL) {
		DEBUG(10, ("Did not find symbol %s in lib %s\n",
			   fn_name, libname));
		if (did_open_here) {
			dlclose(*libhandle_pointer);
			*libhandle_pointer = NULL;
		}
		return false;
	}

	return true;
}

static bool init_gpfs_function(void *fn_pointer, const char *fn_name)
{
	static void *libgpfs_handle = NULL;
	static void *libgpfs_gpl_handle = NULL;

	if (init_gpfs_function_lib(&libgpfs_handle, "libgpfs.so",
				   fn_pointer, fn_name)) {
		return true;
	}
	if (init_gpfs_function_lib(&libgpfs_gpl_handle, "libgpfs_gpl.so",
				   fn_pointer, fn_name)) {
		return true;
	}
	return false;
}

void init_gpfs(void)
{
	init_gpfs_function(&gpfs_set_share_fn, "gpfs_set_share");
	init_gpfs_function(&gpfs_set_lease_fn, "gpfs_set_lease");
	init_gpfs_function(&gpfs_getacl_fn, "gpfs_getacl");
	init_gpfs_function(&gpfs_putacl_fn, "gpfs_putacl");
	init_gpfs_function(&gpfs_get_realfilename_path_fn,
			   "gpfs_get_realfilename_path");
	init_gpfs_function(&gpfs_get_winattrs_path_fn,"gpfs_get_winattrs_path");
        init_gpfs_function(&gpfs_set_winattrs_path_fn,"gpfs_set_winattrs_path");
        init_gpfs_function(&gpfs_get_winattrs_fn,"gpfs_get_winattrs");


	gpfs_share_modes = lp_parm_bool(-1, "gpfs", "sharemodes", True);
	gpfs_leases      = lp_parm_bool(-1, "gpfs", "leases", True);
	gpfs_getrealfilename = lp_parm_bool(-1, "gpfs", "getrealfilename",
					    True);
	gpfs_winattr = lp_parm_bool(-1, "gpfs", "winattr", False);

	return;
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

bool set_gpfs_sharemode(files_struct *fsp, uint32 access_mask,
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

int smbd_gpfs_get_realfilename_path(char *pathname, char *fileamep,
				    int *buflen)
{
	errno = ENOSYS;
	return -1;
}

int set_gpfs_winattrs(char *pathname,int flags,struct gpfs_winattr *attrs)
{
        errno = ENOSYS;
        return -1;
}

int get_gpfs_winattrs(char *pathname,struct gpfs_winattr *attrs)
{
        errno = ENOSYS;
        return -1;
}

void init_gpfs(void)
{
	return;
}

#endif /* HAVE_GPFS */
