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

extern int DEBUGLEVEL;
extern pstring samlogon_user;
extern BOOL sam_logon_in_ssb;

static int pw_file_lock_depth;

enum pwf_access_type { PWF_READ, PWF_UPDATE, PWF_CREATE };

/***************************************************************
 Internal fn to enumerate the smbpasswd list. Returns a void pointer
 to ensure no modification outside this module. Checks for atomic
 rename of smbpasswd file on update or create once the lock has
 been granted to prevent race conditions. JRA.
****************************************************************/

static void *startsmbfilepwent_internal(const char *pfile, enum pwf_access_type type, int *lock_depth)
{
  FILE *fp = NULL;
  const char *open_mode = NULL;
  int race_loop = 0;
  int lock_type;

  if (!*pfile) {
    DEBUG(0, ("startsmbfilepwent: No SMB password file set\n"));
    return (NULL);
  }

  switch(type) {
  case PWF_READ:
    open_mode = "rb";
    lock_type = F_RDLCK;
    break;
  case PWF_UPDATE:
    open_mode = "r+b";
    lock_type = F_WRLCK;
    break;
  case PWF_CREATE:
    /*
     * Ensure atomic file creation.
     */
    {
      int i, fd = -1;

      for(i = 0; i < 5; i++) {
        if((fd = sys_open(pfile, O_CREAT|O_TRUNC|O_EXCL|O_RDWR, 0600))!=-1)
          break;
        sys_usleep(200); /* Spin, spin... */
      }
      if(fd == -1) {
        DEBUG(0,("startsmbfilepwent_internal: too many race conditions creating file %s\n", pfile));
        return NULL;
      }
      close(fd);
      open_mode = "r+b";
      lock_type = F_WRLCK;
      break;
    }
  }
		       
  for(race_loop = 0; race_loop < 5; race_loop++) {
    DEBUG(10, ("startsmbfilepwent_internal: opening file %s\n", pfile));

    if((fp = sys_fopen(pfile, open_mode)) == NULL) {
      DEBUG(0, ("startsmbfilepwent_internal: unable to open file %s. Error was %s\n", pfile, strerror(errno) ));
      return NULL;
    }

    if (!pw_file_lock(fileno(fp), lock_type, 5, lock_depth)) {
      DEBUG(0, ("startsmbfilepwent_internal: unable to lock file %s. Error was %s\n", pfile, strerror(errno) ));
      fclose(fp);
      return NULL;
    }

    /*
     * Only check for replacement races on update or create.
     * For read we don't mind if the data is one record out of date.
     */

    if(type == PWF_READ) {
      break;
    } else {
      SMB_STRUCT_STAT sbuf1, sbuf2;

      /*
       * Avoid the potential race condition between the open and the lock
       * by doing a stat on the filename and an fstat on the fd. If the
       * two inodes differ then someone did a rename between the open and
       * the lock. Back off and try the open again. Only do this 5 times to
       * prevent infinate loops. JRA.
       */

      if (sys_stat(pfile,&sbuf1) != 0) {
        DEBUG(0, ("startsmbfilepwent_internal: unable to stat file %s. Error was %s\n", pfile, strerror(errno)));
        pw_file_unlock(fileno(fp), lock_depth);
        fclose(fp);
        return NULL;
      }

      if (sys_fstat(fileno(fp),&sbuf2) != 0) {
        DEBUG(0, ("startsmbfilepwent_internal: unable to fstat file %s. Error was %s\n", pfile, strerror(errno)));
        pw_file_unlock(fileno(fp), lock_depth);
        fclose(fp);
        return NULL;
      }

      if( sbuf1.st_ino == sbuf2.st_ino) {
        /* No race. */
        break;
      }

      /*
       * Race occurred - back off and try again...
       */

      pw_file_unlock(fileno(fp), lock_depth);
      fclose(fp);
    }
  }

  if(race_loop == 5) {
    DEBUG(0, ("startsmbfilepwent_internal: too many race conditions opening file %s\n", pfile));
    return NULL;
  }

  /* Set a buffer to do more efficient reads */
  setvbuf(fp, (char *)NULL, _IOFBF, 1024);

  /* Make sure it is only rw by the owner */
  if(fchmod(fileno(fp), S_IRUSR|S_IWUSR) == -1) {
    DEBUG(0, ("startsmbfilepwent_internal: failed to set 0600 permissions on password file %s. \
Error was %s\n.", pfile, strerror(errno) ));
    pw_file_unlock(fileno(fp), lock_depth);
    fclose(fp);
    return NULL;
  }

  /* We have a lock on the file. */
  return (void *)fp;
}

/***************************************************************
 Start to enumerate the smbpasswd list. Returns a void pointer
 to ensure no modification outside this module.
****************************************************************/

static void *startsmbfilepwent(BOOL update)
{
  return startsmbfilepwent_internal(lp_smb_passwd_file(), update ? PWF_UPDATE : PWF_READ, &pw_file_lock_depth);
}

/***************************************************************
 End enumeration of the smbpasswd list.
****************************************************************/

static void endsmbfilepwent_internal(void *vp, int *lock_depth)
{
  FILE *fp = (FILE *)vp;

  pw_file_unlock(fileno(fp), lock_depth);
  fclose(fp);
  DEBUG(7, ("endsmbfilepwent_internal: closed password file.\n"));
}

/***************************************************************
 End enumeration of the smbpasswd list - operate on the default
 lock_depth.
****************************************************************/

static void endsmbfilepwent(void *vp)
{
  endsmbfilepwent_internal(vp, &pw_file_lock_depth);
}

/*************************************************************************
 Routine to return the next entry in the smbpasswd list.
 *************************************************************************/

static struct smb_passwd *getsmbfilepwent(void *vp)
{
  /* Static buffers we will return. */
  static struct smb_passwd pw_buf;
  static pstring  user_name;
  static unsigned char smbpwd[16];
  static unsigned char smbntpwd[16];
  FILE *fp = (FILE *)vp;
  char            linebuf[256];
  unsigned char   c;
  unsigned char  *p;
  long            uidval;
  size_t            linebuf_len;

  if(fp == NULL) {
    DEBUG(0,("getsmbfilepwent: Bad password file pointer.\n"));
    return NULL;
  }

  pdb_init_smb(&pw_buf);

  pw_buf.acct_ctrl = ACB_NORMAL;  

  /*
   * Scan the file, a line at a time and check if the name matches.
   */
  while (!feof(fp)) {
    linebuf[0] = '\0';

    fgets(linebuf, 256, fp);
    if (ferror(fp)) {
      return NULL;
    }

    /*
     * Check if the string is terminated with a newline - if not
     * then we must keep reading and discard until we get one.
     */
    linebuf_len = strlen(linebuf);
    if (linebuf[linebuf_len - 1] != '\n') {
      c = '\0';
      while (!ferror(fp) && !feof(fp)) {
        c = fgetc(fp);
        if (c == '\n')
          break;
      }
    } else
      linebuf[linebuf_len - 1] = '\0';

#ifdef DEBUG_PASSWORD
    DEBUG(100, ("getsmbfilepwent: got line |%s|\n", linebuf));
#endif
    if ((linebuf[0] == 0) && feof(fp)) {
      DEBUG(4, ("getsmbfilepwent: end of file reached\n"));
      break;
    }
    /*
     * The line we have should be of the form :-
     * 
     * username:uid:32hex bytes:[Account type]:LCT-12345678....other flags presently
     * ignored....
     * 
     * or,
     *
     * username:uid:32hex bytes:32hex bytes:[Account type]:LCT-12345678....ignored....
     *
     * if Windows NT compatible passwords are also present.
     * [Account type] is an ascii encoding of the type of account.
     * LCT-(8 hex digits) is the time_t value of the last change time.
     */

    if (linebuf[0] == '#' || linebuf[0] == '\0') {
      DEBUG(6, ("getsmbfilepwent: skipping comment or blank line\n"));
      continue;
    }
    p = (unsigned char *) strchr(linebuf, ':');
    if (p == NULL) {
      DEBUG(0, ("getsmbfilepwent: malformed password entry (no :)\n"));
      continue;
    }
    /*
     * As 256 is shorter than a pstring we don't need to check
     * length here - if this ever changes....
     */
    strncpy(user_name, linebuf, PTR_DIFF(p, linebuf));
    user_name[PTR_DIFF(p, linebuf)] = '\0';

    /* Get smb uid. */

    p++;		/* Go past ':' */

    if(*p == '-') {
      DEBUG(0, ("getsmbfilepwent: uids in the smbpasswd file must not be negative.\n"));
      continue;
    }

    if (!isdigit(*p)) {
      DEBUG(0, ("getsmbfilepwent: malformed password entry (uid not number)\n"));
      continue;
    }

    uidval = atoi((char *) p);

    while (*p && isdigit(*p))
      p++;

    if (*p != ':') {
      DEBUG(0, ("getsmbfilepwent: malformed password entry (no : after uid)\n"));
      continue;
    }

    pw_buf.smb_name = user_name;
    pw_buf.smb_userid = uidval;

    /*
     * Now get the password value - this should be 32 hex digits
     * which are the ascii representations of a 16 byte string.
     * Get two at a time and put them into the password.
     */

    /* Skip the ':' */
    p++;

    if (*p == '*' || *p == 'X') {
      /* Password deliberately invalid - end here. */
      DEBUG(10, ("getsmbfilepwent: entry invalidated for user %s\n", user_name));
      pw_buf.smb_nt_passwd = NULL;
      pw_buf.smb_passwd = NULL;
      pw_buf.acct_ctrl |= ACB_DISABLED;
      return &pw_buf;
    }

    if (linebuf_len < (PTR_DIFF(p, linebuf) + 33)) {
      DEBUG(0, ("getsmbfilepwent: malformed password entry (passwd too short)\n"));
      continue;
    }

    if (p[32] != ':') {
      DEBUG(0, ("getsmbfilepwent: malformed password entry (no terminating :)\n"));
      continue;
    }

    if (!strncasecmp((char *) p, "NO PASSWORD", 11)) {
      pw_buf.smb_passwd = NULL;
      pw_buf.acct_ctrl |= ACB_PWNOTREQ;
    } else {
      if (!pdb_gethexpwd((char *)p, smbpwd)) {
        DEBUG(0, ("getsmbfilepwent: Malformed Lanman password entry (non hex chars)\n"));
        continue;
      }
      pw_buf.smb_passwd = smbpwd;
    }

    /* 
     * Now check if the NT compatible password is
     * available.
     */
    pw_buf.smb_nt_passwd = NULL;

    p += 33; /* Move to the first character of the line after
                the lanman password. */
    if ((linebuf_len >= (PTR_DIFF(p, linebuf) + 33)) && (p[32] == ':')) {
      if (*p != '*' && *p != 'X') {
        if(pdb_gethexpwd((char *)p,smbntpwd))
          pw_buf.smb_nt_passwd = smbntpwd;
      }
      p += 33; /* Move to the first character of the line after
                  the NT password. */
    }

    DEBUG(5,("getsmbfilepwent: returning passwd entry for user %s, uid %ld\n",
	     user_name, uidval));

    if (*p == '[')
	{
      unsigned char *end_p = (unsigned char *)strchr((char *)p, ']');
      pw_buf.acct_ctrl = pdb_decode_acct_ctrl((char*)p);

      /* Must have some account type set. */
      if(pw_buf.acct_ctrl == 0)
        pw_buf.acct_ctrl = ACB_NORMAL;

      /* Now try and get the last change time. */
      if(end_p)
        p = end_p + 1;
      if(*p == ':') {
        p++;
        if(*p && (StrnCaseCmp((char *)p, "LCT-", 4)==0)) {
          int i;
          p += 4;
          for(i = 0; i < 8; i++) {
            if(p[i] == '\0' || !isxdigit(p[i]))
              break;
          }
          if(i == 8) {
            /*
             * p points at 8 characters of hex digits - 
             * read into a time_t as the seconds since
             * 1970 that the password was last changed.
             */
            pw_buf.pass_last_set_time = (time_t)strtol((char *)p, NULL, 16);
          }
        }
      }
    } else {
      /* 'Old' style file. Fake up based on user name. */
      /*
       * Currently trust accounts are kept in the same
       * password file as 'normal accounts'. If this changes
       * we will have to fix this code. JRA.
       */
      if(pw_buf.smb_name[strlen(pw_buf.smb_name) - 1] == '$') {
        pw_buf.acct_ctrl &= ~ACB_NORMAL;
        pw_buf.acct_ctrl |= ACB_WSTRUST;
      }
    }

    return &pw_buf;
  }

  DEBUG(5,("getsmbfilepwent: end of file reached.\n"));
  return NULL;
}

/*************************************************************************
 Routine to return the next entry in the smbpasswd list.
 this function is a nice, messy combination of reading:
 - the smbpasswd file
 - the unix password database
 - smb.conf options (not done at present).
 *************************************************************************/

static struct sam_passwd *getsmbfile21pwent(void *vp)
{
	struct smb_passwd *pw_buf = getsmbfilepwent(vp);
	static struct sam_passwd user;
	struct passwd *pwfile;

	static pstring full_name;
	static pstring home_dir;
	static pstring home_drive;
	static pstring logon_script;
	static pstring profile_path;
	static pstring acct_desc;
	static pstring workstations;
	
	DEBUG(5,("getsmbfile21pwent\n"));

	if (pw_buf == NULL) return NULL;

	pwfile = sys_getpwnam(pw_buf->smb_name);
	if (pwfile == NULL)
	{
		DEBUG(0,("getsmbfile21pwent: smbpasswd database is corrupt!\n"));
		DEBUG(0,("getsmbfile21pwent: username %s not in unix passwd database!\n", pw_buf->smb_name));
		return NULL;
	}

	pdb_init_sam(&user);

	pstrcpy(samlogon_user, pw_buf->smb_name);

	if (samlogon_user[strlen(samlogon_user)-1] != '$')
	{
		/* XXXX hack to get standard_sub_basic() to use sam logon username */
		/* possibly a better way would be to do a become_user() call */
		sam_logon_in_ssb = True;

		user.smb_userid    = pw_buf->smb_userid;
		user.smb_grpid     = pwfile->pw_gid;

		user.user_rid  = pdb_uid_to_user_rid (user.smb_userid);
		user.group_rid = pdb_gid_to_group_rid(user.smb_grpid );

		pstrcpy(full_name    , pwfile->pw_gecos        );
		pstrcpy(logon_script , lp_logon_script       ());
		pstrcpy(profile_path , lp_logon_path         ());
		pstrcpy(home_drive   , lp_logon_drive        ());
		pstrcpy(home_dir     , lp_logon_home         ());
		pstrcpy(acct_desc    , "");
		pstrcpy(workstations , "");

		sam_logon_in_ssb = False;
	}
	else
	{
		user.smb_userid    = pw_buf->smb_userid;
		user.smb_grpid     = pwfile->pw_gid;

		user.user_rid  = pdb_uid_to_user_rid (user.smb_userid);
		user.group_rid = DOMAIN_GROUP_RID_USERS; /* lkclXXXX this is OBSERVED behaviour by NT PDCs, enforced here. */

		pstrcpy(full_name    , "");
		pstrcpy(logon_script , "");
		pstrcpy(profile_path , "");
		pstrcpy(home_drive   , "");
		pstrcpy(home_dir     , "");
		pstrcpy(acct_desc    , "");
		pstrcpy(workstations , "");
	}

	user.smb_name     = pw_buf->smb_name;
	user.full_name    = full_name;
	user.home_dir     = home_dir;
	user.dir_drive    = home_drive;
	user.logon_script = logon_script;
	user.profile_path = profile_path;
	user.acct_desc    = acct_desc;
	user.workstations = workstations;

	user.unknown_str = NULL; /* don't know, yet! */
	user.munged_dial = NULL; /* "munged" dial-back telephone number */

	user.smb_nt_passwd = pw_buf->smb_nt_passwd;
	user.smb_passwd    = pw_buf->smb_passwd;
			
	user.acct_ctrl = pw_buf->acct_ctrl;

	user.unknown_3 = 0xffffff; /* don't know */
	user.logon_divs = 168; /* hours per week */
	user.hours_len = 21; /* 21 times 8 bits = 168 */
	memset(user.hours, 0xff, user.hours_len); /* available at all hours */
	user.unknown_5 = 0x00020000; /* don't know */
	user.unknown_5 = 0x000004ec; /* don't know */

	return &user;
}

/*************************************************************************
 Return the current position in the smbpasswd list as an SMB_BIG_UINT.
 This must be treated as an opaque token.
*************************************************************************/

static SMB_BIG_UINT getsmbfilepwpos(void *vp)
{
  return (SMB_BIG_UINT)sys_ftell((FILE *)vp);
}

/*************************************************************************
 Set the current position in the smbpasswd list from an SMB_BIG_UINT.
 This must be treated as an opaque token.
*************************************************************************/

static BOOL setsmbfilepwpos(void *vp, SMB_BIG_UINT tok)
{
  return !sys_fseek((FILE *)vp, (SMB_OFF_T)tok, SEEK_SET);
}

/************************************************************************
 Create a new smbpasswd entry - malloced space returned.
*************************************************************************/

char *format_new_smbpasswd_entry(struct smb_passwd *newpwd)
{
  int new_entry_length;
  char *new_entry;
  char *p;
  int i;

  new_entry_length = strlen(newpwd->smb_name) + 1 + 15 + 1 + 32 + 1 + 32 + 1 + NEW_PW_FORMAT_SPACE_PADDED_LEN + 1 + 13 + 2;

  if((new_entry = (char *)malloc( new_entry_length )) == NULL) {
    DEBUG(0, ("format_new_smbpasswd_entry: Malloc failed adding entry for user %s.\n", newpwd->smb_name ));
    return NULL;
  }

  slprintf(new_entry, new_entry_length - 1, "%s:%u:", newpwd->smb_name, (unsigned)newpwd->smb_userid);
  p = &new_entry[strlen(new_entry)];

  if(newpwd->smb_passwd != NULL) {
    for( i = 0; i < 16; i++) {
      slprintf((char *)&p[i*2], new_entry_length - (p - new_entry) - 1, "%02X", newpwd->smb_passwd[i]);
    }
  } else {
    i=0;
    if(newpwd->acct_ctrl & ACB_PWNOTREQ)
      safe_strcpy((char *)p, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX", new_entry_length - 1 - (p - new_entry));
    else
      safe_strcpy((char *)p, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", new_entry_length - 1 - (p - new_entry));
  }
  
  p += 32;

  *p++ = ':';

  if(newpwd->smb_nt_passwd != NULL) {
    for( i = 0; i < 16; i++) {
      slprintf((char *)&p[i*2], new_entry_length - 1 - (p - new_entry), "%02X", newpwd->smb_nt_passwd[i]);
    }
  } else {
    if(newpwd->acct_ctrl & ACB_PWNOTREQ)
      safe_strcpy((char *)p, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX", new_entry_length - 1 - (p - new_entry));
    else
      safe_strcpy((char *)p, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", new_entry_length - 1 - (p - new_entry));
  }

  p += 32;

  *p++ = ':';

  /* Add the account encoding and the last change time. */
  slprintf((char *)p, new_entry_length - 1 - (p - new_entry),  "%s:LCT-%08X:\n",
           pdb_encode_acct_ctrl(newpwd->acct_ctrl, NEW_PW_FORMAT_SPACE_PADDED_LEN),
           (uint32)newpwd->pass_last_set_time);

  return new_entry;
}

/************************************************************************
 Routine to add an entry to the smbpasswd file.
*************************************************************************/

static BOOL add_smbfilepwd_entry(struct smb_passwd *newpwd)
{
  char *pfile = lp_smb_passwd_file();
  struct smb_passwd *pwd = NULL;
  FILE *fp = NULL;
  int wr_len;
  int fd;
  size_t new_entry_length;
  char *new_entry;
  SMB_OFF_T offpos;

  /* Open the smbpassword file - for update. */
  fp = startsmbfilepwent(True);

  if (fp == NULL) {
    DEBUG(0, ("add_smbfilepwd_entry: unable to open file.\n"));
    return False;
  }

  /*
   * Scan the file, a line at a time and check if the name matches.
   */

  while ((pwd = getsmbfilepwent(fp)) != NULL) {
    if (strequal(newpwd->smb_name, pwd->smb_name)) {
      DEBUG(0, ("add_smbfilepwd_entry: entry with name %s already exists\n", pwd->smb_name));
      endsmbfilepwent(fp);
      return False;
    }
  }

  /* Ok - entry doesn't exist. We can add it */

  /* Create a new smb passwd entry and set it to the given password. */
  /* 
   * The add user write needs to be atomic - so get the fd from 
   * the fp and do a raw write() call.
   */
  fd = fileno(fp);

  if((offpos = sys_lseek(fd, 0, SEEK_END)) == -1) {
    DEBUG(0, ("add_smbfilepwd_entry(sys_lseek): Failed to add entry for user %s to file %s. \
Error was %s\n", newpwd->smb_name, pfile, strerror(errno)));
    endsmbfilepwent(fp);
    return False;
  }

  if((new_entry = format_new_smbpasswd_entry(newpwd)) == NULL) {
    DEBUG(0, ("add_smbfilepwd_entry(malloc): Failed to add entry for user %s to file %s. \
Error was %s\n", newpwd->smb_name, pfile, strerror(errno)));
    endsmbfilepwent(fp);
    return False;
  }

  new_entry_length = strlen(new_entry);

#ifdef DEBUG_PASSWORD
  DEBUG(100, ("add_smbfilepwd_entry(%d): new_entry_len %d made line |%s|", 
		             fd, new_entry_length, new_entry));
#endif

  if ((wr_len = write(fd, new_entry, new_entry_length)) != new_entry_length) {
    DEBUG(0, ("add_smbfilepwd_entry(write): %d Failed to add entry for user %s to file %s. \
Error was %s\n", wr_len, newpwd->smb_name, pfile, strerror(errno)));

    /* Remove the entry we just wrote. */
    if(sys_ftruncate(fd, offpos) == -1) {
      DEBUG(0, ("add_smbfilepwd_entry: ERROR failed to ftruncate file %s. \
Error was %s. Password file may be corrupt ! Please examine by hand !\n", 
             newpwd->smb_name, strerror(errno)));
    }

    endsmbfilepwent(fp);
    free(new_entry);
    return False;
  }

  free(new_entry);
  endsmbfilepwent(fp);
  return True;
}

/************************************************************************
 Routine to search the smbpasswd file for an entry matching the username.
 and then modify its password entry. We can't use the startsmbpwent()/
 getsmbpwent()/endsmbpwent() interfaces here as we depend on looking
 in the actual file to decide how much room we have to write data.
 override = False, normal
 override = True, override XXXXXXXX'd out password or NO PASS
************************************************************************/

static BOOL mod_smbfilepwd_entry(struct smb_passwd* pwd, BOOL override)
{
  /* Static buffers we will return. */
  static pstring  user_name;

  char            linebuf[256];
  char            readbuf[1024];
  unsigned char   c;
  fstring         ascii_p16;
  fstring         encode_bits;
  unsigned char  *p = NULL;
  size_t            linebuf_len = 0;
  FILE           *fp;
  int             lockfd;
  char           *pfile = lp_smb_passwd_file();
  BOOL found_entry = False;
  BOOL got_pass_last_set_time = False;

  SMB_OFF_T pwd_seekpos = 0;

  int i;
  int wr_len;
  int fd;

  if (!*pfile) {
    DEBUG(0, ("No SMB password file set\n"));
    return False;
  }
  DEBUG(10, ("mod_smbfilepwd_entry: opening file %s\n", pfile));

  fp = sys_fopen(pfile, "r+");

  if (fp == NULL) {
    DEBUG(0, ("mod_smbfilepwd_entry: unable to open file %s\n", pfile));
    return False;
  }
  /* Set a buffer to do more efficient reads */
  setvbuf(fp, readbuf, _IOFBF, sizeof(readbuf));

  lockfd = fileno(fp);

  if (!pw_file_lock(lockfd, F_WRLCK, 5, &pw_file_lock_depth)) {
    DEBUG(0, ("mod_smbfilepwd_entry: unable to lock file %s\n", pfile));
    fclose(fp);
    return False;
  }

  /* Make sure it is only rw by the owner */
  chmod(pfile, 0600);

  /* We have a write lock on the file. */
  /*
   * Scan the file, a line at a time and check if the name matches.
   */
  while (!feof(fp)) {
    pwd_seekpos = sys_ftell(fp);

    linebuf[0] = '\0';

    fgets(linebuf, sizeof(linebuf), fp);
    if (ferror(fp)) {
      pw_file_unlock(lockfd, &pw_file_lock_depth);
      fclose(fp);
      return False;
    }

    /*
     * Check if the string is terminated with a newline - if not
     * then we must keep reading and discard until we get one.
     */
    linebuf_len = strlen(linebuf);
    if (linebuf[linebuf_len - 1] != '\n') {
      c = '\0';
      while (!ferror(fp) && !feof(fp)) {
        c = fgetc(fp);
        if (c == '\n') {
          break;
        }
      }
    } else {
      linebuf[linebuf_len - 1] = '\0';
    }

#ifdef DEBUG_PASSWORD
    DEBUG(100, ("mod_smbfilepwd_entry: got line |%s|\n", linebuf));
#endif

    if ((linebuf[0] == 0) && feof(fp)) {
      DEBUG(4, ("mod_smbfilepwd_entry: end of file reached\n"));
      break;
    }

    /*
     * The line we have should be of the form :-
     * 
     * username:uid:[32hex bytes]:....other flags presently
     * ignored....
     * 
     * or,
     *
     * username:uid:[32hex bytes]:[32hex bytes]:[attributes]:LCT-XXXXXXXX:...ignored.
     *
     * if Windows NT compatible passwords are also present.
     */

    if (linebuf[0] == '#' || linebuf[0] == '\0') {
      DEBUG(6, ("mod_smbfilepwd_entry: skipping comment or blank line\n"));
      continue;
    }

    p = (unsigned char *) strchr(linebuf, ':');

    if (p == NULL) {
      DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (no :)\n"));
      continue;
    }

    /*
     * As 256 is shorter than a pstring we don't need to check
     * length here - if this ever changes....
     */
    strncpy(user_name, linebuf, PTR_DIFF(p, linebuf));
    user_name[PTR_DIFF(p, linebuf)] = '\0';
    if (strequal(user_name, pwd->smb_name)) {
      found_entry = True;
      break;
    }
  }

  if (!found_entry) {
    pw_file_unlock(lockfd, &pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  DEBUG(6, ("mod_smbfilepwd_entry: entry exists\n"));

  /* User name matches - get uid and password */
  p++;		/* Go past ':' */

  if (!isdigit(*p)) {
    DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (uid not number)\n"));
    pw_file_unlock(lockfd, &pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  while (*p && isdigit(*p))
    p++;
  if (*p != ':') {
    DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (no : after uid)\n"));
    pw_file_unlock(lockfd, &pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  /*
   * Now get the password value - this should be 32 hex digits
   * which are the ascii representations of a 16 byte string.
   * Get two at a time and put them into the password.
   */
  p++;

  /* Record exact password position */
  pwd_seekpos += PTR_DIFF(p, linebuf);

  if (!override && (*p == '*' || *p == 'X')) {
    /* Password deliberately invalid - end here. */
    DEBUG(10, ("mod_smbfilepwd_entry: entry invalidated for user %s\n", user_name));
    pw_file_unlock(lockfd, &pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  if (linebuf_len < (PTR_DIFF(p, linebuf) + 33)) {
    DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (passwd too short)\n"));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return (False);
  }

  if (p[32] != ':') {
    DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (no terminating :)\n"));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  if (!override && (*p == '*' || *p == 'X')) {
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  /* Now check if the NT compatible password is
     available. */
  p += 33; /* Move to the first character of the line after
              the lanman password. */
  if (linebuf_len < (PTR_DIFF(p, linebuf) + 33)) {
    DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (passwd too short)\n"));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return (False);
  }

  if (p[32] != ':') {
    DEBUG(0, ("mod_smbfilepwd_entry: malformed password entry (no terminating :)\n"));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  /* 
   * Now check if the account info and the password last
   * change time is available.
   */
  p += 33; /* Move to the first character of the line after
              the NT password. */

  /*
   * If both NT and lanman passwords are provided - reset password
   * not required flag.
   */

  if(pwd->smb_passwd != NULL || pwd->smb_nt_passwd != NULL) {
    /* Reqiure password in the future (should ACB_DISABLED also be reset?) */
    pwd->acct_ctrl &= ~(ACB_PWNOTREQ);
  }

  if (*p == '[') {

    i = 0;
    encode_bits[i++] = *p++;
    while((linebuf_len > PTR_DIFF(p, linebuf)) && (*p != ']'))
      encode_bits[i++] = *p++;

    encode_bits[i++] = ']';
    encode_bits[i++] = '\0';

    if(i == NEW_PW_FORMAT_SPACE_PADDED_LEN) {
      /*
       * We are using a new format, space padded
       * acct ctrl field. Encode the given acct ctrl
       * bits into it.
       */
      fstrcpy(encode_bits, pdb_encode_acct_ctrl(pwd->acct_ctrl, NEW_PW_FORMAT_SPACE_PADDED_LEN));
    } else {
      /*
       * If using the old format and the ACB_DISABLED or
       * ACB_PWNOTREQ are set then set the lanman and NT passwords to NULL
       * here as we have no space to encode the change.
       */
      if(pwd->acct_ctrl & (ACB_DISABLED|ACB_PWNOTREQ)) {
        pwd->smb_passwd = NULL;
        pwd->smb_nt_passwd = NULL;
      }
    }

    /* Go past the ']' */
    if(linebuf_len > PTR_DIFF(p, linebuf))
      p++;

    if((linebuf_len > PTR_DIFF(p, linebuf)) && (*p == ':')) {
      p++;

      /* We should be pointing at the LCT entry. */
      if((linebuf_len > (PTR_DIFF(p, linebuf) + 13)) && (StrnCaseCmp((char *)p, "LCT-", 4) == 0)) {

        p += 4;
        for(i = 0; i < 8; i++) {
          if(p[i] == '\0' || !isxdigit(p[i]))
            break;
        }
        if(i == 8) {
          /*
           * p points at 8 characters of hex digits -
           * read into a time_t as the seconds since
           * 1970 that the password was last changed.
           */
          got_pass_last_set_time = True;
        } /* i == 8 */
      } /* *p && StrnCaseCmp() */
    } /* p == ':' */
  } /* p == '[' */

  /* Entry is correctly formed. */

  /* Create the 32 byte representation of the new p16 */
  if(pwd->smb_passwd != NULL) {
    for (i = 0; i < 16; i++) {
      slprintf(&ascii_p16[i*2], sizeof(fstring) - 1, "%02X", (uchar) pwd->smb_passwd[i]);
    }
  } else {
    if(pwd->acct_ctrl & ACB_PWNOTREQ)
      fstrcpy(ascii_p16, "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX");
    else
      fstrcpy(ascii_p16, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  }

  /* Add on the NT md4 hash */
  ascii_p16[32] = ':';
  wr_len = 66;
  if (pwd->smb_nt_passwd != NULL) {
    for (i = 0; i < 16; i++) {
      slprintf(&ascii_p16[(i*2)+33], sizeof(fstring) - 1, "%02X", (uchar) pwd->smb_nt_passwd[i]);
    }
  } else {
    if(pwd->acct_ctrl & ACB_PWNOTREQ)
      fstrcpy(&ascii_p16[33], "NO PASSWORDXXXXXXXXXXXXXXXXXXXXX");
    else
      fstrcpy(&ascii_p16[33], "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  }
  ascii_p16[65] = ':';
  ascii_p16[66] = '\0'; /* null-terminate the string so that strlen works */

  /* Add on the account info bits and the time of last
     password change. */

  pwd->pass_last_set_time = time(NULL);

  if(got_pass_last_set_time) {
    slprintf(&ascii_p16[strlen(ascii_p16)], 
	     sizeof(ascii_p16)-(strlen(ascii_p16)+1),
	     "%s:LCT-%08X:", 
                     encode_bits, (uint32)pwd->pass_last_set_time );
    wr_len = strlen(ascii_p16);
  }

#ifdef DEBUG_PASSWORD
  DEBUG(100,("mod_smbfilepwd_entry: "));
  dump_data(100, ascii_p16, wr_len);
#endif

  if(wr_len > sizeof(linebuf)) {
    DEBUG(0, ("mod_smbfilepwd_entry: line to write (%d) is too long.\n", wr_len+1));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return (False);
  }

  /*
   * Do an atomic write into the file at the position defined by
   * seekpos.
   */

  /* The mod user write needs to be atomic - so get the fd from 
     the fp and do a raw write() call.
   */

  fd = fileno(fp);

  if (sys_lseek(fd, pwd_seekpos - 1, SEEK_SET) != pwd_seekpos - 1) {
    DEBUG(0, ("mod_smbfilepwd_entry: seek fail on file %s.\n", pfile));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  /* Sanity check - ensure the areas we are writing are framed by ':' */
  if (read(fd, linebuf, wr_len+1) != wr_len+1) {
    DEBUG(0, ("mod_smbfilepwd_entry: read fail on file %s.\n", pfile));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  if ((linebuf[0] != ':') || (linebuf[wr_len] != ':'))	{
    DEBUG(0, ("mod_smbfilepwd_entry: check on passwd file %s failed.\n", pfile));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }
 
  if (sys_lseek(fd, pwd_seekpos, SEEK_SET) != pwd_seekpos) {
    DEBUG(0, ("mod_smbfilepwd_entry: seek fail on file %s.\n", pfile));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  if (write(fd, ascii_p16, wr_len) != wr_len) {
    DEBUG(0, ("mod_smbfilepwd_entry: write failed in passwd file %s\n", pfile));
    pw_file_unlock(lockfd,&pw_file_lock_depth);
    fclose(fp);
    return False;
  }

  pw_file_unlock(lockfd,&pw_file_lock_depth);
  fclose(fp);
  return True;
}

/************************************************************************
 Routine to delete an entry in the smbpasswd file by name.
*************************************************************************/

static BOOL del_smbfilepwd_entry(const char *name)
{
  char *pfile = lp_smb_passwd_file();
  pstring pfile2;
  struct smb_passwd *pwd = NULL;
  FILE *fp = NULL;
  FILE *fp_write = NULL;
  int pfile2_lockdepth = 0;

  slprintf(pfile2, sizeof(pfile2)-1, "%s.%u", pfile, (unsigned)getpid() );

  /*
   * Open the smbpassword file - for update. It needs to be update
   * as we need any other processes to wait until we have replaced
   * it.
   */

  if((fp = startsmbfilepwent(True)) == NULL) {
    DEBUG(0, ("del_smbfilepwd_entry: unable to open file %s.\n", pfile));
    return False;
  }

  /*
   * Create the replacement password file.
   */
  if((fp_write = startsmbfilepwent_internal(pfile2, PWF_CREATE, &pfile2_lockdepth)) == NULL) {
    DEBUG(0, ("del_smbfilepwd_entry: unable to open file %s.\n", pfile));
    endsmbfilepwent(fp);
    return False;
  }

  /*
   * Scan the file, a line at a time and check if the name matches.
   */

  while ((pwd = getsmbfilepwent(fp)) != NULL) {
    char *new_entry;
    size_t new_entry_length;

    if (strequal(name, pwd->smb_name)) {
      DEBUG(10, ("add_smbfilepwd_entry: found entry with name %s - deleting it.\n", name));
      continue;
    }

    /*
     * We need to copy the entry out into the second file.
     */

    if((new_entry = format_new_smbpasswd_entry(pwd)) == NULL) {
      DEBUG(0, ("del_smbfilepwd_entry(malloc): Failed to copy entry for user %s to file %s. \
Error was %s\n", pwd->smb_name, pfile2, strerror(errno)));
      unlink(pfile2);
      endsmbfilepwent(fp);
      endsmbfilepwent_internal(fp_write,&pfile2_lockdepth);
      return False;
    }

    new_entry_length = strlen(new_entry);

    if(fwrite(new_entry, 1, new_entry_length, fp_write) != new_entry_length) {
      DEBUG(0, ("del_smbfilepwd_entry(write): Failed to copy entry for user %s to file %s. \
Error was %s\n", pwd->smb_name, pfile2, strerror(errno)));
      unlink(pfile2);
      endsmbfilepwent(fp);
      endsmbfilepwent_internal(fp_write,&pfile2_lockdepth);
      free(new_entry);
      return False;
    }

    free(new_entry);
  }

  /*
   * Ensure pfile2 is flushed before rename.
   */

  if(fflush(fp_write) != 0) {
    DEBUG(0, ("del_smbfilepwd_entry: Failed to flush file %s. Error was %s\n", pfile2, strerror(errno)));
    endsmbfilepwent(fp);
    endsmbfilepwent_internal(fp_write,&pfile2_lockdepth);
    return False;
  }

  /*
   * Do an atomic rename - then release the locks.
   */

  if(rename(pfile2,pfile) != 0) {
    unlink(pfile2);
  }
  endsmbfilepwent(fp);
  endsmbfilepwent_internal(fp_write,&pfile2_lockdepth);
  return True;
}
	
/*
 * Stub functions - implemented in terms of others.
 */

static BOOL mod_smbfile21pwd_entry(struct sam_passwd* pwd, BOOL override)
{
 	return mod_smbfilepwd_entry(pdb_sam_to_smb(pwd), override);
}

static BOOL add_smbfile21pwd_entry(struct sam_passwd *newpwd)
{
 	return add_smbfilepwd_entry(pdb_sam_to_smb(newpwd));
}

static struct sam_disp_info *getsmbfiledispnam(char *name)
{
	return pdb_sam_to_dispinfo(getsam21pwnam(name));
}

static struct sam_disp_info *getsmbfiledisprid(uint32 rid)
{
	return pdb_sam_to_dispinfo(getsam21pwrid(rid));
}

static struct sam_disp_info *getsmbfiledispent(void *vp)
{
	return pdb_sam_to_dispinfo(getsam21pwent(vp));
}

static struct passdb_ops file_ops = {
  startsmbfilepwent,
  endsmbfilepwent,
  getsmbfilepwpos,
  setsmbfilepwpos,
  iterate_getsmbpwnam,          /* In passdb.c */
  iterate_getsmbpwuid,          /* In passdb.c */
  iterate_getsmbpwrid,          /* In passdb.c */
  getsmbfilepwent,
  add_smbfilepwd_entry,
  mod_smbfilepwd_entry,
  del_smbfilepwd_entry,
  getsmbfile21pwent,
  iterate_getsam21pwnam,
  iterate_getsam21pwuid,
  iterate_getsam21pwrid, 
  add_smbfile21pwd_entry,
  mod_smbfile21pwd_entry,
  getsmbfiledispnam,
  getsmbfiledisprid,
  getsmbfiledispent
};

struct passdb_ops *file_initialize_password_db(void)
{    
  return &file_ops;
}

#else
 /* Do *NOT* make this function static. It breaks the compile on gcc. JRA */
 void smbpass_dummy_function(void) { } /* stop some compilers complaining */
#endif /* USE_SMBPASS_DB */
