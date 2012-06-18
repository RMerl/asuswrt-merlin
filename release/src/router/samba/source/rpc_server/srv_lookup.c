
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
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

/*
 *
 *

 this module provides nt user / nt rid lookup functions.
 users, local groups, domain groups.

 no unix / samba functions should be called in this module:
 it should purely provide a gateway to the password database API,
 the local group database API or the domain group database API,
 but first checking built-in rids.

 did i say rids?  oops, what about "S-1-1" the "Everyone" group
 and other such well-known sids...

 speed is not of the essence: no particular optimisation is in place.

 *
 *
 */

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

extern fstring global_sam_name;
extern DOM_SID global_sam_sid;
extern DOM_SID global_sid_S_1_5_20;

/*
 * A list of the rids of well known BUILTIN and Domain users
 * and groups.
 */

rid_name builtin_alias_rids[] =
{  
    { BUILTIN_ALIAS_RID_ADMINS       , "Administrators" },
    { BUILTIN_ALIAS_RID_USERS        , "Users" },
    { BUILTIN_ALIAS_RID_GUESTS       , "Guests" },
    { BUILTIN_ALIAS_RID_POWER_USERS  , "Power Users" },
   
    { BUILTIN_ALIAS_RID_ACCOUNT_OPS  , "Account Operators" },
    { BUILTIN_ALIAS_RID_SYSTEM_OPS   , "System Operators" },
    { BUILTIN_ALIAS_RID_PRINT_OPS    , "Print Operators" },
    { BUILTIN_ALIAS_RID_BACKUP_OPS   , "Backup Operators" },
    { BUILTIN_ALIAS_RID_REPLICATOR   , "Replicator" },
    { 0                             , NULL }
};

/* array lookup of well-known Domain RID users. */
rid_name domain_user_rids[] =
{  
    { DOMAIN_USER_RID_ADMIN         , "Administrator" },
    { DOMAIN_USER_RID_GUEST         , "Guest" },
    { 0                             , NULL }
};

/* array lookup of well-known Domain RID groups. */
rid_name domain_group_rids[] =
{  
    { DOMAIN_GROUP_RID_ADMINS       , "Domain Admins" },
    { DOMAIN_GROUP_RID_USERS        , "Domain Users" },
    { DOMAIN_GROUP_RID_GUESTS       , "Domain Guests" },
    { 0                             , NULL }
};


int make_dom_gids(DOMAIN_GRP *mem, int num_members, DOM_GID **ppgids)
{
	int count;
	int i;
	DOM_GID *gids = NULL;

	*ppgids = NULL;

	DEBUG(4,("make_dom_gids: %d\n", num_members));

	if (mem == NULL || num_members == 0)
	{
		return 0;
	}

	for (i = 0, count = 0; i < num_members && count < LSA_MAX_GROUPS; i++) 
	{
		uint32 status;

		uint32 rid;
		uint8  type;

		uint8  attr  = mem[count].attr;
		char   *name = mem[count].name;

		become_root(True);
		status = lookup_grp_rid(name, &rid, &type);
		unbecome_root(True);

		if (status == 0x0)
		{
			gids = (DOM_GID *)Realloc( gids, sizeof(DOM_GID) * (count+1) );

			if (gids == NULL)
			{
				DEBUG(0,("make_dom_gids: Realloc fail !\n"));
				return 0;
			}

			gids[count].g_rid = rid;
			gids[count].attr  = attr;

			DEBUG(5,("group name: %s rid: %d attr: %d\n",
			          name, rid, attr));
			count++;
		}
		else
		{
			DEBUG(1,("make_dom_gids: unknown group name %s\n", name));
		}
	}

	*ppgids = gids;
	return count;
}

/*******************************************************************
 gets a domain user's groups
 ********************************************************************/
int get_domain_user_groups(DOMAIN_GRP_MEMBER **grp_members, uint32 group_rid)
{
	DOMAIN_GRP *grp;
	int num_mem;

	if (grp_members == NULL) return 0;

	grp = getgrouprid(group_rid, grp_members, &num_mem);

	if (grp == NULL)
	{
		return 0;
	}

	return num_mem;
}


/*******************************************************************
 lookup_builtin_names
 ********************************************************************/
uint32 lookup_builtin_names(uint32 rid, char *name, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_wk_user_name (rid, name, type) : status;
	status = (status != 0x0) ? lookup_wk_group_name(rid, name, type) : status;
	status = (status != 0x0) ? lookup_wk_alias_name(rid, name, type) : status;

	return status;
}


/*******************************************************************
 lookup_added_name - names that have been added to the SAM database by admins.
 ********************************************************************/
uint32 lookup_added_name(uint32 rid, char *name, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_user_name (rid, name, type) : status;
	status = (status != 0x0) ? lookup_group_name(rid, name, type) : status;
	status = (status != 0x0) ? lookup_alias_name(rid, name, type) : status;

	return status;
}


/*******************************************************************
 lookup_name
 ********************************************************************/
uint32 lookup_name(uint32 rid, char *name, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_builtin_names(rid, name, type) : status;
	status = (status != 0x0) ? lookup_added_name   (rid, name, type) : status;

	return status;
}


/*******************************************************************
 lookup_wk_group_name
 ********************************************************************/
uint32 lookup_wk_group_name(uint32 rid, char *group_name, uint8 *type)
{
	int i = 0; 
	(*type) = SID_NAME_WKN_GRP;

	DEBUG(5,("lookup_wk_group_name: rid: %d", rid));

	while (domain_group_rids[i].rid != rid && domain_group_rids[i].rid != 0)
	{
		i++;
	}

	if (domain_group_rids[i].rid != 0)
	{
		fstrcpy(group_name, domain_group_rids[i].name);
		DEBUG(5,(" = %s\n", group_name));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_group_name
 ********************************************************************/
uint32 lookup_group_name(uint32 rid, char *group_name, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;
	DOM_SID sid;

	DEBUG(5,("lookup_group_name: rid: 0x%x", rid));

	sid_copy      (&sid, &global_sam_sid);
	sid_append_rid(&sid, rid);

	(*type) = SID_NAME_DOM_GRP;

	if (map_group_sid_to_name(&sid, group_name, NULL))
	{
		status = 0x0;
	}

	if (status == 0x0)
	{
		DEBUG(5,(" = %s\n", group_name));
	}
	else
	{
		DEBUG(5,(" none mapped\n"));
	}

	return status;
}

/*******************************************************************
 lookup_wk_alias_name
 ********************************************************************/
uint32 lookup_wk_alias_name(uint32 rid, char *alias_name, uint8 *type)
{
	int i = 0; 
	(*type) = SID_NAME_ALIAS;

	DEBUG(5,("lookup_wk_alias_name: rid: %d", rid));

	while (builtin_alias_rids[i].rid != rid && builtin_alias_rids[i].rid != 0)
	{
		i++;
	}

	if (builtin_alias_rids[i].rid != 0)
	{
		fstrcpy(alias_name, builtin_alias_rids[i].name);
		DEBUG(5,(" = %s\n", alias_name));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_alias_name
 ********************************************************************/
uint32 lookup_alias_name(uint32 rid, char *alias_name, uint8 *type)
{
	(*type) = SID_NAME_ALIAS;

	DEBUG(2,("lookup_alias_name: rid: %d\n", rid));
	DEBUG(2,(" NOT IMPLEMENTED\n"));

	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup well-known user name
 ********************************************************************/
uint32 lookup_wk_user_name(uint32 rid, char *user_name, uint8 *type)
{
	int i = 0;
	(*type) = SID_NAME_USER;

	DEBUG(5,("lookup_wk_user_name: rid: %d", rid));

	/* look up the well-known domain user rids first */
	while (domain_user_rids[i].rid != rid && domain_user_rids[i].rid != 0)
	{
		i++;
	}

	if (domain_user_rids[i].rid != 0)
	{
		fstrcpy(user_name, domain_user_rids[i].name);
		DEBUG(5,(" = %s\n", user_name));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup user name
 ********************************************************************/
uint32 lookup_user_name(uint32 rid, char *user_name, uint8 *type)
{
	struct sam_disp_info *disp_info;
	(*type) = SID_NAME_USER;

	DEBUG(5,("lookup_user_name: rid: %d", rid));

	/* find the user account */
	become_root(True);
	disp_info = getsamdisprid(rid);
	unbecome_root(True);

	if (disp_info != NULL)
	{
		fstrcpy(user_name, disp_info->smb_name);
		DEBUG(5,(" = %s\n", user_name));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_group_rid
 ********************************************************************/
uint32 lookup_group_rid(char *group_name, uint32 *rid, uint8 *type)
{
	DOM_SID sid;

	(*rid) = 0;
	(*type) = SID_NAME_DOM_GRP;

	DEBUG(5,("lookup_group_rid: name: %s", group_name));

	if (map_group_name_to_sid(group_name, &sid) &&
	    sid_split_rid(&sid, rid) &&
	    sid_equal(&sid, &global_sam_sid))
	{
		DEBUG(5,(" = 0x%x\n", (*rid)));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_wk_group_rid
 ********************************************************************/
uint32 lookup_wk_group_rid(char *group_name, uint32 *rid, uint8 *type)
{
	char *grp_name;
	int i = -1; /* start do loop at -1 */
	(*rid) = 0;
	(*type) = SID_NAME_WKN_GRP;

	do /* find, if it exists, a group rid for the group name */
	{
		i++;
		(*rid) = domain_group_rids[i].rid;
		grp_name = domain_group_rids[i].name;

	} while (grp_name != NULL && !strequal(grp_name, group_name));

	return (grp_name != NULL) ? 0 : 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_alias_sid
 ********************************************************************/
uint32 lookup_alias_sid(char *alias_name, DOM_SID *sid, uint8 *type)
{
	(*type) = SID_NAME_ALIAS;

	DEBUG(5,("lookup_alias_rid: name: %s", alias_name));

	if (map_alias_name_to_sid(alias_name, sid))
	{
		fstring sid_str;
		sid_to_string(sid_str, sid);
		DEBUG(5,(" = %s\n", sid_str));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_alias_rid
 ********************************************************************/
uint32 lookup_alias_rid(char *alias_name, uint32 *rid, uint8 *type)
{
	DOM_SID sid;

	(*rid) = 0;
	(*type) = SID_NAME_ALIAS;

	DEBUG(5,("lookup_alias_rid: name: %s", alias_name));

	if (map_alias_name_to_sid(alias_name, &sid) &&
	    sid_split_rid(&sid, rid) &&
	    sid_equal(&sid, &global_sam_sid))
	{
		DEBUG(5,(" = 0x%x\n", (*rid)));
		return 0x0;
	}

	DEBUG(5,(" none mapped\n"));
	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_wk_alias_sid
 ********************************************************************/
uint32 lookup_wk_alias_sid(char *alias_name, DOM_SID *sid, uint8 *type)
{
	char *als_name;
	int i = 0;
	uint32 rid;
	(*type) = SID_NAME_ALIAS;

	do /* find, if it exists, a alias rid for the alias name*/
	{
		rid      = builtin_alias_rids[i].rid;
		als_name = builtin_alias_rids[i].name;

		i++;

		if (strequal(als_name, alias_name))
		{
			sid_copy(sid, &global_sid_S_1_5_20);
			sid_append_rid(sid, rid);

			return 0x0;
		}
			
	} while (als_name != NULL);

	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_wk_alias_rid
 ********************************************************************/
uint32 lookup_wk_alias_rid(char *alias_name, uint32 *rid, uint8 *type)
{
	char *als_name;
	int i = -1; /* start do loop at -1 */
	(*rid) = 0;
	(*type) = SID_NAME_ALIAS;

	do /* find, if it exists, a alias rid for the alias name*/
	{
		i++;
		(*rid) = builtin_alias_rids[i].rid;
		als_name = builtin_alias_rids[i].name;

	} while (als_name != NULL && !strequal(als_name, alias_name));

	return (als_name != NULL) ? 0 : 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_sid
 ********************************************************************/
uint32 lookup_sid(char *name, DOM_SID *sid, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;
	fstring domain;
	fstring user;
	
	split_domain_name(name, domain, user);

	if (!strequal(domain, global_sam_name))
	{
		DEBUG(0,("lookup_sid: remote domain %s not supported\n", domain));
		return status;
	}

	status = (status != 0x0) ? lookup_wk_alias_sid(user, sid, type) : status;
	status = (status != 0x0) ? lookup_alias_sid   (user, sid, type) : status;
#if 0
	status = (status != 0x0) ? lookup_domain_sid  (user, sid, type) : status;
#endif

	return status;
}

/*******************************************************************
 lookup_added_user_rid
 ********************************************************************/
uint32 lookup_added_user_rids(char *user_name,
		uint32 *usr_rid, uint32 *grp_rid)
{
	struct sam_passwd *sam_pass;
	(*usr_rid) = 0;
	(*grp_rid) = 0;

	/* find the user account */
	become_root(True);
	sam_pass = getsam21pwnam(user_name);
	unbecome_root(True);

	if (sam_pass != NULL)
	{
		(*usr_rid) = sam_pass->user_rid ;
		(*grp_rid) = sam_pass->group_rid;
		return 0x0;
	}

	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_added_user_rid
 ********************************************************************/
uint32 lookup_added_user_rid(char *user_name, uint32 *rid, uint8 *type)
{
	struct sam_passwd *sam_pass;
	(*rid) = 0;
	(*type) = SID_NAME_USER;

	/* find the user account */
	become_root(True);
	sam_pass = getsam21pwnam(user_name);
	unbecome_root(True);

	if (sam_pass != NULL)
	{
		(*rid) = sam_pass->user_rid;
		return 0x0;
	}

	return 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_wk_user_rid
 ********************************************************************/
uint32 lookup_wk_user_rid(char *user_name, uint32 *rid, uint8 *type)
{
	char *usr_name;
	int i = -1; /* start do loop at -1 */
	(*rid) = 0;
	(*type) = SID_NAME_USER;

	do /* find, if it exists, a alias rid for the alias name*/
	{
		i++;
		(*rid) = domain_user_rids[i].rid;
		usr_name = domain_user_rids[i].name;

	} while (usr_name != NULL && !strequal(usr_name, user_name));

	return (usr_name != NULL) ? 0 : 0xC0000000 | NT_STATUS_NONE_MAPPED;
}

/*******************************************************************
 lookup_added_grp_rid
 ********************************************************************/
uint32 lookup_added_grp_rid(char *name, uint32 *rid, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_group_rid(name, rid, type) : status;
	status = (status != 0x0) ? lookup_alias_rid(name, rid, type) : status;

	return status;
}

/*******************************************************************
 lookup_builtin_grp_rid
 ********************************************************************/
uint32 lookup_builtin_grp_rid(char *name, uint32 *rid, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_wk_group_rid(name, rid, type) : status;
	status = (status != 0x0) ? lookup_wk_alias_rid(name, rid, type) : status;

	return status;
}

/*******************************************************************
 lookup_grp_rid
 ********************************************************************/
uint32 lookup_grp_rid(char *name, uint32 *rid, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_builtin_grp_rid(name, rid, type) : status;
	status = (status != 0x0) ? lookup_added_grp_rid  (name, rid, type) : status;

	return status;
}

/*******************************************************************
 lookup_user_rid
 ********************************************************************/
uint32 lookup_user_rid(char *name, uint32 *rid, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_wk_user_rid   (name, rid, type) : status;
	status = (status != 0x0) ? lookup_added_user_rid(name, rid, type) : status;

	return status;
}

/*******************************************************************
 lookup_rid
 ********************************************************************/
uint32 lookup_rid(char *name, uint32 *rid, uint8 *type)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;

	status = (status != 0x0) ? lookup_user_rid(name, rid, type) : status;
	status = (status != 0x0) ? lookup_grp_rid (name, rid, type) : status;

	return status;
}

/*******************************************************************
 lookup_user_rids
 ********************************************************************/
uint32 lookup_user_rids(char *name, uint32 *usr_rid, uint32 *grp_rid)
{
	uint32 status = 0xC0000000 | NT_STATUS_NONE_MAPPED;
	uint8 type;

	/*
	 * try an ordinary user lookup
	 */

	status = lookup_added_user_rids(name, usr_rid, grp_rid);
	if (status == 0)
	{
		return status;
	}

	/*
	 * hm.  must be a well-known user, in a well-known group.
	 */

	status = lookup_wk_user_rid(name, usr_rid, &type);
	if (status != 0 || type != SID_NAME_USER)
	{
		return status; /* ok, maybe not! */
	}
	if (type != SID_NAME_USER)
	{
		return 0xC0000000 | NT_STATUS_NONE_MAPPED; /* users only... */
	}

	/*
	 * ok, got the user rid: now try the group rid
	 */

	status = lookup_builtin_grp_rid(name, grp_rid, &type);
	if (type == SID_NAME_DOM_GRP ||
	    type == SID_NAME_ALIAS ||
	    type == SID_NAME_WKN_GRP)
	{
		status = 0xC0000000 | NT_STATUS_NONE_MAPPED;
	}

	return status;
}
