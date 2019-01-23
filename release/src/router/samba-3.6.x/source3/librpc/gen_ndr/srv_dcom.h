#include "librpc/gen_ndr/ndr_dcom.h"
#ifndef __SRV_DCOM_UNKNOWN__
#define __SRV_DCOM_UNKNOWN__
void _UseProtSeq(struct pipes_struct *p, struct UseProtSeq *r);
void _GetCustomProtseqInfo(struct pipes_struct *p, struct GetCustomProtseqInfo *r);
void _UpdateResolverBindings(struct pipes_struct *p, struct UpdateResolverBindings *r);
void dcom_Unknown_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_dcom_Unknown_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_dcom_Unknown_shutdown(void);
#endif /* __SRV_DCOM_UNKNOWN__ */
#ifndef __SRV_IUNKNOWN__
#define __SRV_IUNKNOWN__
WERROR _QueryInterface(struct pipes_struct *p, struct QueryInterface *r);
uint32 _AddRef(struct pipes_struct *p, struct AddRef *r);
uint32 _Release(struct pipes_struct *p, struct Release *r);
void IUnknown_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IUnknown_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IUnknown_shutdown(void);
#endif /* __SRV_IUNKNOWN__ */
#ifndef __SRV_ICLASSFACTORY__
#define __SRV_ICLASSFACTORY__
WERROR _CreateInstance(struct pipes_struct *p, struct CreateInstance *r);
WERROR _RemoteCreateInstance(struct pipes_struct *p, struct RemoteCreateInstance *r);
WERROR _LockServer(struct pipes_struct *p, struct LockServer *r);
WERROR _RemoteLockServer(struct pipes_struct *p, struct RemoteLockServer *r);
void IClassFactory_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IClassFactory_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IClassFactory_shutdown(void);
#endif /* __SRV_ICLASSFACTORY__ */
#ifndef __SRV_IREMUNKNOWN__
#define __SRV_IREMUNKNOWN__
WERROR _RemQueryInterface(struct pipes_struct *p, struct RemQueryInterface *r);
WERROR _RemAddRef(struct pipes_struct *p, struct RemAddRef *r);
WERROR _RemRelease(struct pipes_struct *p, struct RemRelease *r);
void IRemUnknown_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IRemUnknown_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IRemUnknown_shutdown(void);
#endif /* __SRV_IREMUNKNOWN__ */
#ifndef __SRV_ICLASSACTIVATOR__
#define __SRV_ICLASSACTIVATOR__
void _GetClassObject(struct pipes_struct *p, struct GetClassObject *r);
void IClassActivator_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IClassActivator_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IClassActivator_shutdown(void);
#endif /* __SRV_ICLASSACTIVATOR__ */
#ifndef __SRV_ISCMLOCALACTIVATOR__
#define __SRV_ISCMLOCALACTIVATOR__
WERROR _ISCMLocalActivator_CreateInstance(struct pipes_struct *p, struct ISCMLocalActivator_CreateInstance *r);
void ISCMLocalActivator_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ISCMLocalActivator_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ISCMLocalActivator_shutdown(void);
#endif /* __SRV_ISCMLOCALACTIVATOR__ */
#ifndef __SRV_IMACHINELOCALACTIVATOR__
#define __SRV_IMACHINELOCALACTIVATOR__
WERROR _IMachineLocalActivator_foo(struct pipes_struct *p, struct IMachineLocalActivator_foo *r);
void IMachineLocalActivator_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IMachineLocalActivator_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IMachineLocalActivator_shutdown(void);
#endif /* __SRV_IMACHINELOCALACTIVATOR__ */
#ifndef __SRV_ILOCALOBJECTEXPORTER__
#define __SRV_ILOCALOBJECTEXPORTER__
WERROR _ILocalObjectExporter_Foo(struct pipes_struct *p, struct ILocalObjectExporter_Foo *r);
void ILocalObjectExporter_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ILocalObjectExporter_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ILocalObjectExporter_shutdown(void);
#endif /* __SRV_ILOCALOBJECTEXPORTER__ */
#ifndef __SRV_ISYSTEMACTIVATOR__
#define __SRV_ISYSTEMACTIVATOR__
WERROR _ISystemActivatorRemoteCreateInstance(struct pipes_struct *p, struct ISystemActivatorRemoteCreateInstance *r);
void ISystemActivator_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ISystemActivator_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ISystemActivator_shutdown(void);
#endif /* __SRV_ISYSTEMACTIVATOR__ */
#ifndef __SRV_IREMUNKNOWN2__
#define __SRV_IREMUNKNOWN2__
WERROR _RemQueryInterface2(struct pipes_struct *p, struct RemQueryInterface2 *r);
void IRemUnknown2_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IRemUnknown2_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IRemUnknown2_shutdown(void);
#endif /* __SRV_IREMUNKNOWN2__ */
#ifndef __SRV_IDISPATCH__
#define __SRV_IDISPATCH__
WERROR _GetTypeInfoCount(struct pipes_struct *p, struct GetTypeInfoCount *r);
WERROR _GetTypeInfo(struct pipes_struct *p, struct GetTypeInfo *r);
WERROR _GetIDsOfNames(struct pipes_struct *p, struct GetIDsOfNames *r);
WERROR _Invoke(struct pipes_struct *p, struct Invoke *r);
void IDispatch_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IDispatch_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IDispatch_shutdown(void);
#endif /* __SRV_IDISPATCH__ */
#ifndef __SRV_IMARSHAL__
#define __SRV_IMARSHAL__
WERROR _MarshalInterface(struct pipes_struct *p, struct MarshalInterface *r);
WERROR _UnMarshalInterface(struct pipes_struct *p, struct UnMarshalInterface *r);
void IMarshal_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IMarshal_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IMarshal_shutdown(void);
#endif /* __SRV_IMARSHAL__ */
#ifndef __SRV_ICOFFEEMACHINE__
#define __SRV_ICOFFEEMACHINE__
WERROR _MakeCoffee(struct pipes_struct *p, struct MakeCoffee *r);
void ICoffeeMachine_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_ICoffeeMachine_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_ICoffeeMachine_shutdown(void);
#endif /* __SRV_ICOFFEEMACHINE__ */
#ifndef __SRV_ISTREAM__
#define __SRV_ISTREAM__
WERROR _Read(struct pipes_struct *p, struct Read *r);
WERROR _Write(struct pipes_struct *p, struct Write *r);
void IStream_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IStream_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IStream_shutdown(void);
#endif /* __SRV_ISTREAM__ */
