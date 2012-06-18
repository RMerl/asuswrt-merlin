#ifndef _LIBCLI_AUTH_SCHANNEL_STATE_PROTO_H__
#define _LIBCLI_AUTH_SCHANNEL_STATE_PROTO_H__

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)

/* this file contains prototypes for functions that are private 
 * to this subsystem or library. These functions should not be 
 * used outside this particular subsystem! */


/* The following definitions come from /home/jeremy/src/samba/git/master/source3/../source4/../libcli/auth/schannel_state.c  */

NTSTATUS schannel_store_session_key_ldb(struct ldb_context *ldb,
					TALLOC_CTX *mem_ctx,
					struct netlogon_creds_CredentialState *creds);
NTSTATUS schannel_fetch_session_key_ldb(struct ldb_context *ldb,
					TALLOC_CTX *mem_ctx,
					const char *computer_name,
					struct netlogon_creds_CredentialState **creds);
NTSTATUS schannel_creds_server_step_check_ldb(struct ldb_context *ldb,
					      TALLOC_CTX *mem_ctx,
					      const char *computer_name,
					      bool schannel_required_for_call,
					      bool schannel_in_use,
					      struct netr_Authenticator *received_authenticator,
					      struct netr_Authenticator *return_authenticator,
					      struct netlogon_creds_CredentialState **creds_out);
NTSTATUS schannel_store_session_key_tdb(struct tdb_context *tdb,
					TALLOC_CTX *mem_ctx,
					struct netlogon_creds_CredentialState *creds);
NTSTATUS schannel_fetch_session_key_tdb(struct tdb_context *tdb,
					TALLOC_CTX *mem_ctx,
					const char *computer_name,
					struct netlogon_creds_CredentialState **creds);
NTSTATUS schannel_creds_server_step_check_tdb(struct tdb_context *tdb,
					      TALLOC_CTX *mem_ctx,
					      const char *computer_name,
					      bool schannel_required_for_call,
					      bool schannel_in_use,
					      struct netr_Authenticator *received_authenticator,
					      struct netr_Authenticator *return_authenticator,
					      struct netlogon_creds_CredentialState **creds_out);

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif
