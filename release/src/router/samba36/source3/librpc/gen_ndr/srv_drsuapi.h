#include "librpc/gen_ndr/ndr_drsuapi.h"
#ifndef __SRV_DRSUAPI__
#define __SRV_DRSUAPI__
WERROR _drsuapi_DsBind(struct pipes_struct *p, struct drsuapi_DsBind *r);
WERROR _drsuapi_DsUnbind(struct pipes_struct *p, struct drsuapi_DsUnbind *r);
WERROR _drsuapi_DsReplicaSync(struct pipes_struct *p, struct drsuapi_DsReplicaSync *r);
WERROR _drsuapi_DsGetNCChanges(struct pipes_struct *p, struct drsuapi_DsGetNCChanges *r);
WERROR _drsuapi_DsReplicaUpdateRefs(struct pipes_struct *p, struct drsuapi_DsReplicaUpdateRefs *r);
WERROR _drsuapi_DsReplicaAdd(struct pipes_struct *p, struct drsuapi_DsReplicaAdd *r);
WERROR _drsuapi_DsReplicaDel(struct pipes_struct *p, struct drsuapi_DsReplicaDel *r);
WERROR _drsuapi_DsReplicaMod(struct pipes_struct *p, struct drsuapi_DsReplicaMod *r);
WERROR _DRSUAPI_VERIFY_NAMES(struct pipes_struct *p, struct DRSUAPI_VERIFY_NAMES *r);
WERROR _drsuapi_DsGetMemberships(struct pipes_struct *p, struct drsuapi_DsGetMemberships *r);
WERROR _DRSUAPI_INTER_DOMAIN_MOVE(struct pipes_struct *p, struct DRSUAPI_INTER_DOMAIN_MOVE *r);
WERROR _drsuapi_DsGetNT4ChangeLog(struct pipes_struct *p, struct drsuapi_DsGetNT4ChangeLog *r);
WERROR _drsuapi_DsCrackNames(struct pipes_struct *p, struct drsuapi_DsCrackNames *r);
WERROR _drsuapi_DsWriteAccountSpn(struct pipes_struct *p, struct drsuapi_DsWriteAccountSpn *r);
WERROR _drsuapi_DsRemoveDSServer(struct pipes_struct *p, struct drsuapi_DsRemoveDSServer *r);
WERROR _DRSUAPI_REMOVE_DS_DOMAIN(struct pipes_struct *p, struct DRSUAPI_REMOVE_DS_DOMAIN *r);
WERROR _drsuapi_DsGetDomainControllerInfo(struct pipes_struct *p, struct drsuapi_DsGetDomainControllerInfo *r);
WERROR _drsuapi_DsAddEntry(struct pipes_struct *p, struct drsuapi_DsAddEntry *r);
WERROR _drsuapi_DsExecuteKCC(struct pipes_struct *p, struct drsuapi_DsExecuteKCC *r);
WERROR _drsuapi_DsReplicaGetInfo(struct pipes_struct *p, struct drsuapi_DsReplicaGetInfo *r);
WERROR _DRSUAPI_ADD_SID_HISTORY(struct pipes_struct *p, struct DRSUAPI_ADD_SID_HISTORY *r);
WERROR _drsuapi_DsGetMemberships2(struct pipes_struct *p, struct drsuapi_DsGetMemberships2 *r);
WERROR _DRSUAPI_REPLICA_VERIFY_OBJECTS(struct pipes_struct *p, struct DRSUAPI_REPLICA_VERIFY_OBJECTS *r);
WERROR _DRSUAPI_GET_OBJECT_EXISTENCE(struct pipes_struct *p, struct DRSUAPI_GET_OBJECT_EXISTENCE *r);
WERROR _drsuapi_QuerySitesByCost(struct pipes_struct *p, struct drsuapi_QuerySitesByCost *r);
void drsuapi_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_drsuapi_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_drsuapi_shutdown(void);
#endif /* __SRV_DRSUAPI__ */
