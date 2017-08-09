#include "librpc/gen_ndr/ndr_efs.h"
#ifndef __SRV_EFS__
#define __SRV_EFS__
WERROR _EfsRpcOpenFileRaw(struct pipes_struct *p, struct EfsRpcOpenFileRaw *r);
WERROR _EfsRpcReadFileRaw(struct pipes_struct *p, struct EfsRpcReadFileRaw *r);
WERROR _EfsRpcWriteFileRaw(struct pipes_struct *p, struct EfsRpcWriteFileRaw *r);
void _EfsRpcCloseRaw(struct pipes_struct *p, struct EfsRpcCloseRaw *r);
WERROR _EfsRpcEncryptFileSrv(struct pipes_struct *p, struct EfsRpcEncryptFileSrv *r);
WERROR _EfsRpcDecryptFileSrv(struct pipes_struct *p, struct EfsRpcDecryptFileSrv *r);
WERROR _EfsRpcQueryUsersOnFile(struct pipes_struct *p, struct EfsRpcQueryUsersOnFile *r);
WERROR _EfsRpcQueryRecoveryAgents(struct pipes_struct *p, struct EfsRpcQueryRecoveryAgents *r);
WERROR _EfsRpcRemoveUsersFromFile(struct pipes_struct *p, struct EfsRpcRemoveUsersFromFile *r);
WERROR _EfsRpcAddUsersToFile(struct pipes_struct *p, struct EfsRpcAddUsersToFile *r);
WERROR _EfsRpcSetFileEncryptionKey(struct pipes_struct *p, struct EfsRpcSetFileEncryptionKey *r);
WERROR _EfsRpcNotSupported(struct pipes_struct *p, struct EfsRpcNotSupported *r);
WERROR _EfsRpcFileKeyInfo(struct pipes_struct *p, struct EfsRpcFileKeyInfo *r);
WERROR _EfsRpcDuplicateEncryptionInfoFile(struct pipes_struct *p, struct EfsRpcDuplicateEncryptionInfoFile *r);
void efs_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_efs_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_efs_shutdown(void);
#endif /* __SRV_EFS__ */
