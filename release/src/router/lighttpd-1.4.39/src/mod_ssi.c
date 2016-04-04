#include "base.h"
#include "log.h"
#include "buffer.h"
#include "stat_cache.h"

#include "plugin.h"
#include "stream.h"

#include "response.h"

#include "mod_ssi.h"

#include "inet_ntop_cache.h"

#include "sys-socket.h"

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#ifdef HAVE_FORK
# include <sys/wait.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "etag.h"
#include "version.h"

/* The newest modified time of included files for include statement */
static volatile time_t include_file_last_mtime = 0;

/* init the plugin data */
INIT_FUNC(mod_ssi_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->timefmt = buffer_init();
	p->stat_fn = buffer_init();

	p->ssi_vars = array_init();
	p->ssi_cgi_env = array_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_ssi_free) {
	plugin_data *p = p_d;
	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->ssi_extension);
			buffer_free(s->content_type);

			free(s);
		}
		free(p->config_storage);
	}

	array_free(p->ssi_vars);
	array_free(p->ssi_cgi_env);
#ifdef HAVE_PCRE_H
	pcre_free(p->ssi_regex);
#endif
	buffer_free(p->timefmt);
	buffer_free(p->stat_fn);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_ssi_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;
#ifdef HAVE_PCRE_H
	const char *errptr;
	int erroff;
#endif

	config_values_t cv[] = {
		{ "ssi.extension",              NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "ssi.content-type",           NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },      /* 1 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->ssi_extension  = array_init();
		s->content_type = buffer_init();

		cv[0].destination = s->ssi_extension;
		cv[1].destination = s->content_type;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

#ifdef HAVE_PCRE_H
	/* allow 2 params */
	if (NULL == (p->ssi_regex = pcre_compile("<!--#([a-z]+)\\s+(?:([a-z]+)=\"(.*?)(?<!\\\\)\"\\s*)?(?:([a-z]+)=\"(.*?)(?<!\\\\)\"\\s*)?-->", 0, &errptr, &erroff, NULL))) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"ssi: pcre ",
				erroff, errptr);
		return HANDLER_ERROR;
	}
#else
	log_error_write(srv, __FILE__, __LINE__, "s",
			"mod_ssi: pcre support is missing, please recompile with pcre support or remove mod_ssi from the list of modules");
	return HANDLER_ERROR;
#endif

	return HANDLER_GO_ON;
}

static int ssi_env_add(array *env, const char *key, const char *val) {
	data_string *ds;

	if (NULL == (ds = (data_string *)array_get_unused_element(env, TYPE_STRING))) {
		ds = data_string_init();
	}
	buffer_copy_string(ds->key,   key);
	buffer_copy_string(ds->value, val);

	array_insert_unique(env, (data_unset *)ds);

	return 0;
}

/**
 *
 *  the next two functions are take from fcgi.c
 *
 */

static int ssi_env_add_request_headers(server *srv, connection *con, plugin_data *p) {
	size_t i;

	for (i = 0; i < con->request.headers->used; i++) {
		data_string *ds;

		ds = (data_string *)con->request.headers->data[i];

		if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key)) {
			/* don't forward the Authorization: Header */
			if (0 == strcasecmp(ds->key->ptr, "AUTHORIZATION")) {
				continue;
			}

			buffer_copy_string_encoded_cgi_varnames(srv->tmp_buf, CONST_BUF_LEN(ds->key), 1);

			ssi_env_add(p->ssi_cgi_env, srv->tmp_buf->ptr, ds->value->ptr);
		}
	}

	for (i = 0; i < con->environment->used; i++) {
		data_string *ds;

		ds = (data_string *)con->environment->data[i];

		if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key)) {
			buffer_copy_string_encoded_cgi_varnames(srv->tmp_buf, CONST_BUF_LEN(ds->key), 0);

			ssi_env_add(p->ssi_cgi_env, srv->tmp_buf->ptr, ds->value->ptr);
		}
	}

	return 0;
}

static int build_ssi_cgi_vars(server *srv, connection *con, plugin_data *p) {
	char buf[LI_ITOSTRING_LENGTH];

	server_socket *srv_sock = con->srv_socket;

#ifdef HAVE_IPV6
	char b2[INET6_ADDRSTRLEN + 1];
#endif

#define CONST_STRING(x) \
		x

	array_reset(p->ssi_cgi_env);

	ssi_env_add(p->ssi_cgi_env, CONST_STRING("SERVER_SOFTWARE"), PACKAGE_DESC);
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("SERVER_NAME"),
#ifdef HAVE_IPV6
		     inet_ntop(srv_sock->addr.plain.sa_family,
			       srv_sock->addr.plain.sa_family == AF_INET6 ?
			       (const void *) &(srv_sock->addr.ipv6.sin6_addr) :
			       (const void *) &(srv_sock->addr.ipv4.sin_addr),
			       b2, sizeof(b2)-1)
#else
		     inet_ntoa(srv_sock->addr.ipv4.sin_addr)
#endif
		     );
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("GATEWAY_INTERFACE"), "CGI/1.1");

	li_utostr(buf,
#ifdef HAVE_IPV6
	       ntohs(srv_sock->addr.plain.sa_family ? srv_sock->addr.ipv6.sin6_port : srv_sock->addr.ipv4.sin_port)
#else
	       ntohs(srv_sock->addr.ipv4.sin_port)
#endif
	       );

	ssi_env_add(p->ssi_cgi_env, CONST_STRING("SERVER_PORT"), buf);

	ssi_env_add(p->ssi_cgi_env, CONST_STRING("REMOTE_ADDR"),
		    inet_ntop_cache_get_ip(srv, &(con->dst_addr)));

	if (con->request.content_length > 0) {
		/* CGI-SPEC 6.1.2 and FastCGI spec 6.3 */

		li_itostr(buf, con->request.content_length);
		ssi_env_add(p->ssi_cgi_env, CONST_STRING("CONTENT_LENGTH"), buf);
	}

	/*
	 * SCRIPT_NAME, PATH_INFO and PATH_TRANSLATED according to
	 * http://cgi-spec.golux.com/draft-coar-cgi-v11-03-clean.html
	 * (6.1.14, 6.1.6, 6.1.7)
	 */

	ssi_env_add(p->ssi_cgi_env, CONST_STRING("SCRIPT_NAME"), con->uri.path->ptr);
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("PATH_INFO"), "");

	/*
	 * SCRIPT_FILENAME and DOCUMENT_ROOT for php. The PHP manual
	 * http://www.php.net/manual/en/reserved.variables.php
	 * treatment of PATH_TRANSLATED is different from the one of CGI specs.
	 * TODO: this code should be checked against cgi.fix_pathinfo php
	 * parameter.
	 */

	if (!buffer_string_is_empty(con->request.pathinfo)) {
		ssi_env_add(p->ssi_cgi_env, CONST_STRING("PATH_INFO"), con->request.pathinfo->ptr);
	}

	ssi_env_add(p->ssi_cgi_env, CONST_STRING("SCRIPT_FILENAME"), con->physical.path->ptr);
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("DOCUMENT_ROOT"), con->physical.doc_root->ptr);

	ssi_env_add(p->ssi_cgi_env, CONST_STRING("REQUEST_URI"), con->request.uri->ptr);
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("QUERY_STRING"), buffer_is_empty(con->uri.query) ? "" : con->uri.query->ptr);
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("REQUEST_METHOD"), get_http_method_name(con->request.http_method));
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("REDIRECT_STATUS"), "200");
	ssi_env_add(p->ssi_cgi_env, CONST_STRING("SERVER_PROTOCOL"), get_http_version_name(con->request.http_version));

	ssi_env_add_request_headers(srv, con, p);

	return 0;
}

static int process_ssi_stmt(server *srv, connection *con, plugin_data *p, const char **l, size_t n, stat_cache_entry *sce) {
	size_t i, ssicmd = 0;
	char buf[255];
	buffer *b = NULL;

	struct {
		const char *var;
		enum { SSI_UNSET, SSI_ECHO, SSI_FSIZE, SSI_INCLUDE, SSI_FLASTMOD,
				SSI_CONFIG, SSI_PRINTENV, SSI_SET, SSI_IF, SSI_ELIF,
				SSI_ELSE, SSI_ENDIF, SSI_EXEC } type;
	} ssicmds[] = {
		{ "echo",     SSI_ECHO },
		{ "include",  SSI_INCLUDE },
		{ "flastmod", SSI_FLASTMOD },
		{ "fsize",    SSI_FSIZE },
		{ "config",   SSI_CONFIG },
		{ "printenv", SSI_PRINTENV },
		{ "set",      SSI_SET },
		{ "if",       SSI_IF },
		{ "elif",     SSI_ELIF },
		{ "endif",    SSI_ENDIF },
		{ "else",     SSI_ELSE },
		{ "exec",     SSI_EXEC },

		{ NULL, SSI_UNSET }
	};

	for (i = 0; ssicmds[i].var; i++) {
		if (0 == strcmp(l[1], ssicmds[i].var)) {
			ssicmd = ssicmds[i].type;
			break;
		}
	}

	switch(ssicmd) {
	case SSI_ECHO: {
		/* echo */
		int var = 0;
		/* int enc = 0; */
		const char *var_val = NULL;

		struct {
			const char *var;
			enum { SSI_ECHO_UNSET, SSI_ECHO_DATE_GMT, SSI_ECHO_DATE_LOCAL, SSI_ECHO_DOCUMENT_NAME, SSI_ECHO_DOCUMENT_URI,
					SSI_ECHO_LAST_MODIFIED, SSI_ECHO_USER_NAME } type;
		} echovars[] = {
			{ "DATE_GMT",      SSI_ECHO_DATE_GMT },
			{ "DATE_LOCAL",    SSI_ECHO_DATE_LOCAL },
			{ "DOCUMENT_NAME", SSI_ECHO_DOCUMENT_NAME },
			{ "DOCUMENT_URI",  SSI_ECHO_DOCUMENT_URI },
			{ "LAST_MODIFIED", SSI_ECHO_LAST_MODIFIED },
			{ "USER_NAME",     SSI_ECHO_USER_NAME },

			{ NULL, SSI_ECHO_UNSET }
		};

/*
		struct {
			const char *var;
			enum { SSI_ENC_UNSET, SSI_ENC_URL, SSI_ENC_NONE, SSI_ENC_ENTITY } type;
		} encvars[] = {
			{ "url",          SSI_ENC_URL },
			{ "none",         SSI_ENC_NONE },
			{ "entity",       SSI_ENC_ENTITY },

			{ NULL, SSI_ENC_UNSET }
		};
*/

		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "var")) {
				int j;

				var_val = l[i+1];

				for (j = 0; echovars[j].var; j++) {
					if (0 == strcmp(l[i+1], echovars[j].var)) {
						var = echovars[j].type;
						break;
					}
				}
			} else if (0 == strcmp(l[i], "encoding")) {
/*
				int j;

				for (j = 0; encvars[j].var; j++) {
					if (0 == strcmp(l[i+1], encvars[j].var)) {
						enc = encvars[j].type;
						break;
					}
				}
*/
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}

		if (p->if_is_false) break;

		if (!var_val) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"ssi: ",
					l[1], "var is missing");
			break;
		}

		switch(var) {
		case SSI_ECHO_USER_NAME: {
			struct passwd *pw;

			b = buffer_init();
#ifdef HAVE_PWD_H
			if (NULL == (pw = getpwuid(sce->st.st_uid))) {
				buffer_copy_int(b, sce->st.st_uid);
			} else {
				buffer_copy_string(b, pw->pw_name);
			}
#else
			buffer_copy_int(b, sce->st.st_uid);
#endif
			chunkqueue_append_buffer(con->write_queue, b);
			buffer_free(b);
			break;
		}
		case SSI_ECHO_LAST_MODIFIED:	{
			time_t t = sce->st.st_mtime;

			if (0 == strftime(buf, sizeof(buf), p->timefmt->ptr, localtime(&t))) {
				chunkqueue_append_mem(con->write_queue, CONST_STR_LEN("(none)"));
			} else {
				chunkqueue_append_mem(con->write_queue, buf, strlen(buf));
			}
			break;
		}
		case SSI_ECHO_DATE_LOCAL: {
			time_t t = time(NULL);

			if (0 == strftime(buf, sizeof(buf), p->timefmt->ptr, localtime(&t))) {
				chunkqueue_append_mem(con->write_queue, CONST_STR_LEN("(none)"));
			} else {
				chunkqueue_append_mem(con->write_queue, buf, strlen(buf));
			}
			break;
		}
		case SSI_ECHO_DATE_GMT: {
			time_t t = time(NULL);

			if (0 == strftime(buf, sizeof(buf), p->timefmt->ptr, gmtime(&t))) {
				chunkqueue_append_mem(con->write_queue, CONST_STR_LEN("(none)"));
			} else {
				chunkqueue_append_mem(con->write_queue, buf, strlen(buf));
			}
			break;
		}
		case SSI_ECHO_DOCUMENT_NAME: {
			char *sl;

			if (NULL == (sl = strrchr(con->physical.path->ptr, '/'))) {
				chunkqueue_append_mem(con->write_queue, CONST_BUF_LEN(con->physical.path));
			} else {
				chunkqueue_append_mem(con->write_queue, sl + 1, strlen(sl + 1));
			}
			break;
		}
		case SSI_ECHO_DOCUMENT_URI: {
			chunkqueue_append_mem(con->write_queue, CONST_BUF_LEN(con->uri.path));
			break;
		}
		default: {
			data_string *ds;
			/* check if it is a cgi-var */

			if (NULL != (ds = (data_string *)array_get_element(p->ssi_cgi_env, var_val))) {
				chunkqueue_append_mem(con->write_queue, CONST_BUF_LEN(ds->value));
			} else {
				chunkqueue_append_mem(con->write_queue, CONST_STR_LEN("(none)"));
			}

			break;
		}
		}
		break;
	}
	case SSI_INCLUDE:
	case SSI_FLASTMOD:
	case SSI_FSIZE: {
		const char * file_path = NULL, *virt_path = NULL;
		struct stat st;
		char *sl;

		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "file")) {
				file_path = l[i+1];
			} else if (0 == strcmp(l[i], "virtual")) {
				virt_path = l[i+1];
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}

		if (!file_path && !virt_path) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"ssi: ",
					l[1], "file or virtual are missing");
			break;
		}

		if (file_path && virt_path) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"ssi: ",
					l[1], "only one of file and virtual is allowed here");
			break;
		}


		if (p->if_is_false) break;

		if (file_path) {
			/* current doc-root */
			if (NULL == (sl = strrchr(con->physical.path->ptr, '/'))) {
				buffer_copy_string_len(p->stat_fn, CONST_STR_LEN("/"));
			} else {
				buffer_copy_string_len(p->stat_fn, con->physical.path->ptr, sl - con->physical.path->ptr + 1);
			}

			buffer_copy_string(srv->tmp_buf, file_path);
			buffer_urldecode_path(srv->tmp_buf);
			buffer_path_simplify(srv->tmp_buf, srv->tmp_buf);
			buffer_append_string_buffer(p->stat_fn, srv->tmp_buf);
		} else {
			/* virtual */

			if (virt_path[0] == '/') {
				buffer_copy_string(p->stat_fn, virt_path);
			} else {
				/* there is always a / */
				sl = strrchr(con->uri.path->ptr, '/');

				buffer_copy_string_len(p->stat_fn, con->uri.path->ptr, sl - con->uri.path->ptr + 1);
				buffer_append_string(p->stat_fn, virt_path);
			}

			buffer_urldecode_path(p->stat_fn);
			buffer_path_simplify(srv->tmp_buf, p->stat_fn);

			/* we have an uri */

			buffer_copy_buffer(p->stat_fn, con->physical.doc_root);
			buffer_append_string_buffer(p->stat_fn, srv->tmp_buf);
		}

		if (0 == stat(p->stat_fn->ptr, &st)) {
			time_t t = st.st_mtime;

			switch (ssicmd) {
			case SSI_FSIZE:
				b = buffer_init();
				if (p->sizefmt) {
					int j = 0;
					const char *abr[] = { " B", " kB", " MB", " GB", " TB", NULL };

					off_t s = st.st_size;

					for (j = 0; s > 1024 && abr[j+1]; s /= 1024, j++);

					buffer_copy_int(b, s);
					buffer_append_string(b, abr[j]);
				} else {
					buffer_copy_int(b, st.st_size);
				}
				chunkqueue_append_buffer(con->write_queue, b);
				buffer_free(b);
				break;
			case SSI_FLASTMOD:
				if (0 == strftime(buf, sizeof(buf), p->timefmt->ptr, localtime(&t))) {
					chunkqueue_append_mem(con->write_queue, CONST_STR_LEN("(none)"));
				} else {
					chunkqueue_append_mem(con->write_queue, buf, strlen(buf));
				}
				break;
			case SSI_INCLUDE:
				chunkqueue_append_file(con->write_queue, p->stat_fn, 0, st.st_size);

				/* Keep the newest mtime of included files */
				if (st.st_mtime > include_file_last_mtime)
					include_file_last_mtime = st.st_mtime;

				break;
			}
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sbs",
					"ssi: stating failed ",
					p->stat_fn, strerror(errno));
		}
		break;
	}
	case SSI_SET: {
		const char *key = NULL, *val = NULL;
		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "var")) {
				key = l[i+1];
			} else if (0 == strcmp(l[i], "value")) {
				val = l[i+1];
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}

		if (p->if_is_false) break;

		if (key && val) {
			data_string *ds;

			if (NULL == (ds = (data_string *)array_get_unused_element(p->ssi_vars, TYPE_STRING))) {
				ds = data_string_init();
			}
			buffer_copy_string(ds->key,   key);
			buffer_copy_string(ds->value, val);

			array_insert_unique(p->ssi_vars, (data_unset *)ds);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"ssi: var and value have to be set in",
					l[0], l[1]);
		}
		break;
	}
	case SSI_CONFIG:
		if (p->if_is_false) break;

		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "timefmt")) {
				buffer_copy_string(p->timefmt, l[i+1]);
			} else if (0 == strcmp(l[i], "sizefmt")) {
				if (0 == strcmp(l[i+1], "abbrev")) {
					p->sizefmt = 1;
				} else if (0 == strcmp(l[i+1], "abbrev")) {
					p->sizefmt = 0;
				} else {
					log_error_write(srv, __FILE__, __LINE__, "sssss",
							"ssi: unknow value for attribute '",
							l[i],
							"' for ",
							l[1], l[i+1]);
				}
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}
		break;
	case SSI_PRINTENV:
		if (p->if_is_false) break;

		b = buffer_init();
		for (i = 0; i < p->ssi_vars->used; i++) {
			data_string *ds = (data_string *)p->ssi_vars->data[p->ssi_vars->sorted[i]];

			buffer_append_string_buffer(b, ds->key);
			buffer_append_string_len(b, CONST_STR_LEN("="));
			buffer_append_string_encoded(b, CONST_BUF_LEN(ds->value), ENCODING_MINIMAL_XML);
			buffer_append_string_len(b, CONST_STR_LEN("\n"));
		}
		for (i = 0; i < p->ssi_cgi_env->used; i++) {
			data_string *ds = (data_string *)p->ssi_cgi_env->data[p->ssi_cgi_env->sorted[i]];

			buffer_append_string_buffer(b, ds->key);
			buffer_append_string_len(b, CONST_STR_LEN("="));
			buffer_append_string_encoded(b, CONST_BUF_LEN(ds->value), ENCODING_MINIMAL_XML);
			buffer_append_string_len(b, CONST_STR_LEN("\n"));
		}
		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);

		break;
	case SSI_EXEC: {
		const char *cmd = NULL;
		pid_t pid;
		int from_exec_fds[2];

		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "cmd")) {
				cmd = l[i+1];
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}

		if (p->if_is_false) break;

		/* create a return pipe and send output to the html-page
		 *
		 * as exec is assumed evil it is implemented synchronously
		 */

		if (!cmd) break;
#ifdef HAVE_FORK
		if (pipe(from_exec_fds)) {
			log_error_write(srv, __FILE__, __LINE__, "ss",
					"pipe failed: ", strerror(errno));
			return -1;
		}

		/* fork, execve */
		switch (pid = fork()) {
		case 0: {
			/* move stdout to from_rrdtool_fd[1] */
			close(STDOUT_FILENO);
			dup2(from_exec_fds[1], STDOUT_FILENO);
			close(from_exec_fds[1]);
			/* not needed */
			close(from_exec_fds[0]);

			/* close stdin */
			close(STDIN_FILENO);

			execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

			log_error_write(srv, __FILE__, __LINE__, "sss", "spawing exec failed:", strerror(errno), cmd);

			/* */
			SEGFAULT();
			break;
		}
		case -1:
			/* error */
			log_error_write(srv, __FILE__, __LINE__, "ss", "fork failed:", strerror(errno));
			break;
		default: {
			/* father */
			int status;
			ssize_t r;
			int was_interrupted = 0;

			close(from_exec_fds[1]);

			/* wait for the client to end */

			/*
			 * OpenBSD and Solaris send a EINTR on SIGCHILD even if we ignore it
			 */
			do {
				if (-1 == waitpid(pid, &status, 0)) {
					if (errno == EINTR) {
						was_interrupted++;
					} else {
						was_interrupted = 0;
						log_error_write(srv, __FILE__, __LINE__, "ss", "waitpid failed:", strerror(errno));
					}
				} else if (WIFEXITED(status)) {
					int toread;
					/* read everything from client and paste it into the output */
					was_interrupted = 0;

					while(1) {
						if (ioctl(from_exec_fds[0], FIONREAD, &toread)) {
							log_error_write(srv, __FILE__, __LINE__, "s",
								"unexpected end-of-file (perhaps the ssi-exec process died)");
							return -1;
						}

						if (toread > 0) {
							char *mem;
							size_t mem_len;

							chunkqueue_get_memory(con->write_queue, &mem, &mem_len, 0, toread);
							r = read(from_exec_fds[0], mem, mem_len);
							chunkqueue_use_memory(con->write_queue, r > 0 ? r : 0);

							if (r < 0) break; /* read failed */
						} else {
							break;
						}
					}
				} else {
					was_interrupted = 0;
					log_error_write(srv, __FILE__, __LINE__, "s", "process exited abnormally");
				}
			} while (was_interrupted > 0 && was_interrupted < 4); /* if waitpid() gets interrupted, retry, but max 4 times */

			close(from_exec_fds[0]);

			break;
		}
		}
#else

		return -1;
#endif

		break;
	}
	case SSI_IF: {
		const char *expr = NULL;

		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "expr")) {
				expr = l[i+1];
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}

		if (!expr) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"ssi: ",
					l[1], "expr missing");
			break;
		}

		if ((!p->if_is_false) &&
		    ((p->if_is_false_level == 0) ||
		     (p->if_level < p->if_is_false_level))) {
			switch (ssi_eval_expr(srv, con, p, expr)) {
			case -1:
			case 0:
				p->if_is_false = 1;
				p->if_is_false_level = p->if_level;
				break;
			case 1:
				p->if_is_false = 0;
				break;
			}
		}

		p->if_level++;

		break;
	}
	case SSI_ELSE:
		p->if_level--;

		if (p->if_is_false) {
			if ((p->if_level == p->if_is_false_level) &&
			    (p->if_is_false_endif == 0)) {
				p->if_is_false = 0;
			}
		} else {
			p->if_is_false = 1;

			p->if_is_false_level = p->if_level;
		}
		p->if_level++;

		break;
	case SSI_ELIF: {
		const char *expr = NULL;
		for (i = 2; i < n; i += 2) {
			if (0 == strcmp(l[i], "expr")) {
				expr = l[i+1];
			} else {
				log_error_write(srv, __FILE__, __LINE__, "sss",
						"ssi: unknow attribute for ",
						l[1], l[i]);
			}
		}

		if (!expr) {
			log_error_write(srv, __FILE__, __LINE__, "sss",
					"ssi: ",
					l[1], "expr missing");
			break;
		}

		p->if_level--;

		if (p->if_level == p->if_is_false_level) {
			if ((p->if_is_false) &&
			    (p->if_is_false_endif == 0)) {
				switch (ssi_eval_expr(srv, con, p, expr)) {
				case -1:
				case 0:
					p->if_is_false = 1;
					p->if_is_false_level = p->if_level;
					break;
				case 1:
					p->if_is_false = 0;
					break;
				}
			} else {
				p->if_is_false = 1;
				p->if_is_false_level = p->if_level;
				p->if_is_false_endif = 1;
			}
		}

		p->if_level++;

		break;
	}
	case SSI_ENDIF:
		p->if_level--;

		if (p->if_level == p->if_is_false_level) {
			p->if_is_false = 0;
			p->if_is_false_endif = 0;
		}

		break;
	default:
		log_error_write(srv, __FILE__, __LINE__, "ss",
				"ssi: unknow ssi-command:",
				l[1]);
		break;
	}

	return 0;

}

static int mod_ssi_handle_request(server *srv, connection *con, plugin_data *p) {
	stream s;
#ifdef  HAVE_PCRE_H
	int i, n;

#define N 10
	int ovec[N * 3];
#endif

	stat_cache_entry *sce = NULL;


	/* get a stream to the file */

	array_reset(p->ssi_vars);
	array_reset(p->ssi_cgi_env);
	buffer_copy_string_len(p->timefmt, CONST_STR_LEN("%a, %d %b %Y %H:%M:%S %Z"));
	p->sizefmt = 0;
	build_ssi_cgi_vars(srv, con, p);
	p->if_is_false = 0;

	/* Reset the modified time of included files */
	include_file_last_mtime = 0;

	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
		log_error_write(srv, __FILE__, __LINE__,  "SB", "stat_cache_get_entry failed: ", con->physical.path);
		return -1;
	}

	if (-1 == stream_open(&s, con->physical.path)) {
		log_error_write(srv, __FILE__, __LINE__, "sb",
				"stream-open: ", con->physical.path);
		return -1;
	}


	/**
	 * <!--#element attribute=value attribute=value ... -->
	 *
	 * config       DONE
	 *   errmsg     -- missing
	 *   sizefmt    DONE
	 *   timefmt    DONE
	 * echo         DONE
	 *   var        DONE
	 *   encoding   -- missing
	 * exec         DONE
	 *   cgi        -- never
	 *   cmd        DONE
	 * fsize        DONE
	 *   file       DONE
	 *   virtual    DONE
	 * flastmod     DONE
	 *   file       DONE
	 *   virtual    DONE
	 * include      DONE
	 *   file       DONE
	 *   virtual    DONE
	 * printenv     DONE
	 * set          DONE
	 *   var        DONE
	 *   value      DONE
	 *
	 * if           DONE
	 * elif         DONE
	 * else         DONE
	 * endif        DONE
	 *
	 *
	 * expressions
	 * AND, OR      DONE
	 * comp         DONE
	 * ${...}       -- missing
	 * $...         DONE
	 * '...'        DONE
	 * ( ... )      DONE
	 *
	 *
	 *
	 * ** all DONE **
	 * DATE_GMT
	 *   The current date in Greenwich Mean Time.
	 * DATE_LOCAL
	 *   The current date in the local time zone.
	 * DOCUMENT_NAME
	 *   The filename (excluding directories) of the document requested by the user.
	 * DOCUMENT_URI
	 *   The (%-decoded) URL path of the document requested by the user. Note that in the case of nested include files, this is not then URL for the current document.
	 * LAST_MODIFIED
	 *   The last modification date of the document requested by the user.
	 * USER_NAME
	 *   Contains the owner of the file which included it.
	 *
	 */
#ifdef HAVE_PCRE_H
	for (i = 0; (n = pcre_exec(p->ssi_regex, NULL, s.start, s.size, i, 0, ovec, N * 3)) > 0; i = ovec[1]) {
		const char **l;
		/* take everything from last offset to current match pos */

		if (!p->if_is_false) chunkqueue_append_file(con->write_queue, con->physical.path, i, ovec[0] - i);

		pcre_get_substring_list(s.start, ovec, n, &l);
		process_ssi_stmt(srv, con, p, l, n, sce);
		pcre_free_substring_list(l);
	}

	switch(n) {
	case PCRE_ERROR_NOMATCH:
		/* copy everything/the rest */
		chunkqueue_append_file(con->write_queue, con->physical.path, i, s.size - i);

		break;
	default:
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"execution error while matching: ", n);
		break;
	}
#endif


	stream_close(&s);

	con->file_started  = 1;
	con->file_finished = 1;
	con->mode = p->id;

	if (buffer_string_is_empty(p->conf.content_type)) {
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
	} else {
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(p->conf.content_type));
	}

	{
		/* Generate "ETag" & "Last-Modified" headers */
		time_t lm_time = 0;
		buffer *mtime = NULL;

		etag_mutate(con->physical.etag, sce->etag);
		response_header_overwrite(srv, con, CONST_STR_LEN("ETag"), CONST_BUF_LEN(con->physical.etag));

		if (sce->st.st_mtime > include_file_last_mtime)
			lm_time = sce->st.st_mtime;
		else
			lm_time = include_file_last_mtime;

		mtime = strftime_cache_get(srv, lm_time);
		response_header_overwrite(srv, con, CONST_STR_LEN("Last-Modified"), CONST_BUF_LEN(mtime));
	}

	/* Reset the modified time of included files */
	include_file_last_mtime = 0;

	/* reset physical.path */
	buffer_reset(con->physical.path);

	return 0;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_ssi_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(ssi_extension);
	PATCH(content_type);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssi.extension"))) {
				PATCH(ssi_extension);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssi.content-type"))) {
				PATCH(content_type);
			}
		}
	}

	return 0;
}
#undef PATCH

URIHANDLER_FUNC(mod_ssi_physical_path) {
	plugin_data *p = p_d;
	size_t k;

	if (con->mode != DIRECT) return HANDLER_GO_ON;

	if (buffer_is_empty(con->physical.path)) return HANDLER_GO_ON;

	mod_ssi_patch_connection(srv, con, p);

	for (k = 0; k < p->conf.ssi_extension->used; k++) {
		data_string *ds = (data_string *)p->conf.ssi_extension->data[k];

		if (buffer_is_empty(ds->value)) continue;

		if (buffer_is_equal_right_len(con->physical.path, ds->value, buffer_string_length(ds->value))) {
			/* handle ssi-request */

			if (mod_ssi_handle_request(srv, con, p)) {
				/* on error */
				con->http_status = 500;
				con->mode = DIRECT;
			}

			return HANDLER_FINISHED;
		}
	}

	/* not found */
	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_ssi_plugin_init(plugin *p);
int mod_ssi_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("ssi");

	p->init        = mod_ssi_init;
	p->handle_subrequest_start = mod_ssi_physical_path;
	p->set_defaults  = mod_ssi_set_defaults;
	p->cleanup     = mod_ssi_free;

	p->data        = NULL;

	return 0;
}
