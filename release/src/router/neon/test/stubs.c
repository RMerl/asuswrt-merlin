/* 
   neon test suite
   Copyright (C) 2002-2005, Joe Orton <joe@manyfish.co.uk>

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

/** These tests show that the stub functions produce appropriate
 * results to provide ABI-compatibility when a particular feature is
 * not supported by the library.
 **/

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ne_request.h"
#include "ne_socket.h"
#include "ne_compress.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

#if defined(NE_HAVE_ZLIB) && defined(NE_HAVE_SSL)
#define NO_TESTS 1
#endif

#define EOL "\r\n"

#ifndef NE_HAVE_ZLIB
static int sd_result = OK;

static int sd_reader(void *ud, const char *block, size_t len)
{
    const char *expect = ud;
    if (strncmp(expect, block, len) != 0) {
	sd_result = FAIL;
	t_context("decompress reader got bad data");
    }
    return 0;
}

static int stub_decompress(void)
{
    ne_session *sess;
    ne_decompress *dc;
    ne_request *req;
    int ret;

    CALL(make_session(&sess, single_serve_string, 
		      "HTTP/1.1 200 OK" EOL
		      "Connection: close" EOL EOL
		      "abcde"));
    
    req = ne_request_create(sess, "GET", "/foo");

    dc = ne_decompress_reader(req, ne_accept_2xx, sd_reader, "abcde");
    
    ret = ne_request_dispatch(req);
    
    CALL(await_server());
    
    ONREQ(ret);

    ne_decompress_destroy(dc);
    ne_request_destroy(req);
    ne_session_destroy(sess);

    /* This is a skeleton test suite file. */
    return sd_result;
}
#endif

#ifndef NE_HAVE_SSL
static int stub_ssl(void)
{
    ne_session *sess = ne_session_create("https", "localhost", 7777);
    ne_ssl_certificate *cert;
    ne_ssl_client_cert *cc;

    /* these should all fail when SSL is not supported. */
    cert = ne_ssl_cert_read("Makefile");
    if (cert) {
        char *dn, digest[60], date[NE_SSL_VDATELEN];
        const ne_ssl_certificate *issuer;

        /* This branch should never be executed, but lets pretend it
         * will to prevent the compiler optimising this code away if
         * it's placed after the cert != NULL test.  And all that
         * needs to be tested is that these functions link OK. */
        dn = ne_ssl_readable_dname(ne_ssl_cert_subject(cert));
        ONN("this code shouldn't run", dn != NULL);
        dn = ne_ssl_readable_dname(ne_ssl_cert_issuer(cert));
        ONN("this code shouldn't run", dn != NULL);
        issuer = ne_ssl_cert_signedby(cert);
        ONN("this code shouldn't run", issuer != NULL);
        ONN("this code shouldn't run", ne_ssl_cert_digest(cert, digest));
        ne_ssl_cert_validity(cert, date, date);
        ONN("this code shouldn't run",
            ne_ssl_dname_cmp(ne_ssl_cert_subject(cert),
                             ne_ssl_cert_issuer(cert)));
        ONN("this code shouldn't run", ne_ssl_cert_identity(issuer) != NULL);
        ONN("this code shouldn't run", ne_ssl_cert_export(cert) != NULL);
    }

    ONN("this code shouldn't run", ne_ssl_cert_import("foo") != NULL);
    ONN("this code shouldn't run", ne_ssl_cert_read("Makefile") != NULL);
    ONN("this code shouldn't succeed", ne_ssl_cert_cmp(NULL, NULL) == 0);

    ONN("certificate load succeeded", cert != NULL);
    ne_ssl_cert_free(cert);

    cc = ne_ssl_clicert_read("Makefile");
    if (cc) {
        const char *name;
        /* dead branch as above. */
        cert = (void *)ne_ssl_clicert_owner(cc);
        ONN("this code shouldn't run", cert != NULL);
        name = ne_ssl_clicert_name(cc);
        ONN("this code shouldn't run", name != NULL);
        ONN("this code shouldn't run", ne_ssl_clicert_decrypt(cc, "fubar"));
        ne_ssl_set_clicert(sess, cc);
    }

    ONN("client certificate load succeeded", cc != NULL);
    ne_ssl_clicert_free(cc);

    ne_ssl_trust_default_ca(sess);

    ne_session_destroy(sess);
    return OK;
}
#endif

#ifdef NO_TESTS
static int null_test(void) { return OK; }
#endif

ne_test tests[] = {
#ifndef NE_HAVE_ZLIB
    T(stub_decompress),
#endif
#ifndef NE_HAVE_SSL
    T(stub_ssl),
#endif
/* to prevent failure when SSL and zlib are supported. */
#ifdef NO_TESTS
    T(null_test),
#endif
    T(NULL) 
};

