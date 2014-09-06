#ifndef NETSNMP_CERT_UTIL_H

#if defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL)

#ifndef HEADER_SSL_H
#error "must include <openssl/ssl.h> before cert_util.h"
#endif
#ifndef HEADER_X509_H
#error "must include <openssl/x509.h> before cert_util.h"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

    /*************************************************************************
     *
     * netsnmp_cert structures, defines and function definitions
     *
     *************************************************************************/

void netsnmp_certs_init(void);
void netsnmp_certs_agent_init(void);
void netsnmp_certs_shutdown(void);
void netsnmp_certs_load(void);
netsnmp_container *netsnmp_cert_get_trustlist(void);

    typedef struct netsnmp_cert_common_s {
        char           *dir;
        char           *filename;
        
        u_char          type;
        u_char          allowed_uses;
        u_char          _pad[2]; /* for future use */
    } netsnmp_cert_common;

    typedef struct netsnmp_key_s {
        netsnmp_cert_common info;

        EVP_PKEY       *okey;
        struct netsnmp_cert_s   *cert;
    } netsnmp_key;

    typedef struct netsnmp_cert_s {
        netsnmp_cert_common info;

        X509           *ocert;
        netsnmp_key    *key;
        struct netsnmp_cert_s *issuer_cert;

        char           *issuer;
        char           *subject;
        char           *fingerprint;
        char           *common_name;

        u_char          hash_type;
        u_char          _pad[3]; /* for future use */
    } netsnmp_cert;

/** types */
    enum { NS_CERT_TYPE_UNKNOWN = 0, NS_CERT_TYPE_PEM, NS_CERT_TYPE_DER,
           NS_CERT_TYPE_PKCS12, NS_CERT_TYPE_KEY };

/** uses */
#define NS_CERT_IDENTITY       0x0001
#define NS_CERT_REMOTE_PEER    0x0002
#define NS_CERT_RESERVED1      0x0004
#define NS_CERT_CA             0x0008

/** source */
#define NS_CERTKEY_DEFAULT       0x000 /* get default from DS store */
#define NS_CERTKEY_FILE          0x001 /* filename/full path */
#define NS_CERTKEY_FINGERPRINT   0x002 /* public key fingerprint */
#define NS_CERTKEY_CA            0x004 /* trusted CA */
#define NS_CERTKEY_SAN_RFC822    0x008 /* subj alt name: rfc822 */
#define NS_CERTKEY_SAN_DNS       0x010 /* subj alt name: DNS */
#define NS_CERTKEY_SAN_IPADDR    0x020 /* subj alt name: IP address */
#define NS_CERTKEY_COMMON_NAME   0x040 /* common name */
#define NS_CERTKEY_TARGET_PARAM  0x080 /* tlstmParamsTable */
#define NS_CERTKEY_TARGET_ADDR   0x100 /* tlstmAddrTable */
#define NS_CERTKEY_MULTIPLE      0x200 /* try multiple sources */

/** RFC 5246 hash algorithms (Section 7.4.1.4.1) */
#define NS_HASH_NONE        0
#define NS_HASH_MD5         1
#define NS_HASH_SHA1        2
#define NS_HASH_SHA224      3
#define NS_HASH_SHA256      4
#define NS_HASH_SHA384      5
#define NS_HASH_SHA512      6
#define NS_HASH_MAX         NS_HASH_SHA512

/** SNMP-TLS-TM-MIB */
#define SNMPTLSFINGERPRINT_MAX_LEN   255

    /*************************************************************************
     * netsnmp_cert function definitions
     *************************************************************************/

    netsnmp_cert *netsnmp_cert_find(int what, int where, void *hint);

    int netsnmp_cert_check_vb_fingerprint(const netsnmp_variable_list *var);

    void netsnmp_fp_lowercase_and_strip_colon(char *fp);
    int netsnmp_cert_parse_hash_type(const char *str);
    int netsnmp_tls_fingerprint_build(int hash_type, const char *hex_fp,
                                      u_char **tls_fp, size_t *tls_fp_len,
                                      int allow_realloc);
    int netsnmp_tls_fingerprint_parse(const u_char *binary_fp, int fp_len,
                                      char **fp_str_ptr, u_int *fp_str_len,
                                      int allow_realloc, u_char *hash_type_ptr);


    int netsnmp_cert_trust(SSL_CTX *ctx, netsnmp_cert *thiscert);
    int netsnmp_cert_trust_ca(SSL_CTX *ctx, netsnmp_cert *thiscertsrootca);

    /*************************************************************************
     *
     *  certificate to Transport Security Name mapping (netsnmp_cert_map)
     *
     *************************************************************************/

#define TSNM_tlstmCertSpecified                 1
#define TSNM_tlstmCertSANRFC822Name             2
#define TSNM_tlstmCertSANDNSName                3
#define TSNM_tlstmCertSANIpAddress              4
#define TSNM_tlstmCertSANAny                    5
#define TSNM_tlstmCertCommonName                6
#define TSNM_tlstmCert_MAX                      TSNM_tlstmCertCommonName

#define NSCM_FROM_CONFIG                        0x0001
#define NSCM_FROM_MIB                           0x0002
#define NSCM_NONVOLATILE                        0x0004

    typedef struct netsnmp_cert_map_s {
        int             priority;
        char           *fingerprint;
        int             mapType;
        char           *data;

        char            hashType;
        char            flags;

        X509           *ocert;
    } netsnmp_cert_map;

    netsnmp_cert_map *netsnmp_cert_map_alloc(char *fp, X509 *ocert);
    void netsnmp_cert_map_free(netsnmp_cert_map *cert_map);
    int netsnmp_cert_map_add(netsnmp_cert_map *map);
    int netsnmp_cert_map_remove(netsnmp_cert_map *map);
    netsnmp_cert_map *netsnmp_cert_map_find(netsnmp_cert_map *map);

    void netsnmp_cert_map_container_free(netsnmp_container *c);
    netsnmp_container *netsnmp_cert_map_container_create(int with_fp);
    netsnmp_container *netsnmp_cert_map_container(void);

    int netsnmp_cert_get_secname_maps(netsnmp_container *cm);

    /*************************************************************************
     *
     *  snmpTlstmParamsTable data
     *
     *************************************************************************/
    typedef struct snmpTlstmParams_s {
        char         *name;
        char         *fingerprint;
        char          hashType;
        u_char        flags;
        u_char        fingerprint_len;
    } snmpTlstmParams;
    
#define TLSTM_PARAMS_FROM_CONFIG          0x01
#define TLSTM_PARAMS_FROM_MIB             0x02
#define TLSTM_PARAMS_NONVOLATILE          0x04
/** ine TLSTM_PARAMS_XXX                  0x08 */

    snmpTlstmParams *netsnmp_tlstmParams_create(const char *tag, int hashType,
                                                const char *fp, int fp_len);
    void netsnmp_tlstmParams_free(snmpTlstmParams *stp);
    snmpTlstmParams *netsnmp_tlstmParams_restore_common(char **line);

    netsnmp_container *netsnmp_tlstmParams_container(void);
    int netsnmp_tlstmParams_add(snmpTlstmParams *stp);
    int netsnmp_tlstmParams_remove(snmpTlstmParams *stp);
    snmpTlstmParams *netsnmp_tlstmParams_find(snmpTlstmParams *stp);

    /*************************************************************************
     *
     *  snmpTlstmAddrTable data
     *
     *************************************************************************/
    typedef struct snmpTlstmAddr_s {
        char         *name;
        char         *fingerprint;
        char         *identity;
        
        u_char        hashType;
        u_char        flags;
        
    } snmpTlstmAddr;

#define TLSTM_ADDR_FROM_CONFIG          0x01
#define TLSTM_ADDR_FROM_MIB             0x02
#define TLSTM_ADDR_NONVOLATILE          0x04
/** ine TLSTM_ADDR_XXX                  0x08 */

    int netsnmp_tlstmAddr_restore_common(char **line, char *name,
                                         size_t *name_len, char *id,
                                         size_t *id_len, char *fp,
                                         size_t *fp_len, u_char *ht);
    netsnmp_container *netsnmp_tlstmAddr_container(void);
    snmpTlstmAddr *netsnmp_tlstmAddr_find(snmpTlstmAddr *entry);
    snmpTlstmAddr *netsnmp_tlstmAddr_create(char *targetAddrName);
    void netsnmp_tlstmAddr_free(snmpTlstmAddr *entry);
    int netsnmp_tlstmAddr_add(snmpTlstmAddr *entry);
    int netsnmp_tlstmAddr_remove(snmpTlstmAddr *entry);
    char *netsnmp_tlstmAddr_get_serverId(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL) */

#endif /* NETSNMP_CERT_UTIL_H */

