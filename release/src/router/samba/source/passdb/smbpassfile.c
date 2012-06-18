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

extern int DEBUGLEVEL;

BOOL global_machine_password_needs_changing = False;

/***************************************************************
 Lock an fd. Abandon after waitsecs seconds.
****************************************************************/

BOOL pw_file_lock(int fd, int type, int secs, int *plock_depth)
{
  if (fd < 0)
    return False;

  if(*plock_depth == 0) {
    if (!do_file_lock(fd, secs, type)) {
      DEBUG(10,("pw_file_lock: locking file failed, error = %s.\n",
                 strerror(errno)));
      return False;
    }
  }

  (*plock_depth)++;

  return True;
}

/***************************************************************
 Unlock an fd. Abandon after waitsecs seconds.
****************************************************************/

BOOL pw_file_unlock(int fd, int *plock_depth)
{
  BOOL ret=True;

  if(*plock_depth == 1)
    ret = do_file_lock(fd, 5, F_UNLCK);

  if (*plock_depth > 0)
    (*plock_depth)--;

  if(!ret)
    DEBUG(10,("pw_file_unlock: unlocking file failed, error = %s.\n",
                 strerror(errno)));
  return ret;
}

static int mach_passwd_lock_depth;
static FILE *mach_passwd_fp;

/************************************************************************
 Routine to get the name for a trust account file.
************************************************************************/

static void get_trust_account_file_name( char *domain, char *name, char *mac_file)
{
  unsigned int mac_file_len;
  char *p;

  pstrcpy(mac_file, lp_smb_passwd_file());
  p = strrchr(mac_file, '/');
  if(p != NULL)
    *++p = '\0';

  mac_file_len = strlen(mac_file);

  if ((int)(sizeof(pstring) - mac_file_len - strlen(domain) - strlen(name) - 6) < 0)
  {
    DEBUG(0,("trust_password_lock: path %s too long to add trust details.\n",
              mac_file));
    return;
  }

  pstrcat(mac_file, domain);
  pstrcat(mac_file, ".");
  pstrcat(mac_file, name);
  pstrcat(mac_file, ".mac");
}
 
/************************************************************************
 Routine to lock the trust account password file for a domain.
************************************************************************/

BOOL trust_password_lock( char *domain, char *name, BOOL update)
{
  pstring mac_file;

  if(mach_passwd_lock_depth == 0) {

    get_trust_account_file_name( domain, name, mac_file);

    if((mach_passwd_fp = sys_fopen(mac_file, "r+b")) == NULL) {
      if(errno == ENOENT && update) {
        mach_passwd_fp = sys_fopen(mac_file, "w+b");
      }

      if(mach_passwd_fp == NULL) {
        DEBUG(0,("trust_password_lock: cannot open file %s - Error was %s.\n",
              mac_file, strerror(errno) ));
        return False;
      }
    }

    chmod(mac_file, 0600);

    if(!pw_file_lock(fileno(mach_passwd_fp), (update ? F_WRLCK : F_RDLCK), 
                                      60, &mach_passwd_lock_depth))
    {
      DEBUG(0,("trust_password_lock: cannot lock file %s\n", mac_file));
      fclose(mach_passwd_fp);
      return False;
    }

  }

  return True;
}

/************************************************************************
 Routine to unlock the trust account password file for a domain.
************************************************************************/

BOOL trust_password_unlock(void)
{
  BOOL ret = pw_file_unlock(fileno(mach_passwd_fp), &mach_passwd_lock_depth);
  if(mach_passwd_lock_depth == 0)
    fclose(mach_passwd_fp);
  return ret;
}

/************************************************************************
 Routine to delete the trust account password file for a domain.
************************************************************************/

BOOL trust_password_delete( char *domain, char *name )
{
  pstring mac_file;

  get_trust_account_file_name( domain, name, mac_file);
  return (unlink( mac_file ) == 0);
}

/************************************************************************
 Routine to get the trust account password for a domain.
 The user of this function must have locked the trust password file.
************************************************************************/

BOOL get_trust_account_password( unsigned char *ret_pwd, time_t *pass_last_set_time)
{
  char linebuf[256];
  char *p;
  int i;

  linebuf[0] = '\0';

  *pass_last_set_time = (time_t)0;
  memset(ret_pwd, '\0', 16);

  if(sys_fseek( mach_passwd_fp, (SMB_OFF_T)0, SEEK_SET) == -1) {
    DEBUG(0,("get_trust_account_password: Failed to seek to start of file. Error was %s.\n",
              strerror(errno) ));
    return False;
  } 

  fgets(linebuf, sizeof(linebuf), mach_passwd_fp);
  if(ferror(mach_passwd_fp)) {
    DEBUG(0,("get_trust_account_password: Failed to read password. Error was %s.\n",
              strerror(errno) ));
    return False;
  }

  if(linebuf[strlen(linebuf)-1] == '\n')
    linebuf[strlen(linebuf)-1] = '\0';

  /*
   * The length of the line read
   * must be 45 bytes ( <---XXXX 32 bytes-->:TLC-12345678
   */

  if(strlen(linebuf) != 45) {
    DEBUG(0,("get_trust_account_password: Malformed trust password file (wrong length \
- was %d, should be 45).\n", (int)strlen(linebuf)));
#ifdef DEBUG_PASSWORD
    DEBUG(100,("get_trust_account_password: line = |%s|\n", linebuf));
#endif
    return False;
  }

  /*
   * Get the hex password.
   */

  if (!pdb_gethexpwd((char *)linebuf, ret_pwd) || linebuf[32] != ':' || 
         strncmp(&linebuf[33], "TLC-", 4)) {
    DEBUG(0,("get_trust_account_password: Malformed trust password file (incorrect format).\n"));
#ifdef DEBUG_PASSWORD
    DEBUG(100,("get_trust_account_password: line = |%s|\n", linebuf));
#endif
    return False;
  }

  /*
   * Get the last changed time.
   */
  p = &linebuf[37];

  for(i = 0; i < 8; i++) {
    if(p[i] == '\0' || !isxdigit((int)p[i])) {
      DEBUG(0,("get_trust_account_password: Malformed trust password file (no timestamp).\n"));
#ifdef DEBUG_PASSWORD
      DEBUG(100,("get_trust_account_password: line = |%s|\n", linebuf));
#endif
      return False;
    }
  }

  /*
   * p points at 8 characters of hex digits -
   * read into a time_t as the seconds since
   * 1970 that the password was last changed.
   */

  *pass_last_set_time = (time_t)strtol(p, NULL, 16);

  return True;
}

/************************************************************************
 Routine to get the trust account password for a domain.
 The user of this function must have locked the trust password file.
************************************************************************/

BOOL set_trust_account_password( unsigned char *md4_new_pwd)
{
  char linebuf[64];
  int i;

  if(sys_fseek( mach_passwd_fp, (SMB_OFF_T)0, SEEK_SET) == -1) {
    DEBUG(0,("set_trust_account_password: Failed to seek to start of file. Error was %s.\n",
              strerror(errno) ));
    return False;
  } 

  for (i = 0; i < 16; i++)
    slprintf(&linebuf[(i*2)], sizeof(linebuf) -  (i*2) - 1, "%02X", md4_new_pwd[i]);

  slprintf(&linebuf[32], 32, ":TLC-%08X\n", (unsigned)time(NULL));

  if(fwrite( linebuf, 1, 46, mach_passwd_fp)!= 46) {
    DEBUG(0,("set_trust_account_password: Failed to write file. Warning - the trust \
account is now invalid. Please recreate. Error was %s.\n", strerror(errno) ));
    return False;
  }

  fflush(mach_passwd_fp);
  return True;
}

BOOL trust_get_passwd( unsigned char trust_passwd[16], char *domain, char *myname)
{
  time_t lct;

  /*
   * Get the machine account password.
   */
  if(!trust_password_lock( domain, myname, False)) {
    DEBUG(0,("domain_client_validate: unable to open the machine account password file for \
machine %s in domain %s.\n", myname, domain ));
    return False;
  }

  if(get_trust_account_password( trust_passwd, &lct) == False) {
    DEBUG(0,("domain_client_validate: unable to read the machine account password for \
machine %s in domain %s.\n", myname, domain ));
    trust_password_unlock();
    return False;
  }

  trust_password_unlock();

  /* 
   * Here we check the last change time to see if the machine
   * password needs changing. JRA. 
   */

  if(time(NULL) > lct + lp_machine_password_timeout())
  {
    global_machine_password_needs_changing = True;
  }
  return True;
}
