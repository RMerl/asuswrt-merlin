/* 
   Unix SMB/CIFS implementation.

   find security related memory leaks

   Copyright (C) Andrew Tridgell 2004
   
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
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "system/time.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/smb_composite/proto.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"

static bool try_failed_login(struct torture_context *tctx, struct smbcli_state *cli)
{
	NTSTATUS status;
	struct smb_composite_sesssetup setup;
	struct smbcli_session *session;
	struct smbcli_session_options options;

	lp_smbcli_session_options(tctx->lp_ctx, &options);

	session = smbcli_session_init(cli->transport, cli, false, options);
	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.workgroup = lp_workgroup(tctx->lp_ctx);
	setup.in.credentials = cli_credentials_init(session);
	setup.in.gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);

	cli_credentials_set_conf(setup.in.credentials, tctx->lp_ctx);
	cli_credentials_set_domain(setup.in.credentials, "INVALID-DOMAIN", CRED_SPECIFIED);
	cli_credentials_set_username(setup.in.credentials, "INVALID-USERNAME", CRED_SPECIFIED);
	cli_credentials_set_password(setup.in.credentials, "INVALID-PASSWORD", CRED_SPECIFIED);

	status = smb_composite_sesssetup(session, &setup);
	talloc_free(session);
	if (NT_STATUS_IS_OK(status)) {
		printf("Allowed session setup with invalid credentials?!\n");
		return false;
	}

	return true;
}

bool torture_sec_leak(struct torture_context *tctx, struct smbcli_state *cli)
{
	time_t t1 = time(NULL);
	int timelimit = torture_setting_int(tctx, "timelimit", 20);

	while (time(NULL) < t1+timelimit) {
		if (!try_failed_login(tctx, cli)) {
			return false;
		}
		talloc_report(NULL, stdout);
	}

	return true;
}
