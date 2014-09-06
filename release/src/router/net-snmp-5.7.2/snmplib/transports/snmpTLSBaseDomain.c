#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>

netsnmp_feature_require(cert_util)

#include <net-snmp/library/snmpTLSBaseDomain.h>

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#include <errno.h>
#include <ctype.h>

/* OpenSSL Includes */
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <net-snmp/types.h>
#include <net-snmp/config_api.h>
#include <net-snmp/library/cert_util.h>
#include <net-snmp/library/snmp_openssl.h>
#include <net-snmp/library/default_store.h>
#include <net-snmp/library/callback.h>
#include <net-snmp/library/snmp_logging.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmp_secmod.h>
#include <net-snmp/library/read_config.h>
#include <net-snmp/library/system.h>

#define LOGANDDIE(msg) do { snmp_log(LOG_ERR, "%s\n", msg); return 0; } while(0)

int openssl_local_index;

/* this is called during negotiationn */
int verify_callback(int ok, X509_STORE_CTX *ctx) {
    int err, depth;
    char buf[1024], *fingerprint;
    X509 *thecert;
    netsnmp_cert *cert;
    _netsnmp_verify_info *verify_info;
    SSL *ssl;

    thecert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);
    
    /* things to do: */

    X509_NAME_oneline(X509_get_subject_name(thecert), buf, sizeof(buf));
    fingerprint = netsnmp_openssl_cert_get_fingerprint(thecert, -1);
    DEBUGMSGTL(("tls_x509:verify", "Cert: %s\n", buf));
    DEBUGMSGTL(("tls_x509:verify", "  fp: %s\n", fingerprint ?
                fingerprint : "unknown"));

    ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    verify_info = SSL_get_ex_data(ssl, openssl_local_index);

    if (verify_info && ok && depth > 0) {
        /* remember that a parent certificate has been marked as trusted */
        verify_info->flags |= VRFY_PARENT_WAS_OK;
    }

    /* this ensures for self-signed certificates we have a valid
       locally known fingerprint and then accept it */
    if (!ok &&
        (X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT == err ||
         X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY == err ||
         X509_V_ERR_CERT_UNTRUSTED == err ||
         X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE == err ||
         X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN == err)) {

        cert = netsnmp_cert_find(NS_CERT_REMOTE_PEER, NS_CERTKEY_FINGERPRINT,
                                 (void*)fingerprint);
        if (cert)
            DEBUGMSGTL(("tls_x509:verify", " Found locally: %s/%s\n",
                        cert->info.dir, cert->info.filename));


        if (cert) {
            DEBUGMSGTL(("tls_x509:verify", "verify_callback called with: ok=%d ctx=%p depth=%d err=%i:%s\n", ok, ctx, depth, err, X509_verify_cert_error_string(err)));
            DEBUGMSGTL(("tls_x509:verify", "  accepting matching fp of self-signed certificate found in: %s\n",
                        cert->info.filename));
            SNMP_FREE(fingerprint);
            return 1;
        } else {
            DEBUGMSGTL(("tls_x509:verify", "  no matching fp found\n"));
            /* log where we are and why called */
            snmp_log(LOG_ERR, "tls verification failure: ok=%d ctx=%p depth=%d err=%i:%s\n", ok, ctx, depth, err, X509_verify_cert_error_string(err));
            SNMP_FREE(fingerprint);
            return 0;
        }

        if (0 == depth && verify_info &&
            (verify_info->flags & VRFY_PARENT_WAS_OK)) {
            DEBUGMSGTL(("tls_x509:verify", "verify_callback called with: ok=%d ctx=%p depth=%d err=%i:%s\n", ok, ctx, depth, err, X509_verify_cert_error_string(err)));
            DEBUGMSGTL(("tls_x509:verify", "  a parent was ok, so returning ok for this child certificate\n"));
            SNMP_FREE(fingerprint);
            return 1; /* we'll check the hostname later at this level */
        }
    }

    if (0 == ok)
        snmp_log(LOG_ERR, "tls verification failure: ok=%d ctx=%p depth=%d err=%i:%s\n", ok, ctx, depth, err, X509_verify_cert_error_string(err));
    else
        DEBUGMSGTL(("tls_x509:verify", "verify_callback called with: ok=%d ctx=%p depth=%d err=%i:%s\n", ok, ctx, depth, err, X509_verify_cert_error_string(err)));
    DEBUGMSGTL(("tls_x509:verify", "  returning the passed in value of %d\n",
                ok));
    SNMP_FREE(fingerprint);
    return(ok);
}

#define VERIFIED_FINGERPRINT      0
#define NO_FINGERPRINT_AVAILABLE  1
#define FAILED_FINGERPRINT_VERIFY 2

static int
_netsnmp_tlsbase_verify_remote_fingerprint(X509 *remote_cert,
                                           _netsnmpTLSBaseData *tlsdata,
                                           int try_default) {

    char            *fingerprint;

    fingerprint =
        netsnmp_openssl_cert_get_fingerprint(remote_cert, -1);

    if (!fingerprint) {
        /* no peer cert */
        snmp_log(LOG_ERR, "failed to get fingerprint of remote certificate\n");
        return FAILED_FINGERPRINT_VERIFY;
    }

    if (!tlsdata->their_fingerprint && tlsdata->their_identity) {
        /* we have an identity; try and find it's fingerprint */
        netsnmp_cert *peer_cert;
        peer_cert =
            netsnmp_cert_find(NS_CERT_REMOTE_PEER, NS_CERTKEY_MULTIPLE,
                              tlsdata->their_identity);

        if (peer_cert)
            tlsdata->their_fingerprint =
                netsnmp_openssl_cert_get_fingerprint(peer_cert->ocert, -1);
    }

    if (!tlsdata->their_fingerprint && try_default) {
        /* try for the default instead */
        netsnmp_cert *peer_cert;
        peer_cert =
            netsnmp_cert_find(NS_CERT_REMOTE_PEER, NS_CERTKEY_DEFAULT,
                              NULL);

        if (peer_cert)
            tlsdata->their_fingerprint =
                netsnmp_openssl_cert_get_fingerprint(peer_cert->ocert, -1);
    }
    
    if (tlsdata->their_fingerprint) {
        netsnmp_fp_lowercase_and_strip_colon(tlsdata->their_fingerprint);
        if (0 != strcmp(tlsdata->their_fingerprint, fingerprint)) {
            snmp_log(LOG_ERR, "The fingerprint from the remote side's certificate didn't match the expected\n");
            snmp_log(LOG_ERR, "  got %s, expected %s\n",
                     fingerprint, tlsdata->their_fingerprint);
            free(fingerprint);
            return FAILED_FINGERPRINT_VERIFY;
        }
    } else {
        DEBUGMSGTL(("tls_x509:verify", "No fingerprint for the remote entity available to verify\n"));
        free(fingerprint);
        return NO_FINGERPRINT_AVAILABLE;
    }

    free(fingerprint);
    return VERIFIED_FINGERPRINT;
}

/* this is called after the connection on the client side by us to check
   other aspects about the connection */
int
netsnmp_tlsbase_verify_server_cert(SSL *ssl, _netsnmpTLSBaseData *tlsdata) {
    /* XXX */
    X509            *remote_cert;
    char            *check_name;
    int              ret;
    
    netsnmp_assert_or_return(ssl != NULL, SNMPERR_GENERR);
    netsnmp_assert_or_return(tlsdata != NULL, SNMPERR_GENERR);
    
    if (NULL == (remote_cert = SSL_get_peer_certificate(ssl))) {
        /* no peer cert */
        DEBUGMSGTL(("tls_x509:verify",
                    "remote connection provided no certificate (yet)\n"));
        return SNMPERR_TLS_NO_CERTIFICATE;
    }

    /* make sure that the fingerprint matches */
    ret = _netsnmp_tlsbase_verify_remote_fingerprint(remote_cert, tlsdata, 1);
    switch(ret) {
    case VERIFIED_FINGERPRINT:
        return SNMPERR_SUCCESS;

    case FAILED_FINGERPRINT_VERIFY:
        return SNMPERR_GENERR;

    case NO_FINGERPRINT_AVAILABLE:
        if (tlsdata->their_hostname && tlsdata->their_hostname[0] != '\0') {
            GENERAL_NAMES      *onames;
            const GENERAL_NAME *oname = NULL;
            int                 i, j;
            int                 count;
            char                buf[SPRINT_MAX_LEN];
            int                 is_wildcarded = 0;
            char               *compare_to;

            /* see if the requested hostname has a wildcard prefix */
            if (strncmp(tlsdata->their_hostname, "*.", 2) == 0) {
                is_wildcarded = 1;
                compare_to = tlsdata->their_hostname + 2;
            } else {
                compare_to = tlsdata->their_hostname;
            }

            /* if the hostname we were expecting to talk to matches
               the cert, then we can accept this connection. */

            /* check against the DNS subjectAltName */
            onames = (GENERAL_NAMES *)X509_get_ext_d2i(remote_cert,
                                                       NID_subject_alt_name,
                                                       NULL, NULL );
            if (NULL != onames) {
                count = sk_GENERAL_NAME_num(onames);

                for (i=0 ; i <count; ++i)  {
                    oname = sk_GENERAL_NAME_value(onames, i);
                    if (GEN_DNS == oname->type) {

                        /* get the value */
                        ASN1_STRING_to_UTF8((unsigned char**)&check_name,
                                            oname->d.ia5);

                        /* convert to lowercase for comparisons */
                        for (j = 0 ;
                             *check_name && j < sizeof(buf)-1;
                             ++check_name, ++j ) {
                            if (isascii(*check_name))
                                buf[j] = tolower(0xFF & *check_name);
                        }
                        if (j < sizeof(buf))
                            buf[j] = '\0';
                        check_name = buf;
                        
                        if (is_wildcarded) {
                            /* we *only* allow passing till the first '.' */
                            /* ie *.example.com can't match a.b.example.com */
                            check_name = strchr(check_name, '.') + 1;
                        }

                        DEBUGMSGTL(("tls_x509:verify", "checking subjectAltname of dns:%s\n", check_name));
                        if (strcmp(compare_to, check_name) == 0) {

                            DEBUGMSGTL(("tls_x509:verify", "Successful match on a subjectAltname of dns:%s\n", check_name));
                            return SNMPERR_SUCCESS;
                        }
                    }
                }
            }

            /* check the common name for a match */
            check_name =
                netsnmp_openssl_cert_get_commonName(remote_cert, NULL, NULL);

            if (is_wildcarded) {
                /* we *only* allow passing till the first '.' */
                /* ie *.example.com can't match a.b.example.com */
                check_name = strchr(check_name, '.') + 1;
            }

            if (strcmp(compare_to, check_name) == 0) {
                DEBUGMSGTL(("tls_x509:verify", "Successful match on a common name of %s\n", check_name));
                return SNMPERR_SUCCESS;
            }

            snmp_log(LOG_ERR, "No matching names in the certificate to match the expected %s\n", tlsdata->their_hostname);
            return SNMPERR_GENERR;

        }
        /* XXX: check for hostname match instead */
        snmp_log(LOG_ERR, "Can not verify a remote server identity without configuration\n");
        return SNMPERR_GENERR;
    }
    DEBUGMSGTL(("tls_x509:verify", "shouldn't get here\n"));
    return SNMPERR_GENERR;
}

/* this is called after the connection on the server side by us to check
   the validity of the client's certificate */
int
netsnmp_tlsbase_verify_client_cert(SSL *ssl, _netsnmpTLSBaseData *tlsdata) {
    /* XXX */
    X509            *remote_cert;
    int ret;

    /* RFC5953: section 5.3.2, paragraph 1:
       A (D)TLS server should accept new session connections from any client
       that it is able to verify the client's credentials for.  This is done
       by authenticating the client's presented certificate through a
       certificate path validation process (e.g.  [RFC5280]) or through
       certificate fingerprint verification using fingerprints configured in
       the snmpTlstmCertToTSNTable.  Afterward the server will determine the
       identity of the remote entity using the following procedures.
    */
    /* Implementation notes:
       + path validation is taken care of during the openssl verify
         routines, our part of which is hanlded in verify_callback
         above.
       + fingerprint verification happens below.
    */
    if (NULL == (remote_cert = SSL_get_peer_certificate(ssl))) {
        /* no peer cert */
        DEBUGMSGTL(("tls_x509:verify",
                    "remote connection provided no certificate (yet)\n"));
        return SNMPERR_TLS_NO_CERTIFICATE;
    }

    /* we don't force a known remote fingerprint for a client since we
       will accept any certificate we know about (and later derive the
       securityName from it and apply access control) */
    ret = _netsnmp_tlsbase_verify_remote_fingerprint(remote_cert, tlsdata, 0);
    switch(ret) {
    case FAILED_FINGERPRINT_VERIFY:
        DEBUGMSGTL(("tls_x509:verify", "failed to verify a client fingerprint\n"));
        return SNMPERR_GENERR;

    case NO_FINGERPRINT_AVAILABLE:
        DEBUGMSGTL(("tls_x509:verify", "no known fingerprint available (not a failure case)\n"));
        return SNMPERR_SUCCESS;

    case VERIFIED_FINGERPRINT:
        DEBUGMSGTL(("tls_x509:verify", "Verified client fingerprint\n"));
        tlsdata->flags |= NETSNMP_TLSBASE_CERT_FP_VERIFIED;
        return SNMPERR_SUCCESS;
    }

    DEBUGMSGTL(("tls_x509:verify", "shouldn't get here\n"));
    return SNMPERR_GENERR;
}

/* this is called after the connection on the server side by us to
   check other aspects about the connection and obtain the
   securityName from the remote certificate. */
int
netsnmp_tlsbase_extract_security_name(SSL *ssl, _netsnmpTLSBaseData *tlsdata) {
    netsnmp_container  *chain_maps;
    netsnmp_cert_map   *cert_map, *peer_cert;
    netsnmp_iterator  *itr;
    int                 rc;

    netsnmp_assert_or_return(ssl != NULL, SNMPERR_GENERR);
    netsnmp_assert_or_return(tlsdata != NULL, SNMPERR_GENERR);

    if (NULL == (chain_maps = netsnmp_openssl_get_cert_chain(ssl)))
        return SNMPERR_GENERR;
    /*
     * map fingerprints to mapping entries
     */
    rc = netsnmp_cert_get_secname_maps(chain_maps);
    if ((-1 == rc) || (CONTAINER_SIZE(chain_maps) == 0)) {
        netsnmp_cert_map_container_free(chain_maps);
        return SNMPERR_GENERR;
    }

    /*
     * change container to sorted (by clearing unsorted option), then
     * iterate over it until we find a map that returns a secname.
     */
    CONTAINER_SET_OPTIONS(chain_maps, 0, rc);
    itr = CONTAINER_ITERATOR(chain_maps);
    if (NULL == itr) {
        snmp_log(LOG_ERR, "could not get iterator for secname fingerprints\n");
        netsnmp_cert_map_container_free(chain_maps);
        return SNMPERR_GENERR;
    }
    peer_cert = cert_map = ITERATOR_FIRST(itr);
    for( ; !tlsdata->securityName && cert_map; cert_map = ITERATOR_NEXT(itr))
        tlsdata->securityName =
            netsnmp_openssl_extract_secname(cert_map, peer_cert);
    ITERATOR_RELEASE(itr);

    netsnmp_cert_map_container_free(chain_maps);
       
    return (tlsdata->securityName ? SNMPERR_SUCCESS : SNMPERR_GENERR);
}

int
_trust_this_cert(SSL_CTX *the_ctx, char *certspec) {
    netsnmp_cert *trustcert;

    DEBUGMSGTL(("sslctx_client", "Trying to load a trusted certificate: %s\n",
                certspec));

    /* load this identifier into the trust chain */
    trustcert = netsnmp_cert_find(NS_CERT_CA,
                                  NS_CERTKEY_MULTIPLE,
                                  certspec);
    if (!trustcert)
        trustcert = netsnmp_cert_find(NS_CERT_REMOTE_PEER,
                                      NS_CERTKEY_MULTIPLE,
                                      certspec);
    if (!trustcert)
        LOGANDDIE("failed to find requested certificate to trust");
        
    /* Add the certificate to the context */
    if (netsnmp_cert_trust_ca(the_ctx, trustcert) != SNMPERR_SUCCESS)
        LOGANDDIE("failed to load trust certificate");

    return 1;
}

void
_load_trusted_certs(SSL_CTX *the_ctx) {
    netsnmp_container *trusted_certs = NULL;
    netsnmp_iterator  *trusted_cert_iterator = NULL;
    char *fingerprint;

    trusted_certs = netsnmp_cert_get_trustlist();
    trusted_cert_iterator = CONTAINER_ITERATOR(trusted_certs);
    if (trusted_cert_iterator) {
        for (fingerprint = (char *) ITERATOR_FIRST(trusted_cert_iterator);
             fingerprint; fingerprint = ITERATOR_NEXT(trusted_cert_iterator)) {
            if (!_trust_this_cert(the_ctx, fingerprint))
                snmp_log(LOG_ERR, "failed to load trust cert: %s\n",
                         fingerprint);
        }
        ITERATOR_RELEASE(trusted_cert_iterator);
    }
}    

SSL_CTX *
_sslctx_common_setup(SSL_CTX *the_ctx, _netsnmpTLSBaseData *tlsbase) {
    char         *crlFile;
    char         *cipherList;
    X509_LOOKUP  *lookup;
    X509_STORE   *cert_store = NULL;

    _load_trusted_certs(the_ctx);

    /* add in the CRLs if available */

    crlFile = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_X509_CRL_FILE);
    if (NULL != crlFile) {
        cert_store = SSL_CTX_get_cert_store(the_ctx);
        DEBUGMSGTL(("sslctx_client", "loading CRL: %s\n", crlFile));
        if (!cert_store)
            LOGANDDIE("failed to find certificate store");
        if (!(lookup = X509_STORE_add_lookup(cert_store, X509_LOOKUP_file())))
            LOGANDDIE("failed to create a lookup function for the CRL file");
        if (X509_load_crl_file(lookup, crlFile, X509_FILETYPE_PEM) != 1)
            LOGANDDIE("failed to load the CRL file");
        /* tell openssl to check CRLs */
        X509_STORE_set_flags(cert_store,
                             X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }

    cipherList = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                       NETSNMP_DS_LIB_TLS_ALGORITMS);
    if (NULL != cipherList) {
        if (SSL_CTX_set_cipher_list(the_ctx, cipherList) != 1)
            LOGANDDIE("failed to set the cipher list to the requested value");
        else
            snmp_log(LOG_INFO,"set SSL cipher list to '%s'\n", cipherList);
    }
    return the_ctx;
}

SSL_CTX *
sslctx_client_setup(const SSL_METHOD *method, _netsnmpTLSBaseData *tlsbase) {
    netsnmp_cert *id_cert, *peer_cert;
    SSL_CTX      *the_ctx;

    /***********************************************************************
     * Set up the client context
     */
    the_ctx = SSL_CTX_new(NETSNMP_REMOVE_CONST(SSL_METHOD *, method));
    if (!the_ctx) {
        snmp_log(LOG_ERR, "ack: %p\n", the_ctx);
        LOGANDDIE("can't create a new context");
    }
    SSL_CTX_set_read_ahead (the_ctx, 1); /* Required for DTLS */
        
    SSL_CTX_set_verify(the_ctx,
                       SSL_VERIFY_PEER|
                       SSL_VERIFY_FAIL_IF_NO_PEER_CERT|
                       SSL_VERIFY_CLIENT_ONCE,
                       &verify_callback);

    if (tlsbase->our_identity) {
        DEBUGMSGTL(("sslctx_client", "looking for local id: %s\n", tlsbase->our_identity));
        id_cert = netsnmp_cert_find(NS_CERT_IDENTITY, NS_CERTKEY_MULTIPLE,
                                    tlsbase->our_identity);
    } else {
        DEBUGMSGTL(("sslctx_client", "looking for default local id: %s\n", tlsbase->our_identity));
        id_cert = netsnmp_cert_find(NS_CERT_IDENTITY, NS_CERTKEY_DEFAULT, NULL);
    }

    if (!id_cert)
        LOGANDDIE ("error finding client identity keys");

    if (!id_cert->key || !id_cert->key->okey)
        LOGANDDIE("failed to load private key");

    DEBUGMSGTL(("sslctx_client", "using public key: %s\n",
                id_cert->info.filename));
    DEBUGMSGTL(("sslctx_client", "using private key: %s\n",
                id_cert->key->info.filename));

    if (SSL_CTX_use_certificate(the_ctx, id_cert->ocert) <= 0)
        LOGANDDIE("failed to set the certificate to use");

    if (SSL_CTX_use_PrivateKey(the_ctx, id_cert->key->okey) <= 0)
        LOGANDDIE("failed to set the private key to use");

    if (!SSL_CTX_check_private_key(the_ctx))
        LOGANDDIE("public and private keys incompatible");

    if (tlsbase->their_identity)
        peer_cert = netsnmp_cert_find(NS_CERT_REMOTE_PEER,
                                      NS_CERTKEY_MULTIPLE,
                                      tlsbase->their_identity);
    else
        peer_cert = netsnmp_cert_find(NS_CERT_REMOTE_PEER, NS_CERTKEY_DEFAULT,
                                      NULL);
    if (peer_cert) {
        DEBUGMSGTL(("sslctx_client", "server's expected public key: %s\n",
                    peer_cert ? peer_cert->info.filename : "none"));

        /* Trust the expected certificate */
        if (netsnmp_cert_trust_ca(the_ctx, peer_cert) != SNMPERR_SUCCESS)
            LOGANDDIE ("failed to set verify paths");
    }

    /* trust a certificate (possibly a CA) aspecifically passed in */
    if (tlsbase->trust_cert) {
        if (!_trust_this_cert(the_ctx, tlsbase->trust_cert))
            return 0;
    }

    return _sslctx_common_setup(the_ctx, tlsbase);
}

SSL_CTX *
sslctx_server_setup(const SSL_METHOD *method) {
    netsnmp_cert *id_cert;

    /***********************************************************************
     * Set up the server context
     */
    /* setting up for ssl */
    SSL_CTX *the_ctx = SSL_CTX_new(NETSNMP_REMOVE_CONST(SSL_METHOD *, method));
    if (!the_ctx) {
        LOGANDDIE("can't create a new context");
    }

    id_cert = netsnmp_cert_find(NS_CERT_IDENTITY, NS_CERTKEY_DEFAULT, NULL);
    if (!id_cert)
        LOGANDDIE ("error finding server identity keys");

    if (!id_cert->key || !id_cert->key->okey)
        LOGANDDIE("failed to load private key");

    DEBUGMSGTL(("sslctx_server", "using public key: %s\n",
                id_cert->info.filename));
    DEBUGMSGTL(("sslctx_server", "using private key: %s\n",
                id_cert->key->info.filename));

    if (SSL_CTX_use_certificate(the_ctx, id_cert->ocert) <= 0)
        LOGANDDIE("failed to set the certificate to use");

    if (SSL_CTX_use_PrivateKey(the_ctx, id_cert->key->okey) <= 0)
        LOGANDDIE("failed to set the private key to use");

    if (!SSL_CTX_check_private_key(the_ctx))
        LOGANDDIE("public and private keys incompatible");

    SSL_CTX_set_read_ahead(the_ctx, 1); /* XXX: DTLS only? */

    SSL_CTX_set_verify(the_ctx,
                       SSL_VERIFY_PEER|
                       SSL_VERIFY_FAIL_IF_NO_PEER_CERT|
                       SSL_VERIFY_CLIENT_ONCE,
                       &verify_callback);

    return _sslctx_common_setup(the_ctx, NULL);
}

int
netsnmp_tlsbase_config(struct netsnmp_transport_s *t, const char *token, const char *value) {
    _netsnmpTLSBaseData *tlsdata;

    netsnmp_assert_or_return(t != NULL, -1);
    netsnmp_assert_or_return(t->data != NULL, -1);

    tlsdata = t->data;

    if ((strcmp(token, "localCert") == 0) ||
        (strcmp(token, "our_identity") == 0)) {
        SNMP_FREE(tlsdata->our_identity);
        tlsdata->our_identity = strdup(value);
        DEBUGMSGT(("tls:config","our identity %s\n", value));
    }

    if ((strcmp(token, "peerCert") == 0) ||
        (strcmp(token, "their_identity") == 0)) {
        SNMP_FREE(tlsdata->their_identity);
        tlsdata->their_identity = strdup(value);
        DEBUGMSGT(("tls:config","their identity %s\n", value));
    }

    if ((strcmp(token, "peerHostname") == 0) ||
        (strcmp(token, "their_hostname") == 0)) {
        SNMP_FREE(tlsdata->their_hostname);
        tlsdata->their_hostname = strdup(value);
    }

    if ((strcmp(token, "trust_cert") == 0) ||
        (strcmp(token, "trustCert") == 0)) {
        SNMP_FREE(tlsdata->trust_cert);
        tlsdata->trust_cert = strdup(value);
    }
    
    return SNMPERR_SUCCESS;
}

int
netsnmp_tlsbase_session_init(struct netsnmp_transport_s *transport,
                             struct snmp_session *sess) {
    /* the default security model here should be TSM; most other
       things won't work with TLS because we'll throw out the packet
       if it doesn't have a proper tmStateRef (and onyl TSM generates
       this at the moment */
    if (!(transport->flags & NETSNMP_TRANSPORT_FLAG_LISTEN)) {
        if (sess->securityModel == SNMP_DEFAULT_SECMODEL) {
            sess->securityModel = SNMP_SEC_MODEL_TSM;
        } else if (sess->securityModel != SNMP_SEC_MODEL_TSM) {
            sess->securityModel = SNMP_SEC_MODEL_TSM;
            snmp_log(LOG_ERR, "A security model other than TSM is being used with (D)TLS; using TSM anyways\n");
        }

        if (NULL == sess->securityName) {
            /* client side doesn't need a real securityName */
            /* XXX: call-home issues require them to set one for VACM; but
               we don't do callhome yet */
            sess->securityName = strdup("__BOGUS__");
            sess->securityNameLen = strlen(sess->securityName);
        }

        if (sess->version != SNMP_VERSION_3) {
            sess->version = SNMP_VERSION_3;
            snmp_log(LOG_ERR, "A SNMP version other than 3 was requested with (D)TLS; using 3 anyways\n");
        }

        if (sess->securityLevel <= 0) {
            sess->securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
        }
    }
    return SNMPERR_SUCCESS;
}

static int have_done_bootstrap = 0;

static int
tls_bootstrap(int majorid, int minorid, void *serverarg, void *clientarg) {
    char indexname[] = "_netsnmp_verify_info";

    /* don't do this more than once */
    if (have_done_bootstrap)
        return 0;
    have_done_bootstrap = 1;

    netsnmp_certs_load();

    openssl_local_index =
        SSL_get_ex_new_index(0, indexname, NULL, NULL, NULL);

    return 0;
}

int
tls_get_verify_info_index() {
    return openssl_local_index;
}

static void _parse_client_cert(const char *tok, char *line)
{
    config_pwarn("clientCert is deprecated. Clients should use localCert, servers should use peerCert");
    if (*line == '"') {
        char buf[SNMP_MAXBUF];
        copy_nword(line, buf, sizeof(buf));
        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                              NETSNMP_DS_LIB_X509_CLIENT_PUB, buf);
    } else
        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                              NETSNMP_DS_LIB_X509_CLIENT_PUB, line);
}

static void _parse_server_cert(const char *tok, char *line)
{
    config_pwarn("serverCert is deprecated. Clients should use peerCert, servers should use localCert.");
    if (*line == '"') {
        char buf[SNMP_MAXBUF];
        copy_nword(line, buf, sizeof(buf));
        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                              NETSNMP_DS_LIB_X509_CLIENT_PUB, buf);
    } else
        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                              NETSNMP_DS_LIB_X509_SERVER_PUB, line);
}

void
netsnmp_tlsbase_ctor(void) {

    /* bootstrap ssl since we'll need it */
    netsnmp_init_openssl();

    /* the private client cert to authenticate with */
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "extraX509SubDir",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_CERT_EXTRA_SUBDIR);

    /* Do we have a CRL list? */
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "x509CRLFile",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_X509_CRL_FILE);

    /* What TLS algorithms should be use */
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "tlsAlgorithms",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_TLS_ALGORITMS);

    /*
     * for the client
     */

    /* the public client cert to authenticate with */
    register_config_handler("snmp", "clientCert", _parse_client_cert, NULL,
                            NULL);

    /*
     * for the server
     */

    /* The X509 server key to use */
    register_config_handler("snmp", "serverCert", _parse_server_cert, NULL,
                            NULL);
    /*
     * remove cert config ambiguity: localCert, peerCert
     */
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "localCert",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_TLS_LOCAL_CERT);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "peerCert",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_TLS_PEER_CERT);

    /*
     * register our boot-strapping needs
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
			   SNMP_CALLBACK_POST_PREMIB_READ_CONFIG,
			   tls_bootstrap, NULL);

}

_netsnmpTLSBaseData *
netsnmp_tlsbase_allocate_tlsdata(netsnmp_transport *t, int isserver) {

    _netsnmpTLSBaseData *tlsdata;

    if (NULL == t)
        return NULL;

    /* allocate our TLS specific data */
    tlsdata = SNMP_MALLOC_TYPEDEF(_netsnmpTLSBaseData);
    if (NULL == tlsdata) {
        SNMP_FREE(t);
        return NULL;
    }

    if (!isserver)
        tlsdata->flags |= NETSNMP_TLSBASE_IS_CLIENT;

    return tlsdata;
}

void netsnmp_tlsbase_free_tlsdata(_netsnmpTLSBaseData *tlsbase) {
    if (!tlsbase)
        return;

    DEBUGMSGTL(("tlsbase","Freeing TLS Base data for a session\n"));

    if (tlsbase->ssl)
        SSL_free(tlsbase->ssl);

    if (tlsbase->ssl_context)
        SSL_CTX_free(tlsbase->ssl_context);
   /* don't free the accept_bio since it's the parent bio */
    /*
    if (tlsbase->accept_bio)
        BIO_free(tlsbase->accept_bio);
    */
    /* and this is freed by the SSL shutdown */
    /* 
    if (tlsbase->accepted_bio)
      BIO_free(tlsbase->accept_bio);
    */

    /* free the config data */
    SNMP_FREE(tlsbase->securityName);
    SNMP_FREE(tlsbase->addr_string);
    SNMP_FREE(tlsbase->our_identity);
    SNMP_FREE(tlsbase->their_identity);
    SNMP_FREE(tlsbase->their_fingerprint);
    SNMP_FREE(tlsbase->their_hostname);
    SNMP_FREE(tlsbase->trust_cert);

    /* free the base itself */
    SNMP_FREE(tlsbase);
}

int netsnmp_tlsbase_wrapup_recv(netsnmp_tmStateReference *tmStateRef,
                                _netsnmpTLSBaseData *tlsdata,
                                void **opaque, int *olength) {
    int no_auth, no_priv;

    if (NULL == tlsdata)
        return SNMPERR_GENERR;

    /* RFC5953 Section 5.1.2 step 2: tmSecurityLevel */
    /*
     * Don't accept null authentication. Null encryption ok.
     *
     * XXX: this should actually check for a configured list of encryption
     *      algorithms to map to NOPRIV, but for the moment we'll
     *      accept any encryption alogrithms that openssl is using.
     */
    netsnmp_openssl_null_checks(tlsdata->ssl, &no_auth, &no_priv);
    if (no_auth == 1) { /* null/unknown authentication */
        /* xxx-rks: snmp_increment_statistic(STAT_???); */
        snmp_log(LOG_ERR, "tls connection with NULL authentication\n");
        SNMP_FREE(tmStateRef);
        return SNMPERR_GENERR;
    }
    else if (no_priv == 1) /* null/unknown encryption */
        tmStateRef->transportSecurityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
    else
        tmStateRef->transportSecurityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
    DEBUGMSGTL(("tls:secLevel", "SecLevel %d\n",
                tmStateRef->transportSecurityLevel));

    /* use x509 cert to do lookup to secname if DNE in cachep yet */

    /* RFC5953: section 5.3.2, paragraph 2:
       The (D)TLS server identifies the authenticated identity from the
       (D)TLS client's principal certificate using configuration information
       from the snmpTlstmCertToTSNTable mapping table.  The (D)TLS server
       MUST request and expect a certificate from the client and MUST NOT
       accept SNMP messages over the (D)TLS connection until the client has
       sent a certificate and it has been authenticated.  The resulting
       derived tmSecurityName is recorded in the tmStateReference cache as
       tmSecurityName.  The details of the lookup process are fully
       described in the DESCRIPTION clause of the snmpTlstmCertToTSNTable
       MIB object.  If any verification fails in any way (for example
       because of failures in cryptographic verification or because of the
       lack of an appropriate row in the snmpTlstmCertToTSNTable) then the
       session establishment MUST fail, and the
       snmpTlstmSessionInvalidClientCertificates object is incremented.  If
       the session can not be opened for any reason at all, including
       cryptographic verification failures, then the
       snmpTlstmSessionOpenErrors counter is incremented and processing
       stops.
    */

    if (!tlsdata->securityName) {
        netsnmp_tlsbase_extract_security_name(tlsdata->ssl, tlsdata);
        if (NULL != tlsdata->securityName) {
            DEBUGMSGTL(("tls", "set SecName to: %s\n", tlsdata->securityName));
        } else {
	    snmp_increment_statistic(STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCLIENTCERTIFICATES);
	    snmp_increment_statistic(STAT_TLSTM_SNMPTLSTMSESSIONOPENERRORS);
            SNMP_FREE(tmStateRef);
            return SNMPERR_GENERR;
        }
    }

    /* RFC5953 Section 5.1.2 step 2: tmSecurityName */
    /* XXX: detect and throw out overflow secname sizes rather
       than truncating. */
    strlcpy(tmStateRef->securityName, tlsdata->securityName,
            sizeof(tmStateRef->securityName));
    tmStateRef->securityNameLen = strlen(tmStateRef->securityName);

    /* RFC5953 Section 5.1.2 step 2: tmSessionID */
    /* use our TLSData pointer as the session ID */
    memcpy(tmStateRef->sessionID, &tlsdata, sizeof(netsnmp_tmStateReference *));

    /* save the tmStateRef in our special pointer */
    *opaque = tmStateRef;
    *olength = sizeof(netsnmp_tmStateReference);

    return SNMPERR_SUCCESS;
}

netsnmp_feature_child_of(_x509_get_error, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE__X509_GET_ERROR
const char * _x509_get_error(int x509failvalue, const char *location) {
    static const char *reason = NULL;
    
    /* XXX: use this instead: X509_verify_cert_error_string(err) */

    switch (x509failvalue) {
    case X509_V_OK:
        reason = "X509_V_OK";
        break;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        reason = "X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT";
        break;
    case X509_V_ERR_UNABLE_TO_GET_CRL:
        reason = "X509_V_ERR_UNABLE_TO_GET_CRL";
        break;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
        reason = "X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE";
        break;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
        reason = "X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE";
        break;
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
        reason = "X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY";
        break;
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
        reason = "X509_V_ERR_CERT_SIGNATURE_FAILURE";
        break;
    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
        reason = "X509_V_ERR_CRL_SIGNATURE_FAILURE";
        break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
        reason = "X509_V_ERR_CERT_NOT_YET_VALID";
        break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
        reason = "X509_V_ERR_CERT_HAS_EXPIRED";
        break;
    case X509_V_ERR_CRL_NOT_YET_VALID:
        reason = "X509_V_ERR_CRL_NOT_YET_VALID";
        break;
    case X509_V_ERR_CRL_HAS_EXPIRED:
        reason = "X509_V_ERR_CRL_HAS_EXPIRED";
        break;
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
        reason = "X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD";
        break;
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
        reason = "X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD";
        break;
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
        reason = "X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD";
        break;
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
        reason = "X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD";
        break;
    case X509_V_ERR_OUT_OF_MEM:
        reason = "X509_V_ERR_OUT_OF_MEM";
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        reason = "X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT";
        break;
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        reason = "X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN";
        break;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        reason = "X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY";
        break;
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        reason = "X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE";
        break;
    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
        reason = "X509_V_ERR_CERT_CHAIN_TOO_LONG";
        break;
    case X509_V_ERR_CERT_REVOKED:
        reason = "X509_V_ERR_CERT_REVOKED";
        break;
    case X509_V_ERR_INVALID_CA:
        reason = "X509_V_ERR_INVALID_CA";
        break;
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
        reason = "X509_V_ERR_PATH_LENGTH_EXCEEDED";
        break;
    case X509_V_ERR_INVALID_PURPOSE:
        reason = "X509_V_ERR_INVALID_PURPOSE";
        break;
    case X509_V_ERR_CERT_UNTRUSTED:
        reason = "X509_V_ERR_CERT_UNTRUSTED";
        break;
    case X509_V_ERR_CERT_REJECTED:
        reason = "X509_V_ERR_CERT_REJECTED";
        break;
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
        reason = "X509_V_ERR_SUBJECT_ISSUER_MISMATCH";
        break;
    case X509_V_ERR_AKID_SKID_MISMATCH:
        reason = "X509_V_ERR_AKID_SKID_MISMATCH";
        break;
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
        reason = "X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH";
        break;
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
        reason = "X509_V_ERR_KEYUSAGE_NO_CERTSIGN";
        break;
    case X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER:
        reason = "X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER";
        break;
    case X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION:
        reason = "X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION";
        break;
    case X509_V_ERR_KEYUSAGE_NO_CRL_SIGN:
        reason = "X509_V_ERR_KEYUSAGE_NO_CRL_SIGN";
        break;
    case X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION:
        reason = "X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION";
        break;
    case X509_V_ERR_INVALID_NON_CA:
        reason = "X509_V_ERR_INVALID_NON_CA";
        break;
    case X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED:
        reason = "X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED";
        break;
    case X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE:
        reason = "X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE";
        break;
    case X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED:
        reason = "X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED";
        break;
#ifdef X509_V_ERR_INVALID_EXTENSION /* not avail on darwin */
    case X509_V_ERR_INVALID_EXTENSION:
        reason = "X509_V_ERR_INVALID_EXTENSION";
        break;
#endif
#ifdef X509_V_ERR_INVALID_POLICY_EXTENSION /* not avail on darwin */
    case X509_V_ERR_INVALID_POLICY_EXTENSION:
        reason = "X509_V_ERR_INVALID_POLICY_EXTENSION";
        break;
#endif
#ifdef X509_V_ERR_NO_EXPLICIT_POLICY /* not avail on darwin */
    case X509_V_ERR_NO_EXPLICIT_POLICY:
        reason = "X509_V_ERR_NO_EXPLICIT_POLICY";
        break;
#endif
#ifdef X509_V_ERR_UNNESTED_RESOURCE /* not avail on darwin */
    case X509_V_ERR_UNNESTED_RESOURCE:
        reason = "X509_V_ERR_UNNESTED_RESOURCE";
        break;
#endif
    case X509_V_ERR_APPLICATION_VERIFICATION:
        reason = "X509_V_ERR_APPLICATION_VERIFICATION";
        break;
    default:
        reason = "unknown failure code";
    }

    return reason;
}
#endif /* NETSNMP_FEATURE_REMOVE__X509_GET_ERROR */

void _openssl_log_error(int rc, SSL *con, const char *location) {
    const char     *reason, *file, *data;
    unsigned long   numerical_reason;
    int             flags, line;

    snmp_log(LOG_ERR, "---- OpenSSL Related Errors: ----\n");

    /* SSL specific errors */
    if (con) {

        int sslnum = SSL_get_error(con, rc);

        switch(sslnum) {
        case SSL_ERROR_NONE:
            reason = "SSL_ERROR_NONE";
            break;

        case SSL_ERROR_SSL:
            reason = "SSL_ERROR_SSL";
            break;

        case SSL_ERROR_WANT_READ:
            reason = "SSL_ERROR_WANT_READ";
            break;

        case SSL_ERROR_WANT_WRITE:
            reason = "SSL_ERROR_WANT_WRITE";
            break;

        case SSL_ERROR_WANT_X509_LOOKUP:
            reason = "SSL_ERROR_WANT_X509_LOOKUP";
            break;

        case SSL_ERROR_SYSCALL:
            reason = "SSL_ERROR_SYSCALL";
            snmp_log(LOG_ERR, "TLS error: %s: rc=%d, sslerror = %d (%s): system_error=%d (%s)\n",
                     location, rc, sslnum, reason, errno, strerror(errno));
            snmp_log(LOG_ERR, "TLS Error: %s\n",
                     ERR_reason_error_string(ERR_get_error()));
            return;

        case SSL_ERROR_ZERO_RETURN:
            reason = "SSL_ERROR_ZERO_RETURN";
            break;

        case SSL_ERROR_WANT_CONNECT:
            reason = "SSL_ERROR_WANT_CONNECT";
            break;

        case SSL_ERROR_WANT_ACCEPT:
            reason = "SSL_ERROR_WANT_ACCEPT";
            break;
            
        default:
            reason = "unknown";
        }

        snmp_log(LOG_ERR, " TLS error: %s: rc=%d, sslerror = %d (%s)\n",
                 location, rc, sslnum, reason);

        snmp_log(LOG_ERR, " TLS Error: %s\n",
                 ERR_reason_error_string(ERR_get_error()));

    }

    /* other errors */
    while ((numerical_reason =
            ERR_get_error_line_data(&file, &line, &data, &flags)) != 0) {
        snmp_log(LOG_ERR, " error: #%lu (file %s, line %d)\n",
                 numerical_reason, file, line);

        /* if we have a text translation: */
        if (data && (flags & ERR_TXT_STRING)) {
            snmp_log(LOG_ERR, "  Textual Error: %s\n", data);
            /*
             * per openssl man page: If it has been allocated by
             * OPENSSL_malloc(), *flags&ERR_TXT_MALLOCED is true.
             *
             * arggh... stupid openssl prototype for ERR_get_error_line_data
             * wants a const char **, but returns something that we might
             * need to free??
             */
            if (flags & ERR_TXT_MALLOCED)
                OPENSSL_free(NETSNMP_REMOVE_CONST(void *, data));        }
    }
    
    snmp_log(LOG_ERR, "---- End of OpenSSL Errors ----\n");
}
