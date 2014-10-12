#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/wmi.h"
#ifndef _HEADER_RPC_IWbemClassObject
#define _HEADER_RPC_IWbemClassObject

extern const struct ndr_interface_table ndr_table_IWbemClassObject;

struct tevent_req *dcerpc_Delete_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Delete *r);
NTSTATUS dcerpc_Delete_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Delete_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Delete *r);
struct tevent_req *dcerpc_Delete_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct dcerpc_binding_handle *h,
				      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				      struct ORPCTHIS _ORPCthis /* [in]  */,
				      const char *_wszName /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_Delete_recv(struct tevent_req *req,
			    TALLOC_CTX *mem_ctx,
			    WERROR *result);
NTSTATUS dcerpc_Delete(struct dcerpc_binding_handle *h,
		       TALLOC_CTX *mem_ctx,
		       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		       struct ORPCTHIS _ORPCthis /* [in]  */,
		       const char *_wszName /* [in] [charset(UTF16),ref] */,
		       WERROR *result);

#endif /* _HEADER_RPC_IWbemClassObject */
#ifndef _HEADER_RPC_IWbemServices
#define _HEADER_RPC_IWbemServices

extern const struct ndr_interface_table ndr_table_IWbemServices;

struct tevent_req *dcerpc_OpenNamespace_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct OpenNamespace *r);
NTSTATUS dcerpc_OpenNamespace_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_OpenNamespace_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct OpenNamespace *r);
struct tevent_req *dcerpc_OpenNamespace_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					     struct ORPCTHIS _ORPCthis /* [in]  */,
					     struct BSTR _strNamespace /* [in]  */,
					     int32_t _lFlags /* [in]  */,
					     struct MInterfacePointer *_pCtx /* [in] [ref] */,
					     struct MInterfacePointer **_ppWorkingNamespace /* [in,out] [unique] */,
					     struct MInterfacePointer **_ppResult /* [in,out] [unique] */);
NTSTATUS dcerpc_OpenNamespace_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_OpenNamespace(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			      struct ORPCTHIS _ORPCthis /* [in]  */,
			      struct BSTR _strNamespace /* [in]  */,
			      int32_t _lFlags /* [in]  */,
			      struct MInterfacePointer *_pCtx /* [in] [ref] */,
			      struct MInterfacePointer **_ppWorkingNamespace /* [in,out] [unique] */,
			      struct MInterfacePointer **_ppResult /* [in,out] [unique] */,
			      WERROR *result);

struct tevent_req *dcerpc_CancelAsyncCall_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct CancelAsyncCall *r);
NTSTATUS dcerpc_CancelAsyncCall_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_CancelAsyncCall_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct CancelAsyncCall *r);
struct tevent_req *dcerpc_CancelAsyncCall_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					       struct ORPCTHIS _ORPCthis /* [in]  */,
					       struct MInterfacePointer *_pSink /* [in] [ref] */);
NTSTATUS dcerpc_CancelAsyncCall_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_CancelAsyncCall(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				struct ORPCTHIS _ORPCthis /* [in]  */,
				struct MInterfacePointer *_pSink /* [in] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_QueryObjectSink_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct QueryObjectSink *r);
NTSTATUS dcerpc_QueryObjectSink_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_QueryObjectSink_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct QueryObjectSink *r);
struct tevent_req *dcerpc_QueryObjectSink_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					       struct ORPCTHIS _ORPCthis /* [in]  */,
					       int32_t _lFlags /* [in]  */,
					       struct MInterfacePointer **_ppResponseHandler /* [out] [ref] */);
NTSTATUS dcerpc_QueryObjectSink_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_QueryObjectSink(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				struct ORPCTHIS _ORPCthis /* [in]  */,
				int32_t _lFlags /* [in]  */,
				struct MInterfacePointer **_ppResponseHandler /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_GetObject_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetObject *r);
NTSTATUS dcerpc_GetObject_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetObject_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetObject *r);
struct tevent_req *dcerpc_GetObject_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 struct BSTR _strObjectPath /* [in]  */,
					 int32_t _lFlags /* [in]  */,
					 struct MInterfacePointer *_pCtx /* [in] [ref] */,
					 struct MInterfacePointer **_ppObject /* [in,out] [unique] */,
					 struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */);
NTSTATUS dcerpc_GetObject_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_GetObject(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  struct BSTR _strObjectPath /* [in]  */,
			  int32_t _lFlags /* [in]  */,
			  struct MInterfacePointer *_pCtx /* [in] [ref] */,
			  struct MInterfacePointer **_ppObject /* [in,out] [unique] */,
			  struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */,
			  WERROR *result);

struct tevent_req *dcerpc_GetObjectAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetObjectAsync *r);
NTSTATUS dcerpc_GetObjectAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetObjectAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetObjectAsync *r);
struct tevent_req *dcerpc_GetObjectAsync_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */,
					      struct BSTR _strObjectPath /* [in]  */,
					      int32_t _lFlags /* [in]  */,
					      struct MInterfacePointer *_pCtx /* [in] [ref] */,
					      struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_GetObjectAsync_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_GetObjectAsync(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       struct BSTR _strObjectPath /* [in]  */,
			       int32_t _lFlags /* [in]  */,
			       struct MInterfacePointer *_pCtx /* [in] [ref] */,
			       struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_PutClass_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PutClass *r);
NTSTATUS dcerpc_PutClass_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PutClass_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PutClass *r);
struct tevent_req *dcerpc_PutClass_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					struct ORPCTHIS _ORPCthis /* [in]  */,
					struct MInterfacePointer *_pObject /* [in] [ref] */,
					int32_t _lFlags /* [in]  */,
					struct MInterfacePointer *_pCtx /* [in] [ref] */,
					struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */);
NTSTATUS dcerpc_PutClass_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_PutClass(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			 struct ORPCTHIS _ORPCthis /* [in]  */,
			 struct MInterfacePointer *_pObject /* [in] [ref] */,
			 int32_t _lFlags /* [in]  */,
			 struct MInterfacePointer *_pCtx /* [in] [ref] */,
			 struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */,
			 WERROR *result);

struct tevent_req *dcerpc_PutClassAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PutClassAsync *r);
NTSTATUS dcerpc_PutClassAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PutClassAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PutClassAsync *r);
struct tevent_req *dcerpc_PutClassAsync_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					     struct ORPCTHIS _ORPCthis /* [in]  */,
					     struct MInterfacePointer *_pObject /* [in] [ref] */,
					     int32_t _lFlags /* [in]  */,
					     struct MInterfacePointer *_pCtx /* [in] [ref] */,
					     struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_PutClassAsync_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_PutClassAsync(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			      struct ORPCTHIS _ORPCthis /* [in]  */,
			      struct MInterfacePointer *_pObject /* [in] [ref] */,
			      int32_t _lFlags /* [in]  */,
			      struct MInterfacePointer *_pCtx /* [in] [ref] */,
			      struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
			      WERROR *result);

struct tevent_req *dcerpc_DeleteClass_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DeleteClass *r);
NTSTATUS dcerpc_DeleteClass_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DeleteClass_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DeleteClass *r);
struct tevent_req *dcerpc_DeleteClass_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					   struct ORPCTHIS _ORPCthis /* [in]  */,
					   struct BSTR _strClass /* [in]  */,
					   int32_t _lFlags /* [in]  */,
					   struct MInterfacePointer *_pCtx /* [in] [ref] */,
					   struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */);
NTSTATUS dcerpc_DeleteClass_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_DeleteClass(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			    struct ORPCTHIS _ORPCthis /* [in]  */,
			    struct BSTR _strClass /* [in]  */,
			    int32_t _lFlags /* [in]  */,
			    struct MInterfacePointer *_pCtx /* [in] [ref] */,
			    struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */,
			    WERROR *result);

struct tevent_req *dcerpc_DeleteClassAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DeleteClassAsync *r);
NTSTATUS dcerpc_DeleteClassAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DeleteClassAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DeleteClassAsync *r);
struct tevent_req *dcerpc_DeleteClassAsync_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */,
						struct BSTR _strClass /* [in]  */,
						int32_t _lFlags /* [in]  */,
						struct MInterfacePointer *_pCtx /* [in] [ref] */,
						struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_DeleteClassAsync_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_DeleteClassAsync(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 struct BSTR _strClass /* [in]  */,
				 int32_t _lFlags /* [in]  */,
				 struct MInterfacePointer *_pCtx /* [in] [ref] */,
				 struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
				 WERROR *result);

struct tevent_req *dcerpc_CreateClassEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct CreateClassEnum *r);
NTSTATUS dcerpc_CreateClassEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_CreateClassEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct CreateClassEnum *r);
struct tevent_req *dcerpc_CreateClassEnum_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					       struct ORPCTHIS _ORPCthis /* [in]  */,
					       struct BSTR _strSuperclass /* [in]  */,
					       int32_t _lFlags /* [in]  */,
					       struct MInterfacePointer *_pCtx /* [in] [ref] */,
					       struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_CreateClassEnum_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_CreateClassEnum(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				struct ORPCTHIS _ORPCthis /* [in]  */,
				struct BSTR _strSuperclass /* [in]  */,
				int32_t _lFlags /* [in]  */,
				struct MInterfacePointer *_pCtx /* [in] [ref] */,
				struct MInterfacePointer **_ppEnum /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_CreateClassEnumAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct CreateClassEnumAsync *r);
NTSTATUS dcerpc_CreateClassEnumAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_CreateClassEnumAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct CreateClassEnumAsync *r);
struct tevent_req *dcerpc_CreateClassEnumAsync_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						    struct ORPCTHIS _ORPCthis /* [in]  */,
						    struct BSTR _strSuperclass /* [in]  */,
						    int32_t _lFlags /* [in]  */,
						    struct MInterfacePointer *_pCtx /* [in] [ref] */,
						    struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_CreateClassEnumAsync_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_CreateClassEnumAsync(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				     struct ORPCTHIS _ORPCthis /* [in]  */,
				     struct BSTR _strSuperclass /* [in]  */,
				     int32_t _lFlags /* [in]  */,
				     struct MInterfacePointer *_pCtx /* [in] [ref] */,
				     struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
				     WERROR *result);

struct tevent_req *dcerpc_PutInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PutInstance *r);
NTSTATUS dcerpc_PutInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PutInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PutInstance *r);
struct tevent_req *dcerpc_PutInstance_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					   struct ORPCTHIS _ORPCthis /* [in]  */,
					   struct MInterfacePointer *_pInst /* [in] [ref] */,
					   int32_t _lFlags /* [in]  */,
					   struct MInterfacePointer *_pCtx /* [in] [ref] */,
					   struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */);
NTSTATUS dcerpc_PutInstance_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_PutInstance(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			    struct ORPCTHIS _ORPCthis /* [in]  */,
			    struct MInterfacePointer *_pInst /* [in] [ref] */,
			    int32_t _lFlags /* [in]  */,
			    struct MInterfacePointer *_pCtx /* [in] [ref] */,
			    struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */,
			    WERROR *result);

struct tevent_req *dcerpc_PutInstanceAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct PutInstanceAsync *r);
NTSTATUS dcerpc_PutInstanceAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_PutInstanceAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct PutInstanceAsync *r);
struct tevent_req *dcerpc_PutInstanceAsync_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */,
						struct MInterfacePointer *_pInst /* [in] [ref] */,
						int32_t _lFlags /* [in]  */,
						struct MInterfacePointer *_pCtx /* [in] [ref] */,
						struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_PutInstanceAsync_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_PutInstanceAsync(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 struct MInterfacePointer *_pInst /* [in] [ref] */,
				 int32_t _lFlags /* [in]  */,
				 struct MInterfacePointer *_pCtx /* [in] [ref] */,
				 struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
				 WERROR *result);

struct tevent_req *dcerpc_DeleteInstance_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DeleteInstance *r);
NTSTATUS dcerpc_DeleteInstance_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DeleteInstance_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DeleteInstance *r);
struct tevent_req *dcerpc_DeleteInstance_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */,
					      struct BSTR _strObjectPath /* [in]  */,
					      int32_t _lFlags /* [in]  */,
					      struct MInterfacePointer *_pCtx /* [in] [ref] */,
					      struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */);
NTSTATUS dcerpc_DeleteInstance_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_DeleteInstance(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       struct BSTR _strObjectPath /* [in]  */,
			       int32_t _lFlags /* [in]  */,
			       struct MInterfacePointer *_pCtx /* [in] [ref] */,
			       struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */,
			       WERROR *result);

struct tevent_req *dcerpc_DeleteInstanceAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DeleteInstanceAsync *r);
NTSTATUS dcerpc_DeleteInstanceAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DeleteInstanceAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DeleteInstanceAsync *r);
struct tevent_req *dcerpc_DeleteInstanceAsync_send(TALLOC_CTX *mem_ctx,
						   struct tevent_context *ev,
						   struct dcerpc_binding_handle *h,
						   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						   struct ORPCTHIS _ORPCthis /* [in]  */,
						   struct BSTR _strObjectPath /* [in]  */,
						   int32_t _lFlags /* [in]  */,
						   struct MInterfacePointer *_pCtx /* [in] [ref] */,
						   struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_DeleteInstanceAsync_recv(struct tevent_req *req,
					 TALLOC_CTX *mem_ctx,
					 WERROR *result);
NTSTATUS dcerpc_DeleteInstanceAsync(struct dcerpc_binding_handle *h,
				    TALLOC_CTX *mem_ctx,
				    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				    struct ORPCTHIS _ORPCthis /* [in]  */,
				    struct BSTR _strObjectPath /* [in]  */,
				    int32_t _lFlags /* [in]  */,
				    struct MInterfacePointer *_pCtx /* [in] [ref] */,
				    struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
				    WERROR *result);

struct tevent_req *dcerpc_CreateInstanceEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct CreateInstanceEnum *r);
NTSTATUS dcerpc_CreateInstanceEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_CreateInstanceEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct CreateInstanceEnum *r);
struct tevent_req *dcerpc_CreateInstanceEnum_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						  struct ORPCTHIS _ORPCthis /* [in]  */,
						  struct BSTR _strFilter /* [in]  */,
						  int32_t _lFlags /* [in]  */,
						  struct MInterfacePointer *_pCtx /* [in] [unique] */,
						  struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_CreateInstanceEnum_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_CreateInstanceEnum(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				   struct ORPCTHIS _ORPCthis /* [in]  */,
				   struct BSTR _strFilter /* [in]  */,
				   int32_t _lFlags /* [in]  */,
				   struct MInterfacePointer *_pCtx /* [in] [unique] */,
				   struct MInterfacePointer **_ppEnum /* [out] [ref] */,
				   WERROR *result);

struct tevent_req *dcerpc_CreateInstanceEnumAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct CreateInstanceEnumAsync *r);
NTSTATUS dcerpc_CreateInstanceEnumAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_CreateInstanceEnumAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct CreateInstanceEnumAsync *r);
struct tevent_req *dcerpc_CreateInstanceEnumAsync_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						       struct ORPCTHIS _ORPCthis /* [in]  */,
						       struct BSTR _strSuperClass /* [in]  */,
						       int32_t _lFlags /* [in]  */,
						       struct MInterfacePointer *_pCtx /* [in] [ref] */,
						       struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_CreateInstanceEnumAsync_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_CreateInstanceEnumAsync(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					struct ORPCTHIS _ORPCthis /* [in]  */,
					struct BSTR _strSuperClass /* [in]  */,
					int32_t _lFlags /* [in]  */,
					struct MInterfacePointer *_pCtx /* [in] [ref] */,
					struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
					WERROR *result);

struct tevent_req *dcerpc_ExecQuery_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ExecQuery *r);
NTSTATUS dcerpc_ExecQuery_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ExecQuery_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ExecQuery *r);
struct tevent_req *dcerpc_ExecQuery_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 struct BSTR _strQueryLanguage /* [in]  */,
					 struct BSTR _strQuery /* [in]  */,
					 int32_t _lFlags /* [in]  */,
					 struct MInterfacePointer *_pCtx /* [in] [unique] */,
					 struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_ExecQuery_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_ExecQuery(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  struct BSTR _strQueryLanguage /* [in]  */,
			  struct BSTR _strQuery /* [in]  */,
			  int32_t _lFlags /* [in]  */,
			  struct MInterfacePointer *_pCtx /* [in] [unique] */,
			  struct MInterfacePointer **_ppEnum /* [out] [ref] */,
			  WERROR *result);

struct tevent_req *dcerpc_ExecQueryAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ExecQueryAsync *r);
NTSTATUS dcerpc_ExecQueryAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ExecQueryAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ExecQueryAsync *r);
struct tevent_req *dcerpc_ExecQueryAsync_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */,
					      struct BSTR _strQueryLanguage /* [in]  */,
					      struct BSTR _strQuery /* [in]  */,
					      int32_t _lFlags /* [in]  */,
					      struct MInterfacePointer *_pCtx /* [in] [ref] */,
					      struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_ExecQueryAsync_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_ExecQueryAsync(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       struct BSTR _strQueryLanguage /* [in]  */,
			       struct BSTR _strQuery /* [in]  */,
			       int32_t _lFlags /* [in]  */,
			       struct MInterfacePointer *_pCtx /* [in] [ref] */,
			       struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
			       WERROR *result);

struct tevent_req *dcerpc_ExecNotificationQuery_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ExecNotificationQuery *r);
NTSTATUS dcerpc_ExecNotificationQuery_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ExecNotificationQuery_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ExecNotificationQuery *r);
struct tevent_req *dcerpc_ExecNotificationQuery_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						     struct ORPCTHIS _ORPCthis /* [in]  */,
						     struct BSTR _strQueryLanguage /* [in]  */,
						     struct BSTR _strQuery /* [in]  */,
						     int32_t _lFlags /* [in]  */,
						     struct MInterfacePointer *_pCtx /* [in] [unique] */,
						     struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_ExecNotificationQuery_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_ExecNotificationQuery(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				      struct ORPCTHIS _ORPCthis /* [in]  */,
				      struct BSTR _strQueryLanguage /* [in]  */,
				      struct BSTR _strQuery /* [in]  */,
				      int32_t _lFlags /* [in]  */,
				      struct MInterfacePointer *_pCtx /* [in] [unique] */,
				      struct MInterfacePointer **_ppEnum /* [out] [ref] */,
				      WERROR *result);

struct tevent_req *dcerpc_ExecNotificationQueryAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ExecNotificationQueryAsync *r);
NTSTATUS dcerpc_ExecNotificationQueryAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ExecNotificationQueryAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ExecNotificationQueryAsync *r);
struct tevent_req *dcerpc_ExecNotificationQueryAsync_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
							  struct ORPCTHIS _ORPCthis /* [in]  */,
							  struct BSTR _strQueryLanguage /* [in]  */,
							  struct BSTR _strQuery /* [in]  */,
							  int32_t _lFlags /* [in]  */,
							  struct MInterfacePointer *_pCtx /* [in] [ref] */,
							  struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_ExecNotificationQueryAsync_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_ExecNotificationQueryAsync(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					   struct ORPCTHIS _ORPCthis /* [in]  */,
					   struct BSTR _strQueryLanguage /* [in]  */,
					   struct BSTR _strQuery /* [in]  */,
					   int32_t _lFlags /* [in]  */,
					   struct MInterfacePointer *_pCtx /* [in] [ref] */,
					   struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_ExecMethod_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ExecMethod *r);
NTSTATUS dcerpc_ExecMethod_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ExecMethod_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ExecMethod *r);
struct tevent_req *dcerpc_ExecMethod_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					  struct ORPCTHIS _ORPCthis /* [in]  */,
					  struct BSTR _strObjectPath /* [in]  */,
					  struct BSTR _strMethodName /* [in]  */,
					  int32_t _lFlags /* [in]  */,
					  struct MInterfacePointer *_pCtx /* [in] [unique] */,
					  struct MInterfacePointer *_pInParams /* [in] [unique] */,
					  struct MInterfacePointer **_ppOutParams /* [in,out] [unique] */,
					  struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */);
NTSTATUS dcerpc_ExecMethod_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_ExecMethod(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			   struct ORPCTHIS _ORPCthis /* [in]  */,
			   struct BSTR _strObjectPath /* [in]  */,
			   struct BSTR _strMethodName /* [in]  */,
			   int32_t _lFlags /* [in]  */,
			   struct MInterfacePointer *_pCtx /* [in] [unique] */,
			   struct MInterfacePointer *_pInParams /* [in] [unique] */,
			   struct MInterfacePointer **_ppOutParams /* [in,out] [unique] */,
			   struct MInterfacePointer **_ppCallResult /* [in,out] [unique] */,
			   WERROR *result);

struct tevent_req *dcerpc_ExecMethodAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ExecMethodAsync *r);
NTSTATUS dcerpc_ExecMethodAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ExecMethodAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ExecMethodAsync *r);
struct tevent_req *dcerpc_ExecMethodAsync_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					       struct ORPCTHIS _ORPCthis /* [in]  */,
					       struct BSTR _strObjectPath /* [in]  */,
					       struct BSTR _strMethodName /* [in]  */,
					       uint32_t _lFlags /* [in]  */,
					       struct MInterfacePointer *_pCtx /* [in] [ref] */,
					       struct MInterfacePointer *_pInParams /* [in] [ref] */,
					       struct MInterfacePointer *_pResponseHandler /* [in] [ref] */);
NTSTATUS dcerpc_ExecMethodAsync_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_ExecMethodAsync(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				struct ORPCTHIS _ORPCthis /* [in]  */,
				struct BSTR _strObjectPath /* [in]  */,
				struct BSTR _strMethodName /* [in]  */,
				uint32_t _lFlags /* [in]  */,
				struct MInterfacePointer *_pCtx /* [in] [ref] */,
				struct MInterfacePointer *_pInParams /* [in] [ref] */,
				struct MInterfacePointer *_pResponseHandler /* [in] [ref] */,
				WERROR *result);

#endif /* _HEADER_RPC_IWbemServices */
#ifndef _HEADER_RPC_IEnumWbemClassObject
#define _HEADER_RPC_IEnumWbemClassObject

extern const struct ndr_interface_table ndr_table_IEnumWbemClassObject;

struct tevent_req *dcerpc_Reset_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Reset *r);
NTSTATUS dcerpc_Reset_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Reset_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Reset *r);
struct tevent_req *dcerpc_Reset_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct dcerpc_binding_handle *h,
				     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				     struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_Reset_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   WERROR *result);
NTSTATUS dcerpc_Reset(struct dcerpc_binding_handle *h,
		      TALLOC_CTX *mem_ctx,
		      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		      struct ORPCTHIS _ORPCthis /* [in]  */,
		      WERROR *result);

struct tevent_req *dcerpc_IEnumWbemClassObject_Next_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct IEnumWbemClassObject_Next *r);
NTSTATUS dcerpc_IEnumWbemClassObject_Next_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_IEnumWbemClassObject_Next_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct IEnumWbemClassObject_Next *r);
struct tevent_req *dcerpc_IEnumWbemClassObject_Next_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
							 struct ORPCTHIS _ORPCthis /* [in]  */,
							 int32_t _lTimeout /* [in]  */,
							 uint32_t _uCount /* [in]  */,
							 struct MInterfacePointer **_apObjects /* [out] [length_is(*puReturned),ref,size_is(uCount)] */,
							 uint32_t *_puReturned /* [out] [ref] */);
NTSTATUS dcerpc_IEnumWbemClassObject_Next_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_IEnumWbemClassObject_Next(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					  struct ORPCTHIS _ORPCthis /* [in]  */,
					  int32_t _lTimeout /* [in]  */,
					  uint32_t _uCount /* [in]  */,
					  struct MInterfacePointer **_apObjects /* [out] [length_is(*puReturned),ref,size_is(uCount)] */,
					  uint32_t *_puReturned /* [out] [ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_NextAsync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NextAsync *r);
NTSTATUS dcerpc_NextAsync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NextAsync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NextAsync *r);
struct tevent_req *dcerpc_NextAsync_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 uint32_t _uCount /* [in]  */,
					 struct MInterfacePointer *_pSink /* [in] [ref] */);
NTSTATUS dcerpc_NextAsync_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_NextAsync(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  uint32_t _uCount /* [in]  */,
			  struct MInterfacePointer *_pSink /* [in] [ref] */,
			  WERROR *result);

struct tevent_req *dcerpc_IEnumWbemClassObject_Clone_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct IEnumWbemClassObject_Clone *r);
NTSTATUS dcerpc_IEnumWbemClassObject_Clone_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_IEnumWbemClassObject_Clone_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct IEnumWbemClassObject_Clone *r);
struct tevent_req *dcerpc_IEnumWbemClassObject_Clone_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
							  struct ORPCTHIS _ORPCthis /* [in]  */,
							  struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_IEnumWbemClassObject_Clone_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_IEnumWbemClassObject_Clone(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					   struct ORPCTHIS _ORPCthis /* [in]  */,
					   struct MInterfacePointer **_ppEnum /* [out] [ref] */,
					   WERROR *result);

struct tevent_req *dcerpc_Skip_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Skip *r);
NTSTATUS dcerpc_Skip_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Skip_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Skip *r);
struct tevent_req *dcerpc_Skip_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct dcerpc_binding_handle *h,
				    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				    struct ORPCTHIS _ORPCthis /* [in]  */,
				    int32_t _lTimeout /* [in]  */,
				    uint32_t _nCount /* [in]  */);
NTSTATUS dcerpc_Skip_recv(struct tevent_req *req,
			  TALLOC_CTX *mem_ctx,
			  WERROR *result);
NTSTATUS dcerpc_Skip(struct dcerpc_binding_handle *h,
		     TALLOC_CTX *mem_ctx,
		     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		     struct ORPCTHIS _ORPCthis /* [in]  */,
		     int32_t _lTimeout /* [in]  */,
		     uint32_t _nCount /* [in]  */,
		     WERROR *result);

#endif /* _HEADER_RPC_IEnumWbemClassObject */
#ifndef _HEADER_RPC_IWbemContext
#define _HEADER_RPC_IWbemContext

extern const struct ndr_interface_table ndr_table_IWbemContext;

struct tevent_req *dcerpc_Clone_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Clone *r);
NTSTATUS dcerpc_Clone_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Clone_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Clone *r);
struct tevent_req *dcerpc_Clone_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct dcerpc_binding_handle *h,
				     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				     struct ORPCTHIS _ORPCthis /* [in]  */,
				     struct MInterfacePointer **_ppNewCopy /* [out] [ref] */);
NTSTATUS dcerpc_Clone_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   WERROR *result);
NTSTATUS dcerpc_Clone(struct dcerpc_binding_handle *h,
		      TALLOC_CTX *mem_ctx,
		      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		      struct ORPCTHIS _ORPCthis /* [in]  */,
		      struct MInterfacePointer **_ppNewCopy /* [out] [ref] */,
		      WERROR *result);

struct tevent_req *dcerpc_GetNames_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetNames *r);
NTSTATUS dcerpc_GetNames_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetNames_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetNames *r);
struct tevent_req *dcerpc_GetNames_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_GetNames_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_GetNames(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			 struct ORPCTHIS _ORPCthis /* [in]  */,
			 WERROR *result);

struct tevent_req *dcerpc_BeginEnumeration_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct BeginEnumeration *r);
NTSTATUS dcerpc_BeginEnumeration_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_BeginEnumeration_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct BeginEnumeration *r);
struct tevent_req *dcerpc_BeginEnumeration_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */,
						int32_t _lFlags /* [in]  */);
NTSTATUS dcerpc_BeginEnumeration_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_BeginEnumeration(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 int32_t _lFlags /* [in]  */,
				 WERROR *result);

struct tevent_req *dcerpc_Next_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Next *r);
NTSTATUS dcerpc_Next_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Next_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Next *r);
struct tevent_req *dcerpc_Next_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct dcerpc_binding_handle *h,
				    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				    struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_Next_recv(struct tevent_req *req,
			  TALLOC_CTX *mem_ctx,
			  WERROR *result);
NTSTATUS dcerpc_Next(struct dcerpc_binding_handle *h,
		     TALLOC_CTX *mem_ctx,
		     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		     struct ORPCTHIS _ORPCthis /* [in]  */,
		     WERROR *result);

struct tevent_req *dcerpc_EndEnumeration_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EndEnumeration *r);
NTSTATUS dcerpc_EndEnumeration_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EndEnumeration_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EndEnumeration *r);
struct tevent_req *dcerpc_EndEnumeration_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					      struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_EndEnumeration_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx,
				    WERROR *result);
NTSTATUS dcerpc_EndEnumeration(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			       struct ORPCTHIS _ORPCthis /* [in]  */,
			       WERROR *result);

struct tevent_req *dcerpc_SetValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct SetValue *r);
NTSTATUS dcerpc_SetValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_SetValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct SetValue *r);
struct tevent_req *dcerpc_SetValue_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_SetValue_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_SetValue(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			 struct ORPCTHIS _ORPCthis /* [in]  */,
			 WERROR *result);

struct tevent_req *dcerpc_GetValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetValue *r);
NTSTATUS dcerpc_GetValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetValue *r);
struct tevent_req *dcerpc_GetValue_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_GetValue_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_GetValue(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			 struct ORPCTHIS _ORPCthis /* [in]  */,
			 WERROR *result);

struct tevent_req *dcerpc_DeleteValue_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DeleteValue *r);
NTSTATUS dcerpc_DeleteValue_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DeleteValue_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DeleteValue *r);
struct tevent_req *dcerpc_DeleteValue_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					   struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_DeleteValue_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_DeleteValue(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			    struct ORPCTHIS _ORPCthis /* [in]  */,
			    WERROR *result);

struct tevent_req *dcerpc_DeleteAll_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct DeleteAll *r);
NTSTATUS dcerpc_DeleteAll_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_DeleteAll_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct DeleteAll *r);
struct tevent_req *dcerpc_DeleteAll_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */);
NTSTATUS dcerpc_DeleteAll_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_DeleteAll(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  WERROR *result);

#endif /* _HEADER_RPC_IWbemContext */
#ifndef _HEADER_RPC_IWbemLevel1Login
#define _HEADER_RPC_IWbemLevel1Login

extern const struct ndr_interface_table ndr_table_IWbemLevel1Login;

struct tevent_req *dcerpc_EstablishPosition_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EstablishPosition *r);
NTSTATUS dcerpc_EstablishPosition_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EstablishPosition_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EstablishPosition *r);
struct tevent_req *dcerpc_EstablishPosition_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						 struct ORPCTHIS _ORPCthis /* [in]  */,
						 const char *_wszLocaleList /* [in] [unique,charset(UTF16)] */,
						 uint32_t _dwNumLocales /* [in]  */,
						 uint32_t *_reserved /* [out] [ref] */);
NTSTATUS dcerpc_EstablishPosition_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_EstablishPosition(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				  struct ORPCTHIS _ORPCthis /* [in]  */,
				  const char *_wszLocaleList /* [in] [unique,charset(UTF16)] */,
				  uint32_t _dwNumLocales /* [in]  */,
				  uint32_t *_reserved /* [out] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_RequestChallenge_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct RequestChallenge *r);
NTSTATUS dcerpc_RequestChallenge_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_RequestChallenge_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct RequestChallenge *r);
struct tevent_req *dcerpc_RequestChallenge_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						struct ORPCTHIS _ORPCthis /* [in]  */,
						const char *_wszNetworkResource /* [in] [charset(UTF16),unique] */,
						const char *_wszUser /* [in] [unique,charset(UTF16)] */,
						uint8_t *_Nonce /* [out] [length_is(16),ref,size_is(16)] */);
NTSTATUS dcerpc_RequestChallenge_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx,
				      WERROR *result);
NTSTATUS dcerpc_RequestChallenge(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				 struct ORPCTHIS _ORPCthis /* [in]  */,
				 const char *_wszNetworkResource /* [in] [charset(UTF16),unique] */,
				 const char *_wszUser /* [in] [unique,charset(UTF16)] */,
				 uint8_t *_Nonce /* [out] [length_is(16),ref,size_is(16)] */,
				 WERROR *result);

struct tevent_req *dcerpc_WBEMLogin_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct WBEMLogin *r);
NTSTATUS dcerpc_WBEMLogin_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_WBEMLogin_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct WBEMLogin *r);
struct tevent_req *dcerpc_WBEMLogin_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 const char *_wszPreferredLocale /* [in] [unique,charset(UTF16)] */,
					 uint8_t *_AccessToken /* [in] [length_is(16),unique,size_is(16)] */,
					 int32_t _lFlags /* [in]  */,
					 struct MInterfacePointer *_pCtx /* [in] [ref] */,
					 struct MInterfacePointer **_ppNamespace /* [out] [ref] */);
NTSTATUS dcerpc_WBEMLogin_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_WBEMLogin(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  const char *_wszPreferredLocale /* [in] [unique,charset(UTF16)] */,
			  uint8_t *_AccessToken /* [in] [length_is(16),unique,size_is(16)] */,
			  int32_t _lFlags /* [in]  */,
			  struct MInterfacePointer *_pCtx /* [in] [ref] */,
			  struct MInterfacePointer **_ppNamespace /* [out] [ref] */,
			  WERROR *result);

struct tevent_req *dcerpc_NTLMLogin_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct NTLMLogin *r);
NTSTATUS dcerpc_NTLMLogin_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_NTLMLogin_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct NTLMLogin *r);
struct tevent_req *dcerpc_NTLMLogin_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 const char *_wszNetworkResource /* [in] [unique,charset(UTF16)] */,
					 const char *_wszPreferredLocale /* [in] [unique,charset(UTF16)] */,
					 int32_t _lFlags /* [in]  */,
					 struct MInterfacePointer *_pCtx /* [in] [unique] */,
					 struct MInterfacePointer **_ppNamespace /* [out] [ref] */);
NTSTATUS dcerpc_NTLMLogin_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_NTLMLogin(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  const char *_wszNetworkResource /* [in] [unique,charset(UTF16)] */,
			  const char *_wszPreferredLocale /* [in] [unique,charset(UTF16)] */,
			  int32_t _lFlags /* [in]  */,
			  struct MInterfacePointer *_pCtx /* [in] [unique] */,
			  struct MInterfacePointer **_ppNamespace /* [out] [ref] */,
			  WERROR *result);

#endif /* _HEADER_RPC_IWbemLevel1Login */
#ifndef _HEADER_RPC_IWbemWCOSmartEnum
#define _HEADER_RPC_IWbemWCOSmartEnum

extern const struct ndr_interface_table ndr_table_IWbemWCOSmartEnum;

struct tevent_req *dcerpc_IWbemWCOSmartEnum_Next_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct IWbemWCOSmartEnum_Next *r);
NTSTATUS dcerpc_IWbemWCOSmartEnum_Next_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_IWbemWCOSmartEnum_Next_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct IWbemWCOSmartEnum_Next *r);
struct tevent_req *dcerpc_IWbemWCOSmartEnum_Next_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						      struct ORPCTHIS _ORPCthis /* [in]  */,
						      struct GUID *_gEWCO /* [in] [ref] */,
						      uint32_t _lTimeOut /* [in]  */,
						      uint32_t _uCount /* [in]  */,
						      uint32_t _unknown /* [in]  */,
						      struct GUID *_gWCO /* [in] [ref] */,
						      uint32_t *_puReturned /* [out] [ref] */,
						      uint32_t *_pSize /* [out] [ref] */,
						      uint8_t **_pData /* [out] [ref,size_is(,*pSize)] */);
NTSTATUS dcerpc_IWbemWCOSmartEnum_Next_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_IWbemWCOSmartEnum_Next(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				       struct ORPCTHIS _ORPCthis /* [in]  */,
				       struct GUID *_gEWCO /* [in] [ref] */,
				       uint32_t _lTimeOut /* [in]  */,
				       uint32_t _uCount /* [in]  */,
				       uint32_t _unknown /* [in]  */,
				       struct GUID *_gWCO /* [in] [ref] */,
				       uint32_t *_puReturned /* [out] [ref] */,
				       uint32_t *_pSize /* [out] [ref] */,
				       uint8_t **_pData /* [out] [ref,size_is(,*pSize)] */,
				       WERROR *result);

#endif /* _HEADER_RPC_IWbemWCOSmartEnum */
#ifndef _HEADER_RPC_IWbemFetchSmartEnum
#define _HEADER_RPC_IWbemFetchSmartEnum

extern const struct ndr_interface_table ndr_table_IWbemFetchSmartEnum;

struct tevent_req *dcerpc_Fetch_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Fetch *r);
NTSTATUS dcerpc_Fetch_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Fetch_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Fetch *r);
struct tevent_req *dcerpc_Fetch_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct dcerpc_binding_handle *h,
				     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				     struct ORPCTHIS _ORPCthis /* [in]  */,
				     struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_Fetch_recv(struct tevent_req *req,
			   TALLOC_CTX *mem_ctx,
			   WERROR *result);
NTSTATUS dcerpc_Fetch(struct dcerpc_binding_handle *h,
		      TALLOC_CTX *mem_ctx,
		      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		      struct ORPCTHIS _ORPCthis /* [in]  */,
		      struct MInterfacePointer **_ppEnum /* [out] [ref] */,
		      WERROR *result);

struct tevent_req *dcerpc_Test_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Test *r);
NTSTATUS dcerpc_Test_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Test_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Test *r);
struct tevent_req *dcerpc_Test_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct dcerpc_binding_handle *h,
				    struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				    struct ORPCTHIS _ORPCthis /* [in]  */,
				    struct MInterfacePointer **_ppEnum /* [out] [ref] */);
NTSTATUS dcerpc_Test_recv(struct tevent_req *req,
			  TALLOC_CTX *mem_ctx,
			  WERROR *result);
NTSTATUS dcerpc_Test(struct dcerpc_binding_handle *h,
		     TALLOC_CTX *mem_ctx,
		     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
		     struct ORPCTHIS _ORPCthis /* [in]  */,
		     struct MInterfacePointer **_ppEnum /* [out] [ref] */,
		     WERROR *result);

#endif /* _HEADER_RPC_IWbemFetchSmartEnum */
#ifndef _HEADER_RPC_IWbemCallResult
#define _HEADER_RPC_IWbemCallResult

extern const struct ndr_interface_table ndr_table_IWbemCallResult;

struct tevent_req *dcerpc_GetResultObject_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetResultObject *r);
NTSTATUS dcerpc_GetResultObject_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetResultObject_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetResultObject *r);
struct tevent_req *dcerpc_GetResultObject_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					       struct ORPCTHIS _ORPCthis /* [in]  */,
					       int32_t _lTimeout /* [in]  */,
					       struct MInterfacePointer **_ppResultObject /* [out] [ref] */);
NTSTATUS dcerpc_GetResultObject_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_GetResultObject(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				struct ORPCTHIS _ORPCthis /* [in]  */,
				int32_t _lTimeout /* [in]  */,
				struct MInterfacePointer **_ppResultObject /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_GetResultString_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetResultString *r);
NTSTATUS dcerpc_GetResultString_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetResultString_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetResultString *r);
struct tevent_req *dcerpc_GetResultString_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					       struct ORPCTHIS _ORPCthis /* [in]  */,
					       int32_t _lTimeout /* [in]  */,
					       struct BSTR *_pstrResultString /* [out] [ref] */);
NTSTATUS dcerpc_GetResultString_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx,
				     WERROR *result);
NTSTATUS dcerpc_GetResultString(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				struct ORPCTHIS _ORPCthis /* [in]  */,
				int32_t _lTimeout /* [in]  */,
				struct BSTR *_pstrResultString /* [out] [ref] */,
				WERROR *result);

struct tevent_req *dcerpc_GetResultServices_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetResultServices *r);
NTSTATUS dcerpc_GetResultServices_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetResultServices_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetResultServices *r);
struct tevent_req *dcerpc_GetResultServices_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
						 struct ORPCTHIS _ORPCthis /* [in]  */,
						 int32_t _lTimeout /* [in]  */,
						 struct MInterfacePointer **_ppServices /* [out] [ref] */);
NTSTATUS dcerpc_GetResultServices_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_GetResultServices(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
				  struct ORPCTHIS _ORPCthis /* [in]  */,
				  int32_t _lTimeout /* [in]  */,
				  struct MInterfacePointer **_ppServices /* [out] [ref] */,
				  WERROR *result);

struct tevent_req *dcerpc_GetCallStatus_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct GetCallStatus *r);
NTSTATUS dcerpc_GetCallStatus_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_GetCallStatus_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct GetCallStatus *r);
struct tevent_req *dcerpc_GetCallStatus_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct dcerpc_binding_handle *h,
					     struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					     struct ORPCTHIS _ORPCthis /* [in]  */,
					     int32_t _lTimeout /* [in]  */,
					     int32_t *_plStatus /* [out] [ref] */);
NTSTATUS dcerpc_GetCallStatus_recv(struct tevent_req *req,
				   TALLOC_CTX *mem_ctx,
				   WERROR *result);
NTSTATUS dcerpc_GetCallStatus(struct dcerpc_binding_handle *h,
			      TALLOC_CTX *mem_ctx,
			      struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			      struct ORPCTHIS _ORPCthis /* [in]  */,
			      int32_t _lTimeout /* [in]  */,
			      int32_t *_plStatus /* [out] [ref] */,
			      WERROR *result);

#endif /* _HEADER_RPC_IWbemCallResult */
#ifndef _HEADER_RPC_IWbemObjectSink
#define _HEADER_RPC_IWbemObjectSink

extern const struct ndr_interface_table ndr_table_IWbemObjectSink;

struct tevent_req *dcerpc_SetStatus_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct SetStatus *r);
NTSTATUS dcerpc_SetStatus_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_SetStatus_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct SetStatus *r);
struct tevent_req *dcerpc_SetStatus_send(TALLOC_CTX *mem_ctx,
					 struct tevent_context *ev,
					 struct dcerpc_binding_handle *h,
					 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					 struct ORPCTHIS _ORPCthis /* [in]  */,
					 int32_t _lFlags /* [in]  */,
					 WERROR _hResult /* [in]  */,
					 struct BSTR _strParam /* [in]  */,
					 struct MInterfacePointer *_pObjParam /* [in] [ref] */);
NTSTATUS dcerpc_SetStatus_recv(struct tevent_req *req,
			       TALLOC_CTX *mem_ctx,
			       WERROR *result);
NTSTATUS dcerpc_SetStatus(struct dcerpc_binding_handle *h,
			  TALLOC_CTX *mem_ctx,
			  struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			  struct ORPCTHIS _ORPCthis /* [in]  */,
			  int32_t _lFlags /* [in]  */,
			  WERROR _hResult /* [in]  */,
			  struct BSTR _strParam /* [in]  */,
			  struct MInterfacePointer *_pObjParam /* [in] [ref] */,
			  WERROR *result);

struct tevent_req *dcerpc_Indicate_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct Indicate *r);
NTSTATUS dcerpc_Indicate_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_Indicate_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct Indicate *r);
struct tevent_req *dcerpc_Indicate_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct dcerpc_binding_handle *h,
					struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
					struct ORPCTHIS _ORPCthis /* [in]  */,
					int32_t _lObjectCount /* [in]  */,
					struct MInterfacePointer **_apObjArray /* [in] [ref,size_is(lObjectCount)] */);
NTSTATUS dcerpc_Indicate_recv(struct tevent_req *req,
			      TALLOC_CTX *mem_ctx,
			      WERROR *result);
NTSTATUS dcerpc_Indicate(struct dcerpc_binding_handle *h,
			 TALLOC_CTX *mem_ctx,
			 struct ORPCTHAT *_ORPCthat /* [out] [ref] */,
			 struct ORPCTHIS _ORPCthis /* [in]  */,
			 int32_t _lObjectCount /* [in]  */,
			 struct MInterfacePointer **_apObjArray /* [in] [ref,size_is(lObjectCount)] */,
			 WERROR *result);

#endif /* _HEADER_RPC_IWbemObjectSink */
