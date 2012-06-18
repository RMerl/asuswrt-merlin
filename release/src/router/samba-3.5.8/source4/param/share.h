/* 
   Unix SMB/CIFS implementation.
   
   Modular services configuration
   
   Copyright (C) Simo Sorce	2006
   
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

#ifndef _SHARE_H
#define _SHARE_H

struct share_ops;

struct share_context {
	const struct share_ops *ops;
	void *priv_data;
};

struct share_config {
	const char *name;
	struct share_context *ctx;
	void *opaque;
};

enum share_info_type {
	SHARE_INFO_STRING,
	SHARE_INFO_INT,
	SHARE_INFO_BLOB
};

struct share_info {
	enum share_info_type type;
	const char *name;
	void *value;
};

struct tevent_context;

struct share_ops {
	const char *name;
	NTSTATUS (*init)(TALLOC_CTX *, const struct share_ops*, struct tevent_context *ev_ctx,
			 struct loadparm_context *lp_ctx,
			 struct share_context **);
	const char *(*string_option)(struct share_config *, const char *, const char *);
	int (*int_option)(struct share_config *, const char *, int);
	bool (*bool_option)(struct share_config *, const char *, bool);
	const char **(*string_list_option)(TALLOC_CTX *, struct share_config *, const char *);
	NTSTATUS (*list_all)(TALLOC_CTX *, struct share_context *, int *, const char ***);
	NTSTATUS (*get_config)(TALLOC_CTX *, struct share_context *, const char *, struct share_config **);
	NTSTATUS (*create)(struct share_context *, const char *, struct share_info *, int);
	NTSTATUS (*set)(struct share_context *, const char *, struct share_info *, int);
	NTSTATUS (*remove)(struct share_context *, const char *);
};

struct loadparm_context;

#include "param/share_proto.h"

/* list of shares options */

#define SHARE_NAME		"name"
#define SHARE_PATH		"path"
#define SHARE_COMMENT		"comment"
#define SHARE_PASSWORD		"password"
#define SHARE_HOSTS_ALLOW	"hosts-allow"
#define SHARE_HOSTS_DENY	"hosts-deny"
#define SHARE_NTVFS_HANDLER	"ntvfs-handler"
#define SHARE_TYPE		"type"
#define SHARE_VOLUME		"volume"
#define SHARE_CSC_POLICY	"csc-policy"
#define SHARE_AVAILABLE		"available"
#define SHARE_BROWSEABLE	"browseable"
#define SHARE_MAX_CONNECTIONS	"max-connections"

/* I'd like to see the following options go away
 * and always use EAs and SECDESCs */
#define SHARE_READONLY		"readonly"
#define SHARE_MAP_SYSTEM	"map-system"
#define SHARE_MAP_HIDDEN	"map-hidden"
#define SHARE_MAP_ARCHIVE	"map-archive"

#define SHARE_STRICT_LOCKING	"strict-locking"
#define SHARE_OPLOCKS	        "oplocks"
#define SHARE_STRICT_SYNC	"strict-sync"
#define SHARE_MSDFS_ROOT	"msdfs-root"
#define SHARE_CI_FILESYSTEM	"ci-filesystem"

#define SHARE_DIR_MASK             "directory mask"
#define SHARE_CREATE_MASK          "create mask"
#define SHARE_FORCE_CREATE_MODE    "force create mode"
#define SHARE_FORCE_DIR_MODE       "force directory mode"

/* defaults */

#define SHARE_HOST_ALLOW_DEFAULT	NULL
#define SHARE_HOST_DENY_DEFAULT		NULL
#define SHARE_VOLUME_DEFAULT		NULL
#define SHARE_TYPE_DEFAULT		"DISK"	
#define SHARE_CSC_POLICY_DEFAULT	0
#define SHARE_AVAILABLE_DEFAULT		true
#define SHARE_BROWSEABLE_DEFAULT	true
#define SHARE_MAX_CONNECTIONS_DEFAULT	0

#define SHARE_DIR_MASK_DEFAULT                   0755
#define SHARE_CREATE_MASK_DEFAULT                0744
#define SHARE_FORCE_CREATE_MODE_DEFAULT          0000
#define SHARE_FORCE_DIR_MODE_DEFAULT             0000



/* I'd like to see the following options go away
 * and always use EAs and SECDESCs */
#define SHARE_READONLY_DEFAULT		true
#define SHARE_MAP_SYSTEM_DEFAULT	false
#define SHARE_MAP_HIDDEN_DEFAULT	false
#define SHARE_MAP_ARCHIVE_DEFAULT	true

#define SHARE_STRICT_LOCKING_DEFAULT	true
#define SHARE_OPLOCKS_DEFAULT	true
#define SHARE_STRICT_SYNC_DEFAULT	false
#define SHARE_MSDFS_ROOT_DEFAULT	false
#define SHARE_CI_FILESYSTEM_DEFAULT	false

#endif /* _SHARE_H */
