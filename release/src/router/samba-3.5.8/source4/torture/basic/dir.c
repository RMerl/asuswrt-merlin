/* 
   Unix SMB/CIFS implementation.

   directory scanning tests

   Copyright (C) Andrew Tridgell 2003
   
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
#include "libcli/libcli.h"
#include "torture/torture.h"
#include "torture/util.h"
#include "system/filesys.h"

static void list_fn(struct clilist_file_info *finfo, const char *name, void *state)
{
	
}

/*
  test directory listing speed
 */
bool torture_dirtest1(struct torture_context *tctx, 
		      struct smbcli_state *cli)
{
	int i;
	int fnum;
	bool correct = true;
	extern int torture_numops;
	struct timeval tv;

	torture_comment(tctx, "Creating %d random filenames\n", torture_numops);

	srandom(0);
	tv = timeval_current();
	for (i=0;i<torture_numops;i++) {
		char *fname;
		asprintf(&fname, "\\%x", (int)random());
		fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
		if (fnum == -1) {
			fprintf(stderr,"(%s) Failed to open %s\n", 
				__location__, fname);
			return false;
		}
		smbcli_close(cli->tree, fnum);
		free(fname);
	}

	torture_comment(tctx, "Matched %d\n", smbcli_list(cli->tree, "a*.*", 0, list_fn, NULL));
	torture_comment(tctx, "Matched %d\n", smbcli_list(cli->tree, "b*.*", 0, list_fn, NULL));
	torture_comment(tctx, "Matched %d\n", smbcli_list(cli->tree, "xyzabc", 0, list_fn, NULL));

	torture_comment(tctx, "dirtest core %g seconds\n", timeval_elapsed(&tv));

	srandom(0);
	for (i=0;i<torture_numops;i++) {
		char *fname;
		asprintf(&fname, "\\%x", (int)random());
		smbcli_unlink(cli->tree, fname);
		free(fname);
	}

	return correct;
}

bool torture_dirtest2(struct torture_context *tctx, 
		      struct smbcli_state *cli)
{
	int i;
	int fnum, num_seen;
	bool correct = true;
	extern int torture_entries;

	if (!torture_setup_dir(cli, "\\LISTDIR")) {
		return false;
	}

	torture_comment(tctx, "Creating %d files\n", torture_entries);

	/* Create torture_entries files and torture_entries directories. */
	for (i=0;i<torture_entries;i++) {
		char *fname;
		asprintf(&fname, "\\LISTDIR\\f%d", i);
		fnum = smbcli_nt_create_full(cli->tree, fname, 0, 
					     SEC_RIGHTS_FILE_ALL,
					     FILE_ATTRIBUTE_ARCHIVE,
					     NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, 
					     NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
		if (fnum == -1) {
			fprintf(stderr,"(%s) Failed to open %s, error=%s\n", 
				__location__, fname, smbcli_errstr(cli->tree));
			return false;
		}
		free(fname);
		smbcli_close(cli->tree, fnum);
	}
	for (i=0;i<torture_entries;i++) {
		char *fname;
		asprintf(&fname, "\\LISTDIR\\d%d", i);
		if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, fname))) {
			fprintf(stderr,"(%s) Failed to open %s, error=%s\n", 
				__location__, fname, smbcli_errstr(cli->tree));
			return false;
		}
		free(fname);
	}

	/* Now ensure that doing an old list sees both files and directories. */
	num_seen = smbcli_list_old(cli->tree, "\\LISTDIR\\*", FILE_ATTRIBUTE_DIRECTORY, list_fn, NULL);
	torture_comment(tctx, "num_seen = %d\n", num_seen );
	/* We should see (torture_entries) each of files & directories + . and .. */
	if (num_seen != (2*torture_entries)+2) {
		correct = false;
		fprintf(stderr,"(%s) entry count mismatch, should be %d, was %d\n",
			__location__, (2*torture_entries)+2, num_seen);
	}
		

	/* Ensure if we have the "must have" bits we only see the
	 * relevant entries.
	 */
	num_seen = smbcli_list_old(cli->tree, "\\LISTDIR\\*", (FILE_ATTRIBUTE_DIRECTORY<<8)|FILE_ATTRIBUTE_DIRECTORY, list_fn, NULL);
	torture_comment(tctx, "num_seen = %d\n", num_seen );
	if (num_seen != torture_entries+2) {
		correct = false;
		fprintf(stderr,"(%s) entry count mismatch, should be %d, was %d\n",
			__location__, torture_entries+2, num_seen);
	}

	num_seen = smbcli_list_old(cli->tree, "\\LISTDIR\\*", (FILE_ATTRIBUTE_ARCHIVE<<8)|FILE_ATTRIBUTE_DIRECTORY, list_fn, NULL);
	torture_comment(tctx, "num_seen = %d\n", num_seen );
	if (num_seen != torture_entries) {
		correct = false;
		fprintf(stderr,"(%s) entry count mismatch, should be %d, was %d\n",
			__location__, torture_entries, num_seen);
	}

	/* Delete everything. */
	if (smbcli_deltree(cli->tree, "\\LISTDIR") == -1) {
		fprintf(stderr,"(%s) Failed to deltree %s, error=%s\n", "\\LISTDIR", 
			__location__, smbcli_errstr(cli->tree));
		return false;
	}

#if 0
	torture_comment(tctx, "Matched %d\n", smbcli_list(cli->tree, "a*.*", 0, list_fn, NULL));
	torture_comment(tctx, "Matched %d\n", smbcli_list(cli->tree, "b*.*", 0, list_fn, NULL));
	torture_comment(tctx, "Matched %d\n", smbcli_list(cli->tree, "xyzabc", 0, list_fn, NULL));
#endif

	return correct;
}
