#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/wkssvc.h"
#ifndef _HEADER_RPC_wkssvc
#define _HEADER_RPC_wkssvc

extern const struct ndr_interface_table ndr_table_wkssvc;

struct tevent_req *dcerpc_wkssvc_NetWkstaGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetWkstaGetInfo *r);
NTSTATUS dcerpc_wkssvc_NetWkstaGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetWkstaGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetWkstaGetInfo *r);
struct tevent_req *dcerpc_wkssvc_NetWkstaGetInfo_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_name /* [in] [unique,charset(UTF16)] */,
						      uint32_t _level /* [in]  */,
						      union wkssvc_NetWkstaInfo *_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_wkssvc_NetWkstaGetInfo_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_wkssvc_NetWkstaGetInfo(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_name /* [in] [unique,charset(UTF16)] */,
				       uint32_t _level /* [in]  */,
				       union wkssvc_NetWkstaInfo *_info /* [out] [switch_is(level),ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetWkstaSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetWkstaSetInfo *r);
NTSTATUS dcerpc_wkssvc_NetWkstaSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetWkstaSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetWkstaSetInfo *r);
struct tevent_req *dcerpc_wkssvc_NetWkstaSetInfo_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_name /* [in] [unique,charset(UTF16)] */,
						      uint32_t _level /* [in]  */,
						      union wkssvc_NetWkstaInfo *_info /* [in] [ref,switch_is(level)] */,
						      uint32_t *_parm_error /* [in,out] [ref] */);
NTSTATUS dcerpc_wkssvc_NetWkstaSetInfo_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_wkssvc_NetWkstaSetInfo(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_name /* [in] [unique,charset(UTF16)] */,
				       uint32_t _level /* [in]  */,
				       union wkssvc_NetWkstaInfo *_info /* [in] [ref,switch_is(level)] */,
				       uint32_t *_parm_error /* [in,out] [ref] */,
				       WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetWkstaEnumUsers_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetWkstaEnumUsers *r);
NTSTATUS dcerpc_wkssvc_NetWkstaEnumUsers_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetWkstaEnumUsers_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetWkstaEnumUsers *r);
struct tevent_req *dcerpc_wkssvc_NetWkstaEnumUsers_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_name /* [in] [charset(UTF16),unique] */,
							struct wkssvc_NetWkstaEnumUsersInfo *_info /* [in,out] [ref] */,
							uint32_t _prefmaxlen /* [in]  */,
							uint32_t *_entries_read /* [out] [ref] */,
							uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_wkssvc_NetWkstaEnumUsers_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_wkssvc_NetWkstaEnumUsers(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_name /* [in] [charset(UTF16),unique] */,
					 struct wkssvc_NetWkstaEnumUsersInfo *_info /* [in,out] [ref] */,
					 uint32_t _prefmaxlen /* [in]  */,
					 uint32_t *_entries_read /* [out] [ref] */,
					 uint32_t *_resume_handle /* [in,out] [unique] */,
					 WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrWkstaUserGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrWkstaUserGetInfo *r);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrWkstaUserGetInfo *r);
struct tevent_req *dcerpc_wkssvc_NetrWkstaUserGetInfo_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_unknown /* [in] [unique,charset(UTF16)] */,
							   uint32_t _level /* [in]  */,
							   union wkssvc_NetrWkstaUserInfo *_info /* [out] [switch_is(level),ref] */);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserGetInfo_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserGetInfo(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_unknown /* [in] [unique,charset(UTF16)] */,
					    uint32_t _level /* [in]  */,
					    union wkssvc_NetrWkstaUserInfo *_info /* [out] [switch_is(level),ref] */,
					    WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrWkstaUserSetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrWkstaUserSetInfo *r);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserSetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserSetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrWkstaUserSetInfo *r);
struct tevent_req *dcerpc_wkssvc_NetrWkstaUserSetInfo_send(TALLOC_CTX *mem_ctx,
							   struct tevent_context *ev,
							   struct dcerpc_binding_handle *h,
							   const char *_unknown /* [in] [unique,charset(UTF16)] */,
							   uint32_t _level /* [in]  */,
							   union wkssvc_NetrWkstaUserInfo *_info /* [in] [ref,switch_is(level)] */,
							   uint32_t *_parm_err /* [in,out] [unique] */);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserSetInfo_recv(struct tevent_req *req,
						 TALLOC_CTX *mem_ctx,
						 WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrWkstaUserSetInfo(struct dcerpc_binding_handle *h,
					    TALLOC_CTX *mem_ctx,
					    const char *_unknown /* [in] [unique,charset(UTF16)] */,
					    uint32_t _level /* [in]  */,
					    union wkssvc_NetrWkstaUserInfo *_info /* [in] [ref,switch_is(level)] */,
					    uint32_t *_parm_err /* [in,out] [unique] */,
					    WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetWkstaTransportEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetWkstaTransportEnum *r);
NTSTATUS dcerpc_wkssvc_NetWkstaTransportEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetWkstaTransportEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetWkstaTransportEnum *r);
struct tevent_req *dcerpc_wkssvc_NetWkstaTransportEnum_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    const char *_server_name /* [in] [charset(UTF16),unique] */,
							    struct wkssvc_NetWkstaTransportInfo *_info /* [in,out] [ref] */,
							    uint32_t _max_buffer /* [in]  */,
							    uint32_t *_total_entries /* [out] [ref] */,
							    uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_wkssvc_NetWkstaTransportEnum_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_wkssvc_NetWkstaTransportEnum(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     const char *_server_name /* [in] [charset(UTF16),unique] */,
					     struct wkssvc_NetWkstaTransportInfo *_info /* [in,out] [ref] */,
					     uint32_t _max_buffer /* [in]  */,
					     uint32_t *_total_entries /* [out] [ref] */,
					     uint32_t *_resume_handle /* [in,out] [unique] */,
					     WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrWkstaTransportAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrWkstaTransportAdd *r);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrWkstaTransportAdd *r);
struct tevent_req *dcerpc_wkssvc_NetrWkstaTransportAdd_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    const char *_server_name /* [in] [unique,charset(UTF16)] */,
							    uint32_t _level /* [in]  */,
							    struct wkssvc_NetWkstaTransportInfo0 *_info0 /* [in] [ref] */,
							    uint32_t *_parm_err /* [in,out] [unique] */);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportAdd_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportAdd(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     const char *_server_name /* [in] [unique,charset(UTF16)] */,
					     uint32_t _level /* [in]  */,
					     struct wkssvc_NetWkstaTransportInfo0 *_info0 /* [in] [ref] */,
					     uint32_t *_parm_err /* [in,out] [unique] */,
					     WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrWkstaTransportDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrWkstaTransportDel *r);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrWkstaTransportDel *r);
struct tevent_req *dcerpc_wkssvc_NetrWkstaTransportDel_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    const char *_server_name /* [in] [charset(UTF16),unique] */,
							    const char *_transport_name /* [in] [unique,charset(UTF16)] */,
							    uint32_t _unknown3 /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportDel_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrWkstaTransportDel(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     const char *_server_name /* [in] [charset(UTF16),unique] */,
					     const char *_transport_name /* [in] [unique,charset(UTF16)] */,
					     uint32_t _unknown3 /* [in]  */,
					     WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrUseAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrUseAdd *r);
NTSTATUS dcerpc_wkssvc_NetrUseAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrUseAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrUseAdd *r);
struct tevent_req *dcerpc_wkssvc_NetrUseAdd_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_server_name /* [in] [unique,charset(UTF16)] */,
						 uint32_t _level /* [in]  */,
						 union wkssvc_NetrUseGetInfoCtr *_ctr /* [in] [switch_is(level),ref] */,
						 uint32_t *_parm_err /* [in,out] [unique] */);
NTSTATUS dcerpc_wkssvc_NetrUseAdd_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrUseAdd(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_server_name /* [in] [unique,charset(UTF16)] */,
				  uint32_t _level /* [in]  */,
				  union wkssvc_NetrUseGetInfoCtr *_ctr /* [in] [switch_is(level),ref] */,
				  uint32_t *_parm_err /* [in,out] [unique] */,
				  WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrUseGetInfo_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrUseGetInfo *r);
NTSTATUS dcerpc_wkssvc_NetrUseGetInfo_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrUseGetInfo_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrUseGetInfo *r);
struct tevent_req *dcerpc_wkssvc_NetrUseGetInfo_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_server_name /* [in] [unique,charset(UTF16)] */,
						     const char *_use_name /* [in] [charset(UTF16),ref] */,
						     uint32_t _level /* [in]  */,
						     union wkssvc_NetrUseGetInfoCtr *_ctr /* [out] [ref,switch_is(level)] */);
NTSTATUS dcerpc_wkssvc_NetrUseGetInfo_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrUseGetInfo(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_server_name /* [in] [unique,charset(UTF16)] */,
				      const char *_use_name /* [in] [charset(UTF16),ref] */,
				      uint32_t _level /* [in]  */,
				      union wkssvc_NetrUseGetInfoCtr *_ctr /* [out] [ref,switch_is(level)] */,
				      WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrUseDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrUseDel *r);
NTSTATUS dcerpc_wkssvc_NetrUseDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrUseDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrUseDel *r);
struct tevent_req *dcerpc_wkssvc_NetrUseDel_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct dcerpc_binding_handle *h,
						 const char *_server_name /* [in] [charset(UTF16),unique] */,
						 const char *_use_name /* [in] [ref,charset(UTF16)] */,
						 uint32_t _force_cond /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrUseDel_recv(struct tevent_req *req,
				       TALLOC_CTX *mem_ctx,
				       WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrUseDel(struct dcerpc_binding_handle *h,
				  TALLOC_CTX *mem_ctx,
				  const char *_server_name /* [in] [charset(UTF16),unique] */,
				  const char *_use_name /* [in] [ref,charset(UTF16)] */,
				  uint32_t _force_cond /* [in]  */,
				  WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrUseEnum_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrUseEnum *r);
NTSTATUS dcerpc_wkssvc_NetrUseEnum_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrUseEnum_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrUseEnum *r);
struct tevent_req *dcerpc_wkssvc_NetrUseEnum_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dcerpc_binding_handle *h,
						  const char *_server_name /* [in] [charset(UTF16),unique] */,
						  struct wkssvc_NetrUseEnumInfo *_info /* [in,out] [ref] */,
						  uint32_t _prefmaxlen /* [in]  */,
						  uint32_t *_entries_read /* [out] [ref] */,
						  uint32_t *_resume_handle /* [in,out] [unique] */);
NTSTATUS dcerpc_wkssvc_NetrUseEnum_recv(struct tevent_req *req,
					TALLOC_CTX *mem_ctx,
					WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrUseEnum(struct dcerpc_binding_handle *h,
				   TALLOC_CTX *mem_ctx,
				   const char *_server_name /* [in] [charset(UTF16),unique] */,
				   struct wkssvc_NetrUseEnumInfo *_info /* [in,out] [ref] */,
				   uint32_t _prefmaxlen /* [in]  */,
				   uint32_t *_entries_read /* [out] [ref] */,
				   uint32_t *_resume_handle /* [in,out] [unique] */,
				   WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrMessageBufferSend_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrMessageBufferSend *r);
NTSTATUS dcerpc_wkssvc_NetrMessageBufferSend_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrMessageBufferSend_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrMessageBufferSend *r);
struct tevent_req *dcerpc_wkssvc_NetrMessageBufferSend_send(TALLOC_CTX *mem_ctx,
							    struct tevent_context *ev,
							    struct dcerpc_binding_handle *h,
							    const char *_server_name /* [in] [unique,charset(UTF16)] */,
							    const char *_message_name /* [in] [charset(UTF16),ref] */,
							    const char *_message_sender_name /* [in] [unique,charset(UTF16)] */,
							    uint8_t *_message_buffer /* [in] [size_is(message_size),ref] */,
							    uint32_t _message_size /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrMessageBufferSend_recv(struct tevent_req *req,
						  TALLOC_CTX *mem_ctx,
						  WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrMessageBufferSend(struct dcerpc_binding_handle *h,
					     TALLOC_CTX *mem_ctx,
					     const char *_server_name /* [in] [unique,charset(UTF16)] */,
					     const char *_message_name /* [in] [charset(UTF16),ref] */,
					     const char *_message_sender_name /* [in] [unique,charset(UTF16)] */,
					     uint8_t *_message_buffer /* [in] [size_is(message_size),ref] */,
					     uint32_t _message_size /* [in]  */,
					     WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrWorkstationStatisticsGet_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrWorkstationStatisticsGet *r);
NTSTATUS dcerpc_wkssvc_NetrWorkstationStatisticsGet_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrWorkstationStatisticsGet_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrWorkstationStatisticsGet *r);
struct tevent_req *dcerpc_wkssvc_NetrWorkstationStatisticsGet_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct dcerpc_binding_handle *h,
								   const char *_server_name /* [in] [charset(UTF16),unique] */,
								   const char *_unknown2 /* [in] [unique,charset(UTF16)] */,
								   uint32_t _unknown3 /* [in]  */,
								   uint32_t _unknown4 /* [in]  */,
								   struct wkssvc_NetrWorkstationStatistics **_info /* [out] [ref] */);
NTSTATUS dcerpc_wkssvc_NetrWorkstationStatisticsGet_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrWorkstationStatisticsGet(struct dcerpc_binding_handle *h,
						    TALLOC_CTX *mem_ctx,
						    const char *_server_name /* [in] [charset(UTF16),unique] */,
						    const char *_unknown2 /* [in] [unique,charset(UTF16)] */,
						    uint32_t _unknown3 /* [in]  */,
						    uint32_t _unknown4 /* [in]  */,
						    struct wkssvc_NetrWorkstationStatistics **_info /* [out] [ref] */,
						    WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrLogonDomainNameAdd_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrLogonDomainNameAdd *r);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameAdd_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameAdd_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrLogonDomainNameAdd *r);
struct tevent_req *dcerpc_wkssvc_NetrLogonDomainNameAdd_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_domain_name /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameAdd_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameAdd(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_domain_name /* [in] [charset(UTF16),ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrLogonDomainNameDel_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrLogonDomainNameDel *r);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameDel_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameDel_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrLogonDomainNameDel *r);
struct tevent_req *dcerpc_wkssvc_NetrLogonDomainNameDel_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_domain_name /* [in] [charset(UTF16),ref] */);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameDel_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrLogonDomainNameDel(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_domain_name /* [in] [charset(UTF16),ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrJoinDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrJoinDomain *r);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrJoinDomain *r);
struct tevent_req *dcerpc_wkssvc_NetrJoinDomain_send(TALLOC_CTX *mem_ctx,
						     struct tevent_context *ev,
						     struct dcerpc_binding_handle *h,
						     const char *_server_name /* [in] [charset(UTF16),unique] */,
						     const char *_domain_name /* [in] [ref,charset(UTF16)] */,
						     const char *_account_ou /* [in] [unique,charset(UTF16)] */,
						     const char *_Account /* [in] [charset(UTF16),unique] */,
						     const char *_password /* [in] [unique,charset(UTF16)] */,
						     uint32_t _join_flags /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain_recv(struct tevent_req *req,
					   TALLOC_CTX *mem_ctx,
					   WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain(struct dcerpc_binding_handle *h,
				      TALLOC_CTX *mem_ctx,
				      const char *_server_name /* [in] [charset(UTF16),unique] */,
				      const char *_domain_name /* [in] [ref,charset(UTF16)] */,
				      const char *_account_ou /* [in] [unique,charset(UTF16)] */,
				      const char *_Account /* [in] [charset(UTF16),unique] */,
				      const char *_password /* [in] [unique,charset(UTF16)] */,
				      uint32_t _join_flags /* [in]  */,
				      WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrUnjoinDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrUnjoinDomain *r);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrUnjoinDomain *r);
struct tevent_req *dcerpc_wkssvc_NetrUnjoinDomain_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [charset(UTF16),unique] */,
						       const char *_Account /* [in] [charset(UTF16),unique] */,
						       const char *_password /* [in] [unique,charset(UTF16)] */,
						       uint32_t _unjoin_flags /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [charset(UTF16),unique] */,
					const char *_Account /* [in] [charset(UTF16),unique] */,
					const char *_password /* [in] [unique,charset(UTF16)] */,
					uint32_t _unjoin_flags /* [in]  */,
					WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrRenameMachineInDomain_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrRenameMachineInDomain *r);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrRenameMachineInDomain *r);
struct tevent_req *dcerpc_wkssvc_NetrRenameMachineInDomain_send(TALLOC_CTX *mem_ctx,
								struct tevent_context *ev,
								struct dcerpc_binding_handle *h,
								const char *_server_name /* [in] [unique,charset(UTF16)] */,
								const char *_NewMachineName /* [in] [unique,charset(UTF16)] */,
								const char *_Account /* [in] [unique,charset(UTF16)] */,
								const char *_password /* [in] [unique,charset(UTF16)] */,
								uint32_t _RenameOptions /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain_recv(struct tevent_req *req,
						      TALLOC_CTX *mem_ctx,
						      WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain(struct dcerpc_binding_handle *h,
						 TALLOC_CTX *mem_ctx,
						 const char *_server_name /* [in] [unique,charset(UTF16)] */,
						 const char *_NewMachineName /* [in] [unique,charset(UTF16)] */,
						 const char *_Account /* [in] [unique,charset(UTF16)] */,
						 const char *_password /* [in] [unique,charset(UTF16)] */,
						 uint32_t _RenameOptions /* [in]  */,
						 WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrValidateName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrValidateName *r);
NTSTATUS dcerpc_wkssvc_NetrValidateName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrValidateName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrValidateName *r);
struct tevent_req *dcerpc_wkssvc_NetrValidateName_send(TALLOC_CTX *mem_ctx,
						       struct tevent_context *ev,
						       struct dcerpc_binding_handle *h,
						       const char *_server_name /* [in] [charset(UTF16),unique] */,
						       const char *_name /* [in] [ref,charset(UTF16)] */,
						       const char *_Account /* [in] [charset(UTF16),unique] */,
						       const char *_Password /* [in] [unique,charset(UTF16)] */,
						       enum wkssvc_NetValidateNameType _name_type /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrValidateName_recv(struct tevent_req *req,
					     TALLOC_CTX *mem_ctx,
					     WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrValidateName(struct dcerpc_binding_handle *h,
					TALLOC_CTX *mem_ctx,
					const char *_server_name /* [in] [charset(UTF16),unique] */,
					const char *_name /* [in] [ref,charset(UTF16)] */,
					const char *_Account /* [in] [charset(UTF16),unique] */,
					const char *_Password /* [in] [unique,charset(UTF16)] */,
					enum wkssvc_NetValidateNameType _name_type /* [in]  */,
					WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrGetJoinInformation_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrGetJoinInformation *r);
NTSTATUS dcerpc_wkssvc_NetrGetJoinInformation_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrGetJoinInformation_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrGetJoinInformation *r);
struct tevent_req *dcerpc_wkssvc_NetrGetJoinInformation_send(TALLOC_CTX *mem_ctx,
							     struct tevent_context *ev,
							     struct dcerpc_binding_handle *h,
							     const char *_server_name /* [in] [charset(UTF16),unique] */,
							     const char **_name_buffer /* [in,out] [charset(UTF16),ref] */,
							     enum wkssvc_NetJoinStatus *_name_type /* [out] [ref] */);
NTSTATUS dcerpc_wkssvc_NetrGetJoinInformation_recv(struct tevent_req *req,
						   TALLOC_CTX *mem_ctx,
						   WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrGetJoinInformation(struct dcerpc_binding_handle *h,
					      TALLOC_CTX *mem_ctx,
					      const char *_server_name /* [in] [charset(UTF16),unique] */,
					      const char **_name_buffer /* [in,out] [charset(UTF16),ref] */,
					      enum wkssvc_NetJoinStatus *_name_type /* [out] [ref] */,
					      WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrGetJoinableOus_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrGetJoinableOus *r);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrGetJoinableOus *r);
struct tevent_req *dcerpc_wkssvc_NetrGetJoinableOus_send(TALLOC_CTX *mem_ctx,
							 struct tevent_context *ev,
							 struct dcerpc_binding_handle *h,
							 const char *_server_name /* [in] [charset(UTF16),unique] */,
							 const char *_domain_name /* [in] [charset(UTF16),ref] */,
							 const char *_Account /* [in] [charset(UTF16),unique] */,
							 const char *_unknown /* [in] [charset(UTF16),unique] */,
							 uint32_t *_num_ous /* [in,out] [ref] */,
							 const char ***_ous /* [out] [charset(UTF16),size_is(,*num_ous),ref] */);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus_recv(struct tevent_req *req,
					       TALLOC_CTX *mem_ctx,
					       WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus(struct dcerpc_binding_handle *h,
					  TALLOC_CTX *mem_ctx,
					  const char *_server_name /* [in] [charset(UTF16),unique] */,
					  const char *_domain_name /* [in] [charset(UTF16),ref] */,
					  const char *_Account /* [in] [charset(UTF16),unique] */,
					  const char *_unknown /* [in] [charset(UTF16),unique] */,
					  uint32_t *_num_ous /* [in,out] [ref] */,
					  const char ***_ous /* [out] [charset(UTF16),size_is(,*num_ous),ref] */,
					  WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrJoinDomain2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrJoinDomain2 *r);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrJoinDomain2 *r);
struct tevent_req *dcerpc_wkssvc_NetrJoinDomain2_send(TALLOC_CTX *mem_ctx,
						      struct tevent_context *ev,
						      struct dcerpc_binding_handle *h,
						      const char *_server_name /* [in] [charset(UTF16),unique] */,
						      const char *_domain_name /* [in] [ref,charset(UTF16)] */,
						      const char *_account_ou /* [in] [charset(UTF16),unique] */,
						      const char *_admin_account /* [in] [unique,charset(UTF16)] */,
						      struct wkssvc_PasswordBuffer *_encrypted_password /* [in] [unique] */,
						      uint32_t _join_flags /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain2_recv(struct tevent_req *req,
					    TALLOC_CTX *mem_ctx,
					    WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrJoinDomain2(struct dcerpc_binding_handle *h,
				       TALLOC_CTX *mem_ctx,
				       const char *_server_name /* [in] [charset(UTF16),unique] */,
				       const char *_domain_name /* [in] [ref,charset(UTF16)] */,
				       const char *_account_ou /* [in] [charset(UTF16),unique] */,
				       const char *_admin_account /* [in] [unique,charset(UTF16)] */,
				       struct wkssvc_PasswordBuffer *_encrypted_password /* [in] [unique] */,
				       uint32_t _join_flags /* [in]  */,
				       WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrUnjoinDomain2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrUnjoinDomain2 *r);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrUnjoinDomain2 *r);
struct tevent_req *dcerpc_wkssvc_NetrUnjoinDomain2_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_name /* [in] [charset(UTF16),unique] */,
							const char *_account /* [in] [charset(UTF16),unique] */,
							struct wkssvc_PasswordBuffer *_encrypted_password /* [in] [unique] */,
							uint32_t _unjoin_flags /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain2_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrUnjoinDomain2(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_name /* [in] [charset(UTF16),unique] */,
					 const char *_account /* [in] [charset(UTF16),unique] */,
					 struct wkssvc_PasswordBuffer *_encrypted_password /* [in] [unique] */,
					 uint32_t _unjoin_flags /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrRenameMachineInDomain2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrRenameMachineInDomain2 *r);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrRenameMachineInDomain2 *r);
struct tevent_req *dcerpc_wkssvc_NetrRenameMachineInDomain2_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 const char *_server_name /* [in] [unique,charset(UTF16)] */,
								 const char *_NewMachineName /* [in] [unique,charset(UTF16)] */,
								 const char *_Account /* [in] [unique,charset(UTF16)] */,
								 struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
								 uint32_t _RenameOptions /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain2_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrRenameMachineInDomain2(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  const char *_server_name /* [in] [unique,charset(UTF16)] */,
						  const char *_NewMachineName /* [in] [unique,charset(UTF16)] */,
						  const char *_Account /* [in] [unique,charset(UTF16)] */,
						  struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
						  uint32_t _RenameOptions /* [in]  */,
						  WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrValidateName2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrValidateName2 *r);
NTSTATUS dcerpc_wkssvc_NetrValidateName2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrValidateName2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrValidateName2 *r);
struct tevent_req *dcerpc_wkssvc_NetrValidateName2_send(TALLOC_CTX *mem_ctx,
							struct tevent_context *ev,
							struct dcerpc_binding_handle *h,
							const char *_server_name /* [in] [charset(UTF16),unique] */,
							const char *_name /* [in] [ref,charset(UTF16)] */,
							const char *_Account /* [in] [charset(UTF16),unique] */,
							struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
							enum wkssvc_NetValidateNameType _name_type /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrValidateName2_recv(struct tevent_req *req,
					      TALLOC_CTX *mem_ctx,
					      WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrValidateName2(struct dcerpc_binding_handle *h,
					 TALLOC_CTX *mem_ctx,
					 const char *_server_name /* [in] [charset(UTF16),unique] */,
					 const char *_name /* [in] [ref,charset(UTF16)] */,
					 const char *_Account /* [in] [charset(UTF16),unique] */,
					 struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
					 enum wkssvc_NetValidateNameType _name_type /* [in]  */,
					 WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrGetJoinableOus2_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrGetJoinableOus2 *r);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus2_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus2_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrGetJoinableOus2 *r);
struct tevent_req *dcerpc_wkssvc_NetrGetJoinableOus2_send(TALLOC_CTX *mem_ctx,
							  struct tevent_context *ev,
							  struct dcerpc_binding_handle *h,
							  const char *_server_name /* [in] [charset(UTF16),unique] */,
							  const char *_domain_name /* [in] [charset(UTF16),ref] */,
							  const char *_Account /* [in] [charset(UTF16),unique] */,
							  struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
							  uint32_t *_num_ous /* [in,out] [ref] */,
							  const char ***_ous /* [out] [ref,size_is(,*num_ous),charset(UTF16)] */);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus2_recv(struct tevent_req *req,
						TALLOC_CTX *mem_ctx,
						WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrGetJoinableOus2(struct dcerpc_binding_handle *h,
					   TALLOC_CTX *mem_ctx,
					   const char *_server_name /* [in] [charset(UTF16),unique] */,
					   const char *_domain_name /* [in] [charset(UTF16),ref] */,
					   const char *_Account /* [in] [charset(UTF16),unique] */,
					   struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
					   uint32_t *_num_ous /* [in,out] [ref] */,
					   const char ***_ous /* [out] [ref,size_is(,*num_ous),charset(UTF16)] */,
					   WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrAddAlternateComputerName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrAddAlternateComputerName *r);
NTSTATUS dcerpc_wkssvc_NetrAddAlternateComputerName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrAddAlternateComputerName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrAddAlternateComputerName *r);
struct tevent_req *dcerpc_wkssvc_NetrAddAlternateComputerName_send(TALLOC_CTX *mem_ctx,
								   struct tevent_context *ev,
								   struct dcerpc_binding_handle *h,
								   const char *_server_name /* [in] [charset(UTF16),unique] */,
								   const char *_NewAlternateMachineName /* [in] [unique,charset(UTF16)] */,
								   const char *_Account /* [in] [charset(UTF16),unique] */,
								   struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
								   uint32_t _Reserved /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrAddAlternateComputerName_recv(struct tevent_req *req,
							 TALLOC_CTX *mem_ctx,
							 WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrAddAlternateComputerName(struct dcerpc_binding_handle *h,
						    TALLOC_CTX *mem_ctx,
						    const char *_server_name /* [in] [charset(UTF16),unique] */,
						    const char *_NewAlternateMachineName /* [in] [unique,charset(UTF16)] */,
						    const char *_Account /* [in] [charset(UTF16),unique] */,
						    struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
						    uint32_t _Reserved /* [in]  */,
						    WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrRemoveAlternateComputerName_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrRemoveAlternateComputerName *r);
NTSTATUS dcerpc_wkssvc_NetrRemoveAlternateComputerName_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrRemoveAlternateComputerName_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrRemoveAlternateComputerName *r);
struct tevent_req *dcerpc_wkssvc_NetrRemoveAlternateComputerName_send(TALLOC_CTX *mem_ctx,
								      struct tevent_context *ev,
								      struct dcerpc_binding_handle *h,
								      const char *_server_name /* [in] [unique,charset(UTF16)] */,
								      const char *_AlternateMachineNameToRemove /* [in] [unique,charset(UTF16)] */,
								      const char *_Account /* [in] [unique,charset(UTF16)] */,
								      struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
								      uint32_t _Reserved /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrRemoveAlternateComputerName_recv(struct tevent_req *req,
							    TALLOC_CTX *mem_ctx,
							    WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrRemoveAlternateComputerName(struct dcerpc_binding_handle *h,
						       TALLOC_CTX *mem_ctx,
						       const char *_server_name /* [in] [unique,charset(UTF16)] */,
						       const char *_AlternateMachineNameToRemove /* [in] [unique,charset(UTF16)] */,
						       const char *_Account /* [in] [unique,charset(UTF16)] */,
						       struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
						       uint32_t _Reserved /* [in]  */,
						       WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrSetPrimaryComputername_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrSetPrimaryComputername *r);
NTSTATUS dcerpc_wkssvc_NetrSetPrimaryComputername_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrSetPrimaryComputername_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrSetPrimaryComputername *r);
struct tevent_req *dcerpc_wkssvc_NetrSetPrimaryComputername_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 const char *_server_name /* [in] [unique,charset(UTF16)] */,
								 const char *_primary_name /* [in] [charset(UTF16),unique] */,
								 const char *_Account /* [in] [unique,charset(UTF16)] */,
								 struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
								 uint32_t _Reserved /* [in]  */);
NTSTATUS dcerpc_wkssvc_NetrSetPrimaryComputername_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrSetPrimaryComputername(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  const char *_server_name /* [in] [unique,charset(UTF16)] */,
						  const char *_primary_name /* [in] [charset(UTF16),unique] */,
						  const char *_Account /* [in] [unique,charset(UTF16)] */,
						  struct wkssvc_PasswordBuffer *_EncryptedPassword /* [in] [unique] */,
						  uint32_t _Reserved /* [in]  */,
						  WERROR *result);

struct tevent_req *dcerpc_wkssvc_NetrEnumerateComputerNames_r_send(TALLOC_CTX *mem_ctx,
	struct tevent_context *ev,
	struct dcerpc_binding_handle *h,
	struct wkssvc_NetrEnumerateComputerNames *r);
NTSTATUS dcerpc_wkssvc_NetrEnumerateComputerNames_r_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS dcerpc_wkssvc_NetrEnumerateComputerNames_r(struct dcerpc_binding_handle *h, TALLOC_CTX *mem_ctx, struct wkssvc_NetrEnumerateComputerNames *r);
struct tevent_req *dcerpc_wkssvc_NetrEnumerateComputerNames_send(TALLOC_CTX *mem_ctx,
								 struct tevent_context *ev,
								 struct dcerpc_binding_handle *h,
								 const char *_server_name /* [in] [unique,charset(UTF16)] */,
								 enum wkssvc_ComputerNameType _name_type /* [in]  */,
								 uint32_t _Reserved /* [in]  */,
								 struct wkssvc_ComputerNamesCtr **_ctr /* [out] [ref] */);
NTSTATUS dcerpc_wkssvc_NetrEnumerateComputerNames_recv(struct tevent_req *req,
						       TALLOC_CTX *mem_ctx,
						       WERROR *result);
NTSTATUS dcerpc_wkssvc_NetrEnumerateComputerNames(struct dcerpc_binding_handle *h,
						  TALLOC_CTX *mem_ctx,
						  const char *_server_name /* [in] [unique,charset(UTF16)] */,
						  enum wkssvc_ComputerNameType _name_type /* [in]  */,
						  uint32_t _Reserved /* [in]  */,
						  struct wkssvc_ComputerNamesCtr **_ctr /* [out] [ref] */,
						  WERROR *result);

#endif /* _HEADER_RPC_wkssvc */
