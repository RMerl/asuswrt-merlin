/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
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

#ifndef __TORTURE_NDR_H__
#define __TORTURE_NDR_H__

#include "torture/torture.h"
#include "librpc/ndr/libndr.h"

_PUBLIC_ struct torture_test *_torture_suite_add_ndr_pull_test(
					struct torture_suite *suite, 
					const char *name, ndr_pull_flags_fn_t fn,
					DATA_BLOB db, 
					size_t struct_size,
					int ndr_flags,
					bool (*check_fn) (struct torture_context *, void *data));

#define torture_suite_add_ndr_pull_test(suite,name,data,check_fn) \
		_torture_suite_add_ndr_pull_test(suite, #name, \
			 (ndr_pull_flags_fn_t)ndr_pull_ ## name, data_blob_talloc(suite, data, sizeof(data)), \
			 sizeof(struct name), 0, (bool (*) (struct torture_context *, void *)) check_fn);

#define torture_suite_add_ndr_pull_fn_test(suite,name,data,flags,check_fn) \
		_torture_suite_add_ndr_pull_test(suite, #name "_" #flags, \
			 (ndr_pull_flags_fn_t)ndr_pull_ ## name, data_blob_talloc(suite, data, sizeof(data)), \
			 sizeof(struct name), flags, (bool (*) (struct torture_context *, void *)) check_fn);

#endif /* __TORTURE_NDR_H__ */
