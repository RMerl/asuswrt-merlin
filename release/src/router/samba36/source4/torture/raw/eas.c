/*
   Unix SMB/CIFS implementation.

   test DOS extended attributes

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Guenter Kukkukk 2005

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
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define BASEDIR "\\testeas"

#define CHECK_STATUS(status, correct) do { \
	torture_assert_ntstatus_equal_goto(tctx, status, correct, ret, done, "Incorrect status"); \
	} while (0)

static	bool maxeadebug; /* need that here, to allow no file delete in debug case */

static bool check_ea(struct smbcli_state *cli,
		     const char *fname, const char *eaname, const char *value)
{
	NTSTATUS status = torture_check_ea(cli, fname, eaname, value);
	return NT_STATUS_IS_OK(status);
}

static bool test_eas(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	union smb_setfileinfo setfile;
	union smb_open io;
	const char *fname = BASEDIR "\\ea.txt";
	bool ret = true;
	int fnum = -1;

	torture_comment(tctx, "TESTING SETFILEINFO EA_SET\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	ret &= check_ea(cli, fname, "EAONE", NULL);

	torture_comment(tctx, "Adding first two EAs\n");
	setfile.generic.level = RAW_SFILEINFO_EA_SET;
	setfile.generic.in.file.fnum = fnum;
	setfile.ea_set.in.num_eas = 2;
	setfile.ea_set.in.eas = talloc_array(tctx, struct ea_struct, 2);
	setfile.ea_set.in.eas[0].flags = 0;
	setfile.ea_set.in.eas[0].name.s = "EAONE";
	setfile.ea_set.in.eas[0].value = data_blob_string_const("VALUE1");
	setfile.ea_set.in.eas[1].flags = 0;
	setfile.ea_set.in.eas[1].name.s = "SECONDEA";
	setfile.ea_set.in.eas[1].value = data_blob_string_const("ValueTwo");

	status = smb_raw_setfileinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_ea(cli, fname, "EAONE", "VALUE1");
	ret &= check_ea(cli, fname, "SECONDEA", "ValueTwo");

	torture_comment(tctx, "Modifying 2nd EA\n");
	setfile.ea_set.in.num_eas = 1;
	setfile.ea_set.in.eas[0].name.s = "SECONDEA";
	setfile.ea_set.in.eas[0].value = data_blob_string_const(" Changed Value");
	status = smb_raw_setfileinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_ea(cli, fname, "EAONE", "VALUE1");
	ret &= check_ea(cli, fname, "SECONDEA", " Changed Value");

	torture_comment(tctx, "Setting a NULL EA\n");
	setfile.ea_set.in.eas[0].value = data_blob(NULL, 0);
	setfile.ea_set.in.eas[0].name.s = "NULLEA";
	status = smb_raw_setfileinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_ea(cli, fname, "EAONE", "VALUE1");
	ret &= check_ea(cli, fname, "SECONDEA", " Changed Value");
	ret &= check_ea(cli, fname, "NULLEA", NULL);

	torture_comment(tctx, "Deleting first EA\n");
	setfile.ea_set.in.eas[0].flags = 0;
	setfile.ea_set.in.eas[0].name.s = "EAONE";
	setfile.ea_set.in.eas[0].value = data_blob(NULL, 0);
	status = smb_raw_setfileinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_ea(cli, fname, "EAONE", NULL);
	ret &= check_ea(cli, fname, "SECONDEA", " Changed Value");

	torture_comment(tctx, "Deleting second EA\n");
	setfile.ea_set.in.eas[0].flags = 0;
	setfile.ea_set.in.eas[0].name.s = "SECONDEA";
	setfile.ea_set.in.eas[0].value = data_blob(NULL, 0);
	status = smb_raw_setfileinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);

	ret &= check_ea(cli, fname, "EAONE", NULL);
	ret &= check_ea(cli, fname, "SECONDEA", NULL);

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}


/*
 * Helper function to retrieve the max. ea size for one ea name
 */
static int test_one_eamax(struct torture_context *tctx,
			  struct smbcli_state *cli, const int fnum,
			  const char *eaname, DATA_BLOB eablob,
			  const int eastart, const int eadebug)
{
	NTSTATUS status;
	struct ea_struct eastruct;
	union smb_setfileinfo setfile;
	int i, high, low, maxeasize;

	setfile.generic.level = RAW_SFILEINFO_EA_SET;
	setfile.generic.in.file.fnum = fnum;
	setfile.ea_set.in.num_eas = 1;
	setfile.ea_set.in.eas = &eastruct;
	setfile.ea_set.in.eas->flags = 0;
	setfile.ea_set.in.eas->name.s = eaname;
	setfile.ea_set.in.eas->value = eablob;

	maxeasize = eablob.length;
	i = eastart;
	low = 0;
	high = maxeasize;

	do {
		if (eadebug) {
			torture_comment(tctx, "Testing EA size: %d\n", i);
		}
		setfile.ea_set.in.eas->value.length = i;

		status = smb_raw_setfileinfo(cli->tree, &setfile);

		if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
			if (eadebug) {
				torture_comment(tctx, "[%s] EA size %d succeeded! "
					"(high=%d low=%d)\n",
					eaname, i, high, low);
			}
			low = i;
			if (low == maxeasize) {
				torture_comment(tctx, "Max. EA size for \"%s\"=%d "
					"[but could be possibly larger]\n",
					eaname, low);
				break;
			}
			if (high - low == 1 && high != maxeasize) {
				torture_comment(tctx, "Max. EA size for \"%s\"=%d\n",
					eaname, low);
				break;
			}
			i += (high - low + 1) / 2;
		} else {
			if (eadebug) {
				torture_comment(tctx, "[%s] EA size %d failed!    "
					"(high=%d low=%d) [%s]\n",
					eaname, i, high, low,
					nt_errstr(status));
			}
			high = i;
			if (high - low <= 1) {
				torture_comment(tctx, "Max. EA size for \"%s\"=%d\n",
					eaname, low);
				break;
			}
			i -= (high - low + 1) / 2;
		}
	} while (true);

	return low;
}

/*
 * Test for maximum ea size - more than one ea name is checked.
 *
 * Additional parameters can be passed, to allow further testing:
 *
 *             default
 * maxeasize    65536   limit the max. size for a single EA name
 * maxeanames     101   limit of the number of tested names
 * maxeastart       1   this EA size is used to test for the 1st EA (atm)
 * maxeadebug       0   if set true, further debug output is done - in addition
 *                      the testfile is not deleted for further inspection!
 *
 * Set some/all of these options on the cmdline with:
 * --option torture:maxeasize=1024 --option torture:maxeadebug=1 ...
 *
 */
static bool test_max_eas(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\ea_max.txt";
	int fnum = -1;
	bool ret = true;
	bool err = false;

	int       i, j, k, last, total;
	DATA_BLOB eablob;
	char      *eaname = NULL;
	int       maxeasize;
	int       maxeanames;
	int       maxeastart;

	torture_comment(tctx, "TESTING SETFILEINFO MAX. EA_SET\n");

	maxeasize  = torture_setting_int(tctx, "maxeasize", 65536);
	maxeanames = torture_setting_int(tctx, "maxeanames", 101);
	maxeastart = torture_setting_int(tctx, "maxeastart", 1);
	maxeadebug = torture_setting_int(tctx, "maxeadebug", 0);

	/* Do some sanity check on possibly passed parms */
	if (maxeasize <= 0) {
		torture_comment(tctx, "Invalid parameter 'maxeasize=%d'",maxeasize);
		err = true;
	}
	if (maxeanames <= 0) {
		torture_comment(tctx, "Invalid parameter 'maxeanames=%d'",maxeanames);
		err = true;
	}
	if (maxeastart <= 0) {
		torture_comment(tctx, "Invalid parameter 'maxeastart=%d'",maxeastart);
		err = true;
	}
	if (maxeadebug < 0) {
		torture_comment(tctx, "Invalid parameter 'maxeadebug=%d'",maxeadebug);
		err = true;
	}
	if (err) {
	  torture_comment(tctx, "\n\n");
	  goto done;
	}
	if (maxeastart > maxeasize) {
		maxeastart = maxeasize;
		torture_comment(tctx, "'maxeastart' outside range - corrected to %d\n",
			maxeastart);
	}
	torture_comment(tctx, "MAXEA parms: maxeasize=%d maxeanames=%d maxeastart=%d"
	       " maxeadebug=%d\n", maxeasize, maxeanames, maxeastart,
	       maxeadebug);

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	eablob = data_blob_talloc(tctx, NULL, maxeasize);
	if (eablob.data == NULL) {
		goto done;
	}
	/*
	 * Fill in some EA data - the offset could be easily checked
	 * during a hexdump.
	 */
	for (i = 0, k = 0; i < eablob.length / 4; i++, k+=4) {
		eablob.data[k]   = k & 0xff;
		eablob.data[k+1] = (k >>  8) & 0xff;
		eablob.data[k+2] = (k >> 16) & 0xff;
		eablob.data[k+3] = (k >> 24) & 0xff;
	}

	i = eablob.length % 4;
	if (i-- > 0) {
		eablob.data[k] = k & 0xff;
		if (i-- > 0) {
			eablob.data[k+1] = (k >>  8) & 0xff;
			if (i-- > 0) {
				eablob.data[k+2] = (k >> 16) & 0xff;
			}
		}
	}
	/*
	 * Filesystems might allow max. EAs data for different EA names.
	 * So more than one EA name should be checked.
	 */
	total = 0;
	last  = maxeastart;

	for (i = 0; i < maxeanames; i++) {
		if (eaname != NULL) {
			talloc_free(eaname);
		}
		eaname = talloc_asprintf(tctx, "MAX%d", i);
		if(eaname == NULL) {
			goto done;
		}
		j = test_one_eamax(tctx, cli, fnum, eaname, eablob, last, maxeadebug);
		if (j <= 0) {
			break;
		}
		total += j;
		last = j;
	}

	torture_comment(tctx, "Total EA size:%d\n", total);
	if (i == maxeanames) {
		torture_comment(tctx, "NOTE: More EAs could be available!\n");
	}
	if (total == 0) {
		ret = false;
	}
done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

/*
  test using NTTRANS CREATE to create a file with an initial EA set
*/
static bool test_nttrans_create(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname = BASEDIR "\\ea2.txt";
	bool ret = true;
	int fnum = -1;
	struct ea_struct eas[3];
	struct smb_ea_list ea_list;

	torture_comment(tctx, "TESTING NTTRANS CREATE WITH EAS\n");

	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	ea_list.num_eas = 3;
	ea_list.eas = eas;

	eas[0].flags = 0;
	eas[0].name.s = "1st EA";
	eas[0].value = data_blob_string_const("Value One");

	eas[1].flags = 0;
	eas[1].name.s = "2nd EA";
	eas[1].value = data_blob_string_const("Second Value");

	eas[2].flags = 0;
	eas[2].name.s = "and 3rd";
	eas[2].value = data_blob_string_const("final value");

	io.ntcreatex.in.ea_list = &ea_list;
	io.ntcreatex.in.sec_desc = NULL;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	ret &= check_ea(cli, fname, "EAONE", NULL);
	ret &= check_ea(cli, fname, "1st EA", "Value One");
	ret &= check_ea(cli, fname, "2nd EA", "Second Value");
	ret &= check_ea(cli, fname, "and 3rd", "final value");

	smbcli_close(cli->tree, fnum);

	torture_comment(tctx, "Trying to add EAs on non-create\n");
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io.ntcreatex.in.fname = fname;

	ea_list.num_eas = 1;
	eas[0].flags = 0;
	eas[0].name.s = "Fourth EA";
	eas[0].value = data_blob_string_const("Value Four");

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	ret &= check_ea(cli, fname, "1st EA", "Value One");
	ret &= check_ea(cli, fname, "2nd EA", "Second Value");
	ret &= check_ea(cli, fname, "and 3rd", "final value");
	ret &= check_ea(cli, fname, "Fourth EA", NULL);

done:
	smbcli_close(cli->tree, fnum);
	return ret;
}

/*
   basic testing of EA calls
*/
bool torture_raw_eas(struct torture_context *torture, struct smbcli_state *cli)
{
	bool ret = true;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_eas(cli, torture);
	ret &= test_nttrans_create(cli, torture);

	smb_raw_exit(cli->session);

	return ret;
}

/*
   test max EA size
*/
bool torture_max_eas(struct torture_context *torture)
{
	struct smbcli_state *cli;
	bool ret = true;

	if (!torture_open_connection(&cli, torture, 0)) {
		return false;
	}

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_max_eas(cli, torture);

	smb_raw_exit(cli->session);
	if (!maxeadebug) {
		/* in no ea debug case, all files are gone now */
		smbcli_deltree(cli->tree, BASEDIR);
	}

	torture_close_connection(cli);
	return ret;
}
