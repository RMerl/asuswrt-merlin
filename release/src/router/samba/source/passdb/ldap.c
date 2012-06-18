/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   LDAP protocol helper functions for SAMBA
   Copyright (C) Jean François Micouleau 1998
   
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
/* Must have this here if we want to test WITH_LDAP ... */
#include "includes.h"

#ifdef WITH_LDAP

#include <lber.h>
#include <ldap.h>

#define ADD_USER 1
#define MODIFY_USER 2

extern int DEBUGLEVEL;

/*******************************************************************
 open a connection to the ldap serve.
******************************************************************/	
static BOOL ldap_open_connection(LDAP **ldap_struct)
{
	if ( (*ldap_struct = ldap_open(lp_ldap_server(),lp_ldap_port()) ) == NULL)
	{
		DEBUG( 0, ( "The LDAP server is not responding !\n" ) );
		return( False );
	}
	DEBUG(2,("ldap_open_connection: connection opened\n"));
	return (True);
}


/*******************************************************************
 connect anonymously to the ldap server.
 FIXME: later (jfm)
******************************************************************/	
static BOOL ldap_connect_anonymous(LDAP *ldap_struct)
{
	if ( ldap_simple_bind_s(ldap_struct,lp_ldap_root(),lp_ldap_rootpasswd()) ! = LDAP_SUCCESS)
	{
		DEBUG( 0, ( "Couldn't bind to the LDAP server !\n" ) );
		return(False);
	}
	return (True);
}


/*******************************************************************
 connect to the ldap server under system privileg.
******************************************************************/	
static BOOL ldap_connect_system(LDAP *ldap_struct)
{
	if ( ldap_simple_bind_s(ldap_struct,lp_ldap_root(),lp_ldap_rootpasswd()) ! = LDAP_SUCCESS)
	{
		DEBUG( 0, ( "Couldn't bind to the LDAP server!\n" ) );
		return(False);
	}
	DEBUG(2,("ldap_connect_system: succesful connection to the LDAP server\n"));
	return (True);
}

/*******************************************************************
 connect to the ldap server under a particular user.
******************************************************************/	
static BOOL ldap_connect_user(LDAP *ldap_struct, char *user, char *password)
{
	if ( ldap_simple_bind_s(ldap_struct,lp_ldap_root(),lp_ldap_rootpasswd()) ! = LDAP_SUCCESS)
	{
		DEBUG( 0, ( "Couldn't bind to the LDAP server !\n" ) );
		return(False);
	}
	DEBUG(2,("ldap_connect_user: succesful connection to the LDAP server\n"));
	return (True);
}

/*******************************************************************
 run the search by name.
******************************************************************/	
static BOOL ldap_search_one_user(LDAP *ldap_struct, char *filter, LDAPMessage **result)
{	
	int scope = LDAP_SCOPE_ONELEVEL;
	int rc;
		
	DEBUG(2,("ldap_search_one_user: searching for:[%s]\n", filter));
		
	rc = ldap_search_s(ldap_struct, lp_ldap_suffix(), scope, filter, NULL, 0, result);

	if (rc ! = LDAP_SUCCESS )
	{
		DEBUG( 0, ( "Problem during the LDAP search\n" ) );
		return(False);
	}
	return (True);
}

/*******************************************************************
 run the search by name.
******************************************************************/	
static BOOL ldap_search_one_user_by_name(LDAP *ldap_struct, char *user, LDAPMessage **result)
{	
	pstring filter;
	/*
	   in the filter expression, replace %u with the real name
	   so in ldap filter, %u MUST exist :-)
	*/	
	pstrcpy(filter,lp_ldap_filter());
	pstring_sub(filter,"%u",user);
	
	if ( !ldap_search_one_user(ldap_struct, filter, result) )
	{
		return(False);
	}
	return (True);
}

/*******************************************************************
 run the search by uid.
******************************************************************/	
static BOOL ldap_search_one_user_by_uid(LDAP *ldap_struct, int uid, LDAPMessage **result)
{	
	pstring filter;
	
	slprintf(filter, sizeof(pstring)-1, "uidAccount = %d", uid);
	
	if ( !ldap_search_one_user(ldap_struct, filter, result) )
	{	
		return(False);
	}
	return (True);
}

/*******************************************************************
 search an attribute and return the first value found.
******************************************************************/
static void get_single_attribute(LDAP *ldap_struct, LDAPMessage *entry, char *attribute, char *value)
{
	char **valeurs;
	
	if ( (valeurs = ldap_get_values(ldap_struct, entry, attribute)) ! = NULL) 
	{
		pstrcpy(value, valeurs[0]);
		ldap_value_free(valeurs);
		DEBUG(3,("get_single_attribute:	[%s] = [%s]\n", attribute, value));	
	}
	else
	{
		value = NULL;
	}
}

/*******************************************************************
 check if the returned entry is a sambaAccount objectclass.
******************************************************************/	
static BOOL ldap_check_user(LDAP *ldap_struct, LDAPMessage *entry)
{
	BOOL sambaAccount = False;
	char **valeur;
	int i;

	DEBUG(2,("ldap_check_user: "));
	valeur = ldap_get_values(ldap_struct, entry, "objectclass");
	if (valeur! = NULL)
	{
		for (i = 0;valeur[i]! = NULL;i++)
		{
			if (!strcmp(valeur[i],"sambaAccount")) sambaAccount = True;
		}
	}
	DEBUG(2,("%s\n",sambaAccount?"yes":"no"));
	ldap_value_free(valeur);
	return (sambaAccount);
}

/*******************************************************************
 check if the returned entry is a sambaTrust objectclass.
******************************************************************/	
static BOOL ldap_check_trust(LDAP *ldap_struct, LDAPMessage *entry)
{
	BOOL sambaTrust = False;
	char **valeur;
	int i;
	
	DEBUG(2,("ldap_check_trust: "));
	valeur = ldap_get_values(ldap_struct, entry, "objectclass");
	if (valeur! = NULL)
	{
		for (i = 0;valeur[i]! = NULL;i++)
		{
			if (!strcmp(valeur[i],"sambaTrust")) sambaTrust = True;
		}
	}	
	DEBUG(2,("%s\n",sambaTrust?"yes":"no"));
	ldap_value_free(valeur);	
	return (sambaTrust);
}

/*******************************************************************
 retrieve the user's info and contruct a smb_passwd structure.
******************************************************************/
static void ldap_get_smb_passwd(LDAP *ldap_struct,LDAPMessage *entry, 
                          struct smb_passwd *user)
{	
	static pstring user_name;
	static pstring user_pass;
	static pstring temp;
	static unsigned char smblmpwd[16];
	static unsigned char smbntpwd[16];

	pdb_init_smb(user);

	memset((char *)smblmpwd, '\0', sizeof(smblmpwd));
	memset((char *)smbntpwd, '\0', sizeof(smbntpwd));

	get_single_attribute(ldap_struct, entry, "cn", user_name);
	DEBUG(2,("ldap_get_smb_passwd: user: %s\n",user_name));
		
#ifdef LDAP_PLAINTEXT_PASSWORD
	get_single_attribute(ldap_struct, entry, "userPassword", temp);
	nt_lm_owf_gen(temp, user->smb_nt_passwd, user->smb_passwd);
	memset((char *)temp, '\0', sizeof(temp)); /* destroy local copy of the password */
#else
	get_single_attribute(ldap_struct, entry, "unicodePwd", temp);
	pdb_gethexpwd(temp, smbntpwd);		
	memset((char *)temp, '\0', sizeof(temp)); /* destroy local copy of the password */

	get_single_attribute(ldap_struct, entry, "dBCSPwd", temp);
	pdb_gethexpwd(temp, smblmpwd);		
	memset((char *)temp, '\0', sizeof(temp)); /* destroy local copy of the password */
#endif
	
	get_single_attribute(ldap_struct, entry, "userAccountControl", temp);
	user->acct_ctrl = pdb_decode_acct_ctrl(temp);

	get_single_attribute(ldap_struct, entry, "pwdLastSet", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	get_single_attribute(ldap_struct, entry, "rid", temp);

	/* the smb (unix) ids are not stored: they are created */
	user->smb_userid = pdb_user_rid_to_uid (atoi(temp));

	if (user->acct_ctrl & (ACB_DOMTRUST|ACB_WSTRUST|ACB_SVRTRUST) )
	{
		DEBUG(0,("Inconsistency in the LDAP database\n"));
	}
	if (user->acct_ctrl & ACB_NORMAL)
	{
		user->smb_name      = user_name;
		user->smb_passwd    = smblmpwd;
		user->smb_nt_passwd = smbntpwd;
	}
}

/*******************************************************************
 retrieve the user's info and contruct a sam_passwd structure.

 calls ldap_get_smb_passwd function first, though, to save code duplication.

******************************************************************/
static void ldap_get_sam_passwd(LDAP *ldap_struct, LDAPMessage *entry, 
                          struct sam_passwd *user)
{	
	static pstring user_name;
	static pstring fullname;
	static pstring home_dir;
	static pstring dir_drive;
	static pstring logon_script;
	static pstring profile_path;
	static pstring acct_desc;
	static pstring workstations;
	static pstring temp;
	static struct smb_passwd pw_buf;

	pdb_init_sam(user);

	ldap_get_smb_passwd(ldap_struct, entry, &pw_buf);
	
	user->pass_last_set_time    = pw_buf.pass_last_set_time;

	get_single_attribute(ldap_struct, entry, "logonTime", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	get_single_attribute(ldap_struct, entry, "logoffTime", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	get_single_attribute(ldap_struct, entry, "kickoffTime", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	get_single_attribute(ldap_struct, entry, "pwdLastSet", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	get_single_attribute(ldap_struct, entry, "pwdCanChange", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	get_single_attribute(ldap_struct, entry, "pwdMustChange", temp);
	user->pass_last_set_time = (time_t)strtol(temp, NULL, 16);

	user->smb_name = pw_buf.smb_name;

	DEBUG(2,("ldap_get_sam_passwd: user: %s\n", user_name));
		
	get_single_attribute(ldap_struct, entry, "userFullName", fullname);
	user->full_name = fullname;

	get_single_attribute(ldap_struct, entry, "homeDirectory", home_dir);
	user->home_dir = home_dir;

	get_single_attribute(ldap_struct, entry, "homeDrive", dir_drive);
	user->dir_drive = dir_drive;

	get_single_attribute(ldap_struct, entry, "scriptPath", logon_script);
	user->logon_script = logon_script;

	get_single_attribute(ldap_struct, entry, "profilePath", profile_path);
	user->profile_path = profile_path;

	get_single_attribute(ldap_struct, entry, "comment", acct_desc);
	user->acct_desc = acct_desc;

	get_single_attribute(ldap_struct, entry, "userWorkstations", workstations);
	user->workstations = workstations;

	user->unknown_str = NULL; /* don't know, yet! */
	user->munged_dial = NULL; /* "munged" dial-back telephone number */

	get_single_attribute(ldap_struct, entry, "rid", temp);
	user->user_rid = atoi(temp);

	get_single_attribute(ldap_struct, entry, "primaryGroupID", temp);
	user->group_rid = atoi(temp);

	/* the smb (unix) ids are not stored: they are created */
	user->smb_userid = pw_buf.smb_userid;
	user->smb_grpid = group_rid_to_uid(user->group_rid);

	user->acct_ctrl = pw_buf.acct_ctrl;

	user->unknown_3 = 0xffffff; /* don't know */
	user->logon_divs = 168; /* hours per week */
	user->hours_len = 21; /* 21 times 8 bits = 168 */
	memset(user->hours, 0xff, user->hours_len); /* available at all hours */
	user->unknown_5 = 0x00020000; /* don't know */
	user->unknown_5 = 0x000004ec; /* don't know */

	if (user->acct_ctrl & (ACB_DOMTRUST|ACB_WSTRUST|ACB_SVRTRUST) )
	{
		DEBUG(0,("Inconsistency in the LDAP database\n"));
	}

	if (!(user->acct_ctrl & ACB_NORMAL))
	{
		DEBUG(0,("User's acct_ctrl bits not set to ACT_NORMAL in LDAP database\n"));
		return;
	}
}

/************************************************************************
 Routine to manage the LDAPMod structure array
 manage memory used by the array, by each struct, and values

************************************************************************/
static void make_a_mod(LDAPMod ***modlist,int modop, char *attribute, char *value)
{
	LDAPMod **mods;
	int i;
	int j;
	
	mods = *modlist;
	
	if (mods == NULL)
	{
		mods = (LDAPMod **)malloc( sizeof(LDAPMod *) );
		if (mods == NULL)
		{
			DEBUG(0,("make_a_mod: out of memory!\n"));
			return;
		}
		mods[0] = NULL;
	}
	
	for ( i = 0; mods[ i ] ! = NULL; ++i )
	{
		if ( mods[ i ]->mod_op == modop && 
		    !strcasecmp( mods[ i ]->mod_type, attribute ) )
		{
			break;
		}
	}
	
	if (mods[i] == NULL)
	{
		mods = (LDAPMod **)realloc( mods,  (i+2) * sizeof( LDAPMod * ) );	
		if (mods == NULL)
		{
			DEBUG(0,("make_a_mod: out of memory!\n"));
			return;
		}
		mods[i] = (LDAPMod *)malloc( sizeof( LDAPMod ) );
		if (mods[i] == NULL)
		{
			DEBUG(0,("make_a_mod: out of memory!\n"));
			return;
		}
		mods[i]->mod_op = modop;
		mods[i]->mod_values = NULL;
		mods[i]->mod_type = strdup( attribute );
		mods[i+1] = NULL;
	}

	if (value ! = NULL )
	{
		j = 0;
		if ( mods[ i ]->mod_values ! = NULL )
		{
			for ( ; mods[ i ]->mod_values[ j ] ! = NULL; j++ );
		}
		mods[ i ]->mod_values = (char **)realloc(mods[ i ]->mod_values,
		                                          (j+2) * sizeof( char * ));
		if ( mods[ i ]->mod_values == NULL)
		{
			DEBUG(0, "make_a_mod: Memory allocation failure!\n");
			return;
		}
		mods[ i ]->mod_values[ j ] = strdup(value);	
		mods[ i ]->mod_values[ j + 1 ] = NULL;		
	}
	*modlist = mods;
}

/************************************************************************
 Add or modify an entry. Only the smb struct values

*************************************************************************/
static BOOL modadd_ldappwd_entry(struct smb_passwd *newpwd, int flag)
{
	
	/* assume the struct is correct and filled
	   that's the job of passdb.c to check */
	int scope = LDAP_SCOPE_ONELEVEL;
	int rc;
	char *smb_name;
	int trust = False;
	int ldap_state;
	pstring filter;
	pstring dn;
	pstring lmhash;
	pstring nthash;
	pstring rid;
	pstring lst;
	pstring temp;

	LDAP *ldap_struct;
	LDAPMessage *result;
	LDAPMod **mods;
	
	smb_name = newpwd->smb_name;

	if (!ldap_open_connection(&ldap_struct)) /* open a connection to the server */
	{
		return False;
	}

	if (!ldap_connect_system(ldap_struct)) /* connect as system account */
	{
		ldap_unbind(ldap_struct);
		return False;
	}
	
	if (smb_name[strlen(smb_name)-1] == '$' )
	{
		smb_name[strlen(smb_name)-1] = '\0';
		trust = True;
	}

	slprintf(filter, sizeof(filter)-1,
	         "(&(cn = %s)(|(objectclass = sambaTrust)(objectclass = sambaAccount)))",
	         smb_name);
	
	rc = ldap_search_s(ldap_struct, lp_ldap_suffix(), scope, filter, NULL, 0, &result);

	switch (flag)
	{
		case ADD_USER:
		{
			if (ldap_count_entries(ldap_struct, result) ! = 0)
			{
				DEBUG(0,("User already in the base, with samba properties\n"));		
				ldap_unbind(ldap_struct);
				return False;
			}
			ldap_state = LDAP_MOD_ADD;
			break;
		}
		case MODIFY_USER:
		{
			if (ldap_count_entries(ldap_struct, result) ! = 1)
			{
				DEBUG(0,("No user to modify !\n"));		
				ldap_unbind(ldap_struct);
				return False;
			}
			ldap_state = LDAP_MOD_REPLACE;
			break;
		}
		default:
		{
			DEBUG(0,("How did you come here? \n"));		
			ldap_unbind(ldap_struct);
			return False;
			break;
		}
	}
	slprintf(dn, sizeof(dn)-1, "cn = %s, %s",smb_name, lp_ldap_suffix() );

	if (newpwd->smb_passwd ! = NULL)
	{
		int i;
		for( i = 0; i < 16; i++)
		{
			slprintf(&temp[2*i], sizeof(temp) - 1, "%02X", newpwd->smb_passwd[i]);
		}
    	
	}
	else
	{
		if (newpwd->acct_ctrl & ACB_PWNOTREQ)
		{
			slprintf(temp, sizeof(temp) - 1, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX");
		}
		else
		{
			slprintf(temp, sizeof(temp) - 1, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
		}
	}
	slprintf(lmhash, sizeof(lmhash)-1, "%s", temp);

	if (newpwd->smb_nt_passwd ! = NULL)
	{
		int i;
   		for( i = 0; i < 16; i++)
		{
			slprintf(&temp[2*i], sizeof(temp) - 1, "%02X", newpwd->smb_nt_passwd[i]);
		}
    	
	}
	else
	{
		if (newpwd->acct_ctrl & ACB_PWNOTREQ)
		{
			slprintf(temp, sizeof(temp) - 1, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX");
		}
		else
		{
			slprintf(temp, sizeof(temp) - 1, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
		}
	}
	slprintf(nthash, sizeof(nthash)-1, "%s", temp);

	slprintf(rid, sizeof(rid)-1, "%d", uid_to_user_rid(newpwd->smb_userid) );
	slprintf(lst, sizeof(lst)-1, "%08X", newpwd->pass_last_set_time);
	
	mods = NULL; 

	if (trust)
	{
		make_a_mod(&mods, ldap_state, "objectclass", "sambaTrust");
		make_a_mod(&mods, ldap_state, "netbiosTrustName", smb_name);
		make_a_mod(&mods, ldap_state, "trustPassword", nthash);
	}
	else
	{
		make_a_mod(&mods, ldap_state, "objectclass", "sambaAccount");
		make_a_mod(&mods, ldap_state, "dBCSPwd", lmhash);
		make_a_mod(&mods, ldap_state, "uid", smb_name);
		make_a_mod(&mods, ldap_state, "unicodePwd", nthash);
	}
	
	make_a_mod(&mods, ldap_state, "cn", smb_name);
	
	make_a_mod(&mods, ldap_state, "rid", rid);
	make_a_mod(&mods, ldap_state, "pwdLastSet", lst);
	make_a_mod(&mods, ldap_state, "userAccountControl", pdb_encode_acct_ctrl(newpwd->acct_ctrl, NEW_PW_FORMAT_SPACE_PADDED_LEN));
	
	switch(flag)
	{
		case ADD_USER:
		{
			ldap_add_s(ldap_struct, dn, mods);
			DEBUG(2,("modadd_ldappwd_entry: added: cn = %s in the LDAP database\n",smb_name));
			break;
		}
		case MODIFY_USER:
		{
			ldap_modify_s(ldap_struct, dn, mods);
			DEBUG(2,("modadd_ldappwd_entry: changed: cn = %s in the LDAP database_n",smb_name));
			break;
		}
		default:
		{
			DEBUG(2,("modadd_ldappwd_entry: How did you come here? \n"));		
			ldap_unbind(ldap_struct);
			return False;
			break;
		}
	}
	
	ldap_mods_free(mods, 1);
	
	ldap_unbind(ldap_struct);
	
	return True;
}

/************************************************************************
 Add or modify an entry. everything except the smb struct

*************************************************************************/
static BOOL modadd_ldap21pwd_entry(struct sam_passwd *newpwd, int flag)
{
	
	/* assume the struct is correct and filled
	   that's the job of passdb.c to check */
	int scope = LDAP_SCOPE_ONELEVEL;
	int rc;
	char *smb_name;
	int trust = False;
	int ldap_state;
	pstring filter;
	pstring dn;
	pstring lmhash;
	pstring nthash;
	pstring rid;
	pstring lst;
	pstring temp;

	LDAP *ldap_struct;
	LDAPMessage *result;
	LDAPMod **mods;
	
	smb_name = newpwd->smb_name;

	if (!ldap_open_connection(&ldap_struct)) /* open a connection to the server */
	{
		return False;
	}

	if (!ldap_connect_system(ldap_struct)) /* connect as system account */
	{
		ldap_unbind(ldap_struct);
		return False;
	}
	
	if (smb_name[strlen(smb_name)-1] == '$' )
	{
		smb_name[strlen(smb_name)-1] = '\0';
		trust = True;
	}

	slprintf(filter, sizeof(filter)-1,
	         "(&(cn = %s)(|(objectclass = sambaTrust)(objectclass = sambaAccount)))",
	         smb_name);
	
	rc = ldap_search_s(ldap_struct, lp_ldap_suffix(), scope, filter, NULL, 0, &result);

	switch (flag)
	{
		case ADD_USER:
		{
			if (ldap_count_entries(ldap_struct, result) ! = 1)
			{
				DEBUG(2,("User already in the base, with samba properties\n"));		
				ldap_unbind(ldap_struct);
				return False;
			}
			ldap_state = LDAP_MOD_ADD;
			break;
		}

		case MODIFY_USER:
		{
			if (ldap_count_entries(ldap_struct, result) ! = 1)
			{
				DEBUG(2,("No user to modify !\n"));		
				ldap_unbind(ldap_struct);
				return False;
			}
			ldap_state = LDAP_MOD_REPLACE;
			break;
		}

		default:
		{
			DEBUG(2,("How did you come here? \n"));		
			ldap_unbind(ldap_struct);
			return False;
			break;
		}
	}
	slprintf(dn, sizeof(dn)-1, "cn = %s, %s",smb_name, lp_ldap_suffix() );
	
	mods = NULL; 

	if (trust)
	{
	}
	else
	{
	}
	
	make_a_mod(&mods, ldap_state, "cn", smb_name);
	
	make_a_mod(&mods, ldap_state, "rid", rid);
	make_a_mod(&mods, ldap_state, "pwdLastSet", lst);
	make_a_mod(&mods, ldap_state, "userAccountControl", pdb_encode_acct_ctrl(newpwd->acct_ctrl,NEW_PW_FORMAT_SPACE_PADDED_LEN));
	
	ldap_modify_s(ldap_struct, dn, mods);
	
	ldap_mods_free(mods, 1);
	
	ldap_unbind(ldap_struct);
	
	return True;
}

/************************************************************************
 Routine to add an entry to the ldap passwd file.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static BOOL add_ldappwd_entry(struct smb_passwd *newpwd)
{
	return (modadd_ldappwd_entry(newpwd, ADD_USER) );
}

/************************************************************************
 Routine to search the ldap passwd file for an entry matching the username.
 and then modify its password entry. We can't use the startldappwent()/
 getldappwent()/endldappwent() interfaces here as we depend on looking
 in the actual file to decide how much room we have to write data.
 override = False, normal
 override = True, override XXXXXXXX'd out password or NO PASS

 do not call this function directly.  use passdb.c instead.

************************************************************************/
static BOOL mod_ldappwd_entry(struct smb_passwd *pwd, BOOL override)
{
	return (modadd_ldappwd_entry(pwd, MODIFY_USER) );
}

/************************************************************************
 Routine to add an entry to the ldap passwd file.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static BOOL add_ldap21pwd_entry(struct sam_passwd *newpwd)
{	
	return( modadd_ldappwd_entry(newpwd, ADD_USER)?
	modadd_ldap21pwd_entry(newpwd, ADD_USER):False);
}

/************************************************************************
 Routine to search the ldap passwd file for an entry matching the username.
 and then modify its password entry. We can't use the startldappwent()/
 getldappwent()/endldappwent() interfaces here as we depend on looking
 in the actual file to decide how much room we have to write data.
 override = False, normal
 override = True, override XXXXXXXX'd out password or NO PASS

 do not call this function directly.  use passdb.c instead.

************************************************************************/
static BOOL mod_ldap21pwd_entry(struct sam_passwd *pwd, BOOL override)
{
	return( modadd_ldappwd_entry(pwd, MODIFY_USER)?
	modadd_ldap21pwd_entry(pwd, MODIFY_USER):False);
}

struct ldap_enum_info
{
	LDAP *ldap_struct;
	LDAPMessage *result;
	LDAPMessage *entry;
};

static struct ldap_enum_info ldap_ent;

/***************************************************************
 Start to enumerate the ldap passwd list. Returns a void pointer
 to ensure no modification outside this module.

 do not call this function directly.  use passdb.c instead.

 ****************************************************************/
static void *startldappwent(BOOL update)
{
	int scope = LDAP_SCOPE_ONELEVEL;
	int rc;

	pstring filter;

	if (!ldap_open_connection(&ldap_ent.ldap_struct)) /* open a connection to the server */
	{
		return NULL;
	}

	if (!ldap_connect_system(ldap_ent.ldap_struct)) /* connect as system account */
	{
		return NULL;
	}

	/* when the class is known the search is much faster */
	switch (0)
	{
		case 1:
		{
			pstrcpy(filter, "objectclass = sambaAccount");
			break;
		}
		case 2:
		{
			pstrcpy(filter, "objectclass = sambaTrust");
			break;
		}
		default:
		{
			pstrcpy(filter, "(|(objectclass = sambaTrust)(objectclass = sambaAccount))");
			break;
		}
	}

	rc = ldap_search_s(ldap_ent.ldap_struct, lp_ldap_suffix(), scope, filter, NULL, 0, &ldap_ent.result);

	DEBUG(2,("%d entries in the base!\n", ldap_count_entries(ldap_ent.ldap_struct, ldap_ent.result) ));

  	ldap_ent.entry = ldap_first_entry(ldap_ent.ldap_struct, ldap_ent.result);

	return &ldap_ent;
}

/*************************************************************************
 Routine to return the next entry in the ldap passwd list.

 do not call this function directly.  use passdb.c instead.

 *************************************************************************/
static struct smb_passwd *getldappwent(void *vp)
{
	static struct smb_passwd user;
	struct ldap_enum_info *ldap_vp = (struct ldap_enum_info *)vp;

	ldap_vp->entry = ldap_next_entry(ldap_vp->ldap_struct, ldap_vp->entry);

	if (ldap_vp->entry ! = NULL)
	{
		ldap_get_smb_passwd(ldap_vp->ldap_struct, ldap_vp->entry, &user);
		return &user;
	}
	return NULL;
}

/*************************************************************************
 Routine to return the next entry in the ldap passwd list.

 do not call this function directly.  use passdb.c instead.

 *************************************************************************/
static struct sam_passwd *getldap21pwent(void *vp)
{
	static struct sam_passwd user;
	struct ldap_enum_info *ldap_vp = (struct ldap_enum_info *)vp;

	ldap_vp->entry = ldap_next_entry(ldap_vp->ldap_struct, ldap_vp->entry);

	if (ldap_vp->entry ! = NULL)
	{
		ldap_get_sam_passwd(ldap_vp->ldap_struct, ldap_vp->entry, &user);
		return &user;
	}
	return NULL;
}

/***************************************************************
 End enumeration of the ldap passwd list.

 do not call this function directly.  use passdb.c instead.

****************************************************************/
static void endldappwent(void *vp)
{
	struct ldap_enum_info *ldap_vp = (struct ldap_enum_info *)vp;
	ldap_msgfree(ldap_vp->result);
	ldap_unbind(ldap_vp->ldap_struct);
}

/*************************************************************************
 Return the current position in the ldap passwd list as an SMB_BIG_UINT.
 This must be treated as an opaque token.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static SMB_BIG_UINT getldappwpos(void *vp)
{
	return (SMB_BIG_UINT)0;
}

/*************************************************************************
 Set the current position in the ldap passwd list from SMB_BIG_UINT.
 This must be treated as an opaque token.

 do not call this function directly.  use passdb.c instead.

*************************************************************************/
static BOOL setldappwpos(void *vp, SMB_BIG_UINT tok)
{
	return False;
}

/*
 * Ldap derived functions.
 */

static struct smb_passwd *getldappwnam(char *name)
{
  return pdb_sam_to_smb(iterate_getsam21pwnam(name));
}

static struct smb_passwd *getldappwuid(uid_t smb_userid)
{
  return pdb_sam_to_smb(iterate_getsam21pwuid(smb_userid));
}

static struct smb_passwd *getldappwrid(uint32 user_rid)
{
  return pdb_sam_to_smb(iterate_getsam21pwuid(pdb_user_rid_to_uid(user_rid)));
}

static struct smb_passwd *getldappwent(void *vp)
{
  return pdb_sam_to_smb(getldap21pwent(vp));
}

static BOOL add_ldappwd_entry(struct smb_passwd *newpwd)
{
  return add_ldap21pwd_entry(pdb_smb_to_sam(newpwd));
}

static BOOL mod_ldappwd_entry(struct smb_passwd* pwd, BOOL override)
{
  return mod_ldap21pwd_entry(pdb_smb_to_sam(pwd), override);
}

static BOOL del_ldappwd_entry(const char *name)
{
  return False; /* Dummy... */
}

static struct sam_disp_info *getldapdispnam(char *name)
{
	return pdb_sam_to_dispinfo(getldap21pwnam(name));
}

static struct sam_disp_info *getldapdisprid(uint32 rid)
{
	return pdb_sam_to_dispinfo(getldap21pwrid(rid));
}

static struct sam_disp_info *getldapdispent(void *vp)
{
	return pdb_sam_to_dispinfo(getldap21pwent(vp));
}

static struct sam_passwd *getldap21pwuid(uid_t uid)
{
	return pdb_smb_to_sam(iterate_getsam21pwuid(pdb_uid_to_user_rid(uid)));
}

static struct passdb_ops ldap_ops =
{
	startldappwent,
	endldappwent,
	getldappwpos,
	setldappwpos,
	getldappwnam,
	getldappwuid,
	getldappwrid,
	getldappwent,
	add_ldappwd_entry,
	mod_ldappwd_entry,
	del_ldappwd_entry,
	getldap21pwent,
	iterate_getsam21pwnam,       /* From passdb.c */
	iterate_getsam21pwuid,       /* From passdb.c */
	iterate_getsam21pwrid,       /* From passdb.c */
	add_ldap21pwd_entry,
	mod_ldap21pwd_entry,
	getldapdispnam,
	getldapdisprid,
	getldapdispent
};

struct passdb_ops *ldap_initialize_password_db(void)
{
  return &ldap_ops;
}

#else
 void dummy_function(void);
 void dummy_function(void) { } /* stop some compilers complaining */
#endif
