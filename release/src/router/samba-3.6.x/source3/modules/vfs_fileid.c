/*
 * VFS module to alter the algorithm to calculate
 * the struct file_id used as key for the share mode
 * and byte range locking db's.
 *
 * Copyright (C) 2007, Stefan Metzmacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "system/filesys.h"

static int vfs_fileid_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_fileid_debug_level

struct fileid_mount_entry {
	SMB_DEV_T device;
	const char *mnt_fsname;
	fsid_t fsid;
	uint64_t devid;
};

struct fileid_handle_data {
	uint64_t (*device_mapping_fn)(struct fileid_handle_data *data,
				      SMB_DEV_T dev);
	unsigned num_mount_entries;
	struct fileid_mount_entry *mount_entries;
};

/* load all the mount entries from the mtab */
static void fileid_load_mount_entries(struct fileid_handle_data *data)
{
	FILE *f;
	struct mntent *m;

	data->num_mount_entries = 0;
	TALLOC_FREE(data->mount_entries);

	f = setmntent("/etc/mtab", "r");
	if (!f) return;

	while ((m = getmntent(f))) {
		struct stat st;
		struct statfs sfs;
		struct fileid_mount_entry *cur;

		if (stat(m->mnt_dir, &st) != 0) continue;
		if (statfs(m->mnt_dir, &sfs) != 0) continue;

		if (strncmp(m->mnt_fsname, "/dev/", 5) == 0) {
			m->mnt_fsname += 5;
		}

		data->mount_entries = TALLOC_REALLOC_ARRAY(data,
							   data->mount_entries,
							   struct fileid_mount_entry,
							   data->num_mount_entries+1);
		if (data->mount_entries == NULL) {
			goto nomem;
		}

		cur = &data->mount_entries[data->num_mount_entries];
		cur->device	= st.st_dev;
		cur->mnt_fsname = talloc_strdup(data->mount_entries,
						m->mnt_fsname);
		if (!cur->mnt_fsname) goto nomem;
		cur->fsid	= sfs.f_fsid;
		cur->devid	= (uint64_t)-1;

		data->num_mount_entries++;
	}
	endmntent(f);
	return;
	
nomem:
	if (f) endmntent(f);

	data->num_mount_entries = 0;
	TALLOC_FREE(data->mount_entries);

	return;
}

/* find a mount entry given a dev_t */
static struct fileid_mount_entry *fileid_find_mount_entry(struct fileid_handle_data *data,
							  SMB_DEV_T dev)
{
	int i;

	if (data->num_mount_entries == 0) {
		fileid_load_mount_entries(data);
	}
	for (i=0;i<data->num_mount_entries;i++) {
		if (data->mount_entries[i].device == dev) {
			return &data->mount_entries[i];
		}
	}
	/* 2nd pass after reloading */
	fileid_load_mount_entries(data);
	for (i=0;i<data->num_mount_entries;i++) {
		if (data->mount_entries[i].device == dev) {
			return &data->mount_entries[i];
		}
	}	
	return NULL;
}


/* a 64 bit hash, based on the one in tdb */
static uint64_t fileid_uint64_hash(const uint8_t *s, size_t len)
{
	uint64_t value;	/* Used to compute the hash value.  */
	uint32_t i;	/* Used to cycle through random values. */

	/* Set the initial value from the key size. */
	for (value = 0x238F13AFLL * len, i=0; i < len; i++)
		value = (value + (s[i] << (i*5 % 24)));

	return (1103515243LL * value + 12345LL);
}

/* a device mapping using a fsname */
static uint64_t fileid_device_mapping_fsname(struct fileid_handle_data *data,
					     SMB_DEV_T dev)
{
	struct fileid_mount_entry *m;

	m = fileid_find_mount_entry(data, dev);
	if (!m) return dev;

	if (m->devid == (uint64_t)-1) {
		m->devid = fileid_uint64_hash((uint8_t *)m->mnt_fsname,
					      strlen(m->mnt_fsname));
	}

	return m->devid;
}

/* device mapping functions using a fsid */
static uint64_t fileid_device_mapping_fsid(struct fileid_handle_data *data,
					   SMB_DEV_T dev)
{
	struct fileid_mount_entry *m;

	m = fileid_find_mount_entry(data, dev);
	if (!m) return dev;

	if (m->devid == (uint64_t)-1) {
		if (sizeof(fsid_t) > sizeof(uint64_t)) {
			m->devid = fileid_uint64_hash((uint8_t *)&m->fsid,
						      sizeof(m->fsid));
		} else {
			union {
				uint64_t ret;
				fsid_t fsid;
			} u;
			ZERO_STRUCT(u);
			u.fsid = m->fsid;
			m->devid = u.ret;
		}
	}

	return m->devid;
}

static int fileid_connect(struct vfs_handle_struct *handle,
			  const char *service, const char *user)
{
	struct fileid_handle_data *data;
	const char *algorithm;
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	data = talloc_zero(handle->conn, struct fileid_handle_data);
	if (!data) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(0, ("talloc_zero() failed\n"));
		return -1;
	}

	/*
	 * "fileid:mapping" is only here as fallback for old setups
	 * "fileid:algorithm" is the option new setups should use
	 */
	algorithm = lp_parm_const_string(SNUM(handle->conn),
					 "fileid", "mapping",
					 "fsname");
	algorithm = lp_parm_const_string(SNUM(handle->conn),
					 "fileid", "algorithm",
					 algorithm);
	if (strcmp("fsname", algorithm) == 0) {
		data->device_mapping_fn	= fileid_device_mapping_fsname;
	} else if (strcmp("fsid", algorithm) == 0) {
		data->device_mapping_fn	= fileid_device_mapping_fsid;
	} else {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(0,("fileid_connect(): unknown algorithm[%s]\n", algorithm));
		return -1;
	}

	SMB_VFS_HANDLE_SET_DATA(handle, data, NULL,
				struct fileid_handle_data,
				return -1);

	DEBUG(10, ("fileid_connect(): connect to service[%s] with algorithm[%s]\n",
		service, algorithm));

	return 0;
}

static void fileid_disconnect(struct vfs_handle_struct *handle)
{
	DEBUG(10,("fileid_disconnect() connect to service[%s].\n",
		lp_servicename(SNUM(handle->conn))));

	SMB_VFS_NEXT_DISCONNECT(handle);
}

static struct file_id fileid_file_id_create(struct vfs_handle_struct *handle,
					    const SMB_STRUCT_STAT *sbuf)
{
	struct fileid_handle_data *data;
	struct file_id id;

	ZERO_STRUCT(id);

	SMB_VFS_HANDLE_GET_DATA(handle, data,
				struct fileid_handle_data,
				return id);

	id.devid	= data->device_mapping_fn(data, sbuf->st_ex_dev);
	id.inode	= sbuf->st_ex_ino;

	return id;
}

static struct vfs_fn_pointers vfs_fileid_fns = {
	.connect_fn = fileid_connect,
	.disconnect = fileid_disconnect,
	.file_id_create = fileid_file_id_create
};

NTSTATUS vfs_fileid_init(void);
NTSTATUS vfs_fileid_init(void)
{
	NTSTATUS ret;

	ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "fileid",
			       &vfs_fileid_fns);
	if (!NT_STATUS_IS_OK(ret)) {
		return ret;
	}

	vfs_fileid_debug_level = debug_add_class("fileid");
	if (vfs_fileid_debug_level == -1) {
		vfs_fileid_debug_level = DBGC_VFS;
		DEBUG(0, ("vfs_fileid: Couldn't register custom debugging class!\n"));
	} else {
		DEBUG(10, ("vfs_fileid: Debug class number of 'fileid': %d\n", vfs_fileid_debug_level));
	}

	return ret;
}
