#include "request.h"
#include "keyvalue.h"
#include "log.h"

#include <sys/stat.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int request_check_hostname(server *srv, connection *con, buffer *host) {
	enum { DOMAINLABEL, TOPLABEL } stage = TOPLABEL;
	size_t i;
	int label_len = 0;
	size_t host_len;
	char *colon;
	int is_ip = -1; /* -1 don't know yet, 0 no, 1 yes */
	int level = 0;

	UNUSED(srv);
	UNUSED(con);

	/*
	 *       hostport      = host [ ":" port ]
	 *       host          = hostname | IPv4address | IPv6address
	 *       hostname      = *( domainlabel "." ) toplabel [ "." ]
	 *       domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
	 *       toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
	 *       IPv4address   = 1*digit "." 1*digit "." 1*digit "." 1*digit
	 *       IPv6address   = "[" ... "]"
	 *       port          = *digit
	 */

	/* no Host: */
	if (buffer_is_empty(host)) return 0;

	host_len = buffer_string_length(host);

	/* IPv6 adress */
	if (host->ptr[0] == '[') {
		char *c = host->ptr + 1;
		int colon_cnt = 0;

		/* check the address inside [...] */
		for (; *c && *c != ']'; c++) {
			if (*c == ':') {
				if (++colon_cnt > 7) {
					return -1;
				}
			} else if (!light_isxdigit(*c) && '.' != *c) {
				return -1;
			}
		}

		/* missing ] */
		if (!*c) {
			return -1;
		}

		/* check port */
		if (*(c+1) == ':') {
			for (c += 2; *c; c++) {
				if (!light_isdigit(*c)) {
					return -1;
				}
			}
		}
		else if ('\0' != *(c+1)) {
			/* only a port is allowed to follow [...] */
			return -1;
		}
		return 0;
	}

	if (NULL != (colon = memchr(host->ptr, ':', host_len))) {
		char *c = colon + 1;

		/* check portnumber */
		for (; *c; c++) {
			if (!light_isdigit(*c)) return -1;
		}

		/* remove the port from the host-len */
		host_len = colon - host->ptr;
	}

	/* Host is empty */
	if (host_len == 0) return -1;

	/* if the hostname ends in a "." strip it */
	if (host->ptr[host_len-1] == '.') {
		/* shift port info one left */
		if (NULL != colon) memmove(colon-1, colon, buffer_string_length(host) - host_len);
		buffer_string_set_length(host, buffer_string_length(host) - 1);
		host_len -= 1;
	}

	if (host_len == 0) return -1;

	/* scan from the right and skip the \0 */
	for (i = host_len; i-- > 0; ) {
		const char c = host->ptr[i];

		switch (stage) {
		case TOPLABEL:
			if (c == '.') {
				/* only switch stage, if this is not the last character */
				if (i != host_len - 1) {
					if (label_len == 0) {
						return -1;
					}

					/* check the first character at right of the dot */
					if (is_ip == 0) {
						if (!light_isalnum(host->ptr[i+1])) {
							return -1;
						}
					} else if (!light_isdigit(host->ptr[i+1])) {
						is_ip = 0;
					} else if ('-' == host->ptr[i+1]) {
						return -1;
					} else {
						/* just digits */
						is_ip = 1;
					}

					stage = DOMAINLABEL;

					label_len = 0;
					level++;
				} else if (i == 0) {
					/* just a dot and nothing else is evil */
					return -1;
				}
			} else if (i == 0) {
				/* the first character of the hostname */
				if (!light_isalnum(c)) {
					return -1;
				}
				label_len++;
			} else {
				if (c != '-' && !light_isalnum(c)) {
					return -1;
				}
				if (is_ip == -1) {
					if (!light_isdigit(c)) is_ip = 0;
				}
				label_len++;
			}

			break;
		case DOMAINLABEL:
			if (is_ip == 1) {
				if (c == '.') {
					if (label_len == 0) {
						return -1;
					}

					label_len = 0;
					level++;
				} else if (!light_isdigit(c)) {
					return -1;
				} else {
					label_len++;
				}
			} else {
				if (c == '.') {
					if (label_len == 0) {
						return -1;
					}

					/* c is either - or alphanum here */
					if ('-' == host->ptr[i+1]) {
						return -1;
					}

					label_len = 0;
					level++;
				} else if (i == 0) {
					if (!light_isalnum(c)) {
						return -1;
					}
					label_len++;
				} else {
					if (c != '-' && !light_isalnum(c)) {
						return -1;
					}
					label_len++;
				}
			}

			break;
		}
	}

	/* a IP has to consist of 4 parts */
	if (is_ip == 1 && level != 3) {
		return -1;
	}

	if (label_len == 0) {
		return -1;
	}

	return 0;
}

#if 0
#define DUMP_HEADER
#endif

static int http_request_split_value(array *vals, buffer *b) {
	size_t i, len;
	int state = 0;

	const char *current;
	const char *token_start = NULL, *token_end = NULL;
	/*
	 * parse
	 *
	 * val1, val2, val3, val4
	 *
	 * into a array (more or less a explode() incl. striping of whitespaces
	 */

	if (buffer_string_is_empty(b)) return 0;

	current = b->ptr;
	len = buffer_string_length(b);
	for (i =  0; i <= len; ++i, ++current) {
		data_string *ds;

		switch (state) {
		case 0: /* find start of a token */
			switch (*current) {
			case ' ':
			case '\t': /* skip white space */
			case ',': /* skip empty token */
				break;
			case '\0': /* end of string */
				return 0;
			default:
				/* found real data, switch to state 1 to find the end of the token */
				token_start = token_end = current;
				state = 1;
				break;
			}
			break;
		case 1: /* find end of token and last non white space character */
			switch (*current) {
			case ' ':
			case '\t':
				/* space - don't update token_end */
				break;
			case ',':
			case '\0': /* end of string also marks the end of a token */
				if (NULL == (ds = (data_string *)array_get_unused_element(vals, TYPE_STRING))) {
					ds = data_string_init();
				}

				buffer_copy_string_len(ds->value, token_start, token_end-token_start+1);
				array_insert_unique(vals, (data_unset *)ds);

				state = 0;
				break;
			default:
				/* no white space, update token_end to include current character */
				token_end = current;
				break;
			}
			break;
		}
	}

	return 0;
}

static int request_uri_is_valid_char(unsigned char c) {
	if (c <= 32) return 0;
	if (c == 127) return 0;
	if (c == 255) return 0;

	return 1;
}

int http_request_parse(server *srv, connection *con) {
	char *uri = NULL, *proto = NULL, *method = NULL, con_length_set;
	int is_key = 1, key_len = 0, is_ws_after_key = 0, in_folding;
	char *value = NULL, *key = NULL;
	char *reqline_host = NULL;
	int reqline_hostlen = 0;

	enum { HTTP_CONNECTION_UNSET, HTTP_CONNECTION_KEEPALIVE, HTTP_CONNECTION_CLOSE } keep_alive_set = HTTP_CONNECTION_UNSET;

	int line = 0;

	int request_line_stage = 0;
	size_t i, first, ilen;

	int done = 0;

	/*
	 * Request: "^(GET|POST|HEAD) ([^ ]+(\\?[^ ]+|)) (HTTP/1\\.[01])$"
	 * Option : "^([-a-zA-Z]+): (.+)$"
	 * End    : "^$"
	 */

	if (con->conf.log_request_header) {
		log_error_write(srv, __FILE__, __LINE__, "sdsdSb",
				"fd:", con->fd,
				"request-len:", buffer_string_length(con->request.request),
				"\n", con->request.request);
	}

	if (con->request_count > 1 &&
	    con->request.request->ptr[0] == '\r' &&
	    con->request.request->ptr[1] == '\n') {
		/* we are in keep-alive and might get \r\n after a previous POST request.*/

		buffer_copy_string_len(con->parse_request, con->request.request->ptr + 2, buffer_string_length(con->request.request) - 2);
	} else {
		/* fill the local request buffer */
		buffer_copy_buffer(con->parse_request, con->request.request);
	}

	keep_alive_set = 0;
	con_length_set = 0;

	/* parse the first line of the request
	 *
	 * should be:
	 *
	 * <method> <uri> <protocol>\r\n
	 * */
	ilen = buffer_string_length(con->parse_request);
	for (i = 0, first = 0; i < ilen && line == 0; i++) {
		switch(con->parse_request->ptr[i]) {
		case '\r':
			if (con->parse_request->ptr[i+1] == '\n') {
				http_method_t r;
				char *nuri = NULL;
				size_t j, jlen;

				/* \r\n -> \0\0 */
				con->parse_request->ptr[i] = '\0';
				con->parse_request->ptr[i+1] = '\0';

				buffer_copy_string_len(con->request.request_line, con->parse_request->ptr, i);

				if (request_line_stage != 2) {
					con->http_status = 400;
					con->response.keep_alive = 0;
					con->keep_alive = 0;

					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "s", "incomplete request line -> 400");
						log_error_write(srv, __FILE__, __LINE__, "Sb",
								"request-header:\n",
								con->request.request);
					}
					return 0;
				}

				proto = con->parse_request->ptr + first;

				*(uri - 1) = '\0';
				*(proto - 1) = '\0';

				/* we got the first one :) */
				if (HTTP_METHOD_UNSET == (r = get_http_method_key(method))) {
					con->http_status = 501;
					con->response.keep_alive = 0;
					con->keep_alive = 0;

					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "s", "unknown http-method -> 501");
						log_error_write(srv, __FILE__, __LINE__, "Sb",
								"request-header:\n",
								con->request.request);
					}

					return 0;
				}

				con->request.http_method = r;

				/*
				 * RFC2616 says:
				 *
				 * HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
				 *
				 * */
				if (0 == strncmp(proto, "HTTP/", sizeof("HTTP/") - 1)) {
					char * major = proto + sizeof("HTTP/") - 1;
					char * minor = strchr(major, '.');
					char *err = NULL;
					int major_num = 0, minor_num = 0;

					int invalid_version = 0;

					if (NULL == minor || /* no dot */
					    minor == major || /* no major */
					    *(minor + 1) == '\0' /* no minor */) {
						invalid_version = 1;
					} else {
						*minor = '\0';
						major_num = strtol(major, &err, 10);

						if (*err != '\0') invalid_version = 1;

						*minor++ = '.';
						minor_num = strtol(minor, &err, 10);

						if (*err != '\0') invalid_version = 1;
					}

					if (invalid_version) {
						con->http_status = 400;
						con->keep_alive = 0;

						if (srv->srvconf.log_request_header_on_error) {
							log_error_write(srv, __FILE__, __LINE__, "s", "unknown protocol -> 400");
							log_error_write(srv, __FILE__, __LINE__, "Sb",
									"request-header:\n",
									con->request.request);
						}
						return 0;
					}

					if (major_num == 1 && minor_num == 1) {
						con->request.http_version = con->conf.allow_http11 ? HTTP_VERSION_1_1 : HTTP_VERSION_1_0;
					} else if (major_num == 1 && minor_num == 0) {
						con->request.http_version = HTTP_VERSION_1_0;
					} else {
						con->http_status = 505;

						if (srv->srvconf.log_request_header_on_error) {
							log_error_write(srv, __FILE__, __LINE__, "s", "unknown HTTP version -> 505");
							log_error_write(srv, __FILE__, __LINE__, "Sb",
									"request-header:\n",
									con->request.request);
						}
						return 0;
					}
				} else {
					con->http_status = 400;
					con->keep_alive = 0;

					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "s", "unknown protocol -> 400");
						log_error_write(srv, __FILE__, __LINE__, "Sb",
								"request-header:\n",
								con->request.request);
					}
					return 0;
				}

				if (0 == strncmp(uri, "http://", 7) &&
				    NULL != (nuri = strchr(uri + 7, '/'))) {
					reqline_host = uri + 7;
					reqline_hostlen = nuri - reqline_host;

					buffer_copy_string_len(con->request.uri, nuri, proto - nuri - 1);
				} else if (0 == strncmp(uri, "https://", 8) &&
				    NULL != (nuri = strchr(uri + 8, '/'))) {
					reqline_host = uri + 8;
					reqline_hostlen = nuri - reqline_host;

					buffer_copy_string_len(con->request.uri, nuri, proto - nuri - 1);
				} else {
					/* everything looks good so far */
					buffer_copy_string_len(con->request.uri, uri, proto - uri - 1);
				}

				/* check uri for invalid characters */
				jlen = buffer_string_length(con->request.uri);
				for (j = 0; j < jlen; j++) {
					if (!request_uri_is_valid_char(con->request.uri->ptr[j])) {
						unsigned char buf[2];
						con->http_status = 400;
						con->keep_alive = 0;

						if (srv->srvconf.log_request_header_on_error) {
							buf[0] = con->request.uri->ptr[j];
							buf[1] = '\0';

							if (con->request.uri->ptr[j] > 32 &&
							    con->request.uri->ptr[j] != 127) {
								/* the character is printable -> print it */
								log_error_write(srv, __FILE__, __LINE__, "ss",
										"invalid character in URI -> 400",
										buf);
							} else {
								/* a control-character, print ascii-code */
								log_error_write(srv, __FILE__, __LINE__, "sd",
										"invalid character in URI -> 400",
										con->request.uri->ptr[j]);
							}

							log_error_write(srv, __FILE__, __LINE__, "Sb",
									"request-header:\n",
									con->request.request);
						}

						return 0;
					}
				}

				buffer_copy_buffer(con->request.orig_uri, con->request.uri);

				con->http_status = 0;

				i++;
				line++;
				first = i+1;
			}
			break;
		case ' ':
			switch(request_line_stage) {
			case 0:
				/* GET|POST|... */
				method = con->parse_request->ptr + first;
				first = i + 1;
				break;
			case 1:
				/* /foobar/... */
				uri = con->parse_request->ptr + first;
				first = i + 1;
				break;
			default:
				/* ERROR, one space to much */
				con->http_status = 400;
				con->response.keep_alive = 0;
				con->keep_alive = 0;

				if (srv->srvconf.log_request_header_on_error) {
					log_error_write(srv, __FILE__, __LINE__, "s", "overlong request line -> 400");
					log_error_write(srv, __FILE__, __LINE__, "Sb",
							"request-header:\n",
							con->request.request);
				}
				return 0;
			}

			request_line_stage++;
			break;
		}
	}

	in_folding = 0;

	if (buffer_string_is_empty(con->request.uri)) {
		con->http_status = 400;
		con->response.keep_alive = 0;
		con->keep_alive = 0;

		if (srv->srvconf.log_request_header_on_error) {
			log_error_write(srv, __FILE__, __LINE__, "s", "no uri specified -> 400");
			log_error_write(srv, __FILE__, __LINE__, "Sb",
							"request-header:\n",
							con->request.request);
		}
		return 0;
	}

	if (reqline_host) {
		/* Insert as host header */
		data_string *ds;

		if (NULL == (ds = (data_string *)array_get_unused_element(con->request.headers, TYPE_STRING))) {
			ds = data_string_init();
		}

		buffer_copy_string_len(ds->key, CONST_STR_LEN("Host"));
		buffer_copy_string_len(ds->value, reqline_host, reqline_hostlen);
		array_insert_unique(con->request.headers, (data_unset *)ds);
		con->request.http_host = ds->value;
	}

	for (; i <= ilen && !done; i++) {
		char *cur = con->parse_request->ptr + i;

		if (is_key) {
			size_t j;
			int got_colon = 0;

			/**
			 * 1*<any CHAR except CTLs or separators>
			 * CTLs == 0-31 + 127, CHAR = 7-bit ascii (0..127)
			 *
			 */
			switch(*cur) {
			case ':':
				is_key = 0;

				value = cur + 1;

				if (is_ws_after_key == 0) {
					key_len = i - first;
				}
				is_ws_after_key = 0;

				break;
			case '(':
			case ')':
			case '<':
			case '>':
			case '@':
			case ',':
			case ';':
			case '\\':
			case '\"':
			case '/':
			case '[':
			case ']':
			case '?':
			case '=':
			case '{':
			case '}':
				con->http_status = 400;
				con->keep_alive = 0;
				con->response.keep_alive = 0;

				if (srv->srvconf.log_request_header_on_error) {
					log_error_write(srv, __FILE__, __LINE__, "sbsds",
						"invalid character in key", con->request.request, cur, *cur, "-> 400");

					log_error_write(srv, __FILE__, __LINE__, "Sb",
						"request-header:\n",
						con->request.request);
				}
				return 0;
			case ' ':
			case '\t':
				if (i == first) {
					is_key = 0;
					in_folding = 1;
					value = cur;

					break;
				}


				key_len = i - first;

				/* skip every thing up to the : */
				for (j = 1; !got_colon; j++) {
					switch(con->parse_request->ptr[j + i]) {
					case ' ':
					case '\t':
						/* skip WS */
						continue;
					case ':':
						/* ok, done; handle the colon the usual way */

						i += j - 1;
						got_colon = 1;
						is_ws_after_key = 1; /* we already know the key length */

						break;
					default:
						/* error */

						if (srv->srvconf.log_request_header_on_error) {
							log_error_write(srv, __FILE__, __LINE__, "s", "WS character in key -> 400");
							log_error_write(srv, __FILE__, __LINE__, "Sb",
								"request-header:\n",
								con->request.request);
						}

						con->http_status = 400;
						con->response.keep_alive = 0;
						con->keep_alive = 0;

						return 0;
					}
				}

				break;
			case '\r':
				if (con->parse_request->ptr[i+1] == '\n' && i == first) {
					/* End of Header */
					con->parse_request->ptr[i] = '\0';
					con->parse_request->ptr[i+1] = '\0';

					i++;

					done = 1;
				} else {
					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "s", "CR without LF -> 400");
						log_error_write(srv, __FILE__, __LINE__, "Sb",
							"request-header:\n",
							con->request.request);
					}

					con->http_status = 400;
					con->keep_alive = 0;
					con->response.keep_alive = 0;
					return 0;
				}
				break;
			default:
				if (*cur < 32 || ((unsigned char)*cur) >= 127) {
					con->http_status = 400;
					con->keep_alive = 0;
					con->response.keep_alive = 0;

					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "sbsds",
							"invalid character in key", con->request.request, cur, *cur, "-> 400");

						log_error_write(srv, __FILE__, __LINE__, "Sb",
							"request-header:\n",
							con->request.request);
					}

					return 0;
				}
				/* ok */
				break;
			}
		} else {
			switch(*cur) {
			case '\r':
				if (con->parse_request->ptr[i+1] == '\n') {
					data_string *ds = NULL;

					/* End of Headerline */
					con->parse_request->ptr[i] = '\0';
					con->parse_request->ptr[i+1] = '\0';

					if (in_folding) {
						buffer *key_b;
						/**
						 * we use a evil hack to handle the line-folding
						 * 
						 * As array_insert_unique() deletes 'ds' in the case of a duplicate
						 * ds points somewhere and we get a evil crash. As a solution we keep the old
						 * "key" and get the current value from the hash and append us
						 *
						 * */

						if (!key || !key_len) {
							/* 400 */

							if (srv->srvconf.log_request_header_on_error) {
								log_error_write(srv, __FILE__, __LINE__, "s", "WS at the start of first line -> 400");

								log_error_write(srv, __FILE__, __LINE__, "Sb",
									"request-header:\n",
									con->request.request);
							}


							con->http_status = 400;
							con->keep_alive = 0;
							con->response.keep_alive = 0;
							return 0;
						}

						key_b = buffer_init();
						buffer_copy_string_len(key_b, key, key_len);

						if (NULL != (ds = (data_string *)array_get_element(con->request.headers, key_b->ptr))) {
							buffer_append_string(ds->value, value);
						}

						buffer_free(key_b);
					} else {
						int s_len;
						key = con->parse_request->ptr + first;

						s_len = cur - value;

						/* strip trailing white-spaces */
						for (; s_len > 0 && 
								(value[s_len - 1] == ' ' || 
								 value[s_len - 1] == '\t'); s_len--);

						value[s_len] = '\0';

						if (s_len > 0) {
							int cmp = 0;
							if (NULL == (ds = (data_string *)array_get_unused_element(con->request.headers, TYPE_STRING))) {
								ds = data_string_init();
							}
							buffer_copy_string_len(ds->key, key, key_len);
							buffer_copy_string_len(ds->value, value, s_len);

							/* retreive values
							 *
							 *
							 * the list of options is sorted to simplify the search
							 */

							if (0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Connection")))) {
								array *vals;
								size_t vi;

								/* split on , */

								vals = srv->split_vals;

								array_reset(vals);

								http_request_split_value(vals, ds->value);

								for (vi = 0; vi < vals->used; vi++) {
									data_string *dsv = (data_string *)vals->data[vi];

									if (0 == buffer_caseless_compare(CONST_BUF_LEN(dsv->value), CONST_STR_LEN("keep-alive"))) {
										keep_alive_set = HTTP_CONNECTION_KEEPALIVE;

										break;
									} else if (0 == buffer_caseless_compare(CONST_BUF_LEN(dsv->value), CONST_STR_LEN("close"))) {
										keep_alive_set = HTTP_CONNECTION_CLOSE;

										break;
									}
								}

							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Content-Length")))) {
								char *err;
								unsigned long int r;
								size_t j, jlen;

								if (con_length_set) {
									con->http_status = 400;
									con->keep_alive = 0;

									if (srv->srvconf.log_request_header_on_error) {
										log_error_write(srv, __FILE__, __LINE__, "s",
												"duplicate Content-Length-header -> 400");
										log_error_write(srv, __FILE__, __LINE__, "Sb",
												"request-header:\n",
												con->request.request);
									}
									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}

								jlen = buffer_string_length(ds->value);
								for (j = 0; j < jlen; j++) {
									char c = ds->value->ptr[j];
									if (!isdigit((unsigned char)c)) {
										log_error_write(srv, __FILE__, __LINE__, "sbs",
												"content-length broken:", ds->value, "-> 400");

										con->http_status = 400;
										con->keep_alive = 0;

										array_insert_unique(con->request.headers, (data_unset *)ds);
										return 0;
									}
								}

								r = strtoul(ds->value->ptr, &err, 10);

								if (*err == '\0') {
									con_length_set = 1;
									con->request.content_length = r;
								} else {
									log_error_write(srv, __FILE__, __LINE__, "sbs",
											"content-length broken:", ds->value, "-> 400");

									con->http_status = 400;
									con->keep_alive = 0;

									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}
							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Content-Type")))) {
								/* if dup, only the first one will survive */
								if (!con->request.http_content_type) {
									con->request.http_content_type = ds->value->ptr;
								} else {
									con->http_status = 400;
									con->keep_alive = 0;

									if (srv->srvconf.log_request_header_on_error) {
										log_error_write(srv, __FILE__, __LINE__, "s",
												"duplicate Content-Type-header -> 400");
										log_error_write(srv, __FILE__, __LINE__, "Sb",
												"request-header:\n",
												con->request.request);
									}
									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}
							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Expect")))) {
								/* HTTP 2616 8.2.3
								 * Expect: 100-continue
								 *
								 *   -> (10.1.1)  100 (read content, process request, send final status-code)
								 *   -> (10.4.18) 417 (close)
								 *
								 * (not handled at all yet, we always send 417 here)
								 *
								 * What has to be added ?
								 * 1. handling of chunked request body
								 * 2. out-of-order sending from the HTTP/1.1 100 Continue
								 *    header
								 *
								 */

								if (srv->srvconf.reject_expect_100_with_417 && 0 == buffer_caseless_compare(CONST_BUF_LEN(ds->value), CONST_STR_LEN("100-continue"))) {
									con->http_status = 417;
									con->keep_alive = 0;
									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}
							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Host")))) {
								if (reqline_host) {
									/* ignore all host: headers as we got the host in the request line */
									ds->free((data_unset*) ds);
									ds = NULL;
								} else if (!con->request.http_host) {
									con->request.http_host = ds->value;
								} else {
									con->http_status = 400;
									con->keep_alive = 0;

									if (srv->srvconf.log_request_header_on_error) {
										log_error_write(srv, __FILE__, __LINE__, "s",
												"duplicate Host-header -> 400");
										log_error_write(srv, __FILE__, __LINE__, "Sb",
												"request-header:\n",
												con->request.request);
									}
									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}
							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("If-Modified-Since")))) {
								/* Proxies sometimes send dup headers
								 * if they are the same we ignore the second
								 * if not, we raise an error */
								if (!con->request.http_if_modified_since) {
									con->request.http_if_modified_since = ds->value->ptr;
								} else if (0 == strcasecmp(con->request.http_if_modified_since,
											ds->value->ptr)) {
									/* ignore it if they are the same */

									ds->free((data_unset *)ds);
									ds = NULL;
								} else {
									con->http_status = 400;
									con->keep_alive = 0;

									if (srv->srvconf.log_request_header_on_error) {
										log_error_write(srv, __FILE__, __LINE__, "s",
												"duplicate If-Modified-Since header -> 400");
										log_error_write(srv, __FILE__, __LINE__, "Sb",
												"request-header:\n",
												con->request.request);
									}
									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}
							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("If-None-Match")))) {
								/* if dup, only the first one will survive */
								if (!con->request.http_if_none_match) {
									con->request.http_if_none_match = ds->value->ptr;
								} else {
									ds->free((data_unset*) ds);
									ds = NULL;
								}
							} else if (cmp > 0 && 0 == (cmp = buffer_caseless_compare(CONST_BUF_LEN(ds->key), CONST_STR_LEN("Range")))) {
								if (!con->request.http_range) {
									/* bytes=.*-.* */

									if (0 == strncasecmp(ds->value->ptr, "bytes=", 6) &&
									    NULL != strchr(ds->value->ptr+6, '-')) {

										/* if dup, only the first one will survive */
										con->request.http_range = ds->value->ptr + 6;
									}
								} else {
									con->http_status = 400;
									con->keep_alive = 0;

									if (srv->srvconf.log_request_header_on_error) {
										log_error_write(srv, __FILE__, __LINE__, "s",
												"duplicate Range-header -> 400");
										log_error_write(srv, __FILE__, __LINE__, "Sb",
												"request-header:\n",
												con->request.request);
									}
									array_insert_unique(con->request.headers, (data_unset *)ds);
									return 0;
								}
							}

							if (ds) array_insert_unique(con->request.headers, (data_unset *)ds);
						} else {
							/* empty header-fields are not allowed by HTTP-RFC, we just ignore them */
						}
					}

					i++;
					first = i+1;
					is_key = 1;
					value = NULL;
#if 0
					/**
					 * for Bug 1230 keep the key_len a live
					 */
					key_len = 0; 
#endif
					in_folding = 0;
				} else {
					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "sbs",
								"CR without LF", con->request.request, "-> 400");
					}

					con->http_status = 400;
					con->keep_alive = 0;
					con->response.keep_alive = 0;
					return 0;
				}
				break;
			case ' ':
			case '\t':
				/* strip leading WS */
				if (value == cur) value = cur+1;
				/* fallthrough */
			default:
				if (*cur >= 0 && *cur < 32 && *cur != '\t') {
					if (srv->srvconf.log_request_header_on_error) {
						log_error_write(srv, __FILE__, __LINE__, "sds",
								"invalid char in header", (int)*cur, "-> 400");
					}

					con->http_status = 400;
					con->keep_alive = 0;

					return 0;
				}
				break;
			}
		}
	}

	con->header_len = i;

	/* do some post-processing */

	if (con->request.http_version == HTTP_VERSION_1_1) {
		if (keep_alive_set != HTTP_CONNECTION_CLOSE) {
			/* no Connection-Header sent */

			/* HTTP/1.1 -> keep-alive default TRUE */
			con->keep_alive = 1;
		} else {
			con->keep_alive = 0;
		}

		/* RFC 2616, 14.23 */
		if (con->request.http_host == NULL ||
		    buffer_string_is_empty(con->request.http_host)) {
			con->http_status = 400;
			con->response.keep_alive = 0;
			con->keep_alive = 0;

			if (srv->srvconf.log_request_header_on_error) {
				log_error_write(srv, __FILE__, __LINE__, "s", "HTTP/1.1 but Host missing -> 400");
				log_error_write(srv, __FILE__, __LINE__, "Sb",
						"request-header:\n",
						con->request.request);
			}
			return 0;
		}
	} else {
		if (keep_alive_set == HTTP_CONNECTION_KEEPALIVE) {
			/* no Connection-Header sent */

			/* HTTP/1.0 -> keep-alive default FALSE  */
			con->keep_alive = 1;
		} else {
			con->keep_alive = 0;
		}
	}

	/* check hostname field if it is set */
	if (NULL != con->request.http_host &&
	    0 != request_check_hostname(srv, con, con->request.http_host)) {

		if (srv->srvconf.log_request_header_on_error) {
			log_error_write(srv, __FILE__, __LINE__, "s",
					"Invalid Hostname -> 400");
			log_error_write(srv, __FILE__, __LINE__, "Sb",
					"request-header:\n",
					con->request.request);
		}

		con->http_status = 400;
		con->response.keep_alive = 0;
		con->keep_alive = 0;

		return 0;
	}

	switch(con->request.http_method) {
	case HTTP_METHOD_GET:
	case HTTP_METHOD_HEAD:
		/* content-length is forbidden for those */
		if (con_length_set && con->request.content_length != 0) {
			/* content-length is missing */
			log_error_write(srv, __FILE__, __LINE__, "s",
					"GET/HEAD with content-length -> 400");

			con->keep_alive = 0;
			con->http_status = 400;
			return 0;
		}
		break;
	case HTTP_METHOD_POST:
		/* content-length is required for them */
		if (!con_length_set) {
			/* content-length is missing */
			log_error_write(srv, __FILE__, __LINE__, "s",
					"POST-request, but content-length missing -> 411");

			con->keep_alive = 0;
			con->http_status = 411;
			return 0;

		}
		break;
	default:
		/* the may have a content-length */
		break;
	}


	/* check if we have read post data */
	if (con_length_set) {
		/* don't handle more the SSIZE_MAX bytes in content-length */
		if (con->request.content_length > SSIZE_MAX) {
			con->http_status = 413;
			con->keep_alive = 0;

			log_error_write(srv, __FILE__, __LINE__, "sos",
					"request-size too long:", (off_t) con->request.content_length, "-> 413");
			return 0;
		}

		/* divide by 1024 as srvconf.max_request_size is in kBytes */
		if (srv->srvconf.max_request_size != 0 &&
		    (con->request.content_length >> 10) > srv->srvconf.max_request_size) {
			/* the request body itself is larger then
			 * our our max_request_size
			 */

			con->http_status = 413;
			con->keep_alive = 0;

			log_error_write(srv, __FILE__, __LINE__, "sos",
					"request-size too long:", (off_t) con->request.content_length, "-> 413");
			return 0;
		}


		/* we have content */
		if (con->request.content_length != 0) {
			return 1;
		}
	}

	return 0;
}

int http_request_header_finished(server *srv, connection *con) {
	UNUSED(srv);

	if (buffer_string_length(con->request.request) < 4) return 0;

	if (0 == memcmp(con->request.request->ptr + buffer_string_length(con->request.request) - 4, CONST_STR_LEN("\r\n\r\n"))) return 1;
	if (NULL != strstr(con->request.request->ptr, "\r\n\r\n")) return 1;

	return 0;
}
