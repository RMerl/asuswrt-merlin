/*
   Samba Unix/Linux SMB client library
   net share commands
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

int net_share_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_(
	 "\nnet [<method>] share [misc. options] [targets] \n"
	 "\tenumerates all exported resources (network shares) "
	 "on target server\n\n"
	 "net [<method>] share ADD <name=serverpath> [misc. options] [targets]"
	"\n\tadds a share from a server (makes the export active)\n\n"
	"net [<method>] share DELETE <sharename> [misc. options] [targets]"
	"\n\tdeletes a share from a server (makes the export inactive)\n\n"
	"net [<method>] share ALLOWEDUSERS [<filename>] "
	"[misc. options] [targets]"
	"\n\tshows a list of all shares together with all users allowed to"
	"\n\taccess them. This needs the output of 'net usersidlist' on"
	"\n\tstdin or in <filename>.\n\n"
	 "net [<method>] share MIGRATE FILES <sharename> [misc. options] [targets]"
	 "\n\tMigrates files from remote to local server\n\n"
	 "net [<method>] share MIGRATE SHARES <sharename> [misc. options] [targets]"
	 "\n\tMigrates shares from remote to local server\n\n"
	 "net [<method>] share MIGRATE SECURITY <sharename> [misc. options] [targets]"
	 "\n\tMigrates share-ACLs from remote to local server\n\n"
	 "net [<method>] share MIGRATE ALL <sharename> [misc. options] [targets]"
	 "\n\tMigrates shares (including directories, files) from remote\n"
	 "\tto local server\n\n"
	 ));
	net_common_methods_usage(c, argc, argv);
	net_common_flags_usage(c, argc, argv);
	d_printf(_(
	 "\t-C or --comment=<comment>\tdescriptive comment (for add only)\n"
	 "\t-M or --maxusers=<num>\t\tmax users allowed for share\n"
	 "\t      --acls\t\t\tcopies ACLs as well\n"
	 "\t      --attrs\t\t\tcopies DOS Attributes as well\n"
	 "\t      --timestamps\t\tpreserve timestamps while copying files\n"
	 "\t      --destination\t\tmigration target server (default: localhost)\n"
	 "\t-e or --exclude\t\t\tlist of shares to be excluded from mirroring\n"
	 "\t-v or --verbose\t\t\tgive verbose output\n"));
	return -1;
}

int net_share(struct net_context *c, int argc, const char **argv)
{
	if (argc > 0 && StrCaseCmp(argv[0], "HELP") == 0) {
			net_share_usage(c, argc, argv);
			return 0;
	}

	if (net_rpc_check(c, 0))
		return net_rpc_share(c, argc, argv);
	return net_rap_share(c, argc, argv);
}

