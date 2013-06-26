/*
   Unix SMB/CIFS implementation.

   SMB torture tester - header file

   Copyright (C) Andrew Tridgell 1997-1998
   Copyright (C) Jeremy Allison 2009

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

#ifndef __TORTURE_H__
#define __TORTURE_H__

struct cli_state;

/* The following definitions come from torture/denytest.c  */

bool torture_denytest1(int dummy);
bool torture_denytest2(int dummy);

/* The following definitions come from torture/mangle_test.c  */

bool torture_mangle(int dummy);

/* The following definitions come from torture/nbio.c  */

double nbio_total(void);
void nb_alarm(int ignore);
void nbio_shmem(int n);
void nb_setup(struct cli_state *cli);
void nb_unlink(const char *fname);
void nb_createx(const char *fname,
		unsigned create_options, unsigned create_disposition, int handle);
void nb_writex(int handle, int offset, int size, int ret_size);
void nb_readx(int handle, int offset, int size, int ret_size);
void nb_close(int handle);
void nb_rmdir(const char *fname);
void nb_rename(const char *oldname, const char *newname);
void nb_qpathinfo(const char *fname);
void nb_qfileinfo(int fnum);
void nb_qfsinfo(int level);
void nb_findfirst(const char *mask);
void nb_flush(int fnum);
void nb_deltree(const char *dname);
void nb_cleanup(void);

/* The following definitions come from torture/scanner.c  */

bool torture_trans2_scan(int dummy);
bool torture_nttrans_scan(int dummy);

/* The following definitions come from torture/torture.c  */

void *shm_setup(int size);
bool smbcli_parse_unc(const char *unc_name, TALLOC_CTX *mem_ctx,
		      char **hostname, char **sharename);
bool torture_open_connection(struct cli_state **c, int conn_index);
bool torture_cli_session_setup2(struct cli_state *cli, uint16 *new_vuid);
bool torture_close_connection(struct cli_state *c);
bool torture_ioctl_test(int dummy);
bool torture_chkpath_test(int dummy);
NTSTATUS torture_setup_unix_extensions(struct cli_state *cli);

/* The following definitions come from torture/utable.c  */

bool torture_utable(int dummy);
bool torture_casetable(int dummy);

/*
 * Misc
 */

bool run_posix_append(int dummy);

bool run_nbench2(int dummy);
bool run_async_echo(int dummy);
bool run_smb_any_connect(int dummy);
bool run_addrchange(int dummy);

#endif /* __TORTURE_H__ */
