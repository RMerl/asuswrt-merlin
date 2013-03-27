/* 
   Authentication tests
   Copyright (C) 2001-2009, Joe Orton <joe@manyfish.co.uk>

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
#include "ne_auth.h"
#include "ne_basic.h"
#include "ne_md5.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

static const char username[] = "Aladdin", password[] = "open sesame";
static int auth_failed;

#define BASIC_WALLY "Basic realm=WallyWorld"
#define CHAL_WALLY "WWW-Authenticate: " BASIC_WALLY

#define EOL "\r\n"

static int auth_cb(void *userdata, const char *realm, int tries, 
		   char *un, char *pw)
{
    if (strcmp(realm, "WallyWorld")) {
        NE_DEBUG(NE_DBG_HTTP, "Got wrong realm '%s'!\n", realm);
        return -1;
    }    
    strcpy(un, username);
    strcpy(pw, password);
    return tries;
}		   

static void auth_hdr(char *value)
{
#define B "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
    auth_failed = strcmp(value, B);
    NE_DEBUG(NE_DBG_HTTP, "Got auth header: [%s]\nWanted header:   [%s]\n"
	     "Result: %d\n", value, B, auth_failed);
#undef B
}

/* Sends a response with given response-code. If hdr is not NULL,
 * sends that header string too (appending an EOL).  If eoc is
 * non-zero, request must be last sent down a connection; otherwise,
 * clength 0 is sent to maintain a persistent connection. */
static int send_response(ne_socket *sock, const char *hdr, int code, int eoc)
{
    char buffer[BUFSIZ];
    
    sprintf(buffer, "HTTP/1.1 %d Blah Blah" EOL, code);
    
    if (hdr) {
	strcat(buffer, hdr);
	strcat(buffer, EOL);
    }

    if (eoc) {
	strcat(buffer, "Connection: close" EOL EOL);
    } else {
	strcat(buffer, "Content-Length: 0" EOL EOL);
    }
	
    return SEND_STRING(sock, buffer);
}

/* Server function which sends two responses: first requires auth,
 * second doesn't. */
static int auth_serve(ne_socket *sock, void *userdata)
{
    char *hdr = userdata;

    auth_failed = 1;

    /* Register globals for discard_request. */
    got_header = auth_hdr;
    want_header = "Authorization";

    discard_request(sock);
    send_response(sock, hdr, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, auth_failed?500:200, 1);

    return 0;
}

/* Test that various Basic auth challenges are correctly handled. */
static int basic(void)
{
    const char *hdrs[] = {
        /* simplest case */
        CHAL_WALLY,

        /* several challenges, one header */
        "WWW-Authenticate: BarFooScheme, " BASIC_WALLY,

        /* several challenges, one header */
        CHAL_WALLY ", BarFooScheme realm=\"PenguinWorld\"",

        /* whitespace tests. */
        "WWW-Authenticate:   Basic realm=WallyWorld   ",

        /* nego test. */
        "WWW-Authenticate: Negotiate fish, Basic realm=WallyWorld",

        /* nego test. */
        "WWW-Authenticate: Negotiate fish, bar=boo, Basic realm=WallyWorld",

        /* nego test. */
        "WWW-Authenticate: Negotiate, Basic realm=WallyWorld",

        /* multi-header case 1 */
        "WWW-Authenticate: BarFooScheme\r\n"
        CHAL_WALLY,
        
        /* multi-header cases 1 */
        CHAL_WALLY "\r\n"
        "WWW-Authenticate: BarFooScheme bar=\"foo\"",

        /* multi-header case 3 */
        "WWW-Authenticate: FooBarChall foo=\"bar\"\r\n"
        CHAL_WALLY "\r\n"
        "WWW-Authenticate: BarFooScheme bar=\"foo\"",

        /* quoting test; fails to handle scheme properly with <= 0.28.2. */
        "WWW-Authenticate: Basic realm=\"WallyWorld\" , BarFooScheme"
    };
    size_t n;
    
    for (n = 0; n < sizeof(hdrs)/sizeof(hdrs[0]); n++) {
        ne_session *sess;
        
        CALL(make_session(&sess, auth_serve, (void *)hdrs[n]));
        ne_set_server_auth(sess, auth_cb, NULL);
        
        CALL(any_2xx_request(sess, "/norman"));
        
        ne_session_destroy(sess);
        CALL(await_server());
    }

    return OK;
}

static int retry_serve(ne_socket *sock, void *ud)
{
    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);
    
    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);
    
    return OK;
}

static int retry_cb(void *userdata, const char *realm, int tries, 
		    char *un, char *pw)
{
    int *count = userdata;

    /* dummy creds; server ignores them anyway. */
    strcpy(un, "a");
    strcpy(pw, "b");

    switch (*count) {
    case 0:
    case 1:
	if (tries == *count) {
	    *count += 1;
	    return 0;
	} else {
	    t_context("On request #%d, got attempt #%d", *count, tries);
	    *count = -1;
	    return 1;
	}
	break;
    case 2:
    case 3:
	/* server fails a subsequent request, check that tries has
	 * reset to zero. */
	if (tries == 0) {
	    *count += 1;
	    return 0;
	} else {
	    t_context("On retry after failure #%d, tries was %d", 
		      *count, tries);
	    *count = -1;
	    return 1;
	}
	break;
    case 4:
    case 5:
	if (tries > 1) {
	    t_context("Attempt counter reached #%d", tries);
	    *count = -1;
	    return 1;
	}
	return tries;
    default:
	t_context("Count reached %d!?", *count);
	*count = -1;
    }
    return 1;
}

/* Test that auth retries are working correctly. */
static int retries(void)
{
    ne_session *sess;
    int count = 0;
    
    CALL(make_session(&sess, retry_serve, NULL));

    ne_set_server_auth(sess, retry_cb, &count);

    /* This request will be 401'ed twice, then succeed. */
    ONREQ(any_request(sess, "/foo"));

    /* auth_cb will have set up context. */
    CALL(count != 2);

    /* this request will be 401'ed once, then succeed. */
    ONREQ(any_request(sess, "/foo"));

    /* auth_cb will have set up context. */
    CALL(count != 3);

    /* some 20x requests. */
    ONREQ(any_request(sess, "/foo"));
    ONREQ(any_request(sess, "/foo"));

    /* this request will be 401'ed once, then succeed. */
    ONREQ(any_request(sess, "/foo"));

    /* auth_cb will have set up context. */
    CALL(count != 4);

    /* First request is 401'ed by the server at both attempts. */
    ONV(any_request(sess, "/foo") != NE_AUTH,
	("auth succeeded, should have failed: %s", ne_get_error(sess)));

    count++;

    /* Second request is 401'ed first time, then will succeed if
     * retried.  0.18.0 didn't reset the attempt counter though so 
     * this didn't work. */
    ONV(any_request(sess, "/foo") == NE_AUTH,
	("auth failed on second try, should have succeeded: %s", ne_get_error(sess)));

    ne_session_destroy(sess);

    CALL(await_server());

    return OK;
}

/* crashes with neon <0.22 */
static int forget_regress(void)
{
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    ne_forget_auth(sess);
    ne_session_destroy(sess);
    return OK;    
}

static int fail_auth_cb(void *ud, const char *realm, int attempt, 
			char *un, char *pw)
{
    return 1;
}

/* this may trigger a segfault in neon 0.21.x and earlier. */
static int tunnel_regress(void)
{
    ne_session *sess = ne_session_create("https", "localhost", 443);
    ne_session_proxy(sess, "localhost", 7777);
    ne_set_server_auth(sess, fail_auth_cb, NULL);
    CALL(spawn_server(7777, single_serve_string,
		      "HTTP/1.1 401 Auth failed.\r\n"
		      "WWW-Authenticate: Basic realm=asda\r\n"
		      "Content-Length: 0\r\n\r\n"));
    any_request(sess, "/foo");
    ne_session_destroy(sess);
    CALL(await_server());
    return OK;
}

/* regression test for parsing a Negotiate challenge with on parameter
 * token. */
static int negotiate_regress(void)
{
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    ne_set_server_auth(sess, fail_auth_cb, NULL);
    CALL(spawn_server(7777, single_serve_string,
		      "HTTP/1.1 401 Auth failed.\r\n"
		      "WWW-Authenticate: Negotiate\r\n"
		      "Content-Length: 0\r\n\r\n"));
    any_request(sess, "/foo");
    ne_session_destroy(sess);
    CALL(await_server());
    return OK;
}

static char *digest_hdr = NULL;

static void dup_header(char *header)
{
    if (digest_hdr) ne_free(digest_hdr);
    digest_hdr = ne_strdup(header);
}

struct digest_parms {
    const char *realm, *nonce, *opaque, *domain;
    int rfc2617;
    int send_ainfo;
    int md5_sess;
    int proxy;
    int send_nextnonce;
    int num_requests;
    int stale;
    enum digest_failure {
        fail_not,
        fail_bogus_alg,
        fail_req0_stale,
        fail_req0_2069_stale,
        fail_omit_qop,
        fail_omit_realm,
        fail_omit_nonce,
        fail_ai_bad_nc,
        fail_ai_bad_nc_syntax,
        fail_ai_bad_digest,
        fail_ai_bad_cnonce,
        fail_ai_omit_cnonce,
        fail_ai_omit_digest,
        fail_ai_omit_nc,
        fail_outside_domain
    } failure;
};

struct digest_state {
    const char *realm, *nonce, *uri, *username, *password, *algorithm, *qop,
        *method, *opaque;
    char *cnonce, *digest, *ncval;
    long nc;
};

/* Write the request-digest into 'digest' (or response-digest if
 * auth_info is non-zero) for given digest auth state and
 * parameters.  */
static void make_digest(struct digest_state *state, struct digest_parms *parms,
                        int auth_info, char digest[33])
{
    struct ne_md5_ctx *ctx;
    char h_a1[33], h_a2[33];

    /* H(A1) */
    ctx = ne_md5_create_ctx();
    ne_md5_process_bytes(state->username, strlen(state->username), ctx);
    ne_md5_process_bytes(":", 1, ctx);
    ne_md5_process_bytes(state->realm, strlen(state->realm), ctx);
    ne_md5_process_bytes(":", 1, ctx);
    ne_md5_process_bytes(state->password, strlen(state->password), ctx);
    ne_md5_finish_ascii(ctx, h_a1);

    if (parms->md5_sess) {
        ne_md5_reset_ctx(ctx);
        ne_md5_process_bytes(h_a1, 32, ctx);
        ne_md5_process_bytes(":", 1, ctx);
        ne_md5_process_bytes(state->nonce, strlen(state->nonce), ctx);
        ne_md5_process_bytes(":", 1, ctx);
        ne_md5_process_bytes(state->cnonce, strlen(state->cnonce), ctx);
        ne_md5_finish_ascii(ctx, h_a1);
    }

    /* H(A2) */
    ne_md5_reset_ctx(ctx);
    if (!auth_info)
        ne_md5_process_bytes(state->method, strlen(state->method), ctx);
    ne_md5_process_bytes(":", 1, ctx);
    ne_md5_process_bytes(state->uri, strlen(state->uri), ctx);
    ne_md5_finish_ascii(ctx, h_a2);

    /* request-digest */
    ne_md5_reset_ctx(ctx);
    ne_md5_process_bytes(h_a1, strlen(h_a1), ctx);
    ne_md5_process_bytes(":", 1, ctx);
    ne_md5_process_bytes(state->nonce, strlen(state->nonce), ctx);
    ne_md5_process_bytes(":", 1, ctx);

    if (parms->rfc2617) {
        ne_md5_process_bytes(state->ncval, strlen(state->ncval), ctx);
        ne_md5_process_bytes(":", 1, ctx);
        ne_md5_process_bytes(state->cnonce, strlen(state->cnonce), ctx);
        ne_md5_process_bytes(":", 1, ctx);
        ne_md5_process_bytes(state->qop, strlen(state->qop), ctx);
        ne_md5_process_bytes(":", 1, ctx);
    }

    ne_md5_process_bytes(h_a2, strlen(h_a2), ctx);
    ne_md5_finish_ascii(ctx, digest);
    ne_md5_destroy_ctx(ctx);
}

/* Verify that the response-digest matches expected state. */
static int check_digest(struct digest_state *state, struct digest_parms *parms)
{
    char digest[33];

    make_digest(state, parms, 0, digest);

    ONV(strcmp(digest, state->digest),
        ("bad digest; expected %s got %s", state->digest, digest));

    return OK;
}

#define DIGCMP(field)                                   \
    do {                                                \
        ONCMP(state->field, newstate.field,            \
              "Digest response header", #field);        \
    } while (0)

#define PARAM(field)                                            \
    do {                                                        \
        if (ne_strcasecmp(name, #field) == 0) {                 \
            ONV(newstate.field != NULL,                        \
                ("received multiple %s params: %s, %s", #field, \
                 newstate.field, val));                        \
            newstate.field = val;                              \
        }                                                       \
    } while (0)

/* Verify that Digest auth request header, 'header', meets expected
 * state and parameters. */
static int verify_digest_header(struct digest_state *state, 
                                struct digest_parms *parms,
                                char *header)
{
    char *ptr;
    struct digest_state newstate = {0};

    ptr = ne_token(&header, ' ');

    ONCMP("Digest", ptr, "Digest response", "scheme name");

    while (header) {
        char *name, *val;

        ptr = ne_qtoken(&header, ',', "\"\'");
        ONN("quoting broken", ptr == NULL);

        name = ne_shave(ptr, " ");

        val = strchr(name, '=');
        ONV(val == NULL, ("bad name/value pair: %s", val));
        
        *val++ = '\0';

        val = ne_shave(val, "\"\' ");

        NE_DEBUG(NE_DBG_HTTP, "got field: [%s] = [%s]\n", name, val);

        PARAM(uri);
        PARAM(realm);
        PARAM(username);
        PARAM(nonce);
        PARAM(algorithm);
        PARAM(qop);
        PARAM(opaque);
        PARAM(cnonce);

        if (ne_strcasecmp(name, "nc") == 0) {
            long nc = strtol(val, NULL, 16);
            
            ONV(nc != state->nc, 
                ("got bad nonce count: %ld (%s) not %ld", 
                 nc, val, state->nc));

            state->ncval = ne_strdup(val);
        }
        else if (ne_strcasecmp(name, "response") == 0) {
            state->digest = ne_strdup(val);
        }
    }

    ONN("cnonce param missing for 2617-style auth",
        parms->rfc2617 && !newstate.cnonce);

    DIGCMP(realm);
    DIGCMP(username);
    if (!parms->domain)
        DIGCMP(uri);
    DIGCMP(nonce);
    DIGCMP(opaque);
    DIGCMP(algorithm);

    if (parms->rfc2617) {
        DIGCMP(qop);
    }
        
    if (newstate.cnonce) {
        state->cnonce = ne_strdup(newstate.cnonce);
    }
    if (parms->domain) {
        state->uri = ne_strdup(newstate.uri);
    }

    ONN("no digest param given", !state->digest);

    CALL(check_digest(state, parms));

    state->nc++;
    
    return OK;
}

static char *make_authinfo_header(struct digest_state *state,
                                  struct digest_parms *parms)
{
    ne_buffer *buf = ne_buffer_create();
    char digest[33], *ncval, *cnonce;

    if (parms->failure == fail_ai_bad_digest) {
        strcpy(digest, "fish");
    } else {
        make_digest(state, parms, 1, digest);
    }

    if (parms->failure == fail_ai_bad_nc_syntax) {
        ncval = "zztop";
    } else if (parms->failure == fail_ai_bad_nc) {
        ncval = "999";
    } else {
        ncval = state->ncval;
    }

    if (parms->failure == fail_ai_bad_cnonce) {
        cnonce = "another-fish";
    } else {
        cnonce = state->cnonce;
    }

    if (parms->proxy) {
        ne_buffer_czappend(buf, "Proxy-");
    }

    ne_buffer_czappend(buf, "Authentication-Info: ");

    if (!parms->rfc2617) {
        ne_buffer_concat(buf, "rspauth=\"", digest, "\"", NULL);
    } else {
        if (parms->failure != fail_ai_omit_nc) {
            ne_buffer_concat(buf, "nc=", ncval, ", ", NULL);
        }
        if (parms->failure != fail_ai_omit_cnonce) {
            ne_buffer_concat(buf, "cnonce=\"", cnonce, "\", ", NULL);
        } 
        if (parms->failure != fail_ai_omit_digest) {
            ne_buffer_concat(buf, "rspauth=\"", digest, "\", ", NULL);
        }
        if (parms->send_nextnonce) {
            state->nonce = ne_concat("next-", state->nonce, NULL);
            ne_buffer_concat(buf, "nextnonce=\"", state->nonce, "\", ", NULL);
            state->nc = 1;
        }
        ne_buffer_czappend(buf, "qop=\"auth\"");
    }
    
    return ne_buffer_finish(buf);
}

static char *make_digest_header(struct digest_state *state,
                                struct digest_parms *parms)
{
    ne_buffer *buf = ne_buffer_create();
    const char *algorithm;

    algorithm = parms->failure == fail_bogus_alg ? "fish" 
        : state->algorithm;

    ne_buffer_concat(buf, 
                     parms->proxy ? "Proxy-Authenticate"
                     : "WWW-Authenticate",
                     ": Digest "
                     "realm=\"", parms->realm, "\", ", NULL);
    
    if (parms->rfc2617) {
        ne_buffer_concat(buf, "algorithm=\"", algorithm, "\", ",
                         "qop=\"", state->qop, "\", ", NULL);
    }

    if (parms->opaque) {
        ne_buffer_concat(buf, "opaque=\"", parms->opaque, "\", ", NULL);
    }

    if (parms->domain) {
        ne_buffer_concat(buf, "domain=\"", parms->domain, "\", ", NULL);
    }

    if (parms->failure == fail_req0_stale
        || parms->failure == fail_req0_2069_stale
        || parms->stale == parms->num_requests) {
        ne_buffer_concat(buf, "stale='true', ", NULL);
    }

    ne_buffer_concat(buf, "nonce=\"", state->nonce, "\"", NULL);

    return ne_buffer_finish(buf);
}

/* Server process for Digest auth handling. */
static int serve_digest(ne_socket *sock, void *userdata)
{
    struct digest_parms *parms = userdata;
    struct digest_state state;
    char resp[NE_BUFSIZ];
    
    if (parms->proxy)
        state.uri = "http://www.example.com/fish";
    else if (parms->domain)
        state.uri = "/fish/0";
    else
        state.uri = "/fish";
    state.method = "GET";
    state.realm = parms->realm;
    state.nonce = parms->nonce;
    state.opaque = parms->opaque;
    state.username = username;
    state.password = password;
    state.nc = 1;
    state.algorithm = parms->md5_sess ? "MD5-sess" : "MD5";
    state.qop = "auth";

    state.cnonce = state.digest = state.ncval = NULL;

    parms->num_requests += parms->stale ? 1 : 0;

    NE_DEBUG(NE_DBG_HTTP, ">>>> Response sequence begins, %d requests.\n",
             parms->num_requests);

    want_header = parms->proxy ? "Proxy-Authorization" : "Authorization";
    digest_hdr = NULL;
    got_header = dup_header;

    CALL(discard_request(sock));

    ONV(digest_hdr != NULL,
        ("got unwarranted WWW-Auth header: %s", digest_hdr));

    ne_snprintf(resp, sizeof resp,
                "HTTP/1.1 %d Auth Denied\r\n"
                "%s\r\n"
                "Content-Length: 0\r\n" "\r\n",
                parms->proxy ? 407 : 401,
                make_digest_header(&state, parms));

    SEND_STRING(sock, resp);

    /* Give up now if we've sent a challenge which should force the
     * client to fail immediately: */
    if (parms->failure == fail_bogus_alg
        || parms->failure == fail_req0_stale
        || parms->failure == fail_req0_2069_stale) {
        return OK;
    }

    do {
        digest_hdr = NULL;
        CALL(discard_request(sock));

        if (digest_hdr && parms->domain && (parms->num_requests & 1) != 0) {
            SEND_STRING(sock, "HTTP/1.1 400 Used Auth Outside Domain\r\n\r\n");
            return OK;
        }
        else if (digest_hdr == NULL && parms->domain && (parms->num_requests & 1) != 0) {
            /* Do nothing. */
            NE_DEBUG(NE_DBG_HTTP, "No Authorization header sent, good.\n");
        }
        else {
            ONN("no Authorization header sent", digest_hdr == NULL);

            CALL(verify_digest_header(&state, parms, digest_hdr));
        }

        if (parms->num_requests == parms->stale) {
            state.nonce = ne_concat("stale-", state.nonce, NULL);
            state.nc = 1;

            ne_snprintf(resp, sizeof resp,
                        "HTTP/1.1 %d Auth Denied\r\n"
                        "%s\r\n"
                        "Content-Length: 0\r\n" "\r\n",
                        parms->proxy ? 407 : 401,
                        make_digest_header(&state, parms));
        }
        else if (parms->send_ainfo) {
            char *ai = make_authinfo_header(&state, parms);
            
            ne_snprintf(resp, sizeof resp,
                        "HTTP/1.1 200 Well, if you insist\r\n"
                        "Content-Length: 0\r\n"
                        "%s\r\n"
                        "\r\n", ai);
            
            ne_free(ai);
        } else {
            ne_snprintf(resp, sizeof resp,
                        "HTTP/1.1 200 You did good\r\n"
                        "Content-Length: 0\r\n" "\r\n");
        }

        SEND_STRING(sock, resp);

        NE_DEBUG(NE_DBG_HTTP, "Handled request; %d requests remain.\n",
                 parms->num_requests - 1);
    } while (--parms->num_requests);

    return OK;
}

static int test_digest(struct digest_parms *parms)
{
    ne_session *sess;

    NE_DEBUG(NE_DBG_HTTP, ">>>> Request sequence begins "
             "(nonce=%s, rfc=%s, stale=%d).\n",
             parms->nonce, parms->rfc2617 ? "2617" : "2069",
             parms->stale);

    if (parms->proxy) {
        sess = ne_session_create("http", "www.example.com", 80);
        ne_session_proxy(sess, "localhost", 7777);
        ne_set_proxy_auth(sess, auth_cb, NULL);
    } 
    else {
        sess = ne_session_create("http", "localhost", 7777);
        ne_set_server_auth(sess, auth_cb, NULL);
    }

    CALL(spawn_server(7777, serve_digest, parms));

    do {
        CALL(any_2xx_request(sess, "/fish"));
    } while (--parms->num_requests);
    
    ne_session_destroy(sess);
    return await_server();
}

/* Test for RFC2617-style Digest auth. */
static int digest(void)
{
    struct digest_parms parms[] = {
        /* RFC 2617-style */
        { "WallyWorld", "this-is-a-nonce", NULL, NULL, 1, 0, 0, 0, 0, 1, 0, fail_not },
        { "WallyWorld", "this-is-also-a-nonce", "opaque-string", NULL, 1, 0, 0, 0, 0, 1, 0, fail_not },
        /* ... with A-I */
        { "WallyWorld", "nonce-nonce-nonce", "opaque-string", NULL, 1, 1, 0, 0, 0, 1, 0, fail_not },
        /* ... with md5-sess. */
        { "WallyWorld", "nonce-nonce-nonce", "opaque-string", NULL, 1, 1, 1, 0, 0, 1, 0, fail_not },
        /* many requests, with changing nonces; tests for next-nonce handling bug. */
        { "WallyWorld", "this-is-a-nonce", "opaque-thingy", NULL, 1, 1, 0, 0, 1, 20, 0, fail_not },

        /* staleness. */
        { "WallyWorld", "this-is-a-nonce", "opaque-thingy", NULL, 1, 1, 0, 0, 0, 3, 2, fail_not },
        /* 2069 + stale */
        { "WallyWorld", "this-is-a-nonce", NULL, NULL, 0, 1, 0, 0, 0, 3, 2, fail_not },

        /* RFC 2069-style */ 
        { "WallyWorld", "lah-di-da-di-dah", NULL, NULL, 0, 0, 0, 0, 0, 1, 0, fail_not },
        { "WallyWorld", "fee-fi-fo-fum", "opaque-string", NULL, 0, 0, 0, 0, 0, 1, 0, fail_not },
        { "WallyWorld", "fee-fi-fo-fum", "opaque-string", NULL, 0, 1, 0, 0, 0, 1, 0, fail_not },

        /* Proxy auth */
        { "WallyWorld", "this-is-also-a-nonce", "opaque-string", NULL, 1, 1, 0, 0, 0, 1, 0, fail_not },
        /* Proxy + A-I */
        { "WallyWorld", "this-is-also-a-nonce", "opaque-string", NULL, 1, 1, 0, 1, 0, 1, 0, fail_not },

        { NULL }
    };
    size_t n;
    
    for (n = 0; parms[n].realm; n++) {
        CALL(test_digest(&parms[n]));

    }

    return OK;
}

static int digest_failures(void)
{
    struct digest_parms parms;
    static const struct {
        enum digest_failure mode;
        const char *message;
    } fails[] = {
        { fail_ai_bad_nc, "nonce count mismatch" },
        { fail_ai_bad_nc_syntax, "could not parse nonce count" },
        { fail_ai_bad_digest, "digest mismatch" },
        { fail_ai_bad_cnonce, "client nonce mismatch" },
        { fail_ai_omit_nc, "missing parameters" },
        { fail_ai_omit_digest, "missing parameters" },
        { fail_ai_omit_cnonce, "missing parameters" },
        { fail_bogus_alg, "unknown algorithm" },
        { fail_req0_stale, "initial Digest challenge was stale" },
        { fail_req0_2069_stale, "initial Digest challenge was stale" },
        { fail_not, NULL }
    };
    size_t n;

    memset(&parms, 0, sizeof parms);
    
    parms.realm = "WallyWorld";
    parms.nonce = "random-invented-string";
    parms.opaque = NULL;
    parms.send_ainfo = 1;
    parms.num_requests = 1;

    for (n = 0; fails[n].message; n++) {
        ne_session *sess = ne_session_create("http", "localhost", 7777);
        int ret;

        parms.failure = fails[n].mode;

        if (parms.failure == fail_req0_2069_stale)
            parms.rfc2617 = 0;
        else
            parms.rfc2617 = 1;

        NE_DEBUG(NE_DBG_HTTP, ">>> New Digest failure test, "
                 "expecting failure '%s'\n", fails[n].message);
        
        ne_set_server_auth(sess, auth_cb, NULL);
        CALL(spawn_server(7777, serve_digest, &parms));
        
        ret = any_2xx_request(sess, "/fish");
        ONV(ret == NE_OK,
            ("request success; expecting error '%s'",
             fails[n].message));

        ONV(strstr(ne_get_error(sess), fails[n].message) == NULL,
            ("request fails with error '%s'; expecting '%s'",
             ne_get_error(sess), fails[n].message));

        ne_session_destroy(sess);
        
        if (fails[n].mode == fail_bogus_alg
            || fails[n].mode == fail_req0_stale) {
            reap_server();
        } else {
            CALL(await_server());
        }
    }

    return OK;
}

static int fail_cb(void *userdata, const char *realm, int tries, 
		   char *un, char *pw)
{
    ne_buffer *buf = userdata;
    char str[64];

    ne_snprintf(str, sizeof str, "<%s, %d>", realm, tries);
    ne_buffer_zappend(buf, str);

    return -1;
}

static int fail_challenge(void)
{
    static const struct {
        const char *resp, *error, *challs;
    } ts[] = {
        /* only possible Basic parse failure. */
        { "Basic", "missing realm in Basic challenge" },

        /* Digest parameter invalid/omitted failure cases: */
        { "Digest algorithm=MD5, qop=auth, nonce=\"foo\"",
          "missing parameter in Digest challenge" },
        { "Digest algorithm=MD5, qop=auth, realm=\"foo\"",
          "missing parameter in Digest challenge" },
        { "Digest algorithm=ZEBEDEE-GOES-BOING, qop=auth, realm=\"foo\"",
          "unknown algorithm in Digest challenge" },
        { "Digest algorithm=MD5-sess, realm=\"foo\"",
          "incompatible algorithm in Digest challenge" },
        { "Digest algorithm=MD5, qop=auth, nonce=\"foo\", realm=\"foo\", "
          "domain=\"http://[::1/\"", "could not parse domain" },

        /* Multiple challenge failure cases: */
        { "Basic, Digest",
          "missing parameter in Digest challenge, missing realm in Basic challenge" },
        
        { "Digest realm=\"foo\", algorithm=MD5, qop=auth, nonce=\"foo\","
          " Basic realm=\"foo\"",
          "rejected Digest challenge, rejected Basic challenge" },

        { "WhizzBangAuth realm=\"foo\", " 
          "Basic realm='foo'",
          "ignored WhizzBangAuth challenge, rejected Basic challenge" },
        { "", "could not parse challenge" },

        /* neon 0.26.x regression in "attempt" handling. */
        { "Basic realm=\"foo\", " 
          "Digest realm=\"bar\", algorithm=MD5, qop=auth, nonce=\"foo\"",
          "rejected Digest challenge, rejected Basic challenge"
          , "<bar, 0><foo, 1>"  /* Digest challenge first, Basic second. */
        }
    };
    unsigned n;
    
    for (n = 0; n < sizeof(ts)/sizeof(ts[0]); n++) {
        char resp[512];
        ne_session *sess;
        int ret;
        ne_buffer *buf = ne_buffer_create();

        ne_snprintf(resp, sizeof resp,
                    "HTTP/1.1 401 Auth Denied\r\n"
                    "WWW-Authenticate: %s\r\n"
                    "Content-Length: 0\r\n" "\r\n",
                    ts[n].resp);
        
        CALL(make_session(&sess, single_serve_string, resp));

        ne_set_server_auth(sess, fail_cb, buf);
        
        ret = any_2xx_request(sess, "/fish");
        ONV(ret == NE_OK,
            ("request success; expecting error '%s'",
             ts[n].error));

        ONV(strstr(ne_get_error(sess), ts[n].error) == NULL,
            ("request fails with error '%s'; expecting '%s'",
             ne_get_error(sess), ts[n].error));
        
        if (ts[n].challs) {
            ONCMP(ts[n].challs, buf->data, "challenge callback", 
                  "invocation order");
        }

        ne_session_destroy(sess);
        ne_buffer_destroy(buf);
        CALL(await_server());
    }

    return OK;
}

struct multi_context {
    int id;
    ne_buffer *buf;
};

static int multi_cb(void *userdata, const char *realm, int tries, 
                    char *un, char *pw)
{
    struct multi_context *ctx = userdata;

    ne_buffer_snprintf(ctx->buf, 128, "[id=%d, realm=%s, tries=%d]", 
                       ctx->id, realm, tries);

    return -1;
}

static int multi_handler(void)
{
    ne_session *sess;
    struct multi_context c[2];
    unsigned n;
    ne_buffer *buf = ne_buffer_create();

    CALL(make_session(&sess, single_serve_string,
                      "HTTP/1.1 401 Auth Denied\r\n"
                      "WWW-Authenticate: Basic realm='fish',"
                      " Digest realm='food', algorithm=MD5, qop=auth, nonce=gaga\r\n"
                      "Content-Length: 0\r\n" "\r\n"));
    
    for (n = 0; n < 2; n++) {
        c[n].buf = buf;
        c[n].id = n + 1;
    }

    ne_add_server_auth(sess, NE_AUTH_BASIC, multi_cb, &c[0]);
    ne_add_server_auth(sess, NE_AUTH_DIGEST, multi_cb, &c[1]);
    
    any_request(sess, "/fish");
    
    ONCMP("[id=2, realm=food, tries=0]"
          "[id=1, realm=fish, tries=0]", buf->data,
          "multiple callback", "invocation order");
    
    ne_session_destroy(sess);
    ne_buffer_destroy(buf);

    return await_server();
}

static int domains(void)
{
    ne_session *sess;
    struct digest_parms parms;

    memset(&parms, 0, sizeof parms);
    parms.realm = "WallyWorld";
    parms.rfc2617 = 1;
    parms.nonce = "agoog";
    parms.domain = "http://localhost:7777/fish/ https://example.com /agaor /other";
    parms.num_requests = 6;

    CALL(make_session(&sess, serve_digest, &parms));

    ne_set_server_auth(sess, auth_cb, NULL);

    ne_session_proxy(sess, "localhost", 7777);

    CALL(any_2xx_request(sess, "/fish/0"));
    CALL(any_2xx_request(sess, "/outside"));
    CALL(any_2xx_request(sess, "/others"));
    CALL(any_2xx_request(sess, "/fish"));
    CALL(any_2xx_request(sess, "/fish/2"));
    CALL(any_2xx_request(sess, "*"));
    
    ne_session_destroy(sess);

    return await_server();
}

/* This segfaulted with 0.28.0 through 0.28.2 inclusive. */
static int CVE_2008_3746(void)
{
    ne_session *sess;
    struct digest_parms parms;

    memset(&parms, 0, sizeof parms);
    parms.realm = "WallyWorld";
    parms.rfc2617 = 1;
    parms.nonce = "agoog";
    parms.domain = "foo";
    parms.num_requests = 1;

    CALL(make_session(&sess, serve_digest, &parms));

    ne_set_server_auth(sess, auth_cb, NULL);

    ne_session_proxy(sess, "localhost", 7777);

    any_2xx_request(sess, "/fish/0");
    
    ne_session_destroy(sess);

    return await_server();
}

static int defaults(void)
{
    ne_session *sess;
    
    CALL(make_session(&sess, auth_serve, CHAL_WALLY));
    ne_add_server_auth(sess, NE_AUTH_DEFAULT, auth_cb, NULL);
    CALL(any_2xx_request(sess, "/norman"));
    ne_session_destroy(sess);
    CALL(await_server());

    CALL(make_session(&sess, auth_serve, CHAL_WALLY));
    ne_add_server_auth(sess, NE_AUTH_ALL, auth_cb, NULL);
    CALL(any_2xx_request(sess, "/norman"));
    ne_session_destroy(sess);
    return await_server();
}

static void fail_hdr(char *value)
{
    auth_failed = 1;
}

static int serve_forgotten(ne_socket *sock, void *userdata)
{
    auth_failed = 0;
    got_header = fail_hdr;
    want_header = "Authorization";
    
    CALL(discard_request(sock));
    if (auth_failed) {
        /* Should not get initial Auth header.  Eek. */
        send_response(sock, NULL, 403, 1);
        return 0;
    }
    send_response(sock, CHAL_WALLY, 401, 0);
    
    got_header = auth_hdr;
    CALL(discard_request(sock));
    if (auth_failed) {
        send_response(sock, NULL, 403, 1);
        return 0;
    }
    send_response(sock, NULL, 200, 0);
    
    ne_sock_read_timeout(sock, 5);

    /* Last time; should get no Auth header. */
    got_header = fail_hdr;
    CALL(discard_request(sock));
    send_response(sock, NULL, auth_failed ? 500 : 200, 1);
    
    return 0;                  
}

static int forget(void)
{
    ne_session *sess;

    CALL(make_session(&sess, serve_forgotten, NULL));

    ne_set_server_auth(sess, auth_cb, NULL);
    
    CALL(any_2xx_request(sess, "/norman"));
    
    ne_forget_auth(sess);

    CALL(any_2xx_request(sess, "/norman"));

    ne_session_destroy(sess);
    
    return await_server();
}

/* proxy auth, proxy AND origin */

ne_test tests[] = {
    T(lookup_localhost),
    T(basic),
    T(retries),
    T(forget_regress),
    T(tunnel_regress),
    T(negotiate_regress),
    T(digest),
    T(digest_failures),
    T(fail_challenge),
    T(multi_handler),
    T(domains),
    T(defaults),
    T(CVE_2008_3746),
    T(forget),
    T(NULL)
};
