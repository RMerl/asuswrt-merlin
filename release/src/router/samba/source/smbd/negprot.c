/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   negprot reply code
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

#include "includes.h"

extern int DEBUGLEVEL;
extern int Protocol;
extern int max_recv;
extern fstring global_myworkgroup;
extern fstring remote_machine;

/****************************************************************************
reply for the core protocol
****************************************************************************/
static int reply_corep(char *outbuf)
{
	int outsize = set_message(outbuf,1,0,True);

	Protocol = PROTOCOL_CORE;
	
	return outsize;
}


/****************************************************************************
reply for the coreplus protocol
****************************************************************************/
static int reply_coreplus(char *outbuf)
{
  int raw = (lp_readraw()?1:0) | (lp_writeraw()?2:0);
  int outsize = set_message(outbuf,13,0,True);
  SSVAL(outbuf,smb_vwv5,raw); /* tell redirector we support
				 readbraw and writebraw (possibly) */
  /* Reply, SMBlockread, SMBwritelock supported. */
  SCVAL(outbuf,smb_flg,FLAG_REPLY|FLAG_SUPPORT_LOCKREAD);
  SSVAL(outbuf,smb_vwv1,0x1); /* user level security, don't encrypt */	

  Protocol = PROTOCOL_COREPLUS;

  return outsize;
}


/****************************************************************************
reply for the lanman 1.0 protocol
****************************************************************************/
static int reply_lanman1(char *outbuf)
{
  int raw = (lp_readraw()?1:0) | (lp_writeraw()?2:0);
  int secword=0;
  BOOL doencrypt = SMBENCRYPT();
  time_t t = time(NULL);

  if (lp_security()>=SEC_USER) secword |= 1;
  if (doencrypt) secword |= 2;

  set_message(outbuf,13,doencrypt?8:0,True);
  SSVAL(outbuf,smb_vwv1,secword); 
  /* Create a token value and add it to the outgoing packet. */
  if (doencrypt) 
    generate_next_challenge(smb_buf(outbuf));

  Protocol = PROTOCOL_LANMAN1;

  /* Reply, SMBlockread, SMBwritelock supported. */
  SCVAL(outbuf,smb_flg,FLAG_REPLY|FLAG_SUPPORT_LOCKREAD);
  SSVAL(outbuf,smb_vwv2,max_recv);
  SSVAL(outbuf,smb_vwv3,lp_maxmux()); /* maxmux */
  SSVAL(outbuf,smb_vwv4,1);
  SSVAL(outbuf,smb_vwv5,raw); /* tell redirector we support
				 readbraw writebraw (possibly) */
  SIVAL(outbuf,smb_vwv6,getpid());
  SSVAL(outbuf,smb_vwv10, TimeDiff(t)/60);

  put_dos_date(outbuf,smb_vwv8,t);

  return (smb_len(outbuf)+4);
}


/****************************************************************************
reply for the lanman 2.0 protocol
****************************************************************************/
static int reply_lanman2(char *outbuf)
{
  int raw = (lp_readraw()?1:0) | (lp_writeraw()?2:0);
  int secword=0;
  BOOL doencrypt = SMBENCRYPT();
  time_t t = time(NULL);
  struct cli_state *cli = NULL;
  char cryptkey[8];
  char crypt_len = 0;

  if (lp_security() == SEC_SERVER) {
	  cli = server_cryptkey();
  }

  if (cli) {
	  DEBUG(3,("using password server validation\n"));
	  doencrypt = ((cli->sec_mode & 2) != 0);
  }

  if (lp_security()>=SEC_USER) secword |= 1;
  if (doencrypt) secword |= 2;

  if (doencrypt) {
	  crypt_len = 8;
	  if (!cli) {
		  generate_next_challenge(cryptkey);
	  } else {
		  memcpy(cryptkey, cli->cryptkey, 8);
		  set_challenge(cli->cryptkey);
	  }
  }

  set_message(outbuf,13,crypt_len,True);
  SSVAL(outbuf,smb_vwv1,secword); 
  SIVAL(outbuf,smb_vwv6,getpid());
  if (doencrypt) 
	  memcpy(smb_buf(outbuf), cryptkey, 8);

  Protocol = PROTOCOL_LANMAN2;

  /* Reply, SMBlockread, SMBwritelock supported. */
  SCVAL(outbuf,smb_flg,FLAG_REPLY|FLAG_SUPPORT_LOCKREAD);
  SSVAL(outbuf,smb_vwv2,max_recv);
  SSVAL(outbuf,smb_vwv3,lp_maxmux()); 
  SSVAL(outbuf,smb_vwv4,1);
  SSVAL(outbuf,smb_vwv5,raw); /* readbraw and/or writebraw */
  SSVAL(outbuf,smb_vwv10, TimeDiff(t)/60);
  put_dos_date(outbuf,smb_vwv8,t);

  return (smb_len(outbuf)+4);
}


/****************************************************************************
reply for the nt protocol
****************************************************************************/
static int reply_nt1(char *outbuf)
{
  /* dual names + lock_and_read + nt SMBs + remote API calls */
  int capabilities = CAP_NT_FIND|CAP_LOCK_AND_READ|
                     (lp_nt_smb_support() ? CAP_NT_SMBS | CAP_RPC_REMOTE_APIS : 0) |
                     (SMB_OFF_T_BITS == 64 ? CAP_LARGE_FILES | CAP_LARGE_READX | CAP_LARGE_WRITEX /*| CAP_W2K_SMBS*/ : 0);


/*
  other valid capabilities which we may support at some time...
                     CAP_LARGE_READX|CAP_STATUS32|CAP_LEVEL_II_OPLOCKS;
 */

  int secword=0;
  BOOL doencrypt = SMBENCRYPT();
  time_t t = time(NULL);
  int data_len;
  struct cli_state *cli = NULL;
  char cryptkey[8];
  char crypt_len = 0;

  if (lp_security() == SEC_SERVER) {
	  cli = server_cryptkey();
  }

  if (cli) {
	  DEBUG(3,("using password server validation\n"));
	  doencrypt = ((cli->sec_mode & 2) != 0);
  }

  if (doencrypt) {
	  crypt_len = 8;
	  if (!cli) {
		  generate_next_challenge(cryptkey);
	  } else {
		  memcpy(cryptkey, cli->cryptkey, 8);
		  set_challenge(cli->cryptkey);
	  }
  }

  if (lp_readraw() && lp_writeraw()) {
	  capabilities |= CAP_RAW_MODE;
  }

  if (lp_security() >= SEC_USER) secword |= 1;
  if (doencrypt) secword |= 2;

  /* decide where (if) to put the encryption challenge, and
     follow it with the OEM'd domain name
   */
  data_len = crypt_len + strlen(global_myworkgroup) + 1;

  set_message(outbuf,17,data_len,True);
  pstrcpy(smb_buf(outbuf)+crypt_len, global_myworkgroup);

  CVAL(outbuf,smb_vwv1) = secword;
  SSVALS(outbuf,smb_vwv16+1,crypt_len);
  if (doencrypt) 
	  memcpy(smb_buf(outbuf), cryptkey, 8);

  Protocol = PROTOCOL_NT1;

  SSVAL(outbuf,smb_vwv1+1,lp_maxmux()); /* maxmpx */
  SSVAL(outbuf,smb_vwv2+1,1); /* num vcs */
  SIVAL(outbuf,smb_vwv3+1,0xffff); /* max buffer. LOTS! */
  SIVAL(outbuf,smb_vwv5+1,0x10000); /* raw size. full 64k */
  SIVAL(outbuf,smb_vwv7+1,getpid()); /* session key */
  SIVAL(outbuf,smb_vwv9+1,capabilities); /* capabilities */
  put_long_date(outbuf+smb_vwv11+1,t);
  SSVALS(outbuf,smb_vwv15+1,TimeDiff(t)/60);
  SSVAL(outbuf,smb_vwv17,data_len); /* length of challenge+domain strings */

  return (smb_len(outbuf)+4);
}

/* these are the protocol lists used for auto architecture detection:

WinNT 3.51:
protocol [PC NETWORK PROGRAM 1.0]
protocol [XENIX CORE]
protocol [MICROSOFT NETWORKS 1.03]
protocol [LANMAN1.0]
protocol [Windows for Workgroups 3.1a]
protocol [LM1.2X002]
protocol [LANMAN2.1]
protocol [NT LM 0.12]

Win95:
protocol [PC NETWORK PROGRAM 1.0]
protocol [XENIX CORE]
protocol [MICROSOFT NETWORKS 1.03]
protocol [LANMAN1.0]
protocol [Windows for Workgroups 3.1a]
protocol [LM1.2X002]
protocol [LANMAN2.1]
protocol [NT LM 0.12]

Win2K:
protocol [PC NETWORK PROGRAM 1.0]
protocol [LANMAN1.0]
protocol [Windows for Workgroups 3.1a]
protocol [LM1.2X002]
protocol [LANMAN2.1]
protocol [NT LM 0.12]

OS/2:
protocol [PC NETWORK PROGRAM 1.0]
protocol [XENIX CORE]
protocol [LANMAN1.0]
protocol [LM1.2X002]
protocol [LANMAN2.1]
*/

/*
  * Modified to recognize the architecture of the remote machine better.
  *
  * This appears to be the matrix of which protocol is used by which
  * MS product.
       Protocol                       WfWg    Win95   WinNT  Win2K  OS/2
       PC NETWORK PROGRAM 1.0          1       1       1      1      1
       XENIX CORE                                      2             2
       MICROSOFT NETWORKS 3.0          2       2       
       DOS LM1.2X002                   3       3       
       MICROSOFT NETWORKS 1.03                         3
       DOS LANMAN2.1                   4       4       
       LANMAN1.0                                       4      2      3
       Windows for Workgroups 3.1a     5       5       5      3
       LM1.2X002                                       6      4      4
       LANMAN2.1                                       7      5      5
       NT LM 0.12                              6       8      6
  *
  *  tim@fsg.com 09/29/95
  *  Win2K added by matty 17/7/99
  */
  
#define ARCH_WFWG     0x3      /* This is a fudge because WfWg is like Win95 */
#define ARCH_WIN95    0x2
#define ARCH_WINNT    0x4
#define ARCH_WIN2K    0xC      /* Win2K is like NT */
#define ARCH_OS2      0x14     /* Again OS/2 is like NT */
#define ARCH_SAMBA    0x20
 
#define ARCH_ALL      0x3F
 
/* List of supported protocols, most desired first */
static struct {
  char *proto_name;
  char *short_name;
  int (*proto_reply_fn)(char *);
  int protocol_level;
} supported_protocols[] = {
  {"NT LANMAN 1.0",           "NT1",      reply_nt1,      PROTOCOL_NT1},
  {"NT LM 0.12",              "NT1",      reply_nt1,      PROTOCOL_NT1},
  {"LM1.2X002",               "LANMAN2",  reply_lanman2,  PROTOCOL_LANMAN2},
  {"Samba",                   "LANMAN2",  reply_lanman2,  PROTOCOL_LANMAN2},
  {"DOS LM1.2X002",           "LANMAN2",  reply_lanman2,  PROTOCOL_LANMAN2},
  {"LANMAN1.0",               "LANMAN1",  reply_lanman1,  PROTOCOL_LANMAN1},
  {"MICROSOFT NETWORKS 3.0",  "LANMAN1",  reply_lanman1,  PROTOCOL_LANMAN1},
  {"MICROSOFT NETWORKS 1.03", "COREPLUS", reply_coreplus, PROTOCOL_COREPLUS},
  {"PC NETWORK PROGRAM 1.0",  "CORE",     reply_corep,    PROTOCOL_CORE}, 
  {NULL,NULL,NULL,0},
};


/****************************************************************************
  reply to a negprot
****************************************************************************/
int reply_negprot(connection_struct *conn, 
		  char *inbuf,char *outbuf, int dum_size, 
		  int dum_buffsize)
{
  int outsize = set_message(outbuf,1,0,True);
  int Index=0;
  int choice= -1;
  int protocol;
  char *p;
  int bcc = SVAL(smb_buf(inbuf),-2);
  int arch = ARCH_ALL;

  p = smb_buf(inbuf)+1;
  while (p < (smb_buf(inbuf) + bcc))
    { 
      Index++;
      DEBUG(3,("Requested protocol [%s]\n",p));
      if (strcsequal(p,"Windows for Workgroups 3.1a"))
	arch &= ( ARCH_WFWG | ARCH_WIN95 | ARCH_WINNT | ARCH_WIN2K );
      else if (strcsequal(p,"DOS LM1.2X002"))
	arch &= ( ARCH_WFWG | ARCH_WIN95 );
      else if (strcsequal(p,"DOS LANMAN2.1"))
	arch &= ( ARCH_WFWG | ARCH_WIN95 );
      else if (strcsequal(p,"NT LM 0.12"))
	arch &= ( ARCH_WIN95 | ARCH_WINNT | ARCH_WIN2K );
      else if (strcsequal(p,"LANMAN2.1"))
	arch &= ( ARCH_WINNT | ARCH_WIN2K | ARCH_OS2 );
      else if (strcsequal(p,"LM1.2X002"))
	arch &= ( ARCH_WINNT | ARCH_WIN2K | ARCH_OS2 );
      else if (strcsequal(p,"MICROSOFT NETWORKS 1.03"))
	arch &= ARCH_WINNT;
      else if (strcsequal(p,"XENIX CORE"))
	arch &= ( ARCH_WINNT | ARCH_OS2 );
      else if (strcsequal(p,"Samba")) {
	arch = ARCH_SAMBA;
	break;
      }
 
      p += strlen(p) + 2;
    }

  switch ( arch ) {
  case ARCH_SAMBA:
    set_remote_arch(RA_SAMBA);
    break;
  case ARCH_WFWG:
    set_remote_arch(RA_WFWG);
    break;
  case ARCH_WIN95:
    set_remote_arch(RA_WIN95);
    break;
  case ARCH_WINNT:
    if(SVAL(inbuf,smb_flg2)==FLAGS2_WIN2K_SIGNATURE)
      set_remote_arch(RA_WIN2K);
    else
      set_remote_arch(RA_WINNT);
    break;
  case ARCH_WIN2K:
    set_remote_arch(RA_WIN2K);
    break;
  case ARCH_OS2:
    set_remote_arch(RA_OS2);
    break;
  default:
    set_remote_arch(RA_UNKNOWN);
    break;
  }
 
  /* possibly reload - change of architecture */
  reload_services(True);      
    
  /* a special case to stop password server loops */
  if (Index == 1 && strequal(remote_machine,myhostname()) && 
      (lp_security()==SEC_SERVER || lp_security()==SEC_DOMAIN))
    exit_server("Password server loop!");
  
  /* Check for protocols, most desirable first */
  for (protocol = 0; supported_protocols[protocol].proto_name; protocol++)
    {
      p = smb_buf(inbuf)+1;
      Index = 0;
      if (lp_maxprotocol() >= supported_protocols[protocol].protocol_level)
	while (p < (smb_buf(inbuf) + bcc))
	  { 
	    if (strequal(p,supported_protocols[protocol].proto_name))
	      choice = Index;
	    Index++;
	    p += strlen(p) + 2;
	  }
      if(choice != -1)
	break;
    }
  
  SSVAL(outbuf,smb_vwv0,choice);
  if(choice != -1) {
    extern fstring remote_proto;
    fstrcpy(remote_proto,supported_protocols[protocol].short_name);
    reload_services(True);          
    outsize = supported_protocols[protocol].proto_reply_fn(outbuf);
    DEBUG(3,("Selected protocol %s\n",supported_protocols[protocol].proto_name));
  }
  else {
    DEBUG(0,("No protocol supported !\n"));
  }
  SSVAL(outbuf,smb_vwv0,choice);
  
  DEBUG( 5, ( "negprot index=%d\n", choice ) );

  return(outsize);
}
