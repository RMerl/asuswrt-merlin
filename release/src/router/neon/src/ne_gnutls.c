/*
   neon SSL/TLS support using GNU TLS
   Copyright (C) 2002-2010, Joe Orton <joe@manyfish.co.uk>
   Copyright (C) 2004, Aleix Conchillo Flaque <aleix@member.fsf.org>

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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <gnutls/gnutls.h>
#include <gnutls/pkcs12.h>

#ifdef NE_HAVE_TS_SSL
#include <errno.h>
#include <pthread.h>
#if LIBGNUTLS_VERSION_NUMBER < 0x020b01
#include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif
#else
#if LIBGNUTLS_VERSION_NUMBER < 0x020b01
#include <gcrypt.h>
#endif
#endif

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include "ne_ssl.h"
#include "ne_string.h"
#include "ne_session.h"
#include "ne_internal.h"

#include "ne_private.h"
#include "ne_privssl.h"

#if LIBGNUTLS_VERSION_NUMBER >= 0x020302
/* The GnuTLS DN functions in 2.3.2 and later allow a simpler DN
 * abstraction to be used. */
#define HAVE_NEW_DN_API
#endif

struct ne_ssl_dname_s {
#ifdef HAVE_NEW_DN_API
    gnutls_x509_dn_t dn;
#else
    int subject; /* non-zero if this is the subject DN object */
    gnutls_x509_crt cert;
#endif
};

struct ne_ssl_certificate_s {
    ne_ssl_dname subj_dn, issuer_dn;
    gnutls_x509_crt subject;
    ne_ssl_certificate *issuer;
    char *identity;
};

struct ne_ssl_client_cert_s {
    gnutls_pkcs12 p12;
    int decrypted; /* non-zero if successfully decrypted. */
    int keyless;
    ne_ssl_certificate cert;
    gnutls_x509_privkey pkey;
    char *friendly_name;
};

/* Returns the highest used index in subject (or issuer) DN of
 * certificate CERT for OID, or -1 if no RDNs are present in the DN
 * using that OID. */
static int oid_find_highest_index(gnutls_x509_crt cert, int subject, const char *oid)
{
    int ret, idx = -1;

    do {
        size_t len = 0;

        if (subject)
            ret = gnutls_x509_crt_get_dn_by_oid(cert, oid, ++idx, 0, 
                                                NULL, &len);
        else
            ret = gnutls_x509_crt_get_issuer_dn_by_oid(cert, oid, ++idx, 0, 
                                                       NULL, &len);
    } while (ret == GNUTLS_E_SHORT_MEMORY_BUFFER);
    
    return idx - 1;
}

#ifdef HAVE_GNUTLS_X509_DN_GET_RDN_AVA
/* New-style RDN handling introduced in GnuTLS 1.7.x. */

#ifdef HAVE_ICONV
static void convert_dirstring(ne_buffer *buf, const char *charset, 
                              gnutls_datum *data)
{
    iconv_t id = iconv_open("UTF-8", charset);
    size_t inlen = data->size, outlen = buf->length - buf->used;
    char *inbuf = (char *)data->data;
    char *outbuf = buf->data + buf->used - 1;
    
    if (id == (iconv_t)-1) {
        char err[128], err2[128];

        ne_snprintf(err, sizeof err, "[unprintable in %s: %s]",
                    charset, ne_strerror(errno, err2, sizeof err2));
        ne_buffer_zappend(buf, err);
        return;
    }
    
    ne_buffer_grow(buf, buf->used + 64);
    
    while (inlen && outlen 
           && iconv(id, &inbuf, &inlen, &outbuf, &outlen) == 0)
        ;
    
    iconv_close(id);
    buf->used += buf->length - buf->used - outlen;
    buf->data[buf->used - 1] = '\0';
}
#endif

/* From section 11.13 of the Dubuisson ASN.1 bible: */
#define TAG_UTF8 (12)
#define TAG_PRINTABLE (19)
#define TAG_T61 (20)
#define TAG_IA5 (22)
#define TAG_VISIBLE (26)
#define TAG_UNIVERSAL (28)
#define TAG_BMP (30)

static void append_dirstring(ne_buffer *buf, gnutls_datum *data, unsigned long tag)
{
    switch (tag) {
    case TAG_UTF8:
    case TAG_IA5:
    case TAG_PRINTABLE:
    case TAG_VISIBLE:
        ne_buffer_append(buf, (char *)data->data, data->size);
        break;
#ifdef HAVE_ICONV
    case TAG_T61:
        convert_dirstring(buf, "ISO-8859-1", data);
        break;
    case TAG_BMP:
        convert_dirstring(buf, "UCS-2BE", data);
        break;
#endif
    default: {
        char tmp[128];
        ne_snprintf(tmp, sizeof tmp, _("[unprintable:#%lu]"), tag);
        ne_buffer_zappend(buf, tmp);
    } break;
    }
}

/* OIDs to not include in readable DNs by default: */
#define OID_emailAddress "1.2.840.113549.1.9.1"
#define OID_commonName "2.5.4.3"

#define CMPOID(a,o) ((a)->oid.size == sizeof(o)                        \
                     && memcmp((a)->oid.data, o, strlen(o)) == 0)

char *ne_ssl_readable_dname(const ne_ssl_dname *name)
{
    gnutls_x509_dn_t dn;
    int ret, rdn = 0, flag = 0;
    ne_buffer *buf;
    gnutls_x509_ava_st val;

#ifdef HAVE_NEW_DN_API
    dn = name->dn;
#else
    if (name->subject)
        ret = gnutls_x509_crt_get_subject(name->cert, &dn);
    else
        ret = gnutls_x509_crt_get_issuer(name->cert, &dn);
    
    if (ret)
        return ne_strdup(_("[unprintable]"));
#endif /* HAVE_NEW_DN_API */

    buf = ne_buffer_create();
    
    /* Find the highest rdn... */
    while (gnutls_x509_dn_get_rdn_ava(dn, rdn++, 0, &val) == 0)
        ;        

    /* ..then iterate back to the first: */
    while (--rdn >= 0) {
        int ava = 0;

        /* Iterate through all AVAs for multivalued AVAs; better than
         * ne_openssl can do! */
        do {
            ret = gnutls_x509_dn_get_rdn_ava(dn, rdn, ava, &val);

            /* If the *only* attribute to append is the common name or
             * email address, use it; otherwise skip those
             * attributes. */
            if (ret == 0 && val.value.size > 0
                && ((!CMPOID(&val, OID_emailAddress)
                     && !CMPOID(&val, OID_commonName))
                    || (buf->used == 1 && rdn == 0))) {
                flag = 1;
                if (buf->used > 1) ne_buffer_append(buf, ", ", 2);

                append_dirstring(buf, &val.value, val.value_tag);
            }
            
            ava++;
        } while (ret == 0);
    }

    return ne_buffer_finish(buf);
}

#else /* !HAVE_GNUTLS_X509_DN_GET_RDN_AVA */

/* Appends the value of RDN with given oid from certitifcate x5
 * subject (if subject is non-zero), or issuer DN to buffer 'buf': */
static void append_rdn(ne_buffer *buf, gnutls_x509_crt x5, int subject, const char *oid)
{
    int idx, top, ret;
    char rdn[50];

    top = oid_find_highest_index(x5, subject, oid);
    
    for (idx = top; idx >= 0; idx--) {
        size_t rdnlen = sizeof rdn;

        if (subject)
            ret = gnutls_x509_crt_get_dn_by_oid(x5, oid, idx, 0, rdn, &rdnlen);
        else
            ret = gnutls_x509_crt_get_issuer_dn_by_oid(x5, oid, idx, 0, rdn, &rdnlen);
        
        if (ret < 0)
            return;

        if (buf->used > 1) {
            ne_buffer_append(buf, ", ", 2);
        }
        
        ne_buffer_append(buf, rdn, rdnlen);
    }
}

char *ne_ssl_readable_dname(const ne_ssl_dname *name)
{
    ne_buffer *buf = ne_buffer_create();
    int ret, idx = 0;

    do {
        char oid[32] = {0};
        size_t oidlen = sizeof oid;
        
        ret = name->subject 
            ? gnutls_x509_crt_get_dn_oid(name->cert, idx, oid, &oidlen)
            : gnutls_x509_crt_get_issuer_dn_oid(name->cert, idx, oid, &oidlen);
        
        if (ret == 0) {
            append_rdn(buf, name->cert, name->subject, oid);
            idx++;
        }
    } while (ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

    return ne_buffer_finish(buf);
}
#endif /* HAVE_GNUTLS_X509_DN_GET_RDN_AVA */

int ne_ssl_dname_cmp(const ne_ssl_dname *dn1, const ne_ssl_dname *dn2)
{
    char c1[1024], c2[1024];
    size_t s1 = sizeof c1, s2 = sizeof c2;

#ifdef HAVE_NEW_DN_API
    if (gnutls_x509_dn_export(dn1->dn, GNUTLS_X509_FMT_DER, c1, &s1))
        return 1;
        
    if (gnutls_x509_dn_export(dn2->dn, GNUTLS_X509_FMT_DER, c2, &s2))
        return -1;
#else
    int ret;

    if (dn1->subject)
        ret = gnutls_x509_crt_get_dn(dn1->cert, c1, &s1);
    else
        ret = gnutls_x509_crt_get_issuer_dn(dn1->cert, c1, &s1);
    if (ret)
        return 1;

    if (dn2->subject)
        ret = gnutls_x509_crt_get_dn(dn2->cert, c2, &s2);
    else
        ret = gnutls_x509_crt_get_issuer_dn(dn2->cert, c2, &s2);
    if (ret)
        return -1;
#endif /* HAVE_NEW_DN_API */
    
    if (s1 != s2)
        return s2 - s1;

    return memcmp(c1, c2, s1);
}

void ne_ssl_clicert_free(ne_ssl_client_cert *cc)
{
    if (cc->p12)
        gnutls_pkcs12_deinit(cc->p12);
    if (cc->decrypted) {
        if (cc->cert.identity) ne_free(cc->cert.identity);
        if (cc->pkey) gnutls_x509_privkey_deinit(cc->pkey);
        if (cc->cert.subject) gnutls_x509_crt_deinit(cc->cert.subject);
    }
    if (cc->friendly_name) ne_free(cc->friendly_name);
    ne_free(cc);
}

void ne_ssl_cert_validity_time(const ne_ssl_certificate *cert,
                               time_t *from, time_t *until)
{
    if (from) {
        *from = gnutls_x509_crt_get_activation_time(cert->subject);
    }
    if (until) {
        *until = gnutls_x509_crt_get_expiration_time(cert->subject);
    }
}

/* Check certificate identity.  Returns zero if identity matches; 1 if
 * identity does not match, or <0 if the certificate had no identity.
 * If 'identity' is non-NULL, store the malloc-allocated identity in
 * *identity.  If 'server' is non-NULL, it must be the network address
 * of the server in use, and identity must be NULL. */
static int check_identity(const ne_uri *server, gnutls_x509_crt cert,
                          char **identity)
{
    char name[255];
    unsigned int critical;
    int ret, seq = 0;
    int match = 0, found = 0;
    size_t len;
    const char *hostname;
    
    hostname = server ? server->host : "";

    do {
        len = sizeof name - 1;
        ret = gnutls_x509_crt_get_subject_alt_name(cert, seq, name, &len,
                                                   &critical);
        switch (ret) {
        case GNUTLS_SAN_DNSNAME:
            name[len] = '\0';
            if (identity && !found) *identity = ne_strdup(name);
            match = ne__ssl_match_hostname(name, len, hostname);
            found = 1;
            break;
        case GNUTLS_SAN_IPADDRESS: {
            ne_inet_addr *ia;
            if (len == 4)
                ia = ne_iaddr_make(ne_iaddr_ipv4, (unsigned char *)name);
            else if (len == 16)
                ia = ne_iaddr_make(ne_iaddr_ipv6, (unsigned char *)name);
            else 
                ia = NULL;
            if (ia) {
                char buf[128];
                
                match = strcmp(hostname, 
                               ne_iaddr_print(ia, buf, sizeof buf)) == 0;
                if (identity) *identity = ne_strdup(buf);
                found = 1;
                ne_iaddr_free(ia);
            } else {
                NE_DEBUG(NE_DBG_SSL, "iPAddress name with unsupported "
                         "address type (length %" NE_FMT_SIZE_T "), skipped.\n",
                         len);
            }
        } break;
        case GNUTLS_SAN_URI: {
            ne_uri uri;
            
            name[len] = '\0';
            
            if (ne_uri_parse(name, &uri) == 0 && uri.host && uri.scheme) {
                ne_uri tmp;
                
                if (identity && !found) *identity = ne_strdup(name);
                found = 1;
                
                if (server) {
                    /* For comparison purposes, all that matters is
                     * host, scheme and port; ignore the rest. */
                    memset(&tmp, 0, sizeof tmp);
                    tmp.host = uri.host;
                    tmp.scheme = uri.scheme;
                    tmp.port = uri.port;
                    
                    match = ne_uri_cmp(server, &tmp) == 0;
                }
            }
            
            ne_uri_free(&uri);
        } break;

        default:
            break;
        }
        seq++;
    } while (!match && ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE);

    /* Check against the commonName if no DNS alt. names were found,
     * as per RFC3280. */
    if (!found) {
        seq = oid_find_highest_index(cert, 1, GNUTLS_OID_X520_COMMON_NAME);

        if (seq >= 0) {
            len = sizeof name;
            name[0] = '\0';
            ret = gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME,
                                                seq, 0, name, &len);
            if (ret == 0) {
                if (identity) *identity = ne_strdup(name);
                match = ne__ssl_match_hostname(name, len, hostname);
            }
        } else {
            return -1;
        }
    }

    if (*hostname)
        NE_DEBUG(NE_DBG_SSL, "ssl: Identity match for '%s': %s\n", hostname, 
                 match ? "good" : "bad");

    return match ? 0 : 1;
}

/* Populate an ne_ssl_certificate structure from an X509 object.  Note
 * that x5 is owned by returned cert object and must not be otherwise
 * freed by the caller.  */
static ne_ssl_certificate *populate_cert(ne_ssl_certificate *cert,
                                         gnutls_x509_crt x5)
{
#ifdef HAVE_NEW_DN_API
    gnutls_x509_crt_get_subject(x5, &cert->subj_dn.dn);
    gnutls_x509_crt_get_issuer(x5, &cert->issuer_dn.dn);
#else
    cert->subj_dn.cert = x5;
    cert->subj_dn.subject = 1;
    cert->issuer_dn.cert = x5;
    cert->issuer_dn.subject = 0;
#endif
    cert->issuer = NULL;
    cert->subject = x5;
    cert->identity = NULL;
    check_identity(NULL, x5, &cert->identity);
    return cert;
}

/* Returns a copy certificate of certificate SRC. */
static gnutls_x509_crt x509_crt_copy(gnutls_x509_crt src)
{
    int ret;
    size_t size;
    gnutls_datum tmp;
    gnutls_x509_crt dest;
    
    if (gnutls_x509_crt_init(&dest) != 0) {
        return NULL;
    }

    if (gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, NULL, &size) 
        != GNUTLS_E_SHORT_MEMORY_BUFFER) {
        gnutls_x509_crt_deinit(dest);
        return NULL;
    }

    tmp.data = ne_malloc(size);
    ret = gnutls_x509_crt_export(src, GNUTLS_X509_FMT_DER, tmp.data, &size);
    if (ret == 0) {
        tmp.size = size;
        ret = gnutls_x509_crt_import(dest, &tmp, GNUTLS_X509_FMT_DER);
    }

    if (ret) {
        gnutls_x509_crt_deinit(dest);
        dest = NULL;
    }

    ne_free(tmp.data);
    return dest;
}

/* Duplicate a client certificate, which must be in the decrypted state. */
static ne_ssl_client_cert *dup_client_cert(const ne_ssl_client_cert *cc)
{
    int ret;
    ne_ssl_client_cert *newcc = ne_calloc(sizeof *newcc);

    newcc->decrypted = 1;
    
    if (cc->keyless) {
        newcc->keyless = 1;
    }
    else {
        ret = gnutls_x509_privkey_init(&newcc->pkey);
        if (ret != 0) goto dup_error;
        
        ret = gnutls_x509_privkey_cpy(newcc->pkey, cc->pkey);
        if (ret != 0) goto dup_error;
    }    

    newcc->cert.subject = x509_crt_copy(cc->cert.subject);
    if (!newcc->cert.subject) goto dup_error;

    if (cc->friendly_name) newcc->friendly_name = ne_strdup(cc->friendly_name);

    populate_cert(&newcc->cert, newcc->cert.subject);
    return newcc;

dup_error:
    if (newcc->pkey) gnutls_x509_privkey_deinit(newcc->pkey);
    if (newcc->cert.subject) gnutls_x509_crt_deinit(newcc->cert.subject);
    ne_free(newcc);
    return NULL;
}    

/* Callback invoked when the SSL server requests a client certificate.  */
static int provide_client_cert(gnutls_session session,
                               const gnutls_datum *req_ca_rdn, int nreqs,
                               const gnutls_pk_algorithm *sign_algos,
                               int sign_algos_length, gnutls_retr_st *st)
{
    ne_session *sess = gnutls_session_get_ptr(session);
    
    if (!sess) {
        return GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER;
    }

    NE_DEBUG(NE_DBG_SSL, "ssl: Client cert provider callback; %d CA names.\n",
             nreqs);

    if (!sess->client_cert && sess->ssl_provide_fn) {
#ifdef HAVE_NEW_DN_API
        const ne_ssl_dname **dns;
        ne_ssl_dname *dnarray;
        unsigned dncount = 0;
        int n;

        dns = ne_malloc(nreqs * sizeof(ne_ssl_dname *));
        dnarray = ne_calloc(nreqs * sizeof(ne_ssl_dname));

        for (n = 0; n < nreqs; n++) {
            gnutls_x509_dn_t dn;

            if (gnutls_x509_dn_init(&dn) == 0) {
                dnarray[n].dn = dn;
                if (gnutls_x509_dn_import(dn, &req_ca_rdn[n]) == 0) {
                    dns[dncount++] = &dnarray[n];
                }
                else {
                    gnutls_x509_dn_deinit(dn);
                }            
            }
        }
       
        NE_DEBUG(NE_DBG_SSL, "ssl: Mapped %d CA names to %u DN objects.\n",
                 nreqs, dncount);

        sess->ssl_provide_fn(sess->ssl_provide_ud, sess, dns, dncount);
        
        for (n = 0; n < nreqs; n++) {
            if (dnarray[n].dn) {
                gnutls_x509_dn_deinit(dnarray[n].dn);
            }
        }

        ne_free(dns);
        ne_free(dnarray);
#else /* HAVE_NEW_DN_API */
        /* Nothing to do here other than pretend no CA names were
         * given, and hope the caller can cope. */
        sess->ssl_provide_fn(sess->ssl_provide_ud, sess, NULL, 0);
#endif
    }

    if (sess->client_cert) {
        gnutls_certificate_type type = gnutls_certificate_type_get(session);
        if (type == GNUTLS_CRT_X509) {
            NE_DEBUG(NE_DBG_SSL, "Supplying client certificate.\n");

            st->type = type;
            st->ncerts = 1;
            st->cert.x509 = &sess->client_cert->cert.subject;
            st->key.x509 = sess->client_cert->pkey;
            
            /* tell GNU TLS not to deallocate the certs. */
            st->deinit_all = 0;
        } else {
            return GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE;
        }
    } 
    else {
        NE_DEBUG(NE_DBG_SSL, "No client certificate supplied.\n");
        st->ncerts = 0;
        sess->ssl_cc_requested = 1;
        return 0;
    }

    return 0;
}

void ne_ssl_set_clicert(ne_session *sess, const ne_ssl_client_cert *cc)
{
    sess->client_cert = dup_client_cert(cc);
}

ne_ssl_context *ne_ssl_context_create(int flags)
{
    ne_ssl_context *ctx = ne_calloc(sizeof *ctx);
    gnutls_certificate_allocate_credentials(&ctx->cred);
    if (flags == NE_SSL_CTX_CLIENT) {
        gnutls_certificate_client_set_retrieve_function(ctx->cred,
                                                        provide_client_cert);
    }
    gnutls_certificate_set_verify_flags(ctx->cred, 
                                        GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);
    return ctx;
}

int ne_ssl_context_keypair(ne_ssl_context *ctx, 
                           const char *cert, const char *key)
{
    gnutls_certificate_set_x509_key_file(ctx->cred, cert, key,
                                         GNUTLS_X509_FMT_PEM);
    return 0;
}

int ne_ssl_context_set_verify(ne_ssl_context *ctx, int required,
                              const char *ca_names, const char *verify_cas)
{
    ctx->verify = required;
    if (verify_cas) {
        gnutls_certificate_set_x509_trust_file(ctx->cred, verify_cas,
                                               GNUTLS_X509_FMT_PEM);
    }
    /* gnutls_certificate_send_x509_rdn_sequence in gnutls >= 1.2 can
     * be used to *suppress* sending the CA names, but not control it,
     * it seems. */
    return 0;
}

void ne_ssl_context_set_flag(ne_ssl_context *ctx, int flag, int value)
{
    /* SSLv2 not supported. */
}

void ne_ssl_context_destroy(ne_ssl_context *ctx)
{
    gnutls_certificate_free_credentials(ctx->cred);
    if (ctx->cache.client.data) {
        ne_free(ctx->cache.client.data);
    } else if (ctx->cache.server.key.data) {
        gnutls_free(ctx->cache.server.key.data);
        gnutls_free(ctx->cache.server.data.data);
    }    
    ne_free(ctx);
}

#ifdef HAVE_GNUTLS_CERTIFICATE_GET_X509_CAS
/* Return the issuer of the given certificate, or NULL if none can be
 * found. */
static gnutls_x509_crt find_issuer(gnutls_x509_crt *ca_list,
                                   unsigned int num_cas,
                                   gnutls_x509_crt cert)
{
    unsigned int n;

    for (n = 0; n < num_cas; n++) {
        if (gnutls_x509_crt_check_issuer(cert, ca_list[n]) == 1)
            return ca_list[n];
    }

    return NULL;
}
#endif

/* Return the certificate chain sent by the peer, or NULL on error. */
static ne_ssl_certificate *make_peers_chain(gnutls_session sock,
                                            gnutls_certificate_credentials crd)
{
    ne_ssl_certificate *current = NULL, *top = NULL;
    const gnutls_datum *certs;
    unsigned int n, count;
    ne_ssl_certificate *cert;

    certs = gnutls_certificate_get_peers(sock, &count);
    if (!certs) {
        return NULL;
    }

    NE_DEBUG(NE_DBG_SSL, "ssl: Got %u certs in peer chain.\n", count);
    
    for (n = 0; n < count; n++) {
        gnutls_x509_crt x5;

        if (gnutls_x509_crt_init(&x5) ||
            gnutls_x509_crt_import(x5, &certs[n], GNUTLS_X509_FMT_DER)) {
            if (top) {
                ne_ssl_cert_free(top);
            }
            return NULL;
        }

        cert = populate_cert(ne_calloc(sizeof *cert), x5);
        
        if (top == NULL) {
            current = top = cert;
        } else {
            current->issuer = cert;
            current = cert;
        }
    }

#ifdef HAVE_GNUTLS_CERTIFICATE_GET_X509_CAS
    /* GnuTLS only returns the peers which were *sent* by the server
     * in the Certificate list during the handshake.  Fill in the
     * complete chain manually against the certs we trust: */
    if (current->issuer == NULL) {
        gnutls_x509_crt issuer;
        gnutls_x509_crt *ca_list;
        unsigned int num_cas;
        
        gnutls_certificate_get_x509_cas(crd, &ca_list, &num_cas);

        do { 
            /* Look up the issuer. */
            issuer = find_issuer(ca_list, num_cas, current->subject);
            if (issuer) {
                issuer = x509_crt_copy(issuer);
                cert = populate_cert(ne_calloc(sizeof *cert), issuer);
                /* Check that the issuer does not match the current
                 * cert. */
                if (ne_ssl_cert_cmp(current, cert)) {
                    current = current->issuer = cert;
                }
                else {
                    ne_ssl_cert_free(cert);
                    issuer = NULL;
                }
            }
        } while (issuer);
    }
#endif
    
    return top;
}

/* Map from GnuTLS verify failure mask *status to NE_SSL_* failure
 * bitmask, which is returned.  *status is modified, removing all
 * mapped bits. */
static int map_verify_failures(unsigned int *status)
{
    static const struct {
        gnutls_certificate_status_t from;
        int to;
    } map[] = {
        { GNUTLS_CERT_REVOKED, NE_SSL_REVOKED },
#if LIBGNUTLS_VERSION_NUMBER >= 0x020800
        { GNUTLS_CERT_NOT_ACTIVATED, NE_SSL_NOTYETVALID },
        { GNUTLS_CERT_EXPIRED, NE_SSL_EXPIRED },
#endif
        { GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND, NE_SSL_UNTRUSTED },
        { GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_CA, NE_SSL_UNTRUSTED }
    };
    size_t n;
    int ret = 0;

    for (n = 0; n < sizeof(map)/sizeof(map[0]); n++) {
        if ((*status & map[n].from) == map[n].from) {
            *status &= ~map[n].from;
            ret |= map[n].to;
        }
    }

    return ret;
}

/* Return a malloc-allocated human-readable error string describing
 * GnuTLS verification error bitmask 'status'; return value must be
 * freed by the caller. */
static char *verify_error_string(unsigned int status)
{
    ne_buffer *buf = ne_buffer_create();

    /* sorry, i18n-ers */
    if (status & GNUTLS_CERT_INSECURE_ALGORITHM) {
        ne_buffer_zappend(buf, _("signed using insecure algorithm"));
    }
    else {
        ne_buffer_snprintf(buf, 64, _("unrecognized errors (%u)"),
                           status);
    }
    
    return ne_buffer_finish(buf);
}

/* Return NE_SSL_* failure bits after checking chain expiry. */
static int check_chain_expiry(ne_ssl_certificate *chain)
{
    time_t before, after, now = time(NULL);
    ne_ssl_certificate *cert;
    int failures = 0;
    
    /* Check that all certs within the chain are inside their defined
     * validity period.  Note that the errors flagged for the server
     * cert are different from the generic error for issues higher up
     * the chain. */
    for (cert = chain; cert; cert = cert->issuer) {
        before = gnutls_x509_crt_get_activation_time(cert->subject);
        after = gnutls_x509_crt_get_expiration_time(cert->subject);
        
        if (now < before)
            failures |= (cert == chain) ? NE_SSL_NOTYETVALID : NE_SSL_BADCHAIN;
        else if (now > after)
            failures |= (cert == chain) ? NE_SSL_EXPIRED : NE_SSL_BADCHAIN;
    }

    return failures;
}

/* Verifies an SSL server certificate. */
static int check_certificate(ne_session *sess, gnutls_session sock,
                             ne_ssl_certificate *chain)
{
    int ret, failures = 0;
    ne_uri server;
    unsigned int status;

    memset(&server, 0, sizeof server);
    ne_fill_server_uri(sess, &server);
    ret = check_identity(&server, chain->subject, NULL);
    ne_uri_free(&server);

    if (ret < 0) {
        ne_set_error(sess, _("Server certificate was missing commonName "
                             "attribute in subject name"));
        return NE_ERROR;
    } 
    else if (ret > 0) {
        failures |= NE_SSL_IDMISMATCH;
    }
    
    failures |= check_chain_expiry(chain);

    ret = gnutls_certificate_verify_peers2(sock, &status);
    NE_DEBUG(NE_DBG_SSL, "ssl: Verify peers returned %d, status=%u\n", 
             ret, status);
    if (ret != GNUTLS_E_SUCCESS) {
        ne_set_error(sess, _("Could not verify server certificate: %s"),
                     gnutls_strerror(ret));
        return NE_ERROR;
    }

    failures |= map_verify_failures(&status);

    NE_DEBUG(NE_DBG_SSL, "ssl: Verification failures = %d (status = %u).\n", 
             failures, status);
    
    if (status && status != GNUTLS_CERT_INVALID) {
        char *errstr = verify_error_string(status);
        ne_set_error(sess, _("Certificate verification error: %s"), errstr);
        ne_free(errstr);       
        return NE_ERROR;
    }

    if (failures == 0) {
        ret = NE_OK;
    } else {
        ne__ssl_set_verify_err(sess, failures);
        ret = NE_ERROR;
        if (sess->ssl_verify_fn
            && sess->ssl_verify_fn(sess->ssl_verify_ud, failures, chain) == 0)
            ret = NE_OK;
    }

    return ret;
}

/* Negotiate an SSL connection. */
int ne__negotiate_ssl(ne_session *sess)
{
    ne_ssl_context *const ctx = sess->ssl_context;
    ne_ssl_certificate *chain;
    gnutls_session sock;

    NE_DEBUG(NE_DBG_SSL, "Negotiating SSL connection.\n");

    /* Pass through the hostname if SNI is enabled. */
    ctx->hostname = 
        sess->flags[NE_SESSFLAG_TLS_SNI] ? sess->server.hostname : NULL;

    if (ne_sock_connect_ssl(sess->socket, ctx, sess)) {
        if (sess->ssl_cc_requested) {
            ne_set_error(sess, _("SSL handshake failed, "
                                 "client certificate was requested: %s"),
                         ne_sock_error(sess->socket));
        }
        else {
            ne_set_error(sess, _("SSL handshake failed: %s"),
                         ne_sock_error(sess->socket));
        }
        return NE_ERROR;
    }

    sock = ne__sock_sslsock(sess->socket);

    chain = make_peers_chain(sock, ctx->cred);
    if (chain == NULL) {
        ne_set_error(sess, _("Server did not send certificate chain"));
        return NE_ERROR;
    }

    if (sess->server_cert && ne_ssl_cert_cmp(sess->server_cert, chain) == 0) {
        /* Same cert as last time; presume OK.  This is not optimal as
         * make_peers_chain() has already gone through and done the
         * expensive DER parsing stuff for the whole chain by now. */
        ne_ssl_cert_free(chain);
        return NE_OK;
    }

    if (check_certificate(sess, sock, chain)) {
        ne_ssl_cert_free(chain);
        return NE_ERROR;
    }

    sess->server_cert = chain;

    return NE_OK;
}

const ne_ssl_dname *ne_ssl_cert_issuer(const ne_ssl_certificate *cert)
{
    return &cert->issuer_dn;
}

const ne_ssl_dname *ne_ssl_cert_subject(const ne_ssl_certificate *cert)
{
    return &cert->subj_dn;
}

const ne_ssl_certificate *ne_ssl_cert_signedby(const ne_ssl_certificate *cert)
{
    return cert->issuer;
}

const char *ne_ssl_cert_identity(const ne_ssl_certificate *cert)
{
    return cert->identity;
}

void ne_ssl_context_trustcert(ne_ssl_context *ctx, const ne_ssl_certificate *cert)
{
    gnutls_x509_crt certs = cert->subject;
    gnutls_certificate_set_x509_trust(ctx->cred, &certs, 1);
}

void ne_ssl_trust_default_ca(ne_session *sess)
{
#ifdef NE_SSL_CA_BUNDLE
    gnutls_certificate_set_x509_trust_file(sess->ssl_context->cred,
                                           NE_SSL_CA_BUNDLE,
                                           GNUTLS_X509_FMT_PEM);
#endif
}

/* Read the contents of file FILENAME into *DATUM. */
static int read_to_datum(const char *filename, gnutls_datum *datum)
{
    FILE *f = fopen(filename, "r");
    ne_buffer *buf;
    char tmp[4192];
    size_t len;

    if (!f) {
        return -1;
    }

    buf = ne_buffer_ncreate(8192);
    while ((len = fread(tmp, 1, sizeof tmp, f)) > 0) {
        ne_buffer_append(buf, tmp, len);
    }

    if (!feof(f)) {
        fclose(f);
        ne_buffer_destroy(buf);
        return -1;
    }
    
    fclose(f);

    datum->size = ne_buffer_size(buf);
    datum->data = (unsigned char *)ne_buffer_finish(buf);
    return 0;
}

/* Parses a PKCS#12 structure and loads the certificate, private key
 * and friendly name if possible.  Returns zero on success, non-zero
 * on error. */
static int pkcs12_parse(gnutls_pkcs12 p12, gnutls_x509_privkey *pkey,
                        gnutls_x509_crt *x5, char **friendly_name,
                        const char *password)
{
    gnutls_pkcs12_bag bag = NULL;
    int i, j, ret = 0;

    for (i = 0; ret == 0; ++i) {
        if (bag) gnutls_pkcs12_bag_deinit(bag);

        ret = gnutls_pkcs12_bag_init(&bag);
        if (ret < 0) continue;

        ret = gnutls_pkcs12_get_bag(p12, i, bag);
        if (ret < 0) continue;

        gnutls_pkcs12_bag_decrypt(bag, password);

        for (j = 0; ret == 0 && j < gnutls_pkcs12_bag_get_count(bag); ++j) {
            gnutls_pkcs12_bag_type type;
            gnutls_datum data;

            if (friendly_name && *friendly_name == NULL) {
                char *name = NULL;
                gnutls_pkcs12_bag_get_friendly_name(bag, j, &name);
                if (name) {
                    if (name[0] == '.') name++; /* weird GnuTLS bug? */
                    *friendly_name = ne_strdup(name);
                }
            }

            type = gnutls_pkcs12_bag_get_type(bag, j);
            switch (type) {
            case GNUTLS_BAG_PKCS8_KEY:
            case GNUTLS_BAG_PKCS8_ENCRYPTED_KEY:
                /* Ignore any but the first key encountered; really
                 * need to match up keyids. */
                if (*pkey) break;

                gnutls_x509_privkey_init(pkey);

                ret = gnutls_pkcs12_bag_get_data(bag, j, &data);
                if (ret < 0) continue;

                ret = gnutls_x509_privkey_import_pkcs8(*pkey, &data,
                                                       GNUTLS_X509_FMT_DER,
                                                       password,
                                                       0);
                if (ret < 0) continue;
                break;
            case GNUTLS_BAG_CERTIFICATE:
                /* Ignore any but the first cert encountered; again,
                 * really need to match up keyids. */
                if (*x5) break;

                ret = gnutls_x509_crt_init(x5);
                if (ret < 0) continue;

                ret = gnutls_pkcs12_bag_get_data(bag, j, &data);
                if (ret < 0) continue;

                ret = gnutls_x509_crt_import(*x5, &data, GNUTLS_X509_FMT_DER);
                if (ret < 0) continue;

                break;
            default:
                break;
            }
        }
    }

    /* Make sure last bag is freed */
    if (bag) gnutls_pkcs12_bag_deinit(bag);

    /* Free in case of error */
    if (ret < 0 && ret != GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) {
        if (*x5) gnutls_x509_crt_deinit(*x5);
        if (*pkey) gnutls_x509_privkey_deinit(*pkey);
        if (friendly_name && *friendly_name) ne_free(*friendly_name);
    }

    if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE) ret = 0;
    return ret;
}

ne_ssl_client_cert *ne_ssl_clicert_read(const char *filename)
{
    int ret;
    gnutls_datum data;
    gnutls_pkcs12 p12;
    ne_ssl_client_cert *cc;
    char *friendly_name = NULL;
    gnutls_x509_crt cert = NULL;
    gnutls_x509_privkey pkey = NULL;

    if (read_to_datum(filename, &data))
        return NULL;

    if (gnutls_pkcs12_init(&p12) != 0) {
        return NULL;
    }

    ret = gnutls_pkcs12_import(p12, &data, GNUTLS_X509_FMT_DER, 0);
    ne_free(data.data);
    if (ret < 0) {
        gnutls_pkcs12_deinit(p12);
        return NULL;
    }

    if (gnutls_pkcs12_verify_mac(p12, "") == 0) {
        if (pkcs12_parse(p12, &pkey, &cert, &friendly_name, "") != 0
            || !cert || !pkey) {
            gnutls_pkcs12_deinit(p12);
            return NULL;
        }

        cc = ne_calloc(sizeof *cc);
        cc->pkey = pkey;
        cc->decrypted = 1;
        cc->friendly_name = friendly_name;
        populate_cert(&cc->cert, cert);
        gnutls_pkcs12_deinit(p12);
        cc->p12 = NULL;
        return cc;
    } else {
        /* TODO: calling pkcs12_parse() here to find the friendly_name
         * seems to break horribly.  */
        cc = ne_calloc(sizeof *cc);
        cc->p12 = p12;
        return cc;
    }
}

ne_ssl_client_cert *ne__ssl_clicert_exkey_import(const unsigned char *der,
                                                 size_t der_len)
{
    ne_ssl_client_cert *cc;
    gnutls_x509_crt x5;
    gnutls_datum datum;

    datum.data = (unsigned char *)der;
    datum.size = der_len;    

    if (gnutls_x509_crt_init(&x5) 
        || gnutls_x509_crt_import(x5, &datum, GNUTLS_X509_FMT_DER)) {
        NE_DEBUG(NE_DBG_SSL, "ssl: crt_import failed.\n");
        return NULL;
    }
    
    cc = ne_calloc(sizeof *cc);
    cc->keyless = 1;
    cc->decrypted = 1;
    populate_cert(&cc->cert, x5);

    return cc;    
}

int ne_ssl_clicert_encrypted(const ne_ssl_client_cert *cc)
{
    return !cc->decrypted;
}

int ne_ssl_clicert_decrypt(ne_ssl_client_cert *cc, const char *password)
{
    int ret;
    gnutls_x509_crt cert = NULL;
    gnutls_x509_privkey pkey = NULL;

    if (gnutls_pkcs12_verify_mac(cc->p12, password) != 0) {
        return -1;
    }        

    ret = pkcs12_parse(cc->p12, &pkey, &cert, NULL, password);
    if (ret < 0)
        return ret;
    
    if (!cert || (!pkey && !cc->keyless)) {
        if (cert) gnutls_x509_crt_deinit(cert);
        if (pkey) gnutls_x509_privkey_deinit(pkey);
        return -1;
    }

    gnutls_pkcs12_deinit(cc->p12);
    populate_cert(&cc->cert, cert);
    cc->pkey = pkey;
    cc->decrypted = 1;
    cc->p12 = NULL;
    return 0;
}

const ne_ssl_certificate *ne_ssl_clicert_owner(const ne_ssl_client_cert *cc)
{
    return &cc->cert;
}

const char *ne_ssl_clicert_name(const ne_ssl_client_cert *ccert)
{
    return ccert->friendly_name;
}

ne_ssl_certificate *ne_ssl_cert_read(const char *filename)
{
    int ret;
    gnutls_datum data;
    gnutls_x509_crt x5;

    if (read_to_datum(filename, &data))
        return NULL;

    if (gnutls_x509_crt_init(&x5) != 0)
        return NULL;

    ret = gnutls_x509_crt_import(x5, &data, GNUTLS_X509_FMT_PEM);
    ne_free(data.data);
    if (ret < 0) {
        gnutls_x509_crt_deinit(x5);
        return NULL;
    }
    
    return populate_cert(ne_calloc(sizeof(struct ne_ssl_certificate_s)), x5);
}

int ne_ssl_cert_write(const ne_ssl_certificate *cert, const char *filename)
{
    unsigned char buffer[10*1024];
    size_t len = sizeof buffer;

    FILE *fp = fopen(filename, "w");

    if (fp == NULL) return -1;

    if (gnutls_x509_crt_export(cert->subject, GNUTLS_X509_FMT_PEM, buffer,
                               &len) < 0) {
        fclose(fp);
        return -1;
    }

    if (fwrite(buffer, len, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0)
        return -1;

    return 0;
}

void ne_ssl_cert_free(ne_ssl_certificate *cert)
{
    gnutls_x509_crt_deinit(cert->subject);
    if (cert->identity) ne_free(cert->identity);
    if (cert->issuer) ne_ssl_cert_free(cert->issuer);
    ne_free(cert);
}

int ne_ssl_cert_cmp(const ne_ssl_certificate *c1, const ne_ssl_certificate *c2)
{
    char digest1[NE_SSL_DIGESTLEN], digest2[NE_SSL_DIGESTLEN];

    if (ne_ssl_cert_digest(c1, digest1) || ne_ssl_cert_digest(c2, digest2)) {
        return -1;
    }

    return strcmp(digest1, digest2);
}

/* The certificate import/export format is the base64 encoding of the
 * raw DER; PEM without the newlines and wrapping. */

ne_ssl_certificate *ne_ssl_cert_import(const char *data)
{
    int ret;
    size_t len;
    unsigned char *der;
    gnutls_datum buffer = { NULL, 0 };
    gnutls_x509_crt x5;

    if (gnutls_x509_crt_init(&x5) != 0)
        return NULL;

    /* decode the base64 to get the raw DER representation */
    len = ne_unbase64(data, &der);
    if (len == 0) return NULL;

    buffer.data = der;
    buffer.size = len;

    ret = gnutls_x509_crt_import(x5, &buffer, GNUTLS_X509_FMT_DER);
    ne_free(der);

    if (ret < 0) {
        gnutls_x509_crt_deinit(x5);
        return NULL;
    }

    return populate_cert(ne_calloc(sizeof(struct ne_ssl_certificate_s)), x5);
}

char *ne_ssl_cert_export(const ne_ssl_certificate *cert)
{
    unsigned char *der;
    size_t len = 0;
    char *ret;

    /* find the length of the DER encoding. */
    if (gnutls_x509_crt_export(cert->subject, GNUTLS_X509_FMT_DER, NULL, &len) != 
        GNUTLS_E_SHORT_MEMORY_BUFFER) {
        return NULL;
    }
    
    der = ne_malloc(len);
    if (gnutls_x509_crt_export(cert->subject, GNUTLS_X509_FMT_DER, der, &len)) {
        ne_free(der);
        return NULL;
    }
    
    ret = ne_base64(der, len);
    ne_free(der);
    return ret;
}

int ne_ssl_cert_digest(const ne_ssl_certificate *cert, char *digest)
{
    char sha1[20], *p;
    int j;
    size_t len = sizeof sha1;

    if (gnutls_x509_crt_get_fingerprint(cert->subject, GNUTLS_DIG_SHA,
                                        sha1, &len) < 0)
        return -1;

    for (j = 0, p = digest; j < 20; j++) {
        *p++ = NE_HEX2ASC((sha1[j] >> 4) & 0x0f);
        *p++ = NE_HEX2ASC(sha1[j] & 0x0f);
        *p++ = ':';
    }

    *--p = '\0';
    return 0;
}

int ne__ssl_init(void)
{
#if LIBGNUTLS_VERSION_NUMBER < 0x020b01
#ifdef NE_HAVE_TS_SSL
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#endif
    gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);
#endif
    return gnutls_global_init();
}

void ne__ssl_exit(void)
{
    /* No way to unregister the thread callbacks.  Doomed. */
#if LIBGNUTLS_VERSION_MAJOR > 1 || LIBGNUTLS_VERSION_MINOR > 3 \
    || (LIBGNUTLS_VERSION_MINOR == 3 && LIBGNUTLS_VERSION_PATCH >= 3)
    /* It's safe to call gnutls_global_deinit() here only with
     * gnutls >= 1.3., since older versions don't refcount and
     * doing so would prevent any other use of gnutls within
     * the process. */
    gnutls_global_deinit();
#endif
}
