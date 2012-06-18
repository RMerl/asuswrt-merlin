/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
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
#include "auth/credentials/credentials.h"
#include "torture/torture.h"

static bool test_init(struct torture_context *tctx)
{
	struct cli_credentials *creds = cli_credentials_init(tctx);

	cli_credentials_set_domain(creds, "bla", CRED_SPECIFIED);

	torture_assert_str_equal(tctx, "BLA", cli_credentials_get_domain(creds),
				 "domain");

	cli_credentials_set_username(creds, "someuser", CRED_SPECIFIED);

	torture_assert_str_equal(tctx, "someuser", 
				 cli_credentials_get_username(creds), 
				 "username");

	cli_credentials_set_password(creds, "p4ssw0rd", CRED_SPECIFIED);

	torture_assert_str_equal(tctx, "p4ssw0rd", 
				 cli_credentials_get_password(creds),
				 "password");

	return true;
}

static bool test_init_anonymous(struct torture_context *tctx)
{
	struct cli_credentials *creds = cli_credentials_init_anon(tctx);

	torture_assert_str_equal(tctx, cli_credentials_get_domain(creds),
				 "", "domain");

	torture_assert_str_equal(tctx, cli_credentials_get_username(creds),
				 "", "username");

	torture_assert(tctx, cli_credentials_get_password(creds) == NULL,
				 "password");

	return true;
}

static bool test_parse_string(struct torture_context *tctx)
{
	struct cli_credentials *creds = cli_credentials_init_anon(tctx);

	/* anonymous */
	cli_credentials_parse_string(creds, "%", CRED_SPECIFIED);

	torture_assert_str_equal(tctx, cli_credentials_get_domain(creds),
				 "", "domain");

	torture_assert_str_equal(tctx, cli_credentials_get_username(creds),
				 "", "username");

	torture_assert(tctx, cli_credentials_get_password(creds) == NULL,
				 "password");

	/* username + password */
	cli_credentials_parse_string(creds, "somebody%secret", 
				     CRED_SPECIFIED);

	torture_assert_str_equal(tctx, cli_credentials_get_domain(creds),
				 "", "domain");

	torture_assert_str_equal(tctx, cli_credentials_get_username(creds),
				 "somebody", "username");

	torture_assert_str_equal(tctx, cli_credentials_get_password(creds),
				 "secret", "password");

	/* principal */
	cli_credentials_parse_string(creds, "prin@styx", 
				     CRED_SPECIFIED);

	torture_assert_str_equal(tctx, cli_credentials_get_realm(creds),
				 "STYX", "realm");

	torture_assert_str_equal(tctx, 
				 cli_credentials_get_principal(creds, tctx),
				 "prin@styx", "principal");

	return true;
}

struct torture_suite *torture_local_credentials(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, 
							   "CREDENTIALS");

	torture_suite_add_simple_test(suite, "init", test_init);
	torture_suite_add_simple_test(suite, "init anonymous", 
				      test_init_anonymous);
	torture_suite_add_simple_test(suite, "parse_string", 
				      test_parse_string);

	return suite;
}

