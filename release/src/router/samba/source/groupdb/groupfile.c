/*
 * Unix SMB/Netbios implementation. Version 1.9. SMB parameters and setup
 * Copyright (C) Andrew Tridgell 1992-1998 Modified by Jeremy Allison 1995.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

#ifdef USE_SMBPASS_DB

static int gp_file_lock_depth = 0;
extern int DEBUGLEVEL;

static char s_readbuf[1024];

/***************************************************************
 Start to enumerate the grppasswd list. Returns a void pointer
 to ensure no modification outside this module.
****************************************************************/

static void *startgrpfilepwent(BOOL update)
{
	return startfilepwent(lp_smb_group_file(),
	                      s_readbuf, sizeof(s_readbuf),
	                      &gp_file_lock_depth, update);
}

/***************************************************************
 End enumeration of the grppasswd list.
****************************************************************/

static void endgrpfilepwent(void *vp)
{
	endfilepwent(vp, &gp_file_lock_depth);
}

/*************************************************************************
 Return the current position in the grppasswd list as an SMB_BIG_UINT.
 This must be treated as an opaque token.
*************************************************************************/
static SMB_BIG_UINT getgrpfilepwpos(void *vp)
{
	return getfilepwpos(vp);
}

/*************************************************************************
 Set the current position in the grppasswd list from an SMB_BIG_UINT.
 This must be treated as an opaque token.
*************************************************************************/
static BOOL setgrpfilepwpos(void *vp, SMB_BIG_UINT tok)
{
	return setfilepwpos(vp, tok);
}

static BOOL make_group_line(char *p, int max_len,
				DOMAIN_GRP *grp,
				DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	int i;
	int len;
	len = slprintf(p, max_len-1, "%s:%s:%d:", grp->name, grp->comment, grp->rid);

	if (len == -1)
	{
		DEBUG(0,("make_group_line: cannot create entry\n"));
		return False;
	}

	p += len;
	max_len -= len;

	if (mem == NULL || num_mem == NULL)
	{
		return True;
	}

	for (i = 0; i < (*num_mem); i++)
	{
		len = strlen((*mem)[i].name);
		p = safe_strcpy(p, (*mem)[i].name, max_len); 

		if (p == NULL)
		{
			DEBUG(0, ("make_group_line: out of space for groups!\n"));
			return False;
		}

		max_len -= len;

		if (i != (*num_mem)-1)
		{
			*p = ',';
			p++;
			max_len--;
		}
	}

	return True;
}

/*************************************************************************
 Routine to return the next entry in the smbdomaingroup list.
 *************************************************************************/
static char *get_group_members(char *p, int *num_mem, DOMAIN_GRP_MEMBER **members)
{
	fstring name;

	if (num_mem == NULL || members == NULL)
	{
		return NULL;
	}

	(*num_mem) = 0;
	(*members) = NULL;

	while (next_token(&p, name, ",", sizeof(fstring)))
	{
		(*members) = Realloc((*members), ((*num_mem)+1) * sizeof(DOMAIN_GRP_MEMBER));
		if ((*members) == NULL)
		{
			return NULL;
		}
		fstrcpy((*members)[(*num_mem)].name, name);
		(*members)[(*num_mem)].attr = 0x07;
		(*num_mem)++;
	}
	return p;
}

/*************************************************************************
 Routine to return the next entry in the smbdomaingroup list.
 *************************************************************************/
static DOMAIN_GRP *getgrpfilepwent(void *vp, DOMAIN_GRP_MEMBER **mem, int *num_mem)
{
	/* Static buffers we will return. */
	static DOMAIN_GRP gp_buf;

	int gidval;

	pstring linebuf;
	char  *p;
	size_t            linebuf_len;

	gpdb_init_grp(&gp_buf);

	/*
	 * Scan the file, a line at a time and check if the name matches.
	 */
	while ((linebuf_len = getfileline(vp, linebuf, sizeof(linebuf))) > 0)
	{
		/* get group name */

		p = strncpyn(gp_buf.name, linebuf, sizeof(gp_buf.name), ':');
		if (p == NULL)
		{
			DEBUG(0, ("getgrpfilepwent: malformed group entry (no :)\n"));
			continue;
		}

		/* Go past ':' */
		p++;

		/* get group comment */

		p = strncpyn(gp_buf.comment, p, sizeof(gp_buf.comment), ':');
		if (p == NULL)
		{
			DEBUG(0, ("getgrpfilepwent: malformed group entry (no :)\n"));
			continue;
		}

		/* Go past ':' */
		p++;

		/* Get group gid. */

		p = Atoic(p, &gidval, ":");

		if (p == NULL)
		{
			DEBUG(0, ("getgrpfilepwent: malformed group entry (no : after uid)\n"));
			continue;
		}

		/* Go past ':' */
		p++;

		/* now get the user's groups.  there are a maximum of 32 */

		if (mem != NULL && num_mem != NULL)
		{
			(*mem) = NULL;
			(*num_mem) = 0;

			p = get_group_members(p, num_mem, mem);
			if (p == NULL)
			{
				DEBUG(0, ("getgrpfilepwent: malformed group entry (no : after members)\n"));
			}
		}

		/* ok, set up the static data structure and return it */

		gp_buf.rid     = pwdb_gid_to_group_rid((gid_t)gidval);
		gp_buf.attr    = 0x07;

		make_group_line(linebuf, sizeof(linebuf), &gp_buf, mem, num_mem);
		DEBUG(10,("line: '%s'\n", linebuf));

		return &gp_buf;
	}

	DEBUG(5,("getgrpfilepwent: end of file reached.\n"));
	return NULL;
}

/************************************************************************
 Routine to add an entry to the grppasswd file.
*************************************************************************/

static BOOL add_grpfilegrp_entry(DOMAIN_GRP *newgrp)
{
	DEBUG(0, ("add_grpfilegrp_entry: NOT IMPLEMENTED\n"));
	return False;
}

/************************************************************************
 Routine to search the grppasswd file for an entry matching the groupname.
 and then modify its group entry. We can't use the startgrppwent()/
 getgrppwent()/endgrppwent() interfaces here as we depend on looking
 in the actual file to decide how much room we have to write data.
 override = False, normal
 override = True, override XXXXXXXX'd out group or NO PASS
************************************************************************/

static BOOL mod_grpfilegrp_entry(DOMAIN_GRP* grp)
{
	DEBUG(0, ("mod_grpfilegrp_entry: NOT IMPLEMENTED\n"));
	return False;
}


static struct groupdb_ops file_ops =
{
	startgrpfilepwent,
	endgrpfilepwent,
	getgrpfilepwpos,
	setgrpfilepwpos,

	iterate_getgroupnam,          /* In groupdb.c */
	iterate_getgroupgid,          /* In groupdb.c */
	iterate_getgrouprid,          /* In groupdb.c */
	getgrpfilepwent,

	add_grpfilegrp_entry,
	mod_grpfilegrp_entry,

	iterate_getusergroupsnam      /* in groupdb.c */
};

struct groupdb_ops *file_initialise_group_db(void)
{    
	return &file_ops;
}

#else
 /* Do *NOT* make this function static. It breaks the compile on gcc. JRA */
 void grppass_dummy_function(void) { } /* stop some compilers complaining */
#endif /* USE_SMBPASS_DB */
