#include "librpc/gen_ndr/ndr_rot.h"
#ifndef __SRV_ROT__
#define __SRV_ROT__
WERROR _rot_add(struct pipes_struct *p, struct rot_add *r);
WERROR _rot_remove(struct pipes_struct *p, struct rot_remove *r);
WERROR _rot_is_listed(struct pipes_struct *p, struct rot_is_listed *r);
WERROR _rot_get_interface_pointer(struct pipes_struct *p, struct rot_get_interface_pointer *r);
WERROR _rot_set_modification_time(struct pipes_struct *p, struct rot_set_modification_time *r);
WERROR _rot_get_modification_time(struct pipes_struct *p, struct rot_get_modification_time *r);
WERROR _rot_enum(struct pipes_struct *p, struct rot_enum *r);
void rot_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_rot_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_rot_shutdown(void);
#endif /* __SRV_ROT__ */
