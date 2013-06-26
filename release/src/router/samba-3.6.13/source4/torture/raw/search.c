/* 
   Unix SMB/CIFS implementation.
   RAW_SEARCH_* individual test suite
   Copyright (C) Andrew Tridgell 2003
   
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
#include "system/filesys.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "lib/util/tsort.h"


#define BASEDIR "\\testsearch"

#define CHECK_STATUS_LEVEL(__tctx, __status, __level, __supp)		\
	do {								\
		if (NT_STATUS_EQUAL(__status,				\
			NT_STATUS_NOT_SUPPORTED) ||			\
		    NT_STATUS_EQUAL(__status,				\
			NT_STATUS_NOT_IMPLEMENTED)) {			\
			torture_warning(__tctx, "(%s) Info "		\
			    "level "#__level" is %s",			\
			    __location__, nt_errstr(__status));		\
			__supp = false;					\
		} else {						\
			torture_assert_ntstatus_ok_goto(__tctx,		\
			    __status, ret, done, #__level" failed");	\
			__supp = true;					\
		}							\
	} while (0)

/*
  callback function for single_search
*/
static bool single_search_callback(void *private_data, const union smb_search_data *file)
{
	union smb_search_data *data = (union smb_search_data *)private_data;

	*data = *file;

	return true;
}

/*
  do a single file (non-wildcard) search 
*/
NTSTATUS torture_single_search(struct smbcli_state *cli, 
			       TALLOC_CTX *tctx,
			       const char *pattern,
			       enum smb_search_level level,
			       enum smb_search_data_level data_level,
			       uint16_t attrib,
			       union smb_search_data *data)
{
	union smb_search_first io;
	union smb_search_close c;
	NTSTATUS status;

	switch (level) {
	case RAW_SEARCH_SEARCH:
	case RAW_SEARCH_FFIRST:
	case RAW_SEARCH_FUNIQUE:
		io.search_first.level = level;
		io.search_first.data_level = RAW_SEARCH_DATA_SEARCH;
		io.search_first.in.max_count = 1;
		io.search_first.in.search_attrib = attrib;
		io.search_first.in.pattern = pattern;
		break;

	case RAW_SEARCH_TRANS2:
		io.t2ffirst.level = RAW_SEARCH_TRANS2;
		io.t2ffirst.data_level = data_level;
		io.t2ffirst.in.search_attrib = attrib;
		io.t2ffirst.in.max_count = 1;
		io.t2ffirst.in.flags = FLAG_TRANS2_FIND_CLOSE;
		io.t2ffirst.in.storage_type = 0;
		io.t2ffirst.in.pattern = pattern;
		break;

	case RAW_SEARCH_SMB2:
		return NT_STATUS_INVALID_LEVEL;
	}

	status = smb_raw_search_first(cli->tree, tctx,
				      &io, (void *)data, single_search_callback);

	if (NT_STATUS_IS_OK(status) && level == RAW_SEARCH_FFIRST) {
		c.fclose.level = RAW_FINDCLOSE_FCLOSE;
		c.fclose.in.max_count = 1;
		c.fclose.in.search_attrib = 0;
		c.fclose.in.id = data->search.id;
		status = smb_raw_search_close(cli->tree, &c);
	}
	
	return status;
}


static struct {
	const char *name;
	enum smb_search_level level;
	enum smb_search_data_level data_level;
	int name_offset;
	int resume_key_offset;
	uint32_t capability_mask;
	NTSTATUS status;
	union smb_search_data data;
} levels[] = {
	{"FFIRST",
	 RAW_SEARCH_FFIRST, RAW_SEARCH_DATA_SEARCH,
	 offsetof(union smb_search_data, search.name),
	 -1,
	},
	{"FUNIQUE",
	 RAW_SEARCH_FUNIQUE, RAW_SEARCH_DATA_SEARCH,
	 offsetof(union smb_search_data, search.name),
	 -1,
	},
	{"SEARCH",
	 RAW_SEARCH_SEARCH, RAW_SEARCH_DATA_SEARCH,
	 offsetof(union smb_search_data, search.name),
	 -1,
	},
	{"STANDARD",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_STANDARD, 
	 offsetof(union smb_search_data, standard.name.s),
	 offsetof(union smb_search_data, standard.resume_key),
	},
	{"EA_SIZE",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_EA_SIZE, 
	 offsetof(union smb_search_data, ea_size.name.s),
	 offsetof(union smb_search_data, ea_size.resume_key),
	},
	{"DIRECTORY_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_DIRECTORY_INFO, 
	 offsetof(union smb_search_data, directory_info.name.s),
	 offsetof(union smb_search_data, directory_info.file_index),
	},
	{"FULL_DIRECTORY_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_FULL_DIRECTORY_INFO, 
	 offsetof(union smb_search_data, full_directory_info.name.s),
	 offsetof(union smb_search_data, full_directory_info.file_index),
	},
	{"NAME_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_NAME_INFO, 
	 offsetof(union smb_search_data, name_info.name.s),
	 offsetof(union smb_search_data, name_info.file_index),
	},
	{"BOTH_DIRECTORY_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO, 
	 offsetof(union smb_search_data, both_directory_info.name.s),
	 offsetof(union smb_search_data, both_directory_info.file_index),
	},
	{"ID_FULL_DIRECTORY_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, 
	 offsetof(union smb_search_data, id_full_directory_info.name.s),
	 offsetof(union smb_search_data, id_full_directory_info.file_index),
	},
	{"ID_BOTH_DIRECTORY_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, 
	 offsetof(union smb_search_data, id_both_directory_info.name.s),
	 offsetof(union smb_search_data, id_both_directory_info.file_index),
	},
	{"UNIX_INFO",
	 RAW_SEARCH_TRANS2, RAW_SEARCH_DATA_UNIX_INFO, 
	 offsetof(union smb_search_data, unix_info.name),
	 offsetof(union smb_search_data, unix_info.file_index),
	 CAP_UNIX}
};


/*
  return level name
*/
static const char *level_name(enum smb_search_level level,
			      enum smb_search_data_level data_level)
{
	int i;
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (level == levels[i].level &&
		    data_level == levels[i].data_level) {
			return levels[i].name;
		}
	}
	return NULL;
}

/*
  extract the name from a smb_data structure and level
*/
static const char *extract_name(void *data, enum smb_search_level level,
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

/*
  extract the name from a smb_data structure and level
*/
static uint32_t extract_resume_key(void *data, enum smb_search_level level,
				   enum smb_search_data_level data_level)
{
	int i;
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (level == levels[i].level &&
		    data_level == levels[i].data_level) {
			return *(uint32_t *)(levels[i].resume_key_offset + (char *)data);
		}
	}
	return 0;
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

/* 
   basic testing of all RAW_SEARCH_* calls using a single file
*/
static bool test_one_file(struct torture_context *tctx, 
			  struct smbcli_state *cli)
{
	bool ret = true;
	int fnum;
	const char *fname = "\\torture_search.txt";
	const char *fname2 = "\\torture_search-NOTEXIST.txt";
	NTSTATUS status;
	int i;
	union smb_fileinfo all_info, alt_info, name_info, internal_info;
	bool all_info_supported, alt_info_supported, name_info_supported,
	    internal_info_supported;
	union smb_search_data *s;

	fnum = create_complex_file(cli, tctx, fname);
	if (fnum == -1) {
		printf("ERROR: open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	/* call all the levels */
	for (i=0;i<ARRAY_SIZE(levels);i++) {
		NTSTATUS expected_status;
		uint32_t cap = cli->transport->negotiate.capabilities;

		torture_comment(tctx, "Testing %s\n", levels[i].name);

		levels[i].status = torture_single_search(cli, tctx, fname, 
							 levels[i].level,
							 levels[i].data_level,
							 0,
							 &levels[i].data);

		/* see if this server claims to support this level */
		if (((cap & levels[i].capability_mask) != levels[i].capability_mask)
		    || NT_STATUS_EQUAL(levels[i].status, NT_STATUS_NOT_SUPPORTED)) {
			printf("search level %s(%d) not supported by server\n",
			       levels[i].name, (int)levels[i].level);
			continue;
		}

		if (!NT_STATUS_IS_OK(levels[i].status)) {
			printf("search level %s(%d) failed - %s\n",
			       levels[i].name, (int)levels[i].level, 
			       nt_errstr(levels[i].status));
			ret = false;
			continue;
		}

		status = torture_single_search(cli, tctx, fname2, 
					       levels[i].level,
					       levels[i].data_level,
					       0,
					       &levels[i].data);
		
		expected_status = NT_STATUS_NO_SUCH_FILE;
		if (levels[i].level == RAW_SEARCH_SEARCH ||
		    levels[i].level == RAW_SEARCH_FFIRST ||
		    levels[i].level == RAW_SEARCH_FUNIQUE) {
			expected_status = STATUS_NO_MORE_FILES;
		}
		if (!NT_STATUS_EQUAL(status, expected_status)) {
			printf("search level %s(%d) should fail with %s - %s\n",
			       levels[i].name, (int)levels[i].level, 
			       nt_errstr(expected_status),
			       nt_errstr(status));
			ret = false;
		}
	}

	/* get the all_info file into to check against */
	all_info.generic.level = RAW_FILEINFO_ALL_INFO;
	all_info.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, tctx, &all_info);
	CHECK_STATUS_LEVEL(tctx, status, "RAW_FILEINFO_ALL_INFO",
	    all_info_supported);

	alt_info.generic.level = RAW_FILEINFO_ALT_NAME_INFO;
	alt_info.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, tctx, &alt_info);
	CHECK_STATUS_LEVEL(tctx, status, "RAW_FILEINFO_ALT_NAME_INFO",
	    alt_info_supported);

	internal_info.generic.level = RAW_FILEINFO_INTERNAL_INFORMATION;
	internal_info.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, tctx, &internal_info);
	CHECK_STATUS_LEVEL(tctx, status, "RAW_FILEINFO_INTERNAL_INFORMATION",
	    internal_info_supported);

	name_info.generic.level = RAW_FILEINFO_NAME_INFO;
	name_info.generic.in.file.path = fname;
	status = smb_raw_pathinfo(cli->tree, tctx, &name_info);
	CHECK_STATUS_LEVEL(tctx, status, "RAW_FILEINFO_NAME_INFO",
	    name_info_supported);

#define CHECK_VAL(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if ((s->sname1.field1) != (v.sname2.out.field2)) { \
			printf("(%s) %s/%s [0x%x] != %s/%s [0x%x]\n", \
			       __location__, \
				#sname1, #field1, (int)s->sname1.field1, \
				#sname2, #field2, (int)v.sname2.out.field2); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_TIME(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if (s->sname1.field1 != (~1 & nt_time_to_unix(v.sname2.out.field2))) { \
			printf("(%s) %s/%s [%s] != %s/%s [%s]\n", \
			       __location__, \
				#sname1, #field1, timestring(tctx, s->sname1.field1), \
				#sname2, #field2, nt_time_string(tctx, v.sname2.out.field2)); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_NTTIME(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if (s->sname1.field1 != v.sname2.out.field2) { \
			printf("(%s) %s/%s [%s] != %s/%s [%s]\n", \
			       __location__, \
				#sname1, #field1, nt_time_string(tctx, s->sname1.field1), \
				#sname2, #field2, nt_time_string(tctx, v.sname2.out.field2)); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_STR(name, sname1, field1, v, sname2, field2) do { \
	s = find(name); \
	if (s) { \
		if (!s->sname1.field1 || strcmp(s->sname1.field1, v.sname2.out.field2.s)) { \
			printf("(%s) %s/%s [%s] != %s/%s [%s]\n", \
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
		    strcmp(s->sname1.field1.s, v.sname2.out.field2.s) || \
		    wire_bad_flags(&s->sname1.field1, flags, cli->transport)) { \
			printf("(%s) %s/%s [%s] != %s/%s [%s]\n", \
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
		    strcmp(s->sname1.field1.s, fname) || \
		    wire_bad_flags(&s->sname1.field1, flags, cli->transport)) { \
			printf("(%s) %s/%s [%s] != %s\n", \
			       __location__, \
				#sname1, #field1, s->sname1.field1.s, \
				fname); \
			ret = false; \
		} \
	}} while (0)

#define CHECK_UNIX_NAME(name, sname1, field1, fname, flags) do { \
	s = find(name); \
	if (s) { \
		if (!s->sname1.field1 || \
		    strcmp(s->sname1.field1, fname)) { \
			printf("(%s) %s/%s [%s] != %s\n", \
			       __location__, \
				#sname1, #field1, s->sname1.field1, \
				fname); \
			ret = false; \
		} \
	}} while (0)
	
	/* check that all the results are as expected */
	CHECK_VAL("SEARCH",              search,              attrib, all_info, all_info, attrib&0xFFF);
	CHECK_VAL("STANDARD",            standard,            attrib, all_info, all_info, attrib&0xFFF);
	CHECK_VAL("EA_SIZE",             ea_size,             attrib, all_info, all_info, attrib&0xFFF);
	CHECK_VAL("DIRECTORY_INFO",      directory_info,      attrib, all_info, all_info, attrib);
	CHECK_VAL("FULL_DIRECTORY_INFO", full_directory_info, attrib, all_info, all_info, attrib);
	CHECK_VAL("BOTH_DIRECTORY_INFO", both_directory_info, attrib, all_info, all_info, attrib);
	CHECK_VAL("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           attrib, all_info, all_info, attrib);
	CHECK_VAL("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           attrib, all_info, all_info, attrib);

	CHECK_TIME("SEARCH",             search,              write_time, all_info, all_info, write_time);
	CHECK_TIME("STANDARD",           standard,            write_time, all_info, all_info, write_time);
	CHECK_TIME("EA_SIZE",            ea_size,             write_time, all_info, all_info, write_time);
	CHECK_TIME("STANDARD",           standard,            create_time, all_info, all_info, create_time);
	CHECK_TIME("EA_SIZE",            ea_size,             create_time, all_info, all_info, create_time);
	CHECK_TIME("STANDARD",           standard,            access_time, all_info, all_info, access_time);
	CHECK_TIME("EA_SIZE",            ea_size,             access_time, all_info, all_info, access_time);

	CHECK_NTTIME("DIRECTORY_INFO",      directory_info,      write_time, all_info, all_info, write_time);
	CHECK_NTTIME("FULL_DIRECTORY_INFO", full_directory_info, write_time, all_info, all_info, write_time);
	CHECK_NTTIME("BOTH_DIRECTORY_INFO", both_directory_info, write_time, all_info, all_info, write_time);
	CHECK_NTTIME("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           write_time, all_info, all_info, write_time);
	CHECK_NTTIME("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           write_time, all_info, all_info, write_time);

	CHECK_NTTIME("DIRECTORY_INFO",      directory_info,      create_time, all_info, all_info, create_time);
	CHECK_NTTIME("FULL_DIRECTORY_INFO", full_directory_info, create_time, all_info, all_info, create_time);
	CHECK_NTTIME("BOTH_DIRECTORY_INFO", both_directory_info, create_time, all_info, all_info, create_time);
	CHECK_NTTIME("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           create_time, all_info, all_info, create_time);
	CHECK_NTTIME("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           create_time, all_info, all_info, create_time);

	CHECK_NTTIME("DIRECTORY_INFO",      directory_info,      access_time, all_info, all_info, access_time);
	CHECK_NTTIME("FULL_DIRECTORY_INFO", full_directory_info, access_time, all_info, all_info, access_time);
	CHECK_NTTIME("BOTH_DIRECTORY_INFO", both_directory_info, access_time, all_info, all_info, access_time);
	CHECK_NTTIME("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           access_time, all_info, all_info, access_time);
	CHECK_NTTIME("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           access_time, all_info, all_info, access_time);

	CHECK_NTTIME("DIRECTORY_INFO",      directory_info,      change_time, all_info, all_info, change_time);
	CHECK_NTTIME("FULL_DIRECTORY_INFO", full_directory_info, change_time, all_info, all_info, change_time);
	CHECK_NTTIME("BOTH_DIRECTORY_INFO", both_directory_info, change_time, all_info, all_info, change_time);
	CHECK_NTTIME("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           change_time, all_info, all_info, change_time);
	CHECK_NTTIME("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           change_time, all_info, all_info, change_time);

	CHECK_VAL("SEARCH",              search,              size, all_info, all_info, size);
	CHECK_VAL("STANDARD",            standard,            size, all_info, all_info, size);
	CHECK_VAL("EA_SIZE",             ea_size,             size, all_info, all_info, size);
	CHECK_VAL("DIRECTORY_INFO",      directory_info,      size, all_info, all_info, size);
	CHECK_VAL("FULL_DIRECTORY_INFO", full_directory_info, size, all_info, all_info, size);
	CHECK_VAL("BOTH_DIRECTORY_INFO", both_directory_info, size, all_info, all_info, size);
	CHECK_VAL("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           size, all_info, all_info, size);
	CHECK_VAL("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           size, all_info, all_info, size);
	CHECK_VAL("UNIX_INFO",           unix_info,           size, all_info, all_info, size);

	CHECK_VAL("STANDARD",            standard,            alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("EA_SIZE",             ea_size,             alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("DIRECTORY_INFO",      directory_info,      alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("FULL_DIRECTORY_INFO", full_directory_info, alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("BOTH_DIRECTORY_INFO", both_directory_info, alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           alloc_size, all_info, all_info, alloc_size);
	CHECK_VAL("UNIX_INFO",           unix_info,           alloc_size, all_info, all_info, alloc_size);

	CHECK_VAL("EA_SIZE",             ea_size,             ea_size, all_info, all_info, ea_size);
	CHECK_VAL("FULL_DIRECTORY_INFO", full_directory_info, ea_size, all_info, all_info, ea_size);
	CHECK_VAL("BOTH_DIRECTORY_INFO", both_directory_info, ea_size, all_info, all_info, ea_size);
	CHECK_VAL("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           ea_size, all_info, all_info, ea_size);
	CHECK_VAL("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           ea_size, all_info, all_info, ea_size);

	if (alt_info_supported) {
		CHECK_STR("SEARCH", search, name, alt_info, alt_name_info,
		    fname);
		CHECK_WSTR("BOTH_DIRECTORY_INFO", both_directory_info,
		    short_name, alt_info, alt_name_info, fname, STR_UNICODE);
	}

	CHECK_NAME("STANDARD",            standard,            name, fname+1, 0);
	CHECK_NAME("EA_SIZE",             ea_size,             name, fname+1, 0);
	CHECK_NAME("DIRECTORY_INFO",      directory_info,      name, fname+1, STR_TERMINATE_ASCII);
	CHECK_NAME("FULL_DIRECTORY_INFO", full_directory_info, name, fname+1, STR_TERMINATE_ASCII);

	if (name_info_supported) {
		CHECK_NAME("NAME_INFO", name_info, name, fname+1,
		    STR_TERMINATE_ASCII);
	}

	CHECK_NAME("BOTH_DIRECTORY_INFO", both_directory_info, name, fname+1, STR_TERMINATE_ASCII);
	CHECK_NAME("ID_FULL_DIRECTORY_INFO", id_full_directory_info,           name, fname+1, STR_TERMINATE_ASCII);
	CHECK_NAME("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,           name, fname+1, STR_TERMINATE_ASCII);
	CHECK_UNIX_NAME("UNIX_INFO",           unix_info,           name, fname+1, STR_TERMINATE_ASCII);

	if (internal_info_supported) {
		CHECK_VAL("ID_FULL_DIRECTORY_INFO", id_full_directory_info,
		    file_id, internal_info, internal_information, file_id);
		CHECK_VAL("ID_BOTH_DIRECTORY_INFO", id_both_directory_info,
		    file_id, internal_info, internal_information, file_id);
	}

done:
	smb_raw_exit(cli->session);
	smbcli_unlink(cli->tree, fname);

	return ret;
}


struct multiple_result {
	TALLOC_CTX *tctx;
	int count;
	union smb_search_data *list;
};

/*
  callback function for multiple_search
*/
static bool multiple_search_callback(void *private_data, const union smb_search_data *file)
{
	struct multiple_result *data = (struct multiple_result *)private_data;


	data->count++;
	data->list = talloc_realloc(data->tctx,
				      data->list, 
				      union smb_search_data,
				      data->count);

	data->list[data->count-1] = *file;

	return true;
}

enum continue_type {CONT_FLAGS, CONT_NAME, CONT_RESUME_KEY};

/*
  do a single file (non-wildcard) search 
*/
static NTSTATUS multiple_search(struct smbcli_state *cli, 
				TALLOC_CTX *tctx,
				const char *pattern,
				enum smb_search_data_level data_level,
				enum continue_type cont_type,
				void *data)
{
	union smb_search_first io;
	union smb_search_next io2;
	NTSTATUS status;
	const int per_search = 100;
	struct multiple_result *result = (struct multiple_result *)data;

	if (data_level == RAW_SEARCH_DATA_SEARCH) {
		io.search_first.level = RAW_SEARCH_SEARCH;
		io.search_first.data_level = RAW_SEARCH_DATA_SEARCH;
		io.search_first.in.max_count = per_search;
		io.search_first.in.search_attrib = 0;
		io.search_first.in.pattern = pattern;
	} else {
		io.t2ffirst.level = RAW_SEARCH_TRANS2;
		io.t2ffirst.data_level = data_level;
		io.t2ffirst.in.search_attrib = 0;
		io.t2ffirst.in.max_count = per_search;
		io.t2ffirst.in.flags = FLAG_TRANS2_FIND_CLOSE_IF_END;
		io.t2ffirst.in.storage_type = 0;
		io.t2ffirst.in.pattern = pattern;
		if (cont_type == CONT_RESUME_KEY) {
			io.t2ffirst.in.flags |= FLAG_TRANS2_FIND_REQUIRE_RESUME | 
				FLAG_TRANS2_FIND_BACKUP_INTENT;
		}
	}

	status = smb_raw_search_first(cli->tree, tctx,
				      &io, data, multiple_search_callback);
	

	while (NT_STATUS_IS_OK(status)) {
		if (data_level == RAW_SEARCH_DATA_SEARCH) {
			io2.search_next.level = RAW_SEARCH_SEARCH;
			io2.search_next.data_level = RAW_SEARCH_DATA_SEARCH;
			io2.search_next.in.max_count = per_search;
			io2.search_next.in.search_attrib = 0;
			io2.search_next.in.id = result->list[result->count-1].search.id;
		} else {
			io2.t2fnext.level = RAW_SEARCH_TRANS2;
			io2.t2fnext.data_level = data_level;
			io2.t2fnext.in.handle = io.t2ffirst.out.handle;
			io2.t2fnext.in.max_count = per_search;
			io2.t2fnext.in.resume_key = 0;
			io2.t2fnext.in.flags = FLAG_TRANS2_FIND_CLOSE_IF_END;
			io2.t2fnext.in.last_name = "";
			switch (cont_type) {
			case CONT_RESUME_KEY:
				io2.t2fnext.in.resume_key = extract_resume_key(&result->list[result->count-1],
									       io2.t2fnext.level, io2.t2fnext.data_level);
				if (io2.t2fnext.in.resume_key == 0) {
					printf("Server does not support resume by key for level %s\n",
					       level_name(io2.t2fnext.level, io2.t2fnext.data_level));
					return NT_STATUS_NOT_SUPPORTED;
				}
				io2.t2fnext.in.flags |= FLAG_TRANS2_FIND_REQUIRE_RESUME |
					FLAG_TRANS2_FIND_BACKUP_INTENT;
				break;
			case CONT_NAME:
				io2.t2fnext.in.last_name = extract_name(&result->list[result->count-1],
									io2.t2fnext.level, io2.t2fnext.data_level);
				break;
			case CONT_FLAGS:
				io2.t2fnext.in.flags |= FLAG_TRANS2_FIND_CONTINUE;
				break;
			}
		}

		status = smb_raw_search_next(cli->tree, tctx,
					     &io2, data, multiple_search_callback);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}
		if (data_level == RAW_SEARCH_DATA_SEARCH) {
			if (io2.search_next.out.count == 0) {
				break;
			}
		} else if (io2.t2fnext.out.count == 0 ||
			   io2.t2fnext.out.end_of_search) {
			break;
		}
	}

	return status;
}

#define CHECK_STATUS(status, correct) torture_assert_ntstatus_equal(tctx, status, correct, "incorrect status")

#define CHECK_VALUE(v, correct) torture_assert_int_equal(tctx, (v), (correct), "incorrect value");

#define CHECK_STRING(v, correct) torture_assert_casestr_equal(tctx, v, correct, "incorrect value");


static enum smb_search_data_level compare_data_level;

static int search_compare(union smb_search_data *d1, union smb_search_data *d2)
{
	const char *s1, *s2;
	enum smb_search_level level;

	if (compare_data_level == RAW_SEARCH_DATA_SEARCH) {
		level = RAW_SEARCH_SEARCH;
	} else {
		level = RAW_SEARCH_TRANS2;
	}

	s1 = extract_name(d1, level, compare_data_level);
	s2 = extract_name(d2, level, compare_data_level);
	return strcmp_safe(s1, s2);
}



/* 
   basic testing of search calls using many files
*/
static bool test_many_files(struct torture_context *tctx, 
			    struct smbcli_state *cli)
{
	const int num_files = 700;
	int i, fnum, t;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;
	struct {
		const char *name;
		const char *cont_name;
		enum smb_search_data_level data_level;
		enum continue_type cont_type;
	} search_types[] = {
		{"SEARCH",              "ID",    RAW_SEARCH_DATA_SEARCH,              CONT_RESUME_KEY},
		{"BOTH_DIRECTORY_INFO", "NAME",  RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO, CONT_NAME},
		{"BOTH_DIRECTORY_INFO", "FLAGS", RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO, CONT_FLAGS},
		{"BOTH_DIRECTORY_INFO", "KEY",   RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO, CONT_RESUME_KEY},
		{"STANDARD",            "FLAGS", RAW_SEARCH_DATA_STANDARD,            CONT_FLAGS},
		{"STANDARD",            "KEY",   RAW_SEARCH_DATA_STANDARD,            CONT_RESUME_KEY},
		{"STANDARD",            "NAME",  RAW_SEARCH_DATA_STANDARD,            CONT_NAME},
		{"EA_SIZE",             "FLAGS", RAW_SEARCH_DATA_EA_SIZE,             CONT_FLAGS},
		{"EA_SIZE",             "KEY",   RAW_SEARCH_DATA_EA_SIZE,             CONT_RESUME_KEY},
		{"EA_SIZE",             "NAME",  RAW_SEARCH_DATA_EA_SIZE,             CONT_NAME},
		{"DIRECTORY_INFO",      "FLAGS", RAW_SEARCH_DATA_DIRECTORY_INFO,      CONT_FLAGS},
		{"DIRECTORY_INFO",      "KEY",   RAW_SEARCH_DATA_DIRECTORY_INFO,      CONT_RESUME_KEY},
		{"DIRECTORY_INFO",      "NAME",  RAW_SEARCH_DATA_DIRECTORY_INFO,      CONT_NAME},
		{"FULL_DIRECTORY_INFO",    "FLAGS", RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,    CONT_FLAGS},
		{"FULL_DIRECTORY_INFO",    "KEY",   RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,    CONT_RESUME_KEY},
		{"FULL_DIRECTORY_INFO",    "NAME",  RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,    CONT_NAME},
		{"ID_FULL_DIRECTORY_INFO", "FLAGS", RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, CONT_FLAGS},
		{"ID_FULL_DIRECTORY_INFO", "KEY",   RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, CONT_RESUME_KEY},
		{"ID_FULL_DIRECTORY_INFO", "NAME",  RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO, CONT_NAME},
		{"ID_BOTH_DIRECTORY_INFO", "NAME",  RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, CONT_NAME},
		{"ID_BOTH_DIRECTORY_INFO", "FLAGS", RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, CONT_FLAGS},
		{"ID_BOTH_DIRECTORY_INFO", "KEY",   RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO, CONT_RESUME_KEY}
	};

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing with %d files\n", num_files);

	for (i=0;i<num_files;i++) {
		fname = talloc_asprintf(cli, BASEDIR "\\t%03d-%d.txt", i, i);
		fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
		torture_assert(tctx, fnum != -1, "Failed to create");
		talloc_free(fname);
		smbcli_close(cli->tree, fnum);
	}


	for (t=0;t<ARRAY_SIZE(search_types);t++) {
		ZERO_STRUCT(result);

		if ((search_types[t].cont_type == CONT_RESUME_KEY) &&
		    (search_types[t].data_level != RAW_SEARCH_DATA_SEARCH) &&
		    !torture_setting_bool(tctx, "resume_key_support", true)) {
			torture_comment(tctx,
					"SKIP: Continue %s via %s\n",
					search_types[t].name, search_types[t].cont_name);
			continue;
		}

		result.tctx = talloc_new(tctx);
	
		torture_comment(tctx,
				"Continue %s via %s\n", search_types[t].name, search_types[t].cont_name);

		status = multiple_search(cli, tctx, BASEDIR "\\*.*", 
					 search_types[t].data_level,
					 search_types[t].cont_type,
					 &result);
		if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
			torture_warning(tctx, "search level %s not supported "
					"by server",
					search_types[t].name);
			continue;
		}
		torture_assert_ntstatus_ok(tctx, status, "search failed");
		CHECK_VALUE(result.count, num_files);

		compare_data_level = search_types[t].data_level;

		TYPESAFE_QSORT(result.list, result.count, search_compare);

		for (i=0;i<result.count;i++) {
			const char *s;
			enum smb_search_level level;
			if (compare_data_level == RAW_SEARCH_DATA_SEARCH) {
				level = RAW_SEARCH_SEARCH;
			} else {
				level = RAW_SEARCH_TRANS2;
			}
			s = extract_name(&result.list[i], level, compare_data_level);
			fname = talloc_asprintf(cli, "t%03d-%d.txt", i, i);
			torture_assert_str_equal(tctx, fname, s, "Incorrect name");
			talloc_free(fname);
		}
		talloc_free(result.tctx);
	}

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/*
  check a individual file result
*/
static bool check_result(struct multiple_result *result, const char *name, bool exist, uint32_t attrib)
{
	int i;
	for (i=0;i<result->count;i++) {
		if (strcmp(name, result->list[i].both_directory_info.name.s) == 0) break;
	}
	if (i == result->count) {
		if (exist) {
			printf("failed: '%s' should exist with attribute %s\n", 
			       name, attrib_string(result->list, attrib));
			return false;
		}
		return true;
	}

	if (!exist) {
		printf("failed: '%s' should NOT exist (has attribute %s)\n", 
		       name, attrib_string(result->list, result->list[i].both_directory_info.attrib));
		return false;
	}

	if ((result->list[i].both_directory_info.attrib&0xFFF) != attrib) {
		printf("failed: '%s' should have attribute 0x%x (has 0x%x)\n",
		       name, 
		       attrib, result->list[i].both_directory_info.attrib);
		return false;
	}
	return true;
}

/* 
   test what happens when the directory is modified during a search
*/
static bool test_modify_search(struct torture_context *tctx, 
							   struct smbcli_state *cli)
{
	const int num_files = 20;
	int i, fnum;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;
	union smb_search_first io;
	union smb_search_next io2;
	union smb_setfileinfo sfinfo;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Creating %d files\n", num_files);

	for (i=num_files-1;i>=0;i--) {
		fname = talloc_asprintf(cli, BASEDIR "\\t%03d-%d.txt", i, i);
		fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
		if (fnum == -1) {
			printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
			ret = false;
			goto done;
		}
		talloc_free(fname);
		smbcli_close(cli->tree, fnum);
	}

	printf("pulling the first file\n");
	ZERO_STRUCT(result);
	result.tctx = talloc_new(tctx);

	io.t2ffirst.level = RAW_SEARCH_TRANS2;
	io.t2ffirst.data_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
	io.t2ffirst.in.search_attrib = 0;
	io.t2ffirst.in.max_count = 0;
	io.t2ffirst.in.flags = 0;
	io.t2ffirst.in.storage_type = 0;
	io.t2ffirst.in.pattern = BASEDIR "\\*.*";

	status = smb_raw_search_first(cli->tree, tctx,
				      &io, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, 1);
	
	printf("pulling the second file\n");
	io2.t2fnext.level = RAW_SEARCH_TRANS2;
	io2.t2fnext.data_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
	io2.t2fnext.in.handle = io.t2ffirst.out.handle;
	io2.t2fnext.in.max_count = 1;
	io2.t2fnext.in.resume_key = 0;
	io2.t2fnext.in.flags = 0;
	io2.t2fnext.in.last_name = result.list[result.count-1].both_directory_info.name.s;

	status = smb_raw_search_next(cli->tree, tctx,
				     &io2, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, 2);

	result.count = 0;

	printf("Changing attributes and deleting\n");
	smbcli_open(cli->tree, BASEDIR "\\T003-03.txt.2", O_CREAT|O_RDWR, DENY_NONE);
	smbcli_open(cli->tree, BASEDIR "\\T013-13.txt.2", O_CREAT|O_RDWR, DENY_NONE);
	fnum = create_complex_file(cli, tctx, BASEDIR "\\T013-13.txt.3");
	smbcli_unlink(cli->tree, BASEDIR "\\T014-14.txt");
	torture_set_file_attribute(cli->tree, BASEDIR "\\T015-15.txt", FILE_ATTRIBUTE_HIDDEN);
	torture_set_file_attribute(cli->tree, BASEDIR "\\T016-16.txt", FILE_ATTRIBUTE_NORMAL);
	torture_set_file_attribute(cli->tree, BASEDIR "\\T017-17.txt", FILE_ATTRIBUTE_SYSTEM);	
	torture_set_file_attribute(cli->tree, BASEDIR "\\T018-18.txt", 0);	
	sfinfo.generic.level = RAW_SFILEINFO_DISPOSITION_INFORMATION;
	sfinfo.generic.in.file.fnum = fnum;
	sfinfo.disposition_info.in.delete_on_close = 1;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	io2.t2fnext.level = RAW_SEARCH_TRANS2;
	io2.t2fnext.data_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
	io2.t2fnext.in.handle = io.t2ffirst.out.handle;
	io2.t2fnext.in.max_count = num_files + 3;
	io2.t2fnext.in.resume_key = 0;
	io2.t2fnext.in.flags = 0;
	io2.t2fnext.in.last_name = ".";

	status = smb_raw_search_next(cli->tree, tctx,
				     &io2, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, 20);

	ret &= check_result(&result, "t009-9.txt", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(&result, "t014-14.txt", false, 0);
	ret &= check_result(&result, "t015-15.txt", false, 0);
	ret &= check_result(&result, "t016-16.txt", true, FILE_ATTRIBUTE_NORMAL);
	ret &= check_result(&result, "t017-17.txt", false, 0);
	ret &= check_result(&result, "t018-18.txt", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(&result, "t019-19.txt", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(&result, "T013-13.txt.2", true, FILE_ATTRIBUTE_ARCHIVE);
	ret &= check_result(&result, "T003-3.txt.2", false, 0);
	ret &= check_result(&result, "T013-13.txt.3", true, FILE_ATTRIBUTE_ARCHIVE);

	if (!ret) {
		for (i=0;i<result.count;i++) {
			printf("%s %s (0x%x)\n", 
			       result.list[i].both_directory_info.name.s, 
			       attrib_string(tctx, result.list[i].both_directory_info.attrib),
			       result.list[i].both_directory_info.attrib);
		}
	}

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/* 
   testing if directories always come back sorted
*/
static bool test_sorted(struct torture_context *tctx, struct smbcli_state *cli)
{
	const int num_files = 700;
	int i, fnum;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Creating %d files\n", num_files);

	for (i=0;i<num_files;i++) {
		fname = talloc_asprintf(cli, BASEDIR "\\%s.txt", generate_random_str_list(tctx, 10, "abcdefgh"));
		fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
		if (fnum == -1) {
			printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
			ret = false;
			goto done;
		}
		talloc_free(fname);
		smbcli_close(cli->tree, fnum);
	}


	ZERO_STRUCT(result);
	result.tctx = tctx;
	
	status = multiple_search(cli, tctx, BASEDIR "\\*.*", 
				 RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,
				 CONT_NAME, &result);	
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, num_files);

	for (i=0;i<num_files-1;i++) {
		const char *name1, *name2;
		name1 = result.list[i].both_directory_info.name.s;
		name2 = result.list[i+1].both_directory_info.name.s;
		if (strcasecmp_m(name1, name2) > 0) {
			printf("non-alphabetical order at entry %d  '%s' '%s'\n", 
			       i, name1, name2);
			printf("Server does not produce sorted directory listings (not an error)\n");
			goto done;
		}
	}

	talloc_free(result.list);

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}



/* 
   basic testing of many old style search calls using separate dirs
*/
static bool test_many_dirs(struct torture_context *tctx, 
						   struct smbcli_state *cli)
{
	const int num_dirs = 20;
	int i, fnum, n;
	char *fname, *dname;
	bool ret = true;
	NTSTATUS status;
	union smb_search_data *file, *file2, *file3;

	if (!torture_setting_bool(tctx, "raw_search_search", true)) {
		torture_comment(tctx, "Skipping these tests as the server "
			"doesn't support old style search calls\n");
		return true;
	}
	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Creating %d dirs\n", num_dirs);

	for (i=0;i<num_dirs;i++) {
		dname = talloc_asprintf(cli, BASEDIR "\\d%d", i);
		status = smbcli_mkdir(cli->tree, dname);
		if (!NT_STATUS_IS_OK(status)) {
			printf("(%s) Failed to create %s - %s\n", 
			       __location__, dname, nt_errstr(status));
			ret = false;
			goto done;
		}

		for (n=0;n<3;n++) {
			fname = talloc_asprintf(cli, BASEDIR "\\d%d\\f%d-%d.txt", i, i, n);
			fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
			if (fnum == -1) {
				printf("(%s) Failed to create %s - %s\n", 
				       __location__, fname, smbcli_errstr(cli->tree));
				ret = false;
				goto done;
			}
			talloc_free(fname);
			smbcli_close(cli->tree, fnum);
		}

		talloc_free(dname);
	}

	file  = talloc_zero_array(tctx, union smb_search_data, num_dirs);
	file2 = talloc_zero_array(tctx, union smb_search_data, num_dirs);
	file3 = talloc_zero_array(tctx, union smb_search_data, num_dirs);

	printf("Search first on %d dirs\n", num_dirs);

	for (i=0;i<num_dirs;i++) {
		union smb_search_first io;
		io.search_first.level = RAW_SEARCH_SEARCH;
		io.search_first.data_level = RAW_SEARCH_DATA_SEARCH;
		io.search_first.in.max_count = 1;
		io.search_first.in.search_attrib = 0;
		io.search_first.in.pattern = talloc_asprintf(tctx, BASEDIR "\\d%d\\*.txt", i);
		fname = talloc_asprintf(tctx, "f%d-", i);

		io.search_first.out.count = 0;

		status = smb_raw_search_first(cli->tree, tctx,
					      &io, (void *)&file[i], single_search_callback);
		if (io.search_first.out.count != 1) {
			printf("(%s) search first gave %d entries for dir %d - %s\n",
			       __location__, io.search_first.out.count, i, nt_errstr(status));
			ret = false;
			goto done;
		}
		CHECK_STATUS(status, NT_STATUS_OK);
		if (strncasecmp(file[i].search.name, fname, strlen(fname)) != 0) {
			printf("(%s) incorrect name '%s' expected '%s'[12].txt\n", 
			       __location__, file[i].search.name, fname);
			ret = false;
			goto done;
		}

		talloc_free(fname);
	}

	printf("Search next on %d dirs\n", num_dirs);

	for (i=0;i<num_dirs;i++) {
		union smb_search_next io2;

		io2.search_next.level = RAW_SEARCH_SEARCH;
		io2.search_next.data_level = RAW_SEARCH_DATA_SEARCH;
		io2.search_next.in.max_count = 1;
		io2.search_next.in.search_attrib = 0;
		io2.search_next.in.id = file[i].search.id;
		fname = talloc_asprintf(tctx, "f%d-", i);

		io2.search_next.out.count = 0;

		status = smb_raw_search_next(cli->tree, tctx,
					     &io2, (void *)&file2[i], single_search_callback);
		if (io2.search_next.out.count != 1) {
			printf("(%s) search next gave %d entries for dir %d - %s\n",
			       __location__, io2.search_next.out.count, i, nt_errstr(status));
			ret = false;
			goto done;
		}
		CHECK_STATUS(status, NT_STATUS_OK);
		if (strncasecmp(file2[i].search.name, fname, strlen(fname)) != 0) {
			printf("(%s) incorrect name '%s' expected '%s'[12].txt\n", 
			       __location__, file2[i].search.name, fname);
			ret = false;
			goto done;
		}

		talloc_free(fname);
	}


	printf("Search next (rewind) on %d dirs\n", num_dirs);

	for (i=0;i<num_dirs;i++) {
		union smb_search_next io2;

		io2.search_next.level = RAW_SEARCH_SEARCH;
		io2.search_next.data_level = RAW_SEARCH_DATA_SEARCH;
		io2.search_next.in.max_count = 1;
		io2.search_next.in.search_attrib = 0;
		io2.search_next.in.id = file[i].search.id;
		fname = talloc_asprintf(tctx, "f%d-", i);
		io2.search_next.out.count = 0;

		status = smb_raw_search_next(cli->tree, tctx,
					     &io2, (void *)&file3[i], single_search_callback);
		if (io2.search_next.out.count != 1) {
			printf("(%s) search next gave %d entries for dir %d - %s\n",
			       __location__, io2.search_next.out.count, i, nt_errstr(status));
			ret = false;
			goto done;
		}
		CHECK_STATUS(status, NT_STATUS_OK);

		if (strncasecmp(file3[i].search.name, file2[i].search.name, 3) != 0) {
			printf("(%s) incorrect name '%s' on rewind at dir %d\n", 
			       __location__, file2[i].search.name, i);
			ret = false;
			goto done;
		}

		if (torture_setting_bool(tctx, "rewind_support", true) &&
		    strcmp(file3[i].search.name, file2[i].search.name) != 0) {
			printf("(%s) server did not rewind - got '%s' expected '%s'\n", 
			       __location__, file3[i].search.name, file2[i].search.name);
			ret = false;
			goto done;
		}

		talloc_free(fname);
	}


done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


/* 
   testing of OS/2 style delete
*/
static bool test_os2_delete(struct torture_context *tctx, 
							struct smbcli_state *cli)
{
	const int num_files = 700;
	const int delete_count = 4;
	int total_deleted = 0;
	int i, fnum;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	union smb_search_first io;
	union smb_search_next io2;
	struct multiple_result result;

	if (!torture_setting_bool(tctx, "search_ea_size", true)){
		torture_comment(tctx,
				"Server does not support RAW_SEARCH_EA_SIZE "
				"level. Skipping this test\n");
		return true;
	}

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Testing OS/2 style delete on %d files\n", num_files);

	for (i=0;i<num_files;i++) {
		fname = talloc_asprintf(cli, BASEDIR "\\file%u.txt", i);
		fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
		if (fnum == -1) {
			printf("Failed to create %s - %s\n", fname, smbcli_errstr(cli->tree));
			ret = false;
			goto done;
		}
		talloc_free(fname);
		smbcli_close(cli->tree, fnum);
	}


	ZERO_STRUCT(result);
	result.tctx = tctx;

	io.t2ffirst.level = RAW_SEARCH_TRANS2;
	io.t2ffirst.data_level = RAW_SEARCH_DATA_EA_SIZE;
	io.t2ffirst.in.search_attrib = 0;
	io.t2ffirst.in.max_count = 100;
	io.t2ffirst.in.flags = FLAG_TRANS2_FIND_REQUIRE_RESUME;
	io.t2ffirst.in.storage_type = 0;
	io.t2ffirst.in.pattern = BASEDIR "\\*";

	status = smb_raw_search_first(cli->tree, tctx,
				      &io, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);

	for (i=0;i<MIN(result.count, delete_count);i++) {
		fname = talloc_asprintf(cli, BASEDIR "\\%s", result.list[i].ea_size.name.s);
		status = smbcli_unlink(cli->tree, fname);
		CHECK_STATUS(status, NT_STATUS_OK);
		total_deleted++;
		talloc_free(fname);
	}

	io2.t2fnext.level = RAW_SEARCH_TRANS2;
	io2.t2fnext.data_level = RAW_SEARCH_DATA_EA_SIZE;
	io2.t2fnext.in.handle = io.t2ffirst.out.handle;
	io2.t2fnext.in.max_count = 100;
	io2.t2fnext.in.resume_key = result.list[i-1].ea_size.resume_key;
	io2.t2fnext.in.flags = FLAG_TRANS2_FIND_REQUIRE_RESUME;
	io2.t2fnext.in.last_name = result.list[i-1].ea_size.name.s;

	do {
		ZERO_STRUCT(result);
		result.tctx = tctx;

		status = smb_raw_search_next(cli->tree, tctx,
					     &io2, &result, multiple_search_callback);
		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		for (i=0;i<MIN(result.count, delete_count);i++) {
			fname = talloc_asprintf(cli, BASEDIR "\\%s", result.list[i].ea_size.name.s);
			status = smbcli_unlink(cli->tree, fname);
			CHECK_STATUS(status, NT_STATUS_OK);
			total_deleted++;
			talloc_free(fname);
		}

		if (i>0) {
			io2.t2fnext.in.resume_key = result.list[i-1].ea_size.resume_key;
			io2.t2fnext.in.last_name = result.list[i-1].ea_size.name.s;
		}
	} while (NT_STATUS_IS_OK(status) && result.count != 0);

	CHECK_STATUS(status, NT_STATUS_OK);

	if (total_deleted != num_files) {
		printf("error: deleted %d - expected to delete %d\n", 
		       total_deleted, num_files);
		ret = false;
	}

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}


static int ealist_cmp(union smb_search_data *r1, union smb_search_data *r2)
{
	return strcmp(r1->ea_list.name.s, r2->ea_list.name.s);
}

/* 
   testing of the rather strange ea_list level
*/
static bool test_ea_list(struct torture_context *tctx, 
						 struct smbcli_state *cli)
{
	int  fnum;
	bool ret = true;
	NTSTATUS status;
	union smb_search_first io;
	union smb_search_next nxt;
	struct multiple_result result;
	union smb_setfileinfo setfile;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Testing RAW_SEARCH_EA_LIST level\n");

	if (!torture_setting_bool(tctx, "search_ea_support", true) ||
	    !torture_setting_bool(tctx, "ea_support", true)) {
		printf("..skipped per target configuration.\n");
		return true;
	}

	fnum = smbcli_open(cli->tree, BASEDIR "\\file1.txt", O_CREAT|O_RDWR, DENY_NONE);
	smbcli_close(cli->tree, fnum);

	fnum = smbcli_open(cli->tree, BASEDIR "\\file2.txt", O_CREAT|O_RDWR, DENY_NONE);
	smbcli_close(cli->tree, fnum);

	fnum = smbcli_open(cli->tree, BASEDIR "\\file3.txt", O_CREAT|O_RDWR, DENY_NONE);
	smbcli_close(cli->tree, fnum);

	setfile.generic.level = RAW_SFILEINFO_EA_SET;
	setfile.generic.in.file.path = BASEDIR "\\file2.txt";
	setfile.ea_set.in.num_eas = 2;
	setfile.ea_set.in.eas = talloc_array(tctx, struct ea_struct, 2);
	setfile.ea_set.in.eas[0].flags = 0;
	setfile.ea_set.in.eas[0].name.s = "EA ONE";
	setfile.ea_set.in.eas[0].value = data_blob_string_const("VALUE 1");
	setfile.ea_set.in.eas[1].flags = 0;
	setfile.ea_set.in.eas[1].name.s = "SECOND EA";
	setfile.ea_set.in.eas[1].value = data_blob_string_const("Value Two");

	status = smb_raw_setpathinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);

	setfile.generic.in.file.path = BASEDIR "\\file3.txt";
	status = smb_raw_setpathinfo(cli->tree, &setfile);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	ZERO_STRUCT(result);
	result.tctx = tctx;

	io.t2ffirst.level = RAW_SEARCH_TRANS2;
	io.t2ffirst.data_level = RAW_SEARCH_DATA_EA_LIST;
	io.t2ffirst.in.search_attrib = 0;
	io.t2ffirst.in.max_count = 2;
	io.t2ffirst.in.flags = FLAG_TRANS2_FIND_REQUIRE_RESUME;
	io.t2ffirst.in.storage_type = 0;
	io.t2ffirst.in.pattern = BASEDIR "\\*";
	io.t2ffirst.in.num_names = 2;
	io.t2ffirst.in.ea_names = talloc_array(tctx, struct ea_name, 2);
	io.t2ffirst.in.ea_names[0].name.s = "SECOND EA";
	io.t2ffirst.in.ea_names[1].name.s = "THIRD EA";

	status = smb_raw_search_first(cli->tree, tctx,
				      &io, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, 2);

	nxt.t2fnext.level = RAW_SEARCH_TRANS2;
	nxt.t2fnext.data_level = RAW_SEARCH_DATA_EA_LIST;
	nxt.t2fnext.in.handle = io.t2ffirst.out.handle;
	nxt.t2fnext.in.max_count = 2;
	nxt.t2fnext.in.resume_key = result.list[1].ea_list.resume_key;
	nxt.t2fnext.in.flags = FLAG_TRANS2_FIND_REQUIRE_RESUME | FLAG_TRANS2_FIND_CONTINUE;
	nxt.t2fnext.in.last_name = result.list[1].ea_list.name.s;
	nxt.t2fnext.in.num_names = 2;
	nxt.t2fnext.in.ea_names = talloc_array(tctx, struct ea_name, 2);
	nxt.t2fnext.in.ea_names[0].name.s = "SECOND EA";
	nxt.t2fnext.in.ea_names[1].name.s = "THIRD EA";

	status = smb_raw_search_next(cli->tree, tctx,
				     &nxt, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* we have to sort the result as different servers can return directories
	   in different orders */
	TYPESAFE_QSORT(result.list, result.count, ealist_cmp);

	CHECK_VALUE(result.count, 3);
	CHECK_VALUE(result.list[0].ea_list.eas.num_eas, 2);
	CHECK_STRING(result.list[0].ea_list.name.s, "file1.txt");
	CHECK_STRING(result.list[0].ea_list.eas.eas[0].name.s, "SECOND EA");
	CHECK_VALUE(result.list[0].ea_list.eas.eas[0].value.length, 0);
	CHECK_STRING(result.list[0].ea_list.eas.eas[1].name.s, "THIRD EA");
	CHECK_VALUE(result.list[0].ea_list.eas.eas[1].value.length, 0);

	CHECK_STRING(result.list[1].ea_list.name.s, "file2.txt");
	CHECK_STRING(result.list[1].ea_list.eas.eas[0].name.s, "SECOND EA");
	CHECK_VALUE(result.list[1].ea_list.eas.eas[0].value.length, 9);
	CHECK_STRING((const char *)result.list[1].ea_list.eas.eas[0].value.data, "Value Two");
	CHECK_STRING(result.list[1].ea_list.eas.eas[1].name.s, "THIRD EA");
	CHECK_VALUE(result.list[1].ea_list.eas.eas[1].value.length, 0);

	CHECK_STRING(result.list[2].ea_list.name.s, "file3.txt");
	CHECK_STRING(result.list[2].ea_list.eas.eas[0].name.s, "SECOND EA");
	CHECK_VALUE(result.list[2].ea_list.eas.eas[0].value.length, 9);
	CHECK_STRING((const char *)result.list[2].ea_list.eas.eas[0].value.data, "Value Two");
	CHECK_STRING(result.list[2].ea_list.eas.eas[1].name.s, "THIRD EA");
	CHECK_VALUE(result.list[2].ea_list.eas.eas[1].value.length, 0);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/*
 Test the behavior of max count parameter in TRANS2_FIND_FIRST2 and
 TRANS2_FIND_NEXT2 queries
*/
static bool test_max_count(struct torture_context *tctx,
			   struct smbcli_state *cli)
{
	const int num_files = 2;
	int i, fnum;
	char *fname;
	bool ret = true;
	NTSTATUS status;
	struct multiple_result result;
	union smb_search_first io;
	union smb_search_next io2;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Creating %d files\n", num_files);

	for (i=num_files-1;i>=0;i--) {
		fname = talloc_asprintf(cli, BASEDIR "\\t%03d-%d.txt", i, i);
		fnum = smbcli_open(cli->tree, fname, O_CREAT|O_RDWR, DENY_NONE);
		if (fnum == -1) {
			torture_comment(tctx,
				"Failed to create %s - %s\n",
				fname, smbcli_errstr(cli->tree));
			ret = false;
			goto done;
		}
		talloc_free(fname);
		smbcli_close(cli->tree, fnum);
	}

	torture_comment(tctx, "Set max_count parameter to 0. "
			"This should return 1 entry\n");
	ZERO_STRUCT(result);
	result.tctx = talloc_new(tctx);

	io.t2ffirst.level = RAW_SEARCH_TRANS2;
	io.t2ffirst.data_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
	io.t2ffirst.in.search_attrib = 0;
	io.t2ffirst.in.max_count = 0;
	io.t2ffirst.in.flags = 0;
	io.t2ffirst.in.storage_type = 0;
	io.t2ffirst.in.pattern = BASEDIR "\\*.*";

	status = smb_raw_search_first(cli->tree, tctx,
				      &io, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, 1);

	torture_comment(tctx, "Set max_count to 1. This should also "
			"return 1 entry\n");
	io2.t2fnext.level = RAW_SEARCH_TRANS2;
	io2.t2fnext.data_level = RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO;
	io2.t2fnext.in.handle = io.t2ffirst.out.handle;
	io2.t2fnext.in.max_count = 1;
	io2.t2fnext.in.resume_key = 0;
	io2.t2fnext.in.flags = 0;
	io2.t2fnext.in.last_name =
		result.list[result.count-1].both_directory_info.name.s;

	status = smb_raw_search_next(cli->tree, tctx,
				     &io2, &result, multiple_search_callback);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(result.count, 2);
done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}

/* 
   basic testing of all RAW_SEARCH_* calls using a single file
*/
struct torture_suite *torture_raw_search(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "search");

	torture_suite_add_1smb_test(suite, "one file search", test_one_file);
	torture_suite_add_1smb_test(suite, "many files", test_many_files);
	torture_suite_add_1smb_test(suite, "sorted", test_sorted);
	torture_suite_add_1smb_test(suite, "modify search", test_modify_search);
	torture_suite_add_1smb_test(suite, "many dirs", test_many_dirs);
	torture_suite_add_1smb_test(suite, "os2 delete", test_os2_delete);
	torture_suite_add_1smb_test(suite, "ea list", test_ea_list);
	torture_suite_add_1smb_test(suite, "max count", test_max_count);

	return suite;
}
