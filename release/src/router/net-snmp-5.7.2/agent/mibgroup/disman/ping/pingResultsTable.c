/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:pingResultsTable.c
 *File Description:Rows of lookupResultsTable MIB add and delete.
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */


/*
 * This should always be included first before anything else 
 */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif




#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "pingCtlTable.h"
#include "pingResultsTable.h"
#include "pingProbeHistoryTable.h"
#include "header_complex.h"


/*
 *pingResultsTable_variables_oid:
 *
 */
oid             pingResultsTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 80, 1, 3 };

struct variable2 pingResultsTable_variables[] = {
    {COLUMN_PINGRESULTSOPERSTATUS,       ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 1}},
    {COLUMN_PINGRESULTSIPTARGETADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 2}},
    {COLUMN_PINGRESULTSIPTARGETADDRESS, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 3}},
    {COLUMN_PINGRESULTSMINRTT,          ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 4}},
    {COLUMN_PINGRESULTSMAXRTT,          ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 5}},
    {COLUMN_PINGRESULTSAVERAGERTT,      ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 6}},
    {COLUMN_PINGRESULTSPROBERESPONSES,  ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 7}},
    {COLUMN_PINGRESULTSSENTPROBES,      ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 8}},
    {COLUMN_PINGRESULTSRTTSUMOFSQUARES, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 9}},
    {COLUMN_PINGRESULTSLASTGOODPROBE,  ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_pingResultsTable, 2, {1, 10}}
};




/*
 * global storage of our data, saved in and configured by header_complex() 
 */

extern struct header_complex_index *pingCtlTableStorage;
extern struct header_complex_index *pingResultsTableStorage;
int
pingResultsTable_inadd(struct pingResultsTable_data *thedata);

void
pingResultsTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr;

    DEBUGMSGTL(("pingResultsTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        header_complex_extract_entry(&pingResultsTableStorage, hciptr);
        DEBUGMSGTL(("pingResultsTable", "cleaner  "));
    }

}
void
init_pingResultsTable(void)
{

    DEBUGMSGTL(("pingResultsTable", "initializing...  "));


    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("pingResultsTable", pingResultsTable_variables, variable2,
                 pingResultsTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("pingResultsTable",
                                  parse_pingResultsTable, NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_pingResultsTable, NULL);

    DEBUGMSGTL(("pingResultsTable", "done.\n"));
}

/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */

void
parse_pingResultsTable(const char *token, char *line)
{
    size_t          tmpint;
    struct pingResultsTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(pingResultsTable_data);

    DEBUGMSGTL(("pingResultsTable", "parsing config...  "));


    if (StorageTmp == NULL) {
        config_perror("malloc failure");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlOwnerIndex,
                              &StorageTmp->pingCtlOwnerIndexLen);
    if (StorageTmp->pingCtlOwnerIndex == NULL) {
        config_perror("invalid specification for pingCtlOwnerIndex");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlTestName,
                              &StorageTmp->pingCtlTestNameLen);
    if (StorageTmp->pingCtlTestName == NULL) {
        config_perror("invalid specification for pingCtlTestName");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingResultsOperStatus, &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingResultsIpTargetAddressType,
                              &tmpint);
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingResultsIpTargetAddress,
                              &StorageTmp->pingResultsIpTargetAddressLen);
    if (StorageTmp->pingResultsIpTargetAddress == NULL) {
        config_perror
            ("invalid specification for pingResultsIpTargetAddress");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingResultsMinRtt, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingResultsMaxRtt, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingResultsAverageRtt, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingResultsProbeResponses,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingResultsSendProbes, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingResultsRttSumOfSquares,
                              &tmpint);
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingResultsLastGoodProbe,
                              &StorageTmp->pingResultsLastGoodProbeLen);
    if (StorageTmp->pingResultsLastGoodProbe == NULL) {
        config_perror
            ("invalid specification for pingResultsLastGoodProbe!");
        return;
    }

    pingResultsTable_inadd(StorageTmp);

    /* pingResultsTable_cleaner(pingResultsTableStorage); */

    DEBUGMSGTL(("pingResultsTable", "done.\n"));
}





/*
 * store_pingResultsTable():
 *   stores .conf file entries needed to configure the mib.
 */

int
store_pingResultsTable(int majorID, int minorID, void *serverarg,
                       void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr;
    size_t          tmpint;
    struct pingResultsTable_data *StorageTmp;
    struct header_complex_index *hcindex;


    DEBUGMSGTL(("pingResultsTable", "storing data...  "));


    for (hcindex = pingResultsTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct pingResultsTable_data *) hcindex->data;

        if (StorageTmp->storageType != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "pingResultsTable ");
            cptr = line + strlen(line);


            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlOwnerIndex,
                                       &StorageTmp->pingCtlOwnerIndexLen);

            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlTestName,
                                       &StorageTmp->pingCtlTestNameLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingResultsOperStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       pingResultsIpTargetAddressType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       pingResultsIpTargetAddress,
                                       &StorageTmp->
                                       pingResultsIpTargetAddressLen);

            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingResultsMinRtt,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingResultsMaxRtt,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingResultsAverageRtt,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       pingResultsProbeResponses, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingResultsSendProbes,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       pingResultsRttSumOfSquares,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       pingResultsLastGoodProbe,
                                       &StorageTmp->
                                       pingResultsLastGoodProbeLen);

            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("pingResultsTable", "done.\n"));
    return SNMPERR_SUCCESS;
}

int
pingResultsTable_inadd(struct pingResultsTable_data *thedata)
{
    netsnmp_variable_list *vars_list;
    vars_list = NULL;


    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->pingCtlOwnerIndex, thedata->pingCtlOwnerIndexLen);  /* pingCtlOwnerIndex */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->pingCtlTestName, thedata->pingCtlTestNameLen);      /* pingCtlTestName */

    /*
     * XXX: fill in default row values here into StorageNew 
     * 
     */


    DEBUGMSGTL(("pingResultsTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */

    header_complex_add_data(&pingResultsTableStorage, vars_list, thedata);
    DEBUGMSGTL(("pingResultsTable", "registered an entry\n"));


    DEBUGMSGTL(("pingResultsTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


/*
 * var_pingResultsTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_pingResultsTable above.
 */
unsigned char  *
var_pingResultsTable(struct variable *vp,
                     oid * name,
                     size_t *length,
                     int exact,
                     size_t *var_len, WriteMethod ** write_method)
{


    struct pingResultsTable_data *StorageTmp = NULL;

    *write_method = NULL;
    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(pingResultsTableStorage, vp, name, length, exact,
                        var_len, write_method)) == NULL) {

        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */

    switch (vp->magic) {

    case COLUMN_PINGRESULTSOPERSTATUS:
        *var_len = sizeof(StorageTmp->pingResultsOperStatus);
        return (u_char *) & StorageTmp->pingResultsOperStatus;

    case COLUMN_PINGRESULTSIPTARGETADDRESSTYPE:
        *var_len = sizeof(StorageTmp->pingResultsIpTargetAddressType);
        return (u_char *) & StorageTmp->pingResultsIpTargetAddressType;

    case COLUMN_PINGRESULTSIPTARGETADDRESS:
        *var_len = (StorageTmp->pingResultsIpTargetAddressLen);
        return (u_char *) StorageTmp->pingResultsIpTargetAddress;

    case COLUMN_PINGRESULTSMINRTT:
        *var_len = sizeof(StorageTmp->pingResultsMinRtt);
        return (u_char *) & StorageTmp->pingResultsMinRtt;

    case COLUMN_PINGRESULTSMAXRTT:
        *var_len = sizeof(StorageTmp->pingResultsMaxRtt);
        return (u_char *) & StorageTmp->pingResultsMaxRtt;

    case COLUMN_PINGRESULTSAVERAGERTT:
        *var_len = sizeof(StorageTmp->pingResultsAverageRtt);
        return (u_char *) & StorageTmp->pingResultsAverageRtt;

    case COLUMN_PINGRESULTSPROBERESPONSES:
        *var_len = sizeof(StorageTmp->pingResultsProbeResponses);
        return (u_char *) & StorageTmp->pingResultsProbeResponses;

    case COLUMN_PINGRESULTSSENTPROBES:
        *var_len = sizeof(StorageTmp->pingResultsSendProbes);
        return (u_char *) & StorageTmp->pingResultsSendProbes;

    case COLUMN_PINGRESULTSRTTSUMOFSQUARES:
        *var_len = sizeof(StorageTmp->pingResultsRttSumOfSquares);
        return (u_char *) & StorageTmp->pingResultsRttSumOfSquares;

    case COLUMN_PINGRESULTSLASTGOODPROBE:
        *var_len = (StorageTmp->pingResultsLastGoodProbeLen);
        return (u_char *) StorageTmp->pingResultsLastGoodProbe;

    default:
        ERROR_MSG("");
    }

    return NULL;
}
