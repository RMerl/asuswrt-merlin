/* 
   Unix SMB/CIFS implementation.

   generalised event loop handling

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher 2005-2009
   Copyright (C) Volker Lendecke 2008

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

#ifndef __TEVENT_H__
#define __TEVENT_H__

#include <stdint.h>
#include <talloc.h>
#include <sys/time.h>
#include <stdbool.h>

struct tevent_context;
struct tevent_ops;
struct tevent_fd;
struct tevent_timer;
struct tevent_immediate;
struct tevent_signal;

/**
 * @defgroup tevent The tevent API
 *
 * The tevent low-level API
 *
 * This API provides the public interface to manage events in the tevent
 * mainloop. Functions are provided for managing low-level events such
 * as timer events, fd events and signal handling.
 *
 * @{
 */

/* event handler types */
/**
 * Called when a file descriptor monitored by tevent has
 * data to be read or written on it.
 */
typedef void (*tevent_fd_handler_t)(struct tevent_context *ev,
				    struct tevent_fd *fde,
				    uint16_t flags,
				    void *private_data);

/**
 * Called when tevent is ceasing the monitoring of a file descriptor.
 */
typedef void (*tevent_fd_close_fn_t)(struct tevent_context *ev,
				     struct tevent_fd *fde,
				     int fd,
				     void *private_data);

/**
 * Called when a tevent timer has fired.
 */
typedef void (*tevent_timer_handler_t)(struct tevent_context *ev,
				       struct tevent_timer *te,
				       struct timeval current_time,
				       void *private_data);

/**
 * Called when a tevent immediate event is invoked.
 */
typedef void (*tevent_immediate_handler_t)(struct tevent_context *ctx,
					   struct tevent_immediate *im,
					   void *private_data);

/**
 * Called after tevent detects the specified signal.
 */
typedef void (*tevent_signal_handler_t)(struct tevent_context *ev,
					struct tevent_signal *se,
					int signum,
					int count,
					void *siginfo,
					void *private_data);

/**
 * @brief Create a event_context structure.
 *
 * This must be the first events call, and all subsequent calls pass this
 * event_context as the first element. Event handlers also receive this as
 * their first argument.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @return              An allocated tevent context, NULL on error.
 *
 * @see tevent_context_init()
 */
struct tevent_context *tevent_context_init(TALLOC_CTX *mem_ctx);

/**
 * @brief Create a event_context structure and name it.
 *
 * This must be the first events call, and all subsequent calls pass this
 * event_context as the first element. Event handlers also receive this as
 * their first argument.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in]  name     The name for the tevent context.
 *
 * @return              An allocated tevent context, NULL on error.
 */
struct tevent_context *tevent_context_init_byname(TALLOC_CTX *mem_ctx, const char *name);

/**
 * @brief List available backends.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @return              A string vector with a terminating NULL element, NULL
 *                      on error.
 */
const char **tevent_backend_list(TALLOC_CTX *mem_ctx);

/**
 * @brief Set the default tevent backent.
 *
 * @param[in]  backend  The name of the backend to set.
 */
void tevent_set_default_backend(const char *backend);

#ifdef DOXYGEN
/**
 * @brief Add a file descriptor based event.
 *
 * @param[in]  ev       The event context to work on.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  fd       The file descriptor to base the event on.
 *
 * @param[in]  flags    #TEVENT_FD_READ or #TEVENT_FD_WRITE
 *
 * @param[in]  handler  The callback handler for the event.
 *
 * @param[in]  private_data  The private data passed to the callback handler.
 *
 * @return              The file descriptor based event, NULL on error.
 *
 * @note To cancel the monitoring of a file descriptor, call talloc_free()
 * on the object returned by this function.
 */
struct tevent_fd *tevent_add_fd(struct tevent_context *ev,
				TALLOC_CTX *mem_ctx,
				int fd,
				uint16_t flags,
				tevent_fd_handler_t handler,
				void *private_data);
#else
struct tevent_fd *_tevent_add_fd(struct tevent_context *ev,
				 TALLOC_CTX *mem_ctx,
				 int fd,
				 uint16_t flags,
				 tevent_fd_handler_t handler,
				 void *private_data,
				 const char *handler_name,
				 const char *location);
#define tevent_add_fd(ev, mem_ctx, fd, flags, handler, private_data) \
	_tevent_add_fd(ev, mem_ctx, fd, flags, handler, private_data, \
		       #handler, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Add a timed event
 *
 * @param[in]  ev       The event context to work on.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  next_event  Timeval specifying the absolute time to fire this
 * event. This is not an offset.
 *
 * @param[in]  handler  The callback handler for the event.
 *
 * @param[in]  private_data  The private data passed to the callback handler.
 *
 * @return The newly-created timer event, or NULL on error.
 *
 * @note To cancel a timer event before it fires, call talloc_free() on the
 * event returned from this function. This event is automatically
 * talloc_free()-ed after its event handler files, if it hasn't been freed yet.
 *
 * @note Unlike some mainloops, tevent timers are one-time events. To set up
 * a recurring event, it is necessary to call tevent_add_timer() again during
 * the handler processing.
 *
 * @note Due to the internal mainloop processing, a timer set to run
 * immediately will do so after any other pending timers fire, but before
 * any further file descriptor or signal handling events fire. Callers should
 * not rely on this behavior!
 */
struct tevent_timer *tevent_add_timer(struct tevent_context *ev,
                                      TALLOC_CTX *mem_ctx,
                                      struct timeval next_event,
                                      tevent_timer_handler_t handler,
                                      void *private_data);
#else
struct tevent_timer *_tevent_add_timer(struct tevent_context *ev,
				       TALLOC_CTX *mem_ctx,
				       struct timeval next_event,
				       tevent_timer_handler_t handler,
				       void *private_data,
				       const char *handler_name,
				       const char *location);
#define tevent_add_timer(ev, mem_ctx, next_event, handler, private_data) \
	_tevent_add_timer(ev, mem_ctx, next_event, handler, private_data, \
			  #handler, __location__)
#endif

#ifdef DOXYGEN
/**
 * Initialize an immediate event object
 *
 * This object can be used to trigger an event to occur immediately after
 * returning from the current event (before any other event occurs)
 *
 * @param[in] mem_ctx  The talloc memory context to use as the parent
 *
 * @return An empty tevent_immediate object. Use tevent_schedule_immediate
 * to populate and use it.
 *
 * @note Available as of tevent 0.9.8
 */
struct tevent_immediate *tevent_create_immediate(TALLOC_CTX *mem_ctx);
#else
struct tevent_immediate *_tevent_create_immediate(TALLOC_CTX *mem_ctx,
						  const char *location);
#define tevent_create_immediate(mem_ctx) \
	_tevent_create_immediate(mem_ctx, __location__)
#endif

#ifdef DOXYGEN

/**
 * Schedule an event for immediate execution. This event will occur
 * immediately after returning from the current event (before any other
 * event occurs)
 *
 * @param[in] im       The tevent_immediate object to populate and use
 * @param[in] ctx      The tevent_context to run this event
 * @param[in] handler  The event handler to run when this event fires
 * @param[in] private_data  Data to pass to the event handler
 */
void tevent_schedule_immediate(struct tevent_immediate *im,
                struct tevent_context *ctx,
                tevent_immediate_handler_t handler,
                void *private_data);
#else
void _tevent_schedule_immediate(struct tevent_immediate *im,
				struct tevent_context *ctx,
				tevent_immediate_handler_t handler,
				void *private_data,
				const char *handler_name,
				const char *location);
#define tevent_schedule_immediate(im, ctx, handler, private_data) \
	_tevent_schedule_immediate(im, ctx, handler, private_data, \
				   #handler, __location__);
#endif

#ifdef DOXYGEN
/**
 * @brief Add a tevent signal handler
 *
 * tevent_add_signal() creates a new event for handling a signal the next
 * time through the mainloop. It implements a very simple traditional signal
 * handler whose only purpose is to add the handler event into the mainloop.
 *
 * @param[in]  ev       The event context to work on.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  signum   The signal to trap
 *
 * @param[in]  handler  The callback handler for the signal.
 *
 * @param[in]  sa_flags sigaction flags for this signal handler.
 *
 * @param[in]  private_data  The private data passed to the callback handler.
 *
 * @return The newly-created signal handler event, or NULL on error.
 *
 * @note To cancel a signal handler, call talloc_free() on the event returned
 * from this function.
 */
struct tevent_signal *tevent_add_signal(struct tevent_context *ev,
                     TALLOC_CTX *mem_ctx,
                     int signum,
                     int sa_flags,
                     tevent_signal_handler_t handler,
                     void *private_data);
#else
struct tevent_signal *_tevent_add_signal(struct tevent_context *ev,
					 TALLOC_CTX *mem_ctx,
					 int signum,
					 int sa_flags,
					 tevent_signal_handler_t handler,
					 void *private_data,
					 const char *handler_name,
					 const char *location);
#define tevent_add_signal(ev, mem_ctx, signum, sa_flags, handler, private_data) \
	_tevent_add_signal(ev, mem_ctx, signum, sa_flags, handler, private_data, \
			   #handler, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Pass a single time through the mainloop
 *
 * This will process any appropriate signal, immediate, fd and timer events
 *
 * @param[in]  ev The event context to process
 *
 * @return Zero on success, nonzero if an internal error occurred
 */
int tevent_loop_once(struct tevent_context *ev);
#else
int _tevent_loop_once(struct tevent_context *ev, const char *location);
#define tevent_loop_once(ev) \
	_tevent_loop_once(ev, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Run the mainloop
 *
 * The mainloop will run until there are no events remaining to be processed
 *
 * @param[in]  ev The event context to process
 *
 * @return Zero if all events have been processed. Nonzero if an internal
 * error occurred.
 */
int tevent_loop_wait(struct tevent_context *ev);
#else
int _tevent_loop_wait(struct tevent_context *ev, const char *location);
#define tevent_loop_wait(ev) \
	_tevent_loop_wait(ev, __location__)
#endif


/**
 * Assign a function to run when a tevent_fd is freed
 *
 * This function is a destructor for the tevent_fd. It does not automatically
 * close the file descriptor. If this is the desired behavior, then it must be
 * performed by the close_fn.
 *
 * @param[in] fde       File descriptor event on which to set the destructor
 * @param[in] close_fn  Destructor to execute when fde is freed
 */
void tevent_fd_set_close_fn(struct tevent_fd *fde,
			    tevent_fd_close_fn_t close_fn);

/**
 * Automatically close the file descriptor when the tevent_fd is freed
 *
 * This function calls close(fd) internally.
 *
 * @param[in] fde  File descriptor event to auto-close
 */
void tevent_fd_set_auto_close(struct tevent_fd *fde);

/**
 * Return the flags set on this file descriptor event
 *
 * @param[in] fde  File descriptor event to query
 *
 * @return The flags set on the event. See #TEVENT_FD_READ and
 * #TEVENT_FD_WRITE
 */
uint16_t tevent_fd_get_flags(struct tevent_fd *fde);

/**
 * Set flags on a file descriptor event
 *
 * @param[in] fde    File descriptor event to set
 * @param[in] flags  Flags to set on the event. See #TEVENT_FD_READ and
 * #TEVENT_FD_WRITE
 */
void tevent_fd_set_flags(struct tevent_fd *fde, uint16_t flags);

/**
 * Query whether tevent supports signal handling
 *
 * @param[in] ev  An initialized tevent context
 *
 * @return True if this platform and tevent context support signal handling
 */
bool tevent_signal_support(struct tevent_context *ev);

void tevent_set_abort_fn(void (*abort_fn)(const char *reason));

/* bits for file descriptor event flags */

/**
 * Monitor a file descriptor for write availability
 */
#define TEVENT_FD_READ 1
/**
 * Monitor a file descriptor for data to be read
 */
#define TEVENT_FD_WRITE 2

/**
 * Convenience function for declaring a tevent_fd writable
 */
#define TEVENT_FD_WRITEABLE(fde) \
	tevent_fd_set_flags(fde, tevent_fd_get_flags(fde) | TEVENT_FD_WRITE)

/**
 * Convenience function for declaring a tevent_fd readable
 */
#define TEVENT_FD_READABLE(fde) \
	tevent_fd_set_flags(fde, tevent_fd_get_flags(fde) | TEVENT_FD_READ)

/**
 * Convenience function for declaring a tevent_fd non-writable
 */
#define TEVENT_FD_NOT_WRITEABLE(fde) \
	tevent_fd_set_flags(fde, tevent_fd_get_flags(fde) & ~TEVENT_FD_WRITE)

/**
 * Convenience function for declaring a tevent_fd non-readable
 */
#define TEVENT_FD_NOT_READABLE(fde) \
	tevent_fd_set_flags(fde, tevent_fd_get_flags(fde) & ~TEVENT_FD_READ)

/**
 * Debug level of tevent
 */
enum tevent_debug_level {
	TEVENT_DEBUG_FATAL,
	TEVENT_DEBUG_ERROR,
	TEVENT_DEBUG_WARNING,
	TEVENT_DEBUG_TRACE
};

/**
 * @brief The tevent debug callbac.
 *
 * @param[in]  context  The memory context to use.
 *
 * @param[in]  level    The debug level.
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ap       The arguments for the format string.
 */
typedef void (*tevent_debug_fn)(void *context,
				enum tevent_debug_level level,
				const char *fmt,
				va_list ap) PRINTF_ATTRIBUTE(3,0);

/**
 * Set destination for tevent debug messages
 *
 * @param[in] ev        Event context to debug
 * @param[in] debug     Function to handle output printing
 * @param[in] context   The context to pass to the debug function.
 *
 * @return Always returns 0 as of version 0.9.8
 *
 * @note Default is to emit no debug messages
 */
int tevent_set_debug(struct tevent_context *ev,
		     tevent_debug_fn debug,
		     void *context);

/**
 * Designate stderr for debug message output
 *
 * @param[in] ev     Event context to debug
 *
 * @note This function will only output TEVENT_DEBUG_FATAL, TEVENT_DEBUG_ERROR
 * and TEVENT_DEBUG_WARNING messages. For TEVENT_DEBUG_TRACE, please define a
 * function for tevent_set_debug()
 */
int tevent_set_debug_stderr(struct tevent_context *ev);

/**
 * @}
 */

/**
 * @defgroup tevent_request The tevent request functions.
 * @ingroup tevent
 *
 * A tevent_req represents an asynchronous computation.
 *
 * The tevent_req group of API calls is the recommended way of
 * programming async computations within tevent. In particular the
 * file descriptor (tevent_add_fd) and timer (tevent_add_timed) events
 * are considered too low-level to be used in larger computations. To
 * read and write from and to sockets, Samba provides two calls on top
 * of tevent_add_fd: read_packet_send/recv and writev_send/recv. These
 * requests are much easier to compose than the low-level event
 * handlers called from tevent_add_fd.
 *
 * A lot of the simplicity tevent_req has brought to the notoriously
 * hairy async programming came via a set of conventions that every
 * async computation programmed should follow. One central piece of
 * these conventions is the naming of routines and variables.
 *
 * Every async computation needs a name (sensibly called "computation"
 * down from here). From this name quite a few naming conventions are
 * derived.
 *
 * Every computation that requires local state needs a
 * @code
 * struct computation_state {
 *     int local_var;
 * };
 * @endcode
 * Even if no local variables are required, such a state struct should
 * be created containing a dummy variable. Quite a few helper
 * functions and macros (for example tevent_req_create()) assume such
 * a state struct.
 *
 * An async computation is started by a computation_send
 * function. When it is finished, its result can be received by a
 * computation_recv function. For an example how to set up an async
 * computation, see the code example in the documentation for
 * tevent_req_create() and tevent_req_post(). The prototypes for _send
 * and _recv functions should follow some conventions:
 *
 * @code
 * struct tevent_req *computation_send(TALLOC_CTX *mem_ctx,
 *                                     struct tevent_req *ev,
 *                                     ... further args);
 * int computation_recv(struct tevent_req *req, ... further output args);
 * @endcode
 *
 * The "int" result of computation_recv() depends on the result the
 * sync version of the function would have, "int" is just an example
 * here.
 *
 * Another important piece of the conventions is that the program flow
 * is interrupted as little as possible. Because a blocking
 * sub-computation requires that the flow needs to continue in a
 * separate function that is the logical sequel of some computation,
 * it should lexically follow sending off the blocking
 * sub-computation. Setting the callback function via
 * tevent_req_set_callback() requires referencing a function lexically
 * below the call to tevent_req_set_callback(), forward declarations
 * are required. A lot of the async computations thus begin with a
 * sequence of declarations such as
 *
 * @code
 * static void computation_step1_done(struct tevent_req *subreq);
 * static void computation_step2_done(struct tevent_req *subreq);
 * static void computation_step3_done(struct tevent_req *subreq);
 * @endcode
 *
 * It really helps readability a lot to do these forward declarations,
 * because the lexically sequential program flow makes the async
 * computations almost as clear to read as a normal, sync program
 * flow.
 *
 * It is up to the user of the async computation to talloc_free it
 * after it has finished. If an async computation should be aborted,
 * the tevent_req structure can be talloc_free'ed. After it has
 * finished, it should talloc_free'ed by the API user.
 *
 * @{
 */

/**
 * An async request moves from TEVENT_REQ_INIT to
 * TEVENT_REQ_IN_PROGRESS. All other states are valid after a request
 * has finished.
 */
enum tevent_req_state {
	/**
	 * We are creating the request
	 */
	TEVENT_REQ_INIT,
	/**
	 * We are waiting the request to complete
	 */
	TEVENT_REQ_IN_PROGRESS,
	/**
	 * The request is finished successfully
	 */
	TEVENT_REQ_DONE,
	/**
	 * A user error has occurred. The user error has been
	 * indicated by tevent_req_error(), it can be retrieved via
	 * tevent_req_is_error().
	 */
	TEVENT_REQ_USER_ERROR,
	/**
	 * Request timed out after the timeout set by tevent_req_set_endtime.
	 */
	TEVENT_REQ_TIMED_OUT,
	/**
	 * An internal allocation has failed, or tevent_req_nomem has
	 * been given a NULL pointer as the first argument.
	 */
	TEVENT_REQ_NO_MEMORY,
	/**
	 * The request has been received by the caller. No further
	 * action is valid.
	 */
	TEVENT_REQ_RECEIVED
};

/**
 * @brief An async request
 */
struct tevent_req;

/**
 * @brief A tevent request callback function.
 *
 * @param[in]  req      The tevent async request which executed this callback.
 */
typedef void (*tevent_req_fn)(struct tevent_req *req);

/**
 * @brief Set an async request callback.
 *
 * See the documentation of tevent_req_post() for an example how this
 * is supposed to be used.
 *
 * @param[in]  req      The async request to set the callback.
 *
 * @param[in]  fn       The callback function to set.
 *
 * @param[in]  pvt      A pointer to private data to pass to the async request
 *                      callback.
 */
void tevent_req_set_callback(struct tevent_req *req, tevent_req_fn fn, void *pvt);

#ifdef DOXYGEN
/**
 * @brief Get the private data cast to the given type for a callback from
 *        a tevent request structure.
 *
 * @code
 * static void computation_done(struct tevent_req *subreq) {
 *     struct tevent_req *req = tevent_req_callback_data(subreq, struct tevent_req);
 *     struct computation_state *state = tevent_req_data(req, struct computation_state);
 *     .... more things, eventually maybe call tevent_req_done(req);
 * }
 * @endcode
 *
 * @param[in]  req      The structure to get the callback data from.
 *
 * @param[in]  type     The type of the private callback data to get.
 *
 * @return              The type casted private data set NULL if not set.
 */
void *tevent_req_callback_data(struct tevent_req *req, #type);
#else
void *_tevent_req_callback_data(struct tevent_req *req);
#define tevent_req_callback_data(_req, _type) \
	talloc_get_type_abort(_tevent_req_callback_data(_req), _type)
#endif

#ifdef DOXYGEN
/**
 * @brief Get the private data for a callback from a tevent request structure.
 *
 * @param[in]  req      The structure to get the callback data from.
 *
 * @param[in]  req      The structure to get the data from.
 *
 * @return              The private data or NULL if not set.
 */
void *tevent_req_callback_data_void(struct tevent_req *req);
#else
#define tevent_req_callback_data_void(_req) \
	_tevent_req_callback_data(_req)
#endif

#ifdef DOXYGEN
/**
 * @brief Get the private data from a tevent request structure.
 *
 * When the tevent_req has been created by tevent_req_create, the
 * result of tevent_req_data() is the state variable created by
 * tevent_req_create() as a child of the req.
 *
 * @param[in]  req      The structure to get the private data from.
 *
 * @param[in]  type	The type of the private data
 *
 * @return              The private data or NULL if not set.
 */
void *tevent_req_data(struct tevent_req *req, #type);
#else
void *_tevent_req_data(struct tevent_req *req);
#define tevent_req_data(_req, _type) \
	talloc_get_type_abort(_tevent_req_data(_req), _type)
#endif

/**
 * @brief The print function which can be set for a tevent async request.
 *
 * @param[in]  req      The tevent async request.
 *
 * @param[in]  ctx      A talloc memory context which can be uses to allocate
 *                      memory.
 *
 * @return              An allocated string buffer to print.
 *
 * Example:
 * @code
 *   static char *my_print(struct tevent_req *req, TALLOC_CTX *mem_ctx)
 *   {
 *     struct my_data *data = tevent_req_data(req, struct my_data);
 *     char *result;
 *
 *     result = tevent_req_default_print(mem_ctx, req);
 *     if (result == NULL) {
 *       return NULL;
 *     }
 *
 *     return talloc_asprintf_append_buffer(result, "foo=%d, bar=%d",
 *       data->foo, data->bar);
 *   }
 * @endcode
 */
typedef char *(*tevent_req_print_fn)(struct tevent_req *req, TALLOC_CTX *ctx);

/**
 * @brief This function sets a print function for the given request.
 *
 * This function can be used to setup a print function for the given request.
 * This will be triggered if the tevent_req_print() function was
 * called on the given request.
 *
 * @param[in]  req      The request to use.
 *
 * @param[in]  fn       A pointer to the print function
 *
 * @note This function should only be used for debugging.
 */
void tevent_req_set_print_fn(struct tevent_req *req, tevent_req_print_fn fn);

/**
 * @brief The default print function for creating debug messages.
 *
 * The function should not be used by users of the async API,
 * but custom print function can use it and append custom text
 * to the string.
 *
 * @param[in]  req      The request to be printed.
 *
 * @param[in]  mem_ctx  The memory context for the result.
 *
 * @return              Text representation of request.
 *
 */
char *tevent_req_default_print(struct tevent_req *req, TALLOC_CTX *mem_ctx);

/**
 * @brief Print an tevent_req structure in debug messages.
 *
 * This function should be used by callers of the async API.
 *
 * @param[in]  mem_ctx  The memory context for the result.
 *
 * @param[in] req       The request to be printed.
 *
 * @return              Text representation of request.
 */
char *tevent_req_print(TALLOC_CTX *mem_ctx, struct tevent_req *req);

/**
 * @brief A typedef for a cancel function for a tevent request.
 *
 * @param[in]  req      The tevent request calling this function.
 *
 * @return              True if the request could be canceled, false if not.
 */
typedef bool (*tevent_req_cancel_fn)(struct tevent_req *req);

/**
 * @brief This function sets a cancel function for the given tevent request.
 *
 * This function can be used to setup a cancel function for the given request.
 * This will be triggered if the tevent_req_cancel() function was
 * called on the given request.
 *
 * @param[in]  req      The request to use.
 *
 * @param[in]  fn       A pointer to the cancel function.
 */
void tevent_req_set_cancel_fn(struct tevent_req *req, tevent_req_cancel_fn fn);

#ifdef DOXYGEN
/**
 * @brief Try to cancel the given tevent request.
 *
 * This function can be used to cancel the given request.
 *
 * It is only possible to cancel a request when the implementation
 * has registered a cancel function via the tevent_req_set_cancel_fn().
 *
 * @param[in]  req      The request to use.
 *
 * @return              This function returns true is the request is cancelable,
 *                      othererwise false is returned.
 *
 * @note Even if the function returns true, the caller need to wait
 *       for the function to complete normally.
 *       Only the _recv() function of the given request indicates
 *       if the request was really canceled.
 */
bool tevent_req_cancel(struct tevent_req *req);
#else
bool _tevent_req_cancel(struct tevent_req *req, const char *location);
#define tevent_req_cancel(req) \
	_tevent_req_cancel(req, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Create an async tevent request.
 *
 * The new async request will be initialized in state TEVENT_REQ_IN_PROGRESS.
 *
 * @code
 * struct tevent_req *req;
 * struct computation_state *state;
 * req = tevent_req_create(mem_ctx, &state, struct computation_state);
 * @endcode
 *
 * Tevent_req_create() creates the state variable as a talloc child of
 * its result. The state variable should be used as the talloc parent
 * for all temporary variables that are allocated during the async
 * computation. This way, when the user of the async computation frees
 * the request, the state as a talloc child will be free'd along with
 * all the temporary variables hanging off the state.
 *
 * @param[in] mem_ctx   The memory context for the result.
 * @param[in] pstate    Pointer to the private request state.
 * @param[in] type      The name of the request.
 *
 * @return              A new async request. NULL on error.
 */
struct tevent_req *tevent_req_create(TALLOC_CTX *mem_ctx,
				     void **pstate, #type);
#else
struct tevent_req *_tevent_req_create(TALLOC_CTX *mem_ctx,
				      void *pstate,
				      size_t state_size,
				      const char *type,
				      const char *location);

#define tevent_req_create(_mem_ctx, _pstate, _type) \
	_tevent_req_create((_mem_ctx), (_pstate), sizeof(_type), \
			   #_type, __location__)
#endif

/**
 * @brief Set a timeout for an async request.
 *
 * @param[in]  req      The request to set the timeout for.
 *
 * @param[in]  ev       The event context to use for the timer.
 *
 * @param[in]  endtime  The endtime of the request.
 *
 * @return              True if succeeded, false if not.
 */
bool tevent_req_set_endtime(struct tevent_req *req,
			    struct tevent_context *ev,
			    struct timeval endtime);

#ifdef DOXYGEN
/**
 * @brief Call the notify callback of the given tevent request manually.
 *
 * @param[in]  req      The tevent request to call the notify function from.
 *
 * @see tevent_req_set_callback()
 */
void tevent_req_notify_callback(struct tevent_req *req);
#else
void _tevent_req_notify_callback(struct tevent_req *req, const char *location);
#define tevent_req_notify_callback(req)		\
	_tevent_req_notify_callback(req, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief An async request has successfully finished.
 *
 * This function is to be used by implementors of async requests. When a
 * request is successfully finished, this function calls the user's completion
 * function.
 *
 * @param[in]  req       The finished request.
 */
void tevent_req_done(struct tevent_req *req);
#else
void _tevent_req_done(struct tevent_req *req,
		      const char *location);
#define tevent_req_done(req) \
	_tevent_req_done(req, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief An async request has seen an error.
 *
 * This function is to be used by implementors of async requests. When a
 * request can not successfully completed, the implementation should call this
 * function with the appropriate status code.
 *
 * If error is 0 the function returns false and does nothing more.
 *
 * @param[in]  req      The request with an error.
 *
 * @param[in]  error    The error code.
 *
 * @return              On success true is returned, false if error is 0.
 *
 * @code
 * int error = first_function();
 * if (tevent_req_error(req, error)) {
 *      return;
 * }
 *
 * error = second_function();
 * if (tevent_req_error(req, error)) {
 *      return;
 * }
 *
 * tevent_req_done(req);
 * return;
 * @endcode
 */
bool tevent_req_error(struct tevent_req *req,
		      uint64_t error);
#else
bool _tevent_req_error(struct tevent_req *req,
		       uint64_t error,
		       const char *location);
#define tevent_req_error(req, error) \
	_tevent_req_error(req, error, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Helper function for nomem check.
 *
 * Convenience helper to easily check alloc failure within a callback
 * implementing the next step of an async request.
 *
 * @param[in]  p        The pointer to be checked.
 *
 * @param[in]  req      The request being processed.
 *
 * @code
 * p = talloc(mem_ctx, bla);
 * if (tevent_req_nomem(p, req)) {
 *      return;
 * }
 * @endcode
 */
bool tevent_req_nomem(const void *p,
		      struct tevent_req *req);
#else
bool _tevent_req_nomem(const void *p,
		       struct tevent_req *req,
		       const char *location);
#define tevent_req_nomem(p, req) \
	_tevent_req_nomem(p, req, __location__)
#endif

/**
 * @brief Finish a request before the caller had the change to set the callback.
 *
 * An implementation of an async request might find that it can either finish
 * the request without waiting for an external event, or it can not even start
 * the engine. To present the illusion of a callback to the user of the API,
 * the implementation can call this helper function which triggers an
 * immediate timed event. This way the caller can use the same calling
 * conventions, independent of whether the request was actually deferred.
 *
 * @code
 * struct tevent_req *computation_send(TALLOC_CTX *mem_ctx,
 *                                     struct tevent_context *ev)
 * {
 *     struct tevent_req *req, *subreq;
 *     struct computation_state *state;
 *     req = tevent_req_create(mem_ctx, &state, struct computation_state);
 *     if (req == NULL) {
 *         return NULL;
 *     }
 *     subreq = subcomputation_send(state, ev);
 *     if (tevent_req_nomem(subreq, req)) {
 *         return tevent_req_post(req, ev);
 *     }
 *     tevent_req_set_callback(subreq, computation_done, req);
 *     return req;
 * }
 * @endcode
 *
 * @param[in]  req      The finished request.
 *
 * @param[in]  ev       The tevent_context for the timed event.
 *
 * @return              The given request will be returned.
 */
struct tevent_req *tevent_req_post(struct tevent_req *req,
				   struct tevent_context *ev);

/**
 * @brief Check if the given request is still in progress.
 *
 * It is typically used by sync wrapper functions.
 *
 * @param[in]  req      The request to poll.
 *
 * @return              The boolean form of "is in progress".
 */
bool tevent_req_is_in_progress(struct tevent_req *req);

/**
 * @brief Actively poll for the given request to finish.
 *
 * This function is typically used by sync wrapper functions.
 *
 * @param[in]  req      The request to poll.
 *
 * @param[in]  ev       The tevent_context to be used.
 *
 * @return              On success true is returned. If a critical error has
 *                      happened in the tevent loop layer false is returned.
 *                      This is not the return value of the given request!
 *
 * @note This should only be used if the given tevent context was created by the
 * caller, to avoid event loop nesting.
 *
 * @code
 * req = tstream_writev_queue_send(mem_ctx,
 *                                 ev_ctx,
 *                                 tstream,
 *                                 send_queue,
 *                                 iov, 2);
 * ok = tevent_req_poll(req, tctx->ev);
 * rc = tstream_writev_queue_recv(req, &sys_errno);
 * TALLOC_FREE(req);
 * @endcode
 */
bool tevent_req_poll(struct tevent_req *req,
		     struct tevent_context *ev);

/**
 * @brief Get the tevent request state and the actual error set by
 * tevent_req_error.
 *
 * @code
 * int computation_recv(struct tevent_req *req, uint64_t *perr)
 * {
 *     enum tevent_req_state state;
 *     uint64_t err;
 *     if (tevent_req_is_error(req, &state, &err)) {
 *         *perr = err;
 *         return -1;
 *     }
 *     return 0;
 * }
 * @endcode
 *
 * @param[in]  req      The tevent request to get the error from.
 *
 * @param[out] state    A pointer to store the tevent request error state.
 *
 * @param[out] error    A pointer to store the error set by tevent_req_error().
 *
 * @return              True if the function could set error and state, false
 *                      otherwise.
 *
 * @see tevent_req_error()
 */
bool tevent_req_is_error(struct tevent_req *req,
			 enum tevent_req_state *state,
			 uint64_t *error);

/**
 * @brief Use as the last action of a _recv() function.
 *
 * This function destroys the attached private data.
 *
 * @param[in]  req      The finished request.
 */
void tevent_req_received(struct tevent_req *req);

/**
 * @brief Create a tevent subrequest at a given time.
 *
 * The idea is that always the same syntax for tevent requests.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  ev       The event handle to setup the request.
 *
 * @param[in]  wakeup_time The time to wakeup and execute the request.
 *
 * @return              The new subrequest, NULL on error.
 *
 * Example:
 * @code
 *   static void my_callback_wakeup_done(tevent_req *subreq)
 *   {
 *     struct tevent_req *req = tevent_req_callback_data(subreq,
 *                              struct tevent_req);
 *     bool ok;
 *
 *     ok = tevent_wakeup_recv(subreq);
 *     TALLOC_FREE(subreq);
 *     if (!ok) {
 *         tevent_req_error(req, -1);
 *         return;
 *     }
 *     ...
 *   }
 * @endcode
 *
 * @code
 *   subreq = tevent_wakeup_send(mem_ctx, ev, wakeup_time);
 *   if (tevent_req_nomem(subreq, req)) {
 *     return false;
 *   }
 *   tevent_set_callback(subreq, my_callback_wakeup_done, req);
 * @endcode
 *
 * @see tevent_wakeup_recv()
 */
struct tevent_req *tevent_wakeup_send(TALLOC_CTX *mem_ctx,
				      struct tevent_context *ev,
				      struct timeval wakeup_time);

/**
 * @brief Check if the wakeup has been correctly executed.
 *
 * This function needs to be called in the callback function set after calling
 * tevent_wakeup_send().
 *
 * @param[in]  req      The tevent request to check.
 *
 * @return              True on success, false otherwise.
 *
 * @see tevent_wakeup_recv()
 */
bool tevent_wakeup_recv(struct tevent_req *req);

/* @} */

/**
 * @defgroup tevent_helpers The tevent helper functiions
 * @ingroup tevent
 *
 * @todo description
 *
 * @{
 */

/**
 * @brief Compare two timeval values.
 *
 * @param[in]  tv1      The first timeval value to compare.
 *
 * @param[in]  tv2      The second timeval value to compare.
 *
 * @return              0 if they are equal.
 *                      1 if the first time is greater than the second.
 *                      -1 if the first time is smaller than the second.
 */
int tevent_timeval_compare(const struct timeval *tv1,
			   const struct timeval *tv2);

/**
 * @brief Get a zero timval value.
 *
 * @return              A zero timval value.
 */
struct timeval tevent_timeval_zero(void);

/**
 * @brief Get a timeval value for the current time.
 *
 * @return              A timval value with the current time.
 */
struct timeval tevent_timeval_current(void);

/**
 * @brief Get a timeval structure with the given values.
 *
 * @param[in]  secs     The seconds to set.
 *
 * @param[in]  usecs    The milliseconds to set.
 *
 * @return              A timeval structure with the given values.
 */
struct timeval tevent_timeval_set(uint32_t secs, uint32_t usecs);

/**
 * @brief Get the difference between two timeval values.
 *
 * @param[in]  tv1      The first timeval.
 *
 * @param[in]  tv2      The second timeval.
 *
 * @return              A timeval structure with the difference between the
 *                      first and the second value.
 */
struct timeval tevent_timeval_until(const struct timeval *tv1,
				    const struct timeval *tv2);

/**
 * @brief Check if a given timeval structure is zero.
 *
 * @param[in]  tv       The timeval to check if it is zero.
 *
 * @return              True if it is zero, false otherwise.
 */
bool tevent_timeval_is_zero(const struct timeval *tv);

/**
 * @brief Add the given amount of time to a timeval structure.
 *
 * @param[in]  tv        The timeval structure to add the time.
 *
 * @param[in]  secs      The seconds to add to the timeval.
 *
 * @param[in]  usecs     The milliseconds to add to the timeval.
 *
 * @return               The timeval structure with the new time.
 */
struct timeval tevent_timeval_add(const struct timeval *tv, uint32_t secs,
				  uint32_t usecs);

/**
 * @brief Get a timeval in the future with a specified offset from now.
 *
 * @param[in]  secs     The seconds of the offset from now.
 *
 * @param[in]  usecs    The milliseconds of the offset from now.
 *
 * @return              A timval with the given offset in the future.
 */
struct timeval tevent_timeval_current_ofs(uint32_t secs, uint32_t usecs);

/* @} */


/**
 * @defgroup tevent_queue The tevent queue functions
 * @ingroup tevent
 *
 * A tevent_queue is used to queue up async requests that must be
 * serialized. For example writing buffers into a socket must be
 * serialized. Writing a large lump of data into a socket can require
 * multiple write(2) or send(2) system calls. If more than one async
 * request is outstanding to write large buffers into a socket, every
 * request must individually be completed before the next one begins,
 * even if multiple syscalls are required.
 *
 * Take a look at @ref tevent_queue_tutorial for more details.
 * @{
 */

struct tevent_queue;

#ifdef DOXYGEN
/**
 * @brief Create and start a tevent queue.
 *
 * @param[in]  mem_ctx  The talloc memory context to allocate the queue.
 *
 * @param[in]  name     The name to use to identify the queue.
 *
 * @return              An allocated tevent queue on success, NULL on error.
 *
 * @see tevent_start()
 * @see tevent_stop()
 */
struct tevent_queue *tevent_queue_create(TALLOC_CTX *mem_ctx,
					 const char *name);
#else
struct tevent_queue *_tevent_queue_create(TALLOC_CTX *mem_ctx,
					  const char *name,
					  const char *location);

#define tevent_queue_create(_mem_ctx, _name) \
	_tevent_queue_create((_mem_ctx), (_name), __location__)
#endif

/**
 * @brief A callback trigger function run by the queue.
 *
 * @param[in]  req      The tevent request the trigger function is executed on.
 *
 * @param[in]  private_data The private data pointer specified by
 *                          tevent_queue_add().
 *
 * @see tevent_queue_add()
 */
typedef void (*tevent_queue_trigger_fn_t)(struct tevent_req *req,
					  void *private_data);

/**
 * @brief Add a tevent request to the queue.
 *
 * @param[in]  queue    The queue to add the request.
 *
 * @param[in]  ev       The event handle to use for the request.
 *
 * @param[in]  req      The tevent request to add to the queue.
 *
 * @param[in]  trigger  The function triggered by the queue when the request
 *                      is called.
 *
 * @param[in]  private_data The private data passed to the trigger function.
 *
 * @return              True if the request has been successfully added, false
 *                      otherwise.
 */
bool tevent_queue_add(struct tevent_queue *queue,
		      struct tevent_context *ev,
		      struct tevent_req *req,
		      tevent_queue_trigger_fn_t trigger,
		      void *private_data);

/**
 * @brief Start a tevent queue.
 *
 * The queue is started by default.
 *
 * @param[in]  queue    The queue to start.
 */
void tevent_queue_start(struct tevent_queue *queue);

/**
 * @brief Stop a tevent queue.
 *
 * The queue is started by default.
 *
 * @param[in]  queue    The queue to stop.
 */
void tevent_queue_stop(struct tevent_queue *queue);

/**
 * @brief Get the length of the queue.
 *
 * @param[in]  queue    The queue to get the length from.
 *
 * @return              The number of elements.
 */
size_t tevent_queue_length(struct tevent_queue *queue);

typedef int (*tevent_nesting_hook)(struct tevent_context *ev,
				   void *private_data,
				   uint32_t level,
				   bool begin,
				   void *stack_ptr,
				   const char *location);
#ifdef TEVENT_DEPRECATED
#ifndef _DEPRECATED_
#if (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1 )
#define _DEPRECATED_ __attribute__ ((deprecated))
#else
#define _DEPRECATED_
#endif
#endif
void tevent_loop_allow_nesting(struct tevent_context *ev) _DEPRECATED_;
void tevent_loop_set_nesting_hook(struct tevent_context *ev,
				  tevent_nesting_hook hook,
				  void *private_data) _DEPRECATED_;
int _tevent_loop_until(struct tevent_context *ev,
		       bool (*finished)(void *private_data),
		       void *private_data,
		       const char *location) _DEPRECATED_;
#define tevent_loop_until(ev, finished, private_data) \
	_tevent_loop_until(ev, finished, private_data, __location__)
#endif

int tevent_re_initialise(struct tevent_context *ev);

/* @} */

/**
 * @defgroup tevent_ops The tevent operation functions
 * @ingroup tevent
 *
 * The following structure and registration functions are exclusively
 * needed for people writing and pluggin a different event engine.
 * There is nothing useful for normal tevent user in here.
 * @{
 */

struct tevent_ops {
	/* context init */
	int (*context_init)(struct tevent_context *ev);

	/* fd_event functions */
	struct tevent_fd *(*add_fd)(struct tevent_context *ev,
				    TALLOC_CTX *mem_ctx,
				    int fd, uint16_t flags,
				    tevent_fd_handler_t handler,
				    void *private_data,
				    const char *handler_name,
				    const char *location);
	void (*set_fd_close_fn)(struct tevent_fd *fde,
				tevent_fd_close_fn_t close_fn);
	uint16_t (*get_fd_flags)(struct tevent_fd *fde);
	void (*set_fd_flags)(struct tevent_fd *fde, uint16_t flags);

	/* timed_event functions */
	struct tevent_timer *(*add_timer)(struct tevent_context *ev,
					  TALLOC_CTX *mem_ctx,
					  struct timeval next_event,
					  tevent_timer_handler_t handler,
					  void *private_data,
					  const char *handler_name,
					  const char *location);

	/* immediate event functions */
	void (*schedule_immediate)(struct tevent_immediate *im,
				   struct tevent_context *ev,
				   tevent_immediate_handler_t handler,
				   void *private_data,
				   const char *handler_name,
				   const char *location);

	/* signal functions */
	struct tevent_signal *(*add_signal)(struct tevent_context *ev,
					    TALLOC_CTX *mem_ctx,
					    int signum, int sa_flags,
					    tevent_signal_handler_t handler,
					    void *private_data,
					    const char *handler_name,
					    const char *location);

	/* loop functions */
	int (*loop_once)(struct tevent_context *ev, const char *location);
	int (*loop_wait)(struct tevent_context *ev, const char *location);
};

bool tevent_register_backend(const char *name, const struct tevent_ops *ops);

/* @} */

/**
 * @defgroup tevent_compat The tevent compatibility functions
 * @ingroup tevent
 *
 * The following definitions are usueful only for compatibility with the
 * implementation originally developed within the samba4 code and will be
 * soon removed. Please NEVER use in new code.
 *
 * @todo Ignore it?
 *
 * @{
 */

#ifdef TEVENT_COMPAT_DEFINES

#define event_context	tevent_context
#define event_ops	tevent_ops
#define fd_event	tevent_fd
#define timed_event	tevent_timer
#define signal_event	tevent_signal

#define event_fd_handler_t	tevent_fd_handler_t
#define event_timed_handler_t	tevent_timer_handler_t
#define event_signal_handler_t	tevent_signal_handler_t

#define event_context_init(mem_ctx) \
	tevent_context_init(mem_ctx)

#define event_context_init_byname(mem_ctx, name) \
	tevent_context_init_byname(mem_ctx, name)

#define event_backend_list(mem_ctx) \
	tevent_backend_list(mem_ctx)

#define event_set_default_backend(backend) \
	tevent_set_default_backend(backend)

#define event_add_fd(ev, mem_ctx, fd, flags, handler, private_data) \
	tevent_add_fd(ev, mem_ctx, fd, flags, handler, private_data)

#define event_add_timed(ev, mem_ctx, next_event, handler, private_data) \
	tevent_add_timer(ev, mem_ctx, next_event, handler, private_data)

#define event_add_signal(ev, mem_ctx, signum, sa_flags, handler, private_data) \
	tevent_add_signal(ev, mem_ctx, signum, sa_flags, handler, private_data)

#define event_loop_once(ev) \
	tevent_loop_once(ev)

#define event_loop_wait(ev) \
	tevent_loop_wait(ev)

#define event_get_fd_flags(fde) \
	tevent_fd_get_flags(fde)

#define event_set_fd_flags(fde, flags) \
	tevent_fd_set_flags(fde, flags)

#define EVENT_FD_READ		TEVENT_FD_READ
#define EVENT_FD_WRITE		TEVENT_FD_WRITE

#define EVENT_FD_WRITEABLE(fde) \
	TEVENT_FD_WRITEABLE(fde)

#define EVENT_FD_READABLE(fde) \
	TEVENT_FD_READABLE(fde)

#define EVENT_FD_NOT_WRITEABLE(fde) \
	TEVENT_FD_NOT_WRITEABLE(fde)

#define EVENT_FD_NOT_READABLE(fde) \
	TEVENT_FD_NOT_READABLE(fde)

#define ev_debug_level		tevent_debug_level

#define EV_DEBUG_FATAL		TEVENT_DEBUG_FATAL
#define EV_DEBUG_ERROR		TEVENT_DEBUG_ERROR
#define EV_DEBUG_WARNING	TEVENT_DEBUG_WARNING
#define EV_DEBUG_TRACE		TEVENT_DEBUG_TRACE

#define ev_set_debug(ev, debug, context) \
	tevent_set_debug(ev, debug, context)

#define ev_set_debug_stderr(_ev) tevent_set_debug_stderr(ev)

#endif /* TEVENT_COMPAT_DEFINES */

/* @} */

#endif /* __TEVENT_H__ */
