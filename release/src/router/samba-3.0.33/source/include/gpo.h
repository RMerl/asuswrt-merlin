/* 
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2005
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


enum GPO_LINK_TYPE {
	GP_LINK_UNKOWN	= 0,
	GP_LINK_MACHINE	= 1,
	GP_LINK_SITE	= 2,
	GP_LINK_DOMAIN	= 3,
	GP_LINK_OU	= 4
};

/* GPO_OPTIONS */
#define GPO_FLAG_DISABLE	0x00000001
#define GPO_FLAG_FORCE		0x00000002

/* GPO_LIST_FLAGS */
#define GPO_LIST_FLAG_MACHINE	0x00000001
#define GPO_LIST_FLAG_SITEONLY	0x00000002

#define GPO_VERSION_USER(x) (x >> 16)
#define GPO_VERSION_MACHINE(x) (x & 0xffff)

struct GROUP_POLICY_OBJECT {
	uint32 options;	/* GPFLAGS_* */ 
	uint32 version;
	const char *ds_path;
	const char *file_sys_path;
	const char *display_name;
	const char *name;
	const char *link;
	uint32 link_type; /* GPO_LINK_TYPE */
	const char *user_extensions;
	const char *machine_extensions;
	struct GROUP_POLICY_OBJECT *next, *prev;
};

/* the following is seen on the DS (see adssearch.pl for details) */

/* the type field in a 'gPLink', the same as GPO_FLAG ? */
#define GPO_LINK_OPT_NONE	0x00000000
#define GPO_LINK_OPT_DISABLED	0x00000001
#define GPO_LINK_OPT_ENFORCED	0x00000002

/* GPO_LINK_OPT_ENFORCED takes precedence over GPOPTIONS_BLOCK_INHERITANCE */

/* 'gPOptions', maybe a bitmask as well */
enum GPO_INHERIT {
	GPOPTIONS_INHERIT		= 0,
	GPOPTIONS_BLOCK_INHERITANCE	= 1
};

/* 'flags' in a 'groupPolicyContainer' object */
#define GPFLAGS_ALL_ENABLED			0x00000000
#define GPFLAGS_USER_SETTINGS_DISABLED		0x00000001
#define GPFLAGS_MACHINE_SETTINGS_DISABLED	0x00000002
#define GPFLAGS_ALL_DISABLED (GPFLAGS_USER_SETTINGS_DISABLED | \
			      GPFLAGS_MACHINE_SETTINGS_DISABLED)

struct GP_LINK {
	const char *gp_link;	/* raw link name */
	uint32 gp_opts;		/* inheritance options GPO_INHERIT */
	uint32 num_links;	/* number of links */
	char **link_names;	/* array of parsed link names */
	uint32 *link_opts;	/* array of parsed link opts GPO_LINK_OPT_* */
};

struct GP_EXT {
	const char *gp_extension;	/* raw extension name */
	uint32 num_exts;
	char **extensions;
	char **extensions_guid;
	char **snapins;
	char **snapins_guid;
};

#define GPO_CACHE_DIR "gpo_cache"
#define GPT_INI "GPT.INI"
