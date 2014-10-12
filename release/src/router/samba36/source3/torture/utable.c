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
#include "torture/proto.h"
#include "../libcli/security/security.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"

bool torture_utable(int dummy)
{
	struct cli_state *cli;
	fstring fname, alt_name;
	uint16_t fnum;
	smb_ucs2_t c2;
	int c, len, fd;
	int chars_allowed=0, alt_allowed=0;
	uint8 valid[0x10000];

	printf("starting utable\n");

	if (!torture_open_connection(&cli, 0)) {
		return False;
	}

	memset(valid, 0, sizeof(valid));

	cli_mkdir(cli, "\\utable");
	cli_unlink(cli, "\\utable\\*", FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);

	for (c=1; c < 0x10000; c++) {
		char *p;

		SSVAL(&c2, 0, c);
		fstrcpy(fname, "\\utable\\x");
		p = fname+strlen(fname);
		len = convert_string(CH_UTF16LE, CH_UNIX, 
				     &c2, 2, 
				     p, sizeof(fname)-strlen(fname), True);
		p[len] = 0;
		fstrcat(fname,"_a_long_extension");

		if (!NT_STATUS_IS_OK(cli_open(cli, fname, O_RDWR | O_CREAT | O_TRUNC, 
				DENY_NONE, &fnum))) {
			continue;
		}

		chars_allowed++;

		cli_qpathinfo_alt_name(cli, fname, alt_name);

		if (strncmp(alt_name, "X_A_L", 5) != 0) {
			alt_allowed++;
			valid[c] = 1;
			d_printf("fname=[%s] alt_name=[%s]\n", fname, alt_name);
		}

		cli_close(cli, fnum);
		cli_unlink(cli, fname, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);

		if (c % 100 == 0) {
			printf("%d (%d/%d)\r", c, chars_allowed, alt_allowed);
		}
	}
	printf("%d (%d/%d)\n", c, chars_allowed, alt_allowed);

	cli_rmdir(cli, "\\utable");

	d_printf("%d chars allowed   %d alt chars allowed\n", chars_allowed, alt_allowed);

	fd = open("valid.dat", O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		d_printf("Failed to create valid.dat - %s", strerror(errno));
		return False;
	}
	if (write(fd, valid, 0x10000) != 0x10000) {
		d_printf("Failed to create valid.dat - %s", strerror(errno));
		close(fd);
		return false;
	}
	close(fd);
	d_printf("wrote valid.dat\n");

	return True;
}


static char *form_name(int c)
{
	static fstring fname;
	smb_ucs2_t c2;
	char *p;
	int len;

	fstrcpy(fname, "\\utable\\");
	p = fname+strlen(fname);
	SSVAL(&c2, 0, c);

	len = convert_string(CH_UTF16LE, CH_UNIX, 
			     &c2, 2, 
			     p, sizeof(fname)-strlen(fname), True);
	p[len] = 0;
	return fname;
}

bool torture_casetable(int dummy)
{
	static struct cli_state *cli;
	char *fname;
	uint16_t fnum;
	int c, i;
#define MAX_EQUIVALENCE 8
	smb_ucs2_t equiv[0x10000][MAX_EQUIVALENCE];
	printf("starting casetable\n");

	if (!torture_open_connection(&cli, 0)) {
		return False;
	}

	memset(equiv, 0, sizeof(equiv));

	cli_unlink(cli, "\\utable\\*", FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
	cli_rmdir(cli, "\\utable");
	if (!NT_STATUS_IS_OK(cli_mkdir(cli, "\\utable"))) {
		printf("Failed to create utable directory!\n");
		return False;
	}

	for (c=1; c < 0x10000; c++) {
		SMB_OFF_T size;

		if (c == '.' || c == '\\') continue;

		printf("%04x (%c)\n", c, isprint(c)?c:'.');

		fname = form_name(c);
		if (!NT_STATUS_IS_OK(cli_ntcreate(cli, fname, 0,
					  GENERIC_ALL_ACCESS, 
					  FILE_ATTRIBUTE_NORMAL,
					  FILE_SHARE_NONE,
					  FILE_OPEN_IF, 0, 0, &fnum))) {
			printf("Failed to create file with char %04x\n", c);
			continue;
		}

		size = 0;

		if (!NT_STATUS_IS_OK(cli_qfileinfo_basic(
					     cli, fnum, NULL, &size,
					     NULL, NULL, NULL, NULL, NULL))) {
			continue;
		}

		if (size > 0) {
			/* found a character equivalence! */
			int c2[MAX_EQUIVALENCE];

			if (size/sizeof(int) >= MAX_EQUIVALENCE) {
				printf("too many chars match?? size=%ld c=0x%04x\n",
				       (unsigned long)size, c);
				cli_close(cli, fnum);
				return False;
			}

			cli_read(cli, fnum, (char *)c2, 0, size);
			printf("%04x: ", c);
			equiv[c][0] = c;
			for (i=0; i<size/sizeof(int); i++) {
				printf("%04x ", c2[i]);
				equiv[c][i+1] = c2[i];
			}
			printf("\n");
			fflush(stdout);
		}

		cli_writeall(cli, fnum, 0, (uint8_t *)&c, size, sizeof(c),
			     NULL);
		cli_close(cli, fnum);
	}

	cli_unlink(cli, "\\utable\\*", FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
	cli_rmdir(cli, "\\utable");

	return True;
}
