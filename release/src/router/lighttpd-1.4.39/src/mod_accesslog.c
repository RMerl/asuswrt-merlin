#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "inet_ntop_cache.h"

#include "sys-socket.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <stdio.h>

#ifdef HAVE_SYSLOG_H
# include <syslog.h>
#endif

typedef struct {
	char key;
	enum {
		FORMAT_UNSET,
			FORMAT_UNSUPPORTED,
			FORMAT_PERCENT,
			FORMAT_REMOTE_HOST,
			FORMAT_REMOTE_IDENT,
			FORMAT_REMOTE_USER,
			FORMAT_TIMESTAMP,
			FORMAT_REQUEST_LINE,
			FORMAT_STATUS,
			FORMAT_BYTES_OUT_NO_HEADER,
			FORMAT_HEADER,

			FORMAT_REMOTE_ADDR,
			FORMAT_LOCAL_ADDR,
			FORMAT_COOKIE,
			FORMAT_TIME_USED_MS,
			FORMAT_ENV,
			FORMAT_FILENAME,
			FORMAT_REQUEST_PROTOCOL,
			FORMAT_REQUEST_METHOD,
			FORMAT_SERVER_PORT,
			FORMAT_QUERY_STRING,
			FORMAT_TIME_USED,
			FORMAT_URL,
			FORMAT_SERVER_NAME,
			FORMAT_HTTP_HOST,
			FORMAT_CONNECTION_STATUS,
			FORMAT_BYTES_IN,
			FORMAT_BYTES_OUT,

			FORMAT_RESPONSE_HEADER
	} type;
} format_mapping;

/**
 *
 *
 * "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\""
 *
 */

static const format_mapping fmap[] =
{
	{ '%', FORMAT_PERCENT },
	{ 'h', FORMAT_REMOTE_HOST },
	{ 'l', FORMAT_REMOTE_IDENT },
	{ 'u', FORMAT_REMOTE_USER },
	{ 't', FORMAT_TIMESTAMP },
	{ 'r', FORMAT_REQUEST_LINE },
	{ 's', FORMAT_STATUS },
	{ 'b', FORMAT_BYTES_OUT_NO_HEADER },
	{ 'i', FORMAT_HEADER },

	{ 'a', FORMAT_REMOTE_ADDR },
	{ 'A', FORMAT_LOCAL_ADDR },
	{ 'B', FORMAT_BYTES_OUT_NO_HEADER },
	{ 'C', FORMAT_COOKIE },
	{ 'D', FORMAT_TIME_USED_MS },
	{ 'e', FORMAT_ENV },
	{ 'f', FORMAT_FILENAME },
	{ 'H', FORMAT_REQUEST_PROTOCOL },
	{ 'm', FORMAT_REQUEST_METHOD },
	{ 'n', FORMAT_UNSUPPORTED }, /* we have no notes */
	{ 'p', FORMAT_SERVER_PORT },
	{ 'P', FORMAT_UNSUPPORTED }, /* we are only one process */
	{ 'q', FORMAT_QUERY_STRING },
	{ 'T', FORMAT_TIME_USED },
	{ 'U', FORMAT_URL }, /* w/o querystring */
	{ 'v', FORMAT_SERVER_NAME },
	{ 'V', FORMAT_HTTP_HOST },
	{ 'X', FORMAT_CONNECTION_STATUS },
	{ 'I', FORMAT_BYTES_IN },
	{ 'O', FORMAT_BYTES_OUT },

	{ 'o', FORMAT_RESPONSE_HEADER },

	{ '\0', FORMAT_UNSET }
};


typedef struct {
	enum { FIELD_UNSET, FIELD_STRING, FIELD_FORMAT } type;

	buffer *string;
	int field;
} format_field;

typedef struct {
	format_field **ptr;

	size_t used;
	size_t size;
} format_fields;

typedef struct {
	buffer *access_logfile;
	int    log_access_fd;
	buffer *access_logbuffer; /* each logfile has a separate buffer */

	unsigned short use_syslog; /* syslog has global buffer */
	unsigned short syslog_level;

	buffer *format;

	time_t last_generated_accesslog_ts;
	time_t *last_generated_accesslog_ts_ptr;

	buffer *ts_accesslog_str;
	buffer *ts_accesslog_fmt_str;
	unsigned short append_tz_offset;

	format_fields *parsed_format;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	plugin_config **config_storage;
	plugin_config conf;

	buffer *syslog_logbuffer; /* syslog has global buffer. no caching, always written directly */
} plugin_data;

INIT_FUNC(mod_accesslog_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));
	p->syslog_logbuffer = buffer_init();

	return p;
}

static void accesslog_write_all(server *srv, const buffer *filename, int fd, const void* buf, size_t count) {
	if (-1 == write_all(fd, buf, count)) {
		log_error_write(srv, __FILE__, __LINE__, "sbs",
			"writing access log entry failed:", filename, strerror(errno));
	}
}

static void accesslog_append_escaped(buffer *dest, buffer *str) {
	char *ptr, *start, *end;

	/* replaces non-printable chars with \xHH where HH is the hex representation of the byte */
	/* exceptions: " => \", \ => \\, whitespace chars => \n \t etc. */
	if (buffer_string_is_empty(str)) return;
	buffer_string_prepare_append(dest, buffer_string_length(str));

	for (ptr = start = str->ptr, end = str->ptr + buffer_string_length(str); ptr < end; ptr++) {
		unsigned char const c = (unsigned char) *ptr;
		if (c >= ' ' && c <= '~' && c != '"' && c != '\\') {
			/* nothing to change, add later as one block */
		} else {
			/* copy previous part */
			if (start < ptr) {
				buffer_append_string_len(dest, start, ptr - start);
			}
			start = ptr + 1;

			switch (c) {
			case '"':
				BUFFER_APPEND_STRING_CONST(dest, "\\\"");
				break;
			case '\\':
				BUFFER_APPEND_STRING_CONST(dest, "\\\\");
				break;
			case '\b':
				BUFFER_APPEND_STRING_CONST(dest, "\\b");
				break;
			case '\n':
				BUFFER_APPEND_STRING_CONST(dest, "\\n");
				break;
			case '\r':
				BUFFER_APPEND_STRING_CONST(dest, "\\r");
				break;
			case '\t':
				BUFFER_APPEND_STRING_CONST(dest, "\\t");
				break;
			case '\v':
				BUFFER_APPEND_STRING_CONST(dest, "\\v");
				break;
			default: {
					/* non printable char => \xHH */
					char hh[5] = {'\\','x',0,0,0};
					char h = c / 16;
					hh[2] = (h > 9) ? (h - 10 + 'A') : (h + '0');
					h = c % 16;
					hh[3] = (h > 9) ? (h - 10 + 'A') : (h + '0');
					buffer_append_string_len(dest, &hh[0], 4);
				}
				break;
			}
		}
	}

	if (start < end) {
		buffer_append_string_len(dest, start, end - start);
	}
}

static int accesslog_parse_format(server *srv, format_fields *fields, buffer *format) {
	size_t i, j, k = 0, start = 0;

	if (buffer_is_empty(format)) return -1;

	for (i = 0; i < buffer_string_length(format); i++) {
		switch(format->ptr[i]) {
		case '%':
			if (i > 0 && start != i) {
				/* copy the string before this % */
				if (fields->size == 0) {
					fields->size = 16;
					fields->used = 0;
					fields->ptr = malloc(fields->size * sizeof(format_field * ));
				} else if (fields->used == fields->size) {
					fields->size += 16;
					fields->ptr = realloc(fields->ptr, fields->size * sizeof(format_field * ));
				}

				fields->ptr[fields->used] = malloc(sizeof(format_field));
				fields->ptr[fields->used]->type = FIELD_STRING;
				fields->ptr[fields->used]->string = buffer_init();

				buffer_copy_string_len(fields->ptr[fields->used]->string, format->ptr + start, i - start);

				fields->used++;
			}

			/* we need a new field */

			if (fields->size == 0) {
				fields->size = 16;
				fields->used = 0;
				fields->ptr = malloc(fields->size * sizeof(format_field * ));
			} else if (fields->used == fields->size) {
				fields->size += 16;
				fields->ptr = realloc(fields->ptr, fields->size * sizeof(format_field * ));
			}

			/* search for the terminating command */
			switch (format->ptr[i+1]) {
			case '>':
			case '<':
				/* after the } has to be a character */
				if (format->ptr[i+2] == '\0') {
					log_error_write(srv, __FILE__, __LINE__, "s", "%< and %> have to be followed by a format-specifier");
					return -1;
				}


				for (j = 0; fmap[j].key != '\0'; j++) {
					if (fmap[j].key != format->ptr[i+2]) continue;

					/* found key */

					fields->ptr[fields->used] = malloc(sizeof(format_field));
					fields->ptr[fields->used]->type = FIELD_FORMAT;
					fields->ptr[fields->used]->field = fmap[j].type;
					fields->ptr[fields->used]->string = NULL;

					fields->used++;

					break;
				}

				if (fmap[j].key == '\0') {
					log_error_write(srv, __FILE__, __LINE__, "s", "%< and %> have to be followed by a valid format-specifier");
					return -1;
				}

				start = i + 3;
				i = start - 1; /* skip the string */

				break;
			case '{':
				/* go forward to } */

				for (k = i+2; k < buffer_string_length(format); k++) {
					if (format->ptr[k] == '}') break;
				}

				if (k == buffer_string_length(format)) {
					log_error_write(srv, __FILE__, __LINE__, "s", "%{ has to be terminated by a }");
					return -1;
				}

				/* after the } has to be a character */
				if (format->ptr[k+1] == '\0') {
					log_error_write(srv, __FILE__, __LINE__, "s", "%{...} has to be followed by a format-specifier");
					return -1;
				}

				if (k == i + 2) {
					log_error_write(srv, __FILE__, __LINE__, "s", "%{...} has to be contain a string");
					return -1;
				}

				for (j = 0; fmap[j].key != '\0'; j++) {
					if (fmap[j].key != format->ptr[k+1]) continue;

					/* found key */

					fields->ptr[fields->used] = malloc(sizeof(format_field));
					fields->ptr[fields->used]->type = FIELD_FORMAT;
					fields->ptr[fields->used]->field = fmap[j].type;
					fields->ptr[fields->used]->string = buffer_init();

					buffer_copy_string_len(fields->ptr[fields->used]->string, format->ptr + i + 2, k - (i + 2));

					fields->used++;

					break;
				}

				if (fmap[j].key == '\0') {
					log_error_write(srv, __FILE__, __LINE__, "s", "%{...} has to be followed by a valid format-specifier");
					return -1;
				}

				start = k + 2;
				i = start - 1; /* skip the string */

				break;
			default:
				/* after the % has to be a character */
				if (format->ptr[i+1] == '\0') {
					log_error_write(srv, __FILE__, __LINE__, "s", "% has to be followed by a format-specifier");
					return -1;
				}

				for (j = 0; fmap[j].key != '\0'; j++) {
					if (fmap[j].key != format->ptr[i+1]) continue;

					/* found key */

					fields->ptr[fields->used] = malloc(sizeof(format_field));
					fields->ptr[fields->used]->type = FIELD_FORMAT;
					fields->ptr[fields->used]->field = fmap[j].type;
					fields->ptr[fields->used]->string = NULL;

					fields->used++;

					break;
				}

				if (fmap[j].key == '\0') {
					log_error_write(srv, __FILE__, __LINE__, "s", "% has to be followed by a valid format-specifier");
					return -1;
				}

				start = i + 2;
				i = start - 1; /* skip the string */

				break;
			}

			break;
		}
	}

	if (start < i) {
		/* copy the string */
		if (fields->size == 0) {
			fields->size = 16;
			fields->used = 0;
			fields->ptr = malloc(fields->size * sizeof(format_field * ));
		} else if (fields->used == fields->size) {
			fields->size += 16;
			fields->ptr = realloc(fields->ptr, fields->size * sizeof(format_field * ));
		}

		fields->ptr[fields->used] = malloc(sizeof(format_field));
		fields->ptr[fields->used]->type = FIELD_STRING;
		fields->ptr[fields->used]->string = buffer_init();

		buffer_copy_string_len(fields->ptr[fields->used]->string, format->ptr + start, i - start);

		fields->used++;
	}

	return 0;
}

FREE_FUNC(mod_accesslog_free) {
	plugin_data *p = p_d;
	size_t i;

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			if (!buffer_string_is_empty(s->access_logbuffer)) {
				if (s->log_access_fd != -1) {
					accesslog_write_all(srv, s->access_logfile, s->log_access_fd, CONST_BUF_LEN(s->access_logbuffer));
				}
			}

			if (s->log_access_fd != -1) close(s->log_access_fd);

			buffer_free(s->ts_accesslog_str);
			buffer_free(s->ts_accesslog_fmt_str);
			buffer_free(s->access_logbuffer);
			buffer_free(s->format);
			buffer_free(s->access_logfile);

			if (s->parsed_format) {
				size_t j;
				for (j = 0; j < s->parsed_format->used; j++) {
					if (s->parsed_format->ptr[j]->string) buffer_free(s->parsed_format->ptr[j]->string);
					free(s->parsed_format->ptr[j]);
				}
				free(s->parsed_format->ptr);
				free(s->parsed_format);
			}

			free(s);
		}

		free(p->config_storage);
	}

	if (p->syslog_logbuffer) buffer_free(p->syslog_logbuffer);
	free(p);

	return HANDLER_GO_ON;
}

SETDEFAULTS_FUNC(log_access_open) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "accesslog.filename",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "accesslog.use-syslog",           NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION },
		{ "accesslog.format",               NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },
		{ "accesslog.syslog-level",         NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },
		{ NULL,                             NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->access_logfile = buffer_init();
		s->format = buffer_init();
		s->access_logbuffer = buffer_init();
		s->ts_accesslog_str = buffer_init();
		s->ts_accesslog_fmt_str = buffer_init();
		s->log_access_fd = -1;
		s->last_generated_accesslog_ts = 0;
		s->last_generated_accesslog_ts_ptr = &(s->last_generated_accesslog_ts);
		s->syslog_level = LOG_INFO;


		cv[0].destination = s->access_logfile;
		cv[1].destination = &(s->use_syslog);
		cv[2].destination = s->format;
		cv[3].destination = &(s->syslog_level);

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}

		if (i == 0 && buffer_string_is_empty(s->format)) {
			/* set a default logfile string */

			buffer_copy_string_len(s->format, CONST_STR_LEN("%h %V %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\""));
		}

		/* parse */

		if (!buffer_is_empty(s->format)) {
			size_t j, count;

			s->parsed_format = calloc(1, sizeof(*(s->parsed_format)));

			if (-1 == accesslog_parse_format(srv, s->parsed_format, s->format)) {

				log_error_write(srv, __FILE__, __LINE__, "sb",
						"parsing accesslog-definition failed:", s->format);

				return HANDLER_ERROR;
			}

			/* make sure they didn't try to send the timestamp in twice...
			 * also, save the format string in a different variable (this
			 * will save a few conditionals later)
			 */
			count = 0;
			for (j = 0; j < s->parsed_format->used; j++) {
				if (FIELD_FORMAT == s->parsed_format->ptr[j]->type) {
					if (FORMAT_TIMESTAMP == s->parsed_format->ptr[j]->field) {
						if (!buffer_string_is_empty(s->parsed_format->ptr[j]->string)) {
							buffer_copy_string(s->ts_accesslog_fmt_str, s->parsed_format->ptr[j]->string->ptr);
						}

						if (++count > 1) {
							log_error_write(srv, __FILE__, __LINE__, "sb",
								"you may not use the timestamp twice in the same access log:", s->format);

							return HANDLER_ERROR;
						}
					}
				}
			}

#if 0
			/* debugging */
			for (j = 0; j < s->parsed_format->used; j++) {
				switch (s->parsed_format->ptr[j]->type) {
				case FIELD_FORMAT:
					log_error_write(srv, __FILE__, __LINE__, "ssds",
							"config:", "format", s->parsed_format->ptr[j]->field,
							s->parsed_format->ptr[j]->string ?
							s->parsed_format->ptr[j]->string->ptr : "" );
					break;
				case FIELD_STRING:
					log_error_write(srv, __FILE__, __LINE__, "ssbs", "config:", "string '", s->parsed_format->ptr[j]->string, "'");
					break;
				default:
					break;
				}
			}
#endif
		}

		s->append_tz_offset = 0;
		if (buffer_string_is_empty(s->ts_accesslog_fmt_str)) {
#if defined(HAVE_STRUCT_TM_GMTOFF)
			BUFFER_COPY_STRING_CONST(s->ts_accesslog_fmt_str, "[%d/%b/%Y:%H:%M:%S ");
			s->append_tz_offset = 1;
#else
			BUFFER_COPY_STRING_CONST(s->ts_accesslog_fmt_str, "[%d/%b/%Y:%H:%M:%S +0000]");
#endif
		}

		if (s->use_syslog) {
			/* ignore the next checks */
			continue;
		}

		if (buffer_string_is_empty(s->access_logfile)) continue;

		if (-1 == (s->log_access_fd = open_logfile_or_pipe(srv, s->access_logfile->ptr)))
			return HANDLER_ERROR;

	}

	return HANDLER_GO_ON;
}

SIGHUP_FUNC(log_access_cycle) {
	plugin_data *p = p_d;
	size_t i;

	if (!p->config_storage) return HANDLER_GO_ON;

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s = p->config_storage[i];

		if (!buffer_string_is_empty(s->access_logbuffer)) {
			if (s->log_access_fd != -1) {
				accesslog_write_all(srv, s->access_logfile, s->log_access_fd, CONST_BUF_LEN(s->access_logbuffer));
			}

			buffer_reset(s->access_logbuffer);
		}

		if (s->use_syslog == 0
			&& !buffer_string_is_empty(s->access_logfile)
			&& s->access_logfile->ptr[0] != '|') {

			if (-1 != s->log_access_fd) close(s->log_access_fd);

			if (-1 == (s->log_access_fd =
				   open(s->access_logfile->ptr, O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644))) {

				log_error_write(srv, __FILE__, __LINE__, "ss", "cycling access-log failed:", strerror(errno));

				return HANDLER_ERROR;
			}
			fd_close_on_exec(s->log_access_fd);
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_accesslog_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(access_logfile);
	PATCH(format);
	PATCH(log_access_fd);
	PATCH(last_generated_accesslog_ts_ptr);
	PATCH(access_logbuffer);
	PATCH(ts_accesslog_str);
	PATCH(ts_accesslog_fmt_str);
	PATCH(append_tz_offset);
	PATCH(parsed_format);
	PATCH(use_syslog);
	PATCH(syslog_level);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("accesslog.filename"))) {
				PATCH(access_logfile);
				PATCH(log_access_fd);
				PATCH(access_logbuffer);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("accesslog.format"))) {
				PATCH(format);
				PATCH(parsed_format);
				PATCH(last_generated_accesslog_ts_ptr);
				PATCH(ts_accesslog_str);
				PATCH(ts_accesslog_fmt_str);
				PATCH(append_tz_offset);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("accesslog.use-syslog"))) {
				PATCH(use_syslog);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("accesslog.syslog-level"))) {
				PATCH(syslog_level);
			}
		}
	}

	return 0;
}
#undef PATCH

REQUESTDONE_FUNC(log_access_write) {
	plugin_data *p = p_d;
	buffer *b;
	size_t j;

	int newts = 0;
	data_string *ds;

	mod_accesslog_patch_connection(srv, con, p);

	/* No output device, nothing to do */
	if (!p->conf.use_syslog && p->conf.log_access_fd == -1) return HANDLER_GO_ON;

	if (p->conf.use_syslog) {
		b = p->syslog_logbuffer;
	} else {
		b = p->conf.access_logbuffer;
	}

	if (buffer_is_empty(b)) {
		buffer_string_set_length(b, 0);
	}

	for (j = 0; j < p->conf.parsed_format->used; j++) {
		switch(p->conf.parsed_format->ptr[j]->type) {
		case FIELD_STRING:
			buffer_append_string_buffer(b, p->conf.parsed_format->ptr[j]->string);
			break;
		case FIELD_FORMAT:
			switch(p->conf.parsed_format->ptr[j]->field) {
			case FORMAT_TIMESTAMP:

				/* cache the generated timestamp */
				if (srv->cur_ts != *(p->conf.last_generated_accesslog_ts_ptr)) {
					struct tm tm;
#if defined(HAVE_STRUCT_TM_GMTOFF)
					long scd, hrs, min;
#endif

					buffer_string_prepare_copy(p->conf.ts_accesslog_str, 255);
#if defined(HAVE_STRUCT_TM_GMTOFF)
# ifdef HAVE_LOCALTIME_R
					localtime_r(&(srv->cur_ts), &tm);
					buffer_append_strftime(p->conf.ts_accesslog_str, p->conf.ts_accesslog_fmt_str->ptr, &tm);
# else /* HAVE_LOCALTIME_R */
					buffer_append_strftime(p->conf.ts_accesslog_str, p->conf.ts_accesslog_fmt_str->ptr, localtime(&(srv->cur_ts)));
# endif /* HAVE_LOCALTIME_R */

					if (p->conf.append_tz_offset) {
						buffer_append_string_len(p->conf.ts_accesslog_str, tm.tm_gmtoff >= 0 ? "+" : "-", 1);

						scd = abs(tm.tm_gmtoff);
						hrs = scd / 3600;
						min = (scd % 3600) / 60;

						/* hours */
						if (hrs < 10) buffer_append_string_len(p->conf.ts_accesslog_str, CONST_STR_LEN("0"));
						buffer_append_int(p->conf.ts_accesslog_str, hrs);

						if (min < 10) buffer_append_string_len(p->conf.ts_accesslog_str, CONST_STR_LEN("0"));
						buffer_append_int(p->conf.ts_accesslog_str, min);
						buffer_append_string_len(p->conf.ts_accesslog_str, CONST_STR_LEN("]"));
					}
#else /* HAVE_STRUCT_TM_GMTOFF */
# ifdef HAVE_GMTIME_R
					gmtime_r(&(srv->cur_ts), &tm);
					buffer_append_strftime(p->conf.ts_accesslog_str, p->conf.ts_accesslog_fmt_str->ptr, &tm);
# else /* HAVE_GMTIME_R */
					buffer_append_strftime(p->conf.ts_accesslog_str, p->conf.ts_accesslog_fmt_str->ptr, gmtime(&(srv->cur_ts)));
# endif /* HAVE_GMTIME_R */
#endif /* HAVE_STRUCT_TM_GMTOFF */

					*(p->conf.last_generated_accesslog_ts_ptr) = srv->cur_ts;
					newts = 1;
				}

				buffer_append_string_buffer(b, p->conf.ts_accesslog_str);

				break;
			case FORMAT_REMOTE_HOST:

				/* handle inet_ntop cache */

				buffer_append_string(b, inet_ntop_cache_get_ip(srv, &(con->dst_addr)));

				break;
			case FORMAT_REMOTE_IDENT:
				/* ident */
				buffer_append_string_len(b, CONST_STR_LEN("-"));
				break;
			case FORMAT_REMOTE_USER:
				if (NULL != (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER")) && !buffer_string_is_empty(ds->value)) {
					accesslog_append_escaped(b, ds->value);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_REQUEST_LINE:
				if (!buffer_string_is_empty(con->request.request_line)) {
					accesslog_append_escaped(b, con->request.request_line);
				}
				break;
			case FORMAT_STATUS:
				buffer_append_int(b, con->http_status);
				break;

			case FORMAT_BYTES_OUT_NO_HEADER:
				if (con->bytes_written > 0) {
					buffer_append_int(b,
							    con->bytes_written - con->bytes_header <= 0 ? 0 : con->bytes_written - con->bytes_header);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_HEADER:
				if (NULL != (ds = (data_string *)array_get_element(con->request.headers, p->conf.parsed_format->ptr[j]->string->ptr))) {
					accesslog_append_escaped(b, ds->value);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_RESPONSE_HEADER:
				if (NULL != (ds = (data_string *)array_get_element(con->response.headers, p->conf.parsed_format->ptr[j]->string->ptr))) {
					accesslog_append_escaped(b, ds->value);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_ENV:
				if (NULL != (ds = (data_string *)array_get_element(con->environment, p->conf.parsed_format->ptr[j]->string->ptr))) {
					accesslog_append_escaped(b, ds->value);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_FILENAME:
				if (!buffer_string_is_empty(con->physical.path)) {
					buffer_append_string_buffer(b, con->physical.path);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_BYTES_OUT:
				if (con->bytes_written > 0) {
					buffer_append_int(b, con->bytes_written);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_BYTES_IN:
				if (con->bytes_read > 0) {
					buffer_append_int(b, con->bytes_read);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_TIME_USED:
				buffer_append_int(b, srv->cur_ts - con->request_start);
				break;
			case FORMAT_SERVER_NAME:
				if (!buffer_string_is_empty(con->server_name)) {
					buffer_append_string_buffer(b, con->server_name);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_HTTP_HOST:
				if (!buffer_string_is_empty(con->uri.authority)) {
					accesslog_append_escaped(b, con->uri.authority);
				} else {
					buffer_append_string_len(b, CONST_STR_LEN("-"));
				}
				break;
			case FORMAT_REQUEST_PROTOCOL:
				buffer_append_string_len(b,
					con->request.http_version == HTTP_VERSION_1_1 ? "HTTP/1.1" : "HTTP/1.0", 8);
				break;
			case FORMAT_REQUEST_METHOD:
				buffer_append_string(b, get_http_method_name(con->request.http_method));
				break;
			case FORMAT_PERCENT:
				buffer_append_string_len(b, CONST_STR_LEN("%"));
				break;
			case FORMAT_SERVER_PORT:
				{
					const char *colon;
					buffer *srvtoken = ((server_socket*)(con->srv_socket))->srv_token;
					if (srvtoken->ptr[0] == '[') {
						colon = strstr(srvtoken->ptr, "]:");
					} else {
						colon = strchr(srvtoken->ptr, ':');
					}
					if (colon) {
						buffer_append_string(b, colon+1);
					} else {
						buffer_append_int(b, srv->srvconf.port);
					}
				}
				break;
			case FORMAT_QUERY_STRING:
				accesslog_append_escaped(b, con->uri.query);
				break;
			case FORMAT_URL:
				accesslog_append_escaped(b, con->uri.path_raw);
				break;
			case FORMAT_CONNECTION_STATUS:
				switch(con->keep_alive) {
				case 0: buffer_append_string_len(b, CONST_STR_LEN("-")); break;
				default: buffer_append_string_len(b, CONST_STR_LEN("+")); break;
				}
				break;
			default:
				/*
				 { 'a', FORMAT_REMOTE_ADDR },
				 { 'A', FORMAT_LOCAL_ADDR },
				 { 'C', FORMAT_COOKIE },
				 { 'D', FORMAT_TIME_USED_MS },
				 */

				break;
			}
			break;
		default:
			break;
		}
	}

	buffer_append_string_len(b, CONST_STR_LEN("\n"));

	if (p->conf.use_syslog ||  /* syslog doesn't cache */
	    (!buffer_string_is_empty(p->conf.access_logfile) && p->conf.access_logfile->ptr[0] == '|') || /* pipes don't cache */
	    newts ||
	    buffer_string_length(b) >= BUFFER_MAX_REUSE_SIZE) {
		if (p->conf.use_syslog) {
#ifdef HAVE_SYSLOG_H
			if (!buffer_string_is_empty(b)) {
				/* syslog appends a \n on its own */
				buffer_string_set_length(b, buffer_string_length(b) - 1);
				syslog(p->conf.syslog_level, "%s", b->ptr);
			}
#endif
		} else if (p->conf.log_access_fd != -1) {
			accesslog_write_all(srv, p->conf.access_logfile, p->conf.log_access_fd, CONST_BUF_LEN(b));
		}
		buffer_reset(b);
	}

	return HANDLER_GO_ON;
}


int mod_accesslog_plugin_init(plugin *p);
int mod_accesslog_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("accesslog");

	p->init        = mod_accesslog_init;
	p->set_defaults= log_access_open;
	p->cleanup     = mod_accesslog_free;

	p->handle_request_done  = log_access_write;
	p->handle_sighup        = log_access_cycle;

	p->data        = NULL;

	return 0;
}
