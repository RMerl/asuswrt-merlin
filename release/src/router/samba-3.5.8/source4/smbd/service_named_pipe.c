/*
   Unix SMB/CIFS implementation.

   helper functions for NAMED PIPE servers

   Copyright (C) Stefan (metze) Metzmacher	2008

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
#include <tevent.h>
#include "smbd/service.h"
#include "param/param.h"
#include "auth/session.h"
#include "auth/auth_sam_reply.h"
#include "lib/stream/packet.h"
#include "librpc/gen_ndr/ndr_named_pipe_auth.h"
#include "system/passwd.h"
#include "libcli/raw/smb.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"

struct named_pipe_socket {
	const char *pipe_name;
	const char *pipe_path;
	const struct stream_server_ops *ops;
	void *private_data;
};

struct named_pipe_connection {
	struct stream_connection *connection;
	struct packet_context *packet;
	const struct named_pipe_socket *pipe_sock;
	NTSTATUS status;
};

static void named_pipe_handover_connection(void *private_data)
{
	struct named_pipe_connection *pipe_conn = talloc_get_type(
		private_data, struct named_pipe_connection);
	struct stream_connection *conn = pipe_conn->connection;

	TEVENT_FD_NOT_WRITEABLE(conn->event.fde);

	packet_set_socket(pipe_conn->packet, NULL);
	packet_set_event_context(pipe_conn->packet, NULL);
	packet_set_fde(pipe_conn->packet, NULL);
	TALLOC_FREE(pipe_conn->packet);

	if (!NT_STATUS_IS_OK(pipe_conn->status)) {
		stream_terminate_connection(conn, nt_errstr(pipe_conn->status));
		return;
	}

	/*
	 * remove the named_pipe layer together with its packet layer
	 */
	conn->ops		= pipe_conn->pipe_sock->ops;
	conn->private_data	= pipe_conn->pipe_sock->private_data;
	talloc_unlink(conn, pipe_conn);

	/* we're now ready to start receiving events on this stream */
	TEVENT_FD_READABLE(conn->event.fde);

	/*
	 * hand over to the real pipe implementation,
	 * now that we have setup the transport session_info
	 */
	conn->ops->accept_connection(conn);

	DEBUG(10,("named_pipe_handover_connection[%s]: succeeded\n",
	      conn->ops->name));
}

static NTSTATUS named_pipe_recv_auth_request(void *private_data,
					     DATA_BLOB req_blob)
{
	struct named_pipe_connection *pipe_conn = talloc_get_type(
		private_data, struct named_pipe_connection);
	struct stream_connection *conn = pipe_conn->connection;
	enum ndr_err_code ndr_err;
	struct named_pipe_auth_req req;
	union netr_Validation val;
	struct auth_serversupplied_info *server_info;
	struct named_pipe_auth_rep rep;
	DATA_BLOB rep_blob;
	NTSTATUS status;

	/*
	 * make sure nothing happens on the socket untill the
	 * real implemenation takes over
	 */
	packet_recv_disable(pipe_conn->packet);

	/*
	 * TODO: check it's a root (uid == 0) pipe
	 */

	ZERO_STRUCT(rep);
	rep.level = 0;
	rep.status = NT_STATUS_INTERNAL_ERROR;

	DEBUG(10,("named_pipe_auth: req_blob.length[%u]\n",
		  (unsigned int)req_blob.length));
	dump_data(11, req_blob.data, req_blob.length);

	/* parse the passed credentials */
	ndr_err = ndr_pull_struct_blob_all(
			&req_blob,
			pipe_conn,
			lp_iconv_convenience(conn->lp_ctx),
			&req,
			(ndr_pull_flags_fn_t)ndr_pull_named_pipe_auth_req);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		rep.status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(2, ("Could not unmarshall named_pipe_auth_req: %s\n",
			  nt_errstr(rep.status)));
		goto reply;
	}

	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(named_pipe_auth_req, &req);
	}

	if (strcmp(NAMED_PIPE_AUTH_MAGIC, req.magic) != 0) {
		DEBUG(2, ("named_pipe_auth_req: invalid magic '%s' != %s\n",
			  req.magic, NAMED_PIPE_AUTH_MAGIC));
		rep.status = NT_STATUS_INVALID_PARAMETER;
		goto reply;
	}

	switch (req.level) {
	case 0:
		/*
		 * anon connection, we don't create a session info
		 * and leave it NULL
		 */
		rep.level = 0;
		rep.status = NT_STATUS_OK;
		break;
	case 1:
		val.sam3 = &req.info.info1;

		rep.level = 1;
		rep.status = make_server_info_netlogon_validation(pipe_conn,
								  "TODO",
								  3, &val,
								  &server_info);
		if (!NT_STATUS_IS_OK(rep.status)) {
			DEBUG(2, ("make_server_info_netlogon_validation returned "
				  "%s\n", nt_errstr(rep.status)));
			goto reply;
		}

		/* setup the session_info on the connection */
		rep.status = auth_generate_session_info(conn,
							conn->event.ctx,
							conn->lp_ctx,
							server_info,
							&conn->session_info);
		if (!NT_STATUS_IS_OK(rep.status)) {
			DEBUG(2, ("auth_generate_session_info failed: %s\n",
				  nt_errstr(rep.status)));
			goto reply;
		}

		break;
	case 2:
		rep.level = 2;
		rep.info.info2.file_type = FILE_TYPE_MESSAGE_MODE_PIPE;
		rep.info.info2.device_state = 0xff | 0x0400 | 0x0100;
		rep.info.info2.allocation_size = 4096;

		if (!req.info.info2.sam_info3) {
			/*
			 * anon connection, we don't create a session info
			 * and leave it NULL
			 */
			rep.status = NT_STATUS_OK;
			break;
		}

		val.sam3 = req.info.info2.sam_info3;

		rep.status = make_server_info_netlogon_validation(pipe_conn,
						val.sam3->base.account_name.string,
						3, &val, &server_info);
		if (!NT_STATUS_IS_OK(rep.status)) {
			DEBUG(2, ("make_server_info_netlogon_validation returned "
				  "%s\n", nt_errstr(rep.status)));
			goto reply;
		}

		/* setup the session_info on the connection */
		rep.status = auth_generate_session_info(conn,
							conn->event.ctx,
							conn->lp_ctx,
							server_info,
							&conn->session_info);
		if (!NT_STATUS_IS_OK(rep.status)) {
			DEBUG(2, ("auth_generate_session_info failed: %s\n",
				  nt_errstr(rep.status)));
			goto reply;
		}

		conn->session_info->session_key = data_blob_const(req.info.info2.session_key,
							req.info.info2.session_key_length);
		talloc_steal(conn->session_info, req.info.info2.session_key);

		break;
	case 3:
		rep.level = 3;
		rep.info.info3.file_type = FILE_TYPE_MESSAGE_MODE_PIPE;
		rep.info.info3.device_state = 0xff | 0x0400 | 0x0100;
		rep.info.info3.allocation_size = 4096;

		if (!req.info.info3.sam_info3) {
			/*
			 * anon connection, we don't create a session info
			 * and leave it NULL
			 */
			rep.status = NT_STATUS_OK;
			break;
		}

		val.sam3 = req.info.info3.sam_info3;

		rep.status = make_server_info_netlogon_validation(pipe_conn,
						val.sam3->base.account_name.string,
						3, &val, &server_info);
		if (!NT_STATUS_IS_OK(rep.status)) {
			DEBUG(2, ("make_server_info_netlogon_validation returned "
				  "%s\n", nt_errstr(rep.status)));
			goto reply;
		}

		/* setup the session_info on the connection */
		rep.status = auth_generate_session_info(conn,
							conn->event.ctx,
							conn->lp_ctx,
							server_info,
							&conn->session_info);
		if (!NT_STATUS_IS_OK(rep.status)) {
			DEBUG(2, ("auth_generate_session_info failed: %s\n",
				  nt_errstr(rep.status)));
			goto reply;
		}

		if (req.info.info3.gssapi_delegated_creds_length) {
			OM_uint32 minor_status;
			gss_buffer_desc cred_token;
			gss_cred_id_t cred_handle;
			int ret;

			DEBUG(10, ("named_pipe_auth: delegated credentials supplied by client\n"));

			cred_token.value = req.info.info3.gssapi_delegated_creds;
			cred_token.length = req.info.info3.gssapi_delegated_creds_length;

			ret = gss_import_cred(&minor_status,
					       &cred_token,
					       &cred_handle);
			if (ret != GSS_S_COMPLETE) {
				rep.status = NT_STATUS_INTERNAL_ERROR;
				goto reply;
			}

			conn->session_info->credentials = cli_credentials_init(conn->session_info);
			if (!conn->session_info->credentials) {
				rep.status = NT_STATUS_NO_MEMORY;
				goto reply;
			}

			cli_credentials_set_conf(conn->session_info->credentials,
						 conn->lp_ctx);
			/* Just so we don't segfault trying to get at a username */
			cli_credentials_set_anonymous(conn->session_info->credentials);

			ret = cli_credentials_set_client_gss_creds(conn->session_info->credentials,
								   conn->event.ctx,
								   conn->lp_ctx,
								   cred_handle,
								   CRED_SPECIFIED);
			if (ret) {
				rep.status = NT_STATUS_INTERNAL_ERROR;
				goto reply;
			}

			/* This credential handle isn't useful for password authentication, so ensure nobody tries to do that */
			cli_credentials_set_kerberos_state(conn->session_info->credentials,
							   CRED_MUST_USE_KERBEROS);
		}

		conn->session_info->session_key = data_blob_const(req.info.info3.session_key,
							req.info.info3.session_key_length);
		talloc_steal(conn->session_info, req.info.info3.session_key);

		break;
	default:
		DEBUG(2, ("named_pipe_auth_req: unknown level %u\n",
			  req.level));
		rep.level = 0;
		rep.status = NT_STATUS_INVALID_LEVEL;
		goto reply;
	}

reply:
	/* create the output */
	ndr_err = ndr_push_struct_blob(&rep_blob, pipe_conn,
			lp_iconv_convenience(conn->lp_ctx),
			&rep,
			(ndr_push_flags_fn_t)ndr_push_named_pipe_auth_rep);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(2, ("Could not marshall named_pipe_auth_rep: %s\n",
			  nt_errstr(status)));
		return status;
	}

	DEBUG(10,("named_pipe_auth reply[%u]\n", (unsigned)rep_blob.length));
	dump_data(11, rep_blob.data, rep_blob.length);
	if (DEBUGLVL(10)) {
		NDR_PRINT_DEBUG(named_pipe_auth_rep, &rep);
	}

	pipe_conn->status = rep.status;
	status = packet_send_callback(pipe_conn->packet, rep_blob,
				      named_pipe_handover_connection,
				      pipe_conn);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("packet_send_callback returned %s\n",
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

/*
  called when a pipe socket becomes readable
*/
static void named_pipe_recv(struct stream_connection *conn, uint16_t flags)
{
	struct named_pipe_connection *pipe_conn = talloc_get_type(
		conn->private_data, struct named_pipe_connection);

	DEBUG(10,("named_pipe_recv\n"));

	packet_recv(pipe_conn->packet);
}

/*
  called when a pipe socket becomes writable
*/
static void named_pipe_send(struct stream_connection *conn, uint16_t flags)
{
	struct named_pipe_connection *pipe_conn = talloc_get_type(
		conn->private_data, struct named_pipe_connection);

	packet_queue_run(pipe_conn->packet);
}

/*
  handle socket recv errors
*/
static void named_pipe_recv_error(void *private_data, NTSTATUS status)
{
	struct named_pipe_connection *pipe_conn = talloc_get_type(
		private_data, struct named_pipe_connection);

	stream_terminate_connection(pipe_conn->connection, nt_errstr(status));
}

static NTSTATUS named_pipe_full_request(void *private_data, DATA_BLOB blob, size_t *size)
{
	if (blob.length < 8) {
		return STATUS_MORE_ENTRIES;
	}

	if (memcmp(NAMED_PIPE_AUTH_MAGIC, &blob.data[4], 4) != 0) {
		DEBUG(0,("named_pipe_full_request: wrong protocol\n"));
		*size = blob.length;
		/* the error will be handled in named_pipe_recv_auth_request */
		return NT_STATUS_OK;
	}

	*size = 4 + RIVAL(blob.data, 0);
	if (*size > blob.length) {
		return STATUS_MORE_ENTRIES;
	}

	return NT_STATUS_OK;
}

static void named_pipe_accept(struct stream_connection *conn)
{
	struct named_pipe_socket *pipe_sock = talloc_get_type(
		conn->private_data, struct named_pipe_socket);
	struct named_pipe_connection *pipe_conn;

	DEBUG(5,("named_pipe_accept\n"));

	pipe_conn = talloc_zero(conn, struct named_pipe_connection);
	if (!pipe_conn) {
		stream_terminate_connection(conn, "out of memory");
		return;
	}

	pipe_conn->packet = packet_init(pipe_conn);
	if (!pipe_conn->packet) {
		stream_terminate_connection(conn, "out of memory");
		return;
	}
	packet_set_private(pipe_conn->packet, pipe_conn);
	packet_set_socket(pipe_conn->packet, conn->socket);
	packet_set_callback(pipe_conn->packet, named_pipe_recv_auth_request);
	packet_set_full_request(pipe_conn->packet, named_pipe_full_request);
	packet_set_error_handler(pipe_conn->packet, named_pipe_recv_error);
	packet_set_event_context(pipe_conn->packet, conn->event.ctx);
	packet_set_fde(pipe_conn->packet, conn->event.fde);
	packet_set_serialise(pipe_conn->packet);
	packet_set_initial_read(pipe_conn->packet, 8);

	pipe_conn->pipe_sock = pipe_sock;

	pipe_conn->connection = conn;
	conn->private_data = pipe_conn;
}

static const struct stream_server_ops named_pipe_stream_ops = {
	.name			= "named_pipe",
	.accept_connection	= named_pipe_accept,
	.recv_handler		= named_pipe_recv,
	.send_handler		= named_pipe_send,
};

NTSTATUS stream_setup_named_pipe(struct tevent_context *event_context,
				 struct loadparm_context *lp_ctx,
				 const struct model_ops *model_ops,
				 const struct stream_server_ops *stream_ops,
				 const char *pipe_name,
				 void *private_data)
{
	char *dirname;
	struct named_pipe_socket *pipe_sock;
	NTSTATUS status = NT_STATUS_NO_MEMORY;;

	pipe_sock = talloc(event_context, struct named_pipe_socket);
	if (pipe_sock == NULL) {
		goto fail;
	}

	/* remember the details about the pipe */
	pipe_sock->pipe_name	= talloc_strdup(pipe_sock, pipe_name);
	if (pipe_sock->pipe_name == NULL) {
		goto fail;
	}

	dirname = talloc_asprintf(pipe_sock, "%s/np", lp_ncalrpc_dir(lp_ctx));
	if (dirname == NULL) {
		goto fail;
	}

	if (!directory_create_or_exist(dirname, geteuid(), 0700)) {
		status = map_nt_error_from_unix(errno);
		DEBUG(0,(__location__ ": Failed to create stream pipe directory %s - %s\n",
			 dirname, nt_errstr(status)));
		goto fail;
	}

	if (strncmp(pipe_name, "\\pipe\\", 6) == 0) {
		pipe_name += 6;
	}

	pipe_sock->pipe_path = talloc_asprintf(pipe_sock, "%s/%s", dirname,
					       pipe_name);
	if (pipe_sock->pipe_path == NULL) {
		goto fail;
	}

	talloc_free(dirname);

	pipe_sock->ops		= stream_ops;
	pipe_sock->private_data	= talloc_reference(pipe_sock, private_data);

	status = stream_setup_socket(event_context,
				     lp_ctx,
				     model_ops,
				     &named_pipe_stream_ops,
				     "unix",
				     pipe_sock->pipe_path,
				     NULL,
				     NULL,
				     pipe_sock);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	return NT_STATUS_OK;

 fail:
	talloc_free(pipe_sock);
	return status;
}
