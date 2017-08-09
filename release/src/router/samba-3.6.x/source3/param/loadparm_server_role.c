/*
   Unix SMB/CIFS implementation.
   Parameter loading functions
   Copyright (C) Karl Auer 1993-1998

   Largely re-written by Andrew Tridgell, September 1994

   Copyright (C) Simo Sorce 2001
   Copyright (C) Alexander Bokovoy 2002
   Copyright (C) Stefan (metze) Metzmacher 2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) Michael Adam 2008
   Copyright (C) Andrew Bartlett 2010

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

/*******************************************************************
 Set the server type we will announce as via nmbd.
********************************************************************/
static int server_role;

static const struct srv_role_tab {
	uint32 role;
	const char *role_str;
} srv_role_tab [] = {
	{ ROLE_STANDALONE, "ROLE_STANDALONE" },
	{ ROLE_DOMAIN_MEMBER, "ROLE_DOMAIN_MEMBER" },
	{ ROLE_DOMAIN_BDC, "ROLE_DOMAIN_BDC" },
	{ ROLE_DOMAIN_PDC, "ROLE_DOMAIN_PDC" },
	{ 0, NULL }
};

const char* server_role_str(uint32 role)
{
	int i = 0;
	for (i=0; srv_role_tab[i].role_str; i++) {
		if (role == srv_role_tab[i].role) {
			return srv_role_tab[i].role_str;
		}
	}
	return NULL;
}

void set_server_role(void)
{
	server_role = ROLE_STANDALONE;

	switch (lp_security()) {
		case SEC_SHARE:
			if (lp_domain_logons())
				DEBUG(0, ("Server's Role (logon server) conflicts with share-level security\n"));
			break;
		case SEC_SERVER:
			if (lp_domain_logons())
				DEBUG(0, ("Server's Role (logon server) conflicts with server-level security\n"));
			/* this used to be considered ROLE_DOMAIN_MEMBER but that's just wrong */
			server_role = ROLE_STANDALONE;
			break;
		case SEC_DOMAIN:
			if (lp_domain_logons()) {
				DEBUG(1, ("Server's Role (logon server) NOT ADVISED with domain-level security\n"));
				server_role = ROLE_DOMAIN_BDC;
				break;
			}
			server_role = ROLE_DOMAIN_MEMBER;
			break;
		case SEC_ADS:
			if (lp_domain_logons()) {
				server_role = ROLE_DOMAIN_PDC;
				break;
			}
			server_role = ROLE_DOMAIN_MEMBER;
			break;
		case SEC_USER:
			if (lp_domain_logons()) {

				if (lp_domain_master_true_or_auto()) /* auto or yes */
					server_role = ROLE_DOMAIN_PDC;
				else
					server_role = ROLE_DOMAIN_BDC;
			}
			break;
		default:
			DEBUG(0, ("Server's Role undefined due to unknown security mode\n"));
			break;
	}

	DEBUG(10, ("set_server_role: role = %s\n", server_role_str(server_role)));
}

/***********************************************************
 returns role of Samba server
************************************************************/

int lp_server_role(void)
{
	return server_role;
}
