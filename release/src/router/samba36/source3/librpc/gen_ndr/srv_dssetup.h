#include "librpc/gen_ndr/ndr_dssetup.h"
#ifndef __SRV_DSSETUP__
#define __SRV_DSSETUP__
WERROR _dssetup_DsRoleGetPrimaryDomainInformation(struct pipes_struct *p, struct dssetup_DsRoleGetPrimaryDomainInformation *r);
WERROR _dssetup_DsRoleDnsNameToFlatName(struct pipes_struct *p, struct dssetup_DsRoleDnsNameToFlatName *r);
WERROR _dssetup_DsRoleDcAsDc(struct pipes_struct *p, struct dssetup_DsRoleDcAsDc *r);
WERROR _dssetup_DsRoleDcAsReplica(struct pipes_struct *p, struct dssetup_DsRoleDcAsReplica *r);
WERROR _dssetup_DsRoleDemoteDc(struct pipes_struct *p, struct dssetup_DsRoleDemoteDc *r);
WERROR _dssetup_DsRoleGetDcOperationProgress(struct pipes_struct *p, struct dssetup_DsRoleGetDcOperationProgress *r);
WERROR _dssetup_DsRoleGetDcOperationResults(struct pipes_struct *p, struct dssetup_DsRoleGetDcOperationResults *r);
WERROR _dssetup_DsRoleCancel(struct pipes_struct *p, struct dssetup_DsRoleCancel *r);
WERROR _dssetup_DsRoleServerSaveStateForUpgrade(struct pipes_struct *p, struct dssetup_DsRoleServerSaveStateForUpgrade *r);
WERROR _dssetup_DsRoleUpgradeDownlevelServer(struct pipes_struct *p, struct dssetup_DsRoleUpgradeDownlevelServer *r);
WERROR _dssetup_DsRoleAbortDownlevelServerUpgrade(struct pipes_struct *p, struct dssetup_DsRoleAbortDownlevelServerUpgrade *r);
void dssetup_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dssetup_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dssetup_shutdown(void);
#endif /* __SRV_DSSETUP__ */
