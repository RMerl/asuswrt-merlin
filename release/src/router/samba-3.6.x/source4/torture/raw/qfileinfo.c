/* 
   Unix SMB/CIFS implementation.
   RAW_FILEINFO_* individual test suite
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007
   
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
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"

static struct {
	const char *name;
	enum smb_fileinfo_level level;
	unsigned int only_paths:1;
	unsigned int only_handles:1;
	uint32_t capability_mask;
	unsigned int expected_ipc_access_denied:1;
	NTSTATUS expected_ipc_fnum_status;
	NTSTATUS fnum_status, fname_status;
	union smb_fileinfo fnum_finfo, fname_finfo;
} levels[] = {
	{ .name = "GETATTR",
	  .level = RAW_FILEINFO_GETATTR,         
	  .only_paths = 1,
	  .only_handles = 0,
	  .expected_ipc_access_denied = 1},
	{  .name ="GETATTRE",                  
	   .level = RAW_FILEINFO_GETATTRE,         
	   .only_paths = 0, 
	   .only_handles = 1 },
	{  .name ="STANDARD",                  
	   .level = RAW_FILEINFO_STANDARD, },
	{  .name ="EA_SIZE",                 
	   .level = RAW_FILEINFO_EA_SIZE },
	{  .name ="ALL_EAS",                 
	   .level = RAW_FILEINFO_ALL_EAS,
	   .expected_ipc_fnum_status = NT_STATUS_ACCESS_DENIED,
	},
	{  .name ="IS_NAME_VALID",          
	   .level =  RAW_FILEINFO_IS_NAME_VALID,
	   .only_paths =  1,
	   .only_handles =  0 },
	{  .name ="BASIC_INFO",            
	   .level =  RAW_FILEINFO_BASIC_INFO },
	{  .name ="STANDARD_INFO",         
	   .level =  RAW_FILEINFO_STANDARD_INFO },
	{  .name ="EA_INFO",               
	   .level =  RAW_FILEINFO_EA_INFO },
	{  .name ="NAME_INFO",           
	   .level =  RAW_FILEINFO_NAME_INFO },
	{  .name ="ALL_INFO",              
	   .level =  RAW_FILEINFO_ALL_INFO },
	{  .name ="ALT_NAME_INFO",        
	   .level =  RAW_FILEINFO_ALT_NAME_INFO,
	   .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{  .name ="STREAM_INFO",           
	   .level =  RAW_FILEINFO_STREAM_INFO,
	   .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{  .name ="COMPRESSION_INFO",        
	   .level =  RAW_FILEINFO_COMPRESSION_INFO,
	   .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{  .name ="UNIX_BASIC_INFO",         
	   .level =  RAW_FILEINFO_UNIX_BASIC,
	   .only_paths =  0, 
	   .only_handles = 0, 
	   .capability_mask = CAP_UNIX},
	{  .name ="UNIX_LINK_INFO",          
	   .level =  RAW_FILEINFO_UNIX_LINK, 
	   .only_paths = 0, 
	   .only_handles = 0, 
	   .capability_mask = CAP_UNIX},
	{  .name ="BASIC_INFORMATION",      
	   .level =  RAW_FILEINFO_BASIC_INFORMATION },
	{  .name ="STANDARD_INFORMATION",   
	   .level =  RAW_FILEINFO_STANDARD_INFORMATION },
	{  .name ="INTERNAL_INFORMATION",   
	   .level =  RAW_FILEINFO_INTERNAL_INFORMATION },
	{  .name ="EA_INFORMATION",        
	   .level =  RAW_FILEINFO_EA_INFORMATION },
	{ .name = "ACCESS_INFORMATION",    
	  .level =  RAW_FILEINFO_ACCESS_INFORMATION },
	{ .name = "NAME_INFORMATION",      
	  .level =  RAW_FILEINFO_NAME_INFORMATION },
	{  .name ="POSITION_INFORMATION",  
	   .level =  RAW_FILEINFO_POSITION_INFORMATION },
	{  .name ="MODE_INFORMATION",       
	   .level =  RAW_FILEINFO_MODE_INFORMATION },
	{  .name ="ALIGNMENT_INFORMATION",  
	   .level =  RAW_FILEINFO_ALIGNMENT_INFORMATION },
	{  .name ="ALL_INFORMATION",       
	   .level =  RAW_FILEINFO_ALL_INFORMATION },
	{  .name ="ALT_NAME_INFORMATION",  
	   .level =  RAW_FILEINFO_ALT_NAME_INFORMATION,
	   .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{  .name ="STREAM_INFORMATION",    
	   .level =  RAW_FILEINFO_STREAM_INFORMATION,
	   .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{ .name = "COMPRESSION_INFORMATION", 
	  .level =  RAW_FILEINFO_COMPRESSION_INFORMATION,
	  .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{  .name ="NETWORK_OPEN_INFORMATION",
	   .level =  RAW_FILEINFO_NETWORK_OPEN_INFORMATION,
	   .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{ .name = "ATTRIBUTE_TAG_INFORMATION",
	  .level =  RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION,
	  .expected_ipc_fnum_status = NT_STATUS_INVALID_PARAMETER
	},
	{ NULL }
};

/*
  compare a dos time (2 second resolution) to a nt time
*/
static int dos_nt_time_cmp(time_t t, NTTIME nt)
{
	time_t t2 = nt_time_to_unix(nt);
	if (abs(t2 - t) <= 2) return 0;
	return t2 > t ? 1 : -1;
}


/*
  find a level in the levels[] table
*/
static union smb_fileinfo *fnum_find(const char *name)
{
	int i;
	for (i=0; levels[i].name; i++) {
		if (NT_STATUS_IS_OK(levels[i].fnum_status) &&
		    strcmp(name, levels[i].name) == 0 && 
		    !levels[i].only_paths) {
			return &levels[i].fnum_finfo;
		}
	}
	return NULL;
}

/*
  find a level in the levels[] table
*/
static union smb_fileinfo *fname_find(bool is_ipc, const char *name)
{
	int i;
	if (is_ipc) {
		return NULL;
	}
	for (i=0; levels[i].name; i++) {
		if (NT_STATUS_IS_OK(levels[i].fname_status) &&
		    strcmp(name, levels[i].name) == 0 && 
		    !levels[i].only_handles) {
			return &levels[i].fname_finfo;
		}
	}
	return NULL;
}

/* local macros to make the code below more readable */
#define VAL_EQUAL(n1, v1, n2, v2) do {if (s1->n1.out.v1 != s2->n2.out.v2) { \
        printf("%s/%s [%u] != %s/%s [%u] at %s(%d)\n", \
               #n1, #v1, (unsigned int)s1->n1.out.v1, \
               #n2, #v2, (unsigned int)s2->n2.out.v2, \
	       __FILE__, __LINE__); \
        ret = false; \
}} while(0)

#define STR_EQUAL(n1, v1, n2, v2) do {if (strcmp_safe(s1->n1.out.v1.s, s2->n2.out.v2.s) || \
					  s1->n1.out.v1.private_length != s2->n2.out.v2.private_length) { \
        printf("%s/%s [%s/%d] != %s/%s [%s/%d] at %s(%d)\n", \
               #n1, #v1, s1->n1.out.v1.s, s1->n1.out.v1.private_length, \
               #n2, #v2, s2->n2.out.v2.s, s2->n2.out.v2.private_length, \
	       __FILE__, __LINE__); \
        ret = false; \
}} while(0)

#define STRUCT_EQUAL(n1, v1, n2, v2) do {if (memcmp(&s1->n1.out.v1,&s2->n2.out.v2,sizeof(s1->n1.out.v1))) { \
        printf("%s/%s != %s/%s at %s(%d)\n", \
               #n1, #v1, \
               #n2, #v2, \
	       __FILE__, __LINE__); \
        ret = false; \
}} while(0)

/* used to find hints on unknown values - and to make sure 
   we zero-fill */
#if 0 /* unused */
#define VAL_UNKNOWN(n1, v1) do {if (s1->n1.out.v1 != 0) { \
        printf("%s/%s non-zero unknown - %u (0x%x) at %s(%d)\n", \
               #n1, #v1, \
	       (unsigned int)s1->n1.out.v1, \
	       (unsigned int)s1->n1.out.v1, \
	       __FILE__, __LINE__); \
        ret = false; \
}} while(0)
#endif

/* basic testing of all RAW_FILEINFO_* calls 
   for each call we test that it succeeds, and where possible test 
   for consistency between the calls. 
*/
static bool torture_raw_qfileinfo_internals(struct torture_context *torture, 
					    TALLOC_CTX *mem_ctx, 	
					    struct smbcli_tree *tree, 
					    int fnum, const char *fname,
					    bool is_ipc)
{
	int i;
	bool ret = true;
	int count;
	union smb_fileinfo *s1, *s2;	
	NTTIME correct_time;
	uint64_t correct_size;
	uint32_t correct_attrib;
	const char *correct_name;
	bool skip_streams = false;

	/* scan all the fileinfo and pathinfo levels */
	for (i=0; levels[i].name; i++) {
		if (!levels[i].only_paths) {
			levels[i].fnum_finfo.generic.level = levels[i].level;
			levels[i].fnum_finfo.generic.in.file.fnum = fnum;
			levels[i].fnum_status = smb_raw_fileinfo(tree, mem_ctx, 
								 &levels[i].fnum_finfo);
		}

		if (!levels[i].only_handles) {
			levels[i].fname_finfo.generic.level = levels[i].level;
			levels[i].fname_finfo.generic.in.file.path = talloc_strdup(mem_ctx, fname);
			levels[i].fname_status = smb_raw_pathinfo(tree, mem_ctx, 
								  &levels[i].fname_finfo);
		}
	}

	/* check for completely broken levels */
	for (count=i=0; levels[i].name; i++) {
		uint32_t cap = tree->session->transport->negotiate.capabilities;
		/* see if this server claims to support this level */
		if ((cap & levels[i].capability_mask) != levels[i].capability_mask) {
			continue;
		}

		if (is_ipc) {
			if (levels[i].expected_ipc_access_denied && NT_STATUS_EQUAL(NT_STATUS_ACCESS_DENIED, levels[i].fname_status)) {
			} else if (!levels[i].only_handles &&
				   NT_STATUS_EQUAL(levels[i].fname_status,
				   NT_STATUS_NOT_SUPPORTED)) {
				torture_warning(torture, "fname level %s %s",
					levels[i].name,
					nt_errstr(levels[i].fname_status));
				continue;
			} else if (!levels[i].only_handles && !NT_STATUS_EQUAL(NT_STATUS_INVALID_DEVICE_REQUEST, levels[i].fname_status)) {
				printf("ERROR: fname level %s failed, expected NT_STATUS_INVALID_DEVICE_REQUEST - %s\n", 
				       levels[i].name, nt_errstr(levels[i].fname_status));
				count++;
			}
			if (!levels[i].only_paths &&
			   (NT_STATUS_EQUAL(levels[i].fnum_status,
			    NT_STATUS_NOT_SUPPORTED) ||
			    NT_STATUS_EQUAL(levels[i].fnum_status,
			    NT_STATUS_NOT_IMPLEMENTED))) {
				torture_warning(torture, "fnum level %s %s",
					levels[i].name,
					nt_errstr(levels[i].fnum_status));
				continue;
			}
			if (!levels[i].only_paths && !NT_STATUS_EQUAL(levels[i].expected_ipc_fnum_status, levels[i].fnum_status)) {
				printf("ERROR: fnum level %s failed, expected %s - %s\n", 
				       levels[i].name, nt_errstr(levels[i].expected_ipc_fnum_status), 
				       nt_errstr(levels[i].fnum_status));
				count++;
			}
		} else {
			if (!levels[i].only_paths &&
			   (NT_STATUS_EQUAL(levels[i].fnum_status,
			    NT_STATUS_NOT_SUPPORTED) ||
			    NT_STATUS_EQUAL(levels[i].fnum_status,
			    NT_STATUS_NOT_IMPLEMENTED))) {
				torture_warning(torture, "fnum level %s %s",
					levels[i].name,
					nt_errstr(levels[i].fnum_status));
				continue;
			}

			if (!levels[i].only_handles &&
			   (NT_STATUS_EQUAL(levels[i].fname_status,
			    NT_STATUS_NOT_SUPPORTED) ||
			    NT_STATUS_EQUAL(levels[i].fname_status,
			    NT_STATUS_NOT_IMPLEMENTED))) {
                                torture_warning(torture, "fname level %s %s",
					levels[i].name,
					nt_errstr(levels[i].fname_status));
				continue;
			}

			if (!levels[i].only_paths && !NT_STATUS_IS_OK(levels[i].fnum_status)) {
				printf("ERROR: fnum level %s failed - %s\n", 
				       levels[i].name, nt_errstr(levels[i].fnum_status));
				count++;
			}
			if (!levels[i].only_handles && !NT_STATUS_IS_OK(levels[i].fname_status)) {
				printf("ERROR: fname level %s failed - %s\n", 
				       levels[i].name, nt_errstr(levels[i].fname_status));
				count++;
			}
		}
		
	}

	if (count != 0) {
		ret = false;
		printf("%d levels failed\n", count);
		if (count > 35) {
			torture_fail(torture, "too many level failures - giving up");
		}
	}

	/* see if we can do streams */
	s1 = fnum_find("STREAM_INFO");
	if (!s1 || s1->stream_info.out.num_streams == 0) {
		if (!is_ipc) {
			printf("STREAM_INFO broken (%d) - skipping streams checks\n",
			       s1 ? s1->stream_info.out.num_streams : -1);
		}
		skip_streams = true;
	}	


	/* this code is incredibly repititive but doesn't lend itself to loops, so
	   we use lots of macros to make it less painful */

	/* first off we check the levels that are supposed to be aliases. It will be quite rare for 
	   this code to fail, but we need to check it for completeness */



#define ALIAS_CHECK(sname1, sname2) \
	do { \
		s1 = fnum_find(sname1);	 s2 = fnum_find(sname2); \
		if (s1 && s2) { INFO_CHECK } \
		s1 = fname_find(is_ipc, sname1); s2 = fname_find(is_ipc, sname2); \
		if (s1 && s2) { INFO_CHECK } \
		s1 = fnum_find(sname1);	 s2 = fname_find(is_ipc, sname2); \
		if (s1 && s2) { INFO_CHECK } \
	} while (0)

#define INFO_CHECK \
		STRUCT_EQUAL(basic_info,  create_time, basic_info, create_time); \
		STRUCT_EQUAL(basic_info,  access_time, basic_info, access_time); \
		STRUCT_EQUAL(basic_info,  write_time,  basic_info, write_time); \
		STRUCT_EQUAL(basic_info,  change_time, basic_info, change_time); \
		VAL_EQUAL   (basic_info,  attrib,      basic_info, attrib);

	ALIAS_CHECK("BASIC_INFO", "BASIC_INFORMATION");

#undef INFO_CHECK
#define INFO_CHECK \
		VAL_EQUAL(standard_info,  alloc_size,     standard_info, alloc_size); \
		VAL_EQUAL(standard_info,  size,           standard_info, size); \
		VAL_EQUAL(standard_info,  nlink,          standard_info, nlink); \
		VAL_EQUAL(standard_info,  delete_pending, standard_info, delete_pending); \
		VAL_EQUAL(standard_info,  directory,      standard_info, directory);

	ALIAS_CHECK("STANDARD_INFO", "STANDARD_INFORMATION");

#undef INFO_CHECK
#define INFO_CHECK \
		VAL_EQUAL(ea_info,  ea_size,     ea_info, ea_size);

	ALIAS_CHECK("EA_INFO", "EA_INFORMATION");

#undef INFO_CHECK
#define INFO_CHECK \
		STR_EQUAL(name_info,  fname,   name_info, fname);

	ALIAS_CHECK("NAME_INFO", "NAME_INFORMATION");

#undef INFO_CHECK
#define INFO_CHECK \
		STRUCT_EQUAL(all_info,  create_time,           all_info, create_time); \
		STRUCT_EQUAL(all_info,  access_time,           all_info, access_time); \
		STRUCT_EQUAL(all_info,  write_time,            all_info, write_time); \
		STRUCT_EQUAL(all_info,  change_time,           all_info, change_time); \
		   VAL_EQUAL(all_info,  attrib,                all_info, attrib); \
		   VAL_EQUAL(all_info,  alloc_size,            all_info, alloc_size); \
		   VAL_EQUAL(all_info,  size,                  all_info, size); \
		   VAL_EQUAL(all_info,  nlink,                 all_info, nlink); \
		   VAL_EQUAL(all_info,  delete_pending,        all_info, delete_pending); \
		   VAL_EQUAL(all_info,  directory,             all_info, directory); \
		   VAL_EQUAL(all_info,  ea_size,               all_info, ea_size); \
		   STR_EQUAL(all_info,  fname,                 all_info, fname);

	ALIAS_CHECK("ALL_INFO", "ALL_INFORMATION");

#undef INFO_CHECK
#define INFO_CHECK \
		VAL_EQUAL(compression_info, compressed_size,compression_info, compressed_size); \
		VAL_EQUAL(compression_info, format,         compression_info, format); \
		VAL_EQUAL(compression_info, unit_shift,     compression_info, unit_shift); \
		VAL_EQUAL(compression_info, chunk_shift,    compression_info, chunk_shift); \
		VAL_EQUAL(compression_info, cluster_shift,  compression_info, cluster_shift);

	ALIAS_CHECK("COMPRESSION_INFO", "COMPRESSION_INFORMATION");


#undef INFO_CHECK
#define INFO_CHECK \
		STR_EQUAL(alt_name_info,  fname,   alt_name_info, fname);

	ALIAS_CHECK("ALT_NAME_INFO", "ALT_NAME_INFORMATION");


#define TIME_CHECK_NT(sname, stype, tfield) do { \
	s1 = fnum_find(sname); \
	if (s1 && memcmp(&s1->stype.out.tfield, &correct_time, sizeof(correct_time)) != 0) { \
		printf("(%d) handle %s/%s incorrect - %s should be %s\n", __LINE__, #stype, #tfield,  \
		       nt_time_string(mem_ctx, s1->stype.out.tfield), \
		       nt_time_string(mem_ctx, correct_time)); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && memcmp(&s1->stype.out.tfield, &correct_time, sizeof(correct_time)) != 0) { \
		printf("(%d) path %s/%s incorrect - %s should be %s\n", __LINE__, #stype, #tfield,  \
		       nt_time_string(mem_ctx, s1->stype.out.tfield), \
		       nt_time_string(mem_ctx, correct_time)); \
		ret = false; \
	}} while (0)

#define TIME_CHECK_DOS(sname, stype, tfield) do { \
	s1 = fnum_find(sname); \
	if (s1 && dos_nt_time_cmp(s1->stype.out.tfield, correct_time) != 0) { \
		printf("(%d) handle %s/%s incorrect - %s should be %s\n", __LINE__, #stype, #tfield,  \
		       timestring(mem_ctx, s1->stype.out.tfield), \
		       nt_time_string(mem_ctx, correct_time)); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && dos_nt_time_cmp(s1->stype.out.tfield, correct_time) != 0) { \
		printf("(%d) path %s/%s incorrect - %s should be %s\n", __LINE__, #stype, #tfield,  \
		       timestring(mem_ctx, s1->stype.out.tfield), \
		       nt_time_string(mem_ctx, correct_time)); \
		ret = false; \
	}} while (0)

#if 0 /* unused */
#define TIME_CHECK_UNX(sname, stype, tfield) do { \
	s1 = fnum_find(sname); \
	if (s1 && unx_nt_time_cmp(s1->stype.out.tfield, correct_time) != 0) { \
		printf("(%d) handle %s/%s incorrect - %s should be %s\n", __LINE__, #stype, #tfield,  \
		       timestring(mem_ctx, s1->stype.out.tfield), \
		       nt_time_string(mem_ctx, correct_time)); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && unx_nt_time_cmp(s1->stype.out.tfield, correct_time) != 0) { \
		printf("(%d) path %s/%s incorrect - %s should be %s\n", __LINE__, #stype, #tfield,  \
		       timestring(mem_ctx, s1->stype.out.tfield), \
		       nt_time_string(mem_ctx, correct_time)); \
		ret = false; \
	}} while (0)
#endif

	/* now check that all the times that are supposed to be equal are correct */
	s1 = fnum_find("BASIC_INFO");
	correct_time = s1->basic_info.out.create_time;
	torture_comment(torture, "create_time: %s\n", nt_time_string(mem_ctx, correct_time));

	TIME_CHECK_NT ("BASIC_INFO",               basic_info, create_time);
	TIME_CHECK_NT ("BASIC_INFORMATION",        basic_info, create_time);
	TIME_CHECK_DOS("GETATTRE",                 getattre,   create_time);
	TIME_CHECK_DOS("STANDARD",                 standard,   create_time);
	TIME_CHECK_DOS("EA_SIZE",                  ea_size,    create_time);
	TIME_CHECK_NT ("ALL_INFO",                 all_info,   create_time);
	TIME_CHECK_NT ("NETWORK_OPEN_INFORMATION", network_open_information, create_time);

	s1 = fnum_find("BASIC_INFO");
	correct_time = s1->basic_info.out.access_time;
	torture_comment(torture, "access_time: %s\n", nt_time_string(mem_ctx, correct_time));

	TIME_CHECK_NT ("BASIC_INFO",               basic_info, access_time);
	TIME_CHECK_NT ("BASIC_INFORMATION",        basic_info, access_time);
	TIME_CHECK_DOS("GETATTRE",                 getattre,   access_time);
	TIME_CHECK_DOS("STANDARD",                 standard,   access_time);
	TIME_CHECK_DOS("EA_SIZE",                  ea_size,    access_time);
	TIME_CHECK_NT ("ALL_INFO",                 all_info,   access_time);
	TIME_CHECK_NT ("NETWORK_OPEN_INFORMATION", network_open_information, access_time);

	s1 = fnum_find("BASIC_INFO");
	correct_time = s1->basic_info.out.write_time;
	torture_comment(torture, "write_time : %s\n", nt_time_string(mem_ctx, correct_time));

	TIME_CHECK_NT ("BASIC_INFO",               basic_info, write_time);
	TIME_CHECK_NT ("BASIC_INFORMATION",        basic_info, write_time);
	TIME_CHECK_DOS("GETATTR",                  getattr,    write_time);
	TIME_CHECK_DOS("GETATTRE",                 getattre,   write_time);
	TIME_CHECK_DOS("STANDARD",                 standard,   write_time);
	TIME_CHECK_DOS("EA_SIZE",                  ea_size,    write_time);
	TIME_CHECK_NT ("ALL_INFO",                 all_info,   write_time);
	TIME_CHECK_NT ("NETWORK_OPEN_INFORMATION", network_open_information, write_time);

	s1 = fnum_find("BASIC_INFO");
	correct_time = s1->basic_info.out.change_time;
	torture_comment(torture, "change_time: %s\n", nt_time_string(mem_ctx, correct_time));

	TIME_CHECK_NT ("BASIC_INFO",               basic_info, change_time);
	TIME_CHECK_NT ("BASIC_INFORMATION",        basic_info, change_time);
	TIME_CHECK_NT ("ALL_INFO",                 all_info,   change_time);
	TIME_CHECK_NT ("NETWORK_OPEN_INFORMATION", network_open_information, change_time);


#define SIZE_CHECK(sname, stype, tfield) do { \
	s1 = fnum_find(sname); \
	if (s1 && s1->stype.out.tfield != correct_size) { \
		printf("(%d) handle %s/%s incorrect - %u should be %u\n", __LINE__, #stype, #tfield,  \
		       (unsigned int)s1->stype.out.tfield, \
		       (unsigned int)correct_size); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && s1->stype.out.tfield != correct_size) { \
		printf("(%d) path %s/%s incorrect - %u should be %u\n", __LINE__, #stype, #tfield,  \
		       (unsigned int)s1->stype.out.tfield, \
		       (unsigned int)correct_size); \
		ret = false; \
	}} while (0)

	s1 = fnum_find("STANDARD_INFO");
	correct_size = s1->standard_info.out.size;
	torture_comment(torture, "size: %u\n", (unsigned int)correct_size);
	
	SIZE_CHECK("GETATTR",                  getattr,                  size);
	SIZE_CHECK("GETATTRE",                 getattre,                 size);
	SIZE_CHECK("STANDARD",                 standard,                 size);
	SIZE_CHECK("EA_SIZE",                  ea_size,                  size);
	SIZE_CHECK("STANDARD_INFO",            standard_info,            size);
	SIZE_CHECK("STANDARD_INFORMATION",     standard_info,            size);
	SIZE_CHECK("ALL_INFO",                 all_info,                 size);
	SIZE_CHECK("ALL_INFORMATION",          all_info,                 size);
	SIZE_CHECK("COMPRESSION_INFO",         compression_info,         compressed_size);
	SIZE_CHECK("COMPRESSION_INFORMATION",  compression_info,         compressed_size);
	SIZE_CHECK("NETWORK_OPEN_INFORMATION", network_open_information, size);
	if (!skip_streams) {
		SIZE_CHECK("STREAM_INFO",              stream_info,   streams[0].size);
		SIZE_CHECK("STREAM_INFORMATION",       stream_info,   streams[0].size);
	}


	s1 = fnum_find("STANDARD_INFO");
	correct_size = s1->standard_info.out.alloc_size;
	torture_comment(torture, "alloc_size: %u\n", (unsigned int)correct_size);
	
	SIZE_CHECK("GETATTRE",                 getattre,                 alloc_size);
	SIZE_CHECK("STANDARD",                 standard,                 alloc_size);
	SIZE_CHECK("EA_SIZE",                  ea_size,                  alloc_size);
	SIZE_CHECK("STANDARD_INFO",            standard_info,            alloc_size);
	SIZE_CHECK("STANDARD_INFORMATION",     standard_info,            alloc_size);
	SIZE_CHECK("ALL_INFO",                 all_info,                 alloc_size);
	SIZE_CHECK("ALL_INFORMATION",          all_info,                 alloc_size);
	SIZE_CHECK("NETWORK_OPEN_INFORMATION", network_open_information, alloc_size);
	if (!skip_streams) {
		SIZE_CHECK("STREAM_INFO",              stream_info,   streams[0].alloc_size);
		SIZE_CHECK("STREAM_INFORMATION",       stream_info,   streams[0].alloc_size);
	}

#define ATTRIB_CHECK(sname, stype, tfield) do { \
	s1 = fnum_find(sname); \
	if (s1 && s1->stype.out.tfield != correct_attrib) { \
		printf("(%d) handle %s/%s incorrect - 0x%x should be 0x%x\n", __LINE__, #stype, #tfield,  \
		       (unsigned int)s1->stype.out.tfield, \
		       (unsigned int)correct_attrib); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && s1->stype.out.tfield != correct_attrib) { \
		printf("(%d) path %s/%s incorrect - 0x%x should be 0x%x\n", __LINE__, #stype, #tfield,  \
		       (unsigned int)s1->stype.out.tfield, \
		       (unsigned int)correct_attrib); \
		ret = false; \
	}} while (0)

	s1 = fnum_find("BASIC_INFO");
	correct_attrib = s1->basic_info.out.attrib;
	torture_comment(torture, "attrib: 0x%x\n", (unsigned int)correct_attrib);
	
	ATTRIB_CHECK("GETATTR",                   getattr,                   attrib);
	if (!is_ipc) {
		ATTRIB_CHECK("GETATTRE",                  getattre,                  attrib);
		ATTRIB_CHECK("STANDARD",                  standard,                  attrib);
		ATTRIB_CHECK("EA_SIZE",                   ea_size,                   attrib);
	}
	ATTRIB_CHECK("BASIC_INFO",                basic_info,                attrib);
	ATTRIB_CHECK("BASIC_INFORMATION",         basic_info,                attrib);
	ATTRIB_CHECK("ALL_INFO",                  all_info,                  attrib);
	ATTRIB_CHECK("ALL_INFORMATION",           all_info,                  attrib);
	ATTRIB_CHECK("NETWORK_OPEN_INFORMATION",  network_open_information,  attrib);
	ATTRIB_CHECK("ATTRIBUTE_TAG_INFORMATION", attribute_tag_information, attrib);

	correct_name = fname;
	torture_comment(torture, "name: %s\n", correct_name);

#define NAME_CHECK(sname, stype, tfield, flags) do { \
	s1 = fnum_find(sname); \
	if (s1 && (strcmp_safe(s1->stype.out.tfield.s, correct_name) != 0 || \
	    		wire_bad_flags(&s1->stype.out.tfield, flags, tree->session->transport))) { \
		printf("(%d) handle %s/%s incorrect - '%s/%d'\n", __LINE__, #stype, #tfield,  \
		       s1->stype.out.tfield.s, s1->stype.out.tfield.private_length); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && (strcmp_safe(s1->stype.out.tfield.s, correct_name) != 0 || \
	    		wire_bad_flags(&s1->stype.out.tfield, flags, tree->session->transport))) { \
		printf("(%d) path %s/%s incorrect - '%s/%d'\n", __LINE__, #stype, #tfield,  \
		       s1->stype.out.tfield.s, s1->stype.out.tfield.private_length); \
		ret = false; \
	}} while (0)

	NAME_CHECK("NAME_INFO",        name_info, fname, STR_UNICODE);
	NAME_CHECK("NAME_INFORMATION", name_info, fname, STR_UNICODE);

	/* the ALL_INFO file name is the full path on the filesystem */
	s1 = fnum_find("ALL_INFO");
	if (s1 && !s1->all_info.out.fname.s) {
		torture_fail(torture, "ALL_INFO didn't give a filename");
	}
	if (s1 && s1->all_info.out.fname.s) {
		char *p = strrchr(s1->all_info.out.fname.s, '\\');
		if (!p) {
			printf("Not a full path in all_info/fname? - '%s'\n", 
			       s1->all_info.out.fname.s);
			ret = false;
		} else {
			if (strcmp_safe(correct_name, p) != 0) {
				printf("incorrect basename in all_info/fname - '%s'\n",
				       s1->all_info.out.fname.s);
				ret = false;
			}
		}
		if (wire_bad_flags(&s1->all_info.out.fname, STR_UNICODE, tree->session->transport)) {
			printf("Should not null terminate all_info/fname\n");
			ret = false;
		}
	}

	s1 = fnum_find("ALT_NAME_INFO");
	if (s1) {
		correct_name = s1->alt_name_info.out.fname.s;
	}

	if (!correct_name) {
		torture_comment(torture, "no alternate name information\n");
	} else {
		torture_comment(torture, "alt_name: %s\n", correct_name);
		
		NAME_CHECK("ALT_NAME_INFO",        alt_name_info, fname, STR_UNICODE);
		NAME_CHECK("ALT_NAME_INFORMATION", alt_name_info, fname, STR_UNICODE);
		
		/* and make sure we can open by alternate name */
		smbcli_close(tree, fnum);
		fnum = smbcli_nt_create_full(tree, correct_name, 0, 
					     SEC_RIGHTS_FILE_ALL,
					     FILE_ATTRIBUTE_NORMAL,
					     NTCREATEX_SHARE_ACCESS_DELETE|
					     NTCREATEX_SHARE_ACCESS_READ|
					     NTCREATEX_SHARE_ACCESS_WRITE, 
					     NTCREATEX_DISP_OVERWRITE_IF, 
					     0, 0);
		if (fnum == -1) {
			printf("Unable to open by alt_name - %s\n", smbcli_errstr(tree));
			ret = false;
		}
		
		if (!skip_streams) {
			correct_name = "::$DATA";
			torture_comment(torture, "stream_name: %s\n", correct_name);
			
			NAME_CHECK("STREAM_INFO",        stream_info, streams[0].stream_name, STR_UNICODE);
			NAME_CHECK("STREAM_INFORMATION", stream_info, streams[0].stream_name, STR_UNICODE);
		}
	}
		
	/* make sure the EAs look right */
	s1 = fnum_find("ALL_EAS");
	s2 = fnum_find("ALL_INFO");
	if (s1) {
		for (i=0;i<s1->all_eas.out.num_eas;i++) {
			printf("  flags=%d %s=%*.*s\n", 
			       s1->all_eas.out.eas[i].flags,
			       s1->all_eas.out.eas[i].name.s,
			       (int)s1->all_eas.out.eas[i].value.length,
			       (int)s1->all_eas.out.eas[i].value.length,
			       s1->all_eas.out.eas[i].value.data);
		}
	}
	if (s1 && s2) {
		if (s1->all_eas.out.num_eas == 0) {
			if (s2->all_info.out.ea_size != 0) {
				printf("ERROR: num_eas==0 but fnum all_info.out.ea_size == %d\n",
				       s2->all_info.out.ea_size);
			}
		} else {
			if (s2->all_info.out.ea_size != 
			    ea_list_size(s1->all_eas.out.num_eas, s1->all_eas.out.eas)) {
				printf("ERROR: ea_list_size=%d != fnum all_info.out.ea_size=%d\n",
				       (int)ea_list_size(s1->all_eas.out.num_eas, s1->all_eas.out.eas),
				       (int)s2->all_info.out.ea_size);
			}
		}
	}
	s2 = fname_find(is_ipc, "ALL_EAS");
	if (s2) {
		VAL_EQUAL(all_eas, num_eas, all_eas, num_eas);
		for (i=0;i<s1->all_eas.out.num_eas;i++) {
			VAL_EQUAL(all_eas, eas[i].flags, all_eas, eas[i].flags);
			STR_EQUAL(all_eas, eas[i].name, all_eas, eas[i].name);
			VAL_EQUAL(all_eas, eas[i].value.length, all_eas, eas[i].value.length);
		}
	}

#define VAL_CHECK(sname1, stype1, tfield1, sname2, stype2, tfield2) do { \
	s1 = fnum_find(sname1); s2 = fnum_find(sname2); \
	if (s1 && s2 && s1->stype1.out.tfield1 != s2->stype2.out.tfield2) { \
		printf("(%d) handle %s/%s != %s/%s - 0x%x vs 0x%x\n", __LINE__, \
                       #stype1, #tfield1, #stype2, #tfield2,  \
		       s1->stype1.out.tfield1, s2->stype2.out.tfield2); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname1); s2 = fname_find(is_ipc, sname2); \
	if (s1 && s2 && s1->stype1.out.tfield1 != s2->stype2.out.tfield2) { \
		printf("(%d) path %s/%s != %s/%s - 0x%x vs 0x%x\n", __LINE__, \
                       #stype1, #tfield1, #stype2, #tfield2,  \
		       s1->stype1.out.tfield1, s2->stype2.out.tfield2); \
		ret = false; \
	} \
	s1 = fnum_find(sname1); s2 = fname_find(is_ipc, sname2); \
	if (s1 && s2 && s1->stype1.out.tfield1 != s2->stype2.out.tfield2) { \
		printf("(%d) handle %s/%s != path %s/%s - 0x%x vs 0x%x\n", __LINE__, \
                       #stype1, #tfield1, #stype2, #tfield2,  \
		       s1->stype1.out.tfield1, s2->stype2.out.tfield2); \
		ret = false; \
	} \
	s1 = fname_find(is_ipc, sname1); s2 = fnum_find(sname2); \
	if (s1 && s2 && s1->stype1.out.tfield1 != s2->stype2.out.tfield2) { \
		printf("(%d) path %s/%s != handle %s/%s - 0x%x vs 0x%x\n", __LINE__, \
                       #stype1, #tfield1, #stype2, #tfield2,  \
		       s1->stype1.out.tfield1, s2->stype2.out.tfield2); \
		ret = false; \
	}} while (0)

	VAL_CHECK("STANDARD_INFO", standard_info, delete_pending, 
		  "ALL_INFO",      all_info,      delete_pending);
	VAL_CHECK("STANDARD_INFO", standard_info, directory, 
		  "ALL_INFO",      all_info,      directory);
	VAL_CHECK("STANDARD_INFO", standard_info, nlink, 
		  "ALL_INFO",      all_info,      nlink);
	s1 = fnum_find("BASIC_INFO");
	if (s1 && is_ipc) {
		if (s1->basic_info.out.attrib != FILE_ATTRIBUTE_NORMAL) {
			printf("(%d) attrib basic_info/nlink incorrect - %d should be %d\n", __LINE__, s1->basic_info.out.attrib, FILE_ATTRIBUTE_NORMAL);
			ret = false;
		}
	}
	s1 = fnum_find("STANDARD_INFO");
	if (s1 && is_ipc) {
		if (s1->standard_info.out.nlink != 1) {
			printf("(%d) nlinks standard_info/nlink incorrect - %d should be 1\n", __LINE__, s1->standard_info.out.nlink);
			ret = false;
		}
		if (s1->standard_info.out.delete_pending != 1) {
			printf("(%d) nlinks standard_info/delete_pending incorrect - %d should be 1\n", __LINE__, s1->standard_info.out.delete_pending);
			ret = false;
		}
	}
	VAL_CHECK("EA_INFO",       ea_info,       ea_size, 
		  "ALL_INFO",      all_info,      ea_size);
	if (!is_ipc) {
		VAL_CHECK("EA_SIZE",       ea_size,       ea_size, 
			  "ALL_INFO",      all_info,      ea_size);
	}

#define NAME_PATH_CHECK(sname, stype, field) do { \
	s1 = fname_find(is_ipc, sname); s2 = fnum_find(sname); \
        if (s1 && s2) { \
		VAL_EQUAL(stype, field, stype, field); \
	} \
} while (0)


	s1 = fnum_find("INTERNAL_INFORMATION");
	if (s1) {
		torture_comment(torture, "file_id=%.0f\n", (double)s1->internal_information.out.file_id);
	}

	NAME_PATH_CHECK("INTERNAL_INFORMATION", internal_information, file_id);
	NAME_PATH_CHECK("POSITION_INFORMATION", position_information, position);
	if (s1 && s2) {
		printf("fnum pos = %.0f, fname pos = %.0f\n",
		       (double)s2->position_information.out.position,
		       (double)s1->position_information.out.position );
	}
	NAME_PATH_CHECK("MODE_INFORMATION", mode_information, mode);
	NAME_PATH_CHECK("ALIGNMENT_INFORMATION", alignment_information, alignment_requirement);
	NAME_PATH_CHECK("ATTRIBUTE_TAG_INFORMATION", attribute_tag_information, attrib);
	NAME_PATH_CHECK("ATTRIBUTE_TAG_INFORMATION", attribute_tag_information, reparse_tag);

#if 0
	/* these are expected to differ */
	NAME_PATH_CHECK("ACCESS_INFORMATION", access_information, access_flags);
#endif

#if 0 /* unused */
#define UNKNOWN_CHECK(sname, stype, tfield) do { \
	s1 = fnum_find(sname); \
	if (s1 && s1->stype.out.tfield != 0) { \
		printf("(%d) handle %s/%s unknown != 0 (0x%x)\n", __LINE__, \
                       #stype, #tfield, \
		       (unsigned int)s1->stype.out.tfield); \
	} \
	s1 = fname_find(is_ipc, sname); \
	if (s1 && s1->stype.out.tfield != 0) { \
		printf("(%d) path %s/%s unknown != 0 (0x%x)\n", __LINE__, \
                       #stype, #tfield, \
		       (unsigned int)s1->stype.out.tfield); \
	}} while (0)
#endif
	/* now get a bit fancier .... */
	
	/* when we set the delete disposition then the link count should drop
	   to 0 and delete_pending should be 1 */
	
	return ret;
}

/* basic testing of all RAW_FILEINFO_* calls 
   for each call we test that it succeeds, and where possible test 
   for consistency between the calls. 
*/
bool torture_raw_qfileinfo(struct torture_context *torture, 
						   struct smbcli_state *cli)
{
	int fnum;
	bool ret;
	const char *fname = "\\torture_qfileinfo.txt";

	fnum = create_complex_file(cli, torture, fname);
	if (fnum == -1) {
		printf("ERROR: open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		return false;
	}

	ret = torture_raw_qfileinfo_internals(torture, torture, cli->tree, fnum, fname, false /* is_ipc */);
	
	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	return ret;
}

bool torture_raw_qfileinfo_pipe(struct torture_context *torture, 
				struct smbcli_state *cli)
{
	bool ret = true;
	int fnum;
	const char *fname = "\\lsass";
	struct dcerpc_pipe *p;
	struct smbcli_tree *ipc_tree;
	NTSTATUS status;

	if (!(p = dcerpc_pipe_init(torture, cli->tree->session->transport->socket->event.ctx))) {
		return false;
	}

	status = dcerpc_pipe_open_smb(p, cli->tree, fname);
	torture_assert_ntstatus_ok(torture, status, "dcerpc_pipe_open_smb failed");

	ipc_tree = dcerpc_smb_tree(p->conn);
	fnum = dcerpc_smb_fnum(p->conn);

	ret = torture_raw_qfileinfo_internals(torture, torture, ipc_tree, fnum, fname, true /* is_ipc */);
	
	talloc_free(p);
	return ret;
}
