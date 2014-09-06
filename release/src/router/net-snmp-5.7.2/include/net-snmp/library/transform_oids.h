#ifndef _net_snmp_transform_oids_h
#define _net_snmp_transform_oids_h

#ifdef __cplusplus
extern          "C" {
#endif
/*
 * transform_oids.h
 *
 * Numeric MIB names for auth and priv transforms.
 */

NETSNMP_IMPORT oid      usmNoAuthProtocol[10];  /* == { 1,3,6,1,6,3,10,1,1,1 }; */
#ifndef NETSNMP_DISABLE_MD5
NETSNMP_IMPORT oid      usmHMACMD5AuthProtocol[10];     /* == { 1,3,6,1,6,3,10,1,1,2 }; */
#endif
NETSNMP_IMPORT oid      usmHMACSHA1AuthProtocol[10];    /* == { 1,3,6,1,6,3,10,1,1,3 }; */
NETSNMP_IMPORT oid      usmNoPrivProtocol[10];  /* == { 1,3,6,1,6,3,10,1,2,1 }; */
#ifndef NETSNMP_DISABLE_DES
NETSNMP_IMPORT oid      usmDESPrivProtocol[10]; /* == { 1,3,6,1,6,3,10,1,2,2 }; */
#endif

/* XXX: OIDs not defined yet */
NETSNMP_IMPORT oid      usmAESPrivProtocol[10]; /* == { 1,3,6,1,6,3,10,1,2,4 }; */
NETSNMP_IMPORT oid      *usmAES128PrivProtocol; /* backwards compat */

#define USM_AUTH_PROTO_NOAUTH_LEN 10
#define USM_AUTH_PROTO_MD5_LEN 10
#define USM_AUTH_PROTO_SHA_LEN 10
#define USM_PRIV_PROTO_NOPRIV_LEN 10
#define USM_PRIV_PROTO_DES_LEN 10

#define USM_PRIV_PROTO_AES_LEN 10
#define USM_PRIV_PROTO_AES128_LEN 10 /* backwards compat */

#ifdef __cplusplus
}
#endif
#endif
