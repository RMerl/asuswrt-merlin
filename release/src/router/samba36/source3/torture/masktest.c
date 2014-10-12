/* 
   Unix SMB/CIFS implementation.
   mask_match tester
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
#include "system/filesys.h"
#include "trans2.h"
#include "libsmb/libsmb.h"
#include "libsmb/nmblib.h"

static fstring password;
static fstring username;
static int got_pass;
static int max_protocol = PROTOCOL_NT1;
static bool showall = False;
static bool old_list = False;
static const char *maskchars = "<>\"?*abc.";
static const char *filechars = "abcdefghijklm.";
static int verbose;
static int die_on_error;
static int NumLoops = 0;
static int ignore_dot_errors = 0;

extern char *optarg;
extern int optind;

/* a test fn for LANMAN mask support */
static int ms_fnmatch_lanman_core(const char *pattern, const char *string)
{
	const char *p = pattern, *n = string;
	char c;

	if (strcmp(p,"?")==0 && strcmp(n,".")==0) goto match;

	while ((c = *p++)) {
		switch (c) {
		case '.':
			/* if (! *n && ! *p) goto match; */
			if (*n != '.') goto nomatch;
			n++;
			break;

		case '?':
			if ((*n == '.' && n[1] != '.') || ! *n) goto next;
			n++;
			break;

		case '>':
			if (n[0] == '.') {
				if (! n[1] && ms_fnmatch_lanman_core(p, n+1) == 0) goto match;
				if (ms_fnmatch_lanman_core(p, n) == 0) goto match;
				goto nomatch;
			}
			if (! *n) goto next;
			n++;
			break;

		case '*':
			if (! *p) goto match;
			for (; *n; n++) {
				if (ms_fnmatch_lanman_core(p, n) == 0) goto match;
			}
			break;

		case '<':
			for (; *n; n++) {
				if (ms_fnmatch_lanman_core(p, n) == 0) goto match;
				if (*n == '.' && !strchr_m(n+1,'.')) {
					n++;
					break;
				}
			}
			break;

		case '"':
			if (*n == 0 && ms_fnmatch_lanman_core(p, n) == 0) goto match;
			if (*n != '.') goto nomatch;
			n++;
			break;

		default:
			if (c != *n) goto nomatch;
			n++;
		}
	}

	if (! *n) goto match;

 nomatch:
	if (verbose) printf("NOMATCH pattern=[%s] string=[%s]\n", pattern, string);
	return -1;

next:
	if (ms_fnmatch_lanman_core(p, n) == 0) goto match;
        goto nomatch;

 match:
	if (verbose) printf("MATCH   pattern=[%s] string=[%s]\n", pattern, string);
	return 0;
}

static int ms_fnmatch_lanman(const char *pattern, const char *string)
{
	if (!strpbrk(pattern, "?*<>\"")) {
		if (strcmp(string,"..") == 0) 
			string = ".";

		return strcmp(pattern, string);
	}

	if (strcmp(string,"..") == 0 || strcmp(string,".") == 0) {
		return ms_fnmatch_lanman_core(pattern, "..") &&
			ms_fnmatch_lanman_core(pattern, ".");
	}

	return ms_fnmatch_lanman_core(pattern, string);
}

static bool reg_match_one(struct cli_state *cli, const char *pattern, const char *file)
{
	/* oh what a weird world this is */
	if (old_list && strcmp(pattern, "*.*") == 0) return True;

	if (strcmp(pattern,".") == 0) return False;

	if (max_protocol <= PROTOCOL_LANMAN2) {
		return ms_fnmatch_lanman(pattern, file)==0;
	}

	if (strcmp(file,"..") == 0) file = ".";

	return ms_fnmatch(pattern, file, cli->protocol, False) == 0;
}

static char *reg_test(struct cli_state *cli, const char *pattern, const char *long_name, const char *short_name)
{
	static fstring ret;
	const char *new_pattern = 1+strrchr_m(pattern,'\\');

	fstrcpy(ret, "---");
	if (reg_match_one(cli, new_pattern, ".")) ret[0] = '+';
	if (reg_match_one(cli, new_pattern, "..")) ret[1] = '+';
	if (reg_match_one(cli, new_pattern, long_name) ||
	    (*short_name && reg_match_one(cli, new_pattern, short_name))) ret[2] = '+';
	return ret;
}


/***************************************************** 
return a connection to a server
*******************************************************/
static struct cli_state *connect_one(char *share)
{
	struct cli_state *c;
	struct nmb_name called, calling;
	char *server_n;
	char *server;
	struct sockaddr_storage ss;
	NTSTATUS status;

	server = share+2;
	share = strchr_m(server,'\\');
	if (!share) return NULL;
	*share = 0;
	share++;

	server_n = server;

	zero_sockaddr(&ss);

	make_nmb_name(&calling, "masktest", 0x0);
	make_nmb_name(&called , server, 0x20);

 again:
        zero_sockaddr(&ss);

	/* have to open a new connection */
	if (!(c=cli_initialise())) {
		DEBUG(0,("Connection to %s failed\n", server_n));
		return NULL;
	}

	status = cli_connect(c, server_n, &ss);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Connection to %s failed. Error %s\n", server_n, nt_errstr(status) ));
		return NULL;
	}

	c->protocol = max_protocol;

	if (!cli_session_request(c, &calling, &called)) {
		DEBUG(0,("session request to %s failed\n", called.name));
		cli_shutdown(c);
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20);
			goto again;
		}
		return NULL;
	}

	DEBUG(4,(" session request ok\n"));

	status = cli_negprot(c);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("protocol negotiation failed: %s\n",
			  nt_errstr(status)));
		cli_shutdown(c);
		return NULL;
	}

	if (!got_pass) {
		char *pass = getpass("Password: ");
		if (pass) {
			fstrcpy(password, pass);
		}
	}

	status = cli_session_setup(c, username,
				   password, strlen(password),
				   password, strlen(password),
				   lp_workgroup());
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("session setup failed: %s\n", nt_errstr(status)));
		return NULL;
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

	status = cli_tcon_andx(c, share, "?????", password,
			       strlen(password)+1);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("tree connect failed: %s\n", nt_errstr(status)));
		cli_shutdown(c);
		return NULL;
	}

	DEBUG(4,(" tconx ok\n"));

	return c;
}

static char *resultp;

struct rn_state {
	char **pp_long_name;
	char *short_name;
};

static NTSTATUS listfn(const char *mnt, struct file_info *f, const char *s,
		   void *private_data)
{
	struct rn_state *state = (struct rn_state *)private_data;
	if (strcmp(f->name,".") == 0) {
		resultp[0] = '+';
	} else if (strcmp(f->name,"..") == 0) {
		resultp[1] = '+';
	} else {
		resultp[2] = '+';
	}

	if (state == NULL) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (ISDOT(f->name) || ISDOTDOT(f->name))  {
		return NT_STATUS_OK;
	}

	fstrcpy(state->short_name, f->short_name);
	strlower_m(state->short_name);
	*state->pp_long_name = SMB_STRDUP(f->name);
	if (!*state->pp_long_name) {
		return NT_STATUS_NO_MEMORY;
	}
	strlower_m(*state->pp_long_name);
	return NT_STATUS_OK;
}

static void get_real_name(struct cli_state *cli,
			  char **pp_long_name, fstring short_name)
{
	struct rn_state state;

	state.pp_long_name = pp_long_name;
	state.short_name = short_name;

	*pp_long_name = NULL;
	/* nasty hack to force level 260 listings - tridge */
	if (max_protocol <= PROTOCOL_LANMAN1) {
		cli_list_trans(cli, "\\masktest\\*.*", FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY,
			       SMB_FIND_FILE_BOTH_DIRECTORY_INFO, listfn,
			       &state);
	} else {
		cli_list_trans(cli, "\\masktest\\*", FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY,
			       SMB_FIND_FILE_BOTH_DIRECTORY_INFO,
			       listfn, &state);
	}

	if (*short_name == 0) {
		fstrcpy(short_name, *pp_long_name);
	}

#if 0
	if (!strchr_m(short_name,'.')) {
		fstrcat(short_name,".");
	}
#endif
}

static void testpair(struct cli_state *cli, const char *mask, const char *file)
{
	uint16_t fnum;
	fstring res1;
	char *res2;
	static int count;
	fstring short_name;
	char *long_name = NULL;

	count++;

	fstrcpy(res1, "---");

	if (!NT_STATUS_IS_OK(cli_open(cli, file, O_CREAT|O_TRUNC|O_RDWR, 0, &fnum))) {
		DEBUG(0,("Can't create %s\n", file));
		return;
	}
	cli_close(cli, fnum);

	resultp = res1;
	fstrcpy(short_name, "");
	get_real_name(cli, &long_name, short_name);
	if (!long_name) {
		return;
	}
	fstrcpy(res1, "---");
	cli_list(cli, mask, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY, listfn, NULL);

	res2 = reg_test(cli, mask, long_name, short_name);

	if (showall ||
	    ((strcmp(res1, res2) && !ignore_dot_errors) ||
	     (strcmp(res1+2, res2+2) && ignore_dot_errors))) {
		DEBUG(0,("%s %s %d mask=[%s] file=[%s] rfile=[%s/%s]\n",
			 res1, res2, count, mask, file, long_name, short_name));
		if (die_on_error) exit(1);
	}

	cli_unlink(cli, file, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);

	if (count % 100 == 0) DEBUG(0,("%d\n", count));
	SAFE_FREE(long_name);
}

static void test_mask(int argc, char *argv[],
		      struct cli_state *cli)
{
	char *mask, *file;
	int l1, l2, i, l;
	int mc_len = strlen(maskchars);
	int fc_len = strlen(filechars);
	TALLOC_CTX *ctx = talloc_tos();

	cli_mkdir(cli, "\\masktest");

	cli_unlink(cli, "\\masktest\\*", FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);

	if (argc >= 2) {
		while (argc >= 2) {
			mask = talloc_asprintf(ctx,
					"\\masktest\\%s",
					argv[0]);
			file = talloc_asprintf(ctx,
					"\\masktest\\%s",
					argv[1]);
			if (!mask || !file) {
				goto finished;
			}
			testpair(cli, mask, file);
			argv += 2;
			argc -= 2;
		}
		goto finished;
	}

	while (1) {
		l1 = 1 + random() % 20;
		l2 = 1 + random() % 20;
		mask = TALLOC_ARRAY(ctx, char, strlen("\\masktest\\")+1+22);
		file = TALLOC_ARRAY(ctx, char, strlen("\\masktest\\")+1+22);
		if (!mask || !file) {
			goto finished;
		}
		memcpy(mask,"\\masktest\\",strlen("\\masktest\\")+1);
		memcpy(file,"\\masktest\\",strlen("\\masktest\\")+1);
		l = strlen(mask);
		for (i=0;i<l1;i++) {
			mask[i+l] = maskchars[random() % mc_len];
		}
		mask[l+l1] = 0;

		for (i=0;i<l2;i++) {
			file[i+l] = filechars[random() % fc_len];
		}
		file[l+l2] = 0;

		if (strcmp(file+l,".") == 0 || 
		    strcmp(file+l,"..") == 0 ||
		    strcmp(mask+l,"..") == 0) continue;

		if (strspn(file+l, ".") == strlen(file+l)) continue;

		testpair(cli, mask, file);
		if (NumLoops && (--NumLoops == 0))
			break;
		TALLOC_FREE(mask);
		TALLOC_FREE(file);
	}

 finished:
	cli_rmdir(cli, "\\masktest");
}


static void usage(void)
{
	printf(
"Usage:\n\
  masktest //server/share [options..]\n\
  options:\n\
	-d debuglevel\n\
	-n numloops\n\
        -W workgroup\n\
        -U user%%pass\n\
        -s seed\n\
        -M max protocol\n\
        -f filechars (default %s)\n\
        -m maskchars (default %s)\n\
	-v                             verbose mode\n\
	-E                             die on error\n\
        -a                             show all tests\n\
        -i                             ignore . and .. errors\n\
\n\
  This program tests wildcard matching between two servers. It generates\n\
  random pairs of filenames/masks and tests that they match in the same\n\
  way on the servers and internally\n\
", 
  filechars, maskchars);
}

/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	char *share;
	struct cli_state *cli;
	int opt;
	char *p;
	int seed;
	TALLOC_CTX *frame = talloc_stackframe();

	setlinebuf(stdout);

	lp_set_cmdline("log level", "0");

	if (argc < 2 || argv[1][0] == '-') {
		usage();
		exit(1);
	}

	share = argv[1];

	all_string_sub(share,"/","\\",0);

	setup_logging(argv[0], DEBUG_STDERR);

	argc -= 1;
	argv += 1;

	load_case_tables();
	lp_load(get_dyn_CONFIGFILE(),True,False,False,True);
	load_interfaces();

	if (getenv("USER")) {
		fstrcpy(username,getenv("USER"));
	}

	seed = time(NULL);

	while ((opt = getopt(argc, argv, "n:d:U:s:hm:f:aoW:M:vEi")) != EOF) {
		switch (opt) {
		case 'n':
			NumLoops = atoi(optarg);
			break;
		case 'd':
			lp_set_cmdline("log level", optarg);
			break;
		case 'E':
			die_on_error = 1;
			break;
		case 'i':
			ignore_dot_errors = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'M':
			max_protocol = interpret_protocol(optarg, max_protocol);
			break;
		case 'U':
			fstrcpy(username,optarg);
			p = strchr_m(username,'%');
			if (p) {
				*p = 0;
				fstrcpy(password, p+1);
				got_pass = 1;
			}
			break;
		case 's':
			seed = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(1);
		case 'm':
			maskchars = optarg;
			break;
		case 'f':
			filechars = optarg;
			break;
		case 'a':
			showall = 1;
			break;
		case 'o':
			old_list = True;
			break;
		default:
			printf("Unknown option %c (%d)\n", (char)opt, opt);
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;


	cli = connect_one(share);
	if (!cli) {
		DEBUG(0,("Failed to connect to %s\n", share));
		exit(1);
	}

	/* need to init seed after connect as clientgen uses random numbers */
	DEBUG(0,("seed=%d\n", seed));
	srandom(seed);

	test_mask(argc, argv, cli);

	TALLOC_FREE(frame);
	return(0);
}
