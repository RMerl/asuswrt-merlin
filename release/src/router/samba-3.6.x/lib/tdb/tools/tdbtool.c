/* 
   Unix SMB/CIFS implementation.
   Samba database functions
   Copyright (C) Andrew Tridgell              1999-2000
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000
   Copyright (C) Andrew Esh                        2001

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

#include "replace.h"
#include "system/locale.h"
#include "system/time.h"
#include "system/filesys.h"
#include "system/wait.h"
#include "tdb.h"

static int do_command(void);
const char *cmdname;
char *arg1, *arg2;
size_t arg1len, arg2len;
int bIterate = 0;
char *line;
TDB_DATA iterate_kbuf;
char cmdline[1024];
static int disable_mmap;

enum commands {
	CMD_CREATE_TDB,
	CMD_OPEN_TDB,
	CMD_TRANSACTION_START,
	CMD_TRANSACTION_COMMIT,
	CMD_TRANSACTION_CANCEL,
	CMD_ERASE,
	CMD_DUMP,
	CMD_INSERT,
	CMD_MOVE,
	CMD_STORE,
	CMD_SHOW,
	CMD_KEYS,
	CMD_HEXKEYS,
	CMD_DELETE,
	CMD_LIST_HASH_FREE,
	CMD_LIST_FREE,
	CMD_INFO,
	CMD_MMAP,
	CMD_SPEED,
	CMD_FIRST,
	CMD_NEXT,
	CMD_SYSTEM,
	CMD_CHECK,
	CMD_QUIT,
	CMD_HELP
};

typedef struct {
	const char *name;
	enum commands cmd;
} COMMAND_TABLE;

COMMAND_TABLE cmd_table[] = {
	{"create",	CMD_CREATE_TDB},
	{"open",	CMD_OPEN_TDB},
	{"transaction_start",	CMD_TRANSACTION_START},
	{"transaction_commit",	CMD_TRANSACTION_COMMIT},
	{"transaction_cancel",	CMD_TRANSACTION_CANCEL},
	{"erase",	CMD_ERASE},
	{"dump",	CMD_DUMP},
	{"insert",	CMD_INSERT},
	{"move",	CMD_MOVE},
	{"store",	CMD_STORE},
	{"show",	CMD_SHOW},
	{"keys",	CMD_KEYS},
	{"hexkeys",	CMD_HEXKEYS},
	{"delete",	CMD_DELETE},
	{"list",	CMD_LIST_HASH_FREE},
	{"free",	CMD_LIST_FREE},
	{"info",	CMD_INFO},
	{"speed",	CMD_SPEED},
	{"mmap",	CMD_MMAP},
	{"first",	CMD_FIRST},
	{"1",		CMD_FIRST},
	{"next",	CMD_NEXT},
	{"n",		CMD_NEXT},
	{"check",	CMD_CHECK},
	{"quit",	CMD_QUIT},
	{"q",		CMD_QUIT},
	{"!",		CMD_SYSTEM},
	{NULL,		CMD_HELP}
};

struct timeval tp1,tp2;

static void _start_timer(void)
{
	gettimeofday(&tp1,NULL);
}

static double _end_timer(void)
{
	gettimeofday(&tp2,NULL);
	return((tp2.tv_sec - tp1.tv_sec) + 
	       (tp2.tv_usec - tp1.tv_usec)*1.0e-6);
}

#ifdef PRINTF_ATTRIBUTE
static void tdb_log(struct tdb_context *tdb, enum tdb_debug_level level, const char *format, ...) PRINTF_ATTRIBUTE(3,4);
#endif
static void tdb_log(struct tdb_context *tdb, enum tdb_debug_level level, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

/* a tdb tool for manipulating a tdb database */

static TDB_CONTEXT *tdb;

static int print_rec(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state);
static int print_key(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state);
static int print_hexkey(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state);

static void print_asc(const char *buf,int len)
{
	int i;

	/* We're probably printing ASCII strings so don't try to display
	   the trailing NULL character. */

	if (buf[len - 1] == 0)
	        len--;

	for (i=0;i<len;i++)
		printf("%c",isprint(buf[i])?buf[i]:'.');
}

static void print_data(const char *buf,int len)
{
	int i=0;
	if (len<=0) return;
	printf("[%03X] ",i);
	for (i=0;i<len;) {
		printf("%02X ",(int)((unsigned char)buf[i]));
		i++;
		if (i%8 == 0) printf(" ");
		if (i%16 == 0) {      
			print_asc(&buf[i-16],8); printf(" ");
			print_asc(&buf[i-8],8); printf("\n");
			if (i<len) printf("[%03X] ",i);
		}
	}
	if (i%16) {
		int n;
		
		n = 16 - (i%16);
		printf(" ");
		if (n>8) printf(" ");
		while (n--) printf("   ");
		
		n = i%16;
		if (n > 8) n = 8;
		print_asc(&buf[i-(i%16)],n); printf(" ");
		n = (i%16) - n;
		if (n>0) print_asc(&buf[i-n],n); 
		printf("\n");    
	}
}

static void help(void)
{
	printf("\n"
"tdbtool: \n"
"  create    dbname     : create a database\n"
"  open      dbname     : open an existing database\n"
"  transaction_start    : start a transaction\n"
"  transaction_commit   : commit a transaction\n"
"  transaction_cancel   : cancel a transaction\n"
"  erase                : erase the database\n"
"  dump                 : dump the database as strings\n"
"  keys                 : dump the database keys as strings\n"
"  hexkeys              : dump the database keys as hex values\n"
"  info                 : print summary info about the database\n"
"  insert    key  data  : insert a record\n"
"  move      key  file  : move a record to a destination tdb\n"
"  store     key  data  : store a record (replace)\n"
"  show      key        : show a record by key\n"
"  delete    key        : delete a record by key\n"
"  list                 : print the database hash table and freelist\n"
"  free                 : print the database freelist\n"
"  check                : check the integrity of an opened database\n"
"  speed                : perform speed tests on the database\n"
"  ! command            : execute system command\n"
"  1 | first            : print the first record\n"
"  n | next             : print the next record\n"
"  q | quit             : terminate\n"
"  \\n                   : repeat 'next' command\n"
"\n");
}

static void terror(const char *why)
{
	printf("%s\n", why);
}

static void create_tdb(const char *tdbname)
{
	struct tdb_logging_context log_ctx;
	log_ctx.log_fn = tdb_log;

	if (tdb) tdb_close(tdb);
	tdb = tdb_open_ex(tdbname, 0, TDB_CLEAR_IF_FIRST | (disable_mmap?TDB_NOMMAP:0),
			  O_RDWR | O_CREAT | O_TRUNC, 0600, &log_ctx, NULL);
	if (!tdb) {
		printf("Could not create %s: %s\n", tdbname, strerror(errno));
	}
}

static void open_tdb(const char *tdbname)
{
	struct tdb_logging_context log_ctx;
	log_ctx.log_fn = tdb_log;

	if (tdb) tdb_close(tdb);
	tdb = tdb_open_ex(tdbname, 0, disable_mmap?TDB_NOMMAP:0, O_RDWR, 0600,
			  &log_ctx, NULL);
	if (!tdb) {
		printf("Could not open %s: %s\n", tdbname, strerror(errno));
	}
}

static void insert_tdb(char *keyname, size_t keylen, char* data, size_t datalen)
{
	TDB_DATA key, dbuf;

	if ((keyname == NULL) || (keylen == 0)) {
		terror("need key");
		return;
	}

	key.dptr = (unsigned char *)keyname;
	key.dsize = keylen;
	dbuf.dptr = (unsigned char *)data;
	dbuf.dsize = datalen;

	if (tdb_store(tdb, key, dbuf, TDB_INSERT) == -1) {
		terror("insert failed");
	}
}

static void store_tdb(char *keyname, size_t keylen, char* data, size_t datalen)
{
	TDB_DATA key, dbuf;

	if ((keyname == NULL) || (keylen == 0)) {
		terror("need key");
		return;
	}

	if ((data == NULL) || (datalen == 0)) {
		terror("need data");
		return;
	}

	key.dptr = (unsigned char *)keyname;
	key.dsize = keylen;
	dbuf.dptr = (unsigned char *)data;
	dbuf.dsize = datalen;

	printf("Storing key:\n");
	print_rec(tdb, key, dbuf, NULL);

	if (tdb_store(tdb, key, dbuf, TDB_REPLACE) == -1) {
		terror("store failed");
	}
}

static void show_tdb(char *keyname, size_t keylen)
{
	TDB_DATA key, dbuf;

	if ((keyname == NULL) || (keylen == 0)) {
		terror("need key");
		return;
	}

	key.dptr = (unsigned char *)keyname;
	key.dsize = keylen;

	dbuf = tdb_fetch(tdb, key);
	if (!dbuf.dptr) {
	    terror("fetch failed");
	    return;
	}
	
	print_rec(tdb, key, dbuf, NULL);
	
	free( dbuf.dptr );
	
	return;
}

static void delete_tdb(char *keyname, size_t keylen)
{
	TDB_DATA key;

	if ((keyname == NULL) || (keylen == 0)) {
		terror("need key");
		return;
	}

	key.dptr = (unsigned char *)keyname;
	key.dsize = keylen;

	if (tdb_delete(tdb, key) != 0) {
		terror("delete failed");
	}
}

static void move_rec(char *keyname, size_t keylen, char* tdbname)
{
	TDB_DATA key, dbuf;
	TDB_CONTEXT *dst_tdb;

	if ((keyname == NULL) || (keylen == 0)) {
		terror("need key");
		return;
	}

	if ( !tdbname ) {
		terror("need destination tdb name");
		return;
	}

	key.dptr = (unsigned char *)keyname;
	key.dsize = keylen;

	dbuf = tdb_fetch(tdb, key);
	if (!dbuf.dptr) {
		terror("fetch failed");
		return;
	}
	
	print_rec(tdb, key, dbuf, NULL);
	
	dst_tdb = tdb_open(tdbname, 0, 0, O_RDWR, 0600);
	if ( !dst_tdb ) {
		terror("unable to open destination tdb");
		return;
	}
	
	if ( tdb_store( dst_tdb, key, dbuf, TDB_REPLACE ) == -1 ) {
		terror("failed to move record");
	}
	else
		printf("record moved\n");
	
	tdb_close( dst_tdb );
	
	return;
}

static int print_rec(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
	printf("\nkey %d bytes\n", (int)key.dsize);
	print_asc((const char *)key.dptr, key.dsize);
	printf("\ndata %d bytes\n", (int)dbuf.dsize);
	print_data((const char *)dbuf.dptr, dbuf.dsize);
	return 0;
}

static int print_key(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
	printf("key %d bytes: ", (int)key.dsize);
	print_asc((const char *)key.dptr, key.dsize);
	printf("\n");
	return 0;
}

static int print_hexkey(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
	printf("key %d bytes\n", (int)key.dsize);
	print_data((const char *)key.dptr, key.dsize);
	printf("\n");
	return 0;
}

static int total_bytes;

static int traverse_fn(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf, void *state)
{
	total_bytes += dbuf.dsize;
	return 0;
}

static void info_tdb(void)
{
	char *summary = tdb_summary(tdb);

	if (!summary) {
		printf("Error = %s\n", tdb_errorstr(tdb));
	} else {
		printf("%s", summary);
		free(summary);
	}
}

static void speed_tdb(const char *tlimit)
{
	const char *str = "store test", *str2 = "transaction test";
	unsigned timelimit = tlimit?atoi(tlimit):0;
	double t;
	int ops;
	if (timelimit == 0) timelimit = 5;

	ops = 0;
	printf("Testing store speed for %u seconds\n", timelimit);
	_start_timer();
	do {
		long int r = random();
		TDB_DATA key, dbuf;
		key.dptr = discard_const_p(uint8_t, str);
		key.dsize = strlen((char *)key.dptr);
		dbuf.dptr = (uint8_t *) &r;
		dbuf.dsize = sizeof(r);
		tdb_store(tdb, key, dbuf, TDB_REPLACE);
		t = _end_timer();
		ops++;
	} while (t < timelimit);
	printf("%10.3f ops/sec\n", ops/t);

	ops = 0;
	printf("Testing fetch speed for %u seconds\n", timelimit);
	_start_timer();
	do {
		long int r = random();
		TDB_DATA key, dbuf;
		key.dptr = discard_const_p(uint8_t, str);
		key.dsize = strlen((char *)key.dptr);
		dbuf.dptr = (uint8_t *) &r;
		dbuf.dsize = sizeof(r);
		tdb_fetch(tdb, key);
		t = _end_timer();
		ops++;
	} while (t < timelimit);
	printf("%10.3f ops/sec\n", ops/t);

	ops = 0;
	printf("Testing transaction speed for %u seconds\n", timelimit);
	_start_timer();
	do {
		long int r = random();
		TDB_DATA key, dbuf;
		key.dptr = discard_const_p(uint8_t, str2);
		key.dsize = strlen((char *)key.dptr);
		dbuf.dptr = (uint8_t *) &r;
		dbuf.dsize = sizeof(r);
		tdb_transaction_start(tdb);
		tdb_store(tdb, key, dbuf, TDB_REPLACE);
		tdb_transaction_commit(tdb);
		t = _end_timer();
		ops++;
	} while (t < timelimit);
	printf("%10.3f ops/sec\n", ops/t);

	ops = 0;
	printf("Testing traverse speed for %u seconds\n", timelimit);
	_start_timer();
	do {
		tdb_traverse(tdb, traverse_fn, NULL);
		t = _end_timer();
		ops++;
	} while (t < timelimit);
	printf("%10.3f ops/sec\n", ops/t);
}

static void toggle_mmap(void)
{
	disable_mmap = !disable_mmap;
	if (disable_mmap) {
		printf("mmap is disabled\n");
	} else {
		printf("mmap is enabled\n");
	}
}

static char *tdb_getline(const char *prompt)
{
	static char thisline[1024];
	char *p;
	fputs(prompt, stdout);
	thisline[0] = 0;
	p = fgets(thisline, sizeof(thisline)-1, stdin);
	if (p) p = strchr(p, '\n');
	if (p) *p = 0;
	return p?thisline:NULL;
}

static int do_delete_fn(TDB_CONTEXT *the_tdb, TDB_DATA key, TDB_DATA dbuf,
                     void *state)
{
    return tdb_delete(the_tdb, key);
}

static void first_record(TDB_CONTEXT *the_tdb, TDB_DATA *pkey)
{
	TDB_DATA dbuf;
	*pkey = tdb_firstkey(the_tdb);
	
	dbuf = tdb_fetch(the_tdb, *pkey);
	if (!dbuf.dptr) terror("fetch failed");
	else {
		print_rec(the_tdb, *pkey, dbuf, NULL);
	}
}

static void next_record(TDB_CONTEXT *the_tdb, TDB_DATA *pkey)
{
	TDB_DATA dbuf;
	*pkey = tdb_nextkey(the_tdb, *pkey);
	
	dbuf = tdb_fetch(the_tdb, *pkey);
	if (!dbuf.dptr) 
		terror("fetch failed");
	else
		print_rec(the_tdb, *pkey, dbuf, NULL);
}

static int count(TDB_DATA key, TDB_DATA data, void *private_data)
{
	(*(unsigned int *)private_data)++;
	return 0;
}

static void check_db(TDB_CONTEXT *the_tdb)
{
	int tdbcount = 0;
	if (!the_tdb)
		printf("Error: No database opened!\n");
	else if (tdb_check(the_tdb, count, &tdbcount) == -1)
		printf("Integrity check for the opened database failed.\n");
	else
		printf("Database integrity is OK and has %d records.\n",
		       tdbcount);
}

static int do_command(void)
{
	COMMAND_TABLE *ctp = cmd_table;
	enum commands mycmd = CMD_HELP;
	int cmd_len;

	if (cmdname && strlen(cmdname) == 0) {
		mycmd = CMD_NEXT;
	} else {
		while (ctp->name) {
			cmd_len = strlen(ctp->name);
			if (strncmp(ctp->name,cmdname,cmd_len) == 0) {
				mycmd = ctp->cmd;
				break;
			}
			ctp++;
		}
	}

	switch (mycmd) {
	case CMD_CREATE_TDB:
		bIterate = 0;
		create_tdb(arg1);
		return 0;
	case CMD_OPEN_TDB:
		bIterate = 0;
		open_tdb(arg1);
		return 0;
	case CMD_SYSTEM:
		/* Shell command */
		if (system(arg1) == -1) {
			terror("system() call failed\n");
		}
		return 0;
	case CMD_QUIT:
		return 1;
	default:
		/* all the rest require a open database */
		if (!tdb) {
			bIterate = 0;
			terror("database not open");
			help();
			return 0;
		}
		switch (mycmd) {
		case CMD_TRANSACTION_START:
			bIterate = 0;
			tdb_transaction_start(tdb);
			return 0;
		case CMD_TRANSACTION_COMMIT:
			bIterate = 0;
			tdb_transaction_commit(tdb);
			return 0;
		case CMD_TRANSACTION_CANCEL:
			bIterate = 0;
			tdb_transaction_cancel(tdb);
			return 0;
		case CMD_ERASE:
			bIterate = 0;
			tdb_traverse(tdb, do_delete_fn, NULL);
			return 0;
		case CMD_DUMP:
			bIterate = 0;
			tdb_traverse(tdb, print_rec, NULL);
			return 0;
		case CMD_INSERT:
			bIterate = 0;
			insert_tdb(arg1, arg1len,arg2,arg2len);
			return 0;
		case CMD_MOVE:
			bIterate = 0;
			move_rec(arg1,arg1len,arg2);
			return 0;
		case CMD_STORE:
			bIterate = 0;
			store_tdb(arg1,arg1len,arg2,arg2len);
			return 0;
		case CMD_SHOW:
			bIterate = 0;
			show_tdb(arg1, arg1len);
			return 0;
		case CMD_KEYS:
			tdb_traverse(tdb, print_key, NULL);
			return 0;
		case CMD_HEXKEYS:
			tdb_traverse(tdb, print_hexkey, NULL);
			return 0;
		case CMD_DELETE:
			bIterate = 0;
			delete_tdb(arg1,arg1len);
			return 0;
		case CMD_LIST_HASH_FREE:
			tdb_dump_all(tdb);
			return 0;
		case CMD_LIST_FREE:
			tdb_printfreelist(tdb);
			return 0;
		case CMD_INFO:
			info_tdb();
			return 0;
		case CMD_SPEED:
			speed_tdb(arg1);
			return 0;
		case CMD_MMAP:
			toggle_mmap();
			return 0;
		case CMD_FIRST:
			bIterate = 1;
			first_record(tdb, &iterate_kbuf);
			return 0;
		case CMD_NEXT:
			if (bIterate)
				next_record(tdb, &iterate_kbuf);
			return 0;
		case CMD_CHECK:
			check_db(tdb);
			return 0;
		case CMD_HELP:
			help();
			return 0;
		case CMD_CREATE_TDB:
		case CMD_OPEN_TDB:
		case CMD_SYSTEM:
		case CMD_QUIT:
			/*
			 * unhandled commands.  cases included here to avoid compiler
			 * warnings.
			 */
			return 0;
		}
	}

	return 0;
}

static char *convert_string(char *instring, size_t *sizep)
{
	size_t length = 0;
	char *outp, *inp;
	char temp[3];

	outp = inp = instring;

	while (*inp) {
		if (*inp == '\\') {
			inp++;
			if (*inp && strchr("0123456789abcdefABCDEF",(int)*inp)) {
				temp[0] = *inp++;
				temp[1] = '\0';
				if (*inp && strchr("0123456789abcdefABCDEF",(int)*inp)) {
					temp[1] = *inp++;
					temp[2] = '\0';
				}
				*outp++ = (char)strtol((const char *)temp,NULL,16);
			} else {
				*outp++ = *inp++;
			}
		} else {
			*outp++ = *inp++;
		}
		length++;
	}
	*sizep = length;
	return instring;
}

int main(int argc, char *argv[])
{
	cmdname = "";
	arg1 = NULL;
	arg1len = 0;
	arg2 = NULL;
	arg2len = 0;

	if (argv[1]) {
		cmdname = "open";
		arg1 = argv[1];
		do_command();
		cmdname =  "";
		arg1 = NULL;
	}

	switch (argc) {
	case 1:
	case 2:
		/* Interactive mode */
		while ((cmdname = tdb_getline("tdb> "))) {
			arg2 = arg1 = NULL;
			if ((arg1 = strchr((const char *)cmdname,' ')) != NULL) {
				arg1++;
				arg2 = arg1;
				while (*arg2) {
					if (*arg2 == ' ') {
						*arg2++ = '\0';
						break;
					}
					if ((*arg2++ == '\\') && (*arg2 == ' ')) {
						arg2++;
					}
				}
			}
			if (arg1) arg1 = convert_string(arg1,&arg1len);
			if (arg2) arg2 = convert_string(arg2,&arg2len);
			if (do_command()) break;
		}
		break;
	case 5:
		arg2 = convert_string(argv[4],&arg2len);
	case 4:
		arg1 = convert_string(argv[3],&arg1len);
	case 3:
		cmdname = argv[2];
	default:
		do_command();
		break;
	}

	if (tdb) tdb_close(tdb);

	return 0;
}
