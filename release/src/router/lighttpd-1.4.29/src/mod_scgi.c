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

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>

#include <stdio.h>

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "sys-socket.h"

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include "version.h"

enum {EOL_UNSET, EOL_N, EOL_RN};

/*
 *
 * TODO:
 *
 * - add timeout for a connect to a non-scgi process
 *   (use state_timestamp + state)
 *
 */

typedef struct scgi_proc {
	size_t id; /* id will be between 1 and max_procs */
	buffer *socket; /* config.socket + "-" + id */
	unsigned port;  /* config.port + pno */

	pid_t pid;   /* PID of the spawned process (0 if not spawned locally) */


	size_t load; /* number of requests waiting on this process */

	time_t last_used; /* see idle_timeout */
	size_t requests;  /* see max_requests */
	struct scgi_proc *prev, *next; /* see first */

	time_t disable_ts; /* replace by host->something */

	int is_local;

	enum { PROC_STATE_UNSET,            /* init-phase */
			PROC_STATE_RUNNING, /* alive */
			PROC_STATE_DIED_WAIT_FOR_PID,
			PROC_STATE_KILLED,  /* was killed as we don't have the load anymore */
			PROC_STATE_DIED,    /* marked as dead, should be restarted */
			PROC_STATE_DISABLED /* proc disabled as it resulted in an error */
	} state;
} scgi_proc;

typedef struct {
	/* list of processes handling this extension
	 * sorted by lowest load
	 *
	 * whenever a job is done move it up in the list
	 * until it is sorted, move it down as soon as the
	 * job is started
	 */
	scgi_proc *first;
	scgi_proc *unused_procs;

	/*
	 * spawn at least min_procs, at max_procs.
	 *
	 * as soon as the load of the first entry
	 * is max_load_per_proc we spawn a new one
	 * and add it to the first entry and give it
	 * the load
	 *
	 */

	unsigned short min_procs;
	unsigned short max_procs;
	size_t num_procs;    /* how many procs are started */
	size_t active_procs; /* how many of them are really running */

	unsigned short max_load_per_proc;

	/*
	 * kick the process from the list if it was not
	 * used for idle_timeout until min_procs is
	 * reached. this helps to get the processlist
	 * small again we had a small peak load.
	 *
	 */

	unsigned short idle_timeout;

	/*
	 * time after a disabled remote connection is tried to be re-enabled
	 *
	 *
	 */

	unsigned short disable_time;

	/*
	 * same scgi processes get a little bit larger
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
	 * if tcp/ip should be used host AND port have
	 * to be specified
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

	/* if socket is local we can start the scgi
	 * process ourself
	 *
	 * bin-path is the path to the binary
	 *
	 * check min_procs and max_procs for the number
	 * of process to start-up
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
	 * check_local tell you if the phys file is stat()ed
	 * or not. FastCGI doesn't care if the service is
	 * remote. If the web-server side doesn't contain
	 * the scgi-files we should not stat() for them
	 * and say '404 not found'.
	 */
	unsigned short check_local;

	/*
	 * append PATH_INFO to SCRIPT_FILENAME
	 *
	 * php needs this if cgi.fix_pathinfo is provied
	 *
	 */

	/*
	 * workaround for program when prefix="/"
	 *
	 * rule to build PATH_INFO is hardcoded for when check_local is disabled
	 * enable this option to use the workaround
	 *
	 */

	unsigned short fix_root_path_name;
	ssize_t load; /* replace by host->load */

	size_t max_id; /* corresponds most of the time to
	num_procs.

	only if a process is killed max_id waits for the process itself
	to die and decrements its afterwards */
} scgi_extension_host;

/*
 * one extension can have multiple hosts assigned
 * one host can spawn additional processes on the same
 *   socket (if we control it)
 *
 * ext -> host -> procs
 *    1:n     1:n
 *
 * if the scgi process is remote that whole goes down
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
	scgi_extension_host **hosts;

	size_t used;
	size_t size;
} scgi_extension;

typedef struct {
	scgi_extension **exts;

	size_t used;
	size_t size;
} scgi_exts;


typedef struct {
	scgi_exts *exts;

	int debug;
} plugin_config;

typedef struct {
	char **ptr;

	size_t size;
	size_t used;
} char_array;

/* generic plugin data, shared between all connections */
typedef struct {
	PLUGIN_DATA;

	buffer *scgi_env;

	buffer *path;
	buffer *parse_response;

	plugin_config **config_storage;

	plugin_config conf; /* this is only used as long as no handler_ctx is setup */
} plugin_data;

/* connection specific data */
typedef enum { FCGI_STATE_INIT, FCGI_STATE_CONNECT, FCGI_STATE_PREPARE_WRITE,
		FCGI_STATE_WRITE, FCGI_STATE_READ
} scgi_connection_state_t;

typedef struct {
	buffer  *response;
	size_t   response_len;
	int      response_type;
	int      response_padding;

	scgi_proc *proc;
	scgi_extension_host *host;

	scgi_connection_state_t state;
	time_t   state_timestamp;

	int      reconnects; /* number of reconnect attempts */

	read_buffer *rb;
	chunkqueue *wb;

	buffer   *response_header;

	int       delayed;   /* flag to mark that the connect() is delayed */

	size_t    request_id;
	int       fd;        /* fd to the scgi process */
	int       fde_ndx;   /* index into the fd-event buffer */

	pid_t     pid;
	int       got_proc;

	plugin_config conf;

	connection *remote_conn;  /* dumb pointer */
	plugin_data *plugin_data; /* dumb pointer */
} handler_ctx;


/* ok, we need a prototype */
static handler_t scgi_handle_fdevent(server *srv, void *ctx, int revents);

int scgi_proclist_sort_down(server *srv, scgi_extension_host *host, scgi_proc *proc);

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

static handler_ctx * handler_ctx_init() {
	handler_ctx * hctx;

	hctx = calloc(1, sizeof(*hctx));
	assert(hctx);

	hctx->fde_ndx = -1;

	hctx->response = buffer_init();
	hctx->response_header = buffer_init();

	hctx->request_id = 0;
	hctx->state = FCGI_STATE_INIT;
	hctx->proc = NULL;

	hctx->response_len = 0;
	hctx->response_type = 0;
	hctx->response_padding = 0;
	hctx->fd = -1;

	hctx->reconnects = 0;

	hctx->wb = chunkqueue_init();

	return hctx;
}

static void handler_ctx_free(handler_ctx *hctx) {
	buffer_free(hctx->response);
	buffer_free(hctx->response_header);

	chunkqueue_free(hctx->wb);

	if (hctx->rb) {
		if (hctx->rb->ptr) free(hctx->rb->ptr);
		free(hctx->rb);
	}

	free(hctx);
}

static scgi_proc *scgi_process_init() {
	scgi_proc *f;

	f = calloc(1, sizeof(*f));
	f->socket = buffer_init();

	f->prev = NULL;
	f->next = NULL;

	return f;
}

static void scgi_process_free(scgi_proc *f) {
	if (!f) return;

	scgi_process_free(f->next);

	buffer_free(f->socket);

	free(f);
}

static scgi_extension_host *scgi_host_init() {
	scgi_extension_host *f;

	f = calloc(1, sizeof(*f));

	f->host = buffer_init();
	f->unixsocket = buffer_init();
	f->docroot = buffer_init();
	f->bin_path = buffer_init();
	f->bin_env = array_init();
	f->bin_env_copy = array_init();

	return f;
}

static void scgi_host_free(scgi_extension_host *h) {
	if (!h) return;

	buffer_free(h->host);
	buffer_free(h->unixsocket);
	buffer_free(h->docroot);
	buffer_free(h->bin_path);
	array_free(h->bin_env);
	array_free(h->bin_env_copy);

	scgi_process_free(h->first);
	scgi_process_free(h->unused_procs);

	free(h);

}

static scgi_exts *scgi_extensions_init() {
	scgi_exts *f;

	f = calloc(1, sizeof(*f));

	return f;
}

static void scgi_extensions_free(scgi_exts *f) {
	size_t i;

	if (!f) return;

	for (i = 0; i < f->used; i++) {
		scgi_extension *fe;
		size_t j;

		fe = f->exts[i];

		for (j = 0; j < fe->used; j++) {
			scgi_extension_host *h;

			h = fe->hosts[j];

			scgi_host_free(h);
		}

		buffer_free(fe->key);
		free(fe->hosts);

		free(fe);
	}

	free(f->exts);

	free(f);
}

static int scgi_extension_insert(scgi_exts *ext, buffer *key, scgi_extension_host *fh) {
	scgi_extension *fe;
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
		assert(fe);
		fe->key = buffer_init();
		buffer_copy_string_buffer(fe->key, key);

		/* */

		if (ext->size == 0) {
			ext->size = 8;
			ext->exts = malloc(ext->size * sizeof(*(ext->exts)));
			assert(ext->exts);
		} else if (ext->used == ext->size) {
			ext->size += 8;
			ext->exts = realloc(ext->exts, ext->size * sizeof(*(ext->exts)));
			assert(ext->exts);
		}
		ext->exts[ext->used++] = fe;
	} else {
		fe = ext->exts[i];
	}

	if (fe->size == 0) {
		fe->size = 4;
		fe->hosts = malloc(fe->size * sizeof(*(fe->hosts)));
		assert(fe->hosts);
	} else if (fe->size == fe->used) {
		fe->size += 4;
		fe->hosts = realloc(fe->hosts, fe->size * sizeof(*(fe->hosts)));
		assert(fe->hosts);
	}

	fe->hosts[fe->used++] = fh;

	return 0;

}

INIT_FUNC(mod_scgi_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->scgi_env = buffer_init();

	p->path = buffer_init();
	p->parse_response = buffer_init();

	return p;
}


FREE_FUNC(mod_scgi_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	buffer_free(p->scgi_env);
	buffer_free(p->path);
	buffer_free(p->parse_response);

	if (p->config_storage) {
		size_t i, j, n;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];
			scgi_exts *exts;

			if (!s) continue;

			exts = s->exts;

			for (j = 0; j < exts->used; j++) {
				scgi_extension *ex;

				ex = exts->exts[j];

				for (n = 0; n < ex->used; n++) {
					scgi_proc *proc;
					scgi_extension_host *host;

					host = ex->hosts[n];

					for (proc = host->first; proc; proc = proc->next) {
						if (proc->pid != 0) kill(proc->pid, SIGTERM);

						if (proc->is_local &&
						    !buffer_is_empty(proc->socket)) {
							unlink(proc->socket->ptr);
						}
					}

					for (proc = host->unused_procs; proc; proc = proc->next) {
						if (proc->pid != 0) kill(proc->pid, SIGTERM);

						if (proc->is_local &&
						    !buffer_is_empty(proc->socket)) {
							unlink(proc->socket->ptr);
						}
					}
				}
			}

			scgi_extensions_free(s->exts);

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
	/* add the \0 from the value */
	memcpy(dst + key_len + 1, val, val_len + 1);

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
	} else if (env->size == env->used) {
		env->size += 16;
		env->ptr = realloc(env->ptr, env->size * sizeof(*env->ptr));
	}

	env->ptr[env->used++] = dst;

	return 0;
}

static int scgi_spawn_connection(server *srv,
				 plugin_data *p,
				 scgi_extension_host *host,
				 scgi_proc *proc) {
	int scgi_fd;
	int socket_type, status;
	struct timeval tv = { 0, 100 * 1000 };
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un scgi_addr_un;
#endif
	struct sockaddr_in scgi_addr_in;
	struct sockaddr *scgi_addr;

	socklen_t servlen;

#ifndef HAVE_FORK
	return -1;
#endif

	if (p->conf.debug) {
		log_error_write(srv, __FILE__, __LINE__, "sdb",
				"new proc, socket:", proc->port, proc->socket);
	}

	if (!buffer_is_empty(proc->socket)) {
		memset(&scgi_addr, 0, sizeof(scgi_addr));

#ifdef HAVE_SYS_UN_H
		scgi_addr_un.sun_family = AF_UNIX;
		strcpy(scgi_addr_un.sun_path, proc->socket->ptr);

#ifdef SUN_LEN
		servlen = SUN_LEN(&scgi_addr_un);
#else
		/* stevens says: */
		servlen = proc->socket->used + sizeof(scgi_addr_un.sun_family);
#endif
		socket_type = AF_UNIX;
		scgi_addr = (struct sockaddr *) &scgi_addr_un;
#else
		log_error_write(srv, __FILE__, __LINE__, "s",
				"ERROR: Unix Domain sockets are not supported.");
		return -1;
#endif
	} else {
		scgi_addr_in.sin_family = AF_INET;

		if (buffer_is_empty(host->host)) {
			scgi_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
		} else {
			struct hostent *he;

			/* set a usefull default */
			scgi_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);


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

			memcpy(&(scgi_addr_in.sin_addr.s_addr), he->h_addr_list[0], he->h_length);

		}
		scgi_addr_in.sin_port = htons(proc->port);
		servlen = sizeof(scgi_addr_in);

		socket_type = AF_INET;
		scgi_addr = (struct sockaddr *) &scgi_addr_in;
	}

	if (-1 == (scgi_fd = socket(socket_type, SOCK_STREAM, 0))) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"failed:", strerror(errno));
		return -1;
	}

	if (-1 == connect(scgi_fd, scgi_addr, servlen)) {
		/* server is not up, spawn in  */
		pid_t child;
		int val;

		if (!buffer_is_empty(proc->socket)) {
			unlink(proc->socket->ptr);
		}

		close(scgi_fd);

		/* reopen socket */
		if (-1 == (scgi_fd = socket(socket_type, SOCK_STREAM, 0))) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"socket failed:", strerror(errno));
			return -1;
		}

		val = 1;
		if (setsockopt(scgi_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"socketsockopt failed:", strerror(errno));
			return -1;
		}

		/* create socket */
		if (-1 == bind(scgi_fd, scgi_addr, servlen)) {
			log_error_write(srv, __FILE__, __LINE__, "sbds",
				"bind failed for:",
				proc->socket,
				proc->port,
				strerror(errno));
			return -1;
		}

		if (-1 == listen(scgi_fd, 1024)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
				"listen failed:", strerror(errno));
			return -1;
		}

#ifdef HAVE_FORK
		switch ((child = fork())) {
		case 0: {
			buffer *b;
			size_t i = 0;
			int fd = 0;
			char_array env;


			/* create environment */
			env.ptr = NULL;
			env.size = 0;
			env.used = 0;

			if (scgi_fd != 0) {
				dup2(scgi_fd, 0);
				close(scgi_fd);
			}

			/* we don't need the client socket */
			for (fd = 3; fd < 256; fd++) {
				close(fd);
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

			b = buffer_init();
			buffer_copy_string_len(b, CONST_STR_LEN("exec "));
			buffer_append_string_buffer(b, host->bin_path);

			reset_signals();

			/* exec the cgi */
			execle("/bin/sh", "sh", "-c", b->ptr, (char *)NULL, env.ptr);

			log_error_write(srv, __FILE__, __LINE__, "sbs",
					"execl failed for:", host->bin_path, strerror(errno));

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
				/* the child should not terminate at all */
				if (WIFEXITED(status)) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"child exited (is this a SCGI binary ?):",
							WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"child signaled:",
							WTERMSIG(status));
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"child died somehow:",
							status);
				}
				return -1;
			}

			/* register process */
			proc->pid = child;
			proc->last_used = srv->cur_ts;
			proc->is_local = 1;

			break;
		}
#endif
	} else {
		proc->is_local = 0;
		proc->pid = 0;

		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					"(debug) socket is already used, won't spawn:",
					proc->socket);
		}
	}

	proc->state = PROC_STATE_RUNNING;
	host->active_procs++;

	close(scgi_fd);

	return 0;
}


SETDEFAULTS_FUNC(mod_scgi_set_defaults) {
	plugin_data *p = p_d;
	data_unset *du;
	size_t i = 0;

	config_values_t cv[] = {
		{ "scgi.server",              NULL, T_CONFIG_LOCAL, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "scgi.debug",               NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;
		array *ca;

		s = malloc(sizeof(plugin_config));
		s->exts          = scgi_extensions_init();
		s->debug         = 0;

		cv[0].destination = s->exts;
		cv[1].destination = &(s->debug);

		p->config_storage[i] = s;
		ca = ((data_config *)srv->config_context->data[i])->value;

		if (0 != config_insert_values_global(srv, ca, cv)) {
			return HANDLER_ERROR;
		}

		/*
		 * <key> = ( ... )
		 */

		if (NULL != (du = array_get_element(ca, "scgi.server"))) {
			size_t j;
			data_array *da = (data_array *)du;

			if (du->type != TYPE_ARRAY) {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"unexpected type for key: ", "scgi.server", "array of strings");

				return HANDLER_ERROR;
			}


			/*
			 * scgi.server = ( "<ext>" => ( ... ),
			 *                    "<ext>" => ( ... ) )
			 */

			for (j = 0; j < da->value->used; j++) {
				size_t n;
				data_array *da_ext = (data_array *)da->value->data[j];

				if (da->value->data[j]->type != TYPE_ARRAY) {
					log_error_write(srv, __FILE__, __LINE__, "sssbs",
							"unexpected type for key: ", "scgi.server",
							"[", da->value->data[j]->key, "](string)");

					return HANDLER_ERROR;
				}

				/*
				 * da_ext->key == name of the extension
				 */

				/*
				 * scgi.server = ( "<ext>" =>
				 *                     ( "<host>" => ( ... ),
				 *                       "<host>" => ( ... )
				 *                     ),
				 *                    "<ext>" => ... )
				 */

				for (n = 0; n < da_ext->value->used; n++) {
					data_array *da_host = (data_array *)da_ext->value->data[n];

					scgi_extension_host *df;

					config_values_t fcv[] = {
						{ "host",              NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
						{ "docroot",           NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 1 */
						{ "socket",            NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 2 */
						{ "bin-path",          NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },       /* 3 */

						{ "check-local",       NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },      /* 4 */
						{ "port",              NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 5 */
						{ "min-procs-not-working",         NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 7 this is broken for now */
						{ "max-procs",         NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 7 */
						{ "max-load-per-proc", NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 8 */
						{ "idle-timeout",      NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 9 */
						{ "disable-time",      NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },        /* 10 */

						{ "bin-environment",   NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },        /* 11 */
						{ "bin-copy-environment", NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },     /* 12 */
						{ "fix-root-scriptname",  NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },   /* 13 */


						{ NULL,                NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
					};

					if (da_host->type != TYPE_ARRAY) {
						log_error_write(srv, __FILE__, __LINE__, "ssSBS",
								"unexpected type for key:",
								"scgi.server",
								"[", da_host->key, "](string)");

						return HANDLER_ERROR;
					}

					df = scgi_host_init();

					df->check_local  = 1;
					df->min_procs    = 4;
					df->max_procs    = 4;
					df->max_load_per_proc = 1;
					df->idle_timeout = 60;
					df->disable_time = 60;
					df->fix_root_path_name = 0;

					fcv[0].destination = df->host;
					fcv[1].destination = df->docroot;
					fcv[2].destination = df->unixsocket;
					fcv[3].destination = df->bin_path;

					fcv[4].destination = &(df->check_local);
					fcv[5].destination = &(df->port);
					fcv[6].destination = &(df->min_procs);
					fcv[7].destination = &(df->max_procs);
					fcv[8].destination = &(df->max_load_per_proc);
					fcv[9].destination = &(df->idle_timeout);
					fcv[10].destination = &(df->disable_time);

					fcv[11].destination = df->bin_env;
					fcv[12].destination = df->bin_env_copy;
					fcv[13].destination = &(df->fix_root_path_name);


					if (0 != config_insert_values_internal(srv, da_host->value, fcv)) {
						return HANDLER_ERROR;
					}

					if ((!buffer_is_empty(df->host) || df->port) &&
					    !buffer_is_empty(df->unixsocket)) {
						log_error_write(srv, __FILE__, __LINE__, "s",
								"either host+port or socket");

						return HANDLER_ERROR;
					}

					if (!buffer_is_empty(df->unixsocket)) {
						/* unix domain socket */
						struct sockaddr_un un;

						if (df->unixsocket->used > sizeof(un.sun_path) - 2) {
							log_error_write(srv, __FILE__, __LINE__, "s",
									"path of the unixdomain socket is too large");
							return HANDLER_ERROR;
						}
					} else {
						/* tcp/ip */

						if (buffer_is_empty(df->host) &&
						    buffer_is_empty(df->bin_path)) {
							log_error_write(srv, __FILE__, __LINE__, "sbbbs",
									"missing key (string):",
									da->key,
									da_ext->key,
									da_host->key,
									"host");

							return HANDLER_ERROR;
						} else if (df->port == 0) {
							log_error_write(srv, __FILE__, __LINE__, "sbbbs",
									"missing key (short):",
									da->key,
									da_ext->key,
									da_host->key,
									"port");
							return HANDLER_ERROR;
						}
					}

					if (!buffer_is_empty(df->bin_path)) {
						/* a local socket + self spawning */
						size_t pno;

						/* HACK:  just to make sure the adaptive spawing is disabled */
						df->min_procs = df->max_procs;

						if (df->min_procs > df->max_procs) df->max_procs = df->min_procs;
						if (df->max_load_per_proc < 1) df->max_load_per_proc = 0;

						if (s->debug) {
							log_error_write(srv, __FILE__, __LINE__, "ssbsdsbsdsd",
									"--- scgi spawning local",
									"\n\tproc:", df->bin_path,
									"\n\tport:", df->port,
									"\n\tsocket", df->unixsocket,
									"\n\tmin-procs:", df->min_procs,
									"\n\tmax-procs:", df->max_procs);
						}

						for (pno = 0; pno < df->min_procs; pno++) {
							scgi_proc *proc;

							proc = scgi_process_init();
							proc->id = df->num_procs++;
							df->max_id++;

							if (buffer_is_empty(df->unixsocket)) {
								proc->port = df->port + pno;
							} else {
								buffer_copy_string_buffer(proc->socket, df->unixsocket);
								buffer_append_string_len(proc->socket, CONST_STR_LEN("-"));
								buffer_append_long(proc->socket, pno);
							}

							if (s->debug) {
								log_error_write(srv, __FILE__, __LINE__, "ssdsbsdsd",
										"--- scgi spawning",
										"\n\tport:", df->port,
										"\n\tsocket", df->unixsocket,
										"\n\tcurrent:", pno, "/", df->min_procs);
							}

							if (scgi_spawn_connection(srv, p, df, proc)) {
								log_error_write(srv, __FILE__, __LINE__, "s",
										"[ERROR]: spawning fcgi failed.");
								return HANDLER_ERROR;
							}

							proc->next = df->first;
							if (df->first) 	df->first->prev = proc;

							df->first = proc;
						}
					} else {
						scgi_proc *fp;

						fp = scgi_process_init();
						fp->id = df->num_procs++;
						df->max_id++;
						df->active_procs++;
						fp->state = PROC_STATE_RUNNING;

						if (buffer_is_empty(df->unixsocket)) {
							fp->port = df->port;
						} else {
							buffer_copy_string_buffer(fp->socket, df->unixsocket);
						}

						df->first = fp;

						df->min_procs = 1;
						df->max_procs = 1;
					}

					/* if extension already exists, take it */
					scgi_extension_insert(s->exts, da_ext->key, df);
				}
			}
		}
	}

	return HANDLER_GO_ON;
}

static int scgi_set_state(server *srv, handler_ctx *hctx, scgi_connection_state_t state) {
	hctx->state = state;
	hctx->state_timestamp = srv->cur_ts;

	return 0;
}


static void scgi_connection_cleanup(server *srv, handler_ctx *hctx) {
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
		hctx->host->load--;

		if (hctx->got_proc) {
			/* after the connect the process gets a load */
			hctx->proc->load--;

			if (p->conf.debug) {
				log_error_write(srv, __FILE__, __LINE__, "sddb",
						"release proc:",
						hctx->fd,
						hctx->proc->pid, hctx->proc->socket);
			}
		}

		scgi_proclist_sort_down(srv, hctx->host, hctx->proc);
	}


	handler_ctx_free(hctx);
	con->plugin_ctx[p->id] = NULL;
}

static int scgi_reconnect(server *srv, handler_ctx *hctx) {
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
	 * if we have more then 5 reconnects for the same request, die
	 *
	 * 2.
	 *
	 * we have a connection but the child died by some other reason
	 *
	 */

	fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
	fdevent_unregister(srv->ev, hctx->fd);
	close(hctx->fd);
	srv->cur_fds--;

	scgi_set_state(srv, hctx, FCGI_STATE_INIT);

	hctx->request_id = 0;
	hctx->reconnects++;

	if (p->conf.debug) {
		log_error_write(srv, __FILE__, __LINE__, "sddb",
				"release proc:",
				hctx->fd,
				hctx->proc->pid, hctx->proc->socket);
	}

	hctx->proc->load--;
	scgi_proclist_sort_down(srv, hctx->host, hctx->proc);

	return 0;
}


static handler_t scgi_connection_reset(server *srv, connection *con, void *p_d) {
	plugin_data *p = p_d;

	scgi_connection_cleanup(srv, con->plugin_ctx[p->id]);

	return HANDLER_GO_ON;
}


static int scgi_env_add(buffer *env, const char *key, size_t key_len, const char *val, size_t val_len) {
	size_t len;

	if (!key || !val) return -1;

	len = key_len + val_len + 2;

	buffer_prepare_append(env, len);

	memcpy(env->ptr + env->used, key, key_len);
	env->ptr[env->used + key_len] = '\0';
	env->used += key_len + 1;
	memcpy(env->ptr + env->used, val, val_len);
	env->ptr[env->used + val_len] = '\0';
	env->used += val_len + 1;

	return 0;
}


/**
 *
 * returns
 *   -1 error
 *    0 connected
 *    1 not connected yet
 */

static int scgi_establish_connection(server *srv, handler_ctx *hctx) {
	struct sockaddr *scgi_addr;
	struct sockaddr_in scgi_addr_in;
#ifdef HAVE_SYS_UN_H
	struct sockaddr_un scgi_addr_un;
#endif
	socklen_t servlen;

	scgi_extension_host *host = hctx->host;
	scgi_proc *proc   = hctx->proc;
	int scgi_fd       = hctx->fd;

	memset(&scgi_addr, 0, sizeof(scgi_addr));

	if (!buffer_is_empty(proc->socket)) {
#ifdef HAVE_SYS_UN_H
		/* use the unix domain socket */
		scgi_addr_un.sun_family = AF_UNIX;
		strcpy(scgi_addr_un.sun_path, proc->socket->ptr);
#ifdef SUN_LEN
		servlen = SUN_LEN(&scgi_addr_un);
#else
		/* stevens says: */
		servlen = proc->socket->used + sizeof(scgi_addr_un.sun_family);
#endif
		scgi_addr = (struct sockaddr *) &scgi_addr_un;
#else
		return -1;
#endif
	} else {
		scgi_addr_in.sin_family = AF_INET;
		if (0 == inet_aton(host->host->ptr, &(scgi_addr_in.sin_addr))) {
			log_error_write(srv, __FILE__, __LINE__, "sbs",
					"converting IP-adress failed for", host->host,
					"\nBe sure to specify an IP address here");

			return -1;
		}
		scgi_addr_in.sin_port = htons(proc->port);
		servlen = sizeof(scgi_addr_in);

		scgi_addr = (struct sockaddr *) &scgi_addr_in;
	}

	if (-1 == connect(scgi_fd, scgi_addr, servlen)) {
		if (errno == EINPROGRESS ||
		    errno == EALREADY ||
		    errno == EINTR) {
			if (hctx->conf.debug) {
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"connect delayed, will continue later:", scgi_fd);
			}

			return 1;
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sdsddb",
					"connect failed:", scgi_fd,
					strerror(errno), errno,
					proc->port, proc->socket);

			if (errno == EAGAIN) {
				/* this is Linux only */

				log_error_write(srv, __FILE__, __LINE__, "s",
						"If this happend on Linux: You have been run out of local ports. "
						"Check the manual, section Performance how to handle this.");
			}

			return -1;
		}
	}
	if (hctx->conf.debug > 1) {
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"connect succeeded: ", scgi_fd);
	}



	return 0;
}

static int scgi_env_add_request_headers(server *srv, connection *con, plugin_data *p) {
	size_t i;

	for (i = 0; i < con->request.headers->used; i++) {
		data_string *ds;

		ds = (data_string *)con->request.headers->data[i];

		if (ds->value->used && ds->key->used) {
			size_t j;
			buffer_reset(srv->tmp_buf);

			if (0 != strcasecmp(ds->key->ptr, "CONTENT-TYPE")) {
				buffer_copy_string_len(srv->tmp_buf, CONST_STR_LEN("HTTP_"));
				srv->tmp_buf->used--;
			}

			buffer_prepare_append(srv->tmp_buf, ds->key->used + 2);
			for (j = 0; j < ds->key->used - 1; j++) {
				srv->tmp_buf->ptr[srv->tmp_buf->used++] =
					light_isalpha(ds->key->ptr[j]) ?
					ds->key->ptr[j] & ~32 : '_';
			}
			srv->tmp_buf->ptr[srv->tmp_buf->used++] = '\0';

			scgi_env_add(p->scgi_env, CONST_BUF_LEN(srv->tmp_buf), CONST_BUF_LEN(ds->value));
		}
	}

	for (i = 0; i < con->environment->used; i++) {
		data_string *ds;

		ds = (data_string *)con->environment->data[i];

		if (ds->value->used && ds->key->used) {
			size_t j;
			buffer_reset(srv->tmp_buf);

			buffer_prepare_append(srv->tmp_buf, ds->key->used + 2);
			for (j = 0; j < ds->key->used - 1; j++) {
				srv->tmp_buf->ptr[srv->tmp_buf->used++] =
					light_isalnum((unsigned char)ds->key->ptr[j]) ?
					toupper((unsigned char)ds->key->ptr[j]) : '_';
			}
			srv->tmp_buf->ptr[srv->tmp_buf->used++] = '\0';

			scgi_env_add(p->scgi_env, CONST_BUF_LEN(srv->tmp_buf), CONST_BUF_LEN(ds->value));
		}
	}

	return 0;
}


static int scgi_create_env(server *srv, handler_ctx *hctx) {
	char buf[32];
	const char *s;
#ifdef HAVE_IPV6
	char b2[INET6_ADDRSTRLEN + 1];
#endif
	buffer *b;

	plugin_data *p    = hctx->plugin_data;
	scgi_extension_host *host= hctx->host;

	connection *con   = hctx->remote_conn;
	server_socket *srv_sock = con->srv_socket;

	sock_addr our_addr;
	socklen_t our_addr_len;

	buffer_prepare_copy(p->scgi_env, 1024);

	/* CGI-SPEC 6.1.2, FastCGI spec 6.3 and SCGI spec */

	/* request.content_length < SSIZE_MAX, see request.c */
	LI_ltostr(buf, con->request.content_length);
	scgi_env_add(p->scgi_env, CONST_STR_LEN("CONTENT_LENGTH"), buf, strlen(buf));
	scgi_env_add(p->scgi_env, CONST_STR_LEN("SCGI"), CONST_STR_LEN("1"));


	if (buffer_is_empty(con->conf.server_tag)) {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_SOFTWARE"), CONST_STR_LEN(PACKAGE_DESC));
	} else {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_SOFTWARE"), CONST_BUF_LEN(con->conf.server_tag));
	}

	if (con->server_name->used) {
		size_t len = con->server_name->used - 1;
		char *colon = strchr(con->server_name->ptr, ':');
		if (colon) len = colon - con->server_name->ptr;

		scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_NAME"), con->server_name->ptr, len);
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
		scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_NAME"), s, strlen(s));
	}

	scgi_env_add(p->scgi_env, CONST_STR_LEN("GATEWAY_INTERFACE"), CONST_STR_LEN("CGI/1.1"));

	LI_ltostr(buf,
#ifdef HAVE_IPV6
	       ntohs(srv_sock->addr.plain.sa_family ? srv_sock->addr.ipv6.sin6_port : srv_sock->addr.ipv4.sin_port)
#else
	       ntohs(srv_sock->addr.ipv4.sin_port)
#endif
	       );

	scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_PORT"), buf, strlen(buf));

	/* get the server-side of the connection to the client */
	our_addr_len = sizeof(our_addr);

	if (-1 == getsockname(con->fd, &(our_addr.plain), &our_addr_len)) {
		s = inet_ntop_cache_get_ip(srv, &(srv_sock->addr));
	} else {
		s = inet_ntop_cache_get_ip(srv, &(our_addr));
	}
	scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_ADDR"), s, strlen(s));

	LI_ltostr(buf,
#ifdef HAVE_IPV6
	       ntohs(con->dst_addr.plain.sa_family ? con->dst_addr.ipv6.sin6_port : con->dst_addr.ipv4.sin_port)
#else
	       ntohs(con->dst_addr.ipv4.sin_port)
#endif
	       );

	scgi_env_add(p->scgi_env, CONST_STR_LEN("REMOTE_PORT"), buf, strlen(buf));

	s = inet_ntop_cache_get_ip(srv, &(con->dst_addr));
	scgi_env_add(p->scgi_env, CONST_STR_LEN("REMOTE_ADDR"), s, strlen(s));

	if (!buffer_is_empty(con->authed_user)) {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("REMOTE_USER"),
			     CONST_BUF_LEN(con->authed_user));
	}


	/*
	 * SCRIPT_NAME, PATH_INFO and PATH_TRANSLATED according to
	 * http://cgi-spec.golux.com/draft-coar-cgi-v11-03-clean.html
	 * (6.1.14, 6.1.6, 6.1.7)
	 */

	scgi_env_add(p->scgi_env, CONST_STR_LEN("SCRIPT_NAME"), CONST_BUF_LEN(con->uri.path));

	if (!buffer_is_empty(con->request.pathinfo)) {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("PATH_INFO"), CONST_BUF_LEN(con->request.pathinfo));

		/* PATH_TRANSLATED is only defined if PATH_INFO is set */

		if (!buffer_is_empty(host->docroot)) {
			buffer_copy_string_buffer(p->path, host->docroot);
		} else {
			buffer_copy_string_buffer(p->path, con->physical.basedir);
		}
		buffer_append_string_buffer(p->path, con->request.pathinfo);
		scgi_env_add(p->scgi_env, CONST_STR_LEN("PATH_TRANSLATED"), CONST_BUF_LEN(p->path));
	} else {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("PATH_INFO"), CONST_STR_LEN(""));
	}

	/*
	 * SCRIPT_FILENAME and DOCUMENT_ROOT for php. The PHP manual
	 * http://www.php.net/manual/en/reserved.variables.php
	 * treatment of PATH_TRANSLATED is different from the one of CGI specs.
	 * TODO: this code should be checked against cgi.fix_pathinfo php
	 * parameter.
	 */

	if (!buffer_is_empty(host->docroot)) {
		/*
		 * rewrite SCRIPT_FILENAME
		 *
		 */

		buffer_copy_string_buffer(p->path, host->docroot);
		buffer_append_string_buffer(p->path, con->uri.path);

		scgi_env_add(p->scgi_env, CONST_STR_LEN("SCRIPT_FILENAME"), CONST_BUF_LEN(p->path));
		scgi_env_add(p->scgi_env, CONST_STR_LEN("DOCUMENT_ROOT"), CONST_BUF_LEN(host->docroot));
	} else {
		buffer_copy_string_buffer(p->path, con->physical.path);

		scgi_env_add(p->scgi_env, CONST_STR_LEN("SCRIPT_FILENAME"), CONST_BUF_LEN(p->path));
		scgi_env_add(p->scgi_env, CONST_STR_LEN("DOCUMENT_ROOT"), CONST_BUF_LEN(con->physical.basedir));
	}
	scgi_env_add(p->scgi_env, CONST_STR_LEN("REQUEST_URI"), CONST_BUF_LEN(con->request.orig_uri));
	if (!buffer_is_equal(con->request.uri, con->request.orig_uri)) {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("REDIRECT_URI"), CONST_BUF_LEN(con->request.uri));
	}
	if (!buffer_is_empty(con->uri.query)) {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("QUERY_STRING"), CONST_BUF_LEN(con->uri.query));
	} else {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("QUERY_STRING"), CONST_STR_LEN(""));
	}

	s = get_http_method_name(con->request.http_method);
	scgi_env_add(p->scgi_env, CONST_STR_LEN("REQUEST_METHOD"), s, strlen(s));
	scgi_env_add(p->scgi_env, CONST_STR_LEN("REDIRECT_STATUS"), CONST_STR_LEN("200")); /* if php is compiled with --force-redirect */
	s = get_http_version_name(con->request.http_version);
	scgi_env_add(p->scgi_env, CONST_STR_LEN("SERVER_PROTOCOL"), s, strlen(s));

#ifdef USE_OPENSSL
	if (srv_sock->is_ssl) {
		scgi_env_add(p->scgi_env, CONST_STR_LEN("HTTPS"), CONST_STR_LEN("on"));
	}
#endif

	scgi_env_add_request_headers(srv, con, p);

	b = chunkqueue_get_append_buffer(hctx->wb);

	buffer_append_long(b, p->scgi_env->used);
	buffer_append_string_len(b, CONST_STR_LEN(":"));
	buffer_append_string_len(b, (const char *)p->scgi_env->ptr, p->scgi_env->used);
	buffer_append_string_len(b, CONST_STR_LEN(","));

	hctx->wb->bytes_in += b->used - 1;

	if (con->request.content_length) {
		chunkqueue *req_cq = con->request_content_queue;
		chunk *req_c;
		off_t offset;

		/* something to send ? */
		for (offset = 0, req_c = req_cq->first; offset != req_cq->bytes_in; req_c = req_c->next) {
			off_t weWant = req_cq->bytes_in - offset;
			off_t weHave = 0;

			/* we announce toWrite octects
			 * now take all the request_content chunk that we need to fill this request
			 * */

			switch (req_c->type) {
			case FILE_CHUNK:
				weHave = req_c->file.length - req_c->offset;

				if (weHave > weWant) weHave = weWant;

				chunkqueue_append_file(hctx->wb, req_c->file.name, req_c->offset, weHave);

				req_c->offset += weHave;
				req_cq->bytes_out += weHave;

				hctx->wb->bytes_in += weHave;

				break;
			case MEM_CHUNK:
				/* append to the buffer */
				weHave = req_c->mem->used - 1 - req_c->offset;

				if (weHave > weWant) weHave = weWant;

				b = chunkqueue_get_append_buffer(hctx->wb);
				buffer_append_memory(b, req_c->mem->ptr + req_c->offset, weHave);
				b->used++; /* add virtual \0 */

				req_c->offset += weHave;
				req_cq->bytes_out += weHave;

				hctx->wb->bytes_in += weHave;

				break;
			default:
				break;
			}

			offset += weHave;
		}
	}

	return 0;
}

static int scgi_response_parse(server *srv, connection *con, plugin_data *p, buffer *in, int eol) {
	char *ns;
	const char *s;
	int line = 0;

	UNUSED(srv);

	buffer_copy_string_buffer(p->parse_response, in);

	for (s = p->parse_response->ptr;
	     NULL != (ns = (eol == EOL_RN ? strstr(s, "\r\n") : strchr(s, '\n')));
	     s = ns + (eol == EOL_RN ? 2 : 1), line++) {
		const char *key, *value;
		int key_len;
		data_string *ds;

		ns[0] = '\0';

		if (line == 0 &&
		    0 == strncmp(s, "HTTP/1.", 7)) {
			/* non-parsed header ... we parse them anyway */

			if ((s[7] == '1' ||
			     s[7] == '0') &&
			    s[8] == ' ') {
				int status;
				/* after the space should be a status code for us */

				status = strtol(s+9, NULL, 10);

				if (status >= 100 && status < 1000) {
					/* we expected 3 digits got them */
					con->parsed_response |= HTTP_STATUS;
					con->http_status = status;
				}
			}
		} else {

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


static int scgi_demux_response(server *srv, handler_ctx *hctx) {
	plugin_data *p    = hctx->plugin_data;
	connection  *con  = hctx->remote_conn;

	while(1) {
		int n;

		buffer_prepare_copy(hctx->response, 1024);
		if (-1 == (n = read(hctx->fd, hctx->response->ptr, hctx->response->size - 1))) {
			if (errno == EAGAIN || errno == EINTR) {
				/* would block, wait for signal */
				return 0;
			}
			/* error */
			log_error_write(srv, __FILE__, __LINE__, "sdd", strerror(errno), con->fd, hctx->fd);
			return -1;
		}

		if (n == 0) {
			/* read finished */

			con->file_finished = 1;

			/* send final chunk */
			http_chunk_append_mem(srv, con, NULL, 0);
			joblist_append(srv, con);

			return 1;
		}

		hctx->response->ptr[n] = '\0';
		hctx->response->used = n+1;

		/* split header from body */

		if (con->file_started == 0) {
			char *c;
			int in_header = 0;
			int header_end = 0;
			int cp, eol = EOL_UNSET;
			size_t used = 0;
			size_t hlen = 0;

			buffer_append_string_buffer(hctx->response_header, hctx->response);

			/* nph (non-parsed headers) */
			if (0 == strncmp(hctx->response_header->ptr, "HTTP/1.", 7)) in_header = 1;

			/* search for the \r\n\r\n or \n\n in the string */
			for (c = hctx->response_header->ptr, cp = 0, used = hctx->response_header->used - 1; used; c++, cp++, used--) {
				if (*c == ':') in_header = 1;
				else if (*c == '\n') {
					if (in_header == 0) {
						/* got a response without a response header */

						c = NULL;
						header_end = 1;
						break;
					}

					if (eol == EOL_UNSET) eol = EOL_N;

					if (*(c+1) == '\n') {
						header_end = 1;
						hlen = cp + 2;
						break;
					}

				} else if (used > 1 && *c == '\r' && *(c+1) == '\n') {
					if (in_header == 0) {
						/* got a response without a response header */

						c = NULL;
						header_end = 1;
						break;
					}

					if (eol == EOL_UNSET) eol = EOL_RN;

					if (used > 3 &&
					    *(c+2) == '\r' &&
					    *(c+3) == '\n') {
						header_end = 1;
						hlen = cp + 4;
						break;
					}

					/* skip the \n */
					c++;
					cp++;
					used--;
				}
			}

			if (header_end) {
				if (c == NULL) {
					/* no header, but a body */

					if (con->request.http_version == HTTP_VERSION_1_1) {
						con->response.transfer_encoding = HTTP_TRANSFER_ENCODING_CHUNKED;
					}

					http_chunk_append_mem(srv, con, hctx->response_header->ptr, hctx->response_header->used);
					joblist_append(srv, con);
				} else {
					size_t blen = hctx->response_header->used - hlen - 1;

					/* a small hack: terminate after at the second \r */
					hctx->response_header->used = hlen;
					hctx->response_header->ptr[hlen - 1] = '\0';

					/* parse the response header */
					scgi_response_parse(srv, con, p, hctx->response_header, eol);

					/* enable chunked-transfer-encoding */
					if (con->request.http_version == HTTP_VERSION_1_1 &&
					    !(con->parsed_response & HTTP_CONTENT_LENGTH)) {
						con->response.transfer_encoding = HTTP_TRANSFER_ENCODING_CHUNKED;
					}

					if ((hctx->response->used != hlen) && blen > 0) {
						http_chunk_append_mem(srv, con, hctx->response_header->ptr + hlen, blen + 1);
						joblist_append(srv, con);
					}
				}

				con->file_started = 1;
			}
		} else {
			http_chunk_append_mem(srv, con, hctx->response->ptr, hctx->response->used);
			joblist_append(srv, con);
		}

#if 0
		log_error_write(srv, __FILE__, __LINE__, "ddss", con->fd, hctx->fd, connection_get_state(con->state), b->ptr);
#endif
	}

	return 0;
}


static int scgi_proclist_sort_up(server *srv, scgi_extension_host *host, scgi_proc *proc) {
	scgi_proc *p;

	UNUSED(srv);

	/* we have been the smallest of the current list
	 * and we want to insert the node sorted as soon
	 * possible
	 *
	 * 1 0 0 0 1 1 1
	 * |      ^
	 * |      |
	 * +------+
	 *
	 */

	/* nothing to sort, only one element */
	if (host->first == proc && proc->next == NULL) return 0;

	for (p = proc; p->next && p->next->load < proc->load; p = p->next);

	/* no need to move something
	 *
	 * 1 2 2 2 3 3 3
	 * ^
	 * |
	 * +
	 *
	 */
	if (p == proc) return 0;

	if (host->first == proc) {
		/* we have been the first elememt */

		host->first = proc->next;
		host->first->prev = NULL;
	}

	/* disconnect proc */

	if (proc->prev) proc->prev->next = proc->next;
	if (proc->next) proc->next->prev = proc->prev;

	/* proc should be right of p */

	proc->next = p->next;
	proc->prev = p;
	if (p->next) p->next->prev = proc;
	p->next = proc;
#if 0
	for(p = host->first; p; p = p->next) {
		log_error_write(srv, __FILE__, __LINE__, "dd",
				p->pid, p->load);
	}
#else
	UNUSED(srv);
#endif

	return 0;
}

int scgi_proclist_sort_down(server *srv, scgi_extension_host *host, scgi_proc *proc) {
	scgi_proc *p;

	UNUSED(srv);

	/* we have been the smallest of the current list
	 * and we want to insert the node sorted as soon
	 * possible
	 *
	 *  0 0 0 0 1 0 1
	 * ^          |
	 * |          |
	 * +----------+
	 *
	 *
	 * the basic is idea is:
	 * - the last active scgi process should be still
	 *   in ram and is not swapped out yet
	 * - processes that are not reused will be killed
	 *   after some time by the trigger-handler
	 * - remember it as:
	 *   everything > 0 is hot
	 *   all unused procs are colder the more right they are
	 *   ice-cold processes are propably unused since more
	 *   than 'unused-timeout', are swaped out and won't be
	 *   reused in the next seconds anyway.
	 *
	 */

	/* nothing to sort, only one element */
	if (host->first == proc && proc->next == NULL) return 0;

	for (p = host->first; p != proc && p->load < proc->load; p = p->next);


	/* no need to move something
	 *
	 * 1 2 2 2 3 3 3
	 * ^
	 * |
	 * +
	 *
	 */
	if (p == proc) return 0;

	/* we have to move left. If we are already the first element
	 * we are done */
	if (host->first == proc) return 0;

	/* release proc */
	if (proc->prev) proc->prev->next = proc->next;
	if (proc->next) proc->next->prev = proc->prev;

	/* proc should be left of p */
	proc->next = p;
	proc->prev = p->prev;
	if (p->prev) p->prev->next = proc;
	p->prev = proc;

	if (proc->prev == NULL) host->first = proc;
#if 0
	for(p = host->first; p; p = p->next) {
		log_error_write(srv, __FILE__, __LINE__, "dd",
				p->pid, p->load);
	}
#else
	UNUSED(srv);
#endif

	return 0;
}

static int scgi_restart_dead_procs(server *srv, plugin_data *p, scgi_extension_host *host) {
	scgi_proc *proc;

	for (proc = host->first; proc; proc = proc->next) {
		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__,  "sbdbdddd",
					"proc:",
					host->host, proc->port,
					proc->socket,
					proc->state,
					proc->is_local,
					proc->load,
					proc->pid);
		}

		if (0 == proc->is_local) {
			/*
			 * external servers might get disabled
			 *
			 * enable the server again, perhaps it is back again
			 */

			if ((proc->state == PROC_STATE_DISABLED) &&
			    (srv->cur_ts - proc->disable_ts > host->disable_time)) {
				proc->state = PROC_STATE_RUNNING;
				host->active_procs++;

				log_error_write(srv, __FILE__, __LINE__,  "sbdb",
						"fcgi-server re-enabled:",
						host->host, host->port,
						host->unixsocket);
			}
		} else {
			/* the child should not terminate at all */
			int status;

			if (proc->state == PROC_STATE_DIED_WAIT_FOR_PID) {
				switch(waitpid(proc->pid, &status, WNOHANG)) {
				case 0:
					/* child is still alive */
					break;
				case -1:
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
			}

			/*
			 * local servers might died, but we restart them
			 *
			 */
			if (proc->state == PROC_STATE_DIED &&
			    proc->load == 0) {
				/* restart the child */

				if (p->conf.debug) {
					log_error_write(srv, __FILE__, __LINE__, "ssdsbsdsd",
							"--- scgi spawning",
							"\n\tport:", host->port,
							"\n\tsocket", host->unixsocket,
							"\n\tcurrent:", 1, "/", host->min_procs);
				}

				if (scgi_spawn_connection(srv, p, host, proc)) {
					log_error_write(srv, __FILE__, __LINE__, "s",
							"ERROR: spawning fcgi failed.");
					return HANDLER_ERROR;
				}

				scgi_proclist_sort_down(srv, host, proc);
			}
		}
	}

	return 0;
}


static handler_t scgi_write_request(server *srv, handler_ctx *hctx) {
	plugin_data *p    = hctx->plugin_data;
	scgi_extension_host *host= hctx->host;
	connection *con   = hctx->remote_conn;

	int ret;

	/* sanity check */
	if (!host) {
		log_error_write(srv, __FILE__, __LINE__, "s", "fatal error: host = NULL");
		return HANDLER_ERROR;
	}
	if (((!host->host->used || !host->port) && !host->unixsocket->used)) {
		log_error_write(srv, __FILE__, __LINE__, "sxddd",
				"write-req: error",
				host,
				host->host->used,
				host->port,
				host->unixsocket->used);
		return HANDLER_ERROR;
	}


	switch(hctx->state) {
	case FCGI_STATE_INIT:
		ret = host->unixsocket->used ? AF_UNIX : AF_INET;

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

		fdevent_register(srv->ev, hctx->fd, scgi_handle_fdevent, hctx);

		if (-1 == fdevent_fcntl_set(srv->ev, hctx->fd)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"fcntl failed: ", strerror(errno));

			return HANDLER_ERROR;
		}

		/* fall through */
	case FCGI_STATE_CONNECT:
		if (hctx->state == FCGI_STATE_INIT) {
			for (hctx->proc = hctx->host->first;
			     hctx->proc && hctx->proc->state != PROC_STATE_RUNNING;
			     hctx->proc = hctx->proc->next);

			/* all childs are dead */
			if (hctx->proc == NULL) {
				hctx->fde_ndx = -1;

				return HANDLER_ERROR;
			}

			if (hctx->proc->is_local) {
				hctx->pid = hctx->proc->pid;
			}

			switch (scgi_establish_connection(srv, hctx)) {
			case 1:
				scgi_set_state(srv, hctx, FCGI_STATE_CONNECT);

				/* connection is in progress, wait for an event and call getsockopt() below */

				fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_OUT);

				return HANDLER_WAIT_FOR_EVENT;
			case -1:
				/* if ECONNREFUSED choose another connection -> FIXME */
				hctx->fde_ndx = -1;

				return HANDLER_ERROR;
			default:
				/* everything is ok, go on */
				break;
			}


		} else {
			int socket_error;
			socklen_t socket_error_len = sizeof(socket_error);

			/* try to finish the connect() */
			if (0 != getsockopt(hctx->fd, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len)) {
				log_error_write(srv, __FILE__, __LINE__, "ss",
						"getsockopt failed:", strerror(errno));

				return HANDLER_ERROR;
			}
			if (socket_error != 0) {
				if (!hctx->proc->is_local || p->conf.debug) {
					/* local procs get restarted */

					log_error_write(srv, __FILE__, __LINE__, "ss",
							"establishing connection failed:", strerror(socket_error),
							"port:", hctx->proc->port);
				}

				return HANDLER_ERROR;
			}
		}

		/* ok, we have the connection */

		hctx->proc->load++;
		hctx->proc->last_used = srv->cur_ts;
		hctx->got_proc = 1;

		if (p->conf.debug) {
			log_error_write(srv, __FILE__, __LINE__, "sddbdd",
					"got proc:",
					hctx->fd,
					hctx->proc->pid,
					hctx->proc->socket,
					hctx->proc->port,
					hctx->proc->load);
		}

		/* move the proc-list entry down the list */
		scgi_proclist_sort_up(srv, hctx->host, hctx->proc);

		scgi_set_state(srv, hctx, FCGI_STATE_PREPARE_WRITE);
		/* fall through */
	case FCGI_STATE_PREPARE_WRITE:
		scgi_create_env(srv, hctx);

		scgi_set_state(srv, hctx, FCGI_STATE_WRITE);

		/* fall through */
	case FCGI_STATE_WRITE:
		ret = srv->network_backend_write(srv, con, hctx->fd, hctx->wb);

		chunkqueue_remove_finished_chunks(hctx->wb);

		if (ret < 0) {
			if (errno == ENOTCONN || ret == -2) {
				/* the connection got dropped after accept()
				 *
				 * this is most of the time a PHP which dies
				 * after PHP_FCGI_MAX_REQUESTS
				 *
				 */
				if (hctx->wb->bytes_out == 0 &&
				    hctx->reconnects < 5) {
					usleep(10000); /* take away the load of the webserver
							* to let the php a chance to restart
							*/

					scgi_reconnect(srv, hctx);

					return HANDLER_WAIT_FOR_FD;
				}

				/* not reconnected ... why
				 *
				 * far@#lighttpd report this for FreeBSD
				 *
				 */

				log_error_write(srv, __FILE__, __LINE__, "ssosd",
						"connection was dropped after accept(). reconnect() denied:",
						"write-offset:", hctx->wb->bytes_out,
						"reconnect attempts:", hctx->reconnects);

				return HANDLER_ERROR;
			} else {
				/* -1 == ret => error on our side */
				log_error_write(srv, __FILE__, __LINE__, "ssd",
					"write failed:", strerror(errno), errno);

				return HANDLER_ERROR;
			}
		}

		if (hctx->wb->bytes_out == hctx->wb->bytes_in) {
			/* we don't need the out event anymore */
			fdevent_event_del(srv->ev, &(hctx->fde_ndx), hctx->fd);
			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_IN);
			scgi_set_state(srv, hctx, FCGI_STATE_READ);
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

SUBREQUEST_FUNC(mod_scgi_handle_subrequest) {
	plugin_data *p = p_d;

	handler_ctx *hctx = con->plugin_ctx[p->id];
	scgi_proc *proc;
	scgi_extension_host *host;

	if (NULL == hctx) return HANDLER_GO_ON;

	/* not my job */
	if (con->mode != p->id) return HANDLER_GO_ON;

	/* ok, create the request */
	switch(scgi_write_request(srv, hctx)) {
	case HANDLER_ERROR:
		proc = hctx->proc;
		host = hctx->host;

		if (proc &&
		    0 == proc->is_local &&
		    proc->state != PROC_STATE_DISABLED) {
			/* only disable remote servers as we don't manage them*/

			log_error_write(srv, __FILE__, __LINE__,  "sbdb", "fcgi-server disabled:",
					host->host,
					proc->port,
					proc->socket);

			/* disable this server */
			proc->disable_ts = srv->cur_ts;
			proc->state = PROC_STATE_DISABLED;
			host->active_procs--;
		}

		if (hctx->state == FCGI_STATE_INIT ||
		    hctx->state == FCGI_STATE_CONNECT) {
			/* connect() or getsockopt() failed,
			 * restart the request-handling
			 */
			if (proc && proc->is_local) {

				if (p->conf.debug) {
					log_error_write(srv, __FILE__, __LINE__,  "sbdb", "connect() to scgi failed, restarting the request-handling:",
							host->host,
							proc->port,
							proc->socket);
				}

				/*
				 * several hctx might reference the same proc
				 *
				 * Only one of them should mark the proc as dead all the other
				 * ones should just take a new one.
				 *
				 * If a new proc was started with the old struct this might lead
				 * the mark a perfect proc as dead otherwise
				 *
				 */
				if (proc->state == PROC_STATE_RUNNING &&
				    hctx->pid == proc->pid) {
					proc->state = PROC_STATE_DIED_WAIT_FOR_PID;
				}
			}
			scgi_restart_dead_procs(srv, p, host);

			scgi_connection_cleanup(srv, hctx);

			buffer_reset(con->physical.path);
			con->mode = DIRECT;
			joblist_append(srv, con);

			/* mis-using HANDLER_WAIT_FOR_FD to break out of the loop
			 * and hope that the childs will be restarted
			 *
			 */
			return HANDLER_WAIT_FOR_FD;
		} else {
			scgi_connection_cleanup(srv, hctx);

			buffer_reset(con->physical.path);
			con->mode = DIRECT;
			con->http_status = 503;

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

static handler_t scgi_connection_close(server *srv, handler_ctx *hctx) {
	connection  *con;

	if (NULL == hctx) return HANDLER_GO_ON;

	con  = hctx->remote_conn;

	log_error_write(srv, __FILE__, __LINE__, "ssdsd",
			"emergency exit: scgi:",
			"connection-fd:", con->fd,
			"fcgi-fd:", hctx->fd);

	scgi_connection_cleanup(srv, hctx);

	return HANDLER_FINISHED;
}


static handler_t scgi_handle_fdevent(server *srv, void *ctx, int revents) {
	handler_ctx *hctx = ctx;
	connection  *con  = hctx->remote_conn;
	plugin_data *p    = hctx->plugin_data;

	scgi_proc *proc   = hctx->proc;
	scgi_extension_host *host= hctx->host;

	if ((revents & FDEVENT_IN) &&
	    hctx->state == FCGI_STATE_READ) {
		switch (scgi_demux_response(srv, hctx)) {
		case 0:
			break;
		case 1:
			/* we are done */
			scgi_connection_cleanup(srv, hctx);

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
						log_error_write(srv, __FILE__, __LINE__, "ssdsbsdsd",
								"--- scgi spawning",
								"\n\tport:", host->port,
								"\n\tsocket", host->unixsocket,
								"\n\tcurrent:", 1, "/", host->min_procs);
					}

					if (scgi_spawn_connection(srv, p, host, proc)) {
						/* child died */
						proc->state = PROC_STATE_DIED;
					} else {
						scgi_proclist_sort_down(srv, host, proc);
					}

					break;
				}
			}

			if (con->file_started == 0) {
				/* nothing has been send out yet, try to use another child */

				if (hctx->wb->bytes_out == 0 &&
				    hctx->reconnects < 5) {
					scgi_reconnect(srv, hctx);

					log_error_write(srv, __FILE__, __LINE__, "ssdsd",
						"response not sent, request not sent, reconnection.",
						"connection-fd:", con->fd,
						"fcgi-fd:", hctx->fd);

					return HANDLER_WAIT_FOR_FD;
				}

				log_error_write(srv, __FILE__, __LINE__, "sosdsd",
						"response not sent, request sent:", hctx->wb->bytes_out,
						"connection-fd:", con->fd,
						"fcgi-fd:", hctx->fd);

				scgi_connection_cleanup(srv, hctx);

				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				buffer_reset(con->physical.path);
				con->http_status = 500;
				con->mode = DIRECT;
			} else {
				/* response might have been already started, kill the connection */
				log_error_write(srv, __FILE__, __LINE__, "ssdsd",
						"response already sent out, termination connection",
						"connection-fd:", con->fd,
						"fcgi-fd:", hctx->fd);

				scgi_connection_cleanup(srv, hctx);

				connection_set_state(srv, con, CON_STATE_ERROR);
			}

			/* */


			joblist_append(srv, con);
			return HANDLER_FINISHED;
		}
	}

	if (revents & FDEVENT_OUT) {
		if (hctx->state == FCGI_STATE_CONNECT ||
		    hctx->state == FCGI_STATE_WRITE) {
			/* we are allowed to send something out
			 *
			 * 1. in a unfinished connect() call
			 * 2. in a unfinished write() call (long POST request)
			 */
			return mod_scgi_handle_subrequest(srv, con, p);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"got a FDEVENT_OUT and didn't know why:",
					hctx->state);
		}
	}

	/* perhaps this issue is already handled */
	if (revents & FDEVENT_HUP) {
		if (hctx->state == FCGI_STATE_CONNECT) {
			/* getoptsock will catch this one (right ?)
			 *
			 * if we are in connect we might get a EINPROGRESS
			 * in the first call and a FDEVENT_HUP in the
			 * second round
			 *
			 * FIXME: as it is a bit ugly.
			 *
			 */
			return mod_scgi_handle_subrequest(srv, con, p);
		} else if (hctx->state == FCGI_STATE_READ &&
			   hctx->proc->port == 0) {
			/* FIXME:
			 *
			 * ioctl says 8192 bytes to read from PHP and we receive directly a HUP for the socket
			 * even if the FCGI_FIN packet is not received yet
			 */
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sbSBSDSd",
					"error: unexpected close of scgi connection for",
					con->uri.path,
					"(no scgi process on host: ",
					host->host,
					", port: ",
					host->port,
					" ?)",
					hctx->state);

			connection_set_state(srv, con, CON_STATE_ERROR);
			scgi_connection_close(srv, hctx);
			joblist_append(srv, con);
		}
	} else if (revents & FDEVENT_ERR) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"fcgi: got a FDEVENT_ERR. Don't know why.");
		/* kill all connections to the scgi process */


		connection_set_state(srv, con, CON_STATE_ERROR);
		scgi_connection_close(srv, hctx);
		joblist_append(srv, con);
	}

	return HANDLER_FINISHED;
}
#define PATCH(x) \
	p->conf.x = s->x;
static int scgi_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(exts);
	PATCH(debug);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("scgi.server"))) {
				PATCH(exts);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("scgi.debug"))) {
				PATCH(debug);
			}
		}
	}

	return 0;
}
#undef PATCH


static handler_t scgi_check_extension(server *srv, connection *con, void *p_d, int uri_path_handler) {
	plugin_data *p = p_d;
	size_t s_len;
	int used = -1;
	size_t k;
	buffer *fn;
	scgi_extension *extension = NULL;
	scgi_extension_host *host = NULL;

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	/* Possibly, we processed already this request */
	if (con->file_started == 1) return HANDLER_GO_ON;

	fn = uri_path_handler ? con->uri.path : con->physical.path;

	if (buffer_is_empty(fn)) return HANDLER_GO_ON;

	s_len = fn->used - 1;

	scgi_patch_connection(srv, con, p);

	/* check if extension matches */
	for (k = 0; k < p->conf.exts->used; k++) {
		size_t ct_len;
		scgi_extension *ext = p->conf.exts->exts[k];

		if (ext->key->used == 0) continue;

		ct_len = ext->key->used - 1;

		if (s_len < ct_len) continue;

		/* check extension in the form "/scgi_pattern" */
		if (*(ext->key->ptr) == '/') {
			if (strncmp(fn->ptr, ext->key->ptr, ct_len) == 0) {
				extension = ext;
				break;
			}
		} else if (0 == strncmp(fn->ptr + s_len - ct_len, ext->key->ptr, ct_len)) {
			/* check extension in the form ".fcg" */
			extension = ext;
			break;
		}
	}

	/* extension doesn't match */
	if (NULL == extension) {
		return HANDLER_GO_ON;
	}

	/* get best server */
	for (k = 0; k < extension->used; k++) {
		scgi_extension_host *h = extension->hosts[k];

		/* we should have at least one proc that can do something */
		if (h->active_procs == 0) {
			continue;
		}

		if (used == -1 || h->load < used) {
			used = h->load;

			host = h;
		}
	}

	if (!host) {
		/* sorry, we don't have a server alive for this ext */
		buffer_reset(con->physical.path);
		con->http_status = 500;

		/* only send the 'no handler' once */
		if (!extension->note_is_sent) {
			extension->note_is_sent = 1;

			log_error_write(srv, __FILE__, __LINE__, "sbsbs",
					"all handlers for ", con->uri.path,
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
			hctx->host             = host;
			hctx->proc	       = NULL;

			hctx->conf.exts        = p->conf.exts;
			hctx->conf.debug       = p->conf.debug;

			con->plugin_ctx[p->id] = hctx;

			host->load++;

			con->mode = p->id;

			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__, "s",
				"handling it in mod_fastcgi");
			}

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
			 */

			/* the rewrite is only done for /prefix/? matches */
			if (host->fix_root_path_name && extension->key->ptr[0] == '/' && extension->key->ptr[1] == '\0') {
				buffer_copy_string(con->request.pathinfo, con->uri.path->ptr);
				con->uri.path->used = 1;
				con->uri.path->ptr[con->uri.path->used - 1] = '\0';
			} else if (extension->key->ptr[0] == '/' &&
			    con->uri.path->used > extension->key->used &&
			    NULL != (pathinfo = strchr(con->uri.path->ptr + extension->key->used - 1, '/'))) {
				/* rewrite uri.path and pathinfo */

				buffer_copy_string(con->request.pathinfo, pathinfo);

				con->uri.path->used -= con->request.pathinfo->used - 1;
				con->uri.path->ptr[con->uri.path->used - 1] = '\0';
			}
		}
	} else {
		handler_ctx *hctx;
		hctx = handler_ctx_init();

		hctx->remote_conn      = con;
		hctx->plugin_data      = p;
		hctx->host             = host;
		hctx->proc             = NULL;

		hctx->conf.exts        = p->conf.exts;
		hctx->conf.debug       = p->conf.debug;

		con->plugin_ctx[p->id] = hctx;

		host->load++;

		con->mode = p->id;

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__, "s", "handling it in mod_fastcgi");
		}
	}

	return HANDLER_GO_ON;
}

/* uri-path handler */
static handler_t scgi_check_extension_1(server *srv, connection *con, void *p_d) {
	return scgi_check_extension(srv, con, p_d, 1);
}

/* start request handler */
static handler_t scgi_check_extension_2(server *srv, connection *con, void *p_d) {
	return scgi_check_extension(srv, con, p_d, 0);
}

JOBLIST_FUNC(mod_scgi_handle_joblist) {
	plugin_data *p = p_d;
	handler_ctx *hctx = con->plugin_ctx[p->id];

	if (hctx == NULL) return HANDLER_GO_ON;

	if (hctx->fd != -1) {
		switch (hctx->state) {
		case FCGI_STATE_READ:
			fdevent_event_set(srv->ev, &(hctx->fde_ndx), hctx->fd, FDEVENT_IN);

			break;
		case FCGI_STATE_CONNECT:
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


static handler_t scgi_connection_close_callback(server *srv, connection *con, void *p_d) {
	plugin_data *p = p_d;

	return scgi_connection_close(srv, con->plugin_ctx[p->id]);
}

TRIGGER_FUNC(mod_scgi_handle_trigger) {
	plugin_data *p = p_d;
	size_t i, j, n;


	/* perhaps we should kill a connect attempt after 10-15 seconds
	 *
	 * currently we wait for the TCP timeout which is on Linux 180 seconds
	 *
	 *
	 *
	 */

	/* check all childs if they are still up */

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *conf;
		scgi_exts *exts;

		conf = p->config_storage[i];

		exts = conf->exts;

		for (j = 0; j < exts->used; j++) {
			scgi_extension *ex;

			ex = exts->exts[j];

			for (n = 0; n < ex->used; n++) {

				scgi_proc *proc;
				unsigned long sum_load = 0;
				scgi_extension_host *host;

				host = ex->hosts[n];

				scgi_restart_dead_procs(srv, p, host);

				for (proc = host->first; proc; proc = proc->next) {
					sum_load += proc->load;
				}

				if (host->num_procs &&
				    host->num_procs < host->max_procs &&
				    (sum_load / host->num_procs) > host->max_load_per_proc) {
					/* overload, spawn new child */
					scgi_proc *fp = NULL;

					if (p->conf.debug) {
						log_error_write(srv, __FILE__, __LINE__, "s",
								"overload detected, spawning a new child");
					}

					for (fp = host->unused_procs; fp && fp->pid != 0; fp = fp->next);

					if (fp) {
						if (fp == host->unused_procs) host->unused_procs = fp->next;

						if (fp->next) fp->next->prev = NULL;

						host->max_id++;
					} else {
						fp = scgi_process_init();
						fp->id = host->max_id++;
					}

					host->num_procs++;

					if (buffer_is_empty(host->unixsocket)) {
						fp->port = host->port + fp->id;
					} else {
						buffer_copy_string_buffer(fp->socket, host->unixsocket);
						buffer_append_string_len(fp->socket, CONST_STR_LEN("-"));
						buffer_append_long(fp->socket, fp->id);
					}

					if (scgi_spawn_connection(srv, p, host, fp)) {
						log_error_write(srv, __FILE__, __LINE__, "s",
								"ERROR: spawning fcgi failed.");
						return HANDLER_ERROR;
					}

					fp->prev = NULL;
					fp->next = host->first;
					if (host->first) {
						host->first->prev = fp;
					}
					host->first = fp;
				}

				for (proc = host->first; proc; proc = proc->next) {
					if (proc->load != 0) break;
					if (host->num_procs <= host->min_procs) break;
					if (proc->pid == 0) continue;

					if (srv->cur_ts - proc->last_used > host->idle_timeout) {
						/* a proc is idling for a long time now,
						 * terminated it */

						if (p->conf.debug) {
							log_error_write(srv, __FILE__, __LINE__, "ssbsd",
									"idle-timeout reached, terminating child:",
									"socket:", proc->socket,
									"pid", proc->pid);
						}


						if (proc->next) proc->next->prev = proc->prev;
						if (proc->prev) proc->prev->next = proc->next;

						if (proc->prev == NULL) host->first = proc->next;

						proc->prev = NULL;
						proc->next = host->unused_procs;

						if (host->unused_procs) host->unused_procs->prev = proc;
						host->unused_procs = proc;

						kill(proc->pid, SIGTERM);

						proc->state = PROC_STATE_KILLED;

						log_error_write(srv, __FILE__, __LINE__, "ssbsd",
									"killed:",
									"socket:", proc->socket,
									"pid", proc->pid);

						host->num_procs--;

						/* proc is now in unused, let the next second handle the next process */
						break;
					}
				}

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
										WEXITSTATUS(status), proc->socket);
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
						proc->state = PROC_STATE_UNSET;
						host->max_id--;
					}
				}
			}
		}
	}

	return HANDLER_GO_ON;
}


int mod_scgi_plugin_init(plugin *p);
int mod_scgi_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name         = buffer_init_string("scgi");

	p->init         = mod_scgi_init;
	p->cleanup      = mod_scgi_free;
	p->set_defaults = mod_scgi_set_defaults;
	p->connection_reset        = scgi_connection_reset;
	p->handle_connection_close = scgi_connection_close_callback;
	p->handle_uri_clean        = scgi_check_extension_1;
	p->handle_subrequest_start = scgi_check_extension_2;
	p->handle_subrequest       = mod_scgi_handle_subrequest;
	p->handle_joblist          = mod_scgi_handle_joblist;
	p->handle_trigger          = mod_scgi_handle_trigger;

	p->data         = NULL;

	return 0;
}
