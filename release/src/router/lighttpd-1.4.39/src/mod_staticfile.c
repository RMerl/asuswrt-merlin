#include "base.h"
#include "log.h"
#include "buffer.h"

#include "plugin.h"

#include "stat_cache.h"
#include "etag.h"
#include "http_chunk.h"
#include "response.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DBE 0

/**
 * this is a staticfile for a lighttpd plugin
 *
 */



/* plugin config for all request/connections */

typedef struct {
	array *exclude_ext;
	unsigned short etags_used;
	unsigned short disable_pathinfo;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *range_buf;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_staticfile_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->range_buf = buffer_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_staticfile_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;
		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (NULL == s) continue;

			array_free(s->exclude_ext);

			free(s);
		}
		free(p->config_storage);
	}
	buffer_free(p->range_buf);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_staticfile_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "static-file.exclude-extensions", NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ "static-file.etags",    NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 1 */
		{ "static-file.disable-pathinfo", NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 2 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(plugin_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		data_config const* config = (data_config const*)srv->config_context->data[i];
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->exclude_ext    = array_init();
		s->etags_used     = 1;
		s->disable_pathinfo = 0;

		cv[0].destination = s->exclude_ext;
		cv[1].destination = &(s->etags_used);
		cv[2].destination = &(s->disable_pathinfo);

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, config->value, cv, i == 0 ? T_CONFIG_SCOPE_SERVER : T_CONFIG_SCOPE_CONNECTION)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_staticfile_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(exclude_ext);
	PATCH(etags_used);
	PATCH(disable_pathinfo);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("static-file.exclude-extensions"))) {
				PATCH(exclude_ext);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("static-file.etags"))) {
				PATCH(etags_used);
			} else if (buffer_is_equal_string(du->key, CONST_STR_LEN("static-file.disable-pathinfo"))) {
				PATCH(disable_pathinfo);
			}
		}
	}

	return 0;
}
#undef PATCH

static int http_response_parse_range(server *srv, connection *con, plugin_data *p) {
	int multipart = 0;
	int error;
	off_t start, end;
	const char *s, *minus;
	char *boundary = "fkj49sn38dcn3";
	data_string *ds;
	stat_cache_entry *sce = NULL;
	buffer *content_type = NULL;

	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, smbc_wrapper_physical_url_path(srv, con), &sce)) {
		SEGFAULT();
	}

	start = 0;
	end = sce->st.st_size - 1;

	con->response.content_length = 0;

	if (NULL != (ds = (data_string *)array_get_element(con->response.headers, "Content-Type"))) {
		content_type = ds->value;
	}
	
	for (s = con->request.http_range, error = 0;
	     !error && *s && NULL != (minus = strchr(s, '-')); ) {
		char *err;
		off_t la, le;

		if (s == minus) {
			/* -<stop> */

			le = strtoll(s, &err, 10);

			if (le == 0) {
				/* RFC 2616 - 14.35.1 */

				con->http_status = 416;
				error = 1;
			} else if (*err == '\0') {
				/* end */
				s = err;

				end = sce->st.st_size - 1;
				start = sce->st.st_size + le;
			} else if (*err == ',') {
				multipart = 1;
				s = err + 1;

				end = sce->st.st_size - 1;
				start = sce->st.st_size + le;
			} else {
				error = 1;
			}

		} else if (*(minus+1) == '\0' || *(minus+1) == ',') {
			/* <start>- */

			la = strtoll(s, &err, 10);

			if (err == minus) {
				/* ok */

				if (*(err + 1) == '\0') {
					s = err + 1;

					end = sce->st.st_size - 1;
					start = la;

				} else if (*(err + 1) == ',') {
					multipart = 1;
					s = err + 2;

					end = sce->st.st_size - 1;
					start = la;
				} else {
					error = 1;
				}
			} else {
				/* error */
				error = 1;
			}
		} else {
			/* <start>-<stop> */

			la = strtoll(s, &err, 10);

			if (err == minus) {
				le = strtoll(minus+1, &err, 10);

				/* RFC 2616 - 14.35.1 */
				if (la > le) {
					error = 1;
				}

				if (*err == '\0') {
					/* ok, end*/
					s = err;

					end = le;
					start = la;
				} else if (*err == ',') {
					multipart = 1;
					s = err + 1;

					end = le;
					start = la;
				} else {
					/* error */

					error = 1;
				}
			} else {
				/* error */

				error = 1;
			}
		}

		if (!error) {
			if (start < 0) start = 0;

			/* RFC 2616 - 14.35.1 */
			if (end > sce->st.st_size - 1) end = sce->st.st_size - 1;

			if (start > sce->st.st_size - 1) {
				error = 1;

				con->http_status = 416;
			}
		}

		if (!error) {
			if (multipart) {
				/* write boundary-header */
				buffer *b = buffer_init();

				buffer_copy_string_len(b, CONST_STR_LEN("\r\n--"));
				buffer_append_string(b, boundary);

				/* write Content-Range */
				buffer_append_string_len(b, CONST_STR_LEN("\r\nContent-Range: bytes "));
				buffer_append_int(b, start);
				buffer_append_string_len(b, CONST_STR_LEN("-"));
				buffer_append_int(b, end);
				buffer_append_string_len(b, CONST_STR_LEN("/"));
				buffer_append_int(b, sce->st.st_size);

				buffer_append_string_len(b, CONST_STR_LEN("\r\nContent-Type: "));
				buffer_append_string_buffer(b, content_type);

				/* write END-OF-HEADER */
				buffer_append_string_len(b, CONST_STR_LEN("\r\n\r\n"));

				con->response.content_length += buffer_string_length(b);
				chunkqueue_append_buffer(con->write_queue, b);
				buffer_free(b);
			}

			//- Sungmin add			
			if(con->mode == SMB_BASIC || con->mode == SMB_NTLM){
				chunkqueue_append_smb_file(con->write_queue, con->url.path, start, end - start + 1);
			}
			else{
				chunkqueue_append_file(con->write_queue, con->physical.path, start, end - start + 1);
			}
			
			con->response.content_length += end - start + 1;
		}
	}

	/* something went wrong */
	if (error) return -1;

	if (multipart) {
		/* add boundary end */
		buffer *b = buffer_init();

		buffer_copy_string_len(b, "\r\n--", 4);
		buffer_append_string(b, boundary);
		buffer_append_string_len(b, "--\r\n", 4);

		con->response.content_length += buffer_string_length(b);
		chunkqueue_append_buffer(con->write_queue, b);
		buffer_free(b);

		/* set header-fields */

		buffer_copy_string_len(p->range_buf, CONST_STR_LEN("multipart/byteranges; boundary="));
		buffer_append_string(p->range_buf, boundary);

		/* overwrite content-type */
		response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(p->range_buf));
	} else {
		/* add Content-Range-header */

		buffer_copy_string_len(p->range_buf, CONST_STR_LEN("bytes "));
		buffer_append_int(p->range_buf, start);
		buffer_append_string_len(p->range_buf, CONST_STR_LEN("-"));
		buffer_append_int(p->range_buf, end);
		buffer_append_string_len(p->range_buf, CONST_STR_LEN("/"));
		buffer_append_int(p->range_buf, sce->st.st_size);

		response_header_insert(srv, con, CONST_STR_LEN("Content-Range"), CONST_BUF_LEN(p->range_buf));
	}

	/* ok, the file is set-up */
	return 0;
}

URIHANDLER_FUNC(mod_staticfile_subrequest) {
	plugin_data *p = p_d;
	size_t k;
	stat_cache_entry *sce = NULL;
	buffer *mtime = NULL;
	data_string *ds;
	int allow_caching = 1;
	
	Cdbg(DBE, "enter..status=[%d], uri=[%s], path=[%s], mode=[%d]", con->http_status, con->uri.path->ptr, con->physical.path->ptr, con->mode);

	/* someone else has done a decision for us */
	if (con->http_status != 0) return HANDLER_GO_ON;
	if (buffer_is_empty(con->uri.path)) return HANDLER_GO_ON;
	if (buffer_is_empty(con->physical.path)) return HANDLER_GO_ON;

	/* someone else has handled this request */
	if (con->mode != DIRECT && con->mode != SMB_BASIC && con->mode != SMB_NTLM) return HANDLER_GO_ON;

	/* we only handle GET, POST and HEAD */
	switch(con->request.http_method) {
	case HTTP_METHOD_GET:
	case HTTP_METHOD_POST:
	case HTTP_METHOD_HEAD:
		break;
	default:
		return HANDLER_GO_ON;
	}

	mod_staticfile_patch_connection(srv, con, p);

	if (p->conf.disable_pathinfo && !buffer_string_is_empty(con->request.pathinfo)) {
		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- NOT handling file as static file, pathinfo forbidden");
		}
		return HANDLER_GO_ON;
	}

	/* ignore certain extensions */
	for (k = 0; k < p->conf.exclude_ext->used; k++) {
		ds = (data_string *)p->conf.exclude_ext->data[k];

		if (buffer_is_empty(ds->value)) continue;

		if (buffer_is_equal_right_len(con->physical.path, ds->value, buffer_string_length(ds->value))) {
			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__,  "s",  "-- NOT handling file as static file, extension forbidden");
			}
			return HANDLER_GO_ON;
		}
	}


	if (con->conf.log_request_handling) {
		log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling file as static file");
	}
	
	if (HANDLER_ERROR == stat_cache_get_entry(srv, con, smbc_wrapper_physical_url_path(srv, con), &sce)) {
		con->http_status = 403;

		log_error_write(srv, __FILE__, __LINE__, "sbsb",
				"not a regular file:", con->uri.path,
				"->", con->physical.path);

		return HANDLER_FINISHED;
	}

	/* we only handline regular files */
#ifdef HAVE_LSTAT
	if ((sce->is_symlink == 1) && !con->conf.follow_symlink) {
		con->http_status = 403;

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- access denied due symlink restriction");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
		}

		buffer_reset(con->physical.path);
		return HANDLER_FINISHED;
	}
#endif
	if (!S_ISREG(sce->st.st_mode)) {
		con->http_status = 404;

		if (con->conf.log_file_not_found) {
			log_error_write(srv, __FILE__, __LINE__, "sbsb",
					"not a regular file:", con->uri.path,
					"->", sce->name);
		}

		return HANDLER_FINISHED;
	}

	/* mod_compress might set several data directly, don't overwrite them */

	/* set response content-type, if not set already */

	if (NULL == array_get_element(con->response.headers, "Content-Type")) {
		if (buffer_string_is_empty(sce->content_type)) {
			/* we are setting application/octet-stream, but also announce that
			 * this header field might change in the seconds few requests 
			 *
			 * This should fix the aggressive caching of FF and the script download
			 * seen by the first installations
			 */
			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("application/octet-stream"));

			allow_caching = 0;
		} else {
			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
		}
	}

	if (con->conf.range_requests) {
		response_header_overwrite(srv, con, CONST_STR_LEN("Accept-Ranges"), CONST_STR_LEN("bytes"));
	}

	if (allow_caching) {
		if (p->conf.etags_used && con->etag_flags != 0 && !buffer_string_is_empty(sce->etag)) {
			if (NULL == array_get_element(con->response.headers, "ETag")) {
				/* generate e-tag */
				etag_mutate(con->physical.etag, sce->etag);

				response_header_overwrite(srv, con, CONST_STR_LEN("ETag"), CONST_BUF_LEN(con->physical.etag));
			}
		}

		/* prepare header */
		if (NULL == (ds = (data_string *)array_get_element(con->response.headers, "Last-Modified"))) {
			mtime = strftime_cache_get(srv, sce->st.st_mtime);
			response_header_overwrite(srv, con, CONST_STR_LEN("Last-Modified"), CONST_BUF_LEN(mtime));
		} else {
			mtime = ds->value;
		}

		if (HANDLER_FINISHED == http_response_handle_cachable(srv, con, mtime)) {
			return HANDLER_FINISHED;
		}
	}

	if (con->request.http_range && con->conf.range_requests) {
		int do_range_request = 1;
		/* check if we have a conditional GET */

		if (NULL != (ds = (data_string *)array_get_element(con->request.headers, "If-Range"))) {
			/* if the value is the same as our ETag, we do a Range-request,
			 * otherwise a full 200 */

			if (ds->value->ptr[0] == '"') {
				/**
				 * client wants a ETag
				 */
				if (!con->physical.etag) {
					do_range_request = 0;
				} else if (!buffer_is_equal(ds->value, con->physical.etag)) {
					do_range_request = 0;
				}
			} else if (!mtime) {
				/**
				 * we don't have a Last-Modified and can match the If-Range: 
				 *
				 * sending all
				 */
				do_range_request = 0;
			} else if (!buffer_is_equal(ds->value, mtime)) {
				do_range_request = 0;
			}
		}

		if (do_range_request) {
			/* content prepared, I'm done */
			con->file_finished = 1;

			if (0 == http_response_parse_range(srv, con, p)) {
				con->http_status = 206;
			}
			return HANDLER_FINISHED;
		}
	}

	/* if we are still here, prepare body */

	/* we add it here for all requests
	 * the HEAD request will drop it afterwards again
	 */
 	//- Sungmin add
 	off_t offset = 0;
 	off_t file_size = sce->st.st_size;
	if(con->mode == SMB_BASIC || con->mode == SMB_NTLM){
		http_chunk_append_smb_file(srv, con, con->url.path, offset, file_size);
	}
	else{
		http_chunk_append_file(srv, con, con->physical.path, offset, file_size);
	}

	con->http_status = 200;
	con->file_finished = 1;

	return HANDLER_FINISHED;
}

/* this function is called at dlopen() time and inits the callbacks */
//#ifndef APP_IPKG
int mod_staticfile_plugin_init(plugin *p);
int mod_staticfile_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("staticfile");

	p->init        = mod_staticfile_init;
	p->handle_subrequest_start = mod_staticfile_subrequest;
	p->set_defaults  = mod_staticfile_set_defaults;
	p->cleanup     = mod_staticfile_free;

	p->data        = NULL;

	return 0;
}
//#else
/*
int aicloud_mod_staticfile_plugin_init(plugin *p);
int aicloud_mod_staticfile_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("staticfile");

	p->init        = mod_staticfile_init;
	p->handle_subrequest_start = mod_staticfile_subrequest;
	p->set_defaults  = mod_staticfile_set_defaults;
	p->cleanup     = mod_staticfile_free;

	p->data        = NULL;

	return 0;
}*/
//#endif
