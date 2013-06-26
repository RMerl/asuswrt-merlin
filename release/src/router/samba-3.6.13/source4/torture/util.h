/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
   
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

#ifndef _TORTURE_UTIL_H_
#define _TORTURE_UTIL_H_

#include "lib/torture/torture.h"

struct smbcli_state;
struct smbcli_tree;

/**
 * Useful target macros for handling server bugs in torture tests.
 */
#define TARGET_IS_WINXP(_tctx) (torture_setting_bool(_tctx, "winxp", false))
#define TARGET_IS_W2K3(_tctx) (torture_setting_bool(_tctx, "w2k3", false))
#define TARGET_IS_W2K8(_tctx) (torture_setting_bool(_tctx, "w2k8", false))
#define TARGET_IS_WIN7(_tctx) (torture_setting_bool(_tctx, "win7", false))
#define TARGET_IS_SAMBA3(_tctx) (torture_setting_bool(_tctx, "samba3", false))
#define TARGET_IS_SAMBA4(_tctx) (torture_setting_bool(_tctx, "samba4", false))

/**
  setup a directory ready for a test
*/
_PUBLIC_ bool torture_setup_dir(struct smbcli_state *cli, const char *dname);
NTSTATUS create_directory_handle(struct smbcli_tree *tree, const char *dname, int *fnum);

/**
  sometimes we need a fairly complex file to work with, so we can test
  all possible attributes. 
*/
_PUBLIC_ int create_complex_file(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *fname);
int create_complex_dir(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *dname);
void *shm_setup(int size);

/**
  check that a wire string matches the flags specified 
  not 100% accurate, but close enough for testing
*/
bool wire_bad_flags(struct smb_wire_string *str, int flags, 
		    struct smbcli_transport *transport);
void dump_all_info(TALLOC_CTX *mem_ctx, union smb_fileinfo *finfo);
void torture_all_info(struct smbcli_tree *tree, const char *fname);
bool torture_set_file_attribute(struct smbcli_tree *tree, const char *fname, uint16_t attrib);
NTSTATUS torture_set_sparse(struct smbcli_tree *tree, int fnum);
NTSTATUS torture_check_ea(struct smbcli_state *cli, 
			  const char *fname, const char *eaname, const char *value);
_PUBLIC_ bool torture_open_connection_share(TALLOC_CTX *mem_ctx,
				   struct smbcli_state **c, 
				   struct torture_context *tctx,
				   const char *hostname, 
				   const char *sharename,
				   struct tevent_context *ev);
_PUBLIC_ bool torture_get_conn_index(int conn_index,
				     TALLOC_CTX *mem_ctx,
				     struct torture_context *tctx,
				     char **host, char **share);
_PUBLIC_ bool torture_open_connection_ev(struct smbcli_state **c,
					 int conn_index,
					 struct torture_context *tctx,
					 struct tevent_context *ev);
_PUBLIC_ bool torture_open_connection(struct smbcli_state **c, struct torture_context *tctx, int conn_index);
_PUBLIC_ bool torture_close_connection(struct smbcli_state *c);
_PUBLIC_ bool check_error(const char *location, struct smbcli_state *c, 
		 uint8_t eclass, uint32_t ecode, NTSTATUS nterr);
double torture_create_procs(struct torture_context *tctx, 
							bool (*fn)(struct torture_context *, struct smbcli_state *, int), bool *result);
_PUBLIC_ struct torture_test *torture_suite_add_smb_multi_test(
									struct torture_suite *suite,
									const char *name,
									bool (*run) (struct torture_context *,
												 struct smbcli_state *,
												int i));
_PUBLIC_ struct torture_test *torture_suite_add_2smb_test(
									struct torture_suite *suite,
									const char *name,
									bool (*run) (struct torture_context *,
												struct smbcli_state *,
												struct smbcli_state *));
_PUBLIC_ struct torture_test *torture_suite_add_1smb_test(
				struct torture_suite *suite,
				const char *name,
				bool (*run) (struct torture_context *, struct smbcli_state *));
NTSTATUS torture_second_tcon(TALLOC_CTX *mem_ctx,
			     struct smbcli_session *session,
			     const char *sharename,
			     struct smbcli_tree **res);


NTSTATUS torture_check_privilege(struct smbcli_state *cli, 
				 const char *sid_str,
				 const char *privilege);


#endif /* _TORTURE_UTIL_H_ */
