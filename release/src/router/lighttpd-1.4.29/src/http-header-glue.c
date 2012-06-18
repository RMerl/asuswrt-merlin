#include "base.h"
#include "array.h"
#include "buffer.h"
#include "log.h"
#include "etag.h"
#include "response.h"

#include <string.h>
#include <errno.h>

#include <time.h>

/*
 * This was 'borrowed' from tcpdump.
 *
 *
 * This is fun.
 *
 * In older BSD systems, socket addresses were fixed-length, and
 * "sizeof (struct sockaddr)" gave the size of the structure.
 * All addresses fit within a "struct sockaddr".
 *
 * In newer BSD systems, the socket address is variable-length, and
 * there's an "sa_len" field giving the length of the structure;
 * this allows socket addresses to be longer than 2 bytes of family
 * and 14 bytes of data.
 *
 * Some commercial UNIXes use the old BSD scheme, some use the RFC 2553
 * variant of the old BSD scheme (with "struct sockaddr_storage" rather
 * than "struct sockaddr"), and some use the new BSD scheme.
 *
 * Some versions of GNU libc use neither scheme, but has an "SA_LEN()"
 * macro that determines the size based on the address family.  Other
 * versions don't have "SA_LEN()" (as it was in drafts of RFC 2553
 * but not in the final version).  On the latter systems, we explicitly
 * check the AF_ type to determine the length; we assume that on
 * all those systems we have "struct sockaddr_storage".
 */

#ifdef HAVE_IPV6
# ifndef SA_LEN
#  ifdef HAVE_SOCKADDR_SA_LEN
#   define SA_LEN(addr)   ((addr)->sa_len)
#  else /* HAVE_SOCKADDR_SA_LEN */
#   ifdef HAVE_STRUCT_SOCKADDR_STORAGE
static size_t get_sa_len(const struct sockaddr *addr) {
	switch (addr->sa_family) {

#    ifdef AF_INET
	case AF_INET:
		return (sizeof (struct sockaddr_in));
#    endif

#    ifdef AF_INET6
	case AF_INET6:
		return (sizeof (struct sockaddr_in6));
#    endif

	default:
		return (sizeof (struct sockaddr));

	}
}
#    define SA_LEN(addr)   (get_sa_len(addr))
#   else /* HAVE_SOCKADDR_STORAGE */
#    define SA_LEN(addr)   (sizeof (struct sockaddr))
#   endif /* HAVE_SOCKADDR_STORAGE */
#  endif /* HAVE_SOCKADDR_SA_LEN */
# endif /* SA_LEN */
#endif




int response_header_insert(server *srv, connection *con, const char *key, size_t keylen, const char *value, size_t vallen) {
	data_string *ds;

	UNUSED(srv);

	if (NULL == (ds = (data_string *)array_get_unused_element(con->response.headers, TYPE_STRING))) {
		ds = data_response_init();
	}
	buffer_copy_string_len(ds->key, key, keylen);
	buffer_copy_string_len(ds->value, value, vallen);

	array_insert_unique(con->response.headers, (data_unset *)ds);

	return 0;
}

int response_header_overwrite(server *srv, connection *con, const char *key, size_t keylen, const char *value, size_t vallen) {
	data_string *ds;

	UNUSED(srv);

	/* if there already is a key by this name overwrite the value */
	if (NULL != (ds = (data_string *)array_get_element(con->response.headers, key))) {
		buffer_copy_string(ds->value, value);

		return 0;
	}

	return response_header_insert(srv, con, key, keylen, value, vallen);
}

int response_header_append(server *srv, connection *con, const char *key, size_t keylen, const char *value, size_t vallen) {
	data_string *ds;

	UNUSED(srv);

	/* if there already is a key by this name append the value */
	if (NULL != (ds = (data_string *)array_get_element(con->response.headers, key))) {
		buffer_append_string_len(ds->value, CONST_STR_LEN(", "));
		buffer_append_string_len(ds->value, value, vallen);
		return 0;
	}

	return response_header_insert(srv, con, key, keylen, value, vallen);
}

int http_response_redirect_to_directory(server *srv, connection *con) {
	buffer *o;

	o = buffer_init();

	if (con->conf.is_ssl) {
		buffer_copy_string_len(o, CONST_STR_LEN("https://"));
	} else {
		buffer_copy_string_len(o, CONST_STR_LEN("http://"));
	}
	if (con->uri.authority->used) {
		buffer_append_string_buffer(o, con->uri.authority);
	} else {
		/* get the name of the currently connected socket */
		struct hostent *he;
#ifdef HAVE_IPV6
		char hbuf[256];
#endif
		sock_addr our_addr;
		socklen_t our_addr_len;

		our_addr_len = sizeof(our_addr);

		if (-1 == getsockname(con->fd, &(our_addr.plain), &our_addr_len)) {
			con->http_status = 500;

			log_error_write(srv, __FILE__, __LINE__, "ss",
					"can't get sockname", strerror(errno));

			buffer_free(o);
			return 0;
		}


		/* Lookup name: secondly try to get hostname for bind address */
		switch(our_addr.plain.sa_family) {
#ifdef HAVE_IPV6
		case AF_INET6:
			if (0 != getnameinfo((const struct sockaddr *)(&our_addr.ipv6),
					     SA_LEN((const struct sockaddr *)&our_addr.ipv6),
					     hbuf, sizeof(hbuf), NULL, 0, 0)) {

				char dst[INET6_ADDRSTRLEN];

				log_error_write(srv, __FILE__, __LINE__,
						"SSS", "NOTICE: getnameinfo failed: ",
						strerror(errno), ", using ip-address instead");

				buffer_append_string(o,
						     inet_ntop(AF_INET6, (char *)&our_addr.ipv6.sin6_addr,
							       dst, sizeof(dst)));
			} else {
				buffer_append_string(o, hbuf);
			}
			break;
#endif
		case AF_INET:
			if (NULL == (he = gethostbyaddr((char *)&our_addr.ipv4.sin_addr, sizeof(struct in_addr), AF_INET))) {
				log_error_write(srv, __FILE__, __LINE__,
						"SdS", "NOTICE: gethostbyaddr failed: ",
						h_errno, ", using ip-address instead");

				buffer_append_string(o, inet_ntoa(our_addr.ipv4.sin_addr));
			} else {
				buffer_append_string(o, he->h_name);
			}
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__,
					"S", "ERROR: unsupported address-type");

			buffer_free(o);
			return -1;
		}

		if (!((con->conf.is_ssl == 0 && srv->srvconf.port == 80) ||
		      (con->conf.is_ssl == 1 && srv->srvconf.port == 443))) {
			buffer_append_string_len(o, CONST_STR_LEN(":"));
			buffer_append_long(o, srv->srvconf.port);
		}
	}
	buffer_append_string_buffer(o, con->uri.path);
	buffer_append_string_len(o, CONST_STR_LEN("/"));
	if (!buffer_is_empty(con->uri.query)) {
		buffer_append_string_len(o, CONST_STR_LEN("?"));
		buffer_append_string_buffer(o, con->uri.query);
	}

	response_header_insert(srv, con, CONST_STR_LEN("Location"), CONST_BUF_LEN(o));

	con->http_status = 301;
	con->file_finished = 1;

	buffer_free(o);

	return 0;
}

buffer * strftime_cache_get(server *srv, time_t last_mod) {
	struct tm *tm;
	size_t i;

	for (i = 0; i < FILE_CACHE_MAX; i++) {
		/* found cache-entry */
		if (srv->mtime_cache[i].mtime == last_mod) return srv->mtime_cache[i].str;

		/* found empty slot */
		if (srv->mtime_cache[i].mtime == 0) break;
	}

	if (i == FILE_CACHE_MAX) {
		i = 0;
	}

	srv->mtime_cache[i].mtime = last_mod;
	buffer_prepare_copy(srv->mtime_cache[i].str, 1024);
	tm = gmtime(&(srv->mtime_cache[i].mtime));
	srv->mtime_cache[i].str->used = strftime(srv->mtime_cache[i].str->ptr,
						 srv->mtime_cache[i].str->size - 1,
						 "%a, %d %b %Y %H:%M:%S GMT", tm);
	srv->mtime_cache[i].str->used++;

	return srv->mtime_cache[i].str;
}


int http_response_handle_cachable(server *srv, connection *con, buffer *mtime) {
	/*
	 * 14.26 If-None-Match
	 *    [...]
	 *    If none of the entity tags match, then the server MAY perform the
	 *    requested method as if the If-None-Match header field did not exist,
	 *    but MUST also ignore any If-Modified-Since header field(s) in the
	 *    request. That is, if no entity tags match, then the server MUST NOT
	 *    return a 304 (Not Modified) response.
	 */

	/* last-modified handling */
	if (con->request.http_if_none_match) {
		if (etag_is_equal(con->physical.etag, con->request.http_if_none_match)) {
			if (con->request.http_method == HTTP_METHOD_GET ||
			    con->request.http_method == HTTP_METHOD_HEAD) {

				/* check if etag + last-modified */
				if (con->request.http_if_modified_since) {
					size_t used_len;
					char *semicolon;

					if (NULL == (semicolon = strchr(con->request.http_if_modified_since, ';'))) {
						used_len = strlen(con->request.http_if_modified_since);
					} else {
						used_len = semicolon - con->request.http_if_modified_since;
					}

					if (0 == strncmp(con->request.http_if_modified_since, mtime->ptr, used_len)) {
						if ('\0' == mtime->ptr[used_len]) con->http_status = 304;
						return HANDLER_FINISHED;
					} else {
						char buf[sizeof("Sat, 23 Jul 2005 21:20:01 GMT")];
						time_t t_header, t_file;
						struct tm tm;

						/* check if we can safely copy the string */
						if (used_len >= sizeof(buf)) {
							log_error_write(srv, __FILE__, __LINE__, "ssdd",
									"DEBUG: Last-Modified check failed as the received timestamp was too long:",
									con->request.http_if_modified_since, used_len, sizeof(buf) - 1);

							con->http_status = 412;
							con->mode = DIRECT;
							return HANDLER_FINISHED;
						}


						strncpy(buf, con->request.http_if_modified_since, used_len);
						buf[used_len] = '\0';

						if (NULL == strptime(buf, "%a, %d %b %Y %H:%M:%S GMT", &tm)) {
							con->http_status = 412;
							con->mode = DIRECT;
							return HANDLER_FINISHED;
						}
						tm.tm_isdst = 0;
						t_header = mktime(&tm);

						strptime(mtime->ptr, "%a, %d %b %Y %H:%M:%S GMT", &tm);
						tm.tm_isdst = 0;
						t_file = mktime(&tm);

						if (t_file > t_header) return HANDLER_GO_ON;

						con->http_status = 304;
						return HANDLER_FINISHED;
					}
				} else {
					con->http_status = 304;
					return HANDLER_FINISHED;
				}
			} else {
				con->http_status = 412;
				con->mode = DIRECT;
				return HANDLER_FINISHED;
			}
		}
	} else if (con->request.http_if_modified_since) {
		size_t used_len;
		char *semicolon;

		if (NULL == (semicolon = strchr(con->request.http_if_modified_since, ';'))) {
			used_len = strlen(con->request.http_if_modified_since);
		} else {
			used_len = semicolon - con->request.http_if_modified_since;
		}

		if (0 == strncmp(con->request.http_if_modified_since, mtime->ptr, used_len)) {
			if ('\0' == mtime->ptr[used_len]) con->http_status = 304;
			return HANDLER_FINISHED;
		} else {
			char buf[sizeof("Sat, 23 Jul 2005 21:20:01 GMT")];
			time_t t_header, t_file;
			struct tm tm;

			/* convert to timestamp */
			if (used_len >= sizeof(buf)) return HANDLER_GO_ON;

			strncpy(buf, con->request.http_if_modified_since, used_len);
			buf[used_len] = '\0';

			if (NULL == strptime(buf, "%a, %d %b %Y %H:%M:%S GMT", &tm)) {
				/**
				 * parsing failed, let's get out of here 
				 */
				return HANDLER_GO_ON;
			}
			tm.tm_isdst = 0;
			t_header = mktime(&tm);

			strptime(mtime->ptr, "%a, %d %b %Y %H:%M:%S GMT", &tm);
			tm.tm_isdst = 0;
			t_file = mktime(&tm);

			if (t_file > t_header) return HANDLER_GO_ON;

			con->http_status = 304;
			return HANDLER_FINISHED;
		}
	}

	return HANDLER_GO_ON;
}
