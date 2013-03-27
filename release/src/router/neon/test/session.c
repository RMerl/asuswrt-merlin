/* 
   Tests for session handling
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

#include "ne_session.h"

#include "tests.h"

static int fill_uri(void)
{
    ne_uri uri = {0};
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    
    ne_fill_server_uri(sess, &uri);

    ONCMP("localhost", uri.host, "fill_uri", "host");
    ONN("port mis-match", uri.port != 7777);
    ONCMP("http", uri.scheme, "fill_uri", "scheme");

    ne_session_destroy(sess);
    ne_uri_free(&uri);

    return OK;
}

static int fill_proxy_uri(void)
{
    ne_uri uri = {0};
    ne_session *sess = ne_session_create("http", "localhost", 7777);

    ne_fill_proxy_uri(sess, &uri);
    
    ONN("no proxy host should be set", uri.host != NULL);
    ONN("no proxy port should be set", uri.port != 0);
    
    ne_session_proxy(sess, "www.example.com", 345);

    ne_fill_proxy_uri(sess, &uri);

    ONCMP("www.example.com", uri.host, "fill_proxy_uri", "host");
    ONN("proxy port mis-match", uri.port != 345);

    ne_session_destroy(sess);
    ne_uri_free(&uri);

    return OK;
}


static int match_hostport(const char *scheme, const char *hostname, int port,
			  const char *hostport)
{
    ne_session *sess = ne_session_create(scheme, hostname, port);
    const char *hp = ne_get_server_hostport(sess);
    ONV(strcmp(hp, hostport),
	("hostport incorrect for %s: `%s' not `%s'", scheme, hp, hostport));
    ne_session_destroy(sess);
    return OK;
}

static int hostports(void)
{
    static const struct {
	const char *scheme, *hostname;
	int port;
	const char *hostport;
    } hps[] = {
	{ "http", "host.name", 80, "host.name" },
	{ "http", "host.name", 555, "host.name:555" },
	{ "http", "host.name", 443, "host.name:443" },
	{ "https", "host.name", 80, "host.name:80" },
	{ "https", "host.name", 443, "host.name" },
	{ "https", "host.name", 700, "host.name:700" },
	{ NULL }
    };
    int n;

    for (n = 0; hps[n].scheme; n++) {
	CALL(match_hostport(hps[n].scheme, hps[n].hostname,
			    hps[n].port, hps[n].hostport));
    }

    return OK;
}


/* Check that ne_set_error is passing through to printf correctly. */
static int errors(void)
{
    ne_session *sess = ne_session_create("http", "foo.com", 80);
    
#define EXPECT "foo, hello world, 100, bar!"

    ne_set_error(sess, "foo, %s, %d, bar!", "hello world", 100);

    ONV(strcmp(ne_get_error(sess), EXPECT),
	("session error was `%s' not `%s'",
	 ne_get_error(sess), EXPECT));
#undef EXPECT

    ne_session_destroy(sess);
    return OK;    
}

#define ID1 "foo"
#define ID2 "bar"

static int privates(void)
{
    ne_session *sess = ne_session_create("http", "localhost", 80);
    char *v1 = "hello", *v2 = "world";

    ne_set_session_private(sess, ID1, v1);
    ne_set_session_private(sess, ID2, v2);

#define PRIV(msg, id, val) \
ONN(msg, ne_get_session_private(sess, id) != val)

    PRIV("private #1 wrong", ID1, v1);
    PRIV("private #2 wrong", ID2, v2);
    PRIV("unknown id wrong", "no such ID", NULL);

    ne_session_destroy(sess);
    return OK;    
}

/* test that ne_session_create doesn't really care what scheme you
 * give it, and that ne_get_scheme() works. */
static int get_scheme(void)
{
    static const char *schemes[] = {
        "http", "https", "ftp", "ldap", "foobar", NULL
    };
    int n;

    for (n = 0; schemes[n]; n++) {
        ne_session *sess = ne_session_create(schemes[n], "localhost", 80);
        ONV(strcmp(ne_get_scheme(sess), schemes[n]),
            ("scheme was `%s' not `%s'!", ne_get_scheme(sess), schemes[n]));
        ne_session_destroy(sess);
    }
    
    return OK;
}

static int flags(void)
{
    ne_session *sess = ne_session_create("https", "localhost", 443);

    ne_set_session_flag(sess, NE_SESSFLAG_PERSIST, 1);
    ONN("persist flag was not set",
        ne_get_session_flag(sess, NE_SESSFLAG_PERSIST) != 1);

    ne_set_session_flag(sess, NE_SESSFLAG_LAST, 1);
    ONN("unsupported flag was recognized",
        ne_get_session_flag(sess, NE_SESSFLAG_LAST) != -1);

    ne_session_destroy(sess);

    return OK;
}

ne_test tests[] = {
    T(fill_uri),
    T(fill_proxy_uri),
    T(hostports),
    T(errors),
    T(privates),
    T(get_scheme),
    T(flags),
    T(NULL)
};

