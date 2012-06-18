/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB client
   Copyright (C) Andrew Tridgell 1994-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define NO_SYSLOG

#include "includes.h"

#ifndef REGISTER
#define REGISTER 0
#endif

struct cli_state *cli;
extern BOOL in_client;
static int port = SMB_PORT;
pstring cur_dir = "\\";
pstring cd_path = "";
static pstring service;
static pstring desthost;
extern pstring global_myname;
static pstring password;
static pstring username;
static pstring workgroup;
static char *cmdstr;
static BOOL got_pass;
static int io_bufsize = 65520;
extern struct in_addr ipzero;

static int name_type = 0x20;

extern pstring user_socket_options;

static int process_tok(fstring tok);
static void cmd_help(void);

/* 30 second timeout on most commands */
#define CLIENT_TIMEOUT (30*1000)
#define SHORT_TIMEOUT (5*1000)

/* value for unused fid field in trans2 secondary request */
#define FID_UNUSED (0xFFFF)

time_t newer_than = 0;
int archive_level = 0;

extern pstring debugf;
extern int DEBUGLEVEL;

BOOL translation = False;

static BOOL have_ip;

/* clitar bits insert */
extern int blocksize;
extern BOOL tar_inc;
extern BOOL tar_reset;
/* clitar bits end */
 

mode_t myumask = 0755;

BOOL prompt = True;

int printmode = 1;

static BOOL recurse = False;
BOOL lowercase = False;

struct in_addr dest_ip;

#define SEPARATORS " \t\n\r"

BOOL abort_mget = True;

pstring fileselection = "";

extern file_info def_finfo;

/* timing globals */
int get_total_size = 0;
int get_total_time_ms = 0;
int put_total_size = 0;
int put_total_time_ms = 0;

/* totals globals */
static double dir_total;

#define USENMB

/****************************************************************************
write to a local file with CR/LF->LF translation if appropriate. return the 
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
  read from a file with LF->CR/LF translation if appropriate. return the 
  number read. read approx n bytes.
****************************************************************************/
static int readfile(char *b, int size, int n, FILE *f)
{
	int i;
	int c;

	if (!translation || (size != 1))
		return(fread(b,size,n,f));
  
	i = 0;
	while (i < (n - 1) && (i < BUFFER_SIZE)) {
		if ((c = getc(f)) == EOF) {
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
static void send_message(void)
{
	int total_len = 0;
	int grp_id;

	if (!cli_message_start(cli, desthost, username, &grp_id)) {
		DEBUG(0,("message start: %s\n", cli_errstr(cli)));
		return;
	}


	printf("Connected. Type your message, ending it with a Control-D\n");

	while (!feof(stdin) && total_len < 1600) {
		int maxlen = MIN(1600 - total_len,127);
		pstring msg;
		int l=0;
		int c;

		ZERO_ARRAY(msg);

		for (l=0;l<maxlen && (c=fgetc(stdin))!=EOF;l++) {
			if (c == '\n')
				msg[l++] = '\r';
			msg[l] = c;   
		}

		/*
		 * The message is in UNIX codepage format. Convert to
		 * DOS before sending.
		 */

		unix_to_dos(msg, True);

		if (!cli_message_text(cli, msg, l, grp_id)) {
			printf("SMBsendtxt failed (%s)\n",cli_errstr(cli));
			return;
		}      
		
		total_len += l;
	}

	if (total_len >= 1600)
		printf("the message was truncated to 1600 bytes\n");
	else
		printf("sent %d bytes\n",total_len);

	if (!cli_message_end(cli, grp_id)) {
		printf("SMBsendend failed (%s)\n",cli_errstr(cli));
		return;
	}      
}



/****************************************************************************
check the space on a device
****************************************************************************/
static void do_dskattr(void)
{
	int total, bsize, avail;

	if (!cli_dskattr(cli, &bsize, &total, &avail)) {
		DEBUG(0,("Error in dskattr: %s\n",cli_errstr(cli))); 
		return;
	}

	DEBUG(0,("\n\t\t%d blocks of size %d. %d blocks available\n",
		 total, bsize, avail));
}

/****************************************************************************
show cd/pwd
****************************************************************************/
static void cmd_pwd(void)
{
	DEBUG(0,("Current directory is %s",service));
	DEBUG(0,("%s\n",cur_dir));
}


/****************************************************************************
change directory - inner section
****************************************************************************/
static void do_cd(char *newdir)
{
	char *p = newdir;
	pstring saved_dir;
	pstring dname;
      
	dos_format(newdir);

	/* Save the current directory in case the
	   new directory is invalid */
	pstrcpy(saved_dir, cur_dir);
	if (*p == '\\')
		pstrcpy(cur_dir,p);
	else
		pstrcat(cur_dir,p);
	if (*(cur_dir+strlen(cur_dir)-1) != '\\') {
		pstrcat(cur_dir, "\\");
	}
	dos_clean_name(cur_dir);
	pstrcpy(dname,cur_dir);
	pstrcat(cur_dir,"\\");
	dos_clean_name(cur_dir);
	
	if (!strequal(cur_dir,"\\")) {
		if (!cli_chkpath(cli, dname)) {
			DEBUG(0,("cd %s: %s\n", dname, cli_errstr(cli)));
			pstrcpy(cur_dir,saved_dir);
		}
	}
	
	pstrcpy(cd_path,cur_dir);
}

/****************************************************************************
change directory
****************************************************************************/
static void cmd_cd(void)
{
	fstring buf;

	if (next_token(NULL,buf,NULL,sizeof(buf)))
		do_cd(buf);
	else
		DEBUG(0,("Current directory is %s\n",cur_dir));
}


/*******************************************************************
  decide if a file should be operated on
  ********************************************************************/
static BOOL do_this_one(file_info *finfo)
{
	if (finfo->mode & aDIR) return(True);

	if (*fileselection && 
	    !mask_match(finfo->name,fileselection,False,False)) {
		DEBUG(3,("match_match %s failed\n", finfo->name));
		return False;
	}

	if (newer_than && finfo->mtime < newer_than) {
		DEBUG(3,("newer_than %s failed\n", finfo->name));
		return(False);
	}

	if ((archive_level==1 || archive_level==2) && !(finfo->mode & aARCH)) {
		DEBUG(3,("archive %s failed\n", finfo->name));
		return(False);
	}
	
	return(True);
}

/****************************************************************************
  display info about a file
  ****************************************************************************/
static void display_finfo(file_info *finfo)
{
	if (do_this_one(finfo)) {
		time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */
		DEBUG(0,("  %-30s%7.7s %8.0f  %s",
			 finfo->name,
			 attrib_string(finfo->mode),
			 (double)finfo->size,
			 asctime(LocalTime(&t))));
		dir_total += finfo->size;
	}
}


/****************************************************************************
   accumulate size of a file
  ****************************************************************************/
static void do_du(file_info *finfo)
{
	if (do_this_one(finfo)) {
		dir_total += finfo->size;
	}
}

static BOOL do_list_recurse;
static BOOL do_list_dirs;
static char *do_list_queue = 0;
static long do_list_queue_size = 0;
static long do_list_queue_start = 0;
static long do_list_queue_end = 0;
static void (*do_list_fn)(file_info *);

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
	if (do_list_queue)
	{
		free(do_list_queue);
	}
	do_list_queue = 0;
	do_list_queue_size = 0;
	do_list_queue_start = 0;
	do_list_queue_end = 0;
}

static void init_do_list_queue(void)
{
	reset_do_list_queue();
	do_list_queue_size = 1024;
	do_list_queue = malloc(do_list_queue_size);
	if (do_list_queue == 0) { 
		DEBUG(0,("malloc fail for size %d\n",
			 (int)do_list_queue_size));
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
	if (do_list_queue && (do_list_queue_start == do_list_queue_end))
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
	long new_end = do_list_queue_end + ((long)strlen(entry)) + 1;
	while (new_end > do_list_queue_size)
	{
		do_list_queue_size *= 2;
		DEBUG(4,("enlarging do_list_queue to %d\n",
			 (int)do_list_queue_size));
		do_list_queue = Realloc(do_list_queue, do_list_queue_size);
		if (! do_list_queue) {
			DEBUG(0,("failure enlarging do_list_queue to %d bytes\n",
				 (int)do_list_queue_size));
			reset_do_list_queue();
		}
		else
		{
			memset(do_list_queue + do_list_queue_size / 2,
			       0, do_list_queue_size / 2);
		}
	}
	if (do_list_queue)
	{
		pstrcpy(do_list_queue + do_list_queue_end, entry);
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
static void do_list_helper(file_info *f, const char *mask)
{
	if (f->mode & aDIR) {
		if (do_list_dirs && do_this_one(f)) {
			do_list_fn(f);
		}
		if (do_list_recurse && 
		    !strequal(f->name,".") && 
		    !strequal(f->name,"..")) {
			pstring mask2;
			char *p;

			pstrcpy(mask2, mask);
			p = strrchr(mask2,'\\');
			if (!p) return;
			p[1] = 0;
			pstrcat(mask2, f->name);
			pstrcat(mask2,"\\*");
			add_to_do_list_queue(mask2);
		}
		return;
	}

	if (do_this_one(f)) {
		do_list_fn(f);
	}
}


/****************************************************************************
a wrapper around cli_list that adds recursion
  ****************************************************************************/
void do_list(const char *mask,uint16 attribute,void (*fn)(file_info *),BOOL rec, BOOL dirs)
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
			 * invalid inside the call to cli_list.  This
			 * would happen if the list were expanded
			 * during the call.
			 * Fix from E. Jay Berkenbilt (ejb@ql.org)
			 */
			pstring head;
			pstrcpy(head, do_list_queue_head());
			cli_list(cli, head, attribute, do_list_helper);
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
				DEBUG(0,("\n%s\n",next_file));
				if (save_ch)
				{
					*save_ch = '\\';
				}
			}
		}
	}
	else
	{
		if (cli_list(cli, mask, attribute, do_list_helper) == -1)
		{
			DEBUG(0, ("%s listing %s\n", cli_errstr(cli), mask));
		}
	}

	in_do_list = 0;
	reset_do_list_queue();
}

/****************************************************************************
  get a directory listing
  ****************************************************************************/
static void cmd_dir(void)
{
	uint16 attribute = aDIR | aSYSTEM | aHIDDEN;
	pstring mask;
	fstring buf;
	char *p=buf;
	
	dir_total = 0;
	pstrcpy(mask,cur_dir);
	if(mask[strlen(mask)-1]!='\\')
		pstrcat(mask,"\\");
	
	if (next_token(NULL,buf,NULL,sizeof(buf))) {
		dos_format(p);
		if (*p == '\\')
			pstrcpy(mask,p);
		else
			pstrcat(mask,p);
	}
	else {
		pstrcat(mask,"*");
	}

	do_list(mask, attribute, display_finfo, recurse, True);

	do_dskattr();

	DEBUG(3, ("Total bytes listed: %.0f\n", dir_total));
}


/****************************************************************************
  get a directory listing
  ****************************************************************************/
static void cmd_du(void)
{
	uint16 attribute = aDIR | aSYSTEM | aHIDDEN;
	pstring mask;
	fstring buf;
	char *p=buf;
	
	dir_total = 0;
	pstrcpy(mask,cur_dir);
	if(mask[strlen(mask)-1]!='\\')
		pstrcat(mask,"\\");
	
	if (next_token(NULL,buf,NULL,sizeof(buf))) {
		dos_format(p);
		if (*p == '\\')
			pstrcpy(mask,p);
		else
			pstrcat(mask,p);
	} else {
		pstrcat(mask,"*");
	}

	do_list(mask, attribute, do_du, recurse, True);

	do_dskattr();

	DEBUG(0, ("Total number of bytes: %.0f\n", dir_total));
}


/****************************************************************************
  get a file from rname to lname
  ****************************************************************************/
static void do_get(char *rname,char *lname)
{  
	int handle=0,fnum;
	BOOL newhandle = False;
	char *data;
	struct timeval tp_start;
	int read_size = io_bufsize;
	uint16 attr;
	size_t size;
	off_t nread = 0;

	GetTimeOfDay(&tp_start);

	if (lowercase) {
		strlower(lname);
	}

	fnum = cli_open(cli, rname, O_RDONLY, DENY_NONE);

	if (fnum == -1) {
		DEBUG(0,("%s opening remote file %s\n",cli_errstr(cli),rname));
		return;
	}

	if(!strcmp(lname,"-")) {
		handle = fileno(stdout);
	} else {
		handle = sys_open(lname,O_WRONLY|O_CREAT|O_TRUNC,0644);
		newhandle = True;
	}
	if (handle < 0) {
		DEBUG(0,("Error opening local file %s\n",lname));
		return;
	}


	if (!cli_qfileinfo(cli, fnum, 
			   &attr, &size, NULL, NULL, NULL, NULL, NULL) &&
	    !cli_getattrE(cli, fnum, 
			  &attr, &size, NULL, NULL, NULL)) {
		DEBUG(0,("getattrib: %s\n",cli_errstr(cli)));
		return;
	}

	DEBUG(2,("getting file %s of size %.0f as %s ", 
		 lname, (double)size, lname));

	if(!(data = (char *)malloc(read_size))) { 
		DEBUG(0,("malloc fail for size %d\n", read_size));
		cli_close(cli, fnum);
		return;
	}

	while (1) {
		int n = cli_read(cli, fnum, data, nread, read_size);

		if (n <= 0) break;
 
		if (writefile(handle,data, n) != n) {
			DEBUG(0,("Error writing local file\n"));
			break;
		}
      
		nread += n;
	}

	if (nread < size) {
		DEBUG (0, ("Short read when getting file %s. Only got %ld bytes.\n",
               rname, (long)nread));
	}

	free(data);
	
	if (!cli_close(cli, fnum)) {
		DEBUG(0,("Error %s closing remote file\n",cli_errstr(cli)));
	}

	if (newhandle) {
		close(handle);
	}

	if (archive_level >= 2 && (attr & aARCH)) {
		cli_setatr(cli, rname, attr & ~(uint16)aARCH, 0);
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
		
		DEBUG(2,("(%g kb/s) (average %g kb/s)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 get_total_size / (1.024*get_total_time_ms)));
	}
}


/****************************************************************************
  get a file
  ****************************************************************************/
static void cmd_get(void)
{
	pstring lname;
	pstring rname;
	char *p;

	pstrcpy(rname,cur_dir);
	pstrcat(rname,"\\");
	
	p = rname + strlen(rname);
	
	if (!next_token(NULL,p,NULL,sizeof(rname)-strlen(rname))) {
		DEBUG(0,("get <filename>\n"));
		return;
	}
	pstrcpy(lname,p);
	dos_clean_name(rname);
	
	next_token(NULL,lname,NULL,sizeof(lname));
	
	do_get(rname, lname);
}


/****************************************************************************
  do a mget operation on one file
  ****************************************************************************/
static void do_mget(file_info *finfo)
{
	pstring rname;
	pstring quest;
	pstring saved_curdir;
	pstring mget_mask;

	if (strequal(finfo->name,".") || strequal(finfo->name,".."))
		return;

	if (abort_mget)	{
		DEBUG(0,("mget aborted\n"));
		return;
	}

	if (finfo->mode & aDIR)
		slprintf(quest,sizeof(pstring)-1,
			 "Get directory %s? ",finfo->name);
	else
		slprintf(quest,sizeof(pstring)-1,
			 "Get file %s? ",finfo->name);

	if (prompt && !yesno(quest)) return;

	if (!(finfo->mode & aDIR)) {
		pstrcpy(rname,cur_dir);
		pstrcat(rname,finfo->name);
		do_get(rname,finfo->name);
		return;
	}

	/* handle directories */
	pstrcpy(saved_curdir,cur_dir);

	pstrcat(cur_dir,finfo->name);
	pstrcat(cur_dir,"\\");

	unix_format(finfo->name);
	if (lowercase)
		strlower(finfo->name);
	
	if (!directory_exist(finfo->name,NULL) && 
	    mkdir(finfo->name,0777) != 0) {
		DEBUG(0,("failed to create directory %s\n",finfo->name));
		pstrcpy(cur_dir,saved_curdir);
		return;
	}
	
	if (chdir(finfo->name) != 0) {
		DEBUG(0,("failed to chdir to directory %s\n",finfo->name));
		pstrcpy(cur_dir,saved_curdir);
		return;
	}

	pstrcpy(mget_mask,cur_dir);
	pstrcat(mget_mask,"*");
	
	do_list(mget_mask, aSYSTEM | aHIDDEN | aDIR,do_mget,False, True);
	chdir("..");
	pstrcpy(cur_dir,saved_curdir);
}


/****************************************************************************
view the file using the pager
****************************************************************************/
static void cmd_more(void)
{
	fstring rname,lname,pager_cmd;
	char *pager;
	int fd;

	fstrcpy(rname,cur_dir);
	fstrcat(rname,"\\");

	slprintf(lname,sizeof(lname)-1, "%s/smbmore.XXXXXX",tmpdir());
	fd = smb_mkstemp(lname);
	if (fd == -1) {
		DEBUG(0,("failed to create temporary file for more\n"));
		return;
	}
	close(fd);

	if (!next_token(NULL,rname+strlen(rname),NULL,sizeof(rname)-strlen(rname))) {
		DEBUG(0,("more <filename>\n"));
		unlink(lname);
		return;
	}
	dos_clean_name(rname);

	do_get(rname,lname);

	pager=getenv("PAGER");

	slprintf(pager_cmd,sizeof(pager_cmd)-1,
		 "%s %s",(pager? pager:PAGER), lname);
	system(pager_cmd);
	unlink(lname);
}



/****************************************************************************
do a mget command
****************************************************************************/
static void cmd_mget(void)
{
	uint16 attribute = aSYSTEM | aHIDDEN;
	pstring mget_mask;
	fstring buf;
	char *p=buf;

	*mget_mask = 0;

	if (recurse)
		attribute |= aDIR;
	
	abort_mget = False;

	while (next_token(NULL,p,NULL,sizeof(buf))) {
		pstrcpy(mget_mask,cur_dir);
		if(mget_mask[strlen(mget_mask)-1]!='\\')
			pstrcat(mget_mask,"\\");
		
		if (*p == '\\')
			pstrcpy(mget_mask,p);
		else
			pstrcat(mget_mask,p);
		do_list(mget_mask, attribute,do_mget,False,True);
	}

	if (!*mget_mask) {
		pstrcpy(mget_mask,cur_dir);
		if(mget_mask[strlen(mget_mask)-1]!='\\')
			pstrcat(mget_mask,"\\");
		pstrcat(mget_mask,"*");
		do_list(mget_mask, attribute,do_mget,False,True);
	}
}


/****************************************************************************
make a directory of name "name"
****************************************************************************/
static BOOL do_mkdir(char *name)
{
	if (!cli_mkdir(cli, name)) {
		DEBUG(0,("%s making remote directory %s\n",
			 cli_errstr(cli),name));
		return(False);
	}

	return(True);
}


/****************************************************************************
 Exit client.
****************************************************************************/
static void cmd_quit(void)
{
	cli_shutdown(cli);
	exit(0);
}


/****************************************************************************
  make a directory
  ****************************************************************************/
static void cmd_mkdir(void)
{
	pstring mask;
	fstring buf;
	char *p=buf;
  
	pstrcpy(mask,cur_dir);

	if (!next_token(NULL,p,NULL,sizeof(buf))) {
		if (!recurse)
			DEBUG(0,("mkdir <dirname>\n"));
		return;
	}
	pstrcat(mask,p);

	if (recurse) {
		pstring ddir;
		pstring ddir2;
		*ddir2 = 0;
		
		pstrcpy(ddir,mask);
		trim_string(ddir,".",NULL);
		p = strtok(ddir,"/\\");
		while (p) {
			pstrcat(ddir2,p);
			if (!cli_chkpath(cli, ddir2)) { 
				do_mkdir(ddir2);
			}
			pstrcat(ddir2,"\\");
			p = strtok(NULL,"/\\");
		}	 
	} else {
		do_mkdir(mask);
	}
}


/****************************************************************************
  put a single file
  ****************************************************************************/
static void do_put(char *rname,char *lname)
{
	int fnum;
	FILE *f;
	int nread=0;
	char *buf=NULL;
	int maxwrite=io_bufsize;
	
	struct timeval tp_start;
	GetTimeOfDay(&tp_start);

	fnum = cli_open(cli, rname, O_WRONLY|O_CREAT|O_TRUNC, DENY_NONE);
  
	if (fnum == -1) {
		DEBUG(0,("%s opening remote file %s\n",cli_errstr(cli),rname));
		return;
	}

	/* allow files to be piped into smbclient
	   jdblair 24.jun.98 */
	if (!strcmp(lname, "-")) {
		f = stdin;
		/* size of file is not known */
	} else {
		f = sys_fopen(lname,"r");
	}

	if (!f) {
		DEBUG(0,("Error opening local file %s\n",lname));
		return;
	}

  
	DEBUG(1,("putting file %s as %s ",lname,
		 rname));
  
	buf = (char *)malloc(maxwrite);
	while (!feof(f)) {
		int n = maxwrite;
		int ret;

		if ((n = readfile(buf,1,n,f)) < 1) {
			if((n == 0) && feof(f))
				break; /* Empty local file. */

			DEBUG(0,("Error reading local file: %s\n", strerror(errno) ));
			break;
		}

		ret = cli_write(cli, fnum, 0, buf, nread, n);

		if (n != ret) {
			DEBUG(0,("Error writing file: %s\n", cli_errstr(cli)));
			break;
		} 

		nread += n;
	}

	if (!cli_close(cli, fnum)) {
		DEBUG(0,("%s closing remote file %s\n",cli_errstr(cli),rname));
		fclose(f);
		if (buf) free(buf);
		return;
	}

	
	fclose(f);
	if (buf) free(buf);

	{
		struct timeval tp_end;
		int this_time;
		
		GetTimeOfDay(&tp_end);
		this_time = 
			(tp_end.tv_sec - tp_start.tv_sec)*1000 +
			(tp_end.tv_usec - tp_start.tv_usec)/1000;
		put_total_time_ms += this_time;
		put_total_size += nread;
		
		DEBUG(1,("(%g kb/s) (average %g kb/s)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 put_total_size / (1.024*put_total_time_ms)));
	}

	if (f == stdin) {
		cli_shutdown(cli);
		exit(0);
	}
}

 

/****************************************************************************
  put a file
  ****************************************************************************/
static void cmd_put(void)
{
	pstring lname;
	pstring rname;
	fstring buf;
	char *p=buf;
	
	pstrcpy(rname,cur_dir);
	pstrcat(rname,"\\");
  
	if (!next_token(NULL,p,NULL,sizeof(buf))) {
		DEBUG(0,("put <filename>\n"));
		return;
	}
	pstrcpy(lname,p);
  
	if (next_token(NULL,p,NULL,sizeof(buf)))
		pstrcat(rname,p);      
	else
		pstrcat(rname,lname);
	
	dos_clean_name(rname);

	{
		SMB_STRUCT_STAT st;
		/* allow '-' to represent stdin
		   jdblair, 24.jun.98 */
		if (!file_exist(lname,&st) &&
		    (strcmp(lname,"-"))) {
			DEBUG(0,("%s does not exist\n",lname));
			return;
		}
	}

	do_put(rname,lname);
}


/****************************************************************************
  seek in a directory/file list until you get something that doesn't start with
  the specified name
  ****************************************************************************/
static BOOL seek_list(FILE *f,char *name)
{
	pstring s;
	while (!feof(f)) {
		if (!fgets(s,sizeof(s),f)) return(False);
		trim_string(s,"./","\n");
		if (strncmp(s,name,strlen(name)) != 0) {
			pstrcpy(name,s);
			return(True);
		}
	}
      
	return(False);
}


/****************************************************************************
  set the file selection mask
  ****************************************************************************/
static void cmd_select(void)
{
	pstrcpy(fileselection,"");
	next_token(NULL,fileselection,NULL,sizeof(fileselection));
}


/****************************************************************************
  mput some files
  ****************************************************************************/
static void cmd_mput(void)
{
	pstring lname;
	pstring rname;
	fstring buf;
	char *p=buf;
	
	while (next_token(NULL,p,NULL,sizeof(buf))) {
		SMB_STRUCT_STAT st;
		pstring cmd;
		pstring tmpname;
		FILE *f;
		int fd;

		slprintf(tmpname,sizeof(tmpname)-1, "%s/ls.smb.XXXXXX",
			 tmpdir());
		fd = smb_mkstemp(tmpname);

		if (fd == -1) {
			DEBUG(0,("Failed to create temporary file %s\n", 
				 tmpname));
			continue;
		}
		
		if (recurse)
			slprintf(cmd,sizeof(pstring)-1,
				"find . -name \"%s\" -print > %s",p,tmpname);
		else
			slprintf(cmd,sizeof(pstring)-1,
				"find . -maxdepth 1 -name \"%s\" -print > %s",p,tmpname);
		system(cmd);
		close(fd);

		f = sys_fopen(tmpname,"r");
		if (!f) continue;
		unlink(tmpname);
		
		while (!feof(f)) {
			pstring quest;

			if (!fgets(lname,sizeof(lname),f)) break;
			trim_string(lname,"./","\n");
			
		again1:
			
			/* check if it's a directory */
			if (directory_exist(lname,&st)) {
				if (!recurse) continue;
				slprintf(quest,sizeof(pstring)-1,
					 "Put directory %s? ",lname);
				if (prompt && !yesno(quest)) {
					pstrcat(lname,"/");
					if (!seek_list(f,lname))
						break;
					goto again1;		    
				}
	      
				pstrcpy(rname,cur_dir);
				pstrcat(rname,lname);
				dos_format(rname);
				if (!cli_chkpath(cli, rname) && !do_mkdir(rname)) {
					pstrcat(lname,"/");
					if (!seek_list(f,lname))
						break;
					goto again1;
				}
				continue;
			} else {
				slprintf(quest,sizeof(quest)-1,
					 "Put file %s? ",lname);
				if (prompt && !yesno(quest)) continue;
				
				pstrcpy(rname,cur_dir);
				pstrcat(rname,lname);
			}

			dos_format(rname);

			do_put(rname,lname);
		}
		fclose(f);
	}
}


/****************************************************************************
  cancel a print job
  ****************************************************************************/
static void do_cancel(int job)
{
	if (cli_printjob_del(cli, job)) {
		printf("Job %d cancelled\n",job);
	} else {
		printf("Error calcelling job %d : %s\n",job,cli_errstr(cli));
	}
}


/****************************************************************************
  cancel a print job
  ****************************************************************************/
static void cmd_cancel(void)
{
	fstring buf;
	int job; 

	if (!next_token(NULL,buf,NULL,sizeof(buf))) {
		printf("cancel <jobid> ...\n");
		return;
	}
	do {
		job = atoi(buf);
		do_cancel(job);
	} while (next_token(NULL,buf,NULL,sizeof(buf)));
}


/****************************************************************************
  print a file
  ****************************************************************************/
static void cmd_print(void)
{
	pstring lname;
	pstring rname;
	char *p;

	if (!next_token(NULL,lname,NULL, sizeof(lname))) {
		DEBUG(0,("print <filename>\n"));
		return;
	}

	pstrcpy(rname,lname);
	p = strrchr(rname,'/');
	if (p) {
		slprintf(rname, sizeof(rname)-1, "%s-%d", p+1, (int)getpid());
	}

	if (strequal(lname,"-")) {
		slprintf(rname, sizeof(rname)-1, "stdin-%d", (int)getpid());
	}

	do_put(rname, lname);
}


/****************************************************************************
 show a print queue entry
****************************************************************************/
static void queue_fn(struct print_job_info *p)
{
	DEBUG(0,("%-6d   %-9d    %s\n", (int)p->id, (int)p->size, p->name));
}

/****************************************************************************
 show a print queue
****************************************************************************/
static void cmd_queue(void)
{
	cli_print_queue(cli, queue_fn);  
}

/****************************************************************************
delete some files
****************************************************************************/
static void do_del(file_info *finfo)
{
	pstring mask;

	pstrcpy(mask,cur_dir);
	pstrcat(mask,finfo->name);

	if (finfo->mode & aDIR) 
		return;

	if (!cli_unlink(cli, mask)) {
		DEBUG(0,("%s deleting remote file %s\n",cli_errstr(cli),mask));
	}
}

/****************************************************************************
delete some files
****************************************************************************/
static void cmd_del(void)
{
	pstring mask;
	fstring buf;
	uint16 attribute = aSYSTEM | aHIDDEN;

	if (recurse)
		attribute |= aDIR;
	
	pstrcpy(mask,cur_dir);
	
	if (!next_token(NULL,buf,NULL,sizeof(buf))) {
		DEBUG(0,("del <filename>\n"));
		return;
	}
	pstrcat(mask,buf);

	do_list(mask, attribute,do_del,False,False);
}

/****************************************************************************
****************************************************************************/
static void cmd_open(void)
{
	pstring mask;
	fstring buf;
	
	pstrcpy(mask,cur_dir);
	
	if (!next_token(NULL,buf,NULL,sizeof(buf))) {
		DEBUG(0,("del <filename>\n"));
		return;
	}
	pstrcat(mask,buf);

	cli_open(cli, mask, O_RDWR, DENY_ALL);
}


/****************************************************************************
remove a directory
****************************************************************************/
static void cmd_rmdir(void)
{
	pstring mask;
	fstring buf;
  
	pstrcpy(mask,cur_dir);
	
	if (!next_token(NULL,buf,NULL,sizeof(buf))) {
		DEBUG(0,("rmdir <dirname>\n"));
		return;
	}
	pstrcat(mask,buf);

	if (!cli_rmdir(cli, mask)) {
		DEBUG(0,("%s removing remote directory file %s\n",
			 cli_errstr(cli),mask));
	}  
}

/****************************************************************************
rename some files
****************************************************************************/
static void cmd_rename(void)
{
	pstring src,dest;
	fstring buf,buf2;
  
	pstrcpy(src,cur_dir);
	pstrcpy(dest,cur_dir);
	
	if (!next_token(NULL,buf,NULL,sizeof(buf)) || 
	    !next_token(NULL,buf2,NULL, sizeof(buf2))) {
		DEBUG(0,("rename <src> <dest>\n"));
		return;
	}

	pstrcat(src,buf);
	pstrcat(dest,buf2);

	if (!cli_rename(cli, src, dest)) {
		DEBUG(0,("%s renaming files\n",cli_errstr(cli)));
		return;
	}  
}


/****************************************************************************
toggle the prompt flag
****************************************************************************/
static void cmd_prompt(void)
{
	prompt = !prompt;
	DEBUG(2,("prompting is now %s\n",prompt?"on":"off"));
}


/****************************************************************************
set the newer than time
****************************************************************************/
static void cmd_newer(void)
{
	fstring buf;
	BOOL ok;
	SMB_STRUCT_STAT sbuf;

	ok = next_token(NULL,buf,NULL,sizeof(buf));
	if (ok && (sys_stat(buf,&sbuf) == 0)) {
		newer_than = sbuf.st_mtime;
		DEBUG(1,("Getting files newer than %s",
			 asctime(LocalTime(&newer_than))));
	} else {
		newer_than = 0;
	}

	if (ok && newer_than == 0)
		DEBUG(0,("Error setting newer-than time\n"));
}

/****************************************************************************
set the archive level
****************************************************************************/
static void cmd_archive(void)
{
	fstring buf;

	if (next_token(NULL,buf,NULL,sizeof(buf))) {
		archive_level = atoi(buf);
	} else
		DEBUG(0,("Archive level is %d\n",archive_level));
}

/****************************************************************************
toggle the lowercaseflag
****************************************************************************/
static void cmd_lowercase(void)
{
	lowercase = !lowercase;
	DEBUG(2,("filename lowercasing is now %s\n",lowercase?"on":"off"));
}




/****************************************************************************
toggle the recurse flag
****************************************************************************/
static void cmd_recurse(void)
{
	recurse = !recurse;
	DEBUG(2,("directory recursion is now %s\n",recurse?"on":"off"));
}

/****************************************************************************
toggle the translate flag
****************************************************************************/
static void cmd_translate(void)
{
	translation = !translation;
	DEBUG(2,("CR/LF<->LF and print text translation now %s\n",
		 translation?"on":"off"));
}


/****************************************************************************
do a printmode command
****************************************************************************/
static void cmd_printmode(void)
{
	fstring buf;
	fstring mode;

	if (next_token(NULL,buf,NULL,sizeof(buf))) {
		if (strequal(buf,"text")) {
			printmode = 0;      
		} else {
			if (strequal(buf,"graphics"))
				printmode = 1;
			else
				printmode = atoi(buf);
		}
	}

	switch(printmode)
		{
		case 0: 
			fstrcpy(mode,"text");
			break;
		case 1: 
			fstrcpy(mode,"graphics");
			break;
		default: 
			slprintf(mode,sizeof(mode)-1,"%d",printmode);
			break;
		}
	
	DEBUG(2,("the printmode is now %s\n",mode));
}

/****************************************************************************
do the lcd command
****************************************************************************/
static void cmd_lcd(void)
{
	fstring buf;
	pstring d;
	
	if (next_token(NULL,buf,NULL,sizeof(buf)))
		chdir(buf);
	DEBUG(2,("the local directory is now %s\n",sys_getwd(d)));
}

/****************************************************************************
list a share name
****************************************************************************/
static void browse_fn(const char *name, uint32 m, const char *comment)
{
        fstring typestr;

        *typestr=0;

        switch (m)
        {
          case STYPE_DISKTREE:
            fstrcpy(typestr,"Disk"); break;
          case STYPE_PRINTQ:
            fstrcpy(typestr,"Printer"); break;
          case STYPE_DEVICE:
            fstrcpy(typestr,"Device"); break;
          case STYPE_IPC:
            fstrcpy(typestr,"IPC"); break;
        }

        printf("\t%-15.15s%-10.10s%s\n",
               name, typestr,comment);
}


/****************************************************************************
try and browse available connections on a host
****************************************************************************/
static BOOL browse_host(BOOL sort)
{
	int ret;

        printf("\n\tSharename      Type      Comment\n");
        printf("\t---------      ----      -------\n");

	if((ret = cli_RNetShareEnum(cli, browse_fn)) == -1)
		printf("Error returning browse list: %s\n", cli_errstr(cli));

	return (ret != -1);
}

/****************************************************************************
list a server name
****************************************************************************/
static void server_fn(const char *name, uint32 m, const char *comment)
{
        printf("\t%-16.16s     %s\n", name, comment);
}

/****************************************************************************
try and browse available connections on a host
****************************************************************************/
static BOOL list_servers(char *wk_grp)
{
	if (!cli->server_domain) return False;

        printf("\n\tServer               Comment\n");
        printf("\t---------            -------\n");

	cli_NetServerEnum(cli, cli->server_domain, SV_TYPE_ALL, server_fn);

        printf("\n\tWorkgroup            Master\n");
        printf("\t---------            -------\n");

	cli_NetServerEnum(cli, cli->server_domain, SV_TYPE_DOMAIN_ENUM, server_fn);
	return True;
}

#if defined(HAVE_LIBREADLINE)
#  if defined(HAVE_READLINE_HISTORY_H) || defined(HAVE_HISTORY_H)
/****************************************************************************
history
****************************************************************************/
static void cmd_history(void)
{
	HIST_ENTRY **hlist;
	register int i;

	hlist = history_list ();	/* Get pointer to history list */
	
	if (hlist)			/* If list not empty */
	{
		for (i = 0; hlist[i]; i++)	/* then display it */
			DEBUG(0, ("%d: %s\n", i, hlist[i]->line));
	}
}
#  endif 
#endif

/* Some constants for completing filename arguments */

#define COMPL_NONE        0          /* No completions */
#define COMPL_REMOTE      1          /* Complete remote filename */
#define COMPL_LOCAL       2          /* Complete local filename */

/* This defines the commands supported by this client */
struct
{
  char *name;
  void (*fn)(void);
  char *description;
  char compl_args[2];      /* Completion argument info */
} commands[] = 
{
  {"ls",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"dir",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"du",cmd_du,"<mask> computes the total size of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"lcd",cmd_lcd,"[directory] change/report the local current working directory",{COMPL_LOCAL,COMPL_NONE}},
  {"cd",cmd_cd,"[directory] change/report the remote directory",{COMPL_REMOTE,COMPL_NONE}},
  {"pwd",cmd_pwd,"show current remote directory (same as 'cd' with no args)",{COMPL_NONE,COMPL_NONE}},
  {"get",cmd_get,"<remote name> [local name] get a file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"mget",cmd_mget,"<mask> get all the matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"put",cmd_put,"<local name> [remote name] put a file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"mput",cmd_mput,"<mask> put all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"rename",cmd_rename,"<src> <dest> rename some files",{COMPL_REMOTE,COMPL_REMOTE}},
  {"more",cmd_more,"<remote name> view a remote file with your pager",{COMPL_REMOTE,COMPL_NONE}},  
  {"mask",cmd_select,"<mask> mask all filenames against this",{COMPL_REMOTE,COMPL_NONE}},
  {"del",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"open",cmd_open,"<mask> open a file",{COMPL_REMOTE,COMPL_NONE}},
  {"rm",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"mkdir",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"md",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"rmdir",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"rd",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"prompt",cmd_prompt,"toggle prompting for filenames for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"recurse",cmd_recurse,"toggle directory recursion for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"translate",cmd_translate,"toggle text translation for printing",{COMPL_NONE,COMPL_NONE}},  
  {"lowercase",cmd_lowercase,"toggle lowercasing of filenames for get",{COMPL_NONE,COMPL_NONE}},  
  {"print",cmd_print,"<file name> print a file",{COMPL_NONE,COMPL_NONE}},
  {"printmode",cmd_printmode,"<graphics or text> set the print mode",{COMPL_NONE,COMPL_NONE}},
  {"queue",cmd_queue,"show the print queue",{COMPL_NONE,COMPL_NONE}},
  {"cancel",cmd_cancel,"<jobid> cancel a print queue entry",{COMPL_NONE,COMPL_NONE}},
  {"quit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"q",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"exit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"newer",cmd_newer,"<file> only mget files newer than the specified local file",{COMPL_LOCAL,COMPL_NONE}},
  {"archive",cmd_archive,"<level>\n0=ignore archive bit\n1=only get archive files\n2=only get archive files and reset archive bit\n3=get all files and reset archive bit",{COMPL_NONE,COMPL_NONE}},
  {"tar",cmd_tar,"tar <c|x>[IXFqbgNan] current directory to/from <file name>",{COMPL_NONE,COMPL_NONE}},
  {"blocksize",cmd_block,"blocksize <number> (default 20)",{COMPL_NONE,COMPL_NONE}},
  {"tarmode",cmd_tarmode,
     "<full|inc|reset|noreset> tar's behaviour towards archive bits",{COMPL_NONE,COMPL_NONE}},
  {"setmode",cmd_setmode,"filename <setmode string> change modes of file",{COMPL_REMOTE,COMPL_NONE}},
  {"help",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"?",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
#ifdef HAVE_LIBREADLINE
  {"history",cmd_history,"displays the command history",{COMPL_NONE,COMPL_NONE}},
#endif
  {"!",NULL,"run a shell command on the local system",{COMPL_NONE,COMPL_NONE}},
  {"",NULL,NULL,{COMPL_NONE,COMPL_NONE}}
};


/*******************************************************************
  lookup a command string in the list of commands, including 
  abbreviations
  ******************************************************************/
static int process_tok(fstring tok)
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
help
****************************************************************************/
static void cmd_help(void)
{
	int i=0,j;
	fstring buf;
	
	if (next_token(NULL,buf,NULL,sizeof(buf))) {
		if ((i = process_tok(buf)) >= 0)
			DEBUG(0,("HELP %s:\n\t%s\n\n",commands[i].name,commands[i].description));		    
	} else {
		while (commands[i].description) {
			for (j=0; commands[i].description && (j<5); j++) {
				DEBUG(0,("%-15s",commands[i].name));
				i++;
			}
			DEBUG(0,("\n"));
		}
	}
}

#ifndef HAVE_LIBREADLINE
/****************************************************************************
wait for keyboard activity, swallowing network packets
****************************************************************************/
static void wait_keyboard(void)
{
	fd_set fds;
	struct timeval timeout;
  
	while (1) {
		FD_ZERO(&fds);
		FD_SET(cli->fd,&fds);
		FD_SET(fileno(stdin),&fds);

		timeout.tv_sec = 20;
		timeout.tv_usec = 0;
		sys_select(MAX(cli->fd,fileno(stdin))+1,&fds,&timeout);
      
		if (FD_ISSET(fileno(stdin),&fds))
			return;

		/* We deliberately use receive_smb instead of
		   client_receive_smb as we want to receive
		   session keepalives and then drop them here.
		*/
		if (FD_ISSET(cli->fd,&fds))
			receive_smb(cli->fd,cli->inbuf,0);
      
		cli_chkpath(cli, "\\");
	}  
}
#endif

/****************************************************************************
process a -c command string
****************************************************************************/
static void process_command_string(char *cmd)
{
	pstring line;
	char *ptr;

	while (cmd[0] != '\0')    {
		char *p;
		fstring tok;
		int i;
		
		if ((p = strchr(cmd, ';')) == 0) {
			strncpy(line, cmd, 999);
			line[1000] = '\0';
			cmd += strlen(cmd);
		} else {
			if (p - cmd > 999) p = cmd + 999;
			strncpy(line, cmd, p - cmd);
			line[p - cmd] = '\0';
			cmd = p + 1;
		}
		
		/* and get the first part of the command */
		ptr = line;
		if (!next_token(&ptr,tok,NULL,sizeof(tok))) continue;
		
		if ((i = process_tok(tok)) >= 0) {
			commands[i].fn();
		} else if (i == -2) {
			DEBUG(0,("%s: command abbreviation ambiguous\n",tok));
		} else {
			DEBUG(0,("%s: command not found\n",tok));
		}
	}
}	

/****************************************************************************
process commands on stdin
****************************************************************************/
static void process_stdin(void)
{
	pstring line;
	char *ptr;

#ifdef HAVE_LIBREADLINE
/* Minimal readline support, 29Jun1999, s.xenitellis@rhbnc.ac.uk */
#ifdef PROMPTSIZE
#undef PROMPTSIZE
#endif
#define PROMPTSIZE 2048
	char prompt_str[PROMPTSIZE];	/* This holds the buffer "smb: \dir1\> " */
	
        char *temp;			/* Gets the buffer from readline() */
	temp = (char *)NULL;
#endif
	while (!feof(stdin)) {
		fstring tok;
		int i;
#ifdef HAVE_LIBREADLINE
		if ( temp != (char *)NULL )
		{
			free( temp );	/* Free memory allocated every time by readline() */
			temp = (char *)NULL;
		}

		snprintf( prompt_str, PROMPTSIZE - 1, "smb: %s> ", cur_dir );

		temp = readline( prompt_str );		/* We read the line here */

		if ( !temp )
			break;		/* EOF occured */

		if ( *temp )		/* If non-empty line, save to history */
			add_history (temp);
	
		strncpy( line, temp, 1023 ); /* Maximum size of (pstring)line. Null is guarranteed. */
#else 
		/* display a prompt */
		DEBUG(0,("smb: %s> ", cur_dir));
		dbgflush( );
		
		wait_keyboard();
		
		/* and get a response */
		if (!fgets(line,1000,stdin))
			break;
#endif

		/* special case - first char is ! */
		if (*line == '!') {
			system(line + 1);
			continue;
		}
      
		/* and get the first part of the command */
		ptr = line;
		if (!next_token(&ptr,tok,NULL,sizeof(tok))) continue;

		if ((i = process_tok(tok)) >= 0) {
			commands[i].fn();
		} else if (i == -2) {
			DEBUG(0,("%s: command abbreviation ambiguous\n",tok));
		} else {
			DEBUG(0,("%s: command not found\n",tok));
		}
	}
}


/***************************************************** 
return a connection to a server
*******************************************************/
struct cli_state *do_connect(char *server, char *share)
{
	struct cli_state *c;
	struct nmb_name called, calling;
	char *server_n;
	struct in_addr ip;
	extern struct in_addr ipzero;

	if (*share == '\\') {
		server = share+2;
		share = strchr(server,'\\');
		if (!share) return NULL;
		*share = 0;
		share++;
	}

	server_n = server;
	
	ip = ipzero;

	make_nmb_name(&calling, global_myname, 0x0);
	make_nmb_name(&called , server, name_type);

 again:
	ip = ipzero;
	if (have_ip) ip = dest_ip;

	/* have to open a new connection */
	if (!(c=cli_initialise(NULL)) || (cli_set_port(c, port) == 0) ||
	    !cli_connect(c, server_n, &ip)) {
		DEBUG(0,("Connection to %s failed\n", server_n));
		return NULL;
	}

	if (!cli_session_request(c, &calling, &called)) {
		char *p;
		DEBUG(0,("session request to %s failed (%s)\n", 
			 called.name, cli_errstr(c)));
		cli_shutdown(c);
		if ((p=strchr(called.name, '.'))) {
			*p = 0;
			goto again;
		}
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20);
			goto again;
		}
		return NULL;
	}

	DEBUG(4,(" session request ok\n"));

	if (!cli_negprot(c)) {
		DEBUG(0,("protocol negotiation failed\n"));
		cli_shutdown(c);
		return NULL;
	}

	if (!got_pass) {
		char *pass = getpass("Password: ");
		if (pass) {
			pstrcpy(password, pass);
		}
	}

	if (!cli_session_setup(c, username, 
			       password, strlen(password),
			       password, strlen(password),
			       workgroup)) {
		/* if a password was not supplied then try again with a null username */
		if (password[0] || !username[0] || 
		    !cli_session_setup(c, "", "", 0, "", 0, workgroup)) { 
			DEBUG(0,("session setup failed: %s\n", cli_errstr(c)));
			return NULL;
		}
		DEBUG(0,("Anonymous login successful\n"));
	}

	/*
	 * These next two lines are needed to emulate
	 * old client behaviour for people who have
	 * scripts based on client output.
	 * QUESTION ? Do we want to have a 'client compatibility
	 * mode to turn these on/off ? JRA.
	 */

	if (*c->server_domain || *c->server_os || *c->server_type)
		DEBUG(1,("Domain=[%s] OS=[%s] Server=[%s]\n",
			c->server_domain,c->server_os,c->server_type));
	
	DEBUG(4,(" session setup ok\n"));

	if (!cli_send_tconX(c, share, "?????",
			    password, strlen(password)+1)) {
		DEBUG(0,("tree connect failed: %s\n", cli_errstr(c)));
		cli_shutdown(c);
		return NULL;
	}

	DEBUG(4,(" tconx ok\n"));

	return c;
}


/****************************************************************************
  process commands from the client
****************************************************************************/
static BOOL process(char *base_directory)
{
	cli = do_connect(desthost, service);
	if (!cli) {
		return(False);
	}

	if (*base_directory) do_cd(base_directory);
	
	if (cmdstr) {
		process_command_string(cmdstr);
	} else {
		process_stdin();
	}
  
	cli_shutdown(cli);
	return(True);
}

/****************************************************************************
usage on the program
****************************************************************************/
static void usage(char *pname)
{
  DEBUG(0,("Usage: %s service <password> [options]", pname));

  DEBUG(0,("\nVersion %s\n",VERSION));
  DEBUG(0,("\t-s smb.conf           pathname to smb.conf file\n"));
  DEBUG(0,("\t-O socket_options     socket options to use\n"));
  DEBUG(0,("\t-R name resolve order use these name resolution services only\n"));
  DEBUG(0,("\t-M host               send a winpopup message to the host\n"));
  DEBUG(0,("\t-i scope              use this NetBIOS scope\n"));
  DEBUG(0,("\t-N                    don't ask for a password\n"));
  DEBUG(0,("\t-n netbios name.      Use this name as my netbios name\n"));
  DEBUG(0,("\t-d debuglevel         set the debuglevel\n"));
  DEBUG(0,("\t-P                    connect to service as a printer\n"));
  DEBUG(0,("\t-p port               connect to the specified port\n"));
  DEBUG(0,("\t-l log basename.      Basename for log/debug files\n"));
  DEBUG(0,("\t-h                    Print this help message.\n"));
  DEBUG(0,("\t-I dest IP            use this IP to connect to\n"));
  DEBUG(0,("\t-E                    write messages to stderr instead of stdout\n"));
  DEBUG(0,("\t-U username           set the network username\n"));
  DEBUG(0,("\t-L host               get a list of shares available on a host\n"));
  DEBUG(0,("\t-t terminal code      terminal i/o code {sjis|euc|jis7|jis8|junet|hex}\n"));
  DEBUG(0,("\t-m max protocol       set the max protocol level\n"));
  DEBUG(0,("\t-W workgroup          set the workgroup name\n"));
  DEBUG(0,("\t-T<c|x>IXFqgbNan      command line tar\n"));
  DEBUG(0,("\t-D directory          start from directory\n"));
  DEBUG(0,("\t-c command string     execute semicolon separated commands\n"));
  DEBUG(0,("\t-b xmit/send buffer   changes the transmit/send buffer (default: 65520)\n"));
  DEBUG(0,("\n"));
}


/****************************************************************************
get a password from a a file or file descriptor
exit on failure
****************************************************************************/
static void get_password_file(void)
{
	int fd = -1;
	char *p;
	BOOL close_it = False;
	pstring spec;
	char pass[128];
		
	if ((p = getenv("PASSWD_FD")) != NULL) {
		pstrcpy(spec, "descriptor ");
		pstrcat(spec, p);
		sscanf(p, "%d", &fd);
		close_it = False;
	} else if ((p = getenv("PASSWD_FILE")) != NULL) {
		fd = sys_open(p, O_RDONLY, 0);
		pstrcpy(spec, p);
		if (fd < 0) {
			fprintf(stderr, "Error opening PASSWD_FILE %s: %s\n",
				spec, strerror(errno));
			exit(1);
		}
		close_it = True;
	}

	for(p = pass, *p = '\0'; /* ensure that pass is null-terminated */
	    p && p - pass < sizeof(pass);) {
		switch (read(fd, p, 1)) {
		case 1:
			if (*p != '\n' && *p != '\0') {
				*++p = '\0'; /* advance p, and null-terminate pass */
				break;
			}
		case 0:
			if (p - pass) {
				*p = '\0'; /* null-terminate it, just in case... */
				p = NULL; /* then force the loop condition to become false */
				break;
			} else {
				fprintf(stderr, "Error reading password from file %s: %s\n",
					spec, "empty password\n");
				exit(1);
			}
			
		default:
			fprintf(stderr, "Error reading password from file %s: %s\n",
				spec, strerror(errno));
			exit(1);
		}
	}
	pstrcpy(password, pass);
	if (close_it)
		close(fd);
}	



/****************************************************************************
handle a -L query
****************************************************************************/
static int do_host_query(char *query_host)
{
	cli = do_connect(query_host, "IPC$");
	if (!cli)
		return 1;

	browse_host(True);
	list_servers(workgroup);

	cli_shutdown(cli);
	
	return(0);
}


/****************************************************************************
handle a tar operation
****************************************************************************/
static int do_tar_op(char *base_directory)
{
	int ret;
	cli = do_connect(desthost, service);
	if (!cli)
		return 1;

	recurse=True;

	if (*base_directory) do_cd(base_directory);
	
	ret=process_tar();

	cli_shutdown(cli);

	return(ret);
}

/****************************************************************************
handle a message operation
****************************************************************************/
static int do_message_op(void)
{
	struct in_addr ip;
	struct nmb_name called, calling;

	ip = ipzero;

	make_nmb_name(&calling, global_myname, 0x0);
	make_nmb_name(&called , desthost, name_type);

	ip = ipzero;
	if (have_ip) ip = dest_ip;
	else {
		if (!resolve_name( desthost, &ip, name_type )) {
			DEBUG(0,("Unable to resolve name %s\n", desthost));
			return 1;
		}
	}

	if (!(cli=cli_initialise(NULL)) || !cli_connect(cli, desthost, &ip)) {
		DEBUG(0,("Connection to %s failed\n", desthost));
		return 1;
	}

	if (!cli_session_request(cli, &calling, &called)) {
		DEBUG(0,("session request failed\n"));
		cli_shutdown(cli);
		return 1;
	}

	send_message();
	cli_shutdown(cli);

	return 0;
}


/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	fstring base_directory;
	char *pname = argv[0];
	int opt;
	extern FILE *dbf;
	extern char *optarg;
	extern int optind;
	pstring query_host;
	BOOL message = False;
	extern char tar_type;
	static pstring servicesf = CONFIGFILE;
	pstring term_code;
	pstring new_name_resolve_order;
	char *p;

#ifdef KANJI
	pstrcpy(term_code, KANJI);
#else /* KANJI */
	*term_code = 0;
#endif /* KANJI */

	*query_host = 0;
	*base_directory = 0;

	*new_name_resolve_order = 0;

	DEBUGLEVEL = 2;

#ifdef HAVE_LIBREADLINE
	/* Allow conditional parsing of the ~/.inputrc file. */
	rl_readline_name = "smbclient";
#endif    
	setup_logging(pname,True);

	/*
	 * If the -E option is given, be careful not to clobber stdout
	 * before processing the options.  28.Feb.99, richard@hacom.nl.
	 * Also pre-parse the -s option to get the service file name.
	 */

	for (opt = 1; opt < argc; opt++) {
		if (strcmp(argv[opt], "-E") == 0)
			dbf = stderr;
		else if(strncmp(argv[opt], "-s", 2) == 0) {
			if(argv[opt][2] != '\0')
				pstrcpy(servicesf, &argv[opt][2]);
			else if(argv[opt+1] != NULL) {
				/*
				 * At least one more arg left.
				 */
				pstrcpy(servicesf, argv[opt+1]);
			} else {
				usage(pname);
				exit(1);
			}
		}
	}

	TimeInit();
	charset_initialise();

	in_client = True;   /* Make sure that we tell lp_load we are */

	if (!lp_load(servicesf,True,False,False)) {
		fprintf(stderr, "Can't load %s - run testparm to debug it\n", servicesf);
	}
	
	codepage_initialise(lp_client_code_page());

#ifdef WITH_SSL
	sslutil_init(0);
#endif

	pstrcpy(workgroup,lp_workgroup());

	load_interfaces();
	myumask = umask(0);
	umask(myumask);

	if (getenv("USER")) {
		pstrcpy(username,getenv("USER"));

		/* modification to support userid%passwd syntax in the USER var
		   25.Aug.97, jdblair@uab.edu */

		if ((p=strchr(username,'%'))) {
			*p = 0;
			pstrcpy(password,p+1);
			got_pass = True;
			memset(strchr(getenv("USER"),'%')+1,'X',strlen(password));
		}
		strupper(username);
	}

	/* modification to support PASSWD environmental var
	   25.Aug.97, jdblair@uab.edu */
	if (getenv("PASSWD")) {
		pstrcpy(password,getenv("PASSWD"));
		got_pass = True;
	}

	if (getenv("PASSWD_FD") || getenv("PASSWD_FILE")) {
		get_password_file();
		got_pass = True;
	}

	if (*username == 0 && getenv("LOGNAME")) {
		pstrcpy(username,getenv("LOGNAME"));
		strupper(username);
	}

	if (*username == 0) {
		pstrcpy(username,"GUEST");
	}

	if (argc < 2) {
		usage(pname);
		exit(1);
	}
  
	if (*argv[1] != '-') {
		pstrcpy(service,argv[1]);  
		/* Convert any '/' characters in the service name to '\' characters */
		string_replace( service, '/','\\');
		argc--;
		argv++;
		
		if (count_chars(service,'\\') < 3) {
			usage(pname);
			printf("\n%s: Not enough '\\' characters in service\n",service);
			exit(1);
		}

		if (argc > 1 && (*argv[1] != '-')) {
			got_pass = True;
			pstrcpy(password,argv[1]);  
			memset(argv[1],'X',strlen(argv[1]));
			argc--;
			argv++;
		}
	}

	while ((opt = 
		getopt(argc, argv,"s:O:R:M:i:Nn:d:Pp:l:hI:EU:L:t:m:W:T:D:c:b:")) != EOF) {
		switch (opt) {
		case 's':
			pstrcpy(servicesf, optarg);
			break;
		case 'O':
			pstrcpy(user_socket_options,optarg);
			break;	
		case 'R':
			pstrcpy(new_name_resolve_order, optarg);
			break;
		case 'M':
			name_type = 0x03; /* messages are sent to NetBIOS name type 0x3 */
			pstrcpy(desthost,optarg);
			message = True;
			break;
		case 'i':
			{
				extern pstring global_scope;
				pstrcpy(global_scope,optarg);
				strupper(global_scope);
			}
			break;
		case 'N':
			got_pass = True;
			break;
		case 'n':
			pstrcpy(global_myname,optarg);
			break;
		case 'd':
			if (*optarg == 'A')
				DEBUGLEVEL = 10000;
			else
				DEBUGLEVEL = atoi(optarg);
			break;
		case 'P':
			/* not needed anymore */
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'l':
			slprintf(debugf,sizeof(debugf)-1, "%s.client",optarg);
			break;
		case 'h':
			usage(pname);
			exit(0);
			break;
		case 'I':
			{
				dest_ip = *interpret_addr2(optarg);
				if (zero_ip(dest_ip))
					exit(1);
				have_ip = True;
			}
			break;
		case 'E':
			dbf = stderr;
			break;
		case 'U':
			{
				char *lp;
				pstrcpy(username,optarg);
				if ((lp=strchr(username,'%'))) {
					*lp = 0;
					pstrcpy(password,lp+1);
					got_pass = True;
					memset(strchr(optarg,'%')+1,'X',strlen(password));
				}
			}
			break;
		case 'L':
			p = optarg;
			while(*p == '\\' || *p == '/')
				p++;
			pstrcpy(query_host,p);
			break;
		case 't':
			pstrcpy(term_code, optarg);
			break;
		case 'm':
			/* no longer supported */
			break;
		case 'W':
			pstrcpy(workgroup,optarg);
			break;
		case 'T':
			if (!tar_parseargs(argc, argv, optarg, optind)) {
				usage(pname);
				exit(1);
			}
			break;
		case 'D':
			pstrcpy(base_directory,optarg);
			break;
		case 'c':
			cmdstr = optarg;
			got_pass = True;
			break;
		case 'b':
			io_bufsize = MAX(1, atoi(optarg));
			break;
		default:
			usage(pname);
			exit(1);
		}
	}

	get_myname((*global_myname)?NULL:global_myname);  

	if(*new_name_resolve_order)
		lp_set_name_resolve_order(new_name_resolve_order);

	if (*term_code)
		interpret_coding_system(term_code);

	if (!tar_type && !*query_host && !*service && !message) {
		usage(pname);
		exit(1);
	}

	DEBUG( 3, ( "Client started (version %s).\n", VERSION ) );

	if (tar_type) {
		return do_tar_op(base_directory);
	}

	if ((p=strchr(query_host,'#'))) {
		*p = 0;
		p++;
		sscanf(p, "%x", &name_type);
	}
  
	if (*query_host) {
		return do_host_query(query_host);
	}

	if (message) {
		return do_message_op();
	}

	if (!process(base_directory)) {
		return(1);
	}

	return(0);
}
