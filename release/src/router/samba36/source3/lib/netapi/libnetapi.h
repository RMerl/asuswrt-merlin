/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
 *  Copyright (C) Guenther Deschner 2007-2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBNETAPI_LIBNETAPI__
#define __LIBNETAPI_LIBNETAPI__
NET_API_STATUS NetJoinDomain(const char * server /* [in] [unique] */,
			     const char * domain /* [in] [ref] */,
			     const char * account_ou /* [in] [unique] */,
			     const char * account /* [in] [unique] */,
			     const char * password /* [in] [unique] */,
			     uint32_t join_flags /* [in] */);
WERROR NetJoinDomain_r(struct libnetapi_ctx *ctx,
		       struct NetJoinDomain *r);
WERROR NetJoinDomain_l(struct libnetapi_ctx *ctx,
		       struct NetJoinDomain *r);
NET_API_STATUS NetUnjoinDomain(const char * server_name /* [in] [unique] */,
			       const char * account /* [in] [unique] */,
			       const char * password /* [in] [unique] */,
			       uint32_t unjoin_flags /* [in] */);
WERROR NetUnjoinDomain_r(struct libnetapi_ctx *ctx,
			 struct NetUnjoinDomain *r);
WERROR NetUnjoinDomain_l(struct libnetapi_ctx *ctx,
			 struct NetUnjoinDomain *r);
NET_API_STATUS NetGetJoinInformation(const char * server_name /* [in] [unique] */,
				     const char * *name_buffer /* [out] [ref] */,
				     uint16_t *name_type /* [out] [ref] */);
WERROR NetGetJoinInformation_r(struct libnetapi_ctx *ctx,
			       struct NetGetJoinInformation *r);
WERROR NetGetJoinInformation_l(struct libnetapi_ctx *ctx,
			       struct NetGetJoinInformation *r);
NET_API_STATUS NetGetJoinableOUs(const char * server_name /* [in] [unique] */,
				 const char * domain /* [in] [ref] */,
				 const char * account /* [in] [unique] */,
				 const char * password /* [in] [unique] */,
				 uint32_t *ou_count /* [out] [ref] */,
				 const char * **ous /* [out] [ref] */);
WERROR NetGetJoinableOUs_r(struct libnetapi_ctx *ctx,
			   struct NetGetJoinableOUs *r);
WERROR NetGetJoinableOUs_l(struct libnetapi_ctx *ctx,
			   struct NetGetJoinableOUs *r);
NET_API_STATUS NetRenameMachineInDomain(const char * server_name /* [in] */,
					const char * new_machine_name /* [in] */,
					const char * account /* [in] */,
					const char * password /* [in] */,
					uint32_t rename_options /* [in] */);
WERROR NetRenameMachineInDomain_r(struct libnetapi_ctx *ctx,
				  struct NetRenameMachineInDomain *r);
WERROR NetRenameMachineInDomain_l(struct libnetapi_ctx *ctx,
				  struct NetRenameMachineInDomain *r);
NET_API_STATUS NetServerGetInfo(const char * server_name /* [in] [unique] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */);
WERROR NetServerGetInfo_r(struct libnetapi_ctx *ctx,
			  struct NetServerGetInfo *r);
WERROR NetServerGetInfo_l(struct libnetapi_ctx *ctx,
			  struct NetServerGetInfo *r);
NET_API_STATUS NetServerSetInfo(const char * server_name /* [in] [unique] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_error /* [out] [ref] */);
WERROR NetServerSetInfo_r(struct libnetapi_ctx *ctx,
			  struct NetServerSetInfo *r);
WERROR NetServerSetInfo_l(struct libnetapi_ctx *ctx,
			  struct NetServerSetInfo *r);
NET_API_STATUS NetGetDCName(const char * server_name /* [in] [unique] */,
			    const char * domain_name /* [in] [unique] */,
			    uint8_t **buffer /* [out] [ref] */);
WERROR NetGetDCName_r(struct libnetapi_ctx *ctx,
		      struct NetGetDCName *r);
WERROR NetGetDCName_l(struct libnetapi_ctx *ctx,
		      struct NetGetDCName *r);
NET_API_STATUS NetGetAnyDCName(const char * server_name /* [in] [unique] */,
			       const char * domain_name /* [in] [unique] */,
			       uint8_t **buffer /* [out] [ref] */);
WERROR NetGetAnyDCName_r(struct libnetapi_ctx *ctx,
			 struct NetGetAnyDCName *r);
WERROR NetGetAnyDCName_l(struct libnetapi_ctx *ctx,
			 struct NetGetAnyDCName *r);
NET_API_STATUS DsGetDcName(const char * server_name /* [in] [unique] */,
			   const char * domain_name /* [in] [ref] */,
			   struct GUID *domain_guid /* [in] [unique] */,
			   const char * site_name /* [in] [unique] */,
			   uint32_t flags /* [in] */,
			   struct DOMAIN_CONTROLLER_INFO **dc_info /* [out] [ref] */);
WERROR DsGetDcName_r(struct libnetapi_ctx *ctx,
		     struct DsGetDcName *r);
WERROR DsGetDcName_l(struct libnetapi_ctx *ctx,
		     struct DsGetDcName *r);
NET_API_STATUS NetUserAdd(const char * server_name /* [in] [unique] */,
			  uint32_t level /* [in] */,
			  uint8_t *buffer /* [in] [ref] */,
			  uint32_t *parm_error /* [out] [ref] */);
WERROR NetUserAdd_r(struct libnetapi_ctx *ctx,
		    struct NetUserAdd *r);
WERROR NetUserAdd_l(struct libnetapi_ctx *ctx,
		    struct NetUserAdd *r);
NET_API_STATUS NetUserDel(const char * server_name /* [in] [unique] */,
			  const char * user_name /* [in] [ref] */);
WERROR NetUserDel_r(struct libnetapi_ctx *ctx,
		    struct NetUserDel *r);
WERROR NetUserDel_l(struct libnetapi_ctx *ctx,
		    struct NetUserDel *r);
NET_API_STATUS NetUserEnum(const char * server_name /* [in] [unique] */,
			   uint32_t level /* [in] */,
			   uint32_t filter /* [in] */,
			   uint8_t **buffer /* [out] [ref] */,
			   uint32_t prefmaxlen /* [in] */,
			   uint32_t *entries_read /* [out] [ref] */,
			   uint32_t *total_entries /* [out] [ref] */,
			   uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetUserEnum_r(struct libnetapi_ctx *ctx,
		     struct NetUserEnum *r);
WERROR NetUserEnum_l(struct libnetapi_ctx *ctx,
		     struct NetUserEnum *r);
NET_API_STATUS NetUserChangePassword(const char * domain_name /* [in] */,
				     const char * user_name /* [in] */,
				     const char * old_password /* [in] */,
				     const char * new_password /* [in] */);
WERROR NetUserChangePassword_r(struct libnetapi_ctx *ctx,
			       struct NetUserChangePassword *r);
WERROR NetUserChangePassword_l(struct libnetapi_ctx *ctx,
			       struct NetUserChangePassword *r);
NET_API_STATUS NetUserGetInfo(const char * server_name /* [in] */,
			      const char * user_name /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t **buffer /* [out] [ref] */);
WERROR NetUserGetInfo_r(struct libnetapi_ctx *ctx,
			struct NetUserGetInfo *r);
WERROR NetUserGetInfo_l(struct libnetapi_ctx *ctx,
			struct NetUserGetInfo *r);
NET_API_STATUS NetUserSetInfo(const char * server_name /* [in] */,
			      const char * user_name /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t *buffer /* [in] [ref] */,
			      uint32_t *parm_err /* [out] [ref] */);
WERROR NetUserSetInfo_r(struct libnetapi_ctx *ctx,
			struct NetUserSetInfo *r);
WERROR NetUserSetInfo_l(struct libnetapi_ctx *ctx,
			struct NetUserSetInfo *r);
NET_API_STATUS NetUserGetGroups(const char * server_name /* [in] */,
				const char * user_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */,
				uint32_t prefmaxlen /* [in] */,
				uint32_t *entries_read /* [out] [ref] */,
				uint32_t *total_entries /* [out] [ref] */);
WERROR NetUserGetGroups_r(struct libnetapi_ctx *ctx,
			  struct NetUserGetGroups *r);
WERROR NetUserGetGroups_l(struct libnetapi_ctx *ctx,
			  struct NetUserGetGroups *r);
NET_API_STATUS NetUserSetGroups(const char * server_name /* [in] */,
				const char * user_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t num_entries /* [in] */);
WERROR NetUserSetGroups_r(struct libnetapi_ctx *ctx,
			  struct NetUserSetGroups *r);
WERROR NetUserSetGroups_l(struct libnetapi_ctx *ctx,
			  struct NetUserSetGroups *r);
NET_API_STATUS NetUserGetLocalGroups(const char * server_name /* [in] */,
				     const char * user_name /* [in] */,
				     uint32_t level /* [in] */,
				     uint32_t flags /* [in] */,
				     uint8_t **buffer /* [out] [ref] */,
				     uint32_t prefmaxlen /* [in] */,
				     uint32_t *entries_read /* [out] [ref] */,
				     uint32_t *total_entries /* [out] [ref] */);
WERROR NetUserGetLocalGroups_r(struct libnetapi_ctx *ctx,
			       struct NetUserGetLocalGroups *r);
WERROR NetUserGetLocalGroups_l(struct libnetapi_ctx *ctx,
			       struct NetUserGetLocalGroups *r);
NET_API_STATUS NetUserModalsGet(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */);
WERROR NetUserModalsGet_r(struct libnetapi_ctx *ctx,
			  struct NetUserModalsGet *r);
WERROR NetUserModalsGet_l(struct libnetapi_ctx *ctx,
			  struct NetUserModalsGet *r);
NET_API_STATUS NetUserModalsSet(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_err /* [out] [ref] */);
WERROR NetUserModalsSet_r(struct libnetapi_ctx *ctx,
			  struct NetUserModalsSet *r);
WERROR NetUserModalsSet_l(struct libnetapi_ctx *ctx,
			  struct NetUserModalsSet *r);
NET_API_STATUS NetQueryDisplayInformation(const char * server_name /* [in] [unique] */,
					  uint32_t level /* [in] */,
					  uint32_t idx /* [in] */,
					  uint32_t entries_requested /* [in] */,
					  uint32_t prefmaxlen /* [in] */,
					  uint32_t *entries_read /* [out] [ref] */,
					  void **buffer /* [out] [noprint,ref] */);
WERROR NetQueryDisplayInformation_r(struct libnetapi_ctx *ctx,
				    struct NetQueryDisplayInformation *r);
WERROR NetQueryDisplayInformation_l(struct libnetapi_ctx *ctx,
				    struct NetQueryDisplayInformation *r);
NET_API_STATUS NetGroupAdd(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t *buffer /* [in] [ref] */,
			   uint32_t *parm_err /* [out] [ref] */);
WERROR NetGroupAdd_r(struct libnetapi_ctx *ctx,
		     struct NetGroupAdd *r);
WERROR NetGroupAdd_l(struct libnetapi_ctx *ctx,
		     struct NetGroupAdd *r);
NET_API_STATUS NetGroupDel(const char * server_name /* [in] */,
			   const char * group_name /* [in] */);
WERROR NetGroupDel_r(struct libnetapi_ctx *ctx,
		     struct NetGroupDel *r);
WERROR NetGroupDel_l(struct libnetapi_ctx *ctx,
		     struct NetGroupDel *r);
NET_API_STATUS NetGroupEnum(const char * server_name /* [in] */,
			    uint32_t level /* [in] */,
			    uint8_t **buffer /* [out] [ref] */,
			    uint32_t prefmaxlen /* [in] */,
			    uint32_t *entries_read /* [out] [ref] */,
			    uint32_t *total_entries /* [out] [ref] */,
			    uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetGroupEnum_r(struct libnetapi_ctx *ctx,
		      struct NetGroupEnum *r);
WERROR NetGroupEnum_l(struct libnetapi_ctx *ctx,
		      struct NetGroupEnum *r);
NET_API_STATUS NetGroupSetInfo(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t *buffer /* [in] [ref] */,
			       uint32_t *parm_err /* [out] [ref] */);
WERROR NetGroupSetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetGroupSetInfo *r);
WERROR NetGroupSetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetGroupSetInfo *r);
NET_API_STATUS NetGroupGetInfo(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t **buffer /* [out] [ref] */);
WERROR NetGroupGetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetGroupGetInfo *r);
WERROR NetGroupGetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetGroupGetInfo *r);
NET_API_STATUS NetGroupAddUser(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       const char * user_name /* [in] */);
WERROR NetGroupAddUser_r(struct libnetapi_ctx *ctx,
			 struct NetGroupAddUser *r);
WERROR NetGroupAddUser_l(struct libnetapi_ctx *ctx,
			 struct NetGroupAddUser *r);
NET_API_STATUS NetGroupDelUser(const char * server_name /* [in] */,
			       const char * group_name /* [in] */,
			       const char * user_name /* [in] */);
WERROR NetGroupDelUser_r(struct libnetapi_ctx *ctx,
			 struct NetGroupDelUser *r);
WERROR NetGroupDelUser_l(struct libnetapi_ctx *ctx,
			 struct NetGroupDelUser *r);
NET_API_STATUS NetGroupGetUsers(const char * server_name /* [in] */,
				const char * group_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t **buffer /* [out] [ref] */,
				uint32_t prefmaxlen /* [in] */,
				uint32_t *entries_read /* [out] [ref] */,
				uint32_t *total_entries /* [out] [ref] */,
				uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetGroupGetUsers_r(struct libnetapi_ctx *ctx,
			  struct NetGroupGetUsers *r);
WERROR NetGroupGetUsers_l(struct libnetapi_ctx *ctx,
			  struct NetGroupGetUsers *r);
NET_API_STATUS NetGroupSetUsers(const char * server_name /* [in] */,
				const char * group_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t num_entries /* [in] */);
WERROR NetGroupSetUsers_r(struct libnetapi_ctx *ctx,
			  struct NetGroupSetUsers *r);
WERROR NetGroupSetUsers_l(struct libnetapi_ctx *ctx,
			  struct NetGroupSetUsers *r);
NET_API_STATUS NetLocalGroupAdd(const char * server_name /* [in] */,
				uint32_t level /* [in] */,
				uint8_t *buffer /* [in] [ref] */,
				uint32_t *parm_err /* [out] [ref] */);
WERROR NetLocalGroupAdd_r(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupAdd *r);
WERROR NetLocalGroupAdd_l(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupAdd *r);
NET_API_STATUS NetLocalGroupDel(const char * server_name /* [in] */,
				const char * group_name /* [in] */);
WERROR NetLocalGroupDel_r(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupDel *r);
WERROR NetLocalGroupDel_l(struct libnetapi_ctx *ctx,
			  struct NetLocalGroupDel *r);
NET_API_STATUS NetLocalGroupGetInfo(const char * server_name /* [in] */,
				    const char * group_name /* [in] */,
				    uint32_t level /* [in] */,
				    uint8_t **buffer /* [out] [ref] */);
WERROR NetLocalGroupGetInfo_r(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupGetInfo *r);
WERROR NetLocalGroupGetInfo_l(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupGetInfo *r);
NET_API_STATUS NetLocalGroupSetInfo(const char * server_name /* [in] */,
				    const char * group_name /* [in] */,
				    uint32_t level /* [in] */,
				    uint8_t *buffer /* [in] [ref] */,
				    uint32_t *parm_err /* [out] [ref] */);
WERROR NetLocalGroupSetInfo_r(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupSetInfo *r);
WERROR NetLocalGroupSetInfo_l(struct libnetapi_ctx *ctx,
			      struct NetLocalGroupSetInfo *r);
NET_API_STATUS NetLocalGroupEnum(const char * server_name /* [in] */,
				 uint32_t level /* [in] */,
				 uint8_t **buffer /* [out] [ref] */,
				 uint32_t prefmaxlen /* [in] */,
				 uint32_t *entries_read /* [out] [ref] */,
				 uint32_t *total_entries /* [out] [ref] */,
				 uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetLocalGroupEnum_r(struct libnetapi_ctx *ctx,
			   struct NetLocalGroupEnum *r);
WERROR NetLocalGroupEnum_l(struct libnetapi_ctx *ctx,
			   struct NetLocalGroupEnum *r);
NET_API_STATUS NetLocalGroupAddMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */);
WERROR NetLocalGroupAddMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupAddMembers *r);
WERROR NetLocalGroupAddMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupAddMembers *r);
NET_API_STATUS NetLocalGroupDelMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */);
WERROR NetLocalGroupDelMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupDelMembers *r);
WERROR NetLocalGroupDelMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupDelMembers *r);
NET_API_STATUS NetLocalGroupGetMembers(const char * server_name /* [in] */,
				       const char * local_group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t **buffer /* [out] [ref] */,
				       uint32_t prefmaxlen /* [in] */,
				       uint32_t *entries_read /* [out] [ref] */,
				       uint32_t *total_entries /* [out] [ref] */,
				       uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetLocalGroupGetMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupGetMembers *r);
WERROR NetLocalGroupGetMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupGetMembers *r);
NET_API_STATUS NetLocalGroupSetMembers(const char * server_name /* [in] */,
				       const char * group_name /* [in] */,
				       uint32_t level /* [in] */,
				       uint8_t *buffer /* [in] [ref] */,
				       uint32_t total_entries /* [in] */);
WERROR NetLocalGroupSetMembers_r(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupSetMembers *r);
WERROR NetLocalGroupSetMembers_l(struct libnetapi_ctx *ctx,
				 struct NetLocalGroupSetMembers *r);
NET_API_STATUS NetRemoteTOD(const char * server_name /* [in] */,
			    uint8_t **buffer /* [out] [ref] */);
WERROR NetRemoteTOD_r(struct libnetapi_ctx *ctx,
		      struct NetRemoteTOD *r);
WERROR NetRemoteTOD_l(struct libnetapi_ctx *ctx,
		      struct NetRemoteTOD *r);
NET_API_STATUS NetShareAdd(const char * server_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t *buffer /* [in] [ref] */,
			   uint32_t *parm_err /* [out] [ref] */);
WERROR NetShareAdd_r(struct libnetapi_ctx *ctx,
		     struct NetShareAdd *r);
WERROR NetShareAdd_l(struct libnetapi_ctx *ctx,
		     struct NetShareAdd *r);
NET_API_STATUS NetShareDel(const char * server_name /* [in] */,
			   const char * net_name /* [in] */,
			   uint32_t reserved /* [in] */);
WERROR NetShareDel_r(struct libnetapi_ctx *ctx,
		     struct NetShareDel *r);
WERROR NetShareDel_l(struct libnetapi_ctx *ctx,
		     struct NetShareDel *r);
NET_API_STATUS NetShareEnum(const char * server_name /* [in] */,
			    uint32_t level /* [in] */,
			    uint8_t **buffer /* [out] [ref] */,
			    uint32_t prefmaxlen /* [in] */,
			    uint32_t *entries_read /* [out] [ref] */,
			    uint32_t *total_entries /* [out] [ref] */,
			    uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetShareEnum_r(struct libnetapi_ctx *ctx,
		      struct NetShareEnum *r);
WERROR NetShareEnum_l(struct libnetapi_ctx *ctx,
		      struct NetShareEnum *r);
NET_API_STATUS NetShareGetInfo(const char * server_name /* [in] */,
			       const char * net_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t **buffer /* [out] [ref] */);
WERROR NetShareGetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetShareGetInfo *r);
WERROR NetShareGetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetShareGetInfo *r);
NET_API_STATUS NetShareSetInfo(const char * server_name /* [in] */,
			       const char * net_name /* [in] */,
			       uint32_t level /* [in] */,
			       uint8_t *buffer /* [in] [ref] */,
			       uint32_t *parm_err /* [out] [ref] */);
WERROR NetShareSetInfo_r(struct libnetapi_ctx *ctx,
			 struct NetShareSetInfo *r);
WERROR NetShareSetInfo_l(struct libnetapi_ctx *ctx,
			 struct NetShareSetInfo *r);
NET_API_STATUS NetFileClose(const char * server_name /* [in] */,
			    uint32_t fileid /* [in] */);
WERROR NetFileClose_r(struct libnetapi_ctx *ctx,
		      struct NetFileClose *r);
WERROR NetFileClose_l(struct libnetapi_ctx *ctx,
		      struct NetFileClose *r);
NET_API_STATUS NetFileGetInfo(const char * server_name /* [in] */,
			      uint32_t fileid /* [in] */,
			      uint32_t level /* [in] */,
			      uint8_t **buffer /* [out] [ref] */);
WERROR NetFileGetInfo_r(struct libnetapi_ctx *ctx,
			struct NetFileGetInfo *r);
WERROR NetFileGetInfo_l(struct libnetapi_ctx *ctx,
			struct NetFileGetInfo *r);
NET_API_STATUS NetFileEnum(const char * server_name /* [in] */,
			   const char * base_path /* [in] */,
			   const char * user_name /* [in] */,
			   uint32_t level /* [in] */,
			   uint8_t **buffer /* [out] [ref] */,
			   uint32_t prefmaxlen /* [in] */,
			   uint32_t *entries_read /* [out] [ref] */,
			   uint32_t *total_entries /* [out] [ref] */,
			   uint32_t *resume_handle /* [in,out] [ref] */);
WERROR NetFileEnum_r(struct libnetapi_ctx *ctx,
		     struct NetFileEnum *r);
WERROR NetFileEnum_l(struct libnetapi_ctx *ctx,
		     struct NetFileEnum *r);
NET_API_STATUS NetShutdownInit(const char * server_name /* [in] */,
			       const char * message /* [in] */,
			       uint32_t timeout /* [in] */,
			       uint8_t force_apps /* [in] */,
			       uint8_t do_reboot /* [in] */);
WERROR NetShutdownInit_r(struct libnetapi_ctx *ctx,
			 struct NetShutdownInit *r);
WERROR NetShutdownInit_l(struct libnetapi_ctx *ctx,
			 struct NetShutdownInit *r);
NET_API_STATUS NetShutdownAbort(const char * server_name /* [in] */);
WERROR NetShutdownAbort_r(struct libnetapi_ctx *ctx,
			  struct NetShutdownAbort *r);
WERROR NetShutdownAbort_l(struct libnetapi_ctx *ctx,
			  struct NetShutdownAbort *r);
NET_API_STATUS I_NetLogonControl(const char * server_name /* [in] */,
				 uint32_t function_code /* [in] */,
				 uint32_t query_level /* [in] */,
				 uint8_t **buffer /* [out] [ref] */);
WERROR I_NetLogonControl_r(struct libnetapi_ctx *ctx,
			   struct I_NetLogonControl *r);
WERROR I_NetLogonControl_l(struct libnetapi_ctx *ctx,
			   struct I_NetLogonControl *r);
NET_API_STATUS I_NetLogonControl2(const char * server_name /* [in] */,
				  uint32_t function_code /* [in] */,
				  uint32_t query_level /* [in] */,
				  uint8_t *data /* [in] [ref] */,
				  uint8_t **buffer /* [out] [ref] */);
WERROR I_NetLogonControl2_r(struct libnetapi_ctx *ctx,
			    struct I_NetLogonControl2 *r);
WERROR I_NetLogonControl2_l(struct libnetapi_ctx *ctx,
			    struct I_NetLogonControl2 *r);
#endif /* __LIBNETAPI_LIBNETAPI__ */
