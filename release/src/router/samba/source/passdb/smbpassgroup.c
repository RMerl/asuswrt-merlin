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

static int grp_file_lock_depth = 0;
extern int DEBUGLEVEL;

/***************************************************************
 Start to enumerate the smbpasswd list. Returns a void pointer
 to ensure no modification outside this module.
****************************************************************/

static void *startsmbfilegrpent(BOOL update)
{
	static char s_readbuf[1024];
	return startfilepwent(lp_smb_passgrp_file(), s_readbuf, sizeof(s_readbuf),
	                      &grp_file_lock_depth, update);
}

/***************************************************************
 End enumeration of the smbpasswd list.
****************************************************************/

static void endsmbfilegrpent(void *vp)
{
	endfilepwent(vp, &grp_file_lock_depth);
}

/*************************************************************************
 Return the current position in the smbpasswd list as an SMB_BIG_UINT.
 This must be treated as an opaque token.
*************************************************************************/

static SMB_BIG_UINT getsmbfilegrppos(void *vp)
{
	return getfilepwpos(vp);
}

/*************************************************************************
 Set the current position in the smbpasswd list from an SMB_BIG_UINT.
 This must be treated as an opaque token.
*************************************************************************/

static BOOL setsmbfilegrppos(void *vp, SMB_BIG_UINT tok)
{
	return setfilepwpos(vp, tok);
}

/*************************************************************************
 Routine to return the next entry in the smbpasswd list.
 *************************************************************************/
static struct smb_passwd *getsmbfilegrpent(void *vp,
		uint32 **grp_rids, int *num_grps,
		uint32 **als_rids, int *num_alss)
{
	/* Static buffers we will return. */
	static struct smb_passwd pw_buf;
	static pstring  user_name;
	struct passwd *pwfile;
	pstring		linebuf;
	unsigned char  *p;
	int            uidval;
	size_t            linebuf_len;

	if (vp == NULL)
	{
		DEBUG(0,("getsmbfilegrpent: Bad password file pointer.\n"));
		return NULL;
	}

	pwdb_init_smb(&pw_buf);

	/*
	 * Scan the file, a line at a time.
	 */
	while ((linebuf_len = getfileline(vp, linebuf, sizeof(linebuf))) > 0)
	{
		/*
		 * The line we have should be of the form :-
		 * 
		 * username:uid:domainrid1,domainrid2..:aliasrid1,aliasrid2..:
		 */

		/*
		 * As 256 is shorter than a pstring we don't need to check
		 * length here - if this ever changes....
		 */
		p = strncpyn(user_name, linebuf, sizeof(user_name), ':');

		/* Go past ':' */
		p++;

		/* Get smb uid. */

		p = Atoic((char *) p, &uidval, ":");

		pw_buf.smb_name = user_name;
		pw_buf.smb_userid = uidval;

		/*
		 * Now get the password value - this should be 32 hex digits
		 * which are the ascii representations of a 16 byte string.
		 * Get two at a time and put them into the password.
		 */

		/* Skip the ':' */
		p++;

		if (grp_rids != NULL && num_grps != NULL)
		{
			int i;
			p = get_numlist(p, grp_rids, num_grps);
			if (p == NULL)
			{
				DEBUG(0,("getsmbfilegrpent: invalid line\n"));
				return NULL;
			}
			for (i = 0; i < (*num_grps); i++)
			{
				(*grp_rids)[i] = pwdb_gid_to_group_rid((*grp_rids)[i]);
			}
		}

		/* Skip the ':' */
		p++;

		if (als_rids != NULL && num_alss != NULL)
		{
			int i;
			p = get_numlist(p, als_rids, num_alss);
			if (p == NULL)
			{
				DEBUG(0,("getsmbfilegrpent: invalid line\n"));
				return NULL;
			}
			for (i = 0; i < (*num_alss); i++)
			{
				(*als_rids)[i] = pwdb_gid_to_alias_rid((*als_rids)[i]);
			}
		}

		pwfile = Get_Pwnam(pw_buf.smb_name, False);
		if (pwfile == NULL)
		{
			DEBUG(0,("getsmbfilegrpent: smbpasswd database is corrupt!\n"));
			DEBUG(0,("getsmbfilegrpent: username %s not in unix passwd database!\n", pw_buf.smb_name));
			return NULL;
		}

		return &pw_buf;
	}

	DEBUG(5,("getsmbfilegrpent: end of file reached.\n"));
	return NULL;
}

static struct passgrp_ops file_ops =
{
	startsmbfilegrpent,
	endsmbfilegrpent,
	getsmbfilegrppos,
	setsmbfilegrppos,
	iterate_getsmbgrpnam,          /* In passgrp.c */
	iterate_getsmbgrpuid,          /* In passgrp.c */
	iterate_getsmbgrprid,          /* In passgrp.c */
	getsmbfilegrpent,
};

struct passgrp_ops *file_initialise_password_grp(void)
{    
  return &file_ops;
}

#else
 /* Do *NOT* make this function static. It breaks the compile on gcc. JRA */
 void smbpass_dummy_function(void) { } /* stop some compilers complaining */
#endif /* USE_SMBPASS_DB */
