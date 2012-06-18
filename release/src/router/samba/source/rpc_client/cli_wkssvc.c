
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison                    1999.
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


#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"

extern int DEBUGLEVEL;

/****************************************************************************
do a WKS Open Policy
****************************************************************************/
BOOL do_wks_query_info(struct cli_state *cli, 
			char *server_name, uint32 switch_value,
			WKS_INFO_100 *wks100)
{
	prs_struct rbuf;
	prs_struct buf; 
	WKS_Q_QUERY_INFO q_o;
	WKS_R_QUERY_INFO r_o;

	if (server_name == 0 || wks100 == NULL)
		return False;

	prs_init(&buf , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rbuf, 0, 4, UNMARSHALL );

	/* create and send a MSRPC command with api WKS_QUERY_INFO */

	DEBUG(4,("WKS Query Info\n"));

	/* store the parameters */
	init_wks_q_query_info(&q_o, server_name, switch_value);

	/* turn parameters into data stream */
	if(!wks_io_q_query_info("", &q_o, &buf, 0)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, WKS_QUERY_INFO, &buf, &rbuf)) {
		prs_mem_free(&buf);
		prs_mem_free(&rbuf);
		return False;
	}

	prs_mem_free(&buf);

	r_o.wks100 = wks100;

	if(!wks_io_r_query_info("", &r_o, &rbuf, 0)) {
		prs_mem_free(&rbuf);
		return False;
	}

	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("WKS_R_QUERY_INFO: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rbuf);
		return False;
	}

	prs_mem_free(&rbuf);

	return True;
}
