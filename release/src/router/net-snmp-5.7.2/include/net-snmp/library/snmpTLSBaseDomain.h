#ifndef _SNMPTLSBASEDOMAIN_H
#define _SNMPTLSBASEDOMAIN_H

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/asn1.h>
#include <net-snmp/library/container.h>

/* OpenSSL Includes */
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

/*
 * Prototypes
 */

    void netsnmp_tlsbase_ctor(void);
    void netsnmp_init_tlsbase(void);
    const char * _x509_get_error(int x509failvalue, const char *location);
    void _openssl_log_error(int rc, SSL *con, const char *location);

    /* will likely go away */
    SSL_CTX *get_client_ctx(void);
    SSL_CTX *get_server_ctx(void);

#define NETSNMP_TLSBASE_IS_CLIENT     0x01
#define NETSNMP_TLSBASE_CERT_FP_VERIFIED 0x02

    /*
     * _Internal_ structures
     */
    typedef struct _netsnmpTLSBaseData_s {
       int                        flags;
       SSL_CTX                   *ssl_context;
       SSL                       *ssl;
       BIO                       *sslbio;
       BIO                       *accept_bio;
       BIO                       *accepted_bio;
       char                      *securityName;
       char                      *addr_string;
       netsnmp_indexed_addr_pair *addr;
       char                      *our_identity;
       char                      *their_identity;
       char                      *their_fingerprint;
       char                      *their_hostname;
       char                      *trust_cert;
    } _netsnmpTLSBaseData;

#define VRFY_PARENT_WAS_OK 1
    typedef struct _netsnmp_verify_info_s {
       int flags;
    } _netsnmp_verify_info;

    SSL_CTX *sslctx_client_setup(const SSL_METHOD *,
                                 _netsnmpTLSBaseData *tlsbase);
    SSL_CTX *sslctx_server_setup(const SSL_METHOD *);

    int netsnmp_tlsbase_verify_server_cert(SSL *ssl,
                                           _netsnmpTLSBaseData *tlsdata);
    int netsnmp_tlsbase_verify_client_cert(SSL *ssl,
                                           _netsnmpTLSBaseData *tlsdata);
    int netsnmp_tlsbase_extract_security_name(SSL *ssl, _netsnmpTLSBaseData *tlsdata);
    _netsnmpTLSBaseData *netsnmp_tlsbase_allocate_tlsdata(netsnmp_transport *t,
                                                          int isserver);
    int netsnmp_tlsbase_wrapup_recv(netsnmp_tmStateReference *tmStateRef,
                                    _netsnmpTLSBaseData *tlsdata,
                                    void **opaque, int *olength);
    int netsnmp_tlsbase_config(struct netsnmp_transport_s *t,
                               const char *token, const char *value);

    int netsnmp_tlsbase_session_init(struct netsnmp_transport_s *,
                                     struct snmp_session *sess);
    int tls_get_verify_info_index(void);

    void netsnmp_tlsbase_free_tlsdata(_netsnmpTLSBaseData *tlsbase);
#ifdef __cplusplus
}
#endif
#endif/*_SNMPTLSBASEDOMAIN_H*/
