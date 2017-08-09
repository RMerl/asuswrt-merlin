/* 
   Unix SMB/CIFS implementation.

   testing of the events subsystem

   Copyright (C) Stefan Metzmacher 2006-2009

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/events/events.h"
#include "system/filesys.h"
#include "torture/torture.h"

static int fde_count;

static void fde_handler(struct tevent_context *ev_ctx, struct tevent_fd *f, 
			uint16_t flags, void *private_data)
{
	int *fd = (int *)private_data;
	char c;
#ifdef SA_SIGINFO
	kill(getpid(), SIGUSR1);
#endif
	kill(getpid(), SIGALRM);
	read(fd[0], &c, 1);
	write(fd[1], &c, 1);
	fde_count++;
}

static void finished_handler(struct tevent_context *ev_ctx, struct tevent_timer *te,
			     struct timeval tval, void *private_data)
{
	int *finished = (int *)private_data;
	(*finished) = 1;
}

static void count_handler(struct tevent_context *ev_ctx, struct signal_event *te,
			  int signum, int count, void *info, void *private_data)
{
	int *countp = (int *)private_data;
	(*countp) += count;
}

static bool test_event_context(struct torture_context *test,
			       const void *test_data)
{
	struct tevent_context *ev_ctx;
	int fd[2] = { -1, -1 };
	const char *backend = (const char *)test_data;
	int alarm_count=0, info_count=0;
	struct tevent_fd *fde;
#ifdef SA_RESTART
	struct tevent_signal *se1 = NULL;
#endif
	struct tevent_signal *se2 = NULL;
#ifdef SA_SIGINFO
	struct tevent_signal *se3 = NULL;
#endif
	int finished=0;
	struct timeval t;
	char c = 0;

	ev_ctx = event_context_init_byname(test, backend);
	if (ev_ctx == NULL) {
		torture_comment(test, "event backend '%s' not supported\n", backend);
		return true;
	}

	torture_comment(test, "Testing event backend '%s'\n", backend);

	/* reset globals */
	fde_count = 0;

	/* create a pipe */
	pipe(fd);

	fde = event_add_fd(ev_ctx, ev_ctx, fd[0], EVENT_FD_READ,
			   fde_handler, fd);
	tevent_fd_set_auto_close(fde);

	event_add_timed(ev_ctx, ev_ctx, timeval_current_ofs(2,0), 
			finished_handler, &finished);

#ifdef SA_RESTART
	se1 = event_add_signal(ev_ctx, ev_ctx, SIGALRM, SA_RESTART, count_handler, &alarm_count);
#endif
#ifdef SA_RESETHAND
	se2 = event_add_signal(ev_ctx, ev_ctx, SIGALRM, SA_RESETHAND, count_handler, &alarm_count);
#endif
#ifdef SA_SIGINFO
	se3 = event_add_signal(ev_ctx, ev_ctx, SIGUSR1, SA_SIGINFO, count_handler, &info_count);
#endif

	write(fd[1], &c, 1);

	t = timeval_current();
	while (!finished) {
		errno = 0;
		if (event_loop_once(ev_ctx) == -1) {
			talloc_free(ev_ctx);
			torture_fail(test, talloc_asprintf(test, "Failed event loop %s\n", strerror(errno)));
		}
	}

	talloc_free(fde);
	close(fd[1]);

	while (alarm_count < fde_count+1) {
		if (event_loop_once(ev_ctx) == -1) {
			break;
		}
	}

	torture_comment(test, "Got %.2f pipe events/sec\n", fde_count/timeval_elapsed(&t));

#ifdef SA_RESTART
	talloc_free(se1);
#endif

	torture_assert_int_equal(test, alarm_count, 1+fde_count, "alarm count mismatch");

#ifdef SA_SIGINFO
	talloc_free(se3);
	torture_assert_int_equal(test, info_count, fde_count, "info count mismatch");
#endif

	talloc_free(ev_ctx);

	return true;
}

struct torture_suite *torture_local_event(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "event");
	const char **list = event_backend_list(suite);
	int i;

	for (i=0;list && list[i];i++) {
		torture_suite_add_simple_tcase_const(suite, list[i],
					       test_event_context,
					       (const void *)list[i]);
	}

	return suite;
}
