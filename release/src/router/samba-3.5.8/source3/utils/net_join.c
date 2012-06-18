/*
   Samba Unix/Linux SMB client library
   net join commands
   Copyright (C) 2002  Jim McDonough  (jmcd@us.ibm.com)
   Copyright (C) 2008  Kai Blin  (kai@samba.org)

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
#include "utils/net.h"

int net_join_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("\nnet [<method>] join [misc. options]\n"
		   "\tjoins this server to a domain\n"));
	d_printf(_("Valid methods: (auto-detected if not specified)\n"));
	d_printf(_("\tads\t\t\t\tActive Directory (LDAP/Kerberos)\n"));
	d_printf(_("\trpc\t\t\t\tDCE-RPC\n"));
	net_common_flags_usage(c, argc, argv);
	return -1;
}

int net_join(struct net_context *c, int argc, const char **argv)
{
	if ((argc > 0) && (StrCaseCmp(argv[0], "HELP") == 0)) {
		net_join_usage(c, argc, argv);
		return 0;
	}

	if (net_ads_check_our_domain(c) == 0) {
		if (net_ads_join(c, argc, argv) == 0)
			return 0;
		else
			d_fprintf(stderr,
				  _("ADS join did not work, falling back to "
				    "RPC...\n"));
	}
	return net_rpc_join(c, argc, argv);
}


