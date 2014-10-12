/* 
   Samba Unix/Linux SMB client library 
   net ads cldap functions 
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2003 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2008 Guenther Deschner (gd@samba.org)
   Copyright (C) 2009 Stefan Metzmacher (metze@samba.org)

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
#include "../libcli/cldap/cldap.h"
#include "../lib/tsocket/tsocket.h"
#include "libads/cldap.h"

/*******************************************************************
  do a cldap netlogon query.  Always 389/udp
*******************************************************************/

bool ads_cldap_netlogon(TALLOC_CTX *mem_ctx,
			const char *server,
			const char *realm,
			uint32_t nt_version,
			struct netlogon_samlogon_response **_reply)
{
	struct cldap_socket *cldap;
	struct cldap_netlogon io;
	struct netlogon_samlogon_response *reply;
	NTSTATUS status;
	struct sockaddr_storage ss;
	char addrstr[INET6_ADDRSTRLEN];
	const char *dest_str;
	int ret;
	struct tsocket_address *dest_addr;

	if (!interpret_string_addr_prefer_ipv4(&ss, server, 0)) {
		DEBUG(2,("Failed to resolve[%s] into an address for cldap\n",
			server));
		return false;
	}
	dest_str = print_sockaddr(addrstr, sizeof(addrstr), &ss);

	ret = tsocket_address_inet_from_strings(mem_ctx, "ip",
						dest_str, LDAP_PORT,
						&dest_addr);
	if (ret != 0) {
		status = map_nt_error_from_unix(errno);
		DEBUG(2,("Failed to create cldap tsocket_address for %s - %s\n",
			 dest_str, nt_errstr(status)));
		return false;
	}

	/*
	 * as we use a connected udp socket
	 */
	status = cldap_socket_init(mem_ctx, NULL, NULL, dest_addr, &cldap);
	TALLOC_FREE(dest_addr);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("Failed to create cldap socket to %s: %s\n",
			 dest_str, nt_errstr(status)));
		return false;
	}

	reply = talloc(cldap, struct netlogon_samlogon_response);
	if (!reply) {
		goto failed;
	}

	/*
	 * as we use a connected socket, so we don't need to specify the
	 * destination
	 */
	io.in.dest_address	= NULL;
	io.in.dest_port		= 0;
	io.in.realm		= realm;
	io.in.host		= NULL;
	io.in.user		= NULL;
	io.in.domain_guid	= NULL;
	io.in.domain_sid	= NULL;
	io.in.acct_control	= 0;
	io.in.version		= nt_version;
	io.in.map_response	= false;

	status = cldap_netlogon(cldap, reply, &io);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("cldap_netlogon() failed: %s\n", nt_errstr(status)));
		goto failed;
	}

	*reply = io.out.netlogon;
	*_reply = talloc_move(mem_ctx, &reply);
	TALLOC_FREE(cldap);
	return true;
failed:
	TALLOC_FREE(cldap);
	return false;
}

/*******************************************************************
  do a cldap netlogon query.  Always 389/udp
*******************************************************************/

bool ads_cldap_netlogon_5(TALLOC_CTX *mem_ctx,
			  const char *server,
			  const char *realm,
			  struct NETLOGON_SAM_LOGON_RESPONSE_EX *reply5)
{
	uint32_t nt_version = NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;
	struct netlogon_samlogon_response *reply = NULL;
	bool ret;

	ret = ads_cldap_netlogon(mem_ctx, server, realm, nt_version, &reply);
	if (!ret) {
		return false;
	}

	if (reply->ntver != NETLOGON_NT_VERSION_5EX) {
		DEBUG(0,("ads_cldap_netlogon_5: nt_version mismatch: 0x%08x\n",
			reply->ntver));
		return false;
	}

	*reply5 = reply->data.nt5_ex;

	return true;
}
