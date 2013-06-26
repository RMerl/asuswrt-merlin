/*
   Samba Unix/Linux SMB client library
   net help commands
   Copyright (C) 2002 Jim McDonough (jmcd@us.ibm.com)

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

static int net_usage(struct net_context *c, int argc, const char **argv);

static int net_help_usage(struct net_context *c, int argc, const char **argv)
{
	c->display_usage = true;
	return net_usage(c, argc, argv);
}

static int net_usage(struct net_context *c, int argc, const char **argv)
{
	struct functable *table = (struct functable*) c->private_data;
	int i;

	d_printf(_("Usage:\n"));
	for (i=0; table[i].funcname != NULL; i++) {
		if (c->display_usage) {
			d_printf(_("net %s usage:\n"), table[i].funcname);
			d_printf("\n%s\n\n", _(table[i].usage));
		} else {
			d_printf("%s %-15s %s\n", "net", table[i].funcname,
				 _(table[i].description));
		}

	}

	net_common_flags_usage(c, argc, argv);
	return -1;
}

/*
  handle "net help *" subcommands
*/
int net_help(struct net_context *c, int argc, const char **argv)
{
	struct functable *func = (struct functable *)c->private_data;

	if (argc == 0) {
		return net_usage(c, argc, argv);
	}

	if (StrCaseCmp(argv[0], "help") == 0) {
		return net_help_usage(c, argc, argv);
	}

	c->display_usage = true;
	return net_run_function(c, argc, argv, "net help", func);
}
