/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Largely rewritten by Jeremy Allison		    2005.
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
#include "librpc/rpc/dcerpc.h"
#include "../librpc/gen_ndr/ndr_lsa.h"
#include "../librpc/gen_ndr/ndr_dssetup.h"
#include "../librpc/gen_ndr/ndr_samr.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../librpc/gen_ndr/ndr_srvsvc.h"
#include "../librpc/gen_ndr/ndr_wkssvc.h"
#include "../librpc/gen_ndr/ndr_winreg.h"
#include "../librpc/gen_ndr/ndr_spoolss.h"
#include "../librpc/gen_ndr/ndr_dfs.h"
#include "../librpc/gen_ndr/ndr_echo.h"
#include "../librpc/gen_ndr/ndr_initshutdown.h"
#include "../librpc/gen_ndr/ndr_svcctl.h"
#include "../librpc/gen_ndr/ndr_eventlog.h"
#include "../librpc/gen_ndr/ndr_ntsvcs.h"
#include "../librpc/gen_ndr/ndr_epmapper.h"
#include "../librpc/gen_ndr/ndr_drsuapi.h"

static const char *get_pipe_name_from_iface(
	TALLOC_CTX *mem_ctx, const struct ndr_interface_table *interface)
{
	int i;
	const struct ndr_interface_string_array *ep = interface->endpoints;
	char *p;

	for (i=0; i<ep->count; i++) {
		if (strncmp(ep->names[i], "ncacn_np:[\\pipe\\", 16) == 0) {
			break;
		}
	}
	if (i == ep->count) {
		return NULL;
	}

	/*
	 * extract the pipe name without \\pipe from for example
	 * ncacn_np:[\\pipe\\epmapper]
	 */
	p = strchr(ep->names[i]+15, ']');
	if (p == NULL) {
		return "PIPE";
	}
	return talloc_strndup(mem_ctx, ep->names[i]+15, p - ep->names[i] - 15);
}

static const struct ndr_interface_table **interfaces;

bool smb_register_ndr_interface(const struct ndr_interface_table *interface)
{
	int num_interfaces = talloc_array_length(interfaces);
	const struct ndr_interface_table **tmp;
	int i;

	for (i=0; i<num_interfaces; i++) {
		if (ndr_syntax_id_equal(&interfaces[i]->syntax_id,
					&interface->syntax_id)) {
			return true;
		}
	}

	tmp = talloc_realloc(NULL, interfaces,
			     const struct ndr_interface_table *,
			     num_interfaces + 1);
	if (tmp == NULL) {
		DEBUG(1, ("smb_register_ndr_interface: talloc failed\n"));
		return false;
	}
	interfaces = tmp;
	interfaces[num_interfaces] = interface;
	return true;
}

static bool initialize_interfaces(void)
{
#ifdef LSA_SUPPORT
	if (!smb_register_ndr_interface(&ndr_table_lsarpc)) {
		return false;
	}
#endif
#ifdef ACTIVE_DIRECTORY
	if (!smb_register_ndr_interface(&ndr_table_dssetup)) {
		return false;
	}
#endif
#ifdef SAMR_SUPPORT
	if (!smb_register_ndr_interface(&ndr_table_samr)) {
		return false;
	}
#endif
#ifdef NETLOGON_SUPPORT
	if (!smb_register_ndr_interface(&ndr_table_netlogon)) {
		return false;
	}
#endif
	if (!smb_register_ndr_interface(&ndr_table_srvsvc)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_wkssvc)) {
		return false;
	}
#ifdef WINREG_SUPPORT
	if (!smb_register_ndr_interface(&ndr_table_winreg)) {
		return false;
	}
#endif
#ifdef PRINTER_SUPPORT
	if (!smb_register_ndr_interface(&ndr_table_spoolss)) {
		return false;
	}
#endif
#ifdef DFS_SUPPORT
	if (!smb_register_ndr_interface(&ndr_table_netdfs)) {
		return false;
	}
#endif
#ifdef DEVELOPER
	if (!smb_register_ndr_interface(&ndr_table_rpcecho)) {
		return false;
	}
#endif
	if (!smb_register_ndr_interface(&ndr_table_initshutdown)) {
		return false;
	}
#ifdef EXTRA_SERVICES
	if (!smb_register_ndr_interface(&ndr_table_svcctl)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_eventlog)) {
		return false;
	}
	if (!smb_register_ndr_interface(&ndr_table_ntsvcs)) {
		return false;
	}
#endif
	if (!smb_register_ndr_interface(&ndr_table_epmapper)) {
		return false;
	}
#ifdef ACTIVE_DIRECTORY
	if (!smb_register_ndr_interface(&ndr_table_drsuapi)) {
		return false;
	}
#endif
	return true;
}

const struct ndr_interface_table *get_iface_from_syntax(
	const struct ndr_syntax_id *syntax)
{
	int num_interfaces;
	int i;

	if (interfaces == NULL) {
		if (!initialize_interfaces()) {
			return NULL;
		}
	}
	num_interfaces = talloc_array_length(interfaces);

	for (i=0; i<num_interfaces; i++) {
		if (ndr_syntax_id_equal(&interfaces[i]->syntax_id, syntax)) {
			return interfaces[i];
		}
	}

	return NULL;
}

/****************************************************************************
 Return the pipe name from the interface.
 ****************************************************************************/

const char *get_pipe_name_from_syntax(TALLOC_CTX *mem_ctx,
				      const struct ndr_syntax_id *syntax)
{
	const struct ndr_interface_table *interface;
	char *guid_str;
	const char *result;

	interface = get_iface_from_syntax(syntax);
	if (interface != NULL) {
		result = get_pipe_name_from_iface(mem_ctx, interface);
		if (result != NULL) {
			return result;
		}
	}

	/*
	 * Here we should ask \\epmapper, but for now our code is only
	 * interested in the known pipes mentioned in pipe_names[]
	 */

	guid_str = GUID_string(talloc_tos(), &syntax->uuid);
	if (guid_str == NULL) {
		return NULL;
	}
	result = talloc_asprintf(mem_ctx, "Interface %s.%d", guid_str,
				 (int)syntax->if_version);
	TALLOC_FREE(guid_str);

	if (result == NULL) {
		return "PIPE";
	}
	return result;
}

