/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)

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

/*
 * A function of this type is passed to the '
 * run_rpc_command' wrapper.  Must go before the net_proto.h
 * include
 */

struct cli_state;

#include "../librpc/gen_ndr/lsa.h"

#include "intl.h"
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#if defined(HAVE_GETTEXT) && !defined(__LCLINT__)
#define _(foo) gettext(foo)
#else
#define _(foo) foo
#endif

#define MODULE_NAME "net"

struct net_context {
	const char *opt_requester_name;
	const char *opt_host;
	const char *opt_password;
	const char *opt_user_name;
	bool opt_user_specified;
	const char *opt_workgroup;
	int opt_long_list_entries;
	int opt_reboot;
	int opt_force;
	int opt_stdin;
	int opt_port;
	int opt_verbose;
	int opt_maxusers;
	const char *opt_comment;
	const char *opt_container;
	int opt_flags;
	int opt_timeout;
	int opt_request_timeout;
	const char *opt_target_workgroup;
	int opt_machine_pass;
	int opt_localgroup;
	int opt_domaingroup;
	int do_talloc_report;
	const char *opt_newntname;
	int opt_rid;
	int opt_acls;
	int opt_attrs;
	int opt_timestamps;
	const char *opt_exclude;
	const char *opt_destination;
	int opt_testmode;
	int opt_kerberos;
	int opt_force_full_repl;
	int opt_ccache;
	int opt_single_obj_repl;
	int opt_clean_old_entries;
	const char *opt_db;
	int opt_lock;
	int opt_auto;
	int opt_repair;

	int opt_have_ip;
	struct sockaddr_storage opt_dest_ip;
	bool smb_encrypt;
	struct libnetapi_ctx *netapi_ctx;
	struct messaging_context *msg_ctx;

	bool display_usage;
	void *private_data;
};

struct net_dc_info {
	bool is_dc;
	bool is_pdc;
	bool is_ad;
	bool is_mixed_mode;
	const char *netbios_domain_name;
	const char *dns_domain_name;
	const char *forest_name;
};

#define NET_TRANSPORT_LOCAL 0x01
#define NET_TRANSPORT_RAP   0x02
#define NET_TRANSPORT_RPC   0x04
#define NET_TRANSPORT_ADS   0x08

struct functable {
	const char *funcname;
	int (*fn)(struct net_context *c, int argc, const char **argv);
	int valid_transports;
	const char *description;
	const char *usage;
};

typedef NTSTATUS (*rpc_command_fn)(struct net_context *c,
				const struct dom_sid *,
				const char *,
				struct cli_state *cli,
				struct rpc_pipe_client *,
				TALLOC_CTX *,
				int,
				const char **);

typedef struct copy_clistate {
	TALLOC_CTX *mem_ctx;
	struct cli_state *cli_share_src;
	struct cli_state *cli_share_dst;
	char *cwd;
	uint16 attribute;
	struct net_context *c;
}copy_clistate;

struct rpc_sh_ctx {
	struct cli_state *cli;

	struct dom_sid *domain_sid;
	const char *domain_name;

	const char *whoami;
	const char *thiscmd;
	struct rpc_sh_cmd *cmds;
	struct rpc_sh_ctx *parent;
};

struct rpc_sh_cmd {
	const char *name;
	struct rpc_sh_cmd *(*sub)(struct net_context *c,
				  TALLOC_CTX *mem_ctx,
				  struct rpc_sh_ctx *ctx);
	const struct ndr_syntax_id *interface;
	NTSTATUS (*fn)(struct net_context *c, TALLOC_CTX *mem_ctx,
		       struct rpc_sh_ctx *ctx,
		       struct rpc_pipe_client *pipe_hnd,
		       int argc, const char **argv);
	const char *help;
};

enum netdom_domain_t { ND_TYPE_NT4, ND_TYPE_AD };

/* INCLUDE FILES */

#include "utils/net_proto.h"
#include "utils/net_help_common.h"

/* MACROS & DEFINES */

#define NET_FLAGS_MASTER 			0x00000001
#define NET_FLAGS_DMB 				0x00000002
#define NET_FLAGS_LOCALHOST_DEFAULT_INSANE 	0x00000004 	/* Would it be insane to set 'localhost'
								   as the default remote host for this
								   operation?  For example, localhost
								   is insane for a 'join' operation.  */
#define NET_FLAGS_PDC 				0x00000008	/* PDC only */
#define NET_FLAGS_ANONYMOUS 			0x00000010	/* use an anonymous connection */
#define NET_FLAGS_NO_PIPE 			0x00000020	/* don't open an RPC pipe */
#define NET_FLAGS_SIGN				0x00000040	/* sign RPC connection */
#define NET_FLAGS_SEAL				0x00000080	/* seal RPC connection */
#define NET_FLAGS_TCP				0x00000100	/* use ncacn_ip_tcp */

/* net share operation modes */
#define NET_MODE_SHARE_MIGRATE 1

