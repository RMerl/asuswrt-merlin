/*
 * HP specific stuff that OpenView recognizes 
 */

#include <net-snmp/net-snmp-config.h>

#include <signal.h>
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_VMMETER_H
#include <sys/vmmeter.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "hpux.h"
#include "mibdefs.h"

void
int_hpux(void)
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct variable2 hp_variables[] = {
        {HPCONF, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_hp, 1, {HPCONF}},
        {HPRECONFIG, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_hp, 1, {HPRECONFIG}},
        {HPFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_hp, 1, {HPFLAG}},
        {HPLOGMASK, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_hp, 1, {ERRORFLAG}},
        {HPSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
         var_hp, 1, {ERRORMSG}}
    };

    struct variable2 hptrap_variables[] = {
        {HPTRAP, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
         var_hp, 1, {HPTRAP}},
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             hp_variables_oid[] =
        { 1, 3, 6, 1, 4, 1, 11, 2, 13, 1, 2, 1 };
    oid             hptrap_variables_oid[] =
        { 1, 3, 6, 1, 4, 1, 11, 2, 13, 2 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/hpux:hp", hp_variables, variable2,
                 hp_variables_oid);
    REGISTER_MIB("ucd-snmp/hpux:hptrap", hptrap_variables, variable2,
                 hptrap_variables_oid);

}


#ifdef RESERVED_FOR_FUTURE_USE
int
writeHP(int action,
        u_char * var_val,
        u_char var_val_type,
        int var_val_len, u_char * statP, oid * name, int name_len)
{
    DODEBUG("Gotto:  writeHP\n");
    return SNMP_ERR_NOERROR;
}
#endif

unsigned char  *
var_hp(struct variable *vp,
       oid * name,
       size_t * length,
       int exact, size_t * var_len, WriteMethod ** write_method)
{

    oid             newname[MAX_OID_LEN];
    int             result;
    static long     long_ret;

    memcpy((char *) newname, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    newname[*length] = 0;
    result =
        snmp_oid_compare(name, *length, newname, (int) vp->namelen + 1);
    if ((exact && (result != 0)) || (!exact && (result >= 0)))
        return NULL;
    memcpy((char *) name, (char *) newname,
           ((int) vp->namelen + 1) * sizeof(oid));
    *length = *length + 1;
    *var_len = sizeof(long);    /* default length */
    switch (vp->magic) {
    case HPFLAG:
    case HPCONF:
    case HPSTATUS:
    case HPRECONFIG:
        long_ret = 1;
        return (u_char *) & long_ret;   /* remove trap */
    case HPLOGMASK:
        long_ret = 3;
        return (u_char *) & long_ret;
    case HPTRAP:
        newname[*length - 1] = 128;
        newname[*length] = 120;
        newname[*length + 1] = 57;
        newname[*length + 2] = 92;
        *length = *length + 3;
        memcpy((char *) name, (char *) newname, *length * sizeof(oid));
        long_ret = ((((((128 << 8) + 120) << 8) + 57) << 8) + 92);
        return (u_char *) & long_ret;
    }
    return NULL;
}
