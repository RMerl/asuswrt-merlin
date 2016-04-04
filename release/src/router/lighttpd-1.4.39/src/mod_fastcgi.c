#include "buffer.h"
#include "server.h"
#include "keyvalue.h"
#include "log.h"

#include "http_chunk.h"
#include "fdevent.h"
#include "connections.h"
#include "response.h"
#include "joblist.h"

#include "plugin.h"

#include "inet_ntop_cache.h"
#include "stat_cache.h"
#include "status_counter.h"

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>

#ifdef HAVE_FASTCGI_FASTCGI_H
# include <fastcgi/fastcgi.h>
#else
# ifdef HAVE_FASTCGI_H
#  include <fastcgi.h>
# else
#  include "fastcgi.h"
# endif
#endif /* HAVE_FASTCGI_FASTCGI_H */

#include <stdio.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "sys-socket.h"

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "version.h"

/*
 *
 * TODO:
 *
 * - add timeout for a connect to a non-fastcgi process
 *   (use state_timestamp + state)
 *
 */

typedef struct fcgi_proc {
	size_t id; /* id will be between 1 and max_procs */
	buffer *unixsocket; /* config.socket + "-" + id */
	unsigned port;  /* config.port + pno */

	buffer *connection_name; /* either tcp:<host>:<port> or unix:<socket> for debugging purposes */

	pid_t pid;   /* PID of the spawned process (0 if not spawned locally) */


	size_t load; /* number of requests waiting on this process */

	size_t requests;  /* see max_requests */
	struct fcgi_proc *prev, *next; /* see first */

	time_t disabled_until; /* this proc is disabled until, use something else until then */

	int is_local;

	enum {
		PROC_STATE_UNSET,    /* init-phase */
		PROC_STATE_RUNNING,  /* alive */
		PROC_STATE_OVERLOADED, /* listen-queue is full,
					  don't send anything to this proc for the next 2 seconds */
		PROC_STATE_DIED_WAIT_FOR_PID, /* */
		PROC_STATE_DIED,     /* marked as dead, should be restarted */
		PROC_STATE_KILLED    /* was killed as we don't have the load anymore */
	} state;
} fcgi_proc;

typedef struct {
	/* the key that is used to reference this value */
	buffer *id;

	/* list of processes handling this extension
	 * sorted by lowest load
	 *
	 * whenever a job is done move it up in the list
	 * until it is sorted, move it down as soon as the
	 * job is started
	 */
	fcgi_proc *first;
	fcgi_proc *unused_procs;

	/*
	 * spawn at least min_procs, at max_procs.
	 *
	 * as soon as the load of the first entry
	 * is max_load_per_proc we spawn a new one
	 * and add it to the first entry and give it
	 * the load
	 *
	 */

	unsigned short max_procs;
	size_t num_procs;    /* how many procs are started */
	size_t active_procs; /* how many of them are really running, i.e. state = PROC_STATE_RUNNING */

	/*
	 * time after a disabled remote connection is tried to be re-enabled
	 *
	 *
	 */

	unsigned short disable_time;

	/*
	 * some fastcgi processes get a little bit larger
	 * than wanted. max_requests_per_proc kills a
	 * process after a number of handled requests.
	 *
	 */
	size_t max_requests_per_proc;


	/* config */

	/*
	 * host:port
	 *
	 * if host is one of the local IP adresses the
	 * whole connection is local
	 *
	 * if port is not 0, and host is not specified,
	 * "localhost" (INADDR_LOOPBACK) is assumed.
	 *
	 */
	buffer *host;
	unsigned short port;

	/*
	 * Unix Domain Socket
	 *
	 * instead of TCP/IP we can use Unix Domain Sockets
	 * - more secure (you have fileperms to play with)
	 * - more control (on locally)
	 * - more speed (no extra overhead)
	 */
	buffer *unixsocket;

	/* if socket is local we can start the fastcgi
	 * process ourself
	 *
	 * bin-path is the path to the binary
	 *
	 * check min_procs and max_procs for the number
	 * of process to start up
	 */
	buffer *bin_path;

	/* bin-path is set bin-environment is taken to
	 * create the environement before starting the
	 * FastCGI process
	 *
	 */
	array *bin_env;

	array *bin_env_copy;

	/*
	 * docroot-translation between URL->phys and the
	 * remote host
	 *
	 * reasons:
	 * - different dir-layout if remote
	 * - chroot if local
	 *
	 */
	buffer *docroot;

	/*
	 * fastcgi-mode:
	 * - responser
	 * - authorizer
	 *
	 */
	unsigned short mode;

	/*
	 * check_local tells you if the phys file is stat()ed
	 * or not. FastCGI doesn't care if the service is
	 * remote. If the web-server side doesn't contain
	 * the fastcgi-files we should not stat() for them
	 * and say '404 not found'.
	 */
	unsigned short check_local;

	/*
	 * append PATH_INFO to SCRIPT_FILENAME
	 *
	 * php needs this if cgi.fix_pathinfo is provided
	 *
	 */

	unsigned short break_scriptfilename_for_php;

	/*
	 * workaround for program when prefix="/"
	 *
	 * rule to build PATH_INFO is hardcoded for when check_local is disabled
	 * enable this option to use the workaround
	 *
	 */

	unsigned short fix_root_path_name;

	/*
	 * If the backend includes X-LIGHTTPD-send-file in the response
	 * we use the value as filename and ignore the content.
	 *
	 */
	unsigned short allow_xsendfile;

	ssize_t load; /* replace by host->load */

	size_t max_id; /* corresponds most of the time to
	num_procs.

	only if a process is killed max_id waits for the process itself
	to die and decrements it afterwards */

	buffer *strip_request_uri;

	unsigned short kill_signal; /* we need a setting for this as libfcgi
				       applications prefer SIGUSR1 while the
				       rest of the world would use SIGTERM
				       *sigh* */
} fcgi_extension_host;

/*
 * one extension can have multiple hosts assigned
 * one host can spawn additional processes on the same
 *   socket (if we control it)
 *
 * ext -> host -> procs
 *    1:n     1:n
 *
 * if the fastcgi process is remote that whole goes down
 * to
 *
 * ext -> host -> procs
 *    1:n     1:1
 *
 * in case of PHP and FCGI_CHILDREN we have again a procs
 * but we don't control it directly.
 *
 */

typedef struct {
	buffer *key; /* like .php */

	int note_is_sent;
	int last_used_ndx;

	fcgi_extension_host **hosts;

	size_t used;
	size_t size;
} fcgi_extension;

typedef struct {
	fcgi_extension **exts;

	size_t used;
	size_t size;
} fcgi_exts;


typedef struct {
	fcgi_exts *exts;

	array *ext_mapping;

	unsigned int debug;
} plugin_config;

typedef struct {
	char **ptr;

	size_t size;
	size_t used;
} char_array;

/* generic plugin data, shared between all connections */
typedef struct {
	PLUGIN_DATA;

	buffer *fcgi_env;

	buffer *path;

	buffer *statuskey;

	plugin_config **config_storage;

	plugin_config conf; /* this is only used as long as no handler_ctx is setup */
} plugin_data;

/* connection specific data */
typedef enum {
	FCGI_STATE_UNSET,
	FCGI_STATE_INIT,
	FCGI_STATE_CONNECT_DELAYED,
	FCGI_STATE_PREPARE_WRITE,
	FCGI_STATE_WRITE,
	FCGI_STATE_READ
} fcgi_connection_state_t;

typedef struct {
	fcgi_proc *proc;
	fcgi_extension_host *host;
	fcgi_extension *ext;

	fcgi_connection_state_t state;
	time_t   state_timestamp;

	int      reconnects; /* number of reconnect attempts */

	chunkqueue *rb; /* read queue */
	chunkqueue *wb; /* write queue */

	buffer   *response_header;

	size_t    request_id;
	int       fd;        /* fd to the fastcgi process */
	int       fde_ndx;   /* index into the fd-event buffer */

	pid_t     pid;
	int       got_proc;

	int       send_content_body;

	plugin_config conf;

	connection *remote_conn;  /* dumb pointer */
	plugin_data *plugin_data; /* dumb pointer */
} handler_ctx;


/* ok, we need a prototype */
static handler_t fcgi_handle_fdevent(server *srv, void *ctx, int revents);

static void reset_signals(void) {
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_DFL);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_DFL);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif
	signal(SIGHUP, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
}

static void fastcgi_status_copy_procname(buffer *b, fcgi_extension_host *host, fcgi_proc *proc) {
	buffer_copy_string_len(b, CONST_STR_LEN("fastcgi.backend."));
	buffer_append_string_buffer(b, host->id);
	if (proc) {
		buffer_append_string_len(b, CONST_STR_LEN("."));
		buffer_append_int(b, proc->id);
	}
}

static void fcgi_proc_load_inc(server *srv, handler_ctx *hctx) {
	plugin_data *p = hctx->plugin_data;
	hctx->proc->load++;

	status_counter_inc(srv, CONST_STR_LEN("fastcgi.active-requests"));

	fastcgi_status_copy_procname(p->statuskey, hctx->host, hctx->proc);
	buffer_append_string_len(p->statuskey, CONST_STR_LEN(".load"));

	status_counter_set(srv, CONST_BUF_LEN(p->statuskey), hctx->proc->load);
}

static void fcgi_proc_load_dec(server *srv, handler_ctx *hctx) {
	plugin_data *p = hctx->plugin_data;
	hctx->proc->load--;

	status_counter_dec(srv, CONST_STR_LEN("fastcgi.active-requests"));

	fastcgi_status_copy_procname(p->statuskey, hctx->host, hctx->proc);
	buffer_append_string_len(p->statuskey, CONST_STR_LEN(".load"));

	status_counter_set(srv, CONST_BUF_LEN(p->statuskey), hctx->proc->load);
}

static void fcgi_host_assign(server *srv, handler_ctx *hctx, fcgi_extension_host *host) {
	plugin_data *p = hctx->plugin_data;
	hctx->host = host;
	hctx->host->load++;

	fastcgi_status_copy_procname(p->statuskey, hctx->host, NULL);
	buffer_append_string_len(p->statuskey, CONST_STR_LEN(".load"));

	status_counter_set(srv, CONST_BUF_LEN(p->statuskey), hctx->host->load);
}

static void fcgi_host_reset(server *srv, handler_ctx *hctx) {
	plugin_data *p = hctx->plugin_data;
	hctx->host->load--;

	fastcgi_status_copy_procname(p->statuskey, hctx->host, NULL);
	buffer_append_string_len(p->statuskey, CONST_STR_LEN(".load"));

	status_counter_set(srv, CONST_BUF_LEN(p->statuskey), hctx->host->load);

	hctx->host = NULL;
}

static void fcgi_host_disable(server *srv, handler_ctx *hctx) {
	plugin_data *p    = hctx->plugin_data;

	if (hctx->host->disable_time || hctx->proc->is_local) {
		if (hctx->proc->state == PROC_STATE_RUNNING) hctx->host->active_procs--;
		hctx->proc->disabled_until = srv->cur_ts + hctx->host->disable_time;
		hctx->proc->state = hctx->proc->is_local ? PROC_STATE_DIED_WAIT_FOR_PID : PROC_STATE_DIED;

		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "sds",
				"backend disabled for", hctx->host->disable_time, "seconds");
		}
	}
}

static int fastcgi_status_init(server *srv, buffer *b, fcgi_extension_host *host, fcgi_proc *proc) {
#define CLEAN(x) \
	fastcgi_status_copy_procname(b, host, proc); \
	buffer_append_string_len(b, CONST_STR_LEN(x)); \
	status_counter_set(srv, CONST_BUF_LEN(b), 0);

	CLEAN(".disabled");
	CLEAN(".died");
	CLEAN(".overloaded");
	CLEAN(".connected");
	CLEAN(".load");

#undef CLEAN

#define CLEAN(x) \
	fastcgi_status_copy_procname(b, host, NULL); \
	buffer_append_string_len(b, CONST_STR_LEN(x)); \
	status_counter_set(srv, CONST_BUF_LEN(b), 0);

	CLEAN(".load");

#undef CLEAN

	return 0;
}

static handler_ctx * handler_ctx_init(void) {
	handler_ctx * hctx;

	hctx = calloc(1, sizeof(*hctx));
	force_assert(hctx);

	hctx->fde_ndx = -1;

	hctx->response_header = buffer_init();

	hctx->request_id = 0;
	hctx->state = FCGI_STATE_INIT;
	hctx->proc = NULL;

	hctx->fd = -1;

	hctx->reconnects = 0;
	hctx->send_content_body = 1;

	hctx->rb = chunkqueue_init();
	hctx->wb = chunkqueue_init();

	return hctx;
}

static void handler_ctx_free(server *srv, handler_ctx *hctx) {
	if (hctx->host) {
		fcgi_host_reset(srv, hctx);
	}

	buffer_free(hctx->response_header);

	chunkqueue_free(hctx->rb);
	chunkqueue_free(hctx->wb);

	free(hctx);
}

static fcgi_proc *fastcgi_process_init(void) {
	fcgi_proc *f;

	f = calloc(1, sizeof(*f));
	f->unixsocket = buffer_init();
	f->connection_name = buffer_init();

	f->prev = NULL;
	f->next = NULL;

	return f;
}

static void fastcgi_process_free(fcgi_proc *f) {
	if (!f) return;

	fastcgi_process_free(f->next);

	buffer_free(f->unixsocket);
	buffer_free(f->connection_name);

	free(f);
}

static fcgi_extension_host *fastcgi_host_init(void) {
	fcgi_extension_host *f;

	f = calloc(1, sizeof(*f));

	f->id = buffer_init();
	f->host = buffer_init();
	f->unixsocket = buffer_init();
	f->docroot = buffer_init();
	f->bin_path = buffer_init();
	f->bin_env = array_init();
	f->bin_env_copy = array_init();
	f->strip_request_uri = buffer_init();

	return f;
}

static void fastcgi_host_free(fcgi_extension_host *h) {
	if (!h) return;

	buffer_free(h->id);
	buffer_free(h->host);
	buffer_free(h->unixsocket);
	buffer_free(h->docroot);
	buffer_free(h->bin_path);
	buffer_free(h->strip_request_uri);
	array_free(h->bin_env);
	array_free(h->bin_env_copy);

	fastcgi_process_free(h->first);
	fastcgi_process_free(h->unused_procs);

	free(h);

}

static fcgi_exts *fastcgi_extensions_init(void) {
	fcgi_exts *f;

	f = calloc(1, sizeof(*f));

	return f;
}

static void fastcgi_extensions_free(fcgi_exts *f) {
	size_t i;

	if (!f) return;

	for (i = 0; i < f->used; i++) {
		fcgi_extension *fe;
		size_t j;

		fe = f->exts[i];

		for (j = 0; j < fe->used; j++) {
			fcgi_extension_host *h;

			h = fe->hosts[j];

			fastcgi_host_free(h);
		}

		buffer_free(fe->key);
		free(fe->hosts);

		free(fe);
	}

	free(f->exts);

	free(f);
}

static int fastcgi_extension_insert(fcgi_exts *ext, buffer *key, fcgi_extension_host *fh) {
	fcgi_extension *fe;
	size_t i;

	/* there is something */

	for (i = 0; i < ext->used; i++) {
		if (buffer_is_equal(key, ext->exts[i]->key)) {
			break;
		}
	}

	if (i == ext->used) {
		/* filextension is new */
		fe = calloc(1, sizeof(*fe));
		force_assert(fe);
		fe->key = buffer_init();
		fe->last_used_ndx = -1;
		buffer_copy_buffer(fe->key, key);

		/* */

		if (ext->size == 0) {
			ext->size = 8;
			ext->exts = malloc(ext->size * sizeof(*(ext->exts)));
			force_assert(ext->exts);
		} else if (ext->used == ext->size) {
			ext->size += 8;
			ext->exts = realloc(ext->exts, ext->size * sizeof(*(ext->exts)));
			force_assert(ext->exts);
		}
		ext->exts[ext->used++] = fe;
	} else {
		fe = ext->exts[i];
	}

	if (fe->size == 0) {
		fe->size = 4;
		fe->hosts = malloc(fe->size * sizeof(*(fe->hosts)));
		force_assert(fe->hosts);
	} else if (fe->size == fe->used) {
		fe->size += 4;
		fe->hosts = realloc(fe->hosts, fe->size * sizeof(*(fe->hosts)));
		force_assert(fe->hosts);
	}

	fe->hosts[fe->used++] = fh;

	return 0;

}

INIT_FUNC(mod_fastcgi_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->fcgi_env = buffer_init();

	p->path = buffer_init();

	p->statuskey = buffer_init();

	return p;
}


FREE_FUNC(mod_fastcgi_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	buffer_free(p->fcgi_env);
	buffer_free(p->path);
	buffer_free(p->statuskey);

	if (p->config_storage) {
		size_t i, j, n;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];
			fcgi_exts *exts;

			if (NULL == s) continue;

			exts = s->exts;

			for (j = 0; j < exts->used; j++) {
				fcgi_extension *ex;

				ex = exts->exts[j];

				for (n = 0; n < ex->used; n++) {
					fcgi_proc *proc;
					fcgi_extension_host *host;

					host = ex->hosts[n];

					for (proc = host->first; proc; proc = proc->next) {
						if (proc->pid != 0) {
							kill(proc->pid, host->kill_signal);
						}

						if (proc->is_local &&
						    !buffer_string_is_empty(proc->unixsocket)) {
							unlink(proc->unixsocket->ptr);
						}
					}

					for (proc = host->unused_procs; proc; proc = proc->next) {
						if (proc->pid != 0) {
							kill(proc->pid, host->kill_signal);
						}
						if (proc->is_local &&
						    !buffer_string_is_empty(proc->unixsocket)) {
							unlink(proc->unixsocket->ptr);
						}
					}
				}
			}

			fastcgi_extensions_free(s->exts);
			array_free(s->ext_mapping);

			free(s);
		}
		free(p->config_storage);
	}

	free(p);

	return HANDLER_GO_ON;
}

static int env_add(char_array *env, const char *key, size_t key_len, const char *val, size_t val_len) {
	char *dst;
	size_t i;

	if (!key || !val) return -1;

	dst = malloc(key_len + val_len + 3);
	memcpy(dst, key, key_len);
	dst[key_len] = '=';
	memcpy(dst + key_len + 1, val, val_len);
	dst[key_len + 1 + val_len] = '\0';

	for (i = 0; i < env->used; i++) {
		if (0 == strncmp(dst, env->ptr[i], key_len + 1)) {
			/* don't care about free as we are in a forked child which is going to exec(...) */
			/* free(env->ptr[i]); */
			env->ptr[i] = dst;
			return 0;
		}
	}

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
	} else if (env->size == env->used + 1) {
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	env->ptr[env->used++] = dst;

	return 0;
}

static int parse_binpath(char_array *env, buffer *b) {
	char *start;
	size_t i;
	/* search for spaces */

	start = b->ptr;
	for (i = 0; i < buffer_string_length(b); i++) {
		switch(b->ptr[i]) {
		case ' ':
		case '\t':
			/* a WS, stop here and copy the argument */

			if (env->size == 0) {
				env->size = 16;
				env->ptr = malloc(env->size * sizeof(*env->ptr));
			} else if (env->size == env->used) {
				env->size += 16;
				env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
			}

			b->ptr[i] = '\0';

			env->ptr[env->used++] = start;

			start = b->ptr + i + 1;
			break;
		default:
			break;
		}
	}

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
	} else if (env->size == env->used) { /* we need one extra for the terminating NULL */
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	/* the rest */
	env->ptr[env->used++] = start;

	if (env->size == 0) {
		env->size = 16;
		env->ptr = malloc(env->size * sizeof(*env->ptr));
	} else if (env->size == env->used) { /* we need one extra for the terminating NULL */
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	/* terminate */
	env->ptr[env->used++] = NULL;

	return 0;
}

static int fcgi_spawn_connection(server *srv,
				 plugin_data *p,
				 fcgi_extension_host *host,
				 fcgi_proc *proc) {
	int fcgi_fd;
	int socket_type, status;
	struct timeval tv = { 0, 100 * 1000 };
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un fcgi_addr_un;
#endif
	struct sockaddr_in fcgi_addr_in;
	struct sockaddr *fcgi_addr;

	socklen_t servlen;

#ifndef HAVE_FORK
	return -1;
#endif

	if (p->conf.debug) {
		log_error_write(srv, __FILE__, __LINE__, "sdb",
				"new proc, socket:", proc->port, proc->unixsocket);
	}

	if (!buffer_string_is_empty(proc->unixsocket)) {
#ifdef HAVE_SYS_UN_H
		memset(&fcgi_addr_un, 0, sizeof(fcgi_addr_un));
		fcgi_addr_un.sun_family = AF_UNIX;
		if (buffer_string_length(proc->unixsocket) + 1 > sizeof(fcgi_addr_un.sun_path)) {
			log_error_write(srv, __FILE__, __LINE__, "sB",
					"ERROR: Unix Domain socket filename too long:",
					proc->unixsocket);
			return -1;
		}
		memcpy(fcgi_addr_un.sun_path, proc->unixsocket->ptr, buffer_string_length(proc->unixsocket) + 1);

#ifdef SUN_LEN
		servlen = SUN_LEN(&fcgi_addr_un);
#else
		/* stevens says: */
		servlen = buffer_string_length(proc->unixsocket) + 1 + sizeof(fcgi_addr_un.sun_family);
#endif
		socket_type = AF_UNIX;
		fcgi_addr = (struct sockaddr *) &fcgi_addr_un;

		buffer_copy_string_len(proc->connection_name, CONST_STR_LEN("unix:"));
		buffer_append_string_buffer(proc->connection_name, proc->unixsocket);

#else
		log_error_write(srv, __FILE__, __LINE__, "s",
				"ERROR: Unix Domain sockets are not supported.");
		return -1;
#endif
	} else {
		memset(&fcgi_addr_in, 0, sizeof(fcgi_addr_in));
		fcgi_addr_in.sin_family = AF_INET;

		if (buffer_string_is_empty(host->host)) {
			fcgi_addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		} else {
			struct hostent *he;

			/* set a useful default */
			fcgi_addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);


			if (NULL == (he = gethostbyname(host->host->ptr))) {
				log_error_write(srv, __FILE__, __LINE__,
						"sdb", "gethostbyname failed: ",
						h_errno, host->host);
				return -1;
			}

			if (he->h_addrtype != AF_INET) {
				log_error_write(srv, __FILE__, __LINE__, "sd", "addr-type != AF_INET: ", he->h_addrtype);
				return -1;
			}

			if (he->h_length != sizeof(struct in_addr)) {
				log_error_write(srv, __FILE__, __LINE__, "sd", "addr-length != sizeof(in_addr): ", he->h_length);
				return -1;
			}

			memcpy(&(fcgi_addr_in.sin_addr.s_addr), he->h_addr_list[0], he->h_length);

		}
		fcgi_addr_in.sin_port = htons(proc->port);
		servlen = sizeof(fcgi_addr_in);

		socket_type = AF_INET;
		fcgi_addr = (struct sockaddr *) &fcgi_addr_in;

		buffer_copy_string_len(proc->connection_name, CONST_STR_LEN("tcp:"));
		if (!buffer_string_is_empty(host->host)) {
			buffer_append_string_buffer(proc->connection_name, host->host);
		} else {
			buffer_append_string_len(proc->connection_name, CONST_STR_LEN("localhost"));
		}
		buffer_append_string_len(proc->connection_name, CONST_STR_LEN(":"));
		buffer_append_int(proc->connection_name, proc->port);
	}

	if (-1 == (fcgi_fd = socket(socket_type, SOCK_STREAM, 0))) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"failed:", strerror(errno));
		return -1;
	}

	if (-1 == connect(fcgi_fd, fcgi_addr, servlen)) {
		/* server is not up, spawn it  */
		pid_t child;
		int val;

		if (errno != ENOENT &&
		    !buffer_string_is_empty(proc->unixsocket)) {
			unlink(proc->unixsocket->ptr);
		}

		close(fcgi_fd);

		/* reopen socket */
		if (-1 == (fcgi_fd = socket(socket_type, SOCK_STREAM, 0))) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"socket failed:", strerror(errno));
			return -1;
		}

		val = 1;
		if (setsockopt(fcgi_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"socketsockopt failed:", strerror(errno));
			close(fcgi_fd);
			return -1;
		}

		/* create socket */
		if (-1 == bind(fcgi_fd, fcgi_addr, servlen)) {
			log_error_write(srv, __FILE__, __LINE__, "sbs",
				"bind failed for:",
				proc->connection_name,
				strerror(errno));
			close(fcgi_fd);
			return -1;
		}

		if (-1 == listen(fcgi_fd, 1024)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"listen failed:", strerror(errno));
			close(fcgi_fd);
			return -1;
		}

#ifdef HAVE_FORK
		switch ((child = fork())) {
		case 0: {
			size_t i = 0;
			char *c;
			char_array env;
			char_array arg;

			/* create environment */
			env.ptr = NULL;
			env.size = 0;
			env.used = 0;

			arg.ptr = NULL;
			arg.size = 0;
			arg.used = 0;

			if(fcgi_fd != FCGI_LISTENSOCK_FILENO) {
				close(FCGI_LISTENSOCK_FILENO);
				dup2(fcgi_fd, FCGI_LISTENSOCK_FILENO);
				close(fcgi_fd);
			}

			/* we don't need the client socket */
			for (i = 3; i < 256; i++) {
				close(i);
			}

			/* build clean environment */
			if (host->bin_env_copy->used) {
				for (i = 0; i < host->bin_env_copy->used; i++) {
					data_string *ds = (data_string *)host->bin_env_copy->data[i];
					char *ge;

					if (NULL != (ge = getenv(ds->value->ptr))) {
						env_add(&env, CONST_BUF_LEN(ds->value), ge, strlen(ge));
					}
				}
			} else {
				for (i = 0; environ[i]; i++) {
					char *eq;

					if (NULL != (eq = strchr(environ[i], '='))) {
						env_add(&env, environ[i], eq - environ[i], eq+1, strlen(eq+1));
					}
				}
			}

			/* create environment */
			for (i = 0; i < host->bin_env->used; i++) {
				data_string *ds = (data_string *)host->bin_env->data[i];

				env_add(&env, CONST_BUF_LEN(ds->key), CONST_BUF_LEN(ds->value));
			}

			for (i = 0; i < env.used; i++) {
				/* search for PHP_FCGI_CHILDREN */
				if (0 == strncmp(env.ptr[i], "PHP_FCGI_CHILDREN=", sizeof("PHP_FCGI_CHILDREN=") - 1)) break;
			}

			/* not found, add a default */
			if (i == env.used) {
				env_add(&env, CONST_STR_LEN("PHP_FCGI_CHILDREN"), CONST_STR_LEN("1"));
			}

			env.ptr[env.used] = NULL;

			parse_binpath(&arg, host->bin_path);

			/* chdir into the base of the bin-path,
			 * search for the last / */
			if (NULL != (c = strrchr(arg.ptr[0], '/'))) {
				*c = '\0';

				/* change to the physical directory */
				if (-1 == chdir(arg.ptr[0])) {
					*c = '/';
					log_error_write(srv, __FILE__, __LINE__, "sss", "chdir failed:", strerror(errno), arg.ptr[0]);
				}
				*c = '/';
			}

			reset_signals();

			/* exec the cgi */
			execve(arg.ptr[0], arg.ptr, env.ptr);

			/* log_error_write(srv, __FILE__, __LINE__, "sbs",
					"execve failed for:", host->bin_path, strerror(errno)); */

			exit(errno);

			break;
		}
		case -1:
			/* error */
			break;
		default:
			/* father */

			/* wait */
			select(0, NULL, NULL, NULL, &tv);

			switch (waitpid(child, &status, WNOHANG)) {
			case 0:
				/* child still running after timeout, good */
				break;
			case -1:
				/* no PID found ? should never happen */
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"pid not found:", strerror(errno));
				return -1;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"the fastcgi-backend", host->bin_path, "failed to start:");
				/* the child should not terminate at all */
				if (WIFEXITED(status)) {
					log_error_write(srv, __FILE__, __LINE__, "sdb",
							"child exited with status",
							WEXITSTATUS(status), host->bin_path);
					log_error_write(srv, __FILE__, __LINE__, "s",
							"If you're trying to run your app as a FastCGI backend, make sure you're using the FastCGI-enabled version.\n"
							"If this is PHP on Gentoo, add 'fastcgi' to the USE flags.");
				} else if (WIFSIGNALED(status)) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"terminated by signal:",
							WTERMSIG(status));

					if (WTERMSIG(status) == 11) {
						log_error_write(srv, __FILE__, __LINE__, "s",
								"to be exact: it segfaulted, crashed, died, ... you get the idea." );
						log_error_write(srv, __FILE__, __LINE__, "s",
								"If this is PHP, try removing the bytecode caches for now and try again.");
					}
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"child died somehow:",
							status);
				}
				return -1;
			}

			/* register process */
			proc->pid = child;
			proc->is_local = 1;

			break;
		}
#endif
	} else {
		proc->is_local = 0;
		proc->pid = 0;

		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					"(debug) socket is already used; won't spawn:",
					proc->connection_name);
		}
	}

	proc->state = PROC_STATE_RUNNING;
	host->active_procs++;

	close(fcgi_fd);

	return 0;
}


SETDEFAULTS_FUNC(mod_fastcgi_set_defaults) {
	plugin_data *p = p_d;
	data_unset *du;
	size_t i = 0;
	buffer *fcgi_mode = buffer_init();
	fcgi_extension_host *host = NULL;

	config_values_t cv[] = {
		{ "fastcgi.server",              NULL, T_CONFIG_LOCAL, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "fastcgi.debug",               NULL, T_CONFIG_INT  , T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ "fastcgi.map-extensions",      NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 2 */
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = malloc(sizeof(plugin_config));
		s->exts          = fastcgi_extensions_init();
		s->debug         = 0;
		s->ext_mapping   = array_init();

		cv[0].destination = s->exts;
		cv[1].destination = &(s->debug);
		cv[2].destination = s->ext_mapping;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			goto error;
		}

		/*
		 * <key> = ( ... )
		 */

		if (NULL != (du = array_get_element(config->value, "fastcgi.server"))) {
			size_t j;
			data_array *da = (data_array *)du;

			if (du->type != TYPE_ARRAY) {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"unexpected type for key: ", "fastcgi.server", "array of strings");

				goto error;
			}


			/*
			 * fastcgi.server = ( "<ext>" => ( ... ),
			 *                    "<ext>" => ( ... ) )
			 */

			for (j = 0; j < da->value->used; j++) {
				size_t n;
				data_array *da_ext = (data_array *)da->value->data[j];

				if (da->value->data[j]->type != TYPE_ARRAY) {
					log_error_write(srv, __FILE__, __LINE__, "sssbs",
							"unexpected type for key: ", "fastcgi.server",
							"[", da->value->data[j]->key, "](string)");

					goto error;
				}

				/*
				 * da_ext->key == name of the extension
				 */

				/*
				 * fastcgi.server = ( "<ext>" =>
				 *                     ( "<host>" => ( ... ),
				 *                       "<host>" => ( ... )
				 *                     ),
				 *                    "<ext>" => ... )
				 */

				for (n = 0; n < da_ext->value->used; n++) {
					data_array *da_host = (data_array *)da_ext->value->data[n];

					config_values_t fcv[] = {
						{ "host",              NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
						{ "docroot",           NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
						{ "mode",              NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 2 */
						{ "socket",            NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 3 */
						{ "bin-path",          NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 4 */

						{ "check-local",       NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },      /* 5 */
						{ "port",              NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 6 */
						{ "max-procs",         NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 7 */
						{ "disable-time",      NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 8 */

						{ "bin-environment",   NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },        /* 9 */
						{ "bin-copy-environment", NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },     /* 10 */

						{ "broken-scriptfilename", NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },  /* 11 */
						{ "allow-x-send-file",  NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },     /* 12 */
						{ "strip-request-uri",  NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },      /* 13 */
						{ "kill-signal",        NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },       /* 14 */
						{ "fix-root-scriptname",   NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },  /* 15 */

						{ NULL,                NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
					};

					if (da_host->type != TYPE_ARRAY) {
						log_error_write(srv, __FILE__, __LINE__, "ssSBS",
								"unexpected type for key:",
								"fastcgi.server",
								"[", da_host->key, "](string)");

						goto error;
					}

					host = fastcgi_host_init();
					buffer_reset(fcgi_mode);

					buffer_copy_buffer(host->id, da_host->key);

					host->check_local  = 1;
					host->max_procs    = 4;
					host->mode = FCGI_RESPONDER;
					host->disable_time = 1;
					host->break_scriptfilename_for_php = 0;
					host->allow_xsendfile = 0; /* handle X-LIGHTTPD-send-file */
					host->kill_signal = SIGTERM;
					host->fix_root_path_name = 0;

					fcv[0].destination = host->host;
					fcv[1].destination = host->docroot;
					fcv[2].destination = fcgi_mode;
					fcv[3].destination = host->unixsocket;
					fcv[4].destination = host->bin_path;

					fcv[5].destination = &(host->check_local);
					fcv[6].destination = &(host->port);
					fcv[7].destination = &(host->max_procs);
					fcv[8].destination = &(host->disable_time);

					fcv[9].destination = host->bin_env;
					fcv[10].destination = host->bin_env_copy;
					fcv[11].destination = &(host->break_scriptfilename_for_php);
					fcv[12].destination = &(host->allow_xsendfile);
					fcv[13].destination = host->strip_request_uri;
					fcv[14].destination = &(host->kill_signal);
					fcv[15].destination = &(host->fix_root_path_name);

					if (0 != config_insert_values_internal(srv, da_host->value, fcv, T_CONFIG_SCOPE_CONNECTION)) {
						goto error;
					}

					if ((!buffer_string_is_empty(host->host) || host->port) &&
					    !buffer_string_is_empty(host->unixsocket)) {
						log_error_write(srv, __FILE__, __LINE__, "sbsbsbs",
								"either host/port or socket have to be set in:",
								da->key, "= (",
								da_ext->key, " => (",
								da_host->key, " ( ...");

						goto error;
					}

					if (!buffer_string_is_empty(host->unixsocket)) {
						/* unix domain socket */
						struct sockaddr_un un;

						if (buffer_string_length(host->unixsocket) + 1 > sizeof(un.sun_path) - 2) {
							log_error_write(srv, __FILE__, __LINE__, "sbsbsbs",
									"unixsocket is too long in:",
									da->key, "= (",
									da_ext->key, " => (",
									da_host->key, " ( ...");

							goto error;
						}
					} else {
						/* tcp/ip */

						if (buffer_string_is_empty(host->host) &&
						    buffer_string_is_empty(host->bin_path)) {
							log_error_write(srv, __FILE__, __LINE__, "sbsbsbs",
									"host or binpath have to be set in:",
									da->key, "= (",
									da_ext->key, " => (",
									da_host->key, " ( ...");

							goto error;
						} else if (host->port == 0) {
							log_error_write(srv, __FILE__, __LINE__, "sbsbsbs",
									"port has to be set in:",
									da->key, "= (",
									da_ext->key, " => (",
									da_host->key, " ( ...");

							goto error;
						}
					}

					if (!buffer_string_is_empty(host->bin_path)) {
						/* a local socket + self spawning */
						size_t pno;

						if (s->debug) {
							log_error_write(srv, __FILE__, __LINE__, "ssbsdsbsd",
									"--- fastcgi spawning local",
									"\n\tproc:", host->bin_path,
									"\n\tport:", host->port,
									"\n\tsocket", host->unixsocket,
									"\n\tmax-procs:", host->max_procs);
						}

						for (pno = 0; pno < host->max_procs; pno++) {
							fcgi_proc *proc;

							proc = fastcgi_process_init();
							proc->id = host->num_procs++;
							host->max_id++;

							if (buffer_string_is_empty(host->unixsocket)) {
								proc->port = host->port + pno;
							} else {
								buffer_copy_buffer(proc->unixsocket, host->unixsocket);
								buffer_append_string_len(proc->unixsocket, CONST_STR_LEN("-"));
								buffer_append_int(proc->unixsocket, pno);
							}

							if (s->debug) {
								log_error_write(srv, __FILE__, __LINE__, "ssdsbsdsd",
										"--- fastcgi spawning",
										"\n\tport:", host->port,
										"\n\tsocket", host->unixsocket,
										"\n\tcurrent:", pno, "/", host->max_procs);
							}

							if (fcgi_spawn_connection(srv, p, host, proc)) {
								log_error_write(srv, __FILE__, __LINE__, "s",
										"[ERROR]: spawning fcgi failed.");
								fastcgi_process_free(proc);
								goto error;
							}

							fastcgi_status_init(srv, p->statuskey, host, proc);

							proc->next = host->first;
							if (host->first) host->first->prev = proc;

							host->first = proc;
						}
					} else {
						fcgi_proc *proc;

						proc = fastcgi_process_init();
						proc->id = host->num_procs++;
						host->max_id++;
						host->active_procs++;
						proc->state = PROC_STATE_RUNNING;

						if (buffer_string_is_empty(host->unixsocket)) {
							proc->port = host->port;
						} else {
							buffer_copy_buffer(proc->unixsocket, host->unixsocket);
						}

						fastcgi_status_init(srv, p->statuskey, host, proc);

						host->first = proc;

						host->max_procs = 1;
					}

					if (!buffer_string_is_empty(fcgi_mode)) {
						if (strcmp(fcgi_mode->ptr, "responder") == 0) {
							host->mode = FCGI_RESPONDER;
						} else if (strcmp(fcgi_mode->ptr, "authorizer") == 0) {
							host->mode = FCGI_AUTHORIZER;
							if (buffer_string_is_empty(host->docroot)) {
								log_error_write(srv, __FILE__, __LINE__, "s",
										"ERROR: docroot is required for authorizer mode.");
								goto error;
							}
						} else {
							log_error_write(srv, __FILE__, __LINE__, "sbs",
									"WARNING: unknown fastcgi mode:",
									fcgi_mode, "(ignored, mode set to responder)");
						}
					}

					/* if extension already exists, take it */
					fastcgi_extension_insert(s->exts, da_ext->key, host);
					host = NULL;
				}
			}
		}
	}

	buffer_free(fcgi_mode);
	return HANDLER_GO_ON;

error:
	if (NULL != host) fastcgi_host_free(host);
	buffer_free(fcgi_mode);
	return HANDLER_ERROR;
}

static int fcgi_set_state(server *srv, handler_ctx *hctx, fcgi_connection_state_t state) {
	hctx->state = state;
	hctx->state_timestamp = srv->cur_ts;

	return 0;
}


static void fcgi_connection_close(server *srv, handler_ctx *hctx) {
	plugin_data *p;
	connection  *con;

	if (NULL == hctx) return;

	p    = hctx->plugin_data;
	con  = hctx->remote_conn;

	if (hctx->fd != -1) {
		fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
		fdevent_unregister(srv->ev, hctx->fd);
		close(hctx->fd);
		srv->cur_fds--;
	}

	if (hctx->host && hctx->proc) {
		if (hctx->got_proc) {
			/* after the connect the process gets a load */
			fcgi_proc_load_dec(srv, hctx);

			if (p->conf.debug) {
				log_error_write(srv, __FILE__, __LINE__, "ssdsbsd",
						"released proc:",
						"pid:", hctx->proc->pid,
						"socket:", hctx->proc->connection_name,
						"load:", hctx->proc->load);
			}
		}
	}


	handler_ctx_free(srv, hctx);
	con->plugin_ctx[p->id] = NULL;
}

static int fcgi_reconnect(server *srv, handler_ctx *hctx) {
	plugin_data *p    = hctx->plugin_data;

	/* child died
	 *
	 * 1.
	 *
	 * connect was ok, connection was accepted
	 * but the php accept loop checks after the accept if it should die or not.
	 *
	 * if yes we can only detect it at a write()
	 *
	 * next step is resetting this attemp and setup a connection again
	 *
	 * if we have more than 5 reconnects for the same request, die
	 *
	 * 2.
	 *
	 * we have a connection but the child died by some other reason
	 *
	 */

	if (hctx->fd != -1) {
		fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
		fdevent_unregister(srv->ev, hctx->fd);
		close(hctx->fd);
		srv->cur_fds--;
		hctx->fd = -1;
	}

	fcgi_set_state(srv, hctx, FCGI_STATE_INIT);

	hctx->request_id = 0;
	hctx->reconnects++;

	if (p->conf.debug > 2) {
		if (hctx->proc) {
			log_error_write(srv, __FILE__, __LINE__, "sdb",
					"release proc for reconnect:",
					hctx->proc->pid, hctx->proc->connection_name);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					"release proc for reconnect:",
					hctx->host->unixsocket);
		}
	}

	if (hctx->proc && hctx->got_proc) {
		fcgi_proc_load_dec(srv, hctx);
	}

	/* perhaps another host gives us more luck */
	fcgi_host_reset(srv, hctx);

	return 0;
}


static handler_t fcgi_connection_reset(server *srv, connection *con, void *p_d) {
	plugin_data *p = p_d;

	fcgi_connection_close(srv, con->plugin_ctx[p->id]);

	return HANDLER_GO_ON;
}


static int fcgi_env_add(buffer *env, const char *key, size_t key_len, const char *val, size_t val_len) {
	size_t len;
	char len_enc[8];
	size_t len_enc_len = 0;

	if (!key || !val) return -1;

	len = key_len + val_len;

	len += key_len > 127 ? 4 : 1;
	len += val_len > 127 ? 4 : 1;

	if (buffer_string_length(env) + len >= FCGI_MAX_LENGTH) {
		/**
		 * we can't append more headers, ignore it
		 */
		return -1;
	}

	/**
	 * field length can be 31bit max
	 *
	 * HINT: this can't happen as FCGI_MAX_LENGTH is only 16bit
	 */
	force_assert(key_len < 0x7fffffffu);
	force_assert(val_len < 0x7fffffffu);

	buffer_string_prepare_append(env, len);

	if (key_len > 127) {
		len_enc[len_enc_len++] = ((key_len >> 24) & 0xff) | 0x80;
		len_enc[len_enc_len++] = (key_len >> 16) & 0xff;
		len_enc[len_enc_len++] = (key_len >> 8) & 0xff;
		len_enc[len_enc_len++] = (key_len >> 0) & 0xff;
	} else {
		len_enc[len_enc_len++] = (key_len >> 0) & 0xff;
	}

	if (val_len > 127) {
		len_enc[len_enc_len++] = ((val_len >> 24) & 0xff) | 0x80;
		len_enc[len_enc_len++] = (val_len >> 16) & 0xff;
		len_enc[len_enc_len++] = (val_len >> 8) & 0xff;
		len_enc[len_enc_len++] = (val_len >> 0) & 0xff;
	} else {
		len_enc[len_enc_len++] = (val_len >> 0) & 0xff;
	}

	buffer_append_string_len(env, len_enc, len_enc_len);
	buffer_append_string_len(env, key, key_len);
	buffer_append_string_len(env, val, val_len);

	return 0;
}

static int fcgi_header(FCGI_Header * header, unsigned char type, size_t request_id, int contentLength, unsigned char paddingLength) {
	force_assert(contentLength <= FCGI_MAX_LENGTH);
	
	header->version = FCGI_VERSION_1;
	header->type = type;
	header->requestIdB0 = request_id & 0xff;
	header->requestIdB1 = (request_id >> 8) & 0xff;
	header->contentLengthB0 = contentLength & 0xff;
	header->contentLengthB1 = (contentLength >> 8) & 0xff;
	header->paddingLength = paddingLength;
	header->reserved = 0;

	return 0;
}

typedef enum {
	CONNECTION_OK,
	CONNECTION_DELAYED, /* retry after event, take same host */
	CONNECTION_OVERLOADED, /* disable for 1 second, take another backend */
	CONNECTION_DEAD /* disable for 60 seconds, take another backend */
} connection_result_t;

static connection_result_t fcgi_establish_connection(server *srv, handler_ctx *hctx) {
	struct sockaddr *fcgi_addr;
	struct sockaddr_in fcgi_addr_in;
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un fcgi_addr_un;
#endif
	socklen_t servlen;

	fcgi_extension_host *host = hctx->host;
	fcgi_proc *proc   = hctx->proc;
	int fcgi_fd       = hctx->fd;

	if (!buffer_string_is_empty(proc->unixsocket)) {
#ifdef HAVE_SYS_UN_H
		/* use the unix domain socket */
		memset(&fcgi_addr_un, 0, sizeof(fcgi_addr_un));
		fcgi_addr_un.sun_family = AF_UNIX;
		if (buffer_string_length(proc->unixsocket) + 1 > sizeof(fcgi_addr_un.sun_path)) {
			log_error_write(srv, __FILE__, __LINE__, "sB",
					"ERROR: Unix Domain socket filename too long:",
					proc->unixsocket);
			return -1;
		}
		memcpy(fcgi_addr_un.sun_path, proc->unixsocket->ptr, buffer_string_length(proc->unixsocket) + 1);

#ifdef SUN_LEN
		servlen = SUN_LEN(&fcgi_addr_un);
#else
		/* stevens says: */
		servlen = buffer_string_length(proc->unixsocket) + 1 + sizeof(fcgi_addr_un.sun_family);
#endif
		fcgi_addr = (struct sockaddr *) &fcgi_addr_un;

		if (buffer_string_is_empty(proc->connection_name)) {
			/* on remote spawing we have to set the connection-name now */
			buffer_copy_string_len(proc->connection_name, CONST_STR_LEN("unix:"));
			buffer_append_string_buffer(proc->connection_name, proc->unixsocket);
		}
#else
		return CONNECTION_DEAD;
#endif
	} else {
		memset(&fcgi_addr_in, 0, sizeof(fcgi_addr_in));
		fcgi_addr_in.sin_family = AF_INET;
		if (!buffer_string_is_empty(host->host)) {
			if (0 == inet_aton(host->host->ptr, &(fcgi_addr_in.sin_addr))) {
				log_error_write(srv, __FILE__, __LINE__, "sbs",
						"converting IP address failed for", host->host,
						"\nBe sure to specify an IP address here");
	
				return CONNECTION_DEAD;
			}
		} else {
			fcgi_addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		}
		fcgi_addr_in.sin_port = htons(proc->port);
		servlen = sizeof(fcgi_addr_in);

		fcgi_addr = (struct sockaddr *) &fcgi_addr_in;

		if (buffer_string_is_empty(proc->connection_name)) {
			/* on remote spawing we have to set the connection-name now */
			buffer_copy_string_len(proc->connection_name, CONST_STR_LEN("tcp:"));
			if (!buffer_string_is_empty(host->host)) {
				buffer_append_string_buffer(proc->connection_name, host->host);
			} else {
				buffer_append_string_len(proc->connection_name, CONST_STR_LEN("localhost"));
			}
			buffer_append_string_len(proc->connection_name, CONST_STR_LEN(":"));
			buffer_append_int(proc->connection_name, proc->port);
		}
	}

	if (-1 == connect(fcgi_fd, fcgi_addr, servlen)) {
		if (errno == EINPROGRESS ||
		    errno == EALREADY ||
		    errno == EINTR) {
			if (hctx->conf.debug > 2) {
				log_error_write(srv, __FILE__, __LINE__, "sb",
					"connect delayed; will continue later:", proc->connection_name);
			}

			return CONNECTION_DELAYED;
		} else if (errno == EAGAIN) {
			if (hctx->conf.debug) {
				log_error_write(srv, __FILE__, __LINE__, "sbsd",
					"This means that you have more incoming requests than your FastCGI backend can handle in parallel."
					"It might help to spawn more FastCGI backends or PHP children; if not, decrease server.max-connections."
					"The load for this FastCGI backend", proc->connection_name, "is", proc->load);
			}

			return CONNECTION_OVERLOADED;
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sssb",
					"connect failed:",
					strerror(errno), "on",
					proc->connection_name);

			return CONNECTION_DEAD;
		}
	}

	hctx->reconnects = 0;
	if (hctx->conf.debug > 1) {
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"connect succeeded: ", fcgi_fd);
	}

	return CONNECTION_OK;
}

#define FCGI_ENV_ADD_CHECK(ret, con) \
	if (ret == -1) { \
		con->http_status = 400; \
		con->file_finished = 1; \
		return -1; \
	};
static int fcgi_env_add_request_headers(server *srv, connection *con, plugin_data *p) {
	size_t i;

	for (i = 0; i < con->request.headers->used; i++) {
		data_string *ds;

		ds = (data_string *)con->request.headers->data[i];

		if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key)) {
			buffer_copy_string_encoded_cgi_varnames(srv->tmp_buf, CONST_BUF_LEN(ds->key), 1);

			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_BUF_LEN(srv->tmp_buf), CONST_BUF_LEN(ds->value)),con);
		}
	}

	for (i = 0; i < con->environment->used; i++) {
		data_string *ds;

		ds = (data_string *)con->environment->data[i];

		if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key)) {
			buffer_copy_string_encoded_cgi_varnames(srv->tmp_buf, CONST_BUF_LEN(ds->key), 0);

			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_BUF_LEN(srv->tmp_buf), CONST_BUF_LEN(ds->value)), con);
		}
	}

	return 0;
}

static int fcgi_create_env(server *srv, handler_ctx *hctx, size_t request_id) {
	FCGI_BeginRequestRecord beginRecord;
	FCGI_Header header;

	char buf[LI_ITOSTRING_LENGTH];
	const char *s;
#ifdef HAVE_IPV6
	char b2[INET6_ADDRSTRLEN + 1];
#endif

	plugin_data *p    = hctx->plugin_data;
	fcgi_extension_host *host= hctx->host;

	connection *con   = hctx->remote_conn;
	server_socket *srv_sock = con->srv_socket;

	sock_addr our_addr;
	socklen_t our_addr_len;

	/* send FCGI_BEGIN_REQUEST */

	fcgi_header(&(beginRecord.header), FCGI_BEGIN_REQUEST, request_id, sizeof(beginRecord.body), 0);
	beginRecord.body.roleB0 = host->mode;
	beginRecord.body.roleB1 = 0;
	beginRecord.body.flags = 0;
	memset(beginRecord.body.reserved, 0, sizeof(beginRecord.body.reserved));

	/* send FCGI_PARAMS */
	buffer_string_prepare_copy(p->fcgi_env, 1023);


	if (buffer_is_empty(con->conf.server_tag)) {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_SOFTWARE"), CONST_STR_LEN(PACKAGE_DESC)),con)
	} else {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_SOFTWARE"), CONST_BUF_LEN(con->conf.server_tag)),con)
	}

	if (!buffer_is_empty(con->server_name)) {
		size_t len = buffer_string_length(con->server_name);

		if (con->server_name->ptr[0] == '[') {
			const char *colon = strstr(con->server_name->ptr, "]:");
			if (colon) len = (colon + 1) - con->server_name->ptr;
		} else {
			const char *colon = strchr(con->server_name->ptr, ':');
			if (colon) len = colon - con->server_name->ptr;
		}

		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_NAME"), con->server_name->ptr, len),con)
	} else {
#ifdef HAVE_IPV6
		s = inet_ntop(srv_sock->addr.plain.sa_family,
			      srv_sock->addr.plain.sa_family == AF_INET6 ?
			      (const void *) &(srv_sock->addr.ipv6.sin6_addr) :
			      (const void *) &(srv_sock->addr.ipv4.sin_addr),
			      b2, sizeof(b2)-1);
#else
		s = inet_ntoa(srv_sock->addr.ipv4.sin_addr);
#endif
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_NAME"), s, strlen(s)),con)
	}

	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("GATEWAY_INTERFACE"), CONST_STR_LEN("CGI/1.1")),con)

	li_utostr(buf,
#ifdef HAVE_IPV6
	       ntohs(srv_sock->addr.plain.sa_family ? srv_sock->addr.ipv6.sin6_port : srv_sock->addr.ipv4.sin_port)
#else
	       ntohs(srv_sock->addr.ipv4.sin_port)
#endif
	       );

	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_PORT"), buf, strlen(buf)),con)

	/* get the server-side of the connection to the client */
	our_addr_len = sizeof(our_addr);

	if (-1 == getsockname(con->fd, &(our_addr.plain), &our_addr_len)) {
		s = inet_ntop_cache_get_ip(srv, &(srv_sock->addr));
	} else {
		s = inet_ntop_cache_get_ip(srv, &(our_addr));
	}
	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_ADDR"), s, strlen(s)),con)

	li_utostr(buf,
#ifdef HAVE_IPV6
	       ntohs(con->dst_addr.plain.sa_family ? con->dst_addr.ipv6.sin6_port : con->dst_addr.ipv4.sin_port)
#else
	       ntohs(con->dst_addr.ipv4.sin_port)
#endif
	       );

	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REMOTE_PORT"), buf, strlen(buf)),con)

	s = inet_ntop_cache_get_ip(srv, &(con->dst_addr));
	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REMOTE_ADDR"), s, strlen(s)),con)

	if (con->request.content_length > 0 && host->mode != FCGI_AUTHORIZER) {
		/* CGI-SPEC 6.1.2 and FastCGI spec 6.3 */

		li_itostr(buf, con->request.content_length);
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("CONTENT_LENGTH"), buf, strlen(buf)),con)
	}

	if (host->mode != FCGI_AUTHORIZER) {
		/*
		 * SCRIPT_NAME, PATH_INFO and PATH_TRANSLATED according to
		 * http://cgi-spec.golux.com/draft-coar-cgi-v11-03-clean.html
		 * (6.1.14, 6.1.6, 6.1.7)
		 * For AUTHORIZER mode these headers should be omitted.
		 */

		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SCRIPT_NAME"), CONST_BUF_LEN(con->uri.path)),con)

		if (!buffer_string_is_empty(con->request.pathinfo)) {
			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("PATH_INFO"), CONST_BUF_LEN(con->request.pathinfo)),con)

			/* PATH_TRANSLATED is only defined if PATH_INFO is set */

			if (!buffer_string_is_empty(host->docroot)) {
				buffer_copy_buffer(p->path, host->docroot);
			} else {
				buffer_copy_buffer(p->path, con->physical.basedir);
			}
			buffer_append_string_buffer(p->path, con->request.pathinfo);
			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("PATH_TRANSLATED"), CONST_BUF_LEN(p->path)),con)
		} else {
			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("PATH_INFO"), CONST_STR_LEN("")),con)
		}
	}

	/*
	 * SCRIPT_FILENAME and DOCUMENT_ROOT for php. The PHP manual
	 * http://www.php.net/manual/en/reserved.variables.php
	 * treatment of PATH_TRANSLATED is different from the one of CGI specs.
	 * TODO: this code should be checked against cgi.fix_pathinfo php
	 * parameter.
	 */

	if (!buffer_string_is_empty(host->docroot)) {
		/*
		 * rewrite SCRIPT_FILENAME
		 *
		 */

		buffer_copy_buffer(p->path, host->docroot);
		buffer_append_string_buffer(p->path, con->uri.path);

		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SCRIPT_FILENAME"), CONST_BUF_LEN(p->path)),con)
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("DOCUMENT_ROOT"), CONST_BUF_LEN(host->docroot)),con)
	} else {
		buffer_copy_buffer(p->path, con->physical.path);

		/* cgi.fix_pathinfo need a broken SCRIPT_FILENAME to find out what PATH_INFO is itself
		 *
		 * see src/sapi/cgi_main.c, init_request_info()
		 */
		if (host->break_scriptfilename_for_php) {
			buffer_append_string_buffer(p->path, con->request.pathinfo);
		}

		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SCRIPT_FILENAME"), CONST_BUF_LEN(p->path)),con)
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("DOCUMENT_ROOT"), CONST_BUF_LEN(con->physical.basedir)),con)
	}

	if (!buffer_string_is_empty(host->strip_request_uri)) {
		/* we need at least one char to strip off */
		/**
		 * /app1/index/list
		 *
		 * stripping /app1 or /app1/ should lead to
		 *
		 * /index/list
		 *
		 */
		if ('/' != host->strip_request_uri->ptr[buffer_string_length(host->strip_request_uri) - 1]) {
			/* fix the user-input to have / as last char */
			buffer_append_string_len(host->strip_request_uri, CONST_STR_LEN("/"));
		}

		if (buffer_string_length(con->request.orig_uri) >= buffer_string_length(host->strip_request_uri) &&
		    0 == strncmp(con->request.orig_uri->ptr, host->strip_request_uri->ptr, buffer_string_length(host->strip_request_uri))) {
			/* the left is the same */

			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REQUEST_URI"),
					con->request.orig_uri->ptr + (buffer_string_length(host->strip_request_uri) - 1),
					buffer_string_length(con->request.orig_uri) - (buffer_string_length(host->strip_request_uri) - 1)), con);
		} else {
			FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REQUEST_URI"), CONST_BUF_LEN(con->request.orig_uri)),con)
		}
	} else {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REQUEST_URI"), CONST_BUF_LEN(con->request.orig_uri)),con)
	}
	if (!buffer_is_equal(con->request.uri, con->request.orig_uri)) {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REDIRECT_URI"), CONST_BUF_LEN(con->request.uri)),con)
	}
	if (!buffer_string_is_empty(con->uri.query)) {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("QUERY_STRING"), CONST_BUF_LEN(con->uri.query)),con)
	} else {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("QUERY_STRING"), CONST_STR_LEN("")),con)
	}

	s = get_http_method_name(con->request.http_method);
	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REQUEST_METHOD"), s, strlen(s)),con)
	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("REDIRECT_STATUS"), CONST_STR_LEN("200")),con) /* if php is compiled with --force-redirect */
	s = get_http_version_name(con->request.http_version);
	FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("SERVER_PROTOCOL"), s, strlen(s)),con)

	if (buffer_is_equal_caseless_string(con->uri.scheme, CONST_STR_LEN("https"))) {
		FCGI_ENV_ADD_CHECK(fcgi_env_add(p->fcgi_env, CONST_STR_LEN("HTTPS"), CONST_STR_LEN("on")),con)
	}

	FCGI_ENV_ADD_CHECK(fcgi_env_add_request_headers(srv, con, p), con);

	{
		buffer *b = buffer_init();

		buffer_copy_string_len(b, (const char *)&beginRecord, sizeof(beginRecord));

		fcgi_header(&(header), FCGI_PARAMS, request_id, buffer_string_length(p->fcgi_env), 0);
		buffer_append_string_len(b, (const char *)&header, sizeof(header));
		buffer_append_string_buffer(b, p->fcgi_env);

		fcgi_header(&(header), FCGI_PARAMS, request_id, 0, 0);
		buffer_append_string_len(b, (const char *)&header, sizeof(header));

		chunkqueue_append_buffer(hctx->wb, b);
		buffer_free(b);
	}

	if (con->request.content_length) {
		chunkqueue *req_cq = con->request_content_queue;
		off_t offset;

		/* something to send ? */
		for (offset = 0; offset != req_cq->bytes_in; ) {
			off_t weWant = req_cq->bytes_in - offset > FCGI_MAX_LENGTH ? FCGI_MAX_LENGTH : req_cq->bytes_in - offset;

			/* we announce toWrite octets
			 * now take all the request_content chunks that we need to fill this request
			 * */

			fcgi_header(&(header), FCGI_STDIN, request_id, weWant, 0);
			chunkqueue_append_mem(hctx->wb, (const char *)&header, sizeof(header));

			if (p->conf.debug > 10) {
				log_error_write(srv, __FILE__, __LINE__, "soso", "tosend:", offset, "/", req_cq->bytes_in);
			}

			chunkqueue_steal(hctx->wb, req_cq, weWant);

			offset += weWant;
		}
	}

	/* terminate STDIN */
	fcgi_header(&(header), FCGI_STDIN, request_id, 0, 0);
	chunkqueue_append_mem(hctx->wb, (const char *)&header, sizeof(header));

	return 0;
}

static int fcgi_response_parse(server *srv, connection *con, plugin_data *p, buffer *in) {
	char *s, *ns;

	handler_ctx *hctx = con->plugin_ctx[p->id];
	fcgi_extension_host *host= hctx->host;
	int have_sendfile2 = 0;
	off_t sendfile2_content_length = 0;

	UNUSED(srv);

	/* search for \n */
	for (s = in->ptr; NULL != (ns = strchr(s, '\n')); s = ns + 1) {
		char *key, *value;
		int key_len;
		data_string *ds = NULL;

		/* a good day. Someone has read the specs and is sending a \r\n to us */

		if (ns > in->ptr &&
		    *(ns-1) == '\r') {
			*(ns-1) = '\0';
		}

		ns[0] = '\0';

		key = s;
		if (NULL == (value = strchr(s, ':'))) {
			/* we expect: "<key>: <value>\n" */
			continue;
		}

		key_len = value - key;

		value++;
		/* strip WS */
		while (*value == ' ' || *value == '\t') value++;

		if (host->mode != FCGI_AUTHORIZER ||
		    !(con->http_status == 0 ||
		      con->http_status == 200)) {
			/* authorizers shouldn't affect the response headers sent back to the client */

			/* don't forward Status: */
			if (0 != strncasecmp(key, "Status", key_len)) {
				if (NULL == (ds = (data_string *)array_get_unused_element(con->response.headers, TYPE_STRING))) {
					ds = data_response_init();
				}
				buffer_copy_string_len(ds->key, key, key_len);
				buffer_copy_string(ds->value, value);

				array_insert_unique(con->response.headers, (data_unset *)ds);
			}
		}

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
		case 11:
			if (host->allow_xsendfile && 0 == strncasecmp(key, "X-Sendfile2", key_len)&& hctx->send_content_body) {
				char *pos = value;
				have_sendfile2 = 1;

				while (*pos) {
					char *filename, *range;
					stat_cache_entry *sce;
					off_t begin_range, end_range, range_len;

					while (' ' == *pos) pos++;
					if (!*pos) break;

					filename = pos;
					if (NULL == (range = strchr(pos, ' '))) {
						/* missing range */
						if (p->conf.debug) {
							log_error_write(srv, __FILE__, __LINE__, "ss", "Couldn't find range after filename:", filename);
						}
						return 1;
					}
					buffer_copy_string_len(srv->tmp_buf, filename, range - filename);

					/* find end of range */
					for (pos = ++range; *pos && *pos != ' ' && *pos != ','; pos++) ;

					buffer_urldecode_path(srv->tmp_buf);
					if (HANDLER_ERROR == stat_cache_get_entry(srv, con, srv->tmp_buf, &sce)) {
						if (p->conf.debug) {
							log_error_write(srv, __FILE__, __LINE__, "sb",
								"send-file error: couldn't get stat_cache entry for X-Sendfile2:",
								srv->tmp_buf);
						}
						return 1;
					} else if (!S_ISREG(sce->st.st_mode)) {
						if (p->conf.debug) {
							log_error_write(srv, __FILE__, __LINE__, "sb",
								"send-file error: wrong filetype for X-Sendfile2:",
								srv->tmp_buf);
						}
						return 1;
					}
					/* found the file */

					/* parse range */
					begin_range = 0; end_range = sce->st.st_size - 1;
					{
						char *rpos = NULL;
						errno = 0;
						begin_range = strtoll(range, &rpos, 10);
						if (errno != 0 || begin_range < 0 || rpos == range) goto range_failed;
						if ('-' != *rpos++) goto range_failed;
						if (rpos != pos) {
							range = rpos;
							end_range = strtoll(range, &rpos, 10);
							if (errno != 0 || end_range < 0 || rpos == range) goto range_failed;
						}
						if (rpos != pos) goto range_failed;

						goto range_success;

range_failed:
						if (p->conf.debug) {
							log_error_write(srv, __FILE__, __LINE__, "ss", "Couldn't decode range after filename:", filename);
						}
						return 1;

range_success: ;
					}

					/* no parameters accepted */

					while (*pos == ' ') pos++;
					if (*pos != '\0' && *pos != ',') return 1;

					range_len = end_range - begin_range + 1;
					if (range_len < 0) return 1;
					if (range_len != 0) {
						http_chunk_append_file(srv, con, srv->tmp_buf, begin_range, range_len);
					}
					sendfile2_content_length += range_len;

					if (*pos == ',') pos++;
				}
			}
			break;
		case 14:
			if (0 == strncasecmp(key, "Content-Length", key_len)) {
				con->response.content_length = strtol(value, NULL, 10);
				con->parsed_response |= HTTP_CONTENT_LENGTH;

				if (con->response.content_length < 0) con->response.content_length = 0;
			}
			break;
		default:
			break;
		}
	}

	if (have_sendfile2) {
		data_string *dcls;

		hctx->send_content_body = 0;
		joblist_append(srv, con);

		/* fix content-length */
		if (NULL == (dcls = (data_string *)array_get_unused_element(con->response.headers, TYPE_STRING))) {
			dcls = data_response_init();
		}

		buffer_copy_string_len(dcls->key, "Content-Length", sizeof("Content-Length")-1);
		buffer_copy_int(dcls->value, sendfile2_content_length);
		dcls = (data_string*) array_replace(con->response.headers, (data_unset *)dcls);
		if (dcls) dcls->free((data_unset*)dcls);

		con->parsed_response |= HTTP_CONTENT_LENGTH;
		con->response.content_length = sendfile2_content_length;
	}

	/* CGI/1.1 rev 03 - 7.2.1.2 */
	if ((con->parsed_response & HTTP_LOCATION) &&
	    !(con->parsed_response & HTTP_STATUS)) {
		con->http_status = 302;
	}

	return 0;
}

typedef struct {
	buffer  *b;
	size_t   len;
	int      type;
	int      padding;
	size_t   request_id;
} fastcgi_response_packet;

static int fastcgi_get_packet(server *srv, handler_ctx *hctx, fastcgi_response_packet *packet) {
	chunk *c;
	size_t offset;
	size_t toread;
	FCGI_Header *header;

	if (!hctx->rb->first) return -1;

	packet->b = buffer_init();
	packet->len = 0;
	packet->type = 0;
	packet->padding = 0;
	packet->request_id = 0;

	offset = 0; toread = 8;
	/* get at least the FastCGI header */
	for (c = hctx->rb->first; c; c = c->next) {
		size_t weHave = buffer_string_length(c->mem) - c->offset;

		if (weHave > toread) weHave = toread;

		buffer_append_string_len(packet->b, c->mem->ptr + c->offset, weHave);
		toread -= weHave;
		offset = weHave; /* skip offset bytes in chunk for "real" data */

		if (0 == toread) break;
	}

	if (buffer_string_length(packet->b) < sizeof(FCGI_Header)) {
		/* no header */
		if (hctx->plugin_data->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "sdsds", "FastCGI: header too small:", buffer_string_length(packet->b), "bytes <", sizeof(FCGI_Header), "bytes, waiting for more data");
		}

		buffer_free(packet->b);

		return -1;
	}

	/* we have at least a header, now check how much me have to fetch */
	header = (FCGI_Header *)(packet->b->ptr);

	packet->len = (header->contentLengthB0 | (header->contentLengthB1 << 8)) + header->paddingLength;
	packet->request_id = (header->requestIdB0 | (header->requestIdB1 << 8));
	packet->type = header->type;
	packet->padding = header->paddingLength;

	/* ->b should only be the content */
	buffer_string_set_length(packet->b, 0);

	if (packet->len) {
		/* copy the content */
		for (; c && (buffer_string_length(packet->b) < packet->len); c = c->next) {
			size_t weWant = packet->len - buffer_string_length(packet->b);
			size_t weHave = buffer_string_length(c->mem) - c->offset - offset;

			if (weHave > weWant) weHave = weWant;

			buffer_append_string_len(packet->b, c->mem->ptr + c->offset + offset, weHave);

			/* we only skipped the first bytes as they belonged to the fcgi header */
			offset = 0;
		}

		if (buffer_string_length(packet->b) < packet->len) {
			/* we didn't get the full packet */

			buffer_free(packet->b);
			return -1;
		}

		buffer_string_set_length(packet->b, buffer_string_length(packet->b) - packet->padding);
	}

	chunkqueue_mark_written(hctx->rb, packet->len + sizeof(FCGI_Header));

	return 0;
}

static int fcgi_demux_response(server *srv, handler_ctx *hctx) {
	int fin = 0;
	int toread;
	ssize_t r;

	plugin_data *p    = hctx->plugin_data;
	connection *con   = hctx->remote_conn;
	int fcgi_fd       = hctx->fd;
	fcgi_extension_host *host= hctx->host;
	fcgi_proc *proc   = hctx->proc;

	/*
	 * check how much we have to read
	 */
	if (ioctl(hctx->fd, FIONREAD, &toread)) {
		if (errno == EAGAIN) return 0;
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"unexpected end-of-file (perhaps the fastcgi process died):",
				fcgi_fd);
		return -1;
	}

	if (toread > 0) {
		char *mem;
		size_t mem_len;

		chunkqueue_get_memory(hctx->rb, &mem, &mem_len, 0, toread);
		r = read(hctx->fd, mem, mem_len);
		chunkqueue_use_memory(hctx->rb, r > 0 ? r : 0);

		if (-1 == r) {
			if (errno == EAGAIN) return 0;
			log_error_write(srv, __FILE__, __LINE__, "sds",
					"unexpected end-of-file (perhaps the fastcgi process died):",
					fcgi_fd, strerror(errno));
			return -1;
		}
	} else {
		log_error_write(srv, __FILE__, __LINE__, "ssdsb",
				"unexpected end-of-file (perhaps the fastcgi process died):",
				"pid:", proc->pid,
				"socket:", proc->connection_name);

		return -1;
	}

	/*
	 * parse the fastcgi packets and forward the content to the write-queue
	 *
	 */
	while (fin == 0) {
		fastcgi_response_packet packet;

		/* check if we have at least one packet */
		if (0 != fastcgi_get_packet(srv, hctx, &packet)) {
			/* no full packet */
			break;
		}

		switch(packet.type) {
		case FCGI_STDOUT:
			if (packet.len == 0) break;

			/* is the header already finished */
			if (0 == con->file_started) {
				char *c;
				data_string *ds;

				/* search for header terminator
				 *
				 * if we start with \r\n check if last packet terminated with \r\n
				 * if we start with \n check if last packet terminated with \n
				 * search for \r\n\r\n
				 * search for \n\n
				 */

				buffer_append_string_buffer(hctx->response_header, packet.b);

				if (NULL != (c = buffer_search_string_len(hctx->response_header, CONST_STR_LEN("\r\n\r\n")))) {
					char *hend = c + 4; /* header end == body start */
					size_t hlen = hend - hctx->response_header->ptr;
					buffer_copy_string_len(packet.b, hend, buffer_string_length(hctx->response_header) - hlen);
					buffer_string_set_length(hctx->response_header, hlen);
				} else if (NULL != (c = buffer_search_string_len(hctx->response_header, CONST_STR_LEN("\n\n")))) {
					char *hend = c + 2; /* header end == body start */
					size_t hlen = hend - hctx->response_header->ptr;
					buffer_copy_string_len(packet.b, hend, buffer_string_length(hctx->response_header) - hlen);
					buffer_string_set_length(hctx->response_header, hlen);
				} else {
					/* no luck, no header found */
					break;
				}

				/* parse the response header */
				if (fcgi_response_parse(srv, con, p, hctx->response_header)) {
					con->http_status = 502;
					hctx->send_content_body = 0;
					con->file_started = 1;
					break;
				}

				con->file_started = 1;

				if (host->mode == FCGI_AUTHORIZER &&
				    (con->http_status == 0 ||
				     con->http_status == 200)) {
					/* a authorizer with approved the static request, ignore the content here */
					hctx->send_content_body = 0;
				}

				if (host->allow_xsendfile && hctx->send_content_body &&
				    (NULL != (ds = (data_string *) array_get_element(con->response.headers, "X-LIGHTTPD-send-file"))
					  || NULL != (ds = (data_string *) array_get_element(con->response.headers, "X-Sendfile")))) {
					stat_cache_entry *sce;

					if (HANDLER_ERROR != stat_cache_get_entry(srv, con, ds->value, &sce)) {
						data_string *dcls;
						if (NULL == (dcls = (data_string *)array_get_unused_element(con->response.headers, TYPE_STRING))) {
							dcls = data_response_init();
						}
						/* found */
						http_chunk_append_file(srv, con, ds->value, 0, sce->st.st_size);
						hctx->send_content_body = 0; /* ignore the content */
						joblist_append(srv, con);

						buffer_copy_string_len(dcls->key, "Content-Length", sizeof("Content-Length")-1);
						buffer_copy_int(dcls->value, sce->st.st_size);
						dcls = (data_string*) array_replace(con->response.headers, (data_unset *)dcls);
						if (dcls) dcls->free((data_unset*)dcls);

						con->parsed_response |= HTTP_CONTENT_LENGTH;
						con->response.content_length = sce->st.st_size;
					} else {
						log_error_write(srv, __FILE__, __LINE__, "sb",
							"send-file error: couldn't get stat_cache entry for:",
							ds->value);
						con->http_status = 502;
						hctx->send_content_body = 0;
						con->file_started = 1;
						break;
					}
				}


				if (hctx->send_content_body && buffer_string_length(packet.b) > 0) {
					/* enable chunked-transfer-encoding */
					if (con->request.http_version == HTTP_VERSION_1_1 &&
					    !(con->parsed_response & HTTP_CONTENT_LENGTH)) {
						con->response.transfer_encoding = HTTP_TRANSFER_ENCODING_CHUNKED;
					}

					http_chunk_append_buffer(srv, con, packet.b);
					joblist_append(srv, con);
				}
			} else if (hctx->send_content_body && !buffer_string_is_empty(packet.b)) {
				if (con->request.http_version == HTTP_VERSION_1_1 &&
				    !(con->parsed_response & HTTP_CONTENT_LENGTH)) {
					/* enable chunked-transfer-encoding */
					con->response.transfer_encoding = HTTP_TRANSFER_ENCODING_CHUNKED;
				}

				http_chunk_append_buffer(srv, con, packet.b);
				joblist_append(srv, con);
			}
			break;
		case FCGI_STDERR:
			if (packet.len == 0) break;

			log_error_write_multiline_buffer(srv, __FILE__, __LINE__, packet.b, "s",
					"FastCGI-stderr:");

			break;
		case FCGI_END_REQUEST:
			con->file_finished = 1;

			if (host->mode != FCGI_AUTHORIZER ||
			    !(con->http_status == 0 ||
			      con->http_status == 200)) {
				/* send chunk-end if necessary */
				http_chunk_close(srv, con);
				joblist_append(srv, con);
			}

			fin = 1;
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"FastCGI: header.type not handled: ", packet.type);
			break;
		}
		buffer_free(packet.b);
	}

	return fin;
}

static int fcgi_restart_dead_procs(server *srv, plugin_data *p, fcgi_extension_host *host) {
	fcgi_proc *proc;

	for (proc = host->first; proc; proc = proc->next) {
		int status;

		if (p->conf.debug > 2) {
			log_error_write(srv, __FILE__, __LINE__,  "sbdddd",
					"proc:",
					proc->connection_name,
					proc->state,
					proc->is_local,
					proc->load,
					proc->pid);
		}

		/*
		 * if the remote side is overloaded, we check back after <n> seconds
		 *
		 */
		switch (proc->state) {
		case PROC_STATE_KILLED:
		case PROC_STATE_UNSET:
			/* this should never happen as long as adaptive spawing is disabled */
			force_assert(0);

			break;
		case PROC_STATE_RUNNING:
			break;
		case PROC_STATE_OVERLOADED:
			if (srv->cur_ts <= proc->disabled_until) break;

			proc->state = PROC_STATE_RUNNING;
			host->active_procs++;

			log_error_write(srv, __FILE__, __LINE__,  "sbdb",
					"fcgi-server re-enabled:",
					host->host, host->port,
					host->unixsocket);
			break;
		case PROC_STATE_DIED_WAIT_FOR_PID:
			/* non-local procs don't have PIDs to wait for */
			if (!proc->is_local) {
				proc->state = PROC_STATE_DIED;
			} else {
				/* the child should not terminate at all */

				for ( ;; ) {
					switch(waitpid(proc->pid, &status, WNOHANG)) {
					case 0:
						/* child is still alive */
						if (srv->cur_ts <= proc->disabled_until) break;

						proc->state = PROC_STATE_RUNNING;
						host->active_procs++;

						log_error_write(srv, __FILE__, __LINE__,  "sbdb",
							"fcgi-server re-enabled:",
							host->host, host->port,
							host->unixsocket);
						break;
					case -1:
						if (errno == EINTR) continue;

						log_error_write(srv, __FILE__, __LINE__, "sd",
							"child died somehow, waitpid failed:",
							errno);
						proc->state = PROC_STATE_DIED;
						break;
					default:
						if (WIFEXITED(status)) {
#if 0
							log_error_write(srv, __FILE__, __LINE__, "sdsd",
								"child exited, pid:", proc->pid,
								"status:", WEXITSTATUS(status));
#endif
						} else if (WIFSIGNALED(status)) {
							log_error_write(srv, __FILE__, __LINE__, "sd",
								"child signaled:",
								WTERMSIG(status));
						} else {
							log_error_write(srv, __FILE__, __LINE__, "sd",
								"child died somehow:",
								status);
						}

						proc->state = PROC_STATE_DIED;
						break;
					}
					break;
				}
			}

			/* fall through if we have a dead proc now */
			if (proc->state != PROC_STATE_DIED) break;

		case PROC_STATE_DIED:
			/* local procs get restarted by us,
			 * remote ones hopefully by the admin */

			if (!buffer_string_is_empty(host->bin_path)) {
				/* we still have connections bound to this proc,
				 * let them terminate first */
				if (proc->load != 0) break;

				/* restart the child */

				if (p->conf.debug) {
					log_error_write(srv, __FILE__, __LINE__, "ssbsdsd",
							"--- fastcgi spawning",
							"\n\tsocket", proc->connection_name,
							"\n\tcurrent:", 1, "/", host->max_procs);
				}

				if (fcgi_spawn_connection(srv, p, host, proc)) {
					log_error_write(srv, __FILE__, __LINE__, "s",
							"ERROR: spawning fcgi failed.");
					return HANDLER_ERROR;
				}
			} else {
				if (srv->cur_ts <= proc->disabled_until) break;

				proc->state = PROC_STATE_RUNNING;
				host->active_procs++;

				log_error_write(srv, __FILE__, __LINE__,  "sb",
						"fcgi-server re-enabled:",
						proc->connection_name);
			}
			break;
		}
	}

	return 0;
}

static handler_t fcgi_write_request(server *srv, handler_ctx *hctx) {
	plugin_data *p    = hctx->plugin_data;
	fcgi_extension_host *host= hctx->host;
	connection *con   = hctx->remote_conn;
	fcgi_proc  *proc;

	int ret;

	/* sanity check:
	 *  - host != NULL
	 *  - either:
	 *     - tcp socket (do not check host->host->uses, as it may be not set which means INADDR_LOOPBACK)
	 *     - unix socket
	 */
	if (!host) {
		log_error_write(srv, __FILE__, __LINE__, "s", "fatal error: host = NULL");
		return HANDLER_ERROR;
	}
	if ((!host->port && buffer_string_is_empty(host->unixsocket))) {
		log_error_write(srv, __FILE__, __LINE__, "s", "fatal error: neither host->port nor host->unixsocket is set");
		return HANDLER_ERROR;
	}

	/* we can't handle this in the switch as we have to fall through in it */
	if (hctx->state == FCGI_STATE_CONNECT_DELAYED) {
		int socket_error;
		socklen_t socket_error_len = sizeof(socket_error);

		/* try to finish the connect() */
		if (0 != getsockopt(hctx->fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"getsockopt failed:", strerror(errno));

			fcgi_host_disable(srv, hctx);

			return HANDLER_ERROR;
		}
		if (socket_error != 0) {
			if (!hctx->proc->is_local || p->conf.debug) {
				/* local procs get restarted */

				log_error_write(srv, __FILE__, __LINE__, "sssb",
						"establishing connection failed:", strerror(socket_error),
						"socket:", hctx->proc->connection_name);
			}

			fcgi_host_disable(srv, hctx);
			log_error_write(srv, __FILE__, __LINE__, "sdssdsd",
				"backend is overloaded; we'll disable it for", hctx->host->disable_time, "seconds and send the request to another backend instead:",
				"reconnects:", hctx->reconnects,
				"load:", host->load);

			fastcgi_status_copy_procname(p->statuskey, hctx->host, hctx->proc);
			buffer_append_string_len(p->statuskey, CONST_STR_LEN(".died"));

			status_counter_inc(srv, CONST_BUF_LEN(p->statuskey));

			return HANDLER_ERROR;
		}
		/* go on with preparing the request */
		hctx->state = FCGI_STATE_PREPARE_WRITE;
	}


	switch(hctx->state) {
	case FCGI_STATE_CONNECT_DELAYED:
		/* should never happen */
		break;
	case FCGI_STATE_INIT:
		/* do we have a running process for this host (max-procs) ? */
		hctx->proc = NULL;

		for (proc = hctx->host->first;
		     proc && proc->state != PROC_STATE_RUNNING;
		     proc = proc->next);

		/* all children are dead */
		if (proc == NULL) {
			hctx->fde_ndx = -1;

			return HANDLER_ERROR;
		}

		hctx->proc = proc;

		/* check the other procs if they have a lower load */
		for (proc = proc->next; proc; proc = proc->next) {
			if (proc->state != PROC_STATE_RUNNING) continue;
			if (proc->load < hctx->proc->load) hctx->proc = proc;
		}

		ret = buffer_string_is_empty(host->unixsocket) ? AF_INET : AF_UNIX;

		if (-1 == (hctx->fd = socket(ret, SOCK_STREAM, 0))) {
			if (errno == EMFILE ||
			    errno == EINTR) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"wait for fd at connection:", con->fd);

				return HANDLER_WAIT_FOR_FD;
			}

			log_error_write(srv, __FILE__, __LINE__, "ssdd",
					"socket failed:", strerror(errno), srv->cur_fds, srv->max_fds);
			return HANDLER_ERROR;
		}
		hctx->fde_ndx = -1;

		srv->cur_fds++;

		fdevent_register(srv->ev, hctx->fd, fcgi_handle_fdevent, hctx);

		if (-1 == fdevent_fcntl_set(srv->ev, hctx->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"fcntl failed:", strerror(errno));

			return HANDLER_ERROR;
		}

		if (hctx->proc->is_local) {
			hctx->pid = hctx->proc->pid;
		}

		switch (fcgi_establish_connection(srv, hctx)) {
		case CONNECTION_DELAYED:
			/* connection is in progress, wait for an event and call getsockopt() below */

			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_OUT);

			fcgi_set_state(srv, hctx, FCGI_STATE_CONNECT_DELAYED);
			return HANDLER_WAIT_FOR_EVENT;
		case CONNECTION_OVERLOADED:
			/* cool down the backend, it is overloaded
			 * -> EAGAIN */

			if (hctx->host->disable_time) {
				log_error_write(srv, __FILE__, __LINE__, "sdssdsd",
					"backend is overloaded; we'll disable it for", hctx->host->disable_time, "seconds and send the request to another backend instead:",
					"reconnects:", hctx->reconnects,
					"load:", host->load);

				hctx->proc->disabled_until = srv->cur_ts + hctx->host->disable_time;
				if (hctx->proc->state == PROC_STATE_RUNNING) hctx->host->active_procs--;
				hctx->proc->state = PROC_STATE_OVERLOADED;
			}

			fastcgi_status_copy_procname(p->statuskey, hctx->host, hctx->proc);
			buffer_append_string_len(p->statuskey, CONST_STR_LEN(".overloaded"));

			status_counter_inc(srv, CONST_BUF_LEN(p->statuskey));

			return HANDLER_ERROR;
		case CONNECTION_DEAD:
			/* we got a hard error from the backend like
			 * - ECONNREFUSED for tcp-ip sockets
			 * - ENOENT for unix-domain-sockets
			 *
			 * for check if the host is back in hctx->host->disable_time seconds
			 *  */

			fcgi_host_disable(srv, hctx);

			log_error_write(srv, __FILE__, __LINE__, "sdssdsd",
				"backend died; we'll disable it for", hctx->host->disable_time, "seconds and send the request to another backend instead:",
				"reconnects:", hctx->reconnects,
				"load:", host->load);

			fastcgi_status_copy_procname(p->statuskey, hctx->host, hctx->proc);
			buffer_append_string_len(p->statuskey, CONST_STR_LEN(".died"));

			status_counter_inc(srv, CONST_BUF_LEN(p->statuskey));

			return HANDLER_ERROR;
		case CONNECTION_OK:
			/* everything is ok, go on */

			fcgi_set_state(srv, hctx, FCGI_STATE_PREPARE_WRITE);

			break;
		}
		/* fallthrough */
	case FCGI_STATE_PREPARE_WRITE:
		/* ok, we have the connection */

		fcgi_proc_load_inc(srv, hctx);
		hctx->got_proc = 1;

		status_counter_inc(srv, CONST_STR_LEN("fastcgi.requests"));

		fastcgi_status_copy_procname(p->statuskey, hctx->host, hctx->proc);
		buffer_append_string_len(p->statuskey, CONST_STR_LEN(".connected"));

		status_counter_inc(srv, CONST_BUF_LEN(p->statuskey));

		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "ssdsbsd",
				"got proc:",
				"pid:", hctx->proc->pid,
				"socket:", hctx->proc->connection_name,
				"load:", hctx->proc->load);
		}

		/* move the proc-list entry down the list */
		if (hctx->request_id == 0) {
			hctx->request_id = 1; /* always use id 1 as we don't use multiplexing */
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"fcgi-request is already in use:", hctx->request_id);
		}

		if (-1 == fcgi_create_env(srv, hctx, hctx->request_id)) return HANDLER_ERROR;

		fcgi_set_state(srv, hctx, FCGI_STATE_WRITE);
		/* fall through */
	case FCGI_STATE_WRITE:
		ret = srv->network_backend_write(srv, con, hctx->fd, hctx->wb, MAX_WRITE_LIMIT);

		chunkqueue_remove_finished_chunks(hctx->wb);

		if (ret < 0) {
			switch(errno) {
			case EPIPE:
			case ENOTCONN:
			case ECONNRESET:
				/* the connection got dropped after accept()
				 * we don't care about that - if you accept() it, you have to handle it.
				 */

				log_error_write(srv, __FILE__, __LINE__, "ssosb",
							"connection was dropped after accept() (perhaps the fastcgi process died),",
						"write-offset:", hctx->wb->bytes_out,
						"socket:", hctx->proc->connection_name);

				return HANDLER_ERROR;
			default:
				log_error_write(srv, __FILE__, __LINE__, "ssd",
						"write failed:", strerror(errno), errno);

				return HANDLER_ERROR;
			}
		}

		if (hctx->wb->bytes_out == hctx->wb->bytes_in) {
			/* we don't need the out event anymore */
			fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_IN);
			fcgi_set_state(srv, hctx, FCGI_STATE_READ);
		} else {
			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_OUT);

			return HANDLER_WAIT_FOR_EVENT;
		}

		break;
	case FCGI_STATE_READ:
		/* waiting for a response */
		break;
	default:
		log_error_write(srv, __FILE__, __LINE__, "s", "(debug) unknown state");
		return HANDLER_ERROR;
	}

	return HANDLER_WAIT_FOR_EVENT;
}


/* might be called on fdevent after a connect() is delay too
 * */
SUBREQUEST_FUNC(mod_fastcgi_handle_subrequest) {
	plugin_data *p = p_d;

	handler_ctx *hctx = con->plugin_ctx[p->id];
	fcgi_extension_host *host;

	if (NULL == hctx) return HANDLER_GO_ON;

	/* not my job */
	if (con->mode != p->id) return HANDLER_GO_ON;

	/* we don't have a host yet, choose one
	 * -> this happens in the first round
	 *    and when the host died and we have to select a new one */
	if (hctx->host == NULL) {
		size_t k;
		int ndx, used = -1;

		/* check if the next server has no load. */
		ndx = hctx->ext->last_used_ndx + 1;
		if(ndx >= (int) hctx->ext->used || ndx < 0) ndx = 0;
		host = hctx->ext->hosts[ndx];
		if (host->load > 0) {
			/* get backend with the least load. */
			for (k = 0, ndx = -1; k < hctx->ext->used; k++) {
				host = hctx->ext->hosts[k];

				/* we should have at least one proc that can do something */
				if (host->active_procs == 0) continue;

				if (used == -1 || host->load < used) {
					used = host->load;

					ndx = k;
				}
			}
		}

		/* found a server */
		if (ndx == -1) {
			/* all hosts are down */

			fcgi_connection_close(srv, hctx);

			con->http_status = 500;
			con->mode = DIRECT;

			return HANDLER_FINISHED;
		}

		hctx->ext->last_used_ndx = ndx;
		host = hctx->ext->hosts[ndx];

		/*
		 * if check-local is disabled, use the uri.path handler
		 *
		 */

		/* init handler-context */

		/* we put a connection on this host, move the other new connections to other hosts
		 *
		 * as soon as hctx->host is unassigned, decrease the load again */
		fcgi_host_assign(srv, hctx, host);
		hctx->proc = NULL;
	} else {
		host = hctx->host;
	}

	/* ok, create the request */
	switch(fcgi_write_request(srv, hctx)) {
	case HANDLER_ERROR:
		if (hctx->state == FCGI_STATE_INIT ||
		    hctx->state == FCGI_STATE_CONNECT_DELAYED) {
			fcgi_restart_dead_procs(srv, p, host);

			/* cleanup this request and let the request handler start this request again */
			if (hctx->reconnects < 5) {
				fcgi_reconnect(srv, hctx);
				joblist_append(srv, con); /* in case we come from the event-handler */

				return HANDLER_WAIT_FOR_FD;
			} else {
				fcgi_connection_close(srv, hctx);

				buffer_reset(con->physical.path);
				con->mode = DIRECT;
				con->http_status = 503;
				joblist_append(srv, con); /* in case we come from the event-handler */

				return HANDLER_FINISHED;
			}
		} else {
			fcgi_connection_close(srv, hctx);

			buffer_reset(con->physical.path);
			con->mode = DIRECT;
			if (con->http_status != 400) con->http_status = 503;
			joblist_append(srv, con); /* really ? */

			return HANDLER_FINISHED;
		}
	case HANDLER_WAIT_FOR_EVENT:
		if (con->file_started == 1) {
			return HANDLER_FINISHED;
		} else {
			return HANDLER_WAIT_FOR_EVENT;
		}
	case HANDLER_WAIT_FOR_FD:
		return HANDLER_WAIT_FOR_FD;
	default:
		log_error_write(srv, __FILE__, __LINE__, "s", "subrequest write-req default");
		return HANDLER_ERROR;
	}
}

static handler_t fcgi_handle_fdevent(server *srv, void *ctx, int revents) {
	handler_ctx *hctx = ctx;
	connection  *con  = hctx->remote_conn;
	plugin_data *p    = hctx->plugin_data;

	fcgi_proc *proc   = hctx->proc;
	fcgi_extension_host *host= hctx->host;

	if ((revents & FDEVENT_IN) &&
	    hctx->state == FCGI_STATE_READ) {
		switch (fcgi_demux_response(srv, hctx)) {
		case 0:
			break;
		case 1:

			if (host->mode == FCGI_AUTHORIZER &&
		   	    (con->http_status == 200 ||
			     con->http_status == 0)) {
				/*
				 * If we are here in AUTHORIZER mode then a request for authorizer
				 * was processed already, and status 200 has been returned. We need
				 * now to handle authorized request.
				 */

				buffer_copy_buffer(con->physical.doc_root, host->docroot);
				buffer_copy_buffer(con->physical.basedir, host->docroot);

				buffer_copy_buffer(con->physical.path, host->docroot);
				buffer_append_string_buffer(con->physical.path, con->uri.path);
				fcgi_connection_close(srv, hctx);

				con->mode = DIRECT;
				con->http_status = 0;
				con->file_started = 1; /* fcgi_extension won't touch the request afterwards */
			} else {
				/* we are done */
				fcgi_connection_close(srv, hctx);
			}

			joblist_append(srv, con);
			return HANDLER_FINISHED;
		case -1:
			if (proc->pid && proc->state != PROC_STATE_DIED) {
				int status;

				/* only fetch the zombie if it is not already done */

				switch(waitpid(proc->pid, &status, WNOHANG)) {
				case 0:
					/* child is still alive */
					break;
				case -1:
					break;
				default:
					/* the child should not terminate at all */
					if (WIFEXITED(status)) {
						log_error_write(srv, __FILE__, __LINE__, "sdsd",
								"child exited, pid:", proc->pid,
								"status:", WEXITSTATUS(status));
					} else if (WIFSIGNALED(status)) {
						log_error_write(srv, __FILE__, __LINE__, "sd",
								"child signaled:",
								WTERMSIG(status));
					} else {
						log_error_write(srv, __FILE__, __LINE__, "sd",
								"child died somehow:",
								status);
					}

					if (p->conf.debug) {
						log_error_write(srv, __FILE__, __LINE__, "ssbsdsd",
								"--- fastcgi spawning",
								"\n\tsocket", proc->connection_name,
								"\n\tcurrent:", 1, "/", host->max_procs);
					}

					if (fcgi_spawn_connection(srv, p, host, proc)) {
						/* respawning failed, retry later */
						proc->state = PROC_STATE_DIED;

						log_error_write(srv, __FILE__, __LINE__, "s",
								"respawning failed, will retry later");
					}

					break;
				}
			}

			if (con->file_started == 0) {
				/* nothing has been sent out yet, try to use another child */

				if (hctx->wb->bytes_out == 0 &&
				    hctx->reconnects < 5) {
					fcgi_reconnect(srv, hctx);

					log_error_write(srv, __FILE__, __LINE__, "ssbsBSBs",
						"response not received, request not sent",
						"on socket:", proc->connection_name,
						"for", con->uri.path, "?", con->uri.query, ", reconnecting");

					return HANDLER_WAIT_FOR_FD;
				}

				log_error_write(srv, __FILE__, __LINE__, "sosbsBSBs",
						"response not received, request sent:", hctx->wb->bytes_out,
						"on socket:", proc->connection_name,
						"for", con->uri.path, "?", con->uri.query, ", closing connection");

				fcgi_connection_close(srv, hctx);

				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				buffer_reset(con->physical.path);
				con->http_status = 500;
				con->mode = DIRECT;
			} else {
				/* response might have been already started, kill the connection */
				fcgi_connection_close(srv, hctx);

				log_error_write(srv, __FILE__, __LINE__, "ssbsBSBs",
						"response already sent out, but backend returned error",
						"on socket:", proc->connection_name,
						"for", con->uri.path, "?", con->uri.query, ", terminating connection");

				connection_set_state(srv, con, CON_STATE_ERROR);
			}

			/* */


			joblist_append(srv, con);
			return HANDLER_FINISHED;
		}
	}

	if (revents & FDEVENT_OUT) {
		if (hctx->state == FCGI_STATE_CONNECT_DELAYED ||
		    hctx->state == FCGI_STATE_WRITE) {
			/* we are allowed to send something out
			 *
			 * 1. in an unfinished connect() call
			 * 2. in an unfinished write() call (long POST request)
			 */
			return mod_fastcgi_handle_subrequest(srv, con, p);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"got a FDEVENT_OUT and didn't know why:",
					hctx->state);
		}
	}

	/* perhaps this issue is already handled */
	if (revents & FDEVENT_HUP) {
		if (hctx->state == FCGI_STATE_CONNECT_DELAYED) {
			/* getoptsock will catch this one (right ?)
			 *
			 * if we are in connect we might get an EINPROGRESS
			 * in the first call and an FDEVENT_HUP in the
			 * second round
			 *
			 * FIXME: as it is a bit ugly.
			 *
			 */
			return mod_fastcgi_handle_subrequest(srv, con, p);
		} else if (hctx->state == FCGI_STATE_READ &&
			   hctx->proc->port == 0) {
			/* FIXME:
			 *
			 * ioctl says 8192 bytes to read from PHP and we receive directly a HUP for the socket
			 * even if the FCGI_FIN packet is not received yet
			 */
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sBSbsbsd",
					"error: unexpected close of fastcgi connection for",
					con->uri.path, "?", con->uri.query,
					"(no fastcgi process on socket:", proc->connection_name, "?)",
					hctx->state);

			connection_set_state(srv, con, CON_STATE_ERROR);
			fcgi_connection_close(srv, hctx);
			joblist_append(srv, con);
		}
	} else if (revents & FDEVENT_ERR) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"fcgi: got a FDEVENT_ERR. Don't know why.");
		/* kill all connections to the fastcgi process */


		connection_set_state(srv, con, CON_STATE_ERROR);
		fcgi_connection_close(srv, hctx);
		joblist_append(srv, con);
	}

	return HANDLER_FINISHED;
}
#define PATCH(x) \
	p->conf.x = s->x;
static int fcgi_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(exts);
	PATCH(debug);
	PATCH(ext_mapping);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("fastcgi.server"))) {
				PATCH(exts);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("fastcgi.debug"))) {
				PATCH(debug);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("fastcgi.map-extensions"))) {
				PATCH(ext_mapping);
			}
		}
	}

	return 0;
}
#undef PATCH


static handler_t fcgi_check_extension(server *srv, connection *con, void *p_d, int uri_path_handler) {
	plugin_data *p = p_d;
	size_t s_len;
	size_t k;
	buffer *fn;
	fcgi_extension *extension = NULL;
	fcgi_extension_host *host = NULL;

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	/* Possibly, we processed already this request */
	if (con->file_started == 1) return HANDLER_GO_ON;

	fn = uri_path_handler ? con->uri.path : con->physical.path;

	if (buffer_string_is_empty(fn)) return HANDLER_GO_ON;

	s_len = buffer_string_length(fn);

	fcgi_patch_connection(srv, con, p);

	/* fastcgi.map-extensions maps extensions to existing fastcgi.server entries
	 *
	 * fastcgi.map-extensions = ( ".php3" => ".php" )
	 *
	 * fastcgi.server = ( ".php" => ... )
	 *
	 * */

	/* check if extension-mapping matches */
	for (k = 0; k < p->conf.ext_mapping->used; k++) {
		data_string *ds = (data_string *)p->conf.ext_mapping->data[k];
		size_t ct_len; /* length of the config entry */

		if (buffer_is_empty(ds->key)) continue;

		ct_len = buffer_string_length(ds->key);

		if (s_len < ct_len) continue;

		/* found a mapping */
		if (0 == strncmp(fn->ptr + s_len - ct_len, ds->key->ptr, ct_len)) {
			/* check if we know the extension */

			/* we can reuse k here */
			for (k = 0; k < p->conf.exts->used; k++) {
				extension = p->conf.exts->exts[k];

				if (buffer_is_equal(ds->value, extension->key)) {
					break;
				}
			}

			if (k == p->conf.exts->used) {
				/* found nothign */
				extension = NULL;
			}
			break;
		}
	}

	if (extension == NULL) {
		size_t uri_path_len = buffer_string_length(con->uri.path);

		/* check if extension matches */
		for (k = 0; k < p->conf.exts->used; k++) {
			size_t ct_len; /* length of the config entry */
			fcgi_extension *ext = p->conf.exts->exts[k];

			if (buffer_is_empty(ext->key)) continue;

			ct_len = buffer_string_length(ext->key);

			/* check _url_ in the form "/fcgi_pattern" */
			if (ext->key->ptr[0] == '/') {
				if ((ct_len <= uri_path_len) &&
				    (strncmp(con->uri.path->ptr, ext->key->ptr, ct_len) == 0)) {
					extension = ext;
					break;
				}
			} else if ((ct_len <= s_len) && (0 == strncmp(fn->ptr + s_len - ct_len, ext->key->ptr, ct_len))) {
				/* check extension in the form ".fcg" */
				extension = ext;
				break;
			}
		}
		/* extension doesn't match */
		if (NULL == extension) {
			return HANDLER_GO_ON;
		}
	}

	/* check if we have at least one server for this extension up and running */
	for (k = 0; k < extension->used; k++) {
		fcgi_extension_host *h = extension->hosts[k];

		/* we should have at least one proc that can do something */
		if (h->active_procs == 0) {
			continue;
		}

		/* we found one host that is alive */
		host = h;
		break;
	}

	if (!host) {
		/* sorry, we don't have a server alive for this ext */
		buffer_reset(con->physical.path);
		con->http_status = 500;

		/* only send the 'no handler' once */
		if (!extension->note_is_sent) {
			extension->note_is_sent = 1;

			log_error_write(srv, __FILE__, __LINE__, "sBSbsbs",
					"all handlers for", con->uri.path, "?", con->uri.query,
					"on", extension->key,
					"are down.");
		}

		return HANDLER_FINISHED;
	}

	/* a note about no handler is not sent yet */
	extension->note_is_sent = 0;

	/*
	 * if check-local is disabled, use the uri.path handler
	 *
	 */

	/* init handler-context */
	if (uri_path_handler) {
		if (host->check_local == 0) {
			handler_ctx *hctx;
			char *pathinfo;

			hctx = handler_ctx_init();

			hctx->remote_conn      = con;
			hctx->plugin_data      = p;
			hctx->proc	       = NULL;
			hctx->ext              = extension;


			hctx->conf.exts        = p->conf.exts;
			hctx->conf.debug       = p->conf.debug;

			con->plugin_ctx[p->id] = hctx;

			con->mode = p->id;

			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__, "s",
				"handling it in mod_fastcgi");
			}

			/* do not split path info for authorizer */
			if (host->mode != FCGI_AUTHORIZER) {
				/* the prefix is the SCRIPT_NAME,
				* everything from start to the next slash
				* this is important for check-local = "disable"
				*
				* if prefix = /admin.fcgi
				*
				* /admin.fcgi/foo/bar
				*
				* SCRIPT_NAME = /admin.fcgi
				* PATH_INFO   = /foo/bar
				*
				* if prefix = /fcgi-bin/
				*
				* /fcgi-bin/foo/bar
				*
				* SCRIPT_NAME = /fcgi-bin/foo
				* PATH_INFO   = /bar
				*
				* if prefix = /, and fix-root-path-name is enable
				*
				* /fcgi-bin/foo/bar
				*
				* SCRIPT_NAME = /fcgi-bin/foo
				* PATH_INFO   = /bar
				*
				*/

				/* the rewrite is only done for /prefix/? matches */
				if (host->fix_root_path_name && extension->key->ptr[0] == '/' && extension->key->ptr[1] == '\0') {
					buffer_copy_string(con->request.pathinfo, con->uri.path->ptr);
					buffer_string_set_length(con->uri.path, 0);
				} else if (extension->key->ptr[0] == '/' &&
					buffer_string_length(con->uri.path) > buffer_string_length(extension->key) &&
					NULL != (pathinfo = strchr(con->uri.path->ptr + buffer_string_length(extension->key), '/'))) {
					/* rewrite uri.path and pathinfo */

					buffer_copy_string(con->request.pathinfo, pathinfo);
					buffer_string_set_length(con->uri.path, buffer_string_length(con->uri.path) - buffer_string_length(con->request.pathinfo));
				}
			}
		}
	} else {
		handler_ctx *hctx;
		hctx = handler_ctx_init();

		hctx->remote_conn      = con;
		hctx->plugin_data      = p;
		hctx->proc             = NULL;
		hctx->ext              = extension;

		hctx->conf.exts        = p->conf.exts;
		hctx->conf.debug       = p->conf.debug;

		con->plugin_ctx[p->id] = hctx;

		con->mode = p->id;

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__, "s", "handling it in mod_fastcgi");
		}
	}

	return HANDLER_GO_ON;
}

/* uri-path handler */
static handler_t fcgi_check_extension_1(server *srv, connection *con, void *p_d) {
	return fcgi_check_extension(srv, con, p_d, 1);
}

/* start request handler */
static handler_t fcgi_check_extension_2(server *srv, connection *con, void *p_d) {
	return fcgi_check_extension(srv, con, p_d, 0);
}

JOBLIST_FUNC(mod_fastcgi_handle_joblist) {
	plugin_data *p = p_d;
	handler_ctx *hctx = con->plugin_ctx[p->id];

	if (hctx == NULL) return HANDLER_GO_ON;

	if (hctx->fd != -1) {
		switch (hctx->state) {
		case FCGI_STATE_READ:
			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_IN);

			break;
		case FCGI_STATE_CONNECT_DELAYED:
		case FCGI_STATE_WRITE:
			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_OUT);

			break;
		case FCGI_STATE_INIT:
			/* at reconnect */
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sd", "unhandled fcgi.state", hctx->state);
			break;
		}
	}

	return HANDLER_GO_ON;
}


static handler_t fcgi_connection_close_callback(server *srv, connection *con, void *p_d) {
	plugin_data *p = p_d;

	fcgi_connection_close(srv, con->plugin_ctx[p->id]);

	return HANDLER_GO_ON;
}

TRIGGER_FUNC(mod_fastcgi_handle_trigger) {
	plugin_data *p = p_d;
	size_t i, j, n;


	/* perhaps we should kill a connect attempt after 10-15 seconds
	 *
	 * currently we wait for the TCP timeout which is 180 seconds on Linux
	 *
	 *
	 *
	 */

	/* check all children if they are still up */

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *conf;
		fcgi_exts *exts;

		conf = p->config_storage[i];

		exts = conf->exts;

		for (j = 0; j < exts->used; j++) {
			fcgi_extension *ex;

			ex = exts->exts[j];

			for (n = 0; n < ex->used; n++) {

				fcgi_proc *proc;
				fcgi_extension_host *host;

				host = ex->hosts[n];

				fcgi_restart_dead_procs(srv, p, host);

				for (proc = host->unused_procs; proc; proc = proc->next) {
					int status;

					if (proc->pid == 0) continue;

					switch (waitpid(proc->pid, &status, WNOHANG)) {
					case 0:
						/* child still running after timeout, good */
						break;
					case -1:
						if (errno != EINTR) {
							/* no PID found ? should never happen */
							log_error_write(srv, __FILE__, __LINE__, "sddss",
									"pid ", proc->pid, proc->state,
									"not found:", strerror(errno));

#if 0
							if (errno == ECHILD) {
								/* someone else has cleaned up for us */
								proc->pid = 0;
								proc->state = PROC_STATE_UNSET;
							}
#endif
						}
						break;
					default:
						/* the child should not terminate at all */
						if (WIFEXITED(status)) {
							if (proc->state != PROC_STATE_KILLED) {
								log_error_write(srv, __FILE__, __LINE__, "sdb",
										"child exited:",
										WEXITSTATUS(status), proc->connection_name);
							}
						} else if (WIFSIGNALED(status)) {
							if (WTERMSIG(status) != SIGTERM) {
								log_error_write(srv, __FILE__, __LINE__, "sd",
										"child signaled:",
										WTERMSIG(status));
							}
						} else {
							log_error_write(srv, __FILE__, __LINE__, "sd",
									"child died somehow:",
									status);
						}
						proc->pid = 0;
						if (proc->state == PROC_STATE_RUNNING) host->active_procs--;
						proc->state = PROC_STATE_UNSET;
						host->max_id--;
					}
				}
			}
		}
	}

	return HANDLER_GO_ON;
}


int mod_fastcgi_plugin_init(plugin *p);
int mod_fastcgi_plugin_init(plugin *p) {
	p->version      = LIGHTTPD_VERSION_ID;
	p->name         = buffer_init_string("fastcgi");

	p->init         = mod_fastcgi_init;
	p->cleanup      = mod_fastcgi_free;
	p->set_defaults = mod_fastcgi_set_defaults;
	p->connection_reset        = fcgi_connection_reset;
	p->handle_connection_close = fcgi_connection_close_callback;
	p->handle_uri_clean        = fcgi_check_extension_1;
	p->handle_subrequest_start = fcgi_check_extension_2;
	p->handle_subrequest       = mod_fastcgi_handle_subrequest;
	p->handle_joblist          = mod_fastcgi_handle_joblist;
	p->handle_trigger          = mod_fastcgi_handle_trigger;

	p->data         = NULL;

	return 0;
}
