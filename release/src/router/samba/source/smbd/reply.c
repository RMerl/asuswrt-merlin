/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Main SMB reply routines
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
/*
   This file handles most of the reply_ calls that the server
   makes to handle specific protocols
*/


#include "includes.h"
#include "trans2.h"
#include "nterr.h"

/* look in server.c for some explanation of these variables */
extern int Protocol;
extern int DEBUGLEVEL;
extern int max_send;
extern int max_recv;
extern char magic_char;
extern BOOL case_sensitive;
extern BOOL case_preserve;
extern BOOL short_case_preserve;
extern pstring sesssetup_user;
extern pstring global_myname;
extern fstring global_myworkgroup;
extern int Client;
extern int global_oplock_break;
uint32 global_client_caps = 0;
unsigned int smb_echo_count = 0;

/****************************************************************************
report a possible attack via the password buffer overflow bug
****************************************************************************/

static void overflow_attack(int len)
{
	if( DEBUGLVL( 0 ) ) {
		dbgtext( "ERROR: Invalid password length %d.\n", len );
		dbgtext( "Your machine may be under attack by someone " );
		dbgtext( "attempting to exploit an old bug.\n" );
		dbgtext( "Attack was from IP = %s.\n", client_addr(Client) );
	}
	exit_server("possible attack");
}


/****************************************************************************
  reply to an special message 
****************************************************************************/

int reply_special(char *inbuf,char *outbuf)
{
	int outsize = 4;
	int msg_type = CVAL(inbuf,0);
	int msg_flags = CVAL(inbuf,1);
	pstring name1,name2;
	extern fstring remote_machine;
	extern fstring local_machine;
	int len;
	char name_type = 0;
	
	*name1 = *name2 = 0;
	
	memset(outbuf,'\0',smb_size);

	smb_setlen(outbuf,0);
	
	switch (msg_type) {
	case 0x81: /* session request */
		CVAL(outbuf,0) = 0x82;
		CVAL(outbuf,3) = 0;
		if (name_len(inbuf+4) > 50 || 
		    name_len(inbuf+4 + name_len(inbuf + 4)) > 50) {
			DEBUG(0,("Invalid name length in session request\n"));
			return(0);
		}
		name_extract(inbuf,4,name1);
		name_extract(inbuf,4 + name_len(inbuf + 4),name2);
		DEBUG(2,("netbios connect: name1=%s name2=%s\n",
			 name1,name2));      

		fstrcpy(remote_machine,name2);
		remote_machine[15] = 0;
		trim_string(remote_machine," "," ");
		strlower(remote_machine);
		alpha_strcpy(remote_machine,remote_machine,SAFE_NETBIOS_CHARS,sizeof(remote_machine)-1);

		fstrcpy(local_machine,name1);
		len = strlen(local_machine);
		if (len == 16) {
			name_type = local_machine[15];
			local_machine[15] = 0;
		}
		trim_string(local_machine," "," ");
		strlower(local_machine);
		alpha_strcpy(local_machine,local_machine,SAFE_NETBIOS_CHARS,sizeof(local_machine)-1);

		if (name_type == 'R') {
			/* We are being asked for a pathworks session --- 
			   no thanks! */
			CVAL(outbuf, 0) = 0x83;
			break;
		}

		add_session_user(remote_machine);

		reload_services(True);
		reopen_logs();

		if (lp_status(-1)) {
			claim_connection(NULL,"STATUS.",MAXSTATUS,True);
		}

		break;
		
	case 0x89: /* session keepalive request 
		      (some old clients produce this?) */
		CVAL(outbuf,0) = 0x85;
		CVAL(outbuf,3) = 0;
		break;
		
	case 0x82: /* positive session response */
	case 0x83: /* negative session response */
	case 0x84: /* retarget session response */
		DEBUG(0,("Unexpected session response\n"));
		break;
		
	case 0x85: /* session keepalive */
	default:
		return(0);
	}
	
	DEBUG(5,("init msg_type=0x%x msg_flags=0x%x\n",
		    msg_type, msg_flags));
	
	return(outsize);
}


/*******************************************************************
work out what error to give to a failed connection
********************************************************************/

static int connection_error(char *inbuf,char *outbuf,int ecode)
{
	if (ecode == ERRnoipc)
		return(ERROR(ERRDOS,ERRnoipc));

	return(ERROR(ERRSRV,ecode));
}



/****************************************************************************
  parse a share descriptor string
****************************************************************************/
static void parse_connect(char *p,char *service,char *user,
			  char *password,int *pwlen,char *dev)
{
  char *p2;

  DEBUG(4,("parsing connect string %s\n",p));
    
  p2 = strrchr(p,'\\');
  if (p2 == NULL)
    fstrcpy(service,p);
  else
    fstrcpy(service,p2+1);
  
  p += strlen(p) + 2;
  
  fstrcpy(password,p);
  *pwlen = strlen(password);

  p += strlen(p) + 2;

  fstrcpy(dev,p);
  
  *user = 0;
  p = strchr(service,'%');
  if (p != NULL)
    {
      *p = 0;
      fstrcpy(user,p+1);
    }
}

/****************************************************************************
 Reply to a tcon.
****************************************************************************/

int reply_tcon(connection_struct *conn,
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	BOOL doencrypt = SMBENCRYPT();
	pstring service;
	pstring user;
	pstring password;
	pstring dev;
	int outsize = 0;
	uint16 vuid = SVAL(inbuf,smb_uid);
	int pwlen=0;
	int ecode = -1;

	*service = *user = *password = *dev = 0;

	parse_connect(smb_buf(inbuf)+1,service,user,password,&pwlen,dev);

    /*
     * Ensure the user and password names are in UNIX codepage format.
     */

    dos_to_unix(user,True);
	if (!doencrypt)
    	dos_to_unix(password,True);

	/*
	 * Pass the user through the NT -> unix user mapping
	 * function.
	 */
   
	(void)map_username(user);

	/*
	 * Do any UNIX username case mangling.
	 */
	(void)Get_Pwnam( user, True);

	conn = make_connection(service,user,password,pwlen,dev,vuid,&ecode);
  
	if (!conn) {
		return(connection_error(inbuf,outbuf,ecode));
	}
  
	outsize = set_message(outbuf,2,0,True);
	SSVAL(outbuf,smb_vwv0,max_recv);
	SSVAL(outbuf,smb_vwv1,conn->cnum);
	SSVAL(outbuf,smb_tid,conn->cnum);
  
	DEBUG(3,("tcon service=%s user=%s cnum=%d\n", 
		 service, user, conn->cnum));
  
	return(outsize);
}

/****************************************************************************
 Reply to a tcon and X.
****************************************************************************/

int reply_tcon_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
	fstring service;
	pstring user;
	pstring password;
	pstring devicename;
	BOOL doencrypt = SMBENCRYPT();
	int ecode = -1;
	uint16 vuid = SVAL(inbuf,smb_uid);
	int passlen = SVAL(inbuf,smb_vwv3);
	char *path;
	char *p;
	
	*service = *user = *password = *devicename = 0;

	/* we might have to close an old one */
	if ((SVAL(inbuf,smb_vwv2) & 0x1) && conn) {
		close_cnum(conn,vuid);
	}

	if (passlen > MAX_PASS_LEN) {
		overflow_attack(passlen);
	}
 
	memcpy(password,smb_buf(inbuf),passlen);
	password[passlen]=0;    
	path = smb_buf(inbuf) + passlen;

	if (passlen != 24) {
		if (strequal(password," "))
			*password = 0;
		passlen = strlen(password);
	}
	
	fstrcpy(service,path+2);
	p = strchr(service,'\\');
	if (!p)
		return(ERROR(ERRSRV,ERRinvnetname));
	*p = 0;
	fstrcpy(service,p+1);
	p = strchr(service,'%');
	if (p) {
		*p++ = 0;
		fstrcpy(user,p);
	}
	StrnCpy(devicename,path + strlen(path) + 1,6);
	DEBUG(4,("Got device type %s\n",devicename));

	/*
	 * Ensure the user and password names are in UNIX codepage format.
	 */

	dos_to_unix(user,True);
	if (!doencrypt)
		dos_to_unix(password,True);

	/*
	 * Pass the user through the NT -> unix user mapping
	 * function.
	 */
	
	(void)map_username(user);
	
	/*
	 * Do any UNIX username case mangling.
	 */
	(void)Get_Pwnam(user, True);
	
	conn = make_connection(service,user,password,passlen,devicename,vuid,&ecode);
	
	if (!conn)
		return(connection_error(inbuf,outbuf,ecode));

	if (Protocol < PROTOCOL_NT1) {
		set_message(outbuf,2,strlen(devicename)+1,True);
		pstrcpy(smb_buf(outbuf),devicename);
	} else {
		char *fsname = lp_fstype(SNUM(conn));

		set_message(outbuf,3,3,True);

		p = smb_buf(outbuf);
		pstrcpy(p,devicename); p = skip_string(p,1); /* device name */
		pstrcpy(p,fsname); p = skip_string(p,1); /* filesystem type e.g NTFS */
		
		set_message(outbuf,3,PTR_DIFF(p,smb_buf(outbuf)),False);
		
		/* what does setting this bit do? It is set by NT4 and
		   may affect the ability to autorun mounted cdroms */
		SSVAL(outbuf, smb_vwv2, SMB_SUPPORT_SEARCH_BITS); 
	}
  
	DEBUG(3,("tconX service=%s user=%s\n",
		 service, user));
  
	/* set the incoming and outgoing tid to the just created one */
	SSVAL(inbuf,smb_tid,conn->cnum);
	SSVAL(outbuf,smb_tid,conn->cnum);

	return chain_reply(inbuf,outbuf,length,bufsize);
}


/****************************************************************************
  reply to an unknown type
****************************************************************************/
int reply_unknown(char *inbuf,char *outbuf)
{
	int type;
	type = CVAL(inbuf,smb_com);
  
	DEBUG(0,("unknown command type (%s): type=%d (0x%X)\n",
		 smb_fn_name(type), type, type));
  
	return(ERROR(ERRSRV,ERRunknownsmb));
}


/****************************************************************************
  reply to an ioctl
****************************************************************************/
int reply_ioctl(connection_struct *conn,
		char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	uint16 device     = SVAL(inbuf,smb_vwv1);
	uint16 function   = SVAL(inbuf,smb_vwv2);
	uint32 ioctl_code = (device << 16) + function;
	int replysize, outsize;
	char *p;

	DEBUG(4, ("Received IOCTL (code 0x%x)\n", ioctl_code));

	switch (ioctl_code)
	{
	    case IOCTL_QUERY_JOB_INFO:
		replysize = 32;
		break;
	    default:
		return(ERROR(ERRSRV,ERRnosupport));
	}

	outsize = set_message(outbuf,8,replysize+1,True);
	SSVAL(outbuf,smb_vwv1,replysize); /* Total data bytes returned */
	SSVAL(outbuf,smb_vwv5,replysize); /* Data bytes this buffer */
	SSVAL(outbuf,smb_vwv6,52);        /* Offset to data */
	p = smb_buf(outbuf) + 1;          /* Allow for alignment */

	switch (ioctl_code)
	{
	    case IOCTL_QUERY_JOB_INFO:
		SSVAL(p,0,1);                            /* Job number */
		StrnCpy(p+2, global_myname, 15);         /* Our NetBIOS name */
		StrnCpy(p+18, lp_servicename(SNUM(conn)), 13); /* Service name */
		break;
	}

	return outsize;
}

/****************************************************************************
 always return an error: it's just a matter of which one...
 ****************************************************************************/
static int session_trust_account(connection_struct *conn, char *inbuf, char *outbuf, char *user,
                                char *smb_passwd, int smb_passlen,
                                char *smb_nt_passwd, int smb_nt_passlen)
{
  struct smb_passwd *smb_trust_acct = NULL; /* check if trust account exists */
  if (lp_security() == SEC_USER)
  {
    smb_trust_acct = getsmbpwnam(user);
  }
  else
  {
    DEBUG(0,("session_trust_account: Trust account %s only supported with security = user\n", user));
    SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
    return(ERROR(0, 0xc0000000|NT_STATUS_LOGON_FAILURE));
  }

  if (smb_trust_acct == NULL)
  {
    /* lkclXXXX: workstation entry doesn't exist */
    DEBUG(0,("session_trust_account: Trust account %s user doesn't exist\n",user));
    SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
    return(ERROR(0, 0xc0000000|NT_STATUS_NO_SUCH_USER));
  }
  else
  {
    if ((smb_passlen != 24) || (smb_nt_passlen != 24))
    {
      DEBUG(0,("session_trust_account: Trust account %s - password length wrong.\n", user));
      SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
      return(ERROR(0, 0xc0000000|NT_STATUS_LOGON_FAILURE));
    }

    if (!smb_password_ok(smb_trust_acct, NULL, (unsigned char *)smb_passwd, (unsigned char *)smb_nt_passwd))
    {
      DEBUG(0,("session_trust_account: Trust Account %s - password failed\n", user));
      SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
      return(ERROR(0, 0xc0000000|NT_STATUS_LOGON_FAILURE));
    }

    if (IS_BITS_SET_ALL(smb_trust_acct->acct_ctrl, ACB_DOMTRUST))
    {
      DEBUG(0,("session_trust_account: Domain trust account %s denied by server\n",user));
      SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
      return(ERROR(0, 0xc0000000|NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT));
    }

    if (IS_BITS_SET_ALL(smb_trust_acct->acct_ctrl, ACB_SVRTRUST))
    {
      DEBUG(0,("session_trust_account: Server trust account %s denied by server\n",user));
      SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
      return(ERROR(0, 0xc0000000|NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT));
    }

    if (IS_BITS_SET_ALL(smb_trust_acct->acct_ctrl, ACB_WSTRUST))
    {
      DEBUG(4,("session_trust_account: Wksta trust account %s denied by server\n", user));
      SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
      return(ERROR(0, 0xc0000000|NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT));
    }
  }

  /* don't know what to do: indicate logon failure */
  SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
  return(ERROR(0, 0xc0000000|NT_STATUS_LOGON_FAILURE));
}

/****************************************************************************
 Create a UNIX user on demand.
****************************************************************************/

static int smb_create_user(char *unix_user)
{
  pstring add_script;
  int ret;

  pstrcpy(add_script, lp_adduser_script());
  pstring_sub(add_script, "%u", unix_user);
  ret = smbrun(add_script,NULL,False);
  DEBUG(3,("smb_create_user: Running the command `%s' gave %d\n",add_script,ret));
  return ret;
}

/****************************************************************************
 Delete a UNIX user on demand.
****************************************************************************/

static int smb_delete_user(char *unix_user)
{
  pstring del_script;
  int ret;

  pstrcpy(del_script, lp_deluser_script());
  pstring_sub(del_script, "%u", unix_user);
  ret = smbrun(del_script,NULL,False);
  DEBUG(3,("smb_delete_user: Running the command `%s' gave %d\n",del_script,ret));
  return ret;
}

/****************************************************************************
 Check user is in correct domain if required
****************************************************************************/

static BOOL check_domain_match(char *user, char *domain) 
{
  /*
   * If we aren't serving to trusted domains, we must make sure that
   * the validation request comes from an account in the same domain
   * as the Samba server
   */

  if (!lp_allow_trusted_domains() &&
      !strequal(lp_workgroup(), domain) ) {
      DEBUG(1, ("check_domain_match: Attempt to connect as user %s from domain %s denied.\n", user, domain));
      return False;
  } else {
      return True;
  }
}

/****************************************************************************
 Check for a valid username and password in security=server mode.
****************************************************************************/

static BOOL check_server_security(char *orig_user, char *domain, char *unix_user,
                                  char *smb_apasswd, int smb_apasslen,
                                  char *smb_ntpasswd, int smb_ntpasslen)
{
  BOOL ret = False;

  if(lp_security() != SEC_SERVER)
    return False;

  if (!check_domain_match(orig_user, domain))
     return False;

  ret = server_validate(orig_user, domain, 
                            smb_apasswd, smb_apasslen, 
                            smb_ntpasswd, smb_ntpasslen);
  if(ret) {
    /*
     * User validated ok against Domain controller.
     * If the admin wants us to try and create a UNIX
     * user on the fly, do so.
     * Note that we can never delete users when in server
     * level security as we never know if it was a failure
     * due to a bad password, or the user really doesn't exist.
     */
    if(lp_adduser_script() && !Get_Pwnam(unix_user,True)) {
      smb_create_user(unix_user);
    }
  }

  return ret;
}

/****************************************************************************
 Check for a valid username and password in security=domain mode.
****************************************************************************/

static BOOL check_domain_security(char *orig_user, char *domain, char *unix_user, 
                                  char *smb_apasswd, int smb_apasslen,
                                  char *smb_ntpasswd, int smb_ntpasslen)
{
  BOOL ret = False;
  BOOL user_exists = True;

  if(lp_security() != SEC_DOMAIN)
    return False;

  if (!check_domain_match(orig_user, domain))
     return False;
#ifdef RPCCLIENT
  ret = domain_client_validate(orig_user, domain,
                                smb_apasswd, smb_apasslen,
                                smb_ntpasswd, smb_ntpasslen,
                                &user_exists);
#endif
  if(ret) {
    /*
     * User validated ok against Domain controller.
     * If the admin wants us to try and create a UNIX
     * user on the fly, do so.
     */
    if(user_exists && lp_adduser_script() && !Get_Pwnam(unix_user,True)) {
      smb_create_user(unix_user);
    }
  } else {
    /*
     * User failed to validate ok against Domain controller.
     * If the failure was "user doesn't exist" and admin 
     * wants us to try and delete that UNIX user on the fly,
     * do so.
     */
    if(!user_exists && lp_deluser_script() && Get_Pwnam(unix_user,True)) {
      smb_delete_user(unix_user);
    }
  }

  return ret;
}

/****************************************************************************
 Return a bad password error configured for the correct client type.
****************************************************************************/       

static int bad_password_error(char *inbuf,char *outbuf)
{
  enum remote_arch_types ra_type = get_remote_arch();

  if(((ra_type == RA_WINNT) || (ra_type == RA_WIN2K)) &&
      (global_client_caps & (CAP_NT_SMBS | CAP_STATUS32 ))) {
    SSVAL(outbuf,smb_flg2,FLAGS2_32_BIT_ERROR_CODES);
    return(ERROR(0,0xc0000000|NT_STATUS_LOGON_FAILURE));
  }

  return(ERROR(ERRSRV,ERRbadpw));
}

/****************************************************************************
reply to a session setup command
****************************************************************************/

int reply_sesssetup_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  uint16 sess_vuid;
  gid_t gid;
  uid_t uid;
  int   smb_bufsize;    
  int   smb_apasslen = 0;   
  pstring smb_apasswd;
  int   smb_ntpasslen = 0;   
  pstring smb_ntpasswd;
  BOOL valid_nt_password = False;
  pstring user;
  pstring orig_user;
  BOOL guest=False;
  static BOOL done_sesssetup = False;
  BOOL doencrypt = SMBENCRYPT();
  char *domain = "";

  *smb_apasswd = 0;
  *smb_ntpasswd = 0;
  
  smb_bufsize = SVAL(inbuf,smb_vwv2);

  if (Protocol < PROTOCOL_NT1) {
    smb_apasslen = SVAL(inbuf,smb_vwv7);
    if (smb_apasslen > MAX_PASS_LEN)
      overflow_attack(smb_apasslen);

    memcpy(smb_apasswd,smb_buf(inbuf),smb_apasslen);
    smb_apasswd[smb_apasslen] = 0;
    pstrcpy(user,smb_buf(inbuf)+smb_apasslen);
    /*
     * Incoming user is in DOS codepage format. Convert
     * to UNIX.
     */
    dos_to_unix(user,True);
  
    if (!doencrypt && (lp_security() != SEC_SERVER)) {
      smb_apasslen = strlen(smb_apasswd);
    }
  } else {
    uint16 passlen1 = SVAL(inbuf,smb_vwv7);
    uint16 passlen2 = SVAL(inbuf,smb_vwv8);
    enum remote_arch_types ra_type = get_remote_arch();
    char *p = smb_buf(inbuf);    

    if(global_client_caps == 0)
      global_client_caps = IVAL(inbuf,smb_vwv11);

    DEBUG(10,("reply_sesssetup_and_X: global_client_caps = 0x%X\n", (unsigned int)global_client_caps));

    /* client_caps is used as final determination if client is NT or Win95. 
       This is needed to return the correct error codes in some
       circumstances.
     */
    
    if(ra_type == RA_WINNT || ra_type == RA_WIN2K || ra_type == RA_WIN95) {
      if(!(global_client_caps & (CAP_NT_SMBS | CAP_STATUS32))) {
        set_remote_arch( RA_WIN95);
      }
    }

    if (passlen1 != 24 && passlen2 != 24)
      doencrypt = False;

    if (passlen1 > MAX_PASS_LEN) {
      overflow_attack(passlen1);
    }

    passlen1 = MIN(passlen1, MAX_PASS_LEN);
    passlen2 = MIN(passlen2, MAX_PASS_LEN);

    if(!doencrypt) {
       /* both Win95 and WinNT stuff up the password lengths for
          non-encrypting systems. Uggh. 
      
          if passlen1==24 its a win95 system, and its setting the
          password length incorrectly. Luckily it still works with the
          default code because Win95 will null terminate the password
          anyway 

          if passlen1>0 and passlen2>0 then maybe its a NT box and its
          setting passlen2 to some random value which really stuffs
          things up. we need to fix that one.  */

      if (passlen1 > 0 && passlen2 > 0 && passlen2 != 24 && passlen2 != 1)
        passlen2 = 0;
    }

    if (lp_restrict_anonymous()) {
      /* there seems to be no reason behind the differences in MS clients formatting
       * various info like the domain, NativeOS, and NativeLanMan fields. Win95
       * in particular seems to have an extra null byte between the username and the
       * domain, or the password length calculation is wrong, which throws off the
       * string extraction routines below.  This makes the value of domain be the
       * empty string, which fails the restrict anonymous check further down.
       * This compensates for that, and allows browsing to work in mixed NT and
       * win95 environments even when restrict anonymous is true. AAB
       */
      dump_data(100, p, 0x70);
      DEBUG(9, ("passlen1=%d, passlen2=%d\n", passlen1, passlen2));
      if (ra_type == RA_WIN95 && !passlen1 && !passlen2 && p[0] == 0 && p[1] == 0) {
        DEBUG(0, ("restrict anonymous parameter used in a win95 environment!\n"));
        DEBUG(0, ("client is win95 and broken passlen1 offset -- attempting fix\n"));
        DEBUG(0, ("if win95 cilents are having difficulty browsing, you will be unable to use restrict anonymous\n"));
        passlen1 = 1;
      }
    }

    if(doencrypt || ((lp_security() == SEC_SERVER) || (lp_security() == SEC_DOMAIN))) {
      /* Save the lanman2 password and the NT md4 password. */
      smb_apasslen = passlen1;
      memcpy(smb_apasswd,p,smb_apasslen);
      smb_apasswd[smb_apasslen] = 0;
      smb_ntpasslen = passlen2;
      memcpy(smb_ntpasswd,p+passlen1,smb_ntpasslen);
      smb_ntpasswd[smb_ntpasslen] = 0;

      /*
       * Ensure the plaintext passwords are in UNIX format.
       */
      if(!doencrypt) {
        dos_to_unix(smb_apasswd,True);
        dos_to_unix(smb_ntpasswd,True);
      }

    } else {
      /* we use the first password that they gave */
      smb_apasslen = passlen1;
      StrnCpy(smb_apasswd,p,smb_apasslen);      
      /*
       * Ensure the plaintext password is in UNIX format.
       */
      dos_to_unix(smb_apasswd,True);
      
      /* trim the password */
      smb_apasslen = strlen(smb_apasswd);

      /* wfwg sometimes uses a space instead of a null */
      if (strequal(smb_apasswd," ")) {
        smb_apasslen = 0;
        *smb_apasswd = 0;
      }
    }
    
    p += passlen1 + passlen2;
    fstrcpy(user,p);
    p = skip_string(p,1);
    /*
     * Incoming user is in DOS codepage format. Convert
     * to UNIX.
     */
    dos_to_unix(user,True);
    domain = p;

    DEBUG(3,("Domain=[%s]  NativeOS=[%s] NativeLanMan=[%s]\n",
	     domain,skip_string(p,1),skip_string(p,2)));
  }


  DEBUG(3,("sesssetupX:name=[%s]\n",user));

  /* If name ends in $ then I think it's asking about whether a */
  /* computer with that name (minus the $) has access. For now */
  /* say yes to everything ending in $. */

  if (*user && (user[strlen(user) - 1] == '$') && (smb_apasslen == 24) && (smb_ntpasslen == 24)) {
    return session_trust_account(conn, inbuf, outbuf, user, 
                                 smb_apasswd, smb_apasslen,
                                 smb_ntpasswd, smb_ntpasslen);
  }

  if (done_sesssetup && lp_restrict_anonymous()) {
    /* tests show that even if browsing is done over already validated connections
     * without a username and password the domain is still provided, which it
     * wouldn't be if it was a purely anonymous connection.  So, in order to
     * restrict anonymous, we only deny connections that have no session
     * information.  If a domain has been provided, then it's not a purely
     * anonymous connection. AAB
     */
    if (!*user && !*smb_apasswd && !*domain) {
      DEBUG(0, ("restrict anonymous is True and anonymous connection attempted. Denying access.\n"));
      return(ERROR(ERRDOS,ERRnoaccess));
    }
  }

  /* If no username is sent use the guest account */
  if (!*user) {
    pstrcpy(user,lp_guestaccount(-1));
    /* If no user and no password then set guest flag. */
    if( *smb_apasswd == 0)
      guest = True;
  }

  strlower(user);

  /*
   * In share level security, only overwrite sesssetup_use if
   * it's a non null-session share. Helps keep %U and %G
   * working.
   */

  if((lp_security() != SEC_SHARE) || (*user && !guest))
    pstrcpy(sesssetup_user,user);

  reload_services(True);

  /*
   * Save the username before mapping. We will use
   * the original username sent to us for security=server
   * and security=domain checking.
   */

  pstrcpy( orig_user, user);

  /*
   * Pass the user through the NT -> unix user mapping
   * function.
   */
   
  (void)map_username(user);

  /*
   * Do any UNIX username case mangling.
   */
  (void)Get_Pwnam( user, True);

  add_session_user(user);

  /*
   * Check if the given username was the guest user with no password.
   */

  if(!guest && strequal(user,lp_guestaccount(-1)) && (*smb_apasswd == 0))
    guest = True;

  /* 
   * Check with orig_user for security=server and
   * security=domain.
   */

  if (!guest && 
			  !check_server_security(orig_user, domain, user,
									 smb_apasswd, smb_apasslen,
									 smb_ntpasswd, smb_ntpasslen) &&
			  !check_domain_security(orig_user, domain, user,
                             smb_apasswd, smb_apasslen,
                             smb_ntpasswd, smb_ntpasslen) &&
      !check_hosts_equiv(user)
     )
  {

    /* 
     * If we get here then the user wasn't guest and the remote
     * authentication methods failed. Check the authentication
     * methods on this local server.
     *
     * If an NT password was supplied try and validate with that
     * first. This is superior as the passwords are mixed case 
     * 128 length unicode.
      */

    if(smb_ntpasslen)
    {
      if(!password_ok(user, smb_ntpasswd,smb_ntpasslen,NULL))
        DEBUG(2,("NT Password did not match for user '%s' ! Defaulting to Lanman\n", user));
      else
        valid_nt_password = True;
    } 

    if (!valid_nt_password && !password_ok(user, smb_apasswd,smb_apasslen,NULL))
    {
      if (lp_security() >= SEC_USER) 
      {
        if (lp_map_to_guest() == NEVER_MAP_TO_GUEST)
        {
          DEBUG(1,("Rejecting user '%s': authentication failed\n", user));
          return bad_password_error(inbuf,outbuf);
        }

        if (lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_USER)
        {
          if (Get_Pwnam(user,True))
          {
            DEBUG(1,("Rejecting user '%s': bad password\n", user));
            return bad_password_error(inbuf,outbuf);
          }
        }

        /*
         * ..else if lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_PASSWORD
         * Then always map to guest account - as done below.
         */
      }

      if (*smb_apasswd || !Get_Pwnam(user,True))
         pstrcpy(user,lp_guestaccount(-1));
      DEBUG(3,("Registered username %s for guest access\n",user));
      guest = True;
    }
  }

  if (!Get_Pwnam(user,True)) {
    DEBUG(3,("No such user %s - using guest account\n",user));
    pstrcpy(user,lp_guestaccount(-1));
    guest = True;
  }

  if (!strequal(user,lp_guestaccount(-1)) &&
      lp_servicenumber(user) < 0)      
  {
    int homes = lp_servicenumber(HOMES_NAME);
    char *home = get_user_home_dir(user);
    if (homes >= 0 && home)
      lp_add_home(user,homes,home);
  }


  /* it's ok - setup a reply */
  if (Protocol < PROTOCOL_NT1) {
    set_message(outbuf,3,0,True);
  } else {
    char *p;
    set_message(outbuf,3,3,True);
    p = smb_buf(outbuf);
    pstrcpy(p,"Unix"); p = skip_string(p,1);
    pstrcpy(p,"Samba "); pstrcat(p,VERSION); p = skip_string(p,1);
    pstrcpy(p,global_myworkgroup); p = skip_string(p,1);
    set_message(outbuf,3,PTR_DIFF(p,smb_buf(outbuf)),False);
    /* perhaps grab OS version here?? */
  }

  /* Set the correct uid in the outgoing and incoming packets
     We will use this on future requests to determine which
     user we should become.
     */
  {
    struct passwd *pw = Get_Pwnam(user,False);
    if (!pw) {
      DEBUG(1,("Username %s is invalid on this system\n",user));
      return bad_password_error(inbuf,outbuf);
    }
    gid = pw->pw_gid;
    uid = pw->pw_uid;
  }

  if (guest)
    SSVAL(outbuf,smb_vwv2,1);

  /* register the name and uid as being validated, so further connections
     to a uid can get through without a password, on the same VC */
  sess_vuid = register_vuid(uid,gid,user,sesssetup_user,guest);
 
  SSVAL(outbuf,smb_uid,sess_vuid);
  SSVAL(inbuf,smb_uid,sess_vuid);

  if (!done_sesssetup)
    max_send = MIN(max_send,smb_bufsize);

  DEBUG(6,("Client requested max send size of %d\n", max_send));

  done_sesssetup = True;

  return chain_reply(inbuf,outbuf,length,bufsize);
}


/****************************************************************************
  reply to a chkpth
****************************************************************************/
int reply_chkpth(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;
  int mode;
  pstring name;
  BOOL ok = False;
  BOOL bad_path = False;
  SMB_STRUCT_STAT st;
 
  pstrcpy(name,smb_buf(inbuf) + 1);
  unix_convert(name,conn,0,&bad_path,&st);

  mode = SVAL(inbuf,smb_vwv0);

  if (check_name(name,conn)) {
    if(VALID_STAT(st))
      ok = S_ISDIR(st.st_mode);
    else
      ok = dos_directory_exist(name,NULL);
  }

  if (!ok)
  {
    /* We special case this - as when a Windows machine
       is parsing a path is steps through the components
       one at a time - if a component fails it expects
       ERRbadpath, not ERRbadfile.
     */
    if(errno == ENOENT)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }

#if 0
    /* Ugly - NT specific hack - maybe not needed ? (JRA) */
    if((errno == ENOTDIR) && (Protocol >= PROTOCOL_NT1) &&
       (get_remote_arch() == RA_WINNT))
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbaddirectory;
    }
#endif

    return(UNIXERROR(ERRDOS,ERRbadpath));
  }

  outsize = set_message(outbuf,0,0,True);

  DEBUG(3,("chkpth %s mode=%d\n", name, mode));

  return(outsize);
}


/****************************************************************************
  reply to a getatr
****************************************************************************/
int reply_getatr(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring fname;
  int outsize = 0;
  SMB_STRUCT_STAT sbuf;
  BOOL ok = False;
  int mode=0;
  SMB_OFF_T size=0;
  time_t mtime=0;
  BOOL bad_path = False;
 
  pstrcpy(fname,smb_buf(inbuf) + 1);

  /* dos smetimes asks for a stat of "" - it returns a "hidden directory"
     under WfWg - weird! */
  if (! (*fname))
  {
    mode = aHIDDEN | aDIR;
    if (!CAN_WRITE(conn)) mode |= aRONLY;
    size = 0;
    mtime = 0;
    ok = True;
  }
  else
  {
    unix_convert(fname,conn,0,&bad_path,&sbuf);
    if (check_name(fname,conn))
    {
      if (VALID_STAT(sbuf) || dos_stat(fname,&sbuf) == 0)
      {
        mode = dos_mode(conn,fname,&sbuf);
        size = sbuf.st_size;
        mtime = sbuf.st_mtime;
        if (mode & aDIR)
          size = 0;
        ok = True;
      }
      else
        DEBUG(3,("stat of %s failed (%s)\n",fname,strerror(errno)));
    }
  }
  
  if (!ok)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }

    return(UNIXERROR(ERRDOS,ERRbadfile));
  }
 
  outsize = set_message(outbuf,10,0,True);

  SSVAL(outbuf,smb_vwv0,mode);
  if(lp_dos_filetime_resolution(SNUM(conn)) )
    put_dos_date3(outbuf,smb_vwv1,mtime & ~1);
  else
    put_dos_date3(outbuf,smb_vwv1,mtime);
  SIVAL(outbuf,smb_vwv3,(uint32)size);

  if (Protocol >= PROTOCOL_NT1) {
    char *p = strrchr(fname,'/');
    uint16 flg2 = SVAL(outbuf,smb_flg2);
    if (!p) p = fname;
    if (!is_8_3(fname, True))
      SSVAL(outbuf,smb_flg2,flg2 | 0x40); /* IS_LONG_NAME */
  }
  
  DEBUG( 3, ( "getatr name=%s mode=%d size=%d\n", fname, mode, (uint32)size ) );
  
  return(outsize);
}


/****************************************************************************
  reply to a setatr
****************************************************************************/
int reply_setatr(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring fname;
  int outsize = 0;
  BOOL ok=False;
  int mode;
  time_t mtime;
  SMB_STRUCT_STAT st;
  BOOL bad_path = False;
 
  pstrcpy(fname,smb_buf(inbuf) + 1);
  unix_convert(fname,conn,0,&bad_path,&st);

  mode = SVAL(inbuf,smb_vwv0);
  mtime = make_unix_date3(inbuf+smb_vwv1);
  
  if (VALID_STAT_OF_DIR(st) || dos_directory_exist(fname,NULL))
    mode |= aDIR;
  if (check_name(fname,conn))
    ok =  (file_chmod(conn,fname,mode,NULL) == 0);
  if (ok)
    ok = set_filetime(conn,fname,mtime);
  
  if (!ok)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }

    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }
 
  outsize = set_message(outbuf,0,0,True);
  
  DEBUG( 3, ( "setatr name=%s mode=%d\n", fname, mode ) );
  
  return(outsize);
}


/****************************************************************************
  reply to a dskattr
****************************************************************************/
int reply_dskattr(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;
  SMB_BIG_UINT dfree,dsize,bsize;
  
  sys_disk_free(".",True,&bsize,&dfree,&dsize);
  
  outsize = set_message(outbuf,5,0,True);
  
  SSVAL(outbuf,smb_vwv0,dsize);
  SSVAL(outbuf,smb_vwv1,bsize/512);
  SSVAL(outbuf,smb_vwv2,512);
  SSVAL(outbuf,smb_vwv3,dfree);

  DEBUG(3,("dskattr dfree=%d\n", (unsigned int)dfree));

  return(outsize);
}


/****************************************************************************
  reply to a search
  Can be called from SMBsearch, SMBffirst or SMBfunique.
****************************************************************************/
int reply_search(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring mask;
  pstring directory;
  pstring fname;
  SMB_OFF_T size;
  int mode;
  time_t date;
  int dirtype;
  int outsize = 0;
  int numentries = 0;
  BOOL finished = False;
  int maxentries;
  int i;
  char *p;
  BOOL ok = False;
  int status_len;
  char *path;
  char status[21];
  int dptr_num= -1;
  BOOL check_descend = False;
  BOOL expect_close = False;
  BOOL can_open = True;
  BOOL bad_path = False;

  *mask = *directory = *fname = 0;

  /* If we were called as SMBffirst then we must expect close. */
  if(CVAL(inbuf,smb_com) == SMBffirst)
    expect_close = True;
  
  outsize = set_message(outbuf,1,3,True);
  maxentries = SVAL(inbuf,smb_vwv0); 
  dirtype = SVAL(inbuf,smb_vwv1);
  path = smb_buf(inbuf) + 1;
  status_len = SVAL(smb_buf(inbuf),3 + strlen(path));

  
  /* dirtype &= ~aDIR; */
  
  DEBUG(5,("reply_search: path=%s status_len=%d\n",path,status_len));

  
  if (status_len == 0)
  {
    pstring dir2;

    pstrcpy(directory,smb_buf(inbuf)+1);
    pstrcpy(dir2,smb_buf(inbuf)+1);
    unix_convert(directory,conn,0,&bad_path,NULL);
    unix_format(dir2);

    if (!check_name(directory,conn))
      can_open = False;

    p = strrchr(dir2,'/');
    if (p == NULL) 
    {
      pstrcpy(mask,dir2);
      *dir2 = 0;
    }
    else
    {
      *p = 0;
      pstrcpy(mask,p+1);
    }

    p = strrchr(directory,'/');
    if (!p) 
      *directory = 0;
    else
      *p = 0;

    if (strlen(directory) == 0)
      pstrcpy(directory,"./");
    memset((char *)status,'\0',21);
    CVAL(status,0) = dirtype;
  }
  else
  {
    memcpy(status,smb_buf(inbuf) + 1 + strlen(path) + 4,21);
    memcpy(mask,status+1,11);
    mask[11] = 0;
    dirtype = CVAL(status,0) & 0x1F;
    conn->dirptr = dptr_fetch(status+12,&dptr_num);      
    if (!conn->dirptr)
      goto SearchEmpty;
    string_set(&conn->dirpath,dptr_path(dptr_num));
    if (!case_sensitive)
      strnorm(mask);
  }

  /* turn strings of spaces into a . */  
  {
    trim_string(mask,NULL," ");
    if ((p = strrchr(mask,' ')))
    {
      fstring ext;
      fstrcpy(ext,p+1);
      *p = 0;
      trim_string(mask,NULL," ");
      pstrcat(mask,".");
      pstrcat(mask,ext);
    }
  }

  /* Convert the formatted mask. (This code lives in trans2.c) */
  mask_convert(mask);

  {
    int skip;
    p = mask;
    while(*p)
    {
      if((skip = get_character_len( *p )) != 0 )
      {
        p += skip;
      }
      else
      {
        if (*p != '?' && *p != '*' && !isdoschar(*p))
        {
          DEBUG(5,("Invalid char [%c] in search mask?\n",*p));
          *p = '?';
        }
        p++;
      }
    }
  }

  if (!strchr(mask,'.') && strlen(mask)>8)
  {
    fstring tmp;
    fstrcpy(tmp,&mask[8]);
    mask[8] = '.';
    mask[9] = 0;
    pstrcat(mask,tmp);
  }

  DEBUG(5,("mask=%s directory=%s\n",mask,directory));
  
  if (can_open)
  {
    p = smb_buf(outbuf) + 3;
      
    ok = True;
     
    if (status_len == 0)
    {
      dptr_num = dptr_create(conn,directory,True,expect_close,SVAL(inbuf,smb_pid));
      if (dptr_num < 0)
      {
        if(dptr_num == -2)
        {
          if((errno == ENOENT) && bad_path)
          {
            unix_ERR_class = ERRDOS;
            unix_ERR_code = ERRbadpath;
          }
          return (UNIXERROR(ERRDOS,ERRnofids));
        }
        return(ERROR(ERRDOS,ERRnofids));
      }
    }

    DEBUG(4,("dptr_num is %d\n",dptr_num));

    if (ok)
    {
      if ((dirtype&0x1F) == aVOLID)
      {	  
        memcpy(p,status,21);
        make_dir_struct(p,"???????????",volume_label(SNUM(conn)),0,aVOLID,0);
        dptr_fill(p+12,dptr_num);
        if (dptr_zero(p+12) && (status_len==0))
          numentries = 1;
        else
          numentries = 0;
        p += DIR_STRUCT_SIZE;
      }
      else 
      {
        DEBUG(8,("dirpath=<%s> dontdescend=<%s>\n",
              conn->dirpath,lp_dontdescend(SNUM(conn))));
        if (in_list(conn->dirpath, lp_dontdescend(SNUM(conn)),True))
          check_descend = True;

        for (i=numentries;(i<maxentries) && !finished;i++)
        {
	  /* check to make sure we have room in the buffer */
	  if ( ((PTR_DIFF(p, outbuf))+DIR_STRUCT_SIZE) > BUFFER_SIZE )
	  	break;
          finished = 
            !get_dir_entry(conn,mask,dirtype,fname,&size,&mode,&date,check_descend);
          if (!finished)
          {
            memcpy(p,status,21);
            make_dir_struct(p,mask,fname,size,mode,date);
            dptr_fill(p+12,dptr_num);
            numentries++;
          }
          p += DIR_STRUCT_SIZE;
        }
      }
	} /* if (ok ) */
  }


  SearchEmpty:

  if (numentries == 0 || !ok)
  {
    CVAL(outbuf,smb_rcls) = ERRDOS;
    SSVAL(outbuf,smb_err,ERRnofiles);
    dptr_close(&dptr_num);
  }

  /* If we were called as SMBffirst with smb_search_id == NULL
     and no entries were found then return error and close dirptr 
     (X/Open spec) */

  if(ok && expect_close && numentries == 0 && status_len == 0)
  {
    CVAL(outbuf,smb_rcls) = ERRDOS;
    SSVAL(outbuf,smb_err,ERRnofiles);
    /* Also close the dptr - we know it's gone */
    dptr_close(&dptr_num);
  }

  /* If we were called as SMBfunique, then we can close the dirptr now ! */
  if(dptr_num >= 0 && CVAL(inbuf,smb_com) == SMBfunique)
    dptr_close(&dptr_num);

  SSVAL(outbuf,smb_vwv0,numentries);
  SSVAL(outbuf,smb_vwv1,3 + numentries * DIR_STRUCT_SIZE);
  CVAL(smb_buf(outbuf),0) = 5;
  SSVAL(smb_buf(outbuf),1,numentries*DIR_STRUCT_SIZE);

  if (Protocol >= PROTOCOL_NT1) {
    uint16 flg2 = SVAL(outbuf,smb_flg2);
    SSVAL(outbuf,smb_flg2,flg2 | 0x40); /* IS_LONG_NAME */
  }
  
  outsize += DIR_STRUCT_SIZE*numentries;
  smb_setlen(outbuf,outsize - 4);
  
  if ((! *directory) && dptr_path(dptr_num))
    slprintf(directory, sizeof(directory)-1, "(%s)",dptr_path(dptr_num));

  DEBUG( 4, ( "%s mask=%s path=%s dtype=%d nument=%d of %d\n",
        smb_fn_name(CVAL(inbuf,smb_com)), 
        mask, directory, dirtype, numentries, maxentries ) );

  return(outsize);
}


/****************************************************************************
  reply to a fclose (stop directory search)
****************************************************************************/
int reply_fclose(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;
  int status_len;
  char *path;
  char status[21];
  int dptr_num= -2;

  outsize = set_message(outbuf,1,0,True);
  path = smb_buf(inbuf) + 1;
  status_len = SVAL(smb_buf(inbuf),3 + strlen(path));

  
  if (status_len == 0)
    return(ERROR(ERRSRV,ERRsrverror));

  memcpy(status,smb_buf(inbuf) + 1 + strlen(path) + 4,21);

  if(dptr_fetch(status+12,&dptr_num)) {
    /*  Close the dptr - we know it's gone */
    dptr_close(&dptr_num);
  }

  SSVAL(outbuf,smb_vwv0,0);

  DEBUG(3,("search close\n"));

  return(outsize);
}


/****************************************************************************
  reply to an open
****************************************************************************/

int reply_open(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring fname;
  int outsize = 0;
  int fmode=0;
  int share_mode;
  SMB_OFF_T size = 0;
  time_t mtime=0;
  mode_t unixmode;
  int rmode=0;
  SMB_STRUCT_STAT sbuf;
  BOOL bad_path = False;
  files_struct *fsp;
  int oplock_request = CORE_OPLOCK_REQUEST(inbuf);
 
  share_mode = SVAL(inbuf,smb_vwv0);

  pstrcpy(fname,smb_buf(inbuf)+1);
  unix_convert(fname,conn,0,&bad_path,NULL);
    
  fsp = file_new();
  if (!fsp)
    return(ERROR(ERRSRV,ERRnofids));

  unixmode = unix_mode(conn,aARCH,fname);
      
  open_file_shared(fsp,conn,fname,share_mode,(FILE_FAIL_IF_NOT_EXIST|FILE_EXISTS_OPEN),
                   unixmode, oplock_request,&rmode,NULL);

  if (!fsp->open)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    file_free(fsp);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  if (sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
    close_file(fsp,False);
    return(ERROR(ERRDOS,ERRnoaccess));
  }
    
  size = sbuf.st_size;
  fmode = dos_mode(conn,fname,&sbuf);
  mtime = sbuf.st_mtime;

  if (fmode & aDIR) {
    DEBUG(3,("attempt to open a directory %s\n",fname));
    close_file(fsp,False);
    return(ERROR(ERRDOS,ERRnoaccess));
  }
  
  outsize = set_message(outbuf,7,0,True);
  SSVAL(outbuf,smb_vwv0,fsp->fnum);
  SSVAL(outbuf,smb_vwv1,fmode);
  if(lp_dos_filetime_resolution(SNUM(conn)) )
    put_dos_date3(outbuf,smb_vwv2,mtime & ~1);
  else
    put_dos_date3(outbuf,smb_vwv2,mtime);
  SIVAL(outbuf,smb_vwv4,(uint32)size);
  SSVAL(outbuf,smb_vwv6,rmode);

  if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
  }
    
  if(EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
  return(outsize);
}


/****************************************************************************
  reply to an open and X
****************************************************************************/
int reply_open_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  pstring fname;
  int smb_mode = SVAL(inbuf,smb_vwv3);
  int smb_attr = SVAL(inbuf,smb_vwv5);
  /* Breakout the oplock request bits so we can set the
     reply bits separately. */
  BOOL ex_oplock_request = EXTENDED_OPLOCK_REQUEST(inbuf);
  BOOL core_oplock_request = CORE_OPLOCK_REQUEST(inbuf);
  BOOL oplock_request = ex_oplock_request | core_oplock_request;
#if 0
  int open_flags = SVAL(inbuf,smb_vwv2);
  int smb_sattr = SVAL(inbuf,smb_vwv4); 
  uint32 smb_time = make_unix_date3(inbuf+smb_vwv6);
#endif
  int smb_ofun = SVAL(inbuf,smb_vwv8);
  mode_t unixmode;
  SMB_OFF_T size=0;
  int fmode=0,mtime=0,rmode=0;
  SMB_STRUCT_STAT sbuf;
  int smb_action = 0;
  BOOL bad_path = False;
  files_struct *fsp;

  /* If it's an IPC, pass off the pipe handler. */
  if (IS_IPC(conn) && lp_nt_pipe_support())
    return reply_open_pipe_and_X(conn, inbuf,outbuf,length,bufsize);

  /* XXXX we need to handle passed times, sattr and flags */

  pstrcpy(fname,smb_buf(inbuf));
  unix_convert(fname,conn,0,&bad_path,NULL);
    
  fsp = file_new();
  if (!fsp)
    return(ERROR(ERRSRV,ERRnofids));

  unixmode = unix_mode(conn,smb_attr | aARCH, fname);
      
  open_file_shared(fsp,conn,fname,smb_mode,smb_ofun,unixmode,
	               oplock_request, &rmode,&smb_action);
      
  if (!fsp->open)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    file_free(fsp);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  if (sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
    close_file(fsp,False);
    return(ERROR(ERRDOS,ERRnoaccess));
  }

  size = sbuf.st_size;
  fmode = dos_mode(conn,fname,&sbuf);
  mtime = sbuf.st_mtime;
  if (fmode & aDIR) {
    close_file(fsp,False);
    return(ERROR(ERRDOS,ERRnoaccess));
  }

  /* If the caller set the extended oplock request bit
     and we granted one (by whatever means) - set the
     correct bit for extended oplock reply.
   */

  if (ex_oplock_request && lp_fake_oplocks(SNUM(conn))) {
    smb_action |= EXTENDED_OPLOCK_GRANTED;
  }

  if(ex_oplock_request && EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
    smb_action |= EXTENDED_OPLOCK_GRANTED;
  }

  /* If the caller set the core oplock request bit
     and we granted one (by whatever means) - set the
     correct bit for core oplock reply.
   */

  if (core_oplock_request && lp_fake_oplocks(SNUM(conn))) {
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
  }

  if(core_oplock_request && EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
  }

  set_message(outbuf,15,0,True);
  SSVAL(outbuf,smb_vwv2,fsp->fnum);
  SSVAL(outbuf,smb_vwv3,fmode);
  if(lp_dos_filetime_resolution(SNUM(conn)) )
    put_dos_date3(outbuf,smb_vwv4,mtime & ~1);
  else
    put_dos_date3(outbuf,smb_vwv4,mtime);
  SIVAL(outbuf,smb_vwv6,(uint32)size);
  SSVAL(outbuf,smb_vwv8,rmode);
  SSVAL(outbuf,smb_vwv11,smb_action);

  return chain_reply(inbuf,outbuf,length,bufsize);
}


/****************************************************************************
  reply to a SMBulogoffX
****************************************************************************/
int reply_ulogoffX(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  uint16 vuid = SVAL(inbuf,smb_uid);
  user_struct *vuser = get_valid_user_struct(vuid);

  if(vuser == 0) {
    DEBUG(3,("ulogoff, vuser id %d does not map to user.\n", vuid));
  }

  /* in user level security we are supposed to close any files
     open by this user */
  if ((vuser != 0) && (lp_security() != SEC_SHARE)) {
	  file_close_user(vuid);
  }

  invalidate_vuid(vuid);

  set_message(outbuf,2,0,True);

  DEBUG( 3, ( "ulogoffX vuid=%d\n", vuid ) );

  return chain_reply(inbuf,outbuf,length,bufsize);
}


/****************************************************************************
  reply to a mknew or a create
****************************************************************************/
int reply_mknew(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring fname;
  int com;
  int outsize = 0;
  int createmode;
  mode_t unixmode;
  int ofun = 0;
  BOOL bad_path = False;
  files_struct *fsp;
  int oplock_request = CORE_OPLOCK_REQUEST(inbuf);
 
  com = SVAL(inbuf,smb_com);

  createmode = SVAL(inbuf,smb_vwv0);
  pstrcpy(fname,smb_buf(inbuf)+1);
  unix_convert(fname,conn,0,&bad_path,NULL);

  if (createmode & aVOLID)
    {
      DEBUG(0,("Attempt to create file (%s) with volid set - please report this\n",fname));
    }
  
  unixmode = unix_mode(conn,createmode,fname);
  
  fsp = file_new();
  if (!fsp)
    return(ERROR(ERRSRV,ERRnofids));

  if(com == SMBmknew)
  {
    /* We should fail if file exists. */
    ofun = FILE_CREATE_IF_NOT_EXIST;
  }
  else
  {
    /* SMBcreate - Create if file doesn't exist, truncate if it does. */
    ofun = FILE_CREATE_IF_NOT_EXIST|FILE_EXISTS_TRUNCATE;
  }

  /* Open file in dos compatibility share mode. */
  open_file_shared(fsp,conn,fname,SET_DENY_MODE(DENY_FCB)|SET_OPEN_MODE(DOS_OPEN_FCB), 
                   ofun, unixmode, oplock_request, NULL, NULL);
  
  if (!fsp->open)
  {
    if((errno == ENOENT) && bad_path) 
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    file_free(fsp);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }
 
  outsize = set_message(outbuf,1,0,True);
  SSVAL(outbuf,smb_vwv0,fsp->fnum);

  if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
  }
 
  if(EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
 
  DEBUG( 2, ( "new file %s\n", fname ) );
  DEBUG( 3, ( "mknew %s fd=%d dmode=%d umode=%o\n",
        fname, fsp->fd_ptr->fd, createmode, (int)unixmode ) );

  return(outsize);
}


/****************************************************************************
  reply to a create temporary file
****************************************************************************/
int reply_ctemp(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring fname;
  pstring fname2;
  int outsize = 0;
  int createmode;
  mode_t unixmode;
  BOOL bad_path = False;
  files_struct *fsp;
  int oplock_request = CORE_OPLOCK_REQUEST(inbuf);
 
  createmode = SVAL(inbuf,smb_vwv0);
  pstrcpy(fname,smb_buf(inbuf)+1);
  pstrcat(fname,"/TMXXXXXX");
  unix_convert(fname,conn,0,&bad_path,NULL);
  
  unixmode = unix_mode(conn,createmode,fname);
  
  fsp = file_new();
  if (fsp)
    return(ERROR(ERRSRV,ERRnofids));

  pstrcpy(fname2,(char *)smbd_mktemp(fname));

  /* Open file in dos compatibility share mode. */
  /* We should fail if file exists. */
  open_file_shared(fsp,conn,fname2,SET_DENY_MODE(DENY_FCB)|SET_OPEN_MODE(DOS_OPEN_FCB), 
                   (FILE_CREATE_IF_NOT_EXIST|FILE_EXISTS_FAIL), unixmode, oplock_request, NULL, NULL);

  if (!fsp->open)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    file_free(fsp);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  outsize = set_message(outbuf,1,2 + strlen(fname2),True);
  SSVAL(outbuf,smb_vwv0,fsp->fnum);
  CVAL(smb_buf(outbuf),0) = 4;
  pstrcpy(smb_buf(outbuf) + 1,fname2);

  if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;
  }
  
  if(EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
    CVAL(outbuf,smb_flg) |= CORE_OPLOCK_GRANTED;

  DEBUG( 2, ( "created temp file %s\n", fname2 ) );
  DEBUG( 3, ( "ctemp %s fd=%d dmode=%d umode=%o\n",
        fname2, fsp->fd_ptr->fd, createmode, (int)unixmode ) );

  return(outsize);
}


/*******************************************************************
check if a user is allowed to delete a file
********************************************************************/
static BOOL can_delete(char *fname,connection_struct *conn, int dirtype)
{
  SMB_STRUCT_STAT sbuf;
  int fmode;

  if (!CAN_WRITE(conn)) return(False);

  if (dos_lstat(fname,&sbuf) != 0) return(False);
  fmode = dos_mode(conn,fname,&sbuf);
  if (fmode & aDIR) return(False);
  if (!lp_delete_readonly(SNUM(conn))) {
    if (fmode & aRONLY) return(False);
  }
  if ((fmode & ~dirtype) & (aHIDDEN | aSYSTEM))
    return(False);
  if (!check_file_sharing(conn,fname,False)) return(False);
  return(True);
}

/****************************************************************************
 Reply to a unlink
****************************************************************************/

int reply_unlink(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;
  pstring name;
  int dirtype;
  pstring directory;
  pstring mask;
  char *p;
  int count=0;
  int error = ERRnoaccess;
  BOOL has_wild;
  BOOL exists=False;
  BOOL bad_path = False;
  BOOL rc = True;

  *directory = *mask = 0;

  dirtype = SVAL(inbuf,smb_vwv0);
  
  pstrcpy(name,smb_buf(inbuf) + 1);
   
  DEBUG(3,("reply_unlink : %s\n",name));
   
  rc = unix_convert(name,conn,0,&bad_path,NULL);

  p = strrchr(name,'/');
  if (!p) {
    pstrcpy(directory,"./");
    pstrcpy(mask,name);
  } else {
    *p = 0;
    pstrcpy(directory,name);
    pstrcpy(mask,p+1);
  }

  /*
   * We should only check the mangled cache
   * here if unix_convert failed. This means
   * that the path in 'mask' doesn't exist
   * on the file system and so we need to look
   * for a possible mangle. This patch from
   * Tine Smukavec <valentin.smukavec@hermes.si>.
   */

  if (!rc && is_mangled(mask))
    check_mangled_cache( mask );

  has_wild = strchr(mask,'*') || strchr(mask,'?');

  if (!has_wild) {
    pstrcat(directory,"/");
    pstrcat(directory,mask);
    if (can_delete(directory,conn,dirtype) && !dos_unlink(directory))
      count++;
    if (!count)
      exists = dos_file_exist(directory,NULL);    
  } else {
    void *dirptr = NULL;
    char *dname;

    if (check_name(directory,conn))
      dirptr = OpenDir(conn, directory, True);

    /* XXXX the CIFS spec says that if bit0 of the flags2 field is set then
       the pattern matches against the long name, otherwise the short name 
       We don't implement this yet XXXX
       */

    if (dirptr)
      {
	error = ERRbadfile;

	if (strequal(mask,"????????.???"))
	  pstrcpy(mask,"*");

	while ((dname = ReadDirName(dirptr)))
	  {
	    pstring fname;
	    pstrcpy(fname,dname);
	    
	    if(!mask_match(fname, mask, case_sensitive, False)) continue;

	    error = ERRnoaccess;
	    slprintf(fname,sizeof(fname)-1, "%s/%s",directory,dname);
	    if (!can_delete(fname,conn,dirtype)) continue;
	    if (!dos_unlink(fname)) count++;
	    DEBUG(3,("reply_unlink : doing unlink on %s\n",fname));
	  }
	CloseDir(dirptr);
      }
  }
  
  if (count == 0) {
    if (exists)
      return(ERROR(ERRDOS,error));
    else
    {
      if((errno == ENOENT) && bad_path)
      {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      return(UNIXERROR(ERRDOS,error));
    }
  }
  
  outsize = set_message(outbuf,0,0,True);
  
  return(outsize);
}


/****************************************************************************
   reply to a readbraw (core+ protocol)
****************************************************************************/

int reply_readbraw(connection_struct *conn, char *inbuf, char *outbuf, int dum_size, int dum_buffsize)
{
  size_t maxcount,mincount;
  size_t nread = 0;
  SMB_OFF_T startpos;
  char *header = outbuf;
  ssize_t ret=0;
  files_struct *fsp;

  /*
   * Special check if an oplock break has been issued
   * and the readraw request croses on the wire, we must
   * return a zero length response here.
   */

  if(global_oplock_break)
  {
    _smb_setlen(header,0);
    transfer_file(0,Client,(SMB_OFF_T)0,header,4,0);
    DEBUG(5,("readbraw - oplock break finished\n"));
    return -1;
  }

  fsp = file_fsp(inbuf,smb_vwv0);

  if (!FNUM_OK(fsp,conn) || !fsp->can_read) {
	  /*
	   * fsp could be NULL here so use the value from the packet. JRA.
	   */
	  DEBUG(3,("fnum %d not open in readbraw - cache prime?\n",(int)SVAL(inbuf,smb_vwv0)));
	  _smb_setlen(header,0);
	  transfer_file(0,Client,(SMB_OFF_T)0,header,4,0);
	  return(-1);
  }

  CHECK_FSP(fsp,conn);

  flush_write_cache(fsp, READRAW_FLUSH);

  startpos = IVAL(inbuf,smb_vwv1);
  if(CVAL(inbuf,smb_wct) == 10) {
    /*
     * This is a large offset (64 bit) read.
     */
#ifdef LARGE_SMB_OFF_T

    startpos |= (((SMB_OFF_T)IVAL(inbuf,smb_vwv8)) << 32);

#else /* !LARGE_SMB_OFF_T */

    /*
     * Ensure we haven't been sent a >32 bit offset.
     */

    if(IVAL(inbuf,smb_vwv8) != 0) {
      DEBUG(0,("readbraw - large offset (%x << 32) used and we don't support \
64 bit offsets.\n", (unsigned int)IVAL(inbuf,smb_vwv8) ));
      _smb_setlen(header,0);
      transfer_file(0,Client,(SMB_OFF_T)0,header,4,0);
      return(-1);
    }

#endif /* LARGE_SMB_OFF_T */

    if(startpos < 0) {
      DEBUG(0,("readbraw - negative 64 bit readraw offset (%.0f) !\n",
            (double)startpos ));
	  _smb_setlen(header,0);
	  transfer_file(0,Client,(SMB_OFF_T)0,header,4,0);
	  return(-1);
    }      
  }
  maxcount = (SVAL(inbuf,smb_vwv3) & 0xFFFF);
  mincount = (SVAL(inbuf,smb_vwv4) & 0xFFFF);

  /* ensure we don't overrun the packet size */
  maxcount = MIN(65535,maxcount);
  maxcount = MAX(mincount,maxcount);

  if (!is_locked(fsp,conn,maxcount,startpos, F_RDLCK))
  {
    SMB_OFF_T size = fsp->size;
    SMB_OFF_T sizeneeded = startpos + maxcount;
	    
    if (size < sizeneeded)
    {
      SMB_STRUCT_STAT st;
      if (sys_fstat(fsp->fd_ptr->fd,&st) == 0)
        size = st.st_size;
      if (!fsp->can_write) 
        fsp->size = size;
    }

    nread = MIN(maxcount,(size - startpos));	  
  }

  if (nread < mincount)
    nread = 0;
  
  DEBUG( 3, ( "readbraw fnum=%d start=%.0f max=%d min=%d nread=%d\n",
	      fsp->fnum, (double)startpos,
	      (int)maxcount, (int)mincount, (int)nread ) );
  
#if UNSAFE_READRAW
  {
    BOOL seek_fail = False;
    int predict=0;
    _smb_setlen(header,nread);

#if USE_READ_PREDICTION
    if (!fsp->can_write)
      predict = read_predict(fsp->fd_ptr->fd,startpos,header+4,NULL,nread);
#endif /* USE_READ_PREDICTION */

    if ((nread-predict) > 0) {
      if(seek_file(fsp,startpos + predict) == -1) {
        DEBUG(0,("reply_readbraw: ERROR: seek_file failed.\n"));
        ret = 0;
        seek_fail = True;
      } 
    }

    if(!seek_fail)
      ret = (ssize_t)transfer_file(fsp->fd_ptr->fd,Client,
                                   (SMB_OFF_T)(nread-predict),header,4+predict, 
                                   startpos+predict);
  }

  if (ret != nread+4)
    DEBUG(0,("ERROR: file read failure on %s at %d for %d bytes (%d)\n",
	     fsp->fsp_name,startpos,nread,ret));

#else /* UNSAFE_READRAW */
  ret = read_file(fsp,header+4,startpos,nread);
  if (ret < mincount) ret = 0;

  _smb_setlen(header,ret);
  transfer_file(0,Client,0,header,4+ret,0);
#endif /* UNSAFE_READRAW */

  DEBUG(5,("readbraw finished\n"));
  return -1;
}


/****************************************************************************
  reply to a lockread (core+ protocol)
****************************************************************************/
int reply_lockread(connection_struct *conn, char *inbuf,char *outbuf, int length, int dum_buffsiz)
{
  ssize_t nread = -1;
  char *data;
  int outsize = 0;
  SMB_OFF_T startpos;
  size_t numtoread;
  int eclass;
  uint32 ecode;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  CHECK_FSP(fsp,conn);
  CHECK_READ(fsp);
  CHECK_ERROR(fsp);

  numtoread = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv2);
  
  outsize = set_message(outbuf,5,3,True);
  numtoread = MIN(BUFFER_SIZE-outsize,numtoread);
  data = smb_buf(outbuf) + 3;
 
  /*
   * NB. Discovered by Menny Hamburger at Mainsoft. This is a core+
   * protocol request that predates the read/write lock concept. 
   * Thus instead of asking for a read lock here we need to ask
   * for a write lock. JRA.
   */

  if(!do_lock( fsp, conn, numtoread, startpos, F_WRLCK, &eclass, &ecode)) {
    if((ecode == ERRlock) && lp_blocking_locks(SNUM(conn))) {
      /*
       * A blocking lock was requested. Package up
       * this smb into a queued request and push it
       * onto the blocking lock queue.
       */
      if(push_blocking_lock_request(inbuf, length, -1, 0))
        return -1;
    }
    return (ERROR(eclass,ecode));
  }

  nread = read_file(fsp,data,startpos,numtoread);

  if (nread < 0)
    return(UNIXERROR(ERRDOS,ERRnoaccess));

  outsize += nread;
  SSVAL(outbuf,smb_vwv0,nread);
  SSVAL(outbuf,smb_vwv5,nread+3);
  SSVAL(smb_buf(outbuf),1,nread);

  DEBUG( 3, ( "lockread fnum=%d num=%d nread=%d\n",
            fsp->fnum, (int)numtoread, (int)nread ) );

  return(outsize);
}


/****************************************************************************
  reply to a read
****************************************************************************/

int reply_read(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  size_t numtoread;
  ssize_t nread = 0;
  char *data;
  SMB_OFF_T startpos;
  int outsize = 0;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  CHECK_FSP(fsp,conn);
  CHECK_READ(fsp);
  CHECK_ERROR(fsp);

  numtoread = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv2);
  
  outsize = set_message(outbuf,5,3,True);
  numtoread = MIN(BUFFER_SIZE-outsize,numtoread);
  data = smb_buf(outbuf) + 3;
  
  if (is_locked(fsp,conn,numtoread,startpos, F_RDLCK))
    return(ERROR(ERRDOS,ERRlock));	

  if (numtoread > 0)
    nread = read_file(fsp,data,startpos,numtoread);
  
  if (nread < 0)
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  
  outsize += nread;
  SSVAL(outbuf,smb_vwv0,nread);
  SSVAL(outbuf,smb_vwv5,nread+3);
  CVAL(smb_buf(outbuf),0) = 1;
  SSVAL(smb_buf(outbuf),1,nread);
  
  DEBUG( 3, ( "read fnum=%d num=%d nread=%d\n",
            fsp->fnum, (int)numtoread, (int)nread ) );

  return(outsize);
}


/****************************************************************************
  reply to a read and X
****************************************************************************/
int reply_read_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  files_struct *fsp = file_fsp(inbuf,smb_vwv2);
  SMB_OFF_T startpos = IVAL(inbuf,smb_vwv3);
  size_t smb_maxcnt = SVAL(inbuf,smb_vwv5);
  size_t smb_mincnt = SVAL(inbuf,smb_vwv6);
  ssize_t nread = -1;
  char *data;

  /* If it's an IPC, pass off the pipe handler. */
  if (IS_IPC(conn))
    return reply_pipe_read_and_X(inbuf,outbuf,length,bufsize);

  CHECK_FSP(fsp,conn);
  CHECK_READ(fsp);
  CHECK_ERROR(fsp);

  set_message(outbuf,12,0,True);
  data = smb_buf(outbuf);

  if(CVAL(inbuf,smb_wct) == 12) {
#ifdef LARGE_SMB_OFF_T
    /*
     * This is a large offset (64 bit) read.
     */
    startpos |= (((SMB_OFF_T)IVAL(inbuf,smb_vwv10)) << 32);

#else /* !LARGE_SMB_OFF_T */

    /*
     * Ensure we haven't been sent a >32 bit offset.
     */

    if(IVAL(inbuf,smb_vwv10) != 0) {
      DEBUG(0,("reply_read_and_X - large offset (%x << 32) used and we don't support \
64 bit offsets.\n", (unsigned int)IVAL(inbuf,smb_vwv10) ));
      return(ERROR(ERRDOS,ERRbadaccess));
    }

#endif /* LARGE_SMB_OFF_T */

  }

  if (is_locked(fsp,conn,smb_maxcnt,startpos, F_RDLCK))
    return(ERROR(ERRDOS,ERRlock));
  nread = read_file(fsp,data,startpos,smb_maxcnt);
  
  if (nread < 0)
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  
  SSVAL(outbuf,smb_vwv5,nread);
  SSVAL(outbuf,smb_vwv6,smb_offset(data,outbuf));
  SSVAL(smb_buf(outbuf),-2,nread);
  
  DEBUG( 3, ( "readX fnum=%d min=%d max=%d nread=%d\n",
	      fsp->fnum, (int)smb_mincnt, (int)smb_maxcnt, (int)nread ) );

  return chain_reply(inbuf,outbuf,length,bufsize);
}

/****************************************************************************
  reply to a writebraw (core+ or LANMAN1.0 protocol)
****************************************************************************/

int reply_writebraw(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  ssize_t nwritten=0;
  ssize_t total_written=0;
  size_t numtowrite=0;
  size_t tcount;
  SMB_OFF_T startpos;
  char *data=NULL;
  BOOL write_through;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);
  int outsize = 0;

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);
  CHECK_ERROR(fsp);
  
  tcount = IVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv3);
  write_through = BITSETW(inbuf+smb_vwv7,0);

  /* We have to deal with slightly different formats depending
     on whether we are using the core+ or lanman1.0 protocol */
  if(Protocol <= PROTOCOL_COREPLUS) {
    numtowrite = SVAL(smb_buf(inbuf),-2);
    data = smb_buf(inbuf);
  } else {
    numtowrite = SVAL(inbuf,smb_vwv10);
    data = smb_base(inbuf) + SVAL(inbuf, smb_vwv11);
  }

  /* force the error type */
  CVAL(inbuf,smb_com) = SMBwritec;
  CVAL(outbuf,smb_com) = SMBwritec;

  if (is_locked(fsp,conn,tcount,startpos, F_WRLCK))
    return(ERROR(ERRDOS,ERRlock));

  if (numtowrite>0)
    nwritten = write_file(fsp,data,startpos,numtowrite);
  
  DEBUG(3,("writebraw1 fnum=%d start=%.0f num=%d wrote=%d sync=%d\n",
	   fsp->fnum, (double)startpos, (int)numtowrite, (int)nwritten, (int)write_through));

  if (nwritten < numtowrite) 
    return(UNIXERROR(ERRHRD,ERRdiskfull));

  total_written = nwritten;

  /* Return a message to the redirector to tell it
     to send more bytes */
  CVAL(outbuf,smb_com) = SMBwritebraw;
  SSVALS(outbuf,smb_vwv0,-1);
  outsize = set_message(outbuf,Protocol>PROTOCOL_COREPLUS?1:0,0,True);
  send_smb(Client,outbuf);
  
  /* Now read the raw data into the buffer and write it */
  if (read_smb_length(Client,inbuf,SMB_SECONDARY_WAIT) == -1) {
    exit_server("secondary writebraw failed");
  }
  
  /* Even though this is not an smb message, smb_len
     returns the generic length of an smb message */
  numtowrite = smb_len(inbuf);

  if (tcount > nwritten+numtowrite) {
    DEBUG(3,("Client overestimated the write %d %d %d\n",
	     (int)tcount,(int)nwritten,(int)numtowrite));
  }

  nwritten = transfer_file(Client,fsp->fd_ptr->fd,(SMB_OFF_T)numtowrite,NULL,0,
			   startpos+nwritten);
  total_written += nwritten;
  
  /* Set up outbuf to return the correct return */
  outsize = set_message(outbuf,1,0,True);
  CVAL(outbuf,smb_com) = SMBwritec;
  SSVAL(outbuf,smb_vwv0,total_written);

  if (nwritten < (ssize_t)numtowrite) {
    CVAL(outbuf,smb_rcls) = ERRHRD;
    SSVAL(outbuf,smb_err,ERRdiskfull);      
  }

  if (lp_syncalways(SNUM(conn)) || write_through)
    sync_file(conn,fsp);

  DEBUG(3,("writebraw2 fnum=%d start=%.0f num=%d wrote=%d\n",
	   fsp->fnum, (double)startpos, (int)numtowrite,(int)total_written));

  /* we won't return a status if write through is not selected - this 
     follows what WfWg does */
  if (!write_through && total_written==tcount)
    return(-1);

  return(outsize);
}

/****************************************************************************
  reply to a writeunlock (core+)
****************************************************************************/

int reply_writeunlock(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  ssize_t nwritten = -1;
  size_t numtowrite;
  SMB_OFF_T startpos;
  char *data;
  int eclass;
  uint32 ecode;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);
  int outsize = 0;

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);
  CHECK_ERROR(fsp);

  numtowrite = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv2);
  data = smb_buf(inbuf) + 3;
  
  if (is_locked(fsp,conn,numtowrite,startpos, F_WRLCK))
    return(ERROR(ERRDOS,ERRlock));

  /* The special X/Open SMB protocol handling of
     zero length writes is *NOT* done for
     this call */
  if(numtowrite == 0)
    nwritten = 0;
  else
    nwritten = write_file(fsp,data,startpos,numtowrite);
  
  if (lp_syncalways(SNUM(conn)))
    sync_file(conn,fsp);

  if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0))
    return(UNIXERROR(ERRDOS,ERRnoaccess));

  if(!do_unlock(fsp, conn, numtowrite, startpos, &eclass, &ecode))
    return(ERROR(eclass,ecode));

  outsize = set_message(outbuf,1,0,True);
  
  SSVAL(outbuf,smb_vwv0,nwritten);
  
  DEBUG( 3, ( "writeunlock fnum=%d num=%d wrote=%d\n",
	      fsp->fnum, (int)numtowrite, (int)nwritten ) );

  return(outsize);
}

/****************************************************************************
  reply to a write
****************************************************************************/
int reply_write(connection_struct *conn, char *inbuf,char *outbuf,int size,int dum_buffsize)
{
  size_t numtowrite;
  ssize_t nwritten = -1;
  SMB_OFF_T startpos;
  char *data;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);
  int outsize = 0;

  /* If it's an IPC, pass off the pipe handler. */
  if (IS_IPC(conn))
    return reply_pipe_write(inbuf,outbuf,size,dum_buffsize);

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);
  CHECK_ERROR(fsp);

  numtowrite = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv2);
  data = smb_buf(inbuf) + 3;
  
  if (is_locked(fsp,conn,numtowrite,startpos, F_WRLCK))
    return(ERROR(ERRDOS,ERRlock));

  /* X/Open SMB protocol says that if smb_vwv1 is
     zero then the file size should be extended or
     truncated to the size given in smb_vwv[2-3] */
  if(numtowrite == 0) {
    if((nwritten = set_filelen(fsp->fd_ptr->fd, (SMB_OFF_T)startpos)) >= 0)
      set_filelen_write_cache(fsp, startpos); 
  } else
    nwritten = write_file(fsp,data,startpos,numtowrite);
  
  if (lp_syncalways(SNUM(conn)))
    sync_file(conn,fsp);

  if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0))
    return(UNIXERROR(ERRDOS,ERRnoaccess));

  outsize = set_message(outbuf,1,0,True);
  
  SSVAL(outbuf,smb_vwv0,nwritten);

  if (nwritten < (ssize_t)numtowrite) {
    CVAL(outbuf,smb_rcls) = ERRHRD;
    SSVAL(outbuf,smb_err,ERRdiskfull);      
  }
  
  DEBUG(3,("write fnum=%d num=%d wrote=%d\n",
	   fsp->fnum, (int)numtowrite, (int)nwritten));

  return(outsize);
}


/****************************************************************************
  reply to a write and X
****************************************************************************/
int reply_write_and_X(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  files_struct *fsp = file_fsp(inbuf,smb_vwv2);
  SMB_OFF_T startpos = IVAL(inbuf,smb_vwv3);
  size_t numtowrite = SVAL(inbuf,smb_vwv10);
  BOOL write_through = BITSETW(inbuf+smb_vwv7,0);
  ssize_t nwritten = -1;
  unsigned int smb_doff = SVAL(inbuf,smb_vwv11);
  unsigned int smblen = smb_len(inbuf);
  char *data;
  BOOL large_writeX = ((CVAL(inbuf,smb_wct) == 14) && (smblen > 0xFFFF));

  /* If it's an IPC, pass off the pipe handler. */
  if (IS_IPC(conn)) {
    return reply_pipe_write_and_X(inbuf,outbuf,length,bufsize);
  }

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);
  CHECK_ERROR(fsp);

  /* Deal with possible LARGE_WRITEX */
  if (large_writeX)
    numtowrite |= ((((size_t)SVAL(inbuf,smb_vwv9)) & 1 )<<16);

  if(smb_doff > smblen || (smb_doff + numtowrite > smblen)) {
    return(ERROR(ERRDOS,ERRbadmem));
  }

  data = smb_base(inbuf) + smb_doff;

  if(CVAL(inbuf,smb_wct) == 14) {
#ifdef LARGE_SMB_OFF_T
    /*
     * This is a large offset (64 bit) write.
     */
    startpos |= (((SMB_OFF_T)IVAL(inbuf,smb_vwv12)) << 32);

#else /* !LARGE_SMB_OFF_T */

    /*
     * Ensure we haven't been sent a >32 bit offset.
     */

    if(IVAL(inbuf,smb_vwv12) != 0) {
      DEBUG(0,("reply_write_and_X - large offset (%x << 32) used and we don't support \
64 bit offsets.\n", (unsigned int)IVAL(inbuf,smb_vwv12) ));
      return(ERROR(ERRDOS,ERRbadaccess));
    }

#endif /* LARGE_SMB_OFF_T */
  }

  if (is_locked(fsp,conn,(SMB_BIG_UINT)numtowrite,(SMB_BIG_UINT)startpos, WRITE_LOCK)) {
    return(ERROR(ERRDOS,ERRlock));
  }

  /* X/Open SMB protocol says that, unlike SMBwrite
     if the length is zero then NO truncation is
     done, just a write of zero. To truncate a file,
     use SMBwrite. */
  if(numtowrite == 0)
    nwritten = 0;
  else
    nwritten = write_file(fsp,data,startpos,numtowrite);
  
  if(((nwritten == 0) && (numtowrite != 0))||(nwritten < 0)) {
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  set_message(outbuf,6,0,True);
  
  SSVAL(outbuf,smb_vwv2,nwritten);
  if (large_writeX)
    SSVAL(outbuf,smb_vwv4,(nwritten>>16)&1);
  
  if (nwritten < (ssize_t)numtowrite) {
    CVAL(outbuf,smb_rcls) = ERRHRD;
    SSVAL(outbuf,smb_err,ERRdiskfull);      
  }

  DEBUG(3,("writeX fnum=%d num=%d wrote=%d\n",
	   fsp->fnum, (int)numtowrite, (int)nwritten));

  if (lp_syncalways(SNUM(conn)) || write_through)
    sync_file(conn,fsp);

  return chain_reply(inbuf,outbuf,length,bufsize);
}


/****************************************************************************
  reply to a lseek
****************************************************************************/

int reply_lseek(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  SMB_OFF_T startpos;
  SMB_OFF_T res= -1;
  int mode,umode;
  int outsize = 0;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  CHECK_FSP(fsp,conn);
  CHECK_ERROR(fsp);

  flush_write_cache(fsp, SEEK_FLUSH);

  mode = SVAL(inbuf,smb_vwv1) & 3;
  startpos = IVALS(inbuf,smb_vwv2);

  switch (mode) {
    case 0: umode = SEEK_SET; break;
    case 1: umode = SEEK_CUR; break;
    case 2: umode = SEEK_END; break;
    default:
      umode = SEEK_SET; break;
  }

  if((res = sys_lseek(fsp->fd_ptr->fd,startpos,umode)) == -1) {
    /*
     * Check for the special case where a seek before the start
     * of the file sets the offset to zero. Added in the CIFS spec,
     * section 4.2.7.
     */

    if(errno == EINVAL) {
      SMB_OFF_T current_pos = startpos;

      if(umode == SEEK_CUR) {

        if((current_pos = sys_lseek(fsp->fd_ptr->fd,0,SEEK_CUR)) == -1)
          return(UNIXERROR(ERRDOS,ERRnoaccess));

        current_pos += startpos;

      } else if (umode == SEEK_END) {

        SMB_STRUCT_STAT sbuf;

        if(sys_fstat( fsp->fd_ptr->fd, &sbuf) == -1)
          return(UNIXERROR(ERRDOS,ERRnoaccess));

        current_pos += sbuf.st_size;
      }
 
      if(current_pos < 0)
        res = sys_lseek(fsp->fd_ptr->fd,0,SEEK_SET);
    }

    if(res == -1)
      return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  fsp->pos = res;
  
  outsize = set_message(outbuf,2,0,True);
  SIVAL(outbuf,smb_vwv0,res);
  
  DEBUG(3,("lseek fnum=%d ofs=%.0f newpos = %.0f mode=%d\n",
	   fsp->fnum, (double)startpos, (double)res, mode));

  return(outsize);
}

/****************************************************************************
  reply to a flush
****************************************************************************/

int reply_flush(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  int outsize = set_message(outbuf,0,0,True);
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  if (fsp) {
	  CHECK_FSP(fsp,conn);
	  CHECK_ERROR(fsp);
  }

  if (!fsp) {
	  file_sync_all(conn);
  } else {
	  sync_file(conn,fsp);
  }

  DEBUG(3,("flush\n"));
  return(outsize);
}


/****************************************************************************
  reply to a exit
****************************************************************************/
int reply_exit(connection_struct *conn, 
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,True);
	DEBUG(3,("exit\n"));

	return(outsize);
}


/****************************************************************************
 Reply to a close - has to deal with closing a directory opened by NT SMB's.
****************************************************************************/
int reply_close(connection_struct *conn, char *inbuf,char *outbuf, int size,
                int dum_buffsize)
{
	int outsize = 0;
	time_t mtime;
	int32 eclass = 0, err = 0;
	files_struct *fsp = NULL;

	outsize = set_message(outbuf,0,0,True);

	/* If it's an IPC, pass off to the pipe handler. */
	if (IS_IPC(conn))
		return reply_pipe_close(conn, inbuf,outbuf);

	fsp = file_fsp(inbuf,smb_vwv0);

	/*
	 * We can only use CHECK_FSP if we know it's not a directory.
	 */

	if(!fsp || !fsp->open || (fsp->conn != conn))
		return(ERROR(ERRDOS,ERRbadfid));

	if(HAS_CACHED_ERROR(fsp)) {
		eclass = fsp->wbmpx_ptr->wr_errclass;
		err = fsp->wbmpx_ptr->wr_error;
	}

	if(fsp->is_directory || fsp->stat_open) {
		/*
		 * Special case - close NT SMB directory or stat file
		 * handle.
		 */
		DEBUG(3,("close %s fnum=%d\n", fsp->is_directory ? "directory" : "stat file open", fsp->fnum));
		close_file(fsp,True);
	} else {
		/*
		 * Close ordinary file.
		 */
		int close_err;

		/*
		 * If there was a modify time outstanding,
		 * try and set it here.
		 */
		if(fsp->pending_modtime)
			set_filetime(conn, fsp->fsp_name, fsp->pending_modtime);

		/*
		 * Now take care of any time sent in the close.
		 */
		mtime = make_unix_date3(inbuf+smb_vwv1);
		
		/* try and set the date */
		set_filetime(conn, fsp->fsp_name,mtime);

		DEBUG(3,("close fd=%d fnum=%d (numopen=%d)\n",
			 fsp->fd_ptr ? fsp->fd_ptr->fd : -1, fsp->fnum,
			 conn->num_files_open));
 
		/*
		 * close_file() returns the unix errno if an error
		 * was detected on close - normally this is due to
		 * a disk full error. If not then it was probably an I/O error.
		 */
 
		if((close_err = close_file(fsp,True)) != 0) {
			errno = close_err;
			return (UNIXERROR(ERRHRD,ERRgeneral));
		}
	}  

	/* We have a cached error */
	if(eclass || err)
		return(ERROR(eclass,err));

	return(outsize);
}


/****************************************************************************
  reply to a writeclose (Core+ protocol)
****************************************************************************/

int reply_writeclose(connection_struct *conn,
		     char *inbuf,char *outbuf, int size, int dum_buffsize)
{
	size_t numtowrite;
	ssize_t nwritten = -1;
	int outsize = 0;
	int close_err = 0;
	SMB_OFF_T startpos;
	char *data;
	time_t mtime;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);

	CHECK_FSP(fsp,conn);
	CHECK_WRITE(fsp);
	CHECK_ERROR(fsp);

	numtowrite = SVAL(inbuf,smb_vwv1);
	startpos = IVAL(inbuf,smb_vwv2);
	mtime = make_unix_date3(inbuf+smb_vwv4);
	data = smb_buf(inbuf) + 1;
  
	if (is_locked(fsp,conn,numtowrite,startpos, F_WRLCK))
		return(ERROR(ERRDOS,ERRlock));
          
	nwritten = write_file(fsp,data,startpos,numtowrite);

	set_filetime(conn, fsp->fsp_name,mtime);
  
	close_err = close_file(fsp,True);

	DEBUG(3,("writeclose fnum=%d num=%d wrote=%d (numopen=%d)\n",
		 fsp->fnum, (int)numtowrite, (int)nwritten,
		 conn->num_files_open));
  
	if (nwritten <= 0)
		return(UNIXERROR(ERRDOS,ERRnoaccess));
 
	if(close_err != 0) {
		errno = close_err;
		return(UNIXERROR(ERRHRD,ERRgeneral));
	}
 
	outsize = set_message(outbuf,1,0,True);
  
	SSVAL(outbuf,smb_vwv0,nwritten);
	return(outsize);
}


/****************************************************************************
  reply to a lock
****************************************************************************/
int reply_lock(connection_struct *conn,
	       char *inbuf,char *outbuf, int length, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,True);
	SMB_OFF_T count,offset;
	int eclass;
	uint32 ecode;
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);

	CHECK_FSP(fsp,conn);
	CHECK_ERROR(fsp);

	count = IVAL(inbuf,smb_vwv1);
	offset = IVAL(inbuf,smb_vwv3);

	DEBUG(3,("lock fd=%d fnum=%d offset=%.0f count=%.0f\n",
		 fsp->fd_ptr->fd, fsp->fnum, (double)offset, (double)count));

	if (!do_lock(fsp, conn, count, offset, F_WRLCK, &eclass, &ecode)) {
      if((ecode == ERRlock) && lp_blocking_locks(SNUM(conn))) {
        /*
         * A blocking lock was requested. Package up
         * this smb into a queued request and push it
         * onto the blocking lock queue.
         */
        if(push_blocking_lock_request(inbuf, length, -1, 0))
          return -1;
      }
      return (ERROR(eclass,ecode));
    }

	return(outsize);
}


/****************************************************************************
  reply to a unlock
****************************************************************************/

int reply_unlock(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  int outsize = set_message(outbuf,0,0,True);
  SMB_OFF_T count,offset;
  int eclass;
  uint32 ecode;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  CHECK_FSP(fsp,conn);
  CHECK_ERROR(fsp);

  count = IVAL(inbuf,smb_vwv1);
  offset = IVAL(inbuf,smb_vwv3);

  if(!do_unlock(fsp, conn, count, offset, &eclass, &ecode))
    return (ERROR(eclass,ecode));

  DEBUG( 3, ( "unlock fd=%d fnum=%d offset=%.0f count=%.0f\n",
        fsp->fd_ptr->fd, fsp->fnum, (double)offset, (double)count ) );
  
  return(outsize);
}


/****************************************************************************
  reply to a tdis
****************************************************************************/
int reply_tdis(connection_struct *conn, 
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,True);
	uint16 vuid;

	vuid = SVAL(inbuf,smb_uid);

	if (!conn) {
		DEBUG(4,("Invalid connection in tdis\n"));
		return(ERROR(ERRSRV,ERRinvnid));
	}

	conn->used = False;

	close_cnum(conn,vuid);
  
	return outsize;
}



/****************************************************************************
  reply to a echo
****************************************************************************/
int reply_echo(connection_struct *conn,
	       char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int smb_reverb = SVAL(inbuf,smb_vwv0);
	int seq_num;
	int data_len = smb_buflen(inbuf);
	int outsize = set_message(outbuf,1,data_len,True);
	
	/* copy any incoming data back out */
	if (data_len > 0)
		memcpy(smb_buf(outbuf),smb_buf(inbuf),data_len);

	if (smb_reverb > 100) {
		DEBUG(0,("large reverb (%d)?? Setting to 100\n",smb_reverb));
		smb_reverb = 100;
	}

	for (seq_num =1 ; seq_num <= smb_reverb ; seq_num++) {
		SSVAL(outbuf,smb_vwv0,seq_num);

		smb_setlen(outbuf,outsize - 4);

		send_smb(Client,outbuf);
	}

	DEBUG(3,("echo %d times\n", smb_reverb));

	smb_echo_count++;

	return -1;
}

#ifdef PRINTING
/****************************************************************************
  reply to a printopen
****************************************************************************/
int reply_printopen(connection_struct *conn, 
		    char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	pstring fname;
	pstring fname2;
	int outsize = 0;
	files_struct *fsp;
	
	*fname = *fname2 = 0;
	
	if (!CAN_PRINT(conn))
		return(ERROR(ERRDOS,ERRnoaccess));

	{
		pstring s;
		char *p;
		pstrcpy(s,smb_buf(inbuf)+1);
		p = s;
		while (*p) {
			if (!(isalnum((int)*p) || strchr("._-",*p)))
				*p = 'X';
			p++;
		}

		if (strlen(s) > 10) s[10] = 0;

		slprintf(fname,sizeof(fname)-1, "%s.XXXXXX",s);  
	}

	fsp = file_new();
	if (!fsp)
		return(ERROR(ERRSRV,ERRnofids));
	
	pstrcpy(fname2,(char *)smbd_mktemp(fname));

	/* Open for exclusive use, write only. */
	open_file_shared(fsp,conn,fname2, SET_DENY_MODE(DENY_ALL)|SET_OPEN_MODE(DOS_OPEN_WRONLY),
                     (FILE_CREATE_IF_NOT_EXIST|FILE_EXISTS_FAIL), unix_mode(conn,0,fname2), 0, NULL, NULL);

	if (!fsp->open) {
		file_free(fsp);
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	}

	/* force it to be a print file */
	fsp->print_file = True;
  
	outsize = set_message(outbuf,1,0,True);
	SSVAL(outbuf,smb_vwv0,fsp->fnum);
  
	DEBUG(3,("openprint %s fd=%d fnum=%d\n",
		   fname2, fsp->fd_ptr->fd, fsp->fnum));

	return(outsize);
}


/****************************************************************************
  reply to a printclose
****************************************************************************/
int reply_printclose(connection_struct *conn,
		     char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = set_message(outbuf,0,0,True);
	files_struct *fsp = file_fsp(inbuf,smb_vwv0);
	int close_err = 0;

	CHECK_FSP(fsp,conn);
	CHECK_ERROR(fsp);

	if (!CAN_PRINT(conn))
		return(ERROR(ERRDOS,ERRnoaccess));
  
	DEBUG(3,("printclose fd=%d fnum=%d\n",
		 fsp->fd_ptr->fd,fsp->fnum));
  
	close_err = close_file(fsp,True);

	if(close_err != 0) {
		errno = close_err;
		return(UNIXERROR(ERRHRD,ERRgeneral));
	}

	return(outsize);
}


/****************************************************************************
  reply to a printqueue
****************************************************************************/
int reply_printqueue(connection_struct *conn,
		     char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
	int outsize = set_message(outbuf,2,3,True);
	int max_count = SVAL(inbuf,smb_vwv0);
	int start_index = SVAL(inbuf,smb_vwv1);

	/* we used to allow the client to get the cnum wrong, but that
	   is really quite gross and only worked when there was only
	   one printer - I think we should now only accept it if they
	   get it right (tridge) */
	if (!CAN_PRINT(conn))
		return(ERROR(ERRDOS,ERRnoaccess));

	SSVAL(outbuf,smb_vwv0,0);
	SSVAL(outbuf,smb_vwv1,0);
	CVAL(smb_buf(outbuf),0) = 1;
	SSVAL(smb_buf(outbuf),1,0);
  
	DEBUG(3,("printqueue start_index=%d max_count=%d\n",
		 start_index, max_count));

	{
		print_queue_struct *queue = NULL;
		char *p = smb_buf(outbuf) + 3;
		int count = get_printqueue(SNUM(conn), conn,&queue,NULL);
		int num_to_get = ABS(max_count);
		int first = (max_count>0?start_index:start_index+max_count+1);
		int i;

		if (first >= count)
			num_to_get = 0;
		else
			num_to_get = MIN(num_to_get,count-first);
    

		for (i=first;i<first+num_to_get;i++) {
			/* check to make sure we have room in the buffer */
			if ( (PTR_DIFF(p, outbuf)+28) > BUFFER_SIZE )
				break;
			put_dos_date2(p,0,queue[i].time);
			CVAL(p,4) = (queue[i].status==LPQ_PRINTING?2:3);
			SSVAL(p,5,printjob_encode(SNUM(conn), 
						  queue[i].job));
			SIVAL(p,7,queue[i].size);
			CVAL(p,11) = 0;
			StrnCpy(p+12,queue[i].user,16);
			p += 28;
		}

		if (count > 0) {
			outsize = set_message(outbuf,2,28*count+3,False); 
			SSVAL(outbuf,smb_vwv0,count);
			SSVAL(outbuf,smb_vwv1,(max_count>0?first+count:first-1));
			CVAL(smb_buf(outbuf),0) = 1;
			SSVAL(smb_buf(outbuf),1,28*count);
		}

		if (queue) free(queue);
	  
		DEBUG(3,("%d entries returned in queue\n",count));
	}
  
	return(outsize);
}


/****************************************************************************
  reply to a printwrite
****************************************************************************/
int reply_printwrite(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int numtowrite;
  int outsize = set_message(outbuf,0,0,True);
  char *data;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);
  
  if (!CAN_PRINT(conn))
    return(ERROR(ERRDOS,ERRnoaccess));

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);
  CHECK_ERROR(fsp);

  numtowrite = SVAL(smb_buf(inbuf),1);
  data = smb_buf(inbuf) + 3;
  
  if (write_file(fsp,data,-1,numtowrite) != numtowrite)
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  
  DEBUG( 3, ( "printwrite fnum=%d num=%d\n", fsp->fnum, numtowrite ) );
  
  return(outsize);
}
#endif

/****************************************************************************
  reply to a mkdir
****************************************************************************/
int reply_mkdir(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring directory;
  int outsize,ret= -1;
  BOOL bad_path = False;
 
  pstrcpy(directory,smb_buf(inbuf) + 1);
  unix_convert(directory,conn,0,&bad_path,NULL);
  
  if (check_name(directory, conn))
    ret = dos_mkdir(directory,unix_mode(conn,aDIR,directory));
  
  if (ret < 0)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  outsize = set_message(outbuf,0,0,True);

  DEBUG( 3, ( "mkdir %s ret=%d\n", directory, ret ) );

  return(outsize);
}

/****************************************************************************
Static function used by reply_rmdir to delete an entire directory
tree recursively.
****************************************************************************/

static BOOL recursive_rmdir(char *directory)
{
  char *dname = NULL;
  BOOL ret = False;
  void *dirptr = OpenDir(NULL, directory, False);

  if(dirptr == NULL)
    return True;

  while((dname = ReadDirName(dirptr)))
  {
    pstring fullname;
    SMB_STRUCT_STAT st;

    if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0))
      continue;

    /* Construct the full name. */
    if(strlen(directory) + strlen(dname) + 1 >= sizeof(fullname))
    {
      errno = ENOMEM;
      ret = True;
      break;
    }
    pstrcpy(fullname, directory);
    pstrcat(fullname, "/");
    pstrcat(fullname, dname);

    if(dos_lstat(fullname, &st) != 0)
    {
      ret = True;
      break;
    }

    if(st.st_mode & S_IFDIR)
    {
      if(recursive_rmdir(fullname)!=0)
      {
        ret = True;
        break;
      }
      if(dos_rmdir(fullname) != 0)
      {
        ret = True;
        break;
      }
    }
    else if(dos_unlink(fullname) != 0)
    {
      ret = True;
      break;
    }
  }
  CloseDir(dirptr);
  return ret;
}

/****************************************************************************
 The internals of the rmdir code - called elsewhere.
****************************************************************************/

BOOL rmdir_internals(connection_struct *conn, char *directory)
{
  BOOL ok;

  ok = (dos_rmdir(directory) == 0);
  if(!ok && ((errno == ENOTEMPTY)||(errno == EEXIST)) && lp_veto_files(SNUM(conn)))
  {
    /* 
     * Check to see if the only thing in this directory are
     * vetoed files/directories. If so then delete them and
     * retry. If we fail to delete any of them (and we *don't*
     * do a recursive delete) then fail the rmdir.
     */
    BOOL all_veto_files = True;
    char *dname;
    void *dirptr = OpenDir(conn, directory, False);

    if(dirptr != NULL)
    {
      int dirpos = TellDir(dirptr);
      while ((dname = ReadDirName(dirptr)))
      {
        if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0))
          continue;
        if(!IS_VETO_PATH(conn, dname))
        {
          all_veto_files = False;
          break;
        }
      }
      if(all_veto_files)
      {
        SeekDir(dirptr,dirpos);
        while ((dname = ReadDirName(dirptr)))
        {
          pstring fullname;
          SMB_STRUCT_STAT st;

          if((strcmp(dname, ".") == 0) || (strcmp(dname, "..")==0))
            continue;

          /* Construct the full name. */
          if(strlen(directory) + strlen(dname) + 1 >= sizeof(fullname))
          {
            errno = ENOMEM;
            break;
          }
          pstrcpy(fullname, directory);
          pstrcat(fullname, "/");
          pstrcat(fullname, dname);
                     
          if(dos_lstat(fullname, &st) != 0)
            break;
          if(st.st_mode & S_IFDIR)
          {
            if(lp_recursive_veto_delete(SNUM(conn)))
            {
              if(recursive_rmdir(fullname) != 0)
                break;
            }
            if(dos_rmdir(fullname) != 0)
              break;
          }
          else if(dos_unlink(fullname) != 0)
            break;
        }
        CloseDir(dirptr);
        /* Retry the rmdir */
        ok = (dos_rmdir(directory) == 0);
      }
      else
        CloseDir(dirptr);
    }
    else
      errno = ENOTEMPTY;
  }
          
  if (!ok)
    DEBUG(3,("rmdir_internals: couldn't remove directory %s : %s\n",
          directory,strerror(errno)));

  return ok;
}

/****************************************************************************
 Reply to a rmdir.
****************************************************************************/

int reply_rmdir(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  pstring directory;
  int outsize = 0;
  BOOL ok = False;
  BOOL bad_path = False;

  pstrcpy(directory,smb_buf(inbuf) + 1);
  unix_convert(directory,conn, NULL,&bad_path,NULL);
  
  if (check_name(directory,conn))
  {
    dptr_closepath(directory,SVAL(inbuf,smb_pid));
    ok = rmdir_internals(conn, directory);
  }
  
  if (!ok)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    return(UNIXERROR(ERRDOS,ERRbadpath));
  }
 
  outsize = set_message(outbuf,0,0,True);
  
  DEBUG( 3, ( "rmdir %s\n", directory ) );
  
  return(outsize);
}


/*******************************************************************
resolve wildcards in a filename rename
********************************************************************/
static BOOL resolve_wildcards(char *name1,char *name2)
{
  fstring root1,root2;
  fstring ext1,ext2;
  char *p,*p2;

  name1 = strrchr(name1,'/');
  name2 = strrchr(name2,'/');

  if (!name1 || !name2) return(False);
  
  fstrcpy(root1,name1);
  fstrcpy(root2,name2);
  p = strrchr(root1,'.');
  if (p) {
    *p = 0;
    fstrcpy(ext1,p+1);
  } else {
    fstrcpy(ext1,"");    
  }
  p = strrchr(root2,'.');
  if (p) {
    *p = 0;
    fstrcpy(ext2,p+1);
  } else {
    fstrcpy(ext2,"");    
  }

  p = root1;
  p2 = root2;
  while (*p2) {
    if (*p2 == '?') {
      *p2 = *p;
      p2++;
    } else {
      p2++;
    }
    if (*p) p++;
  }

  p = ext1;
  p2 = ext2;
  while (*p2) {
    if (*p2 == '?') {
      *p2 = *p;
      p2++;
    } else {
      p2++;
    }
    if (*p) p++;
  }

  pstrcpy(name2,root2);
  if (ext2[0]) {
    pstrcat(name2,".");
    pstrcat(name2,ext2);
  }

  return(True);
}

/*******************************************************************
check if a user is allowed to rename a file
********************************************************************/
static BOOL can_rename(char *fname,connection_struct *conn)
{
  SMB_STRUCT_STAT sbuf;

  if (!CAN_WRITE(conn)) return(False);

  if (dos_lstat(fname,&sbuf) != 0) return(False);
  if (!check_file_sharing(conn,fname,True)) return(False);

  return(True);
}

/****************************************************************************
 The guts of the rename command, split out so it may be called by the NT SMB
 code. 
****************************************************************************/
int rename_internals(connection_struct *conn, 
		     char *inbuf, char *outbuf, char *name, 
		     char *newname, BOOL replace_if_exists)
{
	pstring directory;
	pstring mask;
	pstring newname_last_component;
	char *p;
	BOOL has_wild;
	BOOL bad_path1 = False;
	BOOL bad_path2 = False;
	int count=0;
	int error = ERRnoaccess;
	BOOL exists=False;
	BOOL rc = True;

	*directory = *mask = 0;

	rc = unix_convert(name,conn,0,&bad_path1,NULL);
	unix_convert(newname,conn,newname_last_component,&bad_path2,NULL);

	/*
	 * Split the old name into directory and last component
	 * strings. Note that unix_convert may have stripped off a 
	 * leading ./ from both name and newname if the rename is 
	 * at the root of the share. We need to make sure either both
	 * name and newname contain a / character or neither of them do
	 * as this is checked in resolve_wildcards().
	 */
	
	p = strrchr(name,'/');
	if (!p) {
		pstrcpy(directory,".");
		pstrcpy(mask,name);
	} else {
		*p = 0;
		pstrcpy(directory,name);
		pstrcpy(mask,p+1);
		*p = '/'; /* Replace needed for exceptional test below. */
	}

	/*
	 * We should only check the mangled cache
	 * here if unix_convert failed. This means
	 * that the path in 'mask' doesn't exist
	 * on the file system and so we need to look
	 * for a possible mangle. This patch from
	 * Tine Smukavec <valentin.smukavec@hermes.si>.
	 */

	if (!rc && is_mangled(mask))
		check_mangled_cache( mask );

	has_wild = strchr(mask,'*') || strchr(mask,'?');

	if (!has_wild) {
		/*
		 * No wildcards - just process the one file.
		 */
		BOOL is_short_name = is_8_3(name, True);

		/* Add a terminating '/' to the directory name. */
		pstrcat(directory,"/");
		pstrcat(directory,mask);
		
		/* Ensure newname contains a '/' also */
		if(strrchr(newname,'/') == 0) {
			pstring tmpstr;
			
			pstrcpy(tmpstr, "./");
			pstrcat(tmpstr, newname);
			pstrcpy(newname, tmpstr);
		}
		
		DEBUG(3,("rename_internals: case_sensitive = %d, case_preserve = %d, short case preserve = %d, directory = %s, newname = %s, newname_last_component = %s, is_8_3 = %d\n", 
			 case_sensitive, case_preserve, short_case_preserve, directory, 
			 newname, newname_last_component, is_short_name));

		/*
		 * Check for special case with case preserving and not
		 * case sensitive, if directory and newname are identical,
		 * and the old last component differs from the original
		 * last component only by case, then we should allow
		 * the rename (user is trying to change the case of the
		 * filename).
		 */
		if((case_sensitive == False) && 
		   (((case_preserve == True) && 
		     (is_short_name == False)) || 
		    ((short_case_preserve == True) && 
		     (is_short_name == True))) &&
		   strcsequal(directory, newname)) {
			pstring newname_modified_last_component;

			/*
			 * Get the last component of the modified name.
			 * Note that we guarantee that newname contains a '/'
			 * character above.
			 */
			p = strrchr(newname,'/');
			pstrcpy(newname_modified_last_component,p+1);
			
			if(strcsequal(newname_modified_last_component, 
				      newname_last_component) == False) {
				/*
				 * Replace the modified last component with
				 * the original.
				 */
				pstrcpy(p+1, newname_last_component);
			}
		}
		
		if(replace_if_exists) {
			/*
			 * NT SMB specific flag - rename can overwrite
			 * file with the same name so don't check for
			 * dos_file_exist().
			 */
			if(resolve_wildcards(directory,newname) &&
			   can_rename(directory,conn) &&
			   !dos_rename(directory,newname))
				count++;
		} else {
			if (resolve_wildcards(directory,newname) && 
			    can_rename(directory,conn) && 
			    !dos_file_exist(newname,NULL) &&
			    !dos_rename(directory,newname))
				count++;
		}

		DEBUG(3,("rename_internals: %s doing rename on %s -> %s\n",(count != 0) ? "succeeded" : "failed",
                         directory,newname));
		
		if (!count) exists = dos_file_exist(directory,NULL);
		if (!count && exists && dos_file_exist(newname,NULL)) {
			exists = True;
			error = ERRrename;
		}
	} else {
		/*
		 * Wildcards - process each file that matches.
		 */
		void *dirptr = NULL;
		char *dname;
		pstring destname;
		
		if (check_name(directory,conn))
			dirptr = OpenDir(conn, directory, True);
		
		if (dirptr) {
			error = ERRbadfile;
			
			if (strequal(mask,"????????.???"))
				pstrcpy(mask,"*");
			
			while ((dname = ReadDirName(dirptr))) {
				pstring fname;
				pstrcpy(fname,dname);
				
				if(!mask_match(fname, mask, case_sensitive, False))
					continue;
				
				error = ERRnoaccess;
				slprintf(fname,sizeof(fname)-1,"%s/%s",directory,dname);
				if (!can_rename(fname,conn)) {
					DEBUG(6,("rename %s refused\n", fname));
					continue;
				}
				pstrcpy(destname,newname);
				
				if (!resolve_wildcards(fname,destname)) {
					DEBUG(6,("resolve_wildcards %s %s failed\n", fname, destname));
					continue;
				}
				
				if (!replace_if_exists && dos_file_exist(destname,NULL)) {
					DEBUG(6,("dos_file_exist %s\n", destname));
					error = 183;
					continue;
				}
				
				if (!dos_rename(fname,destname))
					count++;
				DEBUG(3,("rename_internals: doing rename on %s -> %s\n",fname,destname));
			}
			CloseDir(dirptr);
		}
	}
	
	if (count == 0) {
		if (exists)
			return(ERROR(ERRDOS,error));
		else {
			if((errno == ENOENT) && (bad_path1 || bad_path2)) {
				unix_ERR_class = ERRDOS;
				unix_ERR_code = ERRbadpath;
			}
			return(UNIXERROR(ERRDOS,error));
		}
	}
	
	return 0;
}

/****************************************************************************
 Reply to a mv.
****************************************************************************/

int reply_mv(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;
  pstring name;
  pstring newname;

  pstrcpy(name,smb_buf(inbuf) + 1);
  pstrcpy(newname,smb_buf(inbuf) + 3 + strlen(name));
   
  DEBUG(3,("reply_mv : %s -> %s\n",name,newname));

  outsize = rename_internals(conn, inbuf, outbuf, name, newname, False);
  if(outsize == 0) 
    outsize = set_message(outbuf,0,0,True);
  
  return(outsize);
}

/*******************************************************************
  copy a file as part of a reply_copy
  ******************************************************************/

static BOOL copy_file(char *src,char *dest1,connection_struct *conn, int ofun,
		      int count,BOOL target_is_directory, int *err_ret)
{
  int Access,action;
  SMB_STRUCT_STAT st;
  SMB_OFF_T ret=-1;
  files_struct *fsp1,*fsp2;
  pstring dest;
  
  *err_ret = 0;

  pstrcpy(dest,dest1);
  if (target_is_directory) {
    char *p = strrchr(src,'/');
    if (p) 
      p++;
    else
      p = src;
    pstrcat(dest,"/");
    pstrcat(dest,p);
  }

  if (!dos_file_exist(src,&st))
    return(False);

  fsp1 = file_new();
  if (!fsp1)
    return(False);

  open_file_shared(fsp1,conn,src,SET_DENY_MODE(DENY_NONE)|SET_OPEN_MODE(DOS_OPEN_RDONLY),
		   (FILE_FAIL_IF_NOT_EXIST|FILE_EXISTS_OPEN),0,0,&Access,&action);

  if (!fsp1->open) {
	  file_free(fsp1);
	  return(False);
  }

  if (!target_is_directory && count)
    ofun = 1;

  fsp2 = file_new();
  if (!fsp2) {
	  close_file(fsp1,False);
	  return(False);
  }
  open_file_shared(fsp2,conn,dest,SET_DENY_MODE(DENY_NONE)|SET_OPEN_MODE(DOS_OPEN_WRONLY),
		   ofun,st.st_mode,0,&Access,&action);

  if (!fsp2->open) {
    close_file(fsp1,False);
    file_free(fsp2);
    return(False);
  }

  if ((ofun&3) == 1) {
    if(sys_lseek(fsp2->fd_ptr->fd,0,SEEK_END) == -1) {
      DEBUG(0,("copy_file: error - sys_lseek returned error %s\n",
               strerror(errno) ));
      /*
       * Stop the copy from occurring.
       */
      ret = -1;
      st.st_size = 0;
    }
  }
  
  if (st.st_size)
    ret = transfer_file(fsp1->fd_ptr->fd,
                        fsp2->fd_ptr->fd,st.st_size,NULL,0,0);

  close_file(fsp1,False);
  /*
   * As we are opening fsp1 read-only we only expect
   * an error on close on fsp2 if we are out of space.
   * Thus we don't look at the error return from the
   * close of fsp1.
   */
  *err_ret = close_file(fsp2,False);

  return(ret == (SMB_OFF_T)st.st_size);
}



/****************************************************************************
  reply to a file copy.
  ****************************************************************************/
int reply_copy(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;
  pstring name;
  pstring directory;
  pstring mask,newname;
  char *p;
  int count=0;
  int error = ERRnoaccess;
  int err = 0;
  BOOL has_wild;
  BOOL exists=False;
  int tid2 = SVAL(inbuf,smb_vwv0);
  int ofun = SVAL(inbuf,smb_vwv1);
  int flags = SVAL(inbuf,smb_vwv2);
  BOOL target_is_directory=False;
  BOOL bad_path1 = False;
  BOOL bad_path2 = False;
  BOOL rc = True;

  *directory = *mask = 0;

  pstrcpy(name,smb_buf(inbuf));
  pstrcpy(newname,smb_buf(inbuf) + 1 + strlen(name));
   
  DEBUG(3,("reply_copy : %s -> %s\n",name,newname));
   
  if (tid2 != conn->cnum) {
    /* can't currently handle inter share copies XXXX */
    DEBUG(3,("Rejecting inter-share copy\n"));
    return(ERROR(ERRSRV,ERRinvdevice));
  }

  rc = unix_convert(name,conn,0,&bad_path1,NULL);
  unix_convert(newname,conn,0,&bad_path2,NULL);

  target_is_directory = dos_directory_exist(newname,NULL);

  if ((flags&1) && target_is_directory) {
    return(ERROR(ERRDOS,ERRbadfile));
  }

  if ((flags&2) && !target_is_directory) {
    return(ERROR(ERRDOS,ERRbadpath));
  }

  if ((flags&(1<<5)) && dos_directory_exist(name,NULL)) {
    /* wants a tree copy! XXXX */
    DEBUG(3,("Rejecting tree copy\n"));
    return(ERROR(ERRSRV,ERRerror));    
  }

  p = strrchr(name,'/');
  if (!p) {
    pstrcpy(directory,"./");
    pstrcpy(mask,name);
  } else {
    *p = 0;
    pstrcpy(directory,name);
    pstrcpy(mask,p+1);
  }

  /*
   * We should only check the mangled cache
   * here if unix_convert failed. This means
   * that the path in 'mask' doesn't exist
   * on the file system and so we need to look
   * for a possible mangle. This patch from
   * Tine Smukavec <valentin.smukavec@hermes.si>.
   */

  if (!rc && is_mangled(mask))
    check_mangled_cache( mask );

  has_wild = strchr(mask,'*') || strchr(mask,'?');

  if (!has_wild) {
    pstrcat(directory,"/");
    pstrcat(directory,mask);
    if (resolve_wildcards(directory,newname) && 
	copy_file(directory,newname,conn,ofun,
		  count,target_is_directory,&err)) count++;
    if(!count && err) {
		errno = err;
		return(UNIXERROR(ERRHRD,ERRgeneral));
	}
    if (!count) exists = dos_file_exist(directory,NULL);
  } else {
    void *dirptr = NULL;
    char *dname;
    pstring destname;

    if (check_name(directory,conn))
      dirptr = OpenDir(conn, directory, True);

    if (dirptr) {
	error = ERRbadfile;

	if (strequal(mask,"????????.???"))
	  pstrcpy(mask,"*");

	while ((dname = ReadDirName(dirptr))) {
	    pstring fname;
	    pstrcpy(fname,dname);
	    
	    if(!mask_match(fname, mask, case_sensitive, False))
			continue;

	    error = ERRnoaccess;
	    slprintf(fname,sizeof(fname)-1, "%s/%s",directory,dname);
	    pstrcpy(destname,newname);
	    if (resolve_wildcards(fname,destname) && 
		copy_file(fname,destname,conn,ofun,
			  count,target_is_directory,&err)) count++;
	    DEBUG(3,("reply_copy : doing copy on %s -> %s\n",fname,destname));
	  }
	CloseDir(dirptr);
    }
  }
  
  if (count == 0) {
    if(err) {
      /* Error on close... */
      errno = err;
      return(UNIXERROR(ERRHRD,ERRgeneral));
    }

    if (exists)
      return(ERROR(ERRDOS,error));
    else
    {
      if((errno == ENOENT) && (bad_path1 || bad_path2))
      {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      return(UNIXERROR(ERRDOS,error));
    }
  }
  
  outsize = set_message(outbuf,1,0,True);
  SSVAL(outbuf,smb_vwv0,count);

  return(outsize);
}

/****************************************************************************
  reply to a setdir
****************************************************************************/
int reply_setdir(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int snum;
  int outsize = 0;
  BOOL ok = False;
  pstring newdir;
  
  snum = SNUM(conn);
  if (!CAN_SETDIR(snum))
    return(ERROR(ERRDOS,ERRnoaccess));
  
  pstrcpy(newdir,smb_buf(inbuf) + 1);
  strlower(newdir);
  
  if (strlen(newdir) == 0) {
	  ok = True;
  } else {
	  ok = dos_directory_exist(newdir,NULL);
	  if (ok) {
		  string_set(&conn->connectpath,newdir);
	  }
  }
  
  if (!ok)
	  return(ERROR(ERRDOS,ERRbadpath));
  
  outsize = set_message(outbuf,0,0,True);
  CVAL(outbuf,smb_reh) = CVAL(inbuf,smb_reh);
  
  DEBUG(3,("setdir %s\n", newdir));

  return(outsize);
}

/****************************************************************************
 Get a lock count, dealing with large count requests.
****************************************************************************/

SMB_OFF_T get_lock_count( char *data, int data_offset, BOOL large_file_format, BOOL *err)
{
  SMB_OFF_T count = 0;

  *err = False;

  if(!large_file_format) {
    count = (SMB_OFF_T)IVAL(data,SMB_LKLEN_OFFSET(data_offset));
  } else {

#if defined(LARGE_SMB_OFF_T) && !defined(HAVE_BROKEN_FCNTL64_LOCKS)

    count = (((SMB_OFF_T) IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset))) << 32) |
            ((SMB_OFF_T) IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset)));

#else /* !LARGE_SMB_OFF_T || HAVE_BROKEN_FCNTL64_LOCKS */

    /*
     * NT4.x seems to be broken in that it sends large file
     * lockingX calls even if the CAP_LARGE_FILES was *not*
     * negotiated. For boxes without large file locks truncate the
     * lock count by dropping the top 32 bits.
     */

    if(IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)) != 0) {
      DEBUG(3,("get_lock_count: truncating lock count (high)0x%x (low)0x%x to just low count.\n",
            (unsigned int)IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)),
            (unsigned int)IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset)) ));
      SIVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset),0);
    }

    if(IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)) != 0) {
      /*
       * Before we error out, see if we can sensibly map the top bits
       * down to the lower bits - or lose the top bits if they are all 1's.
       * It seems that NT has this horrible bug where it will send 64 bit
       * lock requests even if told not to. JRA.
       */

      if(IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset)) == (uint32)0xFFFFFFFF)
        count = (SMB_OFF_T)IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset));
      else if (IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)) == (uint32)0xFFFFFFFF)
        count = (SMB_OFF_T)IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset));
      else {

        DEBUG(0,("get_lock_count: Error : a large file count (%x << 32 | %x) was sent and we don't \
support large counts.\n", (unsigned int)IVAL(data,SMB_LARGE_LKLEN_OFFSET_HIGH(data_offset)),
            (unsigned int)IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset)) ));

        *err = True;
        return (SMB_OFF_T)-1;
      }
    }
    else
      count = (SMB_OFF_T)IVAL(data,SMB_LARGE_LKLEN_OFFSET_LOW(data_offset));

#endif /* LARGE_SMB_OFF_T */
  }
  return count;
}

/****************************************************************************
 Get a lock offset, dealing with large offset requests.
****************************************************************************/

SMB_OFF_T get_lock_offset( char *data, int data_offset, BOOL large_file_format, BOOL *err)
{
  SMB_OFF_T offset = 0;

  *err = False;

  if(!large_file_format) {
    offset = (SMB_OFF_T)IVAL(data,SMB_LKOFF_OFFSET(data_offset));
  } else {

#if defined(LARGE_SMB_OFF_T) && !defined(HAVE_BROKEN_FCNTL64_LOCKS)

    offset = (((SMB_OFF_T) IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset))) << 32) |
            ((SMB_OFF_T) IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset)));

#else /* !LARGE_SMB_OFF_T || HAVE_BROKEN_FCNTL64_LOCKS */

    /*
     * NT4.x seems to be broken in that it sends large file
     * lockingX calls even if the CAP_LARGE_FILES was *not*
     * negotiated. For boxes without large file locks mangle the
     * lock offset by mapping the top 32 bits onto the lower 32.
     */
      
    if(IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset)) != 0) {
      uint32 low = IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset));
      uint32 high = IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset));
      uint32 new_low = 0;

      if((new_low = map_lock_offset(high, low)) == 0) {
        *err = True;
        return (SMB_OFF_T)-1;
      }

      DEBUG(3,("get_lock_offset: truncating lock offset (high)0x%x (low)0x%x to offset 0x%x.\n",
            (unsigned int)high, (unsigned int)low, (unsigned int)new_low ));
      SIVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset),0);
      SIVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset),new_low);
    }

    if(IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset)) != 0){
      /*
       * Before we error out, see if we can sensibly map the top bits
       * down to the lower bits - or lose the top bits if they are all 1's.
       * It seems that NT has this horrible bug where it will send 64 bit
       * lock requests even if told not to. JRA.
       */

      if(IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset)) == (uint32)0xFFFFFFFF)
        offset = (SMB_OFF_T)IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset));
      else if(IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset)) == (uint32)0xFFFFFFFF)
        offset = (SMB_OFF_T)IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset));
      else {

        DEBUG(0,("get_lock_offset: Error : a large file offset (%x << 32 | %x) was sent and we don't \
support large offsets.\n", (unsigned int)IVAL(data,SMB_LARGE_LKOFF_OFFSET_HIGH(data_offset)),
            (unsigned int)IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset)) ));

        *err = True;
        return (SMB_OFF_T)-1;
      }
    }
    else
      offset = (SMB_OFF_T)IVAL(data,SMB_LARGE_LKOFF_OFFSET_LOW(data_offset));

#endif /* LARGE_SMB_OFF_T */
  }
  return offset;
}

/****************************************************************************
  reply to a lockingX request
****************************************************************************/

int reply_lockingX(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  files_struct *fsp = file_fsp(inbuf,smb_vwv2);
  unsigned char locktype = CVAL(inbuf,smb_vwv3);
#if 0
  unsigned char oplocklevel = CVAL(inbuf,smb_vwv3+1);
#endif
  uint16 num_ulocks = SVAL(inbuf,smb_vwv6);
  uint16 num_locks = SVAL(inbuf,smb_vwv7);
  SMB_OFF_T count = 0, offset = 0;
  int32 lock_timeout = IVAL(inbuf,smb_vwv4);
  int i;
  char *data;
  uint32 ecode=0, dummy2;
  int eclass=0, dummy1;
  BOOL large_file_format = (locktype & LOCKING_ANDX_LARGE_FILES);
  BOOL err1, err2;

  CHECK_FSP(fsp,conn);
  CHECK_ERROR(fsp);

  data = smb_buf(inbuf);

  /* Check if this is an oplock break on a file
     we have granted an oplock on.
   */
  if ((locktype & LOCKING_ANDX_OPLOCK_RELEASE))
  {
    DEBUG(5,("reply_lockingX: oplock break reply from client for fnum = %d\n",
              fsp->fnum));

    /*
     * Make sure we have granted an exclusive or batch oplock on this file.
     */

    if(!EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
    {
      DEBUG(0,("reply_lockingX: Error : oplock break from client for fnum = %d and \
no oplock granted on this file (%s).\n", fsp->fnum, fsp->fsp_name));

      /* if this is a pure oplock break request then don't send a reply */
      if (num_locks == 0 && num_ulocks == 0)
        return -1;
      else
        return ERROR(ERRDOS,ERRlock);
    }

    if (remove_oplock(fsp) == False) {
      DEBUG(0,("reply_lockingX: error in removing oplock on file %s\n",
            fsp->fsp_name ));
    }

    /* if this is a pure oplock break request then don't send a reply */
    if (num_locks == 0 && num_ulocks == 0)
    {
      /* Sanity check - ensure a pure oplock break is not a
         chained request. */
      if(CVAL(inbuf,smb_vwv0) != 0xff)
        DEBUG(0,("reply_lockingX: Error : pure oplock break is a chained %d request !\n",
                 (unsigned int)CVAL(inbuf,smb_vwv0) ));
      return -1;
    }
  }

  /* Data now points at the beginning of the list
     of smb_unlkrng structs */
  for(i = 0; i < (int)num_ulocks; i++) {
    count = get_lock_count( data, i, large_file_format, &err1);
    offset = get_lock_offset( data, i, large_file_format, &err2);

    /*
     * There is no error code marked "stupid client bug".... :-).
     */
    if(err1 || err2)
      return ERROR(ERRDOS,ERRnoaccess);

    DEBUG(10,("reply_lockingX: unlock start=%.0f, len=%.0f for file %s\n",
          (double)offset, (double)count, fsp->fsp_name ));

    if(!do_unlock(fsp,conn,count,offset,&eclass, &ecode))
      return ERROR(eclass,ecode);
  }

  /* Setup the timeout in seconds. */
  lock_timeout = ((lock_timeout == -1) ? -1 : lock_timeout/1000);

  /* Now do any requested locks */
  data += ((large_file_format ? 20 : 10)*num_ulocks);

  /* Data now points at the beginning of the list
     of smb_lkrng structs */

  for(i = 0; i < (int)num_locks; i++) {
    count = get_lock_count( data, i, large_file_format, &err1);
    offset = get_lock_offset( data, i, large_file_format, &err2);

    /*
     * There is no error code marked "stupid client bug".... :-).
     */
    if(err1 || err2)
      return ERROR(ERRDOS,ERRnoaccess);
 
    DEBUG(10,("reply_lockingX: lock start=%.0f, len=%.0f for file %s\n",
          (double)offset, (double)count, fsp->fsp_name ));

    if(!do_lock(fsp,conn,count,offset, ((locktype & 1) ? F_RDLCK : F_WRLCK),
                &eclass, &ecode)) {
      if((ecode == ERRlock) && (lock_timeout != 0) && lp_blocking_locks(SNUM(conn))) {
        /*
         * A blocking lock was requested. Package up
         * this smb into a queued request and push it
         * onto the blocking lock queue.
         */
        if(push_blocking_lock_request(inbuf, length, lock_timeout, i))
          return -1;
      }
      break;
    }
  }

  /* If any of the above locks failed, then we must unlock
     all of the previous locks (X/Open spec). */
  if(i != num_locks && num_locks != 0) {
    /*
     * Ensure we don't do a remove on the lock that just failed,
     * as under POSIX rules, if we have a lock already there, we
     * will delete it (and we shouldn't) .....
     */
    for(i--; i >= 0; i--) {
      count = get_lock_count( data, i, large_file_format, &err1);
      offset = get_lock_offset( data, i, large_file_format, &err2);

      /*
       * There is no error code marked "stupid client bug".... :-).
       */
      if(err1 || err2)
        return ERROR(ERRDOS,ERRnoaccess);
 
      do_unlock(fsp,conn,count,offset,&dummy1,&dummy2);
    }
    return ERROR(eclass,ecode);
  }

  set_message(outbuf,2,0,True);
  
  DEBUG( 3, ( "lockingX fnum=%d type=%d num_locks=%d num_ulocks=%d\n",
	fsp->fnum, (unsigned int)locktype, num_locks, num_ulocks ) );

  return chain_reply(inbuf,outbuf,length,bufsize);
}


/****************************************************************************
  reply to a SMBreadbmpx (read block multiplex) request
****************************************************************************/
int reply_readbmpx(connection_struct *conn, char *inbuf,char *outbuf,int length,int bufsize)
{
  ssize_t nread = -1;
  ssize_t total_read;
  char *data;
  SMB_OFF_T startpos;
  int outsize;
  size_t maxcount;
  int max_per_packet;
  size_t tcount;
  int pad;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  /* this function doesn't seem to work - disable by default */
  if (!lp_readbmpx())
    return(ERROR(ERRSRV,ERRuseSTD));

  outsize = set_message(outbuf,8,0,True);

  CHECK_FSP(fsp,conn);
  CHECK_READ(fsp);
  CHECK_ERROR(fsp);

  startpos = IVAL(inbuf,smb_vwv1);
  maxcount = SVAL(inbuf,smb_vwv3);

  data = smb_buf(outbuf);
  pad = ((long)data)%4;
  if (pad) pad = 4 - pad;
  data += pad;

  max_per_packet = bufsize-(outsize+pad);
  tcount = maxcount;
  total_read = 0;

  if (is_locked(fsp,conn,maxcount,startpos, F_RDLCK))
    return(ERROR(ERRDOS,ERRlock));
	
  do
    {
      size_t N = MIN(max_per_packet,tcount-total_read);
  
      nread = read_file(fsp,data,startpos,N);

      if (nread <= 0) nread = 0;

      if (nread < (ssize_t)N)
        tcount = total_read + nread;

      set_message(outbuf,8,nread,False);
      SIVAL(outbuf,smb_vwv0,startpos);
      SSVAL(outbuf,smb_vwv2,tcount);
      SSVAL(outbuf,smb_vwv6,nread);
      SSVAL(outbuf,smb_vwv7,smb_offset(data,outbuf));

      send_smb(Client,outbuf);

      total_read += nread;
      startpos += nread;
    }
  while (total_read < (ssize_t)tcount);

  return(-1);
}

/****************************************************************************
  reply to a SMBwritebmpx (write block multiplex primary) request
****************************************************************************/

int reply_writebmpx(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  size_t numtowrite;
  ssize_t nwritten = -1;
  int outsize = 0;
  SMB_OFF_T startpos;
  size_t tcount;
  BOOL write_through;
  int smb_doff;
  char *data;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);
  CHECK_ERROR(fsp);

  tcount = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv3);
  write_through = BITSETW(inbuf+smb_vwv7,0);
  numtowrite = SVAL(inbuf,smb_vwv10);
  smb_doff = SVAL(inbuf,smb_vwv11);

  data = smb_base(inbuf) + smb_doff;

  /* If this fails we need to send an SMBwriteC response,
     not an SMBwritebmpx - set this up now so we don't forget */
  CVAL(outbuf,smb_com) = SMBwritec;

  if (is_locked(fsp,conn,tcount,startpos,F_WRLCK))
    return(ERROR(ERRDOS,ERRlock));

  nwritten = write_file(fsp,data,startpos,numtowrite);

  if(lp_syncalways(SNUM(conn)) || write_through)
    sync_file(conn,fsp);
  
  if(nwritten < (ssize_t)numtowrite)
    return(UNIXERROR(ERRHRD,ERRdiskfull));

  /* If the maximum to be written to this file
     is greater than what we just wrote then set
     up a secondary struct to be attached to this
     fd, we will use this to cache error messages etc. */
  if((ssize_t)tcount > nwritten) 
  {
    write_bmpx_struct *wbms;
    if(fsp->wbmpx_ptr != NULL)
      wbms = fsp->wbmpx_ptr; /* Use an existing struct */
    else
      wbms = (write_bmpx_struct *)malloc(sizeof(write_bmpx_struct));
    if(!wbms)
    {
      DEBUG(0,("Out of memory in reply_readmpx\n"));
      return(ERROR(ERRSRV,ERRnoresource));
    }
    wbms->wr_mode = write_through;
    wbms->wr_discard = False; /* No errors yet */
    wbms->wr_total_written = nwritten;
    wbms->wr_errclass = 0;
    wbms->wr_error = 0;
    fsp->wbmpx_ptr = wbms;
  }

  /* We are returning successfully, set the message type back to
     SMBwritebmpx */
  CVAL(outbuf,smb_com) = SMBwriteBmpx;
  
  outsize = set_message(outbuf,1,0,True);
  
  SSVALS(outbuf,smb_vwv0,-1); /* We don't support smb_remaining */
  
  DEBUG( 3, ( "writebmpx fnum=%d num=%d wrote=%d\n",
	    fsp->fnum, (int)numtowrite, (int)nwritten ) );

  if (write_through && tcount==nwritten) {
    /* we need to send both a primary and a secondary response */
    smb_setlen(outbuf,outsize - 4);
    send_smb(Client,outbuf);

    /* now the secondary */
    outsize = set_message(outbuf,1,0,True);
    CVAL(outbuf,smb_com) = SMBwritec;
    SSVAL(outbuf,smb_vwv0,nwritten);
  }

  return(outsize);
}


/****************************************************************************
  reply to a SMBwritebs (write block multiplex secondary) request
****************************************************************************/
int reply_writebs(connection_struct *conn, char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  size_t numtowrite;
  ssize_t nwritten = -1;
  int outsize = 0;
  SMB_OFF_T startpos;
  size_t tcount;
  BOOL write_through;
  int smb_doff;
  char *data;
  write_bmpx_struct *wbms;
  BOOL send_response = False; 
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  CHECK_FSP(fsp,conn);
  CHECK_WRITE(fsp);

  tcount = SVAL(inbuf,smb_vwv1);
  startpos = IVAL(inbuf,smb_vwv2);
  numtowrite = SVAL(inbuf,smb_vwv6);
  smb_doff = SVAL(inbuf,smb_vwv7);

  data = smb_base(inbuf) + smb_doff;

  /* We need to send an SMBwriteC response, not an SMBwritebs */
  CVAL(outbuf,smb_com) = SMBwritec;

  /* This fd should have an auxiliary struct attached,
     check that it does */
  wbms = fsp->wbmpx_ptr;
  if(!wbms) return(-1);

  /* If write through is set we can return errors, else we must
     cache them */
  write_through = wbms->wr_mode;

  /* Check for an earlier error */
  if(wbms->wr_discard)
    return -1; /* Just discard the packet */

  nwritten = write_file(fsp,data,startpos,numtowrite);

  if(lp_syncalways(SNUM(conn)) || write_through)
    sync_file(conn,fsp);
  
  if (nwritten < (ssize_t)numtowrite)
  {
    if(write_through)
    {
      /* We are returning an error - we can delete the aux struct */
      if (wbms) free((char *)wbms);
      fsp->wbmpx_ptr = NULL;
      return(ERROR(ERRHRD,ERRdiskfull));
    }
    return(CACHE_ERROR(wbms,ERRHRD,ERRdiskfull));
  }

  /* Increment the total written, if this matches tcount
     we can discard the auxiliary struct (hurrah !) and return a writeC */
  wbms->wr_total_written += nwritten;
  if(wbms->wr_total_written >= tcount)
  {
    if (write_through)
    {
      outsize = set_message(outbuf,1,0,True);
      SSVAL(outbuf,smb_vwv0,wbms->wr_total_written);    
      send_response = True;
    }

    free((char *)wbms);
    fsp->wbmpx_ptr = NULL;
  }

  if(send_response)
    return(outsize);

  return(-1);
}


/****************************************************************************
  reply to a SMBsetattrE
****************************************************************************/

int reply_setattrE(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  struct utimbuf unix_times;
  int outsize = 0;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  outsize = set_message(outbuf,0,0,True);

  CHECK_FSP(fsp,conn);
  CHECK_ERROR(fsp);

  /* Convert the DOS times into unix times. Ignore create
     time as UNIX can't set this.
     */
  unix_times.actime = make_unix_date2(inbuf+smb_vwv3);
  unix_times.modtime = make_unix_date2(inbuf+smb_vwv5);
  
  /* 
   * Patch from Ray Frush <frush@engr.colostate.edu>
   * Sometimes times are sent as zero - ignore them.
   */

  if ((unix_times.actime == 0) && (unix_times.modtime == 0)) 
  {
    /* Ignore request */
    if( DEBUGLVL( 3 ) )
      {
      dbgtext( "reply_setattrE fnum=%d ", fsp->fnum);
      dbgtext( "ignoring zero request - not setting timestamps of 0\n" );
      }
    return(outsize);
  }
  else if ((unix_times.actime != 0) && (unix_times.modtime == 0)) 
  {
    /* set modify time = to access time if modify time was 0 */
    unix_times.modtime = unix_times.actime;
  }

  /* Set the date on this file */
  if(file_utime(conn, fsp->fsp_name, &unix_times))
    return(ERROR(ERRDOS,ERRnoaccess));
  
  DEBUG( 3, ( "reply_setattrE fnum=%d actime=%d modtime=%d\n",
            fsp->fnum, (int)unix_times.actime, (int)unix_times.modtime ) );

  return(outsize);
}


/****************************************************************************
  reply to a SMBgetattrE
****************************************************************************/

int reply_getattrE(connection_struct *conn, char *inbuf,char *outbuf, int size, int dum_buffsize)
{
  SMB_STRUCT_STAT sbuf;
  int outsize = 0;
  int mode;
  files_struct *fsp = file_fsp(inbuf,smb_vwv0);

  outsize = set_message(outbuf,11,0,True);

  CHECK_FSP(fsp,conn);
  CHECK_ERROR(fsp);

  /* Do an fstat on this file */
  if(sys_fstat(fsp->fd_ptr->fd, &sbuf))
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  
  mode = dos_mode(conn,fsp->fsp_name,&sbuf);
  
  /* Convert the times into dos times. Set create
     date to be last modify date as UNIX doesn't save
     this */
  put_dos_date2(outbuf,smb_vwv0,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
  put_dos_date2(outbuf,smb_vwv2,sbuf.st_atime);
  put_dos_date2(outbuf,smb_vwv4,sbuf.st_mtime);
  if (mode & aDIR)
    {
      SIVAL(outbuf,smb_vwv6,0);
      SIVAL(outbuf,smb_vwv8,0);
    }
  else
    {
      SIVAL(outbuf,smb_vwv6,(uint32)sbuf.st_size);
      SIVAL(outbuf,smb_vwv8,SMB_ROUNDUP(sbuf.st_size,1024));
    }
  SSVAL(outbuf,smb_vwv10, mode);
  
  DEBUG( 3, ( "reply_getattrE fnum=%d\n", fsp->fnum));
  
  return(outsize);
}
