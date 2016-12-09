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

//config:config WGET
//config:	bool "wget"
//config:	default y
//config:	help
//config:	  wget is a utility for non-interactive download of files from HTTP
//config:	  and FTP servers.
//config:
//config:config FEATURE_WGET_STATUSBAR
//config:	bool "Enable a nifty process meter (+2k)"
//config:	default y
//config:	depends on WGET
//config:	help
//config:	  Enable the transfer progress bar for wget transfers.
//config:
//config:config FEATURE_WGET_AUTHENTICATION
//config:	bool "Enable HTTP authentication"
//config:	default y
//config:	depends on WGET
//config:	help
//config:	  Support authenticated HTTP transfers.
//config:
//config:config FEATURE_WGET_LONG_OPTIONS
//config:	bool "Enable long options"
//config:	default y
//config:	depends on WGET && LONG_OPTS
//config:	help
//config:	  Support long options for the wget applet.
//config:
//config:config FEATURE_WGET_TIMEOUT
//config:	bool "Enable timeout option -T SEC"
//config:	default y
//config:	depends on WGET
//config:	help
//config:	  Supports network read and connect timeouts for wget,
//config:	  so that wget will give up and timeout, through the -T
//config:	  command line option.
//config:
//config:	  Currently only connect and network data read timeout are
//config:	  supported (i.e., timeout is not applied to the DNS query). When
//config:	  FEATURE_WGET_LONG_OPTIONS is also enabled, the --timeout option
//config:	  will work in addition to -T.
//config:
//config:config FEATURE_WGET_OPENSSL
//config:	bool "Try to connect to HTTPS using openssl"
//config:	default y
//config:	depends on WGET
//config:	help
//config:	  Choose how wget establishes SSL connection for https:// URLs.
//config:
//config:	  Busybox itself contains no SSL code. wget will spawn
//config:	  a helper program to talk over HTTPS.
//config:
//config:	  OpenSSL has a simple SSL client for debug purposes.
//config:	  If you select "openssl" helper, wget will effectively call
//config:	  "openssl s_client -quiet -connect IP:443 2>/dev/null"
//config:	  and pipe its data through it.
//config:	  Note inconvenient API: host resolution is done twice,
//config:	  and there is no guarantee openssl's idea of IPv6 address
//config:	  format is the same as ours.
//config:	  Another problem is that s_client prints debug information
//config:	  to stderr, and it needs to be suppressed. This means
//config:	  all error messages get suppressed too.
//config:	  openssl is also a big binary, often dynamically linked
//config:	  against ~15 libraries.
//config:
//config:config FEATURE_WGET_SSL_HELPER
//config:	bool "Try to connect to HTTPS using ssl_helper"
//config:	default y
//config:	depends on WGET
//config:	help
//config:	  Choose how wget establishes SSL connection for https:// URLs.
//config:
//config:	  Busybox itself contains no SSL code. wget will spawn
//config:	  a helper program to talk over HTTPS.
//config:
//config:	  ssl_helper is a tool which can be built statically
//config:	  from busybox sources against a small embedded SSL library.
//config:	  Please see networking/ssl_helper/README.
//config:	  It does not require double host resolution and emits
//config:	  error messages to stderr.
//config:
//config:	  Precompiled static binary may be available at
//config:	  http://busybox.net/downloads/binaries/

//applet:IF_WGET(APPLET(wget, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_WGET) += wget.o

//usage:#define wget_trivial_usage
//usage:	IF_FEATURE_WGET_LONG_OPTIONS(
//usage:       "[-c|--continue] [-s|--spider] [-q|--quiet] [-O|--output-document FILE]\n"
//usage:       "	[--header 'header: value'] [-Y|--proxy on/off] [-P DIR]\n"
/* Since we ignore these opts, we don't show them in --help */
/* //usage:    "	[--no-check-certificate] [--no-cache] [--passive-ftp] [-t TRIES]" */
/* //usage:    "	[-nv] [-nc] [-nH] [-np]" */
//usage:       "	[-U|--user-agent AGENT]" IF_FEATURE_WGET_TIMEOUT(" [-T SEC]") " URL..."
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

#if 0
# define log_io(...) bb_error_msg(__VA_ARGS__)
# define SENDFMT(fp, fmt, ...) \
	do { \
		log_io("> " fmt, ##__VA_ARGS__); \
		fprintf(fp, fmt, ##__VA_ARGS__); \
	} while (0);
#else
# define log_io(...) ((void)0)
# define SENDFMT(fp, fmt, ...) fprintf(fp, fmt, ##__VA_ARGS__)
#endif


struct host_info {
	char *allocated;
	const char *path;
	char       *user;
	const char *protocol;
	char       *host;
	int         port;
};
static const char P_FTP[] ALIGN1 = "ftp";
static const char P_HTTP[] ALIGN1 = "http";
#if ENABLE_FEATURE_WGET_OPENSSL || ENABLE_FEATURE_WGET_SSL_HELPER
static const char P_HTTPS[] ALIGN1 = "https";
#endif

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
/* User-specified headers prevent using our corresponding built-in headers.  */
enum {
	HDR_HOST          = (1<<0),
	HDR_USER_AGENT    = (1<<1),
	HDR_RANGE         = (1<<2),
	HDR_AUTH          = (1<<3) * ENABLE_FEATURE_WGET_AUTHENTICATION,
	HDR_PROXY_AUTH    = (1<<4) * ENABLE_FEATURE_WGET_AUTHENTICATION,
};
static const char wget_user_headers[] ALIGN1 =
	"Host:\0"
	"User-Agent:\0"
	"Range:\0"
# if ENABLE_FEATURE_WGET_AUTHENTICATION
	"Authorization:\0"
	"Proxy-Authorization:\0"
# endif
	;
# define USR_HEADER_HOST       (G.user_headers & HDR_HOST)
# define USR_HEADER_USER_AGENT (G.user_headers & HDR_USER_AGENT)
# define USR_HEADER_RANGE      (G.user_headers & HDR_RANGE)
# define USR_HEADER_AUTH       (G.user_headers & HDR_AUTH)
# define USR_HEADER_PROXY_AUTH (G.user_headers & HDR_PROXY_AUTH)
#else /* No long options, no user-headers :( */
# define USR_HEADER_HOST       0
# define USR_HEADER_USER_AGENT 0
# define USR_HEADER_RANGE      0
# define USR_HEADER_AUTH       0
# define USR_HEADER_PROXY_AUTH 0
#endif

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
	unsigned char user_headers; /* Headers mentioned by the user */
#endif
	char *fname_out;        /* where to direct output (-O) */
	const char *proxy_flag; /* Use proxies if env vars are set */
	const char *user_agent; /* "User-Agent" header field */
#if ENABLE_FEATURE_WGET_TIMEOUT
	unsigned timeout_seconds;
	bool die_if_timed_out;
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
} while (0)
#define FINI_G() do { \
	FREE_PTR_TO_GLOBALS(); \
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

#if ENABLE_FEATURE_WGET_TIMEOUT
static void alarm_handler(int sig UNUSED_PARAM)
{
	/* This is theoretically unsafe (uses stdio and malloc in signal handler) */
	if (G.die_if_timed_out)
		bb_error_msg_and_die("download timed out");
}
static void set_alarm(void)
{
	if (G.timeout_seconds) {
		alarm(G.timeout_seconds);
		G.die_if_timed_out = 1;
	}
}
# define clear_alarm() ((void)(G.die_if_timed_out = 0))
#else
# define set_alarm()   ((void)0)
# define clear_alarm() ((void)0)
#endif

static FILE *open_socket(len_and_sockaddr *lsa)
{
	int fd;
	FILE *fp;

	set_alarm();
	fd = xconnect_stream(lsa);
	clear_alarm();

	/* glibc 2.4 seems to try seeking on it - ??! */
	/* hopefully it understands what ESPIPE means... */
	fp = fdopen(fd, "r+");
	if (!fp)
		bb_perror_msg_and_die(bb_msg_memory_exhausted);

	return fp;
}

/* Returns '\n' if it was seen, else '\0'. Trims at first '\r' or '\n' */
static char fgets_and_trim(FILE *fp)
{
	char c;
	char *buf_ptr;

	set_alarm();
	if (fgets(G.wget_buf, sizeof(G.wget_buf) - 1, fp) == NULL)
		bb_perror_msg_and_die("error getting response");
	clear_alarm();

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

	h->protocol = P_FTP;
	p = strstr(url, "://");
	if (p) {
		*p = '\0';
		h->host = p + 3;
		if (strcmp(url, P_FTP) == 0) {
			h->port = bb_lookup_port(P_FTP, "tcp", 21);
		} else
#if ENABLE_FEATURE_WGET_OPENSSL || ENABLE_FEATURE_WGET_SSL_HELPER
		if (strcmp(url, P_HTTPS) == 0) {
			h->port = bb_lookup_port(P_HTTPS, "tcp", 443);
			h->protocol = P_HTTPS;
		} else
#endif
		if (strcmp(url, P_HTTP) == 0) {
 http:
			h->port = bb_lookup_port(P_HTTP, "tcp", 80);
			h->protocol = P_HTTP;
		} else {
			*p = ':';
			bb_error_msg_and_die("not an http or ftp url: %s", sanitize_string(url));
		}
	} else {
		// GNU wget is user-friendly and falls back to http://
		h->host = url;
		goto http;
	}

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

	sp = strrchr(h->host, '@');
	if (sp != NULL) {
		// URL-decode "user:password" string before base64-encoding:
		// wget http://test:my%20pass@example.com should send
		// Authorization: Basic dGVzdDpteSBwYXNz
		// which decodes to "test:my pass".
		// Standard wget and curl do this too.
		*sp = '\0';
		free(h->user);
		h->user = xstrdup(percent_decode_in_place(h->host, /*strict:*/ 0));
		h->host = sp + 1;
	}
	/* else: h->user remains NULL, or as set by original request
	 * before redirect (if we are here after a redirect).
	 */
}

static char *gethdr(FILE *fp)
{
	char *s, *hdrval;
	int c;

	/* retrieve header line */
	c = fgets_and_trim(fp);

	/* end of the headers? */
	if (G.wget_buf[0] == '\0')
		return NULL;

	/* convert the header name to lower case */
	for (s = G.wget_buf; isalnum(*s) || *s == '-' || *s == '.' || *s == '_'; ++s) {
		/*
		 * No-op for 20-3f and 60-7f. "0-9a-z-." are in these ranges.
		 * 40-5f range ("@A-Z[\]^_") maps to 60-7f.
		 * "A-Z" maps to "a-z".
		 * "@[\]" can't occur in header names.
		 * "^_" maps to "~,DEL" (which is wrong).
		 * "^" was never seen yet, "_" was seen from web.archive.org
		 * (x-archive-orig-x_commoncrawl_Signature: HEXSTRING).
		 */
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

static void reset_beg_range_to_zero(void)
{
	bb_error_msg("restart failed");
	G.beg_range = 0;
	xlseek(G.output_fd, 0, SEEK_SET);
	/* Done at the end instead: */
	/* ftruncate(G.output_fd, 0); */
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

	if (G.beg_range != 0) {
		sprintf(G.wget_buf, "REST %"OFF_FMT"u", G.beg_range);
		if (ftpcmd(G.wget_buf, NULL, sfp) == 350)
			G.content_len -= G.beg_range;
		else
			reset_beg_range_to_zero();
	}

	if (ftpcmd("RETR ", target->path, sfp) > 150)
		bb_error_msg_and_die("bad response to %s: %s", "RETR", sanitize_string(G.wget_buf));

	return sfp;
}

#if ENABLE_FEATURE_WGET_OPENSSL
static int spawn_https_helper_openssl(const char *host, unsigned port)
{
	char *allocated = NULL;
	int sp[2];
	int pid;
	IF_FEATURE_WGET_SSL_HELPER(volatile int child_failed = 0;)

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0)
		/* Kernel can have AF_UNIX support disabled */
		bb_perror_msg_and_die("socketpair");

	if (!strchr(host, ':'))
		host = allocated = xasprintf("%s:%u", host, port);

	fflush_all();
	pid = xvfork();
	if (pid == 0) {
		/* Child */
		char *argv[6];

		close(sp[0]);
		xmove_fd(sp[1], 0);
		xdup2(0, 1);
		/*
		 * openssl s_client -quiet -connect www.kernel.org:443 2>/dev/null
		 * It prints some debug stuff on stderr, don't know how to suppress it.
		 * Work around by dev-nulling stderr. We lose all error messages :(
		 */
		xmove_fd(2, 3);
		xopen("/dev/null", O_RDWR);
		argv[0] = (char*)"openssl";
		argv[1] = (char*)"s_client";
		argv[2] = (char*)"-quiet";
		argv[3] = (char*)"-connect";
		argv[4] = (char*)host;
		argv[5] = NULL;
		BB_EXECVP(argv[0], argv);
		xmove_fd(3, 2);
# if ENABLE_FEATURE_WGET_SSL_HELPER
		child_failed = 1;
		xfunc_die();
# else
		bb_perror_msg_and_die("can't execute '%s'", argv[0]);
# endif
		/* notreached */
	}

	/* Parent */
	free(allocated);
	close(sp[1]);
# if ENABLE_FEATURE_WGET_SSL_HELPER
	if (child_failed) {
		close(sp[0]);
		return -1;
	}
# endif
	return sp[0];
}
#endif

/* See networking/ssl_helper/README how to build one */
#if ENABLE_FEATURE_WGET_SSL_HELPER
static void spawn_https_helper_small(int network_fd)
{
	int sp[2];
	int pid;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0)
		/* Kernel can have AF_UNIX support disabled */
		bb_perror_msg_and_die("socketpair");

	pid = BB_MMU ? xfork() : xvfork();
	if (pid == 0) {
		/* Child */
		char *argv[3];

		close(sp[0]);
		xmove_fd(sp[1], 0);
		xdup2(0, 1);
		xmove_fd(network_fd, 3);
		/*
		 * A simple ssl/tls helper
		 */
		argv[0] = (char*)"ssl_helper";
		argv[1] = (char*)"-d3";
		argv[2] = NULL;
		BB_EXECVP(argv[0], argv);
		bb_perror_msg_and_die("can't execute '%s'", argv[0]);
		/* notreached */
	}

	/* Parent */
	close(sp[1]);
	xmove_fd(sp[0], network_fd);
}
#endif

static void NOINLINE retrieve_file_data(FILE *dfp)
{
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
# if ENABLE_FEATURE_WGET_TIMEOUT
	unsigned second_cnt = G.timeout_seconds;
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
		 * clearerr() calls.
		 */
		ndelay_on(polldata.fd);
#endif
		while (1) {
			int n;
			unsigned rdsz;

#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
			/* fread internally uses read loop, which in our case
			 * is usually exited when we get EAGAIN.
			 * In this case, libc sets error marker on the stream.
			 * Need to clear it before next fread to avoid possible
			 * rare false positive ferror below. Rare because usually
			 * fread gets more than zero bytes, and we don't fall
			 * into if (n <= 0) ...
			 */
			clearerr(dfp);
#endif
			errno = 0;
			rdsz = sizeof(G.wget_buf);
			if (G.got_clen) {
				if (G.content_len < (off_t)sizeof(G.wget_buf)) {
					if ((int)G.content_len <= 0)
						break;
					rdsz = (unsigned)G.content_len;
				}
			}
			n = fread(G.wget_buf, 1, rdsz, dfp);

			if (n > 0) {
				xwrite(G.output_fd, G.wget_buf, n);
#if ENABLE_FEATURE_WGET_STATUSBAR
				G.transferred += n;
#endif
				if (G.got_clen) {
					G.content_len -= n;
					if (G.content_len == 0)
						break;
				}
#if ENABLE_FEATURE_WGET_TIMEOUT
				second_cnt = G.timeout_seconds;
#endif
				goto bump;
			}

			/* n <= 0.
			 * man fread:
			 * If error occurs, or EOF is reached, the return value
			 * is a short item count (or zero).
			 * fread does not distinguish between EOF and error.
			 */
			if (errno != EAGAIN) {
				if (ferror(dfp)) {
					progress_meter(PROGRESS_END);
					bb_perror_msg_and_die(bb_msg_read_error);
				}
				break; /* EOF, not error */
			}

#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
			/* It was EAGAIN. There is no data. Wait up to one second
			 * then abort if timed out, or update the bar and try reading again.
			 */
			if (safe_poll(&polldata, 1, 1000) == 0) {
# if ENABLE_FEATURE_WGET_TIMEOUT
				if (second_cnt != 0 && --second_cnt == 0) {
					progress_meter(PROGRESS_END);
					bb_error_msg_and_die("download timed out");
				}
# endif
				/* We used to loop back to poll here,
				 * but there is no great harm in letting fread
				 * to try reading anyway.
				 */
			}
#endif
 bump:
			/* Need to do it _every_ second for "stalled" indicator
			 * to be shown properly.
			 */
			progress_meter(PROGRESS_BUMP);
		} /* while (reading data) */

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
		/*
		 * Note that fgets may result in some data being buffered in dfp.
		 * We loop back to fread, which will retrieve this data.
		 * Also note that code has to be arranged so that fread
		 * is done _before_ one-second poll wait - poll doesn't know
		 * about stdio buffering and can result in spurious one second waits!
		 */
	}

	/* If -c failed, we restart from the beginning,
	 * but we do not truncate file then, we do it only now, at the end.
	 * This lets user to ^C if his 99% complete 10 GB file download
	 * failed to restart *without* losing the almost complete file.
	 */
	{
		off_t pos = lseek(G.output_fd, 0, SEEK_CUR);
		if (pos != (off_t)-1)
			ftruncate(G.output_fd, pos);
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
		proxy = getenv(target.protocol == P_FTP ? "ftp_proxy" : "http_proxy");
//FIXME: what if protocol is https? Ok to use http_proxy?
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
		if (G.dir_prefix)
			G.fname_out = fname_out_alloc = concat_path_file(G.dir_prefix, G.fname_out);
		else {
			/* redirects may free target.path later, need to make a copy */
			G.fname_out = fname_out_alloc = xstrdup(G.fname_out);
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
	if (use_proxy || target.protocol != P_FTP) {
		/*
		 *  HTTP session
		 */
		char *str;
		int status;

		/* Open socket to http(s) server */
#if ENABLE_FEATURE_WGET_OPENSSL
		/* openssl (and maybe ssl_helper) support is configured */
		if (target.protocol == P_HTTPS) {
			/* openssl-based helper
			 * Inconvenient API since we can't give it an open fd
			 */
			int fd = spawn_https_helper_openssl(server.host, server.port);
# if ENABLE_FEATURE_WGET_SSL_HELPER
			if (fd < 0) { /* no openssl? try ssl_helper */
				sfp = open_socket(lsa);
				spawn_https_helper_small(fileno(sfp));
				goto socket_opened;
			}
# else
			/* We don't check for exec("openssl") failure in this case */
# endif
			sfp = fdopen(fd, "r+");
			if (!sfp)
				bb_perror_msg_and_die(bb_msg_memory_exhausted);
			goto socket_opened;
		}
		sfp = open_socket(lsa);
 socket_opened:
#elif ENABLE_FEATURE_WGET_SSL_HELPER
		/* Only ssl_helper support is configured */
		sfp = open_socket(lsa);
		if (target.protocol == P_HTTPS)
			spawn_https_helper_small(fileno(sfp));
#else
		/* ssl (https) support is not configured */
		sfp = open_socket(lsa);
#endif
		/* Send HTTP request */
		if (use_proxy) {
			SENDFMT(sfp, "GET %s://%s/%s HTTP/1.1\r\n",
				target.protocol, target.host,
				target.path);
		} else {
			SENDFMT(sfp, "%s /%s HTTP/1.1\r\n",
				(option_mask32 & WGET_OPT_POST_DATA) ? "POST" : "GET",
				target.path);
		}
		if (!USR_HEADER_HOST)
			SENDFMT(sfp, "Host: %s\r\n", target.host);
		if (!USR_HEADER_USER_AGENT)
			SENDFMT(sfp, "User-Agent: %s\r\n", G.user_agent);

		/* Ask server to close the connection as soon as we are done
		 * (IOW: we do not intend to send more requests)
		 */
		SENDFMT(sfp, "Connection: close\r\n");

#if ENABLE_FEATURE_WGET_AUTHENTICATION
		if (target.user && !USR_HEADER_AUTH) {
			SENDFMT(sfp, "Proxy-Authorization: Basic %s\r\n"+6,
				base64enc(target.user));
		}
		if (use_proxy && server.user && !USR_HEADER_PROXY_AUTH) {
			SENDFMT(sfp, "Proxy-Authorization: Basic %s\r\n",
				base64enc(server.user));
		}
#endif

		if (G.beg_range != 0 && !USR_HEADER_RANGE)
			SENDFMT(sfp, "Range: bytes=%"OFF_FMT"u-\r\n", G.beg_range);

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
		if (G.extra_headers) {
			log_io(G.extra_headers);
			fputs(G.extra_headers, sfp);
		}

		if (option_mask32 & WGET_OPT_POST_DATA) {
			SENDFMT(sfp,
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Content-Length: %u\r\n"
				"\r\n"
				"%s",
				(int) strlen(G.post_data), G.post_data
			);
		} else
#endif
		{
			SENDFMT(sfp, "\r\n");
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
			if (G.beg_range != 0) {
				/* "Range:..." was not honored by the server.
				 * Restart download from the beginning.
				 */
				reset_beg_range_to_zero();
			}
			break;
		case 300:  /* redirection */
		case 301:
		case 302:
		case 303:
			break;
		case 206: /* Partial Content */
			if (G.beg_range != 0)
				/* "Range:..." worked. Good. */
				break;
			/* Partial Content even though we did not ask for it??? */
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
						/* server.user remains untouched */
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
	free(server.user);
	free(target.user);
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
IF_FEATURE_WGET_TIMEOUT(
		"timeout\0"          Required_argument "T")
		/* Ignored: */
IF_DESKTOP(	"tries\0"            Required_argument "t")
		"header\0"           Required_argument "\xff"
		"post-data\0"        Required_argument "\xfe"
		/* Ignored (we always use PASV): */
IF_DESKTOP(	"passive-ftp\0"      No_argument       "\xf0")
		/* Ignored (we don't do ssl) */
IF_DESKTOP(	"no-check-certificate\0" No_argument   "\xf0")
		/* Ignored (we don't support caching) */
IF_DESKTOP(	"no-cache\0"         No_argument       "\xf0")
IF_DESKTOP(	"no-verbose\0"       No_argument       "\xf0")
IF_DESKTOP(	"no-clobber\0"       No_argument       "\xf0")
IF_DESKTOP(	"no-host-directories\0" No_argument    "\xf0")
IF_DESKTOP(	"no-parent\0"        No_argument       "\xf0")
		;
#endif

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	llist_t *headers_llist = NULL;
#endif

	INIT_G();

#if ENABLE_FEATURE_WGET_TIMEOUT
	G.timeout_seconds = 900;
	signal(SIGALRM, alarm_handler);
#endif
	G.proxy_flag = "on";   /* use proxies if env vars are set */
	G.user_agent = "Wget"; /* "User-Agent" header field */

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	applet_long_options = wget_longopts;
#endif
	opt_complementary = "-1" /* at least one URL */
		IF_FEATURE_WGET_TIMEOUT(":T+") /* -T NUM */
		IF_FEATURE_WGET_LONG_OPTIONS(":\xff::"); /* --header is a list */
	getopt32(argv, "csqO:P:Y:U:T:"
		/*ignored:*/ "t:"
		/*ignored:*/ "n::"
		/* wget has exactly four -n<letter> opts, all of which we can ignore:
		 * -nv --no-verbose: be moderately quiet (-q is full quiet)
		 * -nc --no-clobber: abort if exists, neither download to FILE.n nor overwrite FILE
		 * -nH --no-host-directories: wget -r http://host/ won't create host/
		 * -np --no-parent
		 * "n::" above says that we accept -n[ARG].
		 * Specifying "n:" would be a bug: "-n ARG" would eat ARG!
		 */
		, &G.fname_out, &G.dir_prefix,
		&G.proxy_flag, &G.user_agent,
		IF_FEATURE_WGET_TIMEOUT(&G.timeout_seconds) IF_NOT_FEATURE_WGET_TIMEOUT(NULL),
		NULL, /* -t RETRIES */
		NULL  /* -n[ARG] */
		IF_FEATURE_WGET_LONG_OPTIONS(, &headers_llist)
		IF_FEATURE_WGET_LONG_OPTIONS(, &G.post_data)
	);
	argv += optind;

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	if (headers_llist) {
		int size = 0;
		char *hdr;
		llist_t *ll = headers_llist;
		while (ll) {
			size += strlen(ll->data) + 2;
			ll = ll->link;
		}
		G.extra_headers = hdr = xmalloc(size + 1);
		while (headers_llist) {
			int bit;
			const char *words;

			size = sprintf(hdr, "%s\r\n",
					(char*)llist_pop(&headers_llist));
			/* a bit like index_in_substrings but don't match full key */
			bit = 1;
			words = wget_user_headers;
			while (*words) {
				if (strstr(hdr, words) == hdr) {
					G.user_headers |= bit;
					break;
				}
				bit <<= 1;
				words += strlen(words) + 1;
			}
			hdr += size;
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

#if ENABLE_FEATURE_CLEAN_UP && ENABLE_FEATURE_WGET_LONG_OPTIONS
	free(G.extra_headers);
#endif
	FINI_G();

	return EXIT_SUCCESS;
}
