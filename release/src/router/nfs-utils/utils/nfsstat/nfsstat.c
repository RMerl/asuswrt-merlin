/*
 * nfsstat.c		Output NFS statistics
 *
 * Copyright (C) 1995-2005 Olaf Kirch <okir@suse.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define NFSSRVSTAT	"/proc/net/rpc/nfsd"
#define NFSCLTSTAT	"/proc/net/rpc/nfs"

#define MOUNTSFILE	"/proc/mounts"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define MAXNRVALS	32

static unsigned int	srvproc2info[20], srvproc2info_old[20];	/* NFSv2 call counts ([0] == 18) */
static unsigned int	cltproc2info[20], cltproc2info_old[20];	/* NFSv2 call counts ([0] == 18) */
static unsigned int	srvproc3info[24], srvproc3info_old[24];	/* NFSv3 call counts ([0] == 22) */
static unsigned int	cltproc3info[24], cltproc3info_old[24];	/* NFSv3 call counts ([0] == 22) */
static unsigned int	srvproc4info[4], srvproc4info_old[4];	/* NFSv4 call counts ([0] == 2) */
static unsigned int	cltproc4info[37], cltproc4info_old[37];	/* NFSv4 call counts ([0] == 35) */
static unsigned int	srvproc4opsinfo[42], srvproc4opsinfo_old[42];	/* NFSv4 call counts ([0] == 40) */
static unsigned int	srvnetinfo[5], srvnetinfo_old[5];	/* 0  # of received packets
								 * 1  UDP packets
								 * 2  TCP packets
								 * 3  TCP connections
								 */
static unsigned int	cltnetinfo[5], cltnetinfo_old[5];	/* 0  # of received packets
								 * 1  UDP packets
								 * 2  TCP packets
								 * 3  TCP connections
								 */

static unsigned int	srvrpcinfo[6], srvrpcinfo_old[6];	/* 0  total # of RPC calls
								 * 1  total # of bad calls
								 * 2  bad format
								 * 3  authentication failed
								 * 4  unknown client
								 */
static unsigned int	cltrpcinfo[4], cltrpcinfo_old[4];	/* 0  total # of RPC calls
								 * 1  retransmitted calls
								 * 2  cred refreshs
								 */

static unsigned int	srvrcinfo[9], srvrcinfo_old[9];		/* 0  repcache hits
								 * 1  repcache hits
								 * 2  uncached reqs
								 * (for pre-2.4 kernels:)
								 * 3  FH lookups
								 * 4  'anon' FHs
								 * 5  noncached non-directories
								 * 6  noncached directories
								 * 7  stale
								 */

static unsigned int	srvfhinfo[7], srvfhinfo_old[7];		/* (for kernels >= 2.4.0)
								 * 0  stale
								 * 1  FH lookups
								 * 2  'anon' FHs
								 * 3  noncached directories
								 * 4  noncached non-directories
								 * leave hole to relocate stale for order
								 *    compatability.
								 */

static const char *	nfsv2name[18] = {
	"null", "getattr", "setattr", "root",   "lookup",  "readlink",
	"read", "wrcache", "write",   "create", "remove",  "rename",
	"link", "symlink", "mkdir",   "rmdir",  "readdir", "fsstat"
};

static const char *	nfsv3name[22] = {
	"null",   "getattr", "setattr",  "lookup", "access",  "readlink",
	"read",   "write",   "create",   "mkdir",  "symlink", "mknod",
	"remove", "rmdir",   "rename",   "link",   "readdir", "readdirplus",
	"fsstat", "fsinfo",  "pathconf", "commit"
};

static const char *	nfssrvproc4name[2] = {
	"null",
	"compound",
};

static const char *	nfscltproc4name[35] = {
	"null",      "read",      "write",   "commit",      "open",        "open_conf",
	"open_noat", "open_dgrd", "close",   "setattr",     "fsinfo",      "renew",
	"setclntid", "confirm",   "lock",
	"lockt",     "locku",     "access",  "getattr",     "lookup",      "lookup_root",
	"remove",    "rename",    "link",    "symlink",     "create",      "pathconf",
	"statfs",    "readlink",  "readdir", "server_caps", "delegreturn", "getacl",
	"setacl",    "fs_locations"
};

static const char *     nfssrvproc4opname[40] = {
        "op0-unused",   "op1-unused", "op2-future",  "access",     "close",       "commit",
        "create",       "delegpurge", "delegreturn", "getattr",    "getfh",       "link",
        "lock",         "lockt",      "locku",       "lookup",     "lookup_root", "nverify",
        "open",         "openattr",   "open_conf",   "open_dgrd",  "putfh",       "putpubfh",
        "putrootfh",    "read",       "readdir",     "readlink",   "remove",      "rename",
        "renew",        "restorefh",  "savefh",      "secinfo",    "setattr",     "setcltid",
        "setcltidconf", "verify",     "write",       "rellockowner"
};

#define LABEL_srvnet		"Server packet stats:\n"
#define LABEL_srvrpc		"Server rpc stats:\n"
#define LABEL_srvrc		"Server reply cache:\n"
#define LABEL_srvfh		"Server file handle cache:\n"
#define LABEL_srvproc2		"Server nfs v2:\n"
#define LABEL_srvproc3		"Server nfs v3:\n"
#define LABEL_srvproc4		"Server nfs v4:\n"
#define LABEL_srvproc4ops	"Server nfs v4 operations:\n"
#define LABEL_cltnet		"Client packet stats:\n"
#define LABEL_cltrpc		"Client rpc stats:\n"
#define LABEL_cltproc2		"Client nfs v2:\n"
#define LABEL_cltproc3		"Client nfs v3:\n"
#define LABEL_cltproc4		"Client nfs v4:\n"

typedef struct statinfo {
	char		*tag;
	char		*label;
	int		nrvals;
	unsigned int *	valptr;
} statinfo;

/*
 * We now build the arrays of statinfos using macros, which will make it easier
 * to add new variables for --sleep.  e.g., SRV(net) expands into the struct
 * statinfo:  { "net", "Server packet stats:\n", 5, srvnetinfo }
 */
#define ARRAYSIZE(x)		sizeof(x)/sizeof(*x)
#define STATINFO(k, t, s...)	{ #t, LABEL_##k##t, ARRAYSIZE(k##t##info##s), k##t##info##s }
#define SRV(t, s...)		STATINFO(srv, t, s)
#define CLT(t, s...)		STATINFO(clt, t, s)
#define DECLARE_SRV(n, s...)	static statinfo n##s[] = { \
					SRV(net,s), \
					SRV(rpc,s), \
					SRV(rc,s), \
					SRV(fh,s), \
					SRV(proc2,s), \
					SRV(proc3,s),\
					SRV(proc4,s), \
					SRV(proc4ops,s),\
					{ NULL, NULL, 0, NULL }\
				}
#define DECLARE_CLT(n, s...)  	static statinfo n##s[] = { \
					CLT(net,s), \
					CLT(rpc,s), \
					CLT(proc2,s),\
					CLT(proc3,s), \
					CLT(proc4,s),\
					{ NULL, NULL, 0, NULL }\
				}
DECLARE_SRV(srvinfo);
DECLARE_SRV(srvinfo, _old);
DECLARE_CLT(cltinfo);
DECLARE_CLT(cltinfo, _old);

static void		print_all_stats(int, int, int);
static void		print_server_stats(int, int);
static void		print_client_stats(int, int);
static void		print_stats_list(int, int, int);
static void		print_numbers(const char *, unsigned int *,
					unsigned int);
static void		print_callstats(const char *, const char **,
					unsigned int *, unsigned int);
static void		print_callstats_list(const char *, const char **,
					unsigned int *, unsigned int);
static int		parse_raw_statfile(const char *, struct statinfo *);
static int 		parse_pretty_statfile(const char *, struct statinfo *);

static statinfo		*get_stat_info(const char *, struct statinfo *);

static int		mounts(const char *);

static void		get_stats(const char *, struct statinfo *, int *, int,
					int);
static int		has_stats(const unsigned int *);
static int		has_rpcstats(const unsigned int *, int);
static void 		diff_stats(struct statinfo *, struct statinfo *, int);
static void 		unpause(int);
static void 		update_old_counters(struct statinfo *, struct statinfo *);

static time_t		starttime;

#define PRNT_CALLS	0x0001
#define PRNT_RPC	0x0002
#define PRNT_NET	0x0004
#define PRNT_FH		0x0008
#define PRNT_RC		0x0010
#define PRNT_AUTO	0x1000
#define PRNT_V2		0x2000
#define PRNT_V3		0x4000
#define PRNT_V4		0x8000
#define PRNT_ALL	0x0fff

int versions[] = {
	PRNT_V2,
	PRNT_V3,
	PRNT_V4
};

void usage(char *name)
{
	printf("Usage: %s [OPTION]...\n\
\n\
  -m, --mounts		Show statistics on mounted NFS filesystems\n\
  -c, --client		Show NFS client statistics\n\
  -s, --server		Show NFS server statistics\n\
  -2			Show NFS version 2 statistics\n\
  -3			Show NFS version 3 statistics\n\
  -4			Show NFS version 4 statistics\n\
  -o [facility]		Show statistics on particular facilities.\n\
     nfs		NFS protocol information\n\
     rpc		General RPC information\n\
     net		Network layer statistics\n\
     fh			Usage information on the server's file handle cache\n\
     rc			Usage information on the server's request reply cache\n\
     all		Select all of the above\n\
  -v, --verbose, --all	Same as '-o all'\n\
  -r, --rpc		Show RPC statistics\n\
  -n, --nfs		Show NFS statistics\n\
  -Z[#], --sleep[=#]	Collects stats until interrupted.\n\
			    Cumulative stats are then printed\n\
          		    If # is provided, stats will be output every\n\
			    # seconds.\n\
  -S, --since file	Shows difference between current stats and those in 'file'\n\
  -l, --list		Prints stats in list format\n\
  --version		Show program version\n\
  --help		What you just did\n\
\n", name);
	exit(0);
}

static struct option longopts[] =
{
	{ "acl", 0, 0, 'a' },
	{ "all", 0, 0, 'v' },
	{ "auto", 0, 0, '\3' },
	{ "client", 0, 0, 'c' },
	{ "mounted", 0, 0, 'm' },
	{ "nfs", 0, 0, 'n' },
	{ "rpc", 0, 0, 'r' },
	{ "server", 0, 0, 's' },
	{ "verbose", 0, 0, 'v' },
	{ "zero", 0, 0, 'z' },
	{ "help", 0, 0, '\1' },
	{ "version", 0, 0, '\2' },
	{ "sleep", 2, 0, 'Z' },
	{ "since", 1, 0, 'S' },
	{ "list", 0, 0, 'l' },
	{ NULL, 0, 0, 0 }
};
int opt_sleep;

int
main(int argc, char **argv)
{
	int		opt_all = 0,
			opt_srv = 0,
			opt_clt = 0,
			opt_prt = 0,
			sleep_time = 0,
			opt_list =0,
			opt_since = 0;
	int		c;
	char           *progname,
		       *serverfile = NFSSRVSTAT,
		       *clientfile = NFSCLTSTAT;

	struct statinfo *serverinfo = srvinfo,
			*serverinfo_tmp = srvinfo_old,
			*clientinfo = cltinfo,
			*clientinfo_tmp = cltinfo_old;

	struct sigaction act = {
		.sa_handler = unpause,
		.sa_flags = SA_ONESHOT,
	};

	if ((progname = strrchr(argv[0], '/')))
		progname++;
	else
		progname = argv[0];

	while ((c = getopt_long(argc, argv, "234acmno:Z::S:vrslz\1\2", longopts, NULL)) != EOF) {
		switch (c) {
		case 'a':
			fprintf(stderr, "nfsstat: nfs acls are not yet supported.\n");
			return -1;
		case 'c':
			opt_clt = 1;
			break;
		case 'n':
			opt_prt |= PRNT_CALLS;
			break;
		case 'o':
			if (!strcmp(optarg, "nfs"))
				opt_prt |= PRNT_CALLS;
			else if (!strcmp(optarg, "rpc"))
				opt_prt |= PRNT_RPC;
			else if (!strcmp(optarg, "net"))
				opt_prt |= PRNT_NET;
			else if (!strcmp(optarg, "rc"))
				opt_prt |= PRNT_RC;
			else if (!strcmp(optarg, "fh"))
				opt_prt |= PRNT_FH;
			else if (!strcmp(optarg, "all"))
				opt_prt |= PRNT_CALLS | PRNT_RPC | PRNT_NET | PRNT_RC | PRNT_FH;
			else {
				fprintf(stderr, "nfsstat: unknown category: "
						"%s\n", optarg);
				return 2;
			}
			break;
		case 'Z':
			opt_sleep = 1;
			if (optarg) {
				sleep_time = atoi(optarg);
			}
			break;
		case 'S':
			opt_since = 1;
			serverfile = optarg;
			clientfile = optarg;
			break;
		case '2':
		case '3':
		case '4':
			opt_prt |= versions[c - '2'];
			break;
		case 'v':
			opt_all = 1;
			break;
		case '\3':
			opt_prt |= PRNT_AUTO;
			break;
		case 'r':
			opt_prt |= PRNT_RPC;
			break;
		case 's':
			opt_srv = 1;
			break;
		case 'l':
			opt_list = 1;
			break;
		case 'z':
			fprintf(stderr, "nfsstat: zeroing of nfs statistics "
					"not yet supported\n");
			return 2;
		case 'm':
			return mounts(MOUNTSFILE);
		case '\1':
			usage(progname);
			return 0;
		case '\2':
			fprintf(stdout, "nfsstat: " VERSION "\n");
			return 0;
		default:
			printf("Try `%s --help' for more information.\n", progname);
			return -1;
		}
	}

	if (opt_all) {
		opt_srv = opt_clt = 1;
		opt_prt |= PRNT_ALL;
	}
	if (!(opt_srv + opt_clt))
		opt_srv = opt_clt = 1;
	if (!(opt_prt & 0xfff)) {
		opt_prt |= PRNT_CALLS + PRNT_RPC;
	}
	if (!(opt_prt & 0xe000)) {
		opt_prt |= PRNT_AUTO;
	}
	if ((opt_prt & (PRNT_FH|PRNT_RC)) && !opt_srv) {
		fprintf(stderr,
			"You requested file handle or request cache "
			"statistics while using the -c option.\n"
			"This information is available only for the NFS "
			"server.\n");
	}

	if (opt_since || opt_sleep) {
		serverinfo = srvinfo_old;
		serverinfo_tmp = srvinfo;
		clientinfo = cltinfo_old;
		clientinfo_tmp = cltinfo;
	}

	if (opt_srv)
		get_stats(serverfile, serverinfo, &opt_srv, opt_clt, 1);
	if (opt_clt)
		get_stats(clientfile, clientinfo, &opt_clt, opt_srv, 0);

	if (opt_sleep && !sleep_time) {
		starttime = time(NULL);
		printf("Collecting statistics; press CTRL-C to view results from interval (i.e., from pause to CTRL-C).\n");
		if (sigaction(SIGINT, &act, NULL) != 0) {
			fprintf(stderr, "Error: couldn't register for signal and pause.\n");
			return 1;
		}
		pause();
	}

	if (opt_since || opt_sleep) {
		if (opt_srv) {
			get_stats(NFSSRVSTAT, serverinfo_tmp, &opt_srv, opt_clt, 1);
			diff_stats(serverinfo_tmp, serverinfo, 1);
		}
		if (opt_clt) {
			get_stats(NFSCLTSTAT, clientinfo_tmp, &opt_clt, opt_srv, 0);
			diff_stats(clientinfo_tmp, clientinfo, 0);
		}
	}
	if(sleep_time) {
		while(1) {
			if (opt_srv) {
				get_stats(NFSSRVSTAT, serverinfo_tmp , &opt_srv, opt_clt, 1);
				diff_stats(serverinfo_tmp, serverinfo, 1);
			}
			if (opt_clt) {
				get_stats(NFSCLTSTAT, clientinfo_tmp, &opt_clt, opt_srv, 0);
				diff_stats(clientinfo_tmp, clientinfo, 0);
			}
			if (opt_list) {
				print_stats_list(opt_srv, opt_clt, opt_prt);
			} else {
				print_all_stats(opt_srv, opt_clt, opt_prt);
			}
			fflush(stdout);

			if (opt_srv)
				update_old_counters(serverinfo_tmp, serverinfo);
			if (opt_clt)
				update_old_counters(clientinfo_tmp, clientinfo);

			sleep(sleep_time);
		}	
	} else {
		if (opt_list) {
			print_stats_list(opt_srv, opt_clt, opt_prt);
		} else {
			print_all_stats(opt_srv, opt_clt, opt_prt);
		}
	}

	return 0;
}

static void
print_all_stats (int opt_srv, int opt_clt, int opt_prt)
{
	print_server_stats(opt_srv, opt_prt);
	print_client_stats(opt_clt, opt_prt);
}

static void 
print_server_stats(int opt_srv, int opt_prt) 
{
	if (!opt_srv)
		return;

	if (opt_prt & PRNT_NET) {
		if (opt_sleep && !has_rpcstats(srvnetinfo, 4)) {
		} else {
			print_numbers( LABEL_srvnet
				"packets    udp        tcp        tcpconn\n",
			srvnetinfo, 4);
			printf("\n");
		}
	}
	if (opt_prt & PRNT_RPC) {
		if (opt_sleep && !has_rpcstats(srvrpcinfo, 5)) {
			;
		} else {
			print_numbers(LABEL_srvrpc
				"calls      badcalls   badauth    badclnt    xdrcall\n",
				srvrpcinfo, 5);
			printf("\n");
		}
	}
	if (opt_prt & PRNT_RC) {
		if (opt_sleep && !has_rpcstats(srvrcinfo, 3)) {
			;
		} else {
			print_numbers(LABEL_srvrc
				"hits       misses     nocache\n",
				srvrcinfo, 3);
			printf("\n");
		}
	}

	/*
	 * 2.2 puts all fh-related info after the 'rc' header
	 * 2.4 puts all fh-related info after the 'fh' header, but relocates
	 *     'stale' to the start and swaps dir and nondir :-(  
	 *     We preseve the 2.2 order
	 */
	if (opt_prt & PRNT_FH) {
		if (get_stat_info("fh", srvinfo)) {	/* >= 2.4 */
			int t = srvfhinfo[3];
			srvfhinfo[3]=srvfhinfo[4];
			srvfhinfo[4]=t;
			
			srvfhinfo[5]=srvfhinfo[0]; /* relocate 'stale' */
			
			print_numbers(
				LABEL_srvfh
				"lookup     anon       ncachedir  ncachedir  stale\n",
				srvfhinfo + 1, 5);
		} else					/* < 2.4 */
			print_numbers(
				LABEL_srvfh
				"lookup     anon       ncachedir  ncachedir  stale\n",
				srvrcinfo + 3, 5);
		printf("\n");
	}
	if (opt_prt & PRNT_CALLS) {
		if ((opt_prt & PRNT_V2) || 
				((opt_prt & PRNT_AUTO) && has_stats(srvproc2info))) {
			if (opt_sleep && !has_stats(srvproc2info)) {
				;
			} else {
				print_callstats(LABEL_srvproc2,
					nfsv2name, srvproc2info + 1, 
					sizeof(nfsv2name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V3) || 
				((opt_prt & PRNT_AUTO) && has_stats(srvproc3info))) {
			if (opt_sleep && !has_stats(srvproc3info)) {
				;
			} else {
				print_callstats(LABEL_srvproc3,
					nfsv3name, srvproc3info + 1, 
					sizeof(nfsv3name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V4) || 
				((opt_prt & PRNT_AUTO) && has_stats(srvproc4info))) {
			if (opt_sleep && !has_stats(srvproc4info)) {
				;
			} else {
				print_callstats( LABEL_srvproc4,
					nfssrvproc4name, srvproc4info + 1, 
					sizeof(nfssrvproc4name)/sizeof(char *));
				print_callstats(LABEL_srvproc4ops,
					nfssrvproc4opname, srvproc4opsinfo + 1, 
					sizeof(nfssrvproc4opname)/sizeof(char *));
			}
		}
	}
}
static void
print_client_stats(int opt_clt, int opt_prt) 
{
	if (!opt_clt)
		return;

	if (opt_prt & PRNT_NET) {
		if (opt_sleep && !has_rpcstats(cltnetinfo, 4)) {
			;
		} else { 
			print_numbers(LABEL_cltnet
				"packets    udp        tcp        tcpconn\n",
				cltnetinfo, 4);
			printf("\n");
		}
	}
	if (opt_prt & PRNT_RPC) {
		if (opt_sleep && !has_rpcstats(cltrpcinfo, 3)) {
			;
		} else {
			print_numbers(LABEL_cltrpc
				"calls      retrans    authrefrsh\n",
				cltrpcinfo, 3);
			printf("\n");
		}
	}
	if (opt_prt & PRNT_CALLS) {
		if ((opt_prt & PRNT_V2) || 
				((opt_prt & PRNT_AUTO) && has_stats(cltproc2info))) {
			if (opt_sleep && !has_stats(cltproc2info)) {
				;
			} else {
				print_callstats(LABEL_cltproc2,
					nfsv2name, cltproc2info + 1,  
					sizeof(nfsv2name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V3) || 
				((opt_prt & PRNT_AUTO) && has_stats(cltproc3info))) {
			if (opt_sleep && !has_stats(cltproc3info)) {
				;
			} else {
				print_callstats(LABEL_cltproc3,
					nfsv3name, cltproc3info + 1, 
					sizeof(nfsv3name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V4) || 
				((opt_prt & PRNT_AUTO) && has_stats(cltproc4info))) {
			if (opt_sleep && !has_stats(cltproc4info)) {
				;
			} else {
				print_callstats(LABEL_cltproc4,
					nfscltproc4name, cltproc4info + 1,  
					sizeof(nfscltproc4name)/sizeof(char *));
			}
		}
	}
}

static void
print_clnt_list(int opt_prt) 
{
	if (opt_prt & PRNT_CALLS) {
		if ((opt_prt & PRNT_V2) || 
				((opt_prt & PRNT_AUTO) && has_stats(cltproc2info))) {
			if (opt_sleep && !has_stats(cltproc2info)) {
				;
			} else {
				print_callstats_list("nfs v2 client",
					nfsv2name, cltproc2info + 1,  
					sizeof(nfsv2name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V3) || 
				((opt_prt & PRNT_AUTO) && has_stats(cltproc3info))) {
			if (opt_sleep && !has_stats(cltproc3info)) {
				;
			} else { 
				print_callstats_list("nfs v3 client",
					nfsv3name, cltproc3info + 1, 
					sizeof(nfsv3name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V4) || 
				((opt_prt & PRNT_AUTO) && has_stats(cltproc4info))) {
			if (opt_sleep && !has_stats(cltproc4info)) {
				;
			} else {
				print_callstats_list("nfs v4 ops",
					nfssrvproc4opname, srvproc4opsinfo + 1, 
					sizeof(nfssrvproc4opname)/sizeof(char *));
				print_callstats_list("nfs v4 client",
					nfscltproc4name, cltproc4info + 1,  
					sizeof(nfscltproc4name)/sizeof(char *));
			}
		}
	}
}
static void
print_serv_list(int opt_prt) 
{
	if (opt_prt & PRNT_CALLS) {
		if ((opt_prt & PRNT_V2) || 
				((opt_prt & PRNT_AUTO) && has_stats(srvproc2info))) {
			if (opt_sleep && !has_stats(srvproc2info)) {
				;
			} else {
				print_callstats_list("nfs v2 server",
					nfsv2name, srvproc2info + 1, 
					sizeof(nfsv2name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V3) || 
				((opt_prt & PRNT_AUTO) && has_stats(srvproc3info))) {
			if (opt_sleep && !has_stats(srvproc3info)) {
				;
			} else {
				print_callstats_list("nfs v3 server",
					nfsv3name, srvproc3info + 1, 
					sizeof(nfsv3name)/sizeof(char *));
			}
		}
		if ((opt_prt & PRNT_V4) || 
				((opt_prt & PRNT_AUTO) && has_stats(srvproc4opsinfo))) {
			if (opt_sleep && !has_stats(srvproc4info)) {
				;
			} else {
				print_callstats_list("nfs v4 ops",
					nfssrvproc4opname, srvproc4opsinfo + 1, 
					sizeof(nfssrvproc4opname)/sizeof(char *));
			}
		}
	}
}
static void
print_stats_list(int opt_srv, int opt_clt, int opt_prt) 
{
	if (opt_srv)
		print_serv_list(opt_prt);

	if (opt_clt)
		print_clnt_list(opt_prt);
}

static statinfo *
get_stat_info(const char *sp, struct statinfo *statp)
{
	struct statinfo *ip;

	for (ip = statp; ip->tag; ip++) {
		if (!strcmp(sp, ip->tag))
			return ip;
	}

	return NULL;
}

static void
print_numbers(const char *hdr, unsigned int *info, unsigned int nr)
{
	unsigned int	i;

	fputs(hdr, stdout);
	for (i = 0; i < nr; i++)
		printf("%s%-8u", i? "   " : "", info[i]);
	printf("\n");
}

static void
print_callstats(const char *hdr, const char **names,
				 unsigned int *info, unsigned int nr)
{
	unsigned long long	total;
	unsigned long long	pct;
	int		i, j;

	fputs(hdr, stdout);
	for (i = 0, total = 0; i < nr; i++)
		total += info[i];
	if (!total)
		total = 1;
	for (i = 0; i < nr; i += 6) {
		for (j = 0; j < 6 && i + j < nr; j++)
			printf("%-13s", names[i+j]);
		printf("\n");
		for (j = 0; j < 6 && i + j < nr; j++) {
			pct = ((unsigned long long) info[i+j]*100)/total;
			printf("%-8u%3llu%% ", info[i+j], pct);
		}
		printf("\n");
	}
	printf("\n");
}

static void
print_callstats_list(const char *hdr, const char **names,
		 	unsigned int *callinfo, unsigned int nr)
{
	unsigned long long	calltotal;
	int			i;

	for (i = 0, calltotal = 0; i < nr; i++) {
		calltotal += callinfo[i];
	}
	if (!calltotal)
		return;
	printf("%13s %13s %8llu \n", hdr, "total:", calltotal);
	printf("------------- ------------- --------\n");
	for (i = 0; i < nr; i++) {
			if (callinfo[i])
				printf("%13s %12s: %8u \n", hdr, names[i], callinfo[i]);
	}
	printf("\n");
		
}


/* returns 0 on success, 1 otherwise */
static int
parse_raw_statfile(const char *name, struct statinfo *statp)
{
	char	buffer[4096], *next;
	FILE	*fp;

	/* Being unable to read e.g. the nfsd stats file shouldn't
	 * be a fatal error -- it usually means the module isn't loaded.
	 */
	if ((fp = fopen(name, "r")) == NULL) {
		// fprintf(stderr, "Warning: %s: %m\n", name);
		return 1;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		struct statinfo	*ip;
		char		*sp, *line = buffer;
		unsigned int    i, cnt;
		unsigned int	total = 0;

		if ((next = strchr(line, '\n')) != NULL)
			*next++ = '\0';
		if (!(sp = strtok(line, " \t")))
			continue;

		ip = get_stat_info(sp, statp);
		if (!ip)
			continue;

		cnt = ip->nrvals;

		for (i = 0; i < cnt; i++) {
			if (!(sp = strtok(NULL, " \t")))
				break;
			ip->valptr[i] = (unsigned int) strtoul(sp, NULL, 0);
			total += ip->valptr[i];
		}
		ip->valptr[cnt - 1] = total;
	}

	fclose(fp);
	return 0;
}

/* returns 0 on success, 1 otherwise */
static int
parse_pretty_statfile(const char *filename, struct statinfo *info)
{
	int numvals, curindex, numconsumed, n, err = 1;
	unsigned int sum;
	char buf[4096], *bufp, *fmt, is_proc;
	FILE *fp = NULL;
	struct statinfo *ip;

	if ((fp = fopen(filename, "r")) == NULL)
		//err(2, "Unable to open statfile '%s'.\n", filename);
		goto out;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		for (ip = info; ip->tag; ip++) {
			if (strcmp(buf, ip->label))
				continue;

			sum = 0;
			numvals = ip->nrvals - 1;
			is_proc = strncmp("proc", ip->tag, 4) ? 0 : 1;
			if (is_proc) {
				fmt = " %u %*u%% %n";
				curindex = 1;
				ip->valptr[0] = 0;
			} else {
				fmt = " %u %n";
				curindex = 0;
			}
more_stats:
			/* get (and skip) header */
			if (fgets(buf, sizeof(buf), fp) == NULL) {
				fprintf(stderr, "Failed to locate header after "
						"label for '%s' in %s.\n",
						ip->tag, filename);
				goto out;
			}
			/* no header -- done with this "tag" */
			if (*buf == '\n') {
				ip->valptr[numvals] = sum;
				break;
			}
			/* get stats */
			if (fgets(buf, sizeof(buf), fp) == NULL) {
				fprintf(stderr, "Failed to locate stats after "
						"header for '%s' in %s.\n",
						ip->tag, filename);
				goto out;
			}
			bufp = buf;
			for (; curindex < numvals; curindex++) {
				n = sscanf(bufp, fmt, &ip->valptr[curindex],
						&numconsumed);
				if (n != 1)
					break;
				if (is_proc) {
					ip->valptr[0]++;
					sum++;
				}
				sum += ip->valptr[curindex];
				bufp += numconsumed;
			}
			goto more_stats;
		}
	}
	err = 0;
out:
	if (fp)
		fclose(fp);
	return err;
}

static int
mounts(const char *name)
{
	char	buffer[4096], *next;
	FILE	*fp;

	/* Being unable to read e.g. the nfsd stats file shouldn't
	 * be a fatal error -- it usually means the module isn't loaded.
	 */
	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "Warning: %s: %m\n", name);
		return 0;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		char	      *line = buffer;
		char          *device, *mount, *type, *flags;

		if ((next = strchr(line, '\n')) != NULL)
			*next = '\0';

		if (!(device = strtok(line, " \t")))
			continue;

		if (!(mount = strtok(NULL, " \t")))
			continue;

		if (!(type = strtok(NULL, " \t")))
			continue;

		if (strcmp(type, "nfs") && strcmp(type,"nfs4")) {
		    continue;
		}

		if (!(flags = strtok(NULL, " \t")))
			continue;

		printf("%s from %s\n", mount, device);
		printf(" Flags:\t%s\n", flags);
		printf("\n");

		continue;
	}

	fclose(fp);
	return 1;
}

static void
get_stats(const char *file, struct statinfo *info, int *opt, int other_opt,
		int is_srv)
{
	FILE *fp;
	char buf[10];
	int err = 1;
	char *label = is_srv ? "Server" : "Client";

	/* try to guess what type of stat file we're dealing with */
	if ((fp = fopen(file, "r")) == NULL)
		goto out;
	if (fgets(buf, 10, fp) == NULL)
		goto out;
	if (!strncmp(buf, "net ", 4)) {
		/* looks like raw client stats */
		if (is_srv) {
			fprintf(stderr, "Warning: no server info present in "
					"raw client stats file.\n");
			*opt = 0;
		} else
			err = parse_raw_statfile(file, info);
	} else if (!strncmp(buf, "rc ", 3)) {
		/* looks like raw server stats */
		if (!is_srv) {
			fprintf(stderr, "Warning: no client info present in "
					"raw server stats file.\n");
			*opt = 0;
		} else
			err = parse_raw_statfile(file, info);
	} else
		/* looks like pretty client and server stats */
		err = parse_pretty_statfile(file, info);
out:
	if (fp)
		fclose(fp);
	if (err) {
		if (!other_opt) {
			fprintf(stderr, "Error: No %s Stats (%s: %m). \n",
					label, file);
			exit(2);
		}
		*opt = 0;
	}
}

/*
 * This is for proc2/3/4-type stats, where, in the /proc files, the first entry's value
 * denotes the number of subsequent entries.  statinfo value arrays contain an additional
 * field at the end which contains the sum of all previous elements in the array -- so,
 * there are stats if the sum's greater than the entry-count.
 */
static int
has_stats(const unsigned int *info)
{
	return (info[0] && info[info[0] + 1] > info[0]);
}
static int
has_rpcstats(const unsigned int *info, int size)
{
	int i, cnt;

	for (i=0, cnt=0; i < size; i++)
		cnt += info[i];
	return cnt;
}

/*
 * take the difference of each individual stat value in 'new' and 'old'
 * and store the results back into 'new'
 */
static void
diff_stats(struct statinfo *new, struct statinfo *old, int is_srv)
{
	int i, j, nodiff_first_index, should_diff;

	/*
	 * Different stat types have different formats in the /proc
	 * files: for the proc2/3/4-type stats, the first entry has
	 * the total number of subsequent entries; one does not want
	 * to diff that first entry.  The other stat types aren't like
	 * this.  So, we diff a given entry if it's not of one of the
	 * procX types ("i" < 2 for clt, < 4 for srv), or if it's not
	 * the first entry ("j" > 0).
	 */
	nodiff_first_index = 2 + (2 * is_srv);

	for (i = 0; old[i].tag; i++) {
		for (j = 0; j < new[i].nrvals; j++) {
			should_diff = (i < nodiff_first_index || j > 0);
			if (should_diff)
				new[i].valptr[j] -= old[i].valptr[j];
		}

		/*
		 * Make sure that the "totals" entry (last value in
		 * each stat array) for the procX-type stats has the
		 * "numentries" entry's (first value in procX-type
		 * stat arrays) constant value added-back after the
		 * diff -- i.e., it should always be included in the
		 * total.
		 */
		if (!strncmp("proc", new[i].tag, 4) && old[i].valptr[0])
			new[i].valptr[new[i].nrvals - 1] += new[i].valptr[0];
	}
}

static void
unpause(int sig)
{
	double time_diff;
	int minutes, seconds;
	time_t endtime;

	endtime = time(NULL);
	time_diff = difftime(endtime, starttime);
	minutes = time_diff / 60;
	seconds = (int)time_diff % 60;
	printf("Signal received; displaying (only) statistics gathered over the last %d minutes, %d seconds:\n\n", minutes, seconds);
}

static void
update_old_counters(struct statinfo *new, struct statinfo *old)
{
	int z, i;
	for (z = 0; old[z].tag; z++) 
		for (i = 0; i <= old[z].nrvals; i++) 
			old[z].valptr[i] += new[z].valptr[i];

}
