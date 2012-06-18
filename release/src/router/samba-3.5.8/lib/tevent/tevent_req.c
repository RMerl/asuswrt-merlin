/*
   Unix SMB/CIFS implementation.
   Infrastructure for async requests
   Copyright (C) Volker Lendecke 2008
   Copyright (C) Stefan Metzmacher 2009

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

#include "replace.h"
#include "tevent.h"
#include "tevent_internal.h"
#include "tevent_util.h"

/**
 * @brief The default print function for creating debug messages
 * @param[in] req	The request to be printed
 * @param[in] mem_ctx	The memory context for the result
 * @retval		Text representation of req
 *
 * The function should not be used by users of the asynx API,
 * but custom print function can use it and append custom text
 * to the string.
 */

char *tevent_req_default_print(struct tevent_req *req, TALLOC_CTX *mem_ctx)
{
	return talloc_asprintf(mem_ctx,
			       "tevent_req[%p/%s]: state[%d] error[%lld (0x%llX)] "
			       " state[%s (%p)] timer[%p]",
			       req, req->internal.create_location,
			       req->internal.state,
			       (unsigned long long)req->internal.error,
			       (unsigned long long)req->internal.error,
			       talloc_get_name(req->data),
			       req->data,
			       req->internal.timer
			       );
}

/**
 * @brief Print an tevent_req structure in debug messages
 * @param[in] mem_ctx	The memory context for the result
 * @param[in] req	The request to be printed
 * @retval		Text representation of req
 *
 * This function should be used by callers of the async API
 */

char *tevent_req_print(TALLOC_CTX *mem_ctx, struct tevent_req *req)
{
	if (!req->private_print) {
		return tevent_req_default_print(req, mem_ctx);
	}

	return req->private_print(req, mem_ctx);
}

/**
 * @brief Create an async request
 * @param[in] mem_ctx	The memory context for the result
 * @param[in] ev	The event context this async request will be driven by
 * @retval		A new async request
 *
 * The new async request will be initialized in state ASYNC_REQ_IN_PROGRESS
 */

struct tevent_req *_tevent_req_create(TALLOC_CTX *mem_ctx,
				    void *pdata,
				    size_t data_size,
				    const char *type,
				    const char *location)
{
	struct tevent_req *req;
	void **ppdata = (void **)pdata;
	void *data;

	req = talloc_zero(mem_ctx, struct tevent_req);
	if (req == NULL) {
		return NULL;
	}
	req->internal.private_type	= type;
	req->internal.create_location	= location;
	req->internal.finish_location	= NULL;
	req->internal.state		= TEVENT_REQ_IN_PROGRESS;
	req->internal.trigger		= tevent_create_immediate(req);
	if (!req->internal.trigger) {
		talloc_free(req);
		return NULL;
	}

	data = talloc_zero_size(req, data_size);
	if (data == NULL) {
		talloc_free(req);
		return NULL;
	}
	talloc_set_name_const(data, type);

	req->data = data;

	*ppdata = data;
	return req;
}

void _tevent_req_notify_callback(struct tevent_req *req, const char *location)
{
	req->internal.finish_location = location;
	if (req->async.fn != NULL) {
		req->async.fn(req);
	}
}

static void tevent_req_finish(struct tevent_req *req,
			      enum tevent_req_state state,
			      const char *location)
{
	req->internal.state = state;
	_tevent_req_notify_callback(req, location);
}

/**
 * @brief An async request has successfully finished
 * @param[in] req	The finished request
 *
 * tevent_req_done is to be used by implementors of async requests. When a
 * request is successfully finished, this function calls the user's completion
 * function.
 */

void _tevent_req_done(struct tevent_req *req,
		      const char *location)
{
	tevent_req_finish(req, TEVENT_REQ_DONE, location);
}

/**
 * @brief An async request has seen an error
 * @param[in] req	The request with an error
 * @param[in] error	The error code
 *
 * tevent_req_done is to be used by implementors of async requests. When a
 * request can not successfully completed, the implementation should call this
 * function with the appropriate status code.
 *
 * If error is 0 the function returns false and does nothing more.
 *
 * Call pattern would be
 * \code
 * int error = first_function();
 * if (tevent_req_error(req, error)) {
 *	return;
 * }
 *
 * error = second_function();
 * if (tevent_req_error(req, error)) {
 *	return;
 * }
 *
 * tevent_req_done(req);
 * return;
 * \endcode
 */

bool _tevent_req_error(struct tevent_req *req,
		       uint64_t error,
		       const char *location)
{
	if (error == 0) {
		return false;
	}

	req->internal.error = error;
	tevent_req_finish(req, TEVENT_REQ_USER_ERROR, location);
	return true;
}

/**
 * @brief Helper function for nomem check
 * @param[in] p		The pointer to be checked
 * @param[in] req	The request being processed
 *
 * Convenience helper to easily check alloc failure within a callback
 * implementing the next step of an async request.
 *
 * Call pattern would be
 * \code
 * p = talloc(mem_ctx, bla);
 * if (tevent_req_nomem(p, req)) {
 *	return;
 * }
 * \endcode
 */

bool _tevent_req_nomem(const void *p,
		       struct tevent_req *req,
		       const char *location)
{
	if (p != NULL) {
		return false;
	}
	tevent_req_finish(req, TEVENT_REQ_NO_MEMORY, location);
	return true;
}

/**
 * @brief Immediate event callback
 * @param[in] ev	Event context
 * @param[in] im	The immediate event
 * @param[in] priv	The async request to be finished
 */
static void tevent_req_trigger(struct tevent_context *ev,
			       struct tevent_immediate *im,
			       void *private_data)
{
	struct tevent_req *req = talloc_get_type(private_data,
				 struct tevent_req);

	tevent_req_finish(req, req->internal.state,
			  req->internal.finish_location);
}

/**
 * @brief Finish a request before the caller had the change to set the callback
 * @param[in] req	The finished request
 * @param[in] ev	The tevent_context for the timed event
 * @retval		req will be returned
 *
 * An implementation of an async request might find that it can either finish
 * the request without waiting for an external event, or it can't even start
 * the engine. To present the illusion of a callback to the user of the API,
 * the implementation can call this helper function which triggers an
 * immediate timed event. This way the caller can use the same calling
 * conventions, independent of whether the request was actually deferred.
 */

struct tevent_req *tevent_req_post(struct tevent_req *req,
				   struct tevent_context *ev)
{
	tevent_schedule_immediate(req->internal.trigger,
				  ev, tevent_req_trigger, req);
	return req;
}

/**
 * @brief This function destroys the attached private data
 * @param[in] req	The request to poll
 * @retval		The boolean form of "is in progress".
 *
 * This function can be used to ask if the given request
 * is still in progress.
 *
 * This function is typically used by sync wrapper functions.
 */
bool tevent_req_is_in_progress(struct tevent_req *req)
{
	if (req->internal.state == TEVENT_REQ_IN_PROGRESS) {
		return true;
	}

	return false;
}

/**
 * @brief This function destroys the attached private data
 * @param[in] req	The finished request
 *
 * This function can be called as last action of a _recv()
 * function, it destroys the data attached to the tevent_req.
 */
void tevent_req_received(struct tevent_req *req)
{
	TALLOC_FREE(req->data);
	req->private_print = NULL;

	TALLOC_FREE(req->internal.trigger);
	TALLOC_FREE(req->internal.timer);

	req->internal.state = TEVENT_REQ_RECEIVED;
}

/**
 * @brief This function destroys the attached private data
 * @param[in] req	The request to poll
 * @param[in] ev	The tevent_context to be used
 * @retval		If a critical error has happened in the
 * 			tevent loop layer false is returned.
 * 			Otherwise true is returned.
 * 			This is not the return value of the given request!
 *
 * This function can be used to actively poll for the
 * given request to finish.
 *
 * Note: this should only be used if the given tevent context
 *       was created by the caller, to avoid event loop nesting.
 *
 * This function is typically used by sync wrapper functions.
 */
bool tevent_req_poll(struct tevent_req *req,
		     struct tevent_context *ev)
{
	while (tevent_req_is_in_progress(req)) {
		int ret;

		ret = tevent_loop_once(ev);
		if (ret != 0) {
			return false;
		}
	}

	return true;
}

bool tevent_req_is_error(struct tevent_req *req, enum tevent_req_state *state,
			uint64_t *error)
{
	if (req->internal.state == TEVENT_REQ_DONE) {
		return false;
	}
	if (req->internal.state == TEVENT_REQ_USER_ERROR) {
		*error = req->internal.error;
	}
	*state = req->internal.state;
	return true;
}

static void tevent_req_timedout(struct tevent_context *ev,
			       struct tevent_timer *te,
			       struct timeval now,
			       void *private_data)
{
	struct tevent_req *req = talloc_get_type(private_data,
				 struct tevent_req);

	TALLOC_FREE(req->internal.timer);

	tevent_req_finish(req, TEVENT_REQ_TIMED_OUT, __FUNCTION__);
}

bool tevent_req_set_endtime(struct tevent_req *req,
			    struct tevent_context *ev,
			    struct timeval endtime)
{
	TALLOC_FREE(req->internal.timer);

	req->internal.timer = tevent_add_timer(ev, req, endtime,
					       tevent_req_timedout,
					       req);
	if (tevent_req_nomem(req->internal.timer, req)) {
		return false;
	}

	return true;
}

void tevent_req_set_callback(struct tevent_req *req, tevent_req_fn fn, void *pvt)
{
	req->async.fn = fn;
	req->async.private_data = pvt;
}

void *_tevent_req_callback_data(struct tevent_req *req)
{
	return req->async.private_data;
}

void *_tevent_req_data(struct tevent_req *req)
{
	return req->data;
}

/**
 * @brief This function sets a print function for the given request
 * @param[in] req	The given request
 * @param[in] fn	A pointer to the print function
 *
 * This function can be used to setup a print function for the given request.
 * This will be triggered if the tevent_req_print() function was
 * called on the given request.
 *
 * Note: this function should only be used for debugging.
 */
void tevent_req_set_print_fn(struct tevent_req *req, tevent_req_print_fn fn)
{
	req->private_print = fn;
}

/**
 * @brief This function sets a cancel function for the given request
 * @param[in] req	The given request
 * @param[in] fn	A pointer to the cancel function
 *
 * This function can be used to setup a cancel function for the given request.
 * This will be triggered if the tevent_req_cancel() function was
 * called on the given request.
 *
 */
void tevent_req_set_cancel_fn(struct tevent_req *req, tevent_req_cancel_fn fn)
{
	req->private_cancel = fn;
}

/**
 * @brief This function tries to cancel the given request
 * @param[in] req	The given request
 * @param[in] location	Automaticly filled with the __location__ macro
 * 			via the tevent_req_cancel() macro. This is for debugging
 * 			only!
 * @retval		This function returns true is the request is cancelable.
 * 			Otherwise false is returned.
 *
 * This function can be used to cancel the given request.
 *
 * It is only possible to cancel a request when the implementation
 * has registered a cancel function via the tevent_req_set_cancel_fn().
 *
 * Note: Even if the function returns true, the caller need to wait
 *       for the function to complete normally.
 *       Only the _recv() function of the given request indicates
 *       if the request was really canceled.
 */
bool _tevent_req_cancel(struct tevent_req *req, const char *location)
{
	if (req->private_cancel == NULL) {
		return false;
	}

	return req->private_cancel(req);
}
