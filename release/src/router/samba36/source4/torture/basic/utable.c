/* 
   Unix SMB/CIFS implementation.
   SMB torture tester - unicode table dumper
   Copyright (C) Andrew Tridgell 2001
   
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
#include "system/filesys.h"
#include "system/locale.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "param/param.h"

bool torture_utable(struct torture_context *tctx, 
					struct smbcli_state *cli)
{
	char fname[256];
	const char *alt_name;
	int fnum;
	uint8_t c2[4];
	int c, fd;
	size_t len;
	int chars_allowed=0, alt_allowed=0;
	uint8_t valid[0x10000];

	torture_comment(tctx, "Generating valid character table\n");

	memset(valid, 0, sizeof(valid));

	torture_assert(tctx, torture_setup_dir(cli, "\\utable"),
				   "Setting up dir \\utable failed");

	for (c=1; c < 0x10000; c++) {
		char *p;

		SSVAL(c2, 0, c);
		strncpy(fname, "\\utable\\x", sizeof(fname)-1);
		p = fname+strlen(fname);
		len = convert_string(CH_UTF16, CH_UNIX, 
				     c2, 2, 
				     p, sizeof(fname)-strlen(fname), false);
		p[len] = 0;
		strncat(fname,"_a_long_extension",sizeof(fname)-1);

		fnum = smbcli_open(cli->tree, fname, O_RDWR | O_CREAT | O_TRUNC, 
				DENY_NONE);
		if (fnum == -1) continue;

		chars_allowed++;

		smbcli_qpathinfo_alt_name(cli->tree, fname, &alt_name);

		if (strncmp(alt_name, "X_A_L", 5) != 0) {
			alt_allowed++;
			valid[c] = 1;
			torture_comment(tctx, "fname=[%s] alt_name=[%s]\n", fname, alt_name);
		}

		smbcli_close(cli->tree, fnum);
		smbcli_unlink(cli->tree, fname);

		if (c % 100 == 0) {
			if (torture_setting_bool(tctx, "progress", true)) {
				torture_comment(tctx, "%d (%d/%d)\r", c, chars_allowed, alt_allowed);
				fflush(stdout);
			}
		}
	}
	torture_comment(tctx, "%d (%d/%d)\n", c, chars_allowed, alt_allowed);

	smbcli_rmdir(cli->tree, "\\utable");

	torture_comment(tctx, "%d chars allowed   %d alt chars allowed\n", chars_allowed, alt_allowed);

	fd = open("valid.dat", O_WRONLY|O_CREAT|O_TRUNC, 0644);
	torture_assert(tctx, fd != -1, 
		talloc_asprintf(tctx, 
		"Failed to create valid.dat - %s", strerror(errno)));
	write(fd, valid, 0x10000);
	close(fd);
	torture_comment(tctx, "wrote valid.dat\n");

	return true;
}


static char *form_name(int c)
{
	static char fname[256];
	uint8_t c2[4];
	char *p;
	size_t len;

	strncpy(fname, "\\utable\\", sizeof(fname)-1);
	p = fname+strlen(fname);
	SSVAL(c2, 0, c);

	len = convert_string(CH_UTF16, CH_UNIX, 
			     c2, 2, 
			     p, sizeof(fname)-strlen(fname), false);
	if (len == -1)
		return NULL;
	p[len] = 0;
	return fname;
}

bool torture_casetable(struct torture_context *tctx, 
					   struct smbcli_state *cli)
{
	char *fname;
	int fnum;
	int c, i;
#define MAX_EQUIVALENCE 8
	codepoint_t equiv[0x10000][MAX_EQUIVALENCE];

	torture_comment(tctx, "Determining upper/lower case table\n");

	memset(equiv, 0, sizeof(equiv));

	torture_assert(tctx, torture_setup_dir(cli, "\\utable"),
				   "Error setting up dir \\utable");

	for (c=1; c < 0x10000; c++) {
		size_t size;

		if (c == '.' || c == '\\') continue;

		torture_comment(tctx, "%04x (%c)\n", c, isprint(c)?c:'.');

		fname = form_name(c);
		fnum = smbcli_nt_create_full(cli->tree, fname, 0,
#if 0
					     SEC_RIGHT_MAXIMUM_ALLOWED, 
#else
					     SEC_RIGHTS_FILE_ALL,
#endif
					     FILE_ATTRIBUTE_NORMAL,
					     NTCREATEX_SHARE_ACCESS_NONE,
					     NTCREATEX_DISP_OPEN_IF, 0, 0);

		torture_assert(tctx, fnum != -1, 
					   talloc_asprintf(tctx, 
			"Failed to create file with char %04x\n", c));

		size = 0;

		if (NT_STATUS_IS_ERR(smbcli_qfileinfo(cli->tree, fnum, NULL, &size, 
						   NULL, NULL, NULL, NULL, NULL))) continue;

		if (size > 0) {
			/* found a character equivalence! */
			int c2[MAX_EQUIVALENCE];

			if (size/sizeof(int) >= MAX_EQUIVALENCE) {
				torture_comment(tctx, "too many chars match?? size=%d c=0x%04x\n",
				       (int)size, c);
				smbcli_close(cli->tree, fnum);
				return false;
			}

			smbcli_read(cli->tree, fnum, c2, 0, size);
			torture_comment(tctx, "%04x: ", c);
			equiv[c][0] = c;
			for (i=0; i<size/sizeof(int); i++) {
				torture_comment(tctx, "%04x ", c2[i]);
				equiv[c][i+1] = c2[i];
			}
			torture_comment(tctx, "\n");
		}

		smbcli_write(cli->tree, fnum, 0, &c, size, sizeof(c));
		smbcli_close(cli->tree, fnum);
	}

	smbcli_unlink(cli->tree, "\\utable\\*");
	smbcli_rmdir(cli->tree, "\\utable");

	return true;
}
