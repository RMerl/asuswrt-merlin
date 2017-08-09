#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/oxidresolver.h"
#ifndef _HEADER_RPC_IOXIDResolver
#define _HEADER_RPC_IOXIDResolver

extern const struct ndr_interface_table ndr_table_IOXIDResolver;

struct tevent_req *dcerpc_ResolveOxid_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ResolveOxid *r);
NTSTATUS dcerpc_ResolveOxid_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ResolveOxid_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ResolveOxid *r);
struct tevent_req *dcerpc_ResolveOxid_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   uint64_t _pOxid /* [in]  */,
					   uint16_t _cRequestedProtseqs /* [in]  */,
					   uint16_t *_arRequestedProtseqs /* [in] [size_is(cRequestedProtseqs)] */,
					   struct DUALSTRINGARRAY **_ppdsaOxidBindings /* [out] [ref] */,
					   struct GUID *_pipidRemUnknown /* [out] [ref] */,
					   uint32_t *_pAuthnHint /* [out] [ref] */);
NTSTATUS dcerpc_ResolveOxid_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_ResolveOxid(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    uint64_t _pOxid /* [in]  */,
			    uint16_t _cRequestedProtseqs /* [in]  */,
			    uint16_t *_arRequestedProtseqs /* [in] [size_is(cRequestedProtseqs)] */,
			    struct DUALSTRINGARRAY **_ppdsaOxidBindings /* [out] [ref] */,
			    struct GUID *_pipidRemUnknown /* [out] [ref] */,
			    uint32_t *_pAuthnHint /* [out] [ref] */,
			    WERROR *result);

struct tevent_req *dcerpc_SimplePing_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct SimplePing *r);
NTSTATUS dcerpc_SimplePing_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_SimplePing_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct SimplePing *r);
struct tevent_req *dcerpc_SimplePing_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct dcerpc_binding_handle *h,
					  uint64_t *_SetId /* [in] [ref] */);
NTSTATUS dcerpc_SimplePing_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				WERROR *result);
NTSTATUS dcerpc_SimplePing(struct dcerpc_binding_handle *h,
			   TALLOC_CTX *mem_ctx,
			   uint64_t *_SetId /* [in] [ref] */,
			   WERROR *result);

struct tevent_req *dcerpc_ComplexPing_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ComplexPing *r);
NTSTATUS dcerpc_ComplexPing_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ComplexPing_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ComplexPing *r);
struct tevent_req *dcerpc_ComplexPing_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h,
					   uint64_t *_SetId /* [in,out] [ref] */,
					   uint16_t _SequenceNum /* [in]  */,
					   uint16_t _cAddToSet /* [in]  */,
					   uint16_t _cDelFromSet /* [in]  */,
					   struct GUID *_AddToSet /* [in] [size_is(cAddToSet)] */,
					   struct GUID *_DelFromSet /* [in] [size_is(cDelFromSet)] */,
					   uint16_t *_PingBackoffFactor /* [out] [ref] */);
NTSTATUS dcerpc_ComplexPing_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_ComplexPing(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    uint64_t *_SetId /* [in,out] [ref] */,
			    uint16_t _SequenceNum /* [in]  */,
			    uint16_t _cAddToSet /* [in]  */,
			    uint16_t _cDelFromSet /* [in]  */,
			    struct GUID *_AddToSet /* [in] [size_is(cAddToSet)] */,
			    struct GUID *_DelFromSet /* [in] [size_is(cDelFromSet)] */,
			    uint16_t *_PingBackoffFactor /* [out] [ref] */,
			    WERROR *result);

struct tevent_req *dcerpc_ServerAlive_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ServerAlive *r);
NTSTATUS dcerpc_ServerAlive_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ServerAlive_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ServerAlive *r);
struct tevent_req *dcerpc_ServerAlive_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dcerpc_binding_handle *h);
NTSTATUS dcerpc_ServerAlive_recv(struct tevent_req *req,
				 TALLOC_CTX *mem_ctx,
				 WERROR *result);
NTSTATUS dcerpc_ServerAlive(struct dcerpc_binding_handle *h,
			    TALLOC_CTX *mem_ctx,
			    WERROR *result);

struct tevent_req *dcerpc_ResolveOxid2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ResolveOxid2 *r);
NTSTATUS dcerpc_ResolveOxid2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ResolveOxid2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ResolveOxid2 *r);
struct tevent_req *dcerpc_ResolveOxid2_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    uint64_t _pOxid /* [in]  */,
					    uint16_t _cRequestedProtseqs /* [in]  */,
					    uint16_t *_arRequestedProtseqs /* [in] [size_is(cRequestedProtseqs)] */,
					    struct DUALSTRINGARRAY **_pdsaOxidBindings /* [out] [ref] */,
					    struct GUID *_ipidRemUnknown /* [out] [ref] */,
					    uint32_t *_AuthnHint /* [out] [ref] */,
					    struct COMVERSION *_ComVersion /* [out] [ref] */);
NTSTATUS dcerpc_ResolveOxid2_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  WERROR *result);
NTSTATUS dcerpc_ResolveOxid2(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     uint64_t _pOxid /* [in]  */,
			     uint16_t _cRequestedProtseqs /* [in]  */,
			     uint16_t *_arRequestedProtseqs /* [in] [size_is(cRequestedProtseqs)] */,
			     struct DUALSTRINGARRAY **_pdsaOxidBindings /* [out] [ref] */,
			     struct GUID *_ipidRemUnknown /* [out] [ref] */,
			     uint32_t *_AuthnHint /* [out] [ref] */,
			     struct COMVERSION *_ComVersion /* [out] [ref] */,
			     WERROR *result);

struct tevent_req *dcerpc_ServerAlive2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct ServerAlive2 *r);
NTSTATUS dcerpc_ServerAlive2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_ServerAlive2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct ServerAlive2 *r);
struct tevent_req *dcerpc_ServerAlive2_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct dcerpc_binding_handle *h,
					    struct COMINFO *_info /* [out] [ref] */,
					    struct DUALSTRINGARRAY *_dualstring /* [out] [ref] */,
					    uint8_t *_unknown2 /* [out] [ref] */,
					    uint8_t *_unknown3 /* [out] [ref] */,
					    uint8_t *_unknown4 /* [out] [ref] */);
NTSTATUS dcerpc_ServerAlive2_recv(struct tevent_req *req,
				  TALLOC_CTX *mem_ctx,
				  WERROR *result);
NTSTATUS dcerpc_ServerAlive2(struct dcerpc_binding_handle *h,
			     TALLOC_CTX *mem_ctx,
			     struct COMINFO *_info /* [out] [ref] */,
			     struct DUALSTRINGARRAY *_dualstring /* [out] [ref] */,
			     uint8_t *_unknown2 /* [out] [ref] */,
			     uint8_t *_unknown3 /* [out] [ref] */,
			     uint8_t *_unknown4 /* [out] [ref] */,
			     WERROR *result);

#endif /* _HEADER_RPC_IOXIDResolver */
