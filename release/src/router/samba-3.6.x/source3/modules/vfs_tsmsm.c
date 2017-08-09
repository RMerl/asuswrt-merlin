/*
  Unix SMB/CIFS implementation.
  Samba VFS module for handling offline files
  with Tivoli Storage Manager Space Management

  (c) Alexander Bokovoy, 2007, 2008
  (c) Andrew Tridgell, 2007, 2008
  
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
  This VFS module accepts following options:
  tsmsm: hsm script = <path to hsm script> (default does nothing)
         hsm script should point to a shell script which accepts two arguments:
	 <operation> <filepath>
	 where <operation> is currently 'offline' to set offline status of the <filepath>

  tsmsm: online ratio = ratio to check reported size against actual file size (0.5 by default)
  tsmsm: attribute name = name of DMAPI attribute that is present when a file is offline. 
  Default is "IBMobj" (which is what GPFS uses)

  The TSMSM VFS module tries to avoid calling expensive DMAPI calls with some heuristics
  based on the fact that number of blocks reported of a file multiplied by 512 will be
  bigger than 'online ratio' of actual size for online (non-migrated) files.

  If checks fail, we call DMAPI and ask for specific attribute which present for
  offline (migrated) files. If this attribute presents, we consider file offline.
 */

#include "includes.h"
#include "smbd/smbd.h"

#ifndef USE_DMAPI
#error "This module requires DMAPI support!"
#endif

#ifdef HAVE_XFS_DMAPI_H
#include <xfs/dmapi.h>
#elif defined(HAVE_SYS_DMI_H)
#include <sys/dmi.h>
#elif defined(HAVE_SYS_JFSDMAPI_H)
#include <sys/jfsdmapi.h>
#elif defined(HAVE_SYS_DMAPI_H)
#include <sys/dmapi.h>
#elif defined(HAVE_DMAPI_H)
#include <dmapi.h>
#endif

#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE 
#endif

#include <math.h> 

/* optimisation tunables - used to avoid the DMAPI slow path */
#define FILE_IS_ONLINE_RATIO      0.5

/* default attribute name to look for */
#define DM_ATTRIB_OBJECT "IBMObj"

struct tsmsm_struct {
	float online_ratio;
	char *hsmscript;
	const char *attrib_name;
	const char *attrib_value;
};

static void tsmsm_free_data(void **pptr) {
	struct tsmsm_struct **tsmd = (struct tsmsm_struct **)pptr;
	if(!tsmd) return;
	TALLOC_FREE(*tsmd);
}

/* 
   called when a client connects to a share
*/
static int tsmsm_connect(struct vfs_handle_struct *handle,
			 const char *service,
			 const char *user) {
	struct tsmsm_struct *tsmd;
	const char *fres;
	const char *tsmname;
        int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	tsmd = TALLOC_ZERO_P(handle, struct tsmsm_struct);
	if (!tsmd) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(0,("tsmsm_connect: out of memory!\n"));
		return -1;
	}

	if (!dmapi_have_session()) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(0,("tsmsm_connect: no DMAPI session for Samba is available!\n"));
		TALLOC_FREE(tsmd);
		return -1;
	}

	tsmname = (handle->param ? handle->param : "tsmsm");
	
	/* Get 'hsm script' and 'dmapi attribute' parameters to tsmd context */
	tsmd->hsmscript = lp_parm_talloc_string(SNUM(handle->conn), tsmname,
						"hsm script", NULL);
	talloc_steal(tsmd, tsmd->hsmscript);
	
	tsmd->attrib_name = lp_parm_talloc_string(SNUM(handle->conn), tsmname, 
						  "dmapi attribute", DM_ATTRIB_OBJECT);
	talloc_steal(tsmd, tsmd->attrib_name);
	
	tsmd->attrib_value = lp_parm_talloc_string(SNUM(handle->conn), "tsmsm", 
						   "dmapi value", NULL);
	talloc_steal(tsmd, tsmd->attrib_value);
	
	/* retrieve 'online ratio'. In case of error default to FILE_IS_ONLINE_RATIO */
	fres = lp_parm_const_string(SNUM(handle->conn), tsmname, 
				    "online ratio", NULL);
	if (fres == NULL) {
		tsmd->online_ratio = FILE_IS_ONLINE_RATIO;
	} else {
		tsmd->online_ratio = strtof(fres, NULL);
		if (tsmd->online_ratio > 1.0 ||
		    tsmd->online_ratio <= 0.0) {
			DEBUG(1, ("tsmsm_connect: invalid online ration %f - using %f.\n",
				  tsmd->online_ratio, (float)FILE_IS_ONLINE_RATIO));
		}
	}

        /* Store the private data. */
        SMB_VFS_HANDLE_SET_DATA(handle, tsmd, tsmsm_free_data,
                                struct tsmsm_struct, return -1);
        return 0;
}

static bool tsmsm_is_offline(struct vfs_handle_struct *handle, 
			     const struct smb_filename *fname,
			     SMB_STRUCT_STAT *stbuf)
{
	struct tsmsm_struct *tsmd = (struct tsmsm_struct *) handle->data;
	const dm_sessid_t *dmsession_id;
	void *dmhandle = NULL;
	size_t dmhandle_len = 0;
	size_t rlen;
	dm_attrname_t dmname;
	int ret, lerrno;
	bool offline;
	char *buf = NULL;
	size_t buflen;
	NTSTATUS status;
	char *path;

        status = get_full_smb_filename(talloc_tos(), fname, &path);
        if (!NT_STATUS_IS_OK(status)) {
                errno = map_errno_from_nt_status(status);
                return false;
        }

        /* if the file has more than FILE_IS_ONLINE_RATIO of blocks available,
	   then assume it is not offline (it may not be 100%, as it could be sparse) */
	if (512 * stbuf->st_ex_blocks >=
	    stbuf->st_ex_size * tsmd->online_ratio) {
		DEBUG(10,("%s not offline: st_blocks=%llu st_size=%llu "
			  "online_ratio=%.2f\n", path,
			  (unsigned long long)stbuf->st_ex_blocks,
			  (unsigned long long)stbuf->st_ex_size, tsmd->online_ratio));
		return false;
	}

	dmsession_id = dmapi_get_current_session();
	if (dmsession_id == NULL) {
		DEBUG(2, ("tsmsm_is_offline: no DMAPI session available? "
			  "Assume file is online.\n"));
		return false;
	}

        /* using POSIX capabilities does not work here. It's a slow path, so 
	 * become_root() is just as good anyway (tridge) 
	 */

	/* Also, AIX has DMAPI but no POSIX capablities support. In this case,
	 * we need to be root to do DMAPI manipulations.
	 */
	become_root();

	/* go the slow DMAPI route */
	if (dm_path_to_handle((char*)path, &dmhandle, &dmhandle_len) != 0) {
		DEBUG(2,("dm_path_to_handle failed - assuming offline (%s) - %s\n", 
			 path, strerror(errno)));
		offline = true;
		goto done;
	}

	memset(&dmname, 0, sizeof(dmname));
	strlcpy((char *)&dmname.an_chars[0], tsmd->attrib_name, sizeof(dmname.an_chars));

	if (tsmd->attrib_value != NULL) {
		buflen = strlen(tsmd->attrib_value);
	} else {
		buflen = 1;
	}
	buf = talloc_zero_size(tsmd, buflen);
	if (buf == NULL) {
		DEBUG(0,("out of memory in tsmsm_is_offline -- assuming online (%s)\n", path));
		errno = ENOMEM;
		offline = false;
		goto done;
	}

	do {
		lerrno = 0;

		ret = dm_get_dmattr(*dmsession_id, dmhandle, dmhandle_len, 
				    DM_NO_TOKEN, &dmname, buflen, buf, &rlen);
		if (ret == -1 && errno == EINVAL) {
			DEBUG(0, ("Stale DMAPI session, re-creating it.\n"));
			lerrno = EINVAL;
			if (dmapi_new_session()) {
				dmsession_id = dmapi_get_current_session();
			} else {
				DEBUG(0, 
				      ("Unable to re-create DMAPI session, assuming offline (%s) - %s\n", 
				       path, strerror(errno)));
				offline = true;
				dm_handle_free(dmhandle, dmhandle_len);
				goto done;
			}
		}
	} while (ret == -1 && lerrno == EINVAL);

	/* check if we need a specific attribute value */
	if (tsmd->attrib_value != NULL) {
		offline = (ret == 0 && rlen == buflen && 
			    memcmp(buf, tsmd->attrib_value, buflen) == 0);
	} else {
		/* its offline if the specified DMAPI attribute exists */
		offline = (ret == 0 || (ret == -1 && errno == E2BIG));
	}

	DEBUG(10,("dm_get_dmattr %s ret=%d (%s)\n", path, ret, strerror(errno)));

	ret = 0;

	dm_handle_free(dmhandle, dmhandle_len);	

done:
	talloc_free(buf);
	unbecome_root();
	return offline;
}


static bool tsmsm_aio_force(struct vfs_handle_struct *handle, struct files_struct *fsp)
{
	SMB_STRUCT_STAT sbuf;
	struct tsmsm_struct *tsmd = (struct tsmsm_struct *) handle->data;
	/* see if the file might be offline. This is called before each IO
	   to ensure we use AIO if the file is offline. We don't do the full dmapi
	   call as that would be too slow, instead we err on the side of using AIO
	   if the file might be offline
	*/
	if(SMB_VFS_FSTAT(fsp, &sbuf) == 0) {
		DEBUG(10,("tsmsm_aio_force st_blocks=%llu st_size=%llu "
			  "online_ratio=%.2f\n", (unsigned long long)sbuf.st_ex_blocks,
			  (unsigned long long)sbuf.st_ex_size, tsmd->online_ratio));
		return !(512 * sbuf.st_ex_blocks >=
			 sbuf.st_ex_size * tsmd->online_ratio);
	}
	return false;
}

static ssize_t tsmsm_aio_return(struct vfs_handle_struct *handle, struct files_struct *fsp, 
				SMB_STRUCT_AIOCB *aiocb)
{
	ssize_t result;

	result = SMB_VFS_NEXT_AIO_RETURN(handle, fsp, aiocb);
	if(result >= 0) {
		notify_fname(handle->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	return result;
}

static ssize_t tsmsm_sendfile(vfs_handle_struct *handle, int tofd, files_struct *fsp, const DATA_BLOB *hdr,
			      SMB_OFF_T offset, size_t n)
{
	bool file_offline = tsmsm_aio_force(handle, fsp);

	if (file_offline) {
		DEBUG(10,("tsmsm_sendfile on offline file - rejecting\n"));
		errno = ENOSYS;
		return -1;
	}
	    
	return SMB_VFS_NEXT_SENDFILE(handle, tofd, fsp, hdr, offset, n);
}

/* We do overload pread to allow notification when file becomes online after offline status */
/* We don't intercept SMB_VFS_READ here because all file I/O now goes through SMB_VFS_PREAD instead */
static ssize_t tsmsm_pread(struct vfs_handle_struct *handle, struct files_struct *fsp, 
			   void *data, size_t n, SMB_OFF_T offset) {
	ssize_t result;
	bool notify_online = tsmsm_aio_force(handle, fsp);

	result = SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);
	if((result != -1) && notify_online) {
	    /* We can't actually force AIO at this point (came here not from reply_read_and_X) 
	       what we can do is to send notification that file became online
	    */
		notify_fname(handle->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	return result;
}

static ssize_t tsmsm_pwrite(struct vfs_handle_struct *handle, struct files_struct *fsp, 
			    const void *data, size_t n, SMB_OFF_T offset) {
	ssize_t result;
	bool notify_online = tsmsm_aio_force(handle, fsp);

	result = SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);
	if((result != -1) && notify_online) {
	    /* We can't actually force AIO at this point (came here not from reply_read_and_X) 
	       what we can do is to send notification that file became online
	    */
		notify_fname(handle->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	return result;
}

static int tsmsm_set_offline(struct vfs_handle_struct *handle, 
                             const struct smb_filename *fname)
{
	struct tsmsm_struct *tsmd = (struct tsmsm_struct *) handle->data;
	int result = 0;
	char *command;
	NTSTATUS status;
	char *path;

	if (tsmd->hsmscript == NULL) {
		/* no script enabled */
		DEBUG(1, ("tsmsm_set_offline: No 'tsmsm:hsm script' configured\n"));
		return 0;
	}

        status = get_full_smb_filename(talloc_tos(), fname, &path);
        if (!NT_STATUS_IS_OK(status)) {
                errno = map_errno_from_nt_status(status);
                return false;
        }

	/* Now, call the script */
	command = talloc_asprintf(tsmd, "%s offline \"%s\"", tsmd->hsmscript, path);
	if(!command) {
		DEBUG(1, ("tsmsm_set_offline: can't allocate memory to run hsm script"));
		return -1;
	}
	DEBUG(10, ("tsmsm_set_offline: Running [%s]\n", command));
	if((result = smbrun(command, NULL)) != 0) {
		DEBUG(1,("tsmsm_set_offline: Running [%s] returned %d\n", command, result));
	}
	TALLOC_FREE(command);
	return result;
}

static uint32_t tsmsm_fs_capabilities(struct vfs_handle_struct *handle,
			enum timestamp_set_resolution *p_ts_res)
{
	return SMB_VFS_NEXT_FS_CAPABILITIES(handle, p_ts_res) | FILE_SUPPORTS_REMOTE_STORAGE | FILE_SUPPORTS_REPARSE_POINTS;
}

static struct vfs_fn_pointers tsmsm_fns = {
	.connect_fn = tsmsm_connect,
	.fs_capabilities = tsmsm_fs_capabilities,
	.aio_force = tsmsm_aio_force,
	.aio_return_fn = tsmsm_aio_return,
	.pread = tsmsm_pread,
	.pwrite = tsmsm_pwrite,
	.sendfile = tsmsm_sendfile,
	.is_offline = tsmsm_is_offline,
	.set_offline = tsmsm_set_offline,
};

NTSTATUS vfs_tsmsm_init(void);
NTSTATUS vfs_tsmsm_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"tsmsm", &tsmsm_fns);
}
