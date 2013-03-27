/* 
   tests for compressed response handling.
   Copyright (C) 2001-2008, Joe Orton <joe@manyfish.co.uk>

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

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>

#include "ne_compress.h"
#include "ne_auth.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

static enum { f_partial = 0, f_mismatch, f_complete } failed;

static const char *newsfn = "random.txt";

static int init(void)
{
    return lookup_localhost();
}

#define EXTRA_DEBUG 0 /* disabled by default */

static int reader(void *ud, const char *block, size_t len)
{
    struct string *b = ud;

#if EXTRA_DEBUG
    NE_DEBUG(NE_DBG_HTTP, "reader: got (%d): [[[%.*s]]]\n", (int)len,
             (int)len, block);
#endif

    if (failed == f_mismatch) return -1;

    /* catch multiple len == 0 call as issued by 0.25.0 only: */
    if (failed == f_complete) {
        NE_DEBUG(NE_DBG_HTTP, "reader: called after complete, len=%d\n",
                 (int)len);
        failed = f_mismatch;
        return -1;
    }

    if (failed == f_partial && len == 0) {
        if (b->len != 0) {
            NE_DEBUG(NE_DBG_HTTP, "reader: got length %d at EOF\n",
                     (int)b->len);
            failed = f_mismatch;
        } else {
            failed = f_complete;
        }
        return 0;
    }

    if (len > b->len || memcmp(b->data, block, len) != 0) {
        NE_DEBUG(NE_DBG_HTTP, "reader: failed, got [[%.*s]] not [[%.*s]]\n",
                 (int)len, block, (int)b->len, b->data);
	failed = f_mismatch;
        return -1;
    } else {
	b->data += len;
	b->len -= len;
#if EXTRA_DEBUG
        NE_DEBUG(NE_DBG_HTTP, "reader: OK, %d bytes remaining\n", 
                 (int)b->len);
#endif
    }

    return 0;
}

static int file2buf(int fd, ne_buffer *buf)
{
    char buffer[BUFSIZ];
    ssize_t n;
    
    while ((n = read(fd, buffer, BUFSIZ)) > 0) {
	ne_buffer_append(buf, buffer, n);
    }
    
    return 0;
}

static int do_fetch(const char *realfn, const char *gzipfn,
		    int chunked, int expect_fail)
{
    ne_session *sess;
    ne_request *req;
    int fd, ret;
    ne_buffer *buf = ne_buffer_create();
    struct serve_file_args sfargs;
    ne_decompress *dc;
    struct string body;
    
    fd = open(realfn, O_RDONLY);
    ONN("failed to open file", fd < 0);
    file2buf(fd, buf);
    (void) close(fd);

    body.data = buf->data;
    body.len = buf->used - 1;
    
    failed = f_partial;

    if (gzipfn) {
	sfargs.fname = gzipfn;
	sfargs.headers = "Content-Encoding: gzip\r\n";
    } else {
	sfargs.fname = realfn;
	sfargs.headers = NULL;
    }
    sfargs.chunks = chunked;
    
    CALL(make_session(&sess, serve_file, &sfargs));
    
    req = ne_request_create(sess, "GET", "/");
    dc = ne_decompress_reader(req, ne_accept_2xx, reader, &body);

#ifdef NE_DEBUGGING
    ne_debug_init(ne_debug_stream, ne_debug_mask & ~NE_DBG_HTTPBODY);
#endif

    ret = ne_request_dispatch(req);

#ifdef NE_DEBUGGING
    ne_debug_init(ne_debug_stream, ne_debug_mask | NE_DBG_HTTPBODY);
#endif

    ONN("file not served", ne_get_status(req)->code != 200);
    
    ONN("decompress succeeded", expect_fail && !ret);
    ONV(!expect_fail && ret, ("decompress failed: %s", ne_get_error(sess)));

    NE_DEBUG(NE_DBG_HTTP, "session error: %s\n", ne_get_error(sess));

    ne_decompress_destroy(dc);
    ne_request_destroy(req);
    ne_session_destroy(sess);
    ne_buffer_destroy(buf);

    if (expect_fail) {
        /* if the decompress callback fails, the connection may
         * be aborted and hence the server will abort. */
        reap_server();
    } else {
        CALL(await_server());
    }

    if (!expect_fail) {
        ONN("inflated response truncated", failed == f_partial);
        ONN("inflated response mismatch", failed == f_mismatch);
    }

    return OK;
}

static int fetch(const char *realfn, const char *gzipfn, int chunked)
{
    return do_fetch(realfn, gzipfn, chunked, 0);
}

/* Test the no-compression case. */
static int not_compressed(void)
{
    return fetch(newsfn, NULL, 0);
}

static int simple(void)
{
    return fetch(newsfn, "file1.gz", 0);
}

/* file1.gz has an embedded filename. */
static int withname(void)
{
    return fetch(newsfn, "file2.gz", 0);
}

/* deliver various different sizes of chunks: tests the various
 * decoding cases. */
static int chunked_1b_wn(void)
{
    return fetch(newsfn, "file2.gz", 1);
}

static int chunked_1b(void)
{
    return fetch(newsfn, "file1.gz", 1);
}

static int chunked_12b(void)
{
    return fetch(newsfn, "file2.gz", 12);
}

static int chunked_20b(void)
{
    return fetch(newsfn, "file2.gz", 20);
}

static int chunked_10b(void)
{
    return fetch(newsfn, "file1.gz", 10);
}

static int chunked_10b_wn(void)
{
    return fetch(newsfn, "file2.gz", 10);
}

static int fail_trailing(void)
{
    return do_fetch(newsfn, "trailing.gz", 0, 1);
}

static int fail_trailing_1b(void)
{
    return do_fetch(newsfn, "trailing.gz", 1, 1);
}

static int fail_truncate(void)
{
    return do_fetch(newsfn, "truncated.gz", 0, 1);
}

static int fail_bad_csum(void)
{
    return do_fetch(newsfn, "badcsum.gz", 0, 1);
}

static int fail_corrupt1(void)
{
    return do_fetch(newsfn, "corrupt1.gz", 0, 1);
}

static int fail_corrupt2(void)
{
    return do_fetch(newsfn, "corrupt2.gz", 0, 1);
}

static int fail_empty(void)
{
    return do_fetch(newsfn, "empty.gz", 0, 1);
}

static int notcomp_empty(void)
{
    return fetch("empty.gz", NULL, 0);
}

static int auth_cb(void *userdata, const char *realm, int tries, 
		   char *un, char *pw)
{
    strcpy(un, "foo");
    strcpy(pw, "bar");
    return tries;
}

static int retry_compress_helper(ne_accept_response acceptor,
                                 struct double_serve_args *args, 
                                 struct string *expect)
{
    ne_session *sess;
    ne_request *req;
    ne_decompress *dc;
    
    CALL(make_session(&sess, double_serve_sstring, args));

    ne_set_server_auth(sess, auth_cb, NULL);

    req = ne_request_create(sess, "GET", "/");
    dc = ne_decompress_reader(req, acceptor, reader, expect);
    failed = f_partial;

    ONREQ(ne_request_dispatch(req));
    
    ne_decompress_destroy(dc);

    ONN("got bad response body", failed != f_complete);

    CALL(await_server());

    ne_request_destroy(req);
    ne_session_destroy(sess);

    return OK;
}

#define SSTRING(x) { x, sizeof(x) - 1 }

static struct double_serve_args retry_gz_args = { 
    SSTRING("HTTP/1.1 401 Get Away\r\n"
            "Content-Encoding: gzip\r\n"
            "WWW-Authenticate: Basic realm=WallyWorld\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "abcde"),
    SSTRING("HTTP/1.1 200 OK\r\n"
            "Server: foo\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello")
};

/* Test where the response to the retried request does *not* have
 * a content-encoding, whereas the original 401 response did. */
static int retry_notcompress(void)
{
    struct string expect = { "hello", 5 };
    return retry_compress_helper(ne_accept_2xx, &retry_gz_args, &expect);
}

static struct double_serve_args retry_gz_args2 = {
    SSTRING("HTTP/1.1 401 Get Away\r\n"
            "Content-Encoding: gzip\r\n"
            "WWW-Authenticate: Basic realm=WallyWorld\r\n"
            "Content-Length: 25\r\n"
            "\r\n"
            "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\xcb\x48\xcd\xc9\xc9\x07"
            "\x00\x86\xa6\x10\x36\x05\x00\x00\x00"),
    SSTRING("HTTP/1.1 200 OK\r\n"
            "Server: foo\r\n"
            "Content-Encoding: gzip\r\n"
            "Content-Length: 25\r\n"
            "Connection: close\r\n"
            "\r\n"
            "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03\x2b\xcf\x2f\xca\x49\x01"
            "\x00\x43\x11\x77\x3a\x05\x00\x00\x00")
};

static int retry_accept(void *ud, ne_request *req, const ne_status *st)
{
    struct string *expect = ud;

    NE_DEBUG(NE_DBG_HTTP, "retry_accept callback for %d response\n",
             st->code);

    if (expect->len == 4 && strcmp(expect->data, "fish") == 0) {
        /* first time through */
        expect->data = "hello";
    } else {
        expect->data = "world";
    }

    expect->len = 5;
    failed = f_partial; /* reset the state */
    return 1;
}

/* Test where the response to the retried request *does* have a
 * content-encoding, as did the original 401 response. */
static int retry_compress(void)
{
    struct string expect = { "fish", 4 };
    return retry_compress_helper(retry_accept, &retry_gz_args2, &expect);
}

#define READER_ABORT_ERR "reader_abort error string"

static int reader_abort(void *ud, const char *buf, size_t len)
{
    ne_session *sess = ud;
    ne_set_error(sess, READER_ABORT_ERR);
    return len;
}

/* check that a callback abort does abort the response */
static int compress_abort(void)
{
    ne_session *sess;
    ne_request *req;
    struct serve_file_args sfargs;
    ne_decompress *dc;
    int ret;

    sfargs.fname = "file1.gz";
    sfargs.headers = "Content-Encoding: gzip\r\n";
    sfargs.chunks = 0;

    CALL(make_session(&sess, serve_file, &sfargs));

    req = ne_request_create(sess, "GET", "/abort");

    dc = ne_decompress_reader(req, ne_accept_2xx, reader_abort, sess);

    ret = ne_request_dispatch(req);

    reap_server();

    ONN("request was not aborted", ret != NE_ERROR);
    ONV(strcmp(ne_get_error(sess), READER_ABORT_ERR),
        ("session error was %s not %s",
         ne_get_error(sess), READER_ABORT_ERR));
    
    reap_server();
    ne_decompress_destroy(dc);
    ne_request_destroy(req);
    ne_session_destroy(sess);

    return OK;

}

ne_test tests[] = {
    T_LEAKY(init),
    T(not_compressed),
    T(simple),
    T(withname),
    T(fail_trailing),
    T(fail_trailing_1b),
    T(fail_bad_csum),
    T(fail_truncate),
    T(fail_corrupt1),
    T(fail_corrupt2),
    T(fail_empty),
    T(notcomp_empty),
    T(chunked_1b), 
    T(chunked_1b_wn),
    T(chunked_12b), 
    T(chunked_20b),
    T(chunked_10b),
    T(chunked_10b_wn),
    T(retry_notcompress),
    T(retry_compress),
    T(compress_abort),
    T(NULL)
};
