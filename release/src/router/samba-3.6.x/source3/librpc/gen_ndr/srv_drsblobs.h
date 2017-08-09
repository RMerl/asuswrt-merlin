#include "librpc/gen_ndr/ndr_drsblobs.h"
#ifndef __SRV_DRSBLOBS__
#define __SRV_DRSBLOBS__
void _decode_replPropertyMetaData(struct pipes_struct *p, struct decode_replPropertyMetaData *r);
void _decode_replUpToDateVector(struct pipes_struct *p, struct decode_replUpToDateVector *r);
void _decode_repsFromTo(struct pipes_struct *p, struct decode_repsFromTo *r);
void _decode_partialAttributeSet(struct pipes_struct *p, struct decode_partialAttributeSet *r);
void _decode_prefixMap(struct pipes_struct *p, struct decode_prefixMap *r);
void _decode_ldapControlDirSync(struct pipes_struct *p, struct decode_ldapControlDirSync *r);
void _decode_supplementalCredentials(struct pipes_struct *p, struct decode_supplementalCredentials *r);
void _decode_Packages(struct pipes_struct *p, struct decode_Packages *r);
void _decode_PrimaryKerberos(struct pipes_struct *p, struct decode_PrimaryKerberos *r);
void _decode_PrimaryCLEARTEXT(struct pipes_struct *p, struct decode_PrimaryCLEARTEXT *r);
void _decode_PrimaryWDigest(struct pipes_struct *p, struct decode_PrimaryWDigest *r);
void _decode_trustAuthInOut(struct pipes_struct *p, struct decode_trustAuthInOut *r);
void _decode_trustDomainPasswords(struct pipes_struct *p, struct decode_trustDomainPasswords *r);
void _decode_ExtendedErrorInfo(struct pipes_struct *p, struct decode_ExtendedErrorInfo *r);
void _decode_ForestTrustInfo(struct pipes_struct *p, struct decode_ForestTrustInfo *r);
void drsblobs_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_drsblobs_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_drsblobs_shutdown(void);
#endif /* __SRV_DRSBLOBS__ */
