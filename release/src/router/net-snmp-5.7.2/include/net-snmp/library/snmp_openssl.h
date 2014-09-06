/*
 * Header file for the OpenSSL Wrapper
 */

#ifndef SNMP_OPENSSL_H
#define SNMP_OPENSSL_H

#ifdef __cplusplus
extern          "C" {
#endif

    void netsnmp_init_openssl(void);

    /*
     * cert fields
     */
    void netsnmp_openssl_cert_dump_names(X509 *ocert);
    void netsnmp_openssl_cert_dump_extensions(X509 *ocert);

    char *netsnmp_openssl_cert_get_commonName(X509 *, char **buf, int *len);
    char *netsnmp_openssl_cert_get_subjectName(X509 *, char **buf, int *len);
    char *netsnmp_openssl_cert_get_fingerprint(X509 *ocert, int alg);
    int netsnmp_openssl_cert_get_hash_type(X509 *ocert);

    int netsnmp_openssl_cert_issued_by(X509 *issuer, X509 *cert);

    char *netsnmp_openssl_extract_secname(netsnmp_cert_map *cert_map,
                                          netsnmp_cert_map *peer_cert);

    char *netsnmp_openssl_cert_get_subjectAltName(X509 *, char **buf, int *len);

    /*
     * ssl cert chains
     */
    netsnmp_container *netsnmp_openssl_get_cert_chain(SSL *ssl);

    /*
     * misc
     */
    void netsnmp_openssl_err_log(const char *prefix);
    void netsnmp_openssl_null_checks(SSL *ssl, int *nullAuth, int *nullCipher);

#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_OPENSSL_H */
