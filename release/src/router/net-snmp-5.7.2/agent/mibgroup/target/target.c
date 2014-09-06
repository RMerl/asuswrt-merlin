#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#if defined(NETSNMP_TRANSPORT_DTLSUDP_DOMAIN) || defined(NETSNMP_TRANSPORT_TLSTCP_DOMAIN)
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <net-snmp/library/cert_util.h>
#endif
#ifdef NETSNMP_TRANSPORT_TLSTCP_DOMAIN
#include <net-snmp/library/snmpTLSTCPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_DTLSUDP_DOMAIN
#include <net-snmp/library/snmpDTLSUDPDomain.h>
#endif

#include "snmpTargetAddrEntry.h"
#include "snmpTargetParamsEntry.h"
#include "target.h"

netsnmp_feature_require(tdomain_support)
netsnmp_feature_require(tdomain_transport_oid)
netsnmp_feature_want(netsnmp_tlstmAddr_get_serverId)

#define MAX_TAGS 128

netsnmp_session *
get_target_sessions(char *taglist, TargetFilterFunction * filterfunct,
                    void *filterArg)
{
    netsnmp_session *ret = NULL, thissess;
    struct targetAddrTable_struct *targaddrs;
    char            buf[SPRINT_MAX_LEN];
    char            tags[MAX_TAGS][SPRINT_MAX_LEN], *cp;
    int             numtags = 0, i;
#if defined(NETSNMP_TRANSPORT_DTLSUDP_DOMAIN) || defined(NETSNMP_TRANSPORT_TLSTCP_DOMAIN)
    int             tls = 0;
#endif
    static struct targetParamTable_struct *param;

    DEBUGMSGTL(("target_sessions", "looking for: %s\n", taglist));
    for (cp = taglist; cp && numtags < MAX_TAGS;) {
        cp = copy_nword(cp, tags[numtags], sizeof(tags[numtags]));
        DEBUGMSGTL(("target_sessions", " for: %d=%s\n", numtags,
                    tags[numtags]));
        numtags++;
    }

    for (targaddrs = get_addrTable(); targaddrs;
         targaddrs = targaddrs->next) {

        /*
         * legal row? 
         */
        if (targaddrs->tDomain == NULL ||
            targaddrs->tAddress == NULL ||
            targaddrs->rowStatus != SNMP_ROW_ACTIVE) {
            DEBUGMSGTL(("target_sessions", "  which is not ready yet\n"));
            continue;
        }

        if (netsnmp_tdomain_support
            (targaddrs->tDomain, targaddrs->tDomainLen, NULL, NULL) == 0) {
            snmp_log(LOG_ERR,
                     "unsupported domain for target address table entry %s\n",
                     targaddrs->name);
        }

        /*
         * check tag list to see if we match 
         */
        if (targaddrs->tagList) {
            int matched = 0;

            /*
             * loop through tag list looking for requested tags 
             */
            for (cp = targaddrs->tagList; cp && !matched;) {
                cp = copy_nword(cp, buf, sizeof(buf));
                for (i = 0; i < numtags && !matched; i++) {
                    if (strcmp(buf, tags[i]) == 0) {
                        /*
                         * found a valid target table entry 
                         */
                        DEBUGMSGTL(("target_sessions", "found one: %s\n",
                                    tags[i]));

                        if (targaddrs->params) {
                            param = get_paramEntry(targaddrs->params);
                            if (!param
                                || param->rowStatus != SNMP_ROW_ACTIVE) {
                                /*
                                 * parameter entry must exist and be active 
                                 */
                                continue;
                            }
                        } else {
                            /*
                             * parameter entry must be specified 
                             */
                            continue;
                        }

                        /*
                         * last chance for caller to opt-out.  Call
                         * filtering function 
                         */
                        if (filterfunct &&
                            (*(filterfunct)) (targaddrs, param,
                                              filterArg)) {
                            continue;
                        }

                        /*
                         * Only one notification per TargetAddrEntry,
                         * rather than one per tag
                         */
                        matched = 1;

                        if (targaddrs->storageType != ST_READONLY &&
                            targaddrs->sess &&
                            param->updateTime >=
                            targaddrs->sessionCreationTime) {
                            /*
                             * parameters have changed, nuke the old session 
                             */
                            snmp_close(targaddrs->sess);
                            targaddrs->sess = NULL;
                        }

                        /*
                         * target session already exists? 
                         */
                        if (targaddrs->sess == NULL) {
                            /*
                             * create an appropriate snmp session and add
                             * it to our return list 
                             */
                            netsnmp_transport *t = NULL;

                            t = netsnmp_tdomain_transport_oid(targaddrs->
                                                              tDomain,
                                                              targaddrs->
                                                              tDomainLen,
                                                              targaddrs->
                                                              tAddress,
                                                              targaddrs->
                                                              tAddressLen,
                                                              0);
                            if (t == NULL) {
                                DEBUGMSGTL(("target_sessions",
                                            "bad dest \""));
                                DEBUGMSGOID(("target_sessions",
                                             targaddrs->tDomain,
                                             targaddrs->tDomainLen));
                                DEBUGMSG(("target_sessions", "\", \""));
                                DEBUGMSGHEX(("target_sessions",
                                             targaddrs->tAddress,
                                             targaddrs->tAddressLen));
                                DEBUGMSG(("target_sessions", "\n"));
                                continue;
                            } else {
                                char           *dst_str =
                                    t->f_fmtaddr(t, NULL, 0);
                                if (dst_str != NULL) {
                                    DEBUGMSGTL(("target_sessions",
                                                "  to: %s\n", dst_str));
                                    free(dst_str);
                                }
                            }
                            /*
                             * if tDomain is tls related, check for tls config
                             */
#ifdef NETSNMP_TRANSPORT_DTLSUDP_DOMAIN
                            tls = snmp_oid_compare(targaddrs->tDomain,
                                                   targaddrs->tDomainLen,
                                                   netsnmpDTLSUDPDomain,
                                                   netsnmpDTLSUDPDomain_len);

#endif
#ifdef NETSNMP_TRANSPORT_TLSTCP_DOMAIN
                            if (tls)
                                tls = snmp_oid_compare(targaddrs->tDomain,
                                                       targaddrs->tDomainLen,
                                                       netsnmpTLSTCPDomain,
                                                       netsnmpTLSTCPDomain_len);
#endif
#if defined(NETSNMP_TRANSPORT_DTLSUDP_DOMAIN) || defined(NETSNMP_TRANSPORT_TLSTCP_DOMAIN)
                            if (!tls) {
                                netsnmp_cert *cert;
                                char         *server_id = NULL;

                                DEBUGMSGTL(("target_sessions",
                                            "  looking up our id: %s\n",
                                            targaddrs->params));
                                cert =
                                    netsnmp_cert_find(NS_CERT_IDENTITY,
                                                      NS_CERTKEY_TARGET_PARAM,
                                                      targaddrs->params);
                                netsnmp_assert(t->f_config);
                                if (cert) {
                                    DEBUGMSGTL(("target_sessions",
                                            "  found fingerprint: %s\n", 
                                                cert->fingerprint));
                                    t->f_config(t, "localCert",
                                                cert->fingerprint);
                                }
                                DEBUGMSGTL(("target_sessions",
                                            "  looking up their id: %s\n",
                                            targaddrs->name));
                                cert =
                                    netsnmp_cert_find(NS_CERT_REMOTE_PEER,
                                                      NS_CERTKEY_TARGET_ADDR,
                                                      targaddrs->name);
                                if (cert) {
                                    DEBUGMSGTL(("target_sessions",
                                            "  found fingerprint: %s\n", 
                                                cert->fingerprint));
                                    t->f_config(t, "peerCert",
                                                cert->fingerprint);
                                }
#ifndef NETSNMP_FEATURE_REMOVE_TLSTMADDR_GET_SERVERID
                                server_id = netsnmp_tlstmAddr_get_serverId(
                                    targaddrs->name);
#endif /* NETSNMP_FEATURE_REMOVE_TLSTMADDR_GET_SERVERID */
                                if (server_id) {
                                    DEBUGMSGTL(("target_sessions",
                                            "  found serverId: %s\n", 
                                                server_id));
                                    t->f_config(t, "their_hostname", server_id);
                                }
                            }
#endif
                            memset(&thissess, 0, sizeof(thissess));
                            thissess.timeout = (targaddrs->timeout) * 1000;
                            thissess.retries = targaddrs->retryCount;
                            DEBUGMSGTL(("target_sessions",
                                        "timeout: %d -> %ld\n",
                                        targaddrs->timeout,
                                        thissess.timeout));

                            if (param->mpModel == SNMP_VERSION_3 &&
                                param->secModel != SNMP_SEC_MODEL_USM &&
                                param->secModel != SNMP_SEC_MODEL_TSM) {
                                snmp_log(LOG_ERR,
                                         "unsupported mpModel/secModel combo %d/%d for target %s\n",
                                         param->mpModel, param->secModel,
                                         targaddrs->name);
                                /*
                                 * XXX: memleak 
                                 */
                                netsnmp_transport_free(t);
                                continue;
                            }
                            thissess.paramName = strdup(param->paramName);
                            thissess.version = param->mpModel;
                            if (param->mpModel == SNMP_VERSION_3) {
                                thissess.securityName = strdup(param->secName);
                                thissess.securityNameLen =
                                    strlen(thissess.securityName);
                                thissess.securityLevel = param->secLevel;
                                thissess.securityModel = param->secModel;
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
                            } else {
                                thissess.community =
                                    (u_char *) strdup(param->secName);
                                thissess.community_len =
                                    strlen((char *) thissess.community);
#endif
                            }

                            thissess.flags |= SNMP_FLAGS_DONT_PROBE;
                            targaddrs->sess = snmp_add(&thissess, t,
                                                       NULL, NULL);
                            thissess.flags &= ~SNMP_FLAGS_DONT_PROBE;
                            targaddrs->sessionCreationTime = time(NULL);
                        }
                        if (targaddrs->sess) {
                            if (NULL == targaddrs->sess->paramName)
                                targaddrs->sess->paramName =
                                    strdup(param->paramName);

                            targaddrs->sess->next = ret;
                            ret = targaddrs->sess;
                        } else {
                            snmp_sess_perror("target session", &thissess);
                        }
                    }
                }
            }
        }
    }
    return ret;
}
