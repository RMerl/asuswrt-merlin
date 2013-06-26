/* 
   Unix SMB/CIFS implementation.
   SMB torture tester - NBENCH test
   Copyright (C) Andrew Tridgell 1997-2004
   
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
#include "libcli/libcli.h"
#include "torture/util.h"
#include "torture/smbtorture.h"
#include "system/filesys.h"
#include "system/locale.h"

#include "torture/nbench/proto.h"

int nbench_line_count = 0;
static int timelimit = 600;
static int warmup;
static const char *loadfile;
static int read_only;

#define ival(s) strtoll(s, NULL, 0)

static unsigned long nb_max_retries;

#define NB_RETRY(op) \
	for (n=0;n<=nb_max_retries && !op;n++) do_reconnect(&cli, tctx, client)

static void do_reconnect(struct smbcli_state **cli, struct torture_context *tctx, int client)
{
	int n;
	printf("[%d] Reconnecting client %d\n", nbench_line_count, client);
	for (n=0;n<nb_max_retries;n++) {
		if (nb_reconnect(cli, tctx, client)) {
			printf("[%d] Reconnected client %d\n", nbench_line_count, client);
			return;
		}
	}
	printf("[%d] Failed to reconnect client %d\n", nbench_line_count, client);
	nb_exit(1);
}

/* run a test that simulates an approximate netbench client load */
static bool run_netbench(struct torture_context *tctx, struct smbcli_state *cli, int client)
{
	int torture_nprocs = torture_setting_int(tctx, "nprocs", 4);
	int i;
	char line[1024];
	char *cname;
	FILE *f;
	bool correct = true;
	double target_rate = torture_setting_double(tctx, "targetrate", 0);	
	int n = 0;

	if (target_rate != 0 && client == 0) {
		printf("Targetting %.4f MByte/sec\n", target_rate);
	}

	nb_setup(cli, client);

	if (torture_nprocs == 1) {
		if (!read_only) {
			NB_RETRY(torture_setup_dir(cli, "\\clients"));
		}
	}

	asprintf(&cname, "client%d", client+1);

	f = fopen(loadfile, "r");

	if (!f) {
		perror(loadfile);
		return false;
	}

again:
	nbio_time_reset();

	while (fgets(line, sizeof(line)-1, f)) {
		NTSTATUS status;
		const char **params0, **params;

		nbench_line_count++;

		if ((strlen(line) > 0) && line[strlen(line)-1] == '\n') {
			line[strlen(line)-1] = 0;
		}

		all_string_sub(line, "client1", cname, sizeof(line));
		
		params = params0 = const_str_list(
					str_list_make_shell(NULL, line, " "));
		i = str_list_length(params);

		if (i > 0 && isdigit(params[0][0])) {
			double targett = strtod(params[0], NULL);
			if (target_rate != 0) {
				nbio_target_rate(target_rate);
			} else {
				nbio_time_delay(targett);
			}
			params++;
			i--;
		} else if (target_rate != 0) {
			nbio_target_rate(target_rate);
		}

		if (i < 2 || params[0][0] == '#') continue;

		if (!strncmp(params[0],"SMB", 3)) {
			printf("ERROR: You are using a dbench 1 load file\n");
			nb_exit(1);
		}

		if (strncmp(params[i-1], "NT_STATUS_", 10) != 0 &&
		    strncmp(params[i-1], "0x", 2) != 0) {
			printf("Badly formed status at line %d\n", nbench_line_count);
			talloc_free(params);
			continue;
		}

		/* accept numeric or string status codes */
		if (strncmp(params[i-1], "0x", 2) == 0) {
			status = NT_STATUS(strtoul(params[i-1], NULL, 16));
		} else {
			status = nt_status_string_to_code(params[i-1]);
		}

		DEBUG(9,("run_netbench(%d): %s %s\n", client, params[0], params[1]));

		if (!strcmp(params[0],"NTCreateX")) {
			NB_RETRY(nb_createx(params[1], ival(params[2]), ival(params[3]), 
					    ival(params[4]), status));
		} else if (!strcmp(params[0],"Close")) {
			NB_RETRY(nb_close(ival(params[1]), status));
		} else if (!read_only && !strcmp(params[0],"Rename")) {
			NB_RETRY(nb_rename(params[1], params[2], status, n>0));
		} else if (!read_only && !strcmp(params[0],"Unlink")) {
			NB_RETRY(nb_unlink(params[1], ival(params[2]), status, n>0));
		} else if (!read_only && !strcmp(params[0],"Deltree")) {
			NB_RETRY(nb_deltree(params[1], n>0));
		} else if (!read_only && !strcmp(params[0],"Rmdir")) {
			NB_RETRY(nb_rmdir(params[1], status, n>0));
		} else if (!read_only && !strcmp(params[0],"Mkdir")) {
			NB_RETRY(nb_mkdir(params[1], status, n>0));
		} else if (!strcmp(params[0],"QUERY_PATH_INFORMATION")) {
			NB_RETRY(nb_qpathinfo(params[1], ival(params[2]), status));
		} else if (!strcmp(params[0],"QUERY_FILE_INFORMATION")) {
			NB_RETRY(nb_qfileinfo(ival(params[1]), ival(params[2]), status));
		} else if (!strcmp(params[0],"QUERY_FS_INFORMATION")) {
			NB_RETRY(nb_qfsinfo(ival(params[1]), status));
		} else if (!read_only && !strcmp(params[0],"SET_FILE_INFORMATION")) {
			NB_RETRY(nb_sfileinfo(ival(params[1]), ival(params[2]), status));
		} else if (!strcmp(params[0],"FIND_FIRST")) {
			NB_RETRY(nb_findfirst(params[1], ival(params[2]), 
					      ival(params[3]), ival(params[4]), status));
		} else if (!read_only && !strcmp(params[0],"WriteX")) {
			NB_RETRY(nb_writex(ival(params[1]), 
					   ival(params[2]), ival(params[3]), ival(params[4]),
					   status));
		} else if (!read_only && !strcmp(params[0],"Write")) {
			NB_RETRY(nb_write(ival(params[1]), 
					  ival(params[2]), ival(params[3]), ival(params[4]),
					  status));
		} else if (!strcmp(params[0],"LockX")) {
			NB_RETRY(nb_lockx(ival(params[1]), 
					  ival(params[2]), ival(params[3]), status));
		} else if (!strcmp(params[0],"UnlockX")) {
			NB_RETRY(nb_unlockx(ival(params[1]), 
					    ival(params[2]), ival(params[3]), status));
		} else if (!strcmp(params[0],"ReadX")) {
			NB_RETRY(nb_readx(ival(params[1]), 
					  ival(params[2]), ival(params[3]), ival(params[4]),
					  status));
		} else if (!strcmp(params[0],"Flush")) {
			NB_RETRY(nb_flush(ival(params[1]), status));
		} else if (!strcmp(params[0],"Sleep")) {
			nb_sleep(ival(params[1]), status);
		} else {
			printf("[%d] Unknown operation %s\n", nbench_line_count, params[0]);
		}

		if (n > nb_max_retries) {
			printf("Maximum reconnect retries reached for op '%s'\n", params[0]);
			nb_exit(1);
		}

		talloc_free(params0);
		
		if (nb_tick()) goto done;
	}

	rewind(f);
	goto again;

done:
	fclose(f);

	if (!read_only && torture_nprocs == 1) {
		smbcli_deltree(cli->tree, "\\clients");
	}
	if (!torture_close_connection(cli)) {
		correct = false;
	}
	
	return correct;
}


/* run a test that simulates an approximate netbench client load */
bool torture_nbench(struct torture_context *torture)
{
	bool correct = true;
	int torture_nprocs = torture_setting_int(torture, "nprocs", 4);
	struct smbcli_state *cli;
	const char *p;

	read_only = torture_setting_bool(torture, "readonly", false);

	nb_max_retries = torture_setting_int(torture, "nretries", 1);

	p = torture_setting_string(torture, "timelimit", NULL);
	if (p && *p) {
		timelimit = atoi(p);
	}

	warmup = timelimit / 20;

	loadfile = torture_setting_string(torture, "loadfile", NULL);
	if (!loadfile || !*loadfile) {
		loadfile = "client.txt";
	}

	if (torture_nprocs > 1) {
		if (!torture_open_connection(&cli, torture, 0)) {
			return false;
		}

		if (!read_only && !torture_setup_dir(cli, "\\clients")) {
			return false;
		}
	}

	nbio_shmem(torture_nprocs, timelimit, warmup);

	printf("Running for %d seconds with load '%s' and warmup %d secs\n", 
	       timelimit, loadfile, warmup);

	/* we need to reset SIGCHLD here as the name resolution
	   library may have changed it. We rely on correct signals
	   from childs in the main torture code which reaps
	   children. This is why smbtorture BENCH-NBENCH was sometimes
	   failing */
	signal(SIGCHLD, SIG_DFL);


	signal(SIGALRM, nb_alarm);
	alarm(1);
	torture_create_procs(torture, run_netbench, &correct);
	alarm(0);

	if (!read_only && torture_nprocs > 1) {
		smbcli_deltree(cli->tree, "\\clients");
	}

	printf("\nThroughput %g MB/sec\n", nbio_result());
	return correct;
}

NTSTATUS torture_nbench_init(void)
{
	struct torture_suite *suite = torture_suite_create(
						   talloc_autofree_context(), "bench");

	torture_suite_add_simple_test(suite, "nbench", torture_nbench);

	suite->description = talloc_strdup(suite, "Benchmarks");

	torture_register_suite(suite);
	return NT_STATUS_OK;
}
