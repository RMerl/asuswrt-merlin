#include "../librpc/gen_ndr/ndr_epmapper.h"
#ifndef __SRV_EPMAPPER__
#define __SRV_EPMAPPER__
uint32 _epm_Insert(pipes_struct *p, struct epm_Insert *r);
uint32 _epm_Delete(pipes_struct *p, struct epm_Delete *r);
uint32 _epm_Lookup(pipes_struct *p, struct epm_Lookup *r);
uint32 _epm_Map(pipes_struct *p, struct epm_Map *r);
uint32 _epm_LookupHandleFree(pipes_struct *p, struct epm_LookupHandleFree *r);
uint32 _epm_InqObject(pipes_struct *p, struct epm_InqObject *r);
uint32 _epm_MgmtDelete(pipes_struct *p, struct epm_MgmtDelete *r);
uint32 _epm_MapAuth(pipes_struct *p, struct epm_MapAuth *r);
void epmapper_get_pipe_fns(struct api_struct **fns, int *n_fns);
NTSTATUS rpc_epmapper_dispatch(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, const struct ndr_interface_table *table, uint32_t opnum, void *r);
uint32 _epm_Insert(pipes_struct *p, struct epm_Insert *r);
uint32 _epm_Delete(pipes_struct *p, struct epm_Delete *r);
uint32 _epm_Lookup(pipes_struct *p, struct epm_Lookup *r);
uint32 _epm_Map(pipes_struct *p, struct epm_Map *r);
uint32 _epm_LookupHandleFree(pipes_struct *p, struct epm_LookupHandleFree *r);
uint32 _epm_InqObject(pipes_struct *p, struct epm_InqObject *r);
uint32 _epm_MgmtDelete(pipes_struct *p, struct epm_MgmtDelete *r);
uint32 _epm_MapAuth(pipes_struct *p, struct epm_MapAuth *r);
NTSTATUS rpc_epmapper_init(void);
#endif /* __SRV_EPMAPPER__ */
