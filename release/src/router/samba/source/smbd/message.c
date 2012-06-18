/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB messaging
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
   This file handles the messaging system calls for winpopup style
   messages
*/


#include "includes.h"

/* look in server.c for some explanation of these variables */
extern int DEBUGLEVEL;


static char msgbuf[1600];
static int msgpos=0;
static fstring msgfrom="";
static fstring msgto="";

/****************************************************************************
deliver the message
****************************************************************************/
static void msg_deliver(void)
{
  pstring name;
  int i;
  int fd;

  if (! (*lp_msg_command()))
    {
      DEBUG(1,("no messaging command specified\n"));
      msgpos = 0;
      return;
    }

  /* put it in a temporary file */
  slprintf(name,sizeof(name)-1, "%s/msg.XXXXXX",tmpdir());
  fd = smb_mkstemp(name);
  if (fd == -1) {
	  DEBUG(1,("can't open message file %s\n",name));
	  return;
  }

  /*
   * Incoming message is in DOS codepage format. Convert to UNIX in
   * place.
   */

  if(msgpos > 0) {
    msgbuf[msgpos] = '\0'; /* Ensure null terminated. */
    dos_to_unix(msgbuf,True);
  }

  for (i=0;i<msgpos;) {
    if (msgbuf[i]=='\r' && i<(msgpos-1) && msgbuf[i+1]=='\n') {
      i++; continue;      
    }
    write(fd,&msgbuf[i++],1);
  }
  close(fd);


  /* run the command */
  if (*lp_msg_command())
    {
      fstring alpha_msgfrom;
      fstring alpha_msgto;
      pstring s;

      pstrcpy(s,lp_msg_command());
      pstring_sub(s,"%f",alpha_strcpy(alpha_msgfrom,msgfrom,NULL,sizeof(alpha_msgfrom)));
      pstring_sub(s,"%t",alpha_strcpy(alpha_msgto,msgto,NULL,sizeof(alpha_msgto)));
      standard_sub_basic(s);
      pstring_sub(s,"%s",name);
      smbrun(s,NULL,False);
    }

  msgpos = 0;
}



/****************************************************************************
  reply to a sends
****************************************************************************/
int reply_sends(connection_struct *conn,
		char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int len;
  char *orig,*dest,*msg;
  int outsize = 0;

  msgpos = 0;

  if (! (*lp_msg_command()))
    return(ERROR(ERRSRV,ERRmsgoff));

  outsize = set_message(outbuf,0,0,True);

  orig = smb_buf(inbuf)+1;
  dest = skip_string(orig,1)+1;
  msg = skip_string(dest,1)+1;

  fstrcpy(msgfrom,orig);
  fstrcpy(msgto,dest);

  len = SVAL(msg,0);
  len = MIN(len,sizeof(msgbuf)-msgpos);

  memset(msgbuf,'\0',sizeof(msgbuf));

  memcpy(&msgbuf[msgpos],msg+2,len);
  msgpos += len;

  DEBUG( 3, ( "SMBsends (from %s to %s)\n", orig, dest ) );

  msg_deliver();

  return(outsize);
}


/****************************************************************************
  reply to a sendstrt
****************************************************************************/
int reply_sendstrt(connection_struct *conn,
		   char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  char *orig,*dest;
  int outsize = 0;

  if (! (*lp_msg_command()))
    return(ERROR(ERRSRV,ERRmsgoff));

  outsize = set_message(outbuf,1,0,True);

  memset(msgbuf,'\0',sizeof(msgbuf));
  msgpos = 0;

  orig = smb_buf(inbuf)+1;
  dest = skip_string(orig,1)+1;

  fstrcpy(msgfrom,orig);
  fstrcpy(msgto,dest);

  DEBUG( 3, ( "SMBsendstrt (from %s to %s)\n", msgfrom, msgto ) );

  return(outsize);
}


/****************************************************************************
  reply to a sendtxt
****************************************************************************/
int reply_sendtxt(connection_struct *conn,
		  char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int len;
  int outsize = 0;
  char *msg;

  if (! (*lp_msg_command()))
    return(ERROR(ERRSRV,ERRmsgoff));

  outsize = set_message(outbuf,0,0,True);

  msg = smb_buf(inbuf) + 1;

  len = SVAL(msg,0);
  len = MIN(len,sizeof(msgbuf)-msgpos);

  memcpy(&msgbuf[msgpos],msg+2,len);
  msgpos += len;

  DEBUG( 3, ( "SMBsendtxt\n" ) );

  return(outsize);
}


/****************************************************************************
  reply to a sendend
****************************************************************************/
int reply_sendend(connection_struct *conn,
		  char *inbuf,char *outbuf, int dum_size, int dum_buffsize)
{
  int outsize = 0;

  if (! (*lp_msg_command()))
    return(ERROR(ERRSRV,ERRmsgoff));

  outsize = set_message(outbuf,0,0,True);

  DEBUG(3,("SMBsendend\n"));

  msg_deliver();

  return(outsize);
}
