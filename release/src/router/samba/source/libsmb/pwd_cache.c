/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Password cacheing.  obfuscation is planned
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

extern int DEBUGLEVEL;


/****************************************************************************
initialises a password structure
****************************************************************************/
void pwd_init(struct pwd_info *pwd)
{
	memset((char *)pwd->password  , '\0', sizeof(pwd->password  ));
	memset((char *)pwd->smb_lm_pwd, '\0', sizeof(pwd->smb_lm_pwd));
	memset((char *)pwd->smb_nt_pwd, '\0', sizeof(pwd->smb_nt_pwd));
	memset((char *)pwd->smb_lm_owf, '\0', sizeof(pwd->smb_lm_owf));
	memset((char *)pwd->smb_nt_owf, '\0', sizeof(pwd->smb_nt_owf));

	pwd->null_pwd  = True; /* safest option... */
	pwd->cleartext = False;
	pwd->crypted   = False;
}

/****************************************************************************
de-obfuscates a password
****************************************************************************/
static void pwd_deobfuscate(struct pwd_info *pwd)
{
}

/****************************************************************************
obfuscates a password
****************************************************************************/
static void pwd_obfuscate(struct pwd_info *pwd)
{
}

/****************************************************************************
sets the obfuscation key info
****************************************************************************/
void pwd_obfuscate_key(struct pwd_info *pwd, uint32 int_key, char *str_key)
{
}

/****************************************************************************
reads a password
****************************************************************************/
void pwd_read(struct pwd_info *pwd, char *passwd_report, BOOL do_encrypt)
{
	/* grab a password */
	char *user_pass;

	pwd_init(pwd);

	user_pass = (char*)getpass(passwd_report);

	if (user_pass == NULL || user_pass[0] == 0)
	{
		pwd_set_nullpwd(pwd);
	}
	else if (do_encrypt)
	{
		pwd_make_lm_nt_16(pwd, user_pass);
	}
	else
	{
		pwd_set_cleartext(pwd, user_pass);
	}
}

/****************************************************************************
 stores a cleartext password
 ****************************************************************************/
void pwd_set_nullpwd(struct pwd_info *pwd)
{
	pwd_init(pwd);

	pwd->cleartext = False;
	pwd->null_pwd  = True;
	pwd->crypted   = False;
}

/****************************************************************************
 stores a cleartext password
 ****************************************************************************/
void pwd_set_cleartext(struct pwd_info *pwd, char *clr)
{
	pwd_init(pwd);
	fstrcpy(pwd->password, clr);
	unix_to_dos(pwd->password,True);
	pwd->cleartext = True;
	pwd->null_pwd  = False;
	pwd->crypted   = False;

	pwd_obfuscate(pwd);
}

/****************************************************************************
 gets a cleartext password
 ****************************************************************************/
void pwd_get_cleartext(struct pwd_info *pwd, char *clr)
{
	pwd_deobfuscate(pwd);
	if (pwd->cleartext)
	{
		fstrcpy(clr, pwd->password);
		dos_to_unix(clr, True);
	}
	else
	{
		clr[0] = 0;
	}
	pwd_obfuscate(pwd);
}

/****************************************************************************
 stores lm and nt hashed passwords
 ****************************************************************************/
void pwd_set_lm_nt_16(struct pwd_info *pwd, uchar lm_pwd[16], uchar nt_pwd[16])
{
	pwd_init(pwd);

	if (lm_pwd)
	{
		memcpy(pwd->smb_lm_pwd, lm_pwd, 16);
	}
	else
	{
		memset((char *)pwd->smb_lm_pwd, '\0', 16);
	}

	if (nt_pwd)
	{
		memcpy(pwd->smb_nt_pwd, nt_pwd, 16);
	}
	else
	{
		memset((char *)pwd->smb_nt_pwd, '\0', 16);
	}

	pwd->null_pwd  = False;
	pwd->cleartext = False;
	pwd->crypted   = False;

	pwd_obfuscate(pwd);
}

/****************************************************************************
 gets lm and nt hashed passwords
 ****************************************************************************/
void pwd_get_lm_nt_16(struct pwd_info *pwd, uchar lm_pwd[16], uchar nt_pwd[16])
{
	pwd_deobfuscate(pwd);
	if (lm_pwd != NULL)
	{
		memcpy(lm_pwd, pwd->smb_lm_pwd, 16);
	}
	if (nt_pwd != NULL)
	{
		memcpy(nt_pwd, pwd->smb_nt_pwd, 16);
	}
	pwd_obfuscate(pwd);
}

/****************************************************************************
 makes lm and nt hashed passwords
 ****************************************************************************/
void pwd_make_lm_nt_16(struct pwd_info *pwd, char *clr)
{
	pstring dos_passwd;

	pwd_init(pwd);

	pstrcpy(dos_passwd, clr);
	unix_to_dos(dos_passwd, True);

	nt_lm_owf_gen(dos_passwd, pwd->smb_nt_pwd, pwd->smb_lm_pwd);
	pwd->null_pwd  = False;
	pwd->cleartext = False;
	pwd->crypted = False;

	pwd_obfuscate(pwd);
}

/****************************************************************************
 makes lm and nt OWF crypts
 ****************************************************************************/
void pwd_make_lm_nt_owf(struct pwd_info *pwd, uchar cryptkey[8])
{
	pwd_deobfuscate(pwd);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("client cryptkey: "));
	dump_data(100, (char *)cryptkey, 8);
#endif

	SMBOWFencrypt(pwd->smb_nt_pwd, cryptkey, pwd->smb_nt_owf);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("nt_owf_passwd: "));
	dump_data(100, (char *)pwd->smb_nt_owf, sizeof(pwd->smb_nt_owf));
	DEBUG(100,("nt_sess_pwd: "));
	dump_data(100, (char *)pwd->smb_nt_pwd, sizeof(pwd->smb_nt_pwd));
#endif

	SMBOWFencrypt(pwd->smb_lm_pwd, cryptkey, pwd->smb_lm_owf);

#ifdef DEBUG_PASSWORD
	DEBUG(100,("lm_owf_passwd: "));
	dump_data(100, (char *)pwd->smb_lm_owf, sizeof(pwd->smb_lm_owf));
	DEBUG(100,("lm_sess_pwd: "));
	dump_data(100, (char *)pwd->smb_lm_pwd, sizeof(pwd->smb_lm_pwd));
#endif

	pwd->crypted = True;

	pwd_obfuscate(pwd);
}

/****************************************************************************
 gets lm and nt crypts
 ****************************************************************************/
void pwd_get_lm_nt_owf(struct pwd_info *pwd, uchar lm_owf[24], uchar nt_owf[24])
{
	pwd_deobfuscate(pwd);
	if (lm_owf != NULL)
	{
		memcpy(lm_owf, pwd->smb_lm_owf, 24);
	}
	if (nt_owf != NULL)
	{
		memcpy(nt_owf, pwd->smb_nt_owf, 24);
	}
	pwd_obfuscate(pwd);
}
