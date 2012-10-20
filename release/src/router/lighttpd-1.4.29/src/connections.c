#include "buffer.h"
#include "server.h"
#include "log.h"
#include "connections.h"
#include "fdevent.h"

#include "request.h"
#include "response.h"
#include "network.h"
#include "http_chunk.h"
#include "stat_cache.h"
#include "joblist.h"

#include "plugin.h"

#include "inet_ntop_cache.h"

#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <dlinklist.h>
#include <net/if.h>

#ifdef USE_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "sys-socket.h"

#define DBG_ENABLE_CONNECTIONS 1
#define DBE DBG_ENABLE_CONNECTIONS

typedef struct {
	PLUGIN_DATA;
	buffer *tmp_buf;
	smb_info_t *smb_info_list;
} plugin_data;

void *p_d;

static connection *connections_get_new_connection(server *srv) {
	connections *conns = srv->conns;
	size_t i;

	if (conns->size == 0) {
		conns->size = 128;
		conns->ptr = NULL;
		conns->ptr = malloc(sizeof(*conns->ptr) * conns->size);
		for (i = 0; i < conns->size; i++) {
			conns->ptr[i] = connection_init(srv);
		}
	} else if (conns->size == conns->used) {
		conns->size += 128;
		conns->ptr = realloc(conns->ptr, sizeof(*conns->ptr) * conns->size);

		for (i = conns->used; i < conns->size; i++) {
			conns->ptr[i] = connection_init(srv);
		}
	}

	connection_reset(srv, conns->ptr[conns->used]);
#if 0
	fprintf(stderr, "%s.%d: add: ", __FILE__, __LINE__);
	for (i = 0; i < conns->used + 1; i++) {
		fprintf(stderr, "%d ", conns->ptr[i]->fd);
	}
	fprintf(stderr, "\n");
#endif

	conns->ptr[conns->used]->ndx = conns->used;
	return conns->ptr[conns->used++];
}

static int connection_del(server *srv, connection *con) {
	size_t i;
	connections *conns = srv->conns;
	connection *temp;

	if (con == NULL) return -1;

	if (-1 == con->ndx) return -1;

	buffer_reset(con->uri.authority);
	buffer_reset(con->uri.path);
	buffer_reset(con->uri.query);
	buffer_reset(con->request.orig_uri);

	i = con->ndx;

	/* not last element */

	if (i != conns->used - 1) {
		temp = conns->ptr[i];
		conns->ptr[i] = conns->ptr[conns->used - 1];
		conns->ptr[conns->used - 1] = temp;

		conns->ptr[i]->ndx = i;
		conns->ptr[conns->used - 1]->ndx = -1;
	}

	conns->used--;

	con->ndx = -1;
#if 0
	fprintf(stderr, "%s.%d: del: (%d)", __FILE__, __LINE__, conns->used);
	for (i = 0; i < conns->used; i++) {
		fprintf(stderr, "%d ", conns->ptr[i]->fd);
	}
	fprintf(stderr, "\n");
#endif
	return 0;
}

int connection_close(server *srv, connection *con) {
#ifdef USE_OPENSSL
	server_socket *srv_sock = con->srv_socket;
#endif

#ifdef USE_OPENSSL
	if (srv_sock->is_ssl) {
		if (con->ssl) SSL_free(con->ssl);
		con->ssl = NULL;
	}
#endif

	fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
	fdevent_unregister(srv->ev, con->fd);
#ifdef __WIN32
	if (closesocket(con->fd)) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"(warning) close:", con->fd, strerror(errno));
	}
#else
	if (close(con->fd)) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"(warning) close:", con->fd, strerror(errno));
	}
#endif

	srv->cur_fds--;
#if 0
	log_error_write(srv, __FILE__, __LINE__, "sd",
			"closed()", con->fd);
#endif

	connection_del(srv, con);
	connection_set_state(srv, con, CON_STATE_CONNECT);

	return 0;
}

#if 0
static void dump_packet(const unsigned char *data, size_t len) {
	size_t i, j;

	if (len == 0) return;

	for (i = 0; i < len; i++) {
		if (i % 16 == 0) fprintf(stderr, "  ");

		fprintf(stderr, "%02x ", data[i]);

		if ((i + 1) % 16 == 0) {
			fprintf(stderr, "  ");
			for (j = 0; j <= i % 16; j++) {
				unsigned char c;

				if (i-15+j >= len) break;

				c = data[i-15+j];

				fprintf(stderr, "%c", c > 32 && c < 128 ? c : '.');
			}

			fprintf(stderr, "\n");
		}
	}

	if (len % 16 != 0) {
		for (j = i % 16; j < 16; j++) {
			fprintf(stderr, "   ");
		}

		fprintf(stderr, "  ");
		for (j = i & ~0xf; j < len; j++) {
			unsigned char c;

			c = data[j];
			fprintf(stderr, "%c", c > 32 && c < 128 ? c : '.');
		}
		fprintf(stderr, "\n");
	}
}
#endif

static int connection_handle_read_ssl(server *srv, connection *con) {
#ifdef USE_OPENSSL
	int r, ssl_err, len, count = 0, read_offset, toread;
	buffer *b = NULL;

	if (!con->conf.is_ssl) return -1;

	ERR_clear_error();
	do {
		if (NULL != con->read_queue->last) {
			b = con->read_queue->last->mem;
		}

		if (NULL == b || b->size - b->used < 1024) {
			b = chunkqueue_get_append_buffer(con->read_queue);
			len = SSL_pending(con->ssl);
		//	Cdbg(DBE,"len = SSL_pending =%d",len);
			if (len < 4*1024) len = 4*1024; /* always alloc >= 4k buffer */
			buffer_prepare_copy(b, len + 1);

			/* overwrite everything with 0 */
			memset(b->ptr, 0, b->size);
		}

		read_offset = (b->used > 0) ? b->used - 1 : 0;
		toread = b->size - 1 - read_offset;
	//	Cdbg(DBE,"read_offset =%d, toread=%d", read_offset, toread);

		len = SSL_read(con->ssl, b->ptr + read_offset, toread);
	//	Cdbg(DBE,"len=SSL_read=%d", len);

		if (len > 0) {
			if (b->used > 0) b->used--;
			b->used += len;
			b->ptr[b->used++] = '\0';

			con->bytes_read += len;
	//		Cdbg(DBE,"bytes_read=%lli",con->bytes_read);

			//Cdbg(1,"bytes_read=%lli",con->bytes_read);

			count += len;
		}
	} while (len == toread && count < MAX_READ_LIMIT);


	if (len < 0) {
		int oerrno = errno;
		//Cdbg(1," error is  %s",strerror(errno));
		switch ((r = SSL_get_error(con->ssl, len))) {
		   //Cdbg(1,"SSL get error =%d",r);
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			con->is_readable = 0;

			/* the manual says we have to call SSL_read with the same arguments next time.
			 * we ignore this restriction; no one has complained about it in 1.5 yet, so it probably works anyway.
			 */

			return 0;
		case SSL_ERROR_SYSCALL:
			/**
			 * man SSL_get_error()
			 *
			 * SSL_ERROR_SYSCALL
			 *   Some I/O error occurred.  The OpenSSL error queue may contain more
			 *   information on the error.  If the error queue is empty (i.e.
			 *   ERR_get_error() returns 0), ret can be used to find out more about
			 *   the error: If ret == 0, an EOF was observed that violates the
			 *   protocol.  If ret == -1, the underlying BIO reported an I/O error
			 *   (for socket I/O on Unix systems, consult errno for details).
			 *
			 */
			while((ssl_err = ERR_get_error())) {
				/* get all errors from the error-queue */
				log_error_write(srv, __FILE__, __LINE__, "sds", "SSL:",
						r, ERR_error_string(ssl_err, NULL));
			}

			switch(oerrno) {
			default:
				log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL:",
						len, r, oerrno,
						strerror(oerrno));
				break;
			}

			break;
		case SSL_ERROR_ZERO_RETURN:
			/* clean shutdown on the remote side */

			if (r == 0) {
				/* FIXME: later */
			}

			/* fall thourgh */
		default:
			while((ssl_err = ERR_get_error())) {
				switch (ERR_GET_REASON(ssl_err)) {
				case SSL_R_SSL_HANDSHAKE_FAILURE:
				case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
				case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
				case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
					if (!con->conf.log_ssl_noise) continue;
					break;
				default:
					break;
				}
				/* get all errors from the error-queue */
				log_error_write(srv, __FILE__, __LINE__, "sds", "SSL:",
				                r, ERR_error_string(ssl_err, NULL));
			}
			break;
		}

		connection_set_state(srv, con, CON_STATE_ERROR);

		return -1;
	} else if (len == 0) {
		con->is_readable = 0;
		/* the other end close the connection -> KEEP-ALIVE */

		return -2;
	} else {
		joblist_append(srv, con);
	}
	return 0;
#else
	UNUSED(srv);
	UNUSED(con);
	return -1;
#endif
}

/* 0: everything ok, -1: error, -2: con closed */
static int connection_handle_read(server *srv, connection *con) {
	int len=0;
	buffer *b;
	int toread=0, read_offset=0;

	if (con->conf.is_ssl) {
		return connection_handle_read_ssl(srv, con);
	}

	b = (NULL != con->read_queue->last) ? con->read_queue->last->mem : NULL;

	/* default size for chunks is 4kb; only use bigger chunks if FIONREAD tells
	 *  us more than 4kb is available
	 * if FIONREAD doesn't signal a big chunk we fill the previous buffer
	 *  if it has >= 1kb free
	 */
#if defined(__WIN32)
	if (NULL == b || b->size - b->used < 1024) {
		b = chunkqueue_get_append_buffer(con->read_queue);
		buffer_prepare_copy(b, 4 * 1024);
	}

	read_offset = (b->used == 0) ? 0 : b->used - 1;
	len = recv(con->fd, b->ptr + read_offset, b->size - 1 - read_offset, 0);
#else
	if (ioctl(con->fd, FIONREAD, &toread) || toread == 0 || toread <= 4*1024) {
		if (NULL == b || b->size - b->used < 1024) {
			b = chunkqueue_get_append_buffer(con->read_queue);
			buffer_prepare_copy(b, 4 * 1024);
		}
	} else {
		if (toread > MAX_READ_LIMIT) toread = MAX_READ_LIMIT;
		b = chunkqueue_get_append_buffer(con->read_queue);
		buffer_prepare_copy(b, toread + 1);
	}

	read_offset = (b->used == 0) ? 0 : b->used - 1;
	len = read(con->fd, b->ptr + read_offset, b->size - 1 - read_offset);
	//Cdbg(DBE,"len=%d, b->used=%d",len,b->size-1);
#endif

	if (len < 0) {
		con->is_readable = 0;

		if (errno == EAGAIN){ /*Cdbg(DBE,"EAGAIN return 0");*/return 0;}
		if (errno == EINTR) {
			/* we have been interrupted before we could read */
			con->is_readable = 1;
			Cdbg(DBE,"EINTR return 0");
			return 0;
		}

		if (errno != ECONNRESET) {
			/* expected for keep-alive */
			log_error_write(srv, __FILE__, __LINE__, "ssd", "connection closed - read failed: ", strerror(errno), errno);
		}
//Cdbg(DBE,"set state CON_STATE_ERROR return -1");
		connection_set_state(srv, con, CON_STATE_ERROR);

		return -1;
	} else if (len == 0) {
		con->is_readable = 0;
		/* the other end close the connection -> KEEP-ALIVE */

		/* pipelining */

		return -2;
	} else if ((size_t)len < b->size - 1) {
		/* we got less then expected, wait for the next fd-event */

		con->is_readable = 0;
	}

	if (b->used > 0) b->used--;
	b->used += len;
	b->ptr[b->used++] = '\0';

	con->bytes_read += len;
#if 0
	dump_packet(b->ptr, len);
#endif

	return 0;
}

static int connection_handle_write_prepare(server *srv, connection *con) {

	if (con->mode == DIRECT || con->mode == SMB_NTLM || con->mode == SMB_BASIC) {
		/* static files */
		switch(con->request.http_method) {
		   Cdbg(DBE,"http method= %s  , http_status=%d",connection_get_method(con->request.http_method),con->http_status);		   
		case HTTP_METHOD_GET:
		case HTTP_METHOD_POST:
		case HTTP_METHOD_HEAD:
		case HTTP_METHOD_PUT:
		case HTTP_METHOD_MKCOL:
		case HTTP_METHOD_DELETE:
		case HTTP_METHOD_COPY:
		case HTTP_METHOD_MOVE:
		case HTTP_METHOD_PROPFIND:
		case HTTP_METHOD_PROPPATCH:
		case HTTP_METHOD_LOCK:
		case HTTP_METHOD_UNLOCK:
		case HTTP_METHOD_WOL:
		case HTTP_METHOD_GSL:
		case HTTP_METHOD_LOGOUT:
		case HTTP_METHOD_GETSRVTIME:
		case HTTP_METHOD_RESCANSMBPC:		
		case HTTP_METHOD_GETROUTERMAC:
		case HTTP_METHOD_GETFIRMVER:
		case HTTP_METHOD_GETROUTERINFO:
		case HTTP_METHOD_GSLL:
		case HTTP_METHOD_REMOVESL:
		case HTTP_METHOD_GETLATESTVER:
		case HTTP_METHOD_GETDISKSPACE:
		   	//Cdbg(DBE,"http method= %s break;",connection_get_state(con->request.http_method));
			break;
		case HTTP_METHOD_OPTIONS:
			/*
			 * 400 is coming from the request-parser BEFORE uri.path is set
			 * 403 is from the response handler when noone else catched it
			 *
			 * */
			if ((!con->http_status || con->http_status == 200) && con->uri.path->used &&
			    con->uri.path->ptr[0] != '*') {
				response_header_insert(srv, con, CONST_STR_LEN("Allow"), CONST_STR_LEN("OPTIONS, GET, HEAD, POST"));

				con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
				con->parsed_response &= ~HTTP_CONTENT_LENGTH;

				con->http_status = 200;
				con->file_finished = 1;

				chunkqueue_reset(con->write_queue);
			}
			break;
		default:
			switch(con->http_status) {
			case 400: /* bad request */
			case 414: /* overload request header */
			case 505: /* unknown protocol */
			case 207: /* this was webdav */
			//case 401: /* 20120203 Jerry add */				
				break;
			default:
				con->http_status = 501;
				break;
			}
			break;
		}
	}

	if (con->http_status == 0) {
		con->http_status = 403;
	}

	switch(con->http_status) {
	case 204: /* class: header only */
	case 205:
	case 304:
		/* disable chunked encoding again as we have no body */
		con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
		con->parsed_response &= ~HTTP_CONTENT_LENGTH;
		chunkqueue_reset(con->write_queue);

		con->file_finished = 1;
		break;
	default: /* class: header + body */
		if (con->mode != DIRECT && con->mode != SMB_NTLM && con->mode != SMB_BASIC) break;

		/* only custom body for 4xx and 5xx */
		if (con->http_status < 400 || con->http_status >= 600) break;

		con->file_finished = 0;

		buffer_reset(con->physical.path);
		
		/* try to send static errorfile */
		if (!buffer_is_empty(con->conf.errorfile_prefix)) {
			stat_cache_entry *sce = NULL;

			buffer_copy_string_buffer(con->physical.path, con->conf.errorfile_prefix);
			buffer_append_long(con->physical.path, con->http_status);
			buffer_append_string_len(con->physical.path, CONST_STR_LEN(".html"));

			//- 20120531 JerryLin add
			con->mode = DIRECT;
			
			if (HANDLER_ERROR != stat_cache_get_entry(srv, con, smbc_wrapper_physical_url_path(srv, con), &sce)) {
				con->file_finished = 1;
				http_chunk_append_file(srv, con, con->physical.path, 0, sce->st.st_size);
				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
			}
		}

		if (!con->file_finished) {
			buffer *b;

			buffer_reset(con->physical.path);

			con->file_finished = 1;
			b = chunkqueue_get_append_buffer(con->write_queue);

			/* build default error-page */
			buffer_copy_string_len(b, CONST_STR_LEN(
					   "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
					   "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"
					   "         \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
					   "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
					   " <head>\n"
					   "  <title>"));
			buffer_append_long(b, con->http_status);
			buffer_append_string_len(b, CONST_STR_LEN(" - "));
			buffer_append_string(b, get_http_status_name(con->http_status));

			buffer_append_string_len(b, CONST_STR_LEN(
					     "</title>\n"
					     " </head>\n"
					     " <body>\n"
					     "  <h1>"));
			buffer_append_long(b, con->http_status);
			buffer_append_string_len(b, CONST_STR_LEN(" - "));
			buffer_append_string(b, get_http_status_name(con->http_status));

			buffer_append_string_len(b, CONST_STR_LEN("</h1>\n"
					     " </body>\n"
					     "</html>\n"
					     ));

			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
		}
		break;
	}

	if (con->file_finished) {
		/* we have all the content and chunked encoding is not used, set a content-length */
		
		if ((!(con->parsed_response & HTTP_CONTENT_LENGTH)) &&
		    (con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) == 0) {
			off_t qlen = chunkqueue_length(con->write_queue);

			/**
			 * The Content-Length header only can be sent if we have content:
			 * - HEAD doesn't have a content-body (but have a content-length)
			 * - 1xx, 204 and 304 don't have a content-body (RFC 2616 Section 4.3)
			 *
			 * Otherwise generate a Content-Length header as chunked encoding is not 
			 * available
			 */
			if ((con->http_status >= 100 && con->http_status < 200) ||
			    con->http_status == 204 ||
			    con->http_status == 304) {
				data_string *ds;
				/* no Content-Body, no Content-Length */
				if (NULL != (ds = (data_string*) array_get_element(con->response.headers, "Content-Length"))) {
					buffer_reset(ds->value); /* Headers with empty values are ignored for output */
				}
			} else if (qlen > 0 || con->request.http_method != HTTP_METHOD_HEAD) {
				/* qlen = 0 is important for Redirects (301, ...) as they MAY have
				 * a content. Browsers are waiting for a Content otherwise
				 */
				buffer_copy_off_t(srv->tmp_buf, qlen);

				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Length"), CONST_BUF_LEN(srv->tmp_buf));
			}
		}
	} else {
		/**
		 * the file isn't finished yet, but we have all headers
		 *
		 * to get keep-alive we either need:
		 * - Content-Length: ... (HTTP/1.0 and HTTP/1.0) or
		 * - Transfer-Encoding: chunked (HTTP/1.1)
		 */

		if (((con->parsed_response & HTTP_CONTENT_LENGTH) == 0) &&
		    ((con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) == 0)) {
			con->keep_alive = 0;
		}

		/**
		 * if the backend sent a Connection: close, follow the wish
		 *
		 * NOTE: if the backend sent Connection: Keep-Alive, but no Content-Length, we
		 * will close the connection. That's fine. We can always decide the close 
		 * the connection
		 *
		 * FIXME: to be nice we should remove the Connection: ... 
		 */
		if (con->parsed_response & HTTP_CONNECTION) {
			/* a subrequest disable keep-alive although the client wanted it */
			if (con->keep_alive && !con->response.keep_alive) {
				con->keep_alive = 0;
			}
		}
	}

	if (con->request.http_method == HTTP_METHOD_HEAD) {
		/**
		 * a HEAD request has the same as a GET 
		 * without the content
		 */
		con->file_finished = 1;

		chunkqueue_reset(con->write_queue);
		con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
	}

	http_response_write_header(srv, con);
//Cdbg(DBE, "leave");
	return 0;
}

static int connection_handle_write(server *srv, connection *con) {
//Cdbg(DBE, "enter %s", con->url.path->ptr);	
	switch(network_write_chunkqueue(srv, con, con->write_queue)) {
	case 0:
		if (con->file_finished) {
			connection_set_state(srv, con, CON_STATE_RESPONSE_END);
			joblist_append(srv, con);
		}
		break;
	case -1: /* error on our side */
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"connection closed: write failed on fd", con->fd);
		connection_set_state(srv, con, CON_STATE_ERROR);
		joblist_append(srv, con);
		break;
	case -2: /* remote close */
		connection_set_state(srv, con, CON_STATE_ERROR);
		joblist_append(srv, con);
		break;
	case 1:
		con->is_writable = 0;

		/* not finished yet -> WRITE */
		break;
	}

//Cdbg(DBE, "leave");	
	return 0;
}

//- Jerry add 20111007
static void check_direct_file(server *srv, connection *con)
{
	config_values_t cv[] = {
		{ "alias.url",                  NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                        NULL, T_CONFIG_UNSET,  T_CONFIG_SCOPE_UNSET }
	};
	
	size_t i, j;
	for (i = 1; i < srv->config_context->used; i++) {
		int found = 0;
		array* alias = array_init();
		cv[0].destination = alias;
	
		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			continue;
		}
		
		for (j = 0; j < alias->used; j++) {
			data_string *ds = (data_string *)alias->data[j];			
			//Cdbg(DBE, "ds->key=%s, ds->value=%s", ds->key->ptr, ds->value->ptr);			
			//if( strcmp(ds->value->ptr, "/mnt") == 0 )
			//	continue;
			
			if( strncmp(con->request.uri->ptr, ds->key->ptr, ds->key->used-1) == 0 ){
				con->mode = DIRECT;
				found = 1;
				//Cdbg(DBE, "This is DIRECT file[%s]", con->request.uri->ptr );
				break;
			}
		}

		array_free(alias);

		if(found==1)
			break;
	}
}

static int parser_share_file(server *srv, connection *con){	
	int result=0;

	if(strncmp(con->request.uri->ptr, "/ASUSHARE", 9)==0){
		/*
		//- Get user-Agent
		data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");
		if(ds){
			Cdbg(DBE, "parser_share_file->user_agent: %s", ds->value->ptr);
		}
		*/
		char mac[20]="\0";		
		char* decode_str;
		int bExpired = 0;
		get_mac_address("eth0", &mac);
		
		char* key = ldb_base64_encode(mac, strlen(mac));		
		int y = strstr (con->request.uri->ptr+1,"/") - (con->request.uri->ptr);
		
		if(y<=9)
			return -1;
		
		buffer* filename = buffer_init();
		buffer_copy_string_len(filename, con->request.uri->ptr+y, con->request.uri->used-y);

		buffer* substrencode = buffer_init();
		buffer_copy_string_len(substrencode,con->request.uri->ptr+9,y-9);

		decode_str = x123_decode(substrencode->ptr, key, &decode_str);
		
		//Cdbg(DBE, "con->request.uri=%s", con->request.uri->ptr);
		//Cdbg(DBE, "filename=%s", filename->ptr);
		//Cdbg(DBE, "decode_str=%s", decode_str);

		int x = strstr (decode_str,"?") - decode_str;

		int param_len = strlen(decode_str) - x;
		buffer* param = buffer_init();
		buffer* user = buffer_init();
		buffer* pass = buffer_init();
		buffer* expire = buffer_init();
		buffer_copy_string_len(param, decode_str + x + 1, param_len);
		//Cdbg(DBE, "123param=[%s]", param->ptr);

		char * pch;
		pch = strtok(param->ptr, "&");
		while(pch!=NULL){
		
			if(strncmp(pch, "auth=", 5)==0){

				int len = strlen(pch)-5;				
				char* temp = (char*)malloc(len+1);
				memset(temp,'\0', len);				
				strncpy(temp, pch+5, len);
				temp[len]='\0';

				buffer_copy_string( con->share_link_basic_auth, "Basic " );
				buffer_append_string( con->share_link_basic_auth, temp );
				
				free(temp);
			}
			else if(strncmp(pch, "expire=", 7)==0){
				int len = strlen(pch)-7;				
				char* temp = (char*)malloc(len+1);
				memset(temp,'\0', len);				
				strncpy(temp, pch+7, len);
				temp[len]='\0';
				
				struct tm tm;
				time_t expire_time;
				Cdbg(DBE, "expire = %s", temp);
				if (strptime(temp, "%Y-%m-%d-%H:%M:%S", &tm) ){					
					expire_time = mktime(&tm);
					time_t cur_time = time(NULL);

					char strTime[25] = {0};
					strftime(strTime, sizeof(strTime), "%Y-%m-%d-%H:%M:%S", localtime(&cur_time));	

					double offset = difftime(expire_time, cur_time);
					//Cdbg(DBE, "expire offset = %f, strTime=%s", offset, strTime);
					if( offset < 0.0 ){
						buffer_reset(con->share_link_basic_auth);	
						bExpired = 1;
					}
				}
				else
					Cdbg(DBE, "fail to strptime");
			}
		
			pch = strtok( NULL, "&" );
		}

		
		buffer_free(param);
		buffer_free(user);
		buffer_free(pass);
		buffer_free(expire);
		free(key);
		
		buffer_reset(con->request.uri);
		
		buffer_copy_string_len(con->request.uri, decode_str, x);
		
		buffer_append_string_buffer(con->request.uri, filename);
						
		//buffer_free(substrencode);		
		buffer_free(filename);
		
		Cdbg(DBE, "end parser_share_file, con->request.uri=%s", con->request.uri->ptr);

		if(bExpired==0)
			result = 1;
		else if(bExpired==1)
			result = -1;
	}
	else if(strncmp(con->request.uri->ptr, "/AICLOUD", 8)==0){
		int bExpired = 0;
		int y = strstr (con->request.uri->ptr+1,"/") - (con->request.uri->ptr);
		
		if( y <= 8 )
			return -1;

		Cdbg(DBE, "AICLOUD sharelink");
		
		buffer* filename = buffer_init();
		buffer_copy_string_len(filename, con->request.uri->ptr+y+1, con->request.uri->used-y);
		Cdbg(DBE, "filename=%s", filename->ptr );
		
		buffer* sharepath = buffer_init();
		buffer_copy_string_len(sharepath, con->request.uri->ptr+1, y-1);

		share_link_info_t* c=NULL;

		for (c = share_link_info_list; c; c = c->next) {
			if(buffer_is_equal(c->shortpath, sharepath)){				

				time_t cur_time = time(NULL);
				double offset = difftime(c->expiretime, cur_time);					
				if( c->expiretime!=0 && offset < 0.0 ){
					buffer_reset(con->share_link_basic_auth);	
					bExpired = 1;
					free_share_link_info(c);
					DLIST_REMOVE(share_link_info_list, c);
					break;
				}
								
				buffer_copy_string( con->share_link_basic_auth, "Basic " );
				buffer_append_string_buffer( con->share_link_basic_auth, c->auth );

				buffer_copy_string_buffer( con->share_link_shortpath, c->shortpath );
				buffer_copy_string_buffer( con->share_link_filename, c->filename );
		
				Cdbg(DBE, "realpath=%s, con->request.uri=%s", c->realpath->ptr, con->request.uri->ptr);
				//Cdbg(DBE, "auth=%s", con->share_link_basic_auth->ptr);
				
				break;
			}
		}

		if(c==NULL||bExpired==1){
			buffer_free(sharepath);
			buffer_free(filename);
			return -1;
		}
		
		buffer_reset(con->request.uri);		
		buffer_copy_string_buffer(con->request.uri, c->realpath);
		buffer_append_string(con->request.uri, "/");
		//buffer_append_string_buffer(con->request.uri, c->filename);
		buffer_append_string_buffer(con->request.uri, filename);

		buffer_free(sharepath);
		buffer_free(filename);
		
		Cdbg(DBE, "end parser_share_file, con->request.uri=%s, con->mode=%d, con->share_link_basic_auth=%s", 
				con->request.uri->ptr, con->mode, con->share_link_basic_auth->ptr);
		
		log_sys_write(srv, "sbss", "Download file", c->filename, "from ip", con->dst_addr_buf->ptr);
		  
		return 1;
	}
		
	return result;
}

static void get_connection_auth_type(server *srv, connection *con)
{
	data_string *ds;
	int found = 0;
		
	check_direct_file(srv, con);

	if(con->mode==DIRECT)
		return;
	
	if (NULL == (ds = (data_string *)array_get_element(con->request.headers, "user-Agent")))
		return;
		
	config_values_t cv[] = {
		{ "smbdav.auth_ntlm",    NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};
	
	size_t i, j;
	for (i = 1; i < srv->config_context->used; i++) {
		int found = 0;
		array* auth_ntlm_array = array_init();
		cv[0].destination = auth_ntlm_array;
	
		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			continue;
		}
		
		for (j = 0; j < auth_ntlm_array->used; j++) {
			data_string *ds2 = (data_string *)auth_ntlm_array->data[j];

			if(ds2->key->used){
				/*
				Cdbg(DBE, "ds2->key=%s, ds2->value=%s", 
					ds2->key->ptr, 
					ds2->value->ptr );
				*/
				if( strncmp(ds->value->ptr, ds2->value->ptr, ds2->value->used-1) == 0 ){
					con->mode = SMB_NTLM;
					found = 1;
					break;
				}
			}
		}

		array_free(auth_ntlm_array);

		if(found==1)
			break;
	}

}

static smb_info_t *smbdav_get_smb_info_from_pool(server *srv, connection *con, plugin_data *p)
{
	smb_info_t *c;

	if(p->smb_info_list==NULL||con->mode==DIRECT)
		return NULL;
	
	//- Get user-Agent
	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");
	if(ds==NULL){
		return NULL;
	}
	
	char pWorkgroup[30]={0};
	char pServer[64]={0};
	char pShare[1280]={0};
	char pPath[1280]={0};
	
	smbc_wrapper_parse_path2(con, pWorkgroup, pServer, pShare, pPath);

	buffer* buffer_server = buffer_init();
	if(pServer[0] != '\0')
		buffer_append_string(buffer_server,pServer);
	
	buffer* buffer_share = buffer_init();
	if(pShare[0] != '\0')
		buffer_append_string(buffer_share,pShare);
	
	int count = 0;
		
	for (c = p->smb_info_list; c; c = c->next) {
		
		count++;
		
		if(!buffer_is_equal(c->server, buffer_server))
			continue;

		//Cdbg(DBE, "c->share=[%s], buffer_share=[%s]", c->share->ptr, buffer_share->ptr);
		//if(con->mode==SMB_BASIC && !buffer_is_equal(c->share, buffer_share))
		//	continue;

		//Cdbg(DBE, "%d, c->src_ip=[%s], dst_addr_buf=[%s]", count, c->src_ip->ptr, con->dst_addr_buf->ptr);
		if(!buffer_is_equal(c->src_ip, con->dst_addr_buf))
			continue;
		
		//Cdbg(DBE, "%d, c->user_agent=[%s], user_agent=[%s]", count, c->user_agent->ptr, ds->value->ptr);
		if(!buffer_is_equal(c->user_agent, ds->value))
			continue;

		//Cdbg(DBE, "return %d, c->server=[%s]", count, c->server->ptr);

		buffer_free(buffer_server);
		buffer_free(buffer_share);
	
		return c;
	}

	buffer_free(buffer_server);
	buffer_free(buffer_share);
	
	return NULL;
}

static void copy_smb_info(connection *con, smb_info_t *smb_info)
{
	con->smb_info = calloc(1, sizeof(smb_info_t));
	con->smb_info->username = buffer_init();
	con->smb_info->password = buffer_init();
	con->smb_info->workgroup = buffer_init();
	con->smb_info->server = buffer_init();
	con->smb_info->share = buffer_init();
	con->smb_info->path = buffer_init();
	con->smb_info->user_agent = buffer_init();
	con->smb_info->src_ip = buffer_init();
	
	buffer_copy_string_buffer(con->smb_info->username, smb_info->username);
	buffer_copy_string_buffer(con->smb_info->password, smb_info->password);
	buffer_copy_string_buffer(con->smb_info->workgroup, smb_info->workgroup);
	buffer_copy_string_buffer(con->smb_info->server, smb_info->server);
	buffer_copy_string_buffer(con->smb_info->share, smb_info->share);
	buffer_copy_string_buffer(con->smb_info->path, smb_info->path);
	buffer_copy_string_buffer(con->smb_info->user_agent, smb_info->user_agent);
	buffer_copy_string_buffer(con->smb_info->src_ip, smb_info->src_ip);
}

static int connection_smb_info_init(server *srv, connection *con, plugin_data *p) 
{
	UNUSED(srv);

	char pWorkgroup[30]={0};
	char pServer[64]={0};
	char pShare[1280]={0};
	char pPath[1280]={0};
		
	smbc_wrapper_parse_path2(con, pWorkgroup, pServer, pShare, pPath);
	
	buffer* bworkgroup = buffer_init();
	buffer* bserver = buffer_init();
	buffer* bshare = buffer_init();
	buffer* bpath = buffer_init();
	URI_QUERY_TYPE qflag = SMB_FILE_QUERY;
	
	if(pWorkgroup[0] != '\0')
		buffer_copy_string(bworkgroup, pWorkgroup);
	
	if(pServer[0] != '\0') {
		int isHost = smbc_check_connectivity(con->physical_auth_url->ptr);
		if(isHost) {
			buffer_copy_string(bserver, pServer);
		}
		else{
			buffer_free(bworkgroup);
			buffer_free(bserver);
			buffer_free(bshare);
			buffer_free(bpath);
			return 2;
		}
	} 
	else {
		if(qflag == SMB_FILE_QUERY) {
			qflag = SMB_HOST_QUERY;
		}
	}
	
	if(pServer[0] != '\0' && pShare[0] != '\0') {
		buffer_copy_string(bshare, pShare);
	} 
	else {
		if(qflag == SMB_FILE_QUERY)  {
			qflag = SMB_SHARE_QUERY;
		}
	}
	
	if(pServer[0] != '\0' && pShare[0] != '\0' && pPath[0] != '\0') {
		buffer_copy_string(bpath, pPath);
		qflag = SMB_FILE_QUERY;
	}

	data_string *ds = (data_string *)array_get_element(con->request.headers, "user-Agent");

	smb_info_t *smb_info;
	
	if( ds && 
	    ( strstr( ds->value->ptr, "Mozilla" ) || 
	      strstr( ds->value->ptr, "Opera" ) || 
	      con->mode == SMB_NTLM ) ){
	    
		//- From browser, like IE, Chrome, Firefox, Safari		
		if(smb_info = smbdav_get_smb_info_from_pool(srv, con, p)){ 
			Cdbg(DBE, "Get smb_info from pool smb_info->qflag=[%d], smb_info->user=[%s], smb_info->pass=[%s]", 
				smb_info->qflag, smb_info->username->ptr, smb_info->password->ptr);
		}
		else{
			smb_info = calloc(1, sizeof(smb_info_t));
			smb_info->username = buffer_init();
			smb_info->password = buffer_init();
			smb_info->workgroup = buffer_init();
			smb_info->server = buffer_init();
			smb_info->share = buffer_init();
			smb_info->path = buffer_init();
			smb_info->user_agent = buffer_init();
			smb_info->src_ip = buffer_init();			
			
			if(con->mode == SMB_NTLM){
				smb_info->cli = smbc_cli_initialize();
				if(!buffer_is_empty(bserver)){
					smbc_cli_connect(smb_info->cli, bserver->ptr, SMB_PORT);
				}
				smb_info->ntlmssp_state = NULL; 
				smb_info->state = NTLMSSP_INITIAL;
			}

			DLIST_ADD(p->smb_info_list, smb_info);			
		}	
		con->smb_info = smb_info;
			
	}
	else{		
		smb_info = calloc(1, sizeof(smb_info_t));
		smb_info->username = buffer_init();
		smb_info->password = buffer_init();
		smb_info->workgroup = buffer_init();
		smb_info->server = buffer_init();
		smb_info->share = buffer_init();
		smb_info->path = buffer_init();
		smb_info->user_agent = buffer_init();
		smb_info->src_ip = buffer_init();			
		
		con->smb_info = smb_info;
	}
	
	con->smb_info->auth_time = time(NULL);	
	con->smb_info->auth_right = 0;

	if(ds)
		buffer_copy_string(con->smb_info->user_agent, ds->value->ptr);
	
	con->smb_info->qflag = qflag;
	buffer_copy_string_buffer(con->smb_info->workgroup, bworkgroup);
	buffer_copy_string_buffer(con->smb_info->server, bserver);
	buffer_copy_string_buffer(con->smb_info->share, bshare);
	buffer_copy_string_buffer(con->smb_info->path, bpath);
	buffer_copy_string_buffer(con->smb_info->src_ip, con->dst_addr_buf);

	Cdbg(DBE, "con->smb_info->workgroup=[%s]", con->smb_info->workgroup->ptr);
	Cdbg(DBE, "con->smb_info->server=[%s]", con->smb_info->server->ptr);
	Cdbg(DBE, "con->smb_info->share=[%s]", con->smb_info->share->ptr);
	Cdbg(DBE, "con->smb_info->path=[%s]", con->smb_info->path->ptr);
	Cdbg(DBE, "con->smb_info->user_agent=[%s]", con->smb_info->user_agent->ptr);
	Cdbg(DBE, "con->smb_info->src_ip=[%s]", con->smb_info->src_ip->ptr);
		
	buffer_free(bworkgroup);
	buffer_free(bserver);
	buffer_free(bshare);
	buffer_free(bpath);

	return 1;
}

static void connection_smb_info_url_patch(server *srv, connection *con)
{
	char strr[2048]="\0";
	char uri[2048]="\0";
	
	UNUSED(srv);
	
	char* pch = strchr(con->request.uri->ptr,'?');
	if(pch){	
		buffer_copy_string_len(con->url_options, pch+1, strlen(pch)-1);
		int len = pch -con->request.uri->ptr;
		strncpy(uri,con->request.uri->ptr, len);
	}
	else{
		strcpy(uri,con->request.uri->ptr);
	}

	if(con->mode == DIRECT){
		sprintf(strr, "%s", uri);
	}
	else {
		if(con->smb_info&&con->smb_info->server->used) {
			if(con->mode == SMB_BASIC){
				if(con->smb_info->username->used&&con->smb_info->password->used){
					sprintf(strr, "smb://%s:%s@%s", con->smb_info->username->ptr, con->smb_info->password->ptr, uri+1);
				}
				else
					sprintf(strr, "smb://%s", uri+1);
			}
			else if(con->mode == SMB_NTLM){
				sprintf(strr, "smb://%s", uri+1);		
			}
		} else {
			sprintf(strr, "smb://");
		}
	}
	
	buffer_copy_string(con->url.path, strr);
	buffer_copy_string(con->url.rel_path, uri);
}

static int do_connection_auth(server *srv, connection *con)
{	
	plugin_data *p = p_d;
	int res = HANDLER_UNSET;

	//- init the plugin data
	if(p==NULL){
		p = calloc(1, sizeof(*p));
		p->tmp_buf = buffer_init();
		p_d = p;
	}
	
	if(con->mode == DIRECT){
		connection_smb_info_url_patch(srv, con);
		return res;
	}

	Cdbg(DBE,"***************************************");
	Cdbg(DBE,"enter do_connection_auth..con->mode = %d, con->request.uri=[%s]", con->mode, con->request.uri->ptr);
	
	config_cond_cache_reset(srv, con);
	config_patch_connection(srv, con, COMP_SERVER_SOCKET);
	config_patch_connection(srv, con, COMP_HTTP_URL);
		
	buffer_copy_string_buffer(con->physical_auth_url, con->conf.document_root);
	buffer_append_string(con->physical_auth_url, con->request.uri->ptr+1);
	
	int result = connection_smb_info_init(srv, con, p);	
	if( result == 0 ){
		return HANDLER_FINISHED;
	}
	else if( result == 2 ){
		//- Unable to complete the connection, the device is not turned on, or network problems caused!
		con->http_status = 451;
		return HANDLER_FINISHED;
	}
	
	if(con->mode == SMB_NTLM) {
		//try to get NTLM authentication information from HTTP request
		res = ntlm_authentication_handler(srv, con, p);		
	} else if(con->mode == SMB_BASIC){
		//try to get username/password from HTTP request
		res = basic_authentication_handler(srv, con, p);
	}

	//- 20120202 Jerry add
	srv->smb_srv_info_list = p->smb_info_list;	
	connection_smb_info_url_patch(srv, con);

	buffer_reset(con->physical_auth_url);
	return res;
}

connection *connection_init(server *srv) {
	connection *con;

	UNUSED(srv);

	con = calloc(1, sizeof(*con));

	con->fd = 0;
	con->ndx = -1;
	con->fde_ndx = -1;
	con->bytes_written = 0;
	con->bytes_read = 0;
	con->bytes_header = 0;
	con->loops_per_request = 0;

#define CLEAN(x) \
	con->x = buffer_init();

	CLEAN(request.uri);
	CLEAN(request.request_line);
	CLEAN(request.request);
	CLEAN(request.pathinfo);

	CLEAN(request.orig_uri);

	CLEAN(uri.scheme);
	CLEAN(uri.authority);
	CLEAN(uri.path);
	CLEAN(uri.path_raw);
	CLEAN(uri.query);

	CLEAN(physical.doc_root);
	CLEAN(physical.path);
	CLEAN(physical.basedir);
	CLEAN(physical.rel_path);
	CLEAN(physical.etag);
	
	//- Jerry add 20101012
	CLEAN(url.doc_root);
	CLEAN(url.path);
	CLEAN(url.basedir);
	CLEAN(url.rel_path);
	CLEAN(url.etag);
	CLEAN(share_link_basic_auth);
	CLEAN(share_link_shortpath);
	CLEAN(share_link_filename);
	CLEAN(physical_auth_url);
	CLEAN(url_options);
	CLEAN(aidisk_username);
	CLEAN(aidisk_passwd);
	
	CLEAN(parse_request);
	
	CLEAN(authed_user);
	CLEAN(server_name);
	CLEAN(error_handler);
	CLEAN(dst_addr_buf);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
	CLEAN(tlsext_server_name);
#endif

#undef CLEAN
	con->write_queue = chunkqueue_init();
	con->read_queue = chunkqueue_init();
	con->request_content_queue = chunkqueue_init();
	chunkqueue_set_tempdirs(con->request_content_queue, srv->srvconf.upload_tempdirs);

	con->request.headers      = array_init();
	con->response.headers     = array_init();
	con->environment     = array_init();

	/* init plugin specific connection structures */

	con->plugin_ctx = calloc(1, (srv->plugins.used + 1) * sizeof(void *));

	con->cond_cache = calloc(srv->config_context->used, sizeof(cond_cache_t));
	config_setup_connection(srv, con);
	
	return con;
}

void connections_free(server *srv) {
	connections *conns = srv->conns;
	size_t i;
	
	for (i = 0; i < conns->size; i++) {
		connection *con = conns->ptr[i];

		connection_reset(srv, con);

		chunkqueue_free(con->write_queue);
		chunkqueue_free(con->read_queue);
		chunkqueue_free(con->request_content_queue);
		array_free(con->request.headers);
		array_free(con->response.headers);
		array_free(con->environment);

#define CLEAN(x) \
	buffer_free(con->x);

		CLEAN(request.uri);
		CLEAN(request.request_line);
		CLEAN(request.request);
		CLEAN(request.pathinfo);

		CLEAN(request.orig_uri);

		CLEAN(uri.scheme);
		CLEAN(uri.authority);
		CLEAN(uri.path);
		CLEAN(uri.path_raw);
		CLEAN(uri.query);

		CLEAN(physical.doc_root);
		CLEAN(physical.path);
		CLEAN(physical.basedir);
		CLEAN(physical.etag);
		CLEAN(physical.rel_path);
		
		//- Jerry add 20101012
		CLEAN(url.doc_root);
		CLEAN(url.path);
		CLEAN(url.basedir);
		CLEAN(url.rel_path);
		CLEAN(url.etag);
		CLEAN(share_link_basic_auth);
		CLEAN(share_link_shortpath);
		CLEAN(share_link_filename);
		CLEAN(physical_auth_url);
		CLEAN(url_options);
		CLEAN(aidisk_username);
		CLEAN(aidisk_passwd);
	
		CLEAN(parse_request);
		
		CLEAN(authed_user);
		CLEAN(server_name);
		CLEAN(error_handler);
		CLEAN(dst_addr_buf);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
		CLEAN(tlsext_server_name);
#endif
#undef CLEAN
		free(con->plugin_ctx);
		free(con->cond_cache);

		free(con);
	}

	free(conns->ptr);
}


int connection_reset(server *srv, connection *con) {
	size_t i;

	plugins_call_connection_reset(srv, con);

	con->is_readable = 1;
	con->is_writable = 1;
	con->http_status = 0;
	con->file_finished = 0;
	con->file_started = 0;
	con->got_response = 0;

	con->parsed_response = 0;

	con->bytes_written = 0;
	con->bytes_written_cur_second = 0;
	con->bytes_read = 0;
	con->bytes_header = 0;
	con->loops_per_request = 0;

	con->request.http_method = HTTP_METHOD_UNSET;
	con->request.http_version = HTTP_VERSION_UNSET;

	con->request.http_if_modified_since = NULL;
	con->request.http_if_none_match = NULL;

	con->response.keep_alive = 0;
	con->response.content_length = -1;
	con->response.transfer_encoding = 0;

	con->mode = DIRECT;

#define CLEAN(x) \
	if (con->x) buffer_reset(con->x);

	CLEAN(request.uri);
	CLEAN(request.request_line);
	CLEAN(request.pathinfo);
	CLEAN(request.request);

	/* CLEAN(request.orig_uri); */

	CLEAN(uri.scheme);
	/* CLEAN(uri.authority); */
	/* CLEAN(uri.path); */
	CLEAN(uri.path_raw);
	/* CLEAN(uri.query); */

	CLEAN(physical.doc_root);
	CLEAN(physical.path);
	CLEAN(physical.basedir);
	CLEAN(physical.rel_path);
	CLEAN(physical.etag);

	//- Jerry add 20101012
	CLEAN(url.doc_root);
	CLEAN(url.path);
	CLEAN(url.basedir);
	CLEAN(url.rel_path);
	CLEAN(url.etag);
	CLEAN(share_link_basic_auth);
	CLEAN(share_link_shortpath);
	CLEAN(share_link_filename);
	CLEAN(physical_auth_url);
	CLEAN(url_options);
	CLEAN(aidisk_username);
	CLEAN(aidisk_passwd);
	
	CLEAN(parse_request);
	
	CLEAN(authed_user);
	CLEAN(server_name);
	CLEAN(error_handler);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
	CLEAN(tlsext_server_name);
#endif
#undef CLEAN

#define CLEAN(x) \
	if (con->x) con->x->used = 0;

#undef CLEAN

#define CLEAN(x) \
		con->request.x = NULL;

	CLEAN(http_host);
	CLEAN(http_range);
	CLEAN(http_content_type);
#undef CLEAN
	con->request.content_length = 0;

	array_reset(con->request.headers);
	array_reset(con->response.headers);
	array_reset(con->environment);

	chunkqueue_reset(con->write_queue);
	chunkqueue_reset(con->request_content_queue);

	/* the plugins should cleanup themself */
	for (i = 0; i < srv->plugins.used; i++) {
		plugin *p = ((plugin **)(srv->plugins.ptr))[i];
		plugin_data *pd = p->data;

		if (!pd) continue;

		if (con->plugin_ctx[pd->id] != NULL) {
			log_error_write(srv, __FILE__, __LINE__, "sb", "missing cleanup in", p->name);
		}

		con->plugin_ctx[pd->id] = NULL;
	}

	/* The cond_cache gets reset in response.c */
	/* config_cond_cache_reset(srv, con); */

	con->header_len = 0;
	con->in_error_handler = 0;
	
	config_setup_connection(srv, con);

	return 0;
}

/**
 * handle all header and content read
 *
 * we get called by the state-engine and by the fdevent-handler
 */
static int connection_handle_read_state(server *srv, connection *con)  {
	connection_state_t ostate = con->state;
	chunk *c, *last_chunk;
	off_t last_offset;
	chunkqueue *cq = con->read_queue;
	chunkqueue *dst_cq = con->request_content_queue;
	int is_closed = 0; /* the connection got closed, if we don't have a complete header, -> error */

	//Cdbg(DBE,"begins is_readable=%d",con->is_readable);
	if (con->is_readable) {
		con->read_idle_ts = srv->cur_ts;

		switch(connection_handle_read(srv, con)) {
		case -1:
			return -1;
		case -2:
			is_closed = 1;
			break;
		default:
		   //Cdbg(DBE,"return default");
			break;
		}
	}

	/* the last chunk might be empty */
	for (c = cq->first; c;) {
		if (cq->first == c && c->mem->used == 0) {
			/* the first node is empty */
			/* ... and it is empty, move it to unused */

			cq->first = c->next;
			if (cq->first == NULL) cq->last = NULL;

			c->next = cq->unused;
			cq->unused = c;
			cq->unused_chunks++;

			c = cq->first;
		} else if (c->next && c->next->mem->used == 0) {
			chunk *fc;
			/* next node is the last one */
			/* ... and it is empty, move it to unused */

			fc = c->next;
			c->next = fc->next;

			fc->next = cq->unused;
			cq->unused = fc;
			cq->unused_chunks++;

			/* the last node was empty */
			if (c->next == NULL) {
				cq->last = c;
			}

			c = c->next;
		} else {
			c = c->next;
		}
	}

	/* we might have got several packets at once
	 */

	switch(ostate) {
	case CON_STATE_READ:
		/* if there is a \r\n\r\n in the chunkqueue
		 *
		 * scan the chunk-queue twice
		 * 1. to find the \r\n\r\n
		 * 2. to copy the header-packet
		 *
		 */

		last_chunk = NULL;
		last_offset = 0;

		for (c = cq->first; c; c = c->next) {
			buffer b;
			size_t i;

			b.ptr = c->mem->ptr + c->offset;
			b.used = c->mem->used - c->offset;
			if (b.used > 0) b.used--; /* buffer "used" includes terminating zero */

			for (i = 0; i < b.used; i++) {
				char ch = b.ptr[i];

				if ('\r' == ch) {
					/* chec if \n\r\n follows */
					size_t j = i+1;
					chunk *cc = c;
					const char header_end[] = "\r\n\r\n";
					int header_end_match_pos = 1;

					for ( ; cc; cc = cc->next, j = 0 ) {
						buffer bb;
						bb.ptr = cc->mem->ptr + cc->offset;
						bb.used = cc->mem->used - cc->offset;
						if (bb.used > 0) bb.used--; /* buffer "used" includes terminating zero */

						for ( ; j < bb.used; j++) {
							ch = bb.ptr[j];

							if (ch == header_end[header_end_match_pos]) {
								header_end_match_pos++;
								if (4 == header_end_match_pos) {
									last_chunk = cc;
									last_offset = j+1;
									//Cdbg(DBE,"Found Header!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
									goto found_header_end;
								}
							} else {
								goto reset_search;
							}
						}
					}
				}
reset_search: ;
			}
		}
found_header_end:

		/* found */
		if (last_chunk) {
			buffer_reset(con->request.request);
			
			for (c = cq->first; c; c = c->next) {
				buffer b;
				//Cdbg(DBE,"######################################");
				//Cdbg(DBE,"1111 c->offset = %d",c->offset);

				b.ptr = c->mem->ptr + c->offset;
				b.used = c->mem->used - c->offset;

				//Cdbg(DBE,"Request Header = %s", b.ptr);
				//Cdbg(DBE,"######################################");

				if (c == last_chunk) {
					b.used = last_offset + 1;
				}

				buffer_append_string_buffer(con->request.request, &b);
				
				if (c == last_chunk) {
					c->offset += last_offset;

					break;
				} else {
					/* the whole packet was copied */
					c->offset = c->mem->used - 1;
				}
			}
			//Cdbg(DBE,"con->request.request =%s",con->request.request->ptr);
			connection_set_state(srv, con, CON_STATE_REQUEST_END);
		} else if (chunkqueue_length(cq) > 64 * 1024) {
			log_error_write(srv, __FILE__, __LINE__, "s", "oversized request-header -> sending Status 414");

			con->http_status = 414; /* Request-URI too large */
			con->keep_alive = 0;
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		}
		break;
	case CON_STATE_READ_POST:
		//Cdbg(DBE,"CON_STATE_READ_POST: cq->first=%p", cq->first);
		for (c = cq->first; c && (dst_cq->bytes_in != (off_t)con->request.content_length); c = c->next) {
			//Cdbg(DBE,"c->type=%d",c->type);
			off_t weWant, weHave, toRead;

			weWant = con->request.content_length - dst_cq->bytes_in;

			assert(c->mem->used);

			weHave = c->mem->used - c->offset - 1;

			toRead = weHave > weWant ? weWant : weHave;

			/* the new way, copy everything into a chunkqueue whcih might use tempfiles */
			if (con->request.content_length > 64 * 1024) {
//			if (1) {
				chunk *dst_c = NULL;
				/* copy everything to max 1Mb sized tempfiles */

				/*
				 * if the last chunk is
				 * - smaller than 1Mb (size < 1Mb)
				 * - not read yet (offset == 0)
				 * -> append to it
				 * otherwise
				 * -> create a new chunk
				 *
				 * */

				if (dst_cq->last &&
				    dst_cq->last->type == FILE_CHUNK &&
				    dst_cq->last->file.is_temp &&
				    dst_cq->last->offset == 0) {
					/* ok, take the last chunk for our job */

			 		if (dst_cq->last->file.length < 1 * 1024 * 1024) {
						dst_c = dst_cq->last;
						if(con->mode == DIRECT){
							if (dst_c->file.fd == -1) {        
								/* this should not happen as we cache the fd, but you never know */
							    dst_c->file.fd = open(dst_c->file.name->ptr, O_WRONLY | O_APPEND);
#ifdef FD_CLOEXEC
								fcntl(dst_c->file.fd, F_SETFD, FD_CLOEXEC);                                                 
#endif
							}
						}else{
//							if (dst_c->file.fd == -1 || con->smb_info->cur_fd==-1) {
							if (dst_c->file.fd == -1 || con->cur_fd==-1) {
								//Cdbg(DBE,"dst_c file.fd ==-1");
								/* this should not happen as we cache the fd, but you never know */
								if (0> (con->cur_fd = smbc_wrapper_open(con,con->url.path->ptr ,O_WRONLY| O_APPEND, 0) )) 
								{
									con->cur_fd = smbc_wrapper_create(con,con->url.path->ptr ,0 ,0);
								}
								dst_c->file.fd=con->cur_fd;

#ifdef FD_CLOEXEC
								fcntl(dst_c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
							}
						}

					} else {
						/* the chunk is too large now, close it */
						dst_c = dst_cq->last;
						if(con->mode == DIRECT){
							if (dst_c->file.fd != -1) {        
								close(dst_c->file.fd);         
								dst_c->file.fd = -1;       
							}
						}else{
							if(con->cur_fd != -1 || (void*)con->cur_fd != NULL){
								smbc_wrapper_close(con,con->cur_fd);
								dst_c->file.fd = -1;
								con->cur_fd =-1;
							}
						}
						dst_c = chunkqueue_get_append_tempfile(dst_cq);
					}
				} else {
					if(con->mode == DIRECT){
						dst_c = chunkqueue_get_append_tempfile(dst_cq);
					}else{
						dst_c = chunkqueue_get_append_smbfile(dst_cq, con->url.path->ptr, con, toRead, dst_cq->bytes_in );
					}
				}

				/* we have a chunk, let's write to it */

//				if (dst_c->file.fd == -1) {
			//	if(con->smb_info->cur_fd == -1 || dst_c->file.fd == -1){

				if(con->mode == DIRECT){
   				   if( dst_c->file.fd == -1){
						/* we don't have file to write to,
						 * EACCES might be one reason.
						 *
						 * Instead of sending 500 we send 413 and say the request is too large
						 *  */

						log_error_write(srv, __FILE__, __LINE__, "sbs",
								"denying upload as opening to temp-file for upload failed:",
								dst_c->file.name, strerror(errno));
						
						con->http_status = 413; /* Request-Entity too large */
						con->keep_alive = 0;
						connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
	
						//assert(0);
						break;
					}

				}else{
   				   if( con->cur_fd == -1){
						log_error_write(srv, __FILE__, __LINE__, "sbs",
								"denying upload as opening to temp-file for upload failed:",
								dst_c->file.name, strerror(errno));
						
						con->http_status = 413; /* Request-Entity too large */
						con->keep_alive = 0;
						connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
						break;
					}
				}
				off_t write_lens=0;
				if(con->mode == DIRECT){
					write_lens = (off_t)write(dst_c->file.fd, c->mem->ptr + c->offset, (size_t)toRead);
				}else{
					//	Cdbg(1,"fd =%p",con->smb_info->cur_fd);
					write_lens = (off_t)smbc_wrapper_write(con, dst_c->file.fd, c->mem->ptr + c->offset, (size_t)toRead, 0);
				}
				//Cdbg(DBE,"wrlens =%lli ,toREad=%lli ", write_lens, toRead);

				if(toRead != write_lens){

					/* write failed for some reason ... disk full ? */
					log_error_write(srv, __FILE__, __LINE__, "sbs",
							"denying upload as writing to file failed:",
							dst_c->file.name, strerror(errno));

					con->http_status = 413; /* Request-Entity too large */
					con->keep_alive = 0;
					connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

					if(con->mode == DIRECT)	{
						close(dst_c->file.fd);
					}
					else{
						smbc_wrapper_close(con,con->cur_fd);
					}
					dst_c->file.fd = -1;
					con->cur_fd =-1;
					break;
				}

				dst_c->file.length += toRead;

				if (dst_cq->bytes_in + toRead == (off_t)con->request.content_length) {
					/* we read everything, close the chunk */
					if(con->mode == DIRECT){
						if(dst_c->file.fd) close(dst_c->file.fd);
					}else{
						if(con->cur_fd) smbc_wrapper_close(con, con->cur_fd);
					}
					dst_c->file.fd = -1;
					con->cur_fd =-1;
				}
			} else {
				buffer *b;
		//		Cdbg(DBE,"toRead =%lli",toRead);

				if (dst_cq->last &&
				    dst_cq->last->type == MEM_CHUNK) {
					b = dst_cq->last->mem;
					//Cdbg(DBE,"last->mem =%s", b->ptr);
				} else {
					b = chunkqueue_get_append_buffer(dst_cq);
					/* prepare buffer size for remaining POST data; is < 64kb */
					buffer_prepare_copy(b, con->request.content_length - (size_t)dst_cq->bytes_in + 1);
					buffer_prepare_copy(b, con->request.content_length - (size_t)dst_cq->bytes_in + 1);
					//Cdbg(DBE,"data < 64k, cont_len=%d, bytes_in=%lli",con->request.content_length,dst_cq->bytes_in);
				}
				buffer_append_string_len(b, c->mem->ptr + c->offset,(size_t)toRead);
			}

			c->offset += toRead;
			dst_cq->bytes_in += toRead;
		}

		/* Content is ready */
		if (dst_cq->bytes_in == (off_t)con->request.content_length) {
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		}

		break;
	default: break;
	}

	/* the connection got closed and we didn't got enough data to leave one of the READ states
	 * the only way is to leave here */
	if (is_closed && ostate == con->state) {
	//	Cdbg(1,"is close & set state error, file.fd=%d, cur_fd=%d, mod=%d", c->file.fd, con->cur_fd, con->mode);
		connection_set_state(srv, con, CON_STATE_ERROR);
		if( con->mode == SMB_BASIC || con->mode == SMB_NTLM ) {
			if(con->cur_fd == -1)
				con->cur_fd = smbc_wrapper_open(con,con->url.path->ptr ,O_WRONLY| O_APPEND, 0);
			else {
			   // Get size 
				struct stat st; 
				off_t partial_file_sz=0;
				int err;
				
				err = smbc_wrapper_stat(con, con->url.path->ptr, &st);
				if(err != -1){
					partial_file_sz = st.st_size;
				}
				err = smbc_wrapper_close(con, con->cur_fd);
				con->cur_fd =-1;
				if((off_t)con->request.content_length > partial_file_sz){
					err = smbc_wrapper_unlink(con,con->url.path->ptr);
				}
			} 
		}
	}

	chunkqueue_remove_finished_chunks(cq);
//Cdbg(DBE,"ends @ con->state =%s, len =%lli",connection_get_state(con->state), (off_t)con->request.content_length);

	return 0;
}

static handler_t connection_handle_fdevent(server *srv, void *context, int revents) {
	connection *con = context;
	//Cdbg(DBE,"begins....................................................");

	joblist_append(srv, con);
//Cdbg(DBE, "enter, con=[%p], fd=[%d], revents=[%04X]", con, con->fd, revents);
	if (con->conf.is_ssl) {
		/* ssl may read and write for both reads and writes */
		if (revents & (FDEVENT_IN | FDEVENT_OUT)) {
			con->is_readable = 1;
			con->is_writable = 1;
		}
	} else {
		if (revents & FDEVENT_IN) {
			con->is_readable = 1;
		}
		if (revents & FDEVENT_OUT) {
			con->is_writable = 1;
			/* we don't need the event twice */
		}
	}

	if (revents & ~(FDEVENT_IN | FDEVENT_OUT)) {
//	if (!con->is_readable && !con->is_writable) {
		/* looks like an error */

		/* FIXME: revents = 0x19 still means that we should read from the queue */
		if (revents & FDEVENT_HUP) {
			if (con->state == CON_STATE_CLOSE) {
				con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
			} else {
				/* sigio reports the wrong event here
				 *
				 * there was no HUP at all
				 */
#ifdef USE_LINUX_SIGIO
				if (srv->ev->in_sigio == 1) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
						"connection closed: poll() -> HUP", con->fd);
				} else {
					connection_set_state(srv, con, CON_STATE_ERROR);
				}
#else
				connection_set_state(srv, con, CON_STATE_ERROR);
#endif

			}
		} else if (revents & FDEVENT_ERR) {
			/* error, connection reset, whatever... we don't want to spam the logfile */
#if 0
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"connection closed: poll() -> ERR", con->fd);
#endif
			connection_set_state(srv, con, CON_STATE_ERROR);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"connection closed: poll() -> ???", revents);
		}
	}

	if (con->state == CON_STATE_READ ||
	    con->state == CON_STATE_READ_POST) {
		connection_handle_read_state(srv, con);
	}

	if (con->state == CON_STATE_WRITE &&
	    !chunkqueue_is_empty(con->write_queue) &&
	    con->is_writable) {

		if (-1 == connection_handle_write(srv, con)) {
			connection_set_state(srv, con, CON_STATE_ERROR);

			log_error_write(srv, __FILE__, __LINE__, "ds",
					con->fd,
					"handle write failed.");
		} else if (con->state == CON_STATE_WRITE) {
			con->write_request_ts = srv->cur_ts;
		}
	}

	if (con->state == CON_STATE_CLOSE) {
		/* flush the read buffers */
		int len;
		char buf[1024];

		len = read(con->fd, buf, sizeof(buf));
		if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR) ) {
			con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
		}
	}

	return HANDLER_FINISHED;
}


connection *connection_accept(server *srv, server_socket *srv_socket) {
	/* accept everything */

	/* search an empty place */
	int cnt;
	sock_addr cnt_addr;
	socklen_t cnt_len;
	/* accept it and register the fd */

	/**
	 * check if we can still open a new connections
	 *
	 * see #1216
	 */

	if (srv->conns->used >= srv->max_conns) {
		return NULL;
	}

	cnt_len = sizeof(cnt_addr);

	if (-1 == (cnt = accept(srv_socket->fd, (struct sockaddr *) &cnt_addr, &cnt_len))) {
		switch (errno) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
		case EINTR:
			/* we were stopped _before_ we had a connection */
		case ECONNABORTED: /* this is a FreeBSD thingy */
			/* we were stopped _after_ we had a connection */
			break;
		case EMFILE:
			/* out of fds */
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "accept failed:", strerror(errno), errno);
		}
		return NULL;
	} else {
		connection *con;

		srv->cur_fds++;

		/* ok, we have the connection, register it */
#if 0
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"appected()", cnt);
#endif
		srv->con_opened++;

		con = connections_get_new_connection(srv);

		con->fd = cnt;
		con->fde_ndx = -1;
#if 0
		gettimeofday(&(con->start_tv), NULL);
#endif
//Cdbg(DBE, "connection_accept, accept con=[%p], con->fd=[%d]", con, con->fd);
		fdevent_register(srv->ev, con->fd, connection_handle_fdevent, con);

		connection_set_state(srv, con, CON_STATE_REQUEST_START);

		con->connection_start = srv->cur_ts;
		con->dst_addr = cnt_addr;
		buffer_copy_string(con->dst_addr_buf, inet_ntop_cache_get_ip(srv, &(con->dst_addr)));
		con->srv_socket = srv_socket;

		if (-1 == (fdevent_fcntl_set(srv->ev, con->fd))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed: ", strerror(errno));
			return NULL;
		}
#ifdef USE_OPENSSL
		/* connect FD to SSL */
		if (srv_socket->is_ssl) {
			if (NULL == (con->ssl = SSL_new(srv_socket->ssl_ctx))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));

				return NULL;
			}
#ifndef OPENSSL_NO_TLSEXT
			SSL_set_app_data(con->ssl, con);
#endif
			SSL_set_accept_state(con->ssl);
			con->conf.is_ssl=1;

			if (1 != (SSL_set_fd(con->ssl, cnt))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));
				return NULL;
			}
		}
#endif
		return con;
	}
}


int connection_state_machine(server *srv, connection *con) {
//Cdbg(DBE, "enter..state=[%d][%s], status=[%d]", con->state, connection_get_state(con->state), con->http_status);
	int done = 0, r;
#ifdef USE_OPENSSL
	server_socket *srv_sock = con->srv_socket;
#endif

	if (srv->srvconf.log_state_handling) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"state at start",
				con->fd,
				connection_get_state(con->state));
	}

	while (done == 0) {
		size_t ostate = con->state;
//Cdbg(DBE, "state=[%d][%s], status=[%d]", con->state, connection_get_state(con->state), con->http_status);
		switch (con->state) {
		case CON_STATE_REQUEST_START: /* transient */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			con->request_start = srv->cur_ts;
			con->read_idle_ts = srv->cur_ts;

			con->request_count++;
			con->loops_per_request = 0;

			connection_set_state(srv, con, CON_STATE_READ);

			/* patch con->conf.is_ssl if the connection is a ssl-socket already */

#ifdef USE_OPENSSL
			con->conf.is_ssl = srv_sock->is_ssl;
#endif

			break;
		case CON_STATE_REQUEST_END: /* transient */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}
			
			buffer_reset(con->uri.authority);
			buffer_reset(con->uri.path);
			buffer_reset(con->uri.query);
			buffer_reset(con->request.orig_uri);
			
			int res = http_request_parse(srv, con);
						
			//- JerryLin add
			con->mode = SMB_BASIC;
			
			if(strncmp(con->request.uri->ptr, "/GetCaptchaImage", 16)==0){
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				break;
			}

//#if EMBEDDED_EANBLE
#if 0
			Cdbg(1, "asdsadsa %s, %d", con->uri.scheme->ptr, con->conf.is_ssl);
			if(con->conf.is_ssl==0){
				con->file_finished = 1;
				con->http_status = 404;
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				break;
			}
#endif

			//- If url is encrypted share link, then use basic auth
			int result = parser_share_file(srv, con);
			if(result==-1){
				Cdbg(DBE, "fail to parser_share_file");
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
			}
			////////////////////////////////////////////////////////////////////////////////////////

			data_string *ds_userAgent = (data_string *)array_get_element(con->request.headers, "user-Agent");
			char* aa = get_filename_ext(con->request.uri->ptr);
			int len = strlen(aa)+1;			
			char* file_ext = (char*)malloc(len);
			memset(file_ext,'\0', len);
			strcpy(file_ext, aa);
			for (int i = 0; file_ext[i]; i++)
 				file_ext[i] = tolower(file_ext[i]);
			
			if( con->share_link_basic_auth->used &&
				ds_userAgent && 
				con->conf.is_ssl &&
				( strncmp(file_ext, "mp3", 3) == 0 ||
				  strncmp(file_ext, "mp4", 3) == 0 ||
				  strncmp(file_ext, "m4v", 3) == 0 ) &&
		        ( //strstr( ds_userAgent->value->ptr, "Chrome" ) ||
		          strstr( ds_userAgent->value->ptr, "iPhone" ) || 
		          strstr( ds_userAgent->value->ptr, "iPad"   ) || 
		          strstr( ds_userAgent->value->ptr, "iPod"   ) ) ){
		   		
				buffer *out;
				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html; charset=UTF-8"));
				
				out = chunkqueue_get_append_buffer(con->write_queue);

			#if EMBEDDED_EANBLE
				char* webdav_http_port = nvram_get_webdav_http_port();
			#else
				char* webdav_http_port = "8082";
			#endif

				buffer_append_string_len(out, CONST_STR_LEN("<div id=\"http_port\" content=\""));				
				buffer_append_string(out, webdav_http_port);
				buffer_append_string_len(out, CONST_STR_LEN("\"></div>\n"));

				stat_cache_entry *sce = NULL;
				
				buffer* html_file = buffer_init();

				if( strncmp(file_ext, "mp3", 3) == 0 )
					buffer_copy_string(html_file, "/usr/css/iaudio.html");
				else if( strncmp(file_ext, "mp4", 3) == 0 ||
					     strncmp(file_ext, "m4v", 3) == 0 )
					buffer_copy_string(html_file, "/usr/css/ivideo.html");
				
				con->mode = DIRECT;
				if (HANDLER_ERROR != stat_cache_get_entry(srv, con, html_file, &sce)) {
					http_chunk_append_file(srv, con, html_file, 0, sce->st.st_size);
					response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
					con->file_finished = 1;
					con->http_status = 200;					
				}
				else{
					con->file_finished = 1;
					con->http_status = 404;
				}

				Cdbg(DBE, "ds_userAgent->value=%s, file_ext=%s, html_file=%s", ds_userAgent->value->ptr, file_ext, html_file->ptr);

				free(file_ext);
				buffer_free(html_file);
				
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
				
				break;
			}

			free(file_ext);
			////////////////////////////////////////////////////////////////////////////////////////
			
			get_connection_auth_type(srv, con);
			
			if(do_connection_auth(srv, con) != HANDLER_UNSET) {
				chunkqueue_reset(con->read_queue);
				chunkqueue_reset(con->write_queue);
				connection_set_state(srv, con, CON_STATE_RESPONSE_START);
				break;
			}
			/////////////////////////////////////////////////////////////////////

			if (res) {
				/* we have to read some data from the POST request */
				connection_set_state(srv, con, CON_STATE_READ_POST);
				break;
			}
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);			
			break;
			
		case CON_STATE_HANDLE_REQUEST:
			/*
			 * the request is parsed
			 *
			 * decided what to do with the request
			 * -
			 *
			 *
			 */

			//Cdbg(DBE,"CON_STATE_HANDLE_REQUEST: ");
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}
			
			switch (r = http_response_prepare(srv, con)) {
			case HANDLER_FINISHED:
			   //Cdbg(DBE,"HANDLER_FINISHED: ");
				if (con->mode == DIRECT || con->mode == SMB_BASIC || con->mode == SMB_NTLM) {
					if (con->http_status == 404 ||
					    con->http_status == 403) {
						/* 404 error-handler */

						if (con->in_error_handler == 0 &&
						    (!buffer_is_empty(con->conf.error_handler) ||
						     !buffer_is_empty(con->error_handler))) {
							/* call error-handler */

							con->error_handler_saved_status = con->http_status;
							con->http_status = 0;

							if (buffer_is_empty(con->error_handler)) {
								buffer_copy_string_buffer(con->request.uri, con->conf.error_handler);
							} else {
								buffer_copy_string_buffer(con->request.uri, con->error_handler);
							}
							buffer_reset(con->physical.path);

							con->in_error_handler = 1;

							connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

							done = -1;
							assert(0);
							break;
						} else if (con->in_error_handler) {
							/* error-handler is a 404 */

							con->http_status = con->error_handler_saved_status;
						}
					} else if (con->in_error_handler) {
						/* error-handler is back and has generated content */
						/* if Status: was set, take it otherwise use 200 */
					}
				}
				if (con->http_status == 0) con->http_status = 200;

				/* we have something to send, go on */
				//Cdbg(DBE,"set CON_STATE_RESPONSE_START, done=%d",done);
				connection_set_state(srv, con, CON_STATE_RESPONSE_START);
				break;
			case HANDLER_WAIT_FOR_FD:
				srv->want_fds++;

				fdwaitqueue_append(srv, con);

				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

				break;
			case HANDLER_COMEBACK:
				done = -1;
			case HANDLER_WAIT_FOR_EVENT:
				/* come back here */
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

				break;
			case HANDLER_ERROR:
				/* something went wrong */
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sdd", "unknown ret-value: ", con->fd, r);
				break;
			}

			break;
		case CON_STATE_RESPONSE_START:
			/*
			 * the decision is done
			 * - create the HTTP-Response-Header
			 *
			 */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}
			
			if (-1 == connection_handle_write_prepare(srv, con)) {
				connection_set_state(srv, con, CON_STATE_ERROR);

				break;
			}
			
			connection_set_state(srv, con, CON_STATE_WRITE);
			break;
		case CON_STATE_RESPONSE_END: /* transient */
			/* log the request */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}
			
			plugins_call_handle_request_done(srv, con);
			

			srv->con_written++;

			if (con->keep_alive) {
				connection_set_state(srv, con, CON_STATE_REQUEST_START);

#if 0
				con->request_start = srv->cur_ts;
				con->read_idle_ts = srv->cur_ts;
#endif
			} else {
				switch(r = plugins_call_handle_connection_close(srv, con)) {
				case HANDLER_GO_ON:
				case HANDLER_FINISHED:
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "sd", "unhandling return value", r);
					break;
				}
#ifdef USE_OPENSSL
				if (srv_sock->is_ssl) {
					switch (SSL_shutdown(con->ssl)) {
					case 1:
						/* done */
						break;
					case 0:
						/* wait for fd-event
						 *
						 * FIXME: wait for fdevent and call SSL_shutdown again
						 *
						 */

						break;
					default:
						log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
								ERR_error_string(ERR_get_error(), NULL));
					}
				}
#endif
				if ((0 == shutdown(con->fd, SHUT_WR))) {
					con->close_timeout_ts = srv->cur_ts;
					connection_set_state(srv, con, CON_STATE_CLOSE);
				} else {
					connection_close(srv, con);
				}

				srv->con_closed++;
			}
			
			connection_reset(srv, con);
			
			break;
		case CON_STATE_CONNECT:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}
			chunkqueue_reset(con->read_queue);

			con->request_count = 0;

			break;
		case CON_STATE_CLOSE:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			/* we have to do the linger_on_close stuff regardless
			 * of con->keep_alive; even non-keepalive sockets may
			 * still have unread data, and closing before reading
			 * it will make the client not see all our output.
			 */
			{
				int len;
				char buf[1024];

				len = read(con->fd, buf, sizeof(buf));
				if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR) ) {
					con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
				}
			}

			if (srv->cur_ts - con->close_timeout_ts > HTTP_LINGER_TIMEOUT) {
				connection_close(srv, con);

				if (srv->srvconf.log_state_handling) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"connection closed for fd", con->fd);
				}
			}

			break;
		case CON_STATE_READ_POST:
		case CON_STATE_READ:
			//Cdbg(DBE,"CON_STATE_READ:");
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			connection_handle_read_state(srv, con);
			break;
		case CON_STATE_WRITE:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			/* only try to write if we have something in the queue */
			if (!chunkqueue_is_empty(con->write_queue)) {
#if 0
				log_error_write(srv, __FILE__, __LINE__, "dsd",
						con->fd,
						"packets to write:",
						con->write_queue->used);
#endif
			}
			
			if (!chunkqueue_is_empty(con->write_queue) && con->is_writable) {				
				if (-1 == connection_handle_write(srv, con)) {
					log_error_write(srv, __FILE__, __LINE__, "ds",
							con->fd,
							"handle write failed.");
					connection_set_state(srv, con, CON_STATE_ERROR);
				} else if (con->state == CON_STATE_WRITE) {
					con->write_request_ts = srv->cur_ts;
				}
			}

			break;
		case CON_STATE_ERROR: /* transient */

			/* even if the connection was drop we still have to write it to the access log */
			if (con->http_status) {
				plugins_call_handle_request_done(srv, con);
			}
#ifdef USE_OPENSSL
			if (srv_sock->is_ssl) {
				int ret, ssl_r;
				unsigned long err;
				ERR_clear_error();
				switch ((ret = SSL_shutdown(con->ssl))) {
				case 1:
					/* ok */
					break;
				case 0:
					ERR_clear_error();
					if (-1 != (ret = SSL_shutdown(con->ssl))) break;

					/* fall through */
				default:

					switch ((ssl_r = SSL_get_error(con->ssl, ret))) {
					case SSL_ERROR_WANT_WRITE:
					case SSL_ERROR_WANT_READ:
						break;
					case SSL_ERROR_SYSCALL:
						/* perhaps we have error waiting in our error-queue */
						if (0 != (err = ERR_get_error())) {
							do {
								log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
										ssl_r, ret,
										ERR_error_string(err, NULL));
							} while((err = ERR_get_error()));
						} else if (errno != 0) { /* ssl bug (see lighttpd ticket #2213): sometimes errno == 0 */
							log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL (error):",
									ssl_r, ret, errno,
									strerror(errno));
						}
	
						break;
					default:
						while((err = ERR_get_error())) {
							log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
									ssl_r, ret,
									ERR_error_string(err, NULL));
						}
	
						break;
					}
				}
			}
			ERR_clear_error();
#endif

			switch(con->mode) {
			case DIRECT:
			case SMB_BASIC:
			case SMB_NTLM:
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"emergency exit: direct",
						con->fd);
#endif
				break;
			default:
				switch(r = plugins_call_handle_connection_close(srv, con)) {
				case HANDLER_GO_ON:
				case HANDLER_FINISHED:
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "sd", "unhandling return value", r);
					break;
				}
				break;
			}

			connection_reset(srv, con);

			/* close the connection */
			if ((0 == shutdown(con->fd, SHUT_WR))) {
				con->close_timeout_ts = srv->cur_ts;
				connection_set_state(srv, con, CON_STATE_CLOSE);

				if (srv->srvconf.log_state_handling) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"shutdown for fd", con->fd);
				}
			} else {
				connection_close(srv, con);
			}

			con->keep_alive = 0;

			srv->con_closed++;

			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sdd",
					"unknown state:", con->fd, con->state);

			break;
		}

		if (done == -1) {
			done = 0;
		} else if (ostate == con->state) {
			done = 1;
		}
	}
//Cdbg(DBE, "leave while loop..with state=[%s]", connection_get_state(con->state));
	if (srv->srvconf.log_state_handling) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"state at exit:",
				con->fd,
				connection_get_state(con->state));
	}

	switch(con->state) {
	case CON_STATE_READ_POST:
	case CON_STATE_READ:
	case CON_STATE_CLOSE:
		fdevent_event_set(srv->ev, &(con->fde_ndx), con->fd, FDEVENT_IN);
		break;
	case CON_STATE_WRITE:
		/* request write-fdevent only if we really need it
		 * - if we have data to write
		 * - if the socket is not writable yet
		 */
		if (!chunkqueue_is_empty(con->write_queue) &&
		    (con->is_writable == 0) &&
		    (con->traffic_limit_reached == 0)) {
			fdevent_event_set(srv->ev, &(con->fde_ndx), con->fd, FDEVENT_OUT);
		} else {
			fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
		}
		break;
	default:
		fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
		break;
	}

	return 0;
}
