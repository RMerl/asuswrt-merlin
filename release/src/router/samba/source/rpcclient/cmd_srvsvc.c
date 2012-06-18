/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NT Domain Authentication SMB / MSRPC client
   Copyright (C) Andrew Tridgell 1994-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   
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



#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

#define DEBUG_TESTING

extern struct cli_state *smb_cli;

extern FILE* out_hnd;


/****************************************************************************
server get info query
****************************************************************************/
void cmd_srv_query_info(struct client_info *info)
{
	fstring dest_srv;
	fstring tmp;
	SRV_INFO_CTR ctr;
	uint32 info_level = 101;

	BOOL res = True;

	memset((char *)&ctr, '\0', sizeof(ctr));

	fstrcpy(dest_srv, "\\\\");
	fstrcat(dest_srv, info->dest_host);
	strupper(dest_srv);

	if (next_token(NULL, tmp, NULL, sizeof(tmp)-1))
	{
		info_level = (uint32)strtol(tmp, (char**)NULL, 10);
	}

	DEBUG(4,("cmd_srv_query_info: server:%s info level: %d\n",
				dest_srv, (int)info_level));

	DEBUG(5, ("cmd_srv_query_info: smb_cli->fd:%d\n", smb_cli->fd));

	/* open LSARPC session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SRVSVC) : False;

	/* send info level: receive requested info.  hopefully. */
	res = res ? do_srv_net_srv_get_info(smb_cli,
				dest_srv, info_level, &ctr) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_srv_query_info: query succeeded\n"));

		display_srv_info_ctr(out_hnd, ACTION_HEADER   , &ctr);
		display_srv_info_ctr(out_hnd, ACTION_ENUMERATE, &ctr);
		display_srv_info_ctr(out_hnd, ACTION_FOOTER   , &ctr);
	}
	else
	{
		DEBUG(5,("cmd_srv_query_info: query failed\n"));
	}
}

/****************************************************************************
server enum connections
****************************************************************************/
void cmd_srv_enum_conn(struct client_info *info)
{
	fstring dest_srv;
	fstring qual_srv;
	fstring tmp;
	SRV_CONN_INFO_CTR ctr;
	ENUM_HND hnd;
	uint32 info_level = 0;

	BOOL res = True;

	memset((char *)&ctr, '\0', sizeof(ctr));

	fstrcpy(qual_srv, "\\\\");
	fstrcat(qual_srv, info->myhostname);
	strupper(qual_srv);

	fstrcpy(dest_srv, "\\\\");
	fstrcat(dest_srv, info->dest_host);
	strupper(dest_srv);

	if (next_token(NULL, tmp, NULL, sizeof(tmp)-1))
	{
		info_level = (uint32)strtol(tmp, (char**)NULL, 10);
	}

	DEBUG(4,("cmd_srv_enum_conn: server:%s info level: %d\n",
				dest_srv, (int)info_level));

	DEBUG(5, ("cmd_srv_enum_conn: smb_cli->fd:%d\n", smb_cli->fd));

	/* open srvsvc session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SRVSVC) : False;

	hnd.ptr_hnd = 1;
	hnd.handle = 0;

	/* enumerate connections on server */
	res = res ? do_srv_net_srv_conn_enum(smb_cli,
				dest_srv, qual_srv,
	            info_level, &ctr, 0xffffffff, &hnd) : False;

	if (res)
	{
		display_srv_conn_info_ctr(out_hnd, ACTION_HEADER   , &ctr);
		display_srv_conn_info_ctr(out_hnd, ACTION_ENUMERATE, &ctr);
		display_srv_conn_info_ctr(out_hnd, ACTION_FOOTER   , &ctr);
	}

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_srv_enum_conn: query succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_srv_enum_conn: query failed\n"));
	}
}

/****************************************************************************
server enum shares
****************************************************************************/
void cmd_srv_enum_shares(struct client_info *info)
{
	fstring dest_srv;
	fstring tmp;
	SRV_R_NET_SHARE_ENUM r_o;
	ENUM_HND hnd;
	uint32 info_level = 1;

	BOOL res = True;

	fstrcpy(dest_srv, "\\\\");
	fstrcat(dest_srv, info->dest_host);
	strupper(dest_srv);

	if (next_token(NULL, tmp, NULL, sizeof(tmp)-1))
	{
		info_level = (uint32)strtol(tmp, (char**)NULL, 10);
	}

	DEBUG(4,("cmd_srv_enum_shares: server:%s info level: %d\n",
				dest_srv, (int)info_level));

	DEBUG(5, ("cmd_srv_enum_shares: smb_cli->fd:%d\n", smb_cli->fd));

	/* open srvsvc session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SRVSVC) : False;

	hnd.ptr_hnd = 0;
	hnd.handle = 0;

	/* enumerate shares_files on server */
	res = res ? do_srv_net_srv_share_enum(smb_cli,
				dest_srv, 
	            info_level, &r_o, 0xffffffff, &hnd) : False;

	if (res)
	{
		display_srv_share_info_ctr(out_hnd, ACTION_HEADER   , &r_o.ctr);
		display_srv_share_info_ctr(out_hnd, ACTION_ENUMERATE, &r_o.ctr);
		display_srv_share_info_ctr(out_hnd, ACTION_FOOTER   , &r_o.ctr);
		free_srv_r_net_share_enum(&r_o);
	}

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_srv_enum_shares: query succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_srv_enum_shares: query failed\n"));
	}
}

/****************************************************************************
server enum sessions
****************************************************************************/
void cmd_srv_enum_sess(struct client_info *info)
{
	fstring dest_srv;
	fstring tmp;
	SRV_SESS_INFO_CTR ctr;
	ENUM_HND hnd;
	uint32 info_level = 0;

	BOOL res = True;

	memset((char *)&ctr, '\0', sizeof(ctr));

	fstrcpy(dest_srv, "\\\\");
	fstrcat(dest_srv, info->dest_host);
	strupper(dest_srv);

	if (next_token(NULL, tmp, NULL, sizeof(tmp)-1))
	{
		info_level = (uint32)strtol(tmp, (char**)NULL, 10);
	}

	DEBUG(4,("cmd_srv_enum_sess: server:%s info level: %d\n",
				dest_srv, (int)info_level));

	DEBUG(5, ("cmd_srv_enum_sess: smb_cli->fd:%d\n", smb_cli->fd));

	/* open srvsvc session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SRVSVC) : False;

	hnd.ptr_hnd = 1;
	hnd.handle = 0;

	/* enumerate sessions on server */
	res = res ? do_srv_net_srv_sess_enum(smb_cli,
				dest_srv, NULL, info_level, &ctr, 0x1000, &hnd) : False;

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_srv_enum_sess: query succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_srv_enum_sess: query failed\n"));
	}
}

/****************************************************************************
server enum files
****************************************************************************/
void cmd_srv_enum_files(struct client_info *info)
{
	fstring dest_srv;
	fstring tmp;
	SRV_FILE_INFO_CTR ctr;
	ENUM_HND hnd;
	uint32 info_level = 3;

	BOOL res = True;

	memset((char *)&ctr, '\0', sizeof(ctr));

	fstrcpy(dest_srv, "\\\\");
	fstrcat(dest_srv, info->dest_host);
	strupper(dest_srv);

	if (next_token(NULL, tmp, NULL, sizeof(tmp)-1))
	{
		info_level = (uint32)strtol(tmp, (char**)NULL, 10);
	}

	DEBUG(4,("cmd_srv_enum_files: server:%s info level: %d\n",
				dest_srv, (int)info_level));

	DEBUG(5, ("cmd_srv_enum_files: smb_cli->fd:%d\n", smb_cli->fd));

	/* open srvsvc session. */
	res = res ? cli_nt_session_open(smb_cli, PIPE_SRVSVC) : False;

	hnd.ptr_hnd = 1;
	hnd.handle = 0;

	/* enumerate files on server */
	res = res ? do_srv_net_srv_file_enum(smb_cli,
				dest_srv, NULL, info_level, &ctr, 0x1000, &hnd) : False;


	if (res)
	{
		display_srv_file_info_ctr(out_hnd, ACTION_HEADER   , &ctr);
		display_srv_file_info_ctr(out_hnd, ACTION_ENUMERATE, &ctr);
		display_srv_file_info_ctr(out_hnd, ACTION_FOOTER   , &ctr);
	}

	/* close the session */
	cli_nt_session_close(smb_cli);

	if (res)
	{
		DEBUG(5,("cmd_srv_enum_files: query succeeded\n"));
	}
	else
	{
		DEBUG(5,("cmd_srv_enum_files: query failed\n"));
	}
}

