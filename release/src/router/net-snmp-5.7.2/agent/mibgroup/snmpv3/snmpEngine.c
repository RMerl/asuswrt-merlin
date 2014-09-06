/*
 * snmpEngine.c: implement's the SNMP-FRAMEWORK-MIB. 
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/sysORTable.h>

#include "util_funcs/header_generic.h"
#include "snmpEngine.h"

netsnmp_feature_child_of(snmpengine_all, libnetsnmpmibs)

netsnmp_feature_child_of(register_snmpEngine_scalars_context, snmpengine_all)

struct variable2 snmpEngine_variables[] = {
    {SNMPENGINEID, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_snmpEngine, 1, {1}},
#ifndef NETSNMP_NO_WRITE_SUPPORT 
#ifdef NETSNMP_ENABLE_TESTING_CODE
    {SNMPENGINEBOOTS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_snmpEngine, 1, {2}},
    {SNMPENGINETIME, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_snmpEngine, 1, {3}},
#else                           /* !NETSNMP_ENABLE_TESTING_CODE */
    {SNMPENGINEBOOTS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_snmpEngine, 1, {2}},
    {SNMPENGINETIME, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_snmpEngine, 1, {3}},
#endif                          /* NETSNMP_ENABLE_TESTING_CODE */
#else  /* !NETSNMP_NO_WRITE_SUPPORT */ 
    {SNMPENGINEBOOTS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_snmpEngine, 1, {2}},
    {SNMPENGINETIME, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_snmpEngine, 1, {3}},
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
    {SNMPENGINEMAXMESSAGESIZE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_snmpEngine, 1, {4}},
};

/*
 * now load this mib into the agents mib table 
 */
oid             snmpEngine_variables_oid[] =
    { 1, 3, 6, 1, 6, 3, 10, 2, 1 };

void
register_snmpEngine_scalars(void)
{
    REGISTER_MIB("snmpv3/snmpEngine", snmpEngine_variables, variable2,
                 snmpEngine_variables_oid);
}

#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_SNMPENGINE_SCALARS_CONTEXT
void
register_snmpEngine_scalars_context(const char *contextName)
{
    register_mib_context("snmpv3/snmpEngine",
                         (struct variable *) snmpEngine_variables,
                         sizeof(struct variable2),
                         sizeof(snmpEngine_variables)/sizeof(struct variable2),
                         snmpEngine_variables_oid,
                         sizeof(snmpEngine_variables_oid)/sizeof(oid),
                         DEFAULT_MIB_PRIORITY, 0, 0, NULL,
                         contextName, -1, 0);
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_SNMPENGINE_SCALARS_CONTEXT */

void
init_snmpEngine(void)
{
    oid      reg[] = { 1, 3, 6, 1, 6, 3, 10, 3, 1, 1 };
    REGISTER_SYSOR_ENTRY(reg, "The SNMP Management Architecture MIB.");
    register_snmpEngine_scalars();
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
#ifdef NETSNMP_ENABLE_TESTING_CODE
int             write_engineBoots(int, u_char *, u_char, size_t, u_char *,
                                  oid *, size_t);
int             write_engineTime(int, u_char *, u_char, size_t, u_char *,
                                 oid *, size_t);
#endif                          /* NETSNMP_ENABLE_TESTING_CODE */
#endif /* NETSNMP_NO_WRITE_SUPPORT */

u_char         *
var_snmpEngine(struct variable *vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{

    /*
     * variables we may use later 
     */
    static long     long_ret;
    static unsigned char engineID[SNMP_MAXBUF];

    *write_method = (WriteMethod*)0;    /* assume it isnt writable for the time being */
    *var_len = sizeof(long_ret);        /* assume an integer and change later if not */

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return NULL;

    /*
     * this is where we do the value assignments for the mib results. 
     */
    switch (vp->magic) {

    case SNMPENGINEID:
        *var_len = snmpv3_get_engineID(engineID, SNMP_MAXBUF);
        /*
         * XXX  Set ERROR_MSG() upon error? 
         */
        return (unsigned char *) engineID;

    case SNMPENGINEBOOTS:
#ifndef NETSNMP_NO_WRITE_SUPPORT
#ifdef NETSNMP_ENABLE_TESTING_CODE
        *write_method = write_engineBoots;
#endif                          /* NETSNMP_ENABLE_TESTING_CODE */
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        long_ret = snmpv3_local_snmpEngineBoots();
        return (unsigned char *) &long_ret;

    case SNMPENGINETIME:
#ifndef NETSNMP_NO_WRITE_SUPPORT
#ifdef NETSNMP_ENABLE_TESTING_CODE
        *write_method = write_engineTime;
#endif                          /* NETSNMP_ENABLE_TESTING_CODE */
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        long_ret = snmpv3_local_snmpEngineTime();
        return (unsigned char *) &long_ret;

    case SNMPENGINEMAXMESSAGESIZE:
        long_ret = 1500;
        return (unsigned char *) &long_ret;

    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_snmpEngine\n",
                    vp->magic));
    }
    return NULL;
}


#ifndef NETSNMP_NO_WRITE_SUPPORT
#ifdef NETSNMP_ENABLE_TESTING_CODE
/*
 * write_engineBoots():
 * 
 * This is technically not writable a writable mib object, but we
 * allow it so we can run some time synchronization tests.
 */
int
write_engineBoots(int action,
                  u_char * var_val,
                  u_char var_val_type,
                  size_t var_val_len,
                  u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;
    size_t          size;
    int             bigsize = SNMP_MAXBUF_MEDIUM;
    u_char          engineIDBuf[SNMP_MAXBUF_MEDIUM];
    int             engineIDBufLen = 0;

    if (var_val_type != ASN_INTEGER) {
        DEBUGMSGTL(("snmpEngine",
                    "write to engineBoots not ASN_INTEGER\n"));
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long_ret)) {
        DEBUGMSGTL(("snmpEngine", "write to engineBoots: bad length\n"));
        return SNMP_ERR_WRONGLENGTH;
    }
    long_ret = *((long *) var_val);
    if (action == COMMIT) {
        engineIDBufLen =
            snmpv3_get_engineID(engineIDBuf, SNMP_MAXBUF_MEDIUM);
        /*
         * set our local engineTime in the LCD timing cache 
         */
        snmpv3_set_engineBootsAndTime(long_ret,
                                      snmpv3_local_snmpEngineTime());
        set_enginetime(engineIDBuf, engineIDBufLen,
                       snmpv3_local_snmpEngineBoots(),
                       snmpv3_local_snmpEngineTime(), TRUE);
    }
    return SNMP_ERR_NOERROR;
}

/*
 * write_engineTime():
 * 
 * This is technically not a writable mib object, but we
 * allow it so we can run some time synchronization tests.
 */
int
write_engineTime(int action,
                 u_char * var_val,
                 u_char var_val_type,
                 size_t var_val_len,
                 u_char * statP, oid * name, size_t name_len)
{
    /*
     * variables we may use later 
     */
    static long     long_ret;
    size_t          size;
    int             bigsize = SNMP_MAXBUF_MEDIUM;
    u_char          engineIDBuf[SNMP_MAXBUF_MEDIUM];
    int             engineIDBufLen = 0;

    if (var_val_type != ASN_INTEGER) {
        DEBUGMSGTL(("snmpEngine",
                    "write to engineTime not ASN_INTEGER\n"));
        return SNMP_ERR_WRONGTYPE;
    }
    if (var_val_len > sizeof(long_ret)) {
        DEBUGMSGTL(("snmpEngine", "write to engineTime: bad length\n"));
        return SNMP_ERR_WRONGLENGTH;
    }
    long_ret = *((long *) var_val);
    if (action == COMMIT) {
        engineIDBufLen =
            snmpv3_get_engineID(engineIDBuf, SNMP_MAXBUF_MEDIUM);
        /*
         * set our local engineTime in the LCD timing cache 
         */
        snmpv3_set_engineBootsAndTime(snmpv3_local_snmpEngineBoots(),
                                      long_ret);
        set_enginetime(engineIDBuf, engineIDBufLen,
                       snmpv3_local_snmpEngineBoots(),
                       snmpv3_local_snmpEngineTime(), TRUE);
    }
    return SNMP_ERR_NOERROR;
}

#endif                          /* NETSNMP_ENABLE_TESTING_CODE */
#endif /* NETSNMP_NO_WRITE_SUPPORT */
