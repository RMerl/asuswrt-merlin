#include "librpc/gen_ndr/ndr_wkssvc.h"
#ifndef __CLI_WKSSVC__
#define __CLI_WKSSVC__
NTSTATUS rpccli_wkssvc_NetWkstaGetInfo(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, uint32_t level, union wkssvc_NetWkstaInfo *info);
NTSTATUS rpccli_wkssvc_NetWkstaSetInfo(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, uint32_t level, union wkssvc_NetWkstaInfo *info, uint32_t *parm_error);
NTSTATUS rpccli_wkssvc_NetWkstaEnumUsers(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, uint32_t level, union WKS_USER_ENUM_UNION *users, uint32_t prefmaxlen, uint32_t *entriesread, uint32_t *totalentries, uint32_t *resumehandle);
NTSTATUS rpccli_WKSSVC_NETRWKSTAUSERGETINFO(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRWKSTAUSERSETINFO(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_wkssvc_NetWkstaTransportEnum(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, uint32_t *level, union wkssvc_NetWkstaTransportCtr *ctr, uint32_t max_buffer, uint32_t *totalentries, uint32_t *resume_handle);
NTSTATUS rpccli_WKSSVC_NETRWKSTATRANSPORTADD(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRWKSTATRANSPORTDEL(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRUSEADD(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRUSEGETINFO(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRUSEDEL(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRUSEENUM(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRMESSAGEBUFFERSEND(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRWORKSTATIONSTATISTICSGET(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRLOGONDOMAINNAMEADD(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRLOGONDOMAINNAMEDEL(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRJOINDOMAIN(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRUNJOINDOMAIN(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRRENAMEMACHINEINDOMAIN(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRVALIDATENAME(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRGETJOININFORMATION(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRGETJOINABLEOUS(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_wkssvc_NetrJoinDomain2(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, const char *domain_name, const char *account_name, const char *admin_account, struct wkssvc_PasswordBuffer *encrypted_password, uint32_t join_flags);
NTSTATUS rpccli_wkssvc_NetrUnjoinDomain2(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, const char *account, struct wkssvc_PasswordBuffer *encrypted_password, uint32_t unjoin_flags);
NTSTATUS rpccli_wkssvc_NetrRenameMachineInDomain2(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, const char *NewMachineName, const char *Account, struct wkssvc_PasswordBuffer *EncryptedPassword, uint32_t RenameOptions);
NTSTATUS rpccli_WKSSVC_NETRVALIDATENAME2(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRGETJOINABLEOUS2(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_wkssvc_NetrAddAlternateComputerName(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, const char *NewAlternateMachineName, const char *Account, struct wkssvc_PasswordBuffer *EncryptedPassword, uint32_t Reserved);
NTSTATUS rpccli_wkssvc_NetrRemoveAlternateComputerName(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const char *server_name, const char *AlternateMachineNameToRemove, const char *Account, struct wkssvc_PasswordBuffer *EncryptedPassword, uint32_t Reserved);
NTSTATUS rpccli_WKSSVC_NETRSETPRIMARYCOMPUTERNAME(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
NTSTATUS rpccli_WKSSVC_NETRENUMERATECOMPUTERNAMES(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx);
#endif /* __CLI_WKSSVC__ */
