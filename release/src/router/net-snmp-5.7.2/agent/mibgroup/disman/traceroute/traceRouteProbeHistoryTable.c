/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:traceRouteProbeHistoryTable.c
 *File Description:Rows of traceRouteProbeHistoryTable MIB read.
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


oid             traceRouteProbeHistoryTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 81, 1, 4 };

struct variable2 traceRouteProbeHistoryTable_variables[] = {
    {COLUMN_TRACEROUTEPROBEHISTORYHADDRTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_traceRouteProbeHistoryTable, 2, {1, 4}},
    {COLUMN_TRACEROUTEPROBEHISTORYHADDR,   ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_traceRouteProbeHistoryTable, 2, {1, 5}},
    {COLUMN_TRACEROUTEPROBEHISTORYRESPONSE, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_traceRouteProbeHistoryTable, 2, {1, 6}},
    {COLUMN_TRACEROUTEPROBEHISTORYSTATUS,    ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_traceRouteProbeHistoryTable, 2, {1, 7}},
    {COLUMN_TRACEROUTEPROBEHISTORYLASTRC,    ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_traceRouteProbeHistoryTable, 2, {1, 8}},
    {COLUMN_TRACEROUTEPROBEHISTORYTIME,    ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_traceRouteProbeHistoryTable, 2, {1, 9}}
};


/*
 * global storage of our data, saved in and configured by header_complex() 
 */

extern struct header_complex_index *traceRouteCtlTableStorage;
extern struct header_complex_index *traceRouteProbeHistoryTableStorage;
void
traceRouteProbeHistoryTable_inadd(struct traceRouteProbeHistoryTable_data
                                  *thedata);

void
traceRouteProbeHistoryTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr = NULL;
    struct traceRouteProbeHistoryTable_data *StorageDel = NULL;
    DEBUGMSGTL(("traceRouteProbeHistoryTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        StorageDel =
            header_complex_extract_entry
            (&traceRouteProbeHistoryTableStorage, hciptr);
        if (StorageDel != NULL) {
            free(StorageDel->traceRouteCtlOwnerIndex);
            StorageDel->traceRouteCtlOwnerIndex = NULL;
            free(StorageDel->traceRouteCtlTestName);
            StorageDel->traceRouteCtlTestName = NULL;
            free(StorageDel->traceRouteProbeHistoryHAddr);
            StorageDel->traceRouteProbeHistoryHAddr = NULL;
            free(StorageDel->traceRouteProbeHistoryTime);
            StorageDel->traceRouteProbeHistoryTime = NULL;
            free(StorageDel);
            StorageDel = NULL;
        }
        DEBUGMSGTL(("traceRouteProbeHistoryTable", "cleaner  "));
    }

}
void
init_traceRouteProbeHistoryTable(void)
{

    DEBUGMSGTL(("traceRouteProbeHistoryTable", "initializing...  "));


    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("traceRouteProbeHistoryTable",
                 traceRouteProbeHistoryTable_variables, variable2,
                 traceRouteProbeHistoryTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("traceRouteProbeHistoryTable",
                                  parse_traceRouteProbeHistoryTable, NULL,
                                  NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_traceRouteProbeHistoryTable, NULL);

    DEBUGMSGTL(("traceRouteProbeHistoryTable", "done.\n"));
}

/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */

void
parse_traceRouteProbeHistoryTable(const char *token, char *line)
{
    size_t          tmpint;
    struct traceRouteProbeHistoryTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(traceRouteProbeHistoryTable_data);

    DEBUGMSGTL(("traceRouteProbeHistoryTable", "parsing config...  "));


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
                              &StorageTmp->traceRouteProbeHistoryIndex,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteProbeHistoryHopIndex,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->
                              traceRouteProbeHistoryProbeIndex, &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteProbeHistoryHAddrType,
                              &tmpint);
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteProbeHistoryHAddr,
                              &StorageTmp->traceRouteProbeHistoryHAddrLen);
    if (StorageTmp->traceRouteProbeHistoryHAddr == NULL) {
        config_perror
            ("invalid specification for traceRouteProbeHistoryHAddr");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteProbeHistoryResponse,
                              &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteProbeHistoryStatus,
                              &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteProbeHistoryLastRC,
                              &tmpint);
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteProbeHistoryTime,
                              &StorageTmp->traceRouteProbeHistoryTimeLen);
    if (StorageTmp->traceRouteProbeHistoryTime == NULL) {
        config_perror
            ("invalid specification for traceRouteProbeHistoryTime");
        return;
    }


    traceRouteProbeHistoryTable_inadd(StorageTmp);

    /* traceRouteProbeHistoryTable_cleaner(traceRouteProbeHistoryTableStorage); */

    DEBUGMSGTL(("traceRouteProbeHistoryTable", "done.\n"));
}





/*
 * store_traceRouteProbeHistoryTable():
 *   stores .conf file entries needed to configure the mib.
 */

int
store_traceRouteProbeHistoryTable(int majorID, int minorID,
                                  void *serverarg, void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr = NULL;
    size_t          tmpint;
    struct traceRouteProbeHistoryTable_data *StorageTmp = NULL;
    struct header_complex_index *hcindex = NULL;


    DEBUGMSGTL(("traceRouteProbeHistoryTable", "storing data...  "));


    for (hcindex = traceRouteProbeHistoryTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp =
            (struct traceRouteProbeHistoryTable_data *) hcindex->data;

        if (StorageTmp->storageType != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "traceRouteProbeHistoryTable ");
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
                                       &StorageTmp->
                                       traceRouteProbeHistoryIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryHopIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryProbeIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryHAddrType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryHAddr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryHAddrLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryResponse,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryLastRC,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryTime,
                                       &StorageTmp->
                                       traceRouteProbeHistoryTimeLen);

            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("traceRouteProbeHistoryTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


void
traceRouteProbeHistoryTable_inadd(struct traceRouteProbeHistoryTable_data
                                  *thedata)
{
    netsnmp_variable_list *vars_list = NULL;


    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);      /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);  /* traceRouteCtlTestName */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteProbeHistoryIndex, sizeof(thedata->traceRouteProbeHistoryIndex)); /* traceRouteProbeHistoryIndex */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteProbeHistoryHopIndex, sizeof(thedata->traceRouteProbeHistoryHopIndex));   /* traceRouteProbeHistoryHopIndex */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteProbeHistoryProbeIndex, sizeof(thedata->traceRouteProbeHistoryProbeIndex));       /* traceRouteProbeHistoryProbeIndex */


    /*
     * XXX: fill in default row values here into StorageNew 
     * 
     */


    DEBUGMSGTL(("traceRouteProbeHistoryTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */

    header_complex_add_data(&traceRouteProbeHistoryTableStorage, vars_list,
                            thedata);
    DEBUGMSGTL(("traceRouteProbeHistoryTable", "registered an entry\n"));


    DEBUGMSGTL(("traceRouteProbeHistoryTable", "done.\n"));
}


/*
 * var_traceRouteProbeHistoryTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_traceRouteProbeHistoryTable above.
 */
unsigned char  *
var_traceRouteProbeHistoryTable(struct variable *vp,
                                oid * name,
                                size_t *length,
                                int exact,
                                size_t *var_len,
                                WriteMethod ** write_method)
{


    struct traceRouteProbeHistoryTable_data *StorageTmp = NULL;

    *write_method = NULL;

    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(traceRouteProbeHistoryTableStorage, vp, name,
                        length, exact, var_len, write_method)) == NULL) {
        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */

    switch (vp->magic) {

    case COLUMN_TRACEROUTEPROBEHISTORYHADDRTYPE:
        *var_len = sizeof(StorageTmp->traceRouteProbeHistoryHAddrType);
        return (u_char *) & StorageTmp->traceRouteProbeHistoryHAddrType;

    case COLUMN_TRACEROUTEPROBEHISTORYHADDR:
        *var_len = (StorageTmp->traceRouteProbeHistoryHAddrLen);
        return (u_char *) StorageTmp->traceRouteProbeHistoryHAddr;

    case COLUMN_TRACEROUTEPROBEHISTORYRESPONSE:
        *var_len = sizeof(StorageTmp->traceRouteProbeHistoryResponse);
        return (u_char *) & StorageTmp->traceRouteProbeHistoryResponse;

    case COLUMN_TRACEROUTEPROBEHISTORYSTATUS:
        *var_len = sizeof(StorageTmp->traceRouteProbeHistoryStatus);
        return (u_char *) & StorageTmp->traceRouteProbeHistoryStatus;

    case COLUMN_TRACEROUTEPROBEHISTORYLASTRC:
        *var_len = sizeof(StorageTmp->traceRouteProbeHistoryLastRC);
        return (u_char *) & StorageTmp->traceRouteProbeHistoryLastRC;

    case COLUMN_TRACEROUTEPROBEHISTORYTIME:
        *var_len = (StorageTmp->traceRouteProbeHistoryTimeLen);
        return (u_char *) StorageTmp->traceRouteProbeHistoryTime;

    default:
        ERROR_MSG("");
    }

    return NULL;
}
