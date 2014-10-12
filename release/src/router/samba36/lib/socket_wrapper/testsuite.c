/* 
   Unix SMB/CIFS implementation.

   local testing of the socket wrapper

   Copyright (C) Jelmer Vernooij 2007
   
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
#include "system/network.h"
#include "../socket_wrapper/socket_wrapper.h"
#include "torture/torture.h"

static char *old_dir = NULL;
static char *old_iface = NULL;

static void backup_env(void)
{
	old_dir = getenv("SOCKET_WRAPPER_DIR");
	old_iface = getenv("SOCKET_WRAPPER_DEFAULT_IFACE");
}

static void restore_env(void)
{
	if (old_dir == NULL)
		unsetenv("SOCKET_WRAPPER_DIR");
	else
		setenv("SOCKET_WRAPPER_DIR", old_dir, 1);
	if (old_iface == NULL)
		unsetenv("SOCKET_WRAPPER_DEFAULT_IFACE");
	else
		setenv("SOCKET_WRAPPER_DEFAULT_IFACE", old_iface, 1);
}

static bool test_socket_wrapper_dir(struct torture_context *tctx)
{
	backup_env();

	setenv("SOCKET_WRAPPER_DIR", "foo", 1);
	torture_assert_str_equal(tctx, socket_wrapper_dir(), "foo", "setting failed");
	setenv("SOCKET_WRAPPER_DIR", "./foo", 1);
	torture_assert_str_equal(tctx, socket_wrapper_dir(), "foo", "setting failed");
	unsetenv("SOCKET_WRAPPER_DIR");
	torture_assert_str_equal(tctx, socket_wrapper_dir(), NULL, "resetting failed");

	restore_env();

	return true;
}

static bool test_swrap_socket(struct torture_context *tctx)
{
	backup_env();
	setenv("SOCKET_WRAPPER_DIR", "foo", 1);

	torture_assert_int_equal(tctx, swrap_socket(1337, 1337, 0), -1, "unknown address family fails");
	torture_assert_int_equal(tctx, errno, EAFNOSUPPORT, "correct errno set");
	torture_assert_int_equal(tctx, swrap_socket(AF_INET, 1337, 0), -1, "unknown type fails");
	torture_assert_int_equal(tctx, errno, EPROTONOSUPPORT, "correct errno set");
	torture_assert_int_equal(tctx, swrap_socket(AF_INET, SOCK_DGRAM, 10), -1, "unknown protocol fails");
	torture_assert_int_equal(tctx, errno, EPROTONOSUPPORT, "correct errno set");

	restore_env();

	return true;
}

unsigned int socket_wrapper_default_iface(void);
static bool test_socket_wrapper_default_iface(struct torture_context *tctx)
{
	backup_env();
	unsetenv("SOCKET_WRAPPER_DEFAULT_IFACE");
	torture_assert_int_equal(tctx, socket_wrapper_default_iface(), 1, "unset");
	setenv("SOCKET_WRAPPER_DEFAULT_IFACE", "2", 1);
	torture_assert_int_equal(tctx, socket_wrapper_default_iface(), 2, "unset");
	setenv("SOCKET_WRAPPER_DEFAULT_IFACE", "bla", 1);
	torture_assert_int_equal(tctx, socket_wrapper_default_iface(), 1, "unset");
	restore_env();
	return true;
}

struct torture_suite *torture_local_socket_wrapper(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, 
													   "socket-wrapper");

	torture_suite_add_simple_test(suite, "socket_wrapper_dir", test_socket_wrapper_dir);
	torture_suite_add_simple_test(suite, "socket", test_swrap_socket);
	torture_suite_add_simple_test(suite, "socket_wrapper_default_iface", test_socket_wrapper_default_iface);

	return suite;
}
