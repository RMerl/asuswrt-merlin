/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:traceRouteHopsTable.c
 *File Description:Rows of traceRouteHopsTable MIB read.
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

#include <net-snmp/net-snmp-config.h>
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

#include "traceRouteCtlTable.h"
#include "traceRouteResultsTable.h"
#include "traceRouteProbeHistoryTable.h"
#include "traceRouteHopsTable.h"

#include "header_complex.h"


oid             traceRouteHopsTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 81, 1, 5 };

struct variable2 traceRouteHopsTable_variables[] = {
    {COLUMN_TRACEROUTEHOPSIPTGTADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 2}},
    {COLUMN_TRACEROUTEHOPSIPTGTADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 3}},
    {COLUMN_TRACEROUTEHOPSMINRTT,          ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 4}},
    {COLUMN_TRACEROUTEHOPSMAXRTT,          ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 5}},
    {COLUMN_TRACEROUTEHOPSAVERAGERTT,      ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 6}},
    {COLUMN_TRACEROUTEHOPSRTTSUMOFSQUARES, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 7}},
    {COLUMN_TRACEROUTEHOPSSENTPROBES,      ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 8}},
    {COLUMN_TRACEROUTEHOPSPROBERESPONSES,  ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 9}},
    {COLUMN_TRACEROUTEHOPSLASTGOODPROBE,  ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_traceRouteHopsTable, 2, {1, 10}}
};




/*
 * global storage of our data, saved in and configured by header_complex() 
 */

extern struct header_complex_index *traceRouteCtlTableStorage;
extern struct header_complex_index *traceRouteHopsTableStorage;
void
traceRouteHopsTable_inadd(struct traceRouteHopsTable_data *thedata);

void
traceRouteHopsTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr = NULL;
    struct traceRouteHopsTable_data *StorageDel = NULL;
    DEBUGMSGTL(("traceRouteHopsTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        StorageDel =
            header_complex_extract_entry(&traceRouteHopsTableStorage,
                                         hciptr);
        if (StorageDel != NULL) {
            free(StorageDel->traceRouteCtlOwnerIndex);
            StorageDel->traceRouteCtlOwnerIndex = NULL;
            free(StorageDel->traceRouteCtlTestName);
            StorageDel->traceRouteCtlTestName = NULL;
            free(StorageDel->traceRouteHopsLastGoodProbe);
            StorageDel->traceRouteHopsLastGoodProbe = NULL;
            free(StorageDel);
            StorageDel = NULL;
        }
        DEBUGMSGTL(("traceRouteHopsTable", "cleaner  "));
    }

}
void
init_traceRouteHopsTable(void)
{

    DEBUGMSGTL(("traceRouteHopsTable", "initializing...  "));


    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("traceRouteHopsTable", traceRouteHopsTable_variables,
                 variable2, traceRouteHopsTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("traceRouteHopsTable",
                                  parse_traceRouteHopsTable, NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_traceRouteHopsTable, NULL);

    DEBUGMSGTL(("traceRouteHopsTable", "done.\n"));
}

/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */

void
parse_traceRouteHopsTable(const char *token, char *line)
{
    size_t          tmpint;
    struct traceRouteHopsTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(traceRouteHopsTable_data);

    DEBUGMSGTL(("traceRouteHopsTable", "parsing config...  "));


    if (StorageTmp == NULL) {
        config_perror("malloc failure");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlOwnerIndex,
                              &StorageTmp->traceRouteCtlOwnerIndexLen);
    if (StorageTmp->traceRouteCtlOwnerIndex == NULL) {
        config_perror("invalid specification for traceRouteCtlOwnerIndex");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlTestName,
                              &StorageTmp->traceRouteCtlTestNameLen);
    if (StorageTmp->traceRouteCtlTestName == NULL) {
        config_perror("invalid specification for traceRouteCtlTestName");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsHopIndex,
                              &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteHopsIpTgtAddressType,
                              &tmpint);
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteHopsIpTgtAddress,
                              &StorageTmp->traceRouteHopsIpTgtAddressLen);
    if (StorageTmp->traceRouteHopsIpTgtAddress == NULL) {
        config_perror
            ("invalid specification for traceRouteHopsIpTgtAddress");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsMinRtt, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsMaxRtt, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsAverageRtt,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsRttSumOfSquares,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsSentProbes,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteHopsProbeResponses,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteHopsLastGoodProbe,
                              &StorageTmp->traceRouteHopsLastGoodProbeLen);
    if (StorageTmp->traceRouteHopsLastGoodProbe == NULL) {
        config_perror
            ("invalid specification for traceRouteHopsLastGoodProbe");
        return;
    }

    traceRouteHopsTable_inadd(StorageTmp);

    /* traceRouteHopsTable_cleaner(traceRouteHopsTableStorage); */

    DEBUGMSGTL(("traceRouteHopsTable", "done.\n"));
}





/*
 * store_traceRouteHopsTable():
 *   stores .conf file entries needed to configure the mib.
 */

int
store_traceRouteHopsTable(int majorID, int minorID, void *serverarg,
                          void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr = NULL;
    size_t          tmpint;
    struct traceRouteHopsTable_data *StorageTmp = NULL;
    struct header_complex_index *hcindex = NULL;


    DEBUGMSGTL(("traceRouteHopsTable", "storing data...  "));


    for (hcindex = traceRouteHopsTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct traceRouteHopsTable_data *) hcindex->data;

        if (StorageTmp->storageType != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "traceRouteHopsTable ");
            cptr = line + strlen(line);

            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteCtlOwnerIndex,
                                       &StorageTmp->
                                       traceRouteCtlOwnerIndexLen);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->traceRouteCtlTestName,
                                       &StorageTmp->
                                       traceRouteCtlTestNameLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteHopsHopIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteHopsIpTgtAddressType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteHopsIpTgtAddress,
                                       &StorageTmp->
                                       traceRouteHopsIpTgtAddressLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteHopsMinRtt,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteHopsMaxRtt,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteHopsAverageRtt, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteHopsRttSumOfSquares,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteHopsSentProbes, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteHopsProbeResponses,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteHopsLastGoodProbe,
                                       &StorageTmp->
                                       traceRouteHopsLastGoodProbeLen);

            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("traceRouteHopsTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


void
traceRouteHopsTable_inadd(struct traceRouteHopsTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;

    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);      /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);  /* traceRouteCtlTestName */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteHopsHopIndex, sizeof(thedata->traceRouteHopsHopIndex));   /* traceRouteHopsHopIndex */

    DEBUGMSGTL(("traceRouteHopsTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */

    header_complex_add_data(&traceRouteHopsTableStorage, vars_list,
                            thedata);
    DEBUGMSGTL(("traceRouteHopsTable", "registered an entry\n"));


    DEBUGMSGTL(("traceRouteHopsTable", "done.\n"));
}


/*
 * var_traceRouteHopsTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_traceRouteHopsTable above.
 */
unsigned char  *
var_traceRouteHopsTable(struct variable *vp,
                        oid * name,
                        size_t *length,
                        int exact,
                        size_t *var_len, WriteMethod ** write_method)
{


    struct traceRouteHopsTable_data *StorageTmp = NULL;

    *write_method = NULL;
    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(traceRouteHopsTableStorage, vp, name, length,
                        exact, var_len, write_method)) == NULL) {
        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */

    switch (vp->magic) {

    case COLUMN_TRACEROUTEHOPSIPTGTADDRESSTYPE:
        *var_len = sizeof(StorageTmp->traceRouteHopsIpTgtAddressType);
        return (u_char *) & StorageTmp->traceRouteHopsIpTgtAddressType;

    case COLUMN_TRACEROUTEHOPSIPTGTADDRESS:
        *var_len = (StorageTmp->traceRouteHopsIpTgtAddressLen);
        return (u_char *) StorageTmp->traceRouteHopsIpTgtAddress;

    case COLUMN_TRACEROUTEHOPSMINRTT:
        *var_len = sizeof(StorageTmp->traceRouteHopsMinRtt);
        return (u_char *) & StorageTmp->traceRouteHopsMinRtt;

    case COLUMN_TRACEROUTEHOPSMAXRTT:
        *var_len = sizeof(StorageTmp->traceRouteHopsMaxRtt);
        return (u_char *) & StorageTmp->traceRouteHopsMaxRtt;

    case COLUMN_TRACEROUTEHOPSAVERAGERTT:
        *var_len = sizeof(StorageTmp->traceRouteHopsAverageRtt);
        return (u_char *) & StorageTmp->traceRouteHopsAverageRtt;

    case COLUMN_TRACEROUTEHOPSRTTSUMOFSQUARES:
        *var_len = sizeof(StorageTmp->traceRouteHopsRttSumOfSquares);
        return (u_char *) & StorageTmp->traceRouteHopsRttSumOfSquares;

    case COLUMN_TRACEROUTEHOPSSENTPROBES:
        *var_len = sizeof(StorageTmp->traceRouteHopsSentProbes);
        return (u_char *) & StorageTmp->traceRouteHopsSentProbes;

    case COLUMN_TRACEROUTEHOPSPROBERESPONSES:
        *var_len = sizeof(StorageTmp->traceRouteHopsProbeResponses);
        return (u_char *) & StorageTmp->traceRouteHopsProbeResponses;

    case COLUMN_TRACEROUTEHOPSLASTGOODPROBE:
        *var_len = (StorageTmp->traceRouteHopsLastGoodProbeLen);
        return (u_char *) StorageTmp->traceRouteHopsLastGoodProbe;

    default:
        ERROR_MSG("");
    }

    return NULL;
}
