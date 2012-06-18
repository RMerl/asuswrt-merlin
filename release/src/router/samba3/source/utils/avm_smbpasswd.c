/*
 * Unix SMB/CIFS implementation. 
 * Copyright (C) Jeremy Allison 1995-1998
 * Copyright (C) Tim Potter     2001
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.  */

#include "includes.h"


void E_md4hash(const char *passwd, uchar p16[16])
{
	int len;
	smb_ucs2_t wpwd[129];
	int i;

	
	/* Password must be converted to NT unicode - null terminated. */
	len = strlen(passwd);
#if 0
	push_ucs2(NULL, wpwd, (const char *)passwd, 256, STR_UNICODE|STR_NOALIGN|STR_TERMINATE);
#else
	for (i = 0; i < len; i++) {
		wpwd[i] = (unsigned char)passwd[i];
	}
	wpwd[i] = 0; // termination
#endif
	/* Calculate length in bytes */
	len = len /*strlen_w(wpwd)*/ * sizeof(int16);

	mdfour(p16, (unsigned char *)wpwd, len);
	ZERO_STRUCT(wpwd);	
}

/**
 * Creates the DES forward-only Hash of the users password in DOS ASCII charset
 * @param passwd password in 'unix' charset.
 * @param p16 return password hashed with DES, caller allocated 16 byte buffer
 * @return False if password was > 14 characters, and therefore may be incorrect, otherwise True
 * @note p16 is filled in regardless
 */
 
BOOL E_deshash(const char *passwd, uchar p16[16])
{
	BOOL ret = True;
	char dospwd[256+2];
	int i;
	int len;
	
	/* Password must be converted to DOS charset - null terminated, uppercase. */
//	push_ascii(dospwd, passwd, sizeof(dospwd), STR_UPPER|STR_TERMINATE);
	len = strlen(passwd);
	for (i = 0; i < len; i++) {
		char c = passwd[i];
		if (islower(c)) c = toupper(c);
		dospwd[i] = c;
	}
	dospwd[i] = 0;
       
	/* Only the fisrt 14 chars are considered, password need not be null terminated. */
	E_P16((const unsigned char *)dospwd, p16);

	if (strlen(dospwd) > 14) {
		ret = False;
	}

	memset(dospwd, 0, sizeof(dospwd));
	// ZERO_STRUCT(dospwd);	

	return ret;
}

static void my_pdb_sethexpwd(char *p, const unsigned char *pwd)
{
	if (pwd != NULL) {
		int i;
		for (i = 0; i < 16; i++)
			slprintf(&p[i*2], 3, "%02X", pwd[i]);
	} else {
		strncpy(p, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", 32);
	}
}

static void crypt_password (const char *user_name,
			   const char *new_passwd, char *new_lanman_p16, char *new_nt_p16)
{
	/* Calculate the MD4 hash (NT compatible) of the password */
	E_md4hash(new_passwd, new_nt_p16);

	if (!E_deshash(new_passwd, new_lanman_p16)) {
		/* E_deshash returns false for 'long' passwords (> 14
		   DOS chars).  This allows us to match Win2k, which
		   does not store a LM hash for these passwords (which
		   would reduce the effective password length to 14 */

		memset(new_lanman_p16, 0, LM_HASH_LEN);
	}
}

/*
ftpuser:1000:8C6F5D02DEB21501AAD3B435B51404EE:E0FBA38268D0EC66EF1CB452D5885E53:[UX         ]:LCT-00000000:
*/

/*********************************************************
 Start here.
**********************************************************/
int main(int argc, char **argv)
{	
// not for freetz: char *passwd_filename = "/var/samba/private/smbpasswd";
	char *passwd_filename = "/mod/etc/smbpasswd";
	char *cleartext_filename = "/var/tmp/smbpasswd.cleartext";

	if (argc != 1) {
		fprintf(stderr, "use: smbpasswd\n");
		fprintf(stderr, "  file %s will be encrypted to %s\n", cleartext_filename, passwd_filename);
		return -9;
	}

	FILE *fp = fopen(passwd_filename, "w");

	if (fp == NULL) {
		fprintf(stderr, "can't write %s\n", passwd_filename);
		return -10;
	}
	/* Make sure it is only rw by the owner */
	chmod(passwd_filename, 0600);


	FILE *fp_in = fopen(cleartext_filename, "r");
	if (!fp_in) {
		fprintf(stderr, "can't read %s\n", cleartext_filename);
		fclose(fp);
		return -11;
	}


	char line[512];

	unsigned nusers = 0;
	while(line == fgets(line, sizeof(line)-1, fp_in)) {
		char *username, *passwd, *extra;
		unsigned uid;
		uchar new_lanman_p16[LM_HASH_LEN];
		uchar new_nt_p16[NT_HASH_LEN];
		char ascii_p16[32+1];
		char *p;
			
		line[sizeof(line)-1] = '\0';
		if (strlen(line)) {
			p = &line[strlen(line)-1];
			while(p >= line) {
				if (*p != '\n' && *p != '\r') break;
				*p = '\0';
				p--;
			}
		}

		p = line;
		char *p2 = strchr(p, ':');
		if (!p2) goto err;
		*p2 = 0;
		username = p;

		p = p2 + 1;
		p2 = strchr(p, ':');
		if (!p2) goto err;
		*p2 = 0;
		uid = atoi(p);

		p = p2 + 1;
		p2 = strchr(p, ':');
		if (!p2) goto err;
		*p2 = 0;
		passwd = p;

		extra = p2 + 1;

		crypt_password(username, passwd, new_lanman_p16, new_nt_p16);

		fprintf(fp, "%s:%u:", username, uid);

		my_pdb_sethexpwd(ascii_p16, new_lanman_p16);
		ascii_p16[32] = '\0';
		fprintf(fp, "%s:", ascii_p16);

		my_pdb_sethexpwd(ascii_p16, new_nt_p16);
		ascii_p16[32] = '\0';
		fprintf(fp, "%s:", ascii_p16);

		fprintf(fp, "%s\n", extra);

		nusers++;
	} // while

err:
	fclose(fp_in);
	fclose(fp);

fprintf(stderr, "%u samba users written to %s\n", nusers, passwd_filename);
	return 0;
}
