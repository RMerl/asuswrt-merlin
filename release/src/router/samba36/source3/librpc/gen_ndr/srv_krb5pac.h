#include "librpc/gen_ndr/ndr_krb5pac.h"
#ifndef __SRV_KRB5PAC__
#define __SRV_KRB5PAC__
void _decode_pac(struct pipes_struct *p, struct decode_pac *r);
void _decode_pac_raw(struct pipes_struct *p, struct decode_pac_raw *r);
void _decode_login_info(struct pipes_struct *p, struct decode_login_info *r);
void _decode_login_info_ctr(struct pipes_struct *p, struct decode_login_info_ctr *r);
void _decode_pac_validate(struct pipes_struct *p, struct decode_pac_validate *r);
void krb5pac_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_krb5pac_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_krb5pac_shutdown(void);
#endif /* __SRV_KRB5PAC__ */
