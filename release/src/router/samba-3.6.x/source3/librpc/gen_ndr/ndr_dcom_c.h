#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/dcom.h"
#ifndef _HEADER_RPC_dcom_Unknown
#define _HEADER_RPC_dcom_Unknown

extern const struct ndr_interface_table ndr_table_dcom_Unknown;

struct tevent_req *dcerpc_UseProtSeq_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct UseProtSeq *r);
NTSTATUS dcerpc_UseProtSeq_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_UseProtSeq_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct UseProtSeq *r);
struct tevent_req *dcerpc_UseProtSeq_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_UseProtSeq_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_UseProtSeq(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_GetCustomProtseqInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetCustomProtseqInfo *r);
NTSTATUS dcerpc_GetCustomProtseqInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetCustomProtseqInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetCustomProtseqInfo *r);
struct tevent_req *dcerpc_GetCustomProtseqInfo_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_GetCustomProtseqInfo_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetCustomProtseqInfo(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx);

struct tevent_req *dcerpc_UpdateResolverBindings_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct UpdateResolverBindings *r);
NTSTATUS dcerpc_UpdateResolverBindings_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_UpdateResolverBindings_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct UpdateResolverBindings *r);
struct tevent_req *dcerpc_UpdateResolverBindings_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_UpdateResolverBindings_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_UpdateResolverBindings(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx);

#endif /* _HEADER_RPC_dcom_Unknown */
#ifndef _HEADER_RPC_IUnknown
#define _HEADER_RPC_IUnknown

extern const struct ndr_interface_table ndr_table_IUnknown;

struct tevent_req *dcerpc_QueryInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct QueryInterface *r);
NTSTATUS dcerpc_QueryInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_QueryInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct QueryInterface *r);
struct tevent_req *dcerpc_QueryInterface_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */,
					      struct GUID *_iid /* [in] [unique] */,
					      struct MInterfacePointer **_data /* [out] [ref,iid_is(riid)] */);
NTSTATUS dcerpc_QueryInterface_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_QueryInterface(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       struct GUID *_iid /* [in] [unique] */,
			       struct MInterfacePointer **_data /* [out] [ref,iid_is(riid)] */,
			       WERROR *result);

struct tevent_req *dcerpc_AddRef_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct AddRef *r);
NTSTATUS dcerpc_AddRef_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_AddRef_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct AddRef *r);
struct tevent_req *dcerpc_AddRef_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct dcerpc_binding_handle *h,
				      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				      struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_AddRef_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    uint32_t *result);
NTSTATUS dcerpc_AddRef(struct dcerpc_binding_handle *h,
		       TALLOC_CTX *mem_ctx,
		       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		       struct ORPCTHIS _ORPCthis /* [in]  */,
		       uint32_t *result);

struct tevent_req *dcerpc_Release_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Release *r);
NTSTATUS dcerpc_Release_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Release_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Release *r);
struct tevent_req *dcerpc_Release_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct dcerpc_binding_handle *h,
				       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				       struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_Release_recv(struct tevent_req *req,
			     TALLOC_CTX *mem_ctx,
			     uint32_t *result);
NTSTATUS dcerpc_Release(struct dcerpc_binding_handle *h,
			TALLOC_CTX *mem_ctx,
			struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			struct ORPCTHIS _ORPCthis /* [in]  */,
			uint32_t *result);

#endif /* _HEADER_RPC_IUnknown */
#ifndef _HEADER_RPC_IClassFactory
#define _HEADER_RPC_IClassFactory

extern const struct ndr_interface_table ndr_table_IClassFactory;

struct tevent_req *dcerpc_CreateInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct CreateInstance *r);
NTSTATUS dcerpc_CreateInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_CreateInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct CreateInstance *r);
struct tevent_req *dcerpc_CreateInstance_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */,
					      struct MInterfacePointer *_pUnknown /* [in] [unique] */,
					      struct GUID *_iid /* [in] [unique] */,
					      struct MInterfacePointer *_ppv /* [out] [unique,iid_is(riid)] */);
NTSTATUS dcerpc_CreateInstance_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_CreateInstance(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       struct MInterfacePointer *_pUnknown /* [in] [unique] */,
			       struct GUID *_iid /* [in] [unique] */,
			       struct MInterfacePointer *_ppv /* [out] [unique,iid_is(riid)] */,
			       WERROR *result);

struct tevent_req *dcerpc_RemoteCreateInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemoteCreateInstance *r);
NTSTATUS dcerpc_RemoteCreateInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemoteCreateInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemoteCreateInstance *r);
struct tevent_req *dcerpc_RemoteCreateInstance_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						    struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_RemoteCreateInstance_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_RemoteCreateInstance(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				     struct ORPCTHIS _ORPCthis /* [in]  */,
				     WERROR *result);

struct tevent_req *dcerpc_LockServer_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct LockServer *r);
NTSTATUS dcerpc_LockServer_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_LockServer_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct LockServer *r);
struct tevent_req *dcerpc_LockServer_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					  struct ORPCTHIS _ORPCthis /* [in]  */,
					  uint8_t _lock /* [in]  */);
NTSTATUS dcerpc_LockServer_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_LockServer(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			   struct ORPCTHIS _ORPCthis /* [in]  */,
			   uint8_t _lock /* [in]  */,
			   WERROR *result);

struct tevent_req *dcerpc_RemoteLockServer_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemoteLockServer *r);
NTSTATUS dcerpc_RemoteLockServer_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemoteLockServer_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemoteLockServer *r);
struct tevent_req *dcerpc_RemoteLockServer_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_RemoteLockServer_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_RemoteLockServer(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 WERROR *result);

#endif /* _HEADER_RPC_IClassFactory */
#ifndef _HEADER_RPC_IRemUnknown
#define _HEADER_RPC_IRemUnknown

extern const struct ndr_interface_table ndr_table_IRemUnknown;

struct tevent_req *dcerpc_RemQueryInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemQueryInterface *r);
NTSTATUS dcerpc_RemQueryInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemQueryInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemQueryInterface *r);
struct tevent_req *dcerpc_RemQueryInterface_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						 struct ORPCTHIS _ORPCthis /* [in]  */,
						 struct GUID *_ripid /* [in] [unique] */,
						 uint32_t _cRefs /* [in]  */,
						 uint16_t _cIids /* [in]  */,
						 struct GUID *_iids /* [in] [size_is(cIids),unique] */,
						 struct MInterfacePointer *_ip /* [out] [unique,size_is(cIids)] */);
NTSTATUS dcerpc_RemQueryInterface_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_RemQueryInterface(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				  struct ORPCTHIS _ORPCthis /* [in]  */,
				  struct GUID *_ripid /* [in] [unique] */,
				  uint32_t _cRefs /* [in]  */,
				  uint16_t _cIids /* [in]  */,
				  struct GUID *_iids /* [in] [size_is(cIids),unique] */,
				  struct MInterfacePointer *_ip /* [out] [unique,size_is(cIids)] */,
				  WERROR *result);

struct tevent_req *dcerpc_RemAddRef_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemAddRef *r);
NTSTATUS dcerpc_RemAddRef_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemAddRef_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemAddRef *r);
struct tevent_req *dcerpc_RemAddRef_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 uint16_t _cInterfaceRefs /* [in]  */,
					 struct REMINTERFACEREF *_InterfaceRefs /* [in] [size_is(cInterfaceRefs)] */,
					 WERROR *_pResults /* [out] [size_is(cInterfaceRefs),unique] */);
NTSTATUS dcerpc_RemAddRef_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_RemAddRef(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  uint16_t _cInterfaceRefs /* [in]  */,
			  struct REMINTERFACEREF *_InterfaceRefs /* [in] [size_is(cInterfaceRefs)] */,
			  WERROR *_pResults /* [out] [size_is(cInterfaceRefs),unique] */,
			  WERROR *result);

struct tevent_req *dcerpc_RemRelease_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemRelease *r);
NTSTATUS dcerpc_RemRelease_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemRelease_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemRelease *r);
struct tevent_req *dcerpc_RemRelease_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					  struct ORPCTHIS _ORPCthis /* [in]  */,
					  uint16_t _cInterfaceRefs /* [in]  */,
					  struct REMINTERFACEREF *_InterfaceRefs /* [in] [size_is(cInterfaceRefs)] */);
NTSTATUS dcerpc_RemRelease_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_RemRelease(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			   struct ORPCTHIS _ORPCthis /* [in]  */,
			   uint16_t _cInterfaceRefs /* [in]  */,
			   struct REMINTERFACEREF *_InterfaceRefs /* [in] [size_is(cInterfaceRefs)] */,
			   WERROR *result);

#endif /* _HEADER_RPC_IRemUnknown */
#ifndef _HEADER_RPC_IClassActivator
#define _HEADER_RPC_IClassActivator

extern const struct ndr_interface_table ndr_table_IClassActivator;

struct tevent_req *dcerpc_GetClassObject_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetClassObject *r);
NTSTATUS dcerpc_GetClassObject_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetClassObject_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetClassObject *r);
struct tevent_req *dcerpc_GetClassObject_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */,
					      struct GUID _clsid /* [in]  */,
					      uint32_t _context /* [in]  */,
					      uint32_t _locale /* [in]  */,
					      struct GUID _iid /* [in]  */,
					      struct MInterfacePointer *_data /* [out] [iid_is(iid),ref] */);
NTSTATUS dcerpc_GetClassObject_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetClassObject(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       struct GUID _clsid /* [in]  */,
			       uint32_t _context /* [in]  */,
			       uint32_t _locale /* [in]  */,
			       struct GUID _iid /* [in]  */,
			       struct MInterfacePointer *_data /* [out] [iid_is(iid),ref] */);

#endif /* _HEADER_RPC_IClassActivator */
#ifndef _HEADER_RPC_ISCMLocalActivator
#define _HEADER_RPC_ISCMLocalActivator

extern const struct ndr_interface_table ndr_table_ISCMLocalActivator;

struct tevent_req *dcerpc_ISCMLocalActivator_CreateInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ISCMLocalActivator_CreateInstance *r);
NTSTATUS dcerpc_ISCMLocalActivator_CreateInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ISCMLocalActivator_CreateInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ISCMLocalActivator_CreateInstance *r);
struct tevent_req *dcerpc_ISCMLocalActivator_CreateInstance_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
								 struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_ISCMLocalActivator_CreateInstance_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_ISCMLocalActivator_CreateInstance(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						  struct ORPCTHIS _ORPCthis /* [in]  */,
						  WERROR *result);

#endif /* _HEADER_RPC_ISCMLocalActivator */
#ifndef _HEADER_RPC_IMachineLocalActivator
#define _HEADER_RPC_IMachineLocalActivator

extern const struct ndr_interface_table ndr_table_IMachineLocalActivator;

struct tevent_req *dcerpc_IMachineLocalActivator_foo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct IMachineLocalActivator_foo *r);
NTSTATUS dcerpc_IMachineLocalActivator_foo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_IMachineLocalActivator_foo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct IMachineLocalActivator_foo *r);
struct tevent_req *dcerpc_IMachineLocalActivator_foo_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_IMachineLocalActivator_foo_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_IMachineLocalActivator_foo(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);

#endif /* _HEADER_RPC_IMachineLocalActivator */
#ifndef _HEADER_RPC_ILocalObjectExporter
#define _HEADER_RPC_ILocalObjectExporter

extern const struct ndr_interface_table ndr_table_ILocalObjectExporter;

struct tevent_req *dcerpc_ILocalObjectExporter_Foo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ILocalObjectExporter_Foo *r);
NTSTATUS dcerpc_ILocalObjectExporter_Foo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ILocalObjectExporter_Foo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ILocalObjectExporter_Foo *r);
struct tevent_req *dcerpc_ILocalObjectExporter_Foo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_ILocalObjectExporter_Foo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_ILocalObjectExporter_Foo(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);

#endif /* _HEADER_RPC_ILocalObjectExporter */
#ifndef _HEADER_RPC_ISystemActivator
#define _HEADER_RPC_ISystemActivator

extern const struct ndr_interface_table ndr_table_ISystemActivator;

struct tevent_req *dcerpc_ISystemActivatorRemoteCreateInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ISystemActivatorRemoteCreateInstance *r);
NTSTATUS dcerpc_ISystemActivatorRemoteCreateInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ISystemActivatorRemoteCreateInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ISystemActivatorRemoteCreateInstance *r);
struct tevent_req *dcerpc_ISystemActivatorRemoteCreateInstance_send(TALLOC_CTX *mem_ctx,
								    struct tevent_context *ev,
								    struct dcerpc_binding_handle *h,
								    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
								    struct ORPCTHIS _ORPCthis /* [in]  */,
								    uint64_t _unknown1 /* [in]  */,
								    struct MInterfacePointer _iface1 /* [in]  */,
								    uint64_t _unknown2 /* [in]  */,
								    uint32_t *_unknown3 /* [out] [ref] */,
								    struct MInterfacePointer *_iface2 /* [out] [ref] */);
NTSTATUS dcerpc_ISystemActivatorRemoteCreateInstance_recv(struct tevent_req *req,
							  TALLOC_CTX *mem_ctx,
							  WERROR *result);
NTSTATUS dcerpc_ISystemActivatorRemoteCreateInstance(struct dcerpc_binding_handle *h,
						     TALLOC_CTX *mem_ctx,
						     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						     struct ORPCTHIS _ORPCthis /* [in]  */,
						     uint64_t _unknown1 /* [in]  */,
						     struct MInterfacePointer _iface1 /* [in]  */,
						     uint64_t _unknown2 /* [in]  */,
						     uint32_t *_unknown3 /* [out] [ref] */,
						     struct MInterfacePointer *_iface2 /* [out] [ref] */,
						     WERROR *result);

#endif /* _HEADER_RPC_ISystemActivator */
#ifndef _HEADER_RPC_IRemUnknown2
#define _HEADER_RPC_IRemUnknown2

extern const struct ndr_interface_table ndr_table_IRemUnknown2;

struct tevent_req *dcerpc_RemQueryInterface2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RemQueryInterface2 *r);
NTSTATUS dcerpc_RemQueryInterface2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RemQueryInterface2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RemQueryInterface2 *r);
struct tevent_req *dcerpc_RemQueryInterface2_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						  struct ORPCTHIS _ORPCthis /* [in]  */,
						  struct GUID *_ripid /* [in] [unique] */,
						  uint16_t _cIids /* [in]  */,
						  struct GUID *_iids /* [in] [size_is(cIids),unique] */,
						  WERROR *_phr /* [out] [size_is(cIids),unique] */,
						  struct MInterfacePointer *_ppMIF /* [out] [size_is(cIids),unique] */);
NTSTATUS dcerpc_RemQueryInterface2_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_RemQueryInterface2(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				   struct ORPCTHIS _ORPCthis /* [in]  */,
				   struct GUID *_ripid /* [in] [unique] */,
				   uint16_t _cIids /* [in]  */,
				   struct GUID *_iids /* [in] [size_is(cIids),unique] */,
				   WERROR *_phr /* [out] [size_is(cIids),unique] */,
				   struct MInterfacePointer *_ppMIF /* [out] [size_is(cIids),unique] */,
				   WERROR *result);

#endif /* _HEADER_RPC_IRemUnknown2 */
#ifndef _HEADER_RPC_IDispatch
#define _HEADER_RPC_IDispatch

extern const struct ndr_interface_table ndr_table_IDispatch;

struct tevent_req *dcerpc_GetTypeInfoCount_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetTypeInfoCount *r);
NTSTATUS dcerpc_GetTypeInfoCount_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetTypeInfoCount_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetTypeInfoCount *r);
struct tevent_req *dcerpc_GetTypeInfoCount_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */,
						uint16_t *_pctinfo /* [out] [unique] */);
NTSTATUS dcerpc_GetTypeInfoCount_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_GetTypeInfoCount(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 uint16_t *_pctinfo /* [out] [unique] */,
				 WERROR *result);

struct tevent_req *dcerpc_GetTypeInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetTypeInfo *r);
NTSTATUS dcerpc_GetTypeInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetTypeInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetTypeInfo *r);
struct tevent_req *dcerpc_GetTypeInfo_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					   struct ORPCTHIS _ORPCthis /* [in]  */,
					   uint16_t _iTInfo /* [in]  */,
					   uint32_t _lcid /* [in]  */,
					   struct REF_ITypeInfo *_ppTInfo /* [out] [unique] */);
NTSTATUS dcerpc_GetTypeInfo_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_GetTypeInfo(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			    struct ORPCTHIS _ORPCthis /* [in]  */,
			    uint16_t _iTInfo /* [in]  */,
			    uint32_t _lcid /* [in]  */,
			    struct REF_ITypeInfo *_ppTInfo /* [out] [unique] */,
			    WERROR *result);

struct tevent_req *dcerpc_GetIDsOfNames_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetIDsOfNames *r);
NTSTATUS dcerpc_GetIDsOfNames_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetIDsOfNames_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetIDsOfNames *r);
struct tevent_req *dcerpc_GetIDsOfNames_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					     struct ORPCTHIS _ORPCthis /* [in]  */,
					     struct GUID *_riid /* [in] [unique] */,
					     uint16_t _cNames /* [in]  */,
					     uint32_t _lcid /* [in]  */,
					     uint32_t *_rgDispId /* [out] [unique,size_is(cNames)] */);
NTSTATUS dcerpc_GetIDsOfNames_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_GetIDsOfNames(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			      struct ORPCTHIS _ORPCthis /* [in]  */,
			      struct GUID *_riid /* [in] [unique] */,
			      uint16_t _cNames /* [in]  */,
			      uint32_t _lcid /* [in]  */,
			      uint32_t *_rgDispId /* [out] [unique,size_is(cNames)] */,
			      WERROR *result);

struct tevent_req *dcerpc_Invoke_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Invoke *r);
NTSTATUS dcerpc_Invoke_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Invoke_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Invoke *r);
struct tevent_req *dcerpc_Invoke_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct dcerpc_binding_handle *h,
				      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				      struct ORPCTHIS _ORPCthis /* [in]  */,
				      uint32_t _dispIdMember /* [in]  */,
				      struct GUID *_riid /* [in] [unique] */,
				      uint32_t _lcid /* [in]  */,
				      uint16_t _wFlags /* [in]  */,
				      struct DISPPARAMS *_pDispParams /* [in,out] [unique] */,
				      struct VARIANT *_pVarResult /* [out] [unique] */,
				      struct EXCEPINFO *_pExcepInfo /* [out] [unique] */,
				      uint16_t *_puArgErr /* [out] [unique] */);
NTSTATUS dcerpc_Invoke_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    WERROR *result);
NTSTATUS dcerpc_Invoke(struct dcerpc_binding_handle *h,
		       TALLOC_CTX *mem_ctx,
		       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		       struct ORPCTHIS _ORPCthis /* [in]  */,
		       uint32_t _dispIdMember /* [in]  */,
		       struct GUID *_riid /* [in] [unique] */,
		       uint32_t _lcid /* [in]  */,
		       uint16_t _wFlags /* [in]  */,
		       struct DISPPARAMS *_pDispParams /* [in,out] [unique] */,
		       struct VARIANT *_pVarResult /* [out] [unique] */,
		       struct EXCEPINFO *_pExcepInfo /* [out] [unique] */,
		       uint16_t *_puArgErr /* [out] [unique] */,
		       WERROR *result);

#endif /* _HEADER_RPC_IDispatch */
#ifndef _HEADER_RPC_IMarshal
#define _HEADER_RPC_IMarshal

extern const struct ndr_interface_table ndr_table_IMarshal;

struct tevent_req *dcerpc_MarshalInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct MarshalInterface *r);
NTSTATUS dcerpc_MarshalInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_MarshalInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct MarshalInterface *r);
struct tevent_req *dcerpc_MarshalInterface_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_MarshalInterface_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_MarshalInterface(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 WERROR *result);

struct tevent_req *dcerpc_UnMarshalInterface_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct UnMarshalInterface *r);
NTSTATUS dcerpc_UnMarshalInterface_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_UnMarshalInterface_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct UnMarshalInterface *r);
struct tevent_req *dcerpc_UnMarshalInterface_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						  struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_UnMarshalInterface_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_UnMarshalInterface(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				   struct ORPCTHIS _ORPCthis /* [in]  */,
				   WERROR *result);

#endif /* _HEADER_RPC_IMarshal */
#ifndef _HEADER_RPC_ICoffeeMachine
#define _HEADER_RPC_ICoffeeMachine

extern const struct ndr_interface_table ndr_table_ICoffeeMachine;

struct tevent_req *dcerpc_MakeCoffee_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct MakeCoffee *r);
NTSTATUS dcerpc_MakeCoffee_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_MakeCoffee_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct MakeCoffee *r);
struct tevent_req *dcerpc_MakeCoffee_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					  struct ORPCTHIS _ORPCthis /* [in]  */,
					  const char *_flavor /* [in] [ref,charset(UTF16)] */);
NTSTATUS dcerpc_MakeCoffee_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_MakeCoffee(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			   struct ORPCTHIS _ORPCthis /* [in]  */,
			   const char *_flavor /* [in] [ref,charset(UTF16)] */,
			   WERROR *result);

#endif /* _HEADER_RPC_ICoffeeMachine */
#ifndef _HEADER_RPC_IStream
#define _HEADER_RPC_IStream

extern const struct ndr_interface_table ndr_table_IStream;

struct tevent_req *dcerpc_Read_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Read *r);
NTSTATUS dcerpc_Read_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Read_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Read *r);
struct tevent_req *dcerpc_Read_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct dcerpc_binding_handle *h,
				    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				    struct ORPCTHIS _ORPCthis /* [in]  */,
				    uint8_t *_pv /* [out] [length_is(*num_read),size_is(num_requested)] */,
				    uint32_t _num_requested /* [in]  */,
				    uint32_t *_num_readx /* [in] [unique] */,
				    uint32_t *_num_read /* [out] [ref] */);
NTSTATUS dcerpc_Read_recv(struct tevent_req *req,
			  TALLOC_CTX *mem_ctx,
			  WERROR *result);
NTSTATUS dcerpc_Read(struct dcerpc_binding_handle *h,
		     TALLOC_CTX *mem_ctx,
		     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		     struct ORPCTHIS _ORPCthis /* [in]  */,
		     uint8_t *_pv /* [out] [length_is(*num_read),size_is(num_requested)] */,
		     uint32_t _num_requested /* [in]  */,
		     uint32_t *_num_readx /* [in] [unique] */,
		     uint32_t *_num_read /* [out] [ref] */,
		     WERROR *result);

struct tevent_req *dcerpc_Write_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Write *r);
NTSTATUS dcerpc_Write_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Write_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Write *r);
struct tevent_req *dcerpc_Write_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct dcerpc_binding_handle *h,
				     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				     struct ORPCTHIS _ORPCthis /* [in]  */,
				     uint8_t *_data /* [in] [size_is(num_requested),unique] */,
				     uint32_t _num_requested /* [in]  */,
				     uint32_t *_num_written /* [out] [ref] */);
NTSTATUS dcerpc_Write_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   WERROR *result);
NTSTATUS dcerpc_Write(struct dcerpc_binding_handle *h,
		      TALLOC_CTX *mem_ctx,
		      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		      struct ORPCTHIS _ORPCthis /* [in]  */,
		      uint8_t *_data /* [in] [size_is(num_requested),unique] */,
		      uint32_t _num_requested /* [in]  */,
		      uint32_t *_num_written /* [out] [ref] */,
		      WERROR *result);

#endif /* _HEADER_RPC_IStream */
