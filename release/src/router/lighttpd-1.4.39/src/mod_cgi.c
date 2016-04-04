#include "server.h"
#include "stat_cache.h"
#include "keyvalue.h"
#include "log.h"
#include "connections.h"
#include "joblist.h"
#include "http_chunk.h"
#include "network_backends.h"

#include "plugin.h"

#include <sys/types.h>

#ifdef __WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <sys/wait.h>
# include <sys/mman.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fdevent.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>
#include <fcntl.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "version.h"

enum {EOL_UNSET, EOL_N, EOL_RN};

typedef struct {
	char **ptr;

	size_t size;
	size_t used;
} char_array;

typedef struct {
	pid_t *ptr;
	size_t used;
	size_t size;
} buffer_pid_t;

typedef struct {
	array *cgi;
	unsigned short execute_x_only;
} plugin_config;

typedef struct {
	PLUGIN_DATA;
	buffer_pid_t cgi_pid;

	buffer *tmp_buf;
	buffer *parse_response;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

typedef struct {
	pid_t pid;
	int fd;
	int fde_ndx; /* index into the fd-event buffer */

	connection *remote_conn;  /* dumb pointer */
	plugin_data *plugin_data; /* dumb pointer */

	buffer *response;
	buffer *response_header;
} handler_ctx;

static handler_ctx * cgi_handler_ctx_init(void) {
	handler_ctx *hctx = calloc(1, sizeof(*hctx));

	force_assert(hctx);

	hctx->response = buffer_init();
	hctx->response_header = buffer_init();

	return hctx;
}

static void cgi_handler_ctx_free(handler_ctx *hctx) {
	buffer_free(hctx->response);
	buffer_free(hctx->response_header);

	free(hctx);
}

enum {FDEVENT_HANDLED_UNSET, FDEVENT_HANDLED_FINISHED, FDEVENT_HANDLED_NOT_FINISHED, FDEVENT_HANDLED_ERROR};

INIT_FUNC(mod_cgi_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	force_assert(p);

	p->tmp_buf = buffer_init();
	p->parse_response = buffer_init();

	return p;
}


FREE_FUNC(mod_cgi_free) {
	plugin_data *p = p_d;
	buffer_pid_t *r = &(p->cgi_pid);

	UNUSED(srv);

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->cgi);

			free(s);
		}
		free(p->config_storage);
	}


	if (r->ptr) free(r->ptr);

	buffer_free(p->tmp_buf);
	buffer_free(p->parse_response);

	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(mod_fastcgi_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "cgi.assign",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "cgi.execute-x-only",          NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },     /* 1 */
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET}
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));
	force_assert(p->config_storage);

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		force_assert(s);

		s->cgi    = array_init();
		s->execute_x_only = 0;

		cv[0].destination = s->cgi;
		cv[1].destination = &(s->execute_x_only);

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}


static int cgi_pid_add(server *srv, plugin_data *p, pid_t pid) {
	int m = -1;
	size_t i;
	buffer_pid_t *r = &(p->cgi_pid);

	UNUSED(srv);

	for (i = 0; i < r->used; i++) {
		if (r->ptr[i] > m) m = r->ptr[i];
	}

	if (r->size == 0) {
		r->size = 16;
		r->ptr = malloc(sizeof(*r->ptr) * r->size);
		force_assert(r->ptr);
	} else if (r->used == r->size) {
		r->size += 16;
		r->ptr = realloc(r->ptr, sizeof(*r->ptr) * r->size);
		force_assert(r->ptr);
	}

	r->ptr[r->used++] = pid;

	return m;
}

static int cgi_pid_del(server *srv, plugin_data *p, pid_t pid) {
	size_t i;
	buffer_pid_t *r = &(p->cgi_pid);

	UNUSED(srv);

	for (i = 0; i < r->used; i++) {
		if (r->ptr[i] == pid) break;
	}

	if (i != r->used) {
		/* found */

		if (i != r->used - 1) {
			r->ptr[i] = r->ptr[r->used - 1];
		}
		r->used--;
	}

	return 0;
}

static int cgi_response_parse(server *srv, connection *con, plugin_data *p, buffer *in) {
	char *ns;
	const char *s;
	int line = 0;

	UNUSED(srv);

	buffer_copy_buffer(p->parse_response, in);

	for (s = p->parse_response->ptr;
	     NULL != (ns = strchr(s, '\n'));
	     s = ns + 1, line++) {
		const char *key, *value;
		int key_len;
		data_string *ds;

		/* strip the \n */
		ns[0] = '\0';

		if (ns > s && ns[-1] == '\r') ns[-1] = '\0';

		if (line == 0 &&
		    0 == strncmp(s, "HTTP/1.", 7)) {
			/* non-parsed header ... we parse them anyway */

			if ((s[7] == '1' ||
			     s[7] == '0') &&
			    s[8] == ' ') {
				int status;
				/* after the space should be a status code for us */

				status = strtol(s+9, NULL, 10);

				if (status >= 100 &&
				    status < 1000) {
					/* we expected 3 digits and didn't got them */
					con->parsed_response |= HTTP_STATUS;
					con->http_status = status;
				}
			}
		} else {
			/* parse the headers */
			key = s;
			if (NULL == (value = strchr(s, ':'))) {
				/* we expect: "<key>: <value>\r\n" */
				continue;
			}

			key_len = value - key;
			value += 1;

			/* skip LWS */
			while (*value == ' ' || *value == '\t') value++;

			if (NULL == (ds = (data_string *)array_get_unused_element(con->response.headers, TYPE_STRING))) {
				ds = data_response_init();
			}
			buffer_copy_string_len(ds->key, key, key_len);
			buffer_copy_string(ds->value, value);

			array_insert_unique(con->response.headers, (data_unset *)ds);

			switch(key_len) {
			case 4:
				if (0 == strncasecmp(key, "Date", key_len)) {
					con->parsed_response |= HTTP_DATE;
				}
				break;
			case 6:
				if (0 == strncasecmp(key, "Status", key_len)) {
					con->http_status = strtol(value, NULL, 10);
					con->parsed_response |= HTTP_STATUS;
				}
				break;
			case 8:
				if (0 == strncasecmp(key, "Location", key_len)) {
					con->parsed_response |= HTTP_LOCATION;
				}
				break;
			case 10:
				if (0 == strncasecmp(key, "Connection", key_len)) {
					con->response.keep_alive = (0 == strcasecmp(value, "Keep-Alive")) ? 1 : 0;
					con->parsed_response |= HTTP_CONNECTION;
				}
				break;
			case 14:
				if (0 == strncasecmp(key, "Content-Length", key_len)) {
					con->response.content_length = strtol(value, NULL, 10);
					con->parsed_response |= HTTP_CONTENT_LENGTH;
				}
				break;
			default:
				break;
			}
		}
	}

	/* CGI/1.1 rev 03 - 7.2.1.2 */
	if ((con->parsed_response & HTTP_LOCATION) &&
	    !(con->parsed_response & HTTP_STATUS)) {
		con->http_status = 302;
	}

	return 0;
}


static int cgi_demux_response(server *srv, handler_ctx *hctx) {
	plugin_data *p    = hctx->plugin_data;
	connection  *con  = hctx->remote_conn;

	while(1) {
		int n;
		int toread;

#if defined(__WIN32)
		buffer_string_prepare_copy(hctx->response, 4 * 1024);
#else
		if (ioctl(con->fd, FIONREAD, &toread) || toread == 0 || toread <= 4*1024) {
			buffer_string_prepare_copy(hctx->response, 4 * 1024);
		} else {
			if (toread > MAX_READ_LIMIT) toread = MAX_READ_LIMIT;
			buffer_string_prepare_copy(hctx->response, toread);
		}
#endif

		if (-1 == (n = read(hctx->fd, hctx->response->ptr, hctx->response->size - 1))) {
			if (errno == EAGAIN || errno == EINTR) {
				/* would block, wait for signal */
				return FDEVENT_HANDLED_NOT_FINISHED;
			}
			/* error */
			log_error_write(srv, __FILE__, __LINE__, "sdd", strerror(errno), con->fd, hctx->fd);
			return FDEVENT_HANDLED_ERROR;
		}

		if (n == 0) {
			/* read finished */

			con->file_finished = 1;

			/* send final chunk */
			http_chunk_close(srv, con);
			joblist_append(srv, con);

			return FDEVENT_HANDLED_FINISHED;
		}

		buffer_commit(hctx->response, n);

		/* split header from body */

		if (con->file_started == 0) {
			int is_header = 0;
			int is_header_end = 0;
			size_t last_eol = 0;
			size_t i, header_len;

			buffer_append_string_buffer(hctx->response_header, hctx->response);

			/**
			 * we have to handle a few cases:
			 *
			 * nph:
			 * 
			 *   HTTP/1.0 200 Ok\n
			 *   Header: Value\n
			 *   \n
			 *
			 * CGI:
			 *   Header: Value\n
			 *   Status: 200\n
			 *   \n
			 *
			 * and different mixes of \n and \r\n combinations
			 * 
			 * Some users also forget about CGI and just send a response and hope 
			 * we handle it. No headers, no header-content seperator
			 * 
			 */
			
			/* nph (non-parsed headers) */
			if (0 == strncmp(hctx->response_header->ptr, "HTTP/1.", 7)) is_header = 1;

			header_len = buffer_string_length(hctx->response_header);
			for (i = 0; !is_header_end && i < header_len; i++) {
				char c = hctx->response_header->ptr[i];

				switch (c) {
				case ':':
					/* we found a colon
					 *
					 * looks like we have a normal header 
					 */
					is_header = 1;
					break;
				case '\n':
					/* EOL */
					if (is_header == 0) {
						/* we got a EOL but we don't seem to got a HTTP header */

						is_header_end = 1;

						break;
					}

					/**
					 * check if we saw a \n(\r)?\n sequence 
					 */
					if (last_eol > 0 && 
					    ((i - last_eol == 1) || 
					     (i - last_eol == 2 && hctx->response_header->ptr[i - 1] == '\r'))) {
						is_header_end = 1;
						break;
					}

					last_eol = i;

					break;
				}
			}

			if (is_header_end) {
				if (!is_header) {
					/* no header, but a body */

					if (con->request.http_version == HTTP_VERSION_1_1) {
						con->response.transfer_encoding = HTTP_TRANSFER_ENCODING_CHUNKED;
					}

					http_chunk_append_buffer(srv, con, hctx->response_header);
					joblist_append(srv, con);
				} else {
					const char *bstart;
					size_t blen;

					/* the body starts after the EOL */
					bstart = hctx->response_header->ptr + i;
					blen = header_len - i;

					/**
					 * i still points to the char after the terminating EOL EOL
					 *
					 * put it on the last \n again
					 */
					i--;

					/* string the last \r?\n */
					if (i > 0 && (hctx->response_header->ptr[i - 1] == '\r')) {
						i--;
					}

					buffer_string_set_length(hctx->response_header, i);

					/* parse the response header */
					cgi_response_parse(srv, con, p, hctx->response_header);

					/* enable chunked-transfer-encoding */
					if (con->request.http_version == HTTP_VERSION_1_1 &&
					    !(con->parsed_response & HTTP_CONTENT_LENGTH)) {
						con->response.transfer_encoding = HTTP_TRANSFER_ENCODING_CHUNKED;
					}

					if (blen > 0) {
						http_chunk_append_mem(srv, con, bstart, blen);
						joblist_append(srv, con);
					}
				}

				con->file_started = 1;
			}
		} else {
			http_chunk_append_buffer(srv, con, hctx->response);
			joblist_append(srv, con);
		}

#if 0
		log_error_write(srv, __FILE__, __LINE__, "ddss", con->fd, hctx->fd, connection_get_state(con->state), b->ptr);
#endif
	}

	return FDEVENT_HANDLED_NOT_FINISHED;
}

static handler_t cgi_connection_close(server *srv, handler_ctx *hctx) {
	int status;
	pid_t pid;
	plugin_data *p;
	connection  *con;

	if (NULL == hctx) return HANDLER_GO_ON;

	p    = hctx->plugin_data;
	con  = hctx->remote_conn;

	if (con->mode != p->id) return HANDLER_GO_ON;

#ifndef __WIN32

	/* the connection to the browser went away, but we still have a connection
	 * to the CGI script
	 *
	 * close cgi-connection
	 */

	if (hctx->fd != -1) {
		/* close connection to the cgi-script */
		fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
		fdevent_unregister(srv->ev, hctx->fd);

		if (close(hctx->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "sds", "cgi close failed ", hctx->fd, strerror(errno));
		}

		hctx->fd = -1;
		hctx->fde_ndx = -1;
	}

	pid = hctx->pid;

	con->plugin_ctx[p->id] = NULL;

	/* is this a good idea ? */
	cgi_handler_ctx_free(hctx);

	/* if waitpid hasn't been called by response.c yet, do it here */
	if (pid) {
		/* check if the CGI-script is already gone */
		switch(waitpid(pid, &status, WNOHANG)) {
		case 0:
			/* not finished yet */
#if 0
			log_error_write(srv, __FILE__, __LINE__, "sd", "(debug) child isn't done yet, pid:", pid);
#endif
			break;
		case -1:
			/* */
			if (errno == EINTR) break;

			/*
			 * errno == ECHILD happens if _subrequest catches the process-status before
			 * we have read the response of the cgi process
			 *
			 * -> catch status
			 * -> WAIT_FOR_EVENT
			 * -> read response
			 * -> we get here with waitpid == ECHILD
			 *
			 */
			if (errno == ECHILD) return HANDLER_GO_ON;

			log_error_write(srv, __FILE__, __LINE__, "ss", "waitpid failed: ", strerror(errno));
			return HANDLER_ERROR;
		default:
			/* Send an error if we haven't sent any data yet */
			if (0 == con->file_started) {
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				con->http_status = 500;
				con->mode = DIRECT;
			} else {
				con->file_finished = 1;
			}

			if (WIFEXITED(status)) {
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sd", "(debug) cgi exited fine, pid:", pid);
#endif
				return HANDLER_GO_ON;
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sd", "cgi died, pid:", pid);
				return HANDLER_GO_ON;
			}
		}


		kill(pid, SIGTERM);

		/* cgi-script is still alive, queue the PID for removal */
		cgi_pid_add(srv, p, pid);
	}
#endif
	return HANDLER_GO_ON;
}

static handler_t cgi_connection_close_callback(server *srv, connection *con, void *p_d) {
	plugin_data *p = p_d;

	return cgi_connection_close(srv, con->plugin_ctx[p->id]);
}


static handler_t cgi_handle_fdevent(server *srv, void *ctx, int revents) {
	handler_ctx *hctx = ctx;
	connection  *con  = hctx->remote_conn;

	joblist_append(srv, con);

	if (hctx->fd == -1) {
		log_error_write(srv, __FILE__, __LINE__, "ddss", con->fd, hctx->fd, connection_get_state(con->state), "invalid cgi-fd");

		return HANDLER_ERROR;
	}

	if (revents & FDEVENT_IN) {
		switch (cgi_demux_response(srv, hctx)) {
		case FDEVENT_HANDLED_NOT_FINISHED:
			break;
		case FDEVENT_HANDLED_FINISHED:
			/* we are done */

#if 0
			log_error_write(srv, __FILE__, __LINE__, "ddss", con->fd, hctx->fd, connection_get_state(con->state), "finished");
#endif
			cgi_connection_close(srv, hctx);

			/* if we get a IN|HUP and have read everything don't exec the close twice */
			return HANDLER_FINISHED;
		case FDEVENT_HANDLED_ERROR:
			/* Send an error if we haven't sent any data yet */
			if (0 == con->file_started) {
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				con->http_status = 500;
				con->mode = DIRECT;
			} else {
				con->file_finished = 1;
			}

			log_error_write(srv, __FILE__, __LINE__, "s", "demuxer failed: ");
			break;
		}
	}

	if (revents & FDEVENT_OUT) {
		/* nothing to do */
	}

	/* perhaps this issue is already handled */
	if (revents & FDEVENT_HUP) {
		/* check if we still have a unfinished header package which is a body in reality */
		if (con->file_started == 0 && !buffer_string_is_empty(hctx->response_header)) {
			con->file_started = 1;
			http_chunk_append_buffer(srv, con, hctx->response_header);
		}

		if (con->file_finished == 0) {
			http_chunk_close(srv, con);
		}
		con->file_finished = 1;

		joblist_append(srv, con);

# if 0
		log_error_write(srv, __FILE__, __LINE__, "sddd", "got HUP from cgi", con->fd, hctx->fd, revents);
# endif

		/* rtsigs didn't liked the close */
		cgi_connection_close(srv, hctx);
	} else if (revents & FDEVENT_ERR) {
		con->file_finished = 1;

		/* kill all connections to the cgi process */
		cgi_connection_close(srv, hctx);
#if 1
		log_error_write(srv, __FILE__, __LINE__, "s", "cgi-FDEVENT_ERR");
#endif
		return HANDLER_ERROR;
	}

	return HANDLER_FINISHED;
}


static int cgi_env_add(char_array *env, const char *key, size_t key_len, const char *val, size_t val_len) {
	char *dst;

	if (!key || !val) return -1;

	dst = malloc(key_len + val_len + 2);
	force_assert(dst);
	memcpy(dst, key, key_len);
	dst[key_len] = '=';
	memcpy(dst + key_len + 1, val, val_len);
	dst[key_len + 1 + val_len] = '\0';

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
		force_assert(env->ptr);
	} else if (env->size == env->used) {
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
		force_assert(env->ptr);
	}

	env->ptr[env->used++] = dst;

	return 0;
}

/* returns: 0: continue, -1: fatal error, -2: connection reset */
/* similar to network_write_file_chunk_mmap, but doesn't use send on windows (because we're on pipes),
 * also mmaps and sends complete chunk instead of only small parts - the files
 * are supposed to be temp files with reasonable chunk sizes.
 *
 * Also always use mmap; the files are "trusted", as we created them.
 */
static int cgi_write_file_chunk_mmap(server *srv, connection *con, int fd, chunkqueue *cq) {
	chunk* const c = cq->first;
	off_t offset, toSend, file_end;
	ssize_t r;
	size_t mmap_offset, mmap_avail;
	const char *data;

	force_assert(NULL != c);
	force_assert(FILE_CHUNK == c->type);
	force_assert(c->offset >= 0 && c->offset <= c->file.length);

	offset = c->file.start + c->offset;
	toSend = c->file.length - c->offset;
	file_end = c->file.start + c->file.length; /* offset to file end in this chunk */

	if (0 == toSend) {
		chunkqueue_remove_finished_chunks(cq);
		return 0;
	}

	if (0 != network_open_file_chunk(srv, con, cq)) return -1;

	/* (re)mmap the buffer if range is not covered completely */
	if (MAP_FAILED == c->file.mmap.start
		|| offset < c->file.mmap.offset
		|| file_end > (off_t)(c->file.mmap.offset + c->file.mmap.length)) {

		if (MAP_FAILED != c->file.mmap.start) {
			munmap(c->file.mmap.start, c->file.mmap.length);
			c->file.mmap.start = MAP_FAILED;
		}

		c->file.mmap.offset = mmap_align_offset(offset);
		c->file.mmap.length = file_end - c->file.mmap.offset;

		if (MAP_FAILED == (c->file.mmap.start = mmap(NULL, c->file.mmap.length, PROT_READ, MAP_SHARED, c->file.fd, c->file.mmap.offset))) {
			log_error_write(srv, __FILE__, __LINE__, "ssbdoo", "mmap failed:",
				strerror(errno), c->file.name, c->file.fd, c->file.mmap.offset, (off_t) c->file.mmap.length);
			return -1;
		}
	}

	force_assert(offset >= c->file.mmap.offset);
	mmap_offset = offset - c->file.mmap.offset;
	force_assert(c->file.mmap.length > mmap_offset);
	mmap_avail = c->file.mmap.length - mmap_offset;
	force_assert(toSend <= (off_t) mmap_avail);

	data = c->file.mmap.start + mmap_offset;

	if ((r = write(fd, data, toSend)) < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return 0;
		case EPIPE:
		case ECONNRESET:
			return -2;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd",
				"write failed:", strerror(errno), fd);
			return -1;
		}
	}

	if (r >= 0) {
		chunkqueue_mark_written(cq, r);
	}

	return 0;
}

static int cgi_create_env(server *srv, connection *con, plugin_data *p, buffer *cgi_handler) {
	pid_t pid;

#ifdef HAVE_IPV6
	char b2[INET6_ADDRSTRLEN + 1];
#endif

	int to_cgi_fds[2];
	int from_cgi_fds[2];
	struct stat st;

#ifndef __WIN32

	if (!buffer_string_is_empty(cgi_handler)) {
		/* stat the exec file */
		if (-1 == (stat(cgi_handler->ptr, &st))) {
			log_error_write(srv, __FILE__, __LINE__, "sbss",
					"stat for cgi-handler", cgi_handler,
					"failed:", strerror(errno));
			return -1;
		}
	}

	if (pipe(to_cgi_fds)) {
		log_error_write(srv, __FILE__, __LINE__, "ss", "pipe failed:", strerror(errno));
		return -1;
	}

	if (pipe(from_cgi_fds)) {
		close(to_cgi_fds[0]);
		close(to_cgi_fds[1]);
		log_error_write(srv, __FILE__, __LINE__, "ss", "pipe failed:", strerror(errno));
		return -1;
	}

	/* fork, execve */
	switch (pid = fork()) {
	case 0: {
		/* child */
		char **args;
		int argc;
		int i = 0;
		char buf[LI_ITOSTRING_LENGTH];
		size_t n;
		char_array env;
		char *c;
		const char *s;
		server_socket *srv_sock = con->srv_socket;

		/* move stdout to from_cgi_fd[1] */
		close(STDOUT_FILENO);
		dup2(from_cgi_fds[1], STDOUT_FILENO);
		close(from_cgi_fds[1]);
		/* not needed */
		close(from_cgi_fds[0]);

		/* move the stdin to to_cgi_fd[0] */
		close(STDIN_FILENO);
		dup2(to_cgi_fds[0], STDIN_FILENO);
		close(to_cgi_fds[0]);
		/* not needed */
		close(to_cgi_fds[1]);

		/* create environment */
		env.ptr = NULL;
		env.size = 0;
		env.used = 0;

		if (buffer_is_empty(con->conf.server_tag)) {
			cgi_env_add(&env, CONST_STR_LEN("SERVER_SOFTWARE"), CONST_STR_LEN(PACKAGE_DESC));
		} else {
			cgi_env_add(&env, CONST_STR_LEN("SERVER_SOFTWARE"), CONST_BUF_LEN(con->conf.server_tag));
		}

		if (!buffer_string_is_empty(con->server_name)) {
			size_t len = buffer_string_length(con->server_name);

			if (con->server_name->ptr[0] == '[') {
				const char *colon = strstr(con->server_name->ptr, "]:");
				if (colon) len = (colon + 1) - con->server_name->ptr;
			} else {
				const char *colon = strchr(con->server_name->ptr, ':');
				if (colon) len = colon - con->server_name->ptr;
			}

			cgi_env_add(&env, CONST_STR_LEN("SERVER_NAME"), con->server_name->ptr, len);
		} else {
#ifdef HAVE_IPV6
			s = inet_ntop(
				srv_sock->addr.plain.sa_family,
				srv_sock->addr.plain.sa_family == AF_INET6 ?
				(const void *) &(srv_sock->addr.ipv6.sin6_addr) :
				(const void *) &(srv_sock->addr.ipv4.sin_addr),
				b2, sizeof(b2)-1);
#else
			s = inet_ntoa(srv_sock->addr.ipv4.sin_addr);
#endif
			force_assert(s);
			cgi_env_add(&env, CONST_STR_LEN("SERVER_NAME"), s, strlen(s));
		}
		cgi_env_add(&env, CONST_STR_LEN("GATEWAY_INTERFACE"), CONST_STR_LEN("CGI/1.1"));

		s = get_http_version_name(con->request.http_version);
		force_assert(s);
		cgi_env_add(&env, CONST_STR_LEN("SERVER_PROTOCOL"), s, strlen(s));

		li_utostr(buf,
#ifdef HAVE_IPV6
			ntohs(srv_sock->addr.plain.sa_family == AF_INET6 ? srv_sock->addr.ipv6.sin6_port : srv_sock->addr.ipv4.sin_port)
#else
			ntohs(srv_sock->addr.ipv4.sin_port)
#endif
			);
		cgi_env_add(&env, CONST_STR_LEN("SERVER_PORT"), buf, strlen(buf));

		switch (srv_sock->addr.plain.sa_family) {
#ifdef HAVE_IPV6
		case AF_INET6:
			s = inet_ntop(
				srv_sock->addr.plain.sa_family,
				(const void *) &(srv_sock->addr.ipv6.sin6_addr),
				b2, sizeof(b2)-1);
			break;
		case AF_INET:
			s = inet_ntop(
				srv_sock->addr.plain.sa_family,
				(const void *) &(srv_sock->addr.ipv4.sin_addr),
				b2, sizeof(b2)-1);
			break;
#else
		case AF_INET:
			s = inet_ntoa(srv_sock->addr.ipv4.sin_addr);
			break;
#endif
		default:
			s = "";
			break;
		}
		force_assert(s);
		cgi_env_add(&env, CONST_STR_LEN("SERVER_ADDR"), s, strlen(s));

		s = get_http_method_name(con->request.http_method);
		force_assert(s);
		cgi_env_add(&env, CONST_STR_LEN("REQUEST_METHOD"), s, strlen(s));

		if (!buffer_string_is_empty(con->request.pathinfo)) {
			cgi_env_add(&env, CONST_STR_LEN("PATH_INFO"), CONST_BUF_LEN(con->request.pathinfo));
		}
		cgi_env_add(&env, CONST_STR_LEN("REDIRECT_STATUS"), CONST_STR_LEN("200"));
		if (!buffer_string_is_empty(con->uri.query)) {
			cgi_env_add(&env, CONST_STR_LEN("QUERY_STRING"), CONST_BUF_LEN(con->uri.query));
		}
		if (!buffer_string_is_empty(con->request.orig_uri)) {
			cgi_env_add(&env, CONST_STR_LEN("REQUEST_URI"), CONST_BUF_LEN(con->request.orig_uri));
		}


		switch (con->dst_addr.plain.sa_family) {
#ifdef HAVE_IPV6
		case AF_INET6:
			s = inet_ntop(
				con->dst_addr.plain.sa_family,
				(const void *) &(con->dst_addr.ipv6.sin6_addr),
				b2, sizeof(b2)-1);
			break;
		case AF_INET:
			s = inet_ntop(
				con->dst_addr.plain.sa_family,
				(const void *) &(con->dst_addr.ipv4.sin_addr),
				b2, sizeof(b2)-1);
			break;
#else
		case AF_INET:
			s = inet_ntoa(con->dst_addr.ipv4.sin_addr);
			break;
#endif
		default:
			s = "";
			break;
		}
		force_assert(s);
		cgi_env_add(&env, CONST_STR_LEN("REMOTE_ADDR"), s, strlen(s));

		li_utostr(buf,
#ifdef HAVE_IPV6
			ntohs(con->dst_addr.plain.sa_family == AF_INET6 ? con->dst_addr.ipv6.sin6_port : con->dst_addr.ipv4.sin_port)
#else
			ntohs(con->dst_addr.ipv4.sin_port)
#endif
			);
		cgi_env_add(&env, CONST_STR_LEN("REMOTE_PORT"), buf, strlen(buf));

		if (buffer_is_equal_caseless_string(con->uri.scheme, CONST_STR_LEN("https"))) {
			cgi_env_add(&env, CONST_STR_LEN("HTTPS"), CONST_STR_LEN("on"));
		}

		li_itostr(buf, con->request.content_length);
		cgi_env_add(&env, CONST_STR_LEN("CONTENT_LENGTH"), buf, strlen(buf));
		cgi_env_add(&env, CONST_STR_LEN("SCRIPT_FILENAME"), CONST_BUF_LEN(con->physical.path));
		cgi_env_add(&env, CONST_STR_LEN("SCRIPT_NAME"), CONST_BUF_LEN(con->uri.path));
		cgi_env_add(&env, CONST_STR_LEN("DOCUMENT_ROOT"), CONST_BUF_LEN(con->physical.basedir));

		/* for valgrind */
		if (NULL != (s = getenv("LD_PRELOAD"))) {
			cgi_env_add(&env, CONST_STR_LEN("LD_PRELOAD"), s, strlen(s));
		}

		if (NULL != (s = getenv("LD_LIBRARY_PATH"))) {
			cgi_env_add(&env, CONST_STR_LEN("LD_LIBRARY_PATH"), s, strlen(s));
		}
#ifdef __CYGWIN__
		/* CYGWIN needs SYSTEMROOT */
		if (NULL != (s = getenv("SYSTEMROOT"))) {
			cgi_env_add(&env, CONST_STR_LEN("SYSTEMROOT"), s, strlen(s));
		}
#endif

		for (n = 0; n < con->request.headers->used; n++) {
			data_string *ds;

			ds = (data_string *)con->request.headers->data[n];

			if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key)) {
				buffer_copy_string_encoded_cgi_varnames(p->tmp_buf, CONST_BUF_LEN(ds->key), 1);

				cgi_env_add(&env, CONST_BUF_LEN(p->tmp_buf), CONST_BUF_LEN(ds->value));
			}
		}

		for (n = 0; n < con->environment->used; n++) {
			data_string *ds;

			ds = (data_string *)con->environment->data[n];

			if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key)) {
				buffer_copy_string_encoded_cgi_varnames(p->tmp_buf, CONST_BUF_LEN(ds->key), 0);

				cgi_env_add(&env, CONST_BUF_LEN(p->tmp_buf), CONST_BUF_LEN(ds->value));
			}
		}

		if (env.size == env.used) {
			env.size += 16;
			env.ptr = realloc(env.ptr, env.size * sizeof(*env.ptr));
		}

		env.ptr[env.used] = NULL;

		/* set up args */
		argc = 3;
		args = malloc(sizeof(*args) * argc);
		force_assert(args);
		i = 0;

		if (!buffer_string_is_empty(cgi_handler)) {
			args[i++] = cgi_handler->ptr;
		}
		args[i++] = con->physical.path->ptr;
		args[i  ] = NULL;

		/* search for the last / */
		if (NULL != (c = strrchr(con->physical.path->ptr, '/'))) {
			*c = '\0';

			/* change to the physical directory */
			if (-1 == chdir(con->physical.path->ptr)) {
				log_error_write(srv, __FILE__, __LINE__, "ssb", "chdir failed:", strerror(errno), con->physical.path);
			}
			*c = '/';
		}

		/* we don't need the client socket */
		for (i = 3; i < 256; i++) {
			if (i != srv->errorlog_fd) close(i);
		}

		/* exec the cgi */
		execve(args[0], args, env.ptr);

		/* log_error_write(srv, __FILE__, __LINE__, "sss", "CGI failed:", strerror(errno), args[0]); */

		/* */
		SEGFAULT();
		break;
	}
	case -1:
		/* error */
		log_error_write(srv, __FILE__, __LINE__, "ss", "fork failed:", strerror(errno));
		close(from_cgi_fds[0]);
		close(from_cgi_fds[1]);
		close(to_cgi_fds[0]);
		close(to_cgi_fds[1]);
		return -1;
	default: {
		handler_ctx *hctx;
		/* parent proces */

		close(from_cgi_fds[1]);
		close(to_cgi_fds[0]);

		if (con->request.content_length) {
			chunkqueue *cq = con->request_content_queue;
			chunk *c;

			assert(chunkqueue_length(cq) == (off_t)con->request.content_length);

			/* NOTE: yes, this is synchronous sending of CGI post data;
			 * if you need something asynchronous (recommended with large
			 * request bodies), use mod_fastcgi + fcgi-cgi.
			 *
			 * Also: windows doesn't support select() on pipes - wouldn't be
			 * easy to fix for all platforms.
			 */

			/* there is content to send */
			for (c = cq->first; c; c = cq->first) {
				int r = -1;

				switch(c->type) {
				case FILE_CHUNK:
					r = cgi_write_file_chunk_mmap(srv, con, to_cgi_fds[1], cq);
					break;

				case MEM_CHUNK:
					if ((r = write(to_cgi_fds[1], c->mem->ptr + c->offset, buffer_string_length(c->mem) - c->offset)) < 0) {
						switch(errno) {
						case EAGAIN:
						case EINTR:
							/* ignore and try again */
							r = 0;
							break;
						case EPIPE:
						case ECONNRESET:
							/* connection closed */
							r = -2;
							break;
						default:
							/* fatal error */
							log_error_write(srv, __FILE__, __LINE__, "ss", "write failed due to: ", strerror(errno)); 
							r = -1;
							break;
						}
					} else if (r > 0) {
						chunkqueue_mark_written(cq, r);
					}
					break;
				}

				switch (r) {
				case -1:
					/* fatal error */
					close(from_cgi_fds[0]);
					close(to_cgi_fds[1]);
					return -1;
				case -2:
					/* connection reset */
					log_error_write(srv, __FILE__, __LINE__, "s", "failed to send post data to cgi, connection closed by CGI");
					/* skip all remaining data */
					chunkqueue_mark_written(cq, chunkqueue_length(cq));
					break;
				default:
					break;
				}
			}
		}

		close(to_cgi_fds[1]);

		/* register PID and wait for them asyncronously */
		con->mode = p->id;
		buffer_reset(con->physical.path);

		hctx = cgi_handler_ctx_init();

		hctx->remote_conn = con;
		hctx->plugin_data = p;
		hctx->pid = pid;
		hctx->fd = from_cgi_fds[0];
		hctx->fde_ndx = -1;

		con->plugin_ctx[p->id] = hctx;

		fdevent_register(srv->ev, hctx->fd, cgi_handle_fdevent, hctx);
		fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_IN);

		if (-1 == fdevent_fcntl_set(srv->ev, hctx->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed: ", strerror(errno));

			fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
			fdevent_unregister(srv->ev, hctx->fd);

			log_error_write(srv, __FILE__, __LINE__, "sd", "cgi close:", hctx->fd);

			close(hctx->fd);

			cgi_handler_ctx_free(hctx);

			con->plugin_ctx[p->id] = NULL;

			return -1;
		}

		break;
	}
	}

	return 0;
#else
	return -1;
#endif
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_cgi_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(cgi);
	PATCH(execute_x_only);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("cgi.assign"))) {
				PATCH(cgi);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("cgi.execute-x-only"))) {
				PATCH(execute_x_only);
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(cgi_is_handled) {
	size_t k, s_len;
	plugin_data *p = p_d;
	buffer *fn = con->physical.path;
	stat_cache_entry *sce = NULL;

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	if (buffer_is_empty(fn)) return HANDLER_GO_ON;

	mod_cgi_patch_connection(srv, con, p);

	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, con->physical.path, &sce)) return HANDLER_GO_ON;
	if (!S_ISREG(sce->st.st_mode)) return HANDLER_GO_ON;
	if (p->conf.execute_x_only == 1 && (sce->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) return HANDLER_GO_ON;

	s_len = buffer_string_length(fn);

	for (k = 0; k < p->conf.cgi->used; k++) {
		data_string *ds = (data_string *)p->conf.cgi->data[k];
		size_t ct_len = buffer_string_length(ds->key);

		if (buffer_is_empty(ds->key)) continue;
		if (s_len < ct_len) continue;

		if (0 == strncmp(fn->ptr + s_len - ct_len, ds->key->ptr, ct_len)) {
			if (cgi_create_env(srv, con, p, ds->value)) {
				con->mode = DIRECT;
				con->http_status = 500;

				buffer_reset(con->physical.path);
				return HANDLER_FINISHED;
			}
			/* one handler is enough for the request */
			break;
		}
	}

	return HANDLER_GO_ON;
}

TRIGGER_FUNC(cgi_trigger) {
	plugin_data *p = p_d;
	size_t ndx;
	/* the trigger handle only cares about lonely PID which we have to wait for */
#ifndef __WIN32

	for (ndx = 0; ndx < p->cgi_pid.used; ndx++) {
		int status;

		switch(waitpid(p->cgi_pid.ptr[ndx], &status, WNOHANG)) {
		case 0:
			/* not finished yet */
#if 0
			log_error_write(srv, __FILE__, __LINE__, "sd", "(debug) child isn't done yet, pid:", p->cgi_pid.ptr[ndx]);
#endif
			break;
		case -1:
			if (errno == ECHILD) {
				/* someone else called waitpid... remove the pid to stop looping the error each time */
				log_error_write(srv, __FILE__, __LINE__, "s", "cgi child vanished, probably someone else called waitpid");

				cgi_pid_del(srv, p, p->cgi_pid.ptr[ndx]);
				ndx--;
				continue;
			}

			log_error_write(srv, __FILE__, __LINE__, "ss", "waitpid failed: ", strerror(errno));

			return HANDLER_ERROR;
		default:

			if (WIFEXITED(status)) {
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sd", "(debug) cgi exited fine, pid:", p->cgi_pid.ptr[ndx]);
#endif
			} else if (WIFSIGNALED(status)) {
				/* FIXME: what if we killed the CGI script with a kill(..., SIGTERM) ?
				 */
				if (WTERMSIG(status) != SIGTERM) {
					log_error_write(srv, __FILE__, __LINE__, "sd", "cleaning up CGI: process died with signal", WTERMSIG(status));
				}
			} else {
				log_error_write(srv, __FILE__, __LINE__, "s", "cleaning up CGI: ended unexpectedly");
			}

			cgi_pid_del(srv, p, p->cgi_pid.ptr[ndx]);
			/* del modified the buffer structure
			 * and copies the last entry to the current one
			 * -> recheck the current index
			 */
			ndx--;
		}
	}
#endif
	return HANDLER_GO_ON;
}

/*
 * - HANDLER_GO_ON : not our job
 * - HANDLER_FINISHED: got response header
 * - HANDLER_WAIT_FOR_EVENT: waiting for response header
 */
SUBREQUEST_FUNC(mod_cgi_handle_subrequest) {
	int status;
	plugin_data *p = p_d;
	handler_ctx *hctx = con->plugin_ctx[p->id];

	if (con->mode != p->id) return HANDLER_GO_ON;
	if (NULL == hctx) return HANDLER_GO_ON;

#if 0
	log_error_write(srv, __FILE__, __LINE__, "sdd", "subrequest, pid =", hctx, hctx->pid);
#endif

	if (hctx->pid == 0) {
		/* cgi already dead */
		if (!con->file_started) return HANDLER_WAIT_FOR_EVENT;
		return HANDLER_FINISHED;
	}

#ifndef __WIN32
	switch(waitpid(hctx->pid, &status, WNOHANG)) {
	case 0:
		/* we only have for events here if we don't have the header yet,
		 * otherwise the event-handler will send us the incoming data */
		if (con->file_started) return HANDLER_FINISHED;

		return HANDLER_WAIT_FOR_EVENT;
	case -1:
		if (errno == EINTR) return HANDLER_WAIT_FOR_EVENT;

		if (errno == ECHILD && con->file_started == 0) {
			/*
			 * second round but still not response
			 */
			return HANDLER_WAIT_FOR_EVENT;
		}

		log_error_write(srv, __FILE__, __LINE__, "ss", "waitpid failed: ", strerror(errno));
		con->mode = DIRECT;
		con->http_status = 500;

		hctx->pid = 0;

		fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
		fdevent_unregister(srv->ev, hctx->fd);

		if (close(hctx->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "sds", "cgi close failed ", hctx->fd, strerror(errno));
		}

		cgi_handler_ctx_free(hctx);

		con->plugin_ctx[p->id] = NULL;

		return HANDLER_FINISHED;
	default:
		/* cgi process exited
		 */

		hctx->pid = 0;

		/* we already have response headers? just continue */
		if (con->file_started) return HANDLER_FINISHED;

		if (WIFEXITED(status)) {
			/* clean exit - just continue */
			return HANDLER_WAIT_FOR_EVENT;
		}

		/* cgi proc died, and we didn't get any data yet - send error message and close cgi con */
		log_error_write(srv, __FILE__, __LINE__, "s", "cgi died ?");

		con->http_status = 500;
		con->mode = DIRECT;

		fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
		fdevent_unregister(srv->ev, hctx->fd);

		if (close(hctx->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "sds", "cgi close failed ", hctx->fd, strerror(errno));
		}

		cgi_handler_ctx_free(hctx);

		con->plugin_ctx[p->id] = NULL;
		return HANDLER_FINISHED;
	}
#else
	return HANDLER_ERROR;
#endif
}


int mod_cgi_plugin_init(plugin *p);
int mod_cgi_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("cgi");

	p->connection_reset = cgi_connection_close_callback;
	p->handle_subrequest_start = cgi_is_handled;
	p->handle_subrequest = mod_cgi_handle_subrequest;
#if 0
	p->handle_fdevent = cgi_handle_fdevent;
#endif
	p->handle_trigger = cgi_trigger;
	p->init           = mod_cgi_init;
	p->cleanup        = mod_cgi_free;
	p->set_defaults   = mod_fastcgi_set_defaults;

	p->data        = NULL;

	return 0;
}
