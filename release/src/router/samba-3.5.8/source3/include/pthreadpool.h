/*
 * Unix SMB/CIFS implementation.
 * threadpool implementation based on pthreads
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

#ifndef __PTHREADPOOL_H__
#define __PTHREADPOOL_H__

struct pthreadpool;

int pthreadpool_init(unsigned max_threads, struct pthreadpool **presult);
int pthreadpool_destroy(struct pthreadpool *pool);

/*
 * Add a job to a pthreadpool.
 */
int pthreadpool_add_job(struct pthreadpool *pool, int job_id,
			void (*fn)(void *private_data), void *private_data);

/*
 * Get the signalling fd out of a thread pool. This fd will become readable
 * when a job is finished. The job that finished can be retrieved via
 * pthreadpool_finished_job().
 */
int pthreadpool_sig_fd(struct pthreadpool *pool);
int pthreadpool_finished_job(struct pthreadpool *pool);

#endif
