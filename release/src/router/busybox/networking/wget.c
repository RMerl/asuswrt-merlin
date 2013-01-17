/* vi: set sw=4 ts=4: */
/*
 * wget - retrieve a file using HTTP or FTP
 *
 * Chip Rosenthal Covad Communications <chip@laserlink.net>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 * Copyright (C) 2010 Bradley M. Kuhn <bkuhn@ebb.org>
 * Kuhn's copyrights are licensed GPLv2-or-later.  File as a whole remains GPLv2.
 */

//usage:#define wget_trivial_usage
//usage:	IF_FEATURE_WGET_LONG_OPTIONS(
//usage:       "[-c|--continue] [-s|--spider] [-q|--quiet] [-O|--output-document FILE]\n"
//usage:       "	[--header 'header: value'] [-Y|--proxy on/off] [-P DIR]\n"
//usage:       "	[--no-check-certificate] [-U|--user-agent AGENT]"
//usage:			IF_FEATURE_WGET_TIMEOUT(" [-T SEC]") " URL..."
//usage:	)
//usage:	IF_NOT_FEATURE_WGET_LONG_OPTIONS(
//usage:       "[-csq] [-O FILE] [-Y on/off] [-P DIR] [-U AGENT]"
//usage:			IF_FEATURE_WGET_TIMEOUT(" [-T SEC]") " URL..."
//usage:	)
//usage:#define wget_full_usage "\n\n"
//usage:       "Retrieve files via HTTP or FTP\n"
//usage:     "\n	-s	Spider mode - only check file existence"
//usage:     "\n	-c	Continue retrieval of aborted transfer"
//usage:     "\n	-q	Quiet"
//usage:     "\n	-P DIR	Save to DIR (default .)"
//usage:	IF_FEATURE_WGET_TIMEOUT(
//usage:     "\n	-T SEC	Network read timeout is SEC seconds"
//usage:	)
//usage:     "\n	-O FILE	Save to FILE ('-' for stdout)"
//usage:     "\n	-U STR	Use STR for User-Agent header"
//usage:     "\n	-Y	Use proxy ('on' or 'off')"

#include "libbb.h"

//#define log_io(...) bb_error_msg(__VA_ARGS__)
#define log_io(...) ((void)0)


struct host_info {
	char *allocated;
	const char *path;
	const char *user;
	char       *host;
	int         port;
	smallint    is_ftp;
};


/* Globals */
struct globals {
	off_t content_len;        /* Content-length of the file */
	off_t beg_range;          /* Range at which continue begins */
#if ENABLE_FEATURE_WGET_STATUSBAR
	off_t transferred;        /* Number of bytes transferred so far */
	const char *curfile;      /* Name of current file being transferred */
	bb_progress_t pmt;
#endif
        char *dir_prefix;
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
        char *post_data;
        char *extra_headers;
#endif
        char *fname_out;        /* where to direct output (-O) */
        const char *proxy_flag; /* Use proxies if env vars are set */
        const char *user_agent; /* "User-Agent" header field */
#if ENABLE_FEATURE_WGET_TIMEOUT
	unsigned timeout_seconds;
#endif
	int output_fd;
	int o_flags;
	smallint chunked;         /* chunked transfer encoding */
	smallint got_clen;        /* got content-length: from server  */
	/* Local downloads do benefit from big buffer.
	 * With 512 byte buffer, it was measured to be
	 * an order of magnitude slower than with big one.
	 */
	uint64_t just_to_align_next_member;
	char wget_buf[CONFIG_FEATURE_COPYBUF_KB*1024];
} FIX_ALIASING;
#define G (*ptr_to_globals)
#define INIT_G() do { \
        SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	IF_FEATURE_WGET_TIMEOUT(G.timeout_seconds = 900;) \
} while (0)


/* Must match option string! */
enum {
	WGET_OPT_CONTINUE   = (1 << 0),
	WGET_OPT_SPIDER     = (1 << 1),
	WGET_OPT_QUIET      = (1 << 2),
	WGET_OPT_OUTNAME    = (1 << 3),
	WGET_OPT_PREFIX     = (1 << 4),
	WGET_OPT_PROXY      = (1 << 5),
	WGET_OPT_USER_AGENT = (1 << 6),
	WGET_OPT_NETWORK_READ_TIMEOUT = (1 << 7),
	WGET_OPT_RETRIES    = (1 << 8),
	WGET_OPT_PASSIVE    = (1 << 9),
	WGET_OPT_HEADER     = (1 << 10) * ENABLE_FEATURE_WGET_LONG_OPTIONS,
	WGET_OPT_POST_DATA  = (1 << 11) * ENABLE_FEATURE_WGET_LONG_OPTIONS,
};

enum {
	PROGRESS_START = -1,
	PROGRESS_END   = 0,
	PROGRESS_BUMP  = 1,
};
#if ENABLE_FEATURE_WGET_STATUSBAR
static void progress_meter(int flag)
{
	if (option_mask32 & WGET_OPT_QUIET)
		return;

	if (flag == PROGRESS_START)
		bb_progress_init(&G.pmt, G.curfile);

	bb_progress_update(&G.pmt,
			G.beg_range,
			G.transferred,
			(G.chunked || !G.got_clen) ? 0 : G.beg_range + G.transferred + G.content_len
	);

	if (flag == PROGRESS_END) {
		bb_progress_free(&G.pmt);
		bb_putchar_stderr('\n');
		G.transferred = 0;
	}
}
#else
static ALWAYS_INLINE void progress_meter(int flag UNUSED_PARAM) { }
#endif


/* IPv6 knows scoped address types i.e. link and site local addresses. Link
 * local addresses can have a scope identifier to specify the
 * interface/link an address is valid on (e.g. fe80::1%eth0). This scope
 * identifier is only valid on a single node.
 *
 * RFC 4007 says that the scope identifier MUST NOT be sent across the wire,
 * unless all nodes agree on the semantic. Apache e.g. regards zone identifiers
 * in the Host header as invalid requests, see
 * https://issues.apache.org/bugzilla/show_bug.cgi?id=35122
 */
static void strip_ipv6_scope_id(char *host)
{
	char *scope, *cp;

	/* bbox wget actually handles IPv6 addresses without [], like
	 * wget "http://::1/xxx", but this is not standard.
	 * To save code, _here_ we do not support it. */

	if (host[0] != '[')
		return; /* not IPv6 */

	scope = strchr(host, '%');
	if (!scope)
		return;

	/* Remove the IPv6 zone identifier from the host address */
	cp = strchr(host, ']');
	if (!cp || (cp[1] != ':' && cp[1] != '\0')) {
		/* malformed address (not "[xx]:nn" or "[xx]") */
		return;
	}

	/* cp points to "]...", scope points to "%eth0]..." */
	overlapping_strcpy(scope, cp);
}

#if ENABLE_FEATURE_WGET_AUTHENTICATION
/* Base64-encode character string. */
static char *base64enc(const char *str)
{
	unsigned len = strlen(str);
	if (len > sizeof(G.wget_buf)/4*3 - 10) /* paranoia */
		len = sizeof(G.wget_buf)/4*3 - 10;
	bb_uuencode(G.wget_buf, str, len, bb_uuenc_tbl_base64);
	return G.wget_buf;
}
#endif

static char* sanitize_string(char *s)
{
	unsigned char *p = (void *) s;
	while (*p >= ' ')
		p++;
	*p = '\0';
	return s;
}

static FILE *open_socket(len_and_sockaddr *lsa)
{
	FILE *fp;

	/* glibc 2.4 seems to try seeking on it - ??! */
	/* hopefully it understands what ESPIPE means... */
	fp = fdopen(xconnect_stream(lsa), "r+");
	if (fp == NULL)
		bb_perror_msg_and_die(bb_msg_memory_exhausted);

	return fp;
}

/* Returns '\n' if it was seen, else '\0'. Trims at first '\r' or '\n' */
static char fgets_and_trim(FILE *fp)
{
	char c;
	char *buf_ptr;

	if (fgets(G.wget_buf, sizeof(G.wget_buf) - 1, fp) == NULL)
		bb_perror_msg_and_die("error getting response");

	buf_ptr = strchrnul(G.wget_buf, '\n');
	c = *buf_ptr;
	*buf_ptr = '\0';
	buf_ptr = strchrnul(G.wget_buf, '\r');
	*buf_ptr = '\0';

	log_io("< %s", G.wget_buf);

	return c;
}

static int ftpcmd(const char *s1, const char *s2, FILE *fp)
{
	int result;
	if (s1) {
		if (!s2)
			s2 = "";
		fprintf(fp, "%s%s\r\n", s1, s2);
		fflush(fp);
		log_io("> %s%s", s1, s2);
	}

	do {
		fgets_and_trim(fp);
	} while (!isdigit(G.wget_buf[0]) || G.wget_buf[3] != ' ');

	G.wget_buf[3] = '\0';
	result = xatoi_positive(G.wget_buf);
	G.wget_buf[3] = ' ';
	return result;
}

static void parse_url(const char *src_url, struct host_info *h)
{
	char *url, *p, *sp;

	free(h->allocated);
	h->allocated = url = xstrdup(src_url);

	if (strncmp(url, "http://", 7) == 0) {
		h->port = bb_lookup_port("http", "tcp", 80);
		h->host = url + 7;
		h->is_ftp = 0;
	} else if (strncmp(url, "ftp://", 6) == 0) {
		h->port = bb_lookup_port("ftp", "tcp", 21);
		h->host = url + 6;
		h->is_ftp = 1;
	} else
		bb_error_msg_and_die("not an http or ftp url: %s", sanitize_string(url));

	// FYI:
	// "Real" wget 'http://busybox.net?var=a/b' sends this request:
	//   'GET /?var=a/b HTTP 1.0'
	//   and saves 'index.html?var=a%2Fb' (we save 'b')
	// wget 'http://busybox.net?login=john@doe':
	//   request: 'GET /?login=john@doe HTTP/1.0'
	//   saves: 'index.html?login=john@doe' (we save '?login=john@doe')
	// wget 'http://busybox.net#test/test':
	//   request: 'GET / HTTP/1.0'
	//   saves: 'index.html' (we save 'test')
	//
	// We also don't add unique .N suffix if file exists...
	sp = strchr(h->host, '/');
	p = strchr(h->host, '?'); if (!sp || (p && sp > p)) sp = p;
	p = strchr(h->host, '#'); if (!sp || (p && sp > p)) sp = p;
	if (!sp) {
		h->path = "";
	} else if (*sp == '/') {
		*sp = '\0';
		h->path = sp + 1;
	} else { // '#' or '?'
		// http://busybox.net?login=john@doe is a valid URL
		// memmove converts to:
		// http:/busybox.nett?login=john@doe...
		memmove(h->host - 1, h->host, sp - h->host);
		h->host--;
		sp[-1] = '\0';
		h->path = sp;
	}

	// We used to set h->user to NULL here, but this interferes
	// with handling of code 302 ("object was moved")

	sp = strrchr(h->host, '@');
	if (sp != NULL) {
		// URL-decode "user:password" string before base64-encoding:
		// wget http://test:my%20pass@example.com should send
		// Authorization: Basic dGVzdDpteSBwYXNz
		// which decodes to "test:my pass".
		// Standard wget and curl do this too.
		*sp = '\0';
		h->user = percent_decode_in_place(h->host, /*strict:*/ 0);
		h->host = sp + 1;
	}

	sp = h->host;
}

static char *gethdr(FILE *fp)
{
	char *s, *hdrval;
	int c;

	/* *istrunc = 0; */

	/* retrieve header line */
	c = fgets_and_trim(fp);

	/* end of the headers? */
	if (G.wget_buf[0] == '\0')
		return NULL;

	/* convert the header name to lower case */
	for (s = G.wget_buf; isalnum(*s) || *s == '-' || *s == '.'; ++s) {
		/* tolower for "A-Z", no-op for "0-9a-z-." */
		*s |= 0x20;
	}

	/* verify we are at the end of the header name */
	if (*s != ':')
		bb_error_msg_and_die("bad header line: %s", sanitize_string(G.wget_buf));

	/* locate the start of the header value */
	*s++ = '\0';
	hdrval = skip_whitespace(s);

	if (c != '\n') {
		/* Rats! The buffer isn't big enough to hold the entire header value */
		while (c = getc(fp), c != EOF && c != '\n')
			continue;
	}

	return hdrval;
}

static FILE* prepare_ftp_session(FILE **dfpp, struct host_info *target, len_and_sockaddr *lsa)
{
	FILE *sfp;
	char *str;
	int port;

	if (!target->user)
		target->user = xstrdup("anonymous:busybox@");

	sfp = open_socket(lsa);
	if (ftpcmd(NULL, NULL, sfp) != 220)
		bb_error_msg_and_die("%s", sanitize_string(G.wget_buf + 4));

	/*
	 * Splitting username:password pair,
	 * trying to log in
	 */
	str = strchr(target->user, ':');
	if (str)
		*str++ = '\0';
	switch (ftpcmd("USER ", target->user, sfp)) {
	case 230:
		break;
	case 331:
		if (ftpcmd("PASS ", str, sfp) == 230)
			break;
		/* fall through (failed login) */
	default:
		bb_error_msg_and_die("ftp login: %s", sanitize_string(G.wget_buf + 4));
	}

	ftpcmd("TYPE I", NULL, sfp);

	/*
	 * Querying file size
	 */
	if (ftpcmd("SIZE ", target->path, sfp) == 213) {
		G.content_len = BB_STRTOOFF(G.wget_buf + 4, NULL, 10);
		if (G.content_len < 0 || errno) {
			bb_error_msg_and_die("SIZE value is garbage");
		}
		G.got_clen = 1;
	}

	/*
	 * Entering passive mode
	 */
	if (ftpcmd("PASV", NULL, sfp) != 227) {
 pasv_error:
		bb_error_msg_and_die("bad response to %s: %s", "PASV", sanitize_string(G.wget_buf));
	}
	// Response is "227 garbageN1,N2,N3,N4,P1,P2[)garbage]
	// Server's IP is N1.N2.N3.N4 (we ignore it)
	// Server's port for data connection is P1*256+P2
	str = strrchr(G.wget_buf, ')');
	if (str) str[0] = '\0';
	str = strrchr(G.wget_buf, ',');
	if (!str) goto pasv_error;
	port = xatou_range(str+1, 0, 255);
	*str = '\0';
	str = strrchr(G.wget_buf, ',');
	if (!str) goto pasv_error;
	port += xatou_range(str+1, 0, 255) * 256;
	set_nport(&lsa->u.sa, htons(port));

	*dfpp = open_socket(lsa);

	if (G.beg_range) {
		sprintf(G.wget_buf, "REST %"OFF_FMT"u", G.beg_range);
		if (ftpcmd(G.wget_buf, NULL, sfp) == 350)
			G.content_len -= G.beg_range;
	}

	if (ftpcmd("RETR ", target->path, sfp) > 150)
		bb_error_msg_and_die("bad response to %s: %s", "RETR", sanitize_string(G.wget_buf));

	return sfp;
}

static void NOINLINE retrieve_file_data(FILE *dfp)
{
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
# if ENABLE_FEATURE_WGET_TIMEOUT
	unsigned second_cnt;
# endif
	struct pollfd polldata;

	polldata.fd = fileno(dfp);
	polldata.events = POLLIN | POLLPRI;
#endif
	progress_meter(PROGRESS_START);

	if (G.chunked)
		goto get_clen;

	/* Loops only if chunked */
	while (1) {

#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
		/* Must use nonblocking I/O, otherwise fread will loop
		 * and *block* until it reads full buffer,
		 * which messes up progress bar and/or timeout logic.
		 * Because of nonblocking I/O, we need to dance
		 * very carefully around EAGAIN. See explanation at
		 * clearerr() call.
		 */
		ndelay_on(polldata.fd);
#endif
		while (1) {
			int n;
			unsigned rdsz;

			rdsz = sizeof(G.wget_buf);
			if (G.got_clen) {
				if (G.content_len < (off_t)sizeof(G.wget_buf)) {
					if ((int)G.content_len <= 0)
						break;
					rdsz = (unsigned)G.content_len;
				}
			}

#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
# if ENABLE_FEATURE_WGET_TIMEOUT
			second_cnt = G.timeout_seconds;
# endif
			while (1) {
				if (safe_poll(&polldata, 1, 1000) != 0)
					break; /* error, EOF, or data is available */
# if ENABLE_FEATURE_WGET_TIMEOUT
				if (second_cnt != 0 && --second_cnt == 0) {
					progress_meter(PROGRESS_END);
					bb_error_msg_and_die("download timed out");
				}
# endif
				/* Needed for "stalled" indicator */
				progress_meter(PROGRESS_BUMP);
			}

			/* fread internally uses read loop, which in our case
			 * is usually exited when we get EAGAIN.
			 * In this case, libc sets error marker on the stream.
			 * Need to clear it before next fread to avoid possible
			 * rare false positive ferror below. Rare because usually
			 * fread gets more than zero bytes, and we don't fall
			 * into if (n <= 0) ...
			 */
			clearerr(dfp);
			errno = 0;
#endif
			n = fread(G.wget_buf, 1, rdsz, dfp);
			/* man fread:
			 * If error occurs, or EOF is reached, the return value
			 * is a short item count (or zero).
			 * fread does not distinguish between EOF and error.
			 */
			if (n <= 0) {
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
				if (errno == EAGAIN) /* poll lied, there is no data? */
					continue; /* yes */
#endif
				if (ferror(dfp))
					bb_perror_msg_and_die(bb_msg_read_error);
				break; /* EOF, not error */
			}

			xwrite(G.output_fd, G.wget_buf, n);

#if ENABLE_FEATURE_WGET_STATUSBAR
			G.transferred += n;
			progress_meter(PROGRESS_BUMP);
#endif
			if (G.got_clen) {
				G.content_len -= n;
				if (G.content_len == 0)
					break;
			}
		}
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
		clearerr(dfp);
		ndelay_off(polldata.fd); /* else fgets can get very unhappy */
#endif
		if (!G.chunked)
			break;

		fgets_and_trim(dfp); /* Eat empty line */
 get_clen:
		fgets_and_trim(dfp);
		G.content_len = STRTOOFF(G.wget_buf, NULL, 16);
		/* FIXME: error check? */
		if (G.content_len == 0)
			break; /* all done! */
		G.got_clen = 1;
	}

	/* Draw full bar and free its resources */
	G.chunked = 0;  /* makes it show 100% even for chunked download */
	G.got_clen = 1; /* makes it show 100% even for download of (formerly) unknown size */
	progress_meter(PROGRESS_END);
}

static void download_one_url(const char *url)
{
	bool use_proxy;                 /* Use proxies if env vars are set  */
	int redir_limit;
	len_and_sockaddr *lsa;
	FILE *sfp;                      /* socket to web/ftp server         */
	FILE *dfp;                      /* socket to ftp server (data)      */
	char *proxy = NULL;
	char *fname_out_alloc;
	char *redirected_path = NULL;
	struct host_info server;
	struct host_info target;

	server.allocated = NULL;
	target.allocated = NULL;
	server.user = NULL;
	target.user = NULL;

	parse_url(url, &target);

	/* Use the proxy if necessary */
	use_proxy = (strcmp(G.proxy_flag, "off") != 0);
	if (use_proxy) {
		proxy = getenv(target.is_ftp ? "ftp_proxy" : "http_proxy");
		use_proxy = (proxy && proxy[0]);
		if (use_proxy)
			parse_url(proxy, &server);
	}
	if (!use_proxy) {
		server.port = target.port;
		if (ENABLE_FEATURE_IPV6) {
			//free(server.allocated); - can't be non-NULL
			server.host = server.allocated = xstrdup(target.host);
		} else {
			server.host = target.host;
		}
	}

	if (ENABLE_FEATURE_IPV6)
		strip_ipv6_scope_id(target.host);

	/* If there was no -O FILE, guess output filename */
	fname_out_alloc = NULL;
	if (!(option_mask32 & WGET_OPT_OUTNAME)) {
		G.fname_out = bb_get_last_path_component_nostrip(target.path);
		/* handle "wget http://kernel.org//" */
		if (G.fname_out[0] == '/' || !G.fname_out[0])
			G.fname_out = (char*)"index.html";
		/* -P DIR is considered only if there was no -O FILE */
		else {
			if (G.dir_prefix)
				G.fname_out = fname_out_alloc = concat_path_file(G.dir_prefix, G.fname_out);
			else {
				/* redirects may free target.path later, need to make a copy */
				G.fname_out = fname_out_alloc = xstrdup(G.fname_out);
			}
		}
	}
#if ENABLE_FEATURE_WGET_STATUSBAR
	G.curfile = bb_get_last_path_component_nostrip(G.fname_out);
#endif

	/* Determine where to start transfer */
	G.beg_range = 0;
	if (option_mask32 & WGET_OPT_CONTINUE) {
		G.output_fd = open(G.fname_out, O_WRONLY);
		if (G.output_fd >= 0) {
			G.beg_range = xlseek(G.output_fd, 0, SEEK_END);
		}
		/* File doesn't exist. We do not create file here yet.
		 * We are not sure it exists on remote side */
	}

	redir_limit = 5;
 resolve_lsa:
	lsa = xhost2sockaddr(server.host, server.port);
	if (!(option_mask32 & WGET_OPT_QUIET)) {
		char *s = xmalloc_sockaddr2dotted(&lsa->u.sa);
		fprintf(stderr, "Connecting to %s (%s)\n", server.host, s);
		free(s);
	}
 establish_session:
	/*G.content_len = 0; - redundant, got_clen = 0 is enough */
	G.got_clen = 0;
	G.chunked = 0;
	if (use_proxy || !target.is_ftp) {
		/*
		 *  HTTP session
		 */
		char *str;
		int status;


		/* Open socket to http server */
		sfp = open_socket(lsa);

		/* Send HTTP request */
		if (use_proxy) {
			fprintf(sfp, "GET %stp://%s/%s HTTP/1.1\r\n",
				target.is_ftp ? "f" : "ht", target.host,
				target.path);
		} else {
			if (option_mask32 & WGET_OPT_POST_DATA)
				fprintf(sfp, "POST /%s HTTP/1.1\r\n", target.path);
			else
				fprintf(sfp, "GET /%s HTTP/1.1\r\n", target.path);
		}

		fprintf(sfp, "Host: %s\r\nUser-Agent: %s\r\n",
			target.host, G.user_agent);

		/* Ask server to close the connection as soon as we are done
		 * (IOW: we do not intend to send more requests)
		 */
		fprintf(sfp, "Connection: close\r\n");

#if ENABLE_FEATURE_WGET_AUTHENTICATION
		if (target.user) {
			fprintf(sfp, "Proxy-Authorization: Basic %s\r\n"+6,
				base64enc(target.user));
		}
		if (use_proxy && server.user) {
			fprintf(sfp, "Proxy-Authorization: Basic %s\r\n",
				base64enc(server.user));
		}
#endif

		if (G.beg_range)
			fprintf(sfp, "Range: bytes=%"OFF_FMT"u-\r\n", G.beg_range);

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
		if (G.extra_headers)
			fputs(G.extra_headers, sfp);

		if (option_mask32 & WGET_OPT_POST_DATA) {
			fprintf(sfp,
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Content-Length: %u\r\n"
				"\r\n"
				"%s",
				(int) strlen(G.post_data), G.post_data
			);
		} else
#endif
		{
			fprintf(sfp, "\r\n");
		}

		fflush(sfp);

		/*
		 * Retrieve HTTP response line and check for "200" status code.
		 */
 read_response:
		fgets_and_trim(sfp);

		str = G.wget_buf;
		str = skip_non_whitespace(str);
		str = skip_whitespace(str);
		// FIXME: no error check
		// xatou wouldn't work: "200 OK"
		status = atoi(str);
		switch (status) {
		case 0:
		case 100:
			while (gethdr(sfp) != NULL)
				/* eat all remaining headers */;
			goto read_response;
		case 200:
/*
Response 204 doesn't say "null file", it says "metadata
has changed but data didn't":

"10.2.5 204 No Content
The server has fulfilled the request but does not need to return
an entity-body, and might want to return updated metainformation.
The response MAY include new or updated metainformation in the form
of entity-headers, which if present SHOULD be associated with
the requested variant.

If the client is a user agent, it SHOULD NOT change its document
view from that which caused the request to be sent. This response
is primarily intended to allow input for actions to take place
without causing a change to the user agent's active document view,
although any new or updated metainformation SHOULD be applied
to the document currently in the user agent's active view.

The 204 response MUST NOT include a message-body, and thus
is always terminated by the first empty line after the header fields."

However, in real world it was observed that some web servers
(e.g. Boa/0.94.14rc21) simply use code 204 when file size is zero.
*/
		case 204:
			break;
		case 300:  /* redirection */
		case 301:
		case 302:
		case 303:
			break;
		case 206:
			if (G.beg_range)
				break;
			/* fall through */
		default:
			bb_error_msg_and_die("server returned error: %s", sanitize_string(G.wget_buf));
		}

		/*
		 * Retrieve HTTP headers.
		 */
		while ((str = gethdr(sfp)) != NULL) {
			static const char keywords[] ALIGN1 =
				"content-length\0""transfer-encoding\0""location\0";
			enum {
				KEY_content_length = 1, KEY_transfer_encoding, KEY_location
			};
			smalluint key;

			/* gethdr converted "FOO:" string to lowercase */

			/* strip trailing whitespace */
			char *s = strchrnul(str, '\0') - 1;
			while (s >= str && (*s == ' ' || *s == '\t')) {
				*s = '\0';
				s--;
			}
			key = index_in_strings(keywords, G.wget_buf) + 1;
			if (key == KEY_content_length) {
				G.content_len = BB_STRTOOFF(str, NULL, 10);
				if (G.content_len < 0 || errno) {
					bb_error_msg_and_die("content-length %s is garbage", sanitize_string(str));
				}
				G.got_clen = 1;
				continue;
			}
			if (key == KEY_transfer_encoding) {
				if (strcmp(str_tolower(str), "chunked") != 0)
					bb_error_msg_and_die("transfer encoding '%s' is not supported", sanitize_string(str));
				G.chunked = 1;
			}
			if (key == KEY_location && status >= 300) {
				if (--redir_limit == 0)
					bb_error_msg_and_die("too many redirections");
				fclose(sfp);
				if (str[0] == '/') {
					free(redirected_path);
					target.path = redirected_path = xstrdup(str+1);
					/* lsa stays the same: it's on the same server */
				} else {
					parse_url(str, &target);
					if (!use_proxy) {
						free(server.allocated);
						server.allocated = NULL;
						server.host = target.host;
						/* strip_ipv6_scope_id(target.host); - no! */
						/* we assume remote never gives us IPv6 addr with scope id */
						server.port = target.port;
						free(lsa);
						goto resolve_lsa;
					} /* else: lsa stays the same: we use proxy */
				}
				goto establish_session;
			}
		}
//		if (status >= 300)
//			bb_error_msg_and_die("bad redirection (no Location: header from server)");

		/* For HTTP, data is pumped over the same connection */
		dfp = sfp;

	} else {
		/*
		 *  FTP session
		 */
		sfp = prepare_ftp_session(&dfp, &target, lsa);
	}

	free(lsa);

	if (!(option_mask32 & WGET_OPT_SPIDER)) {
		if (G.output_fd < 0)
			G.output_fd = xopen(G.fname_out, G.o_flags);
		retrieve_file_data(dfp);
		if (!(option_mask32 & WGET_OPT_OUTNAME)) {
			xclose(G.output_fd);
			G.output_fd = -1;
		}
	}

	if (dfp != sfp) {
		/* It's ftp. Close data connection properly */
		fclose(dfp);
		if (ftpcmd(NULL, NULL, sfp) != 226)
			bb_error_msg_and_die("ftp error: %s", sanitize_string(G.wget_buf + 4));
		/* ftpcmd("QUIT", NULL, sfp); - why bother? */
	}
	fclose(sfp);

	free(server.allocated);
	free(target.allocated);
	free(fname_out_alloc);
	free(redirected_path);
}

int wget_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int wget_main(int argc UNUSED_PARAM, char **argv)
{
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	static const char wget_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"continue\0"         No_argument       "c"
//FIXME: -s isn't --spider, it's --save-headers!
		"spider\0"           No_argument       "s"
		"quiet\0"            No_argument       "q"
		"output-document\0"  Required_argument "O"
		"directory-prefix\0" Required_argument "P"
		"proxy\0"            Required_argument "Y"
		"user-agent\0"       Required_argument "U"
#if ENABLE_FEATURE_WGET_TIMEOUT
		"timeout\0"          Required_argument "T"
#endif
		/* Ignored: */
		// "tries\0"            Required_argument "t"
		/* Ignored (we always use PASV): */
		"passive-ftp\0"      No_argument       "\xff"
		"header\0"           Required_argument "\xfe"
		"post-data\0"        Required_argument "\xfd"
		/* Ignored (we don't do ssl) */
		"no-check-certificate\0" No_argument   "\xfc"
		;
#endif

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	llist_t *headers_llist = NULL;
#endif

	INIT_G();

	IF_FEATURE_WGET_TIMEOUT(G.timeout_seconds = 900;)
	G.proxy_flag = "on";   /* use proxies if env vars are set */
	G.user_agent = "Wget"; /* "User-Agent" header field */

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	applet_long_options = wget_longopts;
#endif
	opt_complementary = "-1" IF_FEATURE_WGET_TIMEOUT(":T+") IF_FEATURE_WGET_LONG_OPTIONS(":\xfe::");
	getopt32(argv, "csqO:P:Y:U:T:" /*ignored:*/ "t:",
		&G.fname_out, &G.dir_prefix,
		&G.proxy_flag, &G.user_agent,
		IF_FEATURE_WGET_TIMEOUT(&G.timeout_seconds) IF_NOT_FEATURE_WGET_TIMEOUT(NULL),
		NULL /* -t RETRIES */
		IF_FEATURE_WGET_LONG_OPTIONS(, &headers_llist)
		IF_FEATURE_WGET_LONG_OPTIONS(, &G.post_data)
	);
	argv += optind;

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	if (headers_llist) {
		int size = 1;
		char *cp;
		llist_t *ll = headers_llist;
		while (ll) {
			size += strlen(ll->data) + 2;
			ll = ll->link;
		}
		G.extra_headers = cp = xmalloc(size);
		while (headers_llist) {
			cp += sprintf(cp, "%s\r\n", (char*)llist_pop(&headers_llist));
		}
	}
#endif

	G.output_fd = -1;
	G.o_flags = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL;
	if (G.fname_out) { /* -O FILE ? */
		if (LONE_DASH(G.fname_out)) { /* -O - ? */
			G.output_fd = 1;
			option_mask32 &= ~WGET_OPT_CONTINUE;
		}
		/* compat with wget: -O FILE can overwrite */
		G.o_flags = O_WRONLY | O_CREAT | O_TRUNC;
	}

	while (*argv)
		download_one_url(*argv++);

	if (G.output_fd >= 0)
		xclose(G.output_fd);

	return EXIT_SUCCESS;
}
