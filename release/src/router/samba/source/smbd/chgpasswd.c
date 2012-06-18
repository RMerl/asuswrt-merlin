/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   
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

/* fork a child process to exec passwd and write to its
* tty to change a users password. This is running as the
* user who is attempting to change the password.
*/

/* 
 * This code was copied/borrowed and stolen from various sources.
 * The primary source was the poppasswd.c from the authors of POPMail. This software
 * was included as a client to change passwords using the 'passwd' program
 * on the remote machine.
 *
 * This routine is called by set_user_password() in password.c only if ALLOW_PASSWORD_CHANGE
 * is defined in the compiler directives located in the Makefile.
 *
 * This code has been hacked by Bob Nance (nance@niehs.nih.gov) and Evan Patterson
 * (patters2@niehs.nih.gov) at the National Institute of Environmental Health Sciences
 * and rights to modify, distribute or incorporate this change to the CAP suite or
 * using it for any other reason are granted, so long as this disclaimer is left intact.
 */

/*
   This code was hacked considerably for inclusion in Samba, primarily
   by Andrew.Tridgell@anu.edu.au. The biggest change was the addition
   of the "password chat" option, which allows the easy runtime
   specification of the expected sequence of events to change a
   password.
   */

#include "includes.h"

extern int DEBUGLEVEL;

#if ALLOW_CHANGE_PASSWORD

static int findpty(char **slave)
{
  int master;
  static fstring line;
  void *dirp;
  char *dpname;
  
#if defined(HAVE_GRANTPT)
  /* Try to open /dev/ptmx. If that fails, fall through to old method. */
  if ((master = sys_open("/dev/ptmx", O_RDWR, 0)) >= 0) {
    grantpt(master);
    unlockpt(master);
    *slave = (char *)ptsname(master);
    if (*slave == NULL) {
      DEBUG(0,("findpty: Unable to create master/slave pty pair.\n"));
      /* Stop fd leak on error. */
      close(master);
      return -1;
    } else {
      DEBUG(10, ("findpty: Allocated slave pty %s\n", *slave));
      return (master);
    }
  }
#endif /* HAVE_GRANTPT */

  fstrcpy( line, "/dev/ptyXX" );

  dirp = OpenDir(NULL, "/dev", False);
  if (!dirp)
    return(-1);
  while ((dpname = ReadDirName(dirp)) != NULL) {
    if (strncmp(dpname, "pty", 3) == 0 && strlen(dpname) == 5) {
      DEBUG(3,("pty: try to open %s, line was %s\n", dpname, line ) );
      line[8] = dpname[3];
      line[9] = dpname[4];
      if ((master = sys_open(line, O_RDWR, 0)) >= 0) {
        DEBUG(3,("pty: opened %s\n", line ) );
        line[5] = 't';
        *slave = line;
        CloseDir(dirp);
        return (master);
      }
    }
  }
  CloseDir(dirp);
  return (-1);
}

static int dochild(int master,char *slavedev, char *name, char *passwordprogram, BOOL as_root)
{
  int slave;
  struct termios stermios;
  struct passwd *pass = Get_Pwnam(name,True);
  gid_t gid;
  uid_t uid;

  if (pass == NULL) {
    DEBUG(0,("dochild: user name %s doesn't exist in the UNIX password database.\n",
              name));
    return False;
  }

  gid = pass->pw_gid;
  uid = pass->pw_uid;

  gain_root_privilege();

  /* Start new session - gets rid of controlling terminal. */
  if (setsid() < 0) {
    DEBUG(3,("Weirdness, couldn't let go of controlling terminal\n"));
    return(False);
  }

  /* Open slave pty and acquire as new controlling terminal. */
  if ((slave = sys_open(slavedev, O_RDWR, 0)) < 0) {
    DEBUG(3,("More weirdness, could not open %s\n", 
	     slavedev));
    return(False);
  }
#ifdef I_PUSH
  ioctl(slave, I_PUSH, "ptem");
  ioctl(slave, I_PUSH, "ldterm");
#elif defined(TIOCSCTTY)
  if (ioctl(slave,TIOCSCTTY,0) <0) {
     DEBUG(3,("Error in ioctl call for slave pty\n"));
     /* return(False); */
  }
#endif 

  /* Close master. */
  close(master);

  /* Make slave stdin/out/err of child. */

  if (dup2(slave, STDIN_FILENO) != STDIN_FILENO) {
    DEBUG(3,("Could not re-direct stdin\n"));
    return(False);
  }
  if (dup2(slave, STDOUT_FILENO) != STDOUT_FILENO) {
    DEBUG(3,("Could not re-direct stdout\n"));
    return(False);
  }
  if (dup2(slave, STDERR_FILENO) != STDERR_FILENO) {
    DEBUG(3,("Could not re-direct stderr\n"));
    return(False);
  }
  if (slave > 2) close(slave);

  /* Set proper terminal attributes - no echo, canonical input processing,
     no map NL to CR/NL on output. */

  if (tcgetattr(0, &stermios) < 0) {
    DEBUG(3,("could not read default terminal attributes on pty\n"));
    return(False);
  }
  stermios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
  stermios.c_lflag |= ICANON;
  stermios.c_oflag &= ~(ONLCR);
  if (tcsetattr(0, TCSANOW, &stermios) < 0) {
    DEBUG(3,("could not set attributes of pty\n"));
    return(False);
  }

  /* make us completely into the right uid */
  if (!as_root) {
	  become_user_permanently(uid, gid);
  }

  DEBUG(10, ("Invoking '%s' as password change program.\n", passwordprogram));

  /* execl() password-change application */
  if (execl("/bin/sh","sh","-c",passwordprogram,NULL) < 0) {
    DEBUG(3,("Bad status returned from %s\n",passwordprogram));
    return(False);
  }
  return(True);
}

static int expect(int master, char *issue, char *expected)
{
	pstring buffer;
	int attempts, timeout, nread, len;
	BOOL match = False;

	for (attempts = 0; attempts < 2; attempts++)
	{
		if (!strequal(issue, "."))
		{
			if (lp_passwd_chat_debug())
				DEBUG(100, ("expect: sending [%s]\n", issue));

			write(master, issue, strlen(issue));
		}

		if (strequal(expected, "."))
			return True;

		timeout = 2000;
		nread = 0;
		buffer[nread] = 0;

		while ((len = read_with_timeout(master, buffer + nread, 1,
				 sizeof(buffer) - nread - 1, timeout)) > 0)
		{
			nread += len;
			buffer[nread] = 0;

			if ((match = unix_do_match(buffer, expected, False)))
				timeout = 200;
		}

		if (lp_passwd_chat_debug())
			DEBUG(100, ("expect: expected [%s] received [%s]\n",
				    expected, buffer));

		if (match)
			break;

		if (len < 0)
		{
			DEBUG(2, ("expect: %s\n", strerror(errno)));
			return False;
		}
	}

	return match;
}

static void pwd_sub(char *buf)
{
	all_string_sub(buf,"\\n","\n",0);
	all_string_sub(buf,"\\r","\r",0);
	all_string_sub(buf,"\\s"," ",0);
	all_string_sub(buf,"\\t","\t",0);
}

static int talktochild(int master, char *seq)
{
	int count = 0;
	fstring issue, expected;

	fstrcpy(issue, ".");

	while (next_token(&seq, expected, NULL, sizeof(expected)))
	{
		pwd_sub(expected);
		count++;

		if (!expect(master, issue, expected))
		{
			DEBUG(3,("Response %d incorrect\n", count));
			return False;
		}

		if (!next_token(&seq, issue, NULL, sizeof(issue)))
			fstrcpy(issue, ".");

		pwd_sub(issue);
	}

	return (count > 0);
}

static BOOL chat_with_program(char *passwordprogram,char *name,char *chatsequence, BOOL as_root)
{
  char *slavedev;
  int master;
  pid_t pid, wpid;
  int wstat;
  BOOL chstat = False;    

  /* allocate a pseudo-terminal device */
  if ((master = findpty (&slavedev)) < 0) {
    DEBUG(3,("Cannot Allocate pty for password change: %s\n",name));
    return(False);
  }

  /*
   * We need to temporarily stop CatchChild from eating
   * SIGCLD signals as it also eats the exit status code. JRA.
   */

  CatchChildLeaveStatus();

  if ((pid = fork()) < 0) {
    DEBUG(3,("Cannot fork() child for password change: %s\n",name));
    close(master);
    CatchChild();
    return(False);
  }

  /* we now have a pty */
  if (pid > 0){			/* This is the parent process */
    if ((chstat = talktochild(master, chatsequence)) == False) {
      DEBUG(3,("Child failed to change password: %s\n",name));
      kill(pid, SIGKILL); /* be sure to end this process */
    }

	while((wpid = sys_waitpid(pid, &wstat, 0)) < 0) {
      if(errno == EINTR) {
        errno = 0;
        continue;
      }
	  break;
    }

    if (wpid < 0) {
      DEBUG(3,("The process is no longer waiting!\n\n"));
      close(master);
      CatchChild();
      return(False);
    }

    /*
     * Go back to ignoring children.
     */
    CatchChild();

    close(master);

    if (pid != wpid) {
      DEBUG(3,("We were waiting for the wrong process ID\n"));	
      return(False);
    }
    if (WIFEXITED(wstat) == 0) {
      DEBUG(3,("The process exited while we were waiting\n"));
      return(False);
    }
    if (WEXITSTATUS(wstat) != 0) {
      DEBUG(3,("The status of the process exiting was %d\n", wstat));
      return(False);
    }
    
  } else {
    /* CHILD */

    /*
     * Lose any oplock capabilities.
     */
    set_process_capability(KERNEL_OPLOCK_CAPABILITY, False);
    set_inherited_process_capability(KERNEL_OPLOCK_CAPABILITY, False);

    /* make sure it doesn't freeze */
    alarm(20);

    if (as_root)
      become_root(False);
    DEBUG(3,("Dochild for user %s (uid=%d,gid=%d)\n",name,(int)getuid(),(int)getgid()));
    chstat = dochild(master, slavedev, name, passwordprogram, as_root);

	/*
	 * The child should never return from dochild() ....
	 */

	DEBUG(0,("chat_with_program: Error: dochild() returned %d\n", chstat ));
	exit(1);
  }

  if (chstat)
    DEBUG(3,("Password change %ssuccessful for user %s\n", (chstat?"":"un"), name));
  return (chstat);
}


BOOL chgpasswd(char *name,char *oldpass,char *newpass, BOOL as_root)
{
  pstring passwordprogram;
  pstring chatsequence;
  size_t i;
  size_t len;

  strlower(name); 
  DEBUG(3,("Password change for user: %s\n",name));

#if DEBUG_PASSWORD
  DEBUG(100,("Passwords: old=%s new=%s\n",oldpass,newpass)); 
#endif

  /* Take the passed information and test it for minimum criteria */
  /* Minimum password length */
  if (strlen(newpass) < lp_min_passwd_length()) /* too short, must be at least MINPASSWDLENGTH */ 
    {
      DEBUG(0,("Password Change: user %s, New password is shorter than minimum password length = %d\n",
            name, lp_min_passwd_length()));
      return (False);		/* inform the user */
    }
  
  /* Password is same as old password */
  if (strcmp(oldpass,newpass) == 0) /* don't allow same password */
    {
      DEBUG(2,("Password Change: %s, New password is same as old\n",name)); /* log the attempt */
      return (False);		/* inform the user */
    }

  pstrcpy(passwordprogram,lp_passwd_program());
  pstrcpy(chatsequence,lp_passwd_chat());

  if (!*chatsequence) {
    DEBUG(2,("Null chat sequence - no password changing\n"));
    return(False);
  }

  if (!*passwordprogram) {
    DEBUG(2,("Null password program - no password changing\n"));
    return(False);
  }

  /* 
   * Check the old and new passwords don't contain any control
   * characters.
   */

  len = strlen(oldpass); 
  for(i = 0; i < len; i++) {
    if (iscntrl((int)oldpass[i])) {
      DEBUG(0,("chat_with_program: oldpass contains control characters (disallowed).\n"));
      return False;
    }
  }

  len = strlen(newpass);
  for(i = 0; i < len; i++) {
    if (iscntrl((int)newpass[i])) {
      DEBUG(0,("chat_with_program: newpass contains control characters (disallowed).\n"));
      return False;
    }
  }

  pstring_sub(passwordprogram,"%u",name);
  /* note that we do NOT substitute the %o and %n in the password program
     as this would open up a security hole where the user could use
     a new password containing shell escape characters */

  pstring_sub(chatsequence,"%u",name);
  all_string_sub(chatsequence,"%o",oldpass,sizeof(pstring));
  all_string_sub(chatsequence,"%n",newpass,sizeof(pstring));
  return(chat_with_program(passwordprogram,name,chatsequence, as_root));
}

#else /* ALLOW_CHANGE_PASSWORD */
BOOL chgpasswd(char *name,char *oldpass,char *newpass, BOOL as_root)
{
  DEBUG(0,("Password changing not compiled in (user=%s)\n",name));
  return(False);
}
#endif /* ALLOW_CHANGE_PASSWORD */

/***********************************************************
 Code to check the lanman hashed password.
************************************************************/

BOOL check_lanman_password(char *user, uchar *pass1, 
                           uchar *pass2, struct smb_passwd **psmbpw)
{
  static uchar null_pw[16];
  uchar unenc_new_pw[16];
  uchar unenc_old_pw[16];
  struct smb_passwd *smbpw;

  *psmbpw = NULL;

  become_root(0);
  smbpw = getsmbpwnam(user);
  unbecome_root(0);

  if (smbpw == NULL)
  {
    DEBUG(0,("check_lanman_password: getsmbpwnam returned NULL\n"));
    return False;
  }

  if (smbpw->acct_ctrl & ACB_DISABLED)
  {
    DEBUG(0,("check_lanman_password: account %s disabled.\n", user));
    return False;
  }

  if ((smbpw->smb_passwd == NULL) && (smbpw->acct_ctrl & ACB_PWNOTREQ))
  {
    uchar no_pw[14];
    memset(no_pw, '\0', 14);
    E_P16(no_pw, null_pw);
    smbpw->smb_passwd = null_pw;
  } else if (smbpw->smb_passwd == NULL) {
    DEBUG(0,("check_lanman_password: no lanman password !\n"));
    return False;
  }

  /* Get the new lanman hash. */
  D_P16(smbpw->smb_passwd, pass2, unenc_new_pw);

  /* Use this to get the old lanman hash. */
  D_P16(unenc_new_pw, pass1, unenc_old_pw);

  /* Check that the two old passwords match. */
  if (memcmp(smbpw->smb_passwd, unenc_old_pw, 16))
  {
    DEBUG(0,("check_lanman_password: old password doesn't match.\n"));
    return False;
  }

  *psmbpw = smbpw;
  return True;
}

/***********************************************************
 Code to change the lanman hashed password.
 It nulls out the NT hashed password as it will
 no longer be valid.
************************************************************/

BOOL change_lanman_password(struct smb_passwd *smbpw, uchar *pass1, uchar *pass2)
{
  static uchar null_pw[16];
  uchar unenc_new_pw[16];
  BOOL ret;

  if (smbpw == NULL)
  { 
    DEBUG(0,("change_lanman_password: no smb password entry.\n"));
    return False;
  }

  if (smbpw->acct_ctrl & ACB_DISABLED)
  {
    DEBUG(0,("change_lanman_password: account %s disabled.\n", smbpw->smb_name));
    return False;
  }

  if ((smbpw->smb_passwd == NULL) && (smbpw->acct_ctrl & ACB_PWNOTREQ))
  {
    uchar no_pw[14];
    memset(no_pw, '\0', 14);
    E_P16(no_pw, null_pw);
    smbpw->smb_passwd = null_pw;
  } else if (smbpw->smb_passwd == NULL) {
    DEBUG(0,("change_lanman_password: no lanman password !\n"));
    return False;
  }

  /* Get the new lanman hash. */
  D_P16(smbpw->smb_passwd, pass2, unenc_new_pw);

  smbpw->smb_passwd = unenc_new_pw;
  smbpw->smb_nt_passwd = NULL; /* We lose the NT hash. Sorry. */

  /* Now write it into the file. */
  become_root(0);
  ret = mod_smbpwd_entry(smbpw,False);
  unbecome_root(0);
    
  return ret;
}

/***********************************************************
 Code to check and change the OEM hashed password.
************************************************************/
BOOL pass_oem_change(char *user,
			uchar *lmdata, uchar *lmhash,
			uchar *ntdata, uchar *nthash)
{
	fstring new_passwd;
	struct smb_passwd *sampw;
	BOOL ret = check_oem_password( user, lmdata, lmhash, ntdata, nthash,
	                               &sampw, 
	                               new_passwd, sizeof(new_passwd));

	/* 
	 * At this point we have the new case-sensitive plaintext
	 * password in the fstring new_passwd. If we wanted to synchronise
	 * with UNIX passwords we would call a UNIX password changing 
	 * function here. However it would have to be done as root
	 * as the plaintext of the old users password is not 
	 * available. JRA.
	 */

	if ( ret && lp_unix_password_sync())
	{
		ret = chgpasswd(user,"", new_passwd, True);
	}

	if (ret)
	{
		ret = change_oem_password( sampw, new_passwd, False );
	}

	memset(new_passwd, 0, sizeof(new_passwd));

	return ret;
}

/***********************************************************
 Code to check the OEM hashed password.

 this function ignores the 516 byte nt OEM hashed password
 but does use the lm OEM password to check the nt hashed-hash.

************************************************************/
BOOL check_oem_password(char *user,
			uchar *lmdata, uchar *lmhash,
			uchar *ntdata, uchar *nthash,
                        struct smb_passwd **psmbpw, char *new_passwd,
                        int new_passwd_size)
{
	static uchar null_pw[16];
	static uchar null_ntpw[16];
	struct smb_passwd *smbpw = NULL;
	int new_pw_len;
	uchar new_ntp16[16];
	uchar unenc_old_ntpw[16];
	uchar new_p16[16];
	uchar unenc_old_pw[16];
	char no_pw[2];

	BOOL nt_pass_set = (ntdata != NULL && nthash != NULL);

	become_root(False);
	*psmbpw = smbpw = getsmbpwnam(user);
	unbecome_root(False);

	if (smbpw == NULL)
	{
		DEBUG(0,("check_oem_password: getsmbpwnam returned NULL\n"));
		return False;
	}

	if (smbpw->acct_ctrl & ACB_DISABLED)
	{
		DEBUG(0,("check_lanman_password: account %s disabled.\n", user));
		return False;
	}

	/* construct a null password (in case one is needed */
	no_pw[0] = 0;
	no_pw[1] = 0;
	nt_lm_owf_gen(no_pw, null_ntpw, null_pw);

	/* check for null passwords */
	if (smbpw->smb_passwd == NULL)
	{
		if (smbpw->acct_ctrl & ACB_PWNOTREQ)
		{
			smbpw->smb_passwd = null_pw;
		}
		else 
		{
			DEBUG(0,("check_oem_password: no lanman password !\n"));
			return False;
		}
	}

	if (smbpw->smb_nt_passwd == NULL && nt_pass_set)
	{
		if (smbpw->acct_ctrl & ACB_PWNOTREQ)
		{
			smbpw->smb_nt_passwd = null_pw;
		}
		else 
		{
			DEBUG(0,("check_oem_password: no ntlm password !\n"));
			return False;
		}
	}

	/* 
	 * Call the hash function to get the new password.
	 */
	SamOEMhash( (uchar *)lmdata, (uchar *)smbpw->smb_passwd, True);

	/* 
	 * The length of the new password is in the last 4 bytes of
	 * the data buffer.
	 */

	new_pw_len = IVAL(lmdata, 512);
	if (new_pw_len < 0 || new_pw_len > new_passwd_size - 1)
	{
		DEBUG(0,("check_oem_password: incorrect password length (%d).\n", new_pw_len));
		return False;
	}

	if (nt_pass_set)
	{
		/*
		 * nt passwords are in unicode
		 */
		int uni_pw_len = new_pw_len;
		char *pw;
		new_pw_len /= 2;
		pw = dos_unistrn2((uint16*)(&lmdata[512-uni_pw_len]), new_pw_len);
		memcpy(new_passwd, pw, new_pw_len+1);
	}
	else
	{
		memcpy(new_passwd, &lmdata[512-new_pw_len], new_pw_len);
		new_passwd[new_pw_len] = '\0';
	}

	/*
	 * To ensure we got the correct new password, hash it and
	 * use it as a key to test the passed old password.
	 */

	nt_lm_owf_gen(new_passwd, new_ntp16, new_p16);

	if (!nt_pass_set)
	{
		/*
		 * Now use new_p16 as the key to see if the old
		 * password matches.
		 */
		D_P16(new_p16  , lmhash, unenc_old_pw);

		if (memcmp(smbpw->smb_passwd, unenc_old_pw, 16))
		{
			DEBUG(0,("check_oem_password: old lm password doesn't match.\n"));
			return False;
		}

#ifdef DEBUG_PASSWORD
		DEBUG(100,("check_oem_password: password %s ok\n", new_passwd));
#endif
		return True;
	}

	/*
	 * Now use new_p16 as the key to see if the old
	 * password matches.
	 */
	D_P16(new_ntp16, lmhash, unenc_old_pw);
	D_P16(new_ntp16, nthash, unenc_old_ntpw);

	if (memcmp(smbpw->smb_passwd, unenc_old_pw, 16))
	{
		DEBUG(0,("check_oem_password: old lm password doesn't match.\n"));
		return False;
	}

	if (memcmp(smbpw->smb_nt_passwd, unenc_old_ntpw, 16))
	{
		DEBUG(0,("check_oem_password: old nt password doesn't match.\n"));
		return False;
	}
#ifdef DEBUG_PASSWORD
	DEBUG(100,("check_oem_password: password %s ok\n", new_passwd));
#endif
	return True;
}

/***********************************************************
 Code to change the oem password. Changes both the lanman
 and NT hashes.
 override = False, normal
 override = True, override XXXXXXXXXX'd password
************************************************************/

BOOL change_oem_password(struct smb_passwd *smbpw, char *new_passwd, BOOL override)
{
  int ret;
  uchar new_nt_p16[16];
  uchar new_p16[16];

  nt_lm_owf_gen(new_passwd, new_nt_p16, new_p16);

  smbpw->smb_passwd = new_p16;
  smbpw->smb_nt_passwd = new_nt_p16;
  
  /* Now write it into the file. */
  become_root(0);
  ret = mod_smbpwd_entry(smbpw,override);
  unbecome_root(0);

  memset(new_passwd, '\0', strlen(new_passwd));

  return ret;
}

/***********************************************************
 Code to check a plaintext password against smbpasswd entries.
***********************************************************/

BOOL check_plaintext_password(char *user,char *old_passwd,
                              int old_passwd_size, struct smb_passwd **psmbpw)
{
  struct smb_passwd *smbpw = NULL;
  uchar old_pw[16],old_ntpw[16];

  become_root(False);
  *psmbpw = smbpw = getsmbpwnam(user);
  unbecome_root(False);

  if (smbpw == NULL) {
    DEBUG(0,("check_plaintext_password: getsmbpwnam returned NULL\n"));
    return False;
  }

  if (smbpw->acct_ctrl & ACB_DISABLED) {
    DEBUG(0,("check_plaintext_password: account %s disabled.\n", user));
    return(False);
  }

  nt_lm_owf_gen(old_passwd,old_ntpw,old_pw);

#ifdef DEBUG_PASSWORD
  DEBUG(100,("check_plaintext_password: smbpw->smb_nt_passwd \n"));
  dump_data(100,smbpw->smb_nt_passwd,16);
  DEBUG(100,("check_plaintext_password: old_ntpw \n"));
  dump_data(100,old_ntpw,16);
  DEBUG(100,("check_plaintext_password: smbpw->smb_passwd \n"));
  dump_data(100,smbpw->smb_passwd,16);
  DEBUG(100,("check_plaintext_password: old_pw\n"));
  dump_data(100,old_pw,16);
#endif

  if(memcmp(smbpw->smb_nt_passwd,old_ntpw,16) && memcmp(smbpw->smb_passwd,old_pw,16))
    return(False);
  else
    return(True);
}
