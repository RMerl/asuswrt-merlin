/*
   Samba Linux/Unix CIFS implementation

   simple tool to test persistent databases

   Copyright (C) Michael Adam     2009

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
#include "dbwrap.h"
#include "messages.h"

#if 0
#include "lib/events/events.h"
#include "system/filesys.h"
#include "popt.h"
#include "cmdline.h"

#include <sys/time.h>
#include <time.h>
#endif

#define DEFAULT_DB_NAME "transaction.tdb"

static int timelimit = 10;
static int torture_delay = 0;
static int verbose = 0;
static int no_trans = 0;
static char *db_name = (char *)discard_const(DEFAULT_DB_NAME);


static unsigned int pnn;

static TDB_DATA old_data;

static int success = true;

static void print_counters(void)
{
	int i;
	uint32_t *old_counters;

	printf("[%4u] Counters: ", getpid());
	old_counters = (uint32_t *)old_data.dptr;
	for (i=0; i < old_data.dsize/sizeof(uint32_t); i++) {
		printf("%6u ", old_counters[i]);
	}
	printf("\n");
}

static void each_second(struct tevent_context *ev,
			struct tevent_timer *te,
			struct timeval t,
			void *private_data)
{
	struct db_context *db = talloc_get_type(private_data, struct db_context);

	print_counters();

	tevent_add_timer(ev, db, timeval_current_ofs(1, 0), each_second, db);
}

static bool check_counters(struct db_context *db, TDB_DATA data)
{
	int i;
	uint32_t *counters, *old_counters;

	counters     = (uint32_t *)data.dptr;
	old_counters = (uint32_t *)old_data.dptr;

	/* check that all the counters are monotonic increasing */
	for (i=0; i < old_data.dsize/sizeof(uint32_t); i++) {
		if (counters[i] < old_counters[i]) {
			printf("[%4u] ERROR: counters has decreased for node %u  From %u to %u\n",
			       getpid(), i, old_counters[i], counters[i]);
			success = false;
			return false;
		}
	}

	if (old_data.dsize != data.dsize) {
		old_data.dsize = data.dsize;
		old_data.dptr = (unsigned char*)talloc_realloc_size(db, old_data.dptr, old_data.dsize);
	}

	memcpy(old_data.dptr, data.dptr, data.dsize);
	if (verbose) print_counters();

	return true;
}


static void do_sleep(unsigned int sec)
{
	unsigned int i;

	if (sec == 0) {
		return;
	}

	for (i=0; i<sec; i++) {
		if (verbose) printf(".");
		sleep(1);
	}

	if (verbose) printf("\n");
}

static void test_store_records(struct db_context *db, struct tevent_context *ev)
{
	TDB_DATA key;
	uint32_t *counters;
	TALLOC_CTX *tmp_ctx = talloc_stackframe();
	struct timeval start;

	key.dptr = (unsigned char *)discard_const("testkey");
	key.dsize = strlen((const char *)key.dptr)+1;

	start = timeval_current();
	while ((timelimit == 0) || (timeval_elapsed(&start) < timelimit)) {
		struct db_record *rec;
		TDB_DATA data;
		int ret;
		NTSTATUS status;

		if (!no_trans) {
			if (verbose) DEBUG(1, ("starting transaction\n"));
			ret = db->transaction_start(db);
			if (ret != 0) {
				DEBUG(0, ("Failed to start transaction on node "
					  "%d\n", pnn));
				goto fail;
			}
			if (verbose) DEBUG(1, ("transaction started\n"));
			do_sleep(torture_delay);
		}

		if (verbose) DEBUG(1, ("calling fetch_lock\n"));
		rec = db->fetch_locked(db, tmp_ctx, key);
		if (rec == NULL) {
			DEBUG(0, ("Failed to fetch record\n"));
			goto fail;
		}
		if (verbose) DEBUG(1, ("fetched record ok\n"));
		do_sleep(torture_delay);

		data.dsize = MAX(rec->value.dsize, sizeof(uint32_t) * (pnn+1));
		data.dptr = (unsigned char *)talloc_zero_size(tmp_ctx,
							      data.dsize);
		if (data.dptr == NULL) {
			DEBUG(0, ("Failed to allocate data\n"));
			goto fail;
		}
		memcpy(data.dptr, rec->value.dptr,rec->value.dsize);

		counters = (uint32_t *)data.dptr;

		/* bump our counter */
		counters[pnn]++;

		if (verbose) DEBUG(1, ("storing data\n"));
		status = rec->store(rec, data, TDB_REPLACE);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to store record\n"));
			if (!no_trans) {
				ret = db->transaction_cancel(db);
				if (ret != 0) {
					DEBUG(0, ("Error cancelling transaction.\n"));
				}
			}
			goto fail;
		}
		talloc_free(rec);
		if (verbose) DEBUG(1, ("stored data ok\n"));
		do_sleep(torture_delay);

		if (!no_trans) {
			if (verbose) DEBUG(1, ("calling transaction_commit\n"));
			ret = db->transaction_commit(db);
			if (ret != 0) {
				DEBUG(0, ("Failed to commit transaction\n"));
				goto fail;
			}
			if (verbose) DEBUG(1, ("transaction committed\n"));
		}

		/* store the counters and verify that they are sane */
		if (verbose || (pnn == 0)) {
			if (!check_counters(db, data)) {
				goto fail;
			}
		}
		talloc_free(data.dptr);

		do_sleep(torture_delay);
	}

	goto done;

fail:
	success = false;

done:
	talloc_free(tmp_ctx);
	return;
}

/*
  main program
*/
int main(int argc, const char *argv[])
{
	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev_ctx;
	struct messaging_context *msg_ctx;
	struct db_context *db;

	int unsafe_writes = 0;
	struct poptOption popt_options[] = {
		POPT_AUTOHELP
		POPT_COMMON_SAMBA
		{ "timelimit", 't', POPT_ARG_INT, &timelimit, 0, "timelimit", "INTEGER" },
		{ "delay", 'D', POPT_ARG_INT, &torture_delay, 0, "delay (in seconds) between operations", "INTEGER" },
		{ "verbose", 'v', POPT_ARG_NONE,  &verbose, 0, "switch on verbose mode", NULL },
		{ "db-name", 'N', POPT_ARG_STRING, &db_name, 0, "name of the test db", "NAME" },
		{ "no-trans", 'n', POPT_ARG_NONE, &no_trans, 0, "use fetch_lock/record store instead of transactions", NULL },
		{ "unsafe-writes", 'u', POPT_ARG_NONE, &unsafe_writes, 0, "do not use tdb transactions when writing", NULL },
		POPT_TABLEEND
	};
	int opt;
	const char **extra_argv;
	int extra_argc = 0;
	poptContext pc;
	int tdb_flags;

	int ret = 1;

	mem_ctx = talloc_stackframe();

	if (verbose) {
		setbuf(stdout, (char *)NULL); /* don't buffer */
	} else {
		setlinebuf(stdout);
	}

	load_case_tables();

	setup_logging(argv[0], DEBUG_STDERR);
	lp_set_cmdline("log level", "0");

	pc = poptGetContext(argv[0], argc, argv, popt_options, POPT_CONTEXT_KEEP_FIRST);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "Invalid option %s: %s\n",
				poptBadOption(pc, 0), poptStrerror(opt));
			goto done;
		}
	}

	/* setup the remaining options for the main program to use */
	extra_argv = poptGetArgs(pc);
	if (extra_argv) {
		extra_argv++;
		while (extra_argv[extra_argc]) extra_argc++;
	}

	lp_load(get_dyn_CONFIGFILE(), true, false, false, true);

	ev_ctx = tevent_context_init(mem_ctx);
	if (ev_ctx == NULL) {
		d_fprintf(stderr, "ERROR: could not init event context\n");
		goto done;
	}

	msg_ctx = messaging_init(mem_ctx, procid_self(), ev_ctx);
	if (msg_ctx == NULL) {
		d_fprintf(stderr, "ERROR: could not init messaging context\n");
		goto done;
	}

	if (unsafe_writes == 1) {
		tdb_flags = TDB_NOSYNC;
	} else {
		tdb_flags = TDB_DEFAULT;
	}

	if (no_trans) {
		tdb_flags |= TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH;
	}

	db = db_open(mem_ctx, db_name, 0, tdb_flags,  O_RDWR | O_CREAT, 0644);

	if (db == NULL) {
		d_fprintf(stderr, "failed to open db '%s': %s\n", db_name,
			  strerror(errno));
		goto done;
	}

	if (get_my_vnn() == NONCLUSTER_VNN) {
		set_my_vnn(0);
	}
	pnn = get_my_vnn();

	printf("Starting test on node %u. running for %u seconds. "
	       "sleep delay: %u seconds.\n", pnn, timelimit, torture_delay);

	if (!verbose && (pnn == 0)) {
		tevent_add_timer(ev_ctx, db, timeval_current_ofs(1, 0), each_second, db);
	}

	test_store_records(db, ev_ctx);

	if (verbose || (pnn == 0)) {
		if (success != true) {
			printf("The test FAILED\n");
			ret = 2;
		} else {
			printf("SUCCESS!\n");
			ret = 0;
		}
	}

done:
	talloc_free(mem_ctx);
	return ret;
}
