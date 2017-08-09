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

int net_common_methods_usage(struct net_context *c, int argc, const char**argv)
{
	d_printf(_("Valid methods: (auto-detected if not specified)\n"));
	d_printf(_("\tads\t\t\t\tActive Directory (LDAP/Kerberos)\n"));
	d_printf(_("\trpc\t\t\t\tDCE-RPC\n"));
	d_printf(_("\trap\t\t\t\tRAP (older systems)\n"));
	d_printf("\n");
	return 0;
}

int net_common_flags_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("Valid targets: choose one (none defaults to localhost)\n"));
	d_printf(_("\t-S or --server=<server>\t\tserver name\n"));
	d_printf(_("\t-I or --ipaddress=<ipaddr>\taddress of target server\n"));
	d_printf(_("\t-w or --workgroup=<wg>\t\ttarget workgroup or domain\n"));

	d_printf("\n");
	d_printf(_("Valid miscellaneous options are:\n")); /* misc options */
	d_printf(_("\t-p or --port=<port>\t\tconnection port on target\n"));
	d_printf(_("\t-W or --myworkgroup=<wg>\tclient workgroup\n"));
	d_printf(_("\t-d or --debuglevel=<level>\tdebug level (0-10)\n"));
	d_printf(_("\t-n or --myname=<name>\t\tclient name\n"));
	d_printf(_("\t-U or --user=<name>\t\tuser name\n"));
	d_printf(_("\t-s or --configfile=<path>\tpathname of smb.conf file\n"));
	d_printf(_("\t-l or --long\t\t\tDisplay full information\n"));
	d_printf(_("\t-V or --version\t\t\tPrint samba version information\n"));
	d_printf(_("\t-P or --machine-pass\t\tAuthenticate as machine "
		   "account\n"));
	d_printf(_("\t-e or --encrypt\t\t\tEncrypt SMB transport "
		   "(UNIX extended servers only)\n"));
	d_printf(_("\t-k or --kerberos\t\tUse kerberos (active directory) "
		   "authentication\n"));
	return -1;
}

