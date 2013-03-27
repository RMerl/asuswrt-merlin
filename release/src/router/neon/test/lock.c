/* 
   lock tests
   Copyright (C) 2002-2006, Joe Orton <joe@manyfish.co.uk>

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

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ne_request.h"
#include "ne_locks.h"
#include "ne_socket.h"
#include "ne_basic.h"
#include "ne_auth.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

#define EOL "\r\n"

/* returns an activelock XML element. */
static char *activelock(enum ne_lock_scope scope,
			int depth,
			const char *owner,
			long timeout,
			const char *token_href)
{
    static char buf[BUFSIZ];
    
    ne_snprintf(buf, BUFSIZ,
		"<D:activelock>\n"
		"<D:locktype><D:write/></D:locktype>\n"
		"<D:lockscope><D:%s/></D:lockscope>\n"
		"<D:depth>%d</D:depth>\n"
		"<D:owner>%s</D:owner>\n"
		"<D:timeout>Second-%ld</D:timeout>\n"
		"<D:locktoken><D:href>%s</D:href></D:locktoken>\n"
		"</D:activelock>",
		scope==ne_lockscope_exclusive?"exclusive":"shared",
		depth, owner, timeout, token_href);

    return buf;
}	

/* return body of LOCK response for given lock. */
static char *lock_response(enum ne_lock_scope scope,
			   int depth,
			   const char *owner,
			   long timeout,
			   const char *token_href)
{
    static char buf[BUFSIZ];
    sprintf(buf, 
	    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	    "<D:prop xmlns:D=\"DAV:\">"
	    "<D:lockdiscovery>%s</D:lockdiscovery></D:prop>\n",
	    activelock(scope, depth, owner, timeout, token_href));
    return buf;
}

/* return body of LOCK response where response gives multiple
 * activelocks (i.e. shared locks). */
static char *multi_lock_response(struct ne_lock **locks)
{
    ne_buffer *buf = ne_buffer_create();
    int n;

    ne_buffer_zappend(buf,
		      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		      "<D:prop xmlns:D=\"DAV:\">"
		      "<D:lockdiscovery>");
    
    for (n = 0; locks[n] != NULL; n++) {
	char *lk = activelock(locks[n]->scope, locks[n]->depth,
			      locks[n]->owner, locks[n]->timeout,
			      locks[n]->token);
	ne_buffer_zappend(buf, lk);
    }

    ne_buffer_zappend(buf, "</D:lockdiscovery></D:prop>");
    return ne_buffer_finish(buf);
}

static char *discover_response(const char *href, const struct ne_lock *lk)
{
    static char buf[BUFSIZ];
    ne_snprintf(buf, BUFSIZ,
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<D:multistatus xmlns:D='DAV:'>\n"
		"<D:response><D:href>%s</D:href><D:propstat>\n"
		"<D:prop><D:lockdiscovery>%s</D:lockdiscovery></D:prop>\n"
		"<D:status>HTTP/1.1 200 OK</D:status></D:propstat>\n"
		"</D:response></D:multistatus>\n",
		href, activelock(lk->scope, lk->depth, lk->owner,
				 7200, lk->token));
    return buf;
}

static struct ne_lock *make_lock(const char *path, const char *token,
				 enum ne_lock_scope scope, int depth)
{
    struct ne_lock *lock = ne_calloc(sizeof *lock);

    if (lock->token) lock->token = ne_strdup(token);
    lock->scope = scope;
    lock->depth = depth;
    lock->uri.host = ne_strdup("localhost");
    lock->uri.scheme = ne_strdup("http");
    lock->uri.path = ne_strdup(path);
    lock->uri.port = 7777;

    return lock;
}

/* Tests for lock store handling. */
static int store_single(void)
{
    ne_lock_store *store = ne_lockstore_create();
    struct ne_lock *lk = make_lock("/foo", "blah", ne_lockscope_exclusive, 0);
    struct ne_lock *lk2;

    ONN("create failed", store == NULL);

    ONN("new lock store not empty", ne_lockstore_first(store) != NULL);

    ne_lockstore_add(store, lk);

    ONN("lock not found in store", ne_lockstore_first(store) != lk);

    ONN(">1 locks in store?", ne_lockstore_next(store) != NULL);

    lk2 = ne_lockstore_findbyuri(store, &lk->uri);

    ONN("lock not found by URI", lk2 == NULL);
    ONN("other lock found by URI", lk2 != lk);
    
    ne_lockstore_remove(store, lk);

    ONN("store not empty after removing lock",
	ne_lockstore_first(store) != NULL);

    ONN("lock still found after removing lock",
	ne_lockstore_findbyuri(store, &lk->uri) != NULL);

    ne_lockstore_destroy(store);
    ne_lock_destroy(lk);
    
    return OK;
}

static int store_several(void)
{
    ne_lock_store *store = ne_lockstore_create();
    struct ne_lock *lk = make_lock("/foo", "blah", ne_lockscope_exclusive, 0);
    struct ne_lock *lk2 = make_lock("/bar", "blee", ne_lockscope_exclusive, 0);
    struct ne_lock *lf, *lf2;

    ONN("create failed", store == NULL);

    ne_lockstore_add(store, lk);
    ne_lockstore_add(store, lk2);

    lf = ne_lockstore_first(store);
    ONN("lock store empty", lf == NULL);
    lf2 = ne_lockstore_next(store);
    ONN("lock store >2 locks", ne_lockstore_next(store) != NULL);

    /* guarantee that _first, _next returned either of the
     * combinations: (lf, lf2) or (lf2, lf) */
    ONN("found wrong locks", ((lf != lk && lf != lk2) ||
			      (lf2 != lk && lf2 != lk2) ||
			      (lf == lf2)));

    ONN("first find failed",
	ne_lockstore_findbyuri(store, &lk->uri) != lk);
    ONN("second find failed",
	ne_lockstore_findbyuri(store, &lk2->uri) != lk2);
    
    ne_lockstore_remove(store, lk);
    ne_lock_destroy(lk);

    ONN("remove left stray lock?", ne_lockstore_first(store) != lk2);

    ONN("remove left >1 lock?", ne_lockstore_next(store) != NULL);

    ne_lockstore_remove(store, lk2);
    ne_lock_destroy(lk2);

    ONN("store not empty after removing all locks",
	ne_lockstore_first(store) != NULL);

    ne_lockstore_destroy(store);
    
    return OK;
}

/* regression test for <= 0.18.2, where timeout field was not parsed correctly. */
static int lock_timeout(void)
{
    ne_session *sess;
    char *resp, *rbody = lock_response(ne_lockscope_exclusive, 0, "me",
				       6500, "opaquelocktoken:foo");
    struct ne_lock *lock = ne_lock_create();

    resp = ne_concat("HTTP/1.1 200 OK\r\n" "Server: neon-test-server\r\n"
		     "Content-type: application/xml" EOL
		     "Lock-Token: <opaquelocktoken:foo>" EOL
		     "Connection: close\r\n\r\n", rbody, NULL);

    CALL(make_session(&sess, single_serve_string, resp));
    ne_free(resp);

    ne_fill_server_uri(sess, &lock->uri);
    lock->uri.path = ne_strdup("/foo");
    lock->timeout = 5;

    ONREQ(ne_lock(sess, lock));

    ONN("lock timeout ignored in response",
	lock->timeout != 6500);

    ne_session_destroy(sess);
    ne_lock_destroy(lock);

    CALL(await_server());

    return OK;
}

static int verify_if;
static const char *verify_if_expect;

static void got_if_header(char *value)
{
    verify_if = !strcmp(verify_if_expect, value);
    NE_DEBUG(NE_DBG_HTTP, "Verified If header, %d: got [%s] expected [%s]\n", 
	     verify_if, value, verify_if_expect);
}

/* Server callback which checks that an If: header is recevied. */
static int serve_verify_if(ne_socket *sock, void *userdata)
{
    /* tell us about If headers in the request. */
    want_header = "If";
    got_header = got_if_header;
    verify_if_expect = userdata;

    verify_if = 0;

    CALL(discard_request(sock));

    if (verify_if) {
	ON(SEND_STRING(sock, "HTTP/1.1 200 OK" EOL));
    } else {
	ON(SEND_STRING(sock, "HTTP/1.1 403 Wrong If Header" EOL));
    }
    
    ON(SEND_STRING(sock, "Connection: close" EOL EOL));
    
    return OK;
}

/* Make a request which will require a lock. */
static int do_request(ne_session *sess, const char *path, int depth,
		      int modparent)
{
    ne_request *req = ne_request_create(sess, "RANDOM", path);

    if (depth > 0) {
	ne_add_depth_header(req, depth);
    }

    if (depth != -1)
	ne_lock_using_resource(req, path, depth);
    if (modparent)
	ne_lock_using_parent(req, path);

    ONREQ(ne_request_dispatch(req));

    ONV(ne_get_status(req)->code != 200,
	("request failed: %s", ne_get_error(sess)));
    
    ne_request_destroy(req);

    return OK;
}

/* Tests If: header submission, for a lock of depth 'lockdepth' at
 * 'lockpath', with a request to 'reqpath' which Depth header of
 * 'reqdepth'.  If modparent is non-zero; the request is flagged to
 * modify the parent resource too. */
static int submit_test(const char *lockpath, int lockdepth,
		       const char *reqpath, int reqdepth,
		       int modparent)
{
    ne_lock_store *store = ne_lockstore_create();
    ne_session *sess;
    struct ne_lock *lk = ne_lock_create();
    char *expect_if;
    int ret;
    
    expect_if = ne_concat("<http://localhost:7777", lockpath, 
			  "> (<somelocktoken>)", NULL);
    CALL(make_session(&sess, serve_verify_if, expect_if));
    ne_free(expect_if);

    ne_fill_server_uri(sess, &lk->uri);
    lk->uri.path = ne_strdup(lockpath);
    lk->token = ne_strdup("somelocktoken");
    lk->depth = lockdepth;
    
    /* register the lock store, and add our lock for "/foo" to it. */
    ne_lockstore_register(store, sess);
    ne_lockstore_add(store, lk);

    ret = do_request(sess, reqpath, reqdepth, modparent);
    CALL(await_server());

    ne_lockstore_destroy(store);
    ne_session_destroy(sess);

    return ret;
}

static int if_simple(void)
{
    return submit_test("/foo", 0, "/foo", 0, 0);
}

static int if_under_infinite(void)
{
    return submit_test("/foo", NE_DEPTH_INFINITE, "/foo/bar", 0, 0);
}

static int if_infinite_over(void)
{
    return submit_test("/foo/bar", 0, "/foo/", NE_DEPTH_INFINITE, 0);
}

static int if_child(void)
{
    return submit_test("/foo/", 0, "/foo/bar", 0, 1);
}

/* this is a special test, where the PARENT resource of "/foo/bar" is
 * modified, but NOT "/foo/bar" itself.  An UNLOCK request on a
 * lock-null resource can do this; see ne_unlock() for the comment.
 * Regression test for neon <= 0.19.3, which didn't handle this
 * correctly. */
static int if_covered_child(void)
{
    return submit_test("/", NE_DEPTH_INFINITE, "/foo/bar", -1, 1);
}

static int serve_discovery(ne_socket *sock, void *userdata)
{
    char buf[BUFSIZ], *resp = userdata;

    ON(discard_request(sock));
    ONN("no PROPFIND body", clength == 0);
    ON(ne_sock_read(sock, buf, clength) < 0);
    ON(SEND_STRING(sock, "HTTP/1.0 207 OK" EOL
		   "Connection: close" EOL EOL));
    ON(SEND_STRING(sock, resp));
    return OK;
}

struct result_args {
    struct ne_lock *lock;
    int result;
};

static int lock_compare(const char *ctx,
			const struct ne_lock *a, const struct ne_lock *b)
{
    ONV(!a->uri.host || !a->uri.scheme || !a->uri.path,
	("URI structure incomplete in %s", ctx));
    ONV(ne_uri_cmp(&a->uri, &b->uri) != 0,
	("URI comparison failed for %s: %s not %s", ctx,
	 ne_uri_unparse(&a->uri), ne_uri_unparse(&b->uri)));
    ONV(a->depth != b->depth, 
	("%s depth was %d not %d", ctx, a->depth, b->depth));
    ONV(a->scope != b->scope,
	("%s scope was %d not %d", ctx, a->scope, b->scope));
    ONV(a->type != b->type,
	("%s type was %d not %d", ctx, a->type, b->type));
    return OK;
}

static void discover_result(void *userdata, const struct ne_lock *lk,
			    const ne_uri *uri, const ne_status *st)
{
    struct result_args *args = userdata;
    args->result = lock_compare("discovered lock", lk, args->lock);
}

static int discover(void)
{
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    char *response;
    int ret;
    struct result_args args;
    
    args.lock = ne_lock_create();

    args.lock->owner = ne_strdup("someowner");
    args.lock->token = ne_strdup("sometoken");

    /* default */
    args.result = FAIL;
    t_context("results callback never invoked");

    ne_fill_server_uri(sess, &args.lock->uri);
    args.lock->uri.path = ne_strdup("/lockme");

    response = discover_response("/lockme", args.lock);
    CALL(spawn_server(7777, serve_discovery, response));

    ret = ne_lock_discover(sess, "/lockme", discover_result, &args);
    CALL(await_server());
    ONREQ(ret);

    ne_lock_destroy(args.lock);
    ne_session_destroy(sess);

    return args.result;
}

/* Check that the token for the response header */
static int lock_shared(void)
{
    ne_session *sess;
    char *resp, *rbody;
    struct ne_lock *lock, *resplocks[3];

#define FILLK(l, s) do { \
(l)->token = strdup("opaquelocktoken:" s); \
(l)->owner = strdup("owner " s); \
(l)->uri.path = strdup("/" s); (l)->uri.host = strdup("localhost"); \
(l)->uri.scheme = strdup("http"); (l)->uri.port = 7777; } while (0)
    
    resplocks[0] = ne_lock_create();
    resplocks[1] = ne_lock_create();
    resplocks[2] = NULL;
    FILLK(resplocks[0], "alpha");
    FILLK(resplocks[1], "beta");
    resplocks[0]->timeout = 100;
    resplocks[1]->timeout = 200;

    rbody = multi_lock_response(resplocks);
    
    resp = ne_concat("HTTP/1.1 200 OK\r\n" "Server: neon-test-server\r\n"
		     "Content-type: application/xml" EOL
		     "Lock-Token: <opaquelocktoken:beta>" EOL
		     "Connection: close\r\n\r\n", rbody, NULL);
    ne_free(rbody);

    CALL(make_session(&sess, single_serve_string, resp));
    ne_free(resp);

    lock = ne_lock_create();
    ne_fill_server_uri(sess, &lock->uri);
    lock->uri.path = ne_strdup("/beta");

    ONREQ(ne_lock(sess, lock));

    CALL(await_server());

    CALL(lock_compare("returned lock", resplocks[1], lock));
    
    ne_session_destroy(sess);
    ne_lock_destroy(lock);
    ne_lock_destroy(resplocks[0]);
    ne_lock_destroy(resplocks[1]);

    return OK;
}

static void dummy_discover(void *userdata, const struct ne_lock *lock,
                           const ne_uri *uri, const ne_status *status)
{
}

/* This failed with neon 0.25.x and earlier when memory leak detection
 * is enabled. */
static int fail_discover(void)
{
    ne_session *sess;
    int ret;
    
    CALL(make_session(&sess, single_serve_string, 
                      "HTTP/1.0 207 OK\r\n" "Connection: close\r\n" "\r\n"
                      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                      "<D:multistatus xmlns:D='DAV:'>\n"
                      "<D:response><D:href>/foo/bar</D:href><D:propstat>\n"
                      "</parse this, my friend>\n"));
    
    ret = ne_lock_discover(sess, "/foo", dummy_discover, NULL);
    CALL(await_server());

    ONN("discovery okay for response with invalid XML!?", ret != NE_ERROR);

    ne_session_destroy(sess);
    return OK;
}

static int no_creds(void *ud, const char *realm, int attempt,
                    char *username, char *password)
{
    return -1;
}

static int fail_lockauth(void)
{
    ne_session *sess;
    struct ne_lock *lock;
    int ret;
    struct many_serve_args args;

    args.str = "HTTP/1.1 401 Auth Denied\r\n"
        "WWW-Authenticate: Basic realm=\"realm@host\"\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    args.count = 2;

    CALL(make_session(&sess, many_serve_string, &args));

    ne_set_server_auth(sess, no_creds, NULL);

    lock = make_lock("/foo", NULL, ne_lockscope_exclusive, NE_DEPTH_ZERO);
    ret = ne_lock(sess, lock);
    ONV(ret != NE_AUTH,
        ("attempt to lock did not fail with NE_AUTH: %d (%s)",
         ret, ne_get_error(sess)));
    ne_lock_destroy(lock);

    lock = make_lock("/bar", "fish", ne_lockscope_exclusive, NE_DEPTH_ZERO);
    lock->token = ne_strdup("opaquelocktoken:gah");
    ret  = ne_unlock(sess, lock);
    ONV(ret != NE_AUTH,
        ("attempt to unlock did not fail with NE_AUTH: %d (%s)",
         ret, ne_get_error(sess)));
    ne_lock_destroy(lock);

    CALL(await_server());
    ne_session_destroy(sess);

    return OK;
}

/* Regression test for neon 0.25.0 regression in ne_lock() error
 * handling. */
static int fail_noheader(void)
{
    ne_session *sess;
    char *resp, *rbody = lock_response(ne_lockscope_exclusive, 0, "me",
                                       6500, "opaquelocktoken:foo");
    struct ne_lock *lock = ne_lock_create();
    int ret;

    resp = ne_concat("HTTP/1.1 200 OK\r\n" "Server: neon-test-server\r\n"
                     "Content-type: application/xml" EOL
                     "Connection: close\r\n\r\n", rbody, NULL);

    CALL(make_session(&sess, single_serve_string, resp));
    ne_free(resp);

    ne_fill_server_uri(sess, &lock->uri);
    lock->uri.path = ne_strdup("/foo");
    lock->timeout = NE_TIMEOUT_INFINITE;

    ret = ne_lock(sess, lock);
    ONN("LOCK request did not fail", ret != NE_ERROR);
    
    ONV(strstr(ne_get_error(sess), 
               "LOCK response missing Lock-Token header") == NULL,
        ("unexpected error: %s", ne_get_error(sess)));

    ne_session_destroy(sess);
    ne_lock_destroy(lock);

    return await_server();
}

ne_test tests[] = {
    T(lookup_localhost),
    T(store_single),
    T(store_several),
    T(if_simple),
    T(if_under_infinite),
    T(if_infinite_over),
    T(if_child),
    T(if_covered_child),
    T(lock_timeout),
    T(lock_shared),
    T(discover),
    T(fail_discover),
    T(fail_lockauth),
    T(fail_noheader),
    T(NULL)
};

