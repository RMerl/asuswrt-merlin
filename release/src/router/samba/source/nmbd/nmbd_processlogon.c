/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   
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
   
   Revision History:

*/

#include "includes.h"

extern int DEBUGLEVEL;

extern pstring global_myname;
extern fstring global_myworkgroup;

/****************************************************************************
Process a domain logon packet
**************************************************************************/

void process_logon_packet(struct packet_struct *p,char *buf,int len, 
                          char *mailslot)
{
  struct dgram_packet *dgram = &p->packet.dgram;
  pstring my_name;
  fstring reply_name;
  pstring outbuf;
  int code;
  uint16 token = 0;
  uint32 ntversion = 0;
  uint16 lmnttoken = 0;
  uint16 lm20token = 0;
  uint32 domainsidsize;
  BOOL short_request = False;
  char *getdc;
  char *uniuser; /* Unicode user name. */
  pstring ascuser;
  char *unicomp; /* Unicode computer name. */

  memset(outbuf, 0, sizeof(outbuf));

  if (!lp_domain_logons())
  {
    DEBUG(3,("process_logon_packet: Logon packet received from IP %s and domain \
logons are not enabled.\n", inet_ntoa(p->ip) ));
    return;
  }

  pstrcpy(my_name, global_myname);
  strupper(my_name);

  code = SVAL(buf,0);
  DEBUG(1,("process_logon_packet: Logon from %s: code = 0x%x\n", inet_ntoa(p->ip), code));

  switch (code)
  {
    case 0:    
    {
      char *q = buf + 2;
      char *machine = q;
      char *user = skip_string(machine,1);

      getdc = skip_string(user,1);
      q = skip_string(getdc,1);
      token = SVAL(q,3);

      fstrcpy(reply_name,my_name); 

      DEBUG(3,("process_logon_packet: Domain login request from %s at IP %s user=%s token=%x\n",
             machine,inet_ntoa(p->ip),user,token));

      q = outbuf;
      SSVAL(q, 0, 6);
      q += 2;

      fstrcpy(reply_name, "\\\\");
      fstrcat(reply_name, my_name);
      fstrcpy(q, reply_name); q = skip_string(q, 1); /* PDC name */

      SSVAL(q, 0, token);
      q += 2;

      dump_data(4, outbuf, PTR_DIFF(q, outbuf));

      send_mailslot(True, getdc, 
                    outbuf,PTR_DIFF(q,outbuf),
		    global_myname, 0x0,
                    dgram->source_name.name,
                    dgram->source_name.name_type,
                    p->ip, *iface_ip(p->ip), p->port);  
      break;
    }

    case QUERYFORPDC:
    {
      char *q = buf + 2;
      char *machine = q;

      getdc = skip_string(machine,1);
      q = skip_string(getdc,1);
      q = ALIGN2(q, buf);

      /* at this point we can work out if this is a W9X or NT style
         request. Experiments show that the difference is wether the
         packet ends here. For a W9X request we now end with a pair of
         bytes (usually 0xFE 0xFF) whereas with NT we have two further
         strings - the following is a simple way of detecting this */
      if (len - PTR_DIFF(q, buf) <= 3) {
	      short_request = True;
      } else {
	      unicomp = q;

	      /* A full length (NT style) request */
	      q = skip_unibuf(unicomp, PTR_DIFF(buf + len, unicomp));

	      if (len - PTR_DIFF(q, buf) > 8) {
					/* with NT5 clients we can sometimes
					   get additional data - a length specificed string
					   containing the domain name, then 16 bytes of
					   data (no idea what it is) */
					int dom_len = CVAL(q, 0);
					q++;
					if (dom_len != 0) {
						q += dom_len + 1;
					}
					q += 16;
	      }
	      ntversion = IVAL(q, 0);
	      lmnttoken = SVAL(q, 4);
	      lm20token = SVAL(q, 6);
      }

      /* Construct reply. */
      q = outbuf;
      SSVAL(q, 0, QUERYFORPDC_R);
      q += 2;

      fstrcpy(reply_name,my_name);
      fstrcpy(q, reply_name);
      q = skip_string(q, 1); /* PDC name */

      /* PDC and domain name */
      if (!short_request)  /* Make a full reply */
      {
        q = ALIGN2(q, buf);

        q += dos_PutUniCode(q, my_name, sizeof(pstring), True); /* PDC name */
        q += dos_PutUniCode(q, global_myworkgroup,sizeof(pstring), True); /* Domain name*/

        SIVAL(q, 0, 1); /* our nt version */
        SSVAL(q, 4, 0xffff); /* our lmnttoken */
        SSVAL(q, 6, 0xffff); /* our lm20token */
        q += 8;
      }

      /* RJS, 21-Feb-2000, we send a short reply if the request was short */

      DEBUG(3,("process_logon_packet: GETDC request from %s at IP %s, \
reporting %s domain %s 0x%x ntversion=%x lm_nt token=%x lm_20 token=%x\n",
            machine,inet_ntoa(p->ip), reply_name, global_myworkgroup,
            QUERYFORPDC_R, (uint32)ntversion, (uint32)lmnttoken,
            (uint32)lm20token ));

      dump_data(4, outbuf, PTR_DIFF(q, outbuf));

      send_mailslot(True, getdc,
                  outbuf,PTR_DIFF(q,outbuf),
		    global_myname, 0x0,
                  dgram->source_name.name,
                  dgram->source_name.name_type,
                  p->ip, *iface_ip(p->ip), p->port);  
      return;
    }

    case SAMLOGON:
    {
      char *q = buf + 2;

      q += 2;
      unicomp = q;
      uniuser = skip_unibuf(unicomp, PTR_DIFF(buf+len, unicomp));
      getdc = skip_unibuf(uniuser,PTR_DIFF(buf+len, uniuser));
      q = skip_string(getdc,1);
      q += 4; /* Account Control Bits - indicating username type */
      domainsidsize = IVAL(q, 0);
      q += 4;

      DEBUG(3,("process_logon_packet: SAMLOGON sidsize %d, len = %d\n", domainsidsize, len));

      if (domainsidsize < (len - PTR_DIFF(q, buf)) && (domainsidsize != 0)) {
	      q += domainsidsize;
	      q = ALIGN4(q, buf);
      }

      DEBUG(3,("process_logon_packet: len = %d PTR_DIFF(q, buf) = %d\n", len, PTR_DIFF(q, buf) ));

      if (len - PTR_DIFF(q, buf) > 8) {
	      /* with NT5 clients we can sometimes
		 get additional data - a length specificed string
		 containing the domain name, then 16 bytes of
		 data (no idea what it is) */
	      int dom_len = CVAL(q, 0);
	      q++;
	      if (dom_len < (len - PTR_DIFF(q, buf)) && (dom_len != 0)) {
		      q += dom_len + 1;
	      }
	      q += 16;
      }

      ntversion = IVAL(q, 0);
      lmnttoken = SVAL(q, 4);
      lm20token = SVAL(q, 6);
      q += 8;

      DEBUG(3,("process_logon_packet: SAMLOGON sidsize %d ntv %d\n", domainsidsize, ntversion));

      /*
       * we respond regadless of whether the machine is in our password 
       * database. If it isn't then we let smbd send an appropriate error.
       * Let's ignore the SID.
       */

      pstrcpy(ascuser, dos_unistr(uniuser));
      DEBUG(3,("process_logon_packet: SAMLOGON user %s\n", ascuser));

      fstrcpy(reply_name,"\\\\"); /* Here it wants \\LOGONSERVER. */
      fstrcpy(reply_name+2,my_name); 

      DEBUG(3,("process_logon_packet: SAMLOGON request from %s(%s) for %s, returning logon svr %s domain %s code %x token=%x\n",
	       dos_unistr(unicomp),inet_ntoa(p->ip), ascuser, reply_name, global_myworkgroup,
	       SAMLOGON_R ,lmnttoken));

      /* Construct reply. */

      q = outbuf;
      if (SVAL(uniuser, 0) == 0) {
	      SSVAL(q, 0, SAMLOGON_UNK_R);	/* user unknown */
      }	else {
	      SSVAL(q, 0, SAMLOGON_R);
      }
      q += 2;

      q += dos_PutUniCode(q, reply_name,sizeof(pstring), True);
      q += dos_PutUniCode(q, ascuser, sizeof(pstring), True);
      q += dos_PutUniCode(q, global_myworkgroup,sizeof(pstring), True);

      /* tell the client what version we are */
      SIVAL(q, 0, 1); /* our ntversion */
      SSVAL(q, 4, 0xffff); /* our lmnttoken */ 
      SSVAL(q, 6, 0xffff); /* our lm20token */
      q += 8;

      dump_data(4, outbuf, PTR_DIFF(q, outbuf));

      send_mailslot(True, getdc,
                   outbuf,PTR_DIFF(q,outbuf),
		    global_myname, 0x0,
                   dgram->source_name.name,
                   dgram->source_name.name_type,
                   p->ip, *iface_ip(p->ip), p->port);  
      break;
    }

    default:
    {
      DEBUG(3,("process_logon_packet: Unknown domain request %d\n",code));
      return;
    }
  }
}
