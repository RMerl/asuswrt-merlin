#include "librpc/gen_ndr/ndr_wbint.h"
#ifndef __SRV_WBINT__
#define __SRV_WBINT__
void _wbint_Ping(struct pipes_struct *p, struct wbint_Ping *r);
NTSTATUS _wbint_LookupSid(struct pipes_struct *p, struct wbint_LookupSid *r);
NTSTATUS _wbint_LookupSids(struct pipes_struct *p, struct wbint_LookupSids *r);
NTSTATUS _wbint_LookupName(struct pipes_struct *p, struct wbint_LookupName *r);
NTSTATUS _wbint_Sid2Uid(struct pipes_struct *p, struct wbint_Sid2Uid *r);
NTSTATUS _wbint_Sid2Gid(struct pipes_struct *p, struct wbint_Sid2Gid *r);
NTSTATUS _wbint_Sids2UnixIDs(struct pipes_struct *p, struct wbint_Sids2UnixIDs *r);
NTSTATUS _wbint_Uid2Sid(struct pipes_struct *p, struct wbint_Uid2Sid *r);
NTSTATUS _wbint_Gid2Sid(struct pipes_struct *p, struct wbint_Gid2Sid *r);
NTSTATUS _wbint_AllocateUid(struct pipes_struct *p, struct wbint_AllocateUid *r);
NTSTATUS _wbint_AllocateGid(struct pipes_struct *p, struct wbint_AllocateGid *r);
NTSTATUS _wbint_QueryUser(struct pipes_struct *p, struct wbint_QueryUser *r);
NTSTATUS _wbint_LookupUserAliases(struct pipes_struct *p, struct wbint_LookupUserAliases *r);
NTSTATUS _wbint_LookupUserGroups(struct pipes_struct *p, struct wbint_LookupUserGroups *r);
NTSTATUS _wbint_QuerySequenceNumber(struct pipes_struct *p, struct wbint_QuerySequenceNumber *r);
NTSTATUS _wbint_LookupGroupMembers(struct pipes_struct *p, struct wbint_LookupGroupMembers *r);
NTSTATUS _wbint_QueryUserList(struct pipes_struct *p, struct wbint_QueryUserList *r);
NTSTATUS _wbint_QueryGroupList(struct pipes_struct *p, struct wbint_QueryGroupList *r);
NTSTATUS _wbint_DsGetDcName(struct pipes_struct *p, struct wbint_DsGetDcName *r);
NTSTATUS _wbint_LookupRids(struct pipes_struct *p, struct wbint_LookupRids *r);
NTSTATUS _wbint_CheckMachineAccount(struct pipes_struct *p, struct wbint_CheckMachineAccount *r);
NTSTATUS _wbint_ChangeMachineAccount(struct pipes_struct *p, struct wbint_ChangeMachineAccount *r);
NTSTATUS _wbint_PingDc(struct pipes_struct *p, struct wbint_PingDc *r);
void wbint_get_pipe_fns(struct api_struct **fns, int *n_fns);
#endif /* __SRV_WBINT__ */
