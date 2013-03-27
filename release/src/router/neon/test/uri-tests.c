/* 
   URI handling tests
   Copyright (C) 2001-2006, Joe Orton <joe@manyfish.co.uk>

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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ne_uri.h"
#include "ne_alloc.h"

#include "tests.h"

static int simple(void)
{
    ne_uri p = {0};
    ON(ne_uri_parse("http://www.webdav.org/foo", &p));
    ON(strcmp(p.host, "www.webdav.org"));
    ON(strcmp(p.path, "/foo"));
    ON(strcmp(p.scheme, "http"));
    ON(p.port);
    ON(p.userinfo != NULL);
    ne_uri_free(&p);
    return 0;
}

static int simple_ssl(void)
{
    ne_uri p = {0};
    ON(ne_uri_parse("https://webdav.org/", &p));
    ON(strcmp(p.scheme, "https"));
    ON(p.port);
    ne_uri_free(&p);
    return OK;
}

static int no_path(void)
{
    ne_uri p = {0};
    ON(ne_uri_parse("https://webdav.org", &p));
    ON(strcmp(p.path, "/"));
    ne_uri_free(&p);
    return OK;
}

static int escapes(void)
{
    const struct {
        const char *plain, *escaped;
    } paths[] = {
        { "/foo%", "/foo%25" },
        { "/foo bar", "/foo%20bar" },
        { "/foo_bar", "/foo_bar" },
        { "/foobar", "/foobar" },
        { "/a\xb9\xb2\xb3\xbc\xbd/", "/a%b9%b2%b3%bc%bd/" },
        { NULL, NULL}
    };
    size_t n;

    for (n = 0; paths[n].plain; n++) {
        char *esc = ne_path_escape(paths[n].plain), *un;

        ONCMP(paths[n].escaped, esc, paths[n].plain, "escape");
        
        un = ne_path_unescape(esc);

        ONCMP(paths[n].plain, un, paths[n].plain, "unescape");
        ne_free(un);
        ne_free(esc);   
    }

    ONN("unescape accepted invalid URI", 
        ne_path_unescape("/foo%zzbar") != NULL);
    ONN("unescape accepted invalid URI", 
        ne_path_unescape("/foo%1zbar") != NULL);

    return OK;
}    

static int parents(void)
{
    static const struct {
	const char *path, *parent;
    } ps[] = {
	{ "/a/b/c", "/a/b/" },
	{ "/a/b/c/", "/a/b/" },
	{ "/alpha/beta", "/alpha/" },
	{ "/foo", "/" },
	{ "norman", NULL },
	{ "/", NULL },
	{ "", NULL },
	{ NULL, NULL }
    };
    int n;
    
    for (n = 0; ps[n].path != NULL; n++) {
	char *p = ne_path_parent(ps[n].path);
	if (ps[n].parent == NULL) {
	    ONV(p != NULL, ("parent of `%s' was `%s' not NULL", 
			    ps[n].path, p));
	} else {
	    ONV(p == NULL, ("parent of `%s' was NULL", ps[n].path));
	    ONV(strcmp(p, ps[n].parent),
		("parent of `%s' was `%s' not `%s'",
		 ps[n].path, p, ps[n].parent));
	    ne_free(p);
	}
    }

    return OK;
}

static int compares(void)
{
    const char *alpha = "/alpha";

    ON(ne_path_compare("/a", "/a/") != 0);
    ON(ne_path_compare("/a/", "/a") != 0);
    ON(ne_path_compare("/ab", "/a/") == 0);
    ON(ne_path_compare("/a/", "/ab") == 0);
    ON(ne_path_compare("/a/", "/a/") != 0);
    ON(ne_path_compare("/alpha/", "/beta/") == 0);
    ON(ne_path_compare("/alpha", "/b") == 0);
    ON(ne_path_compare("/alpha/", "/alphash") == 0);
    ON(ne_path_compare("/fish/", "/food") == 0);
    ON(ne_path_compare(alpha, alpha) != 0);
    ON(ne_path_compare("/a/b/c/d", "/a/b/c/") == 0);
    return OK;
}

static int cmp(void)
{
    static const struct {
        const char *left, *right;
    } eq[] = {
        { "http://example.com/alpha", "http://example.com/alpha" },
        { "//example.com/alpha", "//example.com/alpha" },
        { "http://example.com/alpha#foo", "http://example.com/alpha#foo" },
        { "http://example.com/alpha?bar", "http://example.com/alpha?bar" },
        { "http://jim@example.com/alpha", "http://jim@example.com/alpha" },
        { "HTTP://example.com/alpha", "http://example.com/alpha" },
        { "http://example.com/", "http://example.com" },
        { "http://Example.Com/", "http://example.com" },
        { NULL, NULL}
    }, diff[] = {
        { "http://example.com/alpha", "http://example.com/beta" },
        { "http://example.com/alpha", "https://example.com/alpha" },
        { "http://example.com/alpha", "http://www.example.com/alpha" },
        { "http://example.com:443/alpha", "http://example.com:8080/alpha" },
        { "http://example.com/alpha", "http://jim@example.com/alpha" },
        { "http://bob@example.com/alpha", "http://jim@example.com/alpha" },
        { "http://example.com/alpha", "http://example.com/alpha?fish" },
        { "http://example.com/alpha?fish", "http://example.com/alpha?food" },
        { "http://example.com/alpha", "http://example.com/alpha#foo" },
        { "http://example.com/alpha#bar", "http://example.com/alpha#foo" },
        { "http://example.com/alpha", "//example.com/alpha" },
        { "http://example.com/alpha", "///alpha" },
        { NULL, NULL}
    };
    size_t n;

    for (n = 0; eq[n].left; n++) {
        ne_uri alpha, beta;
        int r1, r2;

        ONV(ne_uri_parse(eq[n].left, &alpha),
            ("could not parse left URI '%s'", eq[n].left));

        ONV(ne_uri_parse(eq[n].right, &beta),
            ("could not parse right URI '%s'", eq[n].right));

        r1 = ne_uri_cmp(&alpha, &beta); 
        r2 = ne_uri_cmp(&beta, &alpha); 

        ONV(r1 != 0,
            ("cmp('%s', '%s') = %d not zero",
             eq[n].left, eq[n].right, r1));

        ONV(r2 != 0,
            ("cmp('%s', '%s') = %d not zero",
             eq[n].right, eq[n].left, r2));

        ne_uri_free(&alpha);
        ne_uri_free(&beta);
    }

    for (n = 0; diff[n].left; n++) {
        ne_uri alpha, beta;
        int r1, r2;

        ONV(ne_uri_parse(diff[n].left, &alpha),
            ("could not parse left URI '%s'", diff[n].left));

        ONV(ne_uri_parse(diff[n].right, &beta),
            ("could not parse right URI '%s'", diff[n].right));

        r1 = ne_uri_cmp(&alpha, &beta); 
        r2 = ne_uri_cmp(&beta, &alpha); 

        ONV(r1 == 0,
            ("'%s' and '%s' did not compare as different",
             diff[n].left, diff[n].right));

        ONV(r1 + r2 != 0,
            ("'%s' and '%s' did not compare reflexively (%d vs %d)",
             diff[n].left, diff[n].right, r1, r2));

        ne_uri_free(&alpha);
        ne_uri_free(&beta);
    }

    return OK;
}

static int children(void)
{
    ON(ne_path_childof("/a", "/a/b") == 0);
    ON(ne_path_childof("/a/", "/a/b") == 0);
    ON(ne_path_childof("/aa/b/c", "/a/b/c/d/e") != 0);
    ON(ne_path_childof("////", "/a") != 0);
    return OK;
}

static int slash(void)
{
    ON(ne_path_has_trailing_slash("/a/") == 0);
    ON(ne_path_has_trailing_slash("/a") != 0);
    {
	/* check the uri == "" case. */
	char *foo = "/";
	ON(ne_path_has_trailing_slash(&foo[1]));
    }
    return OK;
}

static int default_port(void)
{
    ONN("default http: port incorrect", ne_uri_defaultport("http") != 80);
    ONN("default https: port incorrect", ne_uri_defaultport("https") != 443);
    ONN("unspecified scheme: port incorrect", ne_uri_defaultport("ldap") != 0);
    return OK;
}

static int parse(void)
{
    static const struct test_uri {
	const char *uri, *scheme, *host;
	unsigned int port;
	const char *path, *userinfo, *query, *fragment;
    } uritests[] = {
        { "http://webdav.org/norman", "http", "webdav.org", 0, "/norman", NULL, NULL, NULL },
        { "http://webdav.org:/norman", "http", "webdav.org", 0, "/norman", NULL, NULL, NULL },
        { "https://webdav.org/foo", "https", "webdav.org", 0, "/foo", NULL, NULL, NULL },
        { "http://webdav.org:8080/bar", "http", "webdav.org", 8080, "/bar", NULL, NULL, NULL },
        { "http://a/b", "http", "a", 0, "/b", NULL, NULL, NULL },
        { "http://webdav.org/bar:fish", "http", "webdav.org", 0, "/bar:fish", NULL, NULL, NULL },
        { "http://webdav.org", "http", "webdav.org", 0, "/", NULL, NULL, NULL },
        { "http://webdav.org/fish@food", "http", "webdav.org", 0, "/fish@food", NULL, NULL, NULL },

        /* query/fragments */
        { "http://foo/bar?alpha", "http", "foo", 0, "/bar", NULL, "alpha", NULL },
        { "http://foo/bar?alpha#beta", "http", "foo", 0, "/bar", NULL, "alpha", "beta" },
        { "http://foo/bar#alpha?beta", "http", "foo", 0, "/bar", NULL, NULL, "alpha?beta" },
        { "http://foo/bar#beta", "http", "foo", 0, "/bar", NULL, NULL, "beta" },
        { "http://foo/bar?#beta", "http", "foo", 0, "/bar", NULL, "", "beta" },
        { "http://foo/bar?alpha?beta", "http", "foo", 0, "/bar", NULL, "alpha?beta", NULL },

        /* Examples from RFC3986§1.1.2: */
        { "ftp://ftp.is.co.za/rfc/rfc1808.txt", "ftp", "ftp.is.co.za", 0, "/rfc/rfc1808.txt", NULL, NULL, NULL },
        { "http://www.ietf.org/rfc/rfc2396.txt", "http", "www.ietf.org", 0, "/rfc/rfc2396.txt", NULL, NULL, NULL },
        { "ldap://[2001:db8::7]/c=GB?objectClass?one", "ldap", "[2001:db8::7]", 0, "/c=GB", NULL, "objectClass?one", NULL },
        { "mailto:John.Doe@example.com", "mailto", NULL, 0, "John.Doe@example.com", NULL, NULL, NULL }, 
        { "news:comp.infosystems.www.servers.unix", "news", NULL, 0, "comp.infosystems.www.servers.unix", NULL, NULL, NULL },
        { "tel:+1-816-555-1212", "tel", NULL, 0, "+1-816-555-1212", NULL, NULL, NULL },
        { "telnet://192.0.2.16:80/", "telnet", "192.0.2.16", 80, "/", NULL, NULL, NULL },
        { "urn:oasis:names:specification:docbook:dtd:xml:4.1.2", "urn", NULL, 0, 
          "oasis:names:specification:docbook:dtd:xml:4.1.2", NULL}, 

        /* userinfo */
        { "ftp://jim:bob@jim.com", "ftp", "jim.com", 0, "/", "jim:bob", NULL, NULL },
        { "ldap://fred:bloggs@fish.com/foobar", "ldap", "fish.com", 0, 
          "/foobar", "fred:bloggs", NULL, NULL },

        /* IPv6 literals: */
        { "http://[::1]/foo", "http", "[::1]", 0, "/foo", NULL, NULL, NULL },
        { "http://[a:a:a:a::0]/foo", "http", "[a:a:a:a::0]", 0, "/foo", NULL, NULL, NULL },
        { "http://[::1]:8080/bar", "http", "[::1]", 8080, "/bar", NULL, NULL, NULL },
        { "ftp://[feed::cafe]:555", "ftp", "[feed::cafe]", 555, "/", NULL, NULL, NULL },

        { "DAV:", "DAV", NULL, 0, "", NULL, NULL, NULL },
        
        /* Some odd cases: heir-part and relative-ref will both match
         * with a zero-length expansion of "authority" (since *
         * reg-name can be zero-length); so a triple-slash URI-ref
         * will be matched as "//" followed by a zero-length authority
         * followed by a path of "/". */
        { "foo:///", "foo", "", 0, "/", NULL, NULL, NULL },
        { "///", NULL, "", 0, "/", NULL, NULL, NULL },
        /* port grammar is "*DIGIT" so may be empty: */        
        { "ftp://[feed::cafe]:", "ftp", "[feed::cafe]", 0, "/", NULL, NULL, NULL },
        { "ftp://[feed::cafe]:/", "ftp", "[feed::cafe]", 0, "/", NULL, NULL, NULL },
        { "http://foo:/", "http", "foo", 0, "/", NULL, NULL, NULL },

        /* URI-references: */
        { "//foo.com/bar", NULL, "foo.com", 0, "/bar", NULL, NULL, NULL },
        { "//foo.com", NULL, "foo.com", 0, "/", NULL, NULL, NULL },
        { "//[::1]/foo", NULL, "[::1]", 0, "/foo", NULL, NULL, NULL },
        { "/bar", NULL, NULL, 0, "/bar", NULL, NULL, NULL }, /* path-absolute */
        { "foo/bar", NULL, NULL, 0, "foo/bar", NULL, NULL, NULL }, /* path-noscheme */
        { "", NULL, NULL, 0, "", NULL, NULL, NULL }, /* path-empty */

        /* CVE-2007-0157: buffer under-read in 0.26.[012]. */
        { "http://webdav.org\xFF", "http", "webdav.org\xFF", 0, "/", NULL, NULL, NULL },

	{ NULL }
    };
    int n;

    for (n = 0; uritests[n].uri != NULL; n++) {
	ne_uri res;
	const struct test_uri *e = &uritests[n];
	ONV(ne_uri_parse(e->uri, &res) != 0,
	    ("'%s': parse failed", e->uri));
	ONV(res.port != e->port,
	    ("'%s': parsed port was %d not %d", e->uri, res.port, e->port));
	ONCMP(e->scheme, res.scheme, e->uri, "scheme");
	ONCMP(e->host, res.host, e->uri, "host");
	ONV(strcmp(res.path, e->path),
	    ("'%s': parsed path was '%s' not '%s'", e->uri, res.path, e->path));
        ONCMP(e->userinfo, res.userinfo, e->uri, "userinfo");
        ONCMP(e->query, res.query, e->uri, "query");
        ONCMP(e->fragment, res.fragment, e->uri, "fragment");
	ne_uri_free(&res);
    }

    return OK;
}

static int failparse(void)
{
    static const char *uris[] = {
	"http://[::1/",
	"http://[::1]f:80/",
	"http://[::1]]:80/",
        "http://foo/bar asda",
        "http://fish/[foo]/bar",
	NULL
    };
    int n;
    
    for (n = 0; uris[n] != NULL; n++) {
	ne_uri p;
	ONV(ne_uri_parse(uris[n], &p) == 0,
	    ("`%s' did not fail to parse", uris[n]));
	ne_uri_free(&p);
    }

    return 0;
}

static int unparse(void)
{
    const char *uris[] = {
	"http://foo.com/bar",
	"https://bar.com/foo/wishbone",
	"http://www.random.com:8000/",
	"http://[::1]:8080/",
	"ftp://ftp.foo.bar/abc/def",
	"ftp://joe@bar.com/abc/def",
	"http://a/b?c#d",
	"http://a/b?c",
	"http://a/b#d",
        "mailto:foo@bar.com",
        "//foo.com/bar",
        "//foo.com:8080/bar",
	NULL
    };
    int n;

    for (n = 0; uris[n] != NULL; n++) {
	ne_uri parsed;
	char *unp;

	ONV(ne_uri_parse(uris[n], &parsed),
	    ("failed to parse %s", uris[n]));
	
        if (parsed.port == 0 && parsed.scheme)
            parsed.port = ne_uri_defaultport(parsed.scheme);

	unp = ne_uri_unparse(&parsed);

	ONV(strcmp(unp, uris[n]),
	    ("unparse got %s from %s", unp, uris[n]));
	
        ne_uri_free(&parsed);
	ne_free(unp);
    }

    return OK;    
}

#define BASE "http://a/b/c/d;p?q"

static int resolve(void)
{
    static const struct {
        const char *base, *relative, *expected;
    } ts[] = {
        /* Examples from RFC3986§5.4: */
        { BASE, "g:h", "g:h" },
        { BASE, "g", "http://a/b/c/g" },
        { BASE, "./g", "http://a/b/c/g" },
        { BASE, "g/", "http://a/b/c/g/" },
        { BASE, "/g", "http://a/g" },
        { BASE, "//g", "http://g/" }, /* NOTE: modified to mandate non-empty path */
        { BASE, "?y", "http://a/b/c/d;p?y" },
        { BASE, "g?y", "http://a/b/c/g?y" },
        { BASE, "#s", "http://a/b/c/d;p?q#s" },
        { BASE, "g#s", "http://a/b/c/g#s" },
        { BASE, "g?y#s", "http://a/b/c/g?y#s" },
        { BASE, ";x", "http://a/b/c/;x" },
        { BASE, "g;x", "http://a/b/c/g;x" },
        { BASE, "g;x?y#s", "http://a/b/c/g;x?y#s" },
        { BASE, "", "http://a/b/c/d;p?q" },
        { BASE, ".", "http://a/b/c/" },
        { BASE, "./", "http://a/b/c/" },
        { BASE, "..", "http://a/b/" },
        { BASE, "../", "http://a/b/" },
        { BASE, "../g", "http://a/b/g" },
        { BASE, "../..", "http://a/" },
        { BASE, "../../", "http://a/" },
        { BASE, "../../g", "http://a/g" },
        { BASE, "../../../g", "http://a/g" },
        { BASE, "../../../../g", "http://a/g" },
        { BASE, "/./g", "http://a/g" },
        { BASE, "/../g", "http://a/g" },
        { BASE, "g.", "http://a/b/c/g." },
        { BASE, ".g", "http://a/b/c/.g" },
        { BASE, "g..", "http://a/b/c/g.." },
        { BASE, "..g", "http://a/b/c/..g" },
        { BASE, "./../g", "http://a/b/g" },
        { BASE, "./g/.", "http://a/b/c/g/" },
        { BASE, "g/./h", "http://a/b/c/g/h" },
        { BASE, "g/../h", "http://a/b/c/h" },
        { BASE, "g;x=1/./y", "http://a/b/c/g;x=1/y" },
        { BASE, "g;x=1/../y", "http://a/b/c/y" },
        { BASE, "g?y/./x", "http://a/b/c/g?y/./x" },
        { BASE, "g?y/../x", "http://a/b/c/g?y/../x" },
        { BASE, "g#s/./x", "http://a/b/c/g#s/./x" },
        { BASE, "g#s/../x", "http://a/b/c/g#s/../x" },
        { BASE, "http:g", "http:g" },
        /* Additional examples: */
        { BASE, ".", "http://a/b/c/" },
        { "http://foo.com/alpha/beta", "../gamma", "http://foo.com/gamma" },
        { "http://foo.com/alpha//beta", "../gamma", "http://foo.com/alpha/gamma" },

        { "http://foo.com", "../gamma", "http://foo.com/gamma" },
        { "", "zzz:.", "zzz:" },
        { "", "zzz:./foo", "zzz:foo" },
        { "", "zzz:../foo", "zzz:foo" },
        { "", "zzz:/./foo", "zzz:/foo" },
        { "", "zzz:/.", "zzz:/" },
        { "", "zzz:/../", "zzz:/" },
        { "", "zzz:.", "zzz:" },
        { "", "zzz:..", "zzz:" },
        { "", "zzz://foo@bar/", "zzz://foo@bar/" },
        { "", "zzz://foo/?bar", "zzz://foo/?bar" },
        { "zzz://foo/?bar", "//baz/?jam", "zzz://baz/?jam" },
        { "zzz://foo/baz?biz", "", "zzz://foo/baz?biz" },
        { "zzz://foo/baz", "", "zzz://foo/baz" },
        { "//foo/baz", "", "//foo/baz" },


        { NULL, NULL, NULL }
    };
    size_t n;

    for (n = 0; ts[n].base; n++) {
        ne_uri base, relative, resolved;
        char *actual;

        ONV(ne_uri_parse(ts[n].base, &base),
            ("could not parse base URI '%s'", ts[n].base));

        ONV(ne_uri_parse(ts[n].relative, &relative),
            ("could not parse input URI '%s'", ts[n].relative));

        ONN("bad pointer was returned", 
            ne_uri_resolve(&base, &relative, &resolved) != &resolved);

        ONN("NULL path after resolve", resolved.path == NULL);

        actual = ne_uri_unparse(&resolved);
        
        ONCMP(ts[n].expected, actual, ts[n].relative, "output mismatch");
        
        ne_uri_free(&relative);
        ne_uri_free(&resolved);
        ne_uri_free(&base);
        ne_free(actual);
    }

    return OK;
}

static int copy(void)
{
    static const char *ts[] = {
        "http://jim@foo.com:8080/bar?baz#bee",
        "",
        NULL,
    };
    size_t n;

    for (n = 0; ts[n]; n++) {
        ne_uri parsed, parsed2;
        char *actual;

        ONV(ne_uri_parse(ts[n], &parsed), ("could not parse URI '%s'", ts[n]));
        ONN("ne_uri_copy returned wrong pointer",
            ne_uri_copy(&parsed2, &parsed) != &parsed2);

        actual = ne_uri_unparse(&parsed2);

        ONCMP(ts[n], actual, "copied URI", "unparsed URI");

        ne_uri_free(&parsed2);
        ne_uri_free(&parsed);
        ne_free(actual);
    }

    return OK;
}

ne_test tests[] = {
    T(simple),
    T(simple_ssl),
    T(no_path),
    T(escapes),
    T(parents),
    T(compares),
    T(cmp),
    T(children),
    T(slash),
    T(default_port),
    T(parse),
    T(failparse),
    T(unparse),
    T(resolve),
    T(copy),
    T(NULL)
};
