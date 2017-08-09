/*
 * Unix SMB/CIFS implementation.
 * thread pool implementation
 * Copyright (C) Volker Lendecke 2009
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/time.h>

#include "pthreadpool.h"
#include "lib/util/dlinklist.h"

struct pthreadpool_job {
	struct pthreadpool_job *next;
	int id;
	void (*fn)(void *private_data);
	void *private_data;
};

struct pthreadpool {
	/*
	 * List pthreadpools for fork safety
	 */
	struct pthreadpool *prev, *next;

	/*
	 * Control access to this struct
	 */
	pthread_mutex_t mutex;

	/*
	 * Threads waiting for work do so here
	 */
	pthread_cond_t condvar;

	/*
	 * List of work jobs
	 */
	struct pthreadpool_job *jobs, *last_job;

	/*
	 * pipe for signalling
	 */
	int sig_pipe[2];

	/*
	 * indicator to worker threads that they should shut down
	 */
	int shutdown;

	/*
	 * maximum number of threads
	 */
	int max_threads;

	/*
	 * Number of threads
	 */
	int num_threads;

	/*
	 * Number of idle threads
	 */
	int num_idle;

	/*
	 * An array of threads that require joining.
	 */
	int			num_exited;
	pthread_t		*exited; /* We alloc more */
};

static pthread_mutex_t pthreadpools_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct pthreadpool *pthreadpools = NULL;
static pthread_once_t pthreadpool_atfork_initialized = PTHREAD_ONCE_INIT;

static void pthreadpool_prep_atfork(void);

/*
 * Initialize a thread pool
 */

int pthreadpool_init(unsigned max_threads, struct pthreadpool **presult)
{
	struct pthreadpool *pool;
	int ret;

	pool = (struct pthreadpool *)malloc(sizeof(struct pthreadpool));
	if (pool == NULL) {
		return ENOMEM;
	}

	ret = pipe(pool->sig_pipe);
	if (ret == -1) {
		int err = errno;
		free(pool);
		return err;
	}

	ret = pthread_mutex_init(&pool->mutex, NULL);
	if (ret != 0) {
		close(pool->sig_pipe[0]);
		close(pool->sig_pipe[1]);
		free(pool);
		return ret;
	}

	ret = pthread_cond_init(&pool->condvar, NULL);
	if (ret != 0) {
		pthread_mutex_destroy(&pool->mutex);
		close(pool->sig_pipe[0]);
		close(pool->sig_pipe[1]);
		free(pool);
		return ret;
	}

	pool->shutdown = 0;
	pool->jobs = pool->last_job = NULL;
	pool->num_threads = 0;
	pool->num_exited = 0;
	pool->exited = NULL;
	pool->max_threads = max_threads;
	pool->num_idle = 0;

	ret = pthread_mutex_lock(&pthreadpools_mutex);
	if (ret != 0) {
		pthread_cond_destroy(&pool->condvar);
		pthread_mutex_destroy(&pool->mutex);
		close(pool->sig_pipe[0]);
		close(pool->sig_pipe[1]);
		free(pool);
		return ret;
	}
	DLIST_ADD(pthreadpools, pool);

	ret = pthread_mutex_unlock(&pthreadpools_mutex);
	assert(ret == 0);

	pthread_once(&pthreadpool_atfork_initialized, pthreadpool_prep_atfork);

	*presult = pool;

	return 0;
}

static void pthreadpool_prepare(void)
{
	int ret;
	struct pthreadpool *pool;

	ret = pthread_mutex_lock(&pthreadpools_mutex);
	assert(ret == 0);

	pool = pthreadpools;

	while (pool != NULL) {
		ret = pthread_mutex_lock(&pool->mutex);
		assert(ret == 0);
		pool = pool->next;
	}
}

static void pthreadpool_parent(void)
{
	int ret;
	struct pthreadpool *pool;

	pool = DLIST_TAIL(pthreadpools);

	while (1) {
		ret = pthread_mutex_unlock(&pool->mutex);
		assert(ret == 0);

		if (pool == pthreadpools) {
			break;
		}
		pool = pool->prev;
	}

	ret = pthread_mutex_unlock(&pthreadpools_mutex);
	assert(ret == 0);
}

static void pthreadpool_child(void)
{
	int ret;
	struct pthreadpool *pool;

	pool = DLIST_TAIL(pthreadpools);

	while (1) {
		close(pool->sig_pipe[0]);
		close(pool->sig_pipe[1]);

		ret = pipe(pool->sig_pipe);
		assert(ret == 0);

		pool->num_threads = 0;

		pool->num_exited = 0;
		free(pool->exited);
		pool->exited = NULL;

		pool->num_idle = 0;

		while (pool->jobs != NULL) {
			struct pthreadpool_job *job;
			job = pool->jobs;
			pool->jobs = job->next;
			free(job);
		}
		pool->last_job = NULL;

		ret = pthread_mutex_unlock(&pool->mutex);
		assert(ret == 0);

		if (pool == pthreadpools) {
			break;
		}
		pool = pool->prev;
	}

	ret = pthread_mutex_unlock(&pthreadpools_mutex);
	assert(ret == 0);
}

static void pthreadpool_prep_atfork(void)
{
	pthread_atfork(pthreadpool_prepare, pthreadpool_parent,
		       pthreadpool_child);
}

/*
 * Return the file descriptor which becomes readable when a job has
 * finished
 */

int pthreadpool_signal_fd(struct pthreadpool *pool)
{
	return pool->sig_pipe[0];
}

/*
 * Do a pthread_join() on all children that have exited, pool->mutex must be
 * locked
 */
static void pthreadpool_join_children(struct pthreadpool *pool)
{
	int i;

	for (i=0; i<pool->num_exited; i++) {
		pthread_join(pool->exited[i], NULL);
	}
	pool->num_exited = 0;

	/*
	 * Deliberately not free and NULL pool->exited. That will be
	 * re-used by realloc later.
	 */
}

/*
 * Fetch a finished job number from the signal pipe
 */

int pthreadpool_finished_job(struct pthreadpool *pool, int *jobid)
{
	int ret_jobid;
	ssize_t nread;

	nread = -1;
	errno = EINTR;

	while ((nread == -1) && (errno == EINTR)) {
		nread = read(pool->sig_pipe[0], &ret_jobid, sizeof(int));
	}
	if (nread == -1) {
		return errno;
	}
	if (nread != sizeof(int)) {
		return EINVAL;
	}
	*jobid = ret_jobid;
	return 0;
}

/*
 * Destroy a thread pool, finishing all threads working for it
 */

int pthreadpool_destroy(struct pthreadpool *pool)
{
	int ret, ret1;

	ret = pthread_mutex_lock(&pool->mutex);
	if (ret != 0) {
		return ret;
	}

	if ((pool->jobs != NULL) || pool->shutdown) {
		ret = pthread_mutex_unlock(&pool->mutex);
		assert(ret == 0);
		return EBUSY;
	}

	if (pool->num_threads > 0) {
		/*
		 * We have active threads, tell them to finish, wait for that.
		 */

		pool->shutdown = 1;

		if (pool->num_idle > 0) {
			/*
			 * Wake the idle threads. They will find pool->quit to
			 * be set and exit themselves
			 */
			ret = pthread_cond_broadcast(&pool->condvar);
			if (ret != 0) {
				pthread_mutex_unlock(&pool->mutex);
				return ret;
			}
		}

		while ((pool->num_threads > 0) || (pool->num_exited > 0)) {

			if (pool->num_exited > 0) {
				pthreadpool_join_children(pool);
				continue;
			}
			/*
			 * A thread that shuts down will also signal
			 * pool->condvar
			 */
			ret = pthread_cond_wait(&pool->condvar, &pool->mutex);
			if (ret != 0) {
				pthread_mutex_unlock(&pool->mutex);
				return ret;
			}
		}
	}

	ret = pthread_mutex_unlock(&pool->mutex);
	if (ret != 0) {
		return ret;
	}
	ret = pthread_mutex_destroy(&pool->mutex);
	ret1 = pthread_cond_destroy(&pool->condvar);

	if (ret != 0) {
		return ret;
	}
	if (ret1 != 0) {
		return ret1;
	}

	ret = pthread_mutex_lock(&pthreadpools_mutex);
	if (ret != 0) {
		return ret;
	}
	DLIST_REMOVE(pthreadpools, pool);
	ret = pthread_mutex_unlock(&pthreadpools_mutex);
	assert(ret == 0);

	close(pool->sig_pipe[0]);
	pool->sig_pipe[0] = -1;

	close(pool->sig_pipe[1]);
	pool->sig_pipe[1] = -1;

	free(pool->exited);
	free(pool);

	return 0;
}

/*
 * Prepare for pthread_exit(), pool->mutex must be locked
 */
static void pthreadpool_server_exit(struct pthreadpool *pool)
{
	pthread_t *exited;

	pool->num_threads -= 1;

	exited = (pthread_t *)realloc(
		pool->exited, sizeof(pthread_t *) * (pool->num_exited + 1));

	if (exited == NULL) {
		/* lost a thread status */
		return;
	}
	pool->exited = exited;

	pool->exited[pool->num_exited] = pthread_self();
	pool->num_exited += 1;
}

static void *pthreadpool_server(void *arg)
{
	struct pthreadpool *pool = (struct pthreadpool *)arg;
	int res;

	res = pthread_mutex_lock(&pool->mutex);
	if (res != 0) {
		return NULL;
	}

	while (1) {
		struct timeval tv;
		struct timespec ts;
		struct pthreadpool_job *job;

		/*
		 * idle-wait at most 1 second. If nothing happens in that
		 * time, exit this thread.
		 */

		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec + 1;
		ts.tv_nsec = tv.tv_usec*1000;

		while ((pool->jobs == NULL) && (pool->shutdown == 0)) {

			pool->num_idle += 1;
			res = pthread_cond_timedwait(
				&pool->condvar, &pool->mutex, &ts);
			pool->num_idle -= 1;

			if (res == ETIMEDOUT) {

				if (pool->jobs == NULL) {
					/*
					 * we timed out and still no work for
					 * us. Exit.
					 */
					pthreadpool_server_exit(pool);
					pthread_mutex_unlock(&pool->mutex);
					return NULL;
				}

				break;
			}
			assert(res == 0);
		}

		job = pool->jobs;

		if (job != NULL) {
			ssize_t written;

			/*
			 * Ok, there's work for us to do, remove the job from
			 * the pthreadpool list
			 */
			pool->jobs = job->next;
			if (pool->last_job == job) {
				pool->last_job = NULL;
			}

			/*
			 * Do the work with the mutex unlocked
			 */

			res = pthread_mutex_unlock(&pool->mutex);
			assert(res == 0);

			job->fn(job->private_data);

			written = write(pool->sig_pipe[1], &job->id,
					sizeof(int));

			free(job);

			res = pthread_mutex_lock(&pool->mutex);
			assert(res == 0);

			if (written != sizeof(int)) {
				pthreadpool_server_exit(pool);
				pthread_mutex_unlock(&pool->mutex);
				return NULL;
			}
		}

		if ((pool->jobs == NULL) && (pool->shutdown != 0)) {
			/*
			 * No more work to do and we're asked to shut down, so
			 * exit
			 */
			pthreadpool_server_exit(pool);

			if (pool->num_threads == 0) {
				/*
				 * Ping the main thread waiting for all of us
				 * workers to have quit.
				 */
				pthread_cond_broadcast(&pool->condvar);
			}

			pthread_mutex_unlock(&pool->mutex);
			return NULL;
		}
	}
}

int pthreadpool_add_job(struct pthreadpool *pool, int job_id,
			void (*fn)(void *private_data), void *private_data)
{
	struct pthreadpool_job *job;
	pthread_t thread_id;
	int res;
	sigset_t mask, omask;

	job = (struct pthreadpool_job *)malloc(sizeof(struct pthreadpool_job));
	if (job == NULL) {
		return ENOMEM;
	}

	job->fn = fn;
	job->private_data = private_data;
	job->id = job_id;
	job->next = NULL;

	res = pthread_mutex_lock(&pool->mutex);
	if (res != 0) {
		free(job);
		return res;
	}

	if (pool->shutdown) {
		/*
		 * Protect against the pool being shut down while
		 * trying to add a job
		 */
		res = pthread_mutex_unlock(&pool->mutex);
		assert(res == 0);
		free(job);
		return EINVAL;
	}

	/*
	 * Just some cleanup under the mutex
	 */
	pthreadpool_join_children(pool);

	/*
	 * Add job to the end of the queue
	 */
	if (pool->jobs == NULL) {
		pool->jobs = job;
	}
	else {
		pool->last_job->next = job;
	}
	pool->last_job = job;

	if (pool->num_idle > 0) {
		/*
		 * We have idle threads, wake one.
		 */
		res = pthread_cond_signal(&pool->condvar);
		pthread_mutex_unlock(&pool->mutex);
		return res;
	}

	if ((pool->max_threads != 0) &&
	    (pool->num_threads >= pool->max_threads)) {
		/*
		 * No more new threads, we just queue the request
		 */
		pthread_mutex_unlock(&pool->mutex);
		return 0;
	}

	/*
	 * Create a new worker thread. It should not receive any signals.
	 */

	sigfillset(&mask);

        res = pthread_sigmask(SIG_BLOCK, &mask, &omask);
	if (res != 0) {
		pthread_mutex_unlock(&pool->mutex);
		return res;
	}

	res = pthread_create(&thread_id, NULL, pthreadpool_server,
				(void *)pool);
	if (res == 0) {
		pool->num_threads += 1;
	}

        assert(pthread_sigmask(SIG_SETMASK, &omask, NULL) == 0);

	pthread_mutex_unlock(&pool->mutex);
	return res;
}
