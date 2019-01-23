#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/remact.h"
#ifndef _HEADER_RPC_IRemoteActivation
#define _HEADER_RPC_IRemoteActivation

extern const struct ndr_interface_table ndr_table_IRemoteActivation;

struct tevent_req *dcerpc_RemoteActivation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemoteActivation *r);
NTSTATUS dcerpc_RemoteActivation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemoteActivation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemoteActivation *r);
struct tevent_req *dcerpc_RemoteActivation_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHIS _this_object /* [in]  */,
						struct ORPCTHAT *_that /* [out] [ref] */,
						struct GUID _Clsid /* [in]  */,
						const char *_pwszObjectName /* [in] [ref,charset(UTF16)] */,
						struct MInterfacePointer *_pObjectStorage /* [in] [ref] */,
						uint32_t _ClientImpLevel /* [in]  */,
						uint32_t _Mode /* [in]  */,
						uint32_t _Interfaces /* [in] [range(1,32768)] */,
						struct GUID *_pIIDs /* [in] [ref,size_is(Interfaces)] */,
						uint16_t _num_protseqs /* [in]  */,
						uint16_t *_protseq /* [in] [size_is(num_protseqs)] */,
						uint64_t *_pOxid /* [out] [ref] */,
						struct DUALSTRINGARRAY *_pdsaOxidBindings /* [out] [ref] */,
						struct GUID *_ipidRemUnknown /* [out] [ref] */,
						uint32_t *_AuthnHint /* [out] [ref] */,
						struct COMVERSION *_ServerVersion /* [out] [ref] */,
						WERROR *_hr /* [out] [ref] */,
						struct MInterfacePointer **_ifaces /* [out] [size_is(Interfaces),ref] */,
						WERROR *_results /* [out] [size_is(Interfaces)] */);
NTSTATUS dcerpc_RemoteActivation_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_RemoteActivation(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHIS _this_object /* [in]  */,
				 struct ORPCTHAT *_that /* [out] [ref] */,
				 struct GUID _Clsid /* [in]  */,
				 const char *_pwszObjectName /* [in] [ref,charset(UTF16)] */,
				 struct MInterfacePointer *_pObjectStorage /* [in] [ref] */,
				 uint32_t _ClientImpLevel /* [in]  */,
				 uint32_t _Mode /* [in]  */,
				 uint32_t _Interfaces /* [in] [range(1,32768)] */,
				 struct GUID *_pIIDs /* [in] [ref,size_is(Interfaces)] */,
				 uint16_t _num_protseqs /* [in]  */,
				 uint16_t *_protseq /* [in] [size_is(num_protseqs)] */,
				 uint64_t *_pOxid /* [out] [ref] */,
				 struct DUALSTRINGARRAY *_pdsaOxidBindings /* [out] [ref] */,
				 struct GUID *_ipidRemUnknown /* [out] [ref] */,
				 uint32_t *_AuthnHint /* [out] [ref] */,
				 struct COMVERSION *_ServerVersion /* [out] [ref] */,
				 WERROR *_hr /* [out] [ref] */,
				 struct MInterfacePointer **_ifaces /* [out] [size_is(Interfaces),ref] */,
				 WERROR *_results /* [out] [size_is(Interfaces)] */,
				 WERROR *result);

#endif /* _HEADER_RPC_IRemoteActivation */
