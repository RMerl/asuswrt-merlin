/* 
   Unix SMB/CIFS implementation.
   byte range lock tester - with local filesystem support
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
#include "system/passwd.h"
#include "lib/events/events.h"

static fstring password;
static fstring username;
static int got_pass;
static int numops = 1000;
static bool showall;
static bool analyze;
static bool hide_unlock_fails;
static bool use_oplocks;

#define FILENAME "\\locktest.dat"
#define LOCKRANGE 100
#define LOCKBASE 0

/*
#define LOCKBASE (0x40000000 - 50)
*/

#define READ_PCT 50
#define LOCK_PCT 25
#define UNLOCK_PCT 65
#define RANGE_MULTIPLE 1

#define NSERVERS 2
#define NCONNECTIONS 2
#define NUMFSTYPES 2
#define NFILES 2
#define LOCK_TIMEOUT 0

#define FSTYPE_SMB 0
#define FSTYPE_NFS 1

struct record {
	char r1, r2;
	char conn, f, fstype;
	uint_t start, len;
	char needed;
};

static struct record *recorded;

static int try_open(struct smbcli_state *c, char *nfs, int fstype, const char *fname, int flags)
{
	char *path;
	int ret;

	switch (fstype) {
	case FSTYPE_SMB:
		return smbcli_open(c, fname, flags, DENY_NONE);

	case FSTYPE_NFS:
		asprintf(&path, "%s%s", nfs, fname);
		string_replace(path,'\\', '/');
		ret = open(path, flags, 0666);
		SAFE_FREE(Path);
		return ret;
	}

	return -1;
}

static bool try_close(struct smbcli_state *c, int fstype, int fd)
{
	switch (fstype) {
	case FSTYPE_SMB:
		return smbcli_close(c, fd);

	case FSTYPE_NFS:
		return close(fd) == 0;
	}

	return false;
}

static bool try_lock(struct smbcli_state *c, int fstype, 
		     int fd, uint_t start, uint_t len,
		     enum brl_type op)
{
	struct flock lock;

	switch (fstype) {
	case FSTYPE_SMB:
		return smbcli_lock(c, fd, start, len, LOCK_TIMEOUT, op);

	case FSTYPE_NFS:
		lock.l_type = (op==READ_LOCK) ? F_RDLCK:F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = start;
		lock.l_len = len;
		lock.l_pid = getpid();
		return fcntl(fd,F_SETLK,&lock) == 0;
	}

	return false;
}

static bool try_unlock(struct smbcli_state *c, int fstype, 
		       int fd, uint_t start, uint_t len)
{
	struct flock lock;

	switch (fstype) {
	case FSTYPE_SMB:
		return smbcli_unlock(c, fd, start, len);

	case FSTYPE_NFS:
		lock.l_type = F_UNLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = start;
		lock.l_len = len;
		lock.l_pid = getpid();
		return fcntl(fd,F_SETLK,&lock) == 0;
	}

	return false;
}	

/***************************************************** 
return a connection to a server
*******************************************************/
static struct smbcli_state *connect_one(TALLOC_CTX *mem_ctx,
										char *share, const char **ports,
					struct smb_options *options,
					struct smb_options *session_options,
					struct gensec_settings *gensec_settings,
					struct tevent_context *ev)
{
	struct smbcli_state *c;
	char *server_n;
	char *server;
	char *myname;
	static int count;
	NTSTATUS nt_status;

	server = talloc_strdup(mem_ctx, share+2);
	share = strchr_m(server,'\\');
	if (!share) return NULL;
	*share = 0;
	share++;

	server_n = server;
	
	if (!got_pass) {
		char *pass = getpass("Password: ");
		if (pass) {
			password = talloc_strdup(mem_ctx, pass);
		}
	}

	myname = talloc_asprintf(mem_ctx, "lock-%u-%u", getpid(), count++);

	nt_status = smbcli_full_connection(NULL, 
			   &c, myname, server_n, ports, share, NULL,
			   username, lp_workgroup(), password, ev,
			   options, session_options, gensec_settings);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0, ("smbcli_full_connection failed with error %s\n", nt_errstr(nt_status)));
		return NULL;
	}

	c->use_oplocks = use_oplocks;

	return c;
}


static void reconnect(TALLOC_CTX *mem_ctx, 
					  struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		      char *nfs[NSERVERS], 
		      int fnum[NSERVERS][NUMFSTYPES][NCONNECTIONS][NFILES],
		      const char **ports,
		      struct smbcli_options *options,
		      struct smbcli_session_options *session_options,
			  struct gensec_settings *gensec_settings,
		      struct tevent_context *ev,
		      char *share1, char *share2)
{
	int server, conn, f, fstype;
	char *share[2];
	share[0] = share1;
	share[1] = share2;

	fstype = FSTYPE_SMB;

	for (server=0;server<NSERVERS;server++)
	for (conn=0;conn<NCONNECTIONS;conn++) {
		if (cli[server][conn]) {
			for (f=0;f<NFILES;f++) {
				smbcli_close(cli[server][conn], fnum[server][fstype][conn][f]);
			}
			smbcli_ulogoff(cli[server][conn]);
			talloc_free(cli[server][conn]);
		}
		cli[server][conn] = connect_one(mem_ctx, share[server], ports, options, session_options, gensec_settings, ev);
		if (!cli[server][conn]) {
			DEBUG(0,("Failed to connect to %s\n", share[server]));
			exit(1);
		}
	}
}



static bool test_one(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		     char *nfs[NSERVERS],
		     int fnum[NSERVERS][NUMFSTYPES][NCONNECTIONS][NFILES],
		     struct record *rec)
{
	uint_t conn = rec->conn;
	uint_t f = rec->f;
	uint_t fstype = rec->fstype;
	uint_t start = rec->start;
	uint_t len = rec->len;
	uint_t r1 = rec->r1;
	uint_t r2 = rec->r2;
	enum brl_type op;
	int server;
	bool ret[NSERVERS];

	if (r1 < READ_PCT) {
		op = READ_LOCK; 
	} else {
		op = WRITE_LOCK; 
	}

	if (r2 < LOCK_PCT) {
		/* set a lock */
		for (server=0;server<NSERVERS;server++) {
			ret[server] = try_lock(cli[server][conn], fstype,
					       fnum[server][fstype][conn][f],
					       start, len, op);
		}
		if (showall || ret[0] != ret[1]) {
			printf("lock   conn=%u fstype=%u f=%u range=%u:%u(%u) op=%s -> %u:%u\n",
			       conn, fstype, f, 
			       start, start+len-1, len,
			       op==READ_LOCK?"READ_LOCK":"WRITE_LOCK",
			       ret[0], ret[1]);
		}
		if (ret[0] != ret[1]) return false;
	} else if (r2 < LOCK_PCT+UNLOCK_PCT) {
		/* unset a lock */
		for (server=0;server<NSERVERS;server++) {
			ret[server] = try_unlock(cli[server][conn], fstype,
						 fnum[server][fstype][conn][f],
						 start, len);
		}
		if (showall || (!hide_unlock_fails && (ret[0] != ret[1]))) {
			printf("unlock conn=%u fstype=%u f=%u range=%u:%u(%u)       -> %u:%u\n",
			       conn, fstype, f, 
			       start, start+len-1, len,
			       ret[0], ret[1]);
		}
		if (!hide_unlock_fails && ret[0] != ret[1]) return false;
	} else {
		/* reopen the file */
		for (server=0;server<NSERVERS;server++) {
			try_close(cli[server][conn], fstype, fnum[server][fstype][conn][f]);
			fnum[server][fstype][conn][f] = try_open(cli[server][conn], nfs[server], fstype, FILENAME,
								 O_RDWR|O_CREAT);
			if (fnum[server][fstype][conn][f] == -1) {
				printf("failed to reopen on share1\n");
				return false;
			}
		}
		if (showall) {
			printf("reopen conn=%u fstype=%u f=%u\n",
			       conn, fstype, f);
		}
	}
	return true;
}

static void close_files(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
			char *nfs[NSERVERS],
			int fnum[NSERVERS][NUMFSTYPES][NCONNECTIONS][NFILES])
{
	int server, conn, f, fstype; 

	for (server=0;server<NSERVERS;server++)
	for (fstype=0;fstype<NUMFSTYPES;fstype++)
	for (conn=0;conn<NCONNECTIONS;conn++)
	for (f=0;f<NFILES;f++) {
		if (fnum[server][fstype][conn][f] != -1) {
			try_close(cli[server][conn], fstype, fnum[server][fstype][conn][f]);
			fnum[server][fstype][conn][f] = -1;
		}
	}
	for (server=0;server<NSERVERS;server++) {
		smbcli_unlink(cli[server][0], FILENAME);
	}
}

static void open_files(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		       char *nfs[NSERVERS],
		       int fnum[NSERVERS][NUMFSTYPES][NCONNECTIONS][NFILES])
{
	int server, fstype, conn, f; 

	for (server=0;server<NSERVERS;server++)
	for (fstype=0;fstype<NUMFSTYPES;fstype++)
	for (conn=0;conn<NCONNECTIONS;conn++)
	for (f=0;f<NFILES;f++) {
		fnum[server][fstype][conn][f] = try_open(cli[server][conn], nfs[server], fstype, FILENAME,
							 O_RDWR|O_CREAT);
		if (fnum[server][fstype][conn][f] == -1) {
			fprintf(stderr,"Failed to open fnum[%u][%u][%u][%u]\n",
				server, fstype, conn, f);
			exit(1);
		}
	}
}


static int retest(struct smbcli_state *cli[NSERVERS][NCONNECTIONS], 
		  char *nfs[NSERVERS],
		  int fnum[NSERVERS][NUMFSTYPES][NCONNECTIONS][NFILES],
		  int n)
{
	int i;
	printf("testing %u ...\n", n);
	for (i=0; i<n; i++) {
		if (i && i % 100 == 0) {
			printf("%u\n", i);
		}

		if (recorded[i].needed &&
		    !test_one(cli, nfs, fnum, &recorded[i])) return i;
	}
	return n;
}


/* each server has two connections open to it. Each connection has two file
   descriptors open on the file - 8 file descriptors in total 

   we then do random locking ops in tamdem on the 4 fnums from each
   server and ensure that the results match
 */
static void test_locks(TALLOC_CTX *mem_ctx, char *share1, char *share2,
			char *nfspath1, char *nfspath2,
			const char **ports,
			struct smbcli_options *options,
			struct smbcli_session_options *session_options,
			struct gensec_settings *gensec_settings,
			struct tevent_context *ev)
{
	struct smbcli_state *cli[NSERVERS][NCONNECTIONS];
	char *nfs[NSERVERS];
	int fnum[NSERVERS][NUMFSTYPES][NCONNECTIONS][NFILES];
	int n, i, n1; 

	nfs[0] = nfspath1;
	nfs[1] = nfspath2;

	ZERO_STRUCT(fnum);
	ZERO_STRUCT(cli);

	recorded = malloc_array_p(struct record, numops);

	for (n=0; n<numops; n++) {
		recorded[n].conn = random() % NCONNECTIONS;
		recorded[n].fstype = random() % NUMFSTYPES;
		recorded[n].f = random() % NFILES;
		recorded[n].start = LOCKBASE + ((uint_t)random() % (LOCKRANGE-1));
		recorded[n].len = 1 + 
			random() % (LOCKRANGE-(recorded[n].start-LOCKBASE));
		recorded[n].start *= RANGE_MULTIPLE;
		recorded[n].len *= RANGE_MULTIPLE;
		recorded[n].r1 = random() % 100;
		recorded[n].r2 = random() % 100;
		recorded[n].needed = true;
	}

	reconnect(mem_ctx, cli, nfs, fnum, ports, options, session_options, gensec_settings, ev, share1, share2);
	open_files(cli, nfs, fnum);
	n = retest(cli, nfs, fnum, numops);

	if (n == numops || !analyze) return;
	n++;

	while (1) {
		n1 = n;

		close_files(cli, nfs, fnum);
		reconnect(mem_ctx, cli, nfs, fnum, ports, options, session_options, ev, share1, share2);
		open_files(cli, nfs, fnum);

		for (i=0;i<n-1;i++) {
			int m;
			recorded[i].needed = false;

			close_files(cli, nfs, fnum);
			open_files(cli, nfs, fnum);

			m = retest(cli, nfs, fnum, n);
			if (m == n) {
				recorded[i].needed = true;
			} else {
				if (i < m) {
					memmove(&recorded[i], &recorded[i+1],
						(m-i)*sizeof(recorded[0]));
				}
				n = m;
				i--;
			}
		}

		if (n1 == n) break;
	}

	close_files(cli, nfs, fnum);
	reconnect(mem_ctx, cli, nfs, fnum, ports, options, session_options, gensec_settings, ev, share1, share2);
	open_files(cli, nfs, fnum);
	showall = true;
	n1 = retest(cli, nfs, fnum, n);
	if (n1 != n-1) {
		printf("ERROR - inconsistent result (%u %u)\n", n1, n);
	}
	close_files(cli, nfs, fnum);

	for (i=0;i<n;i++) {
		printf("{%u, %u, %u, %u, %u, %u, %u, %u},\n",
		       recorded[i].r1,
		       recorded[i].r2,
		       recorded[i].conn,
		       recorded[i].fstype,
		       recorded[i].f,
		       recorded[i].start,
		       recorded[i].len,
		       recorded[i].needed);
	}	
}



static void usage(void)
{
	printf(
"Usage:\n\
  locktest //server1/share1 //server2/share2 /path1 /path2 [options..]\n\
  options:\n\
        -U user%%pass\n\
        -s seed\n\
        -o numops\n\
        -u          hide unlock fails\n\
        -a          (show all ops)\n\
        -O          use oplocks\n\
");
}

/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	char *share1, *share2, *nfspath1, *nfspath2;
	extern char *optarg;
	extern int optind;
	struct smbcli_options options;
	struct smbcli_session_options session_options;
	TALLOC_CTX *mem_ctx;
	int opt;
	char *p;
	int seed;
	struct loadparm_context *lp_ctx;
	struct tevent_context *ev;

	mem_ctx = talloc_autofree_context();

	setlinebuf(stdout);

	dbf = x_stderr;

	if (argc < 5 || argv[1][0] == '-') {
		usage();
		exit(1);
	}

	share1 = argv[1];
	share2 = argv[2];
	nfspath1 = argv[3];
	nfspath2 = argv[4];

	all_string_sub(share1,"/","\\",0);
	all_string_sub(share2,"/","\\",0);

	setup_logging(argv[0], DEBUG_STDOUT);

	argc -= 4;
	argv += 4;

	lp_ctx = loadparm_init(mem_ctx);
	lp_load(lp_ctx, dyn_CONFIGFILE);

	if (getenv("USER")) {
		username = talloc_strdup(mem_ctx, getenv("USER"));
	}

	seed = time(NULL);

	while ((opt = getopt(argc, argv, "U:s:ho:aAW:O")) != EOF) {
		switch (opt) {
		case 'U':
			username = talloc_strdup(mem_ctx, optarg);
			p = strchr_m(username,'%');
			if (p) {
				*p = 0;
				password = talloc_strdup(mem_ctx, p+1);
				got_pass = 1;
			}
			break;
		case 's':
			seed = atoi(optarg);
			break;
		case 'u':
			hide_unlock_fails = true;
			break;
		case 'o':
			numops = atoi(optarg);
			break;
		case 'O':
			use_oplocks = true;
			break;
		case 'a':
			showall = true;
			break;
		case 'A':
			analyze = true;
			break;
		case 'h':
			usage();
			exit(1);
		default:
			printf("Unknown option %c (%d)\n", (char)opt, opt);
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	DEBUG(0,("seed=%u\n", seed));
	srandom(seed);

	ev = s4_event_context_init(mem_ctx);

	locking_init(1);
	lp_smbcli_options(lp_ctx, &options);
	lp_smbcli_session_options(lp_ctx, &session_options);
	test_locks(mem_ctx, share1, share2, nfspath1, nfspath2, 
			   lp_smb_ports(lp_ctx),
			   &options, &session_options, lp_gensec_settings(lp_ctx), ev);

	return(0);
}
