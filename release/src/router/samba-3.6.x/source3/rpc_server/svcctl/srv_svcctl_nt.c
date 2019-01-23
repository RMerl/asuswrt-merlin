/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (C) Marcin Krzysztof Porwit           2005.
 *
 *  Largely Rewritten (Again) by:
 *  Copyright (C) Gerald (Jerry) Carter             2005.
 *  Copyright (C) Guenther Deschner                 2008,2009.
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

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_svcctl.h"
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "services/services.h"
#include "services/svc_winreg_glue.h"
#include "auth.h"
#include "rpc_server/svcctl/srv_svcctl_nt.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

struct service_control_op {
	const char *name;
	SERVICE_CONTROL_OPS *ops;
};

/* handle external services */
extern SERVICE_CONTROL_OPS rcinit_svc_ops;

/* builtin services (see service_db.c and services/svc_*.c */
extern SERVICE_CONTROL_OPS spoolss_svc_ops;
extern SERVICE_CONTROL_OPS netlogon_svc_ops;
extern SERVICE_CONTROL_OPS winreg_svc_ops;
extern SERVICE_CONTROL_OPS wins_svc_ops;

/* make sure this number patches the number of builtin
   SERVICE_CONTROL_OPS structure listed above */

#define SVCCTL_NUM_INTERNAL_SERVICES	4

struct service_control_op *svcctl_ops;

static const struct generic_mapping scm_generic_map =
	{ SC_MANAGER_READ_ACCESS, SC_MANAGER_WRITE_ACCESS, SC_MANAGER_EXECUTE_ACCESS, SC_MANAGER_ALL_ACCESS };
static const struct generic_mapping svc_generic_map =
	{ SERVICE_READ_ACCESS, SERVICE_WRITE_ACCESS, SERVICE_EXECUTE_ACCESS, SERVICE_ALL_ACCESS };


/********************************************************************
********************************************************************/

bool init_service_op_table( void )
{
	const char **service_list = lp_svcctl_list();
	int num_services = SVCCTL_NUM_INTERNAL_SERVICES + str_list_length( service_list );
	int i;

	if ( !(svcctl_ops = TALLOC_ARRAY( NULL, struct service_control_op, num_services+1)) ) {
		DEBUG(0,("init_service_op_table: talloc() failed!\n"));
		return False;
	}

	/* services listed in smb.conf get the rc.init interface */

	for ( i=0; service_list && service_list[i]; i++ ) {
		svcctl_ops[i].name = talloc_strdup( svcctl_ops, service_list[i] );
		svcctl_ops[i].ops  = &rcinit_svc_ops;
	}

	/* add builtin services */

#ifdef PRINTER_SUPPORT
	svcctl_ops[i].name = talloc_strdup( svcctl_ops, "Spooler" );
	svcctl_ops[i].ops  = &spoolss_svc_ops;
	i++;
#endif

#ifdef NETLOGON_SUPPORT
	svcctl_ops[i].name = talloc_strdup( svcctl_ops, "NETLOGON" );
	svcctl_ops[i].ops  = &netlogon_svc_ops;
	i++;
#endif

#ifdef WINREG_SUPPORT
	svcctl_ops[i].name = talloc_strdup( svcctl_ops, "RemoteRegistry" );
	svcctl_ops[i].ops  = &winreg_svc_ops;
	i++;
#endif

	svcctl_ops[i].name = talloc_strdup( svcctl_ops, "WINS" );
	svcctl_ops[i].ops  = &wins_svc_ops;
	i++;

	/* NULL terminate the array */

	svcctl_ops[i].name = NULL;
	svcctl_ops[i].ops  = NULL;

	return True;
}

bool shutdown_service_op_table(void)
{
	TALLOC_FREE(svcctl_ops);

	return true;
}

/********************************************************************
********************************************************************/

static struct service_control_op* find_service_by_name( const char *name )
{
	int i;

	for ( i=0; svcctl_ops[i].name; i++ ) {
		if ( strequal( name, svcctl_ops[i].name ) )
			return &svcctl_ops[i];
	}

	return NULL;
}
/********************************************************************
********************************************************************/

static NTSTATUS svcctl_access_check( struct security_descriptor *sec_desc, struct security_token *token,
                                     uint32 access_desired, uint32 *access_granted )
{
	if ( geteuid() == sec_initial_uid() ) {
		DEBUG(5,("svcctl_access_check: using root's token\n"));
		token = get_root_nt_token();
	}

	return se_access_check( sec_desc, token, access_desired, access_granted);
}

/********************************************************************
********************************************************************/

static struct security_descriptor* construct_scm_sd( TALLOC_CTX *ctx )
{
	struct security_ace ace[2];
	size_t i = 0;
	struct security_descriptor *sd;
	struct security_acl *theacl;
	size_t sd_size;

	/* basic access for Everyone */

	init_sec_ace(&ace[i++], &global_sid_World,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SC_MANAGER_READ_ACCESS, 0);

	/* Full Access 'BUILTIN\Administrators' */

	init_sec_ace(&ace[i++], &global_sid_Builtin_Administrators,
		SEC_ACE_TYPE_ACCESS_ALLOWED, SC_MANAGER_ALL_ACCESS, 0);


	/* create the security descriptor */

	if ( !(theacl = make_sec_acl(ctx, NT4_ACL_REVISION, i, ace)) )
		return NULL;

	if ( !(sd = make_sec_desc(ctx, SECURITY_DESCRIPTOR_REVISION_1,
				  SEC_DESC_SELF_RELATIVE, NULL, NULL, NULL,
				  theacl, &sd_size)) )
		return NULL;

	return sd;
}

/******************************************************************
 Find a registry key handle and return a SERVICE_INFO
 *****************************************************************/

static SERVICE_INFO *find_service_info_by_hnd(struct pipes_struct *p,
					      struct policy_handle *hnd)
{
	SERVICE_INFO *service_info = NULL;

	if( !find_policy_by_hnd( p, hnd, (void **)(void *)&service_info) ) {
		DEBUG(2,("find_service_info_by_hnd: handle not found\n"));
		return NULL;
	}

	return service_info;
}

/******************************************************************
 *****************************************************************/

static WERROR create_open_service_handle(struct pipes_struct *p,
					 struct policy_handle *handle,
					 uint32_t type,
					 const char *service,
					 uint32_t access_granted)
{
	SERVICE_INFO *info = NULL;
	WERROR result = WERR_OK;
	struct service_control_op *s_op;

	if ( !(info = TALLOC_ZERO_P( NULL, SERVICE_INFO )) )
		return WERR_NOMEM;

	/* the Service Manager has a NULL name */

	info->type = SVC_HANDLE_IS_SCM;

	switch ( type ) {
	case SVC_HANDLE_IS_SCM:
		info->type = SVC_HANDLE_IS_SCM;
		break;

	case SVC_HANDLE_IS_DBLOCK:
		info->type = SVC_HANDLE_IS_DBLOCK;
		break;

	case SVC_HANDLE_IS_SERVICE:
		info->type = SVC_HANDLE_IS_SERVICE;

		/* lookup the SERVICE_CONTROL_OPS */

		if ( !(s_op = find_service_by_name( service )) ) {
			result = WERR_NO_SUCH_SERVICE;
			goto done;
		}

		info->ops = s_op->ops;

		if ( !(info->name  = talloc_strdup( info, s_op->name )) ) {
			result = WERR_NOMEM;
			goto done;
		}
		break;

	default:
		result = WERR_NO_SUCH_SERVICE;
		goto done;
	}

	info->access_granted = access_granted;

	/* store the SERVICE_INFO and create an open handle */

	if ( !create_policy_hnd( p, handle, info ) ) {
		result = WERR_ACCESS_DENIED;
		goto done;
	}

done:
	if ( !W_ERROR_IS_OK(result) )
		TALLOC_FREE(info);

	return result;
}

/********************************************************************
 _svcctl_OpenSCManagerW
********************************************************************/

WERROR _svcctl_OpenSCManagerW(struct pipes_struct *p,
			      struct svcctl_OpenSCManagerW *r)
{
	struct security_descriptor *sec_desc;
	uint32 access_granted = 0;
	NTSTATUS status;

	/* perform access checks */

	if ( !(sec_desc = construct_scm_sd( p->mem_ctx )) )
		return WERR_NOMEM;

	se_map_generic( &r->in.access_mask, &scm_generic_map );
	status = svcctl_access_check( sec_desc, p->session_info->security_token,
				      r->in.access_mask, &access_granted );
	if ( !NT_STATUS_IS_OK(status) )
		return ntstatus_to_werror( status );

	return create_open_service_handle( p, r->out.handle, SVC_HANDLE_IS_SCM, NULL, access_granted );
}

/********************************************************************
 _svcctl_OpenServiceW
********************************************************************/

WERROR _svcctl_OpenServiceW(struct pipes_struct *p,
			    struct svcctl_OpenServiceW *r)
{
	struct security_descriptor *sec_desc;
	uint32 access_granted = 0;
	NTSTATUS status;
	const char *service = NULL;

	service = r->in.ServiceName;
	if (!service) {
		return WERR_NOMEM;
	}
	DEBUG(5, ("_svcctl_OpenServiceW: Attempting to open Service [%s], \n", service));

	/* based on my tests you can open a service if you have a valid scm handle */

	if ( !find_service_info_by_hnd( p, r->in.scmanager_handle) )
		return WERR_BADFID;

	/*
	 * Perform access checks. Use the system session_info in order to ensure
	 * that we retrieve the security descriptor
	 */
	sec_desc = svcctl_get_secdesc(p->mem_ctx,
				      p->msg_ctx,
				      get_session_info_system(),
				      service);
	if (sec_desc == NULL) {
		DEBUG(0, ("_svcctl_OpenServiceW: Failed to get a valid security "
			  "descriptor"));
		return WERR_NOMEM;
	}

	se_map_generic( &r->in.access_mask, &svc_generic_map );
	status = svcctl_access_check( sec_desc, p->session_info->security_token,
				      r->in.access_mask, &access_granted );
	if ( !NT_STATUS_IS_OK(status) )
		return ntstatus_to_werror( status );

	return create_open_service_handle( p, r->out.handle, SVC_HANDLE_IS_SERVICE, service, access_granted );
}

/********************************************************************
 _svcctl_CloseServiceHandle
********************************************************************/

WERROR _svcctl_CloseServiceHandle(struct pipes_struct *p,
				  struct svcctl_CloseServiceHandle *r)
{
	if ( !close_policy_hnd( p, r->in.handle ) )
		return  WERR_BADFID;

	ZERO_STRUCTP(r->out.handle);

	return WERR_OK;
}

/********************************************************************
 _svcctl_GetServiceDisplayNameW
********************************************************************/

WERROR _svcctl_GetServiceDisplayNameW(struct pipes_struct *p,
				      struct svcctl_GetServiceDisplayNameW *r)
{
	const char *service;
	const char *display_name;
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );

	/* can only use an SCM handle here */

	if ( !info || (info->type != SVC_HANDLE_IS_SCM) )
		return WERR_BADFID;

	service = r->in.service_name;

	display_name = svcctl_lookup_dispname(p->mem_ctx,
					      p->msg_ctx,
					      p->session_info,
					      service);
	if (!display_name) {
		display_name = "";
	}

	*r->out.display_name = display_name;
	*r->out.display_name_length = strlen(display_name);

	return WERR_OK;
}

/********************************************************************
 _svcctl_QueryServiceStatus
********************************************************************/

WERROR _svcctl_QueryServiceStatus(struct pipes_struct *p,
				  struct svcctl_QueryServiceStatus *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_SVC_QUERY_STATUS) )
		return WERR_ACCESS_DENIED;

	/* try the service specific status call */

	return info->ops->service_status( info->name, r->out.service_status );
}

/********************************************************************
********************************************************************/

static int enumerate_status(TALLOC_CTX *ctx,
			    struct messaging_context *msg_ctx,
			    struct auth_serversupplied_info *session_info,
			    struct ENUM_SERVICE_STATUSW **status)
{
	int num_services = 0;
	int i;
	struct ENUM_SERVICE_STATUSW *st;
	const char *display_name;

	/* just count */
	while ( svcctl_ops[num_services].name )
		num_services++;

	if ( !(st = TALLOC_ARRAY( ctx, struct ENUM_SERVICE_STATUSW, num_services )) ) {
		DEBUG(0,("enumerate_status: talloc() failed!\n"));
		return -1;
	}

	for ( i=0; i<num_services; i++ ) {
		st[i].service_name = talloc_strdup(st, svcctl_ops[i].name );

		display_name = svcctl_lookup_dispname(ctx,
						      msg_ctx,
						      session_info,
						      svcctl_ops[i].name);
		st[i].display_name = talloc_strdup(st, display_name ? display_name : "");

		svcctl_ops[i].ops->service_status( svcctl_ops[i].name, &st[i].status );
	}

	*status = st;

	return num_services;
}

/********************************************************************
 _svcctl_EnumServicesStatusW
********************************************************************/

WERROR _svcctl_EnumServicesStatusW(struct pipes_struct *p,
				   struct svcctl_EnumServicesStatusW *r)
{
	struct ENUM_SERVICE_STATUSW *services = NULL;
	int num_services;
	int i = 0;
	size_t buffer_size = 0;
	WERROR result = WERR_OK;
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );
	DATA_BLOB blob = data_blob_null;

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SCM) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_MGR_ENUMERATE_SERVICE) ) {
		return WERR_ACCESS_DENIED;
	}

	num_services = enumerate_status(p->mem_ctx,
					p->msg_ctx,
					p->session_info,
					&services);
	if (num_services == -1 ) {
		return WERR_NOMEM;
	}

        for ( i=0; i<num_services; i++ ) {
		buffer_size += ndr_size_ENUM_SERVICE_STATUSW(&services[i], 0);
	}

	buffer_size += buffer_size % 4;

	if (buffer_size > r->in.offered) {
		num_services = 0;
		result = WERR_MORE_DATA;
	}

	if ( W_ERROR_IS_OK(result) ) {

		enum ndr_err_code ndr_err;
		struct ndr_push *ndr;

		ndr = ndr_push_init_ctx(p->mem_ctx);
		if (ndr == NULL) {
			return WERR_INVALID_PARAM;
		}

		ndr_err = ndr_push_ENUM_SERVICE_STATUSW_array(
			ndr, num_services, services);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ntstatus_to_werror(ndr_map_error2ntstatus(ndr_err));
		}
		blob = ndr_push_blob(ndr);
		memcpy(r->out.service, blob.data, MIN(blob.length, r->in.offered));
	}

	*r->out.needed			= (buffer_size > r->in.offered) ? buffer_size : r->in.offered;
	*r->out.services_returned	= (uint32)num_services;
	if (r->out.resume_handle) {
		*r->out.resume_handle	= 0;
	}

	return result;
}

/********************************************************************
 _svcctl_StartServiceW
********************************************************************/

WERROR _svcctl_StartServiceW(struct pipes_struct *p,
			     struct svcctl_StartServiceW *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_SVC_START) )
		return WERR_ACCESS_DENIED;

	return info->ops->start_service( info->name );
}

/********************************************************************
 _svcctl_ControlService
********************************************************************/

WERROR _svcctl_ControlService(struct pipes_struct *p,
			      struct svcctl_ControlService *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	switch ( r->in.control ) {
	case SVCCTL_CONTROL_STOP:
		if ( !(info->access_granted & SC_RIGHT_SVC_STOP) )
			return WERR_ACCESS_DENIED;

		return info->ops->stop_service( info->name,
						r->out.service_status );

	case SVCCTL_CONTROL_INTERROGATE:
		if ( !(info->access_granted & SC_RIGHT_SVC_QUERY_STATUS) )
			return WERR_ACCESS_DENIED;

		return info->ops->service_status( info->name,
						  r->out.service_status );
	default:
		return WERR_INVALID_PARAM;
	}
}

/********************************************************************
 _svcctl_EnumDependentServicesW
********************************************************************/

WERROR _svcctl_EnumDependentServicesW(struct pipes_struct *p,
				      struct svcctl_EnumDependentServicesW *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.service );

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_SVC_ENUMERATE_DEPENDENTS) )
		return WERR_ACCESS_DENIED;

	switch (r->in.state) {
	case SERVICE_STATE_ACTIVE:
	case SERVICE_STATE_INACTIVE:
	case SERVICE_STATE_ALL:
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	/* we have to set the outgoing buffer size to the same as the
	   incoming buffer size (even in the case of failure */
	/* this is done in the autogenerated server already - gd */

	*r->out.needed = r->in.offered;

	/* no dependent services...basically a stub function */
	*r->out.services_returned = 0;

	return WERR_OK;
}

/********************************************************************
 _svcctl_QueryServiceStatusEx
********************************************************************/

WERROR _svcctl_QueryServiceStatusEx(struct pipes_struct *p,
				    struct svcctl_QueryServiceStatusEx *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );
	uint32 buffer_size;

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_SVC_QUERY_STATUS) )
		return WERR_ACCESS_DENIED;

	/* we have to set the outgoing buffer size to the same as the
	   incoming buffer size (even in the case of failure) */
	*r->out.needed = r->in.offered;

	switch ( r->in.info_level ) {
		case SVC_STATUS_PROCESS_INFO:
		{
			struct SERVICE_STATUS_PROCESS svc_stat_proc;
			enum ndr_err_code ndr_err;
			DATA_BLOB blob;

			/* Get the status of the service.. */
			info->ops->service_status( info->name, &svc_stat_proc.status );
			svc_stat_proc.process_id     = sys_getpid();
			svc_stat_proc.service_flags  = 0x0;

			ndr_err = ndr_push_struct_blob(&blob, p->mem_ctx, &svc_stat_proc,
						       (ndr_push_flags_fn_t)ndr_push_SERVICE_STATUS_PROCESS);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return WERR_INVALID_PARAM;
			}

			r->out.buffer = blob.data;
	                buffer_size = sizeof(struct SERVICE_STATUS_PROCESS);
			break;
		}

		default:
			return WERR_UNKNOWN_LEVEL;
	}


        buffer_size += buffer_size % 4;
	*r->out.needed = (buffer_size > r->in.offered) ? buffer_size : r->in.offered;

        if (buffer_size > r->in.offered ) {
                return WERR_INSUFFICIENT_BUFFER;
	}

	return WERR_OK;
}

/********************************************************************
********************************************************************/

static WERROR fill_svc_config(TALLOC_CTX *mem_ctx,
			      struct messaging_context *msg_ctx,
			      struct auth_serversupplied_info *session_info,
			      const char *name,
			      struct QUERY_SERVICE_CONFIG *config)
{
	const char *result = NULL;

	/* now fill in the individual values */

	ZERO_STRUCTP(config);

	config->displayname = svcctl_lookup_dispname(mem_ctx,
						     msg_ctx,
						     session_info,
						     name);

	result = svcctl_get_string_value(mem_ctx,
					 msg_ctx,
					 session_info,
					 name,
					 "ObjectName");
	if (result != NULL) {
		config->startname = result;
	}

	result = svcctl_get_string_value(mem_ctx,
					 msg_ctx,
					 session_info,
					 name,
					 "ImagePath");
	if (result != NULL) {
		config->executablepath = result;
	}

	/* a few hard coded values */
	/* loadordergroup and dependencies are empty */

	config->tag_id           = 0x00000000;			/* unassigned loadorder group */
	config->service_type     = SERVICE_TYPE_WIN32_OWN_PROCESS;
	config->error_control    = SVCCTL_SVC_ERROR_NORMAL;

	/* set the start type.  NetLogon and WINS are disabled to prevent
	   the client from showing the "Start" button (if of course the services
	   are not running */

	if ( strequal( name, "NETLOGON" ) && ( lp_servicenumber(name) == -1 ) )
		config->start_type = SVCCTL_DISABLED;
	else if ( strequal( name, "WINS" ) && ( !lp_wins_support() ))
		config->start_type = SVCCTL_DISABLED;
	else
		config->start_type = SVCCTL_DEMAND_START;

	return WERR_OK;
}

/********************************************************************
 _svcctl_QueryServiceConfigW
********************************************************************/

WERROR _svcctl_QueryServiceConfigW(struct pipes_struct *p,
				   struct svcctl_QueryServiceConfigW *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );
	uint32 buffer_size;
	WERROR wresult;

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_SVC_QUERY_CONFIG) )
		return WERR_ACCESS_DENIED;

	/* we have to set the outgoing buffer size to the same as the
	   incoming buffer size (even in the case of failure */

	*r->out.needed = r->in.offered;

	wresult = fill_svc_config(p->mem_ctx,
				  p->msg_ctx,
				  p->session_info,
				  info->name,
				  r->out.query);
	if ( !W_ERROR_IS_OK(wresult) )
		return wresult;

	buffer_size = ndr_size_QUERY_SERVICE_CONFIG(r->out.query, 0);
	*r->out.needed = (buffer_size > r->in.offered) ? buffer_size : r->in.offered;

        if (buffer_size > r->in.offered ) {
		ZERO_STRUCTP(r->out.query);
                return WERR_INSUFFICIENT_BUFFER;
	}

	return WERR_OK;
}

/********************************************************************
 _svcctl_QueryServiceConfig2W
********************************************************************/

WERROR _svcctl_QueryServiceConfig2W(struct pipes_struct *p,
				    struct svcctl_QueryServiceConfig2W *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );
	uint32_t buffer_size;
	DATA_BLOB blob = data_blob_null;

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SERVICE) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_SVC_QUERY_CONFIG) )
		return WERR_ACCESS_DENIED;

	/* we have to set the outgoing buffer size to the same as the
	   incoming buffer size (even in the case of failure */
	*r->out.needed = r->in.offered;

	switch ( r->in.info_level ) {
	case SERVICE_CONFIG_DESCRIPTION:
		{
			struct SERVICE_DESCRIPTION desc_buf;
			const char *description;
			enum ndr_err_code ndr_err;

			description = svcctl_lookup_description(p->mem_ctx,
								p->msg_ctx,
								p->session_info,
								info->name);

			desc_buf.description = description;

			ndr_err = ndr_push_struct_blob(&blob, p->mem_ctx, &desc_buf,
						       (ndr_push_flags_fn_t)ndr_push_SERVICE_DESCRIPTION);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return WERR_INVALID_PARAM;
			}

			break;
		}
		break;
	case SERVICE_CONFIG_FAILURE_ACTIONS:
		{
			struct SERVICE_FAILURE_ACTIONS actions;
			enum ndr_err_code ndr_err;

			/* nothing to say...just service the request */

			ZERO_STRUCT( actions );

			ndr_err = ndr_push_struct_blob(&blob, p->mem_ctx, &actions,
						       (ndr_push_flags_fn_t)ndr_push_SERVICE_FAILURE_ACTIONS);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				return WERR_INVALID_PARAM;
			}

			break;
		}
		break;

	default:
		return WERR_UNKNOWN_LEVEL;
	}

	buffer_size = blob.length;
	buffer_size += buffer_size % 4;
	*r->out.needed = (buffer_size > r->in.offered) ? buffer_size : r->in.offered;

        if (buffer_size > r->in.offered)
                return WERR_INSUFFICIENT_BUFFER;

	memcpy(r->out.buffer, blob.data, blob.length);

	return WERR_OK;
}

/********************************************************************
 _svcctl_LockServiceDatabase
********************************************************************/

WERROR _svcctl_LockServiceDatabase(struct pipes_struct *p,
				   struct svcctl_LockServiceDatabase *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );

	/* perform access checks */

	if ( !info || (info->type != SVC_HANDLE_IS_SCM) )
		return WERR_BADFID;

	if ( !(info->access_granted & SC_RIGHT_MGR_LOCK) )
		return WERR_ACCESS_DENIED;

	/* Just open a handle.  Doesn't actually lock anything */

	return create_open_service_handle( p, r->out.lock, SVC_HANDLE_IS_DBLOCK, NULL, 0 );
}

/********************************************************************
 _svcctl_UnlockServiceDatabase
********************************************************************/

WERROR _svcctl_UnlockServiceDatabase(struct pipes_struct *p,
				     struct svcctl_UnlockServiceDatabase *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.lock );


	if ( !info || (info->type != SVC_HANDLE_IS_DBLOCK) )
		return WERR_BADFID;

	return close_policy_hnd( p, r->out.lock) ? WERR_OK : WERR_BADFID;
}

/********************************************************************
 _svcctl_QueryServiceObjectSecurity
********************************************************************/

WERROR _svcctl_QueryServiceObjectSecurity(struct pipes_struct *p,
					  struct svcctl_QueryServiceObjectSecurity *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );
	struct security_descriptor *sec_desc;
	NTSTATUS status;
	uint8_t *buffer = NULL;
	size_t len = 0;


	/* only support the SCM and individual services */

	if ( !info || !(info->type & (SVC_HANDLE_IS_SERVICE|SVC_HANDLE_IS_SCM)) )
		return WERR_BADFID;

	/* check access reights (according to MSDN) */

	if ( !(info->access_granted & SEC_STD_READ_CONTROL) )
		return WERR_ACCESS_DENIED;

	/* TODO: handle something besides SECINFO_DACL */

	if ( (r->in.security_flags & SECINFO_DACL) != SECINFO_DACL )
		return WERR_INVALID_PARAM;

	/* Lookup the security descriptor and marshall it up for a reply */
	sec_desc = svcctl_get_secdesc(p->mem_ctx,
				      p->msg_ctx,
				      get_session_info_system(),
				      info->name);
	if (sec_desc == NULL) {
		return WERR_NOMEM;
	}

	*r->out.needed = ndr_size_security_descriptor(sec_desc, 0);

	if ( *r->out.needed > r->in.offered) {
		return WERR_INSUFFICIENT_BUFFER;
	}

	status = marshall_sec_desc(p->mem_ctx, sec_desc, &buffer, &len);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	*r->out.needed = len;
	memcpy(r->out.buffer, buffer, len);

	return WERR_OK;
}

/********************************************************************
 _svcctl_SetServiceObjectSecurity
********************************************************************/

WERROR _svcctl_SetServiceObjectSecurity(struct pipes_struct *p,
					struct svcctl_SetServiceObjectSecurity *r)
{
	SERVICE_INFO *info = find_service_info_by_hnd( p, r->in.handle );
	struct security_descriptor *sec_desc = NULL;
	uint32 required_access;
	NTSTATUS status;

	if ( !info || !(info->type & (SVC_HANDLE_IS_SERVICE|SVC_HANDLE_IS_SCM))  )
		return WERR_BADFID;

	/* can't set the security de4scriptor on the ServiceControlManager */

	if ( info->type == SVC_HANDLE_IS_SCM )
		return WERR_ACCESS_DENIED;

	/* check the access on the open handle */

	switch ( r->in.security_flags ) {
		case SECINFO_DACL:
			required_access = SEC_STD_WRITE_DAC;
			break;

		case SECINFO_OWNER:
		case SECINFO_GROUP:
			required_access = SEC_STD_WRITE_OWNER;
			break;

		case SECINFO_SACL:
			return WERR_INVALID_PARAM;
		default:
			return WERR_INVALID_PARAM;
	}

	if ( !(info->access_granted & required_access) )
		return WERR_ACCESS_DENIED;

	/* read the security descfriptor */

	status = unmarshall_sec_desc(p->mem_ctx,
				     r->in.buffer,
				     r->in.offered,
				     &sec_desc);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	/* store the new SD */

	if (!svcctl_set_secdesc(p->msg_ctx, p->session_info, info->name, sec_desc))
		return WERR_ACCESS_DENIED;

	return WERR_OK;
}


WERROR _svcctl_DeleteService(struct pipes_struct *p,
			     struct svcctl_DeleteService *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_SetServiceStatus(struct pipes_struct *p,
				struct svcctl_SetServiceStatus *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_NotifyBootConfigStatus(struct pipes_struct *p,
				      struct svcctl_NotifyBootConfigStatus *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_SCSetServiceBitsW(struct pipes_struct *p,
				 struct svcctl_SCSetServiceBitsW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_ChangeServiceConfigW(struct pipes_struct *p,
				    struct svcctl_ChangeServiceConfigW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_CreateServiceW(struct pipes_struct *p,
			      struct svcctl_CreateServiceW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_QueryServiceLockStatusW(struct pipes_struct *p,
				       struct svcctl_QueryServiceLockStatusW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_GetServiceKeyNameW(struct pipes_struct *p,
				  struct svcctl_GetServiceKeyNameW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_SCSetServiceBitsA(struct pipes_struct *p,
				 struct svcctl_SCSetServiceBitsA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_ChangeServiceConfigA(struct pipes_struct *p,
				    struct svcctl_ChangeServiceConfigA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_CreateServiceA(struct pipes_struct *p,
			      struct svcctl_CreateServiceA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_EnumDependentServicesA(struct pipes_struct *p,
				      struct svcctl_EnumDependentServicesA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_EnumServicesStatusA(struct pipes_struct *p,
				   struct svcctl_EnumServicesStatusA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_OpenSCManagerA(struct pipes_struct *p,
			      struct svcctl_OpenSCManagerA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_OpenServiceA(struct pipes_struct *p,
			    struct svcctl_OpenServiceA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_QueryServiceConfigA(struct pipes_struct *p,
				   struct svcctl_QueryServiceConfigA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_QueryServiceLockStatusA(struct pipes_struct *p,
				       struct svcctl_QueryServiceLockStatusA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_StartServiceA(struct pipes_struct *p,
			     struct svcctl_StartServiceA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_GetServiceDisplayNameA(struct pipes_struct *p,
				      struct svcctl_GetServiceDisplayNameA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_GetServiceKeyNameA(struct pipes_struct *p,
				  struct svcctl_GetServiceKeyNameA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_GetCurrentGroupeStateW(struct pipes_struct *p,
				      struct svcctl_GetCurrentGroupeStateW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_EnumServiceGroupW(struct pipes_struct *p,
				 struct svcctl_EnumServiceGroupW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_ChangeServiceConfig2A(struct pipes_struct *p,
				     struct svcctl_ChangeServiceConfig2A *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_ChangeServiceConfig2W(struct pipes_struct *p,
				     struct svcctl_ChangeServiceConfig2W *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_QueryServiceConfig2A(struct pipes_struct *p,
				    struct svcctl_QueryServiceConfig2A *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _EnumServicesStatusExA(struct pipes_struct *p,
			      struct EnumServicesStatusExA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _EnumServicesStatusExW(struct pipes_struct *p,
			      struct EnumServicesStatusExW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

WERROR _svcctl_SCSendTSMessage(struct pipes_struct *p,
			       struct svcctl_SCSendTSMessage *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}
