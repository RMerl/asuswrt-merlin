/* 
   Unix SMB/CIFS implementation.

   generalised event loop handling

   INTERNAL STRUCTS. THERE ARE NO API GUARANTEES.
   External users should only ever have to include this header when 
   implementing new tevent backends.

   Copyright (C) Stefan Metzmacher 2005-2009

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

struct tevent_req {
	/**
	 * @brief What to do on completion
	 *
	 * This is used for the user of an async request, fn is called when
	 * the request completes, either successfully or with an error.
	 */
	struct {
		/**
		 * @brief Completion function
		 * Completion function, to be filled by the API user
		 */
		tevent_req_fn fn;
		/**
		 * @brief Private data for the completion function
		 */
		void *private_data;
	} async;

	/**
	 * @brief Private state pointer for the actual implementation
	 *
	 * The implementation doing the work for the async request needs to
	 * keep around current data like for example a fd event. The user of
	 * an async request should not touch this.
	 */
	void *data;

	/**
	 * @brief A function to overwrite the default print function
	 *
	 * The implementation doing the work may want to implement a
	 * custom function to print the text representation of the async
	 * request.
	 */
	tevent_req_print_fn private_print;

	/**
	 * @brief A function to cancel the request
	 *
	 * The implementation might want to set a function
	 * that is called when the tevent_req_cancel() function
	 * was called.
	 */
	tevent_req_cancel_fn private_cancel;

	/**
	 * @brief Internal state of the request
	 *
	 * Callers should only access this via functions and never directly.
	 */
	struct {
		/**
		 * @brief The talloc type of the data pointer
		 *
		 * This is filled by the tevent_req_create() macro.
		 *
		 * This for debugging only.
		 */
		const char *private_type;

		/**
		 * @brief The location where the request was created
		 *
		 * This uses the __location__ macro via the tevent_req_create()
		 * macro.
		 *
		 * This for debugging only.
		 */
		const char *create_location;

		/**
		 * @brief The location where the request was finished
		 *
		 * This uses the __location__ macro via the tevent_req_done(),
		 * tevent_req_error() or tevent_req_nomem() macro.
		 *
		 * This for debugging only.
		 */
		const char *finish_location;

		/**
		 * @brief The location where the request was canceled
		 *
		 * This uses the __location__ macro via the
		 * tevent_req_cancel() macro.
		 *
		 * This for debugging only.
		 */
		const char *cancel_location;

		/**
		 * @brief The external state - will be queried by the caller
		 *
		 * While the async request is being processed, state will remain in
		 * TEVENT_REQ_IN_PROGRESS. A request is finished if
		 * req->state>=TEVENT_REQ_DONE.
		 */
		enum tevent_req_state state;

		/**
		 * @brief status code when finished
		 *
		 * This status can be queried in the async completion function. It
		 * will be set to 0 when everything went fine.
		 */
		uint64_t error;

		/**
		 * @brief the immediate event used by tevent_req_post
		 *
		 */
		struct tevent_immediate *trigger;

		/**
		 * @brief the timer event if tevent_req_set_endtime was used
		 *
		 */
		struct tevent_timer *timer;
	} internal;
};

struct tevent_fd {
	struct tevent_fd *prev, *next;
	struct tevent_context *event_ctx;
	int fd;
	uint16_t flags; /* see TEVENT_FD_* flags */
	tevent_fd_handler_t handler;
	tevent_fd_close_fn_t close_fn;
	/* this is private for the specific handler */
	void *private_data;
	/* this is for debugging only! */
	const char *handler_name;
	const char *location;
	/* this is private for the events_ops implementation */
	uint64_t additional_flags;
	void *additional_data;
};

struct tevent_timer {
	struct tevent_timer *prev, *next;
	struct tevent_context *event_ctx;
	struct timeval next_event;
	tevent_timer_handler_t handler;
	/* this is private for the specific handler */
	void *private_data;
	/* this is for debugging only! */
	const char *handler_name;
	const char *location;
	/* this is private for the events_ops implementation */
	void *additional_data;
};

struct tevent_immediate {
	struct tevent_immediate *prev, *next;
	struct tevent_context *event_ctx;
	tevent_immediate_handler_t handler;
	/* this is private for the specific handler */
	void *private_data;
	/* this is for debugging only! */
	const char *handler_name;
	const char *create_location;
	const char *schedule_location;
	/* this is private for the events_ops implementation */
	void (*cancel_fn)(struct tevent_immediate *im);
	void *additional_data;
};

struct tevent_signal {
	struct tevent_signal *prev, *next;
	struct tevent_context *event_ctx;
	int signum;
	int sa_flags;
	tevent_signal_handler_t handler;
	/* this is private for the specific handler */
	void *private_data;
	/* this is for debugging only! */
	const char *handler_name;
	const char *location;
	/* this is private for the events_ops implementation */
	void *additional_data;
};

struct tevent_debug_ops {
	void (*debug)(void *context, enum tevent_debug_level level,
		      const char *fmt, va_list ap) PRINTF_ATTRIBUTE(3,0);
	void *context;
};

void tevent_debug(struct tevent_context *ev, enum tevent_debug_level level,
		  const char *fmt, ...) PRINTF_ATTRIBUTE(3,4);

struct tevent_context {
	/* the specific events implementation */
	const struct tevent_ops *ops;

	/* list of fd events - used by common code */
	struct tevent_fd *fd_events;

	/* list of timed events - used by common code */
	struct tevent_timer *timer_events;

	/* list of immediate events - used by common code */
	struct tevent_immediate *immediate_events;

	/* list of signal events - used by common code */
	struct tevent_signal *signal_events;

	/* this is private for the events_ops implementation */
	void *additional_data;

	/* pipe hack used with signal handlers */
	struct tevent_fd *pipe_fde;
	int pipe_fds[2];

	/* debugging operations */
	struct tevent_debug_ops debug_ops;

	/* info about the nesting status */
	struct {
		bool allowed;
		uint32_t level;
		tevent_nesting_hook hook_fn;
		void *hook_private;
	} nesting;
};


int tevent_common_context_destructor(struct tevent_context *ev);
int tevent_common_loop_wait(struct tevent_context *ev,
			    const char *location);

int tevent_common_fd_destructor(struct tevent_fd *fde);
struct tevent_fd *tevent_common_add_fd(struct tevent_context *ev,
				       TALLOC_CTX *mem_ctx,
				       int fd,
				       uint16_t flags,
				       tevent_fd_handler_t handler,
				       void *private_data,
				       const char *handler_name,
				       const char *location);
void tevent_common_fd_set_close_fn(struct tevent_fd *fde,
				   tevent_fd_close_fn_t close_fn);
uint16_t tevent_common_fd_get_flags(struct tevent_fd *fde);
void tevent_common_fd_set_flags(struct tevent_fd *fde, uint16_t flags);

struct tevent_timer *tevent_common_add_timer(struct tevent_context *ev,
					     TALLOC_CTX *mem_ctx,
					     struct timeval next_event,
					     tevent_timer_handler_t handler,
					     void *private_data,
					     const char *handler_name,
					     const char *location);
struct timeval tevent_common_loop_timer_delay(struct tevent_context *);

void tevent_common_schedule_immediate(struct tevent_immediate *im,
				      struct tevent_context *ev,
				      tevent_immediate_handler_t handler,
				      void *private_data,
				      const char *handler_name,
				      const char *location);
bool tevent_common_loop_immediate(struct tevent_context *ev);

struct tevent_signal *tevent_common_add_signal(struct tevent_context *ev,
					       TALLOC_CTX *mem_ctx,
					       int signum,
					       int sa_flags,
					       tevent_signal_handler_t handler,
					       void *private_data,
					       const char *handler_name,
					       const char *location);
int tevent_common_check_signal(struct tevent_context *ev);
void tevent_cleanup_pending_signal_handlers(struct tevent_signal *se);

bool tevent_standard_init(void);
bool tevent_select_init(void);
bool tevent_poll_init(void);
#ifdef HAVE_EPOLL
bool tevent_epoll_init(void);
#endif
