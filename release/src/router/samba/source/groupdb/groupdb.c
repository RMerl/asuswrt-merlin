/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Password and authentication handling
   Copyright (C) Jeremy Allison 1996-1998
   Copyright (C) Luke Kenneth Casson Leighton 1996-1998
      
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

/*
 * NOTE. All these functions are abstracted into a structure
 * that points to the correct function for the selected database. JRA.
 */

static struct groupdb_ops *gpdb_ops;

/***************************************************************
 Initialise the group db operations.
***************************************************************/

BOOL initialise_group_db(void)
{
  if (gpdb_ops)
  {
    return True;
  }

#ifdef WITH_NISPLUS
  gpdb_ops =  nisplus_initialise_group_db();
#elif defined(WITH_LDAP)
  gpdb_ops = ldap_initialise_group_db();
#else 
  gpdb_ops = file_initialise_group_db();
#endif 

  return (gpdb_ops != NULL);
}

/*
 * Functions that return/manipulate a DOMAIN_GRP.
 */

/************************************************************************
 Utility function to search group database by gid: the DOMAIN_GRP
 structure does not have a gid member, so we have to convert here
 from gid to group rid.
*************************************************************************/
DOMAIN_GRP *iterate_getgroupgid(gid_t gid, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	return iterate_getgrouprid(pwdb_gid_to_group_rid(gid), mem, num_mem);
}

/************************************************************************
 Utility function to search group database by rid.  use this if your database
 does not have search facilities.
*************************************************************************/
DOMAIN_GRP *iterate_getgrouprid(uint32 rid, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	DOMAIN_GRP *grp = NULL;
	void *fp = NULL;

	DEBUG(10, ("search by rid: 0x%x\n", rid));

	/* Open the group database file - not for update. */
	fp = startgroupent(False);

	if (fp == NULL)
	{
		DEBUG(0, ("unable to open group database.\n"));
		return NULL;
	}

	while ((grp = getgroupent(fp, mem, num_mem)) != NULL && grp->rid != rid)
	{
	}

	if (grp != NULL)
	{
		DEBUG(10, ("found group %s by rid: 0x%x\n", grp->name, rid));
	}

	endgroupent(fp);
	return grp;
}

/************************************************************************
 Utility function to search group database by name.  use this if your database
 does not have search facilities.
*************************************************************************/
DOMAIN_GRP *iterate_getgroupnam(char *name, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	DOMAIN_GRP *grp = NULL;
	void *fp = NULL;

	DEBUG(10, ("search by name: %s\n", name));

	/* Open the group database file - not for update. */
	fp = startgroupent(False);

	if (fp == NULL)
	{
		DEBUG(0, ("unable to open group database.\n"));
		return NULL;
	}

	while ((grp = getgroupent(fp, mem, num_mem)) != NULL && !strequal(grp->name, name))
	{
	}

	if (grp != NULL)
	{
		DEBUG(10, ("found by name: %s\n", name));
	}

	endgroupent(fp);
	return grp;
}

/*************************************************************************
 Routine to return the next entry in the smbdomaingroup list.
 *************************************************************************/
BOOL add_domain_group(DOMAIN_GRP **grps, int *num_grps, DOMAIN_GRP *grp)
{
	if (grps == NULL || num_grps == NULL || grp == NULL)
	{
		return False;
	}

	(*grps) = Realloc((*grps), ((*num_grps)+1) * sizeof(DOMAIN_GRP));
	if ((*grps) == NULL)
	{
		return False;
	}

	DEBUG(10,("adding group %s(%s)\n", grp->name, grp->comment));

	fstrcpy((*grps)[(*num_grps)].name   , grp->name);
	fstrcpy((*grps)[(*num_grps)].comment, grp->comment);
	(*grps)[(*num_grps)].attr = grp->attr;
	(*grps)[(*num_grps)].rid  = grp->rid ;

	(*num_grps)++;

	return True;
}

/*************************************************************************
 checks to see if a user is a member of a domain group
 *************************************************************************/
static BOOL user_is_member(char *user_name, DOMAIN_GRP_MEMBER *mem, int num_mem)
{
	int i;
	for (i = 0; i < num_mem; i++)
	{
		DEBUG(10,("searching against user %s...\n", mem[i].name));
		if (strequal(mem[i].name, user_name))
		{
			DEBUG(10,("searching for user %s: found\n", user_name));
			return True;
		}
	}
	DEBUG(10,("searching for user %s: not found\n", user_name));
	return False;
}

/*************************************************************************
 gets an array of groups that a user is in.  use this if your database
 does not have search facilities
 *************************************************************************/
BOOL iterate_getusergroupsnam(char *user_name, DOMAIN_GRP **grps, int *num_grps)
{
	DOMAIN_GRP *grp;
	DOMAIN_GRP_MEMBER *mem = NULL;
	int num_mem = 0;
	void *fp = NULL;

	DEBUG(10, ("search for usergroups by name: %s\n", user_name));

	if (user_name == NULL || grp == NULL || num_grps == NULL)
	{
		return False;
	}

	(*grps) = NULL;
	(*num_grps) = 0;

	/* Open the group database file - not for update. */
	fp = startgroupent(False);

	if (fp == NULL)
	{
		DEBUG(0, ("unable to open group database.\n"));
		return False;
	}

	/* iterate through all groups.  search members for required user */
	while ((grp = getgroupent(fp, &mem, &num_mem)) != NULL)
	{
		DEBUG(5,("group name %s members: %d\n", grp->name, num_mem));
		if (num_mem != 0 && mem != NULL)
		{
			BOOL ret = True;
			if (user_is_member(user_name, mem, num_mem))
			{
				ret = add_domain_group(grps, num_grps, grp);
			}

			free(mem);
			mem = NULL;
			num_mem = 0;

			if (!ret)
			{
				(*num_grps) = 0;
				break;
			}
		}
	}

	if ((*num_grps) != 0)
	{
		DEBUG(10, ("found %d user groups:\n", (*num_grps)));
	}

	endgroupent(fp);
	return True;
}

/*************************************************************************
 gets an array of groups that a user is in.  use this if your database
 does not have search facilities
 *************************************************************************/
BOOL enumdomgroups(DOMAIN_GRP **grps, int *num_grps)
{
	DOMAIN_GRP *grp;
	void *fp = NULL;

	DEBUG(10, ("enum user groups\n"));

	if (grp == NULL || num_grps == NULL)
	{
		return False;
	}

	(*grps) = NULL;
	(*num_grps) = 0;

	/* Open the group database file - not for update. */
	fp = startgroupent(False);

	if (fp == NULL)
	{
		DEBUG(0, ("unable to open group database.\n"));
		return False;
	}

	/* iterate through all groups. */
	while ((grp = getgroupent(fp, NULL, NULL)) != NULL)
	{
		if (!add_domain_group(grps, num_grps, grp))
		{
			DEBUG(0,("unable to add group while enumerating\n"));
			return False;
		}
	}

	if ((*num_grps) != 0)
	{
		DEBUG(10, ("found %d user groups:\n", (*num_grps)));
	}

	endgroupent(fp);
	return True;
}

/***************************************************************
 Start to enumerate the group database list. Returns a void pointer
 to ensure no modification outside this module.
****************************************************************/

void *startgroupent(BOOL update)
{
  return gpdb_ops->startgroupent(update);
}

/***************************************************************
 End enumeration of the group database list.
****************************************************************/

void endgroupent(void *vp)
{
  gpdb_ops->endgroupent(vp);
}

/*************************************************************************
 Routine to return the next entry in the group database list.
 *************************************************************************/

DOMAIN_GRP *getgroupent(void *vp, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	return gpdb_ops->getgroupent(vp, mem, num_mem);
}

/************************************************************************
 Routine to add an entry to the group database file.
*************************************************************************/

BOOL add_group_entry(DOMAIN_GRP *newgrp)
{
 	return gpdb_ops->add_group_entry(newgrp);
}

/************************************************************************
 Routine to search the group database file for an entry matching the groupname.
 and then replace the entry.
************************************************************************/

BOOL mod_group_entry(DOMAIN_GRP* grp)
{
 	return gpdb_ops->mod_group_entry(grp);
}

/************************************************************************
 Routine to search group database by name.
*************************************************************************/

DOMAIN_GRP *getgroupnam(char *name, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	return gpdb_ops->getgroupnam(name, mem, num_mem);
}

/************************************************************************
 Routine to search group database by group rid.
*************************************************************************/

DOMAIN_GRP *getgrouprid(uint32 group_rid, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	return gpdb_ops->getgrouprid(group_rid, mem, num_mem);
}

/************************************************************************
 Routine to search group database by gid.
*************************************************************************/

DOMAIN_GRP *getgroupgid(gid_t gid, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	return gpdb_ops->getgroupgid(gid, mem, num_mem);
}

/*************************************************************************
 gets an array of groups that a user is in.
 *************************************************************************/
BOOL getusergroupsnam(char *user_name, DOMAIN_GRP **grp, int *num_grps)
{
	return gpdb_ops->getusergroupsnam(user_name, grp, num_grps);
}

/*************************************************************
 initialises a DOMAIN_GRP.
 **************************************************************/

void gpdb_init_grp(DOMAIN_GRP *grp)
{
	if (grp == NULL) return;
	ZERO_STRUCTP(grp);
}

