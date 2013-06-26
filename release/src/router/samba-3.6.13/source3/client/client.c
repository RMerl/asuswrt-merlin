/*
   Unix SMB/CIFS implementation.
   SMB client
   Copyright (C) Andrew Tridgell          1994-1998
   Copyright (C) Simo Sorce               2001-2002
   Copyright (C) Jelmer Vernooij          2003
   Copyright (C) Gerald (Jerry) Carter    2004
   Copyright (C) Jeremy Allison           1994-2007

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
#include "popt_common.h"
#include "rpc_client/cli_pipe.h"
#include "client/client_proto.h"
#include "../librpc/gen_ndr/ndr_srvsvc_c.h"
#include "../lib/util/select.h"
#include "system/readline.h"
#include "../libcli/smbreadline/smbreadline.h"
#include "../libcli/security/security.h"
#include "system/select.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"
#include "trans2.h"
#include "libsmb/nmblib.h"

#ifndef REGISTER
#define REGISTER 0
#endif

extern int do_smb_browse(void); /* mDNS browsing */

extern bool override_logfile;
extern char tar_type;

static int port = 0;
static char *service;
static char *desthost;
static char *calling_name;
static bool grepable = false;
static char *cmdstr = NULL;
const char *cmd_ptr = NULL;

static int io_bufsize = 524288;

static int name_type = 0x20;
static int max_protocol = PROTOCOL_NT1;

static int process_tok(char *tok);
static int cmd_help(void);

#define CREATE_ACCESS_READ READ_CONTROL_ACCESS

/* 30 second timeout on most commands */
#define CLIENT_TIMEOUT (30*1000)
#define SHORT_TIMEOUT (5*1000)

/* value for unused fid field in trans2 secondary request */
#define FID_UNUSED (0xFFFF)

time_t newer_than = 0;
static int archive_level = 0;

static bool translation = false;
static bool have_ip;

/* clitar bits insert */
extern int blocksize;
extern bool tar_inc;
extern bool tar_reset;
/* clitar bits end */

static bool prompt = true;

static bool recurse = false;
static bool showacls = false;
bool lowercase = false;

static struct sockaddr_storage dest_ss;
static char dest_ss_str[INET6_ADDRSTRLEN];

#define SEPARATORS " \t\n\r"

static bool abort_mget = true;

/* timing globals */
uint64_t get_total_size = 0;
unsigned int get_total_time_ms = 0;
static uint64_t put_total_size = 0;
static unsigned int put_total_time_ms = 0;

/* totals globals */
static double dir_total;

/* encrypted state. */
static bool smb_encrypt;

/* root cli_state connection */

struct cli_state *cli;

static char CLI_DIRSEP_CHAR = '\\';
static char CLI_DIRSEP_STR[] = { '\\', '\0' };

/* Authentication for client connections. */
struct user_auth_info *auth_info;

/* Accessor functions for directory paths. */
static char *fileselection;
static const char *client_get_fileselection(void)
{
	if (fileselection) {
		return fileselection;
	}
	return "";
}

static const char *client_set_fileselection(const char *new_fs)
{
	SAFE_FREE(fileselection);
	if (new_fs) {
		fileselection = SMB_STRDUP(new_fs);
	}
	return client_get_fileselection();
}

static char *cwd;
static const char *client_get_cwd(void)
{
	if (cwd) {
		return cwd;
	}
	return CLI_DIRSEP_STR;
}

static const char *client_set_cwd(const char *new_cwd)
{
	SAFE_FREE(cwd);
	if (new_cwd) {
		cwd = SMB_STRDUP(new_cwd);
	}
	return client_get_cwd();
}

static char *cur_dir;
const char *client_get_cur_dir(void)
{
	if (cur_dir) {
		return cur_dir;
	}
	return CLI_DIRSEP_STR;
}

const char *client_set_cur_dir(const char *newdir)
{
	SAFE_FREE(cur_dir);
	if (newdir) {
		cur_dir = SMB_STRDUP(newdir);
	}
	return client_get_cur_dir();
}

/****************************************************************************
 Put up a yes/no prompt.
****************************************************************************/

static bool yesno(const char *p)
{
	char ans[20];
	printf("%s",p);

	if (!fgets(ans,sizeof(ans)-1,stdin))
		return(False);

	if (*ans == 'y' || *ans == 'Y')
		return(True);

	return(False);
}

/****************************************************************************
 Write to a local file with CR/LF->LF translation if appropriate. Return the
 number taken from the buffer. This may not equal the number written.
****************************************************************************/

static int writefile(int f, char *b, int n)
{
	int i;

	if (!translation) {
		return write(f,b,n);
	}

	i = 0;
	while (i < n) {
		if (*b == '\r' && (i<(n-1)) && *(b+1) == '\n') {
			b++;i++;
		}
		if (write(f, b, 1) != 1) {
			break;
		}
		b++;
		i++;
	}

	return(i);
}

/****************************************************************************
 Read from a file with LF->CR/LF translation if appropriate. Return the
 number read. read approx n bytes.
****************************************************************************/

static int readfile(uint8_t *b, int n, XFILE *f)
{
	int i;
	int c;

	if (!translation)
		return x_fread(b,1,n,f);

	i = 0;
	while (i < (n - 1) && (i < BUFFER_SIZE)) {
		if ((c = x_getc(f)) == EOF) {
			break;
		}

		if (c == '\n') { /* change all LFs to CR/LF */
			b[i++] = '\r';
		}

		b[i++] = c;
	}

	return(i);
}

struct push_state {
	XFILE *f;
	SMB_OFF_T nread;
};

static size_t push_source(uint8_t *buf, size_t n, void *priv)
{
	struct push_state *state = (struct push_state *)priv;
	int result;

	if (x_feof(state->f)) {
		return 0;
	}

	result = readfile(buf, n, state->f);
	state->nread += result;
	return result;
}

/****************************************************************************
 Send a message.
****************************************************************************/

static void send_message(const char *username)
{
	char buf[1600];
	NTSTATUS status;
	int i;

	d_printf("Type your message, ending it with a Control-D\n");

	i = 0;
	while (i<sizeof(buf)-2) {
		int c = fgetc(stdin);
		if (c == EOF) {
			break;
		}
		if (c == '\n') {
			buf[i++] = '\r';
		}
		buf[i++] = c;
	}
	buf[i] = '\0';

	status = cli_message(cli, desthost, username, buf);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "cli_message returned %s\n",
			  nt_errstr(status));
	}
}

/****************************************************************************
 Check the space on a device.
****************************************************************************/

static int do_dskattr(void)
{
	int total, bsize, avail;
	struct cli_state *targetcli = NULL;
	char *targetpath = NULL;
	TALLOC_CTX *ctx = talloc_tos();
	NTSTATUS status;

	if ( !cli_resolve_path(ctx, "", auth_info, cli, client_get_cur_dir(), &targetcli, &targetpath)) {
		d_printf("Error in dskattr: %s\n", cli_errstr(cli));
		return 1;
	}

	status = cli_dskattr(targetcli, &bsize, &total, &avail);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Error in dskattr: %s\n", nt_errstr(status));
		return 1;
	}

	d_printf("\n\t\t%d blocks of size %d. %d blocks available\n",
		 total, bsize, avail);

	return 0;
}

/****************************************************************************
 Show cd/pwd.
****************************************************************************/

static int cmd_pwd(void)
{
	d_printf("Current directory is %s",service);
	d_printf("%s\n",client_get_cur_dir());
	return 0;
}

/****************************************************************************
 Ensure name has correct directory separators.
****************************************************************************/

static void normalize_name(char *newdir)
{
	if (!(cli->requested_posix_capabilities & CIFS_UNIX_POSIX_PATHNAMES_CAP)) {
		string_replace(newdir,'/','\\');
	}
}

/****************************************************************************
 Change directory - inner section.
****************************************************************************/

static int do_cd(const char *new_dir)
{
	char *newdir = NULL;
	char *saved_dir = NULL;
	char *new_cd = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	SMB_STRUCT_STAT sbuf;
	uint32 attributes;
	int ret = 1;
	TALLOC_CTX *ctx = talloc_stackframe();

	newdir = talloc_strdup(ctx, new_dir);
	if (!newdir) {
		TALLOC_FREE(ctx);
		return 1;
	}

	normalize_name(newdir);

	/* Save the current directory in case the new directory is invalid */

	saved_dir = talloc_strdup(ctx, client_get_cur_dir());
	if (!saved_dir) {
		TALLOC_FREE(ctx);
		return 1;
	}

	if (*newdir == CLI_DIRSEP_CHAR) {
		client_set_cur_dir(newdir);
		new_cd = newdir;
	} else {
		new_cd = talloc_asprintf(ctx, "%s%s",
				client_get_cur_dir(),
				newdir);
		if (!new_cd) {
			goto out;
		}
	}

	/* Ensure cur_dir ends in a DIRSEP */
	if ((new_cd[0] != '\0') && (*(new_cd+strlen(new_cd)-1) != CLI_DIRSEP_CHAR)) {
		new_cd = talloc_asprintf_append(new_cd, "%s", CLI_DIRSEP_STR);
		if (!new_cd) {
			goto out;
		}
	}
	client_set_cur_dir(new_cd);

	new_cd = clean_name(ctx, new_cd);
	client_set_cur_dir(new_cd);

	if ( !cli_resolve_path(ctx, "", auth_info, cli, new_cd, &targetcli, &targetpath)) {
		d_printf("cd %s: %s\n", new_cd, cli_errstr(cli));
		client_set_cur_dir(saved_dir);
		goto out;
	}

	if (strequal(targetpath,CLI_DIRSEP_STR )) {
		TALLOC_FREE(ctx);
		return 0;
	}

	/* Use a trans2_qpathinfo to test directories for modern servers.
	   Except Win9x doesn't support the qpathinfo_basic() call..... */

	if (targetcli->protocol > PROTOCOL_LANMAN2 && !targetcli->win95) {
		NTSTATUS status;

		status = cli_qpathinfo_basic(targetcli, targetpath, &sbuf,
					     &attributes);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("cd %s: %s\n", new_cd, nt_errstr(status));
			client_set_cur_dir(saved_dir);
			goto out;
		}

		if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
			d_printf("cd %s: not a directory\n", new_cd);
			client_set_cur_dir(saved_dir);
			goto out;
		}
	} else {
		NTSTATUS status;

		targetpath = talloc_asprintf(ctx,
				"%s%s",
				targetpath,
				CLI_DIRSEP_STR );
		if (!targetpath) {
			client_set_cur_dir(saved_dir);
			goto out;
		}
		targetpath = clean_name(ctx, targetpath);
		if (!targetpath) {
			client_set_cur_dir(saved_dir);
			goto out;
		}

		status = cli_chkpath(targetcli, targetpath);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("cd %s: %s\n", new_cd, nt_errstr(status));
			client_set_cur_dir(saved_dir);
			goto out;
		}
	}

	ret = 0;

out:

	TALLOC_FREE(ctx);
	return ret;
}

/****************************************************************************
 Change directory.
****************************************************************************/

static int cmd_cd(void)
{
	char *buf = NULL;
	int rc = 0;

	if (next_token_talloc(talloc_tos(), &cmd_ptr, &buf,NULL)) {
		rc = do_cd(buf);
	} else {
		d_printf("Current directory is %s\n",client_get_cur_dir());
	}

	return rc;
}

/****************************************************************************
 Change directory.
****************************************************************************/

static int cmd_cd_oneup(void)
{
	return do_cd("..");
}

/*******************************************************************
 Decide if a file should be operated on.
********************************************************************/

static bool do_this_one(struct file_info *finfo)
{
	if (!finfo->name) {
		return false;
	}

	if (finfo->mode & FILE_ATTRIBUTE_DIRECTORY) {
		return true;
	}

	if (*client_get_fileselection() &&
	    !mask_match(finfo->name,client_get_fileselection(),false)) {
		DEBUG(3,("mask_match %s failed\n", finfo->name));
		return false;
	}

	if (newer_than && finfo->mtime_ts.tv_sec < newer_than) {
		DEBUG(3,("newer_than %s failed\n", finfo->name));
		return false;
	}

	if ((archive_level==1 || archive_level==2) && !(finfo->mode & FILE_ATTRIBUTE_ARCHIVE)) {
		DEBUG(3,("archive %s failed\n", finfo->name));
		return false;
	}

	return true;
}

/****************************************************************************
 Display info about a file.
****************************************************************************/

static NTSTATUS display_finfo(struct cli_state *cli_state, struct file_info *finfo,
			  const char *dir)
{
	time_t t;
	TALLOC_CTX *ctx = talloc_tos();
	NTSTATUS status = NT_STATUS_OK;

	if (!do_this_one(finfo)) {
		return NT_STATUS_OK;
	}

	t = finfo->mtime_ts.tv_sec; /* the time is assumed to be passed as GMT */
	if (!showacls) {
		d_printf("  %-30s%7.7s %8.0f  %s",
			 finfo->name,
			 attrib_string(finfo->mode),
		 	(double)finfo->size,
			time_to_asc(t));
		dir_total += finfo->size;
	} else {
		char *afname = NULL;
		uint16_t fnum;

		/* skip if this is . or .. */
		if ( strequal(finfo->name,"..") || strequal(finfo->name,".") )
			return NT_STATUS_OK;
		/* create absolute filename for cli_ntcreate() FIXME */
		afname = talloc_asprintf(ctx,
					"%s%s%s",
					dir,
					CLI_DIRSEP_STR,
					finfo->name);
		if (!afname) {
			return NT_STATUS_NO_MEMORY;
		}
		/* print file meta date header */
		d_printf( "FILENAME:%s\n", finfo->name);
		d_printf( "MODE:%s\n", attrib_string(finfo->mode));
		d_printf( "SIZE:%.0f\n", (double)finfo->size);
		d_printf( "MTIME:%s", time_to_asc(t));
		status = cli_ntcreate(cli_state, afname, 0,
				      CREATE_ACCESS_READ, 0,
				      FILE_SHARE_READ|FILE_SHARE_WRITE,
				      FILE_OPEN, 0x0, 0x0, &fnum);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG( 0, ("display_finfo() Failed to open %s: %s\n",
				   afname, nt_errstr(status)));
		} else {
			struct security_descriptor *sd = NULL;
			sd = cli_query_secdesc(cli_state, fnum, ctx);
			if (!sd) {
				DEBUG( 0, ("display_finfo() failed to "
					"get security descriptor: %s",
					cli_errstr(cli_state)));
				status = cli_nt_error(cli_state);
			} else {
				display_sec_desc(sd);
			}
			TALLOC_FREE(sd);
		}
		TALLOC_FREE(afname);
	}
	return status;
}

/****************************************************************************
 Accumulate size of a file.
****************************************************************************/

static NTSTATUS do_du(struct cli_state *cli_state, struct file_info *finfo,
		  const char *dir)
{
	if (do_this_one(finfo)) {
		dir_total += finfo->size;
	}
	return NT_STATUS_OK;
}

static bool do_list_recurse;
static bool do_list_dirs;
static char *do_list_queue = 0;
static long do_list_queue_size = 0;
static long do_list_queue_start = 0;
static long do_list_queue_end = 0;
static NTSTATUS (*do_list_fn)(struct cli_state *cli_state, struct file_info *,
			  const char *dir);

/****************************************************************************
 Functions for do_list_queue.
****************************************************************************/

/*
 * The do_list_queue is a NUL-separated list of strings stored in a
 * char*.  Since this is a FIFO, we keep track of the beginning and
 * ending locations of the data in the queue.  When we overflow, we
 * double the size of the char*.  When the start of the data passes
 * the midpoint, we move everything back.  This is logically more
 * complex than a linked list, but easier from a memory management
 * angle.  In any memory error condition, do_list_queue is reset.
 * Functions check to ensure that do_list_queue is non-NULL before
 * accessing it.
 */

static void reset_do_list_queue(void)
{
	SAFE_FREE(do_list_queue);
	do_list_queue_size = 0;
	do_list_queue_start = 0;
	do_list_queue_end = 0;
}

static void init_do_list_queue(void)
{
	reset_do_list_queue();
	do_list_queue_size = 1024;
	do_list_queue = (char *)SMB_MALLOC(do_list_queue_size);
	if (do_list_queue == 0) {
		d_printf("malloc fail for size %d\n",
			 (int)do_list_queue_size);
		reset_do_list_queue();
	} else {
		memset(do_list_queue, 0, do_list_queue_size);
	}
}

static void adjust_do_list_queue(void)
{
	/*
	 * If the starting point of the queue is more than half way through,
	 * move everything toward the beginning.
	 */

	if (do_list_queue == NULL) {
		DEBUG(4,("do_list_queue is empty\n"));
		do_list_queue_start = do_list_queue_end = 0;
		return;
	}

	if (do_list_queue_start == do_list_queue_end) {
		DEBUG(4,("do_list_queue is empty\n"));
		do_list_queue_start = do_list_queue_end = 0;
		*do_list_queue = '\0';
	} else if (do_list_queue_start > (do_list_queue_size / 2)) {
		DEBUG(4,("sliding do_list_queue backward\n"));
		memmove(do_list_queue,
			do_list_queue + do_list_queue_start,
			do_list_queue_end - do_list_queue_start);
		do_list_queue_end -= do_list_queue_start;
		do_list_queue_start = 0;
	}
}

static void add_to_do_list_queue(const char *entry)
{
	long new_end = do_list_queue_end + ((long)strlen(entry)) + 1;
	while (new_end > do_list_queue_size) {
		do_list_queue_size *= 2;
		DEBUG(4,("enlarging do_list_queue to %d\n",
			 (int)do_list_queue_size));
		do_list_queue = (char *)SMB_REALLOC(do_list_queue, do_list_queue_size);
		if (! do_list_queue) {
			d_printf("failure enlarging do_list_queue to %d bytes\n",
				 (int)do_list_queue_size);
			reset_do_list_queue();
		} else {
			memset(do_list_queue + do_list_queue_size / 2,
			       0, do_list_queue_size / 2);
		}
	}
	if (do_list_queue) {
		safe_strcpy_base(do_list_queue + do_list_queue_end,
				 entry, do_list_queue, do_list_queue_size);
		do_list_queue_end = new_end;
		DEBUG(4,("added %s to do_list_queue (start=%d, end=%d)\n",
			 entry, (int)do_list_queue_start, (int)do_list_queue_end));
	}
}

static char *do_list_queue_head(void)
{
	return do_list_queue + do_list_queue_start;
}

static void remove_do_list_queue_head(void)
{
	if (do_list_queue_end > do_list_queue_start) {
		do_list_queue_start += strlen(do_list_queue_head()) + 1;
		adjust_do_list_queue();
		DEBUG(4,("removed head of do_list_queue (start=%d, end=%d)\n",
			 (int)do_list_queue_start, (int)do_list_queue_end));
	}
}

static int do_list_queue_empty(void)
{
	return (! (do_list_queue && *do_list_queue));
}

/****************************************************************************
 A helper for do_list.
****************************************************************************/

static NTSTATUS do_list_helper(const char *mntpoint, struct file_info *f,
			   const char *mask, void *state)
{
	struct cli_state *cli_state = (struct cli_state *)state;
	TALLOC_CTX *ctx = talloc_tos();
	char *dir = NULL;
	char *dir_end = NULL;
	NTSTATUS status = NT_STATUS_OK;

	/* Work out the directory. */
	dir = talloc_strdup(ctx, mask);
	if (!dir) {
		return NT_STATUS_NO_MEMORY;
	}
	if ((dir_end = strrchr(dir, CLI_DIRSEP_CHAR)) != NULL) {
		*dir_end = '\0';
	}

	if (f->mode & FILE_ATTRIBUTE_DIRECTORY) {
		if (do_list_dirs && do_this_one(f)) {
			status = do_list_fn(cli_state, f, dir);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}
		if (do_list_recurse &&
		    f->name &&
		    !strequal(f->name,".") &&
		    !strequal(f->name,"..")) {
			char *mask2 = NULL;
			char *p = NULL;

			if (!f->name[0]) {
				d_printf("Empty dir name returned. Possible server misconfiguration.\n");
				TALLOC_FREE(dir);
				return NT_STATUS_UNSUCCESSFUL;
			}

			mask2 = talloc_asprintf(ctx,
					"%s%s",
					mntpoint,
					mask);
			if (!mask2) {
				TALLOC_FREE(dir);
				return NT_STATUS_NO_MEMORY;
			}
			p = strrchr_m(mask2,CLI_DIRSEP_CHAR);
			if (p) {
				p[1] = 0;
			} else {
				mask2[0] = '\0';
			}
			mask2 = talloc_asprintf_append(mask2,
					"%s%s*",
					f->name,
					CLI_DIRSEP_STR);
			if (!mask2) {
				TALLOC_FREE(dir);
				return NT_STATUS_NO_MEMORY;
			}
			add_to_do_list_queue(mask2);
			TALLOC_FREE(mask2);
		}
		TALLOC_FREE(dir);
		return NT_STATUS_OK;
	}

	if (do_this_one(f)) {
		status = do_list_fn(cli_state, f, dir);
	}
	TALLOC_FREE(dir);
	return status;
}

/****************************************************************************
 A wrapper around cli_list that adds recursion.
****************************************************************************/

NTSTATUS do_list(const char *mask,
			uint16 attribute,
			NTSTATUS (*fn)(struct cli_state *cli_state, struct file_info *,
				   const char *dir),
			bool rec,
			bool dirs)
{
	static int in_do_list = 0;
	TALLOC_CTX *ctx = talloc_tos();
	struct cli_state *targetcli = NULL;
	char *targetpath = NULL;
	NTSTATUS ret_status = NT_STATUS_OK;
	NTSTATUS status = NT_STATUS_OK;

	if (in_do_list && rec) {
		fprintf(stderr, "INTERNAL ERROR: do_list called recursively when the recursive flag is true\n");
		exit(1);
	}

	in_do_list = 1;

	do_list_recurse = rec;
	do_list_dirs = dirs;
	do_list_fn = fn;

	if (rec) {
		init_do_list_queue();
		add_to_do_list_queue(mask);

		while (!do_list_queue_empty()) {
			/*
			 * Need to copy head so that it doesn't become
			 * invalid inside the call to cli_list.  This
			 * would happen if the list were expanded
			 * during the call.
			 * Fix from E. Jay Berkenbilt (ejb@ql.org)
			 */
			char *head = talloc_strdup(ctx, do_list_queue_head());

			if (!head) {
				return NT_STATUS_NO_MEMORY;
			}

			/* check for dfs */

			if ( !cli_resolve_path(ctx, "", auth_info, cli, head, &targetcli, &targetpath ) ) {
				d_printf("do_list: [%s] %s\n", head, cli_errstr(cli));
				remove_do_list_queue_head();
				continue;
			}

			status = cli_list(targetcli, targetpath, attribute,
				 do_list_helper, targetcli);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf("%s listing %s\n",
					 nt_errstr(status), targetpath);
				ret_status = status;
			}
			remove_do_list_queue_head();
			if ((! do_list_queue_empty()) && (fn == display_finfo)) {
				char *next_file = do_list_queue_head();
				char *save_ch = 0;
				if ((strlen(next_file) >= 2) &&
				    (next_file[strlen(next_file) - 1] == '*') &&
				    (next_file[strlen(next_file) - 2] == CLI_DIRSEP_CHAR)) {
					save_ch = next_file +
						strlen(next_file) - 2;
					*save_ch = '\0';
					if (showacls) {
						/* cwd is only used if showacls is on */
						client_set_cwd(next_file);
					}
				}
				if (!showacls) /* don't disturbe the showacls output */
					d_printf("\n%s\n",next_file);
				if (save_ch) {
					*save_ch = CLI_DIRSEP_CHAR;
				}
			}
			TALLOC_FREE(head);
			TALLOC_FREE(targetpath);
		}
	} else {
		/* check for dfs */
		if (cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetpath)) {

			status = cli_list(targetcli, targetpath, attribute,
					  do_list_helper, targetcli);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf("%s listing %s\n",
					 nt_errstr(status), targetpath);
				ret_status = status;
			}
			TALLOC_FREE(targetpath);
		} else {
			d_printf("do_list: [%s] %s\n", mask, cli_errstr(cli));
			ret_status = cli_nt_error(cli);
		}
	}

	in_do_list = 0;
	reset_do_list_queue();
	return ret_status;
}

/****************************************************************************
 Get a directory listing.
****************************************************************************/

static int cmd_dir(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	uint16 attribute = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	char *mask = NULL;
	char *buf = NULL;
	int rc = 1;
	NTSTATUS status;

	dir_total = 0;
	mask = talloc_strdup(ctx, client_get_cur_dir());
	if (!mask) {
		return 1;
	}

	if (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		normalize_name(buf);
		if (*buf == CLI_DIRSEP_CHAR) {
			mask = talloc_strdup(ctx, buf);
		} else {
			mask = talloc_asprintf_append(mask, "%s", buf);
		}
	} else {
		mask = talloc_asprintf_append(mask, "*");
	}
	if (!mask) {
		return 1;
	}

	if (showacls) {
		/* cwd is only used if showacls is on */
		client_set_cwd(client_get_cur_dir());
	}

	status = do_list(mask, attribute, display_finfo, recurse, true);
	if (!NT_STATUS_IS_OK(status)) {
		return 1;
	}

	rc = do_dskattr();

	DEBUG(3, ("Total bytes listed: %.0f\n", dir_total));

	return rc;
}

/****************************************************************************
 Get a directory listing.
****************************************************************************/

static int cmd_du(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	uint16 attribute = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	char *mask = NULL;
	char *buf = NULL;
	NTSTATUS status;
	int rc = 1;

	dir_total = 0;
	mask = talloc_strdup(ctx, client_get_cur_dir());
	if (!mask) {
		return 1;
	}
	if ((mask[0] != '\0') && (mask[strlen(mask)-1]!=CLI_DIRSEP_CHAR)) {
		mask = talloc_asprintf_append(mask, "%s", CLI_DIRSEP_STR);
		if (!mask) {
			return 1;
		}
	}

	if (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		normalize_name(buf);
		if (*buf == CLI_DIRSEP_CHAR) {
			mask = talloc_strdup(ctx, buf);
		} else {
			mask = talloc_asprintf_append(mask, "%s", buf);
		}
	} else {
		mask = talloc_strdup(ctx, "*");
	}

	status = do_list(mask, attribute, do_du, recurse, true);
	if (!NT_STATUS_IS_OK(status)) {
		return 1;
	}

	rc = do_dskattr();

	d_printf("Total number of bytes: %.0f\n", dir_total);

	return rc;
}

static int cmd_echo(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *num;
	char *data;
	NTSTATUS status;

	if (!next_token_talloc(ctx, &cmd_ptr, &num, NULL)
	    || !next_token_talloc(ctx, &cmd_ptr, &data, NULL)) {
		d_printf("echo <num> <data>\n");
		return 1;
	}

	status = cli_echo(cli, atoi(num), data_blob_const(data, strlen(data)));

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("echo failed: %s\n", nt_errstr(status));
		return 1;
	}

	return 0;
}

/****************************************************************************
 Get a file from rname to lname
****************************************************************************/

static NTSTATUS writefile_sink(char *buf, size_t n, void *priv)
{
	int *pfd = (int *)priv;
	if (writefile(*pfd, buf, n) == -1) {
		return map_nt_error_from_unix(errno);
	}
	return NT_STATUS_OK;
}

static int do_get(const char *rname, const char *lname_in, bool reget)
{
	TALLOC_CTX *ctx = talloc_tos();
	int handle = 0;
	uint16_t fnum;
	bool newhandle = false;
	struct timespec tp_start;
	uint16 attr;
	SMB_OFF_T size;
	off_t start = 0;
	SMB_OFF_T nread = 0;
	int rc = 0;
	struct cli_state *targetcli = NULL;
	char *targetname = NULL;
	char *lname = NULL;
	NTSTATUS status;

	lname = talloc_strdup(ctx, lname_in);
	if (!lname) {
		return 1;
	}

	if (lowercase) {
		strlower_m(lname);
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, rname, &targetcli, &targetname ) ) {
		d_printf("Failed to open %s: %s\n", rname, cli_errstr(cli));
		return 1;
	}

	clock_gettime_mono(&tp_start);

	status = cli_open(targetcli, targetname, O_RDONLY, DENY_NONE, &fnum);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s opening remote file %s\n", nt_errstr(status),
			 rname);
		return 1;
	}

	if(!strcmp(lname,"-")) {
		handle = fileno(stdout);
	} else {
		if (reget) {
			handle = sys_open(lname, O_WRONLY|O_CREAT, 0644);
			if (handle >= 0) {
				start = sys_lseek(handle, 0, SEEK_END);
				if (start == -1) {
					d_printf("Error seeking local file\n");
					return 1;
				}
			}
		} else {
			handle = sys_open(lname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		}
		newhandle = true;
	}
	if (handle < 0) {
		d_printf("Error opening local file %s\n",lname);
		return 1;
	}


	status = cli_qfileinfo_basic(targetcli, fnum, &attr, &size, NULL, NULL,
				     NULL, NULL, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		status = cli_getattrE(targetcli, fnum, &attr, &size, NULL, NULL,
				      NULL);
		if(!NT_STATUS_IS_OK(status)) {
			d_printf("getattrib: %s\n", nt_errstr(status));
			return 1;
		}
	}

	DEBUG(1,("getting file %s of size %.0f as %s ",
		 rname, (double)size, lname));

	status = cli_pull(targetcli, fnum, start, size, io_bufsize,
			  writefile_sink, (void *)&handle, &nread);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "parallel_read returned %s\n",
			  nt_errstr(status));
		cli_close(targetcli, fnum);
		return 1;
	}

	status = cli_close(targetcli, fnum);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Error %s closing remote file\n", nt_errstr(status));
		rc = 1;
	}

	if (newhandle) {
		close(handle);
	}

	if (archive_level >= 2 && (attr & FILE_ATTRIBUTE_ARCHIVE)) {
		cli_setatr(cli, rname, attr & ~(uint16)FILE_ATTRIBUTE_ARCHIVE, 0);
	}

	{
		struct timespec tp_end;
		int this_time;

		clock_gettime_mono(&tp_end);
		this_time = nsec_time_diff(&tp_end,&tp_start)/1000000;
		get_total_time_ms += this_time;
		get_total_size += nread;

		DEBUG(1,("(%3.1f KiloBytes/sec) (average %3.1f KiloBytes/sec)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 get_total_size / (1.024*get_total_time_ms)));
	}

	TALLOC_FREE(targetname);
	return rc;
}

/****************************************************************************
 Get a file.
****************************************************************************/

static int cmd_get(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *lname = NULL;
	char *rname = NULL;
	char *fname = NULL;

	rname = talloc_strdup(ctx, client_get_cur_dir());
	if (!rname) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&fname,NULL)) {
		d_printf("get <filename> [localname]\n");
		return 1;
	}
	rname = talloc_asprintf_append(rname, "%s", fname);
	if (!rname) {
		return 1;
	}
	rname = clean_name(ctx, rname);
	if (!rname) {
		return 1;
	}

	next_token_talloc(ctx, &cmd_ptr,&lname,NULL);
	if (!lname) {
		lname = fname;
	}

	return do_get(rname, lname, false);
}

/****************************************************************************
 Do an mget operation on one file.
****************************************************************************/

static NTSTATUS do_mget(struct cli_state *cli_state, struct file_info *finfo,
		    const char *dir)
{
	TALLOC_CTX *ctx = talloc_tos();
	NTSTATUS status = NT_STATUS_OK;
	char *rname = NULL;
	char *quest = NULL;
	char *saved_curdir = NULL;
	char *mget_mask = NULL;
	char *new_cd = NULL;

	if (!finfo->name) {
		return NT_STATUS_OK;
	}

	if (strequal(finfo->name,".") || strequal(finfo->name,".."))
		return NT_STATUS_OK;

	if (abort_mget)	{
		d_printf("mget aborted\n");
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (finfo->mode & FILE_ATTRIBUTE_DIRECTORY) {
		if (asprintf(&quest,
			 "Get directory %s? ",finfo->name) < 0) {
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		if (asprintf(&quest,
			 "Get file %s? ",finfo->name) < 0) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	if (prompt && !yesno(quest)) {
		SAFE_FREE(quest);
		return NT_STATUS_OK;
	}
	SAFE_FREE(quest);

	if (!(finfo->mode & FILE_ATTRIBUTE_DIRECTORY)) {
		rname = talloc_asprintf(ctx,
				"%s%s",
				client_get_cur_dir(),
				finfo->name);
		if (!rname) {
			return NT_STATUS_NO_MEMORY;
		}
		do_get(rname, finfo->name, false);
		TALLOC_FREE(rname);
		return NT_STATUS_OK;
	}

	/* handle directories */
	saved_curdir = talloc_strdup(ctx, client_get_cur_dir());
	if (!saved_curdir) {
		return NT_STATUS_NO_MEMORY;
	}

	new_cd = talloc_asprintf(ctx,
				"%s%s%s",
				client_get_cur_dir(),
				finfo->name,
				CLI_DIRSEP_STR);
	if (!new_cd) {
		return NT_STATUS_NO_MEMORY;
	}
	client_set_cur_dir(new_cd);

	string_replace(finfo->name,'\\','/');
	if (lowercase) {
		strlower_m(finfo->name);
	}

	if (!directory_exist(finfo->name) &&
	    mkdir(finfo->name,0777) != 0) {
		d_printf("failed to create directory %s\n",finfo->name);
		client_set_cur_dir(saved_curdir);
		return map_nt_error_from_unix(errno);
	}

	if (chdir(finfo->name) != 0) {
		d_printf("failed to chdir to directory %s\n",finfo->name);
		client_set_cur_dir(saved_curdir);
		return map_nt_error_from_unix(errno);
	}

	mget_mask = talloc_asprintf(ctx,
			"%s*",
			client_get_cur_dir());

	if (!mget_mask) {
		return NT_STATUS_NO_MEMORY;
	}

	status = do_list(mget_mask,
			 (FILE_ATTRIBUTE_SYSTEM
			  | FILE_ATTRIBUTE_HIDDEN
			  | FILE_ATTRIBUTE_DIRECTORY),
			 do_mget, false, true);
	if (!NT_STATUS_IS_OK(status)
	 && !NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		/*
		 * Ignore access denied errors to ensure all permitted files are
		 * pulled down.
		 */
		return status;
	}

	if (chdir("..") == -1) {
		d_printf("do_mget: failed to chdir to .. (error %s)\n",
			strerror(errno) );
		return map_nt_error_from_unix(errno);
	}
	client_set_cur_dir(saved_curdir);
	TALLOC_FREE(mget_mask);
	TALLOC_FREE(saved_curdir);
	TALLOC_FREE(new_cd);
	return NT_STATUS_OK;
}

/****************************************************************************
 View the file using the pager.
****************************************************************************/

static int cmd_more(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *rname = NULL;
	char *fname = NULL;
	char *lname = NULL;
	char *pager_cmd = NULL;
	const char *pager;
	int fd;
	int rc = 0;

	rname = talloc_strdup(ctx, client_get_cur_dir());
	if (!rname) {
		return 1;
	}

	lname = talloc_asprintf(ctx, "%s/smbmore.XXXXXX",tmpdir());
	if (!lname) {
		return 1;
	}
	fd = mkstemp(lname);
	if (fd == -1) {
		d_printf("failed to create temporary file for more\n");
		return 1;
	}
	close(fd);

	if (!next_token_talloc(ctx, &cmd_ptr,&fname,NULL)) {
		d_printf("more <filename>\n");
		unlink(lname);
		return 1;
	}
	rname = talloc_asprintf_append(rname, "%s", fname);
	if (!rname) {
		return 1;
	}
	rname = clean_name(ctx,rname);
	if (!rname) {
		return 1;
	}

	rc = do_get(rname, lname, false);

	pager=getenv("PAGER");

	pager_cmd = talloc_asprintf(ctx,
				"%s %s",
				(pager? pager:PAGER),
				lname);
	if (!pager_cmd) {
		return 1;
	}
	if (system(pager_cmd) == -1) {
		d_printf("system command '%s' returned -1\n",
			pager_cmd);
	}
	unlink(lname);

	return rc;
}

/****************************************************************************
 Do a mget command.
****************************************************************************/

static int cmd_mget(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	uint16 attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	char *mget_mask = NULL;
	char *buf = NULL;
	NTSTATUS status = NT_STATUS_OK;

	if (recurse) {
		attribute |= FILE_ATTRIBUTE_DIRECTORY;
	}

	abort_mget = false;

	while (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {

		mget_mask = talloc_strdup(ctx, client_get_cur_dir());
		if (!mget_mask) {
			return 1;
		}
		if (*buf == CLI_DIRSEP_CHAR) {
			mget_mask = talloc_strdup(ctx, buf);
		} else {
			mget_mask = talloc_asprintf_append(mget_mask,
							"%s", buf);
		}
		if (!mget_mask) {
			return 1;
		}
		status = do_list(mget_mask, attribute, do_mget, false, true);
		if (!NT_STATUS_IS_OK(status)) {
			return 1;
		}
	}

	if (mget_mask == NULL) {
		d_printf("nothing to mget\n");
		return 0;
	}

	if (!*mget_mask) {
		mget_mask = talloc_asprintf(ctx,
					"%s*",
					client_get_cur_dir());
		if (!mget_mask) {
			return 1;
		}
		status = do_list(mget_mask, attribute, do_mget, false, true);
		if (!NT_STATUS_IS_OK(status)) {
			return 1;
		}
	}

	return 0;
}

/****************************************************************************
 Make a directory of name "name".
****************************************************************************/

static bool do_mkdir(const char *name)
{
	TALLOC_CTX *ctx = talloc_tos();
	struct cli_state *targetcli;
	char *targetname = NULL;
	NTSTATUS status;

	if (!cli_resolve_path(ctx, "", auth_info, cli, name, &targetcli, &targetname)) {
		d_printf("mkdir %s: %s\n", name, cli_errstr(cli));
		return false;
	}

	status = cli_mkdir(targetcli, targetname);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s making remote directory %s\n",
			 nt_errstr(status),name);
		return false;
	}

	return true;
}

/****************************************************************************
 Show 8.3 name of a file.
****************************************************************************/

static bool do_altname(const char *name)
{
	fstring altname;
	NTSTATUS status;

	status = cli_qpathinfo_alt_name(cli, name, altname);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s getting alt name for %s\n",
			 nt_errstr(status),name);
		return false;
	}
	d_printf("%s\n", altname);

	return true;
}

/****************************************************************************
 Exit client.
****************************************************************************/

static int cmd_quit(void)
{
	cli_shutdown(cli);
	exit(0);
	/* NOTREACHED */
	return 0;
}

/****************************************************************************
 Make a directory.
****************************************************************************/

static int cmd_mkdir(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;

	mask = talloc_strdup(ctx, client_get_cur_dir());
	if (!mask) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		if (!recurse) {
			d_printf("mkdir <dirname>\n");
		}
		return 1;
	}
	mask = talloc_asprintf_append(mask, "%s", buf);
	if (!mask) {
		return 1;
	}

	if (recurse) {
		char *ddir = NULL;
		char *ddir2 = NULL;
		struct cli_state *targetcli;
		char *targetname = NULL;
		char *p = NULL;
		char *saveptr;

		ddir2 = talloc_strdup(ctx, "");
		if (!ddir2) {
			return 1;
		}

		if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
			return 1;
		}

		ddir = talloc_strdup(ctx, targetname);
		if (!ddir) {
			return 1;
		}
		trim_char(ddir,'.','\0');
		p = strtok_r(ddir, "/\\", &saveptr);
		while (p) {
			ddir2 = talloc_asprintf_append(ddir2, "%s", p);
			if (!ddir2) {
				return 1;
			}
			if (!NT_STATUS_IS_OK(cli_chkpath(targetcli, ddir2))) {
				do_mkdir(ddir2);
			}
			ddir2 = talloc_asprintf_append(ddir2, "%s", CLI_DIRSEP_STR);
			if (!ddir2) {
				return 1;
			}
			p = strtok_r(NULL, "/\\", &saveptr);
		}
	} else {
		do_mkdir(mask);
	}

	return 0;
}

/****************************************************************************
 Show alt name.
****************************************************************************/

static int cmd_altname(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *name;
	char *buf;

	name = talloc_strdup(ctx, client_get_cur_dir());
	if (!name) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr, &buf, NULL)) {
		d_printf("altname <file>\n");
		return 1;
	}
	name = talloc_asprintf_append(name, "%s", buf);
	if (!name) {
		return 1;
	}
	do_altname(name);
	return 0;
}

static char *attr_str(TALLOC_CTX *mem_ctx, uint16_t mode)
{
	char *attrs = TALLOC_ZERO_ARRAY(mem_ctx, char, 17);
	int i = 0;

	if (!(mode & FILE_ATTRIBUTE_NORMAL)) {
		if (mode & FILE_ATTRIBUTE_ENCRYPTED) {
			attrs[i++] = 'E';
		}
		if (mode & FILE_ATTRIBUTE_NONINDEXED) {
			attrs[i++] = 'N';
		}
		if (mode & FILE_ATTRIBUTE_OFFLINE) {
			attrs[i++] = 'O';
		}
		if (mode & FILE_ATTRIBUTE_COMPRESSED) {
			attrs[i++] = 'C';
		}
		if (mode & FILE_ATTRIBUTE_REPARSE_POINT) {
			attrs[i++] = 'r';
		}
		if (mode & FILE_ATTRIBUTE_SPARSE) {
			attrs[i++] = 's';
		}
		if (mode & FILE_ATTRIBUTE_TEMPORARY) {
			attrs[i++] = 'T';
		}
		if (mode & FILE_ATTRIBUTE_NORMAL) {
			attrs[i++] = 'N';
		}
		if (mode & FILE_ATTRIBUTE_READONLY) {
			attrs[i++] = 'R';
		}
		if (mode & FILE_ATTRIBUTE_HIDDEN) {
			attrs[i++] = 'H';
		}
		if (mode & FILE_ATTRIBUTE_SYSTEM) {
			attrs[i++] = 'S';
		}
		if (mode & FILE_ATTRIBUTE_DIRECTORY) {
			attrs[i++] = 'D';
		}
		if (mode & FILE_ATTRIBUTE_ARCHIVE) {
			attrs[i++] = 'A';
		}
	}
	return attrs;
}

/****************************************************************************
 Show all info we can get
****************************************************************************/

static int do_allinfo(const char *name)
{
	fstring altname;
	struct timespec b_time, a_time, m_time, c_time;
	SMB_OFF_T size;
	uint16_t mode;
	SMB_INO_T ino;
	NTTIME tmp;
	uint16_t fnum;
	unsigned int num_streams;
	struct stream_struct *streams;
	int num_snapshots;
	char **snapshots;
	unsigned int i;
	NTSTATUS status;

	status = cli_qpathinfo_alt_name(cli, name, altname);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s getting alt name for %s\n", nt_errstr(status),
			 name);
		return false;
	}
	d_printf("altname: %s\n", altname);

	status = cli_qpathinfo2(cli, name, &b_time, &a_time, &m_time, &c_time,
				&size, &mode, &ino);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s getting pathinfo for %s\n", nt_errstr(status),
			 name);
		return false;
	}

	unix_timespec_to_nt_time(&tmp, b_time);
	d_printf("create_time:    %s\n", nt_time_string(talloc_tos(), tmp));

	unix_timespec_to_nt_time(&tmp, a_time);
	d_printf("access_time:    %s\n", nt_time_string(talloc_tos(), tmp));

	unix_timespec_to_nt_time(&tmp, m_time);
	d_printf("write_time:     %s\n", nt_time_string(talloc_tos(), tmp));

	unix_timespec_to_nt_time(&tmp, c_time);
	d_printf("change_time:    %s\n", nt_time_string(talloc_tos(), tmp));

	d_printf("attributes: %s (%x)\n", attr_str(talloc_tos(), mode), mode);

	status = cli_qpathinfo_streams(cli, name, talloc_tos(), &num_streams,
				       &streams);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s getting streams for %s\n", nt_errstr(status),
			 name);
		return false;
	}

	for (i=0; i<num_streams; i++) {
		d_printf("stream: [%s], %lld bytes\n", streams[i].name,
			 (unsigned long long)streams[i].size);
	}

	status = cli_ntcreate(cli, name, 0,
			      SEC_FILE_READ_DATA | SEC_FILE_READ_ATTRIBUTE |
			      SEC_STD_SYNCHRONIZE, 0,
			      FILE_SHARE_READ|FILE_SHARE_WRITE
			      |FILE_SHARE_DELETE,
			      FILE_OPEN, 0x0, 0x0, &fnum);
	if (!NT_STATUS_IS_OK(status)) {
		/*
		 * Ignore failure, it does not hurt if we can't list
		 * snapshots
		 */
		return 0;
	}
	status = cli_shadow_copy_data(talloc_tos(), cli, fnum,
				      true, &snapshots, &num_snapshots);
	if (!NT_STATUS_IS_OK(status)) {
		cli_close(cli, fnum);
		return 0;
	}

	for (i=0; i<num_snapshots; i++) {
		char *snap_name;

		d_printf("%s\n", snapshots[i]);
		snap_name = talloc_asprintf(talloc_tos(), "%s%s",
					    snapshots[i], name);
		status = cli_qpathinfo2(cli, snap_name, &b_time, &a_time,
					&m_time, &c_time, &size,
					NULL, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			d_fprintf(stderr, "pathinfo(%s) failed: %s\n",
				  snap_name, nt_errstr(status));
			TALLOC_FREE(snap_name);
			continue;
		}
		unix_timespec_to_nt_time(&tmp, b_time);
		d_printf("create_time:    %s\n", nt_time_string(talloc_tos(), tmp));
		unix_timespec_to_nt_time(&tmp, a_time);
		d_printf("access_time:    %s\n", nt_time_string(talloc_tos(), tmp));
		unix_timespec_to_nt_time(&tmp, m_time);
		d_printf("write_time:     %s\n", nt_time_string(talloc_tos(), tmp));
		unix_timespec_to_nt_time(&tmp, c_time);
		d_printf("change_time:    %s\n", nt_time_string(talloc_tos(), tmp));
		d_printf("size: %d\n", (int)size);
	}

	TALLOC_FREE(snapshots);

	return 0;
}

/****************************************************************************
 Show all info we can get
****************************************************************************/

static int cmd_allinfo(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *name;
	char *buf;

	name = talloc_strdup(ctx, client_get_cur_dir());
	if (!name) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr, &buf, NULL)) {
		d_printf("allinfo <file>\n");
		return 1;
	}
	name = talloc_asprintf_append(name, "%s", buf);
	if (!name) {
		return 1;
	}

	do_allinfo(name);

	return 0;
}

/****************************************************************************
 Put a single file.
****************************************************************************/

static int do_put(const char *rname, const char *lname, bool reput)
{
	TALLOC_CTX *ctx = talloc_tos();
	uint16_t fnum;
	XFILE *f;
	SMB_OFF_T start = 0;
	int rc = 0;
	struct timespec tp_start;
	struct cli_state *targetcli;
	char *targetname = NULL;
	struct push_state state;
	NTSTATUS status;

	if (!cli_resolve_path(ctx, "", auth_info, cli, rname, &targetcli, &targetname)) {
		d_printf("Failed to open %s: %s\n", rname, cli_errstr(cli));
		return 1;
	}

	clock_gettime_mono(&tp_start);

	if (reput) {
		status = cli_open(targetcli, targetname, O_RDWR|O_CREAT, DENY_NONE, &fnum);
		if (NT_STATUS_IS_OK(status)) {
			if (!NT_STATUS_IS_OK(status = cli_qfileinfo_basic(
						     targetcli, fnum, NULL,
						     &start, NULL, NULL,
						     NULL, NULL, NULL)) &&
			    !NT_STATUS_IS_OK(status = cli_getattrE(
						     targetcli, fnum, NULL,
						     &start, NULL, NULL,
						     NULL))) {
				d_printf("getattrib: %s\n", nt_errstr(status));
				return 1;
			}
		}
	} else {
		status = cli_open(targetcli, targetname, O_RDWR|O_CREAT|O_TRUNC, DENY_NONE, &fnum);
	}

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s opening remote file %s\n", nt_errstr(status),
			 rname);
		return 1;
	}

	/* allow files to be piped into smbclient
	   jdblair 24.jun.98

	   Note that in this case this function will exit(0) rather
	   than returning. */
	if (!strcmp(lname, "-")) {
		f = x_stdin;
		/* size of file is not known */
	} else {
		f = x_fopen(lname,O_RDONLY, 0);
		if (f && reput) {
			if (x_tseek(f, start, SEEK_SET) == -1) {
				d_printf("Error seeking local file\n");
				x_fclose(f);
				return 1;
			}
		}
	}

	if (!f) {
		d_printf("Error opening local file %s\n",lname);
		return 1;
	}

	DEBUG(1,("putting file %s as %s ",lname,
		 rname));

	x_setvbuf(f, NULL, X_IOFBF, io_bufsize);

	state.f = f;
	state.nread = 0;

	status = cli_push(targetcli, fnum, 0, 0, io_bufsize, push_source,
			  &state);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "cli_push returned %s\n", nt_errstr(status));
		rc = 1;
	}

	status = cli_close(targetcli, fnum);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s closing remote file %s\n", nt_errstr(status),
			 rname);
		if (f != x_stdin) {
			x_fclose(f);
		}
		return 1;
	}

	if (f != x_stdin) {
		x_fclose(f);
	}

	{
		struct timespec tp_end;
		int this_time;

		clock_gettime_mono(&tp_end);
		this_time = nsec_time_diff(&tp_end,&tp_start)/1000000;
		put_total_time_ms += this_time;
		put_total_size += state.nread;

		DEBUG(1,("(%3.1f kb/s) (average %3.1f kb/s)\n",
			 state.nread / (1.024*this_time + 1.0e-4),
			 put_total_size / (1.024*put_total_time_ms)));
	}

	if (f == x_stdin) {
		cli_shutdown(cli);
		exit(rc);
	}

	return rc;
}

/****************************************************************************
 Put a file.
****************************************************************************/

static int cmd_put(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *lname;
	char *rname;
	char *buf;

	rname = talloc_strdup(ctx, client_get_cur_dir());
	if (!rname) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&lname,NULL)) {
		d_printf("put <filename>\n");
		return 1;
	}

	if (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		rname = talloc_asprintf_append(rname, "%s", buf);
	} else {
		rname = talloc_asprintf_append(rname, "%s", lname);
	}
	if (!rname) {
		return 1;
	}

	rname = clean_name(ctx, rname);
	if (!rname) {
		return 1;
	}

	{
		SMB_STRUCT_STAT st;
		/* allow '-' to represent stdin
		   jdblair, 24.jun.98 */
		if (!file_exist_stat(lname, &st, false) &&
		    (strcmp(lname,"-"))) {
			d_printf("%s does not exist\n",lname);
			return 1;
		}
	}

	return do_put(rname, lname, false);
}

/*************************************
 File list structure.
*************************************/

static struct file_list {
	struct file_list *prev, *next;
	char *file_path;
	bool isdir;
} *file_list;

/****************************************************************************
 Free a file_list structure.
****************************************************************************/

static void free_file_list (struct file_list *l_head)
{
	struct file_list *list, *next;

	for (list = l_head; list; list = next) {
		next = list->next;
		DLIST_REMOVE(l_head, list);
		SAFE_FREE(list->file_path);
		SAFE_FREE(list);
	}
}

/****************************************************************************
 Seek in a directory/file list until you get something that doesn't start with
 the specified name.
****************************************************************************/

static bool seek_list(struct file_list *list, char *name)
{
	while (list) {
		trim_string(list->file_path,"./","\n");
		if (strncmp(list->file_path, name, strlen(name)) != 0) {
			return true;
		}
		list = list->next;
	}

	return false;
}

/****************************************************************************
 Set the file selection mask.
****************************************************************************/

static int cmd_select(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *new_fs = NULL;
	next_token_talloc(ctx, &cmd_ptr,&new_fs,NULL)
		;
	if (new_fs) {
		client_set_fileselection(new_fs);
	} else {
		client_set_fileselection("");
	}
	return 0;
}

/****************************************************************************
  Recursive file matching function act as find
  match must be always set to true when calling this function
****************************************************************************/

static int file_find(struct file_list **list, const char *directory,
		      const char *expression, bool match)
{
	SMB_STRUCT_DIR *dir;
	struct file_list *entry;
        struct stat statbuf;
        int ret;
        char *path;
	bool isdir;
	const char *dname;

        dir = sys_opendir(directory);
	if (!dir)
		return -1;

        while ((dname = readdirname(dir))) {
		if (!strcmp("..", dname))
			continue;
		if (!strcmp(".", dname))
			continue;

		if (asprintf(&path, "%s/%s", directory, dname) <= 0) {
			continue;
		}

		isdir = false;
		if (!match || !gen_fnmatch(expression, dname)) {
			if (recurse) {
				ret = stat(path, &statbuf);
				if (ret == 0) {
					if (S_ISDIR(statbuf.st_mode)) {
						isdir = true;
						ret = file_find(list, path, expression, false);
					}
				} else {
					d_printf("file_find: cannot stat file %s\n", path);
				}

				if (ret == -1) {
					SAFE_FREE(path);
					sys_closedir(dir);
					return -1;
				}
			}
			entry = SMB_MALLOC_P(struct file_list);
			if (!entry) {
				d_printf("Out of memory in file_find\n");
				sys_closedir(dir);
				return -1;
			}
			entry->file_path = path;
			entry->isdir = isdir;
                        DLIST_ADD(*list, entry);
		} else {
			SAFE_FREE(path);
		}
        }

	sys_closedir(dir);
	return 0;
}

/****************************************************************************
 mput some files.
****************************************************************************/

static int cmd_mput(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *p = NULL;

	while (next_token_talloc(ctx, &cmd_ptr,&p,NULL)) {
		int ret;
		struct file_list *temp_list;
		char *quest, *lname, *rname;

		file_list = NULL;

		ret = file_find(&file_list, ".", p, true);
		if (ret) {
			free_file_list(file_list);
			continue;
		}

		quest = NULL;
		lname = NULL;
		rname = NULL;

		for (temp_list = file_list; temp_list;
		     temp_list = temp_list->next) {

			SAFE_FREE(lname);
			if (asprintf(&lname, "%s/", temp_list->file_path) <= 0) {
				continue;
			}
			trim_string(lname, "./", "/");

			/* check if it's a directory */
			if (temp_list->isdir) {
				/* if (!recurse) continue; */

				SAFE_FREE(quest);
				if (asprintf(&quest, "Put directory %s? ", lname) < 0) {
					break;
				}
				if (prompt && !yesno(quest)) { /* No */
					/* Skip the directory */
					lname[strlen(lname)-1] = '/';
					if (!seek_list(temp_list, lname))
						break;
				} else { /* Yes */
	      				SAFE_FREE(rname);
					if(asprintf(&rname, "%s%s", client_get_cur_dir(), lname) < 0) {
						break;
					}
					normalize_name(rname);
					if (!NT_STATUS_IS_OK(cli_chkpath(cli, rname)) &&
					    !do_mkdir(rname)) {
						DEBUG (0, ("Unable to make dir, skipping..."));
						/* Skip the directory */
						lname[strlen(lname)-1] = '/';
						if (!seek_list(temp_list, lname)) {
							break;
						}
					}
				}
				continue;
			} else {
				SAFE_FREE(quest);
				if (asprintf(&quest,"Put file %s? ", lname) < 0) {
					break;
				}
				if (prompt && !yesno(quest)) {
					/* No */
					continue;
				}

				/* Yes */
				SAFE_FREE(rname);
				if (asprintf(&rname, "%s%s", client_get_cur_dir(), lname) < 0) {
					break;
				}
			}

			normalize_name(rname);

			do_put(rname, lname, false);
		}
		free_file_list(file_list);
		SAFE_FREE(quest);
		SAFE_FREE(lname);
		SAFE_FREE(rname);
	}

	return 0;
}

/****************************************************************************
 Cancel a print job.
****************************************************************************/

static int do_cancel(int job)
{
	if (cli_printjob_del(cli, job)) {
		d_printf("Job %d cancelled\n",job);
		return 0;
	} else {
		d_printf("Error cancelling job %d : %s\n",job,cli_errstr(cli));
		return 1;
	}
}

/****************************************************************************
 Cancel a print job.
****************************************************************************/

static int cmd_cancel(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf = NULL;
	int job;

	if (!next_token_talloc(ctx, &cmd_ptr, &buf,NULL)) {
		d_printf("cancel <jobid> ...\n");
		return 1;
	}
	do {
		job = atoi(buf);
		do_cancel(job);
	} while (next_token_talloc(ctx, &cmd_ptr,&buf,NULL));

	return 0;
}

/****************************************************************************
 Print a file.
****************************************************************************/

static int cmd_print(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *lname = NULL;
	char *rname = NULL;
	char *p = NULL;

	if (!next_token_talloc(ctx, &cmd_ptr, &lname,NULL)) {
		d_printf("print <filename>\n");
		return 1;
	}

	rname = talloc_strdup(ctx, lname);
	if (!rname) {
		return 1;
	}
	p = strrchr_m(rname,'/');
	if (p) {
		rname = talloc_asprintf(ctx,
					"%s-%d",
					p+1,
					(int)sys_getpid());
	}
	if (strequal(lname,"-")) {
		rname = talloc_asprintf(ctx,
				"stdin-%d",
				(int)sys_getpid());
	}
	if (!rname) {
		return 1;
	}

	return do_put(rname, lname, false);
}

/****************************************************************************
 Show a print queue entry.
****************************************************************************/

static void queue_fn(struct print_job_info *p)
{
	d_printf("%-6d   %-9d    %s\n", (int)p->id, (int)p->size, p->name);
}

/****************************************************************************
 Show a print queue.
****************************************************************************/

static int cmd_queue(void)
{
	cli_print_queue(cli, queue_fn);
	return 0;
}

/****************************************************************************
 Delete some files.
****************************************************************************/

static NTSTATUS do_del(struct cli_state *cli_state, struct file_info *finfo,
		   const char *dir)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	NTSTATUS status;

	mask = talloc_asprintf(ctx,
				"%s%c%s",
				dir,
				CLI_DIRSEP_CHAR,
				finfo->name);
	if (!mask) {
		return NT_STATUS_NO_MEMORY;
	}

	if (finfo->mode & FILE_ATTRIBUTE_DIRECTORY) {
		TALLOC_FREE(mask);
		return NT_STATUS_OK;
	}

	status = cli_unlink(cli_state, mask, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s deleting remote file %s\n",
			 nt_errstr(status), mask);
	}
	TALLOC_FREE(mask);
	return status;
}

/****************************************************************************
 Delete some files.
****************************************************************************/

static int cmd_del(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	NTSTATUS status = NT_STATUS_OK;
	uint16 attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;

	if (recurse) {
		attribute |= FILE_ATTRIBUTE_DIRECTORY;
	}

	mask = talloc_strdup(ctx, client_get_cur_dir());
	if (!mask) {
		return 1;
	}
	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("del <filename>\n");
		return 1;
	}
	mask = talloc_asprintf_append(mask, "%s", buf);
	if (!mask) {
		return 1;
	}

	status = do_list(mask,attribute,do_del,false,false);
	if (!NT_STATUS_IS_OK(status)) {
		return 1;
	}
	return 0;
}

/****************************************************************************
 Wildcard delete some files.
****************************************************************************/

static int cmd_wdel(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	uint16 attribute;
	struct cli_state *targetcli;
	char *targetname = NULL;
	NTSTATUS status;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("wdel 0x<attrib> <wcard>\n");
		return 1;
	}

	attribute = (uint16)strtol(buf, (char **)NULL, 16);

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("wdel 0x<attrib> <wcard>\n");
		return 1;
	}

	mask = talloc_asprintf(ctx, "%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("cmd_wdel %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	status = cli_unlink(targetcli, targetname, attribute);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s deleting remote files %s\n", nt_errstr(status),
			 targetname);
	}
	return 0;
}

/****************************************************************************
****************************************************************************/

static int cmd_open(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	uint16_t fnum = (uint16_t)-1;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("open <filename>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("open %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_ntcreate(targetcli, targetname, 0,
			FILE_READ_DATA|FILE_WRITE_DATA, 0,
			FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum))) {
		if (NT_STATUS_IS_OK(cli_ntcreate(targetcli, targetname, 0,
				FILE_READ_DATA, 0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum))) {
			d_printf("open file %s: for read/write fnum %d\n", targetname, fnum);
		} else {
			d_printf("Failed to open file %s. %s\n", targetname, cli_errstr(cli));
		}
	} else {
		d_printf("open file %s: for read/write fnum %d\n", targetname, fnum);
	}
	return 0;
}

static int cmd_posix_encrypt(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;

	if (cli->use_kerberos) {
		status = cli_gss_smb_encryption_start(cli);
	} else {
		char *domain = NULL;
		char *user = NULL;
		char *password = NULL;

		if (!next_token_talloc(ctx, &cmd_ptr,&domain,NULL)) {
			d_printf("posix_encrypt domain user password\n");
			return 1;
		}

		if (!next_token_talloc(ctx, &cmd_ptr,&user,NULL)) {
			d_printf("posix_encrypt domain user password\n");
			return 1;
		}

		if (!next_token_talloc(ctx, &cmd_ptr,&password,NULL)) {
			d_printf("posix_encrypt domain user password\n");
			return 1;
		}

		status = cli_raw_ntlm_smb_encryption_start(cli,
							user,
							password,
							domain);
	}

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("posix_encrypt failed with error %s\n", nt_errstr(status));
	} else {
		d_printf("encryption on\n");
		smb_encrypt = true;
	}

	return 0;
}

/****************************************************************************
****************************************************************************/

static int cmd_posix_open(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	mode_t mode;
	uint16_t fnum;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("posix_open <filename> 0<mode>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("posix_open <filename> 0<mode>\n");
		return 1;
	}
	mode = (mode_t)strtol(buf, (char **)NULL, 8);

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("posix_open %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_open(targetcli, targetname, O_CREAT|O_RDWR, mode, &fnum))) {
		if (NT_STATUS_IS_OK(cli_posix_open(targetcli, targetname, O_CREAT|O_RDONLY, mode, &fnum))) {
			d_printf("posix_open file %s: for readonly fnum %d\n", targetname, fnum);
		} else {
			d_printf("Failed to open file %s. %s\n", targetname, cli_errstr(cli));
		}
	} else {
		d_printf("posix_open file %s: for read/write fnum %d\n", targetname, fnum);
	}

	return 0;
}

static int cmd_posix_mkdir(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	mode_t mode;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("posix_mkdir <filename> 0<mode>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("posix_mkdir <filename> 0<mode>\n");
		return 1;
	}
	mode = (mode_t)strtol(buf, (char **)NULL, 8);

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("posix_mkdir %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_mkdir(targetcli, targetname, mode))) {
		d_printf("Failed to open file %s. %s\n", targetname, cli_errstr(cli));
	} else {
		d_printf("posix_mkdir created directory %s\n", targetname);
	}
	return 0;
}

static int cmd_posix_unlink(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("posix_unlink <filename>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("posix_unlink %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_unlink(targetcli, targetname))) {
		d_printf("Failed to unlink file %s. %s\n", targetname, cli_errstr(cli));
	} else {
		d_printf("posix_unlink deleted file %s\n", targetname);
	}

	return 0;
}

static int cmd_posix_rmdir(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("posix_rmdir <filename>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("posix_rmdir %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_rmdir(targetcli, targetname))) {
		d_printf("Failed to unlink directory %s. %s\n", targetname, cli_errstr(cli));
	} else {
		d_printf("posix_rmdir deleted directory %s\n", targetname);
	}

	return 0;
}

static int cmd_close(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf = NULL;
	int fnum;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("close <fnum>\n");
		return 1;
	}

	fnum = atoi(buf);
	/* We really should use the targetcli here.... */
	if (!NT_STATUS_IS_OK(cli_close(cli, fnum))) {
		d_printf("close %d: %s\n", fnum, cli_errstr(cli));
		return 1;
	}
	return 0;
}

static int cmd_posix(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	uint16 major, minor;
	uint32 caplow, caphigh;
	char *caps;
	NTSTATUS status;

	if (!SERVER_HAS_UNIX_CIFS(cli)) {
		d_printf("Server doesn't support UNIX CIFS extensions.\n");
		return 1;
	}

	status = cli_unix_extensions_version(cli, &major, &minor, &caplow,
					     &caphigh);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Can't get UNIX CIFS extensions version from "
			 "server: %s\n", nt_errstr(status));
		return 1;
	}

	d_printf("Server supports CIFS extensions %u.%u\n", (unsigned int)major, (unsigned int)minor);

	caps = talloc_strdup(ctx, "");
	if (!caps) {
		return 1;
	}
        if (caplow & CIFS_UNIX_FCNTL_LOCKS_CAP) {
		caps = talloc_asprintf_append(caps, "locks ");
		if (!caps) {
			return 1;
		}
	}
        if (caplow & CIFS_UNIX_POSIX_ACLS_CAP) {
		caps = talloc_asprintf_append(caps, "acls ");
		if (!caps) {
			return 1;
		}
	}
        if (caplow & CIFS_UNIX_XATTTR_CAP) {
		caps = talloc_asprintf_append(caps, "eas ");
		if (!caps) {
			return 1;
		}
	}
        if (caplow & CIFS_UNIX_POSIX_PATHNAMES_CAP) {
		caps = talloc_asprintf_append(caps, "pathnames ");
		if (!caps) {
			return 1;
		}
	}
        if (caplow & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		caps = talloc_asprintf_append(caps, "posix_path_operations ");
		if (!caps) {
			return 1;
		}
	}
        if (caplow & CIFS_UNIX_LARGE_READ_CAP) {
		caps = talloc_asprintf_append(caps, "large_read ");
		if (!caps) {
			return 1;
		}
	}
        if (caplow & CIFS_UNIX_LARGE_WRITE_CAP) {
		caps = talloc_asprintf_append(caps, "large_write ");
		if (!caps) {
			return 1;
		}
	}
	if (caplow & CIFS_UNIX_TRANSPORT_ENCRYPTION_CAP) {
		caps = talloc_asprintf_append(caps, "posix_encrypt ");
		if (!caps) {
			return 1;
		}
	}
	if (caplow & CIFS_UNIX_TRANSPORT_ENCRYPTION_MANDATORY_CAP) {
		caps = talloc_asprintf_append(caps, "mandatory_posix_encrypt ");
		if (!caps) {
			return 1;
		}
	}

	if (*caps && caps[strlen(caps)-1] == ' ') {
		caps[strlen(caps)-1] = '\0';
	}

	d_printf("Server supports CIFS capabilities %s\n", caps);

	status = cli_set_unix_extensions_capabilities(cli, major, minor,
						      caplow, caphigh);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Can't set UNIX CIFS extensions capabilities. %s.\n",
			 nt_errstr(status));
		return 1;
	}

	if (caplow & CIFS_UNIX_POSIX_PATHNAMES_CAP) {
		CLI_DIRSEP_CHAR = '/';
		*CLI_DIRSEP_STR = '/';
		client_set_cur_dir(CLI_DIRSEP_STR);
	}

	return 0;
}

static int cmd_lock(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf = NULL;
	uint64_t start, len;
	enum brl_type lock_type;
	int fnum;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("lock <fnum> [r|w] <hex-start> <hex-len>\n");
		return 1;
	}
	fnum = atoi(buf);

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("lock <fnum> [r|w] <hex-start> <hex-len>\n");
		return 1;
	}

	if (*buf == 'r' || *buf == 'R') {
		lock_type = READ_LOCK;
	} else if (*buf == 'w' || *buf == 'W') {
		lock_type = WRITE_LOCK;
	} else {
		d_printf("lock <fnum> [r|w] <hex-start> <hex-len>\n");
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("lock <fnum> [r|w] <hex-start> <hex-len>\n");
		return 1;
	}

	start = (uint64_t)strtol(buf, (char **)NULL, 16);

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("lock <fnum> [r|w] <hex-start> <hex-len>\n");
		return 1;
	}

	len = (uint64_t)strtol(buf, (char **)NULL, 16);

	if (!NT_STATUS_IS_OK(cli_posix_lock(cli, fnum, start, len, true, lock_type))) {
		d_printf("lock failed %d: %s\n", fnum, cli_errstr(cli));
	}

	return 0;
}

static int cmd_unlock(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf = NULL;
	uint64_t start, len;
	int fnum;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("unlock <fnum> <hex-start> <hex-len>\n");
		return 1;
	}
	fnum = atoi(buf);

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("unlock <fnum> <hex-start> <hex-len>\n");
		return 1;
	}

	start = (uint64_t)strtol(buf, (char **)NULL, 16);

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("unlock <fnum> <hex-start> <hex-len>\n");
		return 1;
	}

	len = (uint64_t)strtol(buf, (char **)NULL, 16);

	if (!NT_STATUS_IS_OK(cli_posix_unlock(cli, fnum, start, len))) {
		d_printf("unlock failed %d: %s\n", fnum, cli_errstr(cli));
	}

	return 0;
}


/****************************************************************************
 Remove a directory.
****************************************************************************/

static int cmd_rmdir(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *mask = NULL;
	char *buf = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("rmdir <dirname>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!mask) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, mask, &targetcli, &targetname)) {
		d_printf("rmdir %s: %s\n", mask, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_rmdir(targetcli, targetname))) {
		d_printf("%s removing remote directory file %s\n",
			 cli_errstr(targetcli),mask);
	}

	return 0;
}

/****************************************************************************
 UNIX hardlink.
****************************************************************************/

static int cmd_link(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *oldname = NULL;
	char *newname = NULL;
	char *buf = NULL;
	char *buf2 = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf2,NULL)) {
		d_printf("link <oldname> <newname>\n");
		return 1;
	}
	oldname = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!oldname) {
		return 1;
	}
	newname = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf2);
	if (!newname) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, oldname, &targetcli, &targetname)) {
		d_printf("link %s: %s\n", oldname, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(targetcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_hardlink(targetcli, targetname, newname))) {
		d_printf("%s linking files (%s -> %s)\n", cli_errstr(targetcli), newname, oldname);
		return 1;
	}
	return 0;
}

/****************************************************************************
 UNIX readlink.
****************************************************************************/

static int cmd_readlink(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *name= NULL;
	char *buf = NULL;
	char *targetname = NULL;
	char linkname[PATH_MAX+1];
	struct cli_state *targetcli;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("readlink <name>\n");
		return 1;
	}
	name = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!name) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, name, &targetcli, &targetname)) {
		d_printf("readlink %s: %s\n", name, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(targetcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_readlink(targetcli, name,
			linkname, PATH_MAX+1))) {
		d_printf("%s readlink on file %s\n",
			cli_errstr(targetcli), name);
		return 1;
	}

	d_printf("%s -> %s\n", name, linkname);

	return 0;
}


/****************************************************************************
 UNIX symlink.
****************************************************************************/

static int cmd_symlink(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *oldname = NULL;
	char *newname = NULL;
	char *buf = NULL;
	char *buf2 = NULL;
	struct cli_state *newcli;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf2,NULL)) {
		d_printf("symlink <oldname> <newname>\n");
		return 1;
	}
	/* Oldname (link target) must be an untouched blob. */
	oldname = buf;

	newname = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf2);
	if (!newname) {
		return 1;
	}

	/* New name must be present in share namespace. */
	if (!cli_resolve_path(ctx, "", auth_info, cli, newname, &newcli, &newname)) {
		d_printf("link %s: %s\n", oldname, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(newcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_symlink(newcli, oldname, newname))) {
		d_printf("%s symlinking files (%s -> %s)\n",
			cli_errstr(newcli), newname, newname);
		return 1;
	}

	return 0;
}

/****************************************************************************
 UNIX chmod.
****************************************************************************/

static int cmd_chmod(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src = NULL;
	char *buf = NULL;
	char *buf2 = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	mode_t mode;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf2,NULL)) {
		d_printf("chmod mode file\n");
		return 1;
	}
	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf2);
	if (!src) {
		return 1;
	}

	mode = (mode_t)strtol(buf, NULL, 8);

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli, &targetname)) {
		d_printf("chmod %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(targetcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_chmod(targetcli, targetname, mode))) {
		d_printf("%s chmod file %s 0%o\n",
			cli_errstr(targetcli), src, (unsigned int)mode);
		return 1;
	}

	return 0;
}

static const char *filetype_to_str(mode_t mode)
{
	if (S_ISREG(mode)) {
		return "regular file";
	} else if (S_ISDIR(mode)) {
		return "directory";
	} else
#ifdef S_ISCHR
	if (S_ISCHR(mode)) {
		return "character device";
	} else
#endif
#ifdef S_ISBLK
	if (S_ISBLK(mode)) {
		return "block device";
	} else
#endif
#ifdef S_ISFIFO
	if (S_ISFIFO(mode)) {
		return "fifo";
	} else
#endif
#ifdef S_ISLNK
	if (S_ISLNK(mode)) {
		return "symbolic link";
	} else
#endif
#ifdef S_ISSOCK
	if (S_ISSOCK(mode)) {
		return "socket";
	} else
#endif
	return "";
}

static char rwx_to_str(mode_t m, mode_t bt, char ret)
{
	if (m & bt) {
		return ret;
	} else {
		return '-';
	}
}

static char *unix_mode_to_str(char *s, mode_t m)
{
	char *p = s;
	const char *str = filetype_to_str(m);

	switch(str[0]) {
		case 'd':
			*p++ = 'd';
			break;
		case 'c':
			*p++ = 'c';
			break;
		case 'b':
			*p++ = 'b';
			break;
		case 'f':
			*p++ = 'p';
			break;
		case 's':
			*p++ = str[1] == 'y' ? 'l' : 's';
			break;
		case 'r':
		default:
			*p++ = '-';
			break;
	}
	*p++ = rwx_to_str(m, S_IRUSR, 'r');
	*p++ = rwx_to_str(m, S_IWUSR, 'w');
	*p++ = rwx_to_str(m, S_IXUSR, 'x');
	*p++ = rwx_to_str(m, S_IRGRP, 'r');
	*p++ = rwx_to_str(m, S_IWGRP, 'w');
	*p++ = rwx_to_str(m, S_IXGRP, 'x');
	*p++ = rwx_to_str(m, S_IROTH, 'r');
	*p++ = rwx_to_str(m, S_IWOTH, 'w');
	*p++ = rwx_to_str(m, S_IXOTH, 'x');
	*p++ = '\0';
	return s;
}

/****************************************************************************
 Utility function for UNIX getfacl.
****************************************************************************/

static char *perms_to_string(fstring permstr, unsigned char perms)
{
	fstrcpy(permstr, "---");
	if (perms & SMB_POSIX_ACL_READ) {
		permstr[0] = 'r';
	}
	if (perms & SMB_POSIX_ACL_WRITE) {
		permstr[1] = 'w';
	}
	if (perms & SMB_POSIX_ACL_EXECUTE) {
		permstr[2] = 'x';
	}
	return permstr;
}

/****************************************************************************
 UNIX getfacl.
****************************************************************************/

static int cmd_getfacl(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src = NULL;
	char *name = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	uint16 major, minor;
	uint32 caplow, caphigh;
	char *retbuf = NULL;
	size_t rb_size = 0;
	SMB_STRUCT_STAT sbuf;
	uint16 num_file_acls = 0;
	uint16 num_dir_acls = 0;
	uint16 i;
	NTSTATUS status;

	if (!next_token_talloc(ctx, &cmd_ptr,&name,NULL)) {
		d_printf("getfacl filename\n");
		return 1;
	}
	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			name);
	if (!src) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli, &targetname)) {
		d_printf("stat %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(targetcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	status = cli_unix_extensions_version(targetcli, &major, &minor,
					     &caplow, &caphigh);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Can't get UNIX CIFS version from server: %s.\n",
			 nt_errstr(status));
		return 1;
	}

	if (!(caplow & CIFS_UNIX_POSIX_ACLS_CAP)) {
		d_printf("This server supports UNIX extensions "
			"but doesn't support POSIX ACLs.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_stat(targetcli, targetname, &sbuf))) {
		d_printf("%s getfacl doing a stat on file %s\n",
			cli_errstr(targetcli), src);
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_getfacl(targetcli, targetname, ctx, &rb_size, &retbuf))) {
		d_printf("%s getfacl file %s\n",
			cli_errstr(targetcli), src);
		return 1;
	}

	/* ToDo : Print out the ACL values. */
	if (rb_size < 6 || SVAL(retbuf,0) != SMB_POSIX_ACL_VERSION) {
		d_printf("getfacl file %s, unknown POSIX acl version %u.\n",
			src, (unsigned int)CVAL(retbuf,0) );
		return 1;
	}

	num_file_acls = SVAL(retbuf,2);
	num_dir_acls = SVAL(retbuf,4);
	if (rb_size != SMB_POSIX_ACL_HEADER_SIZE + SMB_POSIX_ACL_ENTRY_SIZE*(num_file_acls+num_dir_acls)) {
		d_printf("getfacl file %s, incorrect POSIX acl buffer size (should be %u, was %u).\n",
			src,
			(unsigned int)(SMB_POSIX_ACL_HEADER_SIZE + SMB_POSIX_ACL_ENTRY_SIZE*(num_file_acls+num_dir_acls)),
			(unsigned int)rb_size);
		return 1;
	}

	d_printf("# file: %s\n", src);
	d_printf("# owner: %u\n# group: %u\n", (unsigned int)sbuf.st_ex_uid, (unsigned int)sbuf.st_ex_gid);

	if (num_file_acls == 0 && num_dir_acls == 0) {
		d_printf("No acls found.\n");
	}

	for (i = 0; i < num_file_acls; i++) {
		uint32 uorg;
		fstring permstring;
		unsigned char tagtype = CVAL(retbuf, SMB_POSIX_ACL_HEADER_SIZE+(i*SMB_POSIX_ACL_ENTRY_SIZE));
		unsigned char perms = CVAL(retbuf, SMB_POSIX_ACL_HEADER_SIZE+(i*SMB_POSIX_ACL_ENTRY_SIZE)+1);

		switch(tagtype) {
			case SMB_POSIX_ACL_USER_OBJ:
				d_printf("user::");
				break;
			case SMB_POSIX_ACL_USER:
				uorg = IVAL(retbuf,SMB_POSIX_ACL_HEADER_SIZE+(i*SMB_POSIX_ACL_ENTRY_SIZE)+2);
				d_printf("user:%u:", uorg);
				break;
			case SMB_POSIX_ACL_GROUP_OBJ:
				d_printf("group::");
				break;
			case SMB_POSIX_ACL_GROUP:
				uorg = IVAL(retbuf,SMB_POSIX_ACL_HEADER_SIZE+(i*SMB_POSIX_ACL_ENTRY_SIZE)+2);
				d_printf("group:%u:", uorg);
				break;
			case SMB_POSIX_ACL_MASK:
				d_printf("mask::");
				break;
			case SMB_POSIX_ACL_OTHER:
				d_printf("other::");
				break;
			default:
				d_printf("getfacl file %s, incorrect POSIX acl tagtype (%u).\n",
					src, (unsigned int)tagtype );
				SAFE_FREE(retbuf);
				return 1;
		}

		d_printf("%s\n", perms_to_string(permstring, perms));
	}

	for (i = 0; i < num_dir_acls; i++) {
		uint32 uorg;
		fstring permstring;
		unsigned char tagtype = CVAL(retbuf, SMB_POSIX_ACL_HEADER_SIZE+((i+num_file_acls)*SMB_POSIX_ACL_ENTRY_SIZE));
		unsigned char perms = CVAL(retbuf, SMB_POSIX_ACL_HEADER_SIZE+((i+num_file_acls)*SMB_POSIX_ACL_ENTRY_SIZE)+1);

		switch(tagtype) {
			case SMB_POSIX_ACL_USER_OBJ:
				d_printf("default:user::");
				break;
			case SMB_POSIX_ACL_USER:
				uorg = IVAL(retbuf,SMB_POSIX_ACL_HEADER_SIZE+((i+num_file_acls)*SMB_POSIX_ACL_ENTRY_SIZE)+2);
				d_printf("default:user:%u:", uorg);
				break;
			case SMB_POSIX_ACL_GROUP_OBJ:
				d_printf("default:group::");
				break;
			case SMB_POSIX_ACL_GROUP:
				uorg = IVAL(retbuf,SMB_POSIX_ACL_HEADER_SIZE+((i+num_file_acls)*SMB_POSIX_ACL_ENTRY_SIZE)+2);
				d_printf("default:group:%u:", uorg);
				break;
			case SMB_POSIX_ACL_MASK:
				d_printf("default:mask::");
				break;
			case SMB_POSIX_ACL_OTHER:
				d_printf("default:other::");
				break;
			default:
				d_printf("getfacl file %s, incorrect POSIX acl tagtype (%u).\n",
					src, (unsigned int)tagtype );
				SAFE_FREE(retbuf);
				return 1;
		}

		d_printf("%s\n", perms_to_string(permstring, perms));
	}

	return 0;
}

static void printf_cb(const char *buf, void *private_data)
{
	printf("%s", buf);
}

/****************************************************************************
 Get the EA list of a file
****************************************************************************/

static int cmd_geteas(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src = NULL;
	char *name = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	NTSTATUS status;
	size_t i, num_eas;
	struct ea_struct *eas;

	if (!next_token_talloc(ctx, &cmd_ptr,&name,NULL)) {
		d_printf("geteas filename\n");
		return 1;
	}
	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			name);
	if (!src) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli,
			      &targetname)) {
		d_printf("stat %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	status = cli_get_ea_list_path(targetcli, targetname, talloc_tos(),
				      &num_eas, &eas);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("cli_get_ea_list_path: %s\n", nt_errstr(status));
		return 1;
	}

	for (i=0; i<num_eas; i++) {
		d_printf("%s (%d) =\n", eas[i].name, (int)eas[i].flags);
		dump_data_cb(eas[i].value.data, eas[i].value.length, false,
			     printf_cb, NULL);
		d_printf("\n");
	}

	TALLOC_FREE(eas);

	return 0;
}

/****************************************************************************
 Set an EA of a file
****************************************************************************/

static int cmd_setea(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src = NULL;
	char *name = NULL;
	char *eaname = NULL;
	char *eavalue = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	NTSTATUS status;

	if (!next_token_talloc(ctx, &cmd_ptr, &name, NULL)
	    || !next_token_talloc(ctx, &cmd_ptr, &eaname, NULL)) {
		d_printf("setea filename eaname value\n");
		return 1;
	}
	if (!next_token_talloc(ctx, &cmd_ptr, &eavalue, NULL)) {
		eavalue = talloc_strdup(ctx, "");
	}
	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			name);
	if (!src) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli,
			      &targetname)) {
		d_printf("stat %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	status =  cli_set_ea_path(targetcli, targetname, eaname, eavalue,
				  strlen(eavalue));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("set_ea %s: %s\n", src, nt_errstr(status));
		return 1;
	}

	return 0;
}

/****************************************************************************
 UNIX stat.
****************************************************************************/

static int cmd_stat(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src = NULL;
	char *name = NULL;
	char *targetname = NULL;
	struct cli_state *targetcli;
	fstring mode_str;
	SMB_STRUCT_STAT sbuf;
	struct tm *lt;
	time_t tmp_time;

	if (!next_token_talloc(ctx, &cmd_ptr,&name,NULL)) {
		d_printf("stat file\n");
		return 1;
	}
	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			name);
	if (!src) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli, &targetname)) {
		d_printf("stat %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(targetcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_stat(targetcli, targetname, &sbuf))) {
		d_printf("%s stat file %s\n",
			cli_errstr(targetcli), src);
		return 1;
	}

	/* Print out the stat values. */
	d_printf("File: %s\n", src);
	d_printf("Size: %-12.0f\tBlocks: %u\t%s\n",
		(double)sbuf.st_ex_size,
		(unsigned int)sbuf.st_ex_blocks,
		filetype_to_str(sbuf.st_ex_mode));

#if defined(S_ISCHR) && defined(S_ISBLK)
	if (S_ISCHR(sbuf.st_ex_mode) || S_ISBLK(sbuf.st_ex_mode)) {
		d_printf("Inode: %.0f\tLinks: %u\tDevice type: %u,%u\n",
			(double)sbuf.st_ex_ino,
			(unsigned int)sbuf.st_ex_nlink,
			unix_dev_major(sbuf.st_ex_rdev),
			unix_dev_minor(sbuf.st_ex_rdev));
	} else
#endif
		d_printf("Inode: %.0f\tLinks: %u\n",
			(double)sbuf.st_ex_ino,
			(unsigned int)sbuf.st_ex_nlink);

	d_printf("Access: (0%03o/%s)\tUid: %u\tGid: %u\n",
		((int)sbuf.st_ex_mode & 0777),
		unix_mode_to_str(mode_str, sbuf.st_ex_mode),
		(unsigned int)sbuf.st_ex_uid,
		(unsigned int)sbuf.st_ex_gid);

	tmp_time = convert_timespec_to_time_t(sbuf.st_ex_atime);
	lt = localtime(&tmp_time);
	if (lt) {
		strftime(mode_str, sizeof(mode_str), "%Y-%m-%d %T %z", lt);
	} else {
		fstrcpy(mode_str, "unknown");
	}
	d_printf("Access: %s\n", mode_str);

	tmp_time = convert_timespec_to_time_t(sbuf.st_ex_mtime);
	lt = localtime(&tmp_time);
	if (lt) {
		strftime(mode_str, sizeof(mode_str), "%Y-%m-%d %T %z", lt);
	} else {
		fstrcpy(mode_str, "unknown");
	}
	d_printf("Modify: %s\n", mode_str);

	tmp_time = convert_timespec_to_time_t(sbuf.st_ex_ctime);
	lt = localtime(&tmp_time);
	if (lt) {
		strftime(mode_str, sizeof(mode_str), "%Y-%m-%d %T %z", lt);
	} else {
		fstrcpy(mode_str, "unknown");
	}
	d_printf("Change: %s\n", mode_str);

	return 0;
}


/****************************************************************************
 UNIX chown.
****************************************************************************/

static int cmd_chown(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src = NULL;
	uid_t uid;
	gid_t gid;
	char *buf, *buf2, *buf3;
	struct cli_state *targetcli;
	char *targetname = NULL;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf2,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf3,NULL)) {
		d_printf("chown uid gid file\n");
		return 1;
	}

	uid = (uid_t)atoi(buf);
	gid = (gid_t)atoi(buf2);

	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf3);
	if (!src) {
		return 1;
	}
	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli, &targetname) ) {
		d_printf("chown %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	if (!SERVER_HAS_UNIX_CIFS(targetcli)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_posix_chown(targetcli, targetname, uid, gid))) {
		d_printf("%s chown file %s uid=%d, gid=%d\n",
			cli_errstr(targetcli), src, (int)uid, (int)gid);
		return 1;
	}

	return 0;
}

/****************************************************************************
 Rename some file.
****************************************************************************/

static int cmd_rename(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src, *dest;
	char *buf, *buf2;
	struct cli_state *targetcli;
	char *targetsrc;
	char *targetdest;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf2,NULL)) {
		d_printf("rename <src> <dest>\n");
		return 1;
	}

	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!src) {
		return 1;
	}

	dest = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf2);
	if (!dest) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli, &targetsrc)) {
		d_printf("rename %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, dest, &targetcli, &targetdest)) {
		d_printf("rename %s: %s\n", dest, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_rename(targetcli, targetsrc, targetdest))) {
		d_printf("%s renaming files %s -> %s \n",
			cli_errstr(targetcli),
			targetsrc,
			targetdest);
		return 1;
	}

	return 0;
}

/****************************************************************************
 Print the volume name.
****************************************************************************/

static int cmd_volume(void)
{
	fstring volname;
	uint32 serial_num;
	time_t create_date;
	NTSTATUS status;

	status = cli_get_fs_volume_info(cli, volname, &serial_num,
					&create_date);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Error %s getting volume info\n", nt_errstr(status));
		return 1;
	}

	d_printf("Volume: |%s| serial number 0x%x\n",
			volname, (unsigned int)serial_num);
	return 0;
}

/****************************************************************************
 Hard link files using the NT call.
****************************************************************************/

static int cmd_hardlink(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *src, *dest;
	char *buf, *buf2;
	struct cli_state *targetcli;
	char *targetname;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL) ||
	    !next_token_talloc(ctx, &cmd_ptr,&buf2,NULL)) {
		d_printf("hardlink <src> <dest>\n");
		return 1;
	}

	src = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf);
	if (!src) {
		return 1;
	}

	dest = talloc_asprintf(ctx,
			"%s%s",
			client_get_cur_dir(),
			buf2);
	if (!dest) {
		return 1;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, src, &targetcli, &targetname)) {
		d_printf("hardlink %s: %s\n", src, cli_errstr(cli));
		return 1;
	}

	if (!NT_STATUS_IS_OK(cli_nt_hardlink(targetcli, targetname, dest))) {
		d_printf("%s doing an NT hard link of files\n",cli_errstr(targetcli));
		return 1;
	}

	return 0;
}

/****************************************************************************
 Toggle the prompt flag.
****************************************************************************/

static int cmd_prompt(void)
{
	prompt = !prompt;
	DEBUG(2,("prompting is now %s\n",prompt?"on":"off"));
	return 1;
}

/****************************************************************************
 Set the newer than time.
****************************************************************************/

static int cmd_newer(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf;
	bool ok;
	SMB_STRUCT_STAT sbuf;

	ok = next_token_talloc(ctx, &cmd_ptr,&buf,NULL);
	if (ok && (sys_stat(buf, &sbuf, false) == 0)) {
		newer_than = convert_timespec_to_time_t(sbuf.st_ex_mtime);
		DEBUG(1,("Getting files newer than %s",
			 time_to_asc(newer_than)));
	} else {
		newer_than = 0;
	}

	if (ok && newer_than == 0) {
		d_printf("Error setting newer-than time\n");
		return 1;
	}

	return 0;
}

/****************************************************************************
 Set the archive level.
****************************************************************************/

static int cmd_archive(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf;

	if (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		archive_level = atoi(buf);
	} else {
		d_printf("Archive level is %d\n",archive_level);
	}

	return 0;
}

/****************************************************************************
 Toggle the lowercaseflag.
****************************************************************************/

static int cmd_lowercase(void)
{
	lowercase = !lowercase;
	DEBUG(2,("filename lowercasing is now %s\n",lowercase?"on":"off"));
	return 0;
}

/****************************************************************************
 Toggle the case sensitive flag.
****************************************************************************/

static int cmd_setcase(void)
{
	bool orig_case_sensitive = cli_set_case_sensitive(cli, false);

	cli_set_case_sensitive(cli, !orig_case_sensitive);
	DEBUG(2,("filename case sensitivity is now %s\n",!orig_case_sensitive ?
		"on":"off"));
	return 0;
}

/****************************************************************************
 Toggle the showacls flag.
****************************************************************************/

static int cmd_showacls(void)
{
	showacls = !showacls;
	DEBUG(2,("showacls is now %s\n",showacls?"on":"off"));
	return 0;
}


/****************************************************************************
 Toggle the recurse flag.
****************************************************************************/

static int cmd_recurse(void)
{
	recurse = !recurse;
	DEBUG(2,("directory recursion is now %s\n",recurse?"on":"off"));
	return 0;
}

/****************************************************************************
 Toggle the translate flag.
****************************************************************************/

static int cmd_translate(void)
{
	translation = !translation;
	DEBUG(2,("CR/LF<->LF and print text translation now %s\n",
		 translation?"on":"off"));
	return 0;
}

/****************************************************************************
 Do the lcd command.
 ****************************************************************************/

static int cmd_lcd(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf;
	char *d;

	if (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		if (chdir(buf) == -1) {
			d_printf("chdir to %s failed (%s)\n",
				buf, strerror(errno));
		}
	}
	d = TALLOC_ARRAY(ctx, char, PATH_MAX+1);
	if (!d) {
		return 1;
	}
	DEBUG(2,("the local directory is now %s\n",sys_getwd(d)));
	return 0;
}

/****************************************************************************
 Get a file restarting at end of local file.
 ****************************************************************************/

static int cmd_reget(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *local_name = NULL;
	char *remote_name = NULL;
	char *fname = NULL;
	char *p = NULL;

	remote_name = talloc_strdup(ctx, client_get_cur_dir());
	if (!remote_name) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr, &fname, NULL)) {
		d_printf("reget <filename>\n");
		return 1;
	}
	remote_name = talloc_asprintf_append(remote_name, "%s", fname);
	if (!remote_name) {
		return 1;
	}
	remote_name = clean_name(ctx,remote_name);
	if (!remote_name) {
		return 1;
	}

	local_name = fname;
	next_token_talloc(ctx, &cmd_ptr, &p, NULL);
	if (p) {
		local_name = p;
	}

	return do_get(remote_name, local_name, true);
}

/****************************************************************************
 Put a file restarting at end of local file.
 ****************************************************************************/

static int cmd_reput(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *local_name = NULL;
	char *remote_name = NULL;
	char *buf;
	SMB_STRUCT_STAT st;

	remote_name = talloc_strdup(ctx, client_get_cur_dir());
	if (!remote_name) {
		return 1;
	}

	if (!next_token_talloc(ctx, &cmd_ptr, &local_name, NULL)) {
		d_printf("reput <filename>\n");
		return 1;
	}

	if (!file_exist_stat(local_name, &st, false)) {
		d_printf("%s does not exist\n", local_name);
		return 1;
	}

	if (next_token_talloc(ctx, &cmd_ptr, &buf, NULL)) {
		remote_name = talloc_asprintf_append(remote_name,
						"%s", buf);
	} else {
		remote_name = talloc_asprintf_append(remote_name,
						"%s", local_name);
	}
	if (!remote_name) {
		return 1;
	}

	remote_name = clean_name(ctx, remote_name);
	if (!remote_name) {
		return 1;
	}

	return do_put(remote_name, local_name, true);
}

/****************************************************************************
 List a share name.
 ****************************************************************************/

static void browse_fn(const char *name, uint32 m,
                      const char *comment, void *state)
{
	const char *typestr = "";

        switch (m & 7) {
	case STYPE_DISKTREE:
		typestr = "Disk";
		break;
	case STYPE_PRINTQ:
		typestr = "Printer";
		break;
	case STYPE_DEVICE:
		typestr = "Device";
		break;
	case STYPE_IPC:
		typestr = "IPC";
		break;
        }
	/* FIXME: If the remote machine returns non-ascii characters
	   in any of these fields, they can corrupt the output.  We
	   should remove them. */
	if (!grepable) {
		d_printf("\t%-15s %-10.10s%s\n",
               		name,typestr,comment);
	} else {
		d_printf ("%s|%s|%s\n",typestr,name,comment);
	}
}

static bool browse_host_rpc(bool sort)
{
	NTSTATUS status;
	struct rpc_pipe_client *pipe_hnd = NULL;
	TALLOC_CTX *frame = talloc_stackframe();
	WERROR werr;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr1 ctr1;
	uint32_t resume_handle = 0;
	uint32_t total_entries = 0;
	int i;
	struct dcerpc_binding_handle *b;

	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_srvsvc.syntax_id,
					  &pipe_hnd);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not connect to srvsvc pipe: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(frame);
		return false;
	}

	b = pipe_hnd->binding_handle;

	ZERO_STRUCT(info_ctr);
	ZERO_STRUCT(ctr1);

	info_ctr.level = 1;
	info_ctr.ctr.ctr1 = &ctr1;

	status = dcerpc_srvsvc_NetShareEnumAll(b, frame,
					      pipe_hnd->desthost,
					      &info_ctr,
					      0xffffffff,
					      &total_entries,
					      &resume_handle,
					      &werr);

	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(werr)) {
		TALLOC_FREE(pipe_hnd);
		TALLOC_FREE(frame);
		return false;
	}

	for (i=0; i < info_ctr.ctr.ctr1->count; i++) {
		struct srvsvc_NetShareInfo1 info = info_ctr.ctr.ctr1->array[i];
		browse_fn(info.name, info.type, info.comment, NULL);
	}

	TALLOC_FREE(pipe_hnd);
	TALLOC_FREE(frame);
	return true;
}

/****************************************************************************
 Try and browse available connections on a host.
****************************************************************************/

static bool browse_host(bool sort)
{
	int ret;
	if (!grepable) {
	        d_printf("\n\tSharename       Type      Comment\n");
	        d_printf("\t---------       ----      -------\n");
	}

	if (browse_host_rpc(sort)) {
		return true;
	}

	if((ret = cli_RNetShareEnum(cli, browse_fn, NULL)) == -1)
		d_printf("Error returning browse list: %s\n", cli_errstr(cli));

	return (ret != -1);
}

/****************************************************************************
 List a server name.
****************************************************************************/

static void server_fn(const char *name, uint32 m,
                      const char *comment, void *state)
{

	if (!grepable){
		d_printf("\t%-16s     %s\n", name, comment);
	} else {
		d_printf("%s|%s|%s\n",(char *)state, name, comment);
	}
}

/****************************************************************************
 Try and browse available connections on a host.
****************************************************************************/

static bool list_servers(const char *wk_grp)
{
	fstring state;

	if (!cli->server_domain)
		return false;

	if (!grepable) {
        	d_printf("\n\tServer               Comment\n");
        	d_printf("\t---------            -------\n");
	};
	fstrcpy( state, "Server" );
	cli_NetServerEnum(cli, cli->server_domain, SV_TYPE_ALL, server_fn,
			  state);

	if (!grepable) {
	        d_printf("\n\tWorkgroup            Master\n");
	        d_printf("\t---------            -------\n");
	};

	fstrcpy( state, "Workgroup" );
	cli_NetServerEnum(cli, cli->server_domain, SV_TYPE_DOMAIN_ENUM,
			  server_fn, state);
	return true;
}

/****************************************************************************
 Print or set current VUID
****************************************************************************/

static int cmd_vuid(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		d_printf("Current VUID is %d\n", cli->vuid);
		return 0;
	}

	cli->vuid = atoi(buf);
	return 0;
}

/****************************************************************************
 Setup a new VUID, by issuing a session setup
****************************************************************************/

static int cmd_logon(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *l_username, *l_password;
	NTSTATUS nt_status;

	if (!next_token_talloc(ctx, &cmd_ptr,&l_username,NULL)) {
		d_printf("logon <username> [<password>]\n");
		return 0;
	}

	if (!next_token_talloc(ctx, &cmd_ptr,&l_password,NULL)) {
		char *pass = getpass("Password: ");
		if (pass) {
			l_password = talloc_strdup(ctx,pass);
		}
	}
	if (!l_password) {
		return 1;
	}

	nt_status = cli_session_setup(cli, l_username,
				      l_password, strlen(l_password),
				      l_password, strlen(l_password),
				      lp_workgroup());
	if (!NT_STATUS_IS_OK(nt_status)) {
		d_printf("session setup failed: %s\n", nt_errstr(nt_status));
		return -1;
	}

	d_printf("Current VUID is %d\n", cli->vuid);
	return 0;
}


/****************************************************************************
 list active connections
****************************************************************************/

static int cmd_list_connect(void)
{
	cli_cm_display(cli);
	return 0;
}

/****************************************************************************
 display the current active client connection
****************************************************************************/

static int cmd_show_connect( void )
{
	TALLOC_CTX *ctx = talloc_tos();
	struct cli_state *targetcli;
	char *targetpath;

	if (!cli_resolve_path(ctx, "", auth_info, cli, client_get_cur_dir(),
				&targetcli, &targetpath ) ) {
		d_printf("showconnect %s: %s\n", cur_dir, cli_errstr(cli));
		return 1;
	}

	d_printf("//%s/%s\n", targetcli->desthost, targetcli->share);
	return 0;
}

/****************************************************************************
 iosize command
***************************************************************************/

int cmd_iosize(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *buf;
	int iosize;

	if (!next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		if (!smb_encrypt) {
			d_printf("iosize <n> or iosize 0x<n>. "
				"Minimum is 16384 (0x4000), "
				"max is 16776960 (0xFFFF00)\n");
		} else {
			d_printf("iosize <n> or iosize 0x<n>. "
				"(Encrypted connection) ,"
				"Minimum is 16384 (0x4000), "
				"max is 130048 (0x1FC00)\n");
		}
		return 1;
	}

	iosize = strtol(buf,NULL,0);
	if (smb_encrypt && (iosize < 0x4000 || iosize > 0xFC00)) {
		d_printf("iosize out of range for encrypted "
			"connection (min = 16384 (0x4000), "
			"max = 130048 (0x1FC00)");
		return 1;
	} else if (!smb_encrypt && (iosize < 0x4000 || iosize > 0xFFFF00)) {
		d_printf("iosize out of range (min = 16384 (0x4000), "
			"max = 16776960 (0xFFFF00)");
		return 1;
	}

	io_bufsize = iosize;
	d_printf("iosize is now %d\n", io_bufsize);
	return 0;
}

/****************************************************************************
history
****************************************************************************/
static int cmd_history(void)
{
#if defined(HAVE_LIBREADLINE) && defined(HAVE_HISTORY_LIST)
	HIST_ENTRY **hlist;
	int i;

	hlist = history_list();

	for (i = 0; hlist && hlist[i]; i++) {
		DEBUG(0, ("%d: %s\n", i, hlist[i]->line));
	}
#else
	DEBUG(0,("no history without readline support\n"));
#endif

	return 0;
}

/* Some constants for completing filename arguments */

#define COMPL_NONE        0          /* No completions */
#define COMPL_REMOTE      1          /* Complete remote filename */
#define COMPL_LOCAL       2          /* Complete local filename */

/* This defines the commands supported by this client.
 * NOTE: The "!" must be the last one in the list because it's fn pointer
 *       field is NULL, and NULL in that field is used in process_tok()
 *       (below) to indicate the end of the list.  crh
 */
static struct {
	const char *name;
	int (*fn)(void);
	const char *description;
	char compl_args[2];      /* Completion argument info */
} commands[] = {
  {"?",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"allinfo",cmd_allinfo,"<file> show all available info",
   {COMPL_NONE,COMPL_NONE}},
  {"altname",cmd_altname,"<file> show alt name",{COMPL_NONE,COMPL_NONE}},
  {"archive",cmd_archive,"<level>\n0=ignore archive bit\n1=only get archive files\n2=only get archive files and reset archive bit\n3=get all files and reset archive bit",{COMPL_NONE,COMPL_NONE}},
  {"blocksize",cmd_block,"blocksize <number> (default 20)",{COMPL_NONE,COMPL_NONE}},
  {"cancel",cmd_cancel,"<jobid> cancel a print queue entry",{COMPL_NONE,COMPL_NONE}},
  {"case_sensitive",cmd_setcase,"toggle the case sensitive flag to server",{COMPL_NONE,COMPL_NONE}},
  {"cd",cmd_cd,"[directory] change/report the remote directory",{COMPL_REMOTE,COMPL_NONE}},
  {"chmod",cmd_chmod,"<src> <mode> chmod a file using UNIX permission",{COMPL_REMOTE,COMPL_REMOTE}},
  {"chown",cmd_chown,"<src> <uid> <gid> chown a file using UNIX uids and gids",{COMPL_REMOTE,COMPL_REMOTE}},
  {"close",cmd_close,"<fid> close a file given a fid",{COMPL_REMOTE,COMPL_REMOTE}},
  {"del",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"dir",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"du",cmd_du,"<mask> computes the total size of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"echo",cmd_echo,"ping the server",{COMPL_NONE,COMPL_NONE}},
  {"exit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"get",cmd_get,"<remote name> [local name] get a file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"getfacl",cmd_getfacl,"<file name> get the POSIX ACL on a file (UNIX extensions only)",{COMPL_REMOTE,COMPL_LOCAL}},
  {"geteas", cmd_geteas, "<file name> get the EA list of a file",
   {COMPL_REMOTE, COMPL_LOCAL}},
  {"hardlink",cmd_hardlink,"<src> <dest> create a Windows hard link",{COMPL_REMOTE,COMPL_REMOTE}},
  {"help",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"history",cmd_history,"displays the command history",{COMPL_NONE,COMPL_NONE}},
  {"iosize",cmd_iosize,"iosize <number> (default 64512)",{COMPL_NONE,COMPL_NONE}},
  {"lcd",cmd_lcd,"[directory] change/report the local current working directory",{COMPL_LOCAL,COMPL_NONE}},
  {"link",cmd_link,"<oldname> <newname> create a UNIX hard link",{COMPL_REMOTE,COMPL_REMOTE}},
  {"lock",cmd_lock,"lock <fnum> [r|w] <hex-start> <hex-len> : set a POSIX lock",{COMPL_REMOTE,COMPL_REMOTE}},
  {"lowercase",cmd_lowercase,"toggle lowercasing of filenames for get",{COMPL_NONE,COMPL_NONE}},  
  {"ls",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"l",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"mask",cmd_select,"<mask> mask all filenames against this",{COMPL_REMOTE,COMPL_NONE}},
  {"md",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"mget",cmd_mget,"<mask> get all the matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"mkdir",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"more",cmd_more,"<remote name> view a remote file with your pager",{COMPL_REMOTE,COMPL_NONE}},  
  {"mput",cmd_mput,"<mask> put all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"newer",cmd_newer,"<file> only mget files newer than the specified local file",{COMPL_LOCAL,COMPL_NONE}},
  {"open",cmd_open,"<mask> open a file",{COMPL_REMOTE,COMPL_NONE}},
  {"posix", cmd_posix, "turn on all POSIX capabilities", {COMPL_REMOTE,COMPL_NONE}},
  {"posix_encrypt",cmd_posix_encrypt,"<domain> <user> <password> start up transport encryption",{COMPL_REMOTE,COMPL_NONE}},
  {"posix_open",cmd_posix_open,"<name> 0<mode> open_flags mode open a file using POSIX interface",{COMPL_REMOTE,COMPL_NONE}},
  {"posix_mkdir",cmd_posix_mkdir,"<name> 0<mode> creates a directory using POSIX interface",{COMPL_REMOTE,COMPL_NONE}},
  {"posix_rmdir",cmd_posix_rmdir,"<name> removes a directory using POSIX interface",{COMPL_REMOTE,COMPL_NONE}},
  {"posix_unlink",cmd_posix_unlink,"<name> removes a file using POSIX interface",{COMPL_REMOTE,COMPL_NONE}},
  {"print",cmd_print,"<file name> print a file",{COMPL_NONE,COMPL_NONE}},
  {"prompt",cmd_prompt,"toggle prompting for filenames for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"put",cmd_put,"<local name> [remote name] put a file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"pwd",cmd_pwd,"show current remote directory (same as 'cd' with no args)",{COMPL_NONE,COMPL_NONE}},
  {"q",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"queue",cmd_queue,"show the print queue",{COMPL_NONE,COMPL_NONE}},
  {"quit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"readlink",cmd_readlink,"filename Do a UNIX extensions readlink call on a symlink",{COMPL_REMOTE,COMPL_REMOTE}},
  {"rd",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"recurse",cmd_recurse,"toggle directory recursion for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"reget",cmd_reget,"<remote name> [local name] get a file restarting at end of local file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"rename",cmd_rename,"<src> <dest> rename some files",{COMPL_REMOTE,COMPL_REMOTE}},
  {"reput",cmd_reput,"<local name> [remote name] put a file restarting at end of remote file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"rm",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"rmdir",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"showacls",cmd_showacls,"toggle if ACLs are shown or not",{COMPL_NONE,COMPL_NONE}},  
  {"setea", cmd_setea, "<file name> <eaname> <eaval> Set an EA of a file",
   {COMPL_REMOTE, COMPL_LOCAL}},
  {"setmode",cmd_setmode,"filename <setmode string> change modes of file",{COMPL_REMOTE,COMPL_NONE}},
  {"stat",cmd_stat,"filename Do a UNIX extensions stat call on a file",{COMPL_REMOTE,COMPL_REMOTE}},
  {"symlink",cmd_symlink,"<oldname> <newname> create a UNIX symlink",{COMPL_REMOTE,COMPL_REMOTE}},
  {"tar",cmd_tar,"tar <c|x>[IXFqbgNan] current directory to/from <file name>",{COMPL_NONE,COMPL_NONE}},
  {"tarmode",cmd_tarmode,"<full|inc|reset|noreset> tar's behaviour towards archive bits",{COMPL_NONE,COMPL_NONE}},
  {"translate",cmd_translate,"toggle text translation for printing",{COMPL_NONE,COMPL_NONE}},
  {"unlock",cmd_unlock,"unlock <fnum> <hex-start> <hex-len> : remove a POSIX lock",{COMPL_REMOTE,COMPL_REMOTE}},
  {"volume",cmd_volume,"print the volume name",{COMPL_NONE,COMPL_NONE}},
  {"vuid",cmd_vuid,"change current vuid",{COMPL_NONE,COMPL_NONE}},
  {"wdel",cmd_wdel,"<attrib> <mask> wildcard delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"logon",cmd_logon,"establish new logon",{COMPL_NONE,COMPL_NONE}},
  {"listconnect",cmd_list_connect,"list open connections",{COMPL_NONE,COMPL_NONE}},
  {"showconnect",cmd_show_connect,"display the current active connection",{COMPL_NONE,COMPL_NONE}},
  {"..",cmd_cd_oneup,"change the remote directory (up one level)",{COMPL_REMOTE,COMPL_NONE}},

  /* Yes, this must be here, see crh's comment above. */
  {"!",NULL,"run a shell command on the local system",{COMPL_NONE,COMPL_NONE}},
  {NULL,NULL,NULL,{COMPL_NONE,COMPL_NONE}}
};

/*******************************************************************
 Lookup a command string in the list of commands, including
 abbreviations.
******************************************************************/

static int process_tok(char *tok)
{
	int i = 0, matches = 0;
	int cmd=0;
	int tok_len = strlen(tok);

	while (commands[i].fn != NULL) {
		if (strequal(commands[i].name,tok)) {
			matches = 1;
			cmd = i;
			break;
		} else if (strnequal(commands[i].name, tok, tok_len)) {
			matches++;
			cmd = i;
		}
		i++;
	}

	if (matches == 0)
		return(-1);
	else if (matches == 1)
		return(cmd);
	else
		return(-2);
}

/****************************************************************************
 Help.
****************************************************************************/

static int cmd_help(void)
{
	TALLOC_CTX *ctx = talloc_tos();
	int i=0,j;
	char *buf;

	if (next_token_talloc(ctx, &cmd_ptr,&buf,NULL)) {
		if ((i = process_tok(buf)) >= 0)
			d_printf("HELP %s:\n\t%s\n\n",
				commands[i].name,commands[i].description);
	} else {
		while (commands[i].description) {
			for (j=0; commands[i].description && (j<5); j++) {
				d_printf("%-15s",commands[i].name);
				i++;
			}
			d_printf("\n");
		}
	}
	return 0;
}

/****************************************************************************
 Process a -c command string.
****************************************************************************/

static int process_command_string(const char *cmd_in)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *cmd = talloc_strdup(ctx, cmd_in);
	int rc = 0;

	if (!cmd) {
		return 1;
	}
	/* establish the connection if not already */

	if (!cli) {
		cli = cli_cm_open(talloc_tos(), NULL,
				have_ip ? dest_ss_str : desthost,
				service, auth_info,
				true, smb_encrypt,
				max_protocol, port, name_type);
		if (!cli) {
			return 1;
		}
	}

	while (cmd[0] != '\0')    {
		char *line;
		char *p;
		char *tok;
		int i;

		if ((p = strchr_m(cmd, ';')) == 0) {
			line = cmd;
			cmd += strlen(cmd);
		} else {
			*p = '\0';
			line = cmd;
			cmd = p + 1;
		}

		/* and get the first part of the command */
		cmd_ptr = line;
		if (!next_token_talloc(ctx, &cmd_ptr,&tok,NULL)) {
			continue;
		}

		if ((i = process_tok(tok)) >= 0) {
			rc = commands[i].fn();
		} else if (i == -2) {
			d_printf("%s: command abbreviation ambiguous\n",tok);
		} else {
			d_printf("%s: command not found\n",tok);
		}
	}

	return rc;
}

#define MAX_COMPLETIONS 100

struct completion_remote {
	char *dirmask;
	char **matches;
	int count, samelen;
	const char *text;
	int len;
};

static NTSTATUS completion_remote_filter(const char *mnt,
				struct file_info *f,
				const char *mask,
				void *state)
{
	struct completion_remote *info = (struct completion_remote *)state;

	if (info->count >= MAX_COMPLETIONS - 1) {
		return NT_STATUS_OK;
	}
	if (strncmp(info->text, f->name, info->len) != 0) {
		return NT_STATUS_OK;
	}
	if (ISDOT(f->name) || ISDOTDOT(f->name)) {
		return NT_STATUS_OK;
	}

	if ((info->dirmask[0] == 0) && !(f->mode & FILE_ATTRIBUTE_DIRECTORY))
		info->matches[info->count] = SMB_STRDUP(f->name);
	else {
		TALLOC_CTX *ctx = talloc_stackframe();
		char *tmp;

		tmp = talloc_strdup(ctx,info->dirmask);
		if (!tmp) {
			TALLOC_FREE(ctx);
			return NT_STATUS_NO_MEMORY;
		}
		tmp = talloc_asprintf_append(tmp, "%s", f->name);
		if (!tmp) {
			TALLOC_FREE(ctx);
			return NT_STATUS_NO_MEMORY;
		}
		if (f->mode & FILE_ATTRIBUTE_DIRECTORY) {
			tmp = talloc_asprintf_append(tmp, "%s",
						     CLI_DIRSEP_STR);
		}
		if (!tmp) {
			TALLOC_FREE(ctx);
			return NT_STATUS_NO_MEMORY;
		}
		info->matches[info->count] = SMB_STRDUP(tmp);
		TALLOC_FREE(ctx);
	}
	if (info->matches[info->count] == NULL) {
		return NT_STATUS_OK;
	}
	if (f->mode & FILE_ATTRIBUTE_DIRECTORY) {
		smb_readline_ca_char(0);
	}
	if (info->count == 1) {
		info->samelen = strlen(info->matches[info->count]);
	} else {
		while (strncmp(info->matches[info->count],
			       info->matches[info->count-1],
			       info->samelen) != 0) {
			info->samelen--;
		}
	}
	info->count++;
	return NT_STATUS_OK;
}

static char **remote_completion(const char *text, int len)
{
	TALLOC_CTX *ctx = talloc_stackframe();
	char *dirmask = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	int i;
	struct completion_remote info = { NULL, NULL, 1, 0, NULL, 0 };
	NTSTATUS status;

	/* can't have non-static initialisation on Sun CC, so do it
	   at run time here */
	info.samelen = len;
	info.text = text;
	info.len = len;

	info.matches = SMB_MALLOC_ARRAY(char *,MAX_COMPLETIONS);
	if (!info.matches) {
		TALLOC_FREE(ctx);
		return NULL;
	}

	/*
	 * We're leaving matches[0] free to fill it later with the text to
	 * display: Either the one single match or the longest common subset
	 * of the matches.
	 */
	info.matches[0] = NULL;
	info.count = 1;

	for (i = len-1; i >= 0; i--) {
		if ((text[i] == '/') || (text[i] == CLI_DIRSEP_CHAR)) {
			break;
		}
	}

	info.text = text+i+1;
	info.samelen = info.len = len-i-1;

	if (i > 0) {
		info.dirmask = SMB_MALLOC_ARRAY(char, i+2);
		if (!info.dirmask) {
			goto cleanup;
		}
		strncpy(info.dirmask, text, i+1);
		info.dirmask[i+1] = 0;
		dirmask = talloc_asprintf(ctx,
					"%s%*s*",
					client_get_cur_dir(),
					i-1,
					text);
	} else {
		info.dirmask = SMB_STRDUP("");
		if (!info.dirmask) {
			goto cleanup;
		}
		dirmask = talloc_asprintf(ctx,
					"%s*",
					client_get_cur_dir());
	}
	if (!dirmask) {
		goto cleanup;
	}

	if (!cli_resolve_path(ctx, "", auth_info, cli, dirmask, &targetcli, &targetpath)) {
		goto cleanup;
	}
	status = cli_list(targetcli, targetpath, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN,
			  completion_remote_filter, (void *)&info);
	if (!NT_STATUS_IS_OK(status)) {
		goto cleanup;
	}

	if (info.count == 1) {
		/*
		 * No matches at all, NULL indicates there is nothing
		 */
		SAFE_FREE(info.matches[0]);
		SAFE_FREE(info.matches);
		TALLOC_FREE(ctx);
		return NULL;
	}

	if (info.count == 2) {
		/*
		 * Exactly one match in matches[1], indicate this is the one
		 * in matches[0].
		 */
		info.matches[0] = info.matches[1];
		info.matches[1] = NULL;
		info.count -= 1;
		TALLOC_FREE(ctx);
		return info.matches;
	}

	/*
	 * We got more than one possible match, set the result to the maximum
	 * common subset
	 */

	info.matches[0] = SMB_STRNDUP(info.matches[1], info.samelen);
	info.matches[info.count] = NULL;
	return info.matches;

cleanup:
	for (i = 0; i < info.count; i++) {
		SAFE_FREE(info.matches[i]);
	}
	SAFE_FREE(info.matches);
	SAFE_FREE(info.dirmask);
	TALLOC_FREE(ctx);
	return NULL;
}

static char **completion_fn(const char *text, int start, int end)
{
	smb_readline_ca_char(' ');

	if (start) {
		const char *buf, *sp;
		int i;
		char compl_type;

		buf = smb_readline_get_line_buffer();
		if (buf == NULL)
			return NULL;

		sp = strchr(buf, ' ');
		if (sp == NULL)
			return NULL;

		for (i = 0; commands[i].name; i++) {
			if ((strncmp(commands[i].name, buf, sp - buf) == 0) &&
			    (commands[i].name[sp - buf] == 0)) {
				break;
			}
		}
		if (commands[i].name == NULL)
			return NULL;

		while (*sp == ' ')
			sp++;

		if (sp == (buf + start))
			compl_type = commands[i].compl_args[0];
		else
			compl_type = commands[i].compl_args[1];

		if (compl_type == COMPL_REMOTE)
			return remote_completion(text, end - start);
		else /* fall back to local filename completion */
			return NULL;
	} else {
		char **matches;
		int i, len, samelen = 0, count=1;

		matches = SMB_MALLOC_ARRAY(char *, MAX_COMPLETIONS);
		if (!matches) {
			return NULL;
		}
		matches[0] = NULL;

		len = strlen(text);
		for (i=0;commands[i].fn && count < MAX_COMPLETIONS-1;i++) {
			if (strncmp(text, commands[i].name, len) == 0) {
				matches[count] = SMB_STRDUP(commands[i].name);
				if (!matches[count])
					goto cleanup;
				if (count == 1)
					samelen = strlen(matches[count]);
				else
					while (strncmp(matches[count], matches[count-1], samelen) != 0)
						samelen--;
				count++;
			}
		}

		switch (count) {
		case 0:	/* should never happen */
		case 1:
			goto cleanup;
		case 2:
			matches[0] = SMB_STRDUP(matches[1]);
			break;
		default:
			matches[0] = (char *)SMB_MALLOC(samelen+1);
			if (!matches[0])
				goto cleanup;
			strncpy(matches[0], matches[1], samelen);
			matches[0][samelen] = 0;
		}
		matches[count] = NULL;
		return matches;

cleanup:
		for (i = 0; i < count; i++)
			free(matches[i]);

		free(matches);
		return NULL;
	}
}

static bool finished;

/****************************************************************************
 Make sure we swallow keepalives during idle time.
****************************************************************************/

static void readline_callback(void)
{
	static time_t last_t;
	struct timespec now;
	time_t t;
	int ret, revents;

	clock_gettime_mono(&now);
	t = now.tv_sec;

	if (t - last_t < 5)
		return;

	last_t = t;

 again:

	if (cli->fd == -1)
		return;

	/* We deliberately use receive_smb_raw instead of
	   client_receive_smb as we want to receive
	   session keepalives and then drop them here.
	*/

	ret = poll_intr_one_fd(cli->fd, POLLIN|POLLHUP, 0, &revents);

	if ((ret > 0) && (revents & (POLLIN|POLLHUP|POLLERR))) {
		NTSTATUS status;
		size_t len;

		set_smb_read_error(&cli->smb_rw_error, SMB_READ_OK);

		status = receive_smb_raw(cli->fd, cli->inbuf, cli->bufsize, 0, 0, &len);

		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Read from server failed, maybe it closed "
				  "the connection\n"));

			finished = true;
			smb_readline_done();
			if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
				set_smb_read_error(&cli->smb_rw_error,
						   SMB_READ_EOF);
				return;
			}

			if (NT_STATUS_EQUAL(status, NT_STATUS_IO_TIMEOUT)) {
				set_smb_read_error(&cli->smb_rw_error,
						   SMB_READ_TIMEOUT);
				return;
			}

			set_smb_read_error(&cli->smb_rw_error, SMB_READ_ERROR);
			return;
		}
		if(CVAL(cli->inbuf,0) != SMBkeepalive) {
			DEBUG(0, ("Read from server "
				"returned unexpected packet!\n"));
			return;
		}

		goto again;
	}

	/* Ping the server to keep the connection alive using SMBecho. */
	{
		NTSTATUS status;
		unsigned char garbage[16];
		memset(garbage, 0xf0, sizeof(garbage));
		status = cli_echo(cli, 1, data_blob_const(garbage, sizeof(garbage)));

		if (NT_STATUS_IS_OK(status)) {
			return;
		}

		if (!cli_state_is_connected(cli)) {
			DEBUG(0, ("SMBecho failed (%s). The connection is "
				"disconnected now\n", nt_errstr(status)));
			finished = true;
			smb_readline_done();
		}
	}
}

/****************************************************************************
 Process commands on stdin.
****************************************************************************/

static int process_stdin(void)
{
	int rc = 0;

	while (!finished) {
		TALLOC_CTX *frame = talloc_stackframe();
		char *tok = NULL;
		char *the_prompt = NULL;
		char *line = NULL;
		int i;

		/* display a prompt */
		if (asprintf(&the_prompt, "smb: %s> ", client_get_cur_dir()) < 0) {
			TALLOC_FREE(frame);
			break;
		}
		line = smb_readline(the_prompt, readline_callback, completion_fn);
		SAFE_FREE(the_prompt);
		if (!line) {
			TALLOC_FREE(frame);
			break;
		}

		/* special case - first char is ! */
		if (*line == '!') {
			if (system(line + 1) == -1) {
				d_printf("system() command %s failed.\n",
					line+1);
			}
			SAFE_FREE(line);
			TALLOC_FREE(frame);
			continue;
		}

		/* and get the first part of the command */
		cmd_ptr = line;
		if (!next_token_talloc(frame, &cmd_ptr,&tok,NULL)) {
			TALLOC_FREE(frame);
			SAFE_FREE(line);
			continue;
		}

		if ((i = process_tok(tok)) >= 0) {
			rc = commands[i].fn();
		} else if (i == -2) {
			d_printf("%s: command abbreviation ambiguous\n",tok);
		} else {
			d_printf("%s: command not found\n",tok);
		}
		SAFE_FREE(line);
		TALLOC_FREE(frame);
	}
	return rc;
}

/****************************************************************************
 Process commands from the client.
****************************************************************************/

static int process(const char *base_directory)
{
	int rc = 0;

	cli = cli_cm_open(talloc_tos(), NULL,
			have_ip ? dest_ss_str : desthost,
			service, auth_info, true, smb_encrypt,
			max_protocol, port, name_type);
	if (!cli) {
		return 1;
	}

	if (base_directory && *base_directory) {
		rc = do_cd(base_directory);
		if (rc) {
			cli_shutdown(cli);
			return rc;
		}
	}

	if (cmdstr) {
		rc = process_command_string(cmdstr);
	} else {
		process_stdin();
	}

	cli_shutdown(cli);
	return rc;
}

/****************************************************************************
 Handle a -L query.
****************************************************************************/

static int do_host_query(const char *query_host)
{
	cli = cli_cm_open(talloc_tos(), NULL,
			have_ip ? dest_ss_str : query_host, "IPC$", auth_info, true, smb_encrypt,
			max_protocol, port, name_type);
	if (!cli)
		return 1;

	browse_host(true);

	/* Ensure that the host can do IPv4 */

	if (!interpret_addr(query_host)) {
		struct sockaddr_storage ss;
		if (interpret_string_addr(&ss, query_host, 0) &&
				(ss.ss_family != AF_INET)) {
			d_printf("%s is an IPv6 address -- no workgroup available\n",
				query_host);
			return 1;
		}
	}

	if (port != 139) {

		/* Workgroups simply don't make sense over anything
		   else but port 139... */

		cli_shutdown(cli);
		cli = cli_cm_open(talloc_tos(), NULL,
				have_ip ? dest_ss_str : query_host, "IPC$",
				auth_info, true, smb_encrypt,
				max_protocol, 139, name_type);
	}

	if (cli == NULL) {
		d_printf("NetBIOS over TCP disabled -- no workgroup available\n");
		return 1;
	}

	list_servers(lp_workgroup());

	cli_shutdown(cli);

	return(0);
}

/****************************************************************************
 Handle a tar operation.
****************************************************************************/

static int do_tar_op(const char *base_directory)
{
	int ret;

	/* do we already have a connection? */
	if (!cli) {
		cli = cli_cm_open(talloc_tos(), NULL,
			have_ip ? dest_ss_str : desthost,
			service, auth_info, true, smb_encrypt,
			max_protocol, port, name_type);
		if (!cli)
			return 1;
	}

	recurse=true;

	if (base_directory && *base_directory)  {
		ret = do_cd(base_directory);
		if (ret) {
			cli_shutdown(cli);
			return ret;
		}
	}

	ret=process_tar();

	cli_shutdown(cli);

	return(ret);
}

/****************************************************************************
 Handle a message operation.
****************************************************************************/

static int do_message_op(struct user_auth_info *a_info)
{
	struct sockaddr_storage ss;
	struct nmb_name called, calling;
	fstring server_name;
	char name_type_hex[10];
	int msg_port;
	NTSTATUS status;

	make_nmb_name(&calling, calling_name, 0x0);
	make_nmb_name(&called , desthost, name_type);

	fstrcpy(server_name, desthost);
	snprintf(name_type_hex, sizeof(name_type_hex), "#%X", name_type);
	fstrcat(server_name, name_type_hex);

        zero_sockaddr(&ss);
	if (have_ip)
		ss = dest_ss;

	/* we can only do messages over port 139 (to windows clients at least) */

	msg_port = port ? port : 139;

	if (!(cli=cli_initialise())) {
		d_printf("Connection to %s failed\n", desthost);
		return 1;
	}
	cli_set_port(cli, msg_port);

	status = cli_connect(cli, server_name, &ss);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Connection to %s failed. Error %s\n", desthost, nt_errstr(status));
		return 1;
	}

	if (!cli_session_request(cli, &calling, &called)) {
		d_printf("session request failed\n");
		cli_shutdown(cli);
		return 1;
	}

	send_message(get_cmdline_auth_info_username(a_info));
	cli_shutdown(cli);

	return 0;
}

/****************************************************************************
  main program
****************************************************************************/

 int main(int argc,char *argv[])
{
	char *base_directory = NULL;
	int opt;
	char *query_host = NULL;
	bool message = false;
	static const char *new_name_resolve_order = NULL;
	poptContext pc;
	char *p;
	int rc = 0;
	fstring new_workgroup;
	bool tar_opt = false;
	bool service_opt = false;
	struct poptOption long_options[] = {
		POPT_AUTOHELP

		{ "name-resolve", 'R', POPT_ARG_STRING, &new_name_resolve_order, 'R', "Use these name resolution services only", "NAME-RESOLVE-ORDER" },
		{ "message", 'M', POPT_ARG_STRING, NULL, 'M', "Send message", "HOST" },
		{ "ip-address", 'I', POPT_ARG_STRING, NULL, 'I', "Use this IP to connect to", "IP" },
		{ "stderr", 'E', POPT_ARG_NONE, NULL, 'E', "Write messages to stderr instead of stdout" },
		{ "list", 'L', POPT_ARG_STRING, NULL, 'L', "Get a list of shares available on a host", "HOST" },
		{ "max-protocol", 'm', POPT_ARG_STRING, NULL, 'm', "Set the max protocol level", "LEVEL" },
		{ "tar", 'T', POPT_ARG_STRING, NULL, 'T', "Command line tar", "<c|x>IXFqgbNan" },
		{ "directory", 'D', POPT_ARG_STRING, NULL, 'D', "Start from directory", "DIR" },
		{ "command", 'c', POPT_ARG_STRING, &cmdstr, 'c', "Execute semicolon separated commands" }, 
		{ "send-buffer", 'b', POPT_ARG_INT, &io_bufsize, 'b', "Changes the transmit/send buffer", "BYTES" },
		{ "port", 'p', POPT_ARG_INT, &port, 'p', "Port to connect to", "PORT" },
		{ "grepable", 'g', POPT_ARG_NONE, NULL, 'g', "Produce grepable output" },
                { "browse", 'B', POPT_ARG_NONE, NULL, 'B', "Browse SMB servers using DNS" },
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_TABLEEND
	};
	TALLOC_CTX *frame = talloc_stackframe();

	if (!client_set_cur_dir("\\")) {
		exit(ENOMEM);
	}

	/* initialize the workgroup name so we can determine whether or
	   not it was set by a command line option */

	set_global_myworkgroup( "" );
	set_global_myname( "" );

        /* set default debug level to 1 regardless of what smb.conf sets */
	setup_logging( "smbclient", DEBUG_DEFAULT_STDERR );
	load_case_tables();

	lp_set_cmdline("log level", "1");

	auth_info = user_auth_info_init(frame);
	if (auth_info == NULL) {
		exit(1);
	}
	popt_common_set_auth_info(auth_info);

	/* skip argv(0) */
	pc = poptGetContext("smbclient", argc, (const char **) argv, long_options, 0);
	poptSetOtherOptionHelp(pc, "service <password>");

        lp_set_in_client(true); /* Make sure that we tell lp_load we are */

	while ((opt = poptGetNextOpt(pc)) != -1) {

		/* if the tar option has been called previouslt, now we need to eat out the leftovers */
		/* I see no other way to keep things sane --SSS */
		if (tar_opt == true) {
			while (poptPeekArg(pc)) {
				poptGetArg(pc);
			}
			tar_opt = false;
		}

		/* if the service has not yet been specified lets see if it is available in the popt stack */
		if (!service_opt && poptPeekArg(pc)) {
			service = talloc_strdup(frame, poptGetArg(pc));
			if (!service) {
				exit(ENOMEM);
			}
			service_opt = true;
		}

		/* if the service has already been retrieved then check if we have also a password */
		if (service_opt
		    && (!get_cmdline_auth_info_got_pass(auth_info))
		    && poptPeekArg(pc)) {
			set_cmdline_auth_info_password(auth_info,
						       poptGetArg(pc));
		}

		switch (opt) {
		case 'M':
			/* Messages are sent to NetBIOS name type 0x3
			 * (Messenger Service).  Make sure we default
			 * to port 139 instead of port 445. srl,crh
			 */
			name_type = 0x03;
			desthost = talloc_strdup(frame,poptGetOptArg(pc));
			if (!desthost) {
				exit(ENOMEM);
			}
			if( !port )
				port = 139;
 			message = true;
 			break;
		case 'I':
			{
				if (!interpret_string_addr(&dest_ss, poptGetOptArg(pc), 0)) {
					exit(1);
				}
				have_ip = true;
				print_sockaddr(dest_ss_str, sizeof(dest_ss_str), &dest_ss);
			}
			break;
		case 'E':
			setup_logging("smbclient", DEBUG_STDERR );
			display_set_stderr();
			break;

		case 'L':
			query_host = talloc_strdup(frame, poptGetOptArg(pc));
			if (!query_host) {
				exit(ENOMEM);
			}
			break;
		case 'm':
			max_protocol = interpret_protocol(poptGetOptArg(pc), max_protocol);
			break;
		case 'T':
			/* We must use old option processing for this. Find the
			 * position of the -T option in the raw argv[]. */
			{
				int i;
				for (i = 1; i < argc; i++) {
					if (strncmp("-T", argv[i],2)==0)
						break;
				}
				i++;
				if (!tar_parseargs(argc, argv, poptGetOptArg(pc), i)) {
					poptPrintUsage(pc, stderr, 0);
					exit(1);
				}
			}
			/* this must be the last option, mark we have parsed it so that we know we have */
			tar_opt = true;
			break;
		case 'D':
			base_directory = talloc_strdup(frame, poptGetOptArg(pc));
			if (!base_directory) {
				exit(ENOMEM);
			}
			break;
		case 'g':
			grepable=true;
			break;
		case 'e':
			smb_encrypt=true;
			break;
		case 'B':
			return(do_smb_browse());

		}
	}

	/* We may still have some leftovers after the last popt option has been called */
	if (tar_opt == true) {
		while (poptPeekArg(pc)) {
			poptGetArg(pc);
		}
		tar_opt = false;
	}

	/* if the service has not yet been specified lets see if it is available in the popt stack */
	if (!service_opt && poptPeekArg(pc)) {
		service = talloc_strdup(frame,poptGetArg(pc));
		if (!service) {
			exit(ENOMEM);
		}
		service_opt = true;
	}

	/* if the service has already been retrieved then check if we have also a password */
	if (service_opt
	    && !get_cmdline_auth_info_got_pass(auth_info)
	    && poptPeekArg(pc)) {
		set_cmdline_auth_info_password(auth_info,
					       poptGetArg(pc));
	}

	/* save the workgroup...

	   FIXME!! do we need to do this for other options as well
	   (or maybe a generic way to keep lp_load() from overwriting
	   everything)?  */

	fstrcpy( new_workgroup, lp_workgroup() );
	calling_name = talloc_strdup(frame, global_myname() );
	if (!calling_name) {
		exit(ENOMEM);
	}

	if ( override_logfile )
		setup_logging( lp_logfile(), DEBUG_FILE );

	if (!lp_load(get_dyn_CONFIGFILE(),true,false,false,true)) {
		fprintf(stderr, "%s: Can't load %s - run testparm to debug it\n",
			argv[0], get_dyn_CONFIGFILE());
	}

	if (get_cmdline_auth_info_use_machine_account(auth_info) &&
	    !set_cmdline_auth_info_machine_account_creds(auth_info)) {
		exit(-1);
	}

	load_interfaces();

	if (service_opt && service) {
		size_t len;

		/* Convert any '/' characters in the service name to '\' characters */
		string_replace(service, '/','\\');
		if (count_chars(service,'\\') < 3) {
			d_printf("\n%s: Not enough '\\' characters in service\n",service);
			poptPrintUsage(pc, stderr, 0);
			exit(1);
		}
		/* Remove trailing slashes */
		len = strlen(service);
		while(len > 0 && service[len - 1] == '\\') {
			--len;
			service[len] = '\0';
		}
	}

	if ( strlen(new_workgroup) != 0 ) {
		set_global_myworkgroup( new_workgroup );
	}

	if ( strlen(calling_name) != 0 ) {
		set_global_myname( calling_name );
	} else {
		TALLOC_FREE(calling_name);
		calling_name = talloc_strdup(frame, global_myname() );
	}

	smb_encrypt = get_cmdline_auth_info_smb_encrypt(auth_info);
	if (!init_names()) {
		fprintf(stderr, "init_names() failed\n");
		exit(1);
	}

	if(new_name_resolve_order)
		lp_set_name_resolve_order(new_name_resolve_order);

	if (!tar_type && !query_host && !service && !message) {
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}

	poptFreeContext(pc);

	DEBUG(3,("Client started (version %s).\n", samba_version_string()));

	/* Ensure we have a password (or equivalent). */
	set_cmdline_auth_info_getpass(auth_info);

	if (tar_type) {
		if (cmdstr)
			process_command_string(cmdstr);
		return do_tar_op(base_directory);
	}

	if (query_host && *query_host) {
		char *qhost = query_host;
		char *slash;

		while (*qhost == '\\' || *qhost == '/')
			qhost++;

		if ((slash = strchr_m(qhost, '/'))
		    || (slash = strchr_m(qhost, '\\'))) {
			*slash = 0;
		}

		if ((p=strchr_m(qhost, '#'))) {
			*p = 0;
			p++;
			sscanf(p, "%x", &name_type);
		}

		return do_host_query(qhost);
	}

	if (message) {
		return do_message_op(auth_info);
	}

	if (process(base_directory)) {
		return 1;
	}

	TALLOC_FREE(frame);
	return rc;
}
