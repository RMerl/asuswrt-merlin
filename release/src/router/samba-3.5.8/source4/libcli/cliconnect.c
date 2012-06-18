/*
   Unix SMB/CIFS implementation.

   client connect/disconnect routines

   Copyright (C) Andrew Tridgell 2003-2005
   Copyright (C) James Peach 2005

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
#include "libcli/libcli.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/smb_composite/smb_composite.h"

/*
  wrapper around smbcli_sock_connect()
*/
bool smbcli_socket_connect(struct smbcli_state *cli, const char *server, 
			   const char **ports, 
			   struct tevent_context *ev_ctx,
			   struct resolve_context *resolve_ctx,
			   struct smbcli_options *options,
			   struct smb_iconv_convenience *iconv_convenience,
               const char *socket_options)
{
	struct smbcli_socket *sock;

	sock = smbcli_sock_connect_byname(server, ports, NULL,
					  resolve_ctx, ev_ctx,
                      socket_options);

	if (sock == NULL) return false;
	
	cli->transport = smbcli_transport_init(sock, cli, true, options, 
										   iconv_convenience);
	if (!cli->transport) {
		return false;
	}

	return true;
}

/* wrapper around smbcli_transport_connect() */
bool smbcli_transport_establish(struct smbcli_state *cli, 
				struct nbt_name *calling,
				struct nbt_name *called)
{
	return smbcli_transport_connect(cli->transport, calling, called);
}

/* wrapper around smb_raw_negotiate() */
NTSTATUS smbcli_negprot(struct smbcli_state *cli, bool unicode, int maxprotocol)
{
	return smb_raw_negotiate(cli->transport, unicode, maxprotocol);
}

/* wrapper around smb_raw_sesssetup() */
NTSTATUS smbcli_session_setup(struct smbcli_state *cli, 
			      struct cli_credentials *credentials,
			      const char *workgroup,
			      struct smbcli_session_options options,
			      struct gensec_settings *gensec_settings)
{
	struct smb_composite_sesssetup setup;
	NTSTATUS status;

	cli->session = smbcli_session_init(cli->transport, cli, true,
					   options);
	if (!cli->session) return NT_STATUS_UNSUCCESSFUL;

	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.credentials = credentials;
	setup.in.workgroup = workgroup;
	setup.in.gensec_settings = gensec_settings;

	status = smb_composite_sesssetup(cli->session, &setup);

	cli->session->vuid = setup.out.vuid;

	return status;
}

/* wrapper around smb_raw_tcon() */
NTSTATUS smbcli_tconX(struct smbcli_state *cli, const char *sharename, 
		      const char *devtype, const char *password)
{
	union smb_tcon tcon;
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	cli->tree = smbcli_tree_init(cli->session, cli, true);
	if (!cli->tree) return NT_STATUS_UNSUCCESSFUL;

	mem_ctx = talloc_init("tcon");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	/* setup a tree connect */
	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	if (cli->transport->negotiate.sec_mode & NEGOTIATE_SECURITY_USER_LEVEL) {
		tcon.tconx.in.password = data_blob(NULL, 0);
	} else if (cli->transport->negotiate.sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) {
		tcon.tconx.in.password = data_blob_talloc(mem_ctx, NULL, 24);
		if (cli->transport->negotiate.secblob.length < 8) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		SMBencrypt(password, cli->transport->negotiate.secblob.data, tcon.tconx.in.password.data);
	} else {
		tcon.tconx.in.password = data_blob_talloc(mem_ctx, password, strlen(password)+1);
	}
	tcon.tconx.in.path = sharename;
	tcon.tconx.in.device = devtype;
	
	status = smb_raw_tcon(cli->tree, mem_ctx, &tcon);

	cli->tree->tid = tcon.tconx.out.tid;

	talloc_free(mem_ctx);

	return status;
}


/*
  easy way to get to a fully connected smbcli_state in one call
*/
NTSTATUS smbcli_full_connection(TALLOC_CTX *parent_ctx,
				struct smbcli_state **ret_cli, 
				const char *host,
				const char **ports,
				const char *sharename,
				const char *devtype,
				const char *socket_options,
				struct cli_credentials *credentials,
				struct resolve_context *resolve_ctx,
				struct tevent_context *ev,
				struct smbcli_options *options,
				struct smbcli_session_options *session_options,
				struct smb_iconv_convenience *iconv_convenience,
				struct gensec_settings *gensec_settings)
{
	struct smbcli_tree *tree;
	NTSTATUS status;

	*ret_cli = NULL;

	status = smbcli_tree_full_connection(parent_ctx,
					     &tree, host, ports, 
					     sharename, devtype,
						 socket_options,
					     credentials, resolve_ctx, ev,
					     options,
					     session_options,
						 iconv_convenience,
						 gensec_settings);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	(*ret_cli) = smbcli_state_init(parent_ctx);

	(*ret_cli)->tree = tree;
	(*ret_cli)->session = tree->session;
	(*ret_cli)->transport = tree->session->transport;

	talloc_steal(*ret_cli, tree);
	
done:
	return status;
}


/*
  disconnect the tree
*/
NTSTATUS smbcli_tdis(struct smbcli_state *cli)
{
	return smb_tree_disconnect(cli->tree);
}

/****************************************************************************
 Initialise a client state structure.
****************************************************************************/
struct smbcli_state *smbcli_state_init(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct smbcli_state);
}

/* Insert a NULL at the first separator of the given path and return a pointer
 * to the remainder of the string.
 */
static char *
terminate_path_at_separator(char * path)
{
	char * p;

	if (!path) {
		return NULL;
	}

	if ((p = strchr_m(path, '/'))) {
		*p = '\0';
		return p + 1;
	}

	if ((p = strchr_m(path, '\\'))) {
		*p = '\0';
		return p + 1;
	}
	
	/* No separator. */
	return NULL;
}

/*
  parse a //server/share type UNC name
*/
bool smbcli_parse_unc(const char *unc_name, TALLOC_CTX *mem_ctx,
		      char **hostname, char **sharename)
{
	char *p;

	*hostname = *sharename = NULL;

	if (strncmp(unc_name, "\\\\", 2) &&
	    strncmp(unc_name, "//", 2)) {
		return false;
	}

	*hostname = talloc_strdup(mem_ctx, &unc_name[2]);
	p = terminate_path_at_separator(*hostname);

	if (p != NULL && *p) {
		*sharename = talloc_strdup(mem_ctx, p);
		terminate_path_at_separator(*sharename);
	}

	if (*hostname && *sharename) {
		return true;
	}

	talloc_free(*hostname);
	talloc_free(*sharename);
	*hostname = *sharename = NULL;
	return false;
}



