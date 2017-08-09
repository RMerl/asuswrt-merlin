/*
   Samba Unix/Linux SMB client library
   net afs commands
   Copyright (C) 2003  Volker Lendecke  (vl@samba.org)

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
#include "secrets.h"
#include "system/filesys.h"

int net_afs_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("  net afs key filename\n"
		 "\tImports a OpenAFS KeyFile into our secrets.tdb\n\n"));
	d_printf(_("  net afs impersonate <user> <cell>\n"
		 "\tCreates a token for user@cell\n\n"));
	return -1;
}

int net_afs_key(struct net_context *c, int argc, const char **argv)
{
	int fd;
	struct afs_keyfile keyfile;

	if (argc != 2) {
		d_printf("%s net afs key <keyfile> cell\n", _("Usage:"));
		return -1;
	}

	if (!secrets_init()) {
		d_fprintf(stderr, _("Could not open secrets.tdb\n"));
		return -1;
	}

	if ((fd = open(argv[0], O_RDONLY, 0)) < 0) {
		d_fprintf(stderr, _("Could not open %s\n"), argv[0]);
		return -1;
	}

	if (read(fd, &keyfile, sizeof(keyfile)) != sizeof(keyfile)) {
		d_fprintf(stderr, _("Could not read keyfile\n"));
		close(fd);
		return -1;
	}
	close(fd);

	if (!secrets_store_afs_keyfile(argv[1], &keyfile)) {
		d_fprintf(stderr, _("Could not write keyfile to secrets.tdb\n"));
		return -1;
	}

	return 0;
}

int net_afs_impersonate(struct net_context *c, int argc,
			       const char **argv)
{
	char *token;

	if (argc != 2) {
		d_fprintf(stderr, "%s net afs impersonate <user> <cell>\n",
			  _("Usage:"));
	        exit(1);
	}

	token = afs_createtoken_str(argv[0], argv[1]);

	if (token == NULL) {
		fprintf(stderr, _("Could not create token\n"));
	        exit(1);
	}

	if (!afs_settoken_str(token)) {
		fprintf(stderr, _("Could not set token into kernel\n"));
	        exit(1);
	}

	printf(_("Success: %s@%s\n"), argv[0], argv[1]);
	return 0;
}

int net_afs(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"key",
			net_afs_key,
			NET_TRANSPORT_LOCAL,
			N_("Import an OpenAFS keyfile"),
			N_("net afs key <filename>\n"
			   "    Import kefile from <filename>.")
		},
		{
			"impersonate",
			net_afs_impersonate,
			NET_TRANSPORT_LOCAL,
			N_("Get a user token"),
			N_("net afs impersonate <user> <cell>\n"
			   "    Create token for user@cell")
		},
		{NULL, NULL, 0, NULL, NULL}
	};
	return net_run_function(c, argc, argv, "net afs", func);
}

