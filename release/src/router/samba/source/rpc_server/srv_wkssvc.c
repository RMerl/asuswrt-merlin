
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;
extern pstring global_myname;


/*******************************************************************
 create_wks_info_100
 ********************************************************************/
static void create_wks_info_100(WKS_INFO_100 *inf)
{
	pstring my_name;
	pstring domain;

	DEBUG(5,("create_wks_info_100: %d\n", __LINE__));

	pstrcpy (my_name, global_myname);
	strupper(my_name);

	pstrcpy (domain , lp_workgroup());
	strupper(domain);

	init_wks_info_100(inf,
	                  0x000001f4, /* platform id info */
	                  lp_major_announce_version(),
	                  lp_minor_announce_version(),
	                  my_name, domain);
}

/*******************************************************************
 wks_reply_query_info
 
 only supports info level 100 at the moment.

 ********************************************************************/
static void wks_reply_query_info(WKS_Q_QUERY_INFO *q_u,
				prs_struct *rdata,
				int status)
{
	WKS_R_QUERY_INFO r_u;
	WKS_INFO_100 wks100;

	DEBUG(5,("wks_query_info: %d\n", __LINE__));

	create_wks_info_100(&wks100);
	init_wks_r_query_info(&r_u, q_u->switch_value, &wks100, status);

	/* store the response in the SMB stream */
	wks_io_r_query_info("", &r_u, rdata, 0);

	DEBUG(5,("wks_query_info: %d\n", __LINE__));
}

/*******************************************************************
 api_wks_query_info
 ********************************************************************/
static BOOL api_wks_query_info( uint16 vuid, prs_struct *data,
                                    prs_struct *rdata )
{
	WKS_Q_QUERY_INFO q_u;

	/* grab the net share enum */
	wks_io_q_query_info("", &q_u, data, 0);

	/* construct reply.  always indicate success */
	wks_reply_query_info(&q_u, rdata, 0x0);

	return True;
}


/*******************************************************************
 \PIPE\wkssvc commands
 ********************************************************************/
struct api_struct api_wks_cmds[] =
{
	{ "WKS_Q_QUERY_INFO", WKS_QUERY_INFO, api_wks_query_info },
	{ NULL             , 0            , NULL }
};

/*******************************************************************
 receives a wkssvc pipe and responds.
 ********************************************************************/
BOOL api_wkssvc_rpc(pipes_struct *p, prs_struct *data)
{
	return api_rpcTNP(p, "api_wkssvc_rpc", api_wks_cmds, data);
}

