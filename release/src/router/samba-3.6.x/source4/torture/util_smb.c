/* 
   Unix SMB/CIFS implementation.
   SMB torture tester utility functions
   Copyright (C) Andrew Tridgell 2003
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
#include "lib/cmdline/popt_common.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/raw/ioctl.h"
#include "libcli/libcli.h"
#include "system/filesys.h"
#include "system/shmem.h"
#include "system/wait.h"
#include "system/time.h"
#include "torture/torture.h"
#include "../lib/util/dlinklist.h"
#include "libcli/resolve/resolve.h"
#include "param/param.h"
#include "libcli/security/security.h"
#include "libcli/util/clilsa.h"


/**
  setup a directory ready for a test
*/
_PUBLIC_ bool torture_setup_dir(struct smbcli_state *cli, const char *dname)
{
	smb_raw_exit(cli->session);
	if (smbcli_deltree(cli->tree, dname) == -1 ||
	    NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, dname))) {
		printf("Unable to setup %s - %s\n", dname, smbcli_errstr(cli->tree));
		return false;
	}
	return true;
}

/*
  create a directory, returning a handle to it
*/
NTSTATUS create_directory_handle(struct smbcli_tree *tree, const char *dname, int *fnum)
{
	NTSTATUS status;
	union smb_open io;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_named_const(tree, 0, "create_directory_handle");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = dname;

	status = smb_raw_open(tree, mem_ctx, &io);
	talloc_free(mem_ctx);

	if (NT_STATUS_IS_OK(status)) {
		*fnum = io.ntcreatex.out.file.fnum;
	}

	return status;
}


/**
  sometimes we need a fairly complex file to work with, so we can test
  all possible attributes. 
*/
_PUBLIC_ int create_complex_file(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *fname)
{
	int fnum;
	char buf[7] = "abc";
	union smb_setfileinfo setfile;
	union smb_fileinfo fileinfo;
	time_t t = (time(NULL) & ~1);
	NTSTATUS status;

	smbcli_unlink(cli->tree, fname);
	fnum = smbcli_nt_create_full(cli->tree, fname, 0, 
				     SEC_RIGHTS_FILE_ALL,
				     FILE_ATTRIBUTE_NORMAL,
				     NTCREATEX_SHARE_ACCESS_DELETE|
				     NTCREATEX_SHARE_ACCESS_READ|
				     NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OVERWRITE_IF,
				     0, 0);
	if (fnum == -1) return -1;

	smbcli_write(cli->tree, fnum, 0, buf, 0, sizeof(buf));

	if (strchr(fname, ':') == NULL) {
		/* setup some EAs */
		setfile.generic.level = RAW_SFILEINFO_EA_SET;
		setfile.generic.in.file.fnum = fnum;
		setfile.ea_set.in.num_eas = 2;	
		setfile.ea_set.in.eas = talloc_array(mem_ctx, struct ea_struct, 2);
		setfile.ea_set.in.eas[0].flags = 0;
		setfile.ea_set.in.eas[0].name.s = "EAONE";
		setfile.ea_set.in.eas[0].value = data_blob_talloc(mem_ctx, "VALUE1", 6);
		setfile.ea_set.in.eas[1].flags = 0;
		setfile.ea_set.in.eas[1].name.s = "SECONDEA";
		setfile.ea_set.in.eas[1].value = data_blob_talloc(mem_ctx, "ValueTwo", 8);
		status = smb_raw_setfileinfo(cli->tree, &setfile);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to setup EAs\n");
		}
	}

	/* make sure all the timestamps aren't the same */
	ZERO_STRUCT(setfile);
	setfile.generic.level = RAW_SFILEINFO_BASIC_INFO;
	setfile.generic.in.file.fnum = fnum;

	unix_to_nt_time(&setfile.basic_info.in.create_time,
	    t + 9*30*24*60*60);
	unix_to_nt_time(&setfile.basic_info.in.access_time,
	    t + 6*30*24*60*60);
	unix_to_nt_time(&setfile.basic_info.in.write_time,
	    t + 3*30*24*60*60);

	status = smb_raw_setfileinfo(cli->tree, &setfile);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to setup file times - %s\n", nt_errstr(status));
	}

	/* make sure all the timestamps aren't the same */
	fileinfo.generic.level = RAW_FILEINFO_BASIC_INFO;
	fileinfo.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &fileinfo);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to query file times - %s\n", nt_errstr(status));
	}

	if (setfile.basic_info.in.create_time != fileinfo.basic_info.out.create_time) {
		printf("create_time not setup correctly\n");
	}
	if (setfile.basic_info.in.access_time != fileinfo.basic_info.out.access_time) {
		printf("access_time not setup correctly\n");
	}
	if (setfile.basic_info.in.write_time != fileinfo.basic_info.out.write_time) {
		printf("write_time not setup correctly\n");
	}

	return fnum;
}


/*
  sometimes we need a fairly complex directory to work with, so we can test
  all possible attributes. 
*/
int create_complex_dir(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *dname)
{
	int fnum;
	union smb_setfileinfo setfile;
	union smb_fileinfo fileinfo;
	time_t t = (time(NULL) & ~1);
	NTSTATUS status;

	smbcli_deltree(cli->tree, dname);
	fnum = smbcli_nt_create_full(cli->tree, dname, 0, 
				     SEC_RIGHTS_DIR_ALL,
				     FILE_ATTRIBUTE_DIRECTORY,
				     NTCREATEX_SHARE_ACCESS_READ|
				     NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OPEN_IF,
				     NTCREATEX_OPTIONS_DIRECTORY, 0);
	if (fnum == -1) return -1;

	if (strchr(dname, ':') == NULL) {
		/* setup some EAs */
		setfile.generic.level = RAW_SFILEINFO_EA_SET;
		setfile.generic.in.file.fnum = fnum;
		setfile.ea_set.in.num_eas = 2;	
		setfile.ea_set.in.eas = talloc_array(mem_ctx, struct ea_struct, 2);
		setfile.ea_set.in.eas[0].flags = 0;
		setfile.ea_set.in.eas[0].name.s = "EAONE";
		setfile.ea_set.in.eas[0].value = data_blob_talloc(mem_ctx, "VALUE1", 6);
		setfile.ea_set.in.eas[1].flags = 0;
		setfile.ea_set.in.eas[1].name.s = "SECONDEA";
		setfile.ea_set.in.eas[1].value = data_blob_talloc(mem_ctx, "ValueTwo", 8);
		status = smb_raw_setfileinfo(cli->tree, &setfile);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to setup EAs\n");
		}
	}

	/* make sure all the timestamps aren't the same */
	ZERO_STRUCT(setfile);
	setfile.generic.level = RAW_SFILEINFO_BASIC_INFO;
	setfile.generic.in.file.fnum = fnum;

	unix_to_nt_time(&setfile.basic_info.in.create_time,
	    t + 9*30*24*60*60);
	unix_to_nt_time(&setfile.basic_info.in.access_time,
	    t + 6*30*24*60*60);
	unix_to_nt_time(&setfile.basic_info.in.write_time,
	    t + 3*30*24*60*60);

	status = smb_raw_setfileinfo(cli->tree, &setfile);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to setup file times - %s\n", nt_errstr(status));
	}

	/* make sure all the timestamps aren't the same */
	fileinfo.generic.level = RAW_FILEINFO_BASIC_INFO;
	fileinfo.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &fileinfo);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to query file times - %s\n", nt_errstr(status));
	}

	if (setfile.basic_info.in.create_time != fileinfo.basic_info.out.create_time) {
		printf("create_time not setup correctly\n");
	}
	if (setfile.basic_info.in.access_time != fileinfo.basic_info.out.access_time) {
		printf("access_time not setup correctly\n");
	}
	if (setfile.basic_info.in.write_time != fileinfo.basic_info.out.write_time) {
		printf("write_time not setup correctly\n");
	}

	return fnum;
}



/* return a pointer to a anonymous shared memory segment of size "size"
   which will persist across fork() but will disappear when all processes
   exit 

   The memory is not zeroed 

   This function uses system5 shared memory. It takes advantage of a property
   that the memory is not destroyed if it is attached when the id is removed
   */
void *shm_setup(int size)
{
	int shmid;
	void *ret;

#ifdef __QNXNTO__
	shmid = shm_open("private", O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (shmid == -1) {
		printf("can't get shared memory\n");
		exit(1);
	}
	shm_unlink("private");
	if (ftruncate(shmid, size) == -1) {
		printf("can't set shared memory size\n");
		exit(1);
	}
	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
	if (ret == MAP_FAILED) {
		printf("can't map shared memory\n");
		exit(1);
	}
#else
	shmid = shmget(IPC_PRIVATE, size, SHM_R | SHM_W);
	if (shmid == -1) {
		printf("can't get shared memory\n");
		exit(1);
	}
	ret = (void *)shmat(shmid, 0, 0);
	if (!ret || ret == (void *)-1) {
		printf("can't attach to shared memory\n");
		return NULL;
	}
	/* the following releases the ipc, but note that this process
	   and all its children will still have access to the memory, its
	   just that the shmid is no longer valid for other shm calls. This
	   means we don't leave behind lots of shm segments after we exit 

	   See Stevens "advanced programming in unix env" for details
	   */
	shmctl(shmid, IPC_RMID, 0);
#endif
	
	return ret;
}


/**
  check that a wire string matches the flags specified 
  not 100% accurate, but close enough for testing
*/
bool wire_bad_flags(struct smb_wire_string *str, int flags, 
		    struct smbcli_transport *transport)
{
	bool server_unicode;
	int len;
	if (!str || !str->s) return true;
	len = strlen(str->s);
	if (flags & STR_TERMINATE) len++;

	server_unicode = (transport->negotiate.capabilities&CAP_UNICODE)?true:false;
	if (getenv("CLI_FORCE_ASCII") || !transport->options.unicode) {
		server_unicode = false;
	}

	if ((flags & STR_UNICODE) || server_unicode) {
		len *= 2;
	} else if (flags & STR_TERMINATE_ASCII) {
		len++;
	}
	if (str->private_length != len) {
		printf("Expected wire_length %d but got %d for '%s'\n", 
		       len, str->private_length, str->s);
		return true;
	}
	return false;
}

/*
  dump a all_info QFILEINFO structure
*/
void dump_all_info(TALLOC_CTX *mem_ctx, union smb_fileinfo *finfo)
{
	d_printf("\tcreate_time:    %s\n", nt_time_string(mem_ctx, finfo->all_info.out.create_time));
	d_printf("\taccess_time:    %s\n", nt_time_string(mem_ctx, finfo->all_info.out.access_time));
	d_printf("\twrite_time:     %s\n", nt_time_string(mem_ctx, finfo->all_info.out.write_time));
	d_printf("\tchange_time:    %s\n", nt_time_string(mem_ctx, finfo->all_info.out.change_time));
	d_printf("\tattrib:         0x%x\n", finfo->all_info.out.attrib);
	d_printf("\talloc_size:     %llu\n", (long long)finfo->all_info.out.alloc_size);
	d_printf("\tsize:           %llu\n", (long long)finfo->all_info.out.size);
	d_printf("\tnlink:          %u\n", finfo->all_info.out.nlink);
	d_printf("\tdelete_pending: %u\n", finfo->all_info.out.delete_pending);
	d_printf("\tdirectory:      %u\n", finfo->all_info.out.directory);
	d_printf("\tea_size:        %u\n", finfo->all_info.out.ea_size);
	d_printf("\tfname:          '%s'\n", finfo->all_info.out.fname.s);
}

/*
  dump file infor by name
*/
void torture_all_info(struct smbcli_tree *tree, const char *fname)
{
	TALLOC_CTX *mem_ctx = talloc_named(tree, 0, "%s", fname);
	union smb_fileinfo finfo;
	NTSTATUS status;

	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(tree, mem_ctx, &finfo);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s - %s\n", fname, nt_errstr(status));
		return;
	}

	d_printf("%s:\n", fname);
	dump_all_info(mem_ctx, &finfo);
	talloc_free(mem_ctx);
}


/*
  set a attribute on a file
*/
bool torture_set_file_attribute(struct smbcli_tree *tree, const char *fname, uint16_t attrib)
{
	union smb_setfileinfo sfinfo;
	NTSTATUS status;

	ZERO_STRUCT(sfinfo.basic_info.in);
	sfinfo.basic_info.level = RAW_SFILEINFO_BASIC_INFORMATION;
	sfinfo.basic_info.in.file.path = fname;
	sfinfo.basic_info.in.attrib = attrib;
	status = smb_raw_setpathinfo(tree, &sfinfo);
	return NT_STATUS_IS_OK(status);
}


/*
  set a file descriptor as sparse
*/
NTSTATUS torture_set_sparse(struct smbcli_tree *tree, int fnum)
{
	union smb_ioctl nt;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_named_const(tree, 0, "torture_set_sparse");
	if (!mem_ctx) {
		return NT_STATUS_NO_MEMORY;
	}

	nt.ntioctl.level = RAW_IOCTL_NTIOCTL;
	nt.ntioctl.in.function = FSCTL_SET_SPARSE;
	nt.ntioctl.in.file.fnum = fnum;
	nt.ntioctl.in.fsctl = true;
	nt.ntioctl.in.filter = 0;
	nt.ntioctl.in.max_data = 0;
	nt.ntioctl.in.blob = data_blob(NULL, 0);

	status = smb_raw_ioctl(tree, mem_ctx, &nt);

	talloc_free(mem_ctx);

	return status;
}

/*
  check that an EA has the right value 
*/
NTSTATUS torture_check_ea(struct smbcli_state *cli, 
			  const char *fname, const char *eaname, const char *value)
{
	union smb_fileinfo info;
	NTSTATUS status;
	struct ea_name ea;
	TALLOC_CTX *mem_ctx = talloc_new(cli);

	info.ea_list.level = RAW_FILEINFO_EA_LIST;
	info.ea_list.in.file.path = fname;
	info.ea_list.in.num_names = 1;
	info.ea_list.in.ea_names = &ea;

	ea.name.s = eaname;

	status = smb_raw_pathinfo(cli->tree, mem_ctx, &info);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return status;
	}

	if (info.ea_list.out.num_eas != 1) {
		printf("Expected 1 ea in ea_list\n");
		talloc_free(mem_ctx);
		return NT_STATUS_EA_CORRUPT_ERROR;
	}

	if (strcasecmp_m(eaname, info.ea_list.out.eas[0].name.s) != 0) {
		printf("Expected ea '%s' not '%s' in ea_list\n",
		       eaname, info.ea_list.out.eas[0].name.s);
		talloc_free(mem_ctx);
		return NT_STATUS_EA_CORRUPT_ERROR;
	}

	if (value == NULL) {
		if (info.ea_list.out.eas[0].value.length != 0) {
			printf("Expected zero length ea for %s\n", eaname);
			talloc_free(mem_ctx);
			return NT_STATUS_EA_CORRUPT_ERROR;
		}
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	if (strlen(value) == info.ea_list.out.eas[0].value.length &&
	    memcmp(value, info.ea_list.out.eas[0].value.data,
		   info.ea_list.out.eas[0].value.length) == 0) {
		talloc_free(mem_ctx);
		return NT_STATUS_OK;
	}

	printf("Expected value '%s' not '%*.*s' for ea %s\n",
	       value, 
	       (int)info.ea_list.out.eas[0].value.length,
	       (int)info.ea_list.out.eas[0].value.length,
	       info.ea_list.out.eas[0].value.data,
	       eaname);

	talloc_free(mem_ctx);

	return NT_STATUS_EA_CORRUPT_ERROR;
}

_PUBLIC_ bool torture_open_connection_share(TALLOC_CTX *mem_ctx,
				   struct smbcli_state **c, 
				   struct torture_context *tctx,
				   const char *hostname, 
				   const char *sharename,
				   struct tevent_context *ev)
{
	NTSTATUS status;

	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lpcfg_smbcli_options(tctx->lp_ctx, &options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &session_options);

	options.use_oplocks = torture_setting_bool(tctx, "use_oplocks", true);
	options.use_level2_oplocks = torture_setting_bool(tctx, "use_level2_oplocks", true);

	status = smbcli_full_connection(mem_ctx, c, hostname, 
					lpcfg_smb_ports(tctx->lp_ctx),
					sharename, NULL,
					lpcfg_socket_options(tctx->lp_ctx),
					cmdline_credentials, 
					lpcfg_resolve_context(tctx->lp_ctx),
					ev, &options, &session_options,
					lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to open connection - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}

_PUBLIC_ bool torture_get_conn_index(int conn_index,
				     TALLOC_CTX *mem_ctx,
				     struct torture_context *tctx,
				     char **host, char **share)
{
	char **unc_list = NULL;
	int num_unc_names = 0;
	const char *p;

	(*host) = talloc_strdup(mem_ctx, torture_setting_string(tctx, "host", NULL));
	(*share) = talloc_strdup(mem_ctx, torture_setting_string(tctx, "share", NULL));
	
	p = torture_setting_string(tctx, "unclist", NULL);
	if (!p) {
		return true;
	}

	unc_list = file_lines_load(p, &num_unc_names, 0, NULL);
	if (!unc_list || num_unc_names <= 0) {
		DEBUG(0,("Failed to load unc names list from '%s'\n", p));
		return false;
	}

	p = unc_list[conn_index % num_unc_names];
	if (p[0] != '/' && p[0] != '\\') {
		/* allow UNC lists of hosts */
		(*host) = talloc_strdup(mem_ctx, p);
	} else if (!smbcli_parse_unc(p, mem_ctx, host, share)) {
		DEBUG(0, ("Failed to parse UNC name %s\n",
			  unc_list[conn_index % num_unc_names]));
		return false;
	}

	talloc_free(unc_list);
	return true;
}



_PUBLIC_ bool torture_open_connection_ev(struct smbcli_state **c,
					 int conn_index,
					 struct torture_context *tctx,
					 struct tevent_context *ev)
{
	char *host, *share;
	bool ret;

	if (!torture_get_conn_index(conn_index, ev, tctx, &host, &share)) {
		return false;
	}

	ret = torture_open_connection_share(NULL, c, tctx, host, share, ev);
	talloc_free(host);
	talloc_free(share);

	return ret;
}

_PUBLIC_ bool torture_open_connection(struct smbcli_state **c, struct torture_context *tctx, int conn_index)
{
	return torture_open_connection_ev(c, conn_index, tctx, tctx->ev);
}



_PUBLIC_ bool torture_close_connection(struct smbcli_state *c)
{
	bool ret = true;
	if (!c) return true;
	if (NT_STATUS_IS_ERR(smbcli_tdis(c))) {
		printf("tdis failed (%s)\n", smbcli_errstr(c->tree));
		ret = false;
	}
	talloc_free(c);
	return ret;
}


/* check if the server produced the expected error code */
_PUBLIC_ bool check_error(const char *location, struct smbcli_state *c, 
		 uint8_t eclass, uint32_t ecode, NTSTATUS nterr)
{
	NTSTATUS status;
	
	status = smbcli_nt_error(c->tree);
	if (NT_STATUS_IS_DOS(status)) {
		int classnum, num;
		classnum = NT_STATUS_DOS_CLASS(status);
		num = NT_STATUS_DOS_CODE(status);
                if (eclass != classnum || ecode != num) {
                        printf("unexpected error code %s\n", nt_errstr(status));
                        printf(" expected %s or %s (at %s)\n", 
			       nt_errstr(NT_STATUS_DOS(eclass, ecode)), 
                               nt_errstr(nterr), location);
                        return false;
                }
        } else {
                if (!NT_STATUS_EQUAL(nterr, status)) {
                        printf("unexpected error code %s\n", nt_errstr(status));
                        printf(" expected %s (at %s)\n", nt_errstr(nterr), location);
                        return false;
                }
        }

	return true;
}

static struct smbcli_state *current_cli;
static int procnum; /* records process count number when forking */

static void sigcont(int sig)
{
}

double torture_create_procs(struct torture_context *tctx, 
							bool (*fn)(struct torture_context *, struct smbcli_state *, int), bool *result)
{
	int i, status;
	volatile pid_t *child_status;
	volatile bool *child_status_out;
	int synccount;
	int tries = 8;
	int torture_nprocs = torture_setting_int(tctx, "nprocs", 4);
	double start_time_limit = 10 + (torture_nprocs * 1.5);
	struct timeval tv;

	*result = true;

	synccount = 0;

	signal(SIGCONT, sigcont);

	child_status = (volatile pid_t *)shm_setup(sizeof(pid_t)*torture_nprocs);
	if (!child_status) {
		printf("Failed to setup shared memory\n");
		return -1;
	}

	child_status_out = (volatile bool *)shm_setup(sizeof(bool)*torture_nprocs);
	if (!child_status_out) {
		printf("Failed to setup result status shared memory\n");
		return -1;
	}

	for (i = 0; i < torture_nprocs; i++) {
		child_status[i] = 0;
		child_status_out[i] = true;
	}

	tv = timeval_current();

	for (i=0;i<torture_nprocs;i++) {
		procnum = i;
		if (fork() == 0) {
			char *myname;

			pid_t mypid = getpid();
			srandom(((int)mypid) ^ ((int)time(NULL)));

			if (asprintf(&myname, "CLIENT%d", i) == -1) {
				printf("asprintf failed\n");
				return -1;
			}
			lpcfg_set_cmdline(tctx->lp_ctx, "netbios name", myname);
			free(myname);


			while (1) {
				if (torture_open_connection(&current_cli, tctx, i)) {
					break;
				}
				if (tries-- == 0) {
					printf("pid %d failed to start\n", (int)getpid());
					_exit(1);
				}
				smb_msleep(100);	
			}

			child_status[i] = getpid();

			pause();

			if (child_status[i]) {
				printf("Child %d failed to start!\n", i);
				child_status_out[i] = 1;
				_exit(1);
			}

			child_status_out[i] = fn(tctx, current_cli, i);
			_exit(0);
		}
	}

	do {
		synccount = 0;
		for (i=0;i<torture_nprocs;i++) {
			if (child_status[i]) synccount++;
		}
		if (synccount == torture_nprocs) break;
		smb_msleep(100);
	} while (timeval_elapsed(&tv) < start_time_limit);

	if (synccount != torture_nprocs) {
		printf("FAILED TO START %d CLIENTS (started %d)\n", torture_nprocs, synccount);
		*result = false;
		return timeval_elapsed(&tv);
	}

	printf("Starting %d clients\n", torture_nprocs);

	/* start the client load */
	tv = timeval_current();
	for (i=0;i<torture_nprocs;i++) {
		child_status[i] = 0;
	}

	printf("%d clients started\n", torture_nprocs);

	kill(0, SIGCONT);

	for (i=0;i<torture_nprocs;i++) {
		int ret;
		while ((ret=waitpid(0, &status, 0)) == -1 && errno == EINTR) /* noop */ ;
		if (ret == -1 || WEXITSTATUS(status) != 0) {
			*result = false;
		}
	}

	printf("\n");
	
	for (i=0;i<torture_nprocs;i++) {
		if (!child_status_out[i]) {
			*result = false;
		}
	}
	return timeval_elapsed(&tv);
}

static bool wrap_smb_multi_test(struct torture_context *torture,
								struct torture_tcase *tcase,
								struct torture_test *test)
{
	bool (*fn)(struct torture_context *, struct smbcli_state *, int ) = test->fn;
	bool result;

	torture_create_procs(torture, fn, &result);

	return result;
}

_PUBLIC_ struct torture_test *torture_suite_add_smb_multi_test(
									struct torture_suite *suite,
									const char *name,
									bool (*run) (struct torture_context *,
												 struct smbcli_state *,
												int i))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_smb_multi_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;

}

static bool wrap_simple_2smb_test(struct torture_context *torture_ctx,
									struct torture_tcase *tcase,
									struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct smbcli_state *,
				struct smbcli_state *);
	bool ret;

	struct smbcli_state *cli1, *cli2;

	if (!torture_open_connection(&cli1, torture_ctx, 0) || 
		!torture_open_connection(&cli2, torture_ctx, 1))
		return false;

	fn = test->fn;

	ret = fn(torture_ctx, cli1, cli2);

	talloc_free(cli1);
	talloc_free(cli2);

	return ret;
}



_PUBLIC_ struct torture_test *torture_suite_add_2smb_test(
									struct torture_suite *suite,
									const char *name,
									bool (*run) (struct torture_context *,
												struct smbcli_state *,
												struct smbcli_state *))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_simple_2smb_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;

}

static bool wrap_simple_1smb_test(struct torture_context *torture_ctx,
									struct torture_tcase *tcase,
									struct torture_test *test)
{
	bool (*fn) (struct torture_context *, struct smbcli_state *);
	bool ret;

	struct smbcli_state *cli1;

	if (!torture_open_connection(&cli1, torture_ctx, 0))
		return false;

	fn = test->fn;

	ret = fn(torture_ctx, cli1);

	talloc_free(cli1);

	return ret;
}

_PUBLIC_ struct torture_test *torture_suite_add_1smb_test(
				struct torture_suite *suite,
				const char *name,
				bool (*run) (struct torture_context *, struct smbcli_state *))
{
	struct torture_test *test; 
	struct torture_tcase *tcase;
	
	tcase = torture_suite_add_tcase(suite, name);

	test = talloc(tcase, struct torture_test);

	test->name = talloc_strdup(test, name);
	test->description = NULL;
	test->run = wrap_simple_1smb_test;
	test->fn = run;
	test->dangerous = false;

	DLIST_ADD_END(tcase->tests, test, struct torture_test *);

	return test;
}


NTSTATUS torture_second_tcon(TALLOC_CTX *mem_ctx,
			     struct smbcli_session *session,
			     const char *sharename,
			     struct smbcli_tree **res)
{
	union smb_tcon tcon;
	struct smbcli_tree *result;
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;

	if ((tmp_ctx = talloc_new(mem_ctx)) == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result = smbcli_tree_init(session, tmp_ctx, false);
	if (result == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;

	/* Ignore share mode security here */
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = sharename;
	tcon.tconx.in.device = "?????";

	status = smb_raw_tcon(result, tmp_ctx, &tcon);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	result->tid = tcon.tconx.out.tid;
	*res = talloc_steal(mem_ctx, result);
	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
}

/* 
   a wrapper around smblsa_sid_check_privilege, that tries to take
   account of the fact that the lsa privileges calls don't expand
   group memberships, using an explicit check for administrator. There
   must be a better way ...
 */
NTSTATUS torture_check_privilege(struct smbcli_state *cli, 
				 const char *sid_str,
				 const char *privilege)
{
	struct dom_sid *sid;
	TALLOC_CTX *tmp_ctx = talloc_new(cli);
	uint32_t rid;
	NTSTATUS status;

	sid = dom_sid_parse_talloc(tmp_ctx, sid_str);
	if (sid == NULL) {
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_SID;
	}

	status = dom_sid_split_rid(tmp_ctx, sid, NULL, &rid);
	NT_STATUS_NOT_OK_RETURN_AND_FREE(status, tmp_ctx);

	if (rid == DOMAIN_RID_ADMINISTRATOR) {
		/* assume the administrator has them all */
		return NT_STATUS_OK;
	}

	talloc_free(tmp_ctx);

	return smblsa_sid_check_privilege(cli, sid_str, privilege);
}
