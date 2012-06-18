/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB client generic functions
   Copyright (C) Andrew Tridgell 1994-1998
   
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

#define NO_SYSLOG

#include "includes.h"
#include "trans2.h"


extern int DEBUGLEVEL;
extern pstring user_socket_options;

static void cli_process_oplock(struct cli_state *cli);

/*
 * Change the port number used to call on 
 */
int cli_set_port(struct cli_state *cli, int port)
{
	if (port > 0)
	  cli->port = port;

	return cli->port;
}

/****************************************************************************
recv an smb
****************************************************************************/
static BOOL cli_receive_smb(struct cli_state *cli)
{
	BOOL ret;
 again:
	ret = client_receive_smb(cli->fd,cli->inbuf,cli->timeout);
	
	if (ret) {
		/* it might be an oplock break request */
		if (!(CVAL(cli->inbuf, smb_flg) & FLAG_REPLY) &&
		    CVAL(cli->inbuf,smb_com) == SMBlockingX &&
		    SVAL(cli->inbuf,smb_vwv6) == 0 &&
		    SVAL(cli->inbuf,smb_vwv7) == 0) {
			if (cli->use_oplocks) cli_process_oplock(cli);
			/* try to prevent loops */
			CVAL(cli->inbuf,smb_com) = 0xFF;
			goto again;
		}
	}

	return ret;
}

/****************************************************************************
  send an smb to a fd and re-establish if necessary
****************************************************************************/
static BOOL cli_send_smb(struct cli_state *cli)
{
	size_t len;
	size_t nwritten=0;
	ssize_t ret;
	BOOL reestablished=False;

	len = smb_len(cli->outbuf) + 4;

	while (nwritten < len) {
		ret = write_socket(cli->fd,cli->outbuf+nwritten,len - nwritten);
		if (ret <= 0 && errno == EPIPE && !reestablished) {
			if (cli_reestablish_connection(cli)) {
				reestablished = True;
				nwritten=0;
				continue;
			}
		}
		if (ret <= 0) {
			DEBUG(0,("Error writing %d bytes to client. %d. Exiting\n",
				 (int)len,(int)ret));
			close_sockets();
			exit(1);
		}
		nwritten += ret;
	}
	
	return True;
}

/****************************************************************************
setup basics in a outgoing packet
****************************************************************************/
static void cli_setup_packet(struct cli_state *cli)
{
        cli->rap_error = 0;
        cli->nt_error = 0;
	SSVAL(cli->outbuf,smb_pid,cli->pid);
	SSVAL(cli->outbuf,smb_uid,cli->vuid);
	SSVAL(cli->outbuf,smb_mid,cli->mid);
	if (cli->protocol > PROTOCOL_CORE) {
		SCVAL(cli->outbuf,smb_flg,0x8);
		SSVAL(cli->outbuf,smb_flg2,0x1);
	}
}



/****************************************************************************
process an oplock break request from the server
****************************************************************************/
static void cli_process_oplock(struct cli_state *cli)
{
	char *oldbuf = cli->outbuf;
	pstring buf;
	int fnum;

	fnum = SVAL(cli->inbuf,smb_vwv2);

	/* damn, we really need to keep a record of open files so we
	   can detect a oplock break and a close crossing on the
	   wire. for now this swallows the errors */
	if (fnum == 0) return;

	cli->outbuf = buf;

        memset(buf,'\0',smb_size);
        set_message(buf,8,0,True);

        CVAL(buf,smb_com) = SMBlockingX;
	SSVAL(buf,smb_tid, cli->cnum);
        cli_setup_packet(cli);
	SSVAL(buf,smb_vwv0,0xFF);
	SSVAL(buf,smb_vwv1,0);
	SSVAL(buf,smb_vwv2,fnum);
	SSVAL(buf,smb_vwv3,2); /* oplock break ack */
	SIVAL(buf,smb_vwv4,0); /* timoeut */
	SSVAL(buf,smb_vwv6,0); /* unlockcount */
	SSVAL(buf,smb_vwv7,0); /* lockcount */

        cli_send_smb(cli);	

	cli->outbuf = oldbuf;
}


/*****************************************************
 RAP error codes - a small start but will be extended.
*******************************************************/

struct
{
  int err;
  char *message;
} rap_errmap[] =
{
  {5,    "User has insufficient privilege" },
  {86,   "The specified password is invalid" },
  {2226, "Operation only permitted on a Primary Domain Controller"  },
  {2242, "The password of this user has expired." },
  {2243, "The password of this user cannot change." },
  {2244, "This password cannot be used now (password history conflict)." },
  {2245, "The password is shorter than required." },
  {2246, "The password of this user is too recent to change."},

  /* these really shouldn't be here ... */
  {0x80, "Not listening on called name"},
  {0x81, "Not listening for calling name"},
  {0x82, "Called name not present"},
  {0x83, "Called name present, but insufficient resources"},

  {0, NULL}
};  

/****************************************************************************
  return a description of an SMB error
****************************************************************************/
static char *cli_smb_errstr(struct cli_state *cli)
{
	return smb_errstr(cli->inbuf);
}

/******************************************************
 Return an error message - either an SMB error or a RAP
 error.
*******************************************************/
    
char *cli_errstr(struct cli_state *cli)
{   
	static fstring error_message;
	uint8 errclass;
	uint32 errnum;
	uint32 nt_rpc_error;
	int i;      

	/*  
	 * Errors are of three kinds - smb errors,
	 * dealt with by cli_smb_errstr, NT errors,
	 * whose code is in cli.nt_error, and rap
	 * errors, whose error code is in cli.rap_error.
	 */ 

	cli_error(cli, &errclass, &errnum, &nt_rpc_error);

	if (errclass != 0)
	{
		return cli_smb_errstr(cli);
	}

	/*
	 * Was it an NT error ?
	 */

	if (nt_rpc_error)
	{
		char *nt_msg = get_nt_error_msg(nt_rpc_error);

		if (nt_msg == NULL)
		{
			slprintf(error_message, sizeof(fstring) - 1, "NT code %d", nt_rpc_error);
		}
		else
		{
			fstrcpy(error_message, nt_msg);
		}

		return error_message;
	}

	/*
	 * Must have been a rap error.
	 */

	slprintf(error_message, sizeof(error_message) - 1, "code %d", cli->rap_error);

	for (i = 0; rap_errmap[i].message != NULL; i++)
	{
		if (rap_errmap[i].err == cli->rap_error)
		{
			fstrcpy( error_message, rap_errmap[i].message);
			break;
		}
	} 

	return error_message;
}

/*****************************************************************************
 Convert a character pointer in a cli_call_api() response to a form we can use.
 This function contains code to prevent core dumps if the server returns 
 invalid data.
*****************************************************************************/
static char *fix_char_ptr(unsigned int datap, unsigned int converter, 
			  char *rdata, int rdrcnt)
{
	if (datap == 0)	{	/* turn NULL pointers into zero length strings */
		return "";
	} else {
		unsigned int offset = datap - converter;

		if (offset >= rdrcnt) {
			DEBUG(1,("bad char ptr: datap=%u, converter=%u rdrcnt=%d>",
				 datap, converter, rdrcnt));
			return "<ERROR>";
		} else {
			return &rdata[offset];
		}
	}
}

/****************************************************************************
  send a SMB trans or trans2 request
  ****************************************************************************/
static BOOL cli_send_trans(struct cli_state *cli, int trans, 
                           char *name, int pipe_name_len, 
                           int fid, int flags,
                           uint16 *setup, int lsetup, int msetup,
                           char *param, int lparam, int mparam,
                           char *data, int ldata, int mdata)
{
	int i;
	int this_ldata,this_lparam;
	int tot_data=0,tot_param=0;
	char *outdata,*outparam;
	char *p;

	this_lparam = MIN(lparam,cli->max_xmit - (500+lsetup*2)); /* hack */
	this_ldata = MIN(ldata,cli->max_xmit - (500+lsetup*2+this_lparam));

	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,14+lsetup,0,True);
	CVAL(cli->outbuf,smb_com) = trans;
	SSVAL(cli->outbuf,smb_tid, cli->cnum);
	cli_setup_packet(cli);

	outparam = smb_buf(cli->outbuf)+(trans==SMBtrans ? pipe_name_len+1 : 3);
	outdata = outparam+this_lparam;

	/* primary request */
	SSVAL(cli->outbuf,smb_tpscnt,lparam);	/* tpscnt */
	SSVAL(cli->outbuf,smb_tdscnt,ldata);	/* tdscnt */
	SSVAL(cli->outbuf,smb_mprcnt,mparam);	/* mprcnt */
	SSVAL(cli->outbuf,smb_mdrcnt,mdata);	/* mdrcnt */
	SCVAL(cli->outbuf,smb_msrcnt,msetup);	/* msrcnt */
	SSVAL(cli->outbuf,smb_flags,flags);	/* flags */
	SIVAL(cli->outbuf,smb_timeout,0);		/* timeout */
	SSVAL(cli->outbuf,smb_pscnt,this_lparam);	/* pscnt */
	SSVAL(cli->outbuf,smb_psoff,smb_offset(outparam,cli->outbuf)); /* psoff */
	SSVAL(cli->outbuf,smb_dscnt,this_ldata);	/* dscnt */
	SSVAL(cli->outbuf,smb_dsoff,smb_offset(outdata,cli->outbuf)); /* dsoff */
	SCVAL(cli->outbuf,smb_suwcnt,lsetup);	/* suwcnt */
	for (i=0;i<lsetup;i++)		/* setup[] */
		SSVAL(cli->outbuf,smb_setup+i*2,setup[i]);
	p = smb_buf(cli->outbuf);
	if (trans==SMBtrans) {
		memcpy(p,name, pipe_name_len + 1);  /* name[] */
	} else {
		*p++ = 0;  /* put in a null smb_name */
		*p++ = 'D'; *p++ = ' ';	/* observed in OS/2 */
	}
	if (this_lparam)			/* param[] */
		memcpy(outparam,param,this_lparam);
	if (this_ldata)			/* data[] */
		memcpy(outdata,data,this_ldata);
	set_message(cli->outbuf,14+lsetup,		/* wcnt, bcc */
		    PTR_DIFF(outdata+this_ldata,smb_buf(cli->outbuf)),False);

	show_msg(cli->outbuf);
	cli_send_smb(cli);

	if (this_ldata < ldata || this_lparam < lparam) {
		/* receive interim response */
		if (!cli_receive_smb(cli) || 
		    CVAL(cli->inbuf,smb_rcls) != 0) {
			return(False);
		}      

		tot_data = this_ldata;
		tot_param = this_lparam;
		
		while (tot_data < ldata || tot_param < lparam)  {
			this_lparam = MIN(lparam-tot_param,cli->max_xmit - 500); /* hack */
			this_ldata = MIN(ldata-tot_data,cli->max_xmit - (500+this_lparam));

			set_message(cli->outbuf,trans==SMBtrans?8:9,0,True);
			CVAL(cli->outbuf,smb_com) = trans==SMBtrans ? SMBtranss : SMBtranss2;
			
			outparam = smb_buf(cli->outbuf);
			outdata = outparam+this_lparam;
			
			/* secondary request */
			SSVAL(cli->outbuf,smb_tpscnt,lparam);	/* tpscnt */
			SSVAL(cli->outbuf,smb_tdscnt,ldata);	/* tdscnt */
			SSVAL(cli->outbuf,smb_spscnt,this_lparam);	/* pscnt */
			SSVAL(cli->outbuf,smb_spsoff,smb_offset(outparam,cli->outbuf)); /* psoff */
			SSVAL(cli->outbuf,smb_spsdisp,tot_param);	/* psdisp */
			SSVAL(cli->outbuf,smb_sdscnt,this_ldata);	/* dscnt */
			SSVAL(cli->outbuf,smb_sdsoff,smb_offset(outdata,cli->outbuf)); /* dsoff */
			SSVAL(cli->outbuf,smb_sdsdisp,tot_data);	/* dsdisp */
			if (trans==SMBtrans2)
				SSVALS(cli->outbuf,smb_sfid,fid);		/* fid */
			if (this_lparam)			/* param[] */
				memcpy(outparam,param+tot_param,this_lparam);
			if (this_ldata)			/* data[] */
				memcpy(outdata,data+tot_data,this_ldata);
			set_message(cli->outbuf,trans==SMBtrans?8:9, /* wcnt, bcc */
				    PTR_DIFF(outdata+this_ldata,smb_buf(cli->outbuf)),False);
			
			show_msg(cli->outbuf);
			cli_send_smb(cli);
			
			tot_data += this_ldata;
			tot_param += this_lparam;
		}
	}

	return(True);
}


/****************************************************************************
  receive a SMB trans or trans2 response allocating the necessary memory
  ****************************************************************************/
static BOOL cli_receive_trans(struct cli_state *cli,int trans,
                              char **param, int *param_len,
                              char **data, int *data_len)
{
	int total_data=0;
	int total_param=0;
	int this_data,this_param;
	uint8 eclass;
	uint32 ecode;

	*data_len = *param_len = 0;

	if (!cli_receive_smb(cli))
		return False;

	show_msg(cli->inbuf);
	
	/* sanity check */
	if (CVAL(cli->inbuf,smb_com) != trans) {
		DEBUG(0,("Expected %s response, got command 0x%02x\n",
			 trans==SMBtrans?"SMBtrans":"SMBtrans2", 
			 CVAL(cli->inbuf,smb_com)));
		return(False);
	}

	/*
	 * An NT RPC pipe call can return ERRDOS, ERRmoredata
	 * to a trans call. This is not an error and should not
	 * be treated as such.
	 */

	if (cli_error(cli, &eclass, &ecode, NULL))
	{
        if(cli->nt_pipe_fnum == 0 || !(eclass == ERRDOS && ecode == ERRmoredata))
			return(False);
	}

	/* parse out the lengths */
	total_data = SVAL(cli->inbuf,smb_tdrcnt);
	total_param = SVAL(cli->inbuf,smb_tprcnt);

	/* allocate it */
	*data = Realloc(*data,total_data);
	*param = Realloc(*param,total_param);

	while (1)  {
		this_data = SVAL(cli->inbuf,smb_drcnt);
		this_param = SVAL(cli->inbuf,smb_prcnt);

		if (this_data + *data_len > total_data ||
		    this_param + *param_len > total_param) {
			DEBUG(1,("Data overflow in cli_receive_trans\n"));
			return False;
		}

		if (this_data)
			memcpy(*data + SVAL(cli->inbuf,smb_drdisp),
			       smb_base(cli->inbuf) + SVAL(cli->inbuf,smb_droff),
			       this_data);
		if (this_param)
			memcpy(*param + SVAL(cli->inbuf,smb_prdisp),
			       smb_base(cli->inbuf) + SVAL(cli->inbuf,smb_proff),
			       this_param);
		*data_len += this_data;
		*param_len += this_param;

		/* parse out the total lengths again - they can shrink! */
		total_data = SVAL(cli->inbuf,smb_tdrcnt);
		total_param = SVAL(cli->inbuf,smb_tprcnt);
		
		if (total_data <= *data_len && total_param <= *param_len)
			break;
		
		if (!cli_receive_smb(cli))
			return False;

		show_msg(cli->inbuf);
		
		/* sanity check */
		if (CVAL(cli->inbuf,smb_com) != trans) {
			DEBUG(0,("Expected %s response, got command 0x%02x\n",
				 trans==SMBtrans?"SMBtrans":"SMBtrans2", 
				 CVAL(cli->inbuf,smb_com)));
			return(False);
		}
		if (cli_error(cli, &eclass, &ecode, NULL))
		{
        	if(cli->nt_pipe_fnum == 0 || !(eclass == ERRDOS && ecode == ERRmoredata))
				return(False);
		}
	}
	
	return(True);
}

/****************************************************************************
Call a remote api on an arbitrary pipe.  takes param, data and setup buffers.
****************************************************************************/
BOOL cli_api_pipe(struct cli_state *cli, char *pipe_name, int pipe_name_len,
                  uint16 *setup, uint32 setup_count, uint32 max_setup_count,
                  char *params, uint32 param_count, uint32 max_param_count,
                  char *data, uint32 data_count, uint32 max_data_count,
                  char **rparam, uint32 *rparam_count,
                  char **rdata, uint32 *rdata_count)
{
  if (pipe_name_len == 0)
    pipe_name_len = strlen(pipe_name);

  cli_send_trans(cli, SMBtrans, 
                 pipe_name, pipe_name_len,
                 0,0,                         /* fid, flags */
                 setup, setup_count, max_setup_count,
                 params, param_count, max_param_count,
                 data, data_count, max_data_count);

  return (cli_receive_trans(cli, SMBtrans, 
                            rparam, (int *)rparam_count,
                            rdata, (int *)rdata_count));
}

/****************************************************************************
call a remote api
****************************************************************************/
BOOL cli_api(struct cli_state *cli,
	     char *param, int prcnt, int mprcnt,
	     char *data, int drcnt, int mdrcnt,
	     char **rparam, int *rprcnt,
	     char **rdata, int *rdrcnt)
{
  cli_send_trans(cli,SMBtrans,
                 PIPE_LANMAN,strlen(PIPE_LANMAN), /* Name, length */
                 0,0,                             /* fid, flags */
                 NULL,0,0,                /* Setup, length, max */
                 param, prcnt, mprcnt,    /* Params, length, max */
                 data, drcnt, mdrcnt      /* Data, length, max */ 
                );

  return (cli_receive_trans(cli,SMBtrans,
                            rparam, rprcnt,
                            rdata, rdrcnt));
}


/****************************************************************************
perform a NetWkstaUserLogon
****************************************************************************/
BOOL cli_NetWkstaUserLogon(struct cli_state *cli,char *user, char *workstation)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	int rdrcnt,rprcnt;
	pstring param;

	memset(param, 0, sizeof(param));
	
	/* send a SMBtrans command with api NetWkstaUserLogon */
	p = param;
	SSVAL(p,0,132); /* api number */
	p += 2;
	pstrcpy(p,"OOWb54WrLh");
	p = skip_string(p,1);
	pstrcpy(p,"WB21BWDWWDDDDDDDzzzD");
	p = skip_string(p,1);
	SSVAL(p,0,1);
	p += 2;
	pstrcpy(p,user);
	strupper(p);
	p += 21;
	p++;
	p += 15;
	p++; 
	pstrcpy(p, workstation); 
	strupper(p);
	p += 16;
	SSVAL(p, 0, CLI_BUFFER_SIZE);
	p += 2;
	SSVAL(p, 0, CLI_BUFFER_SIZE);
	p += 2;
	
	if (cli_api(cli, 
                    param, PTR_DIFF(p,param),1024,  /* param, length, max */
                    NULL, 0, CLI_BUFFER_SIZE,           /* data, length, max */
                    &rparam, &rprcnt,               /* return params, return size */
                    &rdata, &rdrcnt                 /* return data, return size */
                   )) {
		cli->rap_error = SVAL(rparam,0);
		p = rdata;
		
		if (cli->rap_error == 0) {
			DEBUG(4,("NetWkstaUserLogon success\n"));
			cli->privileges = SVAL(p, 24);
			fstrcpy(cli->eff_name,p+2);
		} else {
			DEBUG(1,("NetwkstaUserLogon gave error %d\n", cli->rap_error));
		}
	}
	
	if (rparam)
      free(rparam);
	if (rdata)
      free(rdata);
	return (cli->rap_error == 0);
}

/****************************************************************************
call a NetShareEnum - try and browse available connections on a host
****************************************************************************/
int cli_RNetShareEnum(struct cli_state *cli, void (*fn)(const char *, uint32, const char *))
{
  char *rparam = NULL;
  char *rdata = NULL;
  char *p;
  int rdrcnt,rprcnt;
  pstring param;
  int count = -1;

  /* now send a SMBtrans command with api RNetShareEnum */
  p = param;
  SSVAL(p,0,0); /* api number */
  p += 2;
  pstrcpy(p,"WrLeh");
  p = skip_string(p,1);
  pstrcpy(p,"B13BWz");
  p = skip_string(p,1);
  SSVAL(p,0,1);
  /*
   * Win2k needs a *smaller* buffer than 0xFFFF here -
   * it returns "out of server memory" with 0xFFFF !!! JRA.
   */
  SSVAL(p,2,0xFFE0);
  p += 4;

  if (cli_api(cli, 
              param, PTR_DIFF(p,param), 1024,  /* Param, length, maxlen */
              NULL, 0, 0xFFE0,            /* data, length, maxlen - Win2k needs a small buffer here too ! */
              &rparam, &rprcnt,                /* return params, length */
              &rdata, &rdrcnt))                /* return data, length */
    {
      int res = SVAL(rparam,0);
      int converter=SVAL(rparam,2);
      int i;
      
      if (res == 0 || res == ERRmoredata) {
	      count=SVAL(rparam,4);
	      p = rdata;

	      for (i=0;i<count;i++,p+=20) {
		      char *sname = p;
		      int type = SVAL(p,14);
		      int comment_offset = IVAL(p,16) & 0xFFFF;
		      char *cmnt = comment_offset?(rdata+comment_offset-converter):"";
			  dos_to_unix(sname,True);
			  dos_to_unix(cmnt,True);
		      fn(sname, type, cmnt);
	      }
      } else {
	      DEBUG(4,("NetShareEnum res=%d\n", res));
      }      
    } else {
	      DEBUG(4,("NetShareEnum failed\n"));
    }
  
  if (rparam)
    free(rparam);
  if (rdata)
    free(rdata);

  return count;
}


/****************************************************************************
call a NetServerEnum for the specified workgroup and servertype mask.
This function then calls the specified callback function for each name returned.

The callback function takes 3 arguments: the machine name, the server type and
the comment.
****************************************************************************/
BOOL cli_NetServerEnum(struct cli_state *cli, char *workgroup, uint32 stype,
		       void (*fn)(const char *, uint32, const char *))
{
	char *rparam = NULL;
	char *rdata = NULL;
	int rdrcnt,rprcnt;
	char *p;
	pstring param;
	int uLevel = 1;
	int count = -1;
  
	/* send a SMBtrans command with api NetServerEnum */
	p = param;
	SSVAL(p,0,0x68); /* api number */
	p += 2;
	pstrcpy(p,"WrLehDz");
	p = skip_string(p,1);
  
	pstrcpy(p,"B16BBDz");
  
	p = skip_string(p,1);
	SSVAL(p,0,uLevel);
	SSVAL(p,2,CLI_BUFFER_SIZE);
	p += 4;
	SIVAL(p,0,stype);
	p += 4;
	
	pstrcpy(p, workgroup);
	p = skip_string(p,1);
	
	if (cli_api(cli, 
                    param, PTR_DIFF(p,param), 8,        /* params, length, max */
                    NULL, 0, CLI_BUFFER_SIZE,               /* data, length, max */
                    &rparam, &rprcnt,                   /* return params, return size */
                    &rdata, &rdrcnt                     /* return data, return size */
                   )) {
		int res = SVAL(rparam,0);
		int converter=SVAL(rparam,2);
		int i;
			
		if (res == 0 || res == ERRmoredata) {
			count=SVAL(rparam,4);
			p = rdata;
					
			for (i = 0;i < count;i++, p += 26) {
				char *sname = p;
				int comment_offset = (IVAL(p,22) & 0xFFFF)-converter;
				char *cmnt = comment_offset?(rdata+comment_offset):"";
				if (comment_offset < 0 || comment_offset > rdrcnt) continue;

				stype = IVAL(p,18) & ~SV_TYPE_LOCAL_LIST_ONLY;

				dos_to_unix(sname, True);
				dos_to_unix(cmnt, True);
				fn(sname, stype, cmnt);
			}
		}
	}
  
	if (rparam)
      free(rparam);
	if (rdata)
      free(rdata);
	
	return(count > 0);
}




static  struct {
    int prot;
    char *name;
  }
prots[] = 
    {
      {PROTOCOL_CORE,"PC NETWORK PROGRAM 1.0"},
      {PROTOCOL_COREPLUS,"MICROSOFT NETWORKS 1.03"},
      {PROTOCOL_LANMAN1,"MICROSOFT NETWORKS 3.0"},
      {PROTOCOL_LANMAN1,"LANMAN1.0"},
      {PROTOCOL_LANMAN2,"LM1.2X002"},
      {PROTOCOL_LANMAN2,"Samba"},
      {PROTOCOL_NT1,"NT LANMAN 1.0"},
      {PROTOCOL_NT1,"NT LM 0.12"},
      {-1,NULL}
    };


/****************************************************************************
 Send a session setup. The username is in UNIX character format and must be
 converted to DOS codepage format before sending. If the password is in
 plaintext, the same should be done.
****************************************************************************/

BOOL cli_session_setup(struct cli_state *cli, 
		       char *user, 
		       char *pass, int passlen,
		       char *ntpass, int ntpasslen,
		       char *workgroup)
{
	char *p;
	fstring pword, ntpword;

	if (cli->protocol < PROTOCOL_LANMAN1)
		return True;

	if (passlen > sizeof(pword)-1 || ntpasslen > sizeof(ntpword)-1) {
		return False;
	}

	if (((passlen == 0) || (passlen == 1)) && (pass[0] == '\0')) {
		/* Null session connect. */
		pword[0] = '\0';
		ntpword[0] = '\0';
	} else {
		if ((cli->sec_mode & 2) && passlen != 24) {
			/*
			 * Encrypted mode needed, and non encrypted password supplied.
			 */
			passlen = 24;
			ntpasslen = 24;
			fstrcpy(pword, pass);
			unix_to_dos(pword,True);
			fstrcpy(ntpword, ntpass);;
			unix_to_dos(ntpword,True);
			SMBencrypt((uchar *)pword,(uchar *)cli->cryptkey,(uchar *)pword);
			SMBNTencrypt((uchar *)ntpword,(uchar *)cli->cryptkey,(uchar *)ntpword);
		} else if ((cli->sec_mode & 2) && passlen == 24) {
			/*
			 * Encrypted mode needed, and encrypted password supplied.
			 */
			memcpy(pword, pass, passlen);
			if(ntpasslen == 24) {
				memcpy(ntpword, ntpass, ntpasslen);
			} else {
				fstrcpy(ntpword, "");
				ntpasslen = 0;
			}
		} else {
			/*
			 * Plaintext mode needed, assume plaintext supplied.
			 */
			fstrcpy(pword, pass);
			unix_to_dos(pword,True);
			fstrcpy(ntpword, "");
			ntpasslen = 0;
		}
	}

	/* if in share level security then don't send a password now */
	if (!(cli->sec_mode & 1)) {
		fstrcpy(pword, "");
		passlen=1;
		fstrcpy(ntpword, "");
		ntpasslen=1;
	} 

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);

	if (cli->protocol < PROTOCOL_NT1)
	{
		set_message(cli->outbuf,10,1 + strlen(user) + passlen,True);
		CVAL(cli->outbuf,smb_com) = SMBsesssetupX;
		cli_setup_packet(cli);

		CVAL(cli->outbuf,smb_vwv0) = 0xFF;
		SSVAL(cli->outbuf,smb_vwv2,cli->max_xmit);
		SSVAL(cli->outbuf,smb_vwv3,2);
		SSVAL(cli->outbuf,smb_vwv4,1);
		SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
		SSVAL(cli->outbuf,smb_vwv7,passlen);
		p = smb_buf(cli->outbuf);
		memcpy(p,pword,passlen);
		p += passlen;
		pstrcpy(p,user);
		unix_to_dos(p,True);
		strupper(p);
	}
	else
	{
		set_message(cli->outbuf,13,0,True);
		CVAL(cli->outbuf,smb_com) = SMBsesssetupX;
		cli_setup_packet(cli);
		
		CVAL(cli->outbuf,smb_vwv0) = 0xFF;
		SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
		SSVAL(cli->outbuf,smb_vwv3,2);
		SSVAL(cli->outbuf,smb_vwv4,cli->pid);
		SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
		SSVAL(cli->outbuf,smb_vwv7,passlen);
		SSVAL(cli->outbuf,smb_vwv8,ntpasslen);
		SSVAL(cli->outbuf,smb_vwv11,0);
		p = smb_buf(cli->outbuf);
		memcpy(p,pword,passlen); 
		p += SVAL(cli->outbuf,smb_vwv7);
		memcpy(p,ntpword,ntpasslen); 
		p += SVAL(cli->outbuf,smb_vwv8);
		pstrcpy(p,user);
		unix_to_dos(p,True);
		strupper(p);
		p = skip_string(p,1);
		pstrcpy(p,workgroup);
		strupper(p);
		p = skip_string(p,1);
		pstrcpy(p,"Unix");p = skip_string(p,1);
		pstrcpy(p,"Samba");p = skip_string(p,1);
		set_message(cli->outbuf,13,PTR_DIFF(p,smb_buf(cli->outbuf)),False);
	}

      cli_send_smb(cli);
      if (!cli_receive_smb(cli))
	      return False;

      show_msg(cli->inbuf);

      if (CVAL(cli->inbuf,smb_rcls) != 0) {
	      return False;
      }

      /* use the returned vuid from now on */
      cli->vuid = SVAL(cli->inbuf,smb_uid);

      if (cli->protocol >= PROTOCOL_NT1) {
        /*
         * Save off some of the connected server
         * info.
         */
        char *server_domain,*server_os,*server_type;
        server_os = smb_buf(cli->inbuf);
        server_type = skip_string(server_os,1);
        server_domain = skip_string(server_type,1);
        fstrcpy(cli->server_os, server_os);
		dos_to_unix(cli->server_os, True);
        fstrcpy(cli->server_type, server_type);
		dos_to_unix(cli->server_type, True);
        fstrcpy(cli->server_domain, server_domain);
		dos_to_unix(cli->server_domain, True);
      }

      fstrcpy(cli->user_name, user);
      dos_to_unix(cli->user_name, True);

      return True;
}

/****************************************************************************
 Send a uloggoff.
*****************************************************************************/

BOOL cli_ulogoff(struct cli_state *cli)
{
        memset(cli->outbuf,'\0',smb_size);
        set_message(cli->outbuf,2,0,True);
        CVAL(cli->outbuf,smb_com) = SMBulogoffX;
        cli_setup_packet(cli);
	SSVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,0);  /* no additional info */

        cli_send_smb(cli);
        if (!cli_receive_smb(cli))
                return False;

        return CVAL(cli->inbuf,smb_rcls) == 0;
}

/****************************************************************************
send a tconX
****************************************************************************/
BOOL cli_send_tconX(struct cli_state *cli, 
		    char *share, char *dev, char *pass, int passlen)
{
	fstring fullshare, pword, dos_pword;
	char *p;
	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	fstrcpy(cli->share, share);

	/* in user level security don't send a password now */
	if (cli->sec_mode & 1) {
		passlen = 1;
		pass = "";
	}

	if ((cli->sec_mode & 2) && *pass && passlen != 24) {
		/*
		 * Non-encrypted passwords - convert to DOS codepage before encryption.
		 */
		passlen = 24;
		fstrcpy(dos_pword,pass);
		unix_to_dos(dos_pword,True);
		SMBencrypt((uchar *)dos_pword,(uchar *)cli->cryptkey,(uchar *)pword);
	} else {
		if(!(cli->sec_mode & 2)) {
			/*
			 * Non-encrypted passwords - convert to DOS codepage before using.
			 */
			fstrcpy(pword,pass);
			unix_to_dos(pword,True);
		} else {
			memcpy(pword, pass, passlen);
		}
	}

	slprintf(fullshare, sizeof(fullshare)-1,
		 "\\\\%s\\%s", cli->desthost, share);
	unix_to_dos(fullshare, True);
	strupper(fullshare);

	set_message(cli->outbuf,4,
		    2 + strlen(fullshare) + passlen + strlen(dev),True);
	CVAL(cli->outbuf,smb_com) = SMBtconX;
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv3,passlen);

	p = smb_buf(cli->outbuf);
	memcpy(p,pword,passlen);
	p += passlen;
	fstrcpy(p,fullshare);
	p = skip_string(p,1);
	pstrcpy(p,dev);
	unix_to_dos(p,True);

	SCVAL(cli->inbuf,smb_rcls, 1);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli))
		return False;

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	fstrcpy(cli->dev, "A:");

	if (cli->protocol >= PROTOCOL_NT1) {
		fstrcpy(cli->dev, smb_buf(cli->inbuf));
	}

	if (strcasecmp(share,"IPC$")==0) {
		fstrcpy(cli->dev, "IPC");
	}

	/* only grab the device if we have a recent protocol level */
	if (cli->protocol >= PROTOCOL_NT1 &&
	    smb_buflen(cli->inbuf) == 3) {
		/* almost certainly win95 - enable bug fixes */
		cli->win95 = True;
	}

	cli->cnum = SVAL(cli->inbuf,smb_tid);
	return True;
}


/****************************************************************************
send a tree disconnect
****************************************************************************/
BOOL cli_tdis(struct cli_state *cli)
{
	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,0,0,True);
	CVAL(cli->outbuf,smb_com) = SMBtdis;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);
	
	cli_send_smb(cli);
	if (!cli_receive_smb(cli))
		return False;
	
	return CVAL(cli->inbuf,smb_rcls) == 0;
}

/****************************************************************************
rename a file
****************************************************************************/
BOOL cli_rename(struct cli_state *cli, char *fname_src, char *fname_dst)
{
        char *p;

        memset(cli->outbuf,'\0',smb_size);
        memset(cli->inbuf,'\0',smb_size);

        set_message(cli->outbuf,1, 4 + strlen(fname_src) + strlen(fname_dst), True);

        CVAL(cli->outbuf,smb_com) = SMBmv;
        SSVAL(cli->outbuf,smb_tid,cli->cnum);
        cli_setup_packet(cli);

        SSVAL(cli->outbuf,smb_vwv0,aSYSTEM | aHIDDEN | aDIR);

        p = smb_buf(cli->outbuf);
        *p++ = 4;
        pstrcpy(p,fname_src);
        unix_to_dos(p,True);
        p = skip_string(p,1);
        *p++ = 4;
        pstrcpy(p,fname_dst);
        unix_to_dos(p,True);

        cli_send_smb(cli);
        if (!cli_receive_smb(cli)) {
                return False;
        }

        if (CVAL(cli->inbuf,smb_rcls) != 0) {
                return False;
        }

        return True;
}

/****************************************************************************
delete a file
****************************************************************************/
BOOL cli_unlink(struct cli_state *cli, char *fname)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,1, 2 + strlen(fname),True);

	CVAL(cli->outbuf,smb_com) = SMBunlink;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,aSYSTEM | aHIDDEN);
  
	p = smb_buf(cli->outbuf);
	*p++ = 4;      
	pstrcpy(p,fname);
    unix_to_dos(p,True);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}

/****************************************************************************
create a directory
****************************************************************************/
BOOL cli_mkdir(struct cli_state *cli, char *dname)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,0, 2 + strlen(dname),True);

	CVAL(cli->outbuf,smb_com) = SMBmkdir;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	p = smb_buf(cli->outbuf);
	*p++ = 4;      
	pstrcpy(p,dname);
    unix_to_dos(p,True);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}

/****************************************************************************
remove a directory
****************************************************************************/
BOOL cli_rmdir(struct cli_state *cli, char *dname)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,0, 2 + strlen(dname),True);

	CVAL(cli->outbuf,smb_com) = SMBrmdir;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	p = smb_buf(cli->outbuf);
	*p++ = 4;      
	pstrcpy(p,dname);
    unix_to_dos(p,True);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}



/****************************************************************************
open a file
****************************************************************************/
int cli_nt_create(struct cli_state *cli, char *fname)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,24,1 + strlen(fname),True);

	CVAL(cli->outbuf,smb_com) = SMBntcreateX;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,0xFF);
	SIVAL(cli->outbuf,smb_ntcreate_Flags, 0x06);
	SIVAL(cli->outbuf,smb_ntcreate_RootDirectoryFid, 0x0);
	SIVAL(cli->outbuf,smb_ntcreate_DesiredAccess, 0x2019f);
	SIVAL(cli->outbuf,smb_ntcreate_FileAttributes, 0x0);
	SIVAL(cli->outbuf,smb_ntcreate_ShareAccess, 0x03);
	SIVAL(cli->outbuf,smb_ntcreate_CreateDisposition, 0x01);
	SIVAL(cli->outbuf,smb_ntcreate_CreateOptions, 0x0);
	SIVAL(cli->outbuf,smb_ntcreate_ImpersonationLevel, 0x02);
	SSVAL(cli->outbuf,smb_ntcreate_NameLength, strlen(fname));

	p = smb_buf(cli->outbuf);
	pstrcpy(p,fname);
    unix_to_dos(p,True);
	p = skip_string(p,1);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return -1;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return -1;
	}

	return SVAL(cli->inbuf,smb_vwv2 + 1);
}


/****************************************************************************
open a file
WARNING: if you open with O_WRONLY then getattrE won't work!
****************************************************************************/
int cli_open(struct cli_state *cli, char *fname, int flags, int share_mode)
{
	char *p;
	unsigned openfn=0;
	unsigned accessmode=0;

	if (flags & O_CREAT)
		openfn |= (1<<4);
	if (!(flags & O_EXCL)) {
		if (flags & O_TRUNC)
			openfn |= (1<<1);
		else
			openfn |= (1<<0);
	}

	accessmode = (share_mode<<4);

	if ((flags & O_ACCMODE) == O_RDWR) {
		accessmode |= 2;
	} else if ((flags & O_ACCMODE) == O_WRONLY) {
		accessmode |= 1;
	} 

#if defined(O_SYNC)
	if ((flags & O_SYNC) == O_SYNC) {
		accessmode |= (1<<14);
	}
#endif /* O_SYNC */

	if (share_mode == DENY_FCB) {
		accessmode = 0xFF;
	}

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,15,1 + strlen(fname),True);

	CVAL(cli->outbuf,smb_com) = SMBopenX;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,0);  /* no additional info */
	SSVAL(cli->outbuf,smb_vwv3,accessmode);
	SSVAL(cli->outbuf,smb_vwv4,aSYSTEM | aHIDDEN);
	SSVAL(cli->outbuf,smb_vwv5,0);
	SSVAL(cli->outbuf,smb_vwv8,openfn);

	if (cli->use_oplocks) {
		/* if using oplocks then ask for a batch oplock via
                   core and extended methods */
		CVAL(cli->outbuf,smb_flg) |= 
			FLAG_REQUEST_OPLOCK|FLAG_REQUEST_BATCH_OPLOCK;
		SSVAL(cli->outbuf,smb_vwv2,SVAL(cli->outbuf,smb_vwv2) | 6);
	}
  
	p = smb_buf(cli->outbuf);
	pstrcpy(p,fname);
	unix_to_dos(p,True);
	p = skip_string(p,1);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return -1;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return -1;
	}

	return SVAL(cli->inbuf,smb_vwv2);
}




/****************************************************************************
  close a file
****************************************************************************/
BOOL cli_close(struct cli_state *cli, int fnum)
{
	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,3,0,True);

	CVAL(cli->outbuf,smb_com) = SMBclose;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,fnum);
	SIVALS(cli->outbuf,smb_vwv1,-1);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}


/****************************************************************************
  lock a file
****************************************************************************/
BOOL cli_lock(struct cli_state *cli, int fnum, 
	      uint32 offset, uint32 len, int timeout, enum brl_type lock_type)
{
	char *p;
        int saved_timeout = cli->timeout;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0', smb_size);

	set_message(cli->outbuf,8,10,True);

	CVAL(cli->outbuf,smb_com) = SMBlockingX;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	CVAL(cli->outbuf,smb_vwv0) = 0xFF;
	SSVAL(cli->outbuf,smb_vwv2,fnum);
	CVAL(cli->outbuf,smb_vwv3) = (lock_type == READ_LOCK? 1 : 0);
	SIVALS(cli->outbuf, smb_vwv4, timeout);
	SSVAL(cli->outbuf,smb_vwv6,0);
	SSVAL(cli->outbuf,smb_vwv7,1);

	p = smb_buf(cli->outbuf);
	SSVAL(p, 0, cli->pid);
	SIVAL(p, 2, offset);
	SIVAL(p, 6, len);
	cli_send_smb(cli);

        cli->timeout = (timeout == -1) ? 0x7FFFFFFF : (timeout + 2*1000);

	if (!cli_receive_smb(cli)) {
                cli->timeout = saved_timeout;
		return False;
	}

	cli->timeout = saved_timeout;

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}

/****************************************************************************
  unlock a file
****************************************************************************/
BOOL cli_unlock(struct cli_state *cli, int fnum, uint32 offset, uint32 len)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,8,10,True);

	CVAL(cli->outbuf,smb_com) = SMBlockingX;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	CVAL(cli->outbuf,smb_vwv0) = 0xFF;
	SSVAL(cli->outbuf,smb_vwv2,fnum);
	CVAL(cli->outbuf,smb_vwv3) = 0;
	SIVALS(cli->outbuf, smb_vwv4, 0);
	SSVAL(cli->outbuf,smb_vwv6,1);
	SSVAL(cli->outbuf,smb_vwv7,0);

	p = smb_buf(cli->outbuf);
	SSVAL(p, 0, cli->pid);
	SIVAL(p, 2, offset);
	SIVAL(p, 6, len);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}



/****************************************************************************
issue a single SMBread and don't wait for a reply
****************************************************************************/
static void cli_issue_read(struct cli_state *cli, int fnum, off_t offset, 
			   size_t size, int i)
{
	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,10,0,True);
		
	CVAL(cli->outbuf,smb_com) = SMBreadX;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	CVAL(cli->outbuf,smb_vwv0) = 0xFF;
	SSVAL(cli->outbuf,smb_vwv2,fnum);
	SIVAL(cli->outbuf,smb_vwv3,offset);
	SSVAL(cli->outbuf,smb_vwv5,size);
	SSVAL(cli->outbuf,smb_vwv6,size);
	SSVAL(cli->outbuf,smb_mid,cli->mid + i);

	cli_send_smb(cli);
}

/****************************************************************************
  read from a file
****************************************************************************/
size_t cli_read(struct cli_state *cli, int fnum, char *buf, off_t offset, size_t size)
{
	char *p;
	int total = -1;
	int issued=0;
	int received=0;
/*
 * There is a problem in this code when mpx is more than one.
 * for some reason files can get corrupted when being read.
 * Until we understand this fully I am serializing reads (one
 * read/one reply) for now. JRA.
 */
#if 0
	int mpx = MAX(cli->max_mux-1, 1); 
#else
	int mpx = 1;
#endif
	int block = (cli->max_xmit - (smb_size+32)) & ~1023;
	int mid;
	int blocks = (size + (block-1)) / block;

	if (size == 0) return 0;

	while (received < blocks) {
		int size2;

		while (issued - received < mpx && issued < blocks) {
			int size1 = MIN(block, size-issued*block);
			cli_issue_read(cli, fnum, offset+issued*block, size1, issued);
			issued++;
		}

		if (!cli_receive_smb(cli)) {
			return total;
		}

		received++;
		mid = SVAL(cli->inbuf, smb_mid) - cli->mid;
		size2 = SVAL(cli->inbuf, smb_vwv5);

		if (CVAL(cli->inbuf,smb_rcls) != 0) {
			blocks = MIN(blocks, mid-1);
			continue;
		}

		if (size2 <= 0) {
			blocks = MIN(blocks, mid-1);
			/* this distinguishes EOF from an error */
			total = MAX(total, 0);
			continue;
		}

		if (size2 > block) {
			DEBUG(0,("server returned more than we wanted!\n"));
			exit(1);
		}
		if (mid >= issued) {
			DEBUG(0,("invalid mid from server!\n"));
			exit(1);
		}
		p = smb_base(cli->inbuf) + SVAL(cli->inbuf,smb_vwv6);

		memcpy(buf+mid*block, p, size2);

		total = MAX(total, mid*block + size2);
	}

	while (received < issued) {
		cli_receive_smb(cli);
		received++;
	}
	
	return total;
}


/****************************************************************************
issue a single SMBwrite and don't wait for a reply
****************************************************************************/
static void cli_issue_write(struct cli_state *cli, int fnum, off_t offset, uint16 mode, char *buf,
			    size_t size, int i)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,12,size,True);
	
	CVAL(cli->outbuf,smb_com) = SMBwriteX;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);
	
	CVAL(cli->outbuf,smb_vwv0) = 0xFF;
	SSVAL(cli->outbuf,smb_vwv2,fnum);

	SIVAL(cli->outbuf,smb_vwv3,offset);
	SIVAL(cli->outbuf,smb_vwv5,IS_BITS_SET_ALL(mode, 0x0008) ? 0xFFFFFFFF : 0);
	SSVAL(cli->outbuf,smb_vwv7,mode);

	SSVAL(cli->outbuf,smb_vwv8,IS_BITS_SET_ALL(mode, 0x0008) ? size : 0);
	SSVAL(cli->outbuf,smb_vwv10,size);
	SSVAL(cli->outbuf,smb_vwv11,
	      smb_buf(cli->outbuf) - smb_base(cli->outbuf));
	
	p = smb_base(cli->outbuf) + SVAL(cli->outbuf,smb_vwv11);
	memcpy(p, buf, size);

	SSVAL(cli->outbuf,smb_mid,cli->mid + i);
	
	show_msg(cli->outbuf);
	cli_send_smb(cli);
}

/****************************************************************************
  write to a file
  write_mode: 0x0001 disallow write cacheing
              0x0002 return bytes remaining
              0x0004 use raw named pipe protocol
              0x0008 start of message mode named pipe protocol
****************************************************************************/
ssize_t cli_write(struct cli_state *cli,
		  int fnum, uint16 write_mode,
		  char *buf, off_t offset, size_t size)
{
	int bwritten = 0;
	int issued = 0;
	int received = 0;
	int mpx = MAX(cli->max_mux-1, 1);
	int block = (cli->max_xmit - (smb_size+32)) & ~1023;
	int blocks = (size + (block-1)) / block;

	while (received < blocks) {

		while ((issued - received < mpx) && (issued < blocks))
		{
			int bsent = issued * block;
			int size1 = MIN(block, size - bsent);

			cli_issue_write(cli, fnum, offset + bsent,
			                write_mode,
			                buf + bsent,
					size1, issued);
			issued++;
		}

		if (!cli_receive_smb(cli))
		{
			return bwritten;
		}

		received++;

		if (CVAL(cli->inbuf,smb_rcls) != 0)
		{
			break;
		}

		bwritten += SVAL(cli->inbuf, smb_vwv2);
	}

	while (received < issued && cli_receive_smb(cli))
	{
		received++;
	}
	
	return bwritten;
}


/****************************************************************************
  write to a file using a SMBwrite and not bypassing 0 byte writes
****************************************************************************/
ssize_t cli_smbwrite(struct cli_state *cli,
		     int fnum, char *buf, off_t offset, size_t size1)
{
	char *p;
	ssize_t total = 0;

	do {
		size_t size = MIN(size1, cli->max_xmit - 48);
		
		memset(cli->outbuf,'\0',smb_size);
		memset(cli->inbuf,'\0',smb_size);

		set_message(cli->outbuf,5, 3 + size,True);

		CVAL(cli->outbuf,smb_com) = SMBwrite;
		SSVAL(cli->outbuf,smb_tid,cli->cnum);
		cli_setup_packet(cli);
		
		SSVAL(cli->outbuf,smb_vwv0,fnum);
		SSVAL(cli->outbuf,smb_vwv1,size);
		SIVAL(cli->outbuf,smb_vwv2,offset);
		SSVAL(cli->outbuf,smb_vwv4,0);
		
		p = smb_buf(cli->outbuf);
		*p++ = 1;
		SSVAL(p, 0, size);
		memcpy(p+2, buf, size);
		
		cli_send_smb(cli);
		if (!cli_receive_smb(cli)) {
			return -1;
		}
		
		if (CVAL(cli->inbuf,smb_rcls) != 0) {
			return -1;
		}

		size = SVAL(cli->inbuf,smb_vwv0);
		if (size == 0) break;

		size1 -= size;
		total += size;
	} while (size1);

	return total;
}


/****************************************************************************
do a SMBgetattrE call
****************************************************************************/
BOOL cli_getattrE(struct cli_state *cli, int fd, 
		  uint16 *attr, size_t *size, 
		  time_t *c_time, time_t *a_time, time_t *m_time)
{
	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,1,0,True);

	CVAL(cli->outbuf,smb_com) = SMBgetattrE;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,fd);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}
	
	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	if (size) {
		*size = IVAL(cli->inbuf, smb_vwv6);
	}

	if (attr) {
		*attr = SVAL(cli->inbuf,smb_vwv10);
	}

	if (c_time) {
		*c_time = make_unix_date3(cli->inbuf+smb_vwv0);
	}

	if (a_time) {
		*a_time = make_unix_date3(cli->inbuf+smb_vwv2);
	}

	if (m_time) {
		*m_time = make_unix_date3(cli->inbuf+smb_vwv4);
	}

	return True;
}


/****************************************************************************
do a SMBgetatr call
****************************************************************************/
BOOL cli_getatr(struct cli_state *cli, char *fname, 
		uint16 *attr, size_t *size, time_t *t)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,0,strlen(fname)+2,True);

	CVAL(cli->outbuf,smb_com) = SMBgetatr;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	p = smb_buf(cli->outbuf);
	*p = 4;
	pstrcpy(p+1, fname);
    unix_to_dos(p+1,True);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}
	
	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	if (size) {
		*size = IVAL(cli->inbuf, smb_vwv3);
	}

	if (t) {
		*t = make_unix_date3(cli->inbuf+smb_vwv1);
	}

	if (attr) {
		*attr = SVAL(cli->inbuf,smb_vwv0);
	}


	return True;
}


/****************************************************************************
do a SMBsetatr call
****************************************************************************/
BOOL cli_setatr(struct cli_state *cli, char *fname, uint16 attr, time_t t)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	memset(cli->inbuf,'\0',smb_size);

	set_message(cli->outbuf,8,strlen(fname)+4,True);

	CVAL(cli->outbuf,smb_com) = SMBsetatr;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0, attr);
	put_dos_date3(cli->outbuf,smb_vwv1, t);

	p = smb_buf(cli->outbuf);
	*p = 4;
	pstrcpy(p+1, fname);
    unix_to_dos(p+1,True);
	p = skip_string(p,1);
	*p = 4;

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}
	
	if (CVAL(cli->inbuf,smb_rcls) != 0) {
		return False;
	}

	return True;
}

/****************************************************************************
send a qpathinfo call
****************************************************************************/
BOOL cli_qpathinfo(struct cli_state *cli, const char *fname, 
		   time_t *c_time, time_t *a_time, time_t *m_time, 
		   size_t *size, uint16 *mode)
{
	int data_len = 0;
	int param_len = 0;
	uint16 setup = TRANSACT2_QPATHINFO;
	pstring param;
	char *rparam=NULL, *rdata=NULL;
	int count=8;
	BOOL ret;
	time_t (*date_fn)(void *);

	param_len = strlen(fname) + 7;

	memset(param, 0, param_len);
	SSVAL(param, 0, SMB_INFO_STANDARD);
	pstrcpy(&param[6], fname);
    unix_to_dos(&param[6],True);

	do {
		ret = (cli_send_trans(cli, SMBtrans2, 
				      NULL, 0,        /* Name, length */
				      -1, 0,          /* fid, flags */
				      &setup, 1, 0,   /* setup, length, max */
				      param, param_len, 10, /* param, length, max */
				      NULL, data_len, cli->max_xmit /* data, length, max */
				      ) &&
		       cli_receive_trans(cli, SMBtrans2, 
					 &rparam, &param_len,
					 &rdata, &data_len));
		if (!ret) {
			/* we need to work around a Win95 bug - sometimes
			   it gives ERRSRV/ERRerror temprarily */
			uint8 eclass;
			uint32 ecode;
			cli_error(cli, &eclass, &ecode, NULL);
			if (eclass != ERRSRV || ecode != ERRerror) break;
			msleep(100);
		}
	} while (count-- && ret==False);

	if (!ret || !rdata || data_len < 22) {
		return False;
	}

	if (cli->win95) {
		date_fn = make_unix_date;
	} else {
		date_fn = make_unix_date2;
	}

	if (c_time) {
		*c_time = date_fn(rdata+0);
	}
	if (a_time) {
		*a_time = date_fn(rdata+4);
	}
	if (m_time) {
		*m_time = date_fn(rdata+8);
	}
	if (size) {
		*size = IVAL(rdata, 12);
	}
	if (mode) {
		*mode = SVAL(rdata,l1_attrFile);
	}

	if (rdata) free(rdata);
	if (rparam) free(rparam);
	return True;
}

/****************************************************************************
send a qpathinfo call with the SMB_QUERY_FILE_ALL_INFO info level
****************************************************************************/
BOOL cli_qpathinfo2(struct cli_state *cli, const char *fname, 
		    time_t *c_time, time_t *a_time, time_t *m_time, 
		    time_t *w_time, size_t *size, uint16 *mode,
		    SMB_INO_T *ino)
{
	int data_len = 0;
	int param_len = 0;
	uint16 setup = TRANSACT2_QPATHINFO;
	pstring param;
	char *rparam=NULL, *rdata=NULL;

	param_len = strlen(fname) + 7;

	memset(param, 0, param_len);
	SSVAL(param, 0, SMB_QUERY_FILE_ALL_INFO);
	pstrcpy(&param[6], fname);
    unix_to_dos(&param[6],True);

	if (!cli_send_trans(cli, SMBtrans2, 
                            NULL, 0,                      /* name, length */
                            -1, 0,                        /* fid, flags */
                            &setup, 1, 0,                 /* setup, length, max */
                            param, param_len, 10,         /* param, length, max */
                            NULL, data_len, cli->max_xmit /* data, length, max */
                           )) {
		return False;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
                               &rparam, &param_len,
                               &rdata, &data_len)) {
		return False;
	}

	if (!rdata || data_len < 22) {
		return False;
	}

	if (c_time) {
		*c_time = interpret_long_date(rdata+0) - cli->serverzone;
	}
	if (a_time) {
		*a_time = interpret_long_date(rdata+8) - cli->serverzone;
	}
	if (m_time) {
		*m_time = interpret_long_date(rdata+16) - cli->serverzone;
	}
	if (w_time) {
		*w_time = interpret_long_date(rdata+24) - cli->serverzone;
	}
	if (mode) {
		*mode = SVAL(rdata, 32);
	}
	if (size) {
		*size = IVAL(rdata, 48);
	}
	if (ino) {
		*ino = IVAL(rdata, 64);
	}

	if (rdata) free(rdata);
	if (rparam) free(rparam);
	return True;
}


/****************************************************************************
send a qfileinfo call
****************************************************************************/
BOOL cli_qfileinfo(struct cli_state *cli, int fnum, 
		   uint16 *mode, size_t *size,
		   time_t *c_time, time_t *a_time, time_t *m_time, 
		   time_t *w_time, SMB_INO_T *ino)
{
	int data_len = 0;
	int param_len = 0;
	uint16 setup = TRANSACT2_QFILEINFO;
	pstring param;
	char *rparam=NULL, *rdata=NULL;

	/* if its a win95 server then fail this - win95 totally screws it
	   up */
	if (cli->win95) return False;

	param_len = 4;

	memset(param, 0, param_len);
	SSVAL(param, 0, fnum);
	SSVAL(param, 2, SMB_QUERY_FILE_ALL_INFO);

	if (!cli_send_trans(cli, SMBtrans2, 
                            NULL, 0,                        /* name, length */
                            -1, 0,                          /* fid, flags */
                            &setup, 1, 0,                   /* setup, length, max */
                            param, param_len, 2,            /* param, length, max */
                            NULL, data_len, cli->max_xmit   /* data, length, max */
                           )) {
		return False;
	}

	if (!cli_receive_trans(cli, SMBtrans2,
                               &rparam, &param_len,
                               &rdata, &data_len)) {
		return False;
	}

	if (!rdata || data_len < 68) {
		return False;
	}

	if (c_time) {
		*c_time = interpret_long_date(rdata+0) - cli->serverzone;
	}
	if (a_time) {
		*a_time = interpret_long_date(rdata+8) - cli->serverzone;
	}
	if (m_time) {
		*m_time = interpret_long_date(rdata+16) - cli->serverzone;
	}
	if (w_time) {
		*w_time = interpret_long_date(rdata+24) - cli->serverzone;
	}
	if (mode) {
		*mode = SVAL(rdata, 32);
	}
	if (size) {
		*size = IVAL(rdata, 48);
	}
	if (ino) {
		*ino = IVAL(rdata, 64);
	}

	if (rdata) free(rdata);
	if (rparam) free(rparam);
	return True;
}


/****************************************************************************
interpret a long filename structure - this is mostly guesses at the moment
The length of the structure is returned
The structure of a long filename depends on the info level. 260 is used
by NT and 2 is used by OS/2
****************************************************************************/
static int interpret_long_filename(int level,char *p,file_info *finfo)
{
	extern file_info def_finfo;

	if (finfo)
		memcpy(finfo,&def_finfo,sizeof(*finfo));

	switch (level)
		{
		case 1: /* OS/2 understands this */
			if (finfo) {
				/* these dates are converted to GMT by make_unix_date */
				finfo->ctime = make_unix_date2(p+4);
				finfo->atime = make_unix_date2(p+8);
				finfo->mtime = make_unix_date2(p+12);
				finfo->size = IVAL(p,16);
				finfo->mode = CVAL(p,24);
				pstrcpy(finfo->name,p+27);
				dos_to_unix(finfo->name,True);
			}
			return(28 + CVAL(p,26));

		case 2: /* this is what OS/2 uses mostly */
			if (finfo) {
				/* these dates are converted to GMT by make_unix_date */
				finfo->ctime = make_unix_date2(p+4);
				finfo->atime = make_unix_date2(p+8);
				finfo->mtime = make_unix_date2(p+12);
				finfo->size = IVAL(p,16);
				finfo->mode = CVAL(p,24);
				pstrcpy(finfo->name,p+31);
				dos_to_unix(finfo->name,True);
			}
			return(32 + CVAL(p,30));

			/* levels 3 and 4 are untested */
		case 3:
			if (finfo) {
				/* these dates are probably like the other ones */
				finfo->ctime = make_unix_date2(p+8);
				finfo->atime = make_unix_date2(p+12);
				finfo->mtime = make_unix_date2(p+16);
				finfo->size = IVAL(p,20);
				finfo->mode = CVAL(p,28);
				pstrcpy(finfo->name,p+33);
				dos_to_unix(finfo->name,True);
			}
			return(SVAL(p,4)+4);
			
		case 4:
			if (finfo) {
				/* these dates are probably like the other ones */
				finfo->ctime = make_unix_date2(p+8);
				finfo->atime = make_unix_date2(p+12);
				finfo->mtime = make_unix_date2(p+16);
				finfo->size = IVAL(p,20);
				finfo->mode = CVAL(p,28);
				pstrcpy(finfo->name,p+37);
				dos_to_unix(finfo->name,True);
			}
			return(SVAL(p,4)+4);
			
		case 260: /* NT uses this, but also accepts 2 */
			if (finfo) {
				int ret = SVAL(p,0);
				int namelen;
				p += 4; /* next entry offset */
				p += 4; /* fileindex */
				
				/* these dates appear to arrive in a
				   weird way. It seems to be localtime
				   plus the serverzone given in the
				   initial connect. This is GMT when
				   DST is not in effect and one hour
				   from GMT otherwise. Can this really
				   be right??

				   I suppose this could be called
				   kludge-GMT. Is is the GMT you get
				   by using the current DST setting on
				   a different localtime. It will be
				   cheap to calculate, I suppose, as
				   no DST tables will be needed */

				finfo->ctime = interpret_long_date(p); p += 8;
				finfo->atime = interpret_long_date(p); p += 8;
				finfo->mtime = interpret_long_date(p); p += 8; p += 8;
				finfo->size = IVAL(p,0); p += 8;
				p += 8; /* alloc size */
				finfo->mode = CVAL(p,0); p += 4;
				namelen = IVAL(p,0); p += 4;
				p += 4; /* EA size */
				p += 2; /* short name len? */
				p += 24; /* short name? */	  
				StrnCpy(finfo->name,p,MIN(sizeof(finfo->name)-1,namelen));
				dos_to_unix(finfo->name,True);
				return(ret);
			}
			return(SVAL(p,0));
		}
	
	DEBUG(1,("Unknown long filename format %d\n",level));
	return(SVAL(p,0));
}


/****************************************************************************
  do a directory listing, calling fn on each file found
  ****************************************************************************/
int cli_list(struct cli_state *cli,const char *Mask,uint16 attribute, 
	     void (*fn)(file_info *, const char *))
{
	int max_matches = 512;
	/* NT uses 260, OS/2 uses 2. Both accept 1. */
	int info_level = cli->protocol<PROTOCOL_NT1?1:260; 
	char *p, *p2;
	pstring mask;
	file_info finfo;
	int i;
	char *dirlist = NULL;
	int dirlist_len = 0;
	int total_received = -1;
	BOOL First = True;
	int ff_searchcount=0;
	int ff_eos=0;
	int ff_lastname=0;
	int ff_dir_handle=0;
	int loop_count = 0;
	char *rparam=NULL, *rdata=NULL;
	int param_len, data_len;
	
	uint16 setup;
	pstring param;
	
	pstrcpy(mask,Mask);
	unix_to_dos(mask,True);
	
	while (ff_eos == 0) {
		loop_count++;
		if (loop_count > 200) {
			DEBUG(0,("Error: Looping in FIND_NEXT??\n"));
			break;
		}

		param_len = 12+strlen(mask)+1;

		if (First) {
			setup = TRANSACT2_FINDFIRST;
			SSVAL(param,0,attribute); /* attribute */
			SSVAL(param,2,max_matches); /* max count */
			SSVAL(param,4,4+2);	/* resume required + close on end */
			SSVAL(param,6,info_level); 
			SIVAL(param,8,0);
			pstrcpy(param+12,mask);
		} else {
			setup = TRANSACT2_FINDNEXT;
			SSVAL(param,0,ff_dir_handle);
			SSVAL(param,2,max_matches); /* max count */
			SSVAL(param,4,info_level); 
			SIVAL(param,6,0); /* ff_resume_key */
			SSVAL(param,10,8+4+2);	/* continue + resume required + close on end */
			pstrcpy(param+12,mask);

			DEBUG(5,("hand=0x%X ff_lastname=%d mask=%s\n",
				 ff_dir_handle,ff_lastname,mask));
		}

		if (!cli_send_trans(cli, SMBtrans2, 
				    NULL, 0,                /* Name, length */
				    -1, 0,                  /* fid, flags */
				    &setup, 1, 0,           /* setup, length, max */
				    param, param_len, 10,   /* param, length, max */
				    NULL, 0, 
				    cli->max_xmit /* data, length, max */
				    )) {
			break;
		}

		if (!cli_receive_trans(cli, SMBtrans2, 
				       &rparam, &param_len,
				       &rdata, &data_len)) {
			/* we need to work around a Win95 bug - sometimes
			   it gives ERRSRV/ERRerror temprarily */
			uint8 eclass;
			uint32 ecode;
			cli_error(cli, &eclass, &ecode, NULL);
			if (eclass != ERRSRV || ecode != ERRerror) break;
			msleep(100);
			continue;
		}

		if (total_received == -1) total_received = 0;

		/* parse out some important return info */
		p = rparam;
		if (First) {
			ff_dir_handle = SVAL(p,0);
			ff_searchcount = SVAL(p,2);
			ff_eos = SVAL(p,4);
			ff_lastname = SVAL(p,8);
		} else {
			ff_searchcount = SVAL(p,0);
			ff_eos = SVAL(p,2);
			ff_lastname = SVAL(p,6);
		}

		if (ff_searchcount == 0) 
			break;

		/* point to the data bytes */
		p = rdata;

		/* we might need the lastname for continuations */
		if (ff_lastname > 0) {
			switch(info_level)
				{
				case 260:
					StrnCpy(mask,p+ff_lastname,
						MIN(sizeof(mask)-1,data_len-ff_lastname));
					break;
				case 1:
					pstrcpy(mask,p + ff_lastname + 1);
					break;
				}
		} else {
			pstrcpy(mask,"");
		}
 
		dos_to_unix(mask, True);
 
		/* and add them to the dirlist pool */
		dirlist = Realloc(dirlist,dirlist_len + data_len);

		if (!dirlist) {
			DEBUG(0,("Failed to expand dirlist\n"));
			break;
		}

		/* put in a length for the last entry, to ensure we can chain entries 
		   into the next packet */
		for (p2=p,i=0;i<(ff_searchcount-1);i++)
			p2 += interpret_long_filename(info_level,p2,NULL);
		SSVAL(p2,0,data_len - PTR_DIFF(p2,p));

		/* grab the data for later use */
		memcpy(dirlist+dirlist_len,p,data_len);
		dirlist_len += data_len;

		total_received += ff_searchcount;

		if (rdata) free(rdata); rdata = NULL;
		if (rparam) free(rparam); rparam = NULL;
		
		DEBUG(3,("received %d entries (eos=%d)\n",
			 ff_searchcount,ff_eos));

		if (ff_searchcount > 0) loop_count = 0;

		First = False;
	}

	for (p=dirlist,i=0;i<total_received;i++) {
		p += interpret_long_filename(info_level,p,&finfo);
		fn(&finfo, Mask);
	}

	/* free up the dirlist buffer */
	if (dirlist) free(dirlist);
	return(total_received);
}


/****************************************************************************
Send a SamOEMChangePassword command
****************************************************************************/

BOOL cli_oem_change_password(struct cli_state *cli, const char *user, const char *new_password,
                             const char *old_password)
{
  char param[16+sizeof(fstring)];
  char data[532];
  char *p = param;
  fstring upper_case_old_pw;
  fstring upper_case_new_pw;
  unsigned char old_pw_hash[16];
  unsigned char new_pw_hash[16];
  int data_len;
  int param_len = 0;
  char *rparam = NULL;
  char *rdata = NULL;
  int rprcnt, rdrcnt;
  pstring dos_new_password;

  if (strlen(user) >= sizeof(fstring)-1) {
    DEBUG(0,("cli_oem_change_password: user name %s is too long.\n", user));
    return False;
  }

  SSVAL(p,0,214); /* SamOEMChangePassword command. */
  p += 2;
  pstrcpy(p, "zsT");
  p = skip_string(p,1);
  pstrcpy(p, "B516B16");
  p = skip_string(p,1);
  pstrcpy(p,user);
  p = skip_string(p,1);
  SSVAL(p,0,532);
  p += 2;

  param_len = PTR_DIFF(p,param);

  /*
   * Get the Lanman hash of the old password, we
   * use this as the key to make_oem_passwd_hash().
   */
  memset(upper_case_old_pw, '\0', sizeof(upper_case_old_pw));
  fstrcpy(upper_case_old_pw, old_password);
  unix_to_dos(upper_case_old_pw,True);
  strupper(upper_case_old_pw);
  E_P16((uchar *)upper_case_old_pw, old_pw_hash);

  pstrcpy(dos_new_password, new_password);
  unix_to_dos(dos_new_password, True);

  if (!make_oem_passwd_hash( data, dos_new_password, old_pw_hash, False))
    return False;

  /* 
   * Now place the old password hash in the data.
   */
  memset(upper_case_new_pw, '\0', sizeof(upper_case_new_pw));
  fstrcpy(upper_case_new_pw, new_password);
  unix_to_dos(upper_case_new_pw,True);
  strupper(upper_case_new_pw);

  E_P16((uchar *)upper_case_new_pw, new_pw_hash);

  E_old_pw_hash( new_pw_hash, old_pw_hash, (uchar *)&data[516]);

  data_len = 532;
    
  if (cli_send_trans(cli,SMBtrans,
                    PIPE_LANMAN,strlen(PIPE_LANMAN),      /* name, length */
                    0,0,                                  /* fid, flags */
                    NULL,0,0,                             /* setup, length, max */
                    param,param_len,2,                    /* param, length, max */
                    data,data_len,0                       /* data, length, max */
                   ) == False) {
    DEBUG(0,("cli_oem_change_password: Failed to send password change for user %s\n",
              user ));
    return False;
  }

  if (cli_receive_trans(cli,SMBtrans,
                       &rparam, &rprcnt,
                       &rdata, &rdrcnt)) {
    if (rparam)
      cli->rap_error = SVAL(rparam,0);
  }

  if (rparam)
    free(rparam);
  if (rdata)
    free(rdata);

  return (cli->rap_error == 0);
}

/****************************************************************************
send a negprot command
****************************************************************************/
BOOL cli_negprot(struct cli_state *cli)
{
	char *p;
	int numprots;
	int plength;

	memset(cli->outbuf,'\0',smb_size);

	/* setup the protocol strings */
	for (plength=0,numprots=0;
	     prots[numprots].name && prots[numprots].prot<=cli->protocol;
	     numprots++)
		plength += strlen(prots[numprots].name)+2;
    
	set_message(cli->outbuf,0,plength,True);

	p = smb_buf(cli->outbuf);
	for (numprots=0;
	     prots[numprots].name && prots[numprots].prot<=cli->protocol;
	     numprots++) {
		*p++ = 2;
		pstrcpy(p,prots[numprots].name);
		unix_to_dos(p,True);
		p += strlen(p) + 1;
	}

	CVAL(cli->outbuf,smb_com) = SMBnegprot;
	cli_setup_packet(cli);

	CVAL(smb_buf(cli->outbuf),0) = 2;

	cli_send_smb(cli);
	if (!cli_receive_smb(cli))
		return False;

	show_msg(cli->inbuf);

	if (CVAL(cli->inbuf,smb_rcls) != 0 || 
	    ((int)SVAL(cli->inbuf,smb_vwv0) >= numprots)) {
		return(False);
	}

	cli->protocol = prots[SVAL(cli->inbuf,smb_vwv0)].prot;


	if (cli->protocol >= PROTOCOL_NT1) {    
		/* NT protocol */
		cli->sec_mode = CVAL(cli->inbuf,smb_vwv1);
		cli->max_mux = SVAL(cli->inbuf, smb_vwv1+1);
		cli->max_xmit = IVAL(cli->inbuf,smb_vwv3+1);
		cli->sesskey = IVAL(cli->inbuf,smb_vwv7+1);
		cli->serverzone = SVALS(cli->inbuf,smb_vwv15+1);
		cli->serverzone *= 60;
		/* this time arrives in real GMT */
		cli->servertime = interpret_long_date(cli->inbuf+smb_vwv11+1);
		memcpy(cli->cryptkey,smb_buf(cli->inbuf),8);
		cli->capabilities = IVAL(cli->inbuf,smb_vwv9+1);
		if (cli->capabilities & 1) {
			cli->readbraw_supported = True;
			cli->writebraw_supported = True;      
		}
	} else if (cli->protocol >= PROTOCOL_LANMAN1) {
		cli->sec_mode = SVAL(cli->inbuf,smb_vwv1);
		cli->max_xmit = SVAL(cli->inbuf,smb_vwv2);
		cli->sesskey = IVAL(cli->inbuf,smb_vwv6);
		cli->serverzone = SVALS(cli->inbuf,smb_vwv10);
		cli->serverzone *= 60;
		/* this time is converted to GMT by make_unix_date */
		cli->servertime = make_unix_date(cli->inbuf+smb_vwv8);
		cli->readbraw_supported = ((SVAL(cli->inbuf,smb_vwv5) & 0x1) != 0);
		cli->writebraw_supported = ((SVAL(cli->inbuf,smb_vwv5) & 0x2) != 0);
		memcpy(cli->cryptkey,smb_buf(cli->inbuf),8);
	} else {
		/* the old core protocol */
		cli->sec_mode = 0;
		cli->serverzone = TimeDiff(time(NULL));
	}

	cli->max_xmit = MIN(cli->max_xmit, CLI_BUFFER_SIZE);

	return True;
}


/****************************************************************************
  send a session request.  see rfc1002.txt 4.3 and 4.3.2
****************************************************************************/
BOOL cli_session_request(struct cli_state *cli,
			 struct nmb_name *calling, struct nmb_name *called)
{
	char *p;
	int len = 4;
	/* send a session request (RFC 1002) */

	memcpy(&(cli->calling), calling, sizeof(*calling));
	memcpy(&(cli->called ), called , sizeof(*called ));
  
	/* put in the destination name */
	p = cli->outbuf+len;
	name_mangle(cli->called .name, p, cli->called .name_type);
	len += name_len(p);

	/* and my name */
	p = cli->outbuf+len;
	name_mangle(cli->calling.name, p, cli->calling.name_type);
	len += name_len(p);

	/* setup the packet length */
	_smb_setlen(cli->outbuf,len);
	CVAL(cli->outbuf,0) = 0x81;

#ifdef WITH_SSL
retry:
#endif /* WITH_SSL */

	cli_send_smb(cli);
	DEBUG(5,("Sent session request\n"));

	if (!cli_receive_smb(cli))
		return False;

	if (CVAL(cli->inbuf,0) == 0x84) {
		/* C. Hoch  9/14/95 Start */
		/* For information, here is the response structure.
		 * We do the byte-twiddling to for portability.
		struct RetargetResponse{
			unsigned char type;
			unsigned char flags;
			int16 length;
			int32 ip_addr;
			int16 port;
		};
		*/
		int port = (CVAL(cli->inbuf,8)<<8)+CVAL(cli->inbuf,9);
		/* SESSION RETARGET */
		putip((char *)&cli->dest_ip,cli->inbuf+4);

		cli->fd = open_socket_out(SOCK_STREAM, &cli->dest_ip, port, LONG_CONNECT_TIMEOUT);
		if (cli->fd == -1)
			return False;

		DEBUG(3,("Retargeted\n"));

		set_socket_options(cli->fd,user_socket_options);

		/* Try again */
		{
			static int depth;
			BOOL ret;
			if (depth > 4) {
				DEBUG(0,("Retarget recursion - failing\n"));
				return False;
			}
			depth++;
			ret = cli_session_request(cli, calling, called);
			depth--;
			return ret;
		}
	} /* C. Hoch 9/14/95 End */

#ifdef WITH_SSL
    if (CVAL(cli->inbuf,0) == 0x83 && CVAL(cli->inbuf,4) == 0x8e){ /* use ssl */
        if (!sslutil_fd_is_ssl(cli->fd)){
            if (sslutil_connect(cli->fd) == 0)
                goto retry;
        }
    }
#endif /* WITH_SSL */

	if (CVAL(cli->inbuf,0) != 0x82) {
                /* This is the wrong place to put the error... JRA. */
		cli->rap_error = CVAL(cli->inbuf,4);
		return False;
	}
	return(True);
}


/****************************************************************************
open the client sockets
****************************************************************************/
BOOL cli_connect(struct cli_state *cli, const char *host, struct in_addr *ip)
{
	extern struct in_addr ipzero;

	fstrcpy(cli->desthost, host);
	
	if (!ip || ip_equal(*ip, ipzero)) {
                if (!resolve_name( cli->desthost, &cli->dest_ip, 0x20)) {
                        return False;
                }
		if (ip) *ip = cli->dest_ip;
	} else {
		cli->dest_ip = *ip;
	}

        if (cli->port == 0) cli->port = 139;  /* Set to default */

	cli->fd = open_socket_out(SOCK_STREAM, &cli->dest_ip, 
				  cli->port, cli->timeout);
	if (cli->fd == -1)
		return False;

	set_socket_options(cli->fd,user_socket_options);

	return True;
}


/****************************************************************************
initialise a client structure
****************************************************************************/
struct cli_state *cli_initialise(struct cli_state *cli)
{
	if (!cli) {
		cli = (struct cli_state *)malloc(sizeof(*cli));
		if (!cli)
			return NULL;
		ZERO_STRUCTP(cli);
	}

	if (cli->initialised) {
		cli_shutdown(cli);
	}

	ZERO_STRUCTP(cli);

	cli->port = 0;
	cli->fd = -1;
	cli->cnum = -1;
	cli->pid = (uint16)getpid();
	cli->mid = 1;
	cli->vuid = UID_FIELD_INVALID;
	cli->protocol = PROTOCOL_NT1;
	cli->timeout = 20000; /* Timeout is in milliseconds. */
	cli->bufsize = CLI_BUFFER_SIZE+4;
	cli->max_xmit = cli->bufsize;
	cli->outbuf = (char *)malloc(cli->bufsize);
	cli->inbuf = (char *)malloc(cli->bufsize);
	if (!cli->outbuf || !cli->inbuf)
	{
		return False;
	}

	memset(cli->outbuf, '\0', cli->bufsize);
	memset(cli->inbuf, '\0', cli->bufsize);

	cli->initialised = 1;

	return cli;
}

/****************************************************************************
shutdown a client structure
****************************************************************************/
void cli_shutdown(struct cli_state *cli)
{
	if (cli->outbuf)
	{
		free(cli->outbuf);
	}
	if (cli->inbuf)
	{
		free(cli->inbuf);
	}
#ifdef WITH_SSL
    if (cli->fd != -1)
      sslutil_disconnect(cli->fd);
#endif /* WITH_SSL */
	if (cli->fd != -1) 
      close(cli->fd);
	memset(cli, 0, sizeof(*cli));
}


/****************************************************************************
  return error codes for the last packet
  returns 0 if there was no error and the best approx of a unix errno
  otherwise

  for 32 bit "warnings", a return code of 0 is expected.

****************************************************************************/
int cli_error(struct cli_state *cli, uint8 *eclass, uint32 *num, uint32 *nt_rpc_error)
{
	int  flgs2;
	char rcls;
	int code;

	if (eclass) *eclass = 0;
	if (num   ) *num = 0;
	if (nt_rpc_error) *nt_rpc_error = 0;

	if(!cli->initialised)
		return EINVAL;

	if(!cli->inbuf)
		return ENOMEM;

	flgs2 = SVAL(cli->inbuf,smb_flg2);
	if (nt_rpc_error) *nt_rpc_error = cli->nt_error;

	if (flgs2 & FLAGS2_32_BIT_ERROR_CODES) {
		/* 32 bit error codes detected */
		uint32 nt_err = IVAL(cli->inbuf,smb_rcls);
		if (num) *num = nt_err;
		DEBUG(10,("cli_error: 32 bit codes: code=%08x\n", nt_err));
		if (!IS_BITS_SET_ALL(nt_err, 0xc0000000)) return 0;

		switch (nt_err & 0xFFFFFF) {
		case NT_STATUS_ACCESS_VIOLATION: return EACCES;
		case NT_STATUS_NO_SUCH_FILE: return ENOENT;
		case NT_STATUS_NO_SUCH_DEVICE: return ENODEV;
		case NT_STATUS_INVALID_HANDLE: return EBADF;
		case NT_STATUS_NO_MEMORY: return ENOMEM;
		case NT_STATUS_ACCESS_DENIED: return EACCES;
		case NT_STATUS_OBJECT_NAME_NOT_FOUND: return ENOENT;
		case NT_STATUS_SHARING_VIOLATION: return EBUSY;
		case NT_STATUS_OBJECT_PATH_INVALID: return ENOTDIR;
		case NT_STATUS_OBJECT_NAME_COLLISION: return EEXIST;
		}

		/* for all other cases - a default code */
		return EINVAL;
	}

	rcls  = CVAL(cli->inbuf,smb_rcls);
	code  = SVAL(cli->inbuf,smb_err);
	if (rcls == 0) return 0;

	if (eclass) *eclass = rcls;
	if (num   ) *num    = code;

	if (rcls == ERRDOS) {
		switch (code) {
		case ERRbadfile: return ENOENT;
		case ERRbadpath: return ENOTDIR;
		case ERRnoaccess: return EACCES;
		case ERRfilexists: return EEXIST;
		case ERRrename: return EEXIST;
		case ERRbadshare: return EBUSY;
		case ERRlock: return EBUSY;
		}
	}
	if (rcls == ERRSRV) {
		switch (code) {
		case ERRbadpw: return EPERM;
		case ERRaccess: return EACCES;
		case ERRnoresource: return ENOMEM;
		case ERRinvdevice: return ENODEV;
		case ERRinvnetname: return ENODEV;
		}
	}
	/* for other cases */
	return EINVAL;
}

/****************************************************************************
set socket options on a open connection
****************************************************************************/
void cli_sockopt(struct cli_state *cli, char *options)
{
	set_socket_options(cli->fd, options);
}

/****************************************************************************
set the PID to use for smb messages. Return the old pid.
****************************************************************************/
uint16 cli_setpid(struct cli_state *cli, uint16 pid)
{
	uint16 ret = cli->pid;
	cli->pid = pid;
	return ret;
}

/****************************************************************************
re-establishes a connection
****************************************************************************/
BOOL cli_reestablish_connection(struct cli_state *cli)
{
	struct nmb_name calling;
	struct nmb_name called;
	fstring dest_host;
	fstring share;
	fstring dev;
	BOOL do_tcon = False;
	int oldfd = cli->fd;

	if (!cli->initialised || cli->fd == -1)
	{
		DEBUG(3,("cli_reestablish_connection: not connected\n"));
		return False;
	}

	/* copy the parameters necessary to re-establish the connection */

	if (cli->cnum != 0)
	{
		fstrcpy(share, cli->share);
		fstrcpy(dev  , cli->dev);
		do_tcon = True;
	}

	memcpy(&called , &(cli->called ), sizeof(called ));
	memcpy(&calling, &(cli->calling), sizeof(calling));
	fstrcpy(dest_host, cli->full_dest_host_name);

	DEBUG(5,("cli_reestablish_connection: %s connecting to %s (ip %s) - %s [%s]\n",
		 nmb_namestr(&calling), nmb_namestr(&called), 
		 inet_ntoa(cli->dest_ip),
		 cli->user_name, cli->domain));

	cli->fd = -1;

	if (cli_establish_connection(cli,
				     dest_host, &cli->dest_ip,
				     &calling, &called,
				     share, dev, False, do_tcon)) {
		if (cli->fd != oldfd) {
			if (dup2(cli->fd, oldfd) == oldfd) {
				close(cli->fd);
			}
		}
		return True;
	}
	return False;
}

/****************************************************************************
establishes a connection right up to doing tconX, reading in a password.
****************************************************************************/
BOOL cli_establish_connection(struct cli_state *cli, 
				char *dest_host, struct in_addr *dest_ip,
				struct nmb_name *calling, struct nmb_name *called,
				char *service, char *service_type,
				BOOL do_shutdown, BOOL do_tcon)
{
	DEBUG(5,("cli_establish_connection: %s connecting to %s (%s) - %s [%s]\n",
		          nmb_namestr(calling), nmb_namestr(called), inet_ntoa(*dest_ip),
	              cli->user_name, cli->domain));

	/* establish connection */

	if ((!cli->initialised))
	{
		return False;
	}

	if (cli->fd == -1)
	{
		if (!cli_connect(cli, dest_host, dest_ip))
		{
			DEBUG(1,("cli_establish_connection: failed to connect to %s (%s)\n",
					  nmb_namestr(calling), inet_ntoa(*dest_ip)));
			return False;
		}
	}

	if (!cli_session_request(cli, calling, called))
	{
		DEBUG(1,("failed session request\n"));
		if (do_shutdown)
          cli_shutdown(cli);
		return False;
	}

	if (!cli_negprot(cli))
	{
		DEBUG(1,("failed negprot\n"));
		if (do_shutdown)
          cli_shutdown(cli);
		return False;
	}

	if (cli->pwd.cleartext || cli->pwd.null_pwd)
	{
		fstring passwd;
		int pass_len;

		if (cli->pwd.null_pwd)
		{
			/* attempt null session */
			passwd[0] = 0;
			pass_len = 1;
		}
		else
		{
			/* attempt clear-text session */
			pwd_get_cleartext(&(cli->pwd), passwd);
			pass_len = strlen(passwd);
		}

		/* attempt clear-text session */
		if (!cli_session_setup(cli, cli->user_name,
	                       passwd, pass_len,
	                       NULL, 0,
	                       cli->domain))
		{
			DEBUG(1,("failed session setup\n"));
			if (do_shutdown)
			{
				cli_shutdown(cli);
			}
			return False;
		}
		if (do_tcon)
		{
			if (!cli_send_tconX(cli, service, service_type,
			                    (char*)passwd, strlen(passwd)))
			{
				DEBUG(1,("failed tcon_X\n"));
				if (do_shutdown)
				{
					cli_shutdown(cli);
				}
				return False;
			}
		}
	}
	else
	{
		/* attempt encrypted session */
		unsigned char nt_sess_pwd[24];
		unsigned char lm_sess_pwd[24];

		/* creates (storing a copy of) and then obtains a 24 byte password OWF */
		pwd_make_lm_nt_owf(&(cli->pwd), cli->cryptkey);
		pwd_get_lm_nt_owf(&(cli->pwd), lm_sess_pwd, nt_sess_pwd);

		/* attempt encrypted session */
		if (!cli_session_setup(cli, cli->user_name,
	                       (char*)lm_sess_pwd, sizeof(lm_sess_pwd),
	                       (char*)nt_sess_pwd, sizeof(nt_sess_pwd),
	                       cli->domain))
		{
			DEBUG(1,("failed session setup\n"));
			if (do_shutdown)
              cli_shutdown(cli);
			return False;
		}

		if (do_tcon)
		{
			if (!cli_send_tconX(cli, service, service_type,
			                    (char*)nt_sess_pwd, sizeof(nt_sess_pwd)))
			{
				DEBUG(1,("failed tcon_X\n"));
				if (do_shutdown)
                  cli_shutdown(cli);
				return False;
			}
		}
	}

	if (do_shutdown)
      cli_shutdown(cli);

	return True;
}


/****************************************************************************
  cancel a print job
  ****************************************************************************/
int cli_printjob_del(struct cli_state *cli, int job)
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	int rdrcnt,rprcnt, ret = -1;
	pstring param;

	memset(param,'\0',sizeof(param));

	p = param;
	SSVAL(p,0,81);		/* DosPrintJobDel() */
	p += 2;
	pstrcpy(p,"W");
	p = skip_string(p,1);
	pstrcpy(p,"");
	p = skip_string(p,1);
	SSVAL(p,0,job);     
	p += 2;
	
	if (cli_api(cli, 
		    param, PTR_DIFF(p,param), 1024,  /* Param, length, maxlen */
		    NULL, 0, CLI_BUFFER_SIZE,            /* data, length, maxlen */
		    &rparam, &rprcnt,                /* return params, length */
		    &rdata, &rdrcnt)) {               /* return data, length */
		ret = SVAL(rparam,0);
	}

	if (rparam) free(rparam);
	if (rdata) free(rdata);

	return ret;
}


/****************************************************************************
call fn() on each entry in a print queue
****************************************************************************/
int cli_print_queue(struct cli_state *cli, 
		    void (*fn)(struct print_job_info *))
{
	char *rparam = NULL;
	char *rdata = NULL;
	char *p;
	int rdrcnt, rprcnt;
	pstring param;
	int result_code=0;
	int i = -1;
	
	memset(param,'\0',sizeof(param));

	p = param;
	SSVAL(p,0,76);         /* API function number 76 (DosPrintJobEnum) */
	p += 2;
	pstrcpy(p,"zWrLeh");   /* parameter description? */
	p = skip_string(p,1);
	pstrcpy(p,"WWzWWDDzz");  /* returned data format */
	p = skip_string(p,1);
	pstrcpy(p,cli->share);    /* name of queue */
	p = skip_string(p,1);
	SSVAL(p,0,2);   /* API function level 2, PRJINFO_2 data structure */
	SSVAL(p,2,1000); /* size of bytes of returned data buffer */
	p += 4;
	pstrcpy(p,"");   /* subformat */
	p = skip_string(p,1);

	DEBUG(4,("doing cli_print_queue for %s\n", cli->share));

	if (cli_api(cli, 
		    param, PTR_DIFF(p,param), 1024,  /* Param, length, maxlen */
		    NULL, 0, CLI_BUFFER_SIZE,            /* data, length, maxlen */
		    &rparam, &rprcnt,                /* return params, length */
		    &rdata, &rdrcnt)) {               /* return data, length */
		int converter;
		result_code = SVAL(rparam,0);
		converter = SVAL(rparam,2);       /* conversion factor */

		if (result_code == 0) {
			struct print_job_info job;
			
			p = rdata; 

			for (i = 0; i < SVAL(rparam,4); ++i) {
				job.id = SVAL(p,0);
				job.priority = SVAL(p,2);
				fstrcpy(job.user,
					fix_char_ptr(SVAL(p,4), converter, 
						     rdata, rdrcnt));
				job.t = make_unix_date3(p + 12);
				job.size = IVAL(p,16);
				fstrcpy(job.name,fix_char_ptr(SVAL(p,24), 
							      converter, 
							      rdata, rdrcnt));
				fn(&job);				
				p += 28;
			}
		}
	}

	/* If any parameters or data were returned, free the storage. */
	if(rparam) free(rparam);
	if(rdata) free(rdata);

	return i;
}

/****************************************************************************
check for existance of a dir
****************************************************************************/
BOOL cli_chkpath(struct cli_state *cli, char *path)
{
	pstring path2;
	char *p;
	
	safe_strcpy(path2,path,sizeof(pstring));
	trim_string(path2,NULL,"\\");
	if (!*path2) *path2 = '\\';
	
	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,0,4 + strlen(path2),True);
	SCVAL(cli->outbuf,smb_com,SMBchkpth);
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);
	p = smb_buf(cli->outbuf);
	*p++ = 4;
	safe_strcpy(p,path2,strlen(path2));
	unix_to_dos(p,True);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (cli_error(cli, NULL, NULL, NULL)) return False;

	return True;
}


/****************************************************************************
start a message sequence
****************************************************************************/
BOOL cli_message_start(struct cli_state *cli, char *host, char *username, 
			      int *grp)
{
	char *p;

	/* send a SMBsendstrt command */
	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,0,0,True);
	CVAL(cli->outbuf,smb_com) = SMBsendstrt;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);
	
	p = smb_buf(cli->outbuf);
	*p++ = 4;
	pstrcpy(p,username);
	unix_to_dos(p,True);
	p = skip_string(p,1);
	*p++ = 4;
	pstrcpy(p,host);
	unix_to_dos(p,True);
	p = skip_string(p,1);
	
	set_message(cli->outbuf,0,PTR_DIFF(p,smb_buf(cli->outbuf)),False);
	
	cli_send_smb(cli);	
	
	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (cli_error(cli, NULL, NULL, NULL)) return False;

	*grp = SVAL(cli->inbuf,smb_vwv0);

	return True;
}


/****************************************************************************
send a message 
****************************************************************************/
BOOL cli_message_text(struct cli_state *cli, char *msg, int len, int grp)
{
	char *p;

	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,1,len+3,True);
	CVAL(cli->outbuf,smb_com) = SMBsendtxt;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	SSVAL(cli->outbuf,smb_vwv0,grp);
	
	p = smb_buf(cli->outbuf);
	*p = 1;
	SSVAL(p,1,len);
	memcpy(p+3,msg,len);
	cli_send_smb(cli);

	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (cli_error(cli, NULL, NULL, NULL)) return False;

	return True;
}      

/****************************************************************************
end a message 
****************************************************************************/
BOOL cli_message_end(struct cli_state *cli, int grp)
{
	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,1,0,True);
	CVAL(cli->outbuf,smb_com) = SMBsendend;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);

	SSVAL(cli->outbuf,smb_vwv0,grp);

	cli_setup_packet(cli);
	
	cli_send_smb(cli);

	if (!cli_receive_smb(cli)) {
		return False;
	}

	if (cli_error(cli, NULL, NULL, NULL)) return False;

	return True;
}      


/****************************************************************************
query disk space
****************************************************************************/
BOOL cli_dskattr(struct cli_state *cli, int *bsize, int *total, int *avail)
{
	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,0,0,True);
	CVAL(cli->outbuf,smb_com) = SMBdskattr;
	SSVAL(cli->outbuf,smb_tid,cli->cnum);
	cli_setup_packet(cli);

	cli_send_smb(cli);
	if (!cli_receive_smb(cli)) {
		return False;
	}

	*bsize = SVAL(cli->inbuf,smb_vwv1)*SVAL(cli->inbuf,smb_vwv2);
	*total = SVAL(cli->inbuf,smb_vwv0);
	*avail = SVAL(cli->inbuf,smb_vwv3);
	
	return True;
}

/****************************************************************************
 Attempt a NetBIOS session request, falling back to *SMBSERVER if needed.
****************************************************************************/

BOOL attempt_netbios_session_request(struct cli_state *cli, char *srchost, char *desthost,
                                     struct in_addr *pdest_ip)
{
  struct nmb_name calling, called;

  make_nmb_name(&calling, srchost, 0x0);

  /*
   * If the called name is an IP address
   * then use *SMBSERVER immediately.
   */

  if(is_ipaddress(desthost))
    make_nmb_name(&called, "*SMBSERVER", 0x20);
  else
    make_nmb_name(&called, desthost, 0x20);

  if (!cli_session_request(cli, &calling, &called)) {
    struct nmb_name smbservername;

    make_nmb_name(&smbservername , "*SMBSERVER", 0x20);

    /*
     * If the name wasn't *SMBSERVER then
     * try with *SMBSERVER if the first name fails.
     */

    if (nmb_name_equal(&called, &smbservername)) {

        /*
         * The name used was *SMBSERVER, don't bother with another name.
         */

        DEBUG(0,("attempt_netbios_session_request: %s rejected the session for name *SMBSERVER<20> \
with error %s.\n", desthost, cli_errstr(cli) ));
	    cli_shutdown(cli);
		return False;
	}

    cli_shutdown(cli);

    if (!cli_initialise(cli) ||
        !cli_connect(cli, desthost, pdest_ip) ||
        !cli_session_request(cli, &calling, &smbservername)) {
          DEBUG(0,("attempt_netbios_session_request: %s rejected the session for \
name *SMBSERVER with error %s\n", desthost, cli_errstr(cli) ));
          cli_shutdown(cli);
          return False;
    }
  }

  return True;
}
