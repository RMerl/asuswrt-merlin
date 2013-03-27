/* 
   Socket handling tests
   Copyright (C) 2002-2009, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/* This module can be compiled with -DSOCKET_SSL enabled, to run all
 * the tests over an SSL connection. */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h> /* for AF_INET6 */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for gethostname() */
#endif
#include <time.h> /* for time() */

#include "ne_socket.h"
#include "ne_utils.h"
#include "ne_alloc.h"

#include "child.h"
#include "tests.h"
#include "utils.h"

#ifdef SOCKET_SSL
#include "ne_ssl.h"
ne_ssl_context *server_ctx, *client_ctx;
#endif

static ne_sock_addr *localhost;
static char buffer[BUFSIZ];

#if defined(AF_INET6) && defined(USE_GETADDRINFO)
#define TEST_IPV6
#endif

/* tests for doing init/finish multiple times. */
static int multi_init(void)
{
    int res1 = ne_sock_init(), res2 = ne_sock_init();
    
    ONV(res1 != res2, ("cached init result changed from %d to %d",
                       res1, res2));
    
    ne_sock_exit();
    ne_sock_exit();
    ne_sock_exit();

    res1 = ne_sock_init();
    ONV(res1 != res2, ("re-init after exit gave %d not %d",
                       res1, res2));
    ne_sock_exit();

    res2 = ne_sock_init();
    
    ONV(res1 != res2, ("second time, cached init result changed from %d to %d",
                       res1, res2));
    
    return OK;
}

/* Create and connect *sock to address addr on given port. */
static int do_connect(ne_socket **sock, ne_sock_addr *addr, unsigned int port)
{
    const ne_inet_addr *ia;

    *sock = ne_sock_create();
    ONN("could not create socket", *sock == NULL);

    for (ia = ne_addr_first(addr); ia; ia = ne_addr_next(addr)) {
	if (ne_sock_connect(*sock, ia, port) == 0)
            return OK;
    }
    
    t_context("could not connect to server: %s", ne_sock_error(*sock));
    ne_sock_close(*sock);
    return FAIL;
}

#ifdef SOCKET_SSL

static int init_ssl(void)
{
    char *server_key;
    ne_ssl_certificate *cert;
 
    /* take srcdir as argv[1]. */
    if (test_argc > 1) {
	server_key = ne_concat(test_argv[1], "/server.key", NULL);
    } else {
	server_key = ne_strdup("server.key");
    }

    ONN("sock_init failed", ne_sock_init());
    server_ctx = ne_ssl_context_create(1);
    ONN("SSL_CTX_new failed", server_ctx == NULL);

    ne_ssl_context_keypair(server_ctx, "server.cert", server_key);

    client_ctx = ne_ssl_context_create(0);
    ONN("SSL_CTX_new failed for client", client_ctx == NULL);
    
    cert = ne_ssl_cert_read("ca/cert.pem");
    ONN("could not load ca/cert.pem", cert == NULL);

    ne_ssl_context_trustcert(client_ctx, cert);
    ne_free(server_key);

    return OK;
}
#endif

static int resolve(void)
{
    char buf[256];
    localhost = ne_addr_resolve("localhost", 0);
    ONV(ne_addr_result(localhost),
	("could not resolve `localhost': %s", 
	 ne_addr_error(localhost, buf, sizeof buf)));
    /* and again for child.c */
    return lookup_localhost();
}

static int serve_close(ne_socket *sock, void *ud)
{
    return 0;
}

#ifdef SOCKET_SSL
struct serve_pair {
    server_fn fn;
    void *userdata;
};

static int wrap_serve(ne_socket *sock, void *ud)
{
    struct serve_pair *pair = ud;
    
    if (ne_sock_accept_ssl(sock, server_ctx)) {
	NE_DEBUG(NE_DBG_SOCKET, "SSL_accept failed: %s\n", ne_sock_error(sock));
	return 1;
    }
    
    NE_DEBUG(NE_DBG_SOCKET, "SSL accept okay.\n");
    return pair->fn(sock, pair->userdata);
}

static int begin(ne_socket **sock, server_fn fn, void *ud)
{
    struct serve_pair pair;
    pair.fn = fn;
    pair.userdata = ud;
    CALL(spawn_server(7777, wrap_serve, &pair));
    CALL(do_connect(sock, localhost, 7777));
    ONV(ne_sock_connect_ssl(*sock, client_ctx, NULL),
	("SSL negotation failed: %s", ne_sock_error(*sock)));
    return OK;
}

#else
/* non-SSL begin() function. */
static int begin(ne_socket **sock, server_fn fn, void *ud)
{
    CALL(spawn_server(7777, fn, ud));
    return do_connect(sock, localhost, 7777);
}
#endif

static int resolve_numeric(void)
{
    ne_sock_addr *addr = ne_addr_resolve("127.0.0.1", 0);
    ONV(ne_addr_result(addr),
	("failed to resolve 127.0.0.1: %s",
	 ne_addr_error(addr, buffer, sizeof buffer)));
    ONN("ne_addr_first returned NULL", ne_addr_first(addr) == NULL);
    ONN("ne_iaddr_print didn't return buffer", 
	ne_iaddr_print(ne_addr_first(addr), buffer, sizeof buffer) != buffer);
    ONV(strcmp(buffer, "127.0.0.1"), ("ntop gave `%s' not 127.0.0.1", buffer));
    ne_addr_destroy(addr);
    return OK;
}

#if 0
static int resolve_ipv6(void)
{
    char err[256];
    ne_sock_addr *addr = ne_addr_resolve("[::1]", 0);
    ONV(ne_addr_result(addr),
	("could not resolve `[::1]': %s",
	 ne_addr_error(addr, err, sizeof err)));
    ne_addr_destroy(addr);
    return OK;
}
#endif

static const unsigned char raw_127[4] = "\x7f\0\0\01", /* 127.0.0.1 */
    raw6_nuls[16] = /* :: */ "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
#ifdef TEST_IPV6
static const unsigned char 
raw6_local[16] = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1",
raw6_cafe[16] = /* feed::cafe */ "\xfe\xed\0\0\0\0\0\0\0\0\0\0\0\0\xca\xfe",
raw6_babe[16] = /* cafe:babe:: */ "\xca\xfe\xba\xbe\0\0\0\0\0\0\0\0\0\0\0\0";
#endif

/* Check the given inet addr is 127.0.0.1. */
static int check_is_raw127(const ne_inet_addr *ia)
{
    unsigned char raw[5];

    raw[4] = 'Z';
    ONN("bogus ne_iaddr_typeof return", ne_iaddr_typeof(ia) != ne_iaddr_ipv4);
    ONN("ne_iaddr_raw gave bad retval", ne_iaddr_raw(ia, raw) != raw);
    ONN("raw address mismatch", memcmp(raw, raw_127, 4) != 0);
    ONN("ne_iaddr_raw buffer overflow", raw[4] != 'Z');

    return OK;
}

static int addr_make_v4(void)
{
    ne_inet_addr *ia;
    char pr[50];
    
    ia = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    ONN("ne_iaddr_make returned NULL", ia == NULL);

    ne_iaddr_print(ia, pr, sizeof pr);
    ONV(strcmp(pr, "127.0.0.1"), ("address was %s not 127.0.0.1", pr));

    CALL(check_is_raw127(ia));
    
    ne_iaddr_free(ia);

    return OK;
}

static int parse_v4(void)
{
    ne_inet_addr *ia;

    ia = ne_iaddr_parse("127.0.0.1", ne_iaddr_ipv4);
    ONN("parse failed", ia == NULL);

    CALL(check_is_raw127(ia));

    ne_iaddr_free(ia);

    return OK;
}

static int addr_make_v6(void)
{
#ifdef TEST_IPV6   
    struct {
	const unsigned char *addr;
	const char *rep;
    } as[] = {
	{ raw6_cafe, "feed::cafe" },
	{ raw6_babe, "cafe:babe::" },
	{ raw6_nuls, "::" },
	{ NULL, NULL }
    };
    int n;
    
    for (n = 0; as[n].rep != NULL; n++) {
	ne_inet_addr *ia = ne_iaddr_make(ne_iaddr_ipv6, as[n].addr);
	char pr[128];
        unsigned char raw[17];

	ONV(ia == NULL, ("could not make address for '%s'", 
                         as[n].rep));

	ne_iaddr_print(ia, pr, sizeof pr);
	ONV(strcmp(pr, as[n].rep), 
	    ("address %d was '%s' not '%s'", n, pr, as[n].rep));
	
        ONN("bogus ne_iaddr_typeof return", ne_iaddr_typeof(ia) != ne_iaddr_ipv6);

        raw[16] = 'Z';
        ONN("ne_iaddr_raw gave bad retval", ne_iaddr_raw(ia, raw) != raw);
        ONN("raw address mismatch", memcmp(raw, as[n].addr, 4) != 0);
        ONN("ne_iaddr_raw buffer overflow", raw[16] != 'Z');
        
	ne_iaddr_free(ia);
        
        ia = ne_iaddr_parse(as[n].rep, ne_iaddr_ipv6);
        ONV(ia == NULL, ("ne_iaddr_parse failed for %s", as[n].rep));
        ONN("bogus ne_iaddr_typeof return", ne_iaddr_typeof(ia) != ne_iaddr_ipv6);
        ONN("ne_iaddr_raw gave bad retval", ne_iaddr_raw(ia, raw) != raw);
        ONN("raw address mismatch", memcmp(raw, as[n].addr, 4) != 0);
        ONN("ne_iaddr_raw buffer overflow", raw[16] != 'Z');

        ne_iaddr_free(ia);
   }

    return OK;
#else
    /* should fail when lacking IPv6 support. */
    ne_inet_addr *ia = ne_iaddr_make(ne_iaddr_ipv6, raw6_nuls);
    ONN("ne_iaddr_make did not return NULL", ia != NULL);
    ONN("ne_iaddr_parse did not return NULL", ne_iaddr_parse("127.0.0.1", ne_iaddr_ipv6));
#endif
    return OK;
}

static const unsigned char raw_1234[] = "\x01\x02\x03\x04";

static int addr_compare(void)
{
    ne_inet_addr *ia1, *ia2;
    int ret;

    ia1 = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    ia2 = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    ONN("addr_make returned NULL", !ia1 || !ia2);

    ret = ne_iaddr_cmp(ia1, ia2);
    ONV(ret != 0, ("comparison of equal IPv4 addresses was %d", ret));
    ne_iaddr_free(ia2);

    ia2 = ne_iaddr_make(ne_iaddr_ipv4, raw_1234);
    ret = ne_iaddr_cmp(ia1, ia2);
    ONN("comparison of unequal IPv4 addresses was zero", ret == 0);
    
#ifdef TEST_IPV6
    ne_iaddr_free(ia2);
    ia2 = ne_iaddr_make(ne_iaddr_ipv6, raw6_cafe);
    ONN("could not make IPv6 address", !ia2);
    ret = ne_iaddr_cmp(ia1, ia2);
    ONN("comparison of IPv4 and IPv6 addresses was zero", ret == 0);

    ne_iaddr_free(ia1);
    ia1 = ne_iaddr_make(ne_iaddr_ipv6, raw6_cafe);
    ret = ne_iaddr_cmp(ia1, ia2);
    ONN("comparison of equal IPv6 addresses was not zero", ret != 0);

    ne_iaddr_free(ia1);
    ia1 = ne_iaddr_make(ne_iaddr_ipv6, raw6_babe);
    ret = ne_iaddr_cmp(ia1, ia2);
    ONN("comparison of unequal IPv6 address was zero", ret == 0);
#endif    

    ne_iaddr_free(ia1);
    ne_iaddr_free(ia2);
    return OK;
}

static int addr_reverse(void)
{
    ne_inet_addr *ia = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    char buf[128], *syshost = NULL;

#ifdef HAVE_GETHOSTNAME
    char host[128];

    if (gethostname(host, sizeof host) == 0) {
        syshost = host;
    }
#endif

    ONN("ne_iaddr_make returned NULL", ia == NULL);

    ONN("reverse lookup for 127.0.0.1 failed",
        ne_iaddr_reverse(ia, buf, sizeof buf) != 0);

    ONV(!(strcmp(buf, "localhost.localdomain") == 0
          || strcmp(buf, "localhost") == 0
          || (syshost && strcmp(buf, syshost) == 0)),
        ("reverse lookup for 127.0.0.1 got %s", buf));

    ne_iaddr_free(ia);

    return OK;
}

static int just_connect(void)
{
    ne_socket *sock;
    
    CALL(begin(&sock, serve_close, NULL));
    ne_sock_close(sock);
    return await_server();
}

/* Connect to an address crafted using ne_iaddr_make rather than from
 * the resolver. */
static int addr_connect(void)
{
    ne_socket *sock = ne_sock_create();
    ne_inet_addr *ia;

    ia = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    ONN("ne_iaddr_make returned NULL", ia == NULL);
    
    CALL(spawn_server(7777, serve_close, NULL));
    ONN("could not connect", ne_sock_connect(sock, ia, 7777));
    ne_sock_close(sock);
    CALL(await_server());

    ne_iaddr_free(ia);
    return OK;
}

static int addr_peer(void)
{
    ne_socket *sock = ne_sock_create();
    ne_inet_addr *ia, *ia2;
    unsigned int port = 9999;
    int ret;

    ia = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    ONN("ne_iaddr_make returned NULL", ia == NULL);
    
    CALL(spawn_server(7777, serve_close, NULL));
    ONN("could not connect", ne_sock_connect(sock, ia, 7777));

    ia2 = ne_sock_peer(sock, &port);
    ret = ne_iaddr_cmp(ia, ia2);
    ONV(ret != 0,
        ("comparison of peer with server address was %d", ret));

    ONV(port != 7777, ("got peer port %u", port));
 
    ne_sock_close(sock);
    CALL(await_server());

    ne_iaddr_free(ia);
    ne_iaddr_free(ia2);
    return OK;
}

/* Exect a read() to return EOF */
static int expect_close(ne_socket *sock)
{
    ssize_t n = ne_sock_read(sock, buffer, 1);
    ONV(n > 0, ("read got %" NE_FMT_SSIZE_T " bytes not closure", n));
    ONV(n < 0 && n != NE_SOCK_CLOSED,
	("read got error not closure: `%s'", ne_sock_error(sock)));
    return OK;
}

static int good_close(ne_socket *sock)
{
    NE_DEBUG(NE_DBG_SOCKET, "Socket error was %s\n", ne_sock_error(sock));
    ONN("close failed", ne_sock_close(sock));
    return OK;    
}

/* Finish a test, closing socket and rejoining child. If eof is non-zero,
 * expects to read EOF from the socket before closing. */
static int finish(ne_socket *sock, int eof)
{
    if (eof)
	CALL(expect_close(sock));
    CALL(good_close(sock));
    return await_server();
}

/* Exect a ne_sock_peek() to return EOF */
static int expect_peek_close(ne_socket *sock)
{
    ssize_t n = ne_sock_read(sock, buffer, 1);
    ONV(n != NE_SOCK_CLOSED, ("peek gave %" NE_FMT_SSIZE_T " not closure", n));
    return OK;
}

/* Test that just does a connect then a close. */
static int read_close(void)
{
    ne_socket *sock;

    CALL(begin(&sock, serve_close, NULL));
    CALL(expect_close(sock));    
    ONN("close failed", ne_sock_close(sock));
    return await_server();
}

/* Test that just does a connect then a close (but gets the close via
 * ne_sock_peek). */
static int peek_close(void)
{
    ne_socket *sock;

    CALL(begin(&sock, serve_close, NULL));
    CALL(expect_peek_close(sock));    
    ONN("close failed", ne_sock_close(sock));
    return await_server();
}

/* Don't change this string. */
#define STR "Hello, World."

/* do a sock_peek() on sock for 'len' bytes, and expect 'str'. */
static int peek_expect(ne_socket *sock, const char *str, size_t len)
{
    ssize_t ret;
    memset(buffer, '@', sizeof buffer);
    ret = ne_sock_peek(sock, buffer, len);
    ONV((ssize_t)len != ret, 
	("peek got %" NE_FMT_SSIZE_T " bytes not %" NE_FMT_SIZE_T, ret, len));
    ONV(memcmp(str, buffer, len),
	("peek mismatch: `%.*s' not `%.*s'", 
	 (int)len, buffer, (int)len, str));
    ONV(buffer[len] != '@',
        ("buffer overrun: %" NE_FMT_SSIZE_T "nth byte was '%c' not '@'", 
         len, buffer[len]));
    return OK;
}

/* do a sock_read() on sock for 'len' bytes, and expect 'str'. */
static int read_expect(ne_socket *sock, const char *str, size_t len)
{
    ssize_t ret = ne_sock_read(sock, buffer, len);
    ONV((ssize_t)len != ret,
	("read got %" NE_FMT_SSIZE_T " bytes not %" NE_FMT_SIZE_T, ret, len));
    ONV(memcmp(str, buffer, len),
	("read mismatch: `%.*s' not `%.*s'", 
	 (int)len, buffer, (int)len, str));    
    return OK;
}

/* do a sock_read() on sock for 'len' bytes, and expect 'str'. */
static int fullread_expect(ne_socket *sock, const char *str, size_t len)
{
    ssize_t ret = ne_sock_fullread(sock, buffer, len);
    ONV(ret, ("fullread failed (%" NE_FMT_SSIZE_T "): %s", 
              ret, ne_sock_error(sock)));
    ONV(memcmp(str, buffer, len),
	("fullread mismatch: `%.*s' not `%.*s'", 
	 (int)len, buffer, (int)len, str));    
    return OK;
}

#define FULLREAD(str) CALL(fullread_expect(sock, str, strlen(str)))

/* Declare a struct string */
#define DECL(var,str) struct string var = { str, 0 }; var.len = strlen(str)
#define DECL_LONG(var,ch,n) struct string var; var.data = memset(ne_malloc(n), ch, n); var.len = n; 

/* Test a simple read. */
static int single_read(void)
{
    ne_socket *sock;
    DECL(hello, STR);

    CALL(begin(&sock, serve_sstring, &hello));
    CALL(read_expect(sock, STR, strlen(STR)));
    CALL(expect_close(sock));
    CALL(good_close(sock));

    return await_server();
}

/* Test a simple peek. */
static int single_peek(void)
{
    ne_socket *sock;
    DECL(hello, STR);

    CALL(begin(&sock, serve_sstring, &hello));
    CALL(peek_expect(sock, STR, strlen(STR)));
 
    return finish(sock, 0);
}

/* Test lots of 1-byte reads. */
static int small_reads(void)
{
    ne_socket *sock;
    char *pnt;
    DECL(hello, STR);

    CALL(begin(&sock, serve_sstring, &hello));

    /* read the string byte-by-byte. */
    for (pnt = hello.data; *pnt; pnt++) {
	CALL(read_expect(sock, pnt, 1));
    }

    return finish(sock, 1);
}

/* peek or read, expecting to get given string. */
#define READ(str) CALL(read_expect(sock, str, strlen(str)))
#define PEEK(str) CALL(peek_expect(sock, str, strlen(str)))

/* Stress out the read buffer handling a little. */
static int read_and_peek(void)
{
    ne_socket *sock;
    DECL(hello, STR);

    CALL(begin(&sock, serve_sstring, &hello));

    PEEK("Hello");
    PEEK("Hell");
    PEEK(STR);
    READ("He");
    PEEK("llo, ");
    READ("l");
    PEEK("lo, World.");
    READ("lo, Worl");
    PEEK("d."); PEEK("d");
    READ("d.");

    return finish(sock, 1);
}

/* Read more bytes than were written. */
static int larger_read(void)
{
    ne_socket *sock;
    ssize_t nb;
    DECL(hello, STR);

    CALL(begin(&sock, serve_sstring, &hello));
    
    nb = ne_sock_read(sock, buffer, hello.len + 10);
    ONV(nb != (ssize_t)hello.len, 
	("read gave too many bytes (%" NE_FMT_SSIZE_T ")", nb));
    ONN("read gave wrong data", memcmp(buffer, hello.data, hello.len));
    
    return finish(sock, 1);
}

static int line_expect(ne_socket *sock, const char *line)
{
    ssize_t ret = ne_sock_readline(sock, buffer, BUFSIZ);
    size_t len = strlen(line);
    NE_DEBUG(NE_DBG_SOCKET, " -> expected=%s -> actual=%s", line, buffer);
    ONV(ret == NE_SOCK_CLOSED, ("socket closed, expecting `%s'", line));
    ONV(ret < 0, ("socket error `%s', expecting `%s'", 
		  ne_sock_error(sock), line));
    ONV((size_t)ret != len || strcmp(line, buffer),
	("readline mismatch: `%s' not `%s'", buffer, line));
    return OK;
}

#define LINE(x) CALL(line_expect(sock, x))

#define STR2 "Goodbye, cruel world."

static int line_simple(void)
{
    ne_socket *sock;
    DECL(oneline, STR "\n" STR2 "\n");

    CALL(begin(&sock, serve_sstring, &oneline));
    LINE(STR "\n");
    LINE(STR2 "\n");
    
    return finish(sock, 1);
}

static int line_closure(void)
{
    ne_socket *sock;
    ssize_t ret;
    DECL(oneline, STR "\n" "foobar");
    
    CALL(begin(&sock, serve_sstring, &oneline));
    LINE(STR "\n");
    
    ret = ne_sock_readline(sock, buffer, BUFSIZ);
    ONV(ret != NE_SOCK_CLOSED, 
	("readline got %" NE_FMT_SSIZE_T " not EOF: %s", ret,
         ne_sock_error(sock)));
    
    return finish(sock, 0);
}   

/* check that empty lines are handled correctly. */
static int line_empty(void)
{
    ne_socket *sock;
    DECL(oneline, "\n\na\n\n");

    CALL(begin(&sock, serve_sstring, &oneline));
    LINE("\n"); LINE("\n");
    LINE("a\n"); LINE("\n");
    
    return finish(sock, 1);
}

static int line_toolong(void)
{
    ne_socket *sock;
    ssize_t ret;
    DECL(oneline, "AAAAAA\n");

    CALL(begin(&sock, serve_sstring, &oneline));
    ret = ne_sock_readline(sock, buffer, 5);
    ONV(ret != NE_SOCK_ERROR, 
	("readline should fail on long line: %" NE_FMT_SSIZE_T, ret));
    reap_server();
    ne_sock_close(sock);
    return OK;
}

#define OVERLEN (9000)

static int line_overflow(void)
{
    ne_socket *sock;
    ssize_t ret;
    DECL_LONG(line, 'A', OVERLEN);

    CALL(begin(&sock, serve_sstring, &line));

    PEEK("A"); /* fill the read buffer */
    ret = ne_sock_readline(sock, buffer, OVERLEN);
    ONV(ret != NE_SOCK_ERROR,
        ("readline should fail on overlong line: %" NE_FMT_SSIZE_T, ret));

    ne_free(line.data);
    return finish(sock, 0);
}


/* readline()s mingled with other operations: buffering tests. */
static int line_mingle(void)
{
    ne_socket *sock;
    DECL(oneline, "alpha\nbeta\ndelta\ngamma\n");

    CALL(begin(&sock, serve_sstring, &oneline));
    
    READ("a"); LINE("lpha\n");
    READ("beta"); LINE("\n");
    PEEK("d"); PEEK("delt");
    LINE("delta\n");
    READ("gam"); LINE("ma\n");
    
    return finish(sock, 1);
}

/* readline which needs multiple read() calls. */
static int line_chunked(void)
{
    ne_socket *sock;
    DECL(oneline, "this is a line\n");

    CALL(begin(&sock, serve_sstring_slowly, &oneline));
    
    LINE("this is a line\n");
    
    return finish(sock, 1);
}

static int line_long_chunked(void)
{
    ne_socket *sock;
    ssize_t ret;
    DECL_LONG(line, 'Z', OVERLEN);

    CALL(begin(&sock, serve_sstring_slowly, &line));
    
    FULLREAD("ZZZZZZZZ"); /* fill the buffer */
    ret = ne_sock_readline(sock, buffer, sizeof buffer);
    ONV(ret != NE_SOCK_ERROR,
        ("readline gave %" NE_FMT_SSIZE_T " not failure", ret));

    reap_server();
    ne_sock_close(sock);
    ne_free(line.data);
    return OK;
}


static time_t to_start, to_finish;

static int to_begin(ne_socket **sock)
{
    CALL(begin(sock, sleepy_server, NULL));
    ne_sock_read_timeout(*sock, 1);
    to_start = time(NULL);
    return OK;
}

static int to_end(ne_socket *sock)
{
    to_finish = time(NULL);
    reap_server(); /* hopefully it's hung. */
    ONN("timeout ignored, or very slow machine", to_finish - to_start > 3);
    ONN("close failed", ne_sock_close(sock));
    return OK;
}
	 
#define TO_BEGIN ne_socket *sock; CALL(to_begin(&sock))
#define TO_OP(x) do { int to_ret = (x); \
        ONV(to_ret != NE_SOCK_TIMEOUT, ("operation did not timeout: got %d (%s)", to_ret, ne_sock_error(sock))); \
} while (0)
#define TO_FINISH return to_end(sock)

#ifndef TEST_CONNECT_TIMEOUT
#define TEST_CONNECT_TIMEOUT 0
#endif
#if TEST_CONNECT_TIMEOUT

/* No obvious way to reliably test a connect() timeout.  But
 * www.example.com seems to drop packets on ports other than 80 so
 * that actually works pretty well.  Disabled by default. */
static int connect_timeout(void)
{
    static const unsigned char example_dot_com[] = "\xC0\x00\x22\xA6";
    ne_socket *sock = ne_sock_create();
    ne_inet_addr *ia = ne_iaddr_make(ne_iaddr_ipv4, example_dot_com);

    ne_sock_connect_timeout(sock, 1);

    TO_OP(ne_sock_connect(sock, ia, 8080));

    ne_iaddr_free(ia);
    ne_sock_close(sock);

    return OK;
}
#endif

static int peek_timeout(void)
{
    TO_BEGIN;
    TO_OP(ne_sock_peek(sock, buffer, 1));
    TO_FINISH;
}

static int read_timeout(void)
{
    TO_BEGIN;
    TO_OP(ne_sock_read(sock, buffer, 1));
    TO_FINISH;
}

static int readline_timeout(void)
{
    TO_BEGIN;
    TO_OP(ne_sock_readline(sock, buffer, 1));
    TO_FINISH;
}

static int fullread_timeout(void)
{
    TO_BEGIN;
    TO_OP(ne_sock_fullread(sock, buffer, 1));
    TO_FINISH;
}   

static int serve_expect(ne_socket *sock, void *ud)
{
    struct string *str = ud;
    ssize_t ret;

    while (str->len && 
	   (ret = ne_sock_read(sock, buffer, sizeof(buffer))) > 0) {
	NE_DEBUG(NE_DBG_SOCKET, "Got %" NE_FMT_SSIZE_T " bytes.\n", ret);
	ONV(memcmp(str->data, buffer, ret),
	    ("unexpected data: [%.*s] not [%.*s]", 
	     (int)ret, buffer, (int)ret, str->data));
	str->data += ret;
	str->len -= ret;
	NE_DEBUG(NE_DBG_SOCKET, "%" NE_FMT_SIZE_T " bytes left.\n", str->len);
    }

    NE_DEBUG(NE_DBG_SOCKET, "All data read.\n");
    return OK;
}

#define WRITEL(str) CALL(full_write(sock, str, strlen(str))); \
minisleep()

static int small_writes(void)
{
    ne_socket *sock;
    DECL(str, "This\nIs\nSome\nText.\n");
    
    CALL(begin(&sock, serve_expect, &str));
    
    WRITEL("This\n"); WRITEL("Is\n"); WRITEL("Some\n"); WRITEL("Text.\n");
    
    return finish(sock, 1);
}

static int large_writes(void)
{
#define LARGE_SIZE (123456)
    struct string str;
    ne_socket *sock;
    ssize_t n;

    str.data = ne_malloc(LARGE_SIZE);
    str.len = LARGE_SIZE;

    for (n = 0; n < LARGE_SIZE; n++)
	str.data[n] = 41 + n % 130;
    
    CALL(begin(&sock, serve_expect, &str));
    CALL(full_write(sock, str.data, str.len));
    
    ne_free(str.data);
    return finish(sock, 1);    
}

static int full_writev(ne_socket *sock, struct ne_iovec *vec, int count)
{
    int ret = ne_sock_fullwritev(sock, vec, count);
    NE_DEBUG(NE_DBG_SOCKET, "wrote vector (%d)\n", count);
    ONV(ret, ("writev failed (%d): %s", ret, ne_sock_error(sock)));
    return OK;
}

#undef LARGE_SIZE
#define LARGE_SIZE (123456 * 4)

static int large_writev(void)
{
    struct string str;
    ne_socket *sock;
    ssize_t n;
    struct ne_iovec vec[4];

    str.data = ne_malloc(LARGE_SIZE);
    str.len = LARGE_SIZE;

    for (n = 0; n < LARGE_SIZE; n++)
	str.data[n] = 41 + n % 130;
    
    for (n = 0; n < 4; n++) {
        vec[n].base = str.data + n * LARGE_SIZE / 4;
        vec[n].len = LARGE_SIZE / 4;
    }

    CALL(begin(&sock, serve_expect, &str));
    CALL(full_writev(sock, vec, 4));
    
    ne_free(str.data);
    return finish(sock, 1);    
}


/* echoes back lines. */
static int echo_server(ne_socket *sock, void *ud)
{
    ssize_t ret;

    while ((ret = ne_sock_readline(sock, buffer, sizeof(buffer))) > 0) {
	NE_DEBUG(NE_DBG_SOCKET, "Line: %s", buffer);
	ONN("write failed", ne_sock_fullwrite(sock, buffer, ret));
	NE_DEBUG(NE_DBG_SOCKET, "Wrote line.\n");
    }

    ONN("readline failed", ret != NE_SOCK_CLOSED);
    return 0;
}

static int echo_expect(ne_socket *sock, const char *line)
{
    CALL(full_write(sock, line, strlen(line)));
    return line_expect(sock, line);
}

#define ECHO(line) CALL(echo_expect(sock, line))

static int echo_lines(void)
{
    ne_socket *sock;
    CALL(begin(&sock, echo_server, NULL));
    ECHO("hello,\n");
    ECHO("\n");
    ECHO("world\n");
    return finish(sock, 0);
}

#ifdef SOCKET_SSL
/* harder to simulate closure cases for an SSL connection, since it
 * may be doing multiple read()s or write()s per ne_sock_* call. */
static int ssl_closure(void)
{
    ne_socket *sock;
    ssize_t ret;
    CALL(begin(&sock, serve_close, NULL));
    CALL(full_write(sock, "a", 1));
    CALL(await_server());
    do {
        ret = ne_sock_fullwrite(sock, "a", 1);
    } while (ret == 0);
    ONV(ret != NE_SOCK_RESET && ret != NE_SOCK_CLOSED, 
	("write got %" NE_FMT_SSIZE_T " not reset or closure: %s", ret,
         ne_sock_error(sock)));
    return good_close(sock);
}

static int serve_truncate(ne_socket *sock, void *userdata)
{
    exit(0);
}

/* when an EOF is received without a clean shutdown (close_notify
   message). */
static int ssl_truncate(void)
{
    ne_socket *sock; int ret;
    
    CALL(begin(&sock, serve_truncate, NULL));
    ret = ne_sock_read(sock, buffer, 1);
    ONV(ret != NE_SOCK_TRUNC,
	("socket got error %d not truncation: `%s'", ret,
	 ne_sock_error(sock)));
    return finish(sock, 0);
}

#else

/* use W Richard Stevens' SO_LINGER trick to elicit a TCP RST */
static int serve_reset(ne_socket *sock, void *ud)
{
    minisleep();
    reset_socket(sock);
    exit(0);
    return 0;
}

static int write_reset(void)
{
    ne_socket *sock;
    int ret;
    CALL(begin(&sock, serve_reset, NULL));
    CALL(full_write(sock, "a", 1));
    CALL(await_server());
    ret = ne_sock_fullwrite(sock, "a", 1);
    if (ret == 0) {
        ne_sock_close(sock);
        return SKIP;
    }
    if (ret == NE_SOCK_CLOSED) {
        t_warning("got EOF, failed to elicit TCP RST");
    } else {
        ONV(ret != NE_SOCK_RESET, 
            ("write got %d not reset: %s", ret, ne_sock_error(sock)));
    }
    return good_close(sock);
}

static int read_reset(void)
{
    ne_socket *sock;
    ssize_t ret;
    CALL(begin(&sock, serve_reset, NULL));
    CALL(full_write(sock, "a", 1));
    CALL(await_server());
    ret = ne_sock_read(sock, buffer, 1);
    if (ret == NE_SOCK_CLOSED) {
        ne_sock_close(sock);
        return SKIP;
    }
    ONV(ret != NE_SOCK_RESET, 
        ("read got %" NE_FMT_SSIZE_T " not reset: %s", ret,
         ne_sock_error(sock)));
    return good_close(sock);
}    
#endif

static int expect_block_timeout(ne_socket *sock, int timeout, const char *msg)
{
    int ret;
    NE_DEBUG(NE_DBG_SOCKET, "blocking for %d\n", timeout);
    ret = ne_sock_block(sock, timeout);
    ONV(ret != NE_SOCK_TIMEOUT, (
            "ne_sock_block got %d not timeout: %s", ret, msg));
    return OK;
}

static int blocking(void)
{
    ne_socket *sock;
    int ret;
    
    CALL(begin(&sock, echo_server, NULL));
    CALL(expect_block_timeout(sock, 1, "with non-zero timeout"));
         
    WRITEL("Hello, world.\n");
    
    /* poll for data */
    do {
        ret = ne_sock_block(sock, 1);
    } while (ret == NE_SOCK_TIMEOUT);

    ONV(ret != 0, ("ne_sock_block never got data: %d", ret));

    PEEK("Hello,");
    ret = ne_sock_block(sock, 1);
    ONV(ret != 0, ("ne_sock_block failed after peek: %d", ret));

    LINE("Hello, world.\n");

    return finish(sock, 0);
}

static int block_timeout(void)
{
    TO_BEGIN;
    TO_OP(ne_sock_block(sock, 1));
    TO_FINISH;
}

static int ssl_session_id(void)
{
    ne_socket *sock;
    unsigned char buf[128];
    size_t len1;

    CALL(begin(&sock, serve_close, NULL));

#ifdef SOCKET_SSL
    len1 = 0;
    ONN("retrieve session id length",
        ne_sock_sessid(sock, NULL, &len1));

    if (len1 == 0) {
        /* recent versions of OpenSSL seem to do this, not sure
         * why or whether it's bad. */
        finish(sock, 1);
        t_context("zero-length session ID, cannot test further");
        return SKIP;
    }            

    if (len1 < sizeof buf) {
        buf[len1] = 'Z';
    }
    
    {
        size_t len2;

        len2 = sizeof buf;
        ONN("could not retrieve session id",
            ne_sock_sessid(sock, buf, &len2));
        
        ONN("buffer size changed!?", len1 != len2);
    }        

    ONN("buffer written past expected end", 
        len1 < sizeof buf && buf[len1] != 'Z');

    /* Attempt retrieval into too-short buffer: */
    len1 = 0;
    ONN("success for buffer overflow case",
        ne_sock_sessid(sock, buf, &len1) == 0);

#else
    len1 = sizeof buf;
    ONN("retrieved session id for non-SSL socket!?",
        ne_sock_sessid(sock, buf, &len1) == 0);
#endif

    ne_sock_close(sock);
    return await_server();
}

static int serve_ppeer(ne_socket *sock, void *ud)
{
    unsigned int port = 99999;
    ne_inet_addr *ia = ne_sock_peer(sock, &port);
    char buf[128], line[256];

    if (ia == NULL)
        ne_snprintf(line, sizeof line, "error: %s", ne_sock_error(sock));
    else
        ne_snprintf(line, sizeof line,
                    "%s@%u\n", ne_iaddr_print(ia, buf, sizeof buf),
                    port);

    CALL(full_write(sock, line, strlen(line)));
         
    ne_iaddr_free(ia);
    
    return OK;
}

static int try_prebind(int addr, int port)
{
    ne_socket *sock = ne_sock_create();
    ne_inet_addr *ia;
    char buf[128], line[256];

    ia = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
    ONN("ne_iaddr_make returned NULL", ia == NULL);
    
    CALL(spawn_server(7777, serve_ppeer, NULL));

    ne_sock_prebind(sock, addr ? ia : NULL, port ? 7778 : 0);

    ONN("could not connect", ne_sock_connect(sock, ia, 7777));

    ne_snprintf(line, sizeof line,
                "%s@%d\n", ne_iaddr_print(ia, buf, sizeof buf),
                7778);
    
    if (!port) {
        /* Don't know what port will be chosen, so... */
        ssize_t ret = ne_sock_readline(sock, buffer, BUFSIZ);
        
        ONV(ret < 0, ("socket error `%s'", ne_sock_error(sock)));

        ONV(strncmp(line, buffer, strchr(line, '@') - line) != 0,
            ("bad address: '%s', expecting '%s'",
             buffer, line));
    }
    else {
        LINE(line);
    }

    ne_sock_close(sock);
    CALL(await_server());

    ne_iaddr_free(ia);
    return OK;
}

static int prebind(void)
{
    CALL(try_prebind(1, 0));
    CALL(try_prebind(0, 1));
    CALL(try_prebind(1, 1));

    return OK;
}

static int serve_cipher(ne_socket *sock, void *ud)
{
    char *ciph = ne_sock_cipher(sock);
    char *s = ciph && strlen(ciph) ? ciph : "NULL";

    CALL(full_write(sock, s, strlen(s)));

    if (ciph) ne_free(ciph);
    
    return OK;
}

static int cipher(void)
{
    ne_socket *sock;

#ifdef SOCKET_SSL
    char *ciph;

    CALL(begin(&sock, serve_cipher, NULL));

    ciph = ne_sock_cipher(sock);

    ONN("NULL/empty cipher", ciph == NULL || strlen(ciph) == 0);

    FULLREAD(ciph);
    
    ne_free(ciph);
    
#else
    CALL(begin(&sock, serve_cipher, NULL));

    ONN("non-NULL cipher for non-SSL socket", 
        ne_sock_cipher(sock) != NULL);

    FULLREAD("NULL");

#endif
    return finish(sock, 1);
}

static int error(void)
{
    ne_socket *sock = ne_sock_create();

    ne_sock_set_error(sock, "%s:%s", "fish", "42");
    
    ONCMP("fish:42", ne_sock_error(sock), "socket error", "set");

    ne_sock_close(sock);
    return OK;
}

static int begin_socks(ne_socket **sock, struct socks_server *srv,
                       server_fn server, void *userdata)
{
    srv->server = server;
    srv->userdata = userdata;
    srv->say_hello = 1;
    CALL(spawn_server(7777, socks_server, srv));
    return do_connect(sock, localhost, 7777);
}

static int socks_proxy(void)
{
    static const struct {
        enum ne_sock_sversion version;
        int addr;
        const char *fqdn;
        unsigned int port;
        const char *username, *password;
    } ts[] = {
        { NE_SOCK_SOCKSV4, 4, NULL, 55555, NULL, NULL },
        { NE_SOCK_SOCKSV4, 4, NULL, 55555, "foobar", NULL },
        { NE_SOCK_SOCKSV4A, 0, "www.example.com", 55555, NULL, NULL },
        { NE_SOCK_SOCKSV5, 0, "www.example.com", 55555, NULL, NULL },
        { NE_SOCK_SOCKSV5, 4, NULL, 55555, NULL, NULL },
#ifdef TEST_IPV6
        { NE_SOCK_SOCKSV5, 6, NULL, 55555, NULL, NULL },
#endif
        { NE_SOCK_SOCKSV5, 0, "www.example.com", 55555, "norman", "foobar" }
    };
    unsigned n;

    for (n = 0; n < sizeof(ts)/sizeof(ts[n]); n++) {
        ne_socket *sock;
        struct socks_server arg = {0};
        int ret;

        arg.version = ts[n].version;
        arg.expect_port = ts[n].port;
        if (ts[n].addr == 4)
            arg.expect_addr = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
#ifdef TEST_IPV6
        else if (ts[n].addr == 6)
            arg.expect_addr = ne_iaddr_make(ne_iaddr_ipv4, raw6_cafe);
#endif
        else
            arg.expect_fqdn = ts[n].fqdn;
        arg.username = ts[n].username;
        arg.password = ts[n].password;
        
        CALL(begin_socks(&sock, &arg, echo_server, NULL));

        ret = ne_sock_proxy(sock, ts[n].version, arg.expect_addr, 
                            ts[n].fqdn, ts[n].port,
                            ts[n].username, ts[n].password);
        ONV(ret, ("proxy connect #%u gave %d", n, ret));
        FULLREAD("ok!\n");
        ECHO("hello,\n");
        ECHO("\n");
        ECHO("world\n");
        
        if (ts[n].addr)
            ne_iaddr_free(arg.expect_addr);

        CALL(finish(sock, 0));
    }

    return OK;
}

static int fail_socks(void)
{
    static const struct {
        enum ne_sock_sversion version;
        enum socks_failure failure;
        const char *expect;
        const char *username, *password;
    } ts[] = {
        { NE_SOCK_SOCKSV5, fail_init_vers, 
          "Invalid version in proxy response", NULL, NULL },
        { NE_SOCK_SOCKSV5, fail_init_trunc,
          "Could not read initial response from proxy: Connection closed",
          NULL, NULL },
        { NE_SOCK_SOCKSV5, fail_init_close, 
          "Could not read initial response from proxy: Connection closed", 
          NULL, NULL },
        { NE_SOCK_SOCKSV5, fail_no_auth, 
          "No acceptable authentication method",
          NULL, NULL },
        { NE_SOCK_SOCKSV5, fail_bogus_auth, 
          "Unexpected authentication method chosen",
          NULL, NULL },
        { NE_SOCK_SOCKSV5, fail_auth_close, 
          "Could not read login reply: Connection closed",
          "foo", "bar" },
        { NE_SOCK_SOCKSV5, fail_auth_denied, 
          "Authentication failed", "foo", "bar" }
    };
    unsigned n;

    for (n = 0; n < sizeof(ts)/sizeof(ts[n]); n++) {
        ne_socket *sock;
        struct socks_server arg = {0};
        int ret;

        arg.version = ts[n].version;
        arg.failure = ts[n].failure;
        arg.expect_port = 5555;
        arg.expect_addr = ne_iaddr_make(ne_iaddr_ipv4, raw_127);
        arg.username = ts[n].username;
        arg.password = ts[n].password;
        
        CALL(begin_socks(&sock, &arg, echo_server, NULL));

        ret = ne_sock_proxy(sock, ts[n].version, arg.expect_addr, 
                            NULL, arg.expect_port,
                            ts[n].username, ts[n].password);
        ONV(ret == 0, 
            ("proxy connect #%u succeeded, expected failure '%s'", n, 
             ts[n].expect));
        
        if (ret != 0 && strstr(ne_sock_error(sock), ts[n].expect) == NULL) {
            t_warning("proxy connect #%u got unexpected failure '%s', wanted '%s'",
                      n, ne_sock_error(sock), ts[n].expect);
        }    

        ne_iaddr_free(arg.expect_addr);

        CALL(finish(sock, 0));
    }

    return OK;
}

ne_test tests[] = {
    T(multi_init),
    T_LEAKY(resolve),
    T(resolve_numeric),
#ifdef SOCKET_SSL
    T_LEAKY(init_ssl),
#endif
    T(addr_make_v4),
    T(parse_v4),
    T(addr_make_v6),
    T(addr_compare),
    T(addr_reverse),
    T(just_connect),
    T(addr_connect),
    T(addr_peer),
    T(read_close),
    T(peek_close),
    T(single_read),
    T(single_peek),
    T(small_reads),
    T(read_and_peek),
    T(larger_read),
    T(ssl_session_id),
    T(cipher),
    T(line_simple),
    T(line_closure),
    T(line_empty),
    T(line_toolong),
    T(line_overflow),
    T(line_mingle),
    T(line_chunked),
    T(line_long_chunked),
    T(small_writes),
    T(large_writes),
    T(large_writev),
    T(echo_lines),
    T(blocking),
    T(prebind),
    T(error),
#ifdef SOCKET_SSL
    T(ssl_closure),
    T(ssl_truncate),
#else
    T(write_reset),
    T(read_reset),
#endif
#if TEST_CONNECT_TIMEOUT
    T(connect_timeout),
#endif
    T(read_timeout),
    T(peek_timeout),
    T(readline_timeout),
    T(fullread_timeout),
    T(block_timeout),
    T(socks_proxy),
    T(fail_socks),
    T(NULL)
};
