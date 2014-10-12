#include "librpc/gen_ndr/ndr_wmi.h"
#ifndef __SRV_IWBEMCLASSOBJECT__
#define __SRV_IWBEMCLASSOBJECT__
WERROR _Delete(struct pipes_struct *p, struct Delete *r);
void IWbemClassObject_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemClassObject_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemClassObject_shutdown(void);
#endif /* __SRV_IWBEMCLASSOBJECT__ */
#ifndef __SRV_IWBEMSERVICES__
#define __SRV_IWBEMSERVICES__
WERROR _OpenNamespace(struct pipes_struct *p, struct OpenNamespace *r);
WERROR _CancelAsyncCall(struct pipes_struct *p, struct CancelAsyncCall *r);
WERROR _QueryObjectSink(struct pipes_struct *p, struct QueryObjectSink *r);
WERROR _GetObject(struct pipes_struct *p, struct GetObject *r);
WERROR _GetObjectAsync(struct pipes_struct *p, struct GetObjectAsync *r);
WERROR _PutClass(struct pipes_struct *p, struct PutClass *r);
WERROR _PutClassAsync(struct pipes_struct *p, struct PutClassAsync *r);
WERROR _DeleteClass(struct pipes_struct *p, struct DeleteClass *r);
WERROR _DeleteClassAsync(struct pipes_struct *p, struct DeleteClassAsync *r);
WERROR _CreateClassEnum(struct pipes_struct *p, struct CreateClassEnum *r);
WERROR _CreateClassEnumAsync(struct pipes_struct *p, struct CreateClassEnumAsync *r);
WERROR _PutInstance(struct pipes_struct *p, struct PutInstance *r);
WERROR _PutInstanceAsync(struct pipes_struct *p, struct PutInstanceAsync *r);
WERROR _DeleteInstance(struct pipes_struct *p, struct DeleteInstance *r);
WERROR _DeleteInstanceAsync(struct pipes_struct *p, struct DeleteInstanceAsync *r);
WERROR _CreateInstanceEnum(struct pipes_struct *p, struct CreateInstanceEnum *r);
WERROR _CreateInstanceEnumAsync(struct pipes_struct *p, struct CreateInstanceEnumAsync *r);
WERROR _ExecQuery(struct pipes_struct *p, struct ExecQuery *r);
WERROR _ExecQueryAsync(struct pipes_struct *p, struct ExecQueryAsync *r);
WERROR _ExecNotificationQuery(struct pipes_struct *p, struct ExecNotificationQuery *r);
WERROR _ExecNotificationQueryAsync(struct pipes_struct *p, struct ExecNotificationQueryAsync *r);
WERROR _ExecMethod(struct pipes_struct *p, struct ExecMethod *r);
WERROR _ExecMethodAsync(struct pipes_struct *p, struct ExecMethodAsync *r);
void IWbemServices_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemServices_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemServices_shutdown(void);
#endif /* __SRV_IWBEMSERVICES__ */
#ifndef __SRV_IENUMWBEMCLASSOBJECT__
#define __SRV_IENUMWBEMCLASSOBJECT__
WERROR _Reset(struct pipes_struct *p, struct Reset *r);
WERROR _IEnumWbemClassObject_Next(struct pipes_struct *p, struct IEnumWbemClassObject_Next *r);
WERROR _NextAsync(struct pipes_struct *p, struct NextAsync *r);
WERROR _IEnumWbemClassObject_Clone(struct pipes_struct *p, struct IEnumWbemClassObject_Clone *r);
WERROR _Skip(struct pipes_struct *p, struct Skip *r);
void IEnumWbemClassObject_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IEnumWbemClassObject_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IEnumWbemClassObject_shutdown(void);
#endif /* __SRV_IENUMWBEMCLASSOBJECT__ */
#ifndef __SRV_IWBEMCONTEXT__
#define __SRV_IWBEMCONTEXT__
WERROR _Clone(struct pipes_struct *p, struct Clone *r);
WERROR _GetNames(struct pipes_struct *p, struct GetNames *r);
WERROR _BeginEnumeration(struct pipes_struct *p, struct BeginEnumeration *r);
WERROR _Next(struct pipes_struct *p, struct Next *r);
WERROR _EndEnumeration(struct pipes_struct *p, struct EndEnumeration *r);
WERROR _SetValue(struct pipes_struct *p, struct SetValue *r);
WERROR _GetValue(struct pipes_struct *p, struct GetValue *r);
WERROR _DeleteValue(struct pipes_struct *p, struct DeleteValue *r);
WERROR _DeleteAll(struct pipes_struct *p, struct DeleteAll *r);
void IWbemContext_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemContext_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemContext_shutdown(void);
#endif /* __SRV_IWBEMCONTEXT__ */
#ifndef __SRV_IWBEMLEVEL1LOGIN__
#define __SRV_IWBEMLEVEL1LOGIN__
WERROR _EstablishPosition(struct pipes_struct *p, struct EstablishPosition *r);
WERROR _RequestChallenge(struct pipes_struct *p, struct RequestChallenge *r);
WERROR _WBEMLogin(struct pipes_struct *p, struct WBEMLogin *r);
WERROR _NTLMLogin(struct pipes_struct *p, struct NTLMLogin *r);
void IWbemLevel1Login_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemLevel1Login_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemLevel1Login_shutdown(void);
#endif /* __SRV_IWBEMLEVEL1LOGIN__ */
#ifndef __SRV_IWBEMWCOSMARTENUM__
#define __SRV_IWBEMWCOSMARTENUM__
WERROR _IWbemWCOSmartEnum_Next(struct pipes_struct *p, struct IWbemWCOSmartEnum_Next *r);
void IWbemWCOSmartEnum_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemWCOSmartEnum_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemWCOSmartEnum_shutdown(void);
#endif /* __SRV_IWBEMWCOSMARTENUM__ */
#ifndef __SRV_IWBEMFETCHSMARTENUM__
#define __SRV_IWBEMFETCHSMARTENUM__
WERROR _Fetch(struct pipes_struct *p, struct Fetch *r);
WERROR _Test(struct pipes_struct *p, struct Test *r);
void IWbemFetchSmartEnum_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemFetchSmartEnum_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemFetchSmartEnum_shutdown(void);
#endif /* __SRV_IWBEMFETCHSMARTENUM__ */
#ifndef __SRV_IWBEMCALLRESULT__
#define __SRV_IWBEMCALLRESULT__
WERROR _GetResultObject(struct pipes_struct *p, struct GetResultObject *r);
WERROR _GetResultString(struct pipes_struct *p, struct GetResultString *r);
WERROR _GetResultServices(struct pipes_struct *p, struct GetResultServices *r);
WERROR _GetCallStatus(struct pipes_struct *p, struct GetCallStatus *r);
void IWbemCallResult_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemCallResult_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemCallResult_shutdown(void);
#endif /* __SRV_IWBEMCALLRESULT__ */
#ifndef __SRV_IWBEMOBJECTSINK__
#define __SRV_IWBEMOBJECTSINK__
WERROR _SetStatus(struct pipes_struct *p, struct SetStatus *r);
WERROR _Indicate(struct pipes_struct *p, struct Indicate *r);
void IWbemObjectSink_get_pipe_fns(struct api_struct **fns, int *n_fns);
struct rpc_srv_callbacks;
NTSTATUS rpc_IWbemObjectSink_init(const struct rpc_srv_callbacks *rpc_srv_cb);
NTSTATUS rpc_IWbemObjectSink_shutdown(void);
#endif /* __SRV_IWBEMOBJECTSINK__ */
