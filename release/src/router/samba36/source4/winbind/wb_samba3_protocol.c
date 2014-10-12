/* 
   Unix SMB/CIFS implementation.
   Main winbindd samba3 server routines

   Copyright (C) Stefan Metzmacher	2005
   Copyright (C) Volker Lendecke	2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "winbind/wb_server.h"
#include "smbd/service_stream.h"
#include "lib/stream/packet.h"
#include "lib/tsocket/tsocket.h"

/*
  work out if a packet is complete for protocols that use a 32 bit host byte
  order length
*/
NTSTATUS wbsrv_samba3_packet_full_request(void *private_data, DATA_BLOB blob, size_t *size)
{
	uint32_t *len;
	if (blob.length < 4) {
		return STATUS_MORE_ENTRIES;
	}
	len = (uint32_t *)blob.data;
	*size = (*len);
	if (*size > blob.length) {
		return STATUS_MORE_ENTRIES;
	}
	return NT_STATUS_OK;
}


NTSTATUS wbsrv_samba3_pull_request(struct wbsrv_samba3_call *call)
{
	if (call->in.length != sizeof(*call->request)) {
		DEBUG(0,("wbsrv_samba3_pull_request: invalid blob length %lu should be %lu\n"
			 " make sure you use the correct winbind client tools!\n",
			 (long)call->in.length, (long)sizeof(*call->request)));
		return NT_STATUS_INVALID_PARAMETER;
	}

	call->request = talloc_zero(call, struct winbindd_request);
	NT_STATUS_HAVE_NO_MEMORY(call->request);

	/* the packet layout is the same as the in memory layout of the request, so just copy it */
	memcpy(call->request, call->in.data, sizeof(*call->request));

	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_handle_call(struct wbsrv_samba3_call *s3call)
{
	DEBUG(10, ("Got winbind samba3 request %d\n", s3call->request->cmd));

	s3call->response = talloc_zero(s3call, struct winbindd_response);
	NT_STATUS_HAVE_NO_MEMORY(s3call->request);

	s3call->response->length = sizeof(*s3call->response);

	switch(s3call->request->cmd) {
	case WINBINDD_INTERFACE_VERSION:
		return wbsrv_samba3_interface_version(s3call);

	case WINBINDD_CHECK_MACHACC:
		return wbsrv_samba3_check_machacc(s3call);

	case WINBINDD_PING:
		return wbsrv_samba3_ping(s3call);

	case WINBINDD_INFO:
		return wbsrv_samba3_info(s3call);

	case WINBINDD_DOMAIN_NAME:
		return wbsrv_samba3_domain_name(s3call);

	case WINBINDD_NETBIOS_NAME:
		return wbsrv_samba3_netbios_name(s3call);

	case WINBINDD_PRIV_PIPE_DIR:
		return wbsrv_samba3_priv_pipe_dir(s3call);

	case WINBINDD_LOOKUPNAME:
		return wbsrv_samba3_lookupname(s3call);

	case WINBINDD_LOOKUPSID:
		return wbsrv_samba3_lookupsid(s3call);

	case WINBINDD_PAM_AUTH:
		return wbsrv_samba3_pam_auth(s3call);

	case WINBINDD_PAM_AUTH_CRAP:
		return wbsrv_samba3_pam_auth_crap(s3call);

	case WINBINDD_GETDCNAME:
		return wbsrv_samba3_getdcname(s3call);

	case WINBINDD_GETUSERDOMGROUPS:
		return wbsrv_samba3_userdomgroups(s3call);

	case WINBINDD_GETUSERSIDS:
		return wbsrv_samba3_usersids(s3call);

	case WINBINDD_LIST_GROUPS:
		return wbsrv_samba3_list_groups(s3call);

	case WINBINDD_LIST_TRUSTDOM:
		return wbsrv_samba3_list_trustdom(s3call);

	case WINBINDD_LIST_USERS:
		return wbsrv_samba3_list_users(s3call);

	case WINBINDD_GETPWNAM:
		return wbsrv_samba3_getpwnam(s3call);

	case WINBINDD_GETPWUID:
		return wbsrv_samba3_getpwuid(s3call);

	case WINBINDD_SETPWENT:
		return wbsrv_samba3_setpwent(s3call);

	case WINBINDD_GETPWENT:
		return wbsrv_samba3_getpwent(s3call);

	case WINBINDD_ENDPWENT:
		return wbsrv_samba3_endpwent(s3call);

	case WINBINDD_GETGRNAM:
		return wbsrv_samba3_getgrnam(s3call);

	case WINBINDD_GETGRGID:
		return wbsrv_samba3_getgrgid(s3call);

	case WINBINDD_GETGROUPS:
		return wbsrv_samba3_getgroups(s3call);

	case WINBINDD_SETGRENT:
		return wbsrv_samba3_setgrent(s3call);

	case WINBINDD_GETGRENT:
		return wbsrv_samba3_getgrent(s3call);

	case WINBINDD_ENDGRENT:
		return wbsrv_samba3_endgrent(s3call);

	case WINBINDD_SID_TO_UID:
	case WINBINDD_DUAL_SID2UID:
		return wbsrv_samba3_sid2uid(s3call);

	case WINBINDD_SID_TO_GID:
	case WINBINDD_DUAL_SID2GID:
		return wbsrv_samba3_sid2gid(s3call);

	case WINBINDD_UID_TO_SID:
	case WINBINDD_DUAL_UID2SID:
		return wbsrv_samba3_uid2sid(s3call);

	case WINBINDD_GID_TO_SID:
	case WINBINDD_DUAL_GID2SID:
		return wbsrv_samba3_gid2sid(s3call);

	case WINBINDD_DOMAIN_INFO:
		return wbsrv_samba3_domain_info(s3call);

	case WINBINDD_PAM_LOGOFF:
		return wbsrv_samba3_pam_logoff(s3call);

	/* Unimplemented commands */
	case WINBINDD_GETPWSID:
	case WINBINDD_PAM_CHAUTHTOK:
	case WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP:
	case WINBINDD_LOOKUPRIDS:
	case WINBINDD_SIDS_TO_XIDS:
	case WINBINDD_ALLOCATE_UID:
	case WINBINDD_ALLOCATE_GID:
	case WINBINDD_SHOW_SEQUENCE:
	case WINBINDD_WINS_BYIP:
	case WINBINDD_WINS_BYNAME:
	case WINBINDD_GETGRLST:
	case WINBINDD_GETSIDALIASES:
	case WINBINDD_DSGETDCNAME:
	case WINBINDD_INIT_CONNECTION:
	case WINBINDD_DUAL_SIDS2XIDS:
	case WINBINDD_DUAL_USERINFO:
	case WINBINDD_DUAL_GETSIDALIASES:
	case WINBINDD_DUAL_NDRCMD:
	case WINBINDD_CCACHE_NTLMAUTH:
	case WINBINDD_NUM_CMDS:
		DEBUG(10, ("Unimplemented winbind samba3 request %d\n", 
			   s3call->request->cmd));
		break;
	}

	s3call->response->result = WINBINDD_ERROR;
	return NT_STATUS_OK;
}

static NTSTATUS wbsrv_samba3_push_reply(struct wbsrv_samba3_call *call)
{
	uint8_t *extra_data;
	size_t extra_data_len = 0;

	extra_data = (uint8_t *)call->response->extra_data.data;
	if (extra_data != NULL) {
		extra_data_len = call->response->length -
			sizeof(*call->response);
	}

	call->out = data_blob_talloc(call, NULL, call->response->length);
	NT_STATUS_HAVE_NO_MEMORY(call->out.data);

	/* don't push real pointer values into sockets */
	if (extra_data) {
		call->response->extra_data.data = (void *)0xFFFFFFFF;
	}

	memcpy(call->out.data, call->response, sizeof(*call->response));
	/* set back the pointer */
	call->response->extra_data.data = extra_data;

	if (extra_data) {
		memcpy(call->out.data + sizeof(*call->response),
		       extra_data,
		       extra_data_len);
	}

	return NT_STATUS_OK;
}

static void wbsrv_samba3_send_reply_done(struct tevent_req *subreq);

/*
 * queue a wbsrv_call reply on a wbsrv_connection
 * NOTE: that this implies talloc_free(call),
 *       use talloc_reference(call) if you need it after
 *       calling wbsrv_queue_reply
 */
NTSTATUS wbsrv_samba3_send_reply(struct wbsrv_samba3_call *call)
{
	struct wbsrv_connection *wbsrv_conn = call->wbconn;
	struct tevent_req *subreq;
	NTSTATUS status;

	status = wbsrv_samba3_push_reply(call);
	NT_STATUS_NOT_OK_RETURN(status);

	call->out_iov[0].iov_base = (char *) call->out.data;
	call->out_iov[0].iov_len = call->out.length;

	subreq = tstream_writev_queue_send(call,
					   wbsrv_conn->conn->event.ctx,
					   wbsrv_conn->tstream,
					   wbsrv_conn->send_queue,
					   call->out_iov, 1);
	if (subreq == NULL) {
		wbsrv_terminate_connection(wbsrv_conn, "wbsrv_call_loop: "
				"no memory for tstream_writev_queue_send");
		return NT_STATUS_NO_MEMORY;
	}
	tevent_req_set_callback(subreq, wbsrv_samba3_send_reply_done, call);

	return status;
}

static void wbsrv_samba3_send_reply_done(struct tevent_req *subreq)
{
	struct wbsrv_samba3_call *call = tevent_req_callback_data(subreq,
			struct wbsrv_samba3_call);
	int sys_errno;
	int rc;

	rc = tstream_writev_queue_recv(subreq, &sys_errno);
	TALLOC_FREE(subreq);
	if (rc == -1) {
		const char *reason;

		reason = talloc_asprintf(call, "wbsrv_samba3_send_reply_done: "
					 "tstream_writev_queue_recv() - %d:%s",
					 sys_errno, strerror(sys_errno));
		if (reason == NULL) {
			reason = "wbsrv_samba3_send_reply_done: "
				 "tstream_writev_queue_recv() failed";
		}

		wbsrv_terminate_connection(call->wbconn, reason);
		return;
	}

	talloc_free(call);
}

NTSTATUS wbsrv_samba3_process(struct wbsrv_samba3_call *call)
{
	NTSTATUS status;

	status = wbsrv_samba3_pull_request(call);
	
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = wbsrv_samba3_handle_call(call);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(call);
		return status;
	}

	if (call->flags & WBSRV_CALL_FLAGS_REPLY_ASYNC) {
		return NT_STATUS_OK;
	}

	status = wbsrv_samba3_send_reply(call);
	return status;
}

