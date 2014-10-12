#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/efs.h"
#ifndef _HEADER_RPC_efs
#define _HEADER_RPC_efs

extern const struct ndr_interface_table ndr_table_efs;

struct tevent_req *dcerpc_EfsRpcOpenFileRaw_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcOpenFileRaw *r);
NTSTATUS dcerpc_EfsRpcOpenFileRaw_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcOpenFileRaw_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcOpenFileRaw *r);
struct tevent_req *dcerpc_EfsRpcOpenFileRaw_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct policy_handle *_pvContext /* [out] [ref] */,
						 const char *_FileName /* [in] [charset(UTF16)] */,
						 uint32_t _Flags /* [in]  */);
NTSTATUS dcerpc_EfsRpcOpenFileRaw_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_EfsRpcOpenFileRaw(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *_pvContext /* [out] [ref] */,
				  const char *_FileName /* [in] [charset(UTF16)] */,
				  uint32_t _Flags /* [in]  */,
				  WERROR *result);

struct tevent_req *dcerpc_EfsRpcCloseRaw_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcCloseRaw *r);
NTSTATUS dcerpc_EfsRpcCloseRaw_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcCloseRaw_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcCloseRaw *r);
struct tevent_req *dcerpc_EfsRpcCloseRaw_send(TALLOC_CTX *mem_ctx,
					      struct tevent_context *ev,
					      struct dcerpc_binding_handle *h,
					      struct policy_handle *_pvContext /* [in,out] [ref] */);
NTSTATUS dcerpc_EfsRpcCloseRaw_recv(struct tevent_req *req,
				    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcCloseRaw(struct dcerpc_binding_handle *h,
			       TALLOC_CTX *mem_ctx,
			       struct policy_handle *_pvContext /* [in,out] [ref] */);

struct tevent_req *dcerpc_EfsRpcEncryptFileSrv_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcEncryptFileSrv *r);
NTSTATUS dcerpc_EfsRpcEncryptFileSrv_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcEncryptFileSrv_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcEncryptFileSrv *r);
struct tevent_req *dcerpc_EfsRpcEncryptFileSrv_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_Filename /* [in] [charset(UTF16)] */);
NTSTATUS dcerpc_EfsRpcEncryptFileSrv_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_EfsRpcEncryptFileSrv(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_Filename /* [in] [charset(UTF16)] */,
				     WERROR *result);

struct tevent_req *dcerpc_EfsRpcDecryptFileSrv_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcDecryptFileSrv *r);
NTSTATUS dcerpc_EfsRpcDecryptFileSrv_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcDecryptFileSrv_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcDecryptFileSrv *r);
struct tevent_req *dcerpc_EfsRpcDecryptFileSrv_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct dcerpc_binding_handle *h,
						    const char *_FileName /* [in] [charset(UTF16)] */,
						    uint32_t _Reserved /* [in]  */);
NTSTATUS dcerpc_EfsRpcDecryptFileSrv_recv(struct tevent_req *req,
					  TALLOC_CTX *mem_ctx,
					  WERROR *result);
NTSTATUS dcerpc_EfsRpcDecryptFileSrv(struct dcerpc_binding_handle *h,
				     TALLOC_CTX *mem_ctx,
				     const char *_FileName /* [in] [charset(UTF16)] */,
				     uint32_t _Reserved /* [in]  */,
				     WERROR *result);

struct tevent_req *dcerpc_EfsRpcQueryUsersOnFile_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcQueryUsersOnFile *r);
NTSTATUS dcerpc_EfsRpcQueryUsersOnFile_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcQueryUsersOnFile_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcQueryUsersOnFile *r);
struct tevent_req *dcerpc_EfsRpcQueryUsersOnFile_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_FileName /* [in] [charset(UTF16)] */,
						      struct ENCRYPTION_CERTIFICATE_HASH_LIST **_pUsers /* [out] [ref,unique] */);
NTSTATUS dcerpc_EfsRpcQueryUsersOnFile_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_EfsRpcQueryUsersOnFile(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_FileName /* [in] [charset(UTF16)] */,
				       struct ENCRYPTION_CERTIFICATE_HASH_LIST **_pUsers /* [out] [ref,unique] */,
				       WERROR *result);

struct tevent_req *dcerpc_EfsRpcQueryRecoveryAgents_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcQueryRecoveryAgents *r);
NTSTATUS dcerpc_EfsRpcQueryRecoveryAgents_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcQueryRecoveryAgents_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcQueryRecoveryAgents *r);
struct tevent_req *dcerpc_EfsRpcQueryRecoveryAgents_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_FileName /* [in] [charset(UTF16)] */,
							 struct ENCRYPTION_CERTIFICATE_HASH_LIST **_pRecoveryAgents /* [out] [ref,unique] */);
NTSTATUS dcerpc_EfsRpcQueryRecoveryAgents_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_EfsRpcQueryRecoveryAgents(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_FileName /* [in] [charset(UTF16)] */,
					  struct ENCRYPTION_CERTIFICATE_HASH_LIST **_pRecoveryAgents /* [out] [ref,unique] */,
					  WERROR *result);

struct tevent_req *dcerpc_EfsRpcSetFileEncryptionKey_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct EfsRpcSetFileEncryptionKey *r);
NTSTATUS dcerpc_EfsRpcSetFileEncryptionKey_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_EfsRpcSetFileEncryptionKey_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct EfsRpcSetFileEncryptionKey *r);
struct tevent_req *dcerpc_EfsRpcSetFileEncryptionKey_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct ENCRYPTION_CERTIFICATE *_pEncryptionCertificate /* [in] [unique] */);
NTSTATUS dcerpc_EfsRpcSetFileEncryptionKey_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_EfsRpcSetFileEncryptionKey(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct ENCRYPTION_CERTIFICATE *_pEncryptionCertificate /* [in] [unique] */,
					   WERROR *result);

#endif /* _HEADER_RPC_efs */
