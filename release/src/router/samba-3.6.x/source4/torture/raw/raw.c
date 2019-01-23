/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "libcli/raw/libcliraw.h"
#include "torture/util.h"
#include "torture/smbtorture.h"
#include "torture/raw/proto.h"

NTSTATUS torture_raw_init(void)
{
	struct torture_suite *suite = torture_suite_create(
		talloc_autofree_context(), "raw");
	/* RAW smb tests */
	torture_suite_add_simple_test(suite, "bench-oplock", torture_bench_oplock);
	torture_suite_add_simple_test(suite, "ping-pong", torture_ping_pong);
	torture_suite_add_simple_test(suite, "bench-lock", torture_bench_lock);
	torture_suite_add_simple_test(suite, "bench-open", torture_bench_open);
	torture_suite_add_simple_test(suite, "bench-lookup",
		torture_bench_lookup);
	torture_suite_add_simple_test(suite, "bench-tcon",
		torture_bench_treeconnect);
	torture_suite_add_simple_test(suite, "offline", torture_test_offline);
	torture_suite_add_1smb_test(suite, "qfsinfo", torture_raw_qfsinfo);
	torture_suite_add_1smb_test(suite, "qfileinfo", torture_raw_qfileinfo);
	torture_suite_add_1smb_test(suite, "qfileinfo.ipc", torture_raw_qfileinfo_pipe);
	torture_suite_add_suite(suite, torture_raw_sfileinfo(suite));
	torture_suite_add_suite(suite, torture_raw_search(suite));
	torture_suite_add_1smb_test(suite, "close", torture_raw_close);
	torture_suite_add_suite(suite, torture_raw_open(suite));
	torture_suite_add_1smb_test(suite, "mkdir", torture_raw_mkdir);
	torture_suite_add_suite(suite, torture_raw_oplock(suite));
	torture_suite_add_1smb_test(suite, "hold-oplock", torture_hold_oplock);
	torture_suite_add_2smb_test(suite, "notify", torture_raw_notify);
	torture_suite_add_1smb_test(suite, "mux", torture_raw_mux);
	torture_suite_add_1smb_test(suite, "ioctl", torture_raw_ioctl);
	torture_suite_add_1smb_test(suite, "chkpath", torture_raw_chkpath);
	torture_suite_add_suite(suite, torture_raw_unlink(suite));
	torture_suite_add_suite(suite, torture_raw_read(suite));
	torture_suite_add_suite(suite, torture_raw_write(suite));
	torture_suite_add_suite(suite, torture_raw_lock(suite));
	torture_suite_add_1smb_test(suite, "context", torture_raw_context);
	torture_suite_add_suite(suite, torture_raw_rename(suite));
	torture_suite_add_1smb_test(suite, "seek", torture_raw_seek);
	torture_suite_add_1smb_test(suite, "eas", torture_raw_eas);
	torture_suite_add_suite(suite, torture_raw_streams(suite));
	torture_suite_add_suite(suite, torture_raw_acls(suite));
	torture_suite_add_1smb_test(suite, "composite", torture_raw_composite);
	torture_suite_add_simple_test(suite, "samba3hide", torture_samba3_hide);
	torture_suite_add_simple_test(suite, "samba3closeerr", torture_samba3_closeerr);
	torture_suite_add_simple_test(suite, "samba3rootdirfid",
				      torture_samba3_rootdirfid);
	torture_suite_add_simple_test(suite, "samba3checkfsp", torture_samba3_checkfsp);
	torture_suite_add_simple_test(suite, "samba3oplocklogoff", torture_samba3_oplock_logoff);
	torture_suite_add_simple_test(suite, "samba3badpath", torture_samba3_badpath);
	torture_suite_add_simple_test(suite, "samba3caseinsensitive",
				      torture_samba3_caseinsensitive);
	torture_suite_add_simple_test(suite, "samba3posixtimedlock",
				      torture_samba3_posixtimedlock);
	torture_suite_add_simple_test(suite, "scan-eamax", torture_max_eas);

	suite->description = talloc_strdup(suite, "Tests for the raw SMB interface");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
