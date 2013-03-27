/* 
   HTTP Authentication routines
   Copyright (C) 1999-2009, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for getpid() */
#endif

#ifdef WIN32
#include <windows.h> /* for GetCurrentThreadId() etc */
#endif

#ifdef HAVE_OPENSSL
#include <openssl/rand.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#if LIBGNUTLS_VERSION_NUMBER < 0x020b00
#include <gcrypt.h>
#else
#include <gnutls/crypto.h>
#endif
#endif

#include <errno.h>
#include <time.h>

#include "ne_md5.h"
#include "ne_dates.h"
#include "ne_request.h"
#include "ne_auth.h"
#include "ne_string.h"
#include "ne_utils.h"
#include "ne_alloc.h"
#include "ne_uri.h"
#include "ne_internal.h"

#ifdef HAVE_GSSAPI
#ifdef HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#ifdef HAVE_GSSAPI_GSSAPI_GENERIC_H
#include <gssapi/gssapi_generic.h>
#endif
#else
#include <gssapi.h>
#endif
#endif

#ifdef HAVE_SSPI
#include "ne_sspi.h"
#endif

#ifdef HAVE_NTLM
#include "ne_ntlm.h"
#endif
 
#define HOOK_SERVER_ID "http://webdav.org/neon/hooks/server-auth"
#define HOOK_PROXY_ID "http://webdav.org/neon/hooks/proxy-auth"

typedef enum { 
    auth_alg_md5,
    auth_alg_md5_sess,
    auth_alg_unknown
} auth_algorithm;

/* Selected method of qop which the client is using */
typedef enum {
    auth_qop_none,
    auth_qop_auth
} auth_qop;

/* A callback/userdata pair registered by the application for
 * a particular set of protocols. */
struct auth_handler {
    unsigned protomask; 

    ne_auth_creds creds;
    void *userdata;
    int attempt; /* number of invocations of this callback for
                  * current request. */
    
    struct auth_handler *next;
};

/* A challenge */
struct auth_challenge {
    const struct auth_protocol *protocol;
    struct auth_handler *handler;
    const char *realm, *nonce, *opaque, *domain;
    unsigned int stale; /* if stale=true */
    unsigned int got_qop; /* we were given a qop directive */
    unsigned int qop_auth; /* "auth" token in qop attrib */
    auth_algorithm alg;
    struct auth_challenge *next;
};

static const struct auth_class {
    const char *id, *req_hdr, *resp_hdr, *resp_info_hdr;
    int status_code; /* Response status-code to trap. */
    int fail_code;   /* NE_* request to fail with. */
    const char *error_noauth; /* Error message template use when
                               * giving up authentication attempts. */
} ah_server_class = {
    HOOK_SERVER_ID,
    "Authorization", "WWW-Authenticate", "Authentication-Info",
    401, NE_AUTH,
    N_("Could not authenticate to server: %s")
}, ah_proxy_class = {
    HOOK_PROXY_ID,
    "Proxy-Authorization", "Proxy-Authenticate", "Proxy-Authentication-Info",
    407, NE_PROXYAUTH,
    N_("Could not authenticate to proxy server: %s")
};

/* Authentication session state. */
typedef struct {
    ne_session *sess;

    /* Which context will auth challenges be accepted? */
    enum {
        AUTH_ANY, /* ignore nothing. */
        AUTH_CONNECT, /* only in response to a CONNECT request. */
        AUTH_NOTCONNECT /* only in non-CONNECT responsees */
    } context;
    
    /* Protocol type for server/proxy auth. */
    const struct auth_class *spec;
    
    /* The protocol used for this authentication session */
    const struct auth_protocol *protocol;

    struct auth_handler *handlers;

    /*** Session details ***/

    /* The username and password we are using to authenticate with */
    char username[NE_ABUFSIZ];

    /* This used for Basic auth */
    char *basic; 
#ifdef HAVE_GSSAPI
    /* for the GSSAPI/Negotiate scheme: */
    char *gssapi_token;
    gss_ctx_id_t gssctx;
    gss_name_t gssname;
    gss_OID gssmech;
#endif
#ifdef HAVE_SSPI
    /* This is used for SSPI (Negotiate/NTLM) auth */
    char *sspi_token;
    void *sspi_context;
#endif
#ifdef HAVE_NTLM
     /* This is used for NTLM auth */
     ne_ntlm_context *ntlm_context;
#endif
    /* These all used for Digest auth */
    char *realm;
    char *nonce;
    char *cnonce;
    char *opaque;
    char **domains; /* list of paths given as domain. */
    size_t ndomains; /* size of domains array */
    auth_qop qop;
    auth_algorithm alg;
    unsigned int nonce_count;
    /* The ASCII representation of the session's H(A1) value */
    char h_a1[33];

    /* Temporary store for half of the Request-Digest
     * (an optimisation - used in the response-digest calculation) */
    struct ne_md5_ctx *stored_rdig;
} auth_session;

struct auth_request {
    /*** Per-request details. ***/
    ne_request *request; /* the request object. */

    /* The method and URI we are using for the current request */
    const char *uri;
    const char *method;
    
    int attempt; /* number of times this request has been retries due
                  * to auth challenges. */
};

/* Used if this protocol takes an unquoted non-name/value-pair
 * parameter in the challenge. */
#define AUTH_FLAG_OPAQUE_PARAM (0x0001)
/* Used if this Authentication-Info may be sent for non-40[17]
 * response for this protocol. */
#define AUTH_FLAG_VERIFY_NON40x (0x0002)
/* Used for broken the connection-based auth schemes. */
#define AUTH_FLAG_CONN_AUTH (0x0004)

struct auth_protocol {
    unsigned id; /* public NE_AUTH_* id. */

    int strength; /* protocol strength for sort order. */

    const char *name; /* protocol name. */
    
    /* Parse the authentication challenge; returns zero on success, or
     * non-zero if this challenge be handled.  'attempt' is the number
     * of times the request has been resent due to auth challenges.
     * On failure, challenge_error() should be used to append an error
     * message to the error buffer 'errmsg'. */
    int (*challenge)(auth_session *sess, int attempt,
                     struct auth_challenge *chall, ne_buffer **errmsg);

    /* Return the string to send in the -Authenticate request header:
     * (ne_malloc-allocated, NUL-terminated string) */
    char *(*response)(auth_session *sess, struct auth_request *req);
    
    /* Parse a Authentication-Info response; returns NE_* error code
     * on failure; on failure, the session error string must be
     * set. */
    int (*verify)(struct auth_request *req, auth_session *sess,
                  const char *value);
    
    int flags; /* AUTH_FLAG_* flags */
};

/* Helper function to append an error to the buffer during challenge
 * handling.  Pass printf-style string.  *errmsg may be NULL and is
 * allocated if necessary.  errmsg must be non-NULL. */
static void challenge_error(ne_buffer **errmsg, const char *fmt, ...)
    ne_attribute((format(printf, 2, 3)));

/* Free the domains array, precondition sess->ndomains > 0. */
static void free_domains(auth_session *sess)
{
    do {
        ne_free(sess->domains[sess->ndomains - 1]);
    } while (--sess->ndomains);
    ne_free(sess->domains);
    sess->domains = NULL;
}

static void clean_session(auth_session *sess) 
{
    if (sess->basic) ne_free(sess->basic);
    if (sess->nonce) ne_free(sess->nonce);
    if (sess->cnonce) ne_free(sess->cnonce);
    if (sess->opaque) ne_free(sess->opaque);
    if (sess->realm) ne_free(sess->realm);
    sess->realm = sess->basic = sess->cnonce = sess->nonce =
        sess->opaque = NULL;
    if (sess->stored_rdig) {
        ne_md5_destroy_ctx(sess->stored_rdig);
        sess->stored_rdig = NULL;
    }
    if (sess->ndomains) free_domains(sess);
#ifdef HAVE_GSSAPI
    {
        unsigned int major;

        if (sess->gssctx != GSS_C_NO_CONTEXT)
            gss_delete_sec_context(&major, &sess->gssctx, GSS_C_NO_BUFFER);
        
    }
    if (sess->gssapi_token) ne_free(sess->gssapi_token);
    sess->gssapi_token = NULL;
#endif
#ifdef HAVE_SSPI
    if (sess->sspi_token) ne_free(sess->sspi_token);
    sess->sspi_token = NULL;
    ne_sspi_destroy_context(sess->sspi_context);
    sess->sspi_context = NULL;
#endif
#ifdef HAVE_NTLM
    if (sess->ntlm_context) {
        ne__ntlm_destroy_context(sess->ntlm_context);
        sess->ntlm_context = NULL;
    }
#endif

    sess->protocol = NULL;
}

/* Returns client nonce string. */
static char *get_cnonce(void) 
{
    char ret[33];
    unsigned char data[256];
    struct ne_md5_ctx *hash;

    hash = ne_md5_create_ctx();

#ifdef HAVE_GNUTLS
    if (1) {
#if LIBGNUTLS_VERSION_NUMBER < 0x020b00
        gcry_create_nonce(data, sizeof data);
#else
        gnutls_rnd(GNUTLS_RND_NONCE, data, sizeof data);
#endif
        ne_md5_process_bytes(data, sizeof data, hash);
    }
    else
#elif defined(HAVE_OPENSSL)
    if (RAND_status() == 1 && RAND_pseudo_bytes(data, sizeof data) >= 0) {
	ne_md5_process_bytes(data, sizeof data, hash);
    } 
    else 
#endif /* HAVE_OPENSSL */
    {
        /* Fallback sources of random data: all bad, but no good sources
         * are available. */
        
        /* Uninitialized stack data; yes, happy valgrinders, this is
         * supposed to be here. */
        ne_md5_process_bytes(data, sizeof data, hash);
        
        {
#ifdef HAVE_GETTIMEOFDAY
            struct timeval tv;
            if (gettimeofday(&tv, NULL) == 0)
                ne_md5_process_bytes(&tv, sizeof tv, hash);
#else /* HAVE_GETTIMEOFDAY */
            time_t t = time(NULL);
            ne_md5_process_bytes(&t, sizeof t, hash);
#endif
        }
        {
#ifdef WIN32
            DWORD pid = GetCurrentThreadId();
#else
            pid_t pid = getpid();
#endif
            ne_md5_process_bytes(&pid, sizeof pid, hash);
        }
    }

    ne_md5_finish_ascii(hash, ret);
    ne_md5_destroy_ctx(hash);

    return ne_strdup(ret);
}

/* Callback to retrieve user credentials for given session on given
 * attempt (pre request) for given challenge.  Password is written to
 * pwbuf (of size NE_ABUFSIZ.  On error, challenge_error() is used
 * with errmsg. */
static int get_credentials(auth_session *sess, ne_buffer **errmsg, int attempt,
                           struct auth_challenge *chall, char *pwbuf) 
{
    if (chall->handler->creds(chall->handler->userdata, sess->realm, 
                              chall->handler->attempt++, sess->username, pwbuf) == 0) {
        return 0;
    } else {
        challenge_error(errmsg, _("rejected %s challenge"), 
                        chall->protocol->name);
        return -1;
    }
}

/* Examine a Basic auth challenge.
 * Returns 0 if an valid challenge, else non-zero. */
static int basic_challenge(auth_session *sess, int attempt,
                           struct auth_challenge *parms,
                           ne_buffer **errmsg) 
{
    char *tmp, password[NE_ABUFSIZ];

    /* Verify challenge... must have a realm */
    if (parms->realm == NULL) {
        challenge_error(errmsg, _("missing realm in Basic challenge"));
	return -1;
    }

    clean_session(sess);
    
    sess->realm = ne_strdup(parms->realm);

    if (get_credentials(sess, errmsg, attempt, parms, password)) {
	/* Failed to get credentials */
	return -1;
    }

    tmp = ne_concat(sess->username, ":", password, NULL);
    sess->basic = ne_base64((unsigned char *)tmp, strlen(tmp));
    ne_free(tmp);

    /* Paranoia. */
    memset(password, 0, sizeof password);

    return 0;
}

/* Add Basic authentication credentials to a request */
static char *request_basic(auth_session *sess, struct auth_request *req) 
{
    return ne_concat("Basic ", sess->basic, "\r\n", NULL);
}

#ifdef HAVE_GSSAPI
/* Add GSSAPI authentication credentials to a request */
static char *request_negotiate(auth_session *sess, struct auth_request *req)
{
    if (sess->gssapi_token) 
        return ne_concat("Negotiate ", sess->gssapi_token, "\r\n", NULL);
    else
        return NULL;
}

/* Create an GSSAPI name for server HOSTNAME; returns non-zero on
 * error. */
static void get_gss_name(gss_name_t *server, const char *hostname)
{
    unsigned int major, minor;
    gss_buffer_desc token;

    token.value = ne_concat("HTTP@", hostname, NULL);
    token.length = strlen(token.value);

    major = gss_import_name(&minor, &token, GSS_C_NT_HOSTBASED_SERVICE,
                            server);
    ne_free(token.value);
    
    if (GSS_ERROR(major)) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: gss_import_name failed.\n");
        *server = GSS_C_NO_NAME;
    }
}

/* Append GSSAPI error(s) for STATUS of type TYPE to BUF; prepending
 * ": " to each error if *FLAG is non-zero, setting *FLAG after an
 * error has been appended. */
static void make_gss_error(ne_buffer *buf, int *flag,
                           unsigned int status, int type)
{
    unsigned int major, minor;
    unsigned int context = 0;
    
    do {
        gss_buffer_desc msg;
        major = gss_display_status(&minor, status, type,
                                   GSS_C_NO_OID, &context, &msg);
        if (major == GSS_S_COMPLETE && msg.length) {
            if ((*flag)++) ne_buffer_append(buf, ": ", 2);
            ne_buffer_append(buf, msg.value, msg.length);
        }
        if (msg.length) gss_release_buffer(&minor, &msg);
    } while (context);
}

/* Continue a GSS-API Negotiate exchange, using input TOKEN if
 * non-NULL.  Returns non-zero on error, in which case *errmsg is
 * guaranteed to be non-NULL (i.e. an error message is set). */
static int continue_negotiate(auth_session *sess, const char *token,
                              ne_buffer **errmsg)
{
    unsigned int major, minor;
    gss_buffer_desc input = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
    unsigned char *bintoken = NULL;
    int ret;

    if (token) {
        input.length = ne_unbase64(token, &bintoken);
        if (input.length == 0) {
            challenge_error(errmsg, _("invalid Negotiate token"));
            return -1;
        }
        input.value = bintoken;
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: Continuation token [%s]\n", token);
    }
    else if (sess->gssctx != GSS_C_NO_CONTEXT) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: Reset incomplete context.\n");
        gss_delete_sec_context(&minor, &sess->gssctx, GSS_C_NO_BUFFER);
    }

    major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, &sess->gssctx,
                                 sess->gssname, sess->gssmech, 
                                 GSS_C_MUTUAL_FLAG, GSS_C_INDEFINITE, 
                                 GSS_C_NO_CHANNEL_BINDINGS,
                                 &input, &sess->gssmech, &output, NULL, NULL);

    /* done with the input token. */
    if (bintoken) ne_free(bintoken);

    if (GSS_ERROR(major)) {
        int flag = 0;

        challenge_error(errmsg, _("GSSAPI authentication error: "));
        make_gss_error(*errmsg, &flag, major, GSS_C_GSS_CODE);
        make_gss_error(*errmsg, &flag, minor, GSS_C_MECH_CODE);

        return -1;
    }

    if (major == GSS_S_CONTINUE_NEEDED || major == GSS_S_COMPLETE) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: init_sec_context OK. (major=%d)\n",
                 major);
        ret = 0;
    } 
    else {
        challenge_error(errmsg, _("GSSAPI failure (code %u)"), major);
        ret = -1;
    }

    if (major != GSS_S_CONTINUE_NEEDED) {
        /* context no longer needed: destroy it */
        gss_delete_sec_context(&minor, &sess->gssctx, GSS_C_NO_BUFFER);
    }

    if (output.length) {
        sess->gssapi_token = ne_base64(output.value, output.length);
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: Output token: [%s]\n", 
                 sess->gssapi_token);
        gss_release_buffer(&minor, &output);
    } else {
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: No output token.\n");
    }

    return ret;
}

/* Process a Negotiate challange CHALL in session SESS; returns zero
 * if challenge is accepted. */
static int negotiate_challenge(auth_session *sess, int attempt,
                               struct auth_challenge *chall,
                               ne_buffer **errmsg) 
{
    const char *token = chall->opaque;

    /* Respect an initial challenge - which must have no input token,
     * or a continuation - which must have an input token. */
    if (attempt == 0 || token) {
        return continue_negotiate(sess, token, errmsg);
    }
    else {
        challenge_error(errmsg, _("ignoring empty Negotiate continuation"));
        return -1;
    }
}

/* Verify the header HDR in a Negotiate response. */
static int verify_negotiate_response(struct auth_request *req, auth_session *sess,
                                     const char *hdr)
{
    char *duphdr = ne_strdup(hdr);
    char *sep, *ptr = strchr(duphdr, ' ');
    int ret;
    ne_buffer *errmsg = NULL;

    if (!ptr || strncmp(hdr, "Negotiate", ptr - duphdr) != 0) {
        ne_set_error(sess->sess, _("Negotiate response verification failed: "
                                   "invalid response header token"));
        ne_free(duphdr);
        return NE_ERROR;
    }
    
    ptr++;

    if (strlen(ptr) == 0) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: No token in Negotiate response!\n");
        ne_free(duphdr);
        return NE_OK;
    }

    if ((sep = strchr(ptr, ',')) != NULL)
        *sep = '\0';
    if ((sep = strchr(ptr, ' ')) != NULL)
        *sep = '\0';

    NE_DEBUG(NE_DBG_HTTPAUTH, "gssapi: Negotiate response token [%s]\n", ptr);
    ret = continue_negotiate(sess, ptr, &errmsg);
    if (ret) {
        ne_set_error(sess->sess, _("Negotiate response verification failure: %s"),
                     errmsg->data);
    }

    if (errmsg) ne_buffer_destroy(errmsg);
    ne_free(duphdr);

    return ret ? NE_ERROR : NE_OK;
}
#endif

#ifdef HAVE_SSPI
static char *request_sspi(auth_session *sess, struct auth_request *request) 
{
    if (sess->sspi_token)
        return ne_concat(sess->protocol->name, " ", sess->sspi_token, "\r\n", NULL);
    else
        return NULL;
}

static int continue_sspi(auth_session *sess, int ntlm, const char *hdr)
{
    int status;
    char *response = NULL;
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: SSPI challenge.\n");
    
    if (!sess->sspi_context) {
        ne_uri uri = {0};

        ne_fill_server_uri(sess->sess, &uri);

        status = ne_sspi_create_context(&sess->sspi_context, uri.host, ntlm);

        ne_uri_free(&uri);

        if (status) {
            return status;
        }
    }
    
    status = ne_sspi_authenticate(sess->sspi_context, hdr, &response);
    if (status) {
        return status;
    }

    if (response && *response) {
        sess->sspi_token = response;
        
        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: SSPI challenge [%s]\n", sess->sspi_token);
    }

    return 0;
}

static int sspi_challenge(auth_session *sess, int attempt,
                          struct auth_challenge *parms,
                          ne_buffer **errmsg) 
{
    int ntlm = ne_strcasecmp(parms->protocol->name, "NTLM") == 0;

    return continue_sspi(sess, ntlm, parms->opaque);
}

static int verify_sspi(struct auth_request *req, auth_session *sess,
                       const char *hdr)
{
    int ntlm = ne_strncasecmp(hdr, "NTLM ", 5) == 0;
    char *ptr = strchr(hdr, ' ');

    if (!ptr) {
        ne_set_error(sess->sess, _("SSPI response verification failed: "
                                   "invalid response header token"));
        return NE_ERROR;
    }

    while(*ptr == ' ')
        ptr++;

    if (*ptr == '\0') {
        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: No token in SSPI response!\n");
        return NE_OK;
    }

    return continue_sspi(sess, ntlm, ptr);
}

#endif

/* Parse the "domain" challenge parameter and set the domains array up
 * in the session appropriately. */
static int parse_domain(auth_session *sess, const char *domain)
{
    char *cp = ne_strdup(domain), *p = cp;
    ne_uri base;
    int invalid = 0;

    memset(&base, 0, sizeof base);
    ne_fill_server_uri(sess->sess, &base);

    do {
        char *token = ne_token(&p, ' ');
        ne_uri rel, absolute;
        
        if (ne_uri_parse(token, &rel) == 0) {
            /* Resolve relative to the Request-URI. */
            base.path = "/";
            ne_uri_resolve(&base, &rel, &absolute);

            /* Compare against the resolved path to check this URI has
             * the same (scheme, host, port) components; ignore it
             * otherwise: */
            base.path = absolute.path;
            if (absolute.path && ne_uri_cmp(&absolute, &base) == 0) {
                sess->domains = ne_realloc(sess->domains, 
                                           ++sess->ndomains *
                                           sizeof(*sess->domains));
                sess->domains[sess->ndomains - 1] = absolute.path;
                NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Using domain %s from %s\n",
                         absolute.path, token);
                absolute.path = NULL;
            }
            else {
                NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Ignoring domain %s\n",
                         token);
            }

            ne_uri_free(&absolute);
        }
        else {
            invalid = 1;
        }
        
        ne_uri_free(&rel);
        
    } while (p && !invalid);

    if (invalid && sess->ndomains) {
        free_domains(sess);
    }

    ne_free(cp);
    base.path = NULL;
    ne_uri_free(&base);

    return invalid;
}

#ifdef HAVE_NTLM

static char *request_ntlm(auth_session *sess, struct auth_request *request) 
{
    char *token = ne__ntlm_getRequestToken(sess->ntlm_context);
    if (token) {
        char *req = ne_concat(sess->protocol->name, " ", token, "\r\n", NULL);
        ne_free(token);
        return req;
    } else {
        return NULL;
    }
}

static int ntlm_challenge(auth_session *sess, int attempt,
                          struct auth_challenge *parms,
                          ne_buffer **errmsg) 
{
    int status;
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: NTLM challenge.\n");
    
    if (!parms->opaque && (!sess->ntlm_context || (attempt > 1))) {
        char password[NE_ABUFSIZ];

        if (get_credentials(sess, errmsg, attempt, parms, password)) {
            /* Failed to get credentials */
            return -1;
        }

        if (sess->ntlm_context) {
            ne__ntlm_destroy_context(sess->ntlm_context);
            sess->ntlm_context = NULL;
        }

        sess->ntlm_context = ne__ntlm_create_context(sess->username, password);
    }

    status = ne__ntlm_authenticate(sess->ntlm_context, parms->opaque);
    if (status) {
        return status;
    }

    return 0;
}
#endif /* HAVE_NTLM */
  
/* Examine a digest challenge: return 0 if it is a valid Digest challenge,
 * else non-zero. */
static int digest_challenge(auth_session *sess, int attempt,
                            struct auth_challenge *parms,
                            ne_buffer **errmsg) 
{
    char password[NE_ABUFSIZ];

    if (parms->alg == auth_alg_unknown) {
        challenge_error(errmsg, _("unknown algorithm in Digest challenge"));
        return -1;
    }
    else if (parms->alg == auth_alg_md5_sess && !parms->qop_auth) {
        challenge_error(errmsg, _("incompatible algorithm in Digest challenge"));
        return -1;
    }
    else if (parms->realm == NULL || parms->nonce == NULL) {
        challenge_error(errmsg, _("missing parameter in Digest challenge"));
	return -1;
    }
    else if (parms->stale && sess->realm == NULL) {
        challenge_error(errmsg, _("initial Digest challenge was stale"));
        return -1;
    }
    else if (parms->stale && (sess->alg != parms->alg
                              || strcmp(sess->realm, parms->realm))) {
        /* With stale=true the realm and algorithm cannot change since these
         * require re-hashing H(A1) which defeats the point. */
        challenge_error(errmsg, _("stale Digest challenge with new algorithm or realm"));
        return -1;
    }

    if (!parms->stale) {
        /* Non-stale challenge: clear session and request credentials. */
        clean_session(sess);

        /* The domain paramater must be parsed after the session is
         * cleaned; ignore domain for proxy auth. */
        if (parms->domain && sess->spec == &ah_server_class
            && parse_domain(sess, parms->domain)) {
            challenge_error(errmsg, _("could not parse domain in Digest challenge"));
            return -1;
        }

        sess->realm = ne_strdup(parms->realm);
        sess->alg = parms->alg;
        sess->cnonce = get_cnonce();

        if (get_credentials(sess, errmsg, attempt, parms, password)) {
            /* Failed to get credentials */
            return -1;
        }
    }
    else {
        /* Stale challenge: accept a new nonce or opaque. */
        if (sess->nonce) ne_free(sess->nonce);
        if (sess->opaque && parms->opaque) ne_free(sess->opaque);
    }
    
    sess->nonce = ne_strdup(parms->nonce);
    if (parms->opaque) {
	sess->opaque = ne_strdup(parms->opaque);
    }
    
    if (parms->got_qop) {
	/* What type of qop are we to apply to the message? */
	NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Got qop, using 2617-style.\n");
	sess->nonce_count = 0;
        sess->qop = auth_qop_auth;
    } else {
	/* No qop at all/ */
	sess->qop = auth_qop_none;
    }
    
    if (!parms->stale) {
        struct ne_md5_ctx *tmp;

	/* Calculate H(A1).
	 * tmp = H(unq(username-value) ":" unq(realm-value) ":" passwd)
	 */
	tmp = ne_md5_create_ctx();
	ne_md5_process_bytes(sess->username, strlen(sess->username), tmp);
	ne_md5_process_bytes(":", 1, tmp);
	ne_md5_process_bytes(sess->realm, strlen(sess->realm), tmp);
	ne_md5_process_bytes(":", 1, tmp);
	ne_md5_process_bytes(password, strlen(password), tmp);
	memset(password, 0, sizeof password); /* done with that. */
	if (sess->alg == auth_alg_md5_sess) {
	    struct ne_md5_ctx *a1;
	    char tmp_md5_ascii[33];

	    /* Now we calculate the SESSION H(A1)
	     *    A1 = H(...above...) ":" unq(nonce-value) ":" unq(cnonce-value) 
	     */
	    ne_md5_finish_ascii(tmp, tmp_md5_ascii);
	    a1 = ne_md5_create_ctx();
	    ne_md5_process_bytes(tmp_md5_ascii, 32, a1);
	    ne_md5_process_bytes(":", 1, a1);
	    ne_md5_process_bytes(sess->nonce, strlen(sess->nonce), a1);
	    ne_md5_process_bytes(":", 1, a1);
	    ne_md5_process_bytes(sess->cnonce, strlen(sess->cnonce), a1);
	    ne_md5_finish_ascii(a1, sess->h_a1);
            ne_md5_destroy_ctx(a1);
	    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Session H(A1) is [%s]\n", sess->h_a1);
	} else {
	    ne_md5_finish_ascii(tmp, sess->h_a1);
	    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: H(A1) is [%s]\n", sess->h_a1);
	}
        ne_md5_destroy_ctx(tmp);
	
    }
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Accepting digest challenge.\n");

    return 0;
}

/* Returns non-zero if given Request-URI is inside the authentication
 * domain defined for the session. */
static int inside_domain(auth_session *sess, const char *req_uri)
{
    int inside = 0;
    size_t n;
    ne_uri uri;
    
    /* Parse the Request-URI; it will be an absoluteURI if using a
     * proxy, and possibly '*'. */
    if (strcmp(req_uri, "*") == 0 || ne_uri_parse(req_uri, &uri) != 0) {
        /* Presume outside the authentication domain. */
        return 0;
    }

    for (n = 0; n < sess->ndomains && !inside; n++) {
        const char *d = sess->domains[n];
        
        inside = strncmp(uri.path, d, strlen(d)) == 0;
    }
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: '%s' is inside auth domain: %d.\n", 
             uri.path, inside);
    ne_uri_free(&uri);
    
    return inside;
}            

/* Return Digest authentication credentials header value for the given
 * session. */
static char *request_digest(auth_session *sess, struct auth_request *req) 
{
    struct ne_md5_ctx *a2, *rdig;
    char a2_md5_ascii[33], rdig_md5_ascii[33];
    char nc_value[9] = {0};
    const char *qop_value = "auth"; /* qop-value */
    ne_buffer *ret;

    /* Do not submit credentials if an auth domain is defined and this
     * request-uri fails outside it. */
    if (sess->ndomains && !inside_domain(sess, req->uri)) {
        return NULL;
    }

    /* Increase the nonce-count */
    if (sess->qop != auth_qop_none) {
	sess->nonce_count++;
	ne_snprintf(nc_value, 9, "%08x", sess->nonce_count);
    }

    /* Calculate H(A2). */
    a2 = ne_md5_create_ctx();
    ne_md5_process_bytes(req->method, strlen(req->method), a2);
    ne_md5_process_bytes(":", 1, a2);
    ne_md5_process_bytes(req->uri, strlen(req->uri), a2);
    ne_md5_finish_ascii(a2, a2_md5_ascii);
    ne_md5_destroy_ctx(a2);
    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: H(A2): %s\n", a2_md5_ascii);

    /* Now, calculation of the Request-Digest.
     * The first section is the regardless of qop value
     *     H(A1) ":" unq(nonce-value) ":" */
    rdig = ne_md5_create_ctx();

    /* Use the calculated H(A1) */
    ne_md5_process_bytes(sess->h_a1, 32, rdig);

    ne_md5_process_bytes(":", 1, rdig);
    ne_md5_process_bytes(sess->nonce, strlen(sess->nonce), rdig);
    ne_md5_process_bytes(":", 1, rdig);
    if (sess->qop != auth_qop_none) {
	/* Add on:
	 *    nc-value ":" unq(cnonce-value) ":" unq(qop-value) ":"
	 */
	ne_md5_process_bytes(nc_value, 8, rdig);
	ne_md5_process_bytes(":", 1, rdig);
	ne_md5_process_bytes(sess->cnonce, strlen(sess->cnonce), rdig);
	ne_md5_process_bytes(":", 1, rdig);
	/* Store a copy of this structure (see note below) */
        if (sess->stored_rdig) ne_md5_destroy_ctx(sess->stored_rdig);
	sess->stored_rdig = ne_md5_dup_ctx(rdig);
	ne_md5_process_bytes(qop_value, strlen(qop_value), rdig);
	ne_md5_process_bytes(":", 1, rdig);
    }

    /* And finally, H(A2) */
    ne_md5_process_bytes(a2_md5_ascii, 32, rdig);
    ne_md5_finish_ascii(rdig, rdig_md5_ascii);
    ne_md5_destroy_ctx(rdig);

    ret = ne_buffer_create();

    ne_buffer_concat(ret, 
		     "Digest username=\"", sess->username, "\", "
		     "realm=\"", sess->realm, "\", "
		     "nonce=\"", sess->nonce, "\", "
		     "uri=\"", req->uri, "\", "
		     "response=\"", rdig_md5_ascii, "\", "
		     "algorithm=\"", sess->alg == auth_alg_md5 ? "MD5" : "MD5-sess", "\"", 
		     NULL);
    
    if (sess->opaque != NULL) {
	ne_buffer_concat(ret, ", opaque=\"", sess->opaque, "\"", NULL);
    }

    if (sess->qop != auth_qop_none) {
	/* Add in cnonce and nc-value fields */
	ne_buffer_concat(ret, ", cnonce=\"", sess->cnonce, "\", "
			 "nc=", nc_value, ", "
			 "qop=\"", qop_value, "\"", NULL);
    }

    ne_buffer_zappend(ret, "\r\n");

    return ne_buffer_finish(ret);
}

/* Parse line of comma-separated key-value pairs.  If 'ischall' == 1,
 * then also return a leading space-separated token, as *value ==
 * NULL.  Otherwise, if return value is 0, *key and *value will be
 * non-NULL.  If return value is non-zero, parsing has ended.  If
 * 'sep' is non-NULL and ischall is 1, the separator character is
 * written to *sep when a challenge is parsed. */
static int tokenize(char **hdr, char **key, char **value, char *sep,
                    int ischall)
{
    char *pnt = *hdr;
    enum { BEFORE_EQ, AFTER_EQ, AFTER_EQ_QUOTED } state = BEFORE_EQ;
    
    if (**hdr == '\0')
	return 1;

    *key = NULL;

    do {
	switch (state) {
	case BEFORE_EQ:
	    if (*pnt == '=') {
		if (*key == NULL)
		    return -1;
		*pnt = '\0';
		*value = pnt + 1;
		state = AFTER_EQ;
	    } else if ((*pnt == ' ' || *pnt == ',') 
                       && ischall && *key != NULL) {
		*value = NULL;
                if (sep) *sep = *pnt;
		*pnt = '\0';
		*hdr = pnt + 1;
		return 0;
	    } else if (*key == NULL && strchr(" \r\n\t", *pnt) == NULL) {
		*key = pnt;
	    }
	    break;
	case AFTER_EQ:
	    if (*pnt == ',') {
		*pnt = '\0';
		*hdr = pnt + 1;
		return 0;
	    } else if (*pnt == '\"') {
		state = AFTER_EQ_QUOTED;
	    }
	    break;
	case AFTER_EQ_QUOTED:
	    if (*pnt == '\"') {
		state = AFTER_EQ;
                *pnt = '\0';
	    }
	    break;
	}
    } while (*++pnt != '\0');
    
    if (state == BEFORE_EQ && ischall && *key != NULL) {
	*value = NULL;
        if (sep) *sep = '\0';
    }

    *hdr = pnt;

    /* End of string: */
    return 0;
}

/* Pass this the value of the 'Authentication-Info:' header field, if
 * one is received.
 * Returns:
 *    0 if it gives a valid authentication for the server 
 *    non-zero otherwise (don't believe the response in this case!).
 */
static int verify_digest_response(struct auth_request *req, auth_session *sess,
                                  const char *value) 
{
    char *hdr, *pnt, *key, *val;
    auth_qop qop = auth_qop_none;
    char *nextnonce, *rspauth, *cnonce, *nc, *qop_value;
    unsigned int nonce_count;
    int ret = NE_OK;

    nextnonce = rspauth = cnonce = nc = qop_value = NULL;

    pnt = hdr = ne_strdup(value);
    
    NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Got Auth-Info header: %s\n", value);

    while (tokenize(&pnt, &key, &val, NULL, 0) == 0) {
	val = ne_shave(val, "\"");

	if (ne_strcasecmp(key, "qop") == 0) {
            qop_value = val;
            if (ne_strcasecmp(val, "auth") == 0) {
		qop = auth_qop_auth;
	    } else {
		qop = auth_qop_none;
	    }
	} else if (ne_strcasecmp(key, "nextnonce") == 0) {
	    nextnonce = val;
	} else if (ne_strcasecmp(key, "rspauth") == 0) {
	    rspauth = val;
	} else if (ne_strcasecmp(key, "cnonce") == 0) {
	    cnonce = val;
	} else if (ne_strcasecmp(key, "nc") == 0) { 
	    nc = val;
        }
    }

    if (qop == auth_qop_none) {
        /* The 2069-style A-I header only has the entity and nextnonce
         * parameters. */
        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: 2069-style A-I header.\n");
    }
    else if (!rspauth || !cnonce || !nc) {
        ret = NE_ERROR;
        ne_set_error(sess->sess, _("Digest mutual authentication failure: "
                                   "missing parameters"));
    }
    else if (strcmp(cnonce, sess->cnonce) != 0) {
        ret = NE_ERROR;
        ne_set_error(sess->sess, _("Digest mutual authentication failure: "
                                   "client nonce mismatch"));
    }
    else if (nc) {
        char *ptr;
        
        errno = 0;
        nonce_count = strtoul(nc, &ptr, 16);
        if (*ptr != '\0' || errno) {
            ret = NE_ERROR;
            ne_set_error(sess->sess, _("Digest mutual authentication failure: "
                                       "could not parse nonce count"));
        }
        else if (nonce_count != sess->nonce_count) {
            ret = NE_ERROR;
            ne_set_error(sess->sess, _("Digest mutual authentication failure: "
                                       "nonce count mismatch (%u not %u)"),
                         nonce_count, sess->nonce_count);
        }
    }

    /* Finally, for qop=auth cases, if everything else is OK, verify
     * the response-digest field. */    
    if (qop == auth_qop_auth && ret == NE_OK) {
        struct ne_md5_ctx *a2;
        char a2_md5_ascii[33], rdig_md5_ascii[33];

        /* Modified H(A2): */
        a2 = ne_md5_create_ctx();
        ne_md5_process_bytes(":", 1, a2);
        ne_md5_process_bytes(req->uri, strlen(req->uri), a2);
        ne_md5_finish_ascii(a2, a2_md5_ascii);
        ne_md5_destroy_ctx(a2);

        /* sess->stored_rdig contains digest-so-far of:
         *   H(A1) ":" unq(nonce-value) 
         */
        
        /* Add in qop-value */
        ne_md5_process_bytes(qop_value, strlen(qop_value), 
                             sess->stored_rdig);
        ne_md5_process_bytes(":", 1, sess->stored_rdig);

        /* Digest ":" H(A2) */
        ne_md5_process_bytes(a2_md5_ascii, 32, sess->stored_rdig);
        /* All done */
        ne_md5_finish_ascii(sess->stored_rdig, rdig_md5_ascii);
        ne_md5_destroy_ctx(sess->stored_rdig);
        sess->stored_rdig = NULL;

        /* And... do they match? */
        ret = ne_strcasecmp(rdig_md5_ascii, rspauth) == 0 ? NE_OK : NE_ERROR;
        
        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: response-digest match: %s "
                 "(expected [%s] vs actual [%s])\n", 
                 ret == NE_OK ? "yes" : "no", rdig_md5_ascii, rspauth);

        if (ret) {
            ne_set_error(sess->sess, _("Digest mutual authentication failure: "
                                       "request-digest mismatch"));
        }
    }

    /* Check for a nextnonce */
    if (nextnonce != NULL) {
	NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Found nextnonce of [%s].\n", nextnonce);
        ne_free(sess->nonce);
	sess->nonce = ne_strdup(nextnonce);
        sess->nonce_count = 0;
    }

    ne_free(hdr);

    return ret;
}

static const struct auth_protocol protocols[] = {
    { NE_AUTH_BASIC, 10, "Basic",
      basic_challenge, request_basic, NULL,
      0 },
    { NE_AUTH_DIGEST, 20, "Digest",
      digest_challenge, request_digest, verify_digest_response,
      0 },
#ifdef HAVE_GSSAPI
    { NE_AUTH_GSSAPI, 30, "Negotiate",
      negotiate_challenge, request_negotiate, verify_negotiate_response,
      AUTH_FLAG_OPAQUE_PARAM|AUTH_FLAG_VERIFY_NON40x|AUTH_FLAG_CONN_AUTH },
#endif
#ifdef HAVE_SSPI
    { NE_AUTH_NTLM, 30, "NTLM",
      sspi_challenge, request_sspi, NULL,
      AUTH_FLAG_OPAQUE_PARAM|AUTH_FLAG_VERIFY_NON40x|AUTH_FLAG_CONN_AUTH },
    { NE_AUTH_GSSAPI, 30, "Negotiate",
      sspi_challenge, request_sspi, verify_sspi,
      AUTH_FLAG_OPAQUE_PARAM|AUTH_FLAG_VERIFY_NON40x|AUTH_FLAG_CONN_AUTH },
#endif
#ifdef HAVE_NTLM
    { NE_AUTH_NTLM, 30, "NTLM",
      ntlm_challenge, request_ntlm, NULL,
      AUTH_FLAG_OPAQUE_PARAM|AUTH_FLAG_VERIFY_NON40x|AUTH_FLAG_CONN_AUTH },
#endif
    { 0 }
};

/* Insert a new auth challenge for protocol 'proto' in list of
 * challenges 'list'.  The challenge list is kept in sorted order of
 * strength, with highest strength first. */
static struct auth_challenge *insert_challenge(struct auth_challenge **list,
                                               const struct auth_protocol *proto)
{
    struct auth_challenge *ret = ne_calloc(sizeof *ret);
    struct auth_challenge *chall, *prev;

    for (chall = *list, prev = NULL; chall != NULL; 
         prev = chall, chall = chall->next) {
        if (proto->strength > chall->protocol->strength) {
            break;
        }
    }

    if (prev) {
        ret->next = prev->next;
        prev->next = ret;
    } else {
        ret->next = *list;
        *list = ret;
    }

    ret->protocol = proto;

    return ret;
}

static void challenge_error(ne_buffer **errbuf, const char *fmt, ...)
{
    char err[128];
    va_list ap;
    size_t len;
    
    va_start(ap, fmt);
    len = ne_vsnprintf(err, sizeof err, fmt, ap);
    va_end(ap);
    
    if (*errbuf == NULL) {
        *errbuf = ne_buffer_create();
        ne_buffer_append(*errbuf, err, len);
    }
    else {
        ne_buffer_concat(*errbuf, ", ", err, NULL);
    }
}

/* Passed the value of a "(Proxy,WWW)-Authenticate: " header field.
 * Returns 0 if valid challenge was accepted; non-zero if no valid
 * challenge was found. */
static int auth_challenge(auth_session *sess, int attempt,
                          const char *value) 
{
    char *pnt, *key, *val, *hdr, sep;
    struct auth_challenge *chall = NULL, *challenges = NULL;
    ne_buffer *errmsg = NULL;

    pnt = hdr = ne_strdup(value); 

    /* The header value may be made up of one or more challenges.  We
     * split it down into attribute-value pairs, then search for
     * schemes in the pair keys. */

    while (!tokenize(&pnt, &key, &val, &sep, 1)) {

	if (val == NULL) {
            const struct auth_protocol *proto = NULL;
            struct auth_handler *hdl;
            size_t n;

            for (hdl = sess->handlers; hdl; hdl = hdl->next) {
                for (n = 0; protocols[n].id; n++) {
                    if (protocols[n].id & hdl->protomask
                        && ne_strcasecmp(key, protocols[n].name) == 0) {
                        proto = &protocols[n];
                        break;
                    }
                }
                if (proto) break;
            }

            if (proto == NULL) {
                /* Ignore this challenge. */
                chall = NULL;
                challenge_error(&errmsg, _("ignored %s challenge"), key);
                continue;
	    }
            
            NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Got '%s' challenge.\n", proto->name);
            chall = insert_challenge(&challenges, proto);
            chall->handler = hdl;

            if ((proto->flags & AUTH_FLAG_OPAQUE_PARAM) && sep == ' ') {
                /* Cope with the fact that the unquoted base64
                 * paramater token doesn't match the 2617 auth-param
                 * grammar: */
                chall->opaque = ne_shave(ne_token(&pnt, ','), " \t");
                NE_DEBUG(NE_DBG_HTTPAUTH, "auth: %s opaque parameter '%s'\n",
                         proto->name, chall->opaque);
                if (!pnt) break; /* stop parsing at end-of-string. */
            }
	    continue;
	} else if (chall == NULL) {
	    /* Ignore pairs for an unknown challenge. */
            NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Ignored parameter: %s = %s\n", key, val);
	    continue;
	}

	/* Strip quotes off value. */
	val = ne_shave(val, "\"'");

	if (ne_strcasecmp(key, "realm") == 0) {
	    chall->realm = val;
	} else if (ne_strcasecmp(key, "nonce") == 0) {
	    chall->nonce = val;
	} else if (ne_strcasecmp(key, "opaque") == 0) {
	    chall->opaque = val;
	} else if (ne_strcasecmp(key, "stale") == 0) {
	    /* Truth value */
	    chall->stale = (ne_strcasecmp(val, "true") == 0);
	} else if (ne_strcasecmp(key, "algorithm") == 0) {
	    if (ne_strcasecmp(val, "md5") == 0) {
		chall->alg = auth_alg_md5;
	    } else if (ne_strcasecmp(val, "md5-sess") == 0) {
		chall->alg = auth_alg_md5_sess;
	    } else {
		chall->alg = auth_alg_unknown;
	    }
	} else if (ne_strcasecmp(key, "qop") == 0) {
            /* iterate over each token in the value */
            do {
                const char *tok = ne_shave(ne_token(&val, ','), " \t");
                
                if (ne_strcasecmp(tok, "auth") == 0) {
                    chall->qop_auth = 1;
                }
            } while (val);
            
            chall->got_qop = chall->qop_auth;
	}
        else if (ne_strcasecmp(key, "domain") == 0) {
            chall->domain = val;
        }
    }
    
    sess->protocol = NULL;

    /* Iterate through the challenge list (which is sorted from
     * strongest to weakest) attempting to accept each one. */
    for (chall = challenges; chall != NULL; chall = chall->next) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Trying %s challenge...\n",
                 chall->protocol->name);
        if (chall->protocol->challenge(sess, attempt, chall, &errmsg) == 0) {
            NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Accepted %s challenge.\n", 
                     chall->protocol->name);
            sess->protocol = chall->protocol;
            break;
        }
    }

    if (!sess->protocol) {
        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: No challenges accepted.\n");
        ne_set_error(sess->sess, _(sess->spec->error_noauth),
                     errmsg ? errmsg->data : _("could not parse challenge"));
    }

    while (challenges != NULL) {
	chall = challenges->next;
	ne_free(challenges);
	challenges = chall;
    }

    ne_free(hdr);
    if (errmsg) ne_buffer_destroy(errmsg);

    return !(sess->protocol != NULL);
}

static void ah_create(ne_request *req, void *session, const char *method,
		      const char *uri)
{
    auth_session *sess = session;
    int is_connect = strcmp(method, "CONNECT") == 0;

    if (sess->context == AUTH_ANY ||
        (is_connect && sess->context == AUTH_CONNECT) ||
        (!is_connect && sess->context == AUTH_NOTCONNECT)) {
        struct auth_request *areq = ne_calloc(sizeof *areq);
        struct auth_handler *hdl;
        
        NE_DEBUG(NE_DBG_HTTPAUTH, "ah_create, for %s\n", sess->spec->resp_hdr);
        
        areq->method = method;
        areq->uri = uri;
        areq->request = req;
        
        ne_set_request_private(req, sess->spec->id, areq);

        /* For each new request, reset the attempt counter in every
         * registered handler. */
        for (hdl = sess->handlers; hdl; hdl = hdl->next) {
            hdl->attempt = 0;
        }
    }
}


static void ah_pre_send(ne_request *r, void *cookie, ne_buffer *request)
{
    auth_session *sess = cookie;
    struct auth_request *req = ne_get_request_private(r, sess->spec->id);

    if (sess->protocol && req) {
	char *value;

        NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Sending '%s' response.\n",
                 sess->protocol->name);

        value = sess->protocol->response(sess, req);

	if (value != NULL) {
	    ne_buffer_concat(request, sess->spec->req_hdr, ": ", value, NULL);
	    ne_free(value);
	}
    }

}

static int ah_post_send(ne_request *req, void *cookie, const ne_status *status)
{
    auth_session *sess = cookie;
    struct auth_request *areq = ne_get_request_private(req, sess->spec->id);
    const char *auth_hdr, *auth_info_hdr;
    int ret = NE_OK;

    if (!areq) return NE_OK;

    auth_hdr = ne_get_response_header(req, sess->spec->resp_hdr);
    auth_info_hdr = ne_get_response_header(req, sess->spec->resp_info_hdr);

    if (sess->context == AUTH_CONNECT && status->code == 401 && !auth_hdr) {
        /* Some broken proxies issue a 401 as a proxy auth challenge
         * to a CONNECT request; handle this here. */
        auth_hdr = ne_get_response_header(req, "WWW-Authenticate");
        auth_info_hdr = NULL;
    }

#ifdef HAVE_GSSAPI
    /* whatever happens: forget the GSSAPI token cached thus far */
    if (sess->gssapi_token) {
        ne_free(sess->gssapi_token);
        sess->gssapi_token = NULL;
    }
#endif

#ifdef HAVE_SSPI
    /* whatever happens: forget the SSPI token cached thus far */
    if (sess->sspi_token) {
        ne_free(sess->sspi_token);
        sess->sspi_token = NULL;
    }
#endif

    NE_DEBUG(NE_DBG_HTTPAUTH, 
	     "ah_post_send (#%d), code is %d (want %d), %s is %s\n",
	     areq->attempt, status->code, sess->spec->status_code, 
	     sess->spec->resp_hdr, auth_hdr ? auth_hdr : "(none)");
    if (auth_info_hdr && sess->protocol && sess->protocol->verify 
        && (sess->protocol->flags & AUTH_FLAG_VERIFY_NON40x) == 0) {
        ret = sess->protocol->verify(areq, sess, auth_info_hdr);
    }
    else if (sess->protocol && sess->protocol->verify
             && (sess->protocol->flags & AUTH_FLAG_VERIFY_NON40x) 
             && (status->klass == 2 || status->klass == 3)
             && auth_hdr) {
        ret = sess->protocol->verify(areq, sess, auth_hdr);
    }
    else if ((status->code == sess->spec->status_code ||
              (status->code == 401 && sess->context == AUTH_CONNECT)) &&
	       auth_hdr) {
        /* note above: allow a 401 in response to a CONNECT request
         * from a proxy since some buggy proxies send that. */
	NE_DEBUG(NE_DBG_HTTPAUTH, "auth: Got challenge (code %d).\n", status->code);
	if (!auth_challenge(sess, areq->attempt++, auth_hdr)) {
	    ret = NE_RETRY;
	} else {
	    clean_session(sess);
	    ret = sess->spec->fail_code;
	}
        
        /* Set or clear the conn-auth flag according to whether this
         * was an accepted challenge for a borked protocol. */
        ne_set_session_flag(sess->sess, NE_SESSFLAG_CONNAUTH,
                            sess->protocol 
                            && (sess->protocol->flags & AUTH_FLAG_CONN_AUTH));
    }

#ifdef HAVE_SSPI
    /* Clear the SSPI context after successfull authentication. */
    if ((status->klass == 2 || status->klass == 3) && sess->sspi_context) {
        ne_sspi_clear_context(sess->sspi_context);
    }
#endif

    return ret;
}

static void ah_destroy(ne_request *req, void *session)
{
    auth_session *sess = session;
    struct auth_request *areq = ne_get_request_private(req, sess->spec->id);

    if (areq) {
        ne_free(areq);
    }
}

static void free_auth(void *cookie)
{
    auth_session *sess = cookie;
    struct auth_handler *hdl, *next;

#ifdef HAVE_GSSAPI
    if (sess->gssname != GSS_C_NO_NAME) {
        unsigned int major;
        gss_release_name(&major, &sess->gssname);
    }
#endif

    for (hdl = sess->handlers; hdl; hdl = next) {
        next = hdl->next;
        ne_free(hdl);
    }

    clean_session(sess);
    ne_free(sess);
}

static void auth_register(ne_session *sess, int isproxy, unsigned protomask,
			  const struct auth_class *ahc, const char *id, 
			  ne_auth_creds creds, void *userdata) 
{
    auth_session *ahs;
    struct auth_handler **hdl;

    /* Handle the _ALL and _DEFAULT protocol masks: */
    if (protomask == NE_AUTH_ALL) {
        protomask |= NE_AUTH_BASIC | NE_AUTH_DIGEST | NE_AUTH_NEGOTIATE;
    }
    else if (protomask == NE_AUTH_DEFAULT) {
        protomask |= NE_AUTH_BASIC | NE_AUTH_DIGEST;
        
        if (strcmp(ne_get_scheme(sess), "https") == 0 || isproxy) {
            protomask |= NE_AUTH_NEGOTIATE;
        }
    }

    if ((protomask & NE_AUTH_NEGOTIATE) == NE_AUTH_NEGOTIATE) {
        /* Map NEGOTIATE to NTLM | GSSAPI. */
        protomask |= NE_AUTH_GSSAPI | NE_AUTH_NTLM;
    }

    ahs = ne_get_session_private(sess, id);
    if (ahs == NULL) {
        ahs = ne_calloc(sizeof *ahs);
        
        ahs->sess = sess;
        ahs->spec = ahc;
        
        if (strcmp(ne_get_scheme(sess), "https") == 0) {
            ahs->context = isproxy ? AUTH_CONNECT : AUTH_NOTCONNECT;
        } else {
            ahs->context = AUTH_ANY;
        }
        
        /* Register hooks */
        ne_hook_create_request(sess, ah_create, ahs);
        ne_hook_pre_send(sess, ah_pre_send, ahs);
        ne_hook_post_send(sess, ah_post_send, ahs);
        ne_hook_destroy_request(sess, ah_destroy, ahs);
        ne_hook_destroy_session(sess, free_auth, ahs);
        
        ne_set_session_private(sess, id, ahs);
    }

#ifdef HAVE_GSSAPI
    if ((protomask & NE_AUTH_GSSAPI) && ahs->gssname == GSS_C_NO_NAME) {
        ne_uri uri = {0};
        
        if (isproxy)
            ne_fill_proxy_uri(sess, &uri);
        else
            ne_fill_server_uri(sess, &uri);

        get_gss_name(&ahs->gssname, uri.host);

        ne_uri_free(&uri);
    }
#endif

    /* Find the end of the handler list, and add a new one. */
    hdl = &ahs->handlers;
    while (*hdl)
        hdl = &(*hdl)->next;
        
    *hdl = ne_malloc(sizeof **hdl);
    (*hdl)->protomask = protomask;
    (*hdl)->creds = creds;
    (*hdl)->userdata = userdata;
    (*hdl)->next = NULL;
    (*hdl)->attempt = 0;
}

void ne_set_server_auth(ne_session *sess, ne_auth_creds creds, void *userdata)
{
    auth_register(sess, 0, NE_AUTH_DEFAULT, &ah_server_class, HOOK_SERVER_ID,
                  creds, userdata);
}

void ne_set_proxy_auth(ne_session *sess, ne_auth_creds creds, void *userdata)
{
    auth_register(sess, 1, NE_AUTH_DEFAULT, &ah_proxy_class, HOOK_PROXY_ID,
                  creds, userdata);
}

void ne_add_server_auth(ne_session *sess, unsigned protocol, 
                        ne_auth_creds creds, void *userdata)
{
    auth_register(sess, 0, protocol, &ah_server_class, HOOK_SERVER_ID,
                  creds, userdata);
}

void ne_add_proxy_auth(ne_session *sess, unsigned protocol, 
                       ne_auth_creds creds, void *userdata)
{
    auth_register(sess, 1, protocol, &ah_proxy_class, HOOK_PROXY_ID,
                  creds, userdata);
}

void ne_forget_auth(ne_session *sess)
{
    auth_session *as;
    if ((as = ne_get_session_private(sess, HOOK_SERVER_ID)) != NULL)
	clean_session(as);
    if ((as = ne_get_session_private(sess, HOOK_PROXY_ID)) != NULL)
	clean_session(as);
}

