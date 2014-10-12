/*
   Samba Unix/Linux SMB client library
   net file commands
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

int net_file_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net [<method>] file [misc. options] [targets]\n"
		   "\tlists all open files on file server\n"));
	d_printf(_("net [<method>] file USER <username> "
		   "[misc. options] [targets]"
		   "\n\tlists all files opened by username on file server\n"));
	d_printf(_("net [<method>] file CLOSE <id> [misc. options] [targets]\n"
		   "\tcloses specified file on target server\n"));
	d_printf(_("net [rap] file INFO <id> [misc. options] [targets]\n"
		   "\tdisplays information about the specified open file\n"));

	net_common_methods_usage(c, argc, argv);
	net_common_flags_usage(c, argc, argv);
	return -1;
}

int net_file(struct net_context *c, int argc, const char **argv)
{
	if (argc < 1)
		return net_file_usage(c, argc, argv);

	if (StrCaseCmp(argv[0], "HELP") == 0) {
		net_file_usage(c, argc, argv);
		return 0;
	}

	if (net_rpc_check(c, 0))
		return net_rpc_file(c, argc, argv);
	return net_rap_file(c, argc, argv);
}


