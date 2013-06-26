/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Stefan Metzmacher 2007

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
#include "torture/winbind/proto.h"

NTSTATUS torture_winbind_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "winbind");

	torture_suite_add_suite(suite, torture_winbind_struct_init());
	torture_suite_add_suite(suite, torture_wbclient());

	suite->description = talloc_strdup(suite, "WINBIND tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
