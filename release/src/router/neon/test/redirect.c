/* 
   Tests for 3xx redirect interface (ne_redirect.h)
   Copyright (C) 2002-2003, Joe Orton <joe@manyfish.co.uk>

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

#include "ne_redirect.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

struct redir_args {
    int code;
    const char *dest;
    const char *path;
};

static int serve_redir(ne_socket *sock, void *ud)
{
    struct redir_args *args = ud;
    char buf[BUFSIZ];

    CALL(discard_request(sock));

    ne_snprintf(buf, BUFSIZ, 
		"HTTP/1.0 %d Get Ye Away\r\n"
		"Content-Length: 0\r\n"
		"Location: %s\r\n\n",
		args->code, args->dest);

    SEND_STRING(sock, buf);

    return OK;
}

/* Run a request to 'path' and retrieve the redirect destination to
 * *redir. */
static int process_redir(ne_session *sess, const char *path,
                         const ne_uri **redir)
{
    ONN("did not get NE_REDIRECT", any_request(sess, path) != NE_REDIRECT);
    *redir = ne_redirect_location(sess);
    return OK;
}

static int check_redir(struct redir_args *args, const char *expect)
{
    ne_session *sess;
    const ne_uri *loc;
    char *unp;
    
    CALL(make_session(&sess, serve_redir, args));
    ne_redirect_register(sess);
    
    CALL(process_redir(sess, args->path, &loc));
    ONN("redirect location was NULL", loc == NULL);

    unp = ne_uri_unparse(loc);
    ONV(strcmp(unp, expect), ("redirected to `%s' not `%s'", unp, expect));
    ne_free(unp);

    ne_session_destroy(sess);
    CALL(await_server());

    return OK;
}

#define DEST "http://foo.com/blah/blah/bar"
#define PATH "/redir/me"

static int simple(void)
{
    struct redir_args args[] = {
        {301, DEST, PATH},
        {302, DEST, PATH},
        {303, DEST, PATH},
        {307, DEST, PATH},
        {0, NULL, NULL}
    };
    int n;
    
    for (n = 0; args[n].code; n++)
        CALL(check_redir(&args[n], DEST));

    return OK;
}

/* check that a non-absoluteURI is qualified properly */
static int non_absolute(void)
{
    struct redir_args args = {302, "/foo/bar/blah", PATH};
    return check_redir(&args, "http://localhost:7777/foo/bar/blah");
}

static int relative_1(void)
{
    struct redir_args args = {302, "norman", "/foo/bar"};
    return check_redir(&args, "http://localhost:7777/foo/norman");
}
    
static int relative_2(void)
{
    struct redir_args args = {302, "wishbone", "/foo/bar/"};
    return check_redir(&args, "http://localhost:7777/foo/bar/wishbone");
}    

#if 0
/* could implement failure on self-referential redirects, but
 * realistically, the application must implement a max-redirs count
 * check, so it's kind of redundant.  Mozilla takes this approach. */
static int fail_loop(void)
{
    ne_session *sess;
    
    CALL(make_session(&sess, serve_redir, "http://localhost:7777/foo/bar"));

    ne_redirect_register(sess);

    ONN("followed looping redirect", 
	any_request(sess, "/foo/bar") != NE_ERROR);

    ne_session_destroy(sess);
    return OK;
}
#endif

/* ensure that ne_redirect_location returns NULL when no redirect has
 * been encountered, or redirect hooks aren't registered. */
static int no_redirect(void)
{
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    const ne_uri *loc;

    ONN("redirect non-NULL before register", ne_redirect_location(sess));
    ne_redirect_register(sess);
    ONN("initial redirect non-NULL", ne_redirect_location(sess));

    CALL(spawn_server(7777, single_serve_string, 
                      "HTTP/1.0 200 OK\r\n\r\n\r\n"));
    ONREQ(any_request(sess, "/noredir"));
    CALL(await_server());

    ONN("redirect non-NULL after non-redir req", ne_redirect_location(sess));

    CALL(spawn_server(7777, single_serve_string, "HTTP/1.0 302 Get Ye Away\r\n"
                      "Location: /blah\r\n" "\r\n"));
    CALL(process_redir(sess, "/foo", &loc));
    CALL(await_server());

    ne_session_destroy(sess);
    return OK;
}

ne_test tests[] = {
    T(lookup_localhost),
    T(simple),
    T(non_absolute),
    T(relative_1),
    T(relative_2),
    T(no_redirect),
    T(NULL) 
};

