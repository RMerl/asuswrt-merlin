/*
   Samba Unix/Linux SMB client library
   net group commands
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

int net_group_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net [<method>] group [misc. options] [targets]"
		   "\n\tList user groups\n\n"));
	d_printf(_("net rpc group LIST [global|local|builtin]* [misc. options]"
		   "\n\tList specific user groups\n\n"));
	d_printf(_("net [<method>] group DELETE <name> "
		   "[misc. options] [targets]"
		   "\n\tDelete specified group\n"));
	d_printf(_("\nnet [<method>] group ADD <name> [-C comment] "
		   "[-c container] [misc. options] [targets]\n"
		   "\tCreate specified group\n"));
	d_printf(_("\nnet rpc group MEMBERS <name>\n\tList Group Members\n\n"));
	d_printf(_("\nnet rpc group ADDMEM <group> <member>\n"
		   "\tAdd Group Members\n\n"));
	d_printf(_("\nnet rpc group DELMEM <group> <member>\n"
		   "\tDelete Group Members\n\n"));
	net_common_methods_usage(c, argc, argv);
	net_common_flags_usage(c, argc, argv);
	d_printf(_("\t-C or --comment=<comment>\tdescriptive comment "
		   "(for add only)\n"));
	d_printf(_("\t-c or --container=<container>\tLDAP container, "
		   "defaults to cn=Users (for add in ADS only)\n"));
	d_printf(_("\t-L or --localgroup\t\tWhen adding groups, "
		   "create a local group (alias)\n"));
	return -1;
}

int net_group(struct net_context *c, int argc, const char **argv)
{
	if (argc < 1)
		return net_group_usage(c, argc, argv);

	if (StrCaseCmp(argv[0], "HELP") == 0) {
		net_group_usage(c, argc, argv);
		return 0;
	}

	if (net_ads_check(c) == 0)
		return net_ads_group(c, argc, argv);

	return net_rap_group(c, argc, argv);
}

