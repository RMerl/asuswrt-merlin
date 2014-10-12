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
#include "lib/cmdline/popt_common.h"
#include "system/filesys.h"
#include "system/dir.h"
#include "libcli/libcli.h"
#include "system/time.h"
#include "auth/credentials/credentials.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"
#include "lib/events/events.h"

static bool showall = false;
static bool old_list = false;
static const char *maskchars = "<>\"?*abc.";
static const char *filechars = "abcdefghijklm.";
static int die_on_error;
static int NumLoops = 0;
static int max_length = 20;
struct masktest_state {
	TALLOC_CTX *mem_ctx;
};

static bool reg_match_one(struct smbcli_state *cli, const char *pattern, const char *file)
{
	/* oh what a weird world this is */
	if (old_list && strcmp(pattern, "*.*") == 0) return true;

	if (ISDOT(pattern)) return false;

	if (ISDOTDOT(file)) file = ".";

	return ms_fnmatch(pattern, file, cli->transport->negotiate.protocol)==0;
}

static char *reg_test(struct smbcli_state *cli, TALLOC_CTX *mem_ctx, const char *pattern, const char *long_name, const char *short_name)
{
	char *ret;
	ret = talloc_strdup(mem_ctx, "---");

	pattern = 1+strrchr_m(pattern,'\\');

	if (reg_match_one(cli, pattern, ".")) ret[0] = '+';
	if (reg_match_one(cli, pattern, "..")) ret[1] = '+';
	if (reg_match_one(cli, pattern, long_name) || 
	    (*short_name && reg_match_one(cli, pattern, short_name))) ret[2] = '+';
	return ret;
}


/***************************************************** 
return a connection to a server
*******************************************************/
static struct smbcli_state *connect_one(struct resolve_context *resolve_ctx, 
					struct tevent_context *ev,
					TALLOC_CTX *mem_ctx,
					char *share, const char **ports,
					const char *socket_options,
					struct smbcli_options *options,
					struct smbcli_session_options *session_options,
					struct gensec_settings *gensec_settings)
{
	struct smbcli_state *c;
	char *server;
	NTSTATUS status;

	server = talloc_strdup(mem_ctx, share+2);
	share = strchr_m(server,'\\');
	if (!share) return NULL;
	*share = 0;
	share++;

	cli_credentials_set_workstation(cmdline_credentials, "masktest", CRED_SPECIFIED);

	status = smbcli_full_connection(NULL, &c,
					server, 
					ports,
					share, NULL,
					socket_options,
					cmdline_credentials, resolve_ctx, ev,
					options, session_options,
					gensec_settings);

	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	return c;
}

static char *resultp;
static struct {
	char *long_name;
	char *short_name;
} last_hit;
static bool f_info_hit;

static void listfn(struct clilist_file_info *f, const char *s, void *state)
{
	struct masktest_state *m = (struct masktest_state *)state;

	if (ISDOT(f->name)) {
		resultp[0] = '+';
	} else if (ISDOTDOT(f->name)) {
		resultp[1] = '+';
	} else {
		resultp[2] = '+';
	}

	last_hit.long_name = talloc_strdup(m->mem_ctx, f->name);
	last_hit.short_name = talloc_strdup(m->mem_ctx, f->short_name);
	f_info_hit = true;
}

static void get_real_name(TALLOC_CTX *mem_ctx, struct smbcli_state *cli,
			  char **long_name, char **short_name)
{
	const char *mask;
	struct masktest_state state;

	if (cli->transport->negotiate.protocol <= PROTOCOL_LANMAN1) {
		mask = "\\masktest\\*.*";
	} else {
		mask = "\\masktest\\*";
	}

	f_info_hit = false;

	state.mem_ctx = mem_ctx;

	smbcli_list_new(cli->tree, mask,
			FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY,
			RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,
			listfn, &state);

	if (f_info_hit) {
		*short_name = talloc_strdup(mem_ctx, last_hit.short_name);
		strlower(*short_name);
		*long_name = talloc_strdup(mem_ctx, last_hit.long_name);
		strlower(*long_name);
	}

	if (*short_name == '\0') {
		*short_name = talloc_strdup(mem_ctx, *long_name);
	}
}

static void testpair(TALLOC_CTX *mem_ctx, struct smbcli_state *cli, char *mask,
		char *file)
{
	int fnum;
	char res1[256];
	char *res2;
	static int count;
	char *short_name = NULL;
	char *long_name = NULL;
	struct masktest_state state;

	count++;

	safe_strcpy(res1, "---", sizeof(res1));

	state.mem_ctx = mem_ctx;

	fnum = smbcli_open(cli->tree, file, O_CREAT|O_TRUNC|O_RDWR, 0);
	if (fnum == -1) {
		DEBUG(0,("Can't create %s\n", file));
		return;
	}
	smbcli_close(cli->tree, fnum);

	resultp = res1;
	short_name = talloc_strdup(mem_ctx, "");
	get_real_name(mem_ctx, cli, &long_name, &short_name);
	safe_strcpy(res1, "---", sizeof(res1));
	smbcli_list_new(cli->tree, mask,
			FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY,
			RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO,
			listfn, &state);

	res2 = reg_test(cli, mem_ctx, mask, long_name, short_name);

	if (showall || strcmp(res1, res2)) {
		d_printf("%s %s %d mask=[%s] file=[%s] rfile=[%s/%s]\n",
			 res1, res2, count, mask, file, long_name, short_name);
		if (die_on_error) exit(1);
	}

	smbcli_unlink(cli->tree, file);

	if (count % 100 == 0) DEBUG(0,("%d\n", count));

	resultp = NULL;
}

static void test_mask(int argc, char *argv[],
					  TALLOC_CTX *mem_ctx,
		      struct smbcli_state *cli)
{
	char *mask, *file;
	int l1, l2, i, l;
	int mc_len = strlen(maskchars);
	int fc_len = strlen(filechars);

	smbcli_mkdir(cli->tree, "\\masktest");

	smbcli_unlink(cli->tree, "\\masktest\\*");

	if (argc >= 2) {
		while (argc >= 2) {
			mask = talloc_strdup(mem_ctx, "\\masktest\\");
			file = talloc_strdup(mem_ctx, "\\masktest\\");
			mask = talloc_strdup_append(mask, argv[0]);
			file = talloc_strdup_append(file, argv[1]);
			testpair(mem_ctx, cli, mask, file);
			argv += 2;
			argc -= 2;
		}
		goto finished;
	}

	while (1) {
		l1 = 1 + random() % max_length;
		l2 = 1 + random() % max_length;
		mask = talloc_strdup(mem_ctx, "\\masktest\\");
		file = talloc_strdup(mem_ctx, "\\masktest\\");
		mask = talloc_realloc_size(mem_ctx, mask, strlen(mask)+l1+1);
		file = talloc_realloc_size(mem_ctx, file, strlen(file)+l2+1);
		l = strlen(mask);
		for (i=0;i<l1;i++) {
			mask[i+l] = maskchars[random() % mc_len];
		}
		mask[l+l1] = 0;

		for (i=0;i<l2;i++) {
			file[i+l] = filechars[random() % fc_len];
		}
		file[l+l2] = 0;

		if (ISDOT(file+l) || ISDOTDOT(file+l) || ISDOTDOT(mask+l)) {
			continue;
		}

		if (strspn(file+l, ".") == strlen(file+l)) continue;

		testpair(mem_ctx, cli, mask, file);
		if (NumLoops && (--NumLoops == 0))
			break;
	}

 finished:
	smbcli_rmdir(cli->tree, "\\masktest");
	talloc_free(mem_ctx);
}


static void usage(poptContext pc)
{
	printf(
"Usage:\n\
  masktest //server/share [options..]\n\
\n\
  This program tests wildcard matching between two servers. It generates\n\
  random pairs of filenames/masks and tests that they match in the same\n\
  way on the servers and internally\n");
	poptPrintUsage(pc, stdout, 0);
}

/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	char *share;
	struct smbcli_state *cli;	
	int opt;
	int seed;
	struct tevent_context *ev;
	struct loadparm_context *lp_ctx;
	struct smbcli_options options;
	struct smbcli_session_options session_options;
	poptContext pc;
	int argc_new, i;
	char **argv_new;
	TALLOC_CTX *mem_ctx;
	enum {OPT_UNCLIST=1000};
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"seed",	  0, POPT_ARG_INT,  &seed, 	0,	"Seed to use for randomizer", 	NULL},
		{"num-ops",	  0, POPT_ARG_INT,  &NumLoops, 	0, 	"num ops",	NULL},
		{"maxlength",	  0, POPT_ARG_INT,  &max_length,0, 	"maximum length",	NULL},
		{"dieonerror",    0, POPT_ARG_NONE, &die_on_error, 0,   "die on errors", NULL},
		{"showall",       0, POPT_ARG_NONE, &showall,    0,      "display all operations", NULL},
		{"oldlist",       0, POPT_ARG_NONE, &old_list,    0,     "use old list call", NULL},
		{"maskchars",	  0, POPT_ARG_STRING,	&maskchars,    0,"mask characters", 	NULL},
		{"filechars",	  0, POPT_ARG_STRING,	&filechars,    0,"file characters", 	NULL},
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

	poptSetOtherOptionHelp(pc, "<unc>");

	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_UNCLIST:
			lpcfg_set_cmdline(cmdline_lp_ctx, "torture:unclist", poptGetOptArg(pc));
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

	if (!(argc_new >= 2)) {
		usage(pc);
		exit(1);
	}

	setup_logging("masktest", DEBUG_STDOUT);

	share = argv_new[1];

	all_string_sub(share,"/","\\",0);

	lp_ctx = cmdline_lp_ctx;

	mem_ctx = talloc_autofree_context();

	ev = s4_event_context_init(mem_ctx);

	gensec_init(lp_ctx);

	lpcfg_smbcli_options(lp_ctx, &options);
	lpcfg_smbcli_session_options(lp_ctx, &session_options);

	cli = connect_one(lpcfg_resolve_context(lp_ctx), ev, mem_ctx, share,
			  lpcfg_smb_ports(lp_ctx), lpcfg_socket_options(lp_ctx),
			  &options, &session_options,
			  lpcfg_gensec_settings(mem_ctx, lp_ctx));
	if (!cli) {
		DEBUG(0,("Failed to connect to %s\n", share));
		exit(1);
	}

	/* need to init seed after connect as clientgen uses random numbers */
	DEBUG(0,("seed=%d     format --- --- (server, correct)\n", seed));
	srandom(seed);

	test_mask(argc_new-1, argv_new+1, mem_ctx, cli);

	return(0);
}
