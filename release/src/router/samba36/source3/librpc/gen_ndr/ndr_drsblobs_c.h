#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/drsblobs.h"
#ifndef _HEADER_RPC_drsblobs
#define _HEADER_RPC_drsblobs

extern const struct ndr_interface_table ndr_table_drsblobs;

struct tevent_req *dcerpc_decode_replPropertyMetaData_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_replPropertyMetaData *r);
NTSTATUS dcerpc_decode_replPropertyMetaData_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_replPropertyMetaData_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_replPropertyMetaData *r);
struct tevent_req *dcerpc_decode_replPropertyMetaData_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct replPropertyMetaDataBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_replPropertyMetaData_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_replPropertyMetaData(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct replPropertyMetaDataBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_replUpToDateVector_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_replUpToDateVector *r);
NTSTATUS dcerpc_decode_replUpToDateVector_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_replUpToDateVector_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_replUpToDateVector *r);
struct tevent_req *dcerpc_decode_replUpToDateVector_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct replUpToDateVectorBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_replUpToDateVector_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_replUpToDateVector(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct replUpToDateVectorBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_repsFromTo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_repsFromTo *r);
NTSTATUS dcerpc_decode_repsFromTo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_repsFromTo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_repsFromTo *r);
struct tevent_req *dcerpc_decode_repsFromTo_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 struct repsFromToBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_repsFromTo_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_repsFromTo(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  struct repsFromToBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_partialAttributeSet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_partialAttributeSet *r);
NTSTATUS dcerpc_decode_partialAttributeSet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_partialAttributeSet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_partialAttributeSet *r);
struct tevent_req *dcerpc_decode_partialAttributeSet_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  struct partialAttributeSetBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_partialAttributeSet_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_partialAttributeSet(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   struct partialAttributeSetBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_prefixMap_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_prefixMap *r);
NTSTATUS dcerpc_decode_prefixMap_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_prefixMap_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_prefixMap *r);
struct tevent_req *dcerpc_decode_prefixMap_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct dcerpc_binding_handle *h,
						struct prefixMapBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_prefixMap_recv(struct tevent_req *req,
				      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_prefixMap(struct dcerpc_binding_handle *h,
				 TALLOC_CTX *mem_ctx,
				 struct prefixMapBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_ldapControlDirSync_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_ldapControlDirSync *r);
NTSTATUS dcerpc_decode_ldapControlDirSync_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ldapControlDirSync_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_ldapControlDirSync *r);
struct tevent_req *dcerpc_decode_ldapControlDirSync_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 struct ldapControlDirSyncCookie _cookie /* [in]  */);
NTSTATUS dcerpc_decode_ldapControlDirSync_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ldapControlDirSync(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  struct ldapControlDirSyncCookie _cookie /* [in]  */);

struct tevent_req *dcerpc_decode_supplementalCredentials_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_supplementalCredentials *r);
NTSTATUS dcerpc_decode_supplementalCredentials_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_supplementalCredentials_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_supplementalCredentials *r);
struct tevent_req *dcerpc_decode_supplementalCredentials_send(TALLOC_CTX *mem_ctx,
							      struct tevent_context *ev,
							      struct dcerpc_binding_handle *h,
							      struct supplementalCredentialsBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_supplementalCredentials_recv(struct tevent_req *req,
						    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_supplementalCredentials(struct dcerpc_binding_handle *h,
					       TALLOC_CTX *mem_ctx,
					       struct supplementalCredentialsBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_Packages_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_Packages *r);
NTSTATUS dcerpc_decode_Packages_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_Packages_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_Packages *r);
struct tevent_req *dcerpc_decode_Packages_send(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       struct dcerpc_binding_handle *h,
					       struct package_PackagesBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_Packages_recv(struct tevent_req *req,
				     TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_Packages(struct dcerpc_binding_handle *h,
				TALLOC_CTX *mem_ctx,
				struct package_PackagesBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_PrimaryKerberos_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_PrimaryKerberos *r);
NTSTATUS dcerpc_decode_PrimaryKerberos_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_PrimaryKerberos_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_PrimaryKerberos *r);
struct tevent_req *dcerpc_decode_PrimaryKerberos_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct package_PrimaryKerberosBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_PrimaryKerberos_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_PrimaryKerberos(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct package_PrimaryKerberosBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_PrimaryCLEARTEXT_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_PrimaryCLEARTEXT *r);
NTSTATUS dcerpc_decode_PrimaryCLEARTEXT_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_PrimaryCLEARTEXT_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_PrimaryCLEARTEXT *r);
struct tevent_req *dcerpc_decode_PrimaryCLEARTEXT_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       struct package_PrimaryCLEARTEXTBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_PrimaryCLEARTEXT_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_PrimaryCLEARTEXT(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					struct package_PrimaryCLEARTEXTBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_PrimaryWDigest_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_PrimaryWDigest *r);
NTSTATUS dcerpc_decode_PrimaryWDigest_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_PrimaryWDigest_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_PrimaryWDigest *r);
struct tevent_req *dcerpc_decode_PrimaryWDigest_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct package_PrimaryWDigestBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_PrimaryWDigest_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_PrimaryWDigest(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct package_PrimaryWDigestBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_trustAuthInOut_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_trustAuthInOut *r);
NTSTATUS dcerpc_decode_trustAuthInOut_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_trustAuthInOut_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_trustAuthInOut *r);
struct tevent_req *dcerpc_decode_trustAuthInOut_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     struct trustAuthInOutBlob _blob /* [in]  */);
NTSTATUS dcerpc_decode_trustAuthInOut_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_trustAuthInOut(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      struct trustAuthInOutBlob _blob /* [in]  */);

struct tevent_req *dcerpc_decode_trustDomainPasswords_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_trustDomainPasswords *r);
NTSTATUS dcerpc_decode_trustDomainPasswords_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_trustDomainPasswords_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_trustDomainPasswords *r);
struct tevent_req *dcerpc_decode_trustDomainPasswords_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   struct trustDomainPasswords _blob /* [in]  */);
NTSTATUS dcerpc_decode_trustDomainPasswords_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_trustDomainPasswords(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    struct trustDomainPasswords _blob /* [in]  */);

struct tevent_req *dcerpc_decode_ExtendedErrorInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_ExtendedErrorInfo *r);
NTSTATUS dcerpc_decode_ExtendedErrorInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ExtendedErrorInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_ExtendedErrorInfo *r);
struct tevent_req *dcerpc_decode_ExtendedErrorInfo_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							struct ExtendedErrorInfoPtr _ptr /* [in] [subcontext(0xFFFFFC01)] */);
NTSTATUS dcerpc_decode_ExtendedErrorInfo_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ExtendedErrorInfo(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 struct ExtendedErrorInfoPtr _ptr /* [in] [subcontext(0xFFFFFC01)] */);

struct tevent_req *dcerpc_decode_ForestTrustInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct decode_ForestTrustInfo *r);
NTSTATUS dcerpc_decode_ForestTrustInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ForestTrustInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct decode_ForestTrustInfo *r);
struct tevent_req *dcerpc_decode_ForestTrustInfo_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      struct ForestTrustInfo _blob /* [in]  */);
NTSTATUS dcerpc_decode_ForestTrustInfo_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_decode_ForestTrustInfo(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       struct ForestTrustInfo _blob /* [in]  */);

#endif /* _HEADER_RPC_drsblobs */
