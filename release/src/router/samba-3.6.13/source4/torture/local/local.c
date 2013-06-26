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
#include "torture/smbtorture.h"
#include "torture/local/proto.h"
#include "torture/ndr/proto.h"
#include "torture/auth/proto.h"
#include "../lib/crypto/test_proto.h"
#include "lib/registry/tests/proto.h"
#include "lib/replace/replace-test.h"

/* ignore me */ static struct torture_suite *
	(*suite_generators[]) (TALLOC_CTX *mem_ctx) =
{ 
	torture_local_binding_string, 
	torture_ntlmssp, 
	torture_local_messaging, 
	torture_local_irpc, 
	torture_local_util_strlist, 
	torture_local_util_parmlist, 
	torture_local_util_file, 
	torture_local_util_str, 
	torture_local_util_time, 
	torture_local_util_data_blob, 
	torture_local_util_asn1,
	torture_local_util_anonymous_shared,
	torture_local_idtree, 
	torture_local_dlinklist,
	torture_local_genrand, 
	torture_local_iconv,
	torture_local_socket, 
#ifdef SOCKET_WRAPPER
	torture_local_socket_wrapper, 
#endif
#ifdef NSS_WRAPPER
	torture_local_nss_wrapper,
#endif
	torture_pac, 
	torture_local_resolve,
	torture_local_sddl,
	torture_local_ndr, 
	torture_local_tdr, 
	torture_local_share,
	torture_local_loadparm,
	torture_local_charset,
	torture_local_compression,
	torture_local_event, 
	torture_local_torture,
	torture_local_dbspeed, 
	torture_local_credentials,
	torture_ldb,
	torture_dsdb_dn,
	torture_dsdb_syntax,
	torture_registry,
	NULL
};

NTSTATUS torture_local_init(void)
{
	int i;
	struct torture_suite *suite = torture_suite_create(
		talloc_autofree_context(), "local");
	
	torture_suite_add_simple_test(suite, "talloc", torture_local_talloc);
	torture_suite_add_simple_test(suite, "replace", torture_local_replace);
	
	torture_suite_add_simple_test(suite, 
				      "crypto.md4", torture_local_crypto_md4);
	torture_suite_add_simple_test(suite, "crypto.md5", 
				      torture_local_crypto_md5);
	torture_suite_add_simple_test(suite, "crypto.hmacmd5", 
				      torture_local_crypto_hmacmd5);

	for (i = 0; suite_generators[i]; i++)
		torture_suite_add_suite(suite,
					suite_generators[i](talloc_autofree_context()));
	
	suite->description = talloc_strdup(suite, 
					   "Local, Samba-specific tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
