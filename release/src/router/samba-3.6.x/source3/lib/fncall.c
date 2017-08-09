/*
 * Unix SMB/CIFS implementation.
 * Async fn calls
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

#include "includes.h"
#include "../lib/util/tevent_unix.h"

#if WITH_PTHREADPOOL

#include "lib/pthreadpool/pthreadpool.h"

struct fncall_state {
	struct fncall_context *ctx;
	int job_id;
	bool done;

	void *private_parent;
	void *job_private;
};

struct fncall_context {
	struct pthreadpool *pool;
	int next_job_id;
	int sig_fd;
	struct tevent_req **pending;

	struct fncall_state **orphaned;
	int num_orphaned;

	struct fd_event *fde;
};

static void fncall_handler(struct tevent_context *ev, struct tevent_fd *fde,
			   uint16_t flags, void *private_data);

static int fncall_context_destructor(struct fncall_context *ctx)
{
	while (talloc_array_length(ctx->pending) != 0) {
		/* No TALLOC_FREE here */
		talloc_free(ctx->pending[0]);
	}

	while (ctx->num_orphaned != 0) {
		/*
		 * We've got jobs in the queue for which the tevent_req has
		 * been finished already. Wait for all of them to finish.
		 */
		fncall_handler(NULL, NULL, TEVENT_FD_READ, ctx);
	}

	pthreadpool_destroy(ctx->pool);
	ctx->pool = NULL;

	return 0;
}

struct fncall_context *fncall_context_init(TALLOC_CTX *mem_ctx,
					   int max_threads)
{
	struct fncall_context *ctx;
	int ret;

	ctx = talloc_zero(mem_ctx, struct fncall_context);
	if (ctx == NULL) {
		return NULL;
	}

	ret = pthreadpool_init(max_threads, &ctx->pool);
	if (ret != 0) {
		TALLOC_FREE(ctx);
		return NULL;
	}
	talloc_set_destructor(ctx, fncall_context_destructor);

	ctx->sig_fd = pthreadpool_signal_fd(ctx->pool);
	if (ctx->sig_fd == -1) {
		TALLOC_FREE(ctx);
		return NULL;
	}

	return ctx;
}

static int fncall_next_job_id(struct fncall_context *ctx)
{
	int num_pending = talloc_array_length(ctx->pending);
	int result;

	while (true) {
		int i;

		result = ctx->next_job_id++;
		if (result == 0) {
			continue;
		}

		for (i=0; i<num_pending; i++) {
			struct fncall_state *state = tevent_req_data(
				ctx->pending[i], struct fncall_state);

			if (result == state->job_id) {
				break;
			}
		}
		if (i == num_pending) {
			return result;
		}
	}
}

static void fncall_unset_pending(struct tevent_req *req);
static int fncall_destructor(struct tevent_req *req);

static bool fncall_set_pending(struct tevent_req *req,
			       struct fncall_context *ctx,
			       struct tevent_context *ev)
{
	struct tevent_req **pending;
	int num_pending, orphaned_array_length;

	num_pending = talloc_array_length(ctx->pending);

	pending = talloc_realloc(ctx, ctx->pending, struct tevent_req *,
				 num_pending+1);
	if (pending == NULL) {
		return false;
	}
	pending[num_pending] = req;
	num_pending += 1;
	ctx->pending = pending;
	talloc_set_destructor(req, fncall_destructor);

	/*
	 * Make sure that the orphaned array of fncall_state structs has
	 * enough space. A job can change from pending to orphaned in
	 * fncall_destructor, and to fail in a talloc destructor should be
	 * avoided if possible.
	 */

	orphaned_array_length = talloc_array_length(ctx->orphaned);
	if (num_pending > orphaned_array_length) {
		struct fncall_state **orphaned;

		orphaned = talloc_realloc(ctx, ctx->orphaned,
					  struct fncall_state *,
					  orphaned_array_length + 1);
		if (orphaned == NULL) {
			fncall_unset_pending(req);
			return false;
		}
		ctx->orphaned = orphaned;
	}

	if (ctx->fde != NULL) {
		return true;
	}

	ctx->fde = tevent_add_fd(ev, ctx->pending, ctx->sig_fd, TEVENT_FD_READ,
				 fncall_handler, ctx);
	if (ctx->fde == NULL) {
		fncall_unset_pending(req);
		return false;
	}
	return true;
}

static void fncall_unset_pending(struct tevent_req *req)
{
	struct fncall_state *state = tevent_req_data(req, struct fncall_state);
	struct fncall_context *ctx = state->ctx;
	int num_pending = talloc_array_length(ctx->pending);
	int i;

	if (num_pending == 1) {
		TALLOC_FREE(ctx->fde);
		TALLOC_FREE(ctx->pending);
		return;
	}

	for (i=0; i<num_pending; i++) {
		if (req == ctx->pending[i]) {
			break;
		}
	}
	if (i == num_pending) {
		return;
	}
	if (num_pending > 1) {
		ctx->pending[i] = ctx->pending[num_pending-1];
	}
	ctx->pending = talloc_realloc(NULL, ctx->pending, struct tevent_req *,
				      num_pending - 1);
}

static int fncall_destructor(struct tevent_req *req)
{
	struct fncall_state *state = tevent_req_data(
		req, struct fncall_state);
	struct fncall_context *ctx = state->ctx;

	fncall_unset_pending(req);

	if (state->done) {
		return 0;
	}

	/*
	 * Keep around the state of the deleted request until the request has
	 * finished in the helper thread. fncall_handler will destroy it.
	 */
	ctx->orphaned[ctx->num_orphaned] = talloc_move(ctx->orphaned, &state);
	ctx->num_orphaned += 1;

	return 0;
}

struct tevent_req *fncall_send(TALLOC_CTX *mem_ctx, struct tevent_context *ev,
			       struct fncall_context *ctx,
			       void (*fn)(void *private_data),
			       void *private_data)
{
	struct tevent_req *req;
	struct fncall_state *state;
	int ret;

	req = tevent_req_create(mem_ctx, &state, struct fncall_state);
	if (req == NULL) {
		return NULL;
	}
	state->ctx = ctx;
	state->job_id = fncall_next_job_id(state->ctx);
	state->done = false;

	/*
	 * We need to keep the private data we handed out to the thread around
	 * as long as the job is not finished. This is a bit of an abstraction
	 * violation, because the "req->state1->subreq->state2" (we're
	 * "subreq", "req" is the request our caller creates) is broken to
	 * "ctx->state2->state1", but we are right now in the destructor for
	 * "subreq2", so what can we do. We need to keep state1 around,
	 * otherwise the helper thread will have no place to put its results.
	 */

	state->private_parent = talloc_parent(private_data);
	state->job_private = talloc_move(state, &private_data);

	ret = pthreadpool_add_job(state->ctx->pool, state->job_id, fn,
				  state->job_private);
	if (ret == -1) {
		tevent_req_error(req, errno);
		return tevent_req_post(req, ev);
	}
	if (!fncall_set_pending(req, state->ctx, ev)) {
		tevent_req_nomem(NULL, req);
		return tevent_req_post(req, ev);
	}
	return req;
}

static void fncall_handler(struct tevent_context *ev, struct tevent_fd *fde,
			   uint16_t flags, void *private_data)
{
	struct fncall_context *ctx = talloc_get_type_abort(
		private_data, struct fncall_context);
	int i, num_pending;
	int job_id;

	if (pthreadpool_finished_job(ctx->pool, &job_id) != 0) {
		return;
	}

	num_pending = talloc_array_length(ctx->pending);

	for (i=0; i<num_pending; i++) {
		struct fncall_state *state = tevent_req_data(
			ctx->pending[i], struct fncall_state);

		if (job_id == state->job_id) {
			state->done = true;
			talloc_move(state->private_parent,
				    &state->job_private);
			tevent_req_done(ctx->pending[i]);
			return;
		}
	}

	for (i=0; i<ctx->num_orphaned; i++) {
		if (job_id == ctx->orphaned[i]->job_id) {
			break;
		}
	}
	if (i == ctx->num_orphaned) {
		return;
	}

	TALLOC_FREE(ctx->orphaned[i]);

	if (i < ctx->num_orphaned-1) {
		ctx->orphaned[i] = ctx->orphaned[ctx->num_orphaned-1];
	}
	ctx->num_orphaned -= 1;
}

int fncall_recv(struct tevent_req *req, int *perr)
{
	if (tevent_req_is_unix_error(req, perr)) {
		return -1;
	}
	return 0;
}

#else  /* WITH_PTHREADPOOL */

struct fncall_context {
	uint8_t dummy;
};

struct fncall_context *fncall_context_init(TALLOC_CTX *mem_ctx,
					   int max_threads)
{
	return talloc(mem_ctx, struct fncall_context);
}

struct fncall_state {
	uint8_t dummy;
};

struct tevent_req *fncall_send(TALLOC_CTX *mem_ctx, struct tevent_context *ev,
			       struct fncall_context *ctx,
			       void (*fn)(void *private_data),
			       void *private_data)
{
	struct tevent_req *req;
	struct fncall_state *state;

	req = tevent_req_create(mem_ctx, &state, struct fncall_state);
	if (req == NULL) {
		return NULL;
	}
	fn(private_data);
	tevent_req_post(req, ev);
	return req;
}

int fncall_recv(struct tevent_req *req, int *perr)
{
	return 0;
}

#endif
