/* 
   Unix SMB/CIFS implementation.
   randomised byte range lock tester
   Copyright (C) Andrew Tridgell 1999
   
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
#include "lib/events/events.h"
#include "system/filesys.h"
#include "system/time.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "libcli/libcli.h"
#include "param/param.h"
#include "dynconfig/dynconfig.h"
#include "libcli/resolve/resolve.h"

static int numops = 1000;
static int showall;
static int analyze;
static int hide_unlock_fails;
static int use_oplocks;
static uint_t lock_range = 100;
static uint_t lock_base = 0;
static uint_t min_length = 0;
static int exact_error_codes;
static int zero_zero;

#define FILENAME "\\locktest.dat"

#define READ_PCT 50
#define LOCK_PCT 45
#define UNLOCK_PCT 70
#define RANGE_MULTIPLE 1
#define NSERVERS 2
#define NCONNECTIONS 2
#define NFILES 2
#define LOCK_TIMEOUT 0

static struct cli_credentials *servers[NSERVERS];

enum lock_op {OP_LOCK, OP_UNLOCK, OP_REOPEN};

struct record {
	enum lock_op lock_op;
	enum brl_type lock_type;
	char conn, f;
	uint64_t start, len;
	char needed;
	uint16_t pid;
};

#define PRESETS 0

#if PRESETS
static struct record preset[] = {
{OP_LOCK, WRITE_LOCK, 0, 0, 2, 0, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 0, 0, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 3, 0, 1},
{OP_UNLOCK, 0       , 0, 0, 2, 0, 1},
{OP_REOPEN, 0, 0, 0, 0, 0, 1},

{OP_LOCK, READ_LOCK, 0, 0, 2, 0, 1},
{OP_LOCK, READ_LOCK, 0, 0, 1, 1, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 0, 0, 1},
{OP_REOPEN, 0, 0, 0, 0, 0, 1},

{OP_LOCK, READ_LOCK, 0, 0, 2, 0, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 3, 1, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 0, 0, 1},
{OP_REOPEN, 0, 0, 0, 0, 0, 1},

{OP_LOCK, READ_LOCK, 0, 0, 2, 0, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 1, 1, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 0, 0, 1},
{OP_REOPEN, 0, 0, 0, 0, 0, 1},

{OP_LOCK, WRITE_LOCK, 0, 0, 2, 0, 1},
{OP_LOCK, READ_LOCK, 0, 0, 1, 1, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 0, 0, 1},
{OP_REOPEN, 0, 0, 0, 0, 0, 1},

{OP_LOCK, WRITE_LOCK, 0, 0, 2, 0, 1},
{OP_LOCK, READ_LOCK, 0, 0, 3, 1, 1},
{OP_LOCK, WRITE_LOCK, 0, 0, 0, 0, 1},
{OP_REOPEN, 0, 0, 0, 0, 0, 1},

};
#endif

static struct record *recorded;

/***************************************************** 
return a connection to a server
*******************************************************/
static struct smbcli_state *connect_one(struct tevent_context *ev,
					struct loadparm_context *lp_ctx,
					TALLOC_CTX *mem_ctx,
					char *share, int snum, int conn)
{
	struct smbcli_state *c;
	char *server, *myname;
	NTSTATUS status;
	int retries = 10;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lp_smbcli_options(lp_ctx, &options);
	lp_smbcli_session_options(lp_ctx, &session_options);

	printf("connect_one(%s, %d, %d)\n", share, snum, conn);

	server = talloc_strdup(mem_ctx, share+2);
	share = strchr_m(server,'\\');
	if (!share) return NULL;
	*share = 0;
	share++;

	if (snum == 0) {
		char **unc_list = NULL;
		int num_unc_names;
		const char *p;
		p = lp_parm_string(lp_ctx, NULL, "torture", "unclist");
		if (p) {
			char *h, *s;
			unc_list = file_lines_load(p, &num_unc_names, 0, NULL);
			if (!unc_list || num_unc_names <= 0) {
				printf("Failed to load unc names list from '%s'\n", p);
				exit(1);
			}

			if (!smbcli_parse_unc(unc_list[conn % num_unc_names],
					      NULL, &h, &s)) {
				printf("Failed to parse UNC name %s\n",
				       unc_list[conn % num_unc_names]);
				exit(1);
			}
			server = talloc_strdup(mem_ctx, h);
			share = talloc_strdup(mem_ctx, s);
		}
	}


	myname = talloc_asprintf(mem_ctx, "lock-%u-%u", getpid(), snum);
	cli_credentials_set_workstation(servers[snum], myname, CRED_SPECIFIED);

	do {
		printf("\\\\%s\\%s\n", server, share);
		status = smbcli_full_connection(NULL, &c, 
						server, 
						lp_smb_ports(lp_ctx),
						share, NULL,
						lp_socket_options(lp_ctx),
						servers[snum], 
						lp_resolve_context(lp_ctx),
						ev, &options, &session_options,
						lp_iconv_convenience(lp_ctx),
						lp_gensec_settings(mem_ctx, lp_ctx));
		if (!NT_STATUS_IS_OK(status)) {
			sleep(2);
		}
	} while (!NT_STATUS_IS_OK(status) && retries--);

	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return c;
}


static void reconnect(struct tevent_context *ev,
		      struct loadparm_context *lp_ctx,
			  TALLOC_CTX *mem_ctx,
		      struct smbcli_state *cli[NSERVERS][NCONNECTIONS], int fnum[NSERVERS][NCONNECTIONS][NFILES],
		      char *share[NSERVERS])
{
	int server, conn, f;

	for (server=0;server<NSERVERS;server++)
	for (conn=0;conn<NCONNECTIONS;conn++) {
		if (cli[server][conn]) {
			for (f=0;f<NFILES;f++) {
				if (fnum[server][conn][f] != -1) {
					smbcli_close(cli[server][conn]->tree, fnum[server][conn][f]);
					fnum[server][conn][f] = -1;
				}
			}
			talloc_free(cli[server][conn]);
		}
		cli[server][conn] = connect_one(ev, lp_ctx, mem_ctx, share[server], 
						server, conn);
		if (!cli[server][conn]) {
			DEBUG(0,("Failed to connect to %s\n", share[server]));
			exit(1);
		}
	}
}



static bool test_one(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		     int fnum[NSERVERS][NCONNECTIONS][NFILES],
		     struct record *rec)
{
	uint_t conn = rec->conn;
	uint_t f = rec->f;
	uint64_t start = rec->start;
	uint64_t len = rec->len;
	enum brl_type op = rec->lock_type;
	int server;
	bool ret[NSERVERS];
	NTSTATUS status[NSERVERS];

	switch (rec->lock_op) {
	case OP_LOCK:
		/* set a lock */
		for (server=0;server<NSERVERS;server++) {
			NTSTATUS res;
			struct smbcli_tree *tree=cli[server][conn]->tree;
			int fn=fnum[server][conn][f];

			if (!(tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
				res=smbcli_lock(tree, fn, start, len, LOCK_TIMEOUT, rec->lock_op);
			} else {
				union smb_lock parms;
				int ltype;
				struct smb_lock_entry lock[1];

				parms.lockx.level = RAW_LOCK_LOCKX;
				parms.lockx.in.file.fnum = fn;
	
				ltype = (rec->lock_op == READ_LOCK? 1 : 0);
				ltype |= LOCKING_ANDX_LARGE_FILES;
				parms.lockx.in.mode = ltype;
				parms.lockx.in.timeout = LOCK_TIMEOUT;
				parms.lockx.in.ulock_cnt = 0;
				parms.lockx.in.lock_cnt = 1;
				lock[0].pid = rec->pid;
				lock[0].offset = start;
				lock[0].count = len;
				parms.lockx.in.locks = &lock[0];

				res = smb_raw_lock(tree, &parms);
			}

			ret[server] = NT_STATUS_IS_OK(res); 
			status[server] = smbcli_nt_error(cli[server][conn]->tree);
			if (!exact_error_codes && 
			    NT_STATUS_EQUAL(status[server], 
					    NT_STATUS_FILE_LOCK_CONFLICT)) {
				status[server] = NT_STATUS_LOCK_NOT_GRANTED;
			}
		}
		if (showall || !NT_STATUS_EQUAL(status[0],status[1])) {
			printf("lock   conn=%u f=%u range=%.0f(%.0f) op=%s -> %s:%s\n",
			       conn, f, 
			       (double)start, (double)len,
			       op==READ_LOCK?"READ_LOCK":"WRITE_LOCK",
			       nt_errstr(status[0]), nt_errstr(status[1]));
		}
		if (!NT_STATUS_EQUAL(status[0],status[1])) return false;
		break;
		
	case OP_UNLOCK:
		/* unset a lock */
		for (server=0;server<NSERVERS;server++) {
			NTSTATUS res;
			struct smbcli_tree *tree=cli[server][conn]->tree;
			int fn=fnum[server][conn][f];


			if (!(tree->session->transport->negotiate.capabilities & CAP_LARGE_FILES)) {
				res=smbcli_unlock(tree, fn, start, len);
			} else {
				union smb_lock parms;
				struct smb_lock_entry lock[1];

				parms.lockx.level = RAW_LOCK_LOCKX;
				parms.lockx.in.file.fnum = fn;
				parms.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
				parms.lockx.in.timeout = 0;
				parms.lockx.in.ulock_cnt = 1;
				parms.lockx.in.lock_cnt = 0;
				lock[0].pid = rec->pid;
				lock[0].count = len;
				lock[0].offset = start;
				parms.lockx.in.locks = &lock[0];

				res = smb_raw_lock(tree, &parms);
			}

			ret[server] = NT_STATUS_IS_OK(res);
			status[server] = smbcli_nt_error(cli[server][conn]->tree);
		}
		if (showall || 
		    (!hide_unlock_fails && !NT_STATUS_EQUAL(status[0],status[1]))) {
			printf("unlock conn=%u f=%u range=%.0f(%.0f)       -> %s:%s\n",
			       conn, f, 
			       (double)start, (double)len,
			       nt_errstr(status[0]), nt_errstr(status[1]));
		}
		if (!hide_unlock_fails && !NT_STATUS_EQUAL(status[0],status[1])) 
			return false;
		break;

	case OP_REOPEN:
		/* reopen the file */
		for (server=0;server<NSERVERS;server++) {
			smbcli_close(cli[server][conn]->tree, fnum[server][conn][f]);
			fnum[server][conn][f] = -1;
		}
		for (server=0;server<NSERVERS;server++) {
			fnum[server][conn][f] = smbcli_open(cli[server][conn]->tree, FILENAME,
							 O_RDWR|O_CREAT,
							 DENY_NONE);
			if (fnum[server][conn][f] == -1) {
				printf("failed to reopen on share%d\n", server);
				return false;
			}
		}
		if (showall) {
			printf("reopen conn=%u f=%u\n",
			       conn, f);
		}
		break;
	}

	return true;
}

static void close_files(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
			int fnum[NSERVERS][NCONNECTIONS][NFILES])
{
	int server, conn, f; 

	for (server=0;server<NSERVERS;server++)
	for (conn=0;conn<NCONNECTIONS;conn++)
	for (f=0;f<NFILES;f++) {
		if (fnum[server][conn][f] != -1) {
			smbcli_close(cli[server][conn]->tree, fnum[server][conn][f]);
			fnum[server][conn][f] = -1;
		}
	}
	for (server=0;server<NSERVERS;server++) {
		smbcli_unlink(cli[server][0]->tree, FILENAME);
	}
}

static void open_files(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		       int fnum[NSERVERS][NCONNECTIONS][NFILES])
{
	int server, conn, f; 

	for (server=0;server<NSERVERS;server++)
	for (conn=0;conn<NCONNECTIONS;conn++)
	for (f=0;f<NFILES;f++) {
		fnum[server][conn][f] = smbcli_open(cli[server][conn]->tree, FILENAME,
						 O_RDWR|O_CREAT,
						 DENY_NONE);
		if (fnum[server][conn][f] == -1) {
			fprintf(stderr,"Failed to open fnum[%u][%u][%u]\n",
				server, conn, f);
			exit(1);
		}
	}
}


static int retest(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		   int fnum[NSERVERS][NCONNECTIONS][NFILES],
		   int n)
{
	int i;
	printf("testing %u ...\n", n);
	for (i=0; i<n; i++) {
		if (i && i % 100 == 0) {
			printf("%u\n", i);
		}

		if (recorded[i].needed &&
		    !test_one(cli, fnum, &recorded[i])) return i;
	}
	return n;
}


/* each server has two connections open to it. Each connection has two file
   descriptors open on the file - 8 file descriptors in total 

   we then do random locking ops in tamdem on the 4 fnums from each
   server and ensure that the results match
 */
static int test_locks(struct tevent_context *ev,
		      struct loadparm_context *lp_ctx,
			  TALLOC_CTX *mem_ctx,
		      char *share[NSERVERS])
{
	struct smbcli_state *cli[NSERVERS][NCONNECTIONS];
	int fnum[NSERVERS][NCONNECTIONS][NFILES];
	int n, i, n1, skip, r1, r2; 

	ZERO_STRUCT(fnum);
	ZERO_STRUCT(cli);

	recorded = malloc_array_p(struct record, numops);

	for (n=0; n<numops; n++) {
#if PRESETS
		if (n < sizeof(preset) / sizeof(preset[0])) {
			recorded[n] = preset[n];
		} else {
#endif
			recorded[n].conn = random() % NCONNECTIONS;
			recorded[n].f = random() % NFILES;
			recorded[n].start = lock_base + ((uint_t)random() % (lock_range-1));
			recorded[n].len =  min_length +
				random() % (lock_range-(recorded[n].start-lock_base));
			recorded[n].start *= RANGE_MULTIPLE;
			recorded[n].len *= RANGE_MULTIPLE;
			recorded[n].pid = random()%3;
			if (recorded[n].pid == 2) {
				recorded[n].pid = 0xFFFF; /* see if its magic */
			}
			r1 = random() % 100;
			r2 = random() % 100;
			if (r1 < READ_PCT) {
				recorded[n].lock_type = READ_LOCK;
			} else {
				recorded[n].lock_type = WRITE_LOCK;
			}
			if (r2 < LOCK_PCT) {
				recorded[n].lock_op = OP_LOCK;
			} else if (r2 < UNLOCK_PCT) {
				recorded[n].lock_op = OP_UNLOCK;
			} else {
				recorded[n].lock_op = OP_REOPEN;
			}
			recorded[n].needed = true;
			if (!zero_zero && recorded[n].start==0 && recorded[n].len==0) {
				recorded[n].len = 1;
			}
#if PRESETS
		}
#endif
	}

	reconnect(ev, lp_ctx, mem_ctx, cli, fnum, share);
	open_files(cli, fnum);
	n = retest(cli, fnum, numops);

	if (n == numops || !analyze) {
		if (n != numops) {
			return 1;
		}
		return 0;
	}
	n++;

	skip = n/2;

	while (1) {
		n1 = n;

		close_files(cli, fnum);
		reconnect(ev, lp_ctx, mem_ctx, cli, fnum, share);
		open_files(cli, fnum);

		for (i=0;i<n-skip;i+=skip) {
			int m, j;
			printf("excluding %d-%d\n", i, i+skip-1);
			for (j=i;j<i+skip;j++) {
				recorded[j].needed = false;
			}

			close_files(cli, fnum);
			open_files(cli, fnum);

			m = retest(cli, fnum, n);
			if (m == n) {
				for (j=i;j<i+skip;j++) {
					recorded[j].needed = true;
				}
			} else {
				if (i+(skip-1) < m) {
					memmove(&recorded[i], &recorded[i+skip],
						(m-(i+skip-1))*sizeof(recorded[0]));
				}
				n = m-(skip-1);
				i--;
			}
		}

		if (skip > 1) {
			skip = skip/2;
			printf("skip=%d\n", skip);
			continue;
		}

		if (n1 == n) break;
	}

	close_files(cli, fnum);
	reconnect(ev, lp_ctx, mem_ctx, cli, fnum, share);
	open_files(cli, fnum);
	showall = true;
	n1 = retest(cli, fnum, n);
	if (n1 != n-1) {
		printf("ERROR - inconsistent result (%u %u)\n", n1, n);
	}
	close_files(cli, fnum);

	for (i=0;i<n;i++) {
		printf("{%d, %d, %u, %u, %.0f, %.0f, %u},\n",
		       recorded[i].lock_op,
		       recorded[i].lock_type,
		       recorded[i].conn,
		       recorded[i].f,
		       (double)recorded[i].start,
		       (double)recorded[i].len,
		       recorded[i].needed);
	}	

	return 1;
}



static void usage(poptContext pc)
{
	printf("Usage:\n\tlocktest //server1/share1 //server2/share2 [options..]\n");
	poptPrintUsage(pc, stdout, 0);
}

/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	char *share[NSERVERS];
	int opt;
	int seed, server;
	int username_count=0;
	struct tevent_context *ev;
	struct loadparm_context *lp_ctx;
	poptContext pc;
	int argc_new, i;
	char **argv_new;
	enum {OPT_UNCLIST=1000};
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"seed",	  0, POPT_ARG_INT,  &seed, 	0,	"Seed to use for randomizer", 	NULL},
		{"num-ops",	  0, POPT_ARG_INT,  &numops, 	0, 	"num ops",	NULL},
		{"lockrange",     0, POPT_ARG_INT,  &lock_range,0,      "locking range", NULL},
		{"lockbase",      0, POPT_ARG_INT,  &lock_base, 0,      "locking base", NULL},
		{"minlength",     0, POPT_ARG_INT,  &min_length,0,      "min lock length", NULL},
		{"hidefails",     0, POPT_ARG_NONE, &hide_unlock_fails,0,"hide unlock fails", NULL},
		{"oplocks",       0, POPT_ARG_NONE, &use_oplocks,0,      "use oplocks", NULL},
		{"showall",       0, POPT_ARG_NONE, &showall,    0,      "display all operations", NULL},
		{"analyse",       0, POPT_ARG_NONE, &analyze,    0,      "do backtrack analysis", NULL},
		{"zerozero",      0, POPT_ARG_NONE, &zero_zero,    0,      "do zero/zero lock", NULL},
		{"exacterrors",   0, POPT_ARG_NONE, &exact_error_codes,0,"use exact error codes", NULL},
		{"unclist",	  0, POPT_ARG_STRING,	NULL, 	OPT_UNCLIST,	"unclist", 	NULL},
		{ "user", 'U',       POPT_ARG_STRING, NULL, 'U', "Set the network username", "[DOMAIN/]USERNAME[%PASSWORD]" },
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		{ NULL }
	};

	setlinebuf(stdout);
	seed = time(NULL);

	pc = poptGetContext("locktest", argc, (const char **) argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);

	poptSetOtherOptionHelp(pc, "<unc1> <unc2>");

	lp_ctx = cmdline_lp_ctx;
	servers[0] = cli_credentials_init(talloc_autofree_context());
	servers[1] = cli_credentials_init(talloc_autofree_context());
	cli_credentials_guess(servers[0], lp_ctx);
	cli_credentials_guess(servers[1], lp_ctx);

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_UNCLIST:
			lp_set_cmdline(cmdline_lp_ctx, "torture:unclist", poptGetOptArg(pc));
			break;
		case 'U':
			if (username_count == 2) {
				usage(pc);
				exit(1);
			}
			cli_credentials_parse_string(servers[username_count], poptGetOptArg(pc), CRED_SPECIFIED);
			username_count++;
			break;
		}
	}

	argv_new = discard_const_p(char *, poptGetArgs(pc));
	argc_new = argc;
	for (i=0; i<argc; i++) {
		if (argv_new[i] == NULL) {
			argc_new = i;
			break;
		}
	}

	if (!(argc_new >= 3)) {
		usage(pc);
		exit(1);
	}

	setup_logging("locktest", DEBUG_STDOUT);

	for (server=0;server<NSERVERS;server++) {
		share[server] = argv_new[1+server];
		all_string_sub(share[server],"/","\\",0);
	}

	lp_ctx = cmdline_lp_ctx;

	if (username_count == 0) {
		usage(pc);
		return -1;
	}
	if (username_count == 1) {
		servers[1] = servers[0];
	}

	ev = s4_event_context_init(talloc_autofree_context());

	gensec_init(lp_ctx);

	DEBUG(0,("seed=%u base=%d range=%d min_length=%d\n", 
		 seed, lock_base, lock_range, min_length));
	srandom(seed);

	return test_locks(ev, lp_ctx, NULL, share);
}

