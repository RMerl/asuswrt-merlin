/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) Gerald (Jerry) Carter          2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "utils/net.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_svcctl.h"
#include "../librpc/gen_ndr/ndr_svcctl_c.h"

struct svc_state_msg {
	uint32 flag;
	const char *message;
};

static struct svc_state_msg state_msg_table[] = {
	{ SVCCTL_STOPPED,            N_("stopped") },
	{ SVCCTL_START_PENDING,      N_("start pending") },
	{ SVCCTL_STOP_PENDING,       N_("stop pending") },
	{ SVCCTL_RUNNING,            N_("running") },
	{ SVCCTL_CONTINUE_PENDING,   N_("resume pending") },
	{ SVCCTL_PAUSE_PENDING,      N_("pause pending") },
	{ SVCCTL_PAUSED,             N_("paused") },
	{ 0,                          NULL }
};


/********************************************************************
********************************************************************/
const char *svc_status_string( uint32 state )
{
	fstring msg;
	int i;

	fstr_sprintf( msg, _("Unknown State [%d]"), state );

	for ( i=0; state_msg_table[i].message; i++ ) {
		if ( state_msg_table[i].flag == state ) {
			fstrcpy( msg, state_msg_table[i].message );
			break;
		}
	}

	return talloc_strdup(talloc_tos(), msg);
}

/********************************************************************
********************************************************************/

static WERROR open_service(struct dcerpc_binding_handle *b,
			   TALLOC_CTX *mem_ctx,
			   struct policy_handle *hSCM,
			   const char *service,
			   uint32_t access_mask,
			   struct policy_handle *hService)
{
	NTSTATUS status;
	WERROR result;

	status = dcerpc_svcctl_OpenServiceW(b, mem_ctx,
					    hSCM,
					    service,
					    access_mask,
					    hService,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Failed to open service.  [%s]\n"),
			  nt_errstr(status));
		return result;
	}
	if (!W_ERROR_IS_OK(result) ) {
		d_fprintf(stderr, _("Failed to open service.  [%s]\n"),
			  win_errstr(result));
		return result;
	}

	return WERR_OK;
}

/********************************************************************
********************************************************************/

static WERROR open_scm(struct dcerpc_binding_handle *b,
		       TALLOC_CTX *mem_ctx,
		       const char *server_name,
		       uint32_t access_mask,
		       struct policy_handle *hSCM)
{
	NTSTATUS status;
	WERROR result;

	status = dcerpc_svcctl_OpenSCManagerW(b, mem_ctx,
					      server_name,
					      NULL,
					      access_mask,
					      hSCM,
					      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr,
			  _("Failed to open Service Control Manager. [%s]\n"),
			  nt_errstr(status));
		return result;
	}
	if (!W_ERROR_IS_OK(result)) {
		d_fprintf(stderr,
			  _("Failed to open Service Control Manager. [%s]\n"),
			  win_errstr(result));
		return result;
	}

	return WERR_OK;
}

/********************************************************************
********************************************************************/

static WERROR query_service_state(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *hSCM,
				const char *service,
				uint32 *state )
{
	struct policy_handle hService;
	struct SERVICE_STATUS service_status;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	/* now cycle until the status is actually 'watch_state' */

	result = open_service(b, mem_ctx, hSCM, service,
			      SC_RIGHT_SVC_QUERY_STATUS,
			      &hService);
	if (!W_ERROR_IS_OK(result) ) {
		return result;
	}

	status = dcerpc_svcctl_QueryServiceStatus(b, mem_ctx,
						  &hService,
						  &service_status,
						  &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	*state = service_status.state;

 done:
	if (is_valid_policy_hnd(&hService)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hService, &_result);
	}

	return result;
}

/********************************************************************
********************************************************************/

static WERROR watch_service_state(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *hSCM,
				const char *service,
				uint32 watch_state,
				uint32 *final_state )
{
	uint32 i;
	uint32 state = 0;
	WERROR result = WERR_GENERAL_FAILURE;


	i = 0;
	while ( (state != watch_state ) && i<30 ) {
		/* get the status */

		result = query_service_state(pipe_hnd, mem_ctx, hSCM, service, &state  );
		if ( !W_ERROR_IS_OK(result) ) {
			break;
		}

		d_printf(".");
		i++;
		sys_usleep( 100 );
	}
	d_printf("\n");

	*final_state = state;

	return result;
}

/********************************************************************
********************************************************************/

static WERROR control_service(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				struct policy_handle *hSCM,
				const char *service,
				uint32 control,
				uint32 watch_state )
{
	struct policy_handle hService;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	struct SERVICE_STATUS service_status;
	uint32 state = 0;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	/* Open the Service */

	result = open_service(b, mem_ctx, hSCM, service,
			      (SC_RIGHT_SVC_STOP|SC_RIGHT_SVC_PAUSE_CONTINUE),
			      &hService);
	if (!W_ERROR_IS_OK(result) ) {
		return result;
	}

	/* get the status */

	status = dcerpc_svcctl_ControlService(b, mem_ctx,
					      &hService,
					      control,
					      &service_status,
					      &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Control service request failed.  [%s]\n"),
			  nt_errstr(status));
		goto done;
	}
	if (!W_ERROR_IS_OK(result) ) {
		d_fprintf(stderr, _("Control service request failed.  [%s]\n"),
			  win_errstr(result));
		goto done;
	}

	/* loop -- checking the state until we are where we want to be */

	result = watch_service_state(pipe_hnd, mem_ctx, hSCM, service, watch_state, &state );

	d_printf(_("%s service is %s.\n"), service, svc_status_string(state));

done:
	if (is_valid_policy_hnd(&hService)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hService, &_result);
	}

	return result;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_list_internal(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle hSCM;
	struct ENUM_SERVICE_STATUSW *services = NULL;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	int i;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	uint8_t *buffer = NULL;
	uint32_t buf_size = 0;
	uint32_t bytes_needed = 0;
	uint32_t num_services = 0;
	uint32_t resume_handle = 0;

	if (argc != 0 ) {
		d_printf("%s net rpc service list\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	do {
		status = dcerpc_svcctl_EnumServicesStatusW(b, mem_ctx,
							   &hSCM,
							   SERVICE_TYPE_WIN32,
							   SERVICE_STATE_ALL,
							   buffer,
							   buf_size,
							   &bytes_needed,
							   &num_services,
							   &resume_handle,
							   &result);

		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr,
				_("Failed to enumerate services.  [%s]\n"),
				nt_errstr(status));
			break;
		}

		if (W_ERROR_EQUAL(result, WERR_MORE_DATA) && bytes_needed > 0) {
			buffer = talloc_array(mem_ctx, uint8_t, bytes_needed);
			buf_size = bytes_needed;
			continue;
		}

		if (!W_ERROR_IS_OK(result)) {
			status = werror_to_ntstatus(result);
			d_fprintf(stderr,
				_("Failed to enumerate services.  [%s]\n"),
				win_errstr(result));
			break;
		}

		if ( num_services == 0 ) {
			d_printf(_("No services returned\n"));
			break;
		}

		{
			enum ndr_err_code ndr_err;
			DATA_BLOB blob;
			struct ndr_pull *ndr;

			blob.length = buf_size;
			blob.data = talloc_steal(mem_ctx, buffer);

			services = talloc_array(mem_ctx, struct ENUM_SERVICE_STATUSW, num_services);
			if (!services) {
				status = NT_STATUS_NO_MEMORY;
				break;
			}

			ndr = ndr_pull_init_blob(&blob, mem_ctx);
			if (ndr == NULL) {
				status = NT_STATUS_NO_MEMORY;
				break;
			}

			ndr_err = ndr_pull_ENUM_SERVICE_STATUSW_array(
				ndr, num_services, services);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				status = ndr_map_error2ntstatus(ndr_err);
				break;
			}

			for ( i=0; i<num_services; i++ ) {
				d_printf("%-20s    \"%s\"\n",
					services[i].service_name,
					services[i].display_name);
			}
		}

	} while (W_ERROR_EQUAL(result, WERR_MORE_DATA));

	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return status;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_status_internal(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv )
{
	struct policy_handle hSCM, hService;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	struct SERVICE_STATUS service_status;
	struct QUERY_SERVICE_CONFIG config;
	uint32_t buf_size = sizeof(config);
	uint32_t ret_size = 0;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1 ) {
		d_printf("%s net rpc service status <service>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	/* Open the Service */

	result = open_service(b, mem_ctx, &hSCM, argv[0],
			      (SC_RIGHT_SVC_QUERY_STATUS|SC_RIGHT_SVC_QUERY_CONFIG),
			      &hService);
	if (!W_ERROR_IS_OK(result) ) {
		goto done;
	}

	/* get the status */

	status = dcerpc_svcctl_QueryServiceStatus(b, mem_ctx,
						  &hService,
						  &service_status,
						  &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Query status request failed.  [%s]\n"),
			  nt_errstr(status));
		goto done;
	}

	if (!W_ERROR_IS_OK(result) ) {
		d_fprintf(stderr, _("Query status request failed.  [%s]\n"),
			  win_errstr(result));
		goto done;
	}

	d_printf(_("%s service is %s.\n"), argv[0],
		 svc_status_string(service_status.state));

	/* get the config */

	status = dcerpc_svcctl_QueryServiceConfigW(b, mem_ctx,
						   &hService,
						   &config,
						   buf_size,
						   &ret_size,
						   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Query config request failed.  [%s]\n"),
			  nt_errstr(status));
		goto done;
	}

	if (W_ERROR_EQUAL(result, WERR_INSUFFICIENT_BUFFER)) {
		buf_size = ret_size;
		status = dcerpc_svcctl_QueryServiceConfigW(b, mem_ctx,
							   &hService,
							   &config,
							   buf_size,
							   &ret_size,
							   &result);
		if (!NT_STATUS_IS_OK(status)) {
			result = ntstatus_to_werror(status);
			d_fprintf(stderr, _("Query config request failed.  [%s]\n"),
				  nt_errstr(status));
			goto done;
		}
	}

	if (!W_ERROR_IS_OK(result) ) {
		d_fprintf(stderr, _("Query config request failed.  [%s]\n"),
			  win_errstr(result));
		goto done;
	}

	/* print out the configuration information for the service */

	d_printf(_("Configuration details:\n"));
	d_printf(_("\tControls Accepted    = 0x%x\n"),
		 service_status.controls_accepted);
	d_printf(_("\tService Type         = 0x%x\n"), config.service_type);
	d_printf(_("\tStart Type           = 0x%x\n"), config.start_type);
	d_printf(_("\tError Control        = 0x%x\n"), config.error_control);
	d_printf(_("\tTag ID               = 0x%x\n"), config.tag_id);

	if (config.executablepath) {
		d_printf(_("\tExecutable Path      = %s\n"),
			 config.executablepath);
	}

	if (config.loadordergroup) {
		d_printf(_("\tLoad Order Group     = %s\n"),
			 config.loadordergroup);
	}

	if (config.dependencies) {
		d_printf(_("\tDependencies         = %s\n"),
			 config.dependencies);
	}

	if (config.startname) {
		d_printf(_("\tStart Name           = %s\n"), config.startname);
	}

	if (config.displayname) {
		d_printf(_("\tDisplay Name         = %s\n"),
			 config.displayname);
	}

done:
	if (is_valid_policy_hnd(&hService)) {
		WERROR _result;
	        dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hService, &_result);
	}
	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_stop_internal(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle hSCM;
	WERROR result = WERR_GENERAL_FAILURE;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1 ) {
		d_printf("%s net rpc service status <service>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	result = control_service(pipe_hnd, mem_ctx, &hSCM, argv[0],
		SVCCTL_CONTROL_STOP, SVCCTL_STOPPED );

	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_pause_internal(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle hSCM;
	WERROR result = WERR_GENERAL_FAILURE;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1 ) {
		d_printf("%s net rpc service status <service>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	result = control_service(pipe_hnd, mem_ctx, &hSCM, argv[0],
		SVCCTL_CONTROL_PAUSE, SVCCTL_PAUSED );

	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_resume_internal(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle hSCM;
	WERROR result = WERR_GENERAL_FAILURE;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1 ) {
		d_printf("%s net rpc service status <service>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	result = control_service(pipe_hnd, mem_ctx, &hSCM, argv[0],
		SVCCTL_CONTROL_CONTINUE, SVCCTL_RUNNING );

	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_start_internal(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle hSCM, hService;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	uint32 state = 0;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1 ) {
		d_printf("%s net rpc service status <service>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}


	/* Open the Service */

	result = open_service(b, mem_ctx, &hSCM, argv[0],
			      SC_RIGHT_SVC_START,
			      &hService);
	if (!W_ERROR_IS_OK(result) ) {
		goto done;
	}

	/* get the status */

	status = dcerpc_svcctl_StartServiceW(b, mem_ctx,
					     &hService,
					     0,
					     NULL,
					     &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Query status request failed.  [%s]\n"),
			  nt_errstr(status));
		goto done;
	}
	if (!W_ERROR_IS_OK(result) ) {
		d_fprintf(stderr, _("Query status request failed.  [%s]\n"),
			  win_errstr(result));
		goto done;
	}

	result = watch_service_state(pipe_hnd, mem_ctx, &hSCM, argv[0], SVCCTL_RUNNING, &state  );

	if ( W_ERROR_IS_OK(result) && (state == SVCCTL_RUNNING) )
		d_printf(_("Successfully started service: %s\n"),
			 argv[0] );
	else
		d_fprintf(stderr,_("Failed to start service: %s [%s]\n"),
			  argv[0], win_errstr(result) );

done:
	if (is_valid_policy_hnd(&hService)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hService, &_result);
	}
	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_delete_internal(struct net_context *c,
					    const struct dom_sid *domain_sid,
					    const char *domain_name,
					    struct cli_state *cli,
					    struct rpc_pipe_client *pipe_hnd,
					    TALLOC_CTX *mem_ctx,
					    int argc,
					    const char **argv)
{
	struct policy_handle hSCM, hService;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 1 ) {
		d_printf("%s net rpc service delete <service>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	ZERO_STRUCT(hSCM);
	ZERO_STRUCT(hService);

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_ENUMERATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	/* Open the Service */

	result = open_service(b, mem_ctx, &hSCM, argv[0],
			      SERVICE_ALL_ACCESS,
			      &hService);
	if (!W_ERROR_IS_OK(result) ) {
		goto done;
	}

	/* Delete the Service */

	status = dcerpc_svcctl_DeleteService(b, mem_ctx,
					     &hService,
					     &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Delete service request failed.  [%s]\n"),
			nt_errstr(status));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		d_fprintf(stderr, _("Delete service request failed.  [%s]\n"),
			win_errstr(result));
		goto done;
	}

	d_printf(_("Successfully deleted Service: %s\n"), argv[0]);

 done:
	if (is_valid_policy_hnd(&hService)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hService, &_result);
	}
	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_service_create_internal(struct net_context *c,
					    const struct dom_sid *domain_sid,
					    const char *domain_name,
					    struct cli_state *cli,
					    struct rpc_pipe_client *pipe_hnd,
					    TALLOC_CTX *mem_ctx,
					    int argc,
					    const char **argv)
{
	struct policy_handle hSCM, hService;
	WERROR result = WERR_GENERAL_FAILURE;
	NTSTATUS status;
	const char *ServiceName;
	const char *DisplayName;
	const char *binary_path;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc != 3) {
		d_printf("%s net rpc service create <service> "
			 "<displayname> <binarypath>\n", _("Usage:"));
		return NT_STATUS_OK;
	}

	ZERO_STRUCT(hSCM);
	ZERO_STRUCT(hService);

	/* Open the Service Control Manager */
	result = open_scm(b, mem_ctx, pipe_hnd->srv_name_slash,
			  SC_RIGHT_MGR_CREATE_SERVICE,
			  &hSCM);
	if (!W_ERROR_IS_OK(result)) {
		return werror_to_ntstatus(result);
	}

	/* Create the service */

	ServiceName = argv[0];
	DisplayName = argv[1];
	binary_path = argv[2];

	status = dcerpc_svcctl_CreateServiceW(b, mem_ctx,
					      &hSCM,
					      ServiceName,
					      DisplayName,
					      SERVICE_ALL_ACCESS,
					      SERVICE_TYPE_WIN32_OWN_PROCESS,
					      SVCCTL_DEMAND_START,
					      SVCCTL_SVC_ERROR_NORMAL,
					      binary_path,
					      NULL, /* LoadOrderGroupKey */
					      NULL, /* TagId */
					      NULL, /* dependencies */
					      0, /* dependencies_size */
					      NULL, /* service_start_name */
					      NULL, /* password */
					      0, /* password_size */
					      &hService,
					      &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		d_fprintf(stderr, _("Create service request failed.  [%s]\n"),
			  nt_errstr(status));
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		d_fprintf(stderr, _("Create service request failed.  [%s]\n"),
			  win_errstr(result));
		goto done;
	}

	d_printf(_("Successfully created Service: %s\n"), argv[0]);

 done:
	if (is_valid_policy_hnd(&hService)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hService, &_result);
	}
	if (is_valid_policy_hnd(&hSCM)) {
		WERROR _result;
		dcerpc_svcctl_CloseServiceHandle(b, mem_ctx, &hSCM, &_result);
	}

	return werror_to_ntstatus(result);
}

/********************************************************************
********************************************************************/

static int rpc_service_list(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("View configured Win32 services"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_list_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_service_start(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service start <service>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Start a Win32 service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_start_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_service_stop(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service stop <service>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Stop a Win32 service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_stop_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_service_resume(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service resume <service>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Resume a Win32 service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_resume_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_service_pause(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service pause <service>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Pause a Win32 service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_pause_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_service_status(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service status <service>\n"
			   "     %s\n",
			 _("Usage:"),
			 _("Show the current status of a service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_status_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_service_delete(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service delete <service>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Delete a Win32 service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_delete_internal, argc, argv);
}

/********************************************************************
********************************************************************/

static int rpc_service_create(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc service create <service>\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Create a Win32 service"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_svcctl.syntax_id, 0,
		rpc_service_create_internal, argc, argv);
}

/********************************************************************
********************************************************************/

int net_rpc_service(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"list",
			rpc_service_list,
			NET_TRANSPORT_RPC,
			N_("View configured Win32 services"),
			N_("net rpc service list\n"
			   "    View configured Win32 services")
		},
		{
			"start",
			rpc_service_start,
			NET_TRANSPORT_RPC,
			N_("Start a service"),
			N_("net rpc service start\n"
			   "    Start a service")
		},
		{
			"stop",
			rpc_service_stop,
			NET_TRANSPORT_RPC,
			N_("Stop a service"),
			N_("net rpc service stop\n"
			   "    Stop a service")
		},
		{
			"pause",
			rpc_service_pause,
			NET_TRANSPORT_RPC,
			N_("Pause a service"),
			N_("net rpc service pause\n"
			   "    Pause a service")
		},
		{
			"resume",
			rpc_service_resume,
			NET_TRANSPORT_RPC,
			N_("Resume a paused service"),
			N_("net rpc service resume\n"
			   "    Resume a service")
		},
		{
			"status",
			rpc_service_status,
			NET_TRANSPORT_RPC,
			N_("View current status of a service"),
			N_("net rpc service status\n"
			   "    View current status of a service")
		},
		{
			"delete",
			rpc_service_delete,
			NET_TRANSPORT_RPC,
			N_("Delete a service"),
			N_("net rpc service delete\n"
			   "    Deletes a service")
		},
		{
			"create",
			rpc_service_create,
			NET_TRANSPORT_RPC,
			N_("Create a service"),
			N_("net rpc service create\n"
			   "    Creates a service")
		},

		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc service",func);
}
