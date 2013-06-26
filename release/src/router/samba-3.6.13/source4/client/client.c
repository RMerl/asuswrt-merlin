/* 
   Unix SMB/CIFS implementation.
   SMB client
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Simo Sorce 2001-2002
   Copyright (C) Jelmer Vernooij 2003-2004
   Copyright (C) James J Myers   2003 <myersjj@samba.org>
   
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

/* 
 * TODO: remove this ... and don't use talloc_append_string()
 *
 * NOTE: I'm not changing the code yet, because I assume there're
 *       some bugs in the existing code and I'm not sure how to fix
 *	 them correctly.
 */
#define TALLOC_DEPRECATED 1

#include "includes.h"
#include "version.h"
#include "libcli/libcli.h"
#include "lib/events/events.h"
#include "lib/cmdline/popt_common.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"
#include "libcli/util/clilsa.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "../lib/util/dlinklist.h"
#include "system/readline.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "system/time.h" /* needed by some systems for asctime() */
#include "libcli/resolve/resolve.h"
#include "libcli/security/security.h"
#include "../libcli/smbreadline/smbreadline.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "param/param.h"
#include "libcli/raw/raw_proto.h"

/* the default pager to use for the client "more" command. Users can
 *    override this with the PAGER environment variable */
#ifndef DEFAULT_PAGER
#define DEFAULT_PAGER "more"
#endif

struct smbclient_context {
	char *remote_cur_dir;
	struct smbcli_state *cli;
	char *fileselection;
	time_t newer_than;
	bool prompt;
	bool recurse;
	int archive_level;
	bool lowercase;
	int printmode;
	bool translation;
	int io_bufsize;
};

/* timing globals */
static uint64_t get_total_size = 0;
static unsigned int get_total_time_ms = 0;
static uint64_t put_total_size = 0;
static unsigned int put_total_time_ms = 0;

/* Unfortunately, there is no way to pass the a context to the completion function as an argument */
static struct smbclient_context *rl_ctx; 

/* totals globals */
static double dir_total;

/*******************************************************************
 Reduce a file name, removing .. elements.
********************************************************************/
static void dos_clean_name(char *s)
{
	char *p=NULL,*r;

	DEBUG(3,("dos_clean_name [%s]\n",s));

	/* remove any double slashes */
	all_string_sub(s, "\\\\", "\\", 0);

	while ((p = strstr(s,"\\..\\")) != NULL) {
		*p = '\0';
		if ((r = strrchr(s,'\\')) != NULL)
			memmove(r,p+3,strlen(p+3)+1);
	}

	trim_string(s,NULL,"\\..");

	all_string_sub(s, "\\.\\", "\\", 0);
}

/****************************************************************************
write to a local file with CR/LF->LF translation if appropriate. return the 
number taken from the buffer. This may not equal the number written.
****************************************************************************/
static int writefile(int f, const void *_b, int n, bool translation)
{
	const uint8_t *b = (const uint8_t *)_b;
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
  read from a file with LF->CR/LF translation if appropriate. return the 
  number read. read approx n bytes.
****************************************************************************/
static int readfile(void *_b, int n, XFILE *f, bool translation)
{
	uint8_t *b = (uint8_t *)_b;
	int i;
	int c;

	if (!translation)
		return x_fread(b,1,n,f);
  
	i = 0;
	while (i < (n - 1)) {
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
 

/****************************************************************************
send a message
****************************************************************************/
static void send_message(struct smbcli_state *cli, const char *desthost)
{
	char msg[1600];
	int total_len = 0;
	int grp_id;

	if (!smbcli_message_start(cli->tree, desthost, cli_credentials_get_username(cmdline_credentials), &grp_id)) {
		d_printf("message start: %s\n", smbcli_errstr(cli->tree));
		return;
	}


	d_printf("Connected. Type your message, ending it with a Control-D\n");

	while (!feof(stdin) && total_len < 1600) {
		int maxlen = MIN(1600 - total_len,127);
		int l=0;
		int c;

		for (l=0;l<maxlen && (c=fgetc(stdin))!=EOF;l++) {
			if (c == '\n')
				msg[l++] = '\r';
			msg[l] = c;   
		}

		if (!smbcli_message_text(cli->tree, msg, l, grp_id)) {
			d_printf("SMBsendtxt failed (%s)\n",smbcli_errstr(cli->tree));
			return;
		}      
		
		total_len += l;
	}

	if (total_len >= 1600)
		d_printf("the message was truncated to 1600 bytes\n");
	else
		d_printf("sent %d bytes\n",total_len);

	if (!smbcli_message_end(cli->tree, grp_id)) {
		d_printf("SMBsendend failed (%s)\n",smbcli_errstr(cli->tree));
		return;
	}      
}



/****************************************************************************
check the space on a device
****************************************************************************/
static int do_dskattr(struct smbclient_context *ctx)
{
	uint32_t bsize;
	uint64_t total, avail;

	if (NT_STATUS_IS_ERR(smbcli_dskattr(ctx->cli->tree, &bsize, &total, &avail))) {
		d_printf("Error in dskattr: %s\n",smbcli_errstr(ctx->cli->tree)); 
		return 1;
	}

	d_printf("\n\t\t%llu blocks of size %u. %llu blocks available\n",
		 (unsigned long long)total, 
		 (unsigned)bsize, 
		 (unsigned long long)avail);

	return 0;
}

/****************************************************************************
show cd/pwd
****************************************************************************/
static int cmd_pwd(struct smbclient_context *ctx, const char **args)
{
	d_printf("Current directory is %s\n", ctx->remote_cur_dir);
	return 0;
}

/*
  convert a string to dos format
*/
static void dos_format(char *s)
{
	string_replace(s, '/', '\\');
}

/****************************************************************************
change directory - inner section
****************************************************************************/
static int do_cd(struct smbclient_context *ctx, const char *newdir)
{
	char *dname;
      
	/* Save the current directory in case the
	   new directory is invalid */
	if (newdir[0] == '\\')
		dname = talloc_strdup(ctx, newdir);
	else
		dname = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, newdir);

	dos_format(dname);

	if (*(dname+strlen(dname)-1) != '\\') {
		dname = talloc_append_string(NULL, dname, "\\");
	}
	dos_clean_name(dname);
	
	if (NT_STATUS_IS_ERR(smbcli_chkpath(ctx->cli->tree, dname))) {
		d_printf("cd %s: %s\n", dname, smbcli_errstr(ctx->cli->tree));
		talloc_free(dname);
	} else {
		ctx->remote_cur_dir = dname;
	}
	
	return 0;
}

/****************************************************************************
change directory
****************************************************************************/
static int cmd_cd(struct smbclient_context *ctx, const char **args)
{
	int rc = 0;

	if (args[1]) 
		rc = do_cd(ctx, args[1]);
	else
		d_printf("Current directory is %s\n",ctx->remote_cur_dir);

	return rc;
}


static bool mask_match(struct smbcli_state *c, const char *string, 
		const char *pattern, bool is_case_sensitive)
{
	char *p2, *s2;
	bool ret;

	if (ISDOTDOT(string))
		string = ".";
	if (ISDOT(pattern))
		return false;
	
	if (is_case_sensitive)
		return ms_fnmatch(pattern, string, 
				  c->transport->negotiate.protocol) == 0;

	p2 = strlower_talloc(NULL, pattern);
	s2 = strlower_talloc(NULL, string);
	ret = ms_fnmatch(p2, s2, c->transport->negotiate.protocol) == 0;
	talloc_free(p2);
	talloc_free(s2);

	return ret;
}



/*******************************************************************
  decide if a file should be operated on
  ********************************************************************/
static bool do_this_one(struct smbclient_context *ctx, struct clilist_file_info *finfo)
{
	if (finfo->attrib & FILE_ATTRIBUTE_DIRECTORY) return(true);

	if (ctx->fileselection && 
	    !mask_match(ctx->cli, finfo->name,ctx->fileselection,false)) {
		DEBUG(3,("mask_match %s failed\n", finfo->name));
		return false;
	}

	if (ctx->newer_than && finfo->mtime < ctx->newer_than) {
		DEBUG(3,("newer_than %s failed\n", finfo->name));
		return(false);
	}

	if ((ctx->archive_level==1 || ctx->archive_level==2) && !(finfo->attrib & FILE_ATTRIBUTE_ARCHIVE)) {
		DEBUG(3,("archive %s failed\n", finfo->name));
		return(false);
	}
	
	return(true);
}

/****************************************************************************
  display info about a file
  ****************************************************************************/
static void display_finfo(struct smbclient_context *ctx, struct clilist_file_info *finfo)
{
	if (do_this_one(ctx, finfo)) {
		time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */
		char *astr = attrib_string(NULL, finfo->attrib);
		d_printf("  %-30s%7.7s %8.0f  %s",
			 finfo->name,
			 astr,
			 (double)finfo->size,
			 asctime(localtime(&t)));
		dir_total += finfo->size;
		talloc_free(astr);
	}
}


/****************************************************************************
   accumulate size of a file
  ****************************************************************************/
static void do_du(struct smbclient_context *ctx, struct clilist_file_info *finfo)
{
	if (do_this_one(ctx, finfo)) {
		dir_total += finfo->size;
	}
}

static bool do_list_recurse;
static bool do_list_dirs;
static char *do_list_queue = 0;
static long do_list_queue_size = 0;
static long do_list_queue_start = 0;
static long do_list_queue_end = 0;
static void (*do_list_fn)(struct smbclient_context *, struct clilist_file_info *);

/****************************************************************************
functions for do_list_queue
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
	do_list_queue = malloc_array_p(char, do_list_queue_size);
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
	if (do_list_queue == NULL) return;

	/*
	 * If the starting point of the queue is more than half way through,
	 * move everything toward the beginning.
	 */
	if (do_list_queue_start == do_list_queue_end)
	{
		DEBUG(4,("do_list_queue is empty\n"));
		do_list_queue_start = do_list_queue_end = 0;
		*do_list_queue = '\0';
	}
	else if (do_list_queue_start > (do_list_queue_size / 2))
	{
		DEBUG(4,("sliding do_list_queue backward\n"));
		memmove(do_list_queue,
			do_list_queue + do_list_queue_start,
			do_list_queue_end - do_list_queue_start);
		do_list_queue_end -= do_list_queue_start;
		do_list_queue_start = 0;
	}
	   
}

static void add_to_do_list_queue(const char* entry)
{
	char *dlq;
	long new_end = do_list_queue_end + ((long)strlen(entry)) + 1;
	while (new_end > do_list_queue_size)
	{
		do_list_queue_size *= 2;
		DEBUG(4,("enlarging do_list_queue to %d\n",
			 (int)do_list_queue_size));
		dlq = realloc_p(do_list_queue, char, do_list_queue_size);
		if (! dlq) {
			d_printf("failure enlarging do_list_queue to %d bytes\n",
				 (int)do_list_queue_size);
			reset_do_list_queue();
		}
		else
		{
			do_list_queue = dlq;
			memset(do_list_queue + do_list_queue_size / 2,
			       0, do_list_queue_size / 2);
		}
	}
	if (do_list_queue)
	{
		safe_strcpy(do_list_queue + do_list_queue_end, entry, 
			    do_list_queue_size - do_list_queue_end - 1);
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
	if (do_list_queue_end > do_list_queue_start)
	{
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
a helper for do_list
  ****************************************************************************/
static void do_list_helper(struct clilist_file_info *f, const char *mask, void *state)
{
	struct smbclient_context *ctx = (struct smbclient_context *)state;

	if (f->attrib & FILE_ATTRIBUTE_DIRECTORY) {
		if (do_list_dirs && do_this_one(ctx, f)) {
			do_list_fn(ctx, f);
		}
		if (do_list_recurse && 
		    !ISDOT(f->name) &&
		    !ISDOTDOT(f->name)) {
			char *mask2;
			char *p;

			mask2 = talloc_strdup(NULL, mask);
			p = strrchr_m(mask2,'\\');
			if (!p) return;
			p[1] = 0;
			mask2 = talloc_asprintf_append_buffer(mask2, "%s\\*", f->name);
			add_to_do_list_queue(mask2);
		}
		return;
	}

	if (do_this_one(ctx, f)) {
		do_list_fn(ctx, f);
	}
}


/****************************************************************************
a wrapper around smbcli_list that adds recursion
  ****************************************************************************/
static void do_list(struct smbclient_context *ctx, const char *mask,uint16_t attribute,
	     void (*fn)(struct smbclient_context *, struct clilist_file_info *),bool rec, bool dirs)
{
	static int in_do_list = 0;

	if (in_do_list && rec)
	{
		fprintf(stderr, "INTERNAL ERROR: do_list called recursively when the recursive flag is true\n");
		exit(1);
	}

	in_do_list = 1;

	do_list_recurse = rec;
	do_list_dirs = dirs;
	do_list_fn = fn;

	if (rec)
	{
		init_do_list_queue();
		add_to_do_list_queue(mask);
		
		while (! do_list_queue_empty())
		{
			/*
			 * Need to copy head so that it doesn't become
			 * invalid inside the call to smbcli_list.  This
			 * would happen if the list were expanded
			 * during the call.
			 * Fix from E. Jay Berkenbilt (ejb@ql.org)
			 */
			char *head;
			head = do_list_queue_head();
			smbcli_list(ctx->cli->tree, head, attribute, do_list_helper, ctx);
			remove_do_list_queue_head();
			if ((! do_list_queue_empty()) && (fn == display_finfo))
			{
				char* next_file = do_list_queue_head();
				char* save_ch = 0;
				if ((strlen(next_file) >= 2) &&
				    (next_file[strlen(next_file) - 1] == '*') &&
				    (next_file[strlen(next_file) - 2] == '\\'))
				{
					save_ch = next_file +
						strlen(next_file) - 2;
					*save_ch = '\0';
				}
				d_printf("\n%s\n",next_file);
				if (save_ch)
				{
					*save_ch = '\\';
				}
			}
		}
	}
	else
	{
		if (smbcli_list(ctx->cli->tree, mask, attribute, do_list_helper, ctx) == -1)
		{
			d_printf("%s listing %s\n", smbcli_errstr(ctx->cli->tree), mask);
		}
	}

	in_do_list = 0;
	reset_do_list_queue();
}

/****************************************************************************
  get a directory listing
  ****************************************************************************/
static int cmd_dir(struct smbclient_context *ctx, const char **args)
{
	uint16_t attribute = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	char *mask;
	int rc;
	
	dir_total = 0;
	
	mask = talloc_strdup(ctx, ctx->remote_cur_dir);
	if(mask[strlen(mask)-1]!='\\')
		mask = talloc_append_string(ctx, mask,"\\");
	
	if (args[1]) {
		mask = talloc_strdup(ctx, args[1]);
		if (mask[0] != '\\')
			mask = talloc_append_string(ctx, mask, "\\");
		dos_format(mask);
	}
	else {
		if (ctx->cli->tree->session->transport->negotiate.protocol <= 
		    PROTOCOL_LANMAN1) {	
			mask = talloc_append_string(ctx, mask, "*.*");
		} else {
			mask = talloc_append_string(ctx, mask, "*");
		}
	}

	do_list(ctx, mask, attribute, display_finfo, ctx->recurse, true);

	rc = do_dskattr(ctx);

	DEBUG(3, ("Total bytes listed: %.0f\n", dir_total));

	return rc;
}


/****************************************************************************
  get a directory listing
  ****************************************************************************/
static int cmd_du(struct smbclient_context *ctx, const char **args)
{
	uint16_t attribute = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	int rc;
	char *mask;
	
	dir_total = 0;
	
	if (args[1]) {
		if (args[1][0] == '\\')
			mask = talloc_strdup(ctx, args[1]);
		else
			mask = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, args[1]);
		dos_format(mask);
	} else {
		mask = talloc_asprintf(ctx, "%s\\*", ctx->remote_cur_dir);
	}

	do_list(ctx, mask, attribute, do_du, ctx->recurse, true);

	talloc_free(mask);

	rc = do_dskattr(ctx);

	d_printf("Total number of bytes: %.0f\n", dir_total);

	return rc;
}


/****************************************************************************
  get a file from rname to lname
  ****************************************************************************/
static int do_get(struct smbclient_context *ctx, char *rname, const char *p_lname, bool reget)
{  
	int handle = 0, fnum;
	bool newhandle = false;
	uint8_t *data;
	struct timeval tp_start;
	int read_size = ctx->io_bufsize;
	uint16_t attr;
	size_t size;
	off_t start = 0;
	off_t nread = 0;
	int rc = 0;
	char *lname;


	lname = talloc_strdup(ctx, p_lname);
	GetTimeOfDay(&tp_start);

	if (ctx->lowercase) {
		strlower(lname);
	}

	fnum = smbcli_open(ctx->cli->tree, rname, O_RDONLY, DENY_NONE);

	if (fnum == -1) {
		d_printf("%s opening remote file %s\n",smbcli_errstr(ctx->cli->tree),rname);
		return 1;
	}

	if(!strcmp(lname,"-")) {
		handle = fileno(stdout);
	} else {
		if (reget) {
			handle = open(lname, O_WRONLY|O_CREAT, 0644);
			if (handle >= 0) {
				start = lseek(handle, 0, SEEK_END);
				if (start == -1) {
					d_printf("Error seeking local file\n");
					return 1;
				}
			}
		} else {
			handle = open(lname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		}
		newhandle = true;
	}
	if (handle < 0) {
		d_printf("Error opening local file %s\n",lname);
		return 1;
	}


	if (NT_STATUS_IS_ERR(smbcli_qfileinfo(ctx->cli->tree, fnum, 
			   &attr, &size, NULL, NULL, NULL, NULL, NULL)) &&
	    NT_STATUS_IS_ERR(smbcli_getattrE(ctx->cli->tree, fnum, 
			  &attr, &size, NULL, NULL, NULL))) {
		d_printf("getattrib: %s\n",smbcli_errstr(ctx->cli->tree));
		return 1;
	}

	DEBUG(2,("getting file %s of size %.0f as %s ", 
		 rname, (double)size, lname));

	if(!(data = (uint8_t *)malloc(read_size))) { 
		d_printf("malloc fail for size %d\n", read_size);
		smbcli_close(ctx->cli->tree, fnum);
		return 1;
	}

	while (1) {
		int n = smbcli_read(ctx->cli->tree, fnum, data, nread + start, read_size);

		if (n <= 0) break;
 
		if (writefile(handle,data, n, ctx->translation) != n) {
			d_printf("Error writing local file\n");
			rc = 1;
			break;
		}
      
		nread += n;
	}

	if (nread + start < size) {
		DEBUG (0, ("Short read when getting file %s. Only got %ld bytes.\n",
			    rname, (long)nread));

		rc = 1;
	}

	SAFE_FREE(data);
	
	if (NT_STATUS_IS_ERR(smbcli_close(ctx->cli->tree, fnum))) {
		d_printf("Error %s closing remote file\n",smbcli_errstr(ctx->cli->tree));
		rc = 1;
	}

	if (newhandle) {
		close(handle);
	}

	if (ctx->archive_level >= 2 && (attr & FILE_ATTRIBUTE_ARCHIVE)) {
		smbcli_setatr(ctx->cli->tree, rname, attr & ~(uint16_t)FILE_ATTRIBUTE_ARCHIVE, 0);
	}

	{
		struct timeval tp_end;
		int this_time;
		
		GetTimeOfDay(&tp_end);
		this_time = 
			(tp_end.tv_sec - tp_start.tv_sec)*1000 +
			(tp_end.tv_usec - tp_start.tv_usec)/1000;
		get_total_time_ms += this_time;
		get_total_size += nread;
		
		DEBUG(2,("(%3.1f kb/s) (average %3.1f kb/s)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 get_total_size / (1.024*get_total_time_ms)));
	}
	
	return rc;
}


/****************************************************************************
  get a file
  ****************************************************************************/
static int cmd_get(struct smbclient_context *ctx, const char **args)
{
	const char *lname;
	char *rname;

	if (!args[1]) {
		d_printf("get <filename>\n");
		return 1;
	}

	rname = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, args[1]);

	if (args[2]) 
		lname = args[2];
	else 
		lname = args[1];
	
	dos_clean_name(rname);
	
	return do_get(ctx, rname, lname, false);
}

/****************************************************************************
 Put up a yes/no prompt.
****************************************************************************/
static bool yesno(char *p)
{
	char ans[4];
	printf("%s",p);

	if (!fgets(ans,sizeof(ans)-1,stdin))
		return(false);

	if (*ans == 'y' || *ans == 'Y')
		return(true);

	return(false);
}

/****************************************************************************
  do a mget operation on one file
  ****************************************************************************/
static void do_mget(struct smbclient_context *ctx, struct clilist_file_info *finfo)
{
	char *rname;
	char *quest;
	char *mget_mask;
	char *saved_curdir;
	char *l_fname;

	if (ISDOT(finfo->name) || ISDOTDOT(finfo->name))
		return;

	if (finfo->attrib & FILE_ATTRIBUTE_DIRECTORY)
		quest = talloc_asprintf(ctx, "Get directory %s? ",finfo->name);
	else
		quest = talloc_asprintf(ctx, "Get file %s? ",finfo->name);

	if (ctx->prompt && !yesno(quest)) return;

	talloc_free(quest);

	if (!(finfo->attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		rname = talloc_asprintf(ctx, "%s%s",ctx->remote_cur_dir,
					finfo->name);
		do_get(ctx, rname, finfo->name, false);
		talloc_free(rname);
		return;
	}

	/* handle directories */
	saved_curdir = talloc_strdup(ctx, ctx->remote_cur_dir);

	ctx->remote_cur_dir = talloc_asprintf_append_buffer(NULL, "%s\\", finfo->name);

	l_fname = talloc_strdup(ctx, finfo->name);

	string_replace(l_fname, '\\', '/');
	if (ctx->lowercase) {
		strlower(l_fname);
	}
	
	if (!directory_exist(l_fname) &&
	    mkdir(l_fname, 0777) != 0) {
		d_printf("failed to create directory %s\n", l_fname);
		return;
	}
	
	if (chdir(l_fname) != 0) {
		d_printf("failed to chdir to directory %s\n", l_fname);
		return;
	}

	mget_mask = talloc_asprintf(ctx, "%s*", ctx->remote_cur_dir);
	
	do_list(ctx, mget_mask, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY,do_mget,false, true);
	chdir("..");
	talloc_free(ctx->remote_cur_dir);

	ctx->remote_cur_dir = saved_curdir;
}


/****************************************************************************
view the file using the pager
****************************************************************************/
static int cmd_more(struct smbclient_context *ctx, const char **args)
{
	char *rname;
	char *pager_cmd;
	char *lname;
	char *pager;
	int fd;
	int rc = 0;

	lname = talloc_asprintf(ctx, "%s/smbmore.XXXXXX",tmpdir());
	fd = mkstemp(lname);
	if (fd == -1) {
		d_printf("failed to create temporary file for more\n");
		return 1;
	}
	close(fd);

	if (!args[1]) {
		d_printf("more <filename>\n");
		unlink(lname);
		return 1;
	}
	rname = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);
	dos_clean_name(rname);

	rc = do_get(ctx, rname, lname, false);

	pager=getenv("PAGER");

	pager_cmd = talloc_asprintf(ctx, "%s %s",(pager? pager:DEFAULT_PAGER), lname);
	system(pager_cmd);
	unlink(lname);
	
	return rc;
}



/****************************************************************************
do a mget command
****************************************************************************/
static int cmd_mget(struct smbclient_context *ctx, const char **args)
{
	uint16_t attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	char *mget_mask = NULL;
	int i;

	if (ctx->recurse)
		attribute |= FILE_ATTRIBUTE_DIRECTORY;
	
	for (i = 1; args[i]; i++) {
		mget_mask = talloc_strdup(ctx, ctx->remote_cur_dir);
		if(mget_mask[strlen(mget_mask)-1]!='\\')
			mget_mask = talloc_append_string(ctx, mget_mask, "\\");
		
		mget_mask = talloc_strdup(ctx, args[i]);
		if (mget_mask[0] != '\\')
			mget_mask = talloc_append_string(ctx, mget_mask, "\\");
		do_list(ctx, mget_mask, attribute,do_mget,false,true);

		talloc_free(mget_mask);
	}

	if (mget_mask == NULL) {
		mget_mask = talloc_asprintf(ctx, "%s\\*", ctx->remote_cur_dir);
		do_list(ctx, mget_mask, attribute,do_mget,false,true);
		talloc_free(mget_mask);
	}
	
	return 0;
}


/****************************************************************************
make a directory of name "name"
****************************************************************************/
static NTSTATUS do_mkdir(struct smbclient_context *ctx, char *name)
{
	NTSTATUS status;

	if (NT_STATUS_IS_ERR(status = smbcli_mkdir(ctx->cli->tree, name))) {
		d_printf("%s making remote directory %s\n",
			 smbcli_errstr(ctx->cli->tree),name);
		return status;
	}

	return status;
}


/****************************************************************************
 Exit client.
****************************************************************************/
static int cmd_quit(struct smbclient_context *ctx, const char **args)
{
	talloc_free(ctx);
	exit(0);
	/* NOTREACHED */
	return 0;
}


/****************************************************************************
  make a directory
  ****************************************************************************/
static int cmd_mkdir(struct smbclient_context *ctx, const char **args)
{
	char *mask, *p;
  
	if (!args[1]) {
		if (!ctx->recurse)
			d_printf("mkdir <dirname>\n");
		return 1;
	}

	mask = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir,args[1]);

	if (ctx->recurse) {
		dos_clean_name(mask);

		trim_string(mask,".",NULL);
		for (p = strtok(mask,"/\\"); p; p = strtok(p, "/\\")) {
			char *parent = talloc_strndup(ctx, mask, PTR_DIFF(p, mask));
			
			if (NT_STATUS_IS_ERR(smbcli_chkpath(ctx->cli->tree, parent))) { 
				do_mkdir(ctx, parent);
			}

			talloc_free(parent);
		}	 
	} else {
		do_mkdir(ctx, mask);
	}
	
	return 0;
}

/****************************************************************************
show 8.3 name of a file
****************************************************************************/
static int cmd_altname(struct smbclient_context *ctx, const char **args)
{
	const char *altname;
	char *name;
  
	if (!args[1]) {
		d_printf("altname <file>\n");
		return 1;
	}

	name = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);

	if (!NT_STATUS_IS_OK(smbcli_qpathinfo_alt_name(ctx->cli->tree, name, &altname))) {
		d_printf("%s getting alt name for %s\n",
			 smbcli_errstr(ctx->cli->tree),name);
		return(false);
	}
	d_printf("%s\n", altname);

	return 0;
}


/****************************************************************************
  put a single file
  ****************************************************************************/
static int do_put(struct smbclient_context *ctx, char *rname, char *lname, bool reput)
{
	int fnum;
	XFILE *f;
	size_t start = 0;
	off_t nread = 0;
	uint8_t *buf = NULL;
	int maxwrite = ctx->io_bufsize;
	int rc = 0;
	
	struct timeval tp_start;
	GetTimeOfDay(&tp_start);

	if (reput) {
		fnum = smbcli_open(ctx->cli->tree, rname, O_RDWR|O_CREAT, DENY_NONE);
		if (fnum >= 0) {
			if (NT_STATUS_IS_ERR(smbcli_qfileinfo(ctx->cli->tree, fnum, NULL, &start, NULL, NULL, NULL, NULL, NULL)) &&
			    NT_STATUS_IS_ERR(smbcli_getattrE(ctx->cli->tree, fnum, NULL, &start, NULL, NULL, NULL))) {
				d_printf("getattrib: %s\n",smbcli_errstr(ctx->cli->tree));
				return 1;
			}
		}
	} else {
		fnum = smbcli_open(ctx->cli->tree, rname, O_RDWR|O_CREAT|O_TRUNC, 
				DENY_NONE);
	}
  
	if (fnum == -1) {
		d_printf("%s opening remote file %s\n",smbcli_errstr(ctx->cli->tree),rname);
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
  
	buf = (uint8_t *)malloc(maxwrite);
	if (!buf) {
		d_printf("ERROR: Not enough memory!\n");
		return 1;
	}
	while (!x_feof(f)) {
		int n = maxwrite;
		int ret;

		if ((n = readfile(buf,n,f,ctx->translation)) < 1) {
			if((n == 0) && x_feof(f))
				break; /* Empty local file. */

			d_printf("Error reading local file: %s\n", strerror(errno));
			rc = 1;
			break;
		}

		ret = smbcli_write(ctx->cli->tree, fnum, 0, buf, nread + start, n);

		if (n != ret) {
			d_printf("Error writing file: %s\n", smbcli_errstr(ctx->cli->tree));
			rc = 1;
			break;
		} 

		nread += n;
	}

	if (NT_STATUS_IS_ERR(smbcli_close(ctx->cli->tree, fnum))) {
		d_printf("%s closing remote file %s\n",smbcli_errstr(ctx->cli->tree),rname);
		x_fclose(f);
		SAFE_FREE(buf);
		return 1;
	}

	
	if (f != x_stdin) {
		x_fclose(f);
	}

	SAFE_FREE(buf);

	{
		struct timeval tp_end;
		int this_time;
		
		GetTimeOfDay(&tp_end);
		this_time = 
			(tp_end.tv_sec - tp_start.tv_sec)*1000 +
			(tp_end.tv_usec - tp_start.tv_usec)/1000;
		put_total_time_ms += this_time;
		put_total_size += nread;
		
		DEBUG(1,("(%3.1f kb/s) (average %3.1f kb/s)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 put_total_size / (1.024*put_total_time_ms)));
	}

	if (f == x_stdin) {
		talloc_free(ctx);
		exit(0);
	}
	
	return rc;
}

 

/****************************************************************************
  put a file
  ****************************************************************************/
static int cmd_put(struct smbclient_context *ctx, const char **args)
{
	char *lname;
	char *rname;
	
	if (!args[1]) {
		d_printf("put <filename> [<remotename>]\n");
		return 1;
	}

	lname = talloc_strdup(ctx, args[1]);
  
	if (args[2]) {
		if (args[2][0]=='\\')
			rname = talloc_strdup(ctx, args[2]);
		else
			rname = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, args[2]);
	} else {
		rname = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, lname);
	}
	
	dos_clean_name(rname);

	/* allow '-' to represent stdin
	   jdblair, 24.jun.98 */
	if (!file_exist(lname) && (strcmp(lname,"-"))) {
		d_printf("%s does not exist\n",lname);
		return 1;
	}

	return do_put(ctx, rname, lname, false);
}

/*************************************
  File list structure
*************************************/

static struct file_list {
	struct file_list *prev, *next;
	char *file_path;
	bool isdir;
} *file_list;

/****************************************************************************
  Free a file_list structure
****************************************************************************/

static void free_file_list (struct file_list * list)
{
	struct file_list *tmp;
	
	while (list)
	{
		tmp = list;
		DLIST_REMOVE(list, list);
		SAFE_FREE(tmp->file_path);
		SAFE_FREE(tmp);
	}
}

/****************************************************************************
  seek in a directory/file list until you get something that doesn't start with
  the specified name
  ****************************************************************************/
static bool seek_list(struct file_list *list, char *name)
{
	while (list) {
		trim_string(list->file_path,"./","\n");
		if (strncmp(list->file_path, name, strlen(name)) != 0) {
			return(true);
		}
		list = list->next;
	}
      
	return(false);
}

/****************************************************************************
  set the file selection mask
  ****************************************************************************/
static int cmd_select(struct smbclient_context *ctx, const char **args)
{
	talloc_free(ctx->fileselection);
	ctx->fileselection = talloc_strdup(ctx, args[1]);

	return 0;
}

/*******************************************************************
  A readdir wrapper which just returns the file name.
 ********************************************************************/
static const char *readdirname(DIR *p)
{
	struct dirent *ptr;
	char *dname;

	if (!p)
		return(NULL);
  
	ptr = (struct dirent *)readdir(p);
	if (!ptr)
		return(NULL);

	dname = ptr->d_name;

#ifdef NEXT2
	if (telldir(p) < 0)
		return(NULL);
#endif

#ifdef HAVE_BROKEN_READDIR
	/* using /usr/ucb/cc is BAD */
	dname = dname - 2;
#endif

	{
		static char *buf;
		int len = NAMLEN(ptr);
		buf = talloc_strndup(NULL, dname, len);
		dname = buf;
	}

	return(dname);
}

/****************************************************************************
  Recursive file matching function act as find
  match must be always set to true when calling this function
****************************************************************************/
static int file_find(struct smbclient_context *ctx, struct file_list **list, const char *directory, 
		      const char *expression, bool match)
{
	DIR *dir;
	struct file_list *entry;
        struct stat statbuf;
        int ret;
        char *path;
	bool isdir;
	const char *dname;

        dir = opendir(directory);
	if (!dir) return -1;
	
        while ((dname = readdirname(dir))) {
		if (ISDOT(dname) || ISDOTDOT(dname)) {
			continue;
		}
		
		if (asprintf(&path, "%s/%s", directory, dname) <= 0) {
			continue;
		}

		isdir = false;
		if (!match || !gen_fnmatch(expression, dname)) {
			if (ctx->recurse) {
				ret = stat(path, &statbuf);
				if (ret == 0) {
					if (S_ISDIR(statbuf.st_mode)) {
						isdir = true;
						ret = file_find(ctx, list, path, expression, false);
					}
				} else {
					d_printf("file_find: cannot stat file %s\n", path);
				}
				
				if (ret == -1) {
					SAFE_FREE(path);
					closedir(dir);
					return -1;
				}
			}
			entry = malloc_p(struct file_list);
			if (!entry) {
				d_printf("Out of memory in file_find\n");
				closedir(dir);
				return -1;
			}
			entry->file_path = path;
			entry->isdir = isdir;
                        DLIST_ADD(*list, entry);
		} else {
			SAFE_FREE(path);
		}
        }

	closedir(dir);
	return 0;
}

/****************************************************************************
  mput some files
  ****************************************************************************/
static int cmd_mput(struct smbclient_context *ctx, const char **args)
{
	int i;
	
	for (i = 1; args[i]; i++) {
		int ret;
		struct file_list *temp_list;
		char *quest, *lname, *rname;

		printf("%s\n", args[i]);
	
		file_list = NULL;

		ret = file_find(ctx, &file_list, ".", args[i], true);
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
			if (asprintf(&lname, "%s/", temp_list->file_path) <= 0)
				continue;
			trim_string(lname, "./", "/");
			
			/* check if it's a directory */
			if (temp_list->isdir) {
				/* if (!recurse) continue; */
				
				SAFE_FREE(quest);
				if (asprintf(&quest, "Put directory %s? ", lname) < 0) break;
				if (ctx->prompt && !yesno(quest)) { /* No */
					/* Skip the directory */
					lname[strlen(lname)-1] = '/';
					if (!seek_list(temp_list, lname))
						break;		    
				} else { /* Yes */
	      				SAFE_FREE(rname);
					if(asprintf(&rname, "%s%s", ctx->remote_cur_dir, lname) < 0) break;
					dos_format(rname);
					if (NT_STATUS_IS_ERR(smbcli_chkpath(ctx->cli->tree, rname)) && 
					    NT_STATUS_IS_ERR(do_mkdir(ctx, rname))) {
						DEBUG (0, ("Unable to make dir, skipping..."));
						/* Skip the directory */
						lname[strlen(lname)-1] = '/';
						if (!seek_list(temp_list, lname))
							break;
					}
				}
				continue;
			} else {
				SAFE_FREE(quest);
				if (asprintf(&quest,"Put file %s? ", lname) < 0) break;
				if (ctx->prompt && !yesno(quest)) /* No */
					continue;
				
				/* Yes */
				SAFE_FREE(rname);
				if (asprintf(&rname, "%s%s", ctx->remote_cur_dir, lname) < 0) break;
			}

			dos_format(rname);

			do_put(ctx, rname, lname, false);
		}
		free_file_list(file_list);
		SAFE_FREE(quest);
		SAFE_FREE(lname);
		SAFE_FREE(rname);
	}

	return 0;
}


/****************************************************************************
  print a file
  ****************************************************************************/
static int cmd_print(struct smbclient_context *ctx, const char **args)
{
	char *lname, *rname;
	char *p;

	if (!args[1]) {
		d_printf("print <filename>\n");
		return 1;
	}

	lname = talloc_strdup(ctx, args[1]);

	rname = talloc_strdup(ctx, lname);
	p = strrchr_m(rname,'/');
	if (p) {
		slprintf(rname, sizeof(rname)-1, "%s-%d", p+1, (int)getpid());
	}

	if (strequal(lname,"-")) {
		slprintf(rname, sizeof(rname)-1, "stdin-%d", (int)getpid());
	}

	return do_put(ctx, rname, lname, false);
}


static int cmd_rewrite(struct smbclient_context *ctx, const char **args)
{
	d_printf("REWRITE: command not implemented (FIXME!)\n");
	
	return 0;
}

/****************************************************************************
delete some files
****************************************************************************/
static int cmd_del(struct smbclient_context *ctx, const char **args)
{
	char *mask;
	uint16_t attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;

	if (ctx->recurse)
		attribute |= FILE_ATTRIBUTE_DIRECTORY;
	
	if (!args[1]) {
		d_printf("del <filename>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);

	if (NT_STATUS_IS_ERR(smbcli_unlink(ctx->cli->tree, mask))) {
		d_printf("%s deleting remote file %s\n",smbcli_errstr(ctx->cli->tree),mask);
	}
	
	return 0;
}


/****************************************************************************
delete a whole directory tree
****************************************************************************/
static int cmd_deltree(struct smbclient_context *ctx, const char **args)
{
	char *dname;
	int ret;

	if (!args[1]) {
		d_printf("deltree <dirname>\n");
		return 1;
	}

	dname = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);
	
	ret = smbcli_deltree(ctx->cli->tree, dname);

	if (ret == -1) {
		printf("Failed to delete tree %s - %s\n", dname, smbcli_errstr(ctx->cli->tree));
		return -1;
	}

	printf("Deleted %d files in %s\n", ret, dname);
	
	return 0;
}

typedef struct {
	const char  *level_name;
	enum smb_fsinfo_level level;
} fsinfo_level_t;

fsinfo_level_t fsinfo_levels[] = {
	{"dskattr", RAW_QFS_DSKATTR},
	{"allocation", RAW_QFS_ALLOCATION},
	{"volume", RAW_QFS_VOLUME},
	{"volumeinfo", RAW_QFS_VOLUME_INFO},
	{"sizeinfo", RAW_QFS_SIZE_INFO},
	{"deviceinfo", RAW_QFS_DEVICE_INFO},
	{"attributeinfo", RAW_QFS_ATTRIBUTE_INFO},
	{"unixinfo", RAW_QFS_UNIX_INFO},
	{"volume-information", RAW_QFS_VOLUME_INFORMATION},
	{"size-information", RAW_QFS_SIZE_INFORMATION},
	{"device-information", RAW_QFS_DEVICE_INFORMATION},
	{"attribute-information", RAW_QFS_ATTRIBUTE_INFORMATION},
	{"quota-information", RAW_QFS_QUOTA_INFORMATION},
	{"fullsize-information", RAW_QFS_FULL_SIZE_INFORMATION},
	{"objectid", RAW_QFS_OBJECTID_INFORMATION},
	{NULL, RAW_QFS_GENERIC}
};


static int cmd_fsinfo(struct smbclient_context *ctx, const char **args)
{
	union smb_fsinfo fsinfo;
	NTSTATUS status;
	fsinfo_level_t *fsinfo_level;
	
	if (!args[1]) {
		d_printf("fsinfo <level>, where level is one of following:\n");
		fsinfo_level = fsinfo_levels;
		while(fsinfo_level->level_name) {
			d_printf("%s\n", fsinfo_level->level_name);
			fsinfo_level++;
		}
		return 1;
	}
	
	fsinfo_level = fsinfo_levels;
	while(fsinfo_level->level_name && !strequal(args[1],fsinfo_level->level_name)) {
		fsinfo_level++;
	}
  
	if (!fsinfo_level->level_name) {
		d_printf("wrong level name!\n");
		return 1;
	}
  
	fsinfo.generic.level = fsinfo_level->level;
	status = smb_raw_fsinfo(ctx->cli->tree, ctx, &fsinfo);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("fsinfo-level-%s - %s\n", fsinfo_level->level_name, nt_errstr(status));
		return 1;
	}

	d_printf("fsinfo-level-%s:\n", fsinfo_level->level_name);
	switch(fsinfo.generic.level) {
	case RAW_QFS_DSKATTR:
		d_printf("\tunits_total:                %hu\n", 
			 (unsigned short) fsinfo.dskattr.out.units_total);
		d_printf("\tblocks_per_unit:            %hu\n", 
			 (unsigned short) fsinfo.dskattr.out.blocks_per_unit);
		d_printf("\tblocks_size:                %hu\n", 
			 (unsigned short) fsinfo.dskattr.out.block_size);
		d_printf("\tunits_free:                 %hu\n", 
			 (unsigned short) fsinfo.dskattr.out.units_free);
		break;
	case RAW_QFS_ALLOCATION:
		d_printf("\tfs_id:                      %lu\n", 
			 (unsigned long) fsinfo.allocation.out.fs_id);
		d_printf("\tsectors_per_unit:           %lu\n", 
			 (unsigned long) fsinfo.allocation.out.sectors_per_unit);
		d_printf("\ttotal_alloc_units:          %lu\n", 
			 (unsigned long) fsinfo.allocation.out.total_alloc_units);
		d_printf("\tavail_alloc_units:          %lu\n", 
			 (unsigned long) fsinfo.allocation.out.avail_alloc_units);
		d_printf("\tbytes_per_sector:           %hu\n", 
			 (unsigned short) fsinfo.allocation.out.bytes_per_sector);
		break;
	case RAW_QFS_VOLUME:
		d_printf("\tserial_number:              %lu\n", 
			 (unsigned long) fsinfo.volume.out.serial_number);
		d_printf("\tvolume_name:                %s\n", fsinfo.volume.out.volume_name.s);
		break;
	case RAW_QFS_VOLUME_INFO:
	case RAW_QFS_VOLUME_INFORMATION:
		d_printf("\tcreate_time:                %s\n",
			 nt_time_string(ctx,fsinfo.volume_info.out.create_time));
		d_printf("\tserial_number:              %lu\n", 
			 (unsigned long) fsinfo.volume_info.out.serial_number);
		d_printf("\tvolume_name:                %s\n", fsinfo.volume_info.out.volume_name.s);
		break;
	case RAW_QFS_SIZE_INFO:
	case RAW_QFS_SIZE_INFORMATION:
		d_printf("\ttotal_alloc_units:          %llu\n", 
			 (unsigned long long) fsinfo.size_info.out.total_alloc_units);
		d_printf("\tavail_alloc_units:          %llu\n", 
			 (unsigned long long) fsinfo.size_info.out.avail_alloc_units);
		d_printf("\tsectors_per_unit:           %lu\n", 
			 (unsigned long) fsinfo.size_info.out.sectors_per_unit);
		d_printf("\tbytes_per_sector:           %lu\n", 
			 (unsigned long) fsinfo.size_info.out.bytes_per_sector);
		break;
	case RAW_QFS_DEVICE_INFO:
	case RAW_QFS_DEVICE_INFORMATION:
		d_printf("\tdevice_type:                %lu\n", 
			 (unsigned long) fsinfo.device_info.out.device_type);
		d_printf("\tcharacteristics:            0x%lx\n", 
			 (unsigned long) fsinfo.device_info.out.characteristics);
		break;
	case RAW_QFS_ATTRIBUTE_INFORMATION:
	case RAW_QFS_ATTRIBUTE_INFO:
		d_printf("\tfs_attr:                    0x%lx\n", 
			 (unsigned long) fsinfo.attribute_info.out.fs_attr);
		d_printf("\tmax_file_component_length:  %lu\n", 
			 (unsigned long) fsinfo.attribute_info.out.max_file_component_length);
		d_printf("\tfs_type:                    %s\n", fsinfo.attribute_info.out.fs_type.s);
		break;
	case RAW_QFS_UNIX_INFO:
		d_printf("\tmajor_version:              %hu\n", 
			 (unsigned short) fsinfo.unix_info.out.major_version);
		d_printf("\tminor_version:              %hu\n", 
			 (unsigned short) fsinfo.unix_info.out.minor_version);
		d_printf("\tcapability:                 0x%llx\n", 
			 (unsigned long long) fsinfo.unix_info.out.capability);
		break;
	case RAW_QFS_QUOTA_INFORMATION:
		d_printf("\tunknown[3]:                 [%llu,%llu,%llu]\n", 
			 (unsigned long long) fsinfo.quota_information.out.unknown[0],
			 (unsigned long long) fsinfo.quota_information.out.unknown[1],
			 (unsigned long long) fsinfo.quota_information.out.unknown[2]);
		d_printf("\tquota_soft:                 %llu\n", 
			 (unsigned long long) fsinfo.quota_information.out.quota_soft);
		d_printf("\tquota_hard:                 %llu\n", 
			 (unsigned long long) fsinfo.quota_information.out.quota_hard);
		d_printf("\tquota_flags:                0x%llx\n", 
			 (unsigned long long) fsinfo.quota_information.out.quota_flags);
		break;
	case RAW_QFS_FULL_SIZE_INFORMATION:
		d_printf("\ttotal_alloc_units:          %llu\n", 
			 (unsigned long long) fsinfo.full_size_information.out.total_alloc_units);
		d_printf("\tcall_avail_alloc_units:     %llu\n", 
			 (unsigned long long) fsinfo.full_size_information.out.call_avail_alloc_units);
		d_printf("\tactual_avail_alloc_units:   %llu\n", 
			 (unsigned long long) fsinfo.full_size_information.out.actual_avail_alloc_units);
		d_printf("\tsectors_per_unit:           %lu\n", 
			 (unsigned long) fsinfo.full_size_information.out.sectors_per_unit);
		d_printf("\tbytes_per_sector:           %lu\n", 
			 (unsigned long) fsinfo.full_size_information.out.bytes_per_sector);
		break;
	case RAW_QFS_OBJECTID_INFORMATION:
		d_printf("\tGUID:                       %s\n", 
			 GUID_string(ctx,&fsinfo.objectid_information.out.guid));
		d_printf("\tunknown[6]:                 [%llu,%llu,%llu,%llu,%llu,%llu]\n", 
			 (unsigned long long) fsinfo.objectid_information.out.unknown[0],
			 (unsigned long long) fsinfo.objectid_information.out.unknown[1],
			 (unsigned long long) fsinfo.objectid_information.out.unknown[2],
			 (unsigned long long) fsinfo.objectid_information.out.unknown[3],
			 (unsigned long long) fsinfo.objectid_information.out.unknown[4],
			 (unsigned long long) fsinfo.objectid_information.out.unknown[5] );
		break;
	case RAW_QFS_GENERIC:
		d_printf("\twrong level returned\n");
		break;
	}
  
	return 0;
}

/****************************************************************************
show as much information as possible about a file
****************************************************************************/
static int cmd_allinfo(struct smbclient_context *ctx, const char **args)
{
	char *fname;
	union smb_fileinfo finfo;
	NTSTATUS status;
	int fnum;

	if (!args[1]) {
		d_printf("allinfo <filename>\n");
		return 1;
	}
	fname = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);

	/* first a ALL_INFO QPATHINFO */
	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s - %s\n", fname, nt_errstr(status));
		return 1;
	}

	d_printf("\tcreate_time:    %s\n", nt_time_string(ctx, finfo.all_info.out.create_time));
	d_printf("\taccess_time:    %s\n", nt_time_string(ctx, finfo.all_info.out.access_time));
	d_printf("\twrite_time:     %s\n", nt_time_string(ctx, finfo.all_info.out.write_time));
	d_printf("\tchange_time:    %s\n", nt_time_string(ctx, finfo.all_info.out.change_time));
	d_printf("\tattrib:         0x%x\n", finfo.all_info.out.attrib);
	d_printf("\talloc_size:     %lu\n", (unsigned long)finfo.all_info.out.alloc_size);
	d_printf("\tsize:           %lu\n", (unsigned long)finfo.all_info.out.size);
	d_printf("\tnlink:          %u\n", finfo.all_info.out.nlink);
	d_printf("\tdelete_pending: %u\n", finfo.all_info.out.delete_pending);
	d_printf("\tdirectory:      %u\n", finfo.all_info.out.directory);
	d_printf("\tea_size:        %u\n", finfo.all_info.out.ea_size);
	d_printf("\tfname:          '%s'\n", finfo.all_info.out.fname.s);

	/* 8.3 name if any */
	finfo.generic.level = RAW_FILEINFO_ALT_NAME_INFO;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\talt_name:       %s\n", finfo.alt_name_info.out.fname.s);
	}

	/* file_id if available */
	finfo.generic.level = RAW_FILEINFO_INTERNAL_INFORMATION;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\tfile_id         %.0f\n", 
			 (double)finfo.internal_information.out.file_id);
	}

	/* the EAs, if any */
	finfo.generic.level = RAW_FILEINFO_ALL_EAS;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		for (i=0;i<finfo.all_eas.out.num_eas;i++) {
			d_printf("\tEA[%d] flags=%d len=%d '%s'\n", i,
				 finfo.all_eas.out.eas[i].flags,
				 (int)finfo.all_eas.out.eas[i].value.length,
				 finfo.all_eas.out.eas[i].name.s);
		}
	}

	/* streams, if available */
	finfo.generic.level = RAW_FILEINFO_STREAM_INFO;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		for (i=0;i<finfo.stream_info.out.num_streams;i++) {
			d_printf("\tstream %d:\n", i);
			d_printf("\t\tsize       %ld\n", 
				 (long)finfo.stream_info.out.streams[i].size);
			d_printf("\t\talloc size %ld\n", 
				 (long)finfo.stream_info.out.streams[i].alloc_size);
			d_printf("\t\tname       %s\n", finfo.stream_info.out.streams[i].stream_name.s);
		}
	}	

	/* dev/inode if available */
	finfo.generic.level = RAW_FILEINFO_COMPRESSION_INFORMATION;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\tcompressed size %ld\n", (long)finfo.compression_info.out.compressed_size);
		d_printf("\tformat          %ld\n", (long)finfo.compression_info.out.format);
		d_printf("\tunit_shift      %ld\n", (long)finfo.compression_info.out.unit_shift);
		d_printf("\tchunk_shift     %ld\n", (long)finfo.compression_info.out.chunk_shift);
		d_printf("\tcluster_shift   %ld\n", (long)finfo.compression_info.out.cluster_shift);
	}

	/* shadow copies if available */
	fnum = smbcli_open(ctx->cli->tree, fname, O_RDONLY, DENY_NONE);
	if (fnum != -1) {
		struct smb_shadow_copy info;
		int i;
		info.in.file.fnum = fnum;
		info.in.max_data = ~0;
		status = smb_raw_shadow_data(ctx->cli->tree, ctx, &info);
		if (NT_STATUS_IS_OK(status)) {
			d_printf("\tshadow_copy: %u volumes  %u names\n",
				 info.out.num_volumes, info.out.num_names);
			for (i=0;i<info.out.num_names;i++) {
				d_printf("\t%s\n", info.out.names[i]);
				finfo.generic.level = RAW_FILEINFO_ALL_INFO;
				finfo.generic.in.file.path = talloc_asprintf(ctx, "%s%s", 
									     info.out.names[i], fname); 
				status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
				if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_PATH_NOT_FOUND) ||
				    NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
					continue;
				}
				if (!NT_STATUS_IS_OK(status)) {
					d_printf("%s - %s\n", finfo.generic.in.file.path, 
						 nt_errstr(status));
					return 1;
				}
				
				d_printf("\t\tcreate_time:    %s\n", nt_time_string(ctx, finfo.all_info.out.create_time));
				d_printf("\t\twrite_time:     %s\n", nt_time_string(ctx, finfo.all_info.out.write_time));
				d_printf("\t\tchange_time:    %s\n", nt_time_string(ctx, finfo.all_info.out.change_time));
				d_printf("\t\tsize:           %lu\n", (unsigned long)finfo.all_info.out.size);
			}
		}
	}
	
	return 0;
}


/****************************************************************************
shows EA contents
****************************************************************************/
static int cmd_eainfo(struct smbclient_context *ctx, const char **args)
{
	char *fname;
	union smb_fileinfo finfo;
	NTSTATUS status;
	int i;

	if (!args[1]) {
		d_printf("eainfo <filename>\n");
		return 1;
	}
	fname = talloc_strdup(ctx, args[1]);

	finfo.generic.level = RAW_FILEINFO_ALL_EAS;
	finfo.generic.in.file.path = fname;
	status = smb_raw_pathinfo(ctx->cli->tree, ctx, &finfo);
	
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("RAW_FILEINFO_ALL_EAS - %s\n", nt_errstr(status));
		return 1;
	}

	d_printf("%s has %d EAs\n", fname, finfo.all_eas.out.num_eas);

	for (i=0;i<finfo.all_eas.out.num_eas;i++) {
		d_printf("\tEA[%d] flags=%d len=%d '%s'\n", i,
			 finfo.all_eas.out.eas[i].flags,
			 (int)finfo.all_eas.out.eas[i].value.length,
			 finfo.all_eas.out.eas[i].name.s);
		fflush(stdout);
		dump_data(0, 
			  finfo.all_eas.out.eas[i].value.data,
			  finfo.all_eas.out.eas[i].value.length);
	}

	return 0;
}


/****************************************************************************
show any ACL on a file
****************************************************************************/
static int cmd_acl(struct smbclient_context *ctx, const char **args)
{
	char *fname;
	union smb_fileinfo query;
	NTSTATUS status;
	int fnum;

	if (!args[1]) {
		d_printf("acl <filename>\n");
		return 1;
	}
	fname = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);

	fnum = smbcli_nt_create_full(ctx->cli->tree, fname, 0, 
				     SEC_STD_READ_CONTROL,
				     0,
				     NTCREATEX_SHARE_ACCESS_DELETE|
				     NTCREATEX_SHARE_ACCESS_READ|
				     NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OPEN,
				     0, 0);
	if (fnum == -1) {
		d_printf("%s - %s\n", fname, smbcli_errstr(ctx->cli->tree));
		return -1;
	}

	query.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	query.query_secdesc.in.file.fnum = fnum;
	query.query_secdesc.in.secinfo_flags = 0x7;

	status = smb_raw_fileinfo(ctx->cli->tree, ctx, &query);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s - %s\n", fname, nt_errstr(status));
		return 1;
	}

	NDR_PRINT_DEBUG(security_descriptor, query.query_secdesc.out.sd);

	return 0;
}

/****************************************************************************
lookup a name or sid
****************************************************************************/
static int cmd_lookup(struct smbclient_context *ctx, const char **args)
{
	NTSTATUS status;
	struct dom_sid *sid;

	if (!args[1]) {
		d_printf("lookup <sid|name>\n");
		return 1;
	}

	sid = dom_sid_parse_talloc(ctx, args[1]);
	if (sid == NULL) {
		const char *sidstr;
		status = smblsa_lookup_name(ctx->cli, args[1], ctx, &sidstr);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			return 1;
		}

		d_printf("%s\n", sidstr);
	} else {
		const char *name;
		status = smblsa_lookup_sid(ctx->cli, args[1], ctx, &name);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupSids - %s\n", nt_errstr(status));
			return 1;
		}

		d_printf("%s\n", name);
	}

	return 0;
}

/****************************************************************************
show privileges for a user
****************************************************************************/
static int cmd_privileges(struct smbclient_context *ctx, const char **args)
{
	NTSTATUS status;
	struct dom_sid *sid;
	struct lsa_RightSet rights;
	unsigned i;

	if (!args[1]) {
		d_printf("privileges <sid|name>\n");
		return 1;
	}

	sid = dom_sid_parse_talloc(ctx, args[1]);
	if (sid == NULL) {
		const char *sid_str;
		status = smblsa_lookup_name(ctx->cli, args[1], ctx, &sid_str);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			return 1;
		}
		sid = dom_sid_parse_talloc(ctx, sid_str);
	}

	status = smblsa_sid_privileges(ctx->cli, sid, ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("lsa_EnumAccountRights - %s\n", nt_errstr(status));
		return 1;
	}

	for (i=0;i<rights.count;i++) {
		d_printf("\t%s\n", rights.names[i].string);
	}

	return 0;
}


/****************************************************************************
add privileges for a user
****************************************************************************/
static int cmd_addprivileges(struct smbclient_context *ctx, const char **args)
{
	NTSTATUS status;
	struct dom_sid *sid;
	struct lsa_RightSet rights;
	int i;

	if (!args[1]) {
		d_printf("addprivileges <sid|name> <privilege...>\n");
		return 1;
	}

	sid = dom_sid_parse_talloc(ctx, args[1]);
	if (sid == NULL) {
		const char *sid_str;
		status = smblsa_lookup_name(ctx->cli, args[1], ctx, &sid_str);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			return 1;
		}
		sid = dom_sid_parse_talloc(ctx, sid_str);
	}

	ZERO_STRUCT(rights);
	for (i = 2; args[i]; i++) {
		rights.names = talloc_realloc(ctx, rights.names, 
					      struct lsa_StringLarge, rights.count+1);
		rights.names[rights.count].string = talloc_strdup(ctx, args[i]);
		rights.count++;
	}


	status = smblsa_sid_add_privileges(ctx->cli, sid, ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("lsa_AddAccountRights - %s\n", nt_errstr(status));
		return 1;
	}

	return 0;
}

/****************************************************************************
delete privileges for a user
****************************************************************************/
static int cmd_delprivileges(struct smbclient_context *ctx, const char **args)
{
	NTSTATUS status;
	struct dom_sid *sid;
	struct lsa_RightSet rights;
	int i;

	if (!args[1]) {
		d_printf("delprivileges <sid|name> <privilege...>\n");
		return 1;
	}

	sid = dom_sid_parse_talloc(ctx, args[1]);
	if (sid == NULL) {
		const char *sid_str;
		status = smblsa_lookup_name(ctx->cli, args[1], ctx, &sid_str);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			return 1;
		}
		sid = dom_sid_parse_talloc(ctx, sid_str);
	}

	ZERO_STRUCT(rights);
	for (i = 2; args[i]; i++) {
		rights.names = talloc_realloc(ctx, rights.names, 
					      struct lsa_StringLarge, rights.count+1);
		rights.names[rights.count].string = talloc_strdup(ctx, args[i]);
		rights.count++;
	}


	status = smblsa_sid_del_privileges(ctx->cli, sid, ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("lsa_RemoveAccountRights - %s\n", nt_errstr(status));
		return 1;
	}

	return 0;
}


/****************************************************************************
****************************************************************************/
static int cmd_open(struct smbclient_context *ctx, const char **args)
{
	char *mask;
	
	if (!args[1]) {
		d_printf("open <filename>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);

	smbcli_open(ctx->cli->tree, mask, O_RDWR, DENY_ALL);

	return 0;
}


/****************************************************************************
remove a directory
****************************************************************************/
static int cmd_rmdir(struct smbclient_context *ctx, const char **args)
{
	char *mask;
  
	if (!args[1]) {
		d_printf("rmdir <dirname>\n");
		return 1;
	}
	mask = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);

	if (NT_STATUS_IS_ERR(smbcli_rmdir(ctx->cli->tree, mask))) {
		d_printf("%s removing remote directory file %s\n",
			 smbcli_errstr(ctx->cli->tree),mask);
	}
	
	return 0;
}

/****************************************************************************
 UNIX hardlink.
****************************************************************************/
static int cmd_link(struct smbclient_context *ctx, const char **args)
{
	char *src,*dest;
  
	if (!(ctx->cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	
	if (!args[1] || !args[2]) {
		d_printf("link <src> <dest>\n");
		return 1;
	}

	src = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);
	dest = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[2]);

	if (NT_STATUS_IS_ERR(smbcli_unix_hardlink(ctx->cli->tree, src, dest))) {
		d_printf("%s linking files (%s -> %s)\n", smbcli_errstr(ctx->cli->tree), src, dest);
		return 1;
	}  

	return 0;
}

/****************************************************************************
 UNIX symlink.
****************************************************************************/

static int cmd_symlink(struct smbclient_context *ctx, const char **args)
{
	char *src,*dest;
  
	if (!(ctx->cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!args[1] || !args[2]) {
		d_printf("symlink <src> <dest>\n");
		return 1;
	}

	src = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);
	dest = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[2]);

	if (NT_STATUS_IS_ERR(smbcli_unix_symlink(ctx->cli->tree, src, dest))) {
		d_printf("%s symlinking files (%s -> %s)\n",
			smbcli_errstr(ctx->cli->tree), src, dest);
		return 1;
	} 

	return 0;
}

/****************************************************************************
 UNIX chmod.
****************************************************************************/

static int cmd_chmod(struct smbclient_context *ctx, const char **args)
{
	char *src;
	mode_t mode;
  
	if (!(ctx->cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!args[1] || !args[2]) {
		d_printf("chmod mode file\n");
		return 1;
	}

	src = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[2]);
	
	mode = (mode_t)strtol(args[1], NULL, 8);

	if (NT_STATUS_IS_ERR(smbcli_unix_chmod(ctx->cli->tree, src, mode))) {
		d_printf("%s chmod file %s 0%o\n",
			smbcli_errstr(ctx->cli->tree), src, (unsigned)mode);
		return 1;
	} 

	return 0;
}

/****************************************************************************
 UNIX chown.
****************************************************************************/

static int cmd_chown(struct smbclient_context *ctx, const char **args)
{
	char *src;
	uid_t uid;
	gid_t gid;
  
	if (!(ctx->cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	if (!args[1] || !args[2] || !args[3]) {
		d_printf("chown uid gid file\n");
		return 1;
	}

	uid = (uid_t)atoi(args[1]);
	gid = (gid_t)atoi(args[2]);
	src = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[3]);

	if (NT_STATUS_IS_ERR(smbcli_unix_chown(ctx->cli->tree, src, uid, gid))) {
		d_printf("%s chown file %s uid=%d, gid=%d\n",
			smbcli_errstr(ctx->cli->tree), src, (int)uid, (int)gid);
		return 1;
	} 

	return 0;
}

/****************************************************************************
rename some files
****************************************************************************/
static int cmd_rename(struct smbclient_context *ctx, const char **args)
{
	char *src,*dest;
  
	if (!args[1] || !args[2]) {
		d_printf("rename <src> <dest>\n");
		return 1;
	}

	src = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[1]);
	dest = talloc_asprintf(ctx, "%s%s", ctx->remote_cur_dir, args[2]);

	if (NT_STATUS_IS_ERR(smbcli_rename(ctx->cli->tree, src, dest))) {
		d_printf("%s renaming files\n",smbcli_errstr(ctx->cli->tree));
		return 1;
	}
	
	return 0;
}


/****************************************************************************
toggle the prompt flag
****************************************************************************/
static int cmd_prompt(struct smbclient_context *ctx, const char **args)
{
	ctx->prompt = !ctx->prompt;
	DEBUG(2,("prompting is now %s\n",ctx->prompt?"on":"off"));
	
	return 1;
}


/****************************************************************************
set the newer than time
****************************************************************************/
static int cmd_newer(struct smbclient_context *ctx, const char **args)
{
	struct stat sbuf;

	if (args[1] && (stat(args[1],&sbuf) == 0)) {
		ctx->newer_than = sbuf.st_mtime;
		DEBUG(1,("Getting files newer than %s",
			 asctime(localtime(&ctx->newer_than))));
	} else {
		ctx->newer_than = 0;
	}

	if (args[1] && ctx->newer_than == 0) {
		d_printf("Error setting newer-than time\n");
		return 1;
	}

	return 0;
}

/****************************************************************************
set the archive level
****************************************************************************/
static int cmd_archive(struct smbclient_context *ctx, const char **args)
{
	if (args[1]) {
		ctx->archive_level = atoi(args[1]);
	} else
		d_printf("Archive level is %d\n",ctx->archive_level);

	return 0;
}

/****************************************************************************
toggle the lowercaseflag
****************************************************************************/
static int cmd_lowercase(struct smbclient_context *ctx, const char **args)
{
	ctx->lowercase = !ctx->lowercase;
	DEBUG(2,("filename lowercasing is now %s\n",ctx->lowercase?"on":"off"));

	return 0;
}




/****************************************************************************
toggle the recurse flag
****************************************************************************/
static int cmd_recurse(struct smbclient_context *ctx, const char **args)
{
	ctx->recurse = !ctx->recurse;
	DEBUG(2,("directory recursion is now %s\n",ctx->recurse?"on":"off"));

	return 0;
}

/****************************************************************************
toggle the translate flag
****************************************************************************/
static int cmd_translate(struct smbclient_context *ctx, const char **args)
{
	ctx->translation = !ctx->translation;
	DEBUG(2,("CR/LF<->LF and print text translation now %s\n",
		 ctx->translation?"on":"off"));

	return 0;
}


/****************************************************************************
do a printmode command
****************************************************************************/
static int cmd_printmode(struct smbclient_context *ctx, const char **args)
{
	if (args[1]) {
		if (strequal(args[1],"text")) {
			ctx->printmode = 0;      
		} else {
			if (strequal(args[1],"graphics"))
				ctx->printmode = 1;
			else
				ctx->printmode = atoi(args[1]);
		}
	}

	switch(ctx->printmode)
	{
		case 0: 
			DEBUG(2,("the printmode is now text\n"));
			break;
		case 1: 
			DEBUG(2,("the printmode is now graphics\n"));
			break;
		default: 
			DEBUG(2,("the printmode is now %d\n", ctx->printmode));
			break;
	}
	
	return 0;
}

/****************************************************************************
 do the lcd command
 ****************************************************************************/
static int cmd_lcd(struct smbclient_context *ctx, const char **args)
{
	char d[PATH_MAX];
	
	if (args[1]) 
		chdir(args[1]);
	DEBUG(2,("the local directory is now %s\n",getcwd(d, PATH_MAX)));

	return 0;
}

/****************************************************************************
history
****************************************************************************/
static int cmd_history(struct smbclient_context *ctx, const char **args)
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

/****************************************************************************
 get a file restarting at end of local file
 ****************************************************************************/
static int cmd_reget(struct smbclient_context *ctx, const char **args)
{
	char *local_name;
	char *remote_name;

	if (!args[1]) {
		d_printf("reget <filename>\n");
		return 1;
	}
	remote_name = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, args[1]);
	dos_clean_name(remote_name);
	
	if (args[2]) 
		local_name = talloc_strdup(ctx, args[2]);
	else
		local_name = talloc_strdup(ctx, args[1]);
	
	return do_get(ctx, remote_name, local_name, true);
}

/****************************************************************************
 put a file restarting at end of local file
 ****************************************************************************/
static int cmd_reput(struct smbclient_context *ctx, const char **args)
{
	char *local_name;
	char *remote_name;
	
	if (!args[1]) {
		d_printf("reput <filename>\n");
		return 1;
	}
	local_name = talloc_asprintf(ctx, "%s\\%s", ctx->remote_cur_dir, args[1]);
  
	if (!file_exist(local_name)) {
		d_printf("%s does not exist\n", local_name);
		return 1;
	}

	if (args[2]) 
		remote_name = talloc_strdup(ctx, args[2]);
	else
		remote_name = talloc_strdup(ctx, args[1]);
	
	dos_clean_name(remote_name);

	return do_put(ctx, remote_name, local_name, true);
}


/*
  return a string representing a share type
*/
static const char *share_type_str(uint32_t type)
{
	switch (type & 0xF) {
	case STYPE_DISKTREE: 
		return "Disk";
	case STYPE_PRINTQ: 
		return "Printer";
	case STYPE_DEVICE: 
		return "Device";
	case STYPE_IPC: 
		return "IPC";
	default:
		return "Unknown";
	}
}


/*
  display a list of shares from a level 1 share enum
*/
static void display_share_result(struct srvsvc_NetShareCtr1 *ctr1)
{
	int i;

	for (i=0;i<ctr1->count;i++) {
		struct srvsvc_NetShareInfo1 *info = ctr1->array+i;

		printf("\t%-15s %-10.10s %s\n", 
		       info->name, 
		       share_type_str(info->type), 
		       info->comment);
	}
}



/****************************************************************************
try and browse available shares on a host
****************************************************************************/
static bool browse_host(struct loadparm_context *lp_ctx,
			struct tevent_context *ev_ctx,
			const char *query_host)
{
	struct dcerpc_pipe *p;
	char *binding;
	NTSTATUS status;
	struct srvsvc_NetShareEnumAll r;
	struct srvsvc_NetShareInfoCtr info_ctr;
	uint32_t resume_handle = 0;
	TALLOC_CTX *mem_ctx = talloc_init("browse_host");
	struct srvsvc_NetShareCtr1 ctr1;
	uint32_t totalentries = 0;

	binding = talloc_asprintf(mem_ctx, "ncacn_np:%s", query_host);

	status = dcerpc_pipe_connect(mem_ctx, &p, binding, 
					 &ndr_table_srvsvc,
				     cmdline_credentials, ev_ctx,
				     lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Failed to connect to %s - %s\n", 
			 binding, nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	info_ctr.level = 1;
	info_ctr.ctr.ctr1 = &ctr1;

	r.in.server_unc = talloc_asprintf(mem_ctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = ~0;
	r.in.resume_handle = &resume_handle;
	r.out.resume_handle = &resume_handle;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	d_printf("\n\tSharename       Type       Comment\n");
	d_printf("\t---------       ----       -------\n");

	do {
		ZERO_STRUCT(ctr1);
		status = dcerpc_srvsvc_NetShareEnumAll_r(p->binding_handle, mem_ctx, &r);

		if (NT_STATUS_IS_OK(status) && 
		    (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA) ||
		     W_ERROR_IS_OK(r.out.result)) &&
		    r.out.info_ctr->ctr.ctr1) {
			display_share_result(r.out.info_ctr->ctr.ctr1);
			resume_handle += r.out.info_ctr->ctr.ctr1->count;
		}
	} while (NT_STATUS_IS_OK(status) && W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA));

	talloc_free(mem_ctx);

	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("Failed NetShareEnumAll %s - %s/%s\n", 
			 binding, nt_errstr(status), win_errstr(r.out.result));
		return false;
	}

	return false;
}

/****************************************************************************
try and browse available connections on a host
****************************************************************************/
static bool list_servers(const char *wk_grp)
{
	d_printf("REWRITE: list servers not implemented\n");
	return false;
}

/* Some constants for completing filename arguments */

#define COMPL_NONE        0          /* No completions */
#define COMPL_REMOTE      1          /* Complete remote filename */
#define COMPL_LOCAL       2          /* Complete local filename */

static int cmd_help(struct smbclient_context *ctx, const char **args);

/* This defines the commands supported by this client.
 * NOTE: The "!" must be the last one in the list because it's fn pointer
 *       field is NULL, and NULL in that field is used in process_tok()
 *       (below) to indicate the end of the list.  crh
 */
static struct
{
  const char *name;
  int (*fn)(struct smbclient_context *ctx, const char **args);
  const char *description;
  char compl_args[2];      /* Completion argument info */
} commands[] = 
{
  {"?",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"addprivileges",cmd_addprivileges,"<sid|name> <privilege...> add privileges for a user",{COMPL_NONE,COMPL_NONE}},
  {"altname",cmd_altname,"<file> show alt name",{COMPL_NONE,COMPL_NONE}},
  {"acl",cmd_acl,"<file> show file ACL",{COMPL_NONE,COMPL_NONE}},
  {"allinfo",cmd_allinfo,"<file> show all possible info about a file",{COMPL_NONE,COMPL_NONE}},
  {"archive",cmd_archive,"<level>\n0=ignore archive bit\n1=only get archive files\n2=only get archive files and reset archive bit\n3=get all files and reset archive bit",{COMPL_NONE,COMPL_NONE}},
  {"cancel",cmd_rewrite,"<jobid> cancel a print queue entry",{COMPL_NONE,COMPL_NONE}},
  {"cd",cmd_cd,"[directory] change/report the remote directory",{COMPL_REMOTE,COMPL_NONE}},
  {"chmod",cmd_chmod,"<src> <mode> chmod a file using UNIX permission",{COMPL_REMOTE,COMPL_REMOTE}},
  {"chown",cmd_chown,"<src> <uid> <gid> chown a file using UNIX uids and gids",{COMPL_REMOTE,COMPL_REMOTE}},
  {"del",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"delprivileges",cmd_delprivileges,"<sid|name> <privilege...> remove privileges for a user",{COMPL_NONE,COMPL_NONE}},
  {"deltree",cmd_deltree,"<dir> delete a whole directory tree",{COMPL_REMOTE,COMPL_NONE}},
  {"dir",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"du",cmd_du,"<mask> computes the total size of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"eainfo",cmd_eainfo,"<file> show EA contents for a file",{COMPL_NONE,COMPL_NONE}},
  {"exit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"fsinfo",cmd_fsinfo,"query file system info",{COMPL_NONE,COMPL_NONE}},
  {"get",cmd_get,"<remote name> [local name] get a file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"help",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"history",cmd_history,"displays the command history",{COMPL_NONE,COMPL_NONE}},
  {"lcd",cmd_lcd,"[directory] change/report the local current working directory",{COMPL_LOCAL,COMPL_NONE}},
  {"link",cmd_link,"<src> <dest> create a UNIX hard link",{COMPL_REMOTE,COMPL_REMOTE}},
  {"lookup",cmd_lookup,"<sid|name> show SID for name or name for SID",{COMPL_NONE,COMPL_NONE}},
  {"lowercase",cmd_lowercase,"toggle lowercasing of filenames for get",{COMPL_NONE,COMPL_NONE}},  
  {"ls",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"mask",cmd_select,"<mask> mask all filenames against this",{COMPL_REMOTE,COMPL_NONE}},
  {"md",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"mget",cmd_mget,"<mask> get all the matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"mkdir",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"more",cmd_more,"<remote name> view a remote file with your pager",{COMPL_REMOTE,COMPL_NONE}},  
  {"mput",cmd_mput,"<mask> put all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"newer",cmd_newer,"<file> only mget files newer than the specified local file",{COMPL_LOCAL,COMPL_NONE}},
  {"open",cmd_open,"<mask> open a file",{COMPL_REMOTE,COMPL_NONE}},
  {"privileges",cmd_privileges,"<user> show privileges for a user",{COMPL_NONE,COMPL_NONE}},
  {"print",cmd_print,"<file name> print a file",{COMPL_NONE,COMPL_NONE}},
  {"printmode",cmd_printmode,"<graphics or text> set the print mode",{COMPL_NONE,COMPL_NONE}},
  {"prompt",cmd_prompt,"toggle prompting for filenames for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"put",cmd_put,"<local name> [remote name] put a file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"pwd",cmd_pwd,"show current remote directory (same as 'cd' with no args)",{COMPL_NONE,COMPL_NONE}},
  {"q",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"queue",cmd_rewrite,"show the print queue",{COMPL_NONE,COMPL_NONE}},
  {"quit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"rd",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"recurse",cmd_recurse,"toggle directory recursion for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"reget",cmd_reget,"<remote name> [local name] get a file restarting at end of local file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"rename",cmd_rename,"<src> <dest> rename some files",{COMPL_REMOTE,COMPL_REMOTE}},
  {"reput",cmd_reput,"<local name> [remote name] put a file restarting at end of remote file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"rm",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"rmdir",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"symlink",cmd_symlink,"<src> <dest> create a UNIX symlink",{COMPL_REMOTE,COMPL_REMOTE}},
  {"translate",cmd_translate,"toggle text translation for printing",{COMPL_NONE,COMPL_NONE}},
  
  /* Yes, this must be here, see crh's comment above. */
  {"!",NULL,"run a shell command on the local system",{COMPL_NONE,COMPL_NONE}},
  {NULL,NULL,NULL,{COMPL_NONE,COMPL_NONE}}
};


/*******************************************************************
  lookup a command string in the list of commands, including 
  abbreviations
  ******************************************************************/
static int process_tok(const char *tok)
{
	int i = 0, matches = 0;
	int cmd=0;
	int tok_len = strlen(tok);
	
	while (commands[i].fn != NULL) {
		if (strequal(commands[i].name,tok)) {
			matches = 1;
			cmd = i;
			break;
		} else if (strncasecmp(commands[i].name, tok, tok_len) == 0) {
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
help
****************************************************************************/
static int cmd_help(struct smbclient_context *ctx, const char **args)
{
	int i=0,j;
	
	if (args[1]) {
		if ((i = process_tok(args[1])) >= 0)
			d_printf("HELP %s:\n\t%s\n\n",commands[i].name,commands[i].description);
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

static int process_line(struct smbclient_context *ctx, const char *cline);
/****************************************************************************
process a -c command string
****************************************************************************/
static int process_command_string(struct smbclient_context *ctx, const char *cmd)
{
	char **lines;
	int i, rc = 0;

	lines = str_list_make(NULL, cmd, ";");
	for (i = 0; lines[i]; i++) {
		rc |= process_line(ctx, lines[i]);
	}
	talloc_free(lines);

	return rc;
}	

#define MAX_COMPLETIONS 100

typedef struct {
	char *dirmask;
	char **matches;
	int count, samelen;
	const char *text;
	int len;
} completion_remote_t;

static void completion_remote_filter(struct clilist_file_info *f, const char *mask, void *state)
{
	completion_remote_t *info = (completion_remote_t *)state;

	if ((info->count < MAX_COMPLETIONS - 1) && (strncmp(info->text, f->name, info->len) == 0) && (!ISDOT(f->name)) && (!ISDOTDOT(f->name))) {
		if ((info->dirmask[0] == 0) && !(f->attrib & FILE_ATTRIBUTE_DIRECTORY))
			info->matches[info->count] = strdup(f->name);
		else {
			char *tmp;

			if (info->dirmask[0] != 0)
				tmp = talloc_asprintf(NULL, "%s/%s", info->dirmask, f->name);
			else
				tmp = talloc_strdup(NULL, f->name);
			
			if (f->attrib & FILE_ATTRIBUTE_DIRECTORY)
				tmp = talloc_append_string(NULL, tmp, "/");
			info->matches[info->count] = tmp;
		}
		if (info->matches[info->count] == NULL)
			return;
		if (f->attrib & FILE_ATTRIBUTE_DIRECTORY)
			smb_readline_ca_char(0);

		if (info->count == 1)
			info->samelen = strlen(info->matches[info->count]);
		else
			while (strncmp(info->matches[info->count], info->matches[info->count-1], info->samelen) != 0)
				info->samelen--;
		info->count++;
	}
}

static char **remote_completion(const char *text, int len)
{
	char *dirmask;
	int i, ret;
	completion_remote_t info;

	info.samelen = len;
	info.text = text;
	info.len = len;
 
	if (len >= PATH_MAX)
		return(NULL);

	info.matches = malloc_array_p(char *, MAX_COMPLETIONS);
	if (!info.matches) return NULL;
	info.matches[0] = NULL;

	for (i = len-1; i >= 0; i--)
		if ((text[i] == '/') || (text[i] == '\\'))
			break;
	info.text = text+i+1;
	info.samelen = info.len = len-i-1;

	if (i > 0) {
		info.dirmask = talloc_strndup(NULL, text, i+1);
		info.dirmask[i+1] = 0;
		ret = asprintf(&dirmask, "%s%*s*", rl_ctx->remote_cur_dir, i-1,
			       text);
	} else {
		ret = asprintf(&dirmask, "%s*", rl_ctx->remote_cur_dir);
	}
	if (ret < 0) {
		goto cleanup;
	}

	if (smbcli_list(rl_ctx->cli->tree, dirmask, 
		     FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN, 
		     completion_remote_filter, &info) < 0)
		goto cleanup;

	if (info.count == 2)
		info.matches[0] = strdup(info.matches[1]);
	else {
		info.matches[0] = malloc_array_p(char, info.samelen+1);
		if (!info.matches[0])
			goto cleanup;
		strncpy(info.matches[0], info.matches[1], info.samelen);
		info.matches[0][info.samelen] = 0;
	}
	info.matches[info.count] = NULL;
	return info.matches;

cleanup:
	for (i = 0; i < info.count; i++)
		free(info.matches[i]);
	free(info.matches);
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
		
		for (i = 0; commands[i].name; i++)
			if ((strncmp(commands[i].name, text, sp - buf) == 0) && (commands[i].name[sp - buf] == 0))
				break;
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

		matches = malloc_array_p(char *, MAX_COMPLETIONS);
		if (!matches) return NULL;
		matches[0] = NULL;

		len = strlen(text);
		for (i=0;commands[i].fn && count < MAX_COMPLETIONS-1;i++) {
			if (strncmp(text, commands[i].name, len) == 0) {
				matches[count] = strdup(commands[i].name);
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
			matches[0] = strdup(matches[1]);
			break;
		default:
			matches[0] = malloc_array_p(char, samelen+1);
			if (!matches[0])
				goto cleanup;
			strncpy(matches[0], matches[1], samelen);
			matches[0][samelen] = 0;
		}
		matches[count] = NULL;
		return matches;

cleanup:
		count--;
		while (count >= 0) {
			free(matches[count]);
			count--;
		}
		free(matches);
		return NULL;
	}
}

/****************************************************************************
make sure we swallow keepalives during idle time
****************************************************************************/
static void readline_callback(void)
{
	static time_t last_t;
	time_t t;

	t = time(NULL);

	if (t - last_t < 5) return;

	last_t = t;

	smbcli_transport_process(rl_ctx->cli->transport);

	if (rl_ctx->cli->tree) {
		smbcli_chkpath(rl_ctx->cli->tree, "\\");
	}
}

static int process_line(struct smbclient_context *ctx, const char *cline)
{
	const char **args;
	int i;

	/* and get the first part of the command */
	args = (const char **) str_list_make_shell(ctx, cline, NULL);
	if (!args || !args[0])
		return 0;

	if ((i = process_tok(args[0])) >= 0) {
		i = commands[i].fn(ctx, args);
	} else if (i == -2) {
		d_printf("%s: command abbreviation ambiguous\n",args[0]);
	} else {
		d_printf("%s: command not found\n",args[0]);
	}

	talloc_free(args);

	return i;
}

/****************************************************************************
process commands on stdin
****************************************************************************/
static int process_stdin(struct smbclient_context *ctx)
{
	int rc = 0;
	while (1) {
		/* display a prompt */
		char *the_prompt = talloc_asprintf(ctx, "smb: %s> ", ctx->remote_cur_dir);
		char *cline = smb_readline(the_prompt, readline_callback, completion_fn);
		talloc_free(the_prompt);

		if (!cline) break;

		/* special case - first char is ! */
		if (*cline == '!') {
			system(cline + 1);
			free(cline);
			continue;
		}

		rc |= process_command_string(ctx, cline);
		free(cline);

	}

	return rc;
}


/***************************************************** 
return a connection to a server
*******************************************************/
static bool do_connect(struct smbclient_context *ctx, 
		       struct tevent_context *ev_ctx,
		       struct resolve_context *resolve_ctx,
		       const char *specified_server, const char **ports, 
		       const char *specified_share, 
			   const char *socket_options,
		       struct cli_credentials *cred, 
		       struct smbcli_options *options,
		       struct smbcli_session_options *session_options,
			   struct gensec_settings *gensec_settings)
{
	NTSTATUS status;
	char *server, *share;

	rl_ctx = ctx; /* Ugly hack */

	if (strncmp(specified_share, "\\\\", 2) == 0 ||
	    strncmp(specified_share, "//", 2) == 0) {
		smbcli_parse_unc(specified_share, ctx, &server, &share);
	} else {
		share = talloc_strdup(ctx, specified_share);
		server = talloc_strdup(ctx, specified_server);
	}

	ctx->remote_cur_dir = talloc_strdup(ctx, "\\");
	
	status = smbcli_full_connection(ctx, &ctx->cli, server, ports,
					share, NULL, 
					socket_options,
					cred, resolve_ctx, 
					ev_ctx, options, session_options,
					gensec_settings);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Connection to \\\\%s\\%s failed - %s\n", 
			 server, share, nt_errstr(status));
		talloc_free(ctx);
		return false;
	}

	return true;
}

/****************************************************************************
handle a -L query
****************************************************************************/
static int do_host_query(struct loadparm_context *lp_ctx,
			 struct tevent_context *ev_ctx,
			 const char *query_host,
			 const char *workgroup)
{
	browse_host(lp_ctx, ev_ctx, query_host);
	list_servers(workgroup);
	return(0);
}


/****************************************************************************
handle a message operation
****************************************************************************/
static int do_message_op(const char *netbios_name, const char *desthost,
			 const char **destports, const char *destip,
			 int name_type,
			 struct tevent_context *ev_ctx,
			 struct resolve_context *resolve_ctx,
			 struct smbcli_options *options,
             const char *socket_options)
{
	struct nbt_name called, calling;
	const char *server_name;
	struct smbcli_state *cli;

	make_nbt_name_client(&calling, netbios_name);

	nbt_choose_called_name(NULL, &called, desthost, name_type);

	server_name = destip ? destip : desthost;

	if (!(cli = smbcli_state_init(NULL)) ||
	    !smbcli_socket_connect(cli, server_name, destports,
				   ev_ctx, resolve_ctx, options,
                   socket_options)) {
		d_printf("Connection to %s failed\n", server_name);
		return 1;
	}

	if (!smbcli_transport_establish(cli, &calling, &called)) {
		d_printf("session request failed\n");
		talloc_free(cli);
		return 1;
	}

	send_message(cli, desthost);
	talloc_free(cli);

	return 0;
}


/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	char *base_directory = NULL;
	const char *dest_ip = NULL;
	int opt;
	const char *query_host = NULL;
	bool message = false;
	char *desthost = NULL;
	poptContext pc;
	const char *service = NULL;
	int port = 0;
	char *p;
	int rc = 0;
	int name_type = 0x20;
	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev_ctx;
	struct smbclient_context *ctx;
	const char *cmdstr = NULL;
	struct smbcli_options smb_options;
	struct smbcli_session_options smb_session_options;

	struct poptOption long_options[] = {
		POPT_AUTOHELP

		{ "message", 'M', POPT_ARG_STRING, NULL, 'M', "Send message", "HOST" },
		{ "ip-address", 'I', POPT_ARG_STRING, NULL, 'I', "Use this IP to connect to", "IP" },
		{ "stderr", 'E', POPT_ARG_NONE, NULL, 'E', "Write messages to stderr instead of stdout" },
		{ "list", 'L', POPT_ARG_STRING, NULL, 'L', "Get a list of shares available on a host", "HOST" },
		{ "directory", 'D', POPT_ARG_STRING, NULL, 'D', "Start from directory", "DIR" },
		{ "command", 'c', POPT_ARG_STRING, &cmdstr, 'c', "Execute semicolon separated commands" }, 
		{ "send-buffer", 'b', POPT_ARG_INT, NULL, 'b', "Changes the transmit/send buffer", "BYTES" },
		{ "port", 'p', POPT_ARG_INT, &port, 'p', "Port to connect to", "PORT" },
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};
	
	mem_ctx = talloc_init("client.c/main");
	if (!mem_ctx) {
		d_printf("\nclient.c: Not enough memory\n");
		exit(1);
	}

	ctx = talloc_zero(mem_ctx, struct smbclient_context);
	ctx->io_bufsize = 64512;

	pc = poptGetContext("smbclient", argc, (const char **) argv, long_options, 0);
	poptSetOtherOptionHelp(pc, "[OPTIONS] service <password>");

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'M':
			/* Messages are sent to NetBIOS name type 0x3
			 * (Messenger Service).  Make sure we default
			 * to port 139 instead of port 445. srl,crh
			 */
			name_type = 0x03; 
			desthost = strdup(poptGetOptArg(pc));
			if( 0 == port ) port = 139;
 			message = true;
 			break;
		case 'I':
			dest_ip = poptGetOptArg(pc);
			break;
		case 'L':
			query_host = strdup(poptGetOptArg(pc));
			break;
		case 'D':
			base_directory = strdup(poptGetOptArg(pc));
			break;
		case 'b':
			ctx->io_bufsize = MAX(1, atoi(poptGetOptArg(pc)));
			break;
		}
	}

	gensec_init(cmdline_lp_ctx);

	if(poptPeekArg(pc)) {
		char *s = strdup(poptGetArg(pc)); 

		/* Convert any '/' characters in the service name to '\' characters */
		string_replace(s, '/','\\');

		service = s;

		if (count_chars(s,'\\') < 3) {
			d_printf("\n%s: Not enough '\\' characters in service\n",s);
			poptPrintUsage(pc, stderr, 0);
			exit(1);
		}
	}

	if (poptPeekArg(pc)) { 
		cli_credentials_set_password(cmdline_credentials, poptGetArg(pc), CRED_SPECIFIED);
	}

	/*init_names(); */

	if (!query_host && !service && !message) {
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}

	poptFreeContext(pc);

	lpcfg_smbcli_options(cmdline_lp_ctx, &smb_options);
	lpcfg_smbcli_session_options(cmdline_lp_ctx, &smb_session_options);

	ev_ctx = s4_event_context_init(talloc_autofree_context());

	DEBUG( 3, ( "Client started (version %s).\n", SAMBA_VERSION_STRING ) );

	if (query_host && (p=strchr_m(query_host,'#'))) {
		*p = 0;
		p++;
		sscanf(p, "%x", &name_type);
	}
  
	if (query_host) {
		rc = do_host_query(cmdline_lp_ctx, ev_ctx, query_host,
				   lpcfg_workgroup(cmdline_lp_ctx));
		return rc;
	}

	if (message) {
		rc = do_message_op(lpcfg_netbios_name(cmdline_lp_ctx), desthost,
				   lpcfg_smb_ports(cmdline_lp_ctx), dest_ip,
				   name_type, ev_ctx,
				   lpcfg_resolve_context(cmdline_lp_ctx),
				   &smb_options, 
                   lpcfg_socket_options(cmdline_lp_ctx));
		return rc;
	}
	
	if (!do_connect(ctx, ev_ctx, lpcfg_resolve_context(cmdline_lp_ctx),
			desthost, lpcfg_smb_ports(cmdline_lp_ctx), service,
			lpcfg_socket_options(cmdline_lp_ctx),
			cmdline_credentials, &smb_options, &smb_session_options,
			lpcfg_gensec_settings(ctx, cmdline_lp_ctx)))
		return 1;

	if (base_directory) {
		do_cd(ctx, base_directory);
		free(base_directory);
	}
	
	if (cmdstr) {
		rc = process_command_string(ctx, cmdstr);
	} else {
		rc = process_stdin(ctx);
	}

	free(desthost);
	talloc_free(mem_ctx);

	return rc;
}
