/*
   SMB tree connection rate test

   Copyright (C) 2006-2007 James Peach

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
#include "libcli/resolve/resolve.h"
#include "torture/smbtorture.h"
#include "lib/cmdline/popt_common.h"
#include "param/param.h"

#include "system/filesys.h"
#include "system/shmem.h"

#define TIME_LIMIT_SECS 30
#define usec_to_sec(s) ((s) / 1000000)

/* Map a shared memory buffer of at least nelem counters. */
static void * map_count_buffer(unsigned nelem, size_t elemsz)
{
	void * buf;
	size_t bufsz;
	size_t pagesz = getpagesize();

	bufsz = nelem * elemsz;
	bufsz = (bufsz + pagesz) % pagesz; /* round up to pagesz */

#ifdef MAP_ANON
	/* BSD */
	buf = mmap(NULL, bufsz, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED,
			-1 /* fd */, 0 /* offset */);
#else
	buf = mmap(NULL, bufsz, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED,
			open("/dev/zero", O_RDWR), 0 /* offset */);
#endif

	if (buf == MAP_FAILED) {
		printf("failed to map count buffer: %s\n",
				strerror(errno));
		return NULL;
	}

	return buf;

}

static int fork_tcon_client(struct torture_context *tctx,
		int *tcon_count, unsigned tcon_timelimit,
		const char *host, const char *share)
{
	pid_t child;
	struct smbcli_state *cli;
	struct timeval end;
	struct timeval now;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lpcfg_smbcli_options(tctx->lp_ctx, &options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &session_options);

	child = fork();
	if (child == -1) {
		printf("failed to fork child: %s\n,", strerror(errno));
		return -1;
	} else if (child != 0) {
		/* Parent, just return. */
		return 0;
	}

	/* Child. Just make as many connections as possible within the
	 * time limit. Don't bother synchronising the child start times
	 * because it's probably not work the effort, and a bit of startup
	 * jitter is probably a more realistic test.
	 */


	end = timeval_current();
	now = timeval_current();
	end.tv_sec += tcon_timelimit;
	*tcon_count = 0;

	while (timeval_compare(&now, &end) == -1) {
		NTSTATUS status;

		status = smbcli_full_connection(NULL, &cli,
				host, lpcfg_smb_ports(tctx->lp_ctx), share,
				NULL, lpcfg_socket_options(tctx->lp_ctx), cmdline_credentials,
				lpcfg_resolve_context(tctx->lp_ctx),
				tctx->ev, &options, &session_options,
				lpcfg_gensec_settings(tctx, tctx->lp_ctx));

		if (!NT_STATUS_IS_OK(status)) {
			printf("failed to connect to //%s/%s: %s\n",
				host, share, nt_errstr(status));
			goto done;
		}

		smbcli_tdis(cli);
		talloc_free(cli);

		*tcon_count = *tcon_count + 1;
		now = timeval_current();
	}

done:
	exit(0);
}

static bool children_remain(void)
{
	bool res;

	/* Reap as many children as possible. */
	for (;;) {
		pid_t ret = waitpid(-1, NULL, WNOHANG);
		if (ret == 0) {
			/* no children ready */
			res = true;
			break;
		}
		if (ret == -1) {
			/* no children left. maybe */
			res = errno != ECHILD;
			break;
		}
	}
	return res;
}

static double rate_convert_secs(unsigned count,
		const struct timeval *start, const struct timeval *end)
{
	return (double)count /
		usec_to_sec((double)usec_time_diff(end, start));
}

/* Test the rate at which the server will accept connections.  */
bool torture_bench_treeconnect(struct torture_context *tctx)
{
	const char *host = torture_setting_string(tctx, "host", NULL);
	const char *share = torture_setting_string(tctx, "share", NULL);

	int timelimit = torture_setting_int(tctx, "timelimit",
					TIME_LIMIT_SECS);
	int nprocs = torture_setting_int(tctx, "nprocs", 4);

	int *curr_counts = map_count_buffer(nprocs, sizeof(int));
	int *last_counts = talloc_array(NULL, int, nprocs);

	struct timeval now, last, start;
	int i, delta;

	torture_assert(tctx, nprocs > 0, "bad proc count");
	torture_assert(tctx, timelimit > 0, "bad timelimit");
	torture_assert(tctx, curr_counts, "allocation failure");
	torture_assert(tctx, last_counts, "allocation failure");

	start = last = timeval_current();
	for (i = 0; i < nprocs; ++i) {
		fork_tcon_client(tctx, &curr_counts[i], timelimit, host, share);
	}

	while (children_remain()) {

		sleep(1);
		now = timeval_current();

		for (i = 0, delta = 0; i < nprocs; ++i) {
			delta += curr_counts[i] - last_counts[i];
		}

		printf("%u connections/sec\n",
			(unsigned)rate_convert_secs(delta, &last, &now));

		memcpy(last_counts, curr_counts, nprocs * sizeof(int));
		last = timeval_current();
	}

	now = timeval_current();

	for (i = 0, delta = 0; i < nprocs; ++i) {
		delta += curr_counts[i];
	}

	printf("TOTAL: %u connections/sec over %u secs\n",
			(unsigned)rate_convert_secs(delta, &start, &now),
			timelimit);
	return true;
}

/* vim: set sts=8 sw=8 : */
