#ifndef NETSNMP_FEATURES_H
#define NETSNMP_FEATURES_H

/* include files that are only needed if --enable-minimalist is on */
#ifdef NETSNMP_MINIMAL_CODE
#ifdef NETSNMP_FEATURE_CHECKING
#include <net-snmp/feature-details.h>
#else /* ! NETSNMP_FEATURE_CHECKING */
#include <net-snmp/library/features.h>
#ifndef NETSNMP_DISABLE_AGENT
#include <net-snmp/agent/features.h>
#include <net-snmp/agent/features-mibgroups.h>
#endif
#ifndef NETSNMP_DISABLE_APPS
#include <net-snmp/features-snmpnetstat.h>
#include <net-snmp/features-apps.h>
#endif
#endif /* ! NETSNMP_FEATURE_CHECKING */
#endif /* NETSNMP_MINIMAL_CODE */


/* prototypes always needed */
#ifndef NETSNMP_FEATURE_CHECKING
#define netsnmp_feature_provide(X)
#define netsnmp_feature_require(X)
#define netsnmp_feature_want(X)
#define netsnmp_feature_child_of(X, Y)
#endif

#define netsnmp_feature_unused(X) char netsnmp_feature_unused_ ## X

#endif /* NETSNMP_FEATURES_H */
