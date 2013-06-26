/*
   Unix SMB/CIFS implementation.

   SMB2 dir list test suite

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Zachary Loafman 2009
   Copyright (C) Aravind Srinivasan 2009

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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"

#include "torture/torture.h"
#include "torture/smb2/proto.h"
#include "torture/util.h"

#include "system/filesys.h"
#include "lib/util/tsort.h"

#define DNAME	"smb2_dir"
#define NFILES	100

struct file_elem {
	char *name;
	bool found;
};

static NTSTATUS populate_tree(struct torture_context *tctx,
			      TALLOC_CTX *mem_ctx,
			      struct smb2_tree *tree,
			      struct file_elem *files,
			      int nfiles,
			      struct smb2_handle *h_out)
{
	struct smb2_create create;
	char **strs = NULL;
	NTSTATUS status;
	bool ret;
	int i;

	smb2_deltree(tree, DNAME);

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_DIR_ALL;
	create.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	create.in.file_attributes = FILE_ATTRIBUTE_DIRECTORY;
	create.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				 NTCREATEX_SHARE_ACCESS_WRITE |
				 NTCREATEX_SHARE_ACCESS_DELETE;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;
	create.in.fname = DNAME;

	status = smb2_create(tree, mem_ctx, &create);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
	*h_out = create.out.file.handle;

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;

	strs = generate_unique_strs(mem_ctx, 8, nfiles);
	if (strs == NULL) {
		status = NT_STATUS_OBJECT_NAME_COLLISION;
		goto done;
	}
	for (i = 0; i < nfiles; i++) {
		files[i].name = strs[i];
		create.in.fname = talloc_asprintf(mem_ctx, "%s\\%s",
		    DNAME, files[i].name);
		status = smb2_create(tree, mem_ctx, &create);
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		smb2_util_close(tree, create.out.file.handle);
	}
 done:
	return status;
}

/*
  test find continue
*/

static bool test_find(struct torture_context *tctx,
		      struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_handle h;
	struct smb2_find f;
	union smb_search_data *d;
	struct file_elem files[NFILES] = {};
	NTSTATUS status;
	bool ret = true;
	unsigned int count;
	int i, j, file_count = 0;

	status = populate_tree(tctx, mem_ctx, tree, files, NFILES, &h);

	ZERO_STRUCT(f);
	f.in.file.handle	= h;
	f.in.pattern		= "*";
	f.in.continue_flags	= SMB2_CONTINUE_FLAG_SINGLE;
	f.in.max_response_size	= 0x100;
	f.in.level              = SMB2_FIND_BOTH_DIRECTORY_INFO;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

		for (i = 0; i < count; i++) {
			bool expected;
			const char *found = d[i].both_directory_info.name.s;

			if (!strcmp(found, ".") || !strcmp(found, ".."))
				continue;

			expected = false;
			for (j = 0; j < NFILES; j++) {
				if (!strcmp(files[j].name, found)) {
					files[j].found = true;
					expected = true;
					break;
				}
			}

			if (expected)
				continue;

			torture_result(tctx, TORTURE_FAIL,
			    "(%s): didn't expect %s\n",
			    __location__, found);
			ret = false;
			goto done;
		}

		file_count = file_count + i;
		f.in.continue_flags = 0;
		f.in.max_response_size	= 4096;
	} while (count != 0);

	torture_assert_int_equal_goto(tctx, file_count, NFILES + 2, ret, done,
				      "");

	for (i = 0; i < NFILES; i++) {
		if (files[j].found)
			continue;

		torture_result(tctx, TORTURE_FAIL,
		    "(%s): expected to find %s, but didn't\n",
		    __location__, files[j].name);
		ret = false;
		goto done;
	}

 done:
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

/*
  test fixed enumeration
*/

static bool test_fixed(struct torture_context *tctx,
		       struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create create;
	struct smb2_handle h, h2;
	struct smb2_find f;
	union smb_search_data *d;
	struct file_elem files[NFILES] = {};
	NTSTATUS status;
	bool ret = true;
	unsigned int count;
	int i;

	status = populate_tree(tctx, mem_ctx, tree, files, NFILES, &h);

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_DIR_ALL;
	create.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	create.in.file_attributes = FILE_ATTRIBUTE_DIRECTORY;
	create.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				 NTCREATEX_SHARE_ACCESS_WRITE |
				 NTCREATEX_SHARE_ACCESS_DELETE;
	create.in.create_disposition = NTCREATEX_DISP_OPEN;
	create.in.fname = DNAME;

	status = smb2_create(tree, mem_ctx, &create);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
	h2 = create.out.file.handle;

	ZERO_STRUCT(f);
	f.in.file.handle	= h;
	f.in.pattern		= "*";
	f.in.continue_flags	= SMB2_CONTINUE_FLAG_SINGLE;
	f.in.max_response_size	= 0x100;
	f.in.level              = SMB2_FIND_BOTH_DIRECTORY_INFO;

	/* Start enumeration on h, then delete all from h2 */
	status = smb2_find_level(tree, tree, &f, &count, &d);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	f.in.file.handle	= h2;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

		for (i = 0; i < count; i++) {
			const char *found = d[i].both_directory_info.name.s;
			char *path = talloc_asprintf(mem_ctx, "%s\\%s",
			    DNAME, found);

			if (!strcmp(found, ".") || !strcmp(found, ".."))
				continue;

			status = smb2_util_unlink(tree, path);
			torture_assert_ntstatus_ok_goto(tctx, status, ret, done,
							"");

			talloc_free(path);
		}

		f.in.continue_flags = 0;
		f.in.max_response_size	= 4096;
	} while (count != 0);

	/* Now finish h enumeration. */
	f.in.file.handle = h;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

		for (i = 0; i < count; i++) {
			const char *found = d[i].both_directory_info.name.s;

			if (!strcmp(found, ".") || !strcmp(found, ".."))
				continue;

			torture_result(tctx, TORTURE_FAIL,
				       "(%s): didn't expect %s (count=%u)\n",
				       __location__, found, count);
			ret = false;
			goto done;
		}

		f.in.continue_flags = 0;
		f.in.max_response_size	= 4096;
	} while (count != 0);

 done:
	smb2_util_close(tree, h);
	smb2_util_close(tree, h2);
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

static struct {
	const char *name;
	uint8_t level;
	enum smb_search_data_level data_level;
	int name_offset;
	int resume_key_offset;
	uint32_t capability_mask;
	NTSTATUS status;
	union smb_search_data data;
} levels[] = {
	{"SMB2_FIND_DIRECTORY_INFO",
	 SMB2_FIND_DIRECTORY_INFO, RAW_SEARCH_DATA_DIRECTORY_INFO,
	 offsetof(union smb_search_data, directory_info.name.s),
	 offsetof(union smb_search_data, directory_info.file_index),
	},
	{"SMB2_FIND_FULL_DIRECTORY_INFO",
	 SMB2_FIND_FULL_DIRECTORY_INFO, RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,
	 offsetof(union smb_search_data, full_directory_info.name.s),
	 offsetof(union smb_search_data, full_directory_info.file_index),
	},
	{"SMB2_FIND_NAME_INFO",
	 SMB2_FIND_NAME_INFO, RAW_SEARCH_DATA_NAME_INFO,
	 offsetof(union smb_search_data, name_info.name.s),
	 offsetof(union smb_search_data, name_info.file_index),
	},
	{"SMB2_FIND_BOTH_DIRECTORY_INFO",
	 SMB2_FIND_BOTH_DIRECTORY_INFO, RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,
	 offsetof(union smb_search_data, both_directory_info.name.s),
	 offsetof(union smb_search_data, both_directory_info.file_index),
	},
	{"SMB2_FIND_ID_FULL_DIRECTORY_INFO",
	 SMB2_FIND_ID_FULL_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO,
	 offsetof(union smb_search_data, id_full_directory_info.name.s),
	 offsetof(union smb_search_data, id_full_directory_info.file_index),
	},
	{"SMB2_FIND_ID_BOTH_DIRECTORY_INFO",
	 SMB2_FIND_ID_BOTH_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO,
	 offsetof(union smb_search_data, id_both_directory_info.name.s),
	 offsetof(union smb_search_data, id_both_directory_info.file_index),
	}
};

/*
  extract the name from a smb_data structure and level
*/
static const char *extract_name(union smb_search_data *data,
				uint8_t level,
				enum smb_search_data_level data_level)
{
	int i;
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (level == levels[i].level &&
		    data_level == levels[i].data_level) {
			return *(const char **)(levels[i].name_offset + (char *)data);
		}
	}
	return NULL;
}

/* find a level in the table by name */
static union smb_search_data *find(const char *name)
{
	int i;
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (NT_STATUS_IS_OK(levels[i].status) &&
		    strcmp(levels[i].name, name) == 0) {
			return &levels[i].data;
		}
	}
	return NULL;
}

static bool fill_level_data(TALLOC_CTX *mem_ctx,
			    union smb_search_data *data,
			    union smb_search_data *d,
			    unsigned int count,
			    uint8_t level,
			    enum smb_search_data_level data_level)
{
	int i;
	const char *sname = NULL;
	for (i=0; i < count ; i++) {
		sname = extract_name(&d[i], level, data_level);
		if (sname == NULL)
			return false;
		if (!strcmp(sname, ".") || !strcmp(sname, ".."))
			continue;
		*data = d[i];
	}
	return true;
}


NTSTATUS torture_single_file_search(struct smb2_tree *tree,
				    TALLOC_CTX *mem_ctx,
				    const char *pattern,
				    uint8_t level,
				    enum smb_search_data_level data_level,
				    int idx,
				    union smb_search_data *d,
				    unsigned int *count,
				    struct smb2_handle *h)
{
	struct smb2_find f;
	NTSTATUS status;

	ZERO_STRUCT(f);
	f.in.file.handle        = *h;
	f.in.pattern            = pattern;
	f.in.continue_flags     = SMB2_CONTINUE_FLAG_RESTART;
	f.in.max_response_size  = 0x100;
	f.in.level              = level;

	status = smb2_find_level(tree, tree, &f, count, &d);
	if (NT_STATUS_IS_OK(status))
		fill_level_data(mem_ctx, &levels[idx].data, d, *count, level,
				data_level);
	return status;
}

/*
   basic testing of all File Information Classes using a single file
*/
static bool test_one_file(struct torture_context *tctx,
			  struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	bool ret = true;
	const char *fname =  "torture_search.txt";
	NTSTATUS status;
	int i;
	unsigned int count;
	union smb_fileinfo all_info2, alt_info, internal_info;
	union smb_search_data *s;
	union smb_search_data d;
	struct smb2_handle h, h2;

	status = torture_smb2_testdir(tree, DNAME, &h);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	status = smb2_create_complex_file(tree, DNAME "\\torture_search.txt",
					  &h2);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	/* call all the File Information Classes */
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing %s %d\n", levels[i].name,
				levels[i].level);

		levels[i].status = torture_single_file_search(tree, mem_ctx,
				   fname, levels[i].level, levels[i].data_level,
				   i, &d, &count, &h);
		torture_assert_ntstatus_ok_goto(tctx, levels[i].status, ret,
						done, "");
	}

	/* get the all_info file into to check against */
	all_info2.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	all_info2.generic.in.file.handle = h2;
	status = smb2_getinfo_file(tree, tctx, &all_info2);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done,
					"RAW_FILEINFO_ALL_INFO failed");

	alt_info.generic.level = RAW_FILEINFO_ALT_NAME_INFORMATION;
	alt_info.generic.in.file.handle = h2;
	status = smb2_getinfo_file(tree, tctx, &alt_info);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done,
					"RAW_FILEINFO_ALT_NAME_INFO failed");

	internal_info.generic.level = RAW_FILEINFO_INTERNAL_INFORMATION;
	internal_info.generic.in.file.handle = h2;
	status = smb2_getinfo_file(tree, tctx, &internal_info);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done,
					"RAW_FILEINFO_INTERNAL_INFORMATION "
				        "failed");

#define CHECK_VAL(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if ((s->sname1.field1) != (v.sname2.out.field2)) { \
			torture_result(tctx, TORTURE_FAIL, \
			    "(%s) %s/%s [0x%x] != %s/%s [0x%x]\n", \
			    __location__, \
			    #sname1, #field1, (int)s->sname1.field1, \
			    #sname2, #field2, (int)v.sname2.out.field2); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_TIME(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if (s->sname1.field1 != \
		    (~1 & nt_time_to_unix(v.sname2.out.field2))) { \
			torture_result(tctx, TORTURE_FAIL, \
			    "(%s) %s/%s [%s] != %s/%s [%s]\n", \
			    __location__, \
			    #sname1, #field1, \
			    timestring(tctx, s->sname1.field1), \
			    #sname2, #field2, \
			    nt_time_string(tctx, v.sname2.out.field2)); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_NTTIME(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if (s->sname1.field1 != v.sname2.out.field2) { \
			torture_result(tctx, TORTURE_FAIL, \
			    "(%s) %s/%s [%s] != %s/%s [%s]\n", \
			    __location__, \
			    #sname1, #field1, \
			    nt_time_string(tctx, s->sname1.field1), \
			    #sname2, #field2, \
			    nt_time_string(tctx, v.sname2.out.field2)); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_STR(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if (!s->sname1.field1 || \
		    strcmp(s->sname1.field1, v.sname2.out.field2.s)) { \
			torture_result(tctx, TORTURE_FAIL, \
			    "(%s) %s/%s [%s] != %s/%s [%s]\n", \
			    __location__, \
			    #sname1, #field1, s->sname1.field1, \
			    #sname2, #field2, v.sname2.out.field2.s); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_WSTR(name, sname1, field1, v, sname2, field2, flags) do { \
	s = find(name); \
	if (s) { \
		if (!s->sname1.field1.s || \
		    strcmp(s->sname1.field1.s, v.sname2.out.field2.s)) { \
			torture_result(tctx, TORTURE_FAIL, \
			    "(%s) %s/%s [%s] != %s/%s [%s]\n", \
			    __location__, \
			    #sname1, #field1, s->sname1.field1.s, \
			    #sname2, #field2, v.sname2.out.field2.s); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_NAME(name, sname1, field1, fname, flags) do { \
	s = find(name); \
	if (s) { \
		if (!s->sname1.field1.s || \
		    strcmp(s->sname1.field1.s, fname)) { \
			torture_result(tctx, TORTURE_FAIL, \
			    "(%s) %s/%s [%s] != %s\n", \
			    __location__, \
			    #sname1, #field1, s->sname1.field1.s, fname); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_UNIX_NAME(name, sname1, field1, fname, flags) do { \
	s = find(name); \
	if (s) { \
		if (!s->sname1.field1 || \
		    strcmp(s->sname1.field1, fname)) { \
			torture_result(tctx, TORTURE_FAIL, \
			   "(%s) %s/%s [%s] != %s\n", \
			    __location__, \
			    #sname1, #field1, s->sname1.field1, fname); \
			ret = false; \
		} \
	}} while (0)

	/* check that all the results are as expected */
	CHECK_VAL("SMB2_FIND_DIRECTORY_INFO",            directory_info,         attrib, all_info2, all_info2, attrib);
	CHECK_VAL("SMB2_FIND_FULL_DIRECTORY_INFO",       full_directory_info,    attrib, all_info2, all_info2, attrib);
	CHECK_VAL("SMB2_FIND_BOTH_DIRECTORY_INFO",       both_directory_info,    attrib, all_info2, all_info2, attrib);
	CHECK_VAL("SMB2_FIND_ID_FULL_DIRECTORY_INFO",    id_full_directory_info, attrib, all_info2, all_info2, attrib);
	CHECK_VAL("SMB2_FIND_ID_BOTH_DIRECTORY_INFO",    id_both_directory_info, attrib, all_info2, all_info2, attrib);

	CHECK_NTTIME("SMB2_FIND_DIRECTORY_INFO",         directory_info,         write_time, all_info2, all_info2, write_time);
	CHECK_NTTIME("SMB2_FIND_FULL_DIRECTORY_INFO",    full_directory_info,    write_time, all_info2, all_info2, write_time);
	CHECK_NTTIME("SMB2_FIND_BOTH_DIRECTORY_INFO",    both_directory_info,    write_time, all_info2, all_info2, write_time);
	CHECK_NTTIME("SMB2_FIND_ID_FULL_DIRECTORY_INFO", id_full_directory_info, write_time, all_info2, all_info2, write_time);
	CHECK_NTTIME("SMB2_FIND_ID_BOTH_DIRECTORY_INFO", id_both_directory_info, write_time, all_info2, all_info2, write_time);

	CHECK_NTTIME("SMB2_FIND_DIRECTORY_INFO",         directory_info,         create_time, all_info2, all_info2, create_time);
	CHECK_NTTIME("SMB2_FIND_FULL_DIRECTORY_INFO",    full_directory_info,    create_time, all_info2, all_info2, create_time);
	CHECK_NTTIME("SMB2_FIND_BOTH_DIRECTORY_INFO",    both_directory_info,    create_time, all_info2, all_info2, create_time);
	CHECK_NTTIME("SMB2_FIND_ID_FULL_DIRECTORY_INFO", id_full_directory_info, create_time, all_info2, all_info2, create_time);
	CHECK_NTTIME("SMB2_FIND_ID_BOTH_DIRECTORY_INFO", id_both_directory_info, create_time, all_info2, all_info2, create_time);

	CHECK_NTTIME("SMB2_FIND_DIRECTORY_INFO",         directory_info,         access_time, all_info2, all_info2, access_time);
	CHECK_NTTIME("SMB2_FIND_FULL_DIRECTORY_INFO",    full_directory_info,    access_time, all_info2, all_info2, access_time);
	CHECK_NTTIME("SMB2_FIND_BOTH_DIRECTORY_INFO",    both_directory_info,    access_time, all_info2, all_info2, access_time);
	CHECK_NTTIME("SMB2_FIND_ID_FULL_DIRECTORY_INFO", id_full_directory_info, access_time, all_info2, all_info2, access_time);
	CHECK_NTTIME("SMB2_FIND_ID_BOTH_DIRECTORY_INFO", id_both_directory_info, access_time, all_info2, all_info2, access_time);

	CHECK_NTTIME("SMB2_FIND_DIRECTORY_INFO",         directory_info,         change_time, all_info2, all_info2, change_time);
	CHECK_NTTIME("SMB2_FIND_FULL_DIRECTORY_INFO",    full_directory_info,    change_time, all_info2, all_info2, change_time);
	CHECK_NTTIME("SMB2_FIND_BOTH_DIRECTORY_INFO",    both_directory_info,    change_time, all_info2, all_info2, change_time);
	CHECK_NTTIME("SMB2_FIND_ID_FULL_DIRECTORY_INFO", id_full_directory_info, change_time, all_info2, all_info2, change_time);
	CHECK_NTTIME("SMB2_FIND_ID_BOTH_DIRECTORY_INFO", id_both_directory_info, change_time, all_info2, all_info2, change_time);

	CHECK_VAL("SMB2_FIND_DIRECTORY_INFO",            directory_info,         size, all_info2, all_info2, size);
	CHECK_VAL("SMB2_FIND_FULL_DIRECTORY_INFO",       full_directory_info,    size, all_info2, all_info2, size);
	CHECK_VAL("SMB2_FIND_BOTH_DIRECTORY_INFO",       both_directory_info,    size, all_info2, all_info2, size);
	CHECK_VAL("SMB2_FIND_ID_FULL_DIRECTORY_INFO",    id_full_directory_info, size, all_info2, all_info2, size);
	CHECK_VAL("SMB2_FIND_ID_BOTH_DIRECTORY_INFO",    id_both_directory_info, size, all_info2, all_info2, size);

	CHECK_VAL("SMB2_FIND_DIRECTORY_INFO",            directory_info,         alloc_size, all_info2, all_info2, alloc_size);
	CHECK_VAL("SMB2_FIND_FULL_DIRECTORY_INFO",       full_directory_info,    alloc_size, all_info2, all_info2, alloc_size);
	CHECK_VAL("SMB2_FIND_BOTH_DIRECTORY_INFO",       both_directory_info,    alloc_size, all_info2, all_info2, alloc_size);
	CHECK_VAL("SMB2_FIND_ID_FULL_DIRECTORY_INFO",    id_full_directory_info, alloc_size, all_info2, all_info2, alloc_size);
	CHECK_VAL("SMB2_FIND_ID_BOTH_DIRECTORY_INFO",    id_both_directory_info, alloc_size, all_info2, all_info2, alloc_size);

	CHECK_VAL("SMB2_FIND_FULL_DIRECTORY_INFO",       full_directory_info,    ea_size, all_info2, all_info2, ea_size);
	CHECK_VAL("SMB2_FIND_BOTH_DIRECTORY_INFO",       both_directory_info,    ea_size, all_info2, all_info2, ea_size);
	CHECK_VAL("SMB2_FIND_ID_FULL_DIRECTORY_INFO",    id_full_directory_info, ea_size, all_info2, all_info2, ea_size);
	CHECK_VAL("SMB2_FIND_ID_BOTH_DIRECTORY_INFO",    id_both_directory_info, ea_size, all_info2, all_info2, ea_size);

	CHECK_NAME("SMB2_FIND_DIRECTORY_INFO",           directory_info,         name, fname, STR_TERMINATE_ASCII);
	CHECK_NAME("SMB2_FIND_FULL_DIRECTORY_INFO",      full_directory_info,    name, fname, STR_TERMINATE_ASCII);
	CHECK_NAME("SMB2_FIND_NAME_INFO",                name_info,              name, fname, STR_TERMINATE_ASCII);
	CHECK_NAME("SMB2_FIND_BOTH_DIRECTORY_INFO",      both_directory_info,    name, fname, STR_TERMINATE_ASCII);
	CHECK_NAME("SMB2_FIND_ID_FULL_DIRECTORY_INFO",   id_full_directory_info, name, fname, STR_TERMINATE_ASCII);
	CHECK_NAME("SMB2_FIND_ID_BOTH_DIRECTORY_INFO",   id_both_directory_info, name, fname, STR_TERMINATE_ASCII);

	CHECK_WSTR("SMB2_FIND_BOTH_DIRECTORY_INFO",      both_directory_info,    short_name, alt_info, alt_name_info, fname, STR_UNICODE);

	CHECK_VAL("SMB2_FIND_ID_FULL_DIRECTORY_INFO",    id_full_directory_info, file_id, internal_info, internal_information, file_id);
	CHECK_VAL("SMB2_FIND_ID_BOTH_DIRECTORY_INFO",    id_both_directory_info, file_id, internal_info, internal_information, file_id);

done:
	smb2_util_close(tree, h);
	smb2_util_unlink(tree, fname);
	talloc_free(mem_ctx);

	return ret;
}


struct multiple_result {
	TALLOC_CTX *tctx;
	int count;
	union smb_search_data *list;
};

bool fill_result(void *private_data,
		 union smb_search_data *file,
		 int count,
		 uint8_t level,
		 enum smb_search_data_level data_level)
{
	int i;
	const char *sname;
	struct multiple_result *data = (struct multiple_result *)private_data;

	for (i=0; i<count; i++) {
		sname = extract_name(&file[i], level, data_level);
		if (!strcmp(sname, ".") || !(strcmp(sname, "..")))
			continue;
		data->count++;
		data->list = talloc_realloc(data->tctx,
					    data->list,
					    union smb_search_data,
					    data->count);
		data->list[data->count-1] = file[i];
	}
	return true;
}

enum continue_type {CONT_SINGLE, CONT_INDEX, CONT_RESTART};

static NTSTATUS multiple_smb2_search(struct smb2_tree *tree,
				     TALLOC_CTX *tctx,
				     const char *pattern,
				     uint8_t level,
				     enum smb_search_data_level data_level,
				     enum continue_type cont_type,
				     void *data,
				     struct smb2_handle *h)
{
	struct smb2_find f;
	bool ret = true;
	unsigned int count = 0;
	union smb_search_data *d;
	NTSTATUS status;
	struct multiple_result *result = (struct multiple_result *)data;

	ZERO_STRUCT(f);
	f.in.file.handle        = *h;
	f.in.pattern            = pattern;
	f.in.max_response_size  = 0x1000;
	f.in.level              = level;

	/* The search should start from the beginning everytime */
	f.in.continue_flags = SMB2_CONTINUE_FLAG_RESTART;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		if (!fill_result(result, d, count, level, data_level)) {
			return NT_STATUS_UNSUCCESSFUL;
		}

		/*
		 * After the first iteration is complete set the CONTINUE
		 * FLAGS appropriately
		 */
		switch (cont_type) {
			case CONT_INDEX:
				f.in.continue_flags = SMB2_CONTINUE_FLAG_INDEX;
				break;
			case CONT_SINGLE:
				f.in.continue_flags = SMB2_CONTINUE_FLAG_SINGLE;
				break;
			case CONT_RESTART:
			default:
				/* we should prevent staying in the loop
				 * forever */
				f.in.continue_flags = 0;
				break;
		}
	} while (count != 0);
done:
	return status;
}


static enum smb_search_data_level compare_data_level;
uint8_t level_sort;

static int search_compare(union smb_search_data *d1,
			  union smb_search_data *d2)
{
	const char *s1, *s2;

	s1 = extract_name(d1, level_sort, compare_data_level);
	s2 = extract_name(d2, level_sort, compare_data_level);
	return strcmp_safe(s1, s2);
}

/*
   basic testing of search calls using many files
*/
static bool test_many_files(struct torture_context *tctx,
			    struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	const int num_files = 700;
	int i, t;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;
	struct smb2_create create;
	struct smb2_handle h;
	struct {
		const char *name;
		const char *cont_name;
		uint8_t level;
		enum smb_search_data_level data_level;
		enum continue_type cont_type;
	} search_types[] = {
		{"SMB2_FIND_BOTH_DIRECTORY_INFO",    "SINGLE",  SMB2_FIND_BOTH_DIRECTORY_INFO,    RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,    CONT_SINGLE},
		{"SMB2_FIND_BOTH_DIRECTORY_INFO",    "INDEX",   SMB2_FIND_BOTH_DIRECTORY_INFO,    RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,    CONT_INDEX},
		{"SMB2_FIND_BOTH_DIRECTORY_INFO",    "RESTART", SMB2_FIND_BOTH_DIRECTORY_INFO,    RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,    CONT_RESTART},
		{"SMB2_FIND_DIRECTORY_INFO",         "SINGLE",  SMB2_FIND_DIRECTORY_INFO,         RAW_SEARCH_DATA_DIRECTORY_INFO,         CONT_SINGLE},
		{"SMB2_FIND_DIRECTORY_INFO",         "INDEX",   SMB2_FIND_DIRECTORY_INFO,         RAW_SEARCH_DATA_DIRECTORY_INFO,         CONT_INDEX},
		{"SMB2_FIND_DIRECTORY_INFO",         "RESTART", SMB2_FIND_DIRECTORY_INFO,         RAW_SEARCH_DATA_DIRECTORY_INFO,         CONT_RESTART},
		{"SMB2_FIND_FULL_DIRECTORY_INFO",    "SINGLE",  SMB2_FIND_FULL_DIRECTORY_INFO,    RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,    CONT_SINGLE},
		{"SMB2_FIND_FULL_DIRECTORY_INFO",    "INDEX",   SMB2_FIND_FULL_DIRECTORY_INFO,    RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,    CONT_INDEX},
		{"SMB2_FIND_FULL_DIRECTORY_INFO",    "RESTART", SMB2_FIND_FULL_DIRECTORY_INFO,    RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,    CONT_RESTART},
		{"SMB2_FIND_ID_FULL_DIRECTORY_INFO", "SINGLE",  SMB2_FIND_ID_FULL_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, CONT_SINGLE},
		{"SMB2_FIND_ID_FULL_DIRECTORY_INFO", "INDEX",   SMB2_FIND_ID_FULL_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, CONT_INDEX},
		{"SMB2_FIND_ID_FULL_DIRECTORY_INFO", "RESTART", SMB2_FIND_ID_FULL_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, CONT_RESTART},
		{"SMB2_FIND_ID_BOTH_DIRECTORY_INFO", "SINGLE",  SMB2_FIND_ID_BOTH_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, CONT_SINGLE},
		{"SMB2_FIND_ID_BOTH_DIRECTORY_INFO", "INDEX",   SMB2_FIND_ID_BOTH_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, CONT_INDEX},
		{"SMB2_FIND_ID_BOTH_DIRECTORY_INFO", "RESTART", SMB2_FIND_ID_BOTH_DIRECTORY_INFO, RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, CONT_RESTART}
	};

	smb2_deltree(tree, DNAME);
	status = torture_smb2_testdir(tree, DNAME, &h);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	torture_comment(tctx, "Testing with %d files\n", num_files);
	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;

	for (i=num_files-1;i>=0;i--) {
		fname = talloc_asprintf(mem_ctx, DNAME "\\t%03d-%d.txt", i, i);
		create.in.fname = talloc_asprintf(mem_ctx, "%s", fname);
		status = smb2_create(tree, mem_ctx, &create);
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		smb2_util_close(tree, create.out.file.handle);
		talloc_free(fname);
	}

	for (t=0;t<ARRAY_SIZE(search_types);t++) {
		ZERO_STRUCT(result);
		result.tctx = talloc_new(tctx);

		torture_comment(tctx,
				"Continue %s via %s\n", search_types[t].name,
				search_types[t].cont_name);
		status = multiple_smb2_search(tree, tctx, "*",
					      search_types[t].level,
					      search_types[t].data_level,
					      search_types[t].cont_type,
					      &result, &h);

		torture_assert_int_equal_goto(tctx, result.count, num_files,
					      ret, done, "");

		compare_data_level = search_types[t].data_level;
		level_sort = search_types[t].level;

		TYPESAFE_QSORT(result.list, result.count, search_compare);

		for (i=0;i<result.count;i++) {
			const char *s;
			enum smb_search_level level;
			level = RAW_SEARCH_SMB2;
			s = extract_name(&result.list[i],
					 search_types[t].level,
					 compare_data_level);
			fname = talloc_asprintf(mem_ctx, "t%03d-%d.txt", i, i);
			torture_assert_str_equal_goto(tctx, s, fname, ret,
						      done, "Incorrect name");
			talloc_free(fname);
		}
		talloc_free(result.tctx);
	}

done:
	smb2_util_close(tree, h);
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

/*
  check an individual file result
*/
static bool check_result(struct torture_context *tctx,
			 struct multiple_result *result,
			 const char *name,
			 bool exist,
			 uint32_t attrib)
{
	int i;
	for (i=0;i<result->count;i++) {
		if (strcmp(name,
			   result->list[i].both_directory_info.name.s) == 0) {
			break;
		}
	}
	if (i == result->count) {
		if (exist) {
			torture_result(tctx, TORTURE_FAIL,
			    "failed: '%s' should exist with attribute %s\n",
			    name, attrib_string(result->list, attrib));
			return false;
		}
		return true;
	}

	if (!exist) {
		torture_result(tctx, TORTURE_FAIL,
		    "failed: '%s' should NOT exist (has attribute %s)\n",
		    name, attrib_string(result->list,
		    result->list[i].both_directory_info.attrib));
		return false;
	}

	if ((result->list[i].both_directory_info.attrib&0xFFF) != attrib) {
		torture_result(tctx, TORTURE_FAIL,
		    "failed: '%s' should have attribute 0x%x (has 0x%x)\n",
		    name, attrib, result->list[i].both_directory_info.attrib);
		return false;
	}
	return true;
}

/*
   test what happens when the directory is modified during a search
*/
static bool test_modify_search(struct torture_context *tctx,
			       struct smb2_tree *tree)
{
	int num_files = 700;
	struct multiple_result result;
	union smb_setfileinfo sfinfo;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_create create;
	struct smb2_handle h;
	struct smb2_find f;
	union smb_search_data *d;
	struct file_elem files[700] = {};
	NTSTATUS status;
	bool ret = true;
	int i;
	unsigned int count;

	smb2_deltree(tree, DNAME);

	status = torture_smb2_testdir(tree, DNAME, &h);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	torture_comment(tctx, "Creating %d files\n", num_files);

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;

	for (i = num_files-1; i >= 0; i--) {
		files[i].name = talloc_asprintf(mem_ctx, "t%03d-%d.txt", i, i);
		create.in.fname = talloc_asprintf(mem_ctx, "%s\\%s",
						  DNAME, files[i].name);
		status = smb2_create(tree, mem_ctx, &create);
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		smb2_util_close(tree, create.out.file.handle);
	}

	torture_comment(tctx, "pulling the first two files\n");
	ZERO_STRUCT(result);
	result.tctx = talloc_new(tctx);

	ZERO_STRUCT(f);
	f.in.file.handle        = h;
	f.in.pattern            = "*";
	f.in.continue_flags     = SMB2_CONTINUE_FLAG_SINGLE;
	f.in.max_response_size  = 0x100;
	f.in.level              = SMB2_FIND_BOTH_DIRECTORY_INFO;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		if (!fill_result(&result, d, count, f.in.level,
				 RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO)) {
			ret = false;
			goto done;
		}
	} while (result.count < 2);

	torture_comment(tctx, "Changing attributes and deleting\n");

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;

	files[num_files].name = talloc_asprintf(mem_ctx, "T003-03.txt.2");
	create.in.fname = talloc_asprintf(mem_ctx, "%s\\%s", DNAME,
					  files[num_files].name);
	status = smb2_create(tree, mem_ctx, &create);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
	smb2_util_close(tree, create.out.file.handle);

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;

	files[num_files + 1].name = talloc_asprintf(mem_ctx, "T013-13.txt.2");
	create.in.fname = talloc_asprintf(mem_ctx, "%s\\%s", DNAME,
					  files[num_files + 1].name);
	status = smb2_create(tree, mem_ctx, &create);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
	smb2_util_close(tree, create.out.file.handle);

	files[num_files + 2].name = talloc_asprintf(mem_ctx, "T013-13.txt.3");
	status = smb2_create_complex_file(tree, DNAME "\\T013-13.txt.3", &h);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	smb2_util_unlink(tree, DNAME "\\T014-14.txt");
	smb2_util_setatr(tree, DNAME "\\T015-15.txt", FILE_ATTRIBUTE_HIDDEN);
	smb2_util_setatr(tree, DNAME "\\T016-16.txt", FILE_ATTRIBUTE_NORMAL);
	smb2_util_setatr(tree, DNAME "\\T017-17.txt", FILE_ATTRIBUTE_SYSTEM);
	smb2_util_setatr(tree, DNAME "\\T018-18.txt", 0);
	smb2_util_setatr(tree, DNAME "\\T039-39.txt", FILE_ATTRIBUTE_HIDDEN);
	smb2_util_setatr(tree, DNAME "\\T000-0.txt", FILE_ATTRIBUTE_HIDDEN);
	sfinfo.generic.level = RAW_SFILEINFO_DISPOSITION_INFORMATION;
	sfinfo.generic.in.file.path = DNAME "\\T013-13.txt.3";
	sfinfo.disposition_info.in.delete_on_close = 1;
	status = smb2_composite_setpathinfo(tree, &sfinfo);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	/* Reset the numfiles to include the new files and start the
	 * search from the beginning */
	num_files = num_files + 2;
	f.in.pattern = "*";
	f.in.continue_flags = SMB2_CONTINUE_FLAG_RESTART;
	result.count = 0;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		if (!fill_result(&result, d, count, f.in.level,
				 RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO)) {
			ret = false;
			goto done;
		}
		f.in.continue_flags = 0;
		f.in.max_response_size  = 4096;
	} while (count != 0);


	ret &= check_result(tctx, &result, "t039-39.txt", true, FILE_ATTRIBUTE_HIDDEN);
	ret &= check_result(tctx, &result, "t000-0.txt", true, FILE_ATTRIBUTE_HIDDEN);
	ret &= check_result(tctx, &result, "t014-14.txt", false, 0);
	ret &= check_result(tctx, &result, "t015-15.txt", true, FILE_ATTRIBUTE_HIDDEN);
	ret &= check_result(tctx, &result, "t016-16.txt", true, FILE_ATTRIBUTE_NORMAL);
	ret &= check_result(tctx, &result, "t017-17.txt", true, FILE_ATTRIBUTE_SYSTEM);
	ret &= check_result(tctx, &result, "t018-18.txt", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(tctx, &result, "t019-19.txt", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(tctx, &result, "T013-13.txt.2", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(tctx, &result, "T003-3.txt.2", false, 0);
	ret &= check_result(tctx, &result, "T013-13.txt.3", true, FILE_ATTRIBUTE_NORMAL);

	if (!ret) {
		for (i=0;i<result.count;i++) {
			torture_warning(tctx, "%s %s (0x%x)\n",
			       result.list[i].both_directory_info.name.s,
			       attrib_string(tctx,
			       result.list[i].both_directory_info.attrib),
			       result.list[i].both_directory_info.attrib);
		}
	}
 done:
	smb2_util_close(tree, h);
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

/*
   testing if directories always come back sorted
*/
static bool test_sorted(struct torture_context *tctx,
			struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	const int num_files = 700;
	int i;
	struct file_elem files[700] = {};
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;
	struct smb2_handle h;

	torture_comment(tctx, "Testing if directories always come back "
	   "sorted\n");
	status = populate_tree(tctx, mem_ctx, tree, files, num_files, &h);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	ZERO_STRUCT(result);
	result.tctx = tctx;

	status = multiple_smb2_search(tree, tctx, "*",
				      SMB2_FIND_BOTH_DIRECTORY_INFO,
				      RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,
				      SMB2_CONTINUE_FLAG_SINGLE,
				      &result, &h);

	torture_assert_int_equal_goto(tctx, result.count, num_files, ret, done,
				      "");

	for (i=0;i<num_files-1;i++) {
		const char *name1, *name2;
		name1 = result.list[i].both_directory_info.name.s;
		name2 = result.list[i+1].both_directory_info.name.s;
		if (strcasecmp_m(name1, name2) > 0) {
			torture_comment(tctx, "non-alphabetical order at entry "
			    "%d '%s' '%s'\n", i, name1, name2);
			torture_comment(tctx,
			    "Server does not produce sorted directory listings"
			    "(not an error)\n");
			goto done;
		}
	}
	talloc_free(result.list);
done:
	smb2_util_close(tree, h);
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

/* test the behavior of file_index field in the SMB2_FIND struct */
static bool test_file_index(struct torture_context *tctx,
			    struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	const int num_files = 100;
	int resume_index = 4;
	int i;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;
	struct smb2_create create;
	struct smb2_find f;
	struct smb2_handle h;
	union smb_search_data *d;
	unsigned count;

	smb2_deltree(tree, DNAME);

	status = torture_smb2_testdir(tree, DNAME, &h);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

	torture_comment(tctx, "Testing the behavior of file_index flag\n");

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;
	for (i = num_files-1; i >= 0; i--) {
		fname = talloc_asprintf(mem_ctx, DNAME "\\file%u.txt", i);
		create.in.fname = fname;
		status = smb2_create(tree, mem_ctx, &create);
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		talloc_free(fname);
		smb2_util_close(tree, create.out.file.handle);
	}

	ZERO_STRUCT(result);
	result.tctx = tctx;

	ZERO_STRUCT(f);
	f.in.file.handle        = h;
	f.in.pattern            = "*";
	f.in.continue_flags     = SMB2_CONTINUE_FLAG_SINGLE;
	f.in.max_response_size  = 0x1000;
	f.in.level              = SMB2_FIND_FULL_DIRECTORY_INFO;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		if (!fill_result(&result, d, count, f.in.level,
				 RAW_SEARCH_DATA_FULL_DIRECTORY_INFO)) {
			ret = false;
			goto done;
		}
	} while(result.count < 10);

	if (result.list[0].full_directory_info.file_index == 0) {
		torture_skip_goto(tctx, done,
				"Talking to a server that doesn't provide a "
				"file index.\nWindows servers using NTFS do "
				"not provide a file_index. Skipping test\n");
	} else {
		/* We are not talking to a Windows based server.  Windows
		 * servers using NTFS do not provide a file_index.  Windows
		 * servers using FAT do provide a file index, however in both
		 * cases they do not honor a file index on a resume request.
		 * See MS-FSCC <62> and MS-SMB2 <54> for more information. */

		/* Set the file_index flag to point to the fifth file from the
		 * previous enumeration and try to start the subsequent
		 * searches from that point */
		f.in.file_index =
		    result.list[resume_index].full_directory_info.file_index;
		f.in.continue_flags = SMB2_CONTINUE_FLAG_INDEX;

		/* get the name of the next expected file */
		fname = talloc_asprintf(mem_ctx, DNAME "\\%s",
			result.list[resume_index].full_directory_info.name.s);

		ZERO_STRUCT(result);
		result.tctx = tctx;
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			goto done;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		if (!fill_result(&result, d, count, f.in.level,
				 RAW_SEARCH_DATA_FULL_DIRECTORY_INFO)) {
			ret = false;
			goto done;
		}
		if (strcmp(fname,
			result.list[0].full_directory_info.name.s)) {
			torture_comment(tctx, "Next expected file: %s but the "
			    "server returned %s\n", fname,
			    result.list[0].full_directory_info.name.s);
			torture_comment(tctx,
					"Not an error. Resuming using a file "
					"index is an optional feature of the "
					"protocol.\n");
			goto done;
		}
	}
done:
	smb2_util_close(tree, h);
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

/*
 * Tests directory enumeration in a directory containing >1000 files with
 * names of varying lengths.
 */
static bool test_large_files(struct torture_context *tctx,
			     struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	const int num_files = 2000;
	int max_len = 200;
	/* These should be evenly divisible */
	int num_at_len = num_files / max_len;
	struct file_elem files[2000] = {};
	size_t len = 1;
	bool ret = true;
	NTSTATUS status;
	struct smb2_create create;
	struct smb2_find f;
	struct smb2_handle h;
	union smb_search_data *d;
	int i, j, file_count = 0;
	char **strs = NULL;
	unsigned count;

	torture_comment(tctx,
	    "Testing directory enumeration in a directory with >1000 files\n");

	smb2_deltree(tree, DNAME);

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_DIR_ALL;
	create.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	create.in.file_attributes = FILE_ATTRIBUTE_DIRECTORY;
	create.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				 NTCREATEX_SHARE_ACCESS_WRITE |
				 NTCREATEX_SHARE_ACCESS_DELETE;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;
	create.in.fname = DNAME;

	status = smb2_create(tree, mem_ctx, &create);
	torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
	h = create.out.file.handle;

	ZERO_STRUCT(create);
	create.in.desired_access = SEC_RIGHTS_FILE_ALL;
	create.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	create.in.create_disposition = NTCREATEX_DISP_CREATE;

	for (i = 0; i < num_files; i++) {
		if (i % num_at_len == 0) {
		    strs = generate_unique_strs(mem_ctx, len, num_at_len);
		    len++;
		}
		files[i].name = strs[i % num_at_len];
		create.in.fname = talloc_asprintf(mem_ctx, "%s\\%s",
		    DNAME, files[i].name);
		status = smb2_create(tree, mem_ctx, &create);
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");
		smb2_util_close(tree, create.out.file.handle);
	}

	ZERO_STRUCT(f);
	f.in.file.handle        = h;
	f.in.pattern            = "*";
	f.in.max_response_size  = 0x100;
	f.in.level              = SMB2_FIND_BOTH_DIRECTORY_INFO;

	do {
		status = smb2_find_level(tree, tree, &f, &count, &d);
		if (NT_STATUS_EQUAL(status, STATUS_NO_MORE_FILES))
			break;
		torture_assert_ntstatus_ok_goto(tctx, status, ret, done, "");

		for (i = 0; i < count; i++) {
			bool expected;
			const char *found = d[i].both_directory_info.name.s;

			if (!strcmp(found, ".") || !strcmp(found, ".."))
				continue;

			expected = false;
			for (j = 0; j < 2000; j++) {
				if (!strcmp(files[j].name, found)) {
					files[j].found = true;
					expected = true;
					break;
				}
			}

			if (expected)
				continue;

			torture_result(tctx, TORTURE_FAIL,
			    "(%s): didn't expect %s\n",
			    __location__, found);
			ret = false;
			goto done;
		}
		file_count = file_count + i;
		f.in.continue_flags = 0;
		f.in.max_response_size  = 4096;
	} while (count != 0);

	torture_assert_int_equal_goto(tctx, file_count, num_files + 2, ret,
				      done, "");

	for (i = 0; i < num_files; i++) {
		if (files[j].found)
			continue;

		torture_result(tctx, TORTURE_FAIL,
		    "(%s): expected to find %s, but didn't\n",
		    __location__, files[j].name);
		ret = false;
		goto done;
	}
done:
	smb2_util_close(tree, h);
	smb2_deltree(tree, DNAME);
	talloc_free(mem_ctx);

	return ret;
}

struct torture_suite *torture_smb2_dir_init(void)
{
	struct torture_suite *suite =
	    torture_suite_create(talloc_autofree_context(), "dir");

	torture_suite_add_1smb2_test(suite, "find", test_find);
	torture_suite_add_1smb2_test(suite, "fixed", test_fixed);
	torture_suite_add_1smb2_test(suite, "one", test_one_file);
	torture_suite_add_1smb2_test(suite, "many", test_many_files);
	torture_suite_add_1smb2_test(suite, "modify", test_modify_search);
	torture_suite_add_1smb2_test(suite, "sorted", test_sorted);
	torture_suite_add_1smb2_test(suite, "file-index", test_file_index);
	torture_suite_add_1smb2_test(suite, "large-files", test_large_files);
	suite->description = talloc_strdup(suite, "SMB2-DIR tests");

	return suite;
}
