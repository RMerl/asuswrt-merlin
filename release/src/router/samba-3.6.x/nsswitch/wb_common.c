/*
   Unix SMB/CIFS implementation.

   winbind client common code

   Copyright (C) Tim Potter 2000
   Copyright (C) Andrew Tridgell 2000
   Copyright (C) Andrew Bartlett 2002


   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "system/select.h"
#include "winbind_client.h"

/* Global variables.  These are effectively the client state information */

int winbindd_fd = -1;           /* fd for winbindd socket */
static int is_privileged = 0;

/* Free a response structure */

void winbindd_free_response(struct winbindd_response *response)
{
	/* Free any allocated extra_data */

	if (response)
		SAFE_FREE(response->extra_data.data);
}

/* Initialise a request structure */

static void winbindd_init_request(struct winbindd_request *request,
				  int request_type)
{
	request->length = sizeof(struct winbindd_request);

	request->cmd = (enum winbindd_cmd)request_type;
	request->pid = getpid();

}

/* Initialise a response structure */

static void init_response(struct winbindd_response *response)
{
	/* Initialise return value */

	response->result = WINBINDD_ERROR;
}

/* Close established socket */

#if HAVE_FUNCTION_ATTRIBUTE_DESTRUCTOR
__attribute__((destructor))
#endif
static void winbind_close_sock(void)
{
	if (winbindd_fd != -1) {
		close(winbindd_fd);
		winbindd_fd = -1;
	}
}

#define CONNECT_TIMEOUT 30

/* Make sure socket handle isn't stdin, stdout or stderr */
#define RECURSION_LIMIT 3

static int make_nonstd_fd_internals(int fd, int limit /* Recursion limiter */)
{
	int new_fd;
	if (fd >= 0 && fd <= 2) {
#ifdef F_DUPFD
		if ((new_fd = fcntl(fd, F_DUPFD, 3)) == -1) {
			return -1;
		}
		/* Paranoia */
		if (new_fd < 3) {
			close(new_fd);
			return -1;
		}
		close(fd);
		return new_fd;
#else
		if (limit <= 0)
			return -1;

		new_fd = dup(fd);
		if (new_fd == -1)
			return -1;

		/* use the program stack to hold our list of FDs to close */
		new_fd = make_nonstd_fd_internals(new_fd, limit - 1);
		close(fd);
		return new_fd;
#endif
	}
	return fd;
}

/****************************************************************************
 Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
 else
 if SYSV use O_NDELAY
 if BSD use FNDELAY
 Set close on exec also.
****************************************************************************/

static int make_safe_fd(int fd)
{
	int result, flags;
	int new_fd = make_nonstd_fd_internals(fd, RECURSION_LIMIT);
	if (new_fd == -1) {
		close(fd);
		return -1;
	}

	/* Socket should be nonblocking. */
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif

	if ((flags = fcntl(new_fd, F_GETFL)) == -1) {
		close(new_fd);
		return -1;
	}

	flags |= FLAG_TO_SET;
	if (fcntl(new_fd, F_SETFL, flags) == -1) {
		close(new_fd);
		return -1;
	}

#undef FLAG_TO_SET

	/* Socket should be closed on exec() */
#ifdef FD_CLOEXEC
	result = flags = fcntl(new_fd, F_GETFD, 0);
	if (flags >= 0) {
		flags |= FD_CLOEXEC;
		result = fcntl( new_fd, F_SETFD, flags );
	}
	if (result < 0) {
		close(new_fd);
		return -1;
	}
#endif
	return new_fd;
}

/* Connect to winbindd socket */

static int winbind_named_pipe_sock(const char *dir)
{
	struct sockaddr_un sunaddr;
	struct stat st;
	char *path = NULL;
	int fd;
	int wait_time;
	int slept;

	/* Check permissions on unix socket directory */

	if (lstat(dir, &st) == -1) {
		errno = ENOENT;
		return -1;
	}

	if (!S_ISDIR(st.st_mode) ||
	    (st.st_uid != 0 && st.st_uid != geteuid())) {
		errno = ENOENT;
		return -1;
	}

	/* Connect to socket */

	if (asprintf(&path, "%s/%s", dir, WINBINDD_SOCKET_NAME) < 0) {
		return -1;
	}

	ZERO_STRUCT(sunaddr);
	sunaddr.sun_family = AF_UNIX;
	strncpy(sunaddr.sun_path, path, sizeof(sunaddr.sun_path) - 1);

	/* If socket file doesn't exist, don't bother trying to connect
	   with retry.  This is an attempt to make the system usable when
	   the winbindd daemon is not running. */

	if (lstat(path, &st) == -1) {
		errno = ENOENT;
		SAFE_FREE(path);
		return -1;
	}

	SAFE_FREE(path);
	/* Check permissions on unix socket file */

	if (!S_ISSOCK(st.st_mode) ||
	    (st.st_uid != 0 && st.st_uid != geteuid())) {
		errno = ENOENT;
		return -1;
	}

	/* Connect to socket */

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return -1;
	}

	/* Set socket non-blocking and close on exec. */

	if ((fd = make_safe_fd( fd)) == -1) {
		return fd;
	}

	for (wait_time = 0; connect(fd, (struct sockaddr *)&sunaddr, sizeof(sunaddr)) == -1;
			wait_time += slept) {
		struct pollfd pfd;
		int ret;
		int connect_errno = 0;
		socklen_t errnosize;

		if (wait_time >= CONNECT_TIMEOUT)
			goto error_out;

		switch (errno) {
			case EINPROGRESS:
				pfd.fd = fd;
				pfd.events = POLLOUT;

				ret = poll(&pfd, 1, (CONNECT_TIMEOUT - wait_time) * 1000);

				if (ret > 0) {
					errnosize = sizeof(connect_errno);

					ret = getsockopt(fd, SOL_SOCKET,
							SO_ERROR, &connect_errno, &errnosize);

					if (ret >= 0 && connect_errno == 0) {
						/* Connect succeed */
						goto out;
					}
				}

				slept = CONNECT_TIMEOUT;
				break;
			case EAGAIN:
				slept = rand() % 3 + 1;
				sleep(slept);
				break;
			default:
				goto error_out;
		}

	}

  out:

	return fd;

  error_out:

	close(fd);
	return -1;
}

static const char *winbindd_socket_dir(void)
{
#ifdef SOCKET_WRAPPER
	const char *env_dir;

	env_dir = getenv(WINBINDD_SOCKET_DIR_ENVVAR);
	if (env_dir) {
		return env_dir;
	}
#endif

	return WINBINDD_SOCKET_DIR;
}

/* Connect to winbindd socket */

static int winbind_open_pipe_sock(int recursing, int need_priv)
{
#ifdef HAVE_UNIXSOCKET
	static pid_t our_pid;
	struct winbindd_request request;
	struct winbindd_response response;
	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	if (our_pid != getpid()) {
		winbind_close_sock();
		our_pid = getpid();
	}

	if ((need_priv != 0) && (is_privileged == 0)) {
		winbind_close_sock();
	}

	if (winbindd_fd != -1) {
		return winbindd_fd;
	}

	if (recursing) {
		return -1;
	}

	if ((winbindd_fd = winbind_named_pipe_sock(winbindd_socket_dir())) == -1) {
		return -1;
	}

	is_privileged = 0;

	/* version-check the socket */

	request.wb_flags = WBFLAG_RECURSE;
	if ((winbindd_request_response(WINBINDD_INTERFACE_VERSION, &request, &response) != NSS_STATUS_SUCCESS) || (response.data.interface_version != WINBIND_INTERFACE_VERSION)) {
		winbind_close_sock();
		return -1;
	}

	/* try and get priv pipe */

	request.wb_flags = WBFLAG_RECURSE;
	if (winbindd_request_response(WINBINDD_PRIV_PIPE_DIR, &request, &response) == NSS_STATUS_SUCCESS) {
		int fd;
		if ((fd = winbind_named_pipe_sock((char *)response.extra_data.data)) != -1) {
			close(winbindd_fd);
			winbindd_fd = fd;
			is_privileged = 1;
		}
	}

	if ((need_priv != 0) && (is_privileged == 0)) {
		return -1;
	}

	SAFE_FREE(response.extra_data.data);

	return winbindd_fd;
#else
	return -1;
#endif /* HAVE_UNIXSOCKET */
}

/* Write data to winbindd socket */

static int winbind_write_sock(void *buffer, int count, int recursing,
			      int need_priv)
{
	int result, nwritten;

	/* Open connection to winbind daemon */

 restart:

	if (winbind_open_pipe_sock(recursing, need_priv) == -1) {
		errno = ENOENT;
		return -1;
	}

	/* Write data to socket */

	nwritten = 0;

	while(nwritten < count) {
		struct pollfd pfd;
		int ret;

		/* Catch pipe close on other end by checking if a read()
		   call would not block by calling poll(). */

		pfd.fd = winbindd_fd;
		pfd.events = POLLIN|POLLOUT|POLLHUP;

		ret = poll(&pfd, 1, -1);
		if (ret == -1) {
			winbind_close_sock();
			return -1;                   /* poll error */
		}

		/* Write should be OK if fd not available for reading */

		if ((ret == 1) && (pfd.revents & (POLLIN|POLLHUP|POLLERR))) {

			/* Pipe has closed on remote end */

			winbind_close_sock();
			goto restart;
		}

		/* Do the write */

		result = write(winbindd_fd,
			       (char *)buffer + nwritten,
			       count - nwritten);

		if ((result == -1) || (result == 0)) {

			/* Write failed */

			winbind_close_sock();
			return -1;
		}

		nwritten += result;
	}

	return nwritten;
}

/* Read data from winbindd socket */

static int winbind_read_sock(void *buffer, int count)
{
	int nread = 0;
	int total_time = 0;

	if (winbindd_fd == -1) {
		return -1;
	}

	/* Read data from socket */
	while(nread < count) {
		struct pollfd pfd;
		int ret;

		/* Catch pipe close on other end by checking if a read()
		   call would not block by calling poll(). */

		pfd.fd = winbindd_fd;
		pfd.events = POLLIN|POLLHUP;

		/* Wait for 5 seconds for a reply. May need to parameterise this... */

		ret = poll(&pfd, 1, 5000);
		if (ret == -1) {
			winbind_close_sock();
			return -1;                   /* poll error */
		}

		if (ret == 0) {
			/* Not ready for read yet... */
			if (total_time >= 30) {
				/* Timeout */
				winbind_close_sock();
				return -1;
			}
			total_time += 5;
			continue;
		}

		if ((ret == 1) && (pfd.revents & (POLLIN|POLLHUP|POLLERR))) {

			/* Do the Read */

			int result = read(winbindd_fd, (char *)buffer + nread,
			      count - nread);

			if ((result == -1) || (result == 0)) {

				/* Read failed.  I think the only useful thing we
				   can do here is just return -1 and fail since the
				   transaction has failed half way through. */

				winbind_close_sock();
				return -1;
			}

			nread += result;

		}
	}

	return nread;
}

/* Read reply */

static int winbindd_read_reply(struct winbindd_response *response)
{
	int result1, result2 = 0;

	if (!response) {
		return -1;
	}

	/* Read fixed length response */

	result1 = winbind_read_sock(response,
				    sizeof(struct winbindd_response));
	if (result1 == -1) {
		return -1;
	}

	if (response->length < sizeof(struct winbindd_response)) {
		return -1;
	}

	/* We actually send the pointer value of the extra_data field from
	   the server.  This has no meaning in the client's address space
	   so we clear it out. */

	response->extra_data.data = NULL;

	/* Read variable length response */

	if (response->length > sizeof(struct winbindd_response)) {
		int extra_data_len = response->length -
			sizeof(struct winbindd_response);

		/* Mallocate memory for extra data */

		if (!(response->extra_data.data = malloc(extra_data_len))) {
			return -1;
		}

		result2 = winbind_read_sock(response->extra_data.data,
					    extra_data_len);
		if (result2 == -1) {
			winbindd_free_response(response);
			return -1;
		}
	}

	/* Return total amount of data read */

	return result1 + result2;
}

/*
 * send simple types of requests
 */

NSS_STATUS winbindd_send_request(int req_type, int need_priv,
				 struct winbindd_request *request)
{
	struct winbindd_request lrequest;

	/* Check for our tricky environment variable */

	if (winbind_env_set()) {
		return NSS_STATUS_NOTFOUND;
	}

	if (!request) {
		ZERO_STRUCT(lrequest);
		request = &lrequest;
	}

	/* Fill in request and send down pipe */

	winbindd_init_request(request, req_type);

	if (winbind_write_sock(request, sizeof(*request),
			       request->wb_flags & WBFLAG_RECURSE,
			       need_priv) == -1)
	{
		/* Set ENOENT for consistency.  Required by some apps */
		errno = ENOENT;

		return NSS_STATUS_UNAVAIL;
	}

	if ((request->extra_len != 0) &&
	    (winbind_write_sock(request->extra_data.data,
				request->extra_len,
				request->wb_flags & WBFLAG_RECURSE,
				need_priv) == -1))
	{
		/* Set ENOENT for consistency.  Required by some apps */
		errno = ENOENT;

		return NSS_STATUS_UNAVAIL;
	}

	return NSS_STATUS_SUCCESS;
}

/*
 * Get results from winbindd request
 */

NSS_STATUS winbindd_get_response(struct winbindd_response *response)
{
	struct winbindd_response lresponse;

	if (!response) {
		ZERO_STRUCT(lresponse);
		response = &lresponse;
	}

	init_response(response);

	/* Wait for reply */
	if (winbindd_read_reply(response) == -1) {
		/* Set ENOENT for consistency.  Required by some apps */
		errno = ENOENT;

		return NSS_STATUS_UNAVAIL;
	}

	/* Throw away extra data if client didn't request it */
	if (response == &lresponse) {
		winbindd_free_response(response);
	}

	/* Copy reply data from socket */
	if (response->result != WINBINDD_OK) {
		return NSS_STATUS_NOTFOUND;
	}

	return NSS_STATUS_SUCCESS;
}

/* Handle simple types of requests */

NSS_STATUS winbindd_request_response(int req_type,
			    struct winbindd_request *request,
			    struct winbindd_response *response)
{
	NSS_STATUS status = NSS_STATUS_UNAVAIL;
	int count = 0;

	while ((status == NSS_STATUS_UNAVAIL) && (count < 10)) {
		status = winbindd_send_request(req_type, 0, request);
		if (status != NSS_STATUS_SUCCESS)
			return(status);
		status = winbindd_get_response(response);
		count += 1;
	}

	return status;
}

NSS_STATUS winbindd_priv_request_response(int req_type,
					  struct winbindd_request *request,
					  struct winbindd_response *response)
{
	NSS_STATUS status = NSS_STATUS_UNAVAIL;
	int count = 0;

	while ((status == NSS_STATUS_UNAVAIL) && (count < 10)) {
		status = winbindd_send_request(req_type, 1, request);
		if (status != NSS_STATUS_SUCCESS)
			return(status);
		status = winbindd_get_response(response);
		count += 1;
	}

	return status;
}
