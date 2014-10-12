#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "pthreadpool.h"

static int test_init(void)
{
	struct pthreadpool *p;
	int ret;

	ret = pthreadpool_init(1, &p);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_init failed: %s\n",
			strerror(ret));
		return -1;
	}
	ret = pthreadpool_destroy(p);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_init failed: %s\n",
			strerror(ret));
		return -1;
	}
	return 0;
}

static void test_sleep(void *ptr)
{
	int *ptimeout = (int *)ptr;
	int ret;
	ret = poll(NULL, 0, *ptimeout);
	if (ret != 0) {
		fprintf(stderr, "poll returned %d (%s)\n",
			ret, strerror(errno));
	}
}

static int test_jobs(int num_threads, int num_jobs)
{
	char *finished;
	struct pthreadpool *p;
	int timeout = 1;
	int i, ret;

	finished = (char *)calloc(1, num_jobs);
	if (finished == NULL) {
		fprintf(stderr, "calloc failed\n");
		return -1;
	}

	ret = pthreadpool_init(num_threads, &p);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_init failed: %s\n",
			strerror(ret));
		return -1;
	}

	for (i=0; i<num_jobs; i++) {
		ret = pthreadpool_add_job(p, i, test_sleep, &timeout);
		if (ret != 0) {
			fprintf(stderr, "pthreadpool_add_job failed: %s\n",
				strerror(ret));
			return -1;
		}
	}

	for (i=0; i<num_jobs; i++) {
		int jobid = -1;
		ret = pthreadpool_finished_job(p, &jobid);
		if ((ret != 0) || (jobid >= num_jobs)) {
			fprintf(stderr, "invalid job number %d\n", jobid);
			return -1;
		}
		finished[jobid] += 1;
	}

	for (i=0; i<num_jobs; i++) {
		if (finished[i] != 1) {
			fprintf(stderr, "finished[%d] = %d\n",
				i, finished[i]);
			return -1;
		}
	}

	ret = pthreadpool_destroy(p);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_destroy failed: %s\n",
			strerror(ret));
		return -1;
	}

	free(finished);
	return 0;
}

static int test_busydestroy(void)
{
	struct pthreadpool *p;
	int timeout = 50;
	struct pollfd pfd;
	int ret;

	ret = pthreadpool_init(1, &p);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_init failed: %s\n",
			strerror(ret));
		return -1;
	}
	ret = pthreadpool_add_job(p, 1, test_sleep, &timeout);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_add_job failed: %s\n",
			strerror(ret));
		return -1;
	}
	ret = pthreadpool_destroy(p);
	if (ret != EBUSY) {
		fprintf(stderr, "Could destroy a busy pool\n");
		return -1;
	}

	pfd.fd = pthreadpool_signal_fd(p);
	pfd.events = POLLIN|POLLERR;

	poll(&pfd, 1, -1);

	ret = pthreadpool_destroy(p);
	if (ret != 0) {
		fprintf(stderr, "pthreadpool_destroy failed: %s\n",
			strerror(ret));
		return -1;
	}
	return 0;
}

struct threaded_state {
	pthread_t tid;
	struct pthreadpool *p;
	int start_job;
	int num_jobs;
	int timeout;
};

static void *test_threaded_worker(void *p)
{
	struct threaded_state *state = (struct threaded_state *)p;
	int i;

	for (i=0; i<state->num_jobs; i++) {
		int ret = pthreadpool_add_job(state->p, state->start_job + i,
					      test_sleep, &state->timeout);
		if (ret != 0) {
			fprintf(stderr, "pthreadpool_add_job failed: %s\n",
				strerror(ret));
			return NULL;
		}
	}
	return NULL;
}

static int test_threaded_addjob(int num_pools, int num_threads, int poolsize,
				int num_jobs)
{
	struct pthreadpool **pools;
	struct threaded_state *states;
	struct threaded_state *state;
	struct pollfd *pfds;
	char *finished;
	pid_t child;
	int i, ret, poolnum;
	int received;

	states = calloc(num_threads, sizeof(struct threaded_state));
	if (states == NULL) {
		fprintf(stderr, "calloc failed\n");
		return -1;
	}

	finished = calloc(num_threads * num_jobs, 1);
	if (finished == NULL) {
		fprintf(stderr, "calloc failed\n");
		return -1;
	}

	pools = calloc(num_pools, sizeof(struct pthreadpool *));
	if (pools == NULL) {
		fprintf(stderr, "calloc failed\n");
		return -1;
	}

	pfds = calloc(num_pools, sizeof(struct pollfd));
	if (pfds == NULL) {
		fprintf(stderr, "calloc failed\n");
		return -1;
	}

	for (i=0; i<num_pools; i++) {
		ret = pthreadpool_init(poolsize, &pools[i]);
		if (ret != 0) {
			fprintf(stderr, "pthreadpool_init failed: %s\n",
				strerror(ret));
			return -1;
		}
		pfds[i].fd = pthreadpool_signal_fd(pools[i]);
		pfds[i].events = POLLIN|POLLHUP;
	}

	poolnum = 0;

	for (i=0; i<num_threads; i++) {
		state = &states[i];

		state->p = pools[poolnum];
		poolnum = (poolnum + 1) % num_pools;

		state->num_jobs = num_jobs;
		state->timeout = 1;
		state->start_job = i * num_jobs;

		ret = pthread_create(&state->tid, NULL, test_threaded_worker,
				     state);
		if (ret != 0) {
			fprintf(stderr, "pthread_create failed: %s\n",
				strerror(ret));
			return -1;
		}
	}

	if (random() % 1) {
		poll(NULL, 0, 1);
	}

	child = fork();
	if (child < 0) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		return -1;
	}
	if (child == 0) {
		for (i=0; i<num_pools; i++) {
			ret = pthreadpool_destroy(pools[i]);
			if (ret != 0) {
				fprintf(stderr, "pthreadpool_destroy failed: "
					"%s\n", strerror(ret));
				exit(1);
			}
		}
		/* child */
		exit(0);
	}

	for (i=0; i<num_threads; i++) {
		ret = pthread_join(states[i].tid, NULL);
		if (ret != 0) {
			fprintf(stderr, "pthread_join(%d) failed: %s\n",
				i, strerror(ret));
			return -1;
		}
	}

	received = 0;

	while (received < num_threads*num_jobs) {
		int j;

		ret = poll(pfds, num_pools, 1000);
		if (ret == -1) {
			fprintf(stderr, "poll failed: %s\n",
				strerror(errno));
			return -1;
		}
		if (ret == 0) {
			fprintf(stderr, "\npoll timed out\n");
			break;
		}

		for (j=0; j<num_pools; j++) {
			int jobid = -1;

			if ((pfds[j].revents & (POLLIN|POLLHUP)) == 0) {
				continue;
			}

			ret = pthreadpool_finished_job(pools[j], &jobid);
			if ((ret != 0) || (jobid >= num_jobs * num_threads)) {
				fprintf(stderr, "invalid job number %d\n",
					jobid);
				return -1;
			}
			finished[jobid] += 1;
			received += 1;
		}
	}

	for (i=0; i<num_threads*num_jobs; i++) {
		if (finished[i] != 1) {
			fprintf(stderr, "finished[%d] = %d\n",
				i, finished[i]);
			return -1;
		}
	}

	for (i=0; i<num_pools; i++) {
		ret = pthreadpool_destroy(pools[i]);
		if (ret != 0) {
			fprintf(stderr, "pthreadpool_destroy failed: %s\n",
				strerror(ret));
			return -1;
		}
	}

	free(pfds);
	free(pools);
	free(states);
	free(finished);

	return 0;
}

int main(void)
{
	int ret;

	ret = test_init();
	if (ret != 0) {
		fprintf(stderr, "test_init failed\n");
		return 1;
	}

	ret = test_jobs(10, 10000);
	if (ret != 0) {
		fprintf(stderr, "test_jobs failed\n");
		return 1;
	}

	ret = test_busydestroy();
	if (ret != 0) {
		fprintf(stderr, "test_busydestroy failed\n");
		return 1;
	}

	/*
	 * Test 10 threads adding jobs on a single pool
	 */
	ret = test_threaded_addjob(1, 10, 5, 5000);
	if (ret != 0) {
		fprintf(stderr, "test_jobs failed\n");
		return 1;
	}

	/*
	 * Test 10 threads on 3 pools to verify our fork handling
	 * works right.
	 */
	ret = test_threaded_addjob(3, 10, 5, 5000);
	if (ret != 0) {
		fprintf(stderr, "test_jobs failed\n");
		return 1;
	}

	printf("success\n");
	return 0;
}
