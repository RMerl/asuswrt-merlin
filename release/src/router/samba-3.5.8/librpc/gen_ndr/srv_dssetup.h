#include "../librpc/gen_ndr/ndr_dssetup.h"
#ifndef __SRV_DSSETUP__
#define __SRV_DSSETUP__
WERROR _dssetup_DsRoleGetPrimaryDomainInformation(pipes_struct *p, struct dssetup_DsRoleGetPrimaryDomainInformation *r);
WERROR _dssetup_DsRoleDnsNameToFlatName(pipes_struct *p, struct dssetup_DsRoleDnsNameToFlatName *r);
WERROR _dssetup_DsRoleDcAsDc(pipes_struct *p, struct dssetup_DsRoleDcAsDc *r);
WERROR _dssetup_DsRoleDcAsReplica(pipes_struct *p, struct dssetup_DsRoleDcAsReplica *r);
WERROR _dssetup_DsRoleDemoteDc(pipes_struct *p, struct dssetup_DsRoleDemoteDc *r);
WERROR _dssetup_DsRoleGetDcOperationProgress(pipes_struct *p, struct dssetup_DsRoleGetDcOperationProgress *r);
WERROR _dssetup_DsRoleGetDcOperationResults(pipes_struct *p, struct dssetup_DsRoleGetDcOperationResults *r);
WERROR _dssetup_DsRoleCancel(pipes_struct *p, struct dssetup_DsRoleCancel *r);
WERROR _dssetup_DsRoleServerSaveStateForUpgrade(pipes_struct *p, struct dssetup_DsRoleServerSaveStateForUpgrade *r);
WERROR _dssetup_DsRoleUpgradeDownlevelServer(pipes_struct *p, struct dssetup_DsRoleUpgradeDownlevelServer *r);
WERROR _dssetup_DsRoleAbortDownlevelServerUpgrade(pipes_struct *p, struct dssetup_DsRoleAbortDownlevelServerUpgrade *r);
void dssetup_get_pipe_fns(struct api_struct **fns, int *n_fns);
NTSTATUS rpc_dssetup_dispatch(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const struct ndr_interface_table *table, uint32_t opnum, void *r);
WERROR _dssetup_DsRoleGetPrimaryDomainInformation(pipes_struct *p, struct dssetup_DsRoleGetPrimaryDomainInformation *r);
WERROR _dssetup_DsRoleDnsNameToFlatName(pipes_struct *p, struct dssetup_DsRoleDnsNameToFlatName *r);
WERROR _dssetup_DsRoleDcAsDc(pipes_struct *p, struct dssetup_DsRoleDcAsDc *r);
WERROR _dssetup_DsRoleDcAsReplica(pipes_struct *p, struct dssetup_DsRoleDcAsReplica *r);
WERROR _dssetup_DsRoleDemoteDc(pipes_struct *p, struct dssetup_DsRoleDemoteDc *r);
WERROR _dssetup_DsRoleGetDcOperationProgress(pipes_struct *p, struct dssetup_DsRoleGetDcOperationProgress *r);
WERROR _dssetup_DsRoleGetDcOperationResults(pipes_struct *p, struct dssetup_DsRoleGetDcOperationResults *r);
WERROR _dssetup_DsRoleCancel(pipes_struct *p, struct dssetup_DsRoleCancel *r);
WERROR _dssetup_DsRoleServerSaveStateForUpgrade(pipes_struct *p, struct dssetup_DsRoleServerSaveStateForUpgrade *r);
WERROR _dssetup_DsRoleUpgradeDownlevelServer(pipes_struct *p, struct dssetup_DsRoleUpgradeDownlevelServer *r);
WERROR _dssetup_DsRoleAbortDownlevelServerUpgrade(pipes_struct *p, struct dssetup_DsRoleAbortDownlevelServerUpgrade *r);
NTSTATUS rpc_dssetup_init(void);
#endif /* __SRV_DSSETUP__ */
