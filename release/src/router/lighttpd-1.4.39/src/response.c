#include "response.h"
#include "keyvalue.h"
#include "log.h"
#include "stat_cache.h"
#include "chunk.h"

#include "configfile.h"
#include "connections.h"

#include "plugin.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include <stdio.h>

#include "sys-socket.h"
#include "version.h"

int http_response_write_header(server *srv, connection *con) {
	buffer *b;
	size_t i;
	int have_date = 0;
	int have_server = 0;

	b = buffer_init();

	if (con->request.http_version == HTTP_VERSION_1_1) {
		buffer_copy_string_len(b, CONST_STR_LEN("HTTP/1.1 "));
	} else {
		buffer_copy_string_len(b, CONST_STR_LEN("HTTP/1.0 "));
	}
	buffer_append_int(b, con->http_status);
	buffer_append_string_len(b, CONST_STR_LEN(" "));
	buffer_append_string(b, get_http_status_name(con->http_status));

	/* disable keep-alive if requested */
	if (con->request_count > con->conf.max_keep_alive_requests || 0 == con->conf.max_keep_alive_idle) {
		con->keep_alive = 0;
	} else {
		con->keep_alive_idle = con->conf.max_keep_alive_idle;
	}

	if (con->request.http_version != HTTP_VERSION_1_1 || con->keep_alive == 0) {
		if (con->keep_alive) {
			response_header_overwrite(srv, con, CONST_STR_LEN("Connection"), CONST_STR_LEN("keep-alive"));
		} else {
			response_header_overwrite(srv, con, CONST_STR_LEN("Connection"), CONST_STR_LEN("close"));
		}
	}

	if (con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) {
		response_header_overwrite(srv, con, CONST_STR_LEN("Transfer-Encoding"), CONST_STR_LEN("chunked"));
	}


	/* add all headers */
	for (i = 0; i < con->response.headers->used; i++) {
		data_string *ds;

		ds = (data_string *)con->response.headers->data[i];

		if (!buffer_is_empty(ds->value) && !buffer_is_empty(ds->key) &&
		    0 != strncasecmp(ds->key->ptr, CONST_STR_LEN("X-LIGHTTPD-")) &&
			0 != strncasecmp(ds->key->ptr, CONST_STR_LEN("X-Sendfile"))) {
			if (0 == strcasecmp(ds->key->ptr, "Date")) have_date = 1;
			if (0 == strcasecmp(ds->key->ptr, "Server")) have_server = 1;
			if (0 == strcasecmp(ds->key->ptr, "Content-Encoding") && 304 == con->http_status) continue;

			buffer_append_string_len(b, CONST_STR_LEN("\r\n"));
			buffer_append_string_buffer(b, ds->key);
			buffer_append_string_len(b, CONST_STR_LEN(": "));
#if 0
			/** 
			 * the value might contain newlines, encode them with at least one white-space
			 */
			buffer_append_string_encoded(b, CONST_BUF_LEN(ds->value), ENCODING_HTTP_HEADER);
#else
			buffer_append_string_buffer(b, ds->value);
#endif
		}
	}

	if (!have_date) {
		/* HTTP/1.1 requires a Date: header */
		buffer_append_string_len(b, CONST_STR_LEN("\r\nDate: "));

		/* cache the generated timestamp */
		if (srv->cur_ts != srv->last_generated_date_ts) {
			buffer_string_prepare_copy(srv->ts_date_str, 255);

			buffer_append_strftime(srv->ts_date_str, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&(srv->cur_ts)));

			srv->last_generated_date_ts = srv->cur_ts;
		}

		buffer_append_string_buffer(b, srv->ts_date_str);
	}

	if (!have_server) {
		if (buffer_is_empty(con->conf.server_tag)) {
			buffer_append_string_len(b, CONST_STR_LEN("\r\nServer: " PACKAGE_DESC));
		} else if (!buffer_string_is_empty(con->conf.server_tag)) {
			buffer_append_string_len(b, CONST_STR_LEN("\r\nServer: "));
			buffer_append_string_encoded(b, CONST_BUF_LEN(con->conf.server_tag), ENCODING_HTTP_HEADER);
		}
	}

	buffer_append_string_len(b, CONST_STR_LEN("\r\n\r\n"));

	con->bytes_header = buffer_string_length(b);

	if (con->conf.log_response_header) {
		log_error_write(srv, __FILE__, __LINE__, "sSb", "Response-Header:", "\n", b);
	}

	chunkqueue_prepend_buffer(con->write_queue, b);
	buffer_free(b);

	return 0;
}

#ifdef USE_OPENSSL
static void https_add_ssl_entries(connection *con) {
	X509 *xs;
	X509_NAME *xn;
	X509_NAME_ENTRY *xe;
	int i, nentries;

	if (
		SSL_get_verify_result(con->ssl) != X509_V_OK
		|| !(xs = SSL_get_peer_certificate(con->ssl))
	) {
		return;
	}

	xn = X509_get_subject_name(xs);
	for (i = 0, nentries = X509_NAME_entry_count(xn); i < nentries; ++i) {
		int xobjnid;
		const char * xobjsn;
		data_string *envds;

		if (!(xe = X509_NAME_get_entry(xn, i))) {
			continue;
		}
		xobjnid = OBJ_obj2nid((ASN1_OBJECT*)X509_NAME_ENTRY_get_object(xe));
		xobjsn = OBJ_nid2sn(xobjnid);
		if (!xobjsn) {
			continue;
		}

		if (NULL == (envds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
			envds = data_string_init();
		}
		buffer_copy_string_len(envds->key, CONST_STR_LEN("SSL_CLIENT_S_DN_"));
		buffer_append_string(envds->key, xobjsn);
		buffer_copy_string_len(
			envds->value,
			(const char *)xe->value->data, xe->value->length
		);
		/* pick one of the exported values as "REMOTE_USER", for example
		 * ssl.verifyclient.username   = "SSL_CLIENT_S_DN_UID" or "SSL_CLIENT_S_DN_emailAddress"
		 */
		if (buffer_is_equal(con->conf.ssl_verifyclient_username, envds->key)) {
			data_string *ds;
			if (NULL == (ds = (data_string *)array_get_element(con->environment, "REMOTE_USER"))) {
				if (NULL == (ds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
					ds = data_string_init();
				}
				buffer_copy_string(ds->key, "REMOTE_USER");
				array_insert_unique(con->environment, (data_unset *)ds);
			}
			buffer_copy_buffer(ds->value, envds->value);
		}
		array_insert_unique(con->environment, (data_unset *)envds);
	}
	if (con->conf.ssl_verifyclient_export_cert) {
		BIO *bio;
		if (NULL != (bio = BIO_new(BIO_s_mem()))) {
			data_string *envds;
			int n;

			PEM_write_bio_X509(bio, xs);
			n = BIO_pending(bio);

			if (NULL == (envds = (data_string *)array_get_unused_element(con->environment, TYPE_STRING))) {
				envds = data_string_init();
			}

			buffer_copy_string_len(envds->key, CONST_STR_LEN("SSL_CLIENT_CERT"));
			buffer_string_prepare_copy(envds->value, n);
			BIO_read(bio, envds->value->ptr, n);
			BIO_free(bio);
			buffer_commit(envds->value, n);
			array_insert_unique(con->environment, (data_unset *)envds);
		}
	}
	X509_free(xs);
}
#endif


handler_t http_response_prepare(server *srv, connection *con) {
	handler_t r;

	/* looks like someone has already done a decision */
	if ((con->mode == DIRECT || con->mode == SMB_BASIC || con->mode == SMB_NTLM) &&
	    (con->http_status != 0 && con->http_status != 200)) {
		/* remove a packets in the queue */
		if (con->file_finished == 0) {
			chunkqueue_reset(con->write_queue);
		}

		return HANDLER_FINISHED;
	}

	/* no decision yet, build conf->filename */
	if ((con->mode == DIRECT || con->mode == SMB_BASIC || con->mode == SMB_NTLM) && buffer_is_empty(con->physical.path)) {
		char *qstr;

		/* we only come here when we have the parse the full request again
		 *
		 * a HANDLER_COMEBACK from mod_rewrite and mod_fastcgi might be a
		 * problem here as mod_setenv might get called multiple times
		 *
		 * fastcgi-auth might lead to a COMEBACK too
		 * fastcgi again dead server too
		 *
		 * mod_compress might add headers twice too
		 *
		 *  */

		config_cond_cache_reset(srv, con);
		config_setup_connection(srv, con); /* Perhaps this could be removed at other places. */

		if (con->conf.log_condition_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "run condition");
		}
		config_patch_connection(srv, con, COMP_SERVER_SOCKET); /* SERVERsocket */

		/**
		 * prepare strings
		 *
		 * - uri.path_raw
		 * - uri.path (secure)
		 * - uri.query
		 *
		 */

		/**
		 * Name according to RFC 2396
		 *
		 * - scheme
		 * - authority
		 * - path
		 * - query
		 *
		 * (scheme)://(authority)(path)?(query)#fragment
		 *
		 *
		 */

		/* initial scheme value. can be overwritten for example by mod_extforward later */
		if (con->srv_socket->is_ssl) {
			buffer_copy_string_len(con->uri.scheme, CONST_STR_LEN("https"));
		} else {
			buffer_copy_string_len(con->uri.scheme, CONST_STR_LEN("http"));
		}
		buffer_copy_buffer(con->uri.authority, con->request.http_host);
		buffer_to_lower(con->uri.authority);

		config_patch_connection(srv, con, COMP_HTTP_SCHEME);    /* Scheme:      */
		config_patch_connection(srv, con, COMP_HTTP_HOST);      /* Host:        */
		config_patch_connection(srv, con, COMP_HTTP_REMOTE_IP); /* Client-IP */
		config_patch_connection(srv, con, COMP_HTTP_REFERER);   /* Referer:     */
		config_patch_connection(srv, con, COMP_HTTP_USER_AGENT);/* User-Agent:  */
		config_patch_connection(srv, con, COMP_HTTP_LANGUAGE);  /* Accept-Language:  */
		config_patch_connection(srv, con, COMP_HTTP_COOKIE);    /* Cookie:  */
		config_patch_connection(srv, con, COMP_HTTP_REQUEST_METHOD); /* REQUEST_METHOD */

		/** their might be a fragment which has to be cut away */
		if (NULL != (qstr = strchr(con->request.uri->ptr, '#'))) {
			buffer_string_set_length(con->request.uri, qstr - con->request.uri->ptr);
		}

		/** extract query string from request.uri */
		if (NULL != (qstr = strchr(con->request.uri->ptr, '?'))) {
			buffer_copy_string    (con->uri.query, qstr + 1);
			buffer_copy_string_len(con->uri.path_raw, con->request.uri->ptr, qstr - con->request.uri->ptr);
		} else {
			buffer_reset     (con->uri.query);
			buffer_copy_buffer(con->uri.path_raw, con->request.uri);
		}

		/* decode url to path
		 *
		 * - decode url-encodings  (e.g. %20 -> ' ')
		 * - remove path-modifiers (e.g. /../)
		 */

		if (con->request.http_method == HTTP_METHOD_OPTIONS &&
		    con->uri.path_raw->ptr[0] == '*' && con->uri.path_raw->ptr[1] == '\0') {
			/* OPTIONS * ... */
			buffer_copy_buffer(con->uri.path, con->uri.path_raw);
		} else {
			buffer_copy_buffer(srv->tmp_buf, con->uri.path_raw);
			buffer_urldecode_path(srv->tmp_buf);
			buffer_path_simplify(con->uri.path, srv->tmp_buf);
		}

		config_patch_connection(srv, con, COMP_HTTP_URL); /* HTTPurl */
		config_patch_connection(srv, con, COMP_HTTP_QUERY_STRING); /* HTTPqs */

#ifdef USE_OPENSSL
		if (con->srv_socket->is_ssl && con->conf.ssl_verifyclient) {
			https_add_ssl_entries(con);
		}
#endif

		/* do we have to downgrade to 1.0 ? */
		if (!con->conf.allow_http11) {
			con->request.http_version = HTTP_VERSION_1_0;
		}

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- splitting Request-URI");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Request-URI     : ", con->request.uri);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "URI-scheme      : ", con->uri.scheme);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "URI-authority   : ", con->uri.authority);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "URI-path (raw)  : ", con->uri.path_raw);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "URI-path (clean): ", con->uri.path);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "URI-query       : ", con->uri.query);
		}


		/**
		 *
		 * call plugins
		 *
		 * - based on the raw URL
		 *
		 */

		switch(r = plugins_call_handle_uri_raw(srv, con)) {
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
			return r;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sd", "handle_uri_raw: unknown return value", r);
			break;
		}

		/**
		 *
		 * call plugins
		 *
		 * - based on the clean URL
		 *
		 */

		switch(r = plugins_call_handle_uri_clean(srv, con)) {
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
			return r;
		default:
			log_error_write(srv, __FILE__, __LINE__, "");
			break;
		}

		if (con->request.http_method == HTTP_METHOD_OPTIONS &&
		    con->uri.path->ptr[0] == '*' && con->uri.path_raw->ptr[1] == '\0') {
			/* option requests are handled directly without checking of the path */

			response_header_insert(srv, con, CONST_STR_LEN("Allow"), CONST_STR_LEN("OPTIONS, GET, HEAD, POST"));

			con->http_status = 200;
			con->file_finished = 1;

			return HANDLER_FINISHED;
		}

		/***
		 *
		 * border
		 *
		 * logical filename (URI) becomes a physical filename here
		 *
		 *
		 *
		 */




		/* 1. stat()
		 * ... ISREG() -> ok, go on
		 * ... ISDIR() -> index-file -> redirect
		 *
		 * 2. pathinfo()
		 * ... ISREG()
		 *
		 * 3. -> 404
		 *
		 */

		/*
		 * SEARCH DOCUMENT ROOT
		 */

		/* set a default */

		buffer_copy_buffer(con->physical.doc_root, con->conf.document_root);
		buffer_copy_buffer(con->physical.rel_path, con->uri.path);

#if defined(__WIN32) || defined(__CYGWIN__)
		/* strip dots from the end and spaces
		 *
		 * windows/dos handle those filenames as the same file
		 *
		 * foo == foo. == foo..... == "foo...   " == "foo..  ./"
		 *
		 * This will affect in some cases PATHINFO
		 *
		 * on native windows we could prepend the filename with \\?\ to circumvent
		 * this behaviour. I have no idea how to push this through cygwin
		 *
		 * */

		if (con->physical.rel_path->used > 1) {
			buffer *b = con->physical.rel_path;
			size_t len = buffer_string_length(b);
			size_t i;

			/* strip trailing " /" or "./" once */
			if (len > 1 &&
			    b->ptr[len - 1] == '/' &&
			    (b->ptr[len - 2] == ' ' || b->ptr[len - 2] == '.')) {
				len -= 2;
			}
			/* strip all trailing " " and "." */
			while (len > 0 &&  ( ' ' == b->ptr[len-1] || '.' == b->ptr[len-1] ) ) --len;
			buffer_string_set_length(b, len);
		}
#endif

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- before doc_root");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Doc-Root     :", con->physical.doc_root);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Rel-Path     :", con->physical.rel_path);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
		}
		/* the docroot plugin should set the doc_root and might also set the physical.path
		 * for us (all vhost-plugins are supposed to set the doc_root)
		 * */
		switch(r = plugins_call_handle_docroot(srv, con)) {
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
			return r;
		default:
			log_error_write(srv, __FILE__, __LINE__, "");
			break;
		}

		/* MacOS X and Windows can't distiguish between upper and lower-case
		 *
		 * convert to lower-case
		 */
		if (con->conf.force_lowercase_filenames) {
			buffer_to_lower(con->physical.rel_path);
		}

		/* the docroot plugins might set the servername, if they don't we take http-host */
		if (buffer_string_is_empty(con->server_name)) {
			buffer_copy_buffer(con->server_name, con->uri.authority);
		}

		/**
		 * create physical filename
		 * -> physical.path = docroot + rel_path
		 *
		 */

		buffer_copy_buffer(con->physical.basedir, con->physical.doc_root);
		buffer_copy_buffer(con->physical.path, con->physical.doc_root);
		buffer_append_slash(con->physical.path);
		if (!buffer_string_is_empty(con->physical.rel_path) &&
		    con->physical.rel_path->ptr[0] == '/') {
			buffer_append_string_len(con->physical.path, con->physical.rel_path->ptr + 1, buffer_string_length(con->physical.rel_path) - 1);
		} else {
			buffer_append_string_buffer(con->physical.path, con->physical.rel_path);
		}

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- after doc_root");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Doc-Root     :", con->physical.doc_root);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Rel-Path     :", con->physical.rel_path);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
		}

		switch(r = plugins_call_handle_physical(srv, con)) {
		case HANDLER_GO_ON:
			break;
		case HANDLER_FINISHED:
		case HANDLER_COMEBACK:
		case HANDLER_WAIT_FOR_EVENT:
		case HANDLER_ERROR:
			return r;
		default:
			log_error_write(srv, __FILE__, __LINE__, "");
			break;
		}

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- logical -> physical");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Doc-Root     :", con->physical.doc_root);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Basedir      :", con->physical.basedir);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Rel-Path     :", con->physical.rel_path);
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
		}
	}

	/*
	 * Noone catched away the file from normal path of execution yet (like mod_access)
	 *
	 * Go on and check of the file exists at all
	 */
	if (con->mode == DIRECT || con->mode == SMB_BASIC || con->mode == SMB_NTLM) {
		char *slash = NULL;
		char *pathinfo = NULL;
		int found = 0;
		stat_cache_entry *sce = NULL;

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling physical path");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
		}
		
		if ( HANDLER_ERROR != stat_cache_get_entry(srv, con, smbc_wrapper_physical_url_path(srv, con), &sce)) {
			/* file exists */

			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__,  "s",  "-- file found");
				log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
			}
#ifdef HAVE_LSTAT
			if ((sce->is_symlink != 0) && !con->conf.follow_symlink) {
				con->http_status = 403;
				
				if (con->conf.log_request_handling) {
					log_error_write(srv, __FILE__, __LINE__,  "s",  "-- access denied due symlink restriction");
					log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
				}

				buffer_reset(con->physical.path);
				return HANDLER_FINISHED;
			};
#endif
			if (S_ISDIR(sce->st.st_mode)) {
				if (con->uri.path->ptr[buffer_string_length(con->uri.path) - 1] != '/') {
					/* redirect to .../ */

					http_response_redirect_to_directory(srv, con);

					return HANDLER_FINISHED;
				}
#ifdef HAVE_LSTAT
			} else if (!S_ISREG(sce->st.st_mode) && !sce->is_symlink) {
#else
			} else if (!S_ISREG(sce->st.st_mode)) {
#endif
				/* any special handling of non-reg files ?*/


			}
		} else {
			switch (errno) {
			case EACCES:
				con->http_status = 403;
				
				if (con->conf.log_request_handling) {
					log_error_write(srv, __FILE__, __LINE__,  "s",  "-- access denied");
					log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
				}

				buffer_reset(con->physical.path);
				return HANDLER_FINISHED;
			case ENAMETOOLONG:
				/* file name to be read was too long. return 404 */
			case ENOENT:
				con->http_status = 404;

				if (con->conf.log_request_handling) {
					log_error_write(srv, __FILE__, __LINE__,  "s",  "-- file not found");
					log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
				}

				buffer_reset(con->physical.path);
				return HANDLER_FINISHED;
			case ENOTDIR:
				/* PATH_INFO ! :) */
				break;
			default:
				/* we have no idea what happend. let's tell the user so. */
				con->http_status = 500;
				buffer_reset(con->physical.path);

				log_error_write(srv, __FILE__, __LINE__, "ssbsb",
						"file not found ... or so: ", strerror(errno),
						con->uri.path,
						"->", con->physical.path);

				return HANDLER_FINISHED;
			}

			/* not found, perhaps PATHINFO */

			buffer_copy_buffer(srv->tmp_buf, con->physical.path);

			do {
				if (slash) {
					buffer_copy_string_len(con->physical.path, srv->tmp_buf->ptr, slash - srv->tmp_buf->ptr);
				} else {
					buffer_copy_buffer(con->physical.path, srv->tmp_buf);
				}

				if (HANDLER_ERROR != stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
					found = S_ISREG(sce->st.st_mode);
					break;
				}

				if (pathinfo != NULL) {
					*pathinfo = '\0';
				}
				slash = strrchr(srv->tmp_buf->ptr, '/');

				if (pathinfo != NULL) {
					/* restore '/' */
					*pathinfo = '/';
				}

				if (slash) pathinfo = slash;
			} while ((found == 0) && (slash != NULL) && ((size_t)(slash - srv->tmp_buf->ptr) > (buffer_string_length(con->physical.basedir) - 1)));

			if (found == 0) {
				/* no it really doesn't exists */
				con->http_status = 404;

				if (con->conf.log_file_not_found) {
					log_error_write(srv, __FILE__, __LINE__, "sbsb",
							"file not found:", con->uri.path,
							"->", con->physical.path);
				}

				buffer_reset(con->physical.path);

				return HANDLER_FINISHED;
			}

#ifdef HAVE_LSTAT
			if ((sce->is_symlink != 0) && !con->conf.follow_symlink) {
				con->http_status = 403;
				
				if (con->conf.log_request_handling) {
					log_error_write(srv, __FILE__, __LINE__,  "s",  "-- access denied due symlink restriction");
					log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
				}

				buffer_reset(con->physical.path);
				return HANDLER_FINISHED;
			};
#endif

			/* we have a PATHINFO */
			if (pathinfo) {
				buffer_copy_string(con->request.pathinfo, pathinfo);

				/*
				 * shorten uri.path
				 */

				buffer_string_set_length(con->uri.path, buffer_string_length(con->uri.path) - strlen(pathinfo));
			}

			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__,  "s",  "-- after pathinfo check");
				log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
				log_error_write(srv, __FILE__, __LINE__,  "sb", "URI          :", con->uri.path);
				log_error_write(srv, __FILE__, __LINE__,  "sb", "Pathinfo     :", con->request.pathinfo);
			}
		}

		if (con->conf.log_request_handling) {
			log_error_write(srv, __FILE__, __LINE__,  "s",  "-- handling subrequest");
			log_error_write(srv, __FILE__, __LINE__,  "sb", "Path         :", con->physical.path);
		}

		/* call the handlers */
		switch(r = plugins_call_handle_subrequest_start(srv, con)) {
		case HANDLER_GO_ON:
			/* request was not handled */
			break;
		case HANDLER_FINISHED:
		default:
			if (con->conf.log_request_handling) {
				log_error_write(srv, __FILE__, __LINE__,  "s",  "-- subrequest finished");
			}

			/* something strange happend */
			return r;
		}

		/* if we are still here, no one wanted the file, status 403 is ok I think */

		if ((con->mode == DIRECT || con->mode == SMB_BASIC || con->mode == SMB_NTLM) && con->http_status == 0) {
			switch (con->request.http_method) {
			case HTTP_METHOD_OPTIONS:
				con->http_status = 200;
				break;
			default:
				con->http_status = 403;
			}

			return HANDLER_FINISHED;
		}

	}

	switch(r = plugins_call_handle_subrequest(srv, con)) {
	case HANDLER_GO_ON:
		/* request was not handled, looks like we are done */
		return HANDLER_FINISHED;
	case HANDLER_FINISHED:
		/* request is finished */
	default:
		/* something strange happend */
		return r;
	}

	/* can't happen */
	return HANDLER_COMEBACK;
}



