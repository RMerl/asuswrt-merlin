/*
 * Unix SMB/CIFS implementation.
 * collected prototypes header
 *
 * frozen from "make proto" in May 2008
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NET_PROTO_H_
#define _NET_PROTO_H_

#include "libads/ads_status.h"

/* The following definitions come from utils/net.c  */

enum netr_SchannelType get_sec_channel_type(const char *param);

/* The following definitions come from utils/net_ads.c  */
struct ads_struct;
ADS_STATUS ads_startup(struct net_context *c, bool only_own_domain, struct ads_struct **ads);
ADS_STATUS ads_startup_nobind(struct net_context *c, bool only_own_domain, struct ads_struct **ads);
int net_ads_check_our_domain(struct net_context *c);
int net_ads_check(struct net_context *c);
int net_ads_user(struct net_context *c, int argc, const char **argv);
int net_ads_group(struct net_context *c, int argc, const char **argv);
int net_ads_testjoin(struct net_context *c, int argc, const char **argv);
int net_ads_join(struct net_context *c, int argc, const char **argv);
int net_ads_printer_usage(struct net_context *c, int argc, const char **argv);
int net_ads_changetrustpw(struct net_context *c, int argc, const char **argv);
int net_ads_keytab(struct net_context *c, int argc, const char **argv);
int net_ads_kerberos(struct net_context *c, int argc, const char **argv);
int net_ads(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_ads_gpo.c  */

int net_ads_gpo(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_cache.c  */

int net_cache(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_conf.c  */

int net_conf(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_dns.c  */

int get_my_ip_address( struct sockaddr_storage **pp_ss );

/* The following definitions come from utils/net_dom.c  */

int net_dom_usage(struct net_context *c, int argc, const char **argv);
int net_dom(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_file.c  */

int net_file_usage(struct net_context *c, int argc, const char **argv);
int net_file(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_group.c  */

int net_group_usage(struct net_context *c, int argc, const char **argv);
int net_group(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_groupmap.c  */

int net_groupmap_usage(struct net_context *c, int argc, const char **argv);
int net_groupmap(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_help.c  */

int net_help(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_idmap.c  */

int net_idmap(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_join.c  */

int net_join_usage(struct net_context *c, int argc, const char **argv);
int net_join(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_lookup.c  */

int net_lookup_usage(struct net_context *c, int argc, const char **argv);
int net_lookup(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rap.c  */

int net_rap_file_usage(struct net_context *c, int argc, const char **argv);
int net_rap_file(struct net_context *c, int argc, const char **argv);
int net_rap_share_usage(struct net_context *c, int argc, const char **argv);
int net_rap_share(struct net_context *c, int argc, const char **argv);
int net_rap_session_usage(struct net_context *c, int argc, const char **argv);
int net_rap_session(struct net_context *c, int argc, const char **argv);
int net_rap_server_usage(struct net_context *c, int argc, const char **argv);
int net_rap_server(struct net_context *c, int argc, const char **argv);
int net_rap_domain_usage(struct net_context *c, int argc, const char **argv);
int net_rap_domain(struct net_context *c, int argc, const char **argv);
int net_rap_printq_usage(struct net_context *c, int argc, const char **argv);
int net_rap_printq(struct net_context *c, int argc, const char **argv);
int net_rap_user(struct net_context *c, int argc, const char **argv);
int net_rap_group_usage(struct net_context *c, int argc, const char **argv);
int net_rap_group(struct net_context *c, int argc, const char **argv);
int net_rap_groupmember_usage(struct net_context *c, int argc, const char **argv);
int net_rap_groupmember(struct net_context *c, int argc, const char **argv);
int net_rap_validate_usage(struct net_context *c, int argc, const char **argv);
int net_rap_validate(struct net_context *c, int argc, const char **argv);
int net_rap_service_usage(struct net_context *c, int argc, const char **argv);
int net_rap_service(struct net_context *c, int argc, const char **argv);
int net_rap_password_usage(struct net_context *c, int argc, const char **argv);
int net_rap_password(struct net_context *c, int argc, const char **argv);
int net_rap_admin_usage(struct net_context *c, int argc, const char **argv);
int net_rap_admin(struct net_context *c, int argc, const char **argv);
int net_rap(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_registry.c  */

int net_registry(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc.c  */

NTSTATUS net_get_remote_domain_sid(struct cli_state *cli, TALLOC_CTX *mem_ctx,
				   struct dom_sid **domain_sid,
				   const char **domain_name);
int run_rpc_command(struct net_context *c,
			struct cli_state *cli_arg,
			const struct ndr_syntax_id *interface,
			int conn_flags,
			rpc_command_fn fn,
			int argc,
			const char **argv);
int net_rpc_changetrustpw(struct net_context *c, int argc, const char **argv);
int net_rpc_join(struct net_context *c, int argc, const char **argv);
NTSTATUS rpc_info_internals(struct net_context *c,
			const struct dom_sid *domain_sid,
			const char *domain_name,
			struct cli_state *cli,
			struct rpc_pipe_client *pipe_hnd,
			TALLOC_CTX *mem_ctx,
			int argc,
			const char **argv);
int net_rpc_info(struct net_context *c, int argc, const char **argv);
int net_rpc_getsid(struct net_context *c, int argc, const char **argv);
int net_rpc_user(struct net_context *c, int argc, const char **argv);
struct rpc_sh_cmd *net_rpc_user_edit_cmds(struct net_context *c,
					  TALLOC_CTX *mem_ctx,
					  struct rpc_sh_ctx *ctx);
struct rpc_sh_cmd *net_rpc_user_cmds(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx);
int net_rpc_group(struct net_context *c, int argc, const char **argv);
bool copy_top_level_perms(struct net_context *c,
				struct copy_clistate *cp_clistate,
				const char *sharename);
int net_usersidlist(struct net_context *c, int argc, const char **argv);
int net_usersidlist_usage(struct net_context *c, int argc, const char **argv);
int net_rpc_share(struct net_context *c, int argc, const char **argv);
struct rpc_sh_cmd *net_rpc_share_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				      struct rpc_sh_ctx *ctx);
int net_rpc_file(struct net_context *c, int argc, const char **argv);
NTSTATUS rpc_init_shutdown_internals(struct net_context *c,
				     const struct dom_sid *domain_sid,
				     const char *domain_name,
				     struct cli_state *cli,
				     struct rpc_pipe_client *pipe_hnd,
				     TALLOC_CTX *mem_ctx,
				     int argc,
				     const char **argv);
NTSTATUS rpc_reg_shutdown_internals(struct net_context *c,
				    const struct dom_sid *domain_sid,
				    const char *domain_name,
				    struct cli_state *cli,
				    struct rpc_pipe_client *pipe_hnd,
				    TALLOC_CTX *mem_ctx,
				    int argc,
				    const char **argv);
bool net_rpc_check(struct net_context *c, unsigned flags);
int rpc_printer_migrate(struct net_context *c, int argc, const char **argv);
int rpc_printer_usage(struct net_context *c, int argc, const char **argv);
int net_rpc_printer(struct net_context *c, int argc, const char **argv);
int net_rpc(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_audit.c  */

int net_rpc_audit(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_join.c  */

NTSTATUS net_rpc_join_ok(struct net_context *c, const char *domain,
			 const char *server, struct sockaddr_storage *pss);
int net_rpc_join_newstyle(struct net_context *c, int argc, const char **argv);
int net_rpc_testjoin(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_printer.c  */

NTSTATUS net_copy_fileattr(struct net_context *c,
		  TALLOC_CTX *mem_ctx,
		  struct cli_state *cli_share_src,
		  struct cli_state *cli_share_dst,
		  const char *src_name, const char *dst_name,
		  bool copy_acls, bool copy_attrs,
		  bool copy_timestamps, bool is_file);
NTSTATUS net_copy_file(struct net_context *c,
		       TALLOC_CTX *mem_ctx,
		       struct cli_state *cli_share_src,
		       struct cli_state *cli_share_dst,
		       const char *src_name, const char *dst_name,
		       bool copy_acls, bool copy_attrs,
		       bool copy_timestamps, bool is_file);
NTSTATUS rpc_printer_list_internals(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv);
NTSTATUS rpc_printer_driver_list_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_publish_publish_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_publish_unpublish_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_publish_update_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_publish_list_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_migrate_security_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_migrate_forms_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_migrate_drivers_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_migrate_printers_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);
NTSTATUS rpc_printer_migrate_settings_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv);

/* The following definitions come from utils/net_rpc_registry.c  */

int net_rpc_registry(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_rights.c  */

int net_rpc_rights(struct net_context *c, int argc, const char **argv);
struct rpc_sh_cmd *net_rpc_rights_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				       struct rpc_sh_ctx *ctx);

/* The following definitions come from utils/net_rpc_samsync.c  */

NTSTATUS rpc_samdump_internals(struct net_context *c,
				const struct dom_sid *domain_sid,
				const char *domain_name,
				struct cli_state *cli,
				struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				int argc,
				const char **argv);
int rpc_vampire_usage(struct net_context *c, int argc, const char **argv);
int rpc_vampire_passdb(struct net_context *c, int argc, const char **argv);
int rpc_vampire_ldif(struct net_context *c, int argc, const char **argv);
int rpc_vampire_keytab(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_service.c  */

const char *svc_status_string( uint32 state );
int net_rpc_service(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_sh_acct.c  */

struct rpc_sh_cmd *net_rpc_acct_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx);

/* The following definitions come from utils/net_rpc_shell.c  */

int net_rpc_shell(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_sam.c  */

int net_sam(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_share.c  */

int net_share_usage(struct net_context *c, int argc, const char **argv);
int net_share(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_status.c  */

int net_status_usage(struct net_context *c, int argc, const char **argv);
int net_status(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_time.c  */

int net_time_usage(struct net_context *c, int argc, const char **argv);
int net_time(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_user.c  */

int net_user_usage(struct net_context *c, int argc, const char **argv);
int net_user(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_usershare.c  */

int net_usershare_usage(struct net_context *c, int argc, const char **argv);
int net_usershare_help(struct net_context *c, int argc, const char **argv);
int net_usershare(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_eventlog.c  */

int net_eventlog(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_printing.c  */

int net_printing(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_serverid.c  */

int net_serverid(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_util.c  */

NTSTATUS net_rpc_lookup_name(struct net_context *c,
			     TALLOC_CTX *mem_ctx, struct cli_state *cli,
			     const char *name, const char **ret_domain,
			     const char **ret_name, struct dom_sid *ret_sid,
			     enum lsa_SidType *ret_type);
NTSTATUS connect_to_service(struct net_context *c,
					struct cli_state **cli_ctx,
					struct sockaddr_storage *server_ss,
					const char *server_name,
					const char *service_name,
					const char *service_type);
NTSTATUS connect_to_ipc(struct net_context *c,
			struct cli_state **cli_ctx,
			struct sockaddr_storage *server_ss,
			const char *server_name);
NTSTATUS connect_to_ipc_anonymous(struct net_context *c,
				struct cli_state **cli_ctx,
				struct sockaddr_storage *server_ss,
				const char *server_name);
NTSTATUS connect_to_ipc_krb5(struct net_context *c,
			struct cli_state **cli_ctx,
			struct sockaddr_storage *server_ss,
			const char *server_name);
NTSTATUS connect_dst_pipe(struct net_context *c, struct cli_state **cli_dst,
			  struct rpc_pipe_client **pp_pipe_hnd,
			  const struct ndr_syntax_id *interface);
int net_use_krb_machine_account(struct net_context *c);
int net_use_machine_account(struct net_context *c);
bool net_find_server(struct net_context *c,
			const char *domain,
			unsigned flags,
			struct sockaddr_storage *server_ss,
			char **server_name);
bool net_find_pdc(struct sockaddr_storage *server_ss,
		fstring server_name,
		const char *domain_name);
NTSTATUS net_make_ipc_connection(struct net_context *c, unsigned flags,
				 struct cli_state **pcli);
NTSTATUS net_make_ipc_connection_ex(struct net_context *c ,const char *domain,
				    const char *server,
				    struct sockaddr_storage *pss,
				    unsigned flags, struct cli_state **pcli);
const char *net_prompt_pass(struct net_context *c, const char *user);
int net_run_function(struct net_context *c, int argc, const char **argv,
		      const char *whoami, struct functable *table);
void net_display_usage_from_functable(struct functable *table);

const char *net_share_type_str(int num_type);

NTSTATUS net_scan_dc(struct net_context *c,
		     struct cli_state *cli,
		     struct net_dc_info *dc_info);

/* The following definitions come from utils/netlookup.c  */

NTSTATUS net_lookup_name_from_sid(struct net_context *c,
				TALLOC_CTX *ctx,
				struct dom_sid *psid,
				const char **ppdomain,
				const char **ppname);
NTSTATUS net_lookup_sid_from_name(struct net_context *c, TALLOC_CTX *ctx,
				  const char *full_name, struct dom_sid *pret_sid);

/* The following definitions come from utils/passwd_util.c  */

char *stdin_new_passwd( void);
char *get_pass( const char *prompt, bool stdin_get);

/* The following definitions come from utils/net_g_lock.c  */
int net_g_lock(struct net_context *c, int argc, const char **argv);

/* The following definitions come from utils/net_rpc_trust.c  */
int net_rpc_trust(struct net_context *c, int argc, const char **argv);

#endif /*  _NET_PROTO_H_  */
