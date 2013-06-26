/*
   Samba Unix/Linux SMB client library
   net user commands
   Copyright (C) 2002  Jim McDonough  (jmcd@us.ibm.com)
   Copyright (C) 2002  Andrew Tridgell  (tridge@samba.org)
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

int net_user_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("\nnet [<method>] user [misc. options] [targets]"
		   "\n\tList users\n\n"));
	d_printf(_("net [<method>] user DELETE <name> [misc. options] [targets]"
		   "\n\tDelete specified user\n"));
	d_printf(_("\nnet [<method>] user INFO <name> [misc. options] [targets]"
		   "\n\tList the domain groups of the specified user\n"));
	d_printf(_("\nnet [<method>] user ADD <name> [password] [-c container] "
		   "[-F user flags] [misc. options]"
		   " [targets]\n\tAdd specified user\n"));
	d_printf(_("\nnet [<method>] user RENAME <oldusername> <newusername>"
		   " [targets]\n\tRename specified user\n\n"));

	net_common_methods_usage(c, argc, argv);
	net_common_flags_usage(c, argc, argv);
	d_printf(_("\t-C or --comment=<comment>\tdescriptive comment "
		   "(for add only)\n"));
	d_printf(_("\t-c or --container=<container>\tLDAP container, defaults "
		   "to cn=Users (for add in ADS only)\n"));
	return -1;
}

int net_user(struct net_context *c, int argc, const char **argv)
{
	if (argc < 1)
		return net_user_usage(c, argc, argv);

	if (StrCaseCmp(argv[0], "HELP") == 0) {
		net_user_usage(c, argc, argv);
		return 0;
	}

	if (net_ads_check(c) == 0)
		return net_ads_user(c, argc, argv);

	/* if server is not specified, default to PDC? */
	if (net_rpc_check(c, NET_FLAGS_PDC))
		return net_rpc_user(c, argc, argv);

	return net_rap_user(c, argc, argv);
}

