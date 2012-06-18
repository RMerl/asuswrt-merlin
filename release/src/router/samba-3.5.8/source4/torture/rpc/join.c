#include "includes.h"
#include "libnet/libnet.h"
#include "libcli/libcli.h"

#include "auth/credentials/credentials.h"
#include "torture/rpc/rpc.h"

#include "libcli/resolve/resolve.h"
#include "param/param.h"

#define TORTURE_NETBIOS_NAME "smbtorturejoin"


bool torture_rpc_join(struct torture_context *torture)
{
	NTSTATUS status;
	struct test_join *tj;
	struct cli_credentials *machine_account;
	struct smbcli_state *cli;
	const char *host = torture_setting_string(torture, "host", NULL);
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	/* Join domain as a member server. */
	tj = torture_join_domain(torture,
				 TORTURE_NETBIOS_NAME,
				 ACB_WSTRUST,
				 &machine_account);

	if (!tj) {
		DEBUG(0, ("%s failed to join domain as workstation\n",
			  TORTURE_NETBIOS_NAME));
		return false;
	}

	lp_smbcli_options(torture->lp_ctx, &options);
	lp_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(tj, &cli, host,
					lp_smb_ports(torture->lp_ctx),
					"IPC$", NULL,
					lp_socket_options(torture->lp_ctx),
					machine_account,
					lp_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lp_iconv_convenience(torture->lp_ctx),
					lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("%s failed to connect to IPC$ with workstation credentials\n",
			  TORTURE_NETBIOS_NAME));
		return false;	
	}
	smbcli_tdis(cli);
        
	/* Leave domain. */                          
	torture_leave_domain(torture, tj);
        
	/* Join domain as a domain controller. */
	tj = torture_join_domain(torture, TORTURE_NETBIOS_NAME,
				 ACB_SVRTRUST,
				 &machine_account);
	if (!tj) {
		DEBUG(0, ("%s failed to join domain as domain controller\n",
			  TORTURE_NETBIOS_NAME));
		return false;
	}

	status = smbcli_full_connection(tj, &cli, host,
					lp_smb_ports(torture->lp_ctx),
					"IPC$", NULL,
					lp_socket_options(torture->lp_ctx),
					machine_account,
					lp_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lp_iconv_convenience(torture->lp_ctx),
					lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("%s failed to connect to IPC$ with workstation credentials\n",
			  TORTURE_NETBIOS_NAME));
		return false;	
	}

	smbcli_tdis(cli);

	/* Leave domain. */
	torture_leave_domain(torture, tj);

	return true;
}

