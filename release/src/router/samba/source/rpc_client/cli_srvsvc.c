
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
do a server net conn enum
****************************************************************************/
BOOL do_srv_net_srv_conn_enum(struct cli_state *cli,
			char *server_name, char *qual_name,
			uint32 switch_value, SRV_CONN_INFO_CTR *ctr,
			uint32 preferred_len,
			ENUM_HND *hnd)
{
	prs_struct data; 
	prs_struct rdata;
	SRV_Q_NET_CONN_ENUM q_o;
	SRV_R_NET_CONN_ENUM r_o;

	if (server_name == NULL || ctr == NULL || preferred_len == 0)
		return False;

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* create and send a MSRPC command with api SRV_NETCONNENUM */

	DEBUG(4,("SRV Net Server Connection Enum(%s, %s), level %d, enum:%8x\n",
				server_name, qual_name, switch_value, get_enum_hnd(hnd)));
				
	ctr->switch_value = switch_value;
	ctr->ptr_conn_ctr = 1;
	ctr->conn.info0.num_entries_read = 0;
	ctr->conn.info0.ptr_conn_info    = 1;

	/* store the parameters */
	init_srv_q_net_conn_enum(&q_o, server_name, qual_name,
	                         switch_value, ctr,
	                         preferred_len,
	                         hnd);

	/* turn parameters into data stream */
	if(!srv_io_q_net_conn_enum("", &q_o, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if(!rpc_api_pipe_req(cli, SRV_NETCONNENUM, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	r_o.ctr = ctr;

	if(!srv_io_r_net_conn_enum("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SRV_R_NET_SRV_CONN_ENUM: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.ctr->switch_value != switch_value) {
		/* different switch levels.  oops. */
		DEBUG(0,("SRV_R_NET_SRV_CONN_ENUM: info class %d does not match request %d\n",
			r_o.ctr->switch_value, switch_value));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);
	
	return True;
}

/****************************************************************************
do a server net sess enum
****************************************************************************/

BOOL do_srv_net_srv_sess_enum(struct cli_state *cli,
			char *server_name, char *qual_name,
			uint32 switch_value, SRV_SESS_INFO_CTR *ctr,
			uint32 preferred_len,
			ENUM_HND *hnd)
{
	prs_struct data; 
	prs_struct rdata;
	SRV_Q_NET_SESS_ENUM q_o;
	SRV_R_NET_SESS_ENUM r_o;

	if (server_name == NULL || ctr == NULL || preferred_len == 0)
		return False;

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* create and send a MSRPC command with api SRV_NETSESSENUM */

	DEBUG(4,("SRV Net Session Enum (%s), level %d, enum:%8x\n",
				server_name, switch_value, get_enum_hnd(hnd)));
				
	ctr->switch_value = switch_value;
	ctr->ptr_sess_ctr = 1;
	ctr->sess.info0.num_entries_read = 0;
	ctr->sess.info0.ptr_sess_info    = 1;

	/* store the parameters */
	init_srv_q_net_sess_enum(&q_o, server_name, qual_name,
	                         switch_value, ctr,
	                         preferred_len,
	                         hnd);

	/* turn parameters into data stream */
	if(!srv_io_q_net_sess_enum("", &q_o, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SRV_NETSESSENUM, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	r_o.ctr = ctr;

	if(!srv_io_r_net_sess_enum("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SRV_R_NET_SRV_SESS_ENUM: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.ctr->switch_value != switch_value) {
		/* different switch levels.  oops. */
		DEBUG(0,("SRV_R_NET_SRV_SESS_ENUM: info class %d does not match request %d\n",
			r_o.ctr->switch_value, switch_value));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);
	
	return True;
}

/****************************************************************************
do a server net share enum
****************************************************************************/
BOOL do_srv_net_srv_share_enum(struct cli_state *cli,
			char *server_name, 
			uint32 switch_value, SRV_R_NET_SHARE_ENUM *r_o,
			uint32 preferred_len, ENUM_HND *hnd)
{
	prs_struct data; 
	prs_struct rdata;
	SRV_Q_NET_SHARE_ENUM q_o;

	if (server_name == NULL || preferred_len == 0)
		return False;

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* create and send a MSRPC command with api SRV_NETSHAREENUM */

	DEBUG(4,("SRV Get Share Info (%s), level %d, enum:%8x\n",
				server_name, switch_value, get_enum_hnd(hnd)));
				
	/* store the parameters */
	init_srv_q_net_share_enum(&q_o, server_name, switch_value,
				  preferred_len, hnd);

	/* turn parameters into data stream */
	if(!srv_io_q_net_share_enum("", &q_o, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SRV_NETSHAREENUM, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	if(!srv_io_r_net_share_enum("", r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o->status != 0) {
		/* report error code */
		DEBUG(0,("SRV_R_NET_SHARE_ENUM: %s\n", get_nt_error_msg(r_o->status)));
		prs_mem_free(&rdata);
		free_srv_r_net_share_enum(r_o);
		return False;
	}

	if (r_o->ctr.switch_value != switch_value) {
		/* different switch levels.  oops. */
		DEBUG(0,("SRV_R_NET_SHARE_ENUM: info class %d does not match request %d\n",
			r_o->ctr.switch_value, switch_value));
		prs_mem_free(&rdata);
		free_srv_r_net_share_enum(r_o);
		return False;
	}

	prs_mem_free(&rdata);
	
	return True;
}

/****************************************************************************
do a server net file enum
****************************************************************************/

BOOL do_srv_net_srv_file_enum(struct cli_state *cli,
			char *server_name, char *qual_name,
			uint32 switch_value, SRV_FILE_INFO_CTR *ctr,
			uint32 preferred_len,
			ENUM_HND *hnd)
{
	prs_struct data; 
	prs_struct rdata;
	SRV_Q_NET_FILE_ENUM q_o;
	SRV_R_NET_FILE_ENUM r_o;

	if (server_name == NULL || ctr == NULL || preferred_len == 0)
		return False;

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* create and send a MSRPC command with api SRV_NETFILEENUM */

	DEBUG(4,("SRV Get File Info (%s), level %d, enum:%8x\n",
				server_name, switch_value, get_enum_hnd(hnd)));
				
	q_o.file_level = switch_value;

	ctr->switch_value = switch_value;
	ctr->ptr_file_ctr = 1;
	ctr->file.info3.num_entries_read = 0;
	ctr->file.info3.ptr_file_info    = 1;

	/* store the parameters */
	init_srv_q_net_file_enum(&q_o, server_name, qual_name,
	                         switch_value, ctr,
	                         preferred_len,
	                         hnd);

	/* turn parameters into data stream */
	if(!srv_io_q_net_file_enum("", &q_o, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SRV_NETFILEENUM, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	r_o.ctr = ctr;

	if(!srv_io_r_net_file_enum("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}
		
	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SRV_R_NET_FILE_ENUM: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.ctr->switch_value != switch_value) {
		/* different switch levels.  oops. */
		DEBUG(0,("SRV_R_NET_FILE_ENUM: info class %d does not match request %d\n",
			r_o.ctr->switch_value, switch_value));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);
	
	return True;
}

/****************************************************************************
do a server get info 
****************************************************************************/
BOOL do_srv_net_srv_get_info(struct cli_state *cli,
			char *server_name, uint32 switch_value, SRV_INFO_CTR *ctr)
{
	prs_struct data; 
	prs_struct rdata;
	SRV_Q_NET_SRV_GET_INFO q_o;
	SRV_R_NET_SRV_GET_INFO r_o;

	if (server_name == NULL || switch_value == 0 || ctr == NULL)
		return False;

	prs_init(&data , MAX_PDU_FRAG_LEN, 4, MARSHALL);
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* create and send a MSRPC command with api SRV_NET_SRV_GET_INFO */

	DEBUG(4,("SRV Get Server Info (%s), level %d\n", server_name, switch_value));

	/* store the parameters */
	init_srv_q_net_srv_get_info(&q_o, server_name, switch_value);

	/* turn parameters into data stream */
	if(!srv_io_q_net_srv_get_info("", &q_o, &data, 0)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	/* send the data on \PIPE\ */
	if (!rpc_api_pipe_req(cli, SRV_NET_SRV_GET_INFO, &data, &rdata)) {
		prs_mem_free(&data);
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&data);

	r_o.ctr = ctr;

	if(!srv_io_r_net_srv_get_info("", &r_o, &rdata, 0)) {
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.status != 0) {
		/* report error code */
		DEBUG(0,("SRV_R_NET_SRV_GET_INFO: %s\n", get_nt_error_msg(r_o.status)));
		prs_mem_free(&rdata);
		return False;
	}

	if (r_o.ctr->switch_value != q_o.switch_value) {
		/* different switch levels.  oops. */
		DEBUG(0,("SRV_R_NET_SRV_GET_INFO: info class %d does not match request %d\n",
			r_o.ctr->switch_value, q_o.switch_value));
		prs_mem_free(&rdata);
		return False;
	}

	prs_mem_free(&rdata);
	
	return True;
}
