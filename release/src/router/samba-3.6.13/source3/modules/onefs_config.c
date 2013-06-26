/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS
 *
 * Copyright (C) Todd Stecher, 2009
 * Copyright (C) Tim Prouty, 2009
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
#include "onefs_config.h"

#include <ifs/ifs_syscalls.h>

#define ONEFS_DATA_FASTBUF	10

struct onefs_vfs_share_config vfs_share_config[ONEFS_DATA_FASTBUF];
struct onefs_vfs_share_config *pvfs_share_config;

static void onefs_load_faketimestamp_config(struct connection_struct *conn,
					    struct onefs_vfs_share_config *cfg)
{
	const char **parm;
	int snum = SNUM(conn);

	parm = lp_parm_string_list(snum, PARM_ONEFS_TYPE, PARM_ATIME_NOW,
				   PARM_ATIME_NOW_DEFAULT);

	if (parm) {
		cfg->init_flags |= ONEFS_VFS_CONFIG_FAKETIMESTAMPS;
		set_namearray(&cfg->atime_now_list,*parm);
	}

	parm = lp_parm_string_list(snum, PARM_ONEFS_TYPE, PARM_CTIME_NOW,
				   PARM_CTIME_NOW_DEFAULT);

	if (parm) {
		cfg->init_flags |= ONEFS_VFS_CONFIG_FAKETIMESTAMPS;
		set_namearray(&cfg->ctime_now_list,*parm);
	}

	parm = lp_parm_string_list(snum, PARM_ONEFS_TYPE, PARM_MTIME_NOW,
				   PARM_MTIME_NOW_DEFAULT);

	if (parm) {
		cfg->init_flags |= ONEFS_VFS_CONFIG_FAKETIMESTAMPS;
		set_namearray(&cfg->mtime_now_list,*parm);
	}

	parm = lp_parm_string_list(snum, PARM_ONEFS_TYPE, PARM_ATIME_STATIC,
				   PARM_ATIME_STATIC_DEFAULT);

	if (parm) {
		cfg->init_flags |= ONEFS_VFS_CONFIG_FAKETIMESTAMPS;
		set_namearray(&cfg->atime_static_list,*parm);
	}

	parm = lp_parm_string_list(snum, PARM_ONEFS_TYPE, PARM_MTIME_STATIC,
				   PARM_MTIME_STATIC_DEFAULT);

	if (parm) {
		cfg->init_flags |= ONEFS_VFS_CONFIG_FAKETIMESTAMPS;
		set_namearray(&cfg->mtime_static_list,*parm);
	}

	cfg->atime_slop = lp_parm_int(snum, PARM_ONEFS_TYPE, PARM_ATIME_SLOP,
				      PARM_ATIME_SLOP_DEFAULT);
	cfg->ctime_slop = lp_parm_int(snum, PARM_ONEFS_TYPE, PARM_CTIME_SLOP,
				      PARM_CTIME_SLOP_DEFAULT);
	cfg->mtime_slop = lp_parm_int(snum, PARM_ONEFS_TYPE, PARM_MTIME_SLOP,
				      PARM_MTIME_SLOP_DEFAULT);
}

/**
 * Set onefs-specific vfs global config parameters.
 *
 * Since changes in these parameters require calling syscalls, we only want to
 * call them when the configuration actually changes.
 */
static void onefs_load_global_config(connection_struct *conn)
{
	static struct onefs_vfs_global_config global_config;
	bool dot_snap_child_accessible;
	bool dot_snap_child_visible;
	bool dot_snap_root_accessible;
	bool dot_snap_root_visible;
	bool dot_snap_tilde;
	bool reconfig_dso = false;
	bool reconfig_tilde = false;

	/* Check if this is the first time setting the config options. */
	if (!(global_config.init_flags & ONEFS_VFS_CONFIG_INITIALIZED)) {
		global_config.init_flags |= ONEFS_VFS_CONFIG_INITIALIZED;

		/* Set process encoding */
		onefs_sys_config_enc();

		reconfig_dso = true;
		reconfig_tilde = true;
	}

	/* Get the dot snap options from the conf. */
	dot_snap_child_accessible =
	    lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_DOT_SNAP_CHILD_ACCESSIBLE,
			 PARM_DOT_SNAP_CHILD_ACCESSIBLE_DEFAULT);
	dot_snap_child_visible =
	    lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_DOT_SNAP_CHILD_VISIBLE,
			 PARM_DOT_SNAP_CHILD_VISIBLE_DEFAULT);
	dot_snap_root_accessible =
	    lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_DOT_SNAP_ROOT_ACCESSIBLE,
			 PARM_DOT_SNAP_ROOT_ACCESSIBLE_DEFAULT);
	dot_snap_root_visible =
	    lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_DOT_SNAP_ROOT_VISIBLE,
			 PARM_DOT_SNAP_ROOT_VISIBLE_DEFAULT);
	dot_snap_tilde =
	    lp_parm_bool(SNUM(conn), PARM_ONEFS_TYPE,
			 PARM_DOT_SNAP_TILDE,
			 PARM_DOT_SNAP_TILDE_DEFAULT);

	/* Check if any of the dot snap options need updating. */
	if (dot_snap_child_accessible !=
	    global_config.dot_snap_child_accessible) {
		global_config.dot_snap_child_accessible =
		    dot_snap_child_accessible;
		reconfig_dso = true;
	}
	if (dot_snap_child_visible !=
	    global_config.dot_snap_child_visible) {
		global_config.dot_snap_child_visible =
		    dot_snap_child_visible;
		reconfig_dso = true;
	}
	if (dot_snap_root_accessible !=
	    global_config.dot_snap_root_accessible) {
		global_config.dot_snap_root_accessible =
		    dot_snap_root_accessible;
		reconfig_dso = true;
	}
	if (dot_snap_root_visible !=
	    global_config.dot_snap_root_visible) {
		global_config.dot_snap_root_visible =
		    dot_snap_root_visible;
		reconfig_dso = true;
	}
	if (dot_snap_tilde != global_config.dot_snap_tilde) {
		global_config.dot_snap_tilde = dot_snap_tilde;
		reconfig_tilde = true;
	}

	/* If a dot snap option has changed update the process.  */
	if (reconfig_dso) {
		onefs_sys_config_snap_opt(&global_config);
	}

	/* If the dot snap tilde option has changed update the process.  */
	if (reconfig_tilde) {
		onefs_sys_config_tilde(&global_config);
	}
}

int onefs_load_config(connection_struct *conn)
{
	int snum = SNUM(conn);
	int share_count = lp_numservices();

	/* Share config */
	if (!pvfs_share_config) {

		if (share_count <= ONEFS_DATA_FASTBUF)
			pvfs_share_config = vfs_share_config;
		else {
			pvfs_share_config =
			    SMB_MALLOC_ARRAY(struct onefs_vfs_share_config,
					     share_count);
			if (!pvfs_share_config) {
				errno = ENOMEM;
				return -1;
			}

			memset(pvfs_share_config, 0,
			    (sizeof(struct onefs_vfs_share_config) *
				    share_count));
		}
	}

	if ((pvfs_share_config[snum].init_flags &
		ONEFS_VFS_CONFIG_INITIALIZED) == 0) {
			pvfs_share_config[snum].init_flags =
			    ONEFS_VFS_CONFIG_INITIALIZED;
			onefs_load_faketimestamp_config(conn,
						        &pvfs_share_config[snum]);
	}

	/* Global config */
	onefs_load_global_config(conn);

	return 0;
}

bool onefs_get_config(int snum, int config_type,
		      struct onefs_vfs_share_config *cfg)
{
	if ((pvfs_share_config != NULL) &&
	    (pvfs_share_config[snum].init_flags & config_type))
		*cfg = pvfs_share_config[snum];
	else
		return false;

	return true;
}


/**
 * Set the per-process encoding, ignoring errors.
 */
void onefs_sys_config_enc(void)
{
	int ret;

	ret = enc_set_proc(ENC_UTF8);
	if (ret) {
		DEBUG(0, ("Setting process encoding failed: %s\n",
			strerror(errno)));
	}
}

/**
 * Set the per-process .snpashot directory options, ignoring errors.
 */
void onefs_sys_config_snap_opt(struct onefs_vfs_global_config *global_config)
{
	struct ifs_dotsnap_options dso;
	int ret;

	dso.per_proc = 1;
	dso.sub_accessible = global_config->dot_snap_child_accessible;
	dso.sub_visible = global_config->dot_snap_child_visible;
	dso.root_accessible = global_config->dot_snap_root_accessible;
	dso.root_visible = global_config->dot_snap_root_visible;

	ret = ifs_set_dotsnap_options(&dso);
	if (ret) {
		DEBUG(0, ("Setting snapshot visibility/accessibility "
			"failed: %s\n", strerror(errno)));
	}
}

/**
 * Set the per-process flag saying whether or not to accept ~snapshot
 * as an alternative name for .snapshot directories.
 */
void onefs_sys_config_tilde(struct onefs_vfs_global_config *global_config)
{
	int ret;

	ret = ifs_tilde_snapshot(global_config->dot_snap_tilde);
	if (ret) {
		DEBUG(0, ("Setting snapshot tilde failed: %s\n",
			strerror(errno)));
	}
}
