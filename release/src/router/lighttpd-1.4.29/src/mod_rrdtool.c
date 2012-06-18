#include "server.h"
#include "connections.h"
#include "response.h"
#include "connections.h"
#include "log.h"

#include "plugin.h"
#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#ifdef HAVE_FORK
/* no need for waitpid if we don't have fork */
#include <sys/wait.h>
#endif
typedef struct {
	buffer *path_rrdtool_bin;
	buffer *path_rrd;

	double requests, *requests_ptr;
	double bytes_written, *bytes_written_ptr;
	double bytes_read, *bytes_read_ptr;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *cmd;
	buffer *resp;

	int read_fd, write_fd;
	pid_t rrdtool_pid;

	int rrdtool_running;

	plugin_config **config_storage;
	plugin_config conf;
} plugin_data;

INIT_FUNC(mod_rrd_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->resp = buffer_init();
	p->cmd = buffer_init();

	return p;
}

FREE_FUNC(mod_rrd_free) {
	plugin_data *p = p_d;
	size_t i;

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			buffer_free(s->path_rrdtool_bin);
			buffer_free(s->path_rrd);

			free(s);
		}
	}
	buffer_free(p->cmd);
	buffer_free(p->resp);

	free(p->config_storage);

	if (p->rrdtool_pid) {
		int status;
		close(p->read_fd);
		close(p->write_fd);
#ifdef HAVE_FORK
		/* collect status */
		waitpid(p->rrdtool_pid, &status, 0);
#endif
	}

	free(p);

	return HANDLER_GO_ON;
}

static int mod_rrd_create_pipe(server *srv, plugin_data *p) {
#ifdef HAVE_FORK
	pid_t pid;

	int to_rrdtool_fds[2];
	int from_rrdtool_fds[2];
	if (pipe(to_rrdtool_fds)) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"pipe failed: ", strerror(errno));
		return -1;
	}

	if (pipe(from_rrdtool_fds)) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"pipe failed: ", strerror(errno));
		return -1;
	}

	/* fork, execve */
	switch (pid = fork()) {
	case 0: {
		/* child */
		char **args;
		int argc;
		int i = 0;
		char *dash = "-";

		/* move stdout to from_rrdtool_fd[1] */
		close(STDOUT_FILENO);
		dup2(from_rrdtool_fds[1], STDOUT_FILENO);
		close(from_rrdtool_fds[1]);
		/* not needed */
		close(from_rrdtool_fds[0]);

		/* move the stdin to to_rrdtool_fd[0] */
		close(STDIN_FILENO);
		dup2(to_rrdtool_fds[0], STDIN_FILENO);
		close(to_rrdtool_fds[0]);
		/* not needed */
		close(to_rrdtool_fds[1]);

		/* set up args */
		argc = 3;
		args = malloc(sizeof(*args) * argc);
		i = 0;

		args[i++] = p->conf.path_rrdtool_bin->ptr;
		args[i++] = dash;
		args[i  ] = NULL;

		/* we don't need the client socket */
		for (i = 3; i < 256; i++) {
			close(i);
		}

		/* exec the cgi */
		execv(args[0], args);

		/* log_error_write(srv, __FILE__, __LINE__, "sss", "spawing rrdtool failed: ", strerror(errno), args[0]); */

		/* */
		SEGFAULT();
		break;
	}
	case -1:
		/* error */
		log_error_write(srv, __FILE__, __LINE__, "ss", "fork failed: ", strerror(errno));
		break;
	default: {
		/* father */

		close(from_rrdtool_fds[1]);
		close(to_rrdtool_fds[0]);

		/* register PID and wait for them asyncronously */
		p->write_fd = to_rrdtool_fds[1];
		p->read_fd = from_rrdtool_fds[0];
		p->rrdtool_pid = pid;

#ifdef FD_CLOEXEC
		fcntl(p->write_fd, F_SETFD, FD_CLOEXEC);
		fcntl(p->read_fd, F_SETFD, FD_CLOEXEC);
#endif

		break;
	}
	}

	return 0;
#else
	return -1;
#endif
}

/* read/write wrappers to catch EINTR */

/* write to blocking socket; blocks until all data is sent, write returns 0 or an error (apart from EINTR) occurs. */
static ssize_t safe_write(int fd, const void *buf, size_t count) {
	ssize_t res, sum = 0;

	for (;;) {
		res = write(fd, buf, count);
		if (res >= 0) {
			sum += res;
			/* do not try again if res == 0 */
			if (res == 0 || (size_t) res == count) return sum;
			count -= res;
			buf = (const char*) buf + res;
			continue;
		}
		switch (errno) {
		case EINTR:
			continue;
		default:
			return -1;
		}
	}
}

/* this assumes we get enough data on a successful read */
static ssize_t safe_read(int fd, void *buf, size_t count) {
	ssize_t res;

	for (;;) {
		res = read(fd, buf, count);
		if (res >= 0) return res;
		switch (errno) {
		case EINTR:
			continue;
		default:
			return -1;
		}
	}
}

static int mod_rrdtool_create_rrd(server *srv, plugin_data *p, plugin_config *s) {
	struct stat st;
	int r;

	/* check if DB already exists */
	if (0 == stat(s->path_rrd->ptr, &st)) {
		/* check if it is plain file */
		if (!S_ISREG(st.st_mode)) {
			log_error_write(srv, __FILE__, __LINE__, "sb",
					"not a regular file:", s->path_rrd);
			return HANDLER_ERROR;
		}

		/* still create DB if it's empty file */
		if (st.st_size > 0) {
			return HANDLER_GO_ON;
		}
	}

	/* create a new one */
	buffer_copy_string_len(p->cmd, CONST_STR_LEN("create "));
	buffer_append_string_buffer(p->cmd, s->path_rrd);
	buffer_append_string_len(p->cmd, CONST_STR_LEN(
		" --step 60 "
		"DS:InOctets:ABSOLUTE:600:U:U "
		"DS:OutOctets:ABSOLUTE:600:U:U "
		"DS:Requests:ABSOLUTE:600:U:U "
		"RRA:AVERAGE:0.5:1:600 "
		"RRA:AVERAGE:0.5:6:700 "
		"RRA:AVERAGE:0.5:24:775 "
		"RRA:AVERAGE:0.5:288:797 "
		"RRA:MAX:0.5:1:600 "
		"RRA:MAX:0.5:6:700 "
		"RRA:MAX:0.5:24:775 "
		"RRA:MAX:0.5:288:797 "
		"RRA:MIN:0.5:1:600 "
		"RRA:MIN:0.5:6:700 "
		"RRA:MIN:0.5:24:775 "
		"RRA:MIN:0.5:288:797\n"));

	if (-1 == (safe_write(p->write_fd, p->cmd->ptr, p->cmd->used - 1))) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
			"rrdtool-write: failed", strerror(errno));

		return HANDLER_ERROR;
	}

	buffer_prepare_copy(p->resp, 4096);
	if (-1 == (r = safe_read(p->read_fd, p->resp->ptr, p->resp->size))) {
		log_error_write(srv, __FILE__, __LINE__, "ss",
			"rrdtool-read: failed", strerror(errno));

		return HANDLER_ERROR;
	}

	p->resp->used = r;

	if (p->resp->ptr[0] != 'O' ||
		p->resp->ptr[1] != 'K') {
		log_error_write(srv, __FILE__, __LINE__, "sbb",
			"rrdtool-response:", p->cmd, p->resp);

		return HANDLER_ERROR;
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_rrd_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(path_rrdtool_bin);
	PATCH(path_rrd);

	p->conf.bytes_written_ptr = &(s->bytes_written);
	p->conf.bytes_read_ptr = &(s->bytes_read);
	p->conf.requests_ptr = &(s->requests);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("rrdtool.db-name"))) {
				PATCH(path_rrd);
				/* get pointers to double values */

				p->conf.bytes_written_ptr = &(s->bytes_written);
				p->conf.bytes_read_ptr = &(s->bytes_read);
				p->conf.requests_ptr = &(s->requests);
			}
		}
	}

	return 0;
}
#undef PATCH

SETDEFAULTS_FUNC(mod_rrd_set_defaults) {
	plugin_data *p = p_d;
	size_t i;

	config_values_t cv[] = {
		{ "rrdtool.binary",              NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },
		{ "rrdtool.db-name",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->path_rrdtool_bin = buffer_init();
		s->path_rrd = buffer_init();
		s->requests = 0;
		s->bytes_written = 0;
		s->bytes_read = 0;

		cv[0].destination = s->path_rrdtool_bin;
		cv[1].destination = s->path_rrd;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}

		if (i > 0 && !buffer_is_empty(s->path_rrdtool_bin)) {
			/* path_rrdtool_bin is a global option */

			log_error_write(srv, __FILE__, __LINE__, "s",
					"rrdtool.binary can only be set as a global option.");

			return HANDLER_ERROR;
		}

	}

	p->conf.path_rrdtool_bin = p->config_storage[0]->path_rrdtool_bin;
	p->rrdtool_running = 0;

	/* check for dir */

	if (buffer_is_empty(p->conf.path_rrdtool_bin)) {
		log_error_write(srv, __FILE__, __LINE__, "s",
				"rrdtool.binary has to be set");
		return HANDLER_ERROR;
	}

	/* open the pipe to rrdtool */
	if (mod_rrd_create_pipe(srv, p)) {
		return HANDLER_ERROR;
	}

	p->rrdtool_running = 1;

	return HANDLER_GO_ON;
}

TRIGGER_FUNC(mod_rrd_trigger) {
	plugin_data *p = p_d;
	size_t i;

	if (!p->rrdtool_running) return HANDLER_GO_ON;
	if ((srv->cur_ts % 60) != 0) return HANDLER_GO_ON;

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s = p->config_storage[i];
		int r;

		if (buffer_is_empty(s->path_rrd)) continue;

		/* write the data down every minute */

		if (HANDLER_GO_ON != mod_rrdtool_create_rrd(srv, p, s)) return HANDLER_ERROR;

		buffer_copy_string_len(p->cmd, CONST_STR_LEN("update "));
		buffer_append_string_buffer(p->cmd, s->path_rrd);
		buffer_append_string_len(p->cmd, CONST_STR_LEN(" N:"));
		buffer_append_off_t(p->cmd, s->bytes_read);
		buffer_append_string_len(p->cmd, CONST_STR_LEN(":"));
		buffer_append_off_t(p->cmd, s->bytes_written);
		buffer_append_string_len(p->cmd, CONST_STR_LEN(":"));
		buffer_append_long(p->cmd, s->requests);
		buffer_append_string_len(p->cmd, CONST_STR_LEN("\n"));

		if (-1 == (r = safe_write(p->write_fd, p->cmd->ptr, p->cmd->used - 1))) {
			p->rrdtool_running = 0;

			log_error_write(srv, __FILE__, __LINE__, "ss",
					"rrdtool-write: failed", strerror(errno));

			return HANDLER_ERROR;
		}

		buffer_prepare_copy(p->resp, 4096);
		if (-1 == (r = safe_read(p->read_fd, p->resp->ptr, p->resp->size))) {
			p->rrdtool_running = 0;

			log_error_write(srv, __FILE__, __LINE__, "ss",
					"rrdtool-read: failed", strerror(errno));

			return HANDLER_ERROR;
		}

		p->resp->used = r;

		if (p->resp->ptr[0] != 'O' ||
		    p->resp->ptr[1] != 'K') {
			/* don't fail on this error if we just started (graceful restart, the old one might have just updated too) */
			if (!(strstr(p->resp->ptr, "(minimum one second step)") && (srv->cur_ts - srv->startup_ts < 3))) {
				p->rrdtool_running = 0;

				log_error_write(srv, __FILE__, __LINE__, "sbb",
					"rrdtool-response:", p->cmd, p->resp);

				return HANDLER_ERROR;
			}
		}
		s->requests = 0;
		s->bytes_written = 0;
		s->bytes_read = 0;
	}

	return HANDLER_GO_ON;
}

REQUESTDONE_FUNC(mod_rrd_account) {
	plugin_data *p = p_d;

	mod_rrd_patch_connection(srv, con, p);

	*(p->conf.requests_ptr)      += 1;
	*(p->conf.bytes_written_ptr) += con->bytes_written;
	*(p->conf.bytes_read_ptr)    += con->bytes_read;

	return HANDLER_GO_ON;
}

int mod_rrdtool_plugin_init(plugin *p);
int mod_rrdtool_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("rrd");

	p->init        = mod_rrd_init;
	p->cleanup     = mod_rrd_free;
	p->set_defaults= mod_rrd_set_defaults;

	p->handle_trigger      = mod_rrd_trigger;
	p->handle_request_done = mod_rrd_account;

	p->data        = NULL;

	return 0;
}
