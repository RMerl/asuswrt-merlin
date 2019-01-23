/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2001 Steve French  (sfrench@us.ibm.com)
   Copyright (C) 2001 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)

   Originally written by Steve and Jim. Largely rewritten by tridge in
   November 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "../librpc/gen_ndr/rap.h"
#include "../librpc/gen_ndr/svcctl.h"
#include "utils/net.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"

/* The following messages were for error checking that is not properly
   reported at the moment.  Which should be reinstated? */
#define ERRMSG_TARGET_WG_NOT_VALID      "\nTarget workgroup option not valid "\
					"except on net rap server command, ignored"
#define ERRMSG_INVALID_HELP_OPTION	"\nInvalid help option\n"

#define ERRMSG_BOTH_SERVER_IPADDRESS    "\nTarget server and IP address both "\
  "specified. Do not set both at the same time.  The target IP address was used\n"

static int errmsg_not_implemented(void)
{
	d_printf(_("\nNot implemented\n"));
	return 0;
}

int net_rap_file_usage(struct net_context *c, int argc, const char **argv)
{
	return net_file_usage(c, argc, argv);
}

/***************************************************************************
  list info on an open file
***************************************************************************/
static void file_fn(const char * pPath, const char * pUser, uint16 perms,
		    uint16 locks, uint32 id)
{
	d_printf("%-7.1d %-20.20s 0x%-4.2x %-6.1d %s\n",
		 id, pUser, perms, locks, pPath);
}

static void one_file_fn(const char *pPath, const char *pUser, uint16 perms,
			uint16 locks, uint32 id)
{
	d_printf(_("File ID          %d\n"
		   "User name        %s\n"
		   "Locks            0x%-4.2x\n"
		   "Path             %s\n"
		   "Permissions      0x%x\n"),
		 id, pUser, locks, pPath, perms);
}


static int rap_file_close(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc == 0 || c->display_usage) {
		return net_rap_file_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetFileClose(cli, atoi(argv[0]));
	cli_shutdown(cli);
	return ret;
}

static int rap_file_info(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc == 0 || c->display_usage)
		return net_rap_file_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetFileGetInfo(cli, atoi(argv[0]), one_file_fn);
	cli_shutdown(cli);
	return ret;
}

static int rap_file_user(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage)
		return net_rap_file_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
		return -1;

	/* list open files */

	d_printf(_("\nEnumerating open files on remote server:\n\n"
		   "\nFileId  Opened by            Perms  Locks  Path \n"
		   "------  ---------            -----  -----  ---- \n"));
	ret = cli_NetFileEnum(cli, argv[0], NULL, file_fn);

	if (ret == -1)
		d_printf(_("\nOperation not supported by server!\n\n"));

	cli_shutdown(cli);
	return ret;
}

int net_rap_file(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"close",
			rap_file_close,
			NET_TRANSPORT_RAP,
			N_("Close specified file on server"),
			N_("net rap file close\n"
			   "    Close specified file on server")
		},
		{
			"user",
			rap_file_user,
			NET_TRANSPORT_RAP,
			N_("List all files opened by username"),
			N_("net rap file user\n"
			   "    List all files opened by username")
		},
		{
			"info",
			rap_file_info,
			NET_TRANSPORT_RAP,
			N_("Display info about an opened file"),
			N_("net rap file info\n"
			   "    Display info about an opened file")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		struct cli_state *cli;
		int ret;

		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap file\n"
				   "    List all open files on rempte "
				   "server\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                        return -1;

		/* list open files */

		d_printf(_("\nEnumerating open files on remote server:\n\n"
			   "\nFileId  Opened by            Perms  Locks  Path\n"
			   "------  ---------            -----  -----  ----\n"));
		ret = cli_NetFileEnum(cli, NULL, NULL, file_fn);

		if (ret == -1)
			d_printf(_("\nOperation not supported by server!\n\n"));

		cli_shutdown(cli);
		return ret;
	}

	return net_run_function(c, argc, argv, "net rap file", func);
}

int net_rap_share_usage(struct net_context *c, int argc, const char **argv)
{
	return net_share_usage(c, argc, argv);
}

static void long_share_fn(const char *share_name, uint32 type,
			  const char *comment, void *state)
{
	d_printf("%-12s %-8.8s %-50s\n",
		 share_name, net_share_type_str(type), comment);
}

static void share_fn(const char *share_name, uint32 type,
		     const char *comment, void *state)
{
	d_printf("%s\n", share_name);
}

static int rap_share_delete(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage) {
		return net_rap_share_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetShareDelete(cli, argv[0]);
	cli_shutdown(cli);
	return ret;
}

static int rap_share_add(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	struct rap_share_info_2 sinfo;
	char *p;
	char *sharename;

	if (argc == 0 || c->display_usage) {
		return net_rap_share_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	sharename = SMB_STRDUP(argv[0]);
	p = strchr(sharename, '=');
	if (p == NULL) {
		d_printf(_("Server path not specified\n"));
		SAFE_FREE(sharename);
		return net_rap_share_usage(c, argc, argv);
	}
	*p = 0;
	strlcpy((char *)sinfo.share_name, sharename, sizeof(sinfo.share_name));
	sinfo.reserved1 = '\0';
	sinfo.share_type = 0;
	sinfo.comment = c->opt_comment ? smb_xstrdup(c->opt_comment) : "";
	sinfo.perms = 0;
	sinfo.maximum_users = c->opt_maxusers;
	sinfo.active_users = 0;
	sinfo.path = p+1;
	memset(sinfo.password, '\0', sizeof(sinfo.password));
	sinfo.reserved2 = '\0';

	ret = cli_NetShareAdd(cli, &sinfo);
	cli_shutdown(cli);
	SAFE_FREE(sharename);
	return ret;
}


int net_rap_share(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"delete",
			rap_share_delete,
			NET_TRANSPORT_RAP,
			N_("Delete a share from server"),
			N_("net rap share delete\n"
			   "    Delete a share from server")
		},
		{
			"close",
			rap_share_delete,
			NET_TRANSPORT_RAP,
			N_("Delete a share from server"),
			N_("net rap share close\n"
			   "    Delete a share from server\n"
			   "    Alias for net rap share delete")
		},
		{
			"add",
			rap_share_add,
			NET_TRANSPORT_RAP,
			N_("Add a share to server"),
			N_("net rap share add\n"
			   "    Add a share to server")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		struct cli_state *cli;
		int ret;

		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap share\n"
				   "    List all shares on remote server\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
			return -1;

		if (c->opt_long_list_entries) {
			d_printf(_(
	"\nEnumerating shared resources (exports) on remote server:\n\n"
	"\nShare name   Type     Description\n"
	"----------   ----     -----------\n"));
			ret = cli_RNetShareEnum(cli, long_share_fn, NULL);
		} else {
			ret = cli_RNetShareEnum(cli, share_fn, NULL);
		}
		cli_shutdown(cli);
		return ret;
	}

	return net_run_function(c, argc, argv, "net rap share", func);
}

int net_rap_session_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
	 "\nnet rap session [misc. options] [targets]"
	 "\n\tenumerates all active SMB/CIFS sessions on target server\n"));
	d_printf(_(
	 "\nnet rap session DELETE <client_name> [misc. options] [targets] \n"
	 "\tor"
	 "\nnet rap session CLOSE <client_name> [misc. options] [targets]"
	 "\n\tDeletes (closes) a session from specified client to server\n"));
	d_printf(_(
	"\nnet rap session INFO <client_name>"
	"\n\tEnumerates all open files in specified session\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

static void list_sessions_func(char *wsname, char *username, uint16 conns,
			uint16 opens, uint16 users, uint32 sess_time,
			uint32 idle_time, uint32 user_flags, char *clitype)
{
	int hrs = idle_time / 3600;
	int min = (idle_time / 60) % 60;
	int sec = idle_time % 60;

	d_printf("\\\\%-18.18s %-20.20s %-18.18s %5d %2.2d:%2.2d:%2.2d\n",
		 wsname, username, clitype, opens, hrs, min, sec);
}

static void display_session_func(const char *wsname, const char *username,
				 uint16 conns, uint16 opens, uint16 users,
				 uint32 sess_time, uint32 idle_time,
				 uint32 user_flags, const char *clitype)
{
	int ihrs = idle_time / 3600;
	int imin = (idle_time / 60) % 60;
	int isec = idle_time % 60;
	int shrs = sess_time / 3600;
	int smin = (sess_time / 60) % 60;
	int ssec = sess_time % 60;
	d_printf(_("User name       %-20.20s\n"
		   "Computer        %-20.20s\n"
		   "Guest logon     %-20.20s\n"
		   "Client Type     %-40.40s\n"
		   "Sess time       %2.2d:%2.2d:%2.2d\n"
		   "Idle time       %2.2d:%2.2d:%2.2d\n"),
		 username, wsname,
		 (user_flags&0x0)?_("yes"):_("no"), clitype,
		 shrs, smin, ssec, ihrs, imin, isec);
}

static void display_conns_func(uint16 conn_id, uint16 conn_type, uint16 opens,
			       uint16 users, uint32 conn_time,
			       const char *username, const char *netname)
{
	d_printf("%-14.14s %-8.8s %5d\n",
		 netname, net_share_type_str(conn_type), opens);
}

static int rap_session_info(struct net_context *c, int argc, const char **argv)
{
	const char *sessname;
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage)
                return net_rap_session_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	sessname = argv[0];

	ret = cli_NetSessionGetInfo(cli, sessname, display_session_func);
	if (ret < 0) {
		cli_shutdown(cli);
                return ret;
	}

	d_printf(_("Share name     Type     # Opens\n-------------------------"
		   "-----------------------------------------------------\n"));
	ret = cli_NetConnectionEnum(cli, sessname, display_conns_func);
	cli_shutdown(cli);
	return ret;
}

static int rap_session_delete(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage)
                return net_rap_session_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetSessionDel(cli, argv[0]);
	cli_shutdown(cli);
	return ret;
}

int net_rap_session(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"info",
			rap_session_info,
			NET_TRANSPORT_RAP,
			N_("Display information about session"),
			N_("net rap session info\n"
			   "    Display information about session")
		},
		{
			"delete",
			rap_session_delete,
			NET_TRANSPORT_RAP,
			N_("Close specified session"),
			N_("net rap session delete\n"
			   "    Close specified session\n"
			   "    Alias for net rap session close")
		},
		{
			"close",
			rap_session_delete,
			NET_TRANSPORT_RAP,
			N_("Close specified session"),
			N_("net rap session close\n"
			   "    Close specified session")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		struct cli_state *cli;
		int ret;

		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap session\n"
				   "    List all open sessions on remote "
				   "server\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
			return -1;

		d_printf(_("Computer             User name            "
			   "Client Type        Opens Idle time\n"
			   "------------------------------------------"
			   "------------------------------------\n"));
		ret = cli_NetSessionEnum(cli, list_sessions_func);

		cli_shutdown(cli);
		return ret;
	}

	return net_run_function(c, argc, argv, "net rap session", func);
}

/****************************************************************************
list a server name
****************************************************************************/
static void display_server_func(const char *name, uint32 m,
				const char *comment, void * reserved)
{
	d_printf("\t%-16.16s     %s\n", name, comment);
}

static int net_rap_server_name(struct net_context *c, int argc, const char *argv[])
{
	struct cli_state *cli;
	char *name;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rap server name\n"
			   "    Get the name of the server\n"));
		return 0;
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	if (!cli_get_server_name(NULL, cli, &name)) {
		d_fprintf(stderr, _("cli_get_server_name failed\n"));
		cli_shutdown(cli);
		return -1;
	}

	d_printf(_("Server name = %s\n"), name);

	TALLOC_FREE(name);
	cli_shutdown(cli);
	return 0;
}

static int net_rap_server_domain(struct net_context *c, int argc,
				 const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rap server domain\n"
			   "    Enumerate servers in this domain/workgroup\n"));
		return 0;
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	d_printf(_("\nEnumerating servers in this domain or workgroup: \n\n"
		   "\tServer name          Server description\n"
		   "\t-------------        ----------------------------\n"));

	ret = cli_NetServerEnum(cli, cli->server_domain, SV_TYPE_ALL,
				display_server_func,NULL);
	cli_shutdown(cli);
	return ret;
}

int net_rap_server(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"name",
			net_rap_server_name,
			NET_TRANSPORT_RAP,
			N_("Get the name of the server"),
			N_("net rap server name\n"
			   "    Get the name of the server")
		},
		{
			"domain",
			net_rap_server_domain,
			NET_TRANSPORT_RAP,
			N_("Get the servers in this domain/workgroup"),
			N_("net rap server domain\n"
			   "    Get the servers in this domain/workgroup")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	/* smb4k uses 'net [rap|rpc] server domain' to query servers in a domain */
	/* Fall through for 'domain', any other forms will cause to show usage message */
	return net_run_function(c, argc, argv, "net rap server", func);

}

int net_rap_domain_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net rap domain [misc. options] [target]\n\tlists the"
		   " domains or workgroups visible on the current network\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

int net_rap_domain(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (c->display_usage)
		return net_rap_domain_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	d_printf(_("\nEnumerating domains:\n\n"
		   "\tDomain name          Server name of Browse Master\n"
		   "\t-------------        ----------------------------\n"));

	ret = cli_NetServerEnum(cli, cli->server_domain, SV_TYPE_DOMAIN_ENUM,
				display_server_func,NULL);
	cli_shutdown(cli);
	return ret;
}

int net_rap_printq_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
	 "net rap printq [misc. options] [targets]\n"
	 "\tor\n"
	 "net rap printq info [<queue_name>] [misc. options] [targets]\n"
	 "\tlists the specified queue and jobs on the target server.\n"
	 "\tIf the queue name is not specified, all queues are listed.\n\n"));
	d_printf(_(
	 "net rap printq delete [<queue name>] [misc. options] [targets]\n"
	 "\tdeletes the specified job number on the target server, or the\n"
	 "\tprinter queue if no job number is specified\n"));

	net_common_flags_usage(c, argc, argv);

	return -1;
}

static void enum_queue(const char *queuename, uint16 pri, uint16 start,
		       uint16 until, const char *sep, const char *pproc,
		       const char *dest, const char *qparms,
		       const char *qcomment, uint16 status, uint16 jobcount)
{
	d_printf(_("%-17.17s Queue %5d jobs                      "),
		 queuename, jobcount);

	switch (status) {
	case 0:
		d_printf(_("*Printer Active*\n"));
		break;
	case 1:
		d_printf(_("*Printer Paused*\n"));
		break;
	case 2:
		d_printf(_("*Printer error*\n"));
		break;
	case 3:
		d_printf(_("*Delete Pending*\n"));
		break;
	default:
		d_printf(_("**UNKNOWN STATUS**\n"));
	}
}

static void enum_jobs(uint16 jobid, const char *ownername,
		      const char *notifyname, const char *datatype,
		      const char *jparms, uint16 pos, uint16 status,
		      const char *jstatus, unsigned int submitted, unsigned int jobsize,
		      const char *comment)
{
	d_printf("     %-23.23s %5d %9d            ",
		 ownername, jobid, jobsize);
	switch (status) {
	case 0:
		d_printf(_("Waiting\n"));
		break;
	case 1:
		d_printf(_("Held in queue\n"));
		break;
	case 2:
		d_printf(_("Spooling\n"));
		break;
	case 3:
		d_printf(_("Printing\n"));
		break;
	default:
		d_printf(_("**UNKNOWN STATUS**\n"));
	}
}

#define PRINTQ_ENUM_DISPLAY \
    _("Print queues at \\\\%s\n\n"\
      "Name                         Job #      Size            Status\n\n"\
      "------------------------------------------------------------------"\
      "-------------\n")

static int rap_printq_info(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage)
                return net_rap_printq_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	d_printf(PRINTQ_ENUM_DISPLAY, cli->desthost); /* list header */
	ret = cli_NetPrintQGetInfo(cli, argv[0], enum_queue, enum_jobs);
	cli_shutdown(cli);
	return ret;
}

static int rap_printq_delete(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage)
                return net_rap_printq_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_printjob_del(cli, atoi(argv[0]));
	cli_shutdown(cli);
	return ret;
}

int net_rap_printq(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	struct functable func[] = {
		{
			"info",
			rap_printq_info,
			NET_TRANSPORT_RAP,
			N_("Display info about print queues and jobs"),
			N_("net rap printq info [queue]\n"
			   "    Display info about print jobs in queue.\n"
			   "    If queue is not specified, all queues are "
			   "listed")
		},
		{
			"delete",
			rap_printq_delete,
			NET_TRANSPORT_RAP,
			N_("Delete print job(s)"),
			N_("net rap printq delete\n"
			   "    Delete print job(s)")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap printq\n"
				   "    List the print queue\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
			return -1;

		d_printf(PRINTQ_ENUM_DISPLAY, cli->desthost); /* list header */
		ret = cli_NetPrintQEnum(cli, enum_queue, enum_jobs);
		cli_shutdown(cli);
		return ret;
	}

	return net_run_function(c, argc, argv, "net rap printq", func);
}

static int net_rap_user_usage(struct net_context *c, int argc, const char **argv)
{
	return net_user_usage(c, argc, argv);
}

static void user_fn(const char *user_name, void *state)
{
	d_printf("%-21.21s\n", user_name);
}

static void long_user_fn(const char *user_name, const char *comment,
			 const char * home_dir, const char * logon_script,
			 void *state)
{
	d_printf("%-21.21s %s\n",
		 user_name, comment);
}

static void group_member_fn(const char *user_name, void *state)
{
	d_printf("%-21.21s\n", user_name);
}

static int rap_user_delete(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc == 0 || c->display_usage) {
                return net_rap_user_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetUserDelete(cli, argv[0]);
	cli_shutdown(cli);
	return ret;
}

static int rap_user_add(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	struct rap_user_info_1 userinfo;

	if (argc == 0 || c->display_usage) {
                return net_rap_user_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	safe_strcpy((char *)userinfo.user_name, argv[0], sizeof(userinfo.user_name)-1);
	if (c->opt_flags == 0)
                c->opt_flags = 0x21;

	userinfo.userflags = c->opt_flags;
	userinfo.reserved1 = '\0';
        userinfo.comment = smb_xstrdup(c->opt_comment ? c->opt_comment : "");
	userinfo.priv = 1;
	userinfo.home_dir = NULL;
	userinfo.logon_script = NULL;
	userinfo.passwrd[0] = '\0';

	ret = cli_NetUserAdd(cli, &userinfo);

	cli_shutdown(cli);
	return ret;
}

static int rap_user_info(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc == 0 || c->display_usage) {
                return net_rap_user_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetUserGetGroups(cli, argv[0], group_member_fn, NULL);
	cli_shutdown(cli);
	return ret;
}

int net_rap_user(struct net_context *c, int argc, const char **argv)
{
	int ret = -1;
	struct functable func[] = {
		{
			"add",
			rap_user_add,
			NET_TRANSPORT_RAP,
			N_("Add specified user"),
			N_("net rap user add\n"
			   "    Add specified user")
		},
		{
			"info",
			rap_user_info,
			NET_TRANSPORT_RAP,
			N_("List domain groups of specified user"),
			N_("net rap user info\n"
			   "    List domain groups of specified user")

		},
		{
			"delete",
			rap_user_delete,
			NET_TRANSPORT_RAP,
			N_("Remove specified user"),
			N_("net rap user delete\n"
			   "    Remove specified user")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		struct cli_state *cli;
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap user\n"
				   "    List all users\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                        goto done;
		if (c->opt_long_list_entries) {
			d_printf(_("\nUser name             Comment"
				   "\n-----------------------------\n"));
			ret = cli_RNetUserEnum(cli, long_user_fn, NULL);
			cli_shutdown(cli);
			goto done;
		}
		ret = cli_RNetUserEnum0(cli, user_fn, NULL);
		cli_shutdown(cli);
		goto done;
	}

	ret = net_run_function(c, argc, argv, "net rap user", func);
 done:
	if (ret != 0) {
		DEBUG(1, (_("Net user returned: %d\n"), ret));
	}
	return ret;
}


int net_rap_group_usage(struct net_context *c, int argc, const char **argv)
{
	return net_group_usage(c, argc, argv);
}

static void long_group_fn(const char *group_name, const char *comment,
			  void *state)
{
	d_printf("%-21.21s %s\n", group_name, comment);
}

static void group_fn(const char *group_name, void *state)
{
	d_printf("%-21.21s\n", group_name);
}

static int rap_group_delete(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc == 0 || c->display_usage) {
                return net_rap_group_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetGroupDelete(cli, argv[0]);
	cli_shutdown(cli);
	return ret;
}

static int rap_group_add(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	struct rap_group_info_1 grinfo;

	if (argc == 0 || c->display_usage) {
                return net_rap_group_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	/* BB check for length 21 or smaller explicitly ? BB */
	safe_strcpy((char *)grinfo.group_name, argv[0], sizeof(grinfo.group_name)-1);
	grinfo.reserved1 = '\0';
	grinfo.comment = smb_xstrdup(c->opt_comment ? c->opt_comment : "");

	ret = cli_NetGroupAdd(cli, &grinfo);
	cli_shutdown(cli);
	return ret;
}

int net_rap_group(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			rap_group_add,
			NET_TRANSPORT_RAP,
			N_("Add specified group"),
			N_("net rap group add\n"
			   "    Add specified group")
		},
		{
			"delete",
			rap_group_delete,
			NET_TRANSPORT_RAP,
			N_("Delete specified group"),
			N_("net rap group delete\n"
			   "    Delete specified group")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		struct cli_state *cli;
		int ret;
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap group\n"
				   "    List all groups\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                        return -1;
		if (c->opt_long_list_entries) {
			d_printf(_("Group name            Comment\n"
			           "-----------------------------\n"));
			ret = cli_RNetGroupEnum(cli, long_group_fn, NULL);
			cli_shutdown(cli);
			return ret;
		}
		ret = cli_RNetGroupEnum0(cli, group_fn, NULL);
		cli_shutdown(cli);
		return ret;
	}

	return net_run_function(c, argc, argv, "net rap group", func);
}

int net_rap_groupmember_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
	 "net rap groupmember LIST <group> [misc. options] [targets]"
	 "\n\t Enumerate users in a group\n"
	 "\nnet rap groupmember DELETE <group> <user> [misc. options] "
	 "[targets]\n\t Delete specified user from specified group\n"
	 "\nnet rap groupmember ADD <group> <user> [misc. options] [targets]"
	 "\n\t Add specified user to specified group\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}


static int rap_groupmember_add(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc != 2 || c->display_usage) {
                return net_rap_groupmember_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetGroupAddUser(cli, argv[0], argv[1]);
	cli_shutdown(cli);
	return ret;
}

static int rap_groupmember_delete(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc != 2 || c->display_usage) {
                return net_rap_groupmember_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetGroupDelUser(cli, argv[0], argv[1]);
	cli_shutdown(cli);
	return ret;
}

static int rap_groupmember_list(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;
	if (argc == 0 || c->display_usage) {
                return net_rap_groupmember_usage(c, argc, argv);
	}

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	ret = cli_NetGroupGetUsers(cli, argv[0], group_member_fn, NULL );
	cli_shutdown(cli);
	return ret;
}

int net_rap_groupmember(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"add",
			rap_groupmember_add,
			NET_TRANSPORT_RAP,
			N_("Add specified user to group"),
			N_("net rap groupmember add\n"
			   "    Add specified user to group")
		},
		{
			"list",
			rap_groupmember_list,
			NET_TRANSPORT_RAP,
			N_("List users in group"),
			N_("net rap groupmember list\n"
			   "    List users in group")
		},
		{
			"delete",
			rap_groupmember_delete,
			NET_TRANSPORT_RAP,
			N_("Remove user from group"),
			N_("net rap groupmember delete\n"
			   "    Remove user from group")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rap groupmember", func);
}

int net_rap_validate_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net rap validate <username> [password]\n"
		   "\tValidate user and password to check whether they"
		   " can access target server or domain\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

int net_rap_validate(struct net_context *c, int argc, const char **argv)
{
	return errmsg_not_implemented();
}

int net_rap_service_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net rap service [misc. options] [targets] \n"
		 "\tlists all running service daemons on target server\n"));
	d_printf(_("\nnet rap service START <name> [service startup arguments]"
		 " [misc. options] [targets]"
		 "\n\tStart named service on remote server\n"));
	d_printf(_("\nnet rap service STOP <name> [misc. options] [targets]\n"
		 "\n\tStop named service on remote server\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

static int rap_service_start(struct net_context *c, int argc, const char **argv)
{
	return errmsg_not_implemented();
}

static int rap_service_stop(struct net_context *c, int argc, const char **argv)
{
	return errmsg_not_implemented();
}

static void service_fn(const char *service_name, const char *dummy,
		       void *state)
{
	d_printf("%-21.21s\n", service_name);
}

int net_rap_service(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"start",
			rap_service_start,
			NET_TRANSPORT_RAP,
			N_("Start service on remote server"),
			N_("net rap service start\n"
			   "    Start service on remote server")
		},
		{
			"stop",
			rap_service_stop,
			NET_TRANSPORT_RAP,
			N_("Stop named serve on remote server"),
			N_("net rap service stop\n"
			   "    Stop named serve on remote server")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	if (argc == 0) {
		struct cli_state *cli;
		int ret;
		if (c->display_usage) {
			d_printf(_("Usage:\n"));
			d_printf(_("net rap service\n"
				   "    List services on remote server\n"));
			net_display_usage_from_functable(func);
			return 0;
		}

		if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
			return -1;

		if (c->opt_long_list_entries) {
			d_printf(_("Service name          Comment\n"
		                   "-----------------------------\n"));
			ret = cli_RNetServiceEnum(cli, long_group_fn, NULL);
		}
		ret = cli_RNetServiceEnum(cli, service_fn, NULL);
		cli_shutdown(cli);
		return ret;
	}

	return net_run_function(c, argc, argv, "net rap service", func);
}

int net_rap_password_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
	 "net rap password <user> <oldpwo> <newpw> [misc. options] [target]\n"
	 "\tchanges the password for the specified user at target\n"));

	return -1;
}


int net_rap_password(struct net_context *c, int argc, const char **argv)
{
	struct cli_state *cli;
	int ret;

	if (argc < 3 || c->display_usage)
                return net_rap_password_usage(c, argc, argv);

	if (!NT_STATUS_IS_OK(net_make_ipc_connection(c, 0, &cli)))
                return -1;

	/* BB Add check for password lengths? */
	ret = cli_oem_change_password(cli, argv[0], argv[2], argv[1]);
	cli_shutdown(cli);
	return ret;
}

int net_rap_admin_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
   "net rap admin <remote command> [cmd args [env]] [misc. options] [targets]"
   "\n\texecutes a remote command on an os/2 target server\n"));

	return -1;
}


int net_rap_admin(struct net_context *c, int argc, const char **argv)
{
	return errmsg_not_implemented();
}

/* Entry-point for all the RAP functions. */

int net_rap(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"file",
			net_rap_file,
			NET_TRANSPORT_RAP,
			N_("List open files"),
			N_("net rap file\n"
			   "    List open files")
		},
		{
			"share",
			net_rap_share,
			NET_TRANSPORT_RAP,
			N_("List shares exported by server"),
			N_("net rap share\n"
			   "    List shares exported by server")
		},
		{
			"session",
			net_rap_session,
			NET_TRANSPORT_RAP,
			N_("List open sessions"),
			N_("net rap session\n"
			   "    List open sessions")
		},
		{
			"server",
			net_rap_server,
			NET_TRANSPORT_RAP,
			N_("List servers in workgroup"),
			N_("net rap server\n"
			   "    List servers in domain/workgroup")
		},
		{
			"domain",
			net_rap_domain,
			NET_TRANSPORT_RAP,
			N_("List domains in network"),
			N_("net rap domain\n"
			   "    List domains in network")
		},
		{
			"printq",
			net_rap_printq,
			NET_TRANSPORT_RAP,
			N_("List printer queues on server"),
			N_("net rap printq\n"
			   "    List printer queues on server")
		},
		{
			"user",
			net_rap_user,
			NET_TRANSPORT_RAP,
			N_("List users"),
			N_("net rap user\n"
			   "    List users")
		},
		{
			"group",
			net_rap_group,
			NET_TRANSPORT_RAP,
			N_("List user groups"),
			N_("net rap group\n"
			   "    List user groups")
		},
		{
			"validate",
			net_rap_validate,
			NET_TRANSPORT_RAP,
			N_("Check username/password"),
			N_("net rap validate\n"
			   "    Check username/password")
		},
		{
			"groupmember",
			net_rap_groupmember,
			NET_TRANSPORT_RAP,
			N_("List/modify group memberships"),
			N_("net rap groupmember\n"
			   "    List/modify group memberships")
		},
		{
			"admin",
			net_rap_admin,
			NET_TRANSPORT_RAP,
			N_("Execute commands on remote OS/2"),
			N_("net rap admin\n"
			   "    Execute commands on remote OS/2")
		},
		{
			"service",
			net_rap_service,
			NET_TRANSPORT_RAP,
			N_("Start/stop remote service"),
			N_("net rap service\n"
			   "    Start/stop remote service")
		},
		{
			"password",
			net_rap_password,
			NET_TRANSPORT_RAP,
			N_("Change user password"),
			N_("net rap password\n"
			   "    Change user password")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rap", func);
}

