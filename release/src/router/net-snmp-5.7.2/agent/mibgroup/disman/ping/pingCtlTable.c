/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:pingCtlTable.c
 *File Description:Rows of the pingCtlTable MIB add , delete and read.Rows of lookupResultsTable
 *              MIB add and delete.Rows of pingProbeHistoryTable MIB add and delete. 
 *              The main function is also here.
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

/*
 * This should always be included first before anything else 
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

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

#include <netdb.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "pingCtlTable.h"
#include "pingResultsTable.h"
#include "pingProbeHistoryTable.h"
#include "header_complex.h"

static inline void tvsub(struct timeval *, struct timeval *);
static inline int schedule_exit(int, int *, long *, long *, long *, long *);
static inline int in_flight(__u16 *, long *, long *, long *);
static inline void acknowledge(__u16, __u16 *, long *, int *);
static inline void advance_ntransmitted(__u16 *, long *);
static inline void update_interval(int, int, int *, int *);
static long     llsqrt(long long);
static __inline__ int ipv6_addr_any(struct in6_addr *);
static char    *pr_addr(struct in6_addr *, int);
static char    *pr_addr_n(struct in6_addr *);
void pingCtlTable_cleaner(struct header_complex_index *thestuff);

/*
 *pingCtlTable_variables_oid:
 *                                                      
 */


oid             pingCtlTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 80, 1, 2 };
static const int pingCtlTable_variables_oid_len = sizeof(pingCtlTable_variables_oid)/sizeof(pingCtlTable_variables_oid[0]);

/* trap */
oid             pingProbeFailed[] = { 1, 3, 6, 1, 2, 1, 80, 0, 1 };
oid             pingTestFailed[] = { 1, 3, 6, 1, 2, 1, 80, 0, 2 };
oid             pingTestCompleted[] = { 1, 3, 6, 1, 2, 1, 80, 0, 3 };


struct variable2 pingCtlTable_variables[] = {
    /*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix 
     */
    {COLUMN_PINGCTLTARGETADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 3}},
    {COLUMN_PINGCTLTARGETADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 4}},
    {COLUMN_PINGCTLDATASIZE,         ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingCtlTable, 2, {1, 5}},
    {COLUMN_PINGCTLTIMEOUT,          ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingCtlTable, 2, {1, 6}},
    {COLUMN_PINGCTLPROBECOUNT,       ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingCtlTable, 2, {1, 7}},
    {COLUMN_PINGCTLADMINSTATUS,       ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 8}},
    {COLUMN_PINGCTLDATAFILL,        ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 9}},
    {COLUMN_PINGCTLFREQUENCY,        ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 10}},
    {COLUMN_PINGCTLMAXROWS,          ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 11}},
    {COLUMN_PINGCTLSTORAGETYPE,       ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 12}},
    {COLUMN_PINGCTLTRAPGENERATION,  ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 13}},
    {COLUMN_PINGCTLTRAPPROBEFAILUREFILTER, ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 14}},
    {COLUMN_PINGCTLTRAPTESTFAILUREFILTER,  ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 15}},
    {COLUMN_PINGCTLTYPE,            ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 16}},
    {COLUMN_PINGCTLDESCR,           ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 17}},
    {COLUMN_PINGCTLSOURCEADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 18}},
    {COLUMN_PINGCTLSOURCEADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 19}},
    {COLUMN_PINGCTLIFINDEX,           ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 20}},
    {COLUMN_PINGCTLBYPASSROUTETABLE,  ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 21}},
    {COLUMN_PINGCTLDSFIELD,          ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 22}},
    {COLUMN_PINGCTLROWSTATUS,         ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_pingCtlTable, 2, {1, 23}}
};




/*
 * global storage of our data, saved in and configured by header_complex() 
 */


struct header_complex_index *pingCtlTableStorage = NULL;
struct header_complex_index *pingResultsTableStorage = NULL;
struct header_complex_index *pingProbeHistoryTableStorage = NULL;

void
init_pingCtlTable(void)
{
    DEBUGMSGTL(("pingCtlTable", "initializing...  "));
    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("pingCtlTable", pingCtlTable_variables, variable2,
                 pingCtlTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("pingCtlTable", parse_pingCtlTable,
                                  NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_pingCtlTable, NULL);

    DEBUGMSGTL(("pingCtlTable", "done.\n"));
}

void shutdown_pingCtlTable(void)
{
    snmp_unregister_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                             store_pingCtlTable, NULL, 1);
    snmpd_unregister_config_handler("pingCtlTable");
    unregister_mib(pingCtlTable_variables_oid, pingCtlTable_variables_oid_len);
    pingCtlTable_cleaner(pingCtlTableStorage);
    pingCtlTableStorage = NULL;
}

struct pingCtlTable_data *
create_pingCtlTable_data(void)
{
    struct pingCtlTable_data *StorageNew = NULL;

    StorageNew = SNMP_MALLOC_STRUCT(pingCtlTable_data);
    if (StorageNew == NULL)
        return NULL;
    StorageNew->pingCtlTargetAddressType = 1;
    StorageNew->pingCtlTargetAddress = strdup("");
    StorageNew->pingCtlTargetAddressLen = 0;
    StorageNew->pingCtlDataSize = 0;
    StorageNew->pingCtlTimeOut = 3;
    StorageNew->pingCtlProbeCount = 1;
    StorageNew->pingCtlAdminStatus = 2;
    StorageNew->pingCtlDataFill = strdup("00");
    if (StorageNew->pingCtlDataFill == NULL) {
        free(StorageNew);
        return NULL;
    }
    StorageNew->pingCtlDataFillLen = strlen(StorageNew->pingCtlDataFill);
    StorageNew->pingCtlFrequency = 0;
    StorageNew->pingCtlMaxRows = 50;
    StorageNew->pingCtlStorageType = 1;
    StorageNew->pingCtlTrapGeneration = strdup("");
    StorageNew->pingCtlTrapGenerationLen = 0;
    StorageNew->pingCtlTrapProbeFailureFilter = 1;
    StorageNew->pingCtlTrapTestFailureFilter = 1;
    StorageNew->pingCtlType = calloc(1, sizeof(oid) * sizeof(2));       /* 0.0 */
    StorageNew->pingCtlTypeLen = 2;
    StorageNew->pingCtlDescr = strdup("");
    StorageNew->pingCtlDescrLen = 0;
    StorageNew->pingCtlSourceAddressType = 1;
    StorageNew->pingCtlSourceAddress = strdup("");
    StorageNew->pingCtlSourceAddressLen = 0;
    StorageNew->pingCtlIfIndex = 0;
    StorageNew->pingCtlByPassRouteTable = 2;
    StorageNew->pingCtlDSField = 0;
    StorageNew->pingResults = NULL;
    StorageNew->pingProbeHis = NULL;

    StorageNew->storageType = ST_NONVOLATILE;
    StorageNew->pingProbeHistoryMaxIndex = 0;
    return StorageNew;
}

static void free_pingCtlTable_data(struct pingCtlTable_data *StorageDel)
{
    netsnmp_assert(StorageDel);
    free(StorageDel->pingCtlOwnerIndex);
    free(StorageDel->pingCtlTestName);
    free(StorageDel->pingCtlTargetAddress);
    free(StorageDel->pingCtlDataFill);
    free(StorageDel->pingCtlTrapGeneration);
    free(StorageDel->pingCtlType);
    free(StorageDel->pingCtlDescr);
    free(StorageDel->pingCtlSourceAddress);
    free(StorageDel);
}

/*
 * pingCtlTable_add(): adds a structure node to our data set 
 */
int
pingCtlTable_add(struct pingCtlTable_data *thedata)
{
    netsnmp_variable_list *vars = NULL;


    DEBUGMSGTL(("pingCtlTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */


    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlOwnerIndex,
                              thedata->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlTestName,
                              thedata->pingCtlTestNameLen);
    header_complex_add_data(&pingCtlTableStorage, vars, thedata);

    DEBUGMSGTL(("pingCtlTable", "registered an entry\n"));

    return SNMPERR_SUCCESS;
}

int
pingResultsTable_add(struct pingCtlTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;
    struct pingResultsTable_data *p;

    p = thedata->pingResults;
    if (thedata->pingResults != NULL) {

        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                                  p->pingCtlOwnerIndex,
                                  p->pingCtlOwnerIndexLen);
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                                  p->pingCtlTestName, p->pingCtlTestNameLen);

        /*
         * XXX: fill in default row values here into StorageNew 
         * 
         */


        DEBUGMSGTL(("pingResultsTable", "adding data...  "));
        /*
         * add the index variables to the varbind list, which is 
         * used by header_complex to index the data 
         */

        header_complex_add_data(&pingResultsTableStorage, vars_list, p);

        DEBUGMSGTL(("pingResultsTable", "out finished\n"));

    }

    return SNMPERR_SUCCESS;
}


int
pingProbeHistoryTable_add(struct pingProbeHistoryTable_data *thedata)
{
    netsnmp_variable_list *vars_list;

    vars_list = NULL;
    if (thedata != NULL) {
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                                  thedata->pingCtlOwnerIndex,
                                  thedata->pingCtlOwnerIndexLen);
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                                  thedata->pingCtlTestName,
                                  thedata->pingCtlTestNameLen);
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED,
                                  &thedata->pingProbeHistoryIndex,
                                  sizeof(thedata->pingProbeHistoryIndex));

        /*
         * XXX: fill in default row values here into StorageNew 
         * 
         */


        DEBUGMSGTL(("pingProbeHistoryTable", "adding data...  "));
        /*
         * add the index variables to the varbind list, which is 
         * used by header_complex to index the data 
         */

        header_complex_add_data(&pingProbeHistoryTableStorage, vars_list,
                                thedata);
        DEBUGMSGTL(("pingProbeHistoryTable", "out finished\n"));
    }
    
    return SNMPERR_SUCCESS;
}

int
pingProbeHistoryTable_addall(struct pingCtlTable_data *thedata)
{
    netsnmp_variable_list *vars_list;
    struct pingProbeHistoryTable_data *p;

    for (p = thedata->pingProbeHis; p; p = p->next) {
        vars_list = NULL;
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                                  p->pingCtlOwnerIndex,
                                  p->pingCtlOwnerIndexLen);
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                                  p->pingCtlTestName,
                                  p->pingCtlTestNameLen);
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED,
                                  &p->pingProbeHistoryIndex,
                                  sizeof(p->pingProbeHistoryIndex));
        header_complex_add_data(&pingProbeHistoryTableStorage, vars_list, p);
    }

    return SNMPERR_SUCCESS;
}

void
pingCtlTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr, *next;
    struct pingCtlTable_data *StorageDel;

    DEBUGMSGTL(("pingProbeHistoryTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr; hciptr = next) {
        next = hciptr->next;
        StorageDel = header_complex_extract_entry(&pingCtlTableStorage, hciptr);
        free_pingCtlTable_data(StorageDel);
        DEBUGMSGTL(("pingProbeHistoryTable", "cleaner  "));
    }
}

/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */
void
parse_pingCtlTable(const char *token, char *line)
{
    size_t          tmpint;
    struct pingCtlTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(pingCtlTable_data);

    DEBUGMSGTL(("pingCtlTable", "parsing config...  "));


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
                              &StorageTmp->pingCtlTargetAddressType,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlTargetAddress,
                              &StorageTmp->pingCtlTargetAddressLen);
    if (StorageTmp->pingCtlTargetAddress == NULL) {
        config_perror("invalid specification for pingCtlTargetAddress");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlDataSize, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlTimeOut, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlProbeCount, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingCtlAdminStatus, &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlDataFill,
                              &StorageTmp->pingCtlDataFillLen);
    if (StorageTmp->pingCtlDataFill == NULL) {
        config_perror("invalid specification for pingCtlDataFill");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlFrequency, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlMaxRows, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingCtlStorageType, &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlTrapGeneration,
                              &StorageTmp->pingCtlTrapGenerationLen);
    if (StorageTmp->pingCtlTrapGeneration == NULL) {
        config_perror("invalid specification for pingCtlTrapGeneration");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlTrapProbeFailureFilter,
                              &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlTrapTestFailureFilter,
                              &tmpint);

    line =
        read_config_read_data(ASN_OBJECT_ID, line,
                              &StorageTmp->pingCtlType,
                              &StorageTmp->pingCtlTypeLen);
    if (StorageTmp->pingCtlType == NULL) {
        config_perror("invalid specification for pingCtlType");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlDescr,
                              &StorageTmp->pingCtlDescrLen);
    if (StorageTmp->pingCtlDescr == NULL) {
        config_perror("invalid specification for pingCtlTrapDescr");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingCtlSourceAddressType,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingCtlSourceAddress,
                              &StorageTmp->pingCtlSourceAddressLen);
    if (StorageTmp->pingCtlSourceAddress == NULL) {
        config_perror("invalid specification for pingCtlSourceAddress");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingCtlIfIndex, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingCtlByPassRouteTable,
                              &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingCtlDSField, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingCtlRowStatus, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingProbeHistoryMaxIndex,
                              &tmpint);

    StorageTmp->storageType = ST_NONVOLATILE;
    pingCtlTable_add(StorageTmp);
    /* pingCtlTable_cleaner(pingCtlTableStorage); */

    DEBUGMSGTL(("pingCtlTable", "done.\n"));
}



/*
 * store_pingCtlTable():
 *   stores .conf file entries needed to configure the mib.
 */
int
store_pingCtlTable(int majorID, int minorID, void *serverarg,
                   void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr = NULL;
    size_t          tmpint;
    struct pingCtlTable_data *StorageTmp = NULL;
    struct header_complex_index *hcindex = NULL;


    DEBUGMSGTL(("pingCtlTable", "storing data...  "));


    for (hcindex = pingCtlTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct pingCtlTable_data *) hcindex->data;

        if (StorageTmp->storageType != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "pingCtlTable ");
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
                                       &StorageTmp->
                                       pingCtlTargetAddressType, &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlTargetAddress,
                                       &StorageTmp->
                                       pingCtlTargetAddressLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingCtlDataSize,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingCtlTimeOut,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingCtlProbeCount,
                                       &tmpint);

            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingCtlAdminStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlDataFill,
                                       &StorageTmp->pingCtlDataFillLen);

            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingCtlFrequency,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingCtlMaxRows,
                                       &tmpint);

            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingCtlStorageType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlTrapGeneration,
                                       &StorageTmp->
                                       pingCtlTrapGenerationLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       pingCtlTrapProbeFailureFilter,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       pingCtlTrapTestFailureFilter,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OBJECT_ID, cptr,
                                       &StorageTmp->pingCtlType,
                                       &StorageTmp->pingCtlTypeLen);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlDescr,
                                       &StorageTmp->pingCtlDescrLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       pingCtlSourceAddressType, &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingCtlSourceAddress,
                                       &StorageTmp->
                                       pingCtlSourceAddressLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingCtlIfIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       pingCtlByPassRouteTable, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingCtlDSField,
                                       &tmpint);

            if (StorageTmp->pingCtlRowStatus == RS_ACTIVE)
                StorageTmp->pingCtlRowStatus = RS_NOTINSERVICE;

            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingCtlRowStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       pingProbeHistoryMaxIndex, &tmpint);



            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("pingCtlTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


/*
 * var_pingCtlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_mteObjectsTable above.
 */
unsigned char  *
var_pingCtlTable(struct variable *vp,
                 oid * name,
                 size_t *length,
                 int exact, size_t *var_len, WriteMethod ** write_method)
{


    struct pingCtlTable_data *StorageTmp = NULL;

    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(pingCtlTableStorage, vp, name, length, exact,
                        var_len, write_method)) == NULL) {
        if (vp->magic == COLUMN_PINGCTLROWSTATUS)
            *write_method = write_pingCtlRowStatus;
        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */
    switch (vp->magic) {


    case COLUMN_PINGCTLTARGETADDRESSTYPE:
        *write_method = write_pingCtlTargetAddressType;
        *var_len = sizeof(StorageTmp->pingCtlTargetAddressType);
        return (u_char *) & StorageTmp->pingCtlTargetAddressType;

    case COLUMN_PINGCTLTARGETADDRESS:
        *write_method = write_pingCtlTargetAddress;
        *var_len = (StorageTmp->pingCtlTargetAddressLen);

        return (u_char *) StorageTmp->pingCtlTargetAddress;

    case COLUMN_PINGCTLDATASIZE:
        *write_method = write_pingCtlDataSize;
        *var_len = sizeof(StorageTmp->pingCtlDataSize);

        return (u_char *) & StorageTmp->pingCtlDataSize;

    case COLUMN_PINGCTLTIMEOUT:
        *write_method = write_pingCtlTimeOut;
        *var_len = sizeof(StorageTmp->pingCtlTimeOut);

        return (u_char *) & StorageTmp->pingCtlTimeOut;

    case COLUMN_PINGCTLPROBECOUNT:
        *write_method = write_pingCtlProbeCount;
        *var_len = sizeof(StorageTmp->pingCtlProbeCount);

        return (u_char *) & StorageTmp->pingCtlProbeCount;

    case COLUMN_PINGCTLADMINSTATUS:
        *write_method = write_pingCtlAdminStatus;
        *var_len = sizeof(StorageTmp->pingCtlAdminStatus);

        return (u_char *) & StorageTmp->pingCtlAdminStatus;

    case COLUMN_PINGCTLDATAFILL:
        *write_method = write_pingCtlDataFill;
        *var_len = (StorageTmp->pingCtlDataFillLen);

        return (u_char *) StorageTmp->pingCtlDataFill;

    case COLUMN_PINGCTLFREQUENCY:
        *write_method = write_pingCtlFrequency;
        *var_len = sizeof(StorageTmp->pingCtlFrequency);

        return (u_char *) & StorageTmp->pingCtlFrequency;

    case COLUMN_PINGCTLMAXROWS:
        *write_method = write_pingCtlMaxRows;
        *var_len = sizeof(StorageTmp->pingCtlMaxRows);

        return (u_char *) & StorageTmp->pingCtlMaxRows;

    case COLUMN_PINGCTLSTORAGETYPE:
        *write_method = write_pingCtlStorageType;
        *var_len = sizeof(StorageTmp->pingCtlStorageType);

        return (u_char *) & StorageTmp->pingCtlStorageType;

    case COLUMN_PINGCTLTRAPGENERATION:
        *write_method = write_pingCtlTrapGeneration;
        *var_len = (StorageTmp->pingCtlTrapGenerationLen);

        return (u_char *) StorageTmp->pingCtlTrapGeneration;

    case COLUMN_PINGCTLTRAPPROBEFAILUREFILTER:
        *write_method = write_pingCtlTrapProbeFailureFilter;
        *var_len = sizeof(StorageTmp->pingCtlTrapProbeFailureFilter);

        return (u_char *) & StorageTmp->pingCtlTrapProbeFailureFilter;

    case COLUMN_PINGCTLTRAPTESTFAILUREFILTER:
        *write_method = write_pingCtlTrapTestFailureFilter;
        *var_len = sizeof(StorageTmp->pingCtlTrapTestFailureFilter);

        return (u_char *) & StorageTmp->pingCtlTrapTestFailureFilter;

    case COLUMN_PINGCTLTYPE:
        *write_method = write_pingCtlType;
        *var_len = (StorageTmp->pingCtlTypeLen) * sizeof(oid);

        return (u_char *) StorageTmp->pingCtlType;

    case COLUMN_PINGCTLDESCR:
        *write_method = write_pingCtlDescr;
        *var_len = (StorageTmp->pingCtlDescrLen);

        return (u_char *) StorageTmp->pingCtlDescr;

    case COLUMN_PINGCTLSOURCEADDRESSTYPE:
        *write_method = write_pingCtlSourceAddressType;
        *var_len = sizeof(StorageTmp->pingCtlSourceAddressType);

        return (u_char *) & StorageTmp->pingCtlSourceAddressType;

    case COLUMN_PINGCTLSOURCEADDRESS:
        *write_method = write_pingCtlSourceAddress;
        *var_len = (StorageTmp->pingCtlSourceAddressLen);

        return (u_char *) StorageTmp->pingCtlSourceAddress;

    case COLUMN_PINGCTLIFINDEX:
        *write_method = write_pingCtlIfIndex;
        *var_len = sizeof(StorageTmp->pingCtlIfIndex);

        return (u_char *) & StorageTmp->pingCtlIfIndex;

    case COLUMN_PINGCTLBYPASSROUTETABLE:
        *write_method = write_pingCtlByPassRouteTable;
        *var_len = sizeof(StorageTmp->pingCtlByPassRouteTable);

        return (u_char *) & StorageTmp->pingCtlByPassRouteTable;

    case COLUMN_PINGCTLDSFIELD:
        *write_method = write_pingCtlDSField;
        *var_len = sizeof(StorageTmp->pingCtlDSField);

        return (u_char *) & StorageTmp->pingCtlDSField;



    case COLUMN_PINGCTLROWSTATUS:
        *write_method = write_pingCtlRowStatus;
        *var_len = sizeof(StorageTmp->pingCtlRowStatus);

        return (u_char *) & StorageTmp->pingCtlRowStatus;

    default:
        ERROR_MSG("");
    }
    return NULL;
}


unsigned long
pingProbeHistoryTable_count(struct pingCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;
    unsigned long   count = 0;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlOwnerIndex,
                              thedata->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlTestName,
                              thedata->pingCtlTestNameLen);

    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);
    snmp_free_varbind(vars);

    for (hciptr2 = pingProbeHistoryTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            count = count + 1;
        }
    }
    return count;
}


void
pingProbeHistoryTable_delLast(struct pingCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    struct header_complex_index *hcilast = NULL;
    struct pingProbeHistoryTable_data *StorageTmp, *StorageDel;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;
    time_t          last_time = 2147483647;
    time_t          tp;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlOwnerIndex,
                              thedata->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlTestName,
                              thedata->pingCtlTestNameLen);
    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);
    snmp_free_varbind(vars);

    for (hcilast = hciptr2 = pingProbeHistoryTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {

            StorageTmp =
                header_complex_get_from_oid(pingProbeHistoryTableStorage,
                                            hciptr2->name,
                                            hciptr2->namelen);
            tp = StorageTmp->pingProbeHistoryTime_time;

            if (last_time > tp) {
                last_time = tp;
                hcilast = hciptr2;
            }

        }
    }
    StorageDel =
        header_complex_extract_entry(&pingProbeHistoryTableStorage, hcilast);
    for (hciptr2 = pingCtlTableStorage; hciptr2; hciptr2 = hciptr2->next) {
        struct pingCtlTable_data *tmp;

        tmp = hciptr2->data;
        if (tmp->pingProbeHis == StorageDel) {
            tmp->pingProbeHis = tmp->pingProbeHis->next;
            DEBUGMSGTL(("pingProbeHistoryTable",
                        "deleting the last one succeeded!\n"));
            break;
    	}
    }
    if (StorageDel) {
        free(StorageDel->pingProbeHistoryTime);
        free(StorageDel->pingCtlTestName);
        free(StorageDel->pingCtlOwnerIndex);
        free(StorageDel);
    }
    DEBUGMSGTL(("pingProbeHistoryTable",
                "delete the last one success!\n"));
}


char           *
sock_ntop_host(const struct sockaddr *sa, socklen_t salen)
{
    static char     str[128];   /* Unix domain is largest */

    switch (sa->sa_family) {
    case AF_INET:{
            const struct sockaddr_in *sin = (const struct sockaddr_in *) sa;

            if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) ==
                NULL)
                return (NULL);
            return (str);
        }

#ifdef	IPV6
    case AF_INET6:{
            const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *) sa;

            if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) ==
                NULL)
                return (NULL);
            return (str);
        }
#endif

#ifdef	AF_UNIX
    case AF_UNIX:{
            const struct sockaddr_un *unp = (const struct sockaddr_un *) sa;

            /*
             * OK to have no pathname bound to the socket: happens on
             * every connect() unless client calls bind() first. 
             */
            if (unp->sun_path[0] == 0)
                strcpy(str, "(no pathname bound)");
            else
                snprintf(str, sizeof(str), "%s", unp->sun_path);
            return (str);
        }
#endif

#ifdef	HAVE_SOCKADDR_DL_STRUCT
    case AF_LINK:{
            struct sockaddr_dl *sdl = (struct sockaddr_dl *) sa;

            if (sdl->sdl_nlen > 0)
                snprintf(str, sizeof(str), "%*s",
                         sdl->sdl_nlen, &sdl->sdl_data[0]);
            else
                snprintf(str, sizeof(str), "AF_LINK, index=%d",
                         sdl->sdl_index);
            return (str);
        }
#endif
    default:
        snprintf(str, sizeof(str),
                 "sock_ntop_host: unknown AF_xxx: %d, len %d",
                 sa->sa_family, salen);
        return (str);
    }
    return (NULL);
}


char           *
Sock_ntop_host(const struct sockaddr *sa, socklen_t salen)
{
    char           *ptr;

    if ((ptr = sock_ntop_host(sa, salen)) == NULL) {
        /* inet_ntop() sets errno */
        snmp_log_perror("pingCtlTable: sock_ntop_host()");
    }
    return (ptr);
}



unsigned short
in_cksum(unsigned short *addr, int len)
{
    int             nleft = len;
    int             sum = 0;
    unsigned short *w = addr;
    unsigned short  answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /*
     * 4mop up an odd byte, if necessary 
     */
    if (nleft == 1) {
        *(unsigned char *) (&answer) = *(unsigned char *) w;
        sum += answer;
    }

    /*
     * 4add back carry outs from top 16 bits to low 16 bits 
     */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return (answer);
}


struct addrinfo *
host_serv(const char *host, const char *serv, int family, int socktype)
{
    int             n;
    struct addrinfo hints, *res;

    memset(&hints, '\0', sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME;      /* always return canonical name */
    hints.ai_family = family;   /* AF_UNSPEC, AF_INET, AF_INET6, etc. */
    hints.ai_socktype = socktype;       /* 0, SOCK_STREAM, SOCK_DGRAM, etc. */

    if ((n = netsnmp_getaddrinfo(host, serv, &hints, &res)) != 0)
        return (NULL);

    return (res);               /* return pointer to first on linked list */
}

/*
 * end host_serv 
 */

/*
 * There is no easy way to pass back the integer return code from
 * getaddrinfo() in the function above, short of adding another argument
 * that is a pointer, so the easiest way to provide the wrapper function
 * is just to duplicate the simple function as we do here.
 */

struct addrinfo *
Host_serv(const char *host, const char *serv, int family, int socktype)
{
    int             n;
    struct addrinfo hints, *res;

    memset(&hints, '\0', sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME;      /* always return canonical name */
    hints.ai_family = family;   /* 0, AF_INET, AF_INET6, etc. */
    hints.ai_socktype = socktype;       /* 0, SOCK_STREAM, SOCK_DGRAM, etc. */

    if ((n = netsnmp_getaddrinfo(host, serv, &hints, &res)) != 0) {
#if HAVE_GAI_STRERROR
        snmp_log(LOG_ERR, "host_serv error for %s, %s: %s",
                 (host == NULL) ? "(no hostname)" : host,
                 (serv == NULL) ? "(no service name)" : serv,
                 gai_strerror(n));
#else
        snmp_log(LOG_ERR, "host_serv error for %s, %s",
                 (host == NULL) ? "(no hostname)" : host,
                 (serv == NULL) ? "(no service name)" : serv);
#endif
    }

    return (res);               /* return pointer to first on linked list */
}

int
readable_timeo(int fd, int sec)
{
    fd_set          rset;
    struct timeval  tv;
    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    return (select(fd + 1, &rset, NULL, NULL, &tv));

}

/*
 * send trap 
 */
void
send_ping_trap(struct pingCtlTable_data *item,
               oid * trap_oid, size_t trap_oid_len)
{
    static oid      objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };     /* snmpTrapIOD.0 */
    struct pingResultsTable_data *StorageTmp = NULL;
    netsnmp_variable_list *var_list = NULL, *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;

    oid             pingCtlTargetAddress[] =
        { 1, 3, 6, 1, 2, 1, 80, 1, 2, 1, 4 };
    oid             pingResultsMinRtt[] =
        { 1, 3, 6, 1, 2, 1, 80, 1, 3, 1, 4 };
    oid             pingResultsMaxRtt[] =
        { 1, 3, 6, 1, 2, 1, 80, 1, 3, 1, 5 };
    oid             pingResultsAverageRtt[] =
        { 1, 3, 6, 1, 2, 1, 80, 1, 3, 1, 6 };
    oid             pingResultsProbeResponses[] =
        { 1, 3, 6, 1, 2, 1, 80, 1, 3, 1, 7 };
    oid             pingResultsSendProbes[] =
        { 1, 3, 6, 1, 2, 1, 80, 1, 3, 1, 8 };

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlOwnerIndex,
                              item->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlTestName,
                              item->pingCtlTestNameLen);
    StorageTmp = header_complex_get(pingResultsTableStorage, vars);
    snmp_free_varbind(vars);
    if (!StorageTmp)
        return;

    /*
     * snmpTrap oid 
     */
    snmp_varlist_add_variable(&var_list, objid_snmptrap,
                              sizeof(objid_snmptrap) / sizeof(oid),
                              ASN_OBJECT_ID, (u_char *) trap_oid,
                              trap_oid_len * sizeof(oid));
    /*
     * pingCtlTargetAddress 
     */
    memset(newoid, '\0', MAX_OID_LEN * sizeof(oid));
    header_complex_generate_oid(newoid, &newoid_len, pingCtlTargetAddress,
                                sizeof(pingCtlTargetAddress) / sizeof(oid),
                                vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_OCTET_STR,
                              (u_char *) item->pingCtlTargetAddress,
                              item->pingCtlTargetAddressLen);

    /*
     * pingResultsMinRtt
     */
    memset(newoid, '\0', newoid_len);
    header_complex_generate_oid(newoid, &newoid_len, pingResultsMinRtt,
                                sizeof(pingResultsMinRtt) / sizeof(oid),
                                vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_UNSIGNED,
                              (u_char *) & (StorageTmp->pingResultsMinRtt),
                              sizeof(StorageTmp->pingResultsMinRtt));
    /*
     * pingResultsMaxRtt 
     */
    memset(newoid, '\0', newoid_len);
    header_complex_generate_oid(newoid, &newoid_len, pingResultsMaxRtt,
                                sizeof(pingResultsMaxRtt) / sizeof(oid),
                                vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_UNSIGNED,
                              (u_char *) & (StorageTmp->pingResultsMaxRtt),
                              sizeof(StorageTmp->pingResultsMaxRtt));

    /*
     * pingResultsAverageRtt 
     */
    memset(newoid, '\0', newoid_len);
    header_complex_generate_oid(newoid, &newoid_len, pingResultsAverageRtt,
                                sizeof(pingResultsAverageRtt) /
                                sizeof(oid), vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_UNSIGNED,
                              (u_char *) & (StorageTmp->
                                            pingResultsAverageRtt),
                              sizeof(StorageTmp->pingResultsAverageRtt));

    /*
     * pingResultsProbeResponses 
     */
    memset(newoid, '\0', newoid_len);
    header_complex_generate_oid(newoid, &newoid_len,
                                pingResultsProbeResponses,
                                sizeof(pingResultsProbeResponses) /
                                sizeof(oid), vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_UNSIGNED,
                              (u_char *) & (StorageTmp->
                                            pingResultsProbeResponses),
                              sizeof(StorageTmp->
                                     pingResultsProbeResponses));
    /*
     * pingResultsSendProbes 
     */
    memset(newoid, '\0', newoid_len);
    header_complex_generate_oid(newoid, &newoid_len, pingResultsSendProbes,
                                sizeof(pingResultsSendProbes) /
                                sizeof(oid), vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_UNSIGNED,
                              (u_char *) & (StorageTmp->
                                            pingResultsSendProbes),
                              sizeof(StorageTmp->pingResultsSendProbes));

    /*
     * XXX: stuff based on event table 
     */

    DEBUGMSG(("pingTest:send_ping_trap", "success!\n"));

    send_v2trap(var_list);
    snmp_free_varbind(vars);
    vars = NULL;
    snmp_free_varbind(var_list);
    vars = NULL;
}



void
readloop(struct pingCtlTable_data *item, struct addrinfo *ai, int datalen,
         unsigned long *minrtt, unsigned long *maxrtt,
         unsigned long *averagertt, pid_t pid)
{
    char            recvbuf[BUFSIZE];
    char            sendbuf[BUFSIZE];
    int             nsent = 1;
    socklen_t       len;
    ssize_t         n;
    struct timeval  tval;
    /* static int                    loop_num; */
    /* struct pingProbeHistoryTable_data * current=NULL; */
    struct pingProbeHistoryTable_data current_var;
    int             sockfd;
    int             select_result;
    int             current_probe_temp;
    int             success_probe = 0;
    int             fail_probe = 0;
    int             flag;
    unsigned long   sumrtt;
    struct timeval  tv;

    memset(sendbuf, 0, sizeof(sendbuf));

    sockfd = socket(pr->sasend->sa_family, SOCK_RAW, pr->icmpproto);
    if (sockfd < 0) {
	snmp_log_perror("pingCtlTable: failed to create socket");
	return;
    }
    setuid(getuid());           /* don't need special permissions any more */

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (current_probe_temp = 1;
         current_probe_temp <= item->pingCtlProbeCount;
         current_probe_temp++) {
        time_t          timep;
        (*pr->fsend) (datalen, pid, nsent, sockfd, sendbuf);
        nsent++;
        len = pr->salen;
        select_result = readable_timeo(sockfd, item->pingCtlTimeOut);
        do {
            if (select_result == 0) {
                DEBUGMSGTL(("pingCtlTable", "socket timeout\n"));
                n = -1;
                fail_probe = fail_probe + 1;
                flag = 1;
            } else {
                n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, pr->sarecv,
                             &len);
                success_probe = success_probe + 1;
                flag = 0;
            }

            netsnmp_get_monotonic_clock(&tval);

            time(&timep);

            (*pr->fproc) (recvbuf, n, &tval, timep, item, ai, datalen, minrtt,
                          maxrtt, &sumrtt, averagertt, current_probe_temp,
                          success_probe, fail_probe, flag, &current_var, pid);
            select_result = readable_timeo(sockfd, 0);
        } while (select_result > 0);
    }
    close(sockfd);
}

unsigned long
round_double(double number)
{
    unsigned long   ret_num = 0;
    if (number - (unsigned long) number < 0.5)
        ret_num = (unsigned long) number;
    else
        ret_num = (unsigned long) number + 1;
    return ret_num;
}

int
proc_v4(char *ptr, ssize_t len, struct timeval *tvrecv, time_t timep,
        struct pingCtlTable_data *item, struct addrinfo *ai, int datalen,
        unsigned long *minrtt, unsigned long *maxrtt,
        unsigned long *sumrtt, unsigned long *averagertt,
        unsigned long current_probe, int success_probe, int fail_probe,
        int flag, struct pingProbeHistoryTable_data *current_temp,
        pid_t pid)
{
    int             hlen1 = 0, icmplen = 0;
    unsigned long   rtt = 0;

    struct ip      *ip = NULL;
    struct icmp    *icmp = NULL;
    struct timeval *tvsend = NULL;
    struct pingProbeHistoryTable_data *temp = NULL;
    static int      probeFailed = 0;
    static int      testFailed = 0;
    static int      series = 0;
    netsnmp_variable_list *vars = NULL;
    struct pingResultsTable_data *StorageNew = NULL;

    DEBUGMSGTL(("pingCtlTable", "proc_v4(flag = %d)\n", flag));

    if (flag == 0) {
        series = 0;
        ip = (struct ip *) ptr; /* start of IP header */
        hlen1 = ip->ip_hl << 2; /* length of IP header */

        icmp = (struct icmp *) (ptr + hlen1);   /* start of ICMP header */
        if ((icmplen = len - hlen1) < 8)
            DEBUGMSGTL(("pingCtlTable", "icmplen (%d) < 8", icmplen));

        DEBUGMSGTL(("pingCtlTable", "ICMP type = %d (%sa reply)\n",
                    icmp->icmp_type,
                    icmp->icmp_type == ICMP_ECHOREPLY ? "" : "not a "));

        if (icmp->icmp_type == ICMP_ECHOREPLY) {
            if (icmp->icmp_id != pid) {
                DEBUGMSGTL(("pingCtlTable", "not a response to our ECHO_REQUEST\n"));
                return SNMP_ERR_NOERROR;
            }

            if (icmplen < 16)
                DEBUGMSGTL(("pingCtlTable", "icmplen (%d) < 16", icmplen));

            tvsend = (struct timeval *) icmp->icmp_data;

            rtt =
                round_double((1000000 * (tvrecv->tv_sec - tvsend->tv_sec) +
                              tvrecv->tv_usec - tvsend->tv_usec) / 1000);

            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                                      item->pingCtlOwnerIndex,
                                      item->pingCtlOwnerIndexLen);
            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                                      item->pingCtlTestName,
                                      item->pingCtlTestNameLen);

            StorageNew = header_complex_get(pingResultsTableStorage, vars);
            snmp_free_varbind(vars);
            if (!StorageNew) {
                DEBUGMSGTL(("pingCtlTable", "StorageNew == NULL\n"));
                return SNMP_ERR_NOSUCHNAME;
            }

            if (current_probe == 1) {
                *averagertt = rtt;
                *minrtt = rtt;
                *maxrtt = rtt;
                *sumrtt = rtt;
            } else {
                if (rtt < *minrtt)
                    *minrtt = rtt;
                if (rtt > *maxrtt)
                    *maxrtt = rtt;
                *sumrtt = (*sumrtt) + rtt;
                *averagertt =
                    round_double((*sumrtt) /
                                 (StorageNew->pingResultsProbeResponses +
                                  1));
            }


            StorageNew->pingResultsMinRtt = *minrtt;
            StorageNew->pingResultsMaxRtt = *maxrtt;
            StorageNew->pingResultsAverageRtt = *averagertt;
            StorageNew->pingResultsProbeResponses =
                StorageNew->pingResultsProbeResponses + 1;
            StorageNew->pingResultsSendProbes =
                StorageNew->pingResultsSendProbes + 1;
            StorageNew->pingResultsRttSumOfSquares =
                StorageNew->pingResultsRttSumOfSquares + rtt * rtt;

	    StorageNew->pingResultsLastGoodProbe_time = timep;
            free(StorageNew->pingResultsLastGoodProbe);
            memdup(&StorageNew->pingResultsLastGoodProbe,
		date_n_time(&timep,
		    &StorageNew->pingResultsLastGoodProbeLen), 11);

            temp = SNMP_MALLOC_STRUCT(pingProbeHistoryTable_data);

            temp->pingCtlOwnerIndex =
                (char *) malloc(item->pingCtlOwnerIndexLen + 1);
            memcpy(temp->pingCtlOwnerIndex, item->pingCtlOwnerIndex,
                   item->pingCtlOwnerIndexLen + 1);
            temp->pingCtlOwnerIndex[item->pingCtlOwnerIndexLen] = '\0';
            temp->pingCtlOwnerIndexLen = item->pingCtlOwnerIndexLen;

            temp->pingCtlTestName =
                (char *) malloc(item->pingCtlTestNameLen + 1);
            memcpy(temp->pingCtlTestName, item->pingCtlTestName,
                   item->pingCtlTestNameLen + 1);
            temp->pingCtlTestName[item->pingCtlTestNameLen] = '\0';
            temp->pingCtlTestNameLen = item->pingCtlTestNameLen;

            /* add lock to protect */
            pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutex_lock(&counter_mutex);
            if (item->pingProbeHistoryMaxIndex >=
                (unsigned long) (2147483647))
                item->pingProbeHistoryMaxIndex = 0;
            temp->pingProbeHistoryIndex =
                ++(item->pingProbeHistoryMaxIndex);
            pthread_mutex_unlock(&counter_mutex);


            temp->pingProbeHistoryResponse = rtt;
            temp->pingProbeHistoryStatus = 1;
            temp->pingProbeHistoryLastRC = 0;

	    temp->pingProbeHistoryTime_time = timep;
	    memdup(&temp->pingProbeHistoryTime,
		date_n_time(&timep, &temp->pingProbeHistoryTimeLen), 11);

            if (StorageNew->pingResultsSendProbes == 1)
                item->pingProbeHis = temp;
            else
                (current_temp)->next = temp;

            current_temp = temp;

            if (StorageNew->pingResultsSendProbes >= item->pingCtlProbeCount)
                current_temp->next = NULL;

            if (item->pingProbeHis != NULL) {
                if (pingProbeHistoryTable_count(item) >= item->pingCtlMaxRows)
                    pingProbeHistoryTable_delLast(item);
                if (pingProbeHistoryTable_add(current_temp) != SNMPERR_SUCCESS)
                    DEBUGMSGTL(("pingProbeHistoryTable",
                                "failed to add a row\n"));
	    }
        }
    } else if (flag == 1) {
        if (series == 0)
            probeFailed = 1;
        else
            probeFailed = probeFailed + 1;
        series = 1;
        testFailed = testFailed + 1;
        snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                                  item->pingCtlOwnerIndex,
                                  item->pingCtlOwnerIndexLen);
        snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                                  item->pingCtlTestName,
                                  item->pingCtlTestNameLen);

        StorageNew = header_complex_get(pingResultsTableStorage, vars);
        snmp_free_varbind(vars);
        if (!StorageNew)
            return SNMP_ERR_NOSUCHNAME;

        if (current_probe == 1) {
            *averagertt = rtt;
            *minrtt = rtt;
            *maxrtt = rtt;
            *sumrtt = rtt;
        }
        StorageNew->pingResultsSendProbes =
            StorageNew->pingResultsSendProbes + 1;



        temp = SNMP_MALLOC_STRUCT(pingProbeHistoryTable_data);

        temp->pingCtlOwnerIndex =
            (char *) malloc(item->pingCtlOwnerIndexLen + 1);
        memcpy(temp->pingCtlOwnerIndex, item->pingCtlOwnerIndex,
               item->pingCtlOwnerIndexLen + 1);
        temp->pingCtlOwnerIndex[item->pingCtlOwnerIndexLen] = '\0';
        temp->pingCtlOwnerIndexLen = item->pingCtlOwnerIndexLen;

        temp->pingCtlTestName =
            (char *) malloc(item->pingCtlTestNameLen + 1);
        memcpy(temp->pingCtlTestName, item->pingCtlTestName,
               item->pingCtlTestNameLen + 1);
        temp->pingCtlTestName[item->pingCtlTestNameLen] = '\0';
        temp->pingCtlTestNameLen = item->pingCtlTestNameLen;

        /* add lock to protect */
        pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&counter_mutex);
        temp->pingProbeHistoryIndex = ++(item->pingProbeHistoryMaxIndex);
        pthread_mutex_unlock(&counter_mutex);
        /* end */

        temp->pingProbeHistoryResponse = item->pingCtlTimeOut * 1000;
        temp->pingProbeHistoryStatus = 4;
        temp->pingProbeHistoryLastRC = 1;

	temp->pingProbeHistoryTime_time = timep;
	memdup(&temp->pingProbeHistoryTime,
	    date_n_time(&timep, &temp->pingProbeHistoryTimeLen), 11);

        if (StorageNew->pingResultsSendProbes == 1)
            item->pingProbeHis = temp;
        else {
            (current_temp)->next = temp;
        }

        current_temp = temp;

        if (StorageNew->pingResultsSendProbes >= item->pingCtlProbeCount) {
            current_temp->next = NULL;
        }

        if (item->pingProbeHis != NULL) {
            if (pingProbeHistoryTable_count(item) < item->pingCtlMaxRows) {
                if (pingProbeHistoryTable_add(current_temp) !=
                    SNMPERR_SUCCESS)
                    DEBUGMSGTL(("pingProbeHistoryTable",
                                "registered an entry error\n"));
            } else {

                pingProbeHistoryTable_delLast(item);
                if (pingProbeHistoryTable_add(current_temp) !=
                    SNMPERR_SUCCESS)
                    DEBUGMSGTL(("pingProbeHistoryTable",
                                "registered an entry error\n"));

            }
	}

        if ((item->
             pingCtlTrapGeneration[0] & PINGTRAPGENERATION_PROBEFAILED) !=
            0) {
            if (probeFailed >= item->pingCtlTrapProbeFailureFilter)
                send_ping_trap(item, pingProbeFailed,
                               sizeof(pingProbeFailed) / sizeof(oid));
        }


    }

    if (current_probe == item->pingCtlProbeCount) {
        if ((item->
             pingCtlTrapGeneration[0] & PINGTRAPGENERATION_TESTCOMPLETED)
            != 0) {
            send_ping_trap(item, pingTestCompleted,
                           sizeof(pingTestCompleted) / sizeof(oid));
        } else
            if ((item->
                 pingCtlTrapGeneration[0] & PINGTRAPGENERATION_TESTFAILED)
                != 0) {

            if (testFailed >= item->pingCtlTrapTestFailureFilter)
                send_ping_trap(item, pingTestFailed,
                               sizeof(pingTestFailed) / sizeof(oid));
        }

        else if ((item->
                  pingCtlTrapGeneration[0] &
                  PINGTRAPGENERATION_PROBEFAILED) != 0) {;
        } else {
            ;
        }

        series = 0;
        probeFailed = 0;
        testFailed = 0;

    }
    return SNMP_ERR_NOERROR;
}



void
send_v4(int datalen, pid_t pid, int nsent, int sockfd, char *sendbuf)
{
    int             len;
    struct icmp    *icmp = NULL;

    icmp = (struct icmp *) sendbuf;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = pid;
    icmp->icmp_seq = nsent;
    netsnmp_get_monotonic_clock((struct timeval *) icmp->icmp_data);

    len = 8 + datalen;          /* checksum ICMP header and data */
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = in_cksum((u_short *) icmp, len);

    sendto(sockfd, sendbuf, len, 0, pr->sasend, pr->salen);
}


void
run_ping(unsigned int clientreg, void *clientarg)
/* run_ping(struct pingCtlTable_data *item) */
{
    struct pingCtlTable_data *item = clientarg;
    netsnmp_variable_list *vars = NULL;
    struct pingResultsTable_data *StorageNew = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlOwnerIndex,
                              item->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlTestName, item->pingCtlTestNameLen);

    StorageNew = header_complex_get(pingResultsTableStorage, vars);
    snmp_free_varbind(vars);
    if (!StorageNew)
        return;

    StorageNew->pingResultsSendProbes = 0;
    StorageNew->pingResultsProbeResponses = 0;

    if (item->pingCtlTargetAddressType == 1
        || item->pingCtlTargetAddressType == 16) {
        struct proto    proto_v4 =
            { proc_v4, send_v4, NULL, NULL, 0, IPPROTO_ICMP };
        char           *host = NULL;
        pid_t           pid;    /* our PID */

        int             datalen;
        unsigned long  *minrtt = NULL;
        unsigned long  *maxrtt = NULL;
        unsigned long  *averagertt = NULL;

        datalen = 56;           /* data that goes with ICMP echo request */
        struct addrinfo *ai = NULL;
        minrtt = malloc(sizeof(unsigned long));
        maxrtt = malloc(sizeof(unsigned long));
        averagertt = malloc(sizeof(unsigned long));
        host = item->pingCtlTargetAddress;
        pid = getpid();

        ai = host_serv(host, NULL, 0, 0);

        if (ai) {
            DEBUGMSGTL(("pingCtlTable", "PING %s (%s): %d data bytes\n",
                        ai->ai_canonname,
                        sock_ntop_host(ai->ai_addr, ai->ai_addrlen), datalen));

            /*
             * 4initialize according to protocol 
             */
            if (ai->ai_family == AF_INET) {
                pr = &proto_v4;
#ifdef	IPV6
            } else if (ai->ai_family == AF_INET6) {
                pr = &proto_v6;

                if (IN6_IS_ADDR_V4MAPPED(&(((struct sockaddr_in6 *)
                                            ai->ai_addr)->sin6_addr)))
                    snmp_log(LOG_ERR, "cannot ping IPv4-mapped IPv6 address");
#endif
            } else {
                snmp_log(LOG_ERR, "unknown address family %d", ai->ai_family);
            }

            pr->sasend = ai->ai_addr;
            pr->sarecv = calloc(1, ai->ai_addrlen);
            pr->salen = ai->ai_addrlen;
            readloop(item, ai, datalen, minrtt, maxrtt, averagertt, pid);
            free(pr->sarecv);
        } else {
            snmp_log(LOG_ERR, "PING: name resolution for %s failed.\n", host);
        }

        SNMP_FREE(minrtt);
        SNMP_FREE(maxrtt);
        SNMP_FREE(averagertt);
        freeaddrinfo(ai);
    }

    else if (item->pingCtlTargetAddressType == 2) {

        int             hold = 0, packlen = 0;
        u_char         *packet = NULL;
        char           *target = NULL;
        struct sockaddr_in6 firsthop;
        int             socket_errno = 0;
        struct icmp6_filter filter;
        int             err = 0, csum_offset = 0, sz_opt = 0;

        static int      icmp_sock = 0;
        int             uid = 0;
        struct sockaddr_in6 source;
        int             preload = 0;
        static unsigned char cmsgbuf[4096];
        static int      cmsglen = 0;
        struct sockaddr_in6 whereto;    /* who to ping */
        int             options = 0;
        char           *hostname = NULL;
        char           *device = NULL;
        int             interval = 1000;        /* interval between packets (msec) */
        int             pmtudisc = -1;
        int             datalen = DEFDATALEN;
        int             timing = 0;     /* flag to do timing */
        int             working_recverr = 0;
        __u32           flowlabel = 0;

        int             ident = 0;      /* process id to identify our packets */
        u_char          outpack[MAX_PACKET];
        struct timeval  start_time;
        static int      screen_width = INT_MAX;
        int             deadline = 0;   /* time to die */
        int             timeout = 0;

        timeout = item->pingCtlTimeOut;
        memset(&source, 0, sizeof(source));
        icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        socket_errno = errno;

        uid = getuid();
        setuid(uid);

        source.sin6_family = AF_INET6;
        memset(&firsthop, 0, sizeof(firsthop));
        firsthop.sin6_family = AF_INET6;
        preload = 1;

        target = item->pingCtlTargetAddress;

        memset(&whereto, 0, sizeof(struct sockaddr_in6));
        whereto.sin6_family = AF_INET6;
        whereto.sin6_port = htons(IPPROTO_ICMPV6);

        if (inet_pton(AF_INET6, target, &whereto.sin6_addr) <= 0) {
            struct hostent *hp = NULL;

            hp = gethostbyname2(target, AF_INET6);

            if (hp == NULL) {
                fprintf(stderr, "unknown host\n");
                return;
            }

            memcpy(&whereto.sin6_addr, hp->h_addr_list[0], 16);
        } else {
            options |= F_NUMERIC;
        }
        if (ipv6_addr_any(&firsthop.sin6_addr))
            memcpy(&firsthop.sin6_addr, &whereto.sin6_addr, 16);

        hostname = target;

        if (ipv6_addr_any(&source.sin6_addr)) {
            socklen_t       alen;
            int             probe_fd = socket(AF_INET6, SOCK_DGRAM, 0);

            if (probe_fd < 0) {
                snmp_log_perror("pingCtlTable: IPv6 datagram socket creation");
                return;
            }
            if (device) {
                struct ifreq    ifr;
                memset(&ifr, 0, sizeof(ifr));
                strlcpy(ifr.ifr_name, device, IFNAMSIZ);
                if (setsockopt
                    (probe_fd, SOL_SOCKET, SO_BINDTODEVICE, device,
                     strlen(device) + 1) == -1) {
#ifdef HAVE_SIN6_SCOPEID
                    if ((firsthop.sin6_addr.
                         s6_addr16[0] & htons(0xffc0)) == htons(0xfe80)
                        || (firsthop.sin6_addr.
                            s6_addr16[0] & htons(0xffff)) ==
                        htons(0xff02)) {
                        if (ioctl(probe_fd, SIOCGIFINDEX, &ifr) < 0) {
                            fprintf(stderr, "ping: unknown iface %s\n",
                                    device);
                            close(probe_fd);
                            return;
                        }
                        firsthop.sin6_scope_id = ifr.ifr_ifindex;
                    }
#endif
                }
            }
            firsthop.sin6_port = htons(1025);
            if (connect
                (probe_fd, (struct sockaddr *) &firsthop,
                 sizeof(firsthop)) == -1) {
                perror("connect");
                close(probe_fd);
                return;
            }
            alen = sizeof(source);
            if (getsockname(probe_fd, (struct sockaddr *) &source, &alen)
                == -1) {
                perror("getsockname");
                close(probe_fd);
                return;
            }
            source.sin6_port = 0;
            close(probe_fd);
        }

        if (icmp_sock < 0) {
            errno = socket_errno;
            perror("ping: icmp open socket");
            return;
        }

        if ((whereto.sin6_addr.s6_addr16[0] & htons(0xff00)) ==
            htons(0xff00)) {
            if (uid) {
                if (interval < 1000) {
                    fprintf(stderr,
                            "ping: multicast ping with too short interval.\n");
                    return;
                }
                if (pmtudisc >= 0 && pmtudisc != IPV6_PMTUDISC_DO) {
                    fprintf(stderr,
                            "ping: multicast ping does not fragment.\n");
                    return;
                }
            }
            if (pmtudisc < 0)
                pmtudisc = IPV6_PMTUDISC_DO;
        }

        if (pmtudisc >= 0) {
            if (setsockopt
                (icmp_sock, SOL_IPV6, IPV6_MTU_DISCOVER, &pmtudisc,
                 sizeof(pmtudisc)) == -1) {
                perror("ping: IPV6_MTU_DISCOVER");
                return;
            }
        }
        if (bind(icmp_sock, (struct sockaddr *) &source, sizeof(source)) ==
            -1) {
            perror("ping: bind icmp socket");
            return;
        }
        if (datalen >= sizeof(struct timeval))  /* can we time transfer */
            timing = 1;
        packlen = datalen + 8 + 4096 + 40 + 8;  /* 4096 for rthdr */
        if (!(packet = (u_char *) malloc((u_int) packlen))) {
            fprintf(stderr, "ping: out of memory.\n");
            return;
        }

        working_recverr = 1;
        hold = 1;
        if (setsockopt
            (icmp_sock, SOL_IPV6, IPV6_RECVERR, (char *) &hold,
             sizeof(hold))) {
            fprintf(stderr,
                    "WARNING: your kernel is veeery old. No problems.\n");
            working_recverr = 0;
        }

        /*
         * Estimate memory eaten by single packet. It is rough estimate.
         * * Actually, for small datalen's it depends on kernel side a lot. 
         */
        hold = datalen + 8;
        hold += ((hold + 511) / 512) * (40 + 16 + 64 + 160);
        sock_setbufs(icmp_sock, hold, preload);

        csum_offset = 2;
        sz_opt = sizeof(int);

        err =
            setsockopt(icmp_sock, SOL_RAW, IPV6_CHECKSUM, &csum_offset,
                       sz_opt);
        if (err < 0) {
            perror("setsockopt(RAW_CHECKSUM)");
            return;
        }

        /*
         *      select icmp echo reply as icmp type to receive
         */

        ICMPV6_FILTER_SETBLOCKALL(&filter);

        if (!working_recverr) {
            ICMPV6_FILTER_SETPASS(ICMP6_DST_UNREACH, &filter);
            ICMPV6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
            ICMPV6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &filter);
            ICMPV6_FILTER_SETPASS(ICMP6_PARAM_PROB, &filter);
        }

        ICMPV6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);

        err = setsockopt(icmp_sock, SOL_ICMPV6, ICMP6_FILTER, &filter,
                         sizeof(struct icmp6_filter));

        if (err < 0) {
            perror("setsockopt(ICMP6_FILTER)");
            return;
        }

        if (1) {
            int             on = 1;
            if (setsockopt(icmp_sock, IPPROTO_IPV6, IPV6_HOPLIMIT,
                           &on, sizeof(on)) == -1) {
                perror("can't receive hop limit");
                return;
            }
        }

        DEBUGMSGTL(("pingCtlTable", "PING %s(%s) ", hostname,
                    pr_addr(&whereto.sin6_addr, options)));
        if (flowlabel)
            DEBUGMSGTL(("pingCtlTable", ", flow 0x%05x, ",
                        (unsigned) ntohl(flowlabel)));
        if (device || (options & F_NUMERIC)) {
            DEBUGMSGTL(("pingCtlTable", "from %s %s: ",
                        pr_addr_n(&source.sin6_addr), device ? : ""));
        }
        DEBUGMSGTL(("pingCtlTable", "%d data bytes\n", datalen));

        setup(icmp_sock, options, uid, timeout, preload, interval, datalen,
              (char *) outpack, &ident, &start_time, &screen_width,
              &deadline);

        main_loop(item, icmp_sock, preload, packet, packlen, cmsglen,
                  (char *) cmsgbuf, &whereto, options, uid, hostname,
                  interval, datalen, timing, working_recverr,
                  (char *) outpack, &ident, &start_time, &screen_width,
                  &deadline);

        close(icmp_sock);
    }
    return;
}

void
init_resultsTable(struct pingCtlTable_data *item)
{
    struct pingResultsTable_data *StorageTmp = NULL;
    struct pingResultsTable_data *StorageNew = NULL;
    struct addrinfo *ai = NULL;
    char           *host = NULL;
    netsnmp_variable_list *vars = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlOwnerIndex,
                              item->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlTestName, item->pingCtlTestNameLen);

    StorageNew = header_complex_get(pingResultsTableStorage, vars);
    snmp_free_varbind(vars);
    if (StorageNew) {
        StorageNew->pingResultsSendProbes = 0;
        StorageNew->pingResultsProbeResponses = 0;
        return;
    }

    host = item->pingCtlTargetAddress;
    ai = host_serv(host, NULL, 0, 0);
    StorageTmp = SNMP_MALLOC_STRUCT(pingResultsTable_data);

    StorageTmp->pingCtlOwnerIndex =
        (char *) malloc(item->pingCtlOwnerIndexLen + 1);
    memcpy(StorageTmp->pingCtlOwnerIndex, item->pingCtlOwnerIndex,
           item->pingCtlOwnerIndexLen + 1);
    StorageTmp->pingCtlOwnerIndex[item->pingCtlOwnerIndexLen] = '\0';
    StorageTmp->pingCtlOwnerIndexLen = item->pingCtlOwnerIndexLen;

    StorageTmp->pingCtlTestName =
        (char *) malloc(item->pingCtlTestNameLen + 1);
    memcpy(StorageTmp->pingCtlTestName, item->pingCtlTestName,
           item->pingCtlTestNameLen + 1);
    StorageTmp->pingCtlTestName[item->pingCtlTestNameLen] = '\0';
    StorageTmp->pingCtlTestNameLen = item->pingCtlTestNameLen;

    StorageTmp->pingResultsOperStatus = 1;

    if (item->pingCtlTargetAddressType == 1
        || item->pingCtlTargetAddressType == 16) {
        const char* str;

        StorageTmp->pingResultsIpTargetAddressType = ai ? 1 : 0;
        str = ai ? sock_ntop_host(ai->ai_addr, ai->ai_addrlen) : NULL;
        if (!str)
            str = "";
        StorageTmp->pingResultsIpTargetAddress = strdup(str);
        StorageTmp->pingResultsIpTargetAddressLen = strlen(str);
    }
    if (item->pingCtlTargetAddressType == 2) {

        struct sockaddr_in6 whereto;    /* Who to try to reach */
        register struct sockaddr_in6 *to =
            (struct sockaddr_in6 *) &whereto;
        struct hostent *hp = NULL;
        char            pa[64];

        to->sin6_family = AF_INET6;
        to->sin6_port = htons(33434);

        if (inet_pton(AF_INET6, host, &to->sin6_addr) > 0) {
            StorageTmp->pingResultsIpTargetAddressType = 2;
            StorageTmp->pingResultsIpTargetAddress = strdup(host);
            StorageTmp->pingResultsIpTargetAddressLen = strlen(host);
        } else {
            hp = gethostbyname2(host, AF_INET6);
            if (hp != NULL) {
                const char     *hostname = NULL;
                memmove((caddr_t) & to->sin6_addr, hp->h_addr, 16);
                hostname = inet_ntop(AF_INET6, &to->sin6_addr, pa, 64);
                StorageTmp->pingResultsIpTargetAddressType = 2;
                StorageTmp->pingResultsIpTargetAddress = strdup(hostname);
                StorageTmp->pingResultsIpTargetAddressLen = strlen(hostname);
            } else {
                (void) fprintf(stderr,
                               "traceroute: unknown host %s\n", host);
                StorageTmp->pingResultsIpTargetAddressType = 0;
                StorageTmp->pingResultsIpTargetAddress = strdup("");
                StorageTmp->pingResultsIpTargetAddressLen = 0;
            }
        }
    }


    StorageTmp->pingResultsMinRtt = 0;
    StorageTmp->pingResultsMaxRtt = 0;
    StorageTmp->pingResultsAverageRtt = 0;
    StorageTmp->pingResultsProbeResponses = 0;
    StorageTmp->pingResultsSendProbes = 0;
    StorageTmp->pingResultsRttSumOfSquares = 0;

    StorageTmp->pingResultsLastGoodProbeLen = 0;

    item->pingResults = StorageTmp;
    if (item->pingProbeHistoryMaxIndex == 0) {
        if (item->pingResults != NULL) {
            if (pingResultsTable_add(item) != SNMPERR_SUCCESS) {
                DEBUGMSGTL(("pingResultsTable", "init an entry error\n"));
            }
        }
    }
    freeaddrinfo(ai);
}


int
modify_ResultsOper(struct pingCtlTable_data *thedata, long val)
{
    netsnmp_variable_list *vars = NULL;
    struct pingResultsTable_data *StorageTmp = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlOwnerIndex,
                              thedata->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlTestName,
                              thedata->pingCtlTestNameLen);

    StorageTmp = header_complex_get(pingResultsTableStorage, vars);
    snmp_free_varbind(vars);
    if (!StorageTmp)
        return SNMP_ERR_NOSUCHNAME;
    StorageTmp->pingResultsOperStatus = val;

    DEBUGMSGTL(("pingResultsOperStatus", "done.\n"));
    return SNMPERR_SUCCESS;
}


int
pingResultsTable_del(struct pingCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2, *next;
    struct pingResultsTable_data *StorageDel = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlOwnerIndex,
                              thedata->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlTestName,
                              thedata->pingCtlTestNameLen);
    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);
    snmp_free_varbind(vars);

    for (hciptr2 = pingResultsTableStorage; hciptr2; hciptr2 = next) {
        next = hciptr2->next;
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            StorageDel =
                header_complex_extract_entry(&pingResultsTableStorage,
                                             hciptr2);
            if (StorageDel != NULL) {
                SNMP_FREE(StorageDel->pingCtlOwnerIndex);
                SNMP_FREE(StorageDel->pingCtlTestName);
                SNMP_FREE(StorageDel->pingResultsIpTargetAddress);
                SNMP_FREE(StorageDel->pingResultsLastGoodProbe);
                SNMP_FREE(StorageDel);
            }
            DEBUGMSGTL(("pingResultsTable", "delete  success!\n"));

        }
    }
    return SNMPERR_SUCCESS;
}


int
pingProbeHistoryTable_del(struct pingCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2, *next;
    struct pingProbeHistoryTable_data *StorageDel = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlOwnerIndex,
                              thedata->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              thedata->pingCtlTestName,
                              thedata->pingCtlTestNameLen);
    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);
    snmp_free_varbind(vars);

    for (hciptr2 = pingProbeHistoryTableStorage; hciptr2; hciptr2 = next) {
        next = hciptr2->next;
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            StorageDel =
                header_complex_extract_entry(&pingProbeHistoryTableStorage,
                                             hciptr2);
            if (StorageDel != NULL) {
                SNMP_FREE(StorageDel->pingCtlOwnerIndex);
                SNMP_FREE(StorageDel->pingCtlTestName);
                SNMP_FREE(StorageDel->pingProbeHistoryTime);
                SNMP_FREE(StorageDel);
            }
            DEBUGMSGTL(("pingProbeHistoryTable", "delete  success!\n"));

        }
    }
    return SNMPERR_SUCCESS;
}


int
write_pingCtlTargetAddressType(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "write_pingCtlTargetAddressType entering action=%d...  \n",
                action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTargetAddressType not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlTargetAddressType;
        StorageTmp->pingCtlTargetAddressType = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlTargetAddressType = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}



int
write_pingCtlTargetAddress(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTargetAddress not ASN_OCTET_STR\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlTargetAddress;
        tmplen = StorageTmp->pingCtlTargetAddressLen;

        StorageTmp->pingCtlTargetAddress =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->pingCtlTargetAddress == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->pingCtlTargetAddress, var_val, var_val_len);
        StorageTmp->pingCtlTargetAddress[var_val_len] = '\0';
        StorageTmp->pingCtlTargetAddressLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->pingCtlTargetAddress);
        StorageTmp->pingCtlTargetAddress = tmpvar;
        StorageTmp->pingCtlTargetAddressLen = tmplen;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        SNMP_FREE(tmpvar);
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}



int
write_pingCtlDataSize(int action,
                      u_char * var_val,
                      u_char var_val_type,
                      size_t var_val_len,
                      u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlDataSize entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlDataSize not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlDataSize;
        if ((*((long *) var_val)) >= 0 && (*((long *) var_val)) <= 65507)
            StorageTmp->pingCtlDataSize = *((long *) var_val);
        else
            StorageTmp->pingCtlDataSize = 56;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlDataSize = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlTimeOut(int action,
                     u_char * var_val,
                     u_char var_val_type,
                     size_t var_val_len,
                     u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlTimeOut entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlDataSize not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlTimeOut;
        if ((*((long *) var_val)) >= 1 && (*((long *) var_val)) <= 60)
            StorageTmp->pingCtlTimeOut = *((long *) var_val);
        else
            StorageTmp->pingCtlTimeOut = 3;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlTimeOut = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}





int
write_pingCtlProbeCount(int action,
                        u_char * var_val,
                        u_char var_val_type,
                        size_t var_val_len,
                        u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlProbeCount entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlDataSize not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlProbeCount;

        if ((*((long *) var_val)) >= 1 && (*((long *) var_val)) <= 15)
            StorageTmp->pingCtlProbeCount = *((long *) var_val);
        else
            StorageTmp->pingCtlProbeCount = 15;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlProbeCount = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}



int
write_pingCtlAdminStatus(int action,
                         u_char * var_val,
                         u_char var_val_type,
                         size_t var_val_len,
                         u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlAdminStatus entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTargetAddressType not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlAdminStatus;
        StorageTmp->pingCtlAdminStatus = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlAdminStatus = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        if (StorageTmp->pingCtlAdminStatus == 1
            && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
            StorageTmp->pingResults->pingResultsOperStatus = 1;
            modify_ResultsOper(StorageTmp, 1);
            if (StorageTmp->pingCtlFrequency != 0)
                StorageTmp->timer_id =
                    snmp_alarm_register(StorageTmp->pingCtlFrequency,
                                        SA_REPEAT, run_ping, StorageTmp);
            else
                StorageTmp->timer_id = snmp_alarm_register(1, 0, run_ping,
                                                           StorageTmp);

        } else if (StorageTmp->pingCtlAdminStatus == 2
                   && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
            snmp_alarm_unregister(StorageTmp->timer_id);
            StorageTmp->pingResults->pingResultsOperStatus = 2;
            modify_ResultsOper(StorageTmp, 2);
        }

        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlDataFill(int action,
                      u_char * var_val,
                      u_char var_val_type,
                      size_t var_val_len,
                      u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTargetAddress not ASN_OCTET_STR\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlDataFill;
        tmplen = StorageTmp->pingCtlDataFillLen;
        StorageTmp->pingCtlDataFill = (char *) malloc(var_val_len + 1);
        if (StorageTmp->pingCtlDataFill == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->pingCtlDataFill, var_val, var_val_len);
        StorageTmp->pingCtlDataFill[var_val_len] = '\0';
        StorageTmp->pingCtlDataFillLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->pingCtlDataFill);
        StorageTmp->pingCtlDataFill = tmpvar;
        StorageTmp->pingCtlDataFillLen = tmplen;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        SNMP_FREE(tmpvar);
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}


int
write_pingCtlFrequency(int action,
                       u_char * var_val,
                       u_char var_val_type,
                       size_t var_val_len,
                       u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlFrequency entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlDataSize not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlFrequency;
        StorageTmp->pingCtlFrequency = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlFrequency = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}


int
write_pingCtlMaxRows(int action,
                     u_char * var_val,
                     u_char var_val_type,
                     size_t var_val_len,
                     u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlMaxRows entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlMaxRows not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlMaxRows;
        StorageTmp->pingCtlMaxRows = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlMaxRows = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlStorageType(int action,
                         u_char * var_val,
                         u_char var_val_type,
                         size_t var_val_len,
                         u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlStorageType entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to pingCtlStorageType not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlStorageType;
        StorageTmp->pingCtlStorageType = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlStorageType = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlTrapGeneration(int action,
                            u_char * var_val,
                            u_char var_val_type,
                            size_t var_val_len,
                            u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTargetAddress not ASN_OCTET_STR\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlTrapGeneration;
        tmplen = StorageTmp->pingCtlTrapGenerationLen;

        StorageTmp->pingCtlTrapGeneration =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->pingCtlTrapGeneration == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->pingCtlTrapGeneration, var_val, var_val_len);
        StorageTmp->pingCtlTrapGeneration[var_val_len] = '\0';
        StorageTmp->pingCtlTrapGenerationLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->pingCtlTrapGeneration);
        StorageTmp->pingCtlTrapGeneration = tmpvar;
        StorageTmp->pingCtlTrapGenerationLen = tmplen;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        SNMP_FREE(tmpvar);
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}

int
write_pingCtlTrapProbeFailureFilter(int action,
                                    u_char * var_val,
                                    u_char var_val_type,
                                    size_t var_val_len,
                                    u_char * statP, oid * name,
                                    size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlTrapProbeFailureFilter entering action=%d...  \n",
                action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTrapProbeFailureFilter not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlTrapProbeFailureFilter;

        if ((*((long *) var_val)) >= 0 && (*((long *) var_val)) <= 15)
            StorageTmp->pingCtlTrapProbeFailureFilter =
                *((long *) var_val);
        else
            StorageTmp->pingCtlTrapProbeFailureFilter = 1;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlTrapProbeFailureFilter = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}


int
write_pingCtlTrapTestFailureFilter(int action,
                                   u_char * var_val,
                                   u_char var_val_type,
                                   size_t var_val_len,
                                   u_char * statP, oid * name,
                                   size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlTrapTestFailureFilter entering action=%d...  \n",
                action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlTrapTestFailureFilter not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlTrapTestFailureFilter;

        if ((*((long *) var_val)) >= 0 && (*((long *) var_val)) <= 15)
            StorageTmp->pingCtlTrapTestFailureFilter = *((long *) var_val);
        else
            StorageTmp->pingCtlTrapTestFailureFilter = 1;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlTrapTestFailureFilter = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}





int
write_pingCtlType(int action,
                  u_char * var_val,
                  u_char var_val_type,
                  size_t var_val_len,
                  u_char * statP, oid * name, size_t name_len)
{
    static oid     *tmpvar;
    static size_t   tmplen;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OBJECT_ID) {
            snmp_log(LOG_ERR, "write to pingCtlType not ASN_OBJECT_ID\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlType;
        tmplen = StorageTmp->pingCtlTypeLen;

        StorageTmp->pingCtlType = (oid *) malloc(var_val_len);
        if (StorageTmp->pingCtlType == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->pingCtlType, var_val, var_val_len);

        StorageTmp->pingCtlTypeLen = var_val_len / sizeof(oid);

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->pingCtlType);
        StorageTmp->pingCtlType = tmpvar;
        StorageTmp->pingCtlTypeLen = tmplen;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        SNMP_FREE(tmpvar);
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlDescr(int action,
                   u_char * var_val,
                   u_char var_val_type,
                   size_t var_val_len,
                   u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR, "write to pingCtlDescr not ASN_OCTET_STR\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlDescr;
        tmplen = StorageTmp->pingCtlDescrLen;

        StorageTmp->pingCtlDescr = (char *) malloc(var_val_len + 1);
        if (StorageTmp->pingCtlDescr == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->pingCtlDescr, var_val, var_val_len);
        StorageTmp->pingCtlDescr[var_val_len] = '\0';
        StorageTmp->pingCtlDescrLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->pingCtlDescr);
        StorageTmp->pingCtlDescr = tmpvar;
        StorageTmp->pingCtlDescrLen = tmplen;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        SNMP_FREE(tmpvar);
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}


int
write_pingCtlSourceAddressType(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlSourceAddressType entering action=%d...  \n",
                action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to pingCtlSourceAddressType not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlSourceAddressType;
        StorageTmp->pingCtlSourceAddressType = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlSourceAddressType = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlSourceAddress(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to pingCtlSourceAddress not ASN_OCTET_STR\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlSourceAddress;
        tmplen = StorageTmp->pingCtlSourceAddressLen;

        StorageTmp->pingCtlSourceAddress =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->pingCtlSourceAddress == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->pingCtlSourceAddress, var_val, var_val_len);
        StorageTmp->pingCtlSourceAddress[var_val_len] = '\0';
        StorageTmp->pingCtlSourceAddressLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->pingCtlSourceAddress);
        StorageTmp->pingCtlSourceAddress = tmpvar;
        StorageTmp->pingCtlSourceAddressLen = tmplen;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        SNMP_FREE(tmpvar);
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}



int
write_pingCtlIfIndex(int action,
                     u_char * var_val,
                     u_char var_val_type,
                     size_t var_val_len,
                     u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlIfIndex entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR, "write to pingCtlIfIndex not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlIfIndex;
        StorageTmp->pingCtlIfIndex = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlIfIndex = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_pingCtlByPassRouteTable(int action,
                              u_char * var_val,
                              u_char var_val_type,
                              size_t var_val_len,
                              u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlByPassRouteTable entering action=%d...  \n",
                action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to pingCtlByPassRouteTable not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlByPassRouteTable;
        StorageTmp->pingCtlByPassRouteTable = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlByPassRouteTable = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}





int
write_pingCtlDSField(int action,
                     u_char * var_val,
                     u_char var_val_type,
                     size_t var_val_len,
                     u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct pingCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);


    DEBUGMSGTL(("pingCtlTable",
                "pingCtlDSField entering action=%d...  \n", action));

    if ((StorageTmp =
         header_complex(pingCtlTableStorage, NULL,
                        &name[sizeof(pingCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to pingCtlDSField not ASN_UNSIGNED\n");
            return SNMP_ERR_WRONGTYPE;
        }
        break;


    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;


    case FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;


    case ACTION:
        /*
         * The variable has been stored in objid for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the UNDO case 
         */
        tmpvar = StorageTmp->pingCtlDSField;
        StorageTmp->pingCtlDSField = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->pingCtlDSField = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}


int
write_pingCtlRowStatus(int action,
                       u_char * var_val,
                       u_char var_val_type,
                       size_t var_val_len,
                       u_char * statP, oid * name, size_t name_len)
{
    struct pingCtlTable_data *StorageTmp;
    static struct pingCtlTable_data *StorageNew, *StorageDel;
    size_t          newlen =
        name_len - (sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);
    static int      old_value;
    int             set_value;
    struct header_complex_index *hciptr = NULL;

    DEBUGMSGTL(("pingCtlTable",
                "var_pingCtlTable: Entering...  action=%ul\n", action));
    StorageTmp =
        header_complex(pingCtlTableStorage, NULL,
                       &name[sizeof(pingCtlTable_variables_oid) /
                             sizeof(oid) + 3 - 1], &newlen, 1, NULL, NULL);

    if (var_val_type != ASN_INTEGER || var_val == NULL) {
        snmp_log(LOG_ERR, "write to pingCtlRowStatus not ASN_INTEGER\n");
        return SNMP_ERR_WRONGTYPE;
    }
    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    set_value = *((long *) var_val);


    /*
     * check legal range, and notReady is reserved for us, not a user 
     */
    if (set_value < 1 || set_value > 6 || set_value == RS_NOTREADY)
        return SNMP_ERR_INCONSISTENTVALUE;


    switch (action) {
    case RESERVE1:
        /*
         * stage one: test validity 
         */
        if (StorageTmp == NULL) {
            /*
             * create the row now? 
             */


            /*
             * ditch illegal values now 
             */
            if (set_value == RS_ACTIVE || set_value == RS_NOTINSERVICE) {

                return SNMP_ERR_INCONSISTENTVALUE;
            }

            /*
             * destroying a non-existent row is actually legal 
             */
            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }


            /*
             * illegal creation values 
             */
            if (set_value == RS_ACTIVE || set_value == RS_NOTINSERVICE) {
                return SNMP_ERR_INCONSISTENTVALUE;
            }
        } else {
            /*
             * row exists.  Check for a valid state change 
             */
            if (set_value == RS_CREATEANDGO
                || set_value == RS_CREATEANDWAIT) {
                /*
                 * can't create a row that exists 
                 */
                return SNMP_ERR_INCONSISTENTVALUE;
            }

            /*
             * XXX: interaction with row storage type needed 
             */

            if (StorageTmp->pingCtlRowStatus == RS_ACTIVE &&
                set_value != RS_DESTROY) {
                /*
                 * "Once made active an entry may not be modified except to 
                 * delete it."  XXX: doesn't this in fact apply to ALL
                 * columns of the table and not just this one?  
                 */
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (StorageTmp->storageType != ST_NONVOLATILE)
                return SNMP_ERR_NOTWRITABLE;
        }
        break;




    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        if (StorageTmp == NULL) {
            netsnmp_variable_list *vars, *vp;

            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
            /*
             * creation 
             */
            vars = NULL;

            /*
             * 将name为空的三个索引字段加到var变量列表的末尾 
             */
            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);  /* pingCtlOwnerIndex */
            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);  /* pingCtlTestName */

            if (header_complex_parse_oid
                (&
                 (name
                  [sizeof(pingCtlTable_variables_oid) / sizeof(oid) +
                   2]), newlen, vars) != SNMPERR_SUCCESS) {
                snmp_free_varbind(vars);
                return SNMP_ERR_INCONSISTENTNAME;
            }
            vp = vars;

            StorageNew = create_pingCtlTable_data();
            if (!StorageNew)
                return SNMP_ERR_GENERR;
            if (vp->val_len <= 32) {
                StorageNew->pingCtlOwnerIndex = malloc(vp->val_len + 1);
                memcpy(StorageNew->pingCtlOwnerIndex, vp->val.string,
                       vp->val_len);
                StorageNew->pingCtlOwnerIndex[vp->val_len] = '\0';
                StorageNew->pingCtlOwnerIndexLen = vp->val_len;
            } else {
                StorageNew->pingCtlOwnerIndex = malloc(33);
                memcpy(StorageNew->pingCtlOwnerIndex, vp->val.string, 32);
                StorageNew->pingCtlOwnerIndex[32] = '\0';
                StorageNew->pingCtlOwnerIndexLen = 32;
            }

            vp = vp->next_variable;

            if (vp->val_len <= 32) {
                StorageNew->pingCtlTestName = malloc(vp->val_len + 1);
                memcpy(StorageNew->pingCtlTestName, vp->val.string,
                       vp->val_len);
                StorageNew->pingCtlTestName[vp->val_len] = '\0';
                StorageNew->pingCtlTestNameLen = vp->val_len;
            } else {
                StorageNew->pingCtlTestName = malloc(33);
                memcpy(StorageNew->pingCtlTestName, vp->val.string, 32);
                StorageNew->pingCtlTestName[32] = '\0';
                StorageNew->pingCtlTestNameLen = 32;
            }
            vp = vp->next_variable;

            /*
             * XXX: fill in default row values here into StorageNew 
             */

            StorageNew->pingCtlRowStatus = set_value;


            snmp_free_varbind(vars);
        }


        break;




    case FREE:
        /*
         * Release any resources that have been allocated 
         */

        if (set_value == RS_DESTROY && StorageNew)
            free_pingCtlTable_data(StorageNew);
        StorageNew = NULL;

        if (StorageDel) {
            free_pingCtlTable_data(StorageDel);
            StorageDel = NULL;
        }
        break;




    case ACTION:
        /*
         * The variable has been stored in set_value for you to
         * use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in
         * the UNDO case 
         */

        if (StorageTmp == NULL) {
            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
            /*
             * row creation, so add it 
             */
            if (StorageNew != NULL)
#if 1
                DEBUGMSGTL(("pingCtlTable",
                            "write_pingCtlRowStatus entering new=%d...  \n",
                            action));
#endif
            pingCtlTable_add(StorageNew);
            /*
             * XXX: ack, and if it is NULL? 
             */
        } else if (set_value != RS_DESTROY) {
            /*
             * set the flag? 
             */
            old_value = StorageTmp->pingCtlRowStatus;
            StorageTmp->pingCtlRowStatus = *((long *) var_val);
        } else {
            /*
             * destroy...  extract it for now 
             */

            hciptr =
                header_complex_find_entry(pingCtlTableStorage, StorageTmp);
            StorageDel =
                header_complex_extract_entry(&pingCtlTableStorage, hciptr);
            snmp_alarm_unregister(StorageDel->timer_id);

            pingResultsTable_del(StorageTmp);
            pingProbeHistoryTable_del(StorageTmp);
        }
        break;




    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        if (StorageTmp == NULL) {
            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
            /*
             * row creation, so remove it again 
             */
            hciptr =
                header_complex_find_entry(pingCtlTableStorage, StorageNew);
            StorageDel =
                header_complex_extract_entry(&pingCtlTableStorage, hciptr);
            free_pingCtlTable_data(StorageDel);
            StorageDel = NULL;
        } else if (StorageDel != NULL) {
            /*
             * row deletion, so add it again 
             */
            pingCtlTable_add(StorageDel);
            pingResultsTable_add(StorageDel);
            pingProbeHistoryTable_addall(StorageDel);
            StorageDel = NULL;
        } else {
            StorageTmp->pingCtlRowStatus = old_value;
        }
        break;




    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */

        if (StorageTmp == NULL) {
            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
        }

        if (StorageDel != NULL) {
            free_pingCtlTable_data(StorageDel);
            StorageDel = NULL;
        } else {
            if (StorageTmp
                && StorageTmp->pingCtlRowStatus == RS_CREATEANDGO) {
                StorageTmp->pingCtlRowStatus = RS_ACTIVE;
            } else if (StorageTmp &&
                       StorageTmp->pingCtlRowStatus == RS_CREATEANDWAIT) {
                DEBUGMSGTL(("pingCtlTable",
                            "write_pingCtlRowStatus entering pingCtlRowStatus=%ld...  \n",
                            StorageTmp->pingCtlRowStatus));

                StorageTmp->pingCtlRowStatus = RS_NOTINSERVICE;
            }
        }
        if (StorageTmp && StorageTmp->pingCtlRowStatus == RS_ACTIVE) {
#if 1
            DEBUGMSGTL(("pingCtlTable",
                        "write_pingCtlRowStatus entering runbefore \n"));

#endif

            if (StorageTmp->pingCtlAdminStatus == 1) {
                init_resultsTable(StorageTmp);
                if (StorageTmp->pingCtlFrequency != 0)
                    StorageTmp->timer_id =
                        snmp_alarm_register(StorageTmp->pingCtlFrequency,
                                            SA_REPEAT, run_ping,
                                            StorageTmp);
                else
                    StorageTmp->timer_id =
                        snmp_alarm_register(1, 0, run_ping, StorageTmp);

            }

        }
        snmp_store_needed(NULL);

        break;
    }
    return SNMP_ERR_NOERROR;
}


static inline void
tvsub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0) {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}


static inline int
schedule_exit(int next, int *deadline, long *npackets, long *nreceived,
              long *ntransmitted, long *tmax)
{
    if ((*npackets) && (*ntransmitted) >= (*npackets) && !(*deadline))
        next = __schedule_exit(next, nreceived, tmax);
    return next;
}

static inline int
in_flight(__u16 * acked, long *nreceived, long *ntransmitted,
          long *nerrors)
{
    __u16           diff = (__u16) (*ntransmitted) - (*acked);
    return (diff <=
            0x7FFF) ? diff : (*ntransmitted) - (*nreceived) - (*nerrors);
}

static inline void
acknowledge(__u16 seq, __u16 * acked, long *ntransmitted, int *pipesize)
{
    __u16           diff = (__u16) (*ntransmitted) - seq;
    if (diff <= 0x7FFF) {
        if ((int) diff + 1 > (*pipesize))
            (*pipesize) = (int) diff + 1;
        if ((__s16) (seq - (*acked)) > 0 ||
            (__u16) (*ntransmitted) - (*acked) > 0x7FFF)
            *acked = seq;
    }
}

static inline void
advance_ntransmitted(__u16 * acked, long *ntransmitted)
{
    (*ntransmitted)++;
    /*
     * Invalidate acked, if 16 bit seq overflows. 
     */
    if ((__u16) (*ntransmitted) - (*acked) > 0x7FFF)
        *acked = (__u16) (*ntransmitted) + 1;
}


static inline void
update_interval(int uid, int interval, int *rtt_addend, int *rtt)
{
    int             est = (*rtt) ? (*rtt) / 8 : interval * 1000;

    interval = (est + (*rtt_addend) + 500) / 1000;
    if (uid && interval < MINUSERINTERVAL)
        interval = MINUSERINTERVAL;
}



int
__schedule_exit(int next, long *nreceived, long *tmax)
{
    unsigned long   waittime;
#if 0
    struct itimerval it;
#endif

    if (*nreceived) {
        waittime = 2 * (*tmax);
        if (waittime < 1000000)
            waittime = 1000000;
    } else
        waittime = MAXWAIT * 1000000;

    if (next < 0 || next < waittime / 1000)
        next = waittime / 1000;

#if 0
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = waittime / 1000000;
    it.it_value.tv_usec = waittime % 1000000;
    setitimer(ITIMER_REAL, &it, NULL);
#endif
    return next;
}


/*
 * pinger --
 *      Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */

int
pinger(int icmp_sock, int preload, int cmsglen, char *cmsgbuf,
       struct sockaddr_in6 *whereto, int *rtt_addend, int options, int uid,
       int interval, int datalen, int timing, char *outpack, int *rtt,
       int *ident, int *screen_width, int *deadline, __u16 * acked,
       long *npackets, long *nreceived, long *ntransmitted, long *nerrors,
       int *confirm_flag, int *confirm, int *pipesize,
       struct timeval *cur_time)
{
    static int      tokens;
    int             i;

    /*
     * Have we already sent enough? If we have, return an arbitrary positive value. 
     */

    if (exiting
        || ((*npackets) && (*ntransmitted) >= (*npackets)
            && !(*deadline))) {

        return 1000;
    }
    /*
     * Check that packets < rate*time + preload 
     */
    if ((*cur_time).tv_sec == 0) {
        netsnmp_get_monotonic_clock(cur_time);
        tokens = interval * (preload - 1);
    } else {
        long            ntokens;
        struct timeval  tv;

        netsnmp_get_monotonic_clock(&tv);
        ntokens = (tv.tv_sec - (*cur_time).tv_sec) * 1000 +
            (tv.tv_usec - (*cur_time).tv_usec) / 1000;
        if (!interval) {
            /*
             * Case of unlimited flood is special;
             * * if we see no reply, they are limited to 100pps 
             */
            if (ntokens < MININTERVAL
                && in_flight(acked, nreceived, ntransmitted,
                             nerrors) >= preload)
                return MININTERVAL - ntokens;
        }
        ntokens += tokens;

        if (ntokens > interval * preload)
            ntokens = interval * preload;

        if (ntokens < interval) {
            return interval - ntokens;
        }
        *cur_time = tv;
        tokens = ntokens - interval;
    }

  resend:
    i = send_v6(icmp_sock, cmsglen, cmsgbuf, whereto, datalen, timing,
                outpack, ident, ntransmitted, confirm);

    if (i == 0) {
        advance_ntransmitted(acked, ntransmitted);
        if (!(options & F_QUIET) && (options & F_FLOOD)) {
            /*
             * Very silly, but without this output with
             * * high preload or pipe size is very confusing. 
             */
            if ((preload < (*screen_width)
                 && (*pipesize) < (*screen_width))
                || in_flight(acked, nreceived, ntransmitted,
                             nerrors) < (*screen_width))
                write(STDOUT_FILENO, ".", 1);
        }

        return interval - tokens;
    }

    /*
     * And handle various errors... 
     */
    if (i > 0) {
        /*
         * Apparently, it is some fatal bug. 
         */
        abort();
    } else if (errno == ENOBUFS || errno == ENOMEM) {
        /*
         * Device queue overflow or OOM. Packet is not sent. 
         */
        tokens = 0;
        /*
         * Slowdown. This works only in adaptive mode (option -A) 
         */
        (*rtt_addend) += ((*rtt) < 8 * 50000 ? (*rtt) / 8 : 50000);
        if (options & F_ADAPTIVE)
            update_interval(uid, interval, rtt_addend, rtt);

        return SCHINT(interval);
    } else if (errno == EAGAIN) {
        /*
         * Socket buffer is full. 
         */
        tokens += interval;

        return MININTERVAL;
    } else {
        if ((i =
             receive_error_msg(icmp_sock, whereto, options, ident,
                               nerrors)) > 0) {
            /*
             * An ICMP error arrived. 
             */
            tokens += interval;

            return MININTERVAL;
        }
        /*
         * Compatibility with old linuces. 
         */
        if (i == 0 && (*confirm_flag) && errno == EINVAL) {
            *confirm_flag = 0;
            errno = 0;
        }
        if (!errno)
            goto resend;

        /*
         * Hard local error. Pretend we sent packet. 
         */
        advance_ntransmitted(acked, ntransmitted);

        if (i == 0 && !(options & F_QUIET)) {
            if (options & F_FLOOD)
                write(STDOUT_FILENO, "E", 1);
            else
                perror("ping: sendmsg");
        }
        tokens = 0;

        return SCHINT(interval);
    }
}

/*
 * Set socket buffers, "alloc" is an esimate of memory taken by single packet. 
 */

void
sock_setbufs(int icmp_sock, int alloc, int preload)
{
    int             rcvbuf, hold;
    socklen_t       tmplen = sizeof(hold);
    int             sndbuf;

    if (!sndbuf)
        sndbuf = alloc;
    setsockopt(icmp_sock, SOL_SOCKET, SO_SNDBUF, (char *) &sndbuf,
               sizeof(sndbuf));

    rcvbuf = hold = alloc * preload;
    if (hold < 65536)
        hold = 65536;
    setsockopt(icmp_sock, SOL_SOCKET, SO_RCVBUF, (char *) &hold,
               sizeof(hold));
    if (getsockopt
        (icmp_sock, SOL_SOCKET, SO_RCVBUF, (char *) &hold, &tmplen) == 0) {
        if (hold < rcvbuf)
            fprintf(stderr,
                    "WARNING: probably, rcvbuf is not enough to hold preload.\n");
    }
}

/*
 * Protocol independent setup and parameter checks. 
 */

void
setup(int icmp_sock, int options, int uid, int timeout, int preload,
      int interval, int datalen, char *outpack, int *ident,
      struct timeval *start_time, int *screen_width, int *deadline)
{
    int             hold;
    struct timeval  tv;

    if ((options & F_FLOOD) && !(options & F_INTERVAL))
        interval = 0;

    if (uid && interval < MINUSERINTERVAL) {
        fprintf(stderr,
                "ping: cannot flood; minimal interval, allowed for user, is %dms\n",
                MINUSERINTERVAL);
        return;
    }

    if (interval >= INT_MAX / preload) {
        fprintf(stderr, "ping: illegal preload and/or interval\n");
        return;
    }

    hold = 1;
    if (options & F_SO_DEBUG)
        setsockopt(icmp_sock, SOL_SOCKET, SO_DEBUG, (char *) &hold,
                   sizeof(hold));
    if (options & F_SO_DONTROUTE)
        setsockopt(icmp_sock, SOL_SOCKET, SO_DONTROUTE, (char *) &hold,
                   sizeof(hold));

#ifdef SO_TIMESTAMP
    if (!(options & F_LATENCY)) {
        int             on = 1;
        if (setsockopt
            (icmp_sock, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on)))
            fprintf(stderr,
                    "Warning: no SO_TIMESTAMP support, falling back to SIOCGSTAMP\n");
    }
#endif

    /*
     * Set some SNDTIMEO to prevent blocking forever
     * * on sends, when device is too slow or stalls. Just put limit
     * * of one second, or "interval", if it is less.
     */
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (interval < 1000) {
        tv.tv_sec = 0;
        tv.tv_usec = 1000 * SCHINT(interval);
    }
    setsockopt(icmp_sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &tv,
               sizeof(tv));

    /*
     * Set RCVTIMEO to "interval". Note, it is just an optimization
     * * allowing to avoid redundant poll(). 
     */

    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt
        (icmp_sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)))
        options |= F_FLOOD_POLL;

    if (!(options & F_PINGFILLED)) {
        int             i;
        char           *p = outpack + 8;

        /*
         * Do not forget about case of small datalen,
         * * fill timestamp area too!
         */
        for (i = 0; i < datalen; ++i)
            *p++ = i;
    }

    *ident = getpid() & 0xFFFF;

    netsnmp_get_monotonic_clock(start_time);

#if 0
    if (*deadline) {
        struct itimerval it;

        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 0;
        it.it_value.tv_sec = (*deadline);
        it.it_value.tv_usec = 0;
    }
#endif

    if (isatty(STDOUT_FILENO)) {
        struct winsize  w;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {
            if (w.ws_col > 0)
                *screen_width = w.ws_col;
        }
    }
}

void
main_loop(struct pingCtlTable_data *item, int icmp_sock, int preload,
          __u8 * packet, int packlen, int cmsglen, char *cmsgbuf,
          struct sockaddr_in6 *whereto, int options, int uid,
          char *hostname, int interval, int datalen, int timing,
          int working_recverr, char *outpack, int *ident,
          struct timeval *start_time, int *screen_width, int *deadline)
{
    char            addrbuf[128];
    char            ans_data[4096];
    struct iovec    iov;
    struct msghdr   msg;
    struct cmsghdr *c;
    int             cc = 0;
    int             next = 0;
    int             polling = 0;
    int             rtt = 0;
    int             rtt_addend = 0;

    __u16           acked = 0;
    /*
     * counters 
     */
    long            npackets = 0;       /* max packets to transmit */
    long            nreceived = 0;      /* # of packets we got back */
    long            nrepeats = 0;       /* number of duplicates */
    long            ntransmitted = 0;   /* sequence # for outbound packets = #sent */
    long            nchecksum = 0;      /* replies with bad checksum */
    long            nerrors = 0;        /* icmp errors */

    /*
     * timing 
     */
    long            tmin = LONG_MAX;    /* minimum round trip time */
    long            tmax = 0;   /* maximum round trip time */
    long long       tsum = 0;   /* sum of all times, for doing average */
    long long       tsum2 = 0;

    int             confirm_flag = MSG_CONFIRM;
    int             confirm = 0;

    int             pipesize = -1;
    struct timeval  cur_time;
    cur_time.tv_sec = 0;
    cur_time.tv_usec = 0;

    struct pingProbeHistoryTable_data current_temp;
    static int      probeFailed = 0;
    static int      testFailed = 0;
    static int      series = 0;

    iov.iov_base = (char *) packet;
    npackets = item->pingCtlProbeCount;
    for (;;) {
        /*
         * Check exit conditions. 
         */
        if (exiting) {
            break;
        }
        if (npackets && nreceived >= npackets) {
            DEBUGMSGTL(("pingCtlTable", "npackets,nreceived=%ld\n", nreceived));
            break;
        }
        if (deadline && nerrors) {
            DEBUGMSGTL(("pingCtlTable", "deadline\n"));
            break;
        }

        /*
         * Check for and do special actions. 
         */
        if (status_snapshot)
            status(timing, &rtt, &nreceived, &nrepeats, &ntransmitted,
                   &tmin, &tmax, &tsum, &tsum2);

        /*
         * Send probes scheduled to this time. 
         */
        do {
            DEBUGMSGTL(("pingCtlTable", "pinger\n"));
            next =
                pinger(icmp_sock, preload, cmsglen, cmsgbuf, whereto,
                       &rtt_addend, uid, options, interval, datalen,
                       timing, outpack, &rtt, ident, screen_width,
                       deadline, &acked, &npackets, &nreceived,
                       &ntransmitted, &nerrors, &confirm_flag, &confirm,
                       &pipesize, &cur_time);
            DEBUGMSGTL(("pingCtlTable", "1:next=%d\n", next));
            next =
                schedule_exit(next, deadline, &npackets, &nreceived,
                              &ntransmitted, &tmax);
            DEBUGMSGTL(("pingCtlTable", "2:next=%d\n", next));
        } while (next <= 0);

        /*
         * "next" is time to send next probe, if positive.
         * * If next<=0 send now or as soon as possible. 
         */

        /*
         * Technical part. Looks wicked. Could be dropped,
         * * if everyone used the newest kernel. :-) 
         * * Its purpose is:
         * * 1. Provide intervals less than resolution of scheduler.
         * *    Solution: spinning.
         * * 2. Avoid use of poll(), when recvmsg() can provide
         * *    timed waiting (SO_RCVTIMEO). 
         */
        polling = 0;
        if ((options & (F_ADAPTIVE | F_FLOOD_POLL))
            || next < SCHINT(interval)) {
            int             recv_expected =
                in_flight(&acked, &nreceived, &ntransmitted, &nerrors);

            /*
             * If we are here, recvmsg() is unable to wait for
             * * required timeout. 
             */
            if (1000 * next <= 1000000 / (int) HZ) {
                /*
                 * Very short timeout... So, if we wait for
                 * * something, we sleep for MININTERVAL.
                 * * Otherwise, spin! 
                 */
                if (recv_expected) {
                    next = MININTERVAL;
                } else {
                    next = 0;
                    /*
                     * When spinning, no reasons to poll.
                     * * Use nonblocking recvmsg() instead. 
                     */
                    polling = MSG_DONTWAIT;
                    /*
                     * But yield yet. 
                     */
                    sched_yield();
                }
            }

            if (!polling &&
                ((options & (F_ADAPTIVE | F_FLOOD_POLL)) || interval)) {
                struct pollfd   pset;
                pset.fd = icmp_sock;
                pset.events = POLLIN | POLLERR;
                pset.revents = 0;
                if (poll(&pset, 1, next) < 1 ||
                    !(pset.revents & (POLLIN | POLLERR)))
                    continue;
                polling = MSG_DONTWAIT;
            }
        }

        for (;;) {
            struct timeval *recv_timep = NULL;
            struct timeval  recv_time;
            int             not_ours = 0;       /* Raw socket can receive messages
                                                 * destined to other running pings. */

            iov.iov_len = packlen;
            msg.msg_name = addrbuf;
            msg.msg_namelen = sizeof(addrbuf);
            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_control = ans_data;
            msg.msg_controllen = sizeof(ans_data);

            cc = recvmsg(icmp_sock, &msg, polling);
            time_t          timep;
            time(&timep);
            polling = MSG_DONTWAIT;

            if (cc < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    break;
                if (errno == EWOULDBLOCK) {
                    struct pingResultsTable_data *StorageNew = NULL;
                    struct pingProbeHistoryTable_data *temp = NULL;
                    netsnmp_variable_list *vars = NULL;

                    if (series == 0)
                        probeFailed = 1;
                    else
                        probeFailed = probeFailed + 1;
                    series = 1;
                    testFailed = testFailed + 1;

                    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                                              item->pingCtlOwnerIndex,
                                              item->pingCtlOwnerIndexLen);
                    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                                              item->pingCtlTestName,
                                              item->pingCtlTestNameLen);

                    StorageNew = header_complex_get(pingResultsTableStorage,
                                                    vars);
                    snmp_free_varbind(vars);
                    if (!StorageNew)
                        return;

                    StorageNew->pingResultsSendProbes =
                        StorageNew->pingResultsSendProbes + 1;

                    temp = SNMP_MALLOC_STRUCT(pingProbeHistoryTable_data);

                    temp->pingCtlOwnerIndex =
                        (char *) malloc(item->pingCtlOwnerIndexLen + 1);
                    memcpy(temp->pingCtlOwnerIndex,
                           item->pingCtlOwnerIndex,
                           item->pingCtlOwnerIndexLen + 1);
                    temp->pingCtlOwnerIndex[item->pingCtlOwnerIndexLen] =
                        '\0';
                    temp->pingCtlOwnerIndexLen =
                        item->pingCtlOwnerIndexLen;

                    temp->pingCtlTestName =
                        (char *) malloc(item->pingCtlTestNameLen + 1);
                    memcpy(temp->pingCtlTestName, item->pingCtlTestName,
                           item->pingCtlTestNameLen + 1);
                    temp->pingCtlTestName[item->pingCtlTestNameLen] = '\0';
                    temp->pingCtlTestNameLen = item->pingCtlTestNameLen;

                    /* add lock to protect */
                    pthread_mutex_t counter_mutex =
                        PTHREAD_MUTEX_INITIALIZER;
                    pthread_mutex_lock(&counter_mutex);
                    temp->pingProbeHistoryIndex =
                        ++(item->pingProbeHistoryMaxIndex);
                    pthread_mutex_unlock(&counter_mutex);
                    /* end */

                    temp->pingProbeHistoryResponse =
                        item->pingCtlTimeOut * 1000;
                    temp->pingProbeHistoryStatus = 4;
                    temp->pingProbeHistoryLastRC = 1;

		    temp->pingProbeHistoryTime_time = timep;
		    memdup(&temp->pingProbeHistoryTime,
			date_n_time(&timep, &temp->pingProbeHistoryTimeLen), 11);

                    if (StorageNew->pingResultsSendProbes == 1)
                        item->pingProbeHis = temp;
                    else {
                        (current_temp).next = temp;
                    }

                    current_temp = (*temp);

                    if (StorageNew->pingResultsSendProbes >=
                        item->pingCtlProbeCount) {
                        current_temp.next = NULL;
                    }

                    if (item->pingProbeHis != NULL) {
                        if (pingProbeHistoryTable_count(item) <
                            item->pingCtlMaxRows) {
                            if (pingProbeHistoryTable_add(&current_temp) !=
                                SNMPERR_SUCCESS)
                                DEBUGMSGTL(("pingProbeHistoryTable",
                                            "registered an entry error\n"));
                        } else {
                            pingProbeHistoryTable_delLast(item);
                            if (pingProbeHistoryTable_add(&current_temp) !=
                                SNMPERR_SUCCESS)
                                DEBUGMSGTL(("pingProbeHistoryTable",
                                            "registered an entry error\n"));

                        }
		    }
                    if ((item->
                         pingCtlTrapGeneration[0] &
                         PINGTRAPGENERATION_PROBEFAILED) != 0) {
                        if (probeFailed >=
                            item->pingCtlTrapProbeFailureFilter)
                            send_ping_trap(item, pingProbeFailed,
                                           sizeof(pingProbeFailed) /
                                           sizeof(oid));
                    }
                    break;
                }
                /* timeout finish                                 */
                if (!receive_error_msg
                    (icmp_sock, whereto, options, ident, &nerrors)) {
                    if (errno) {
                        perror("ping: recvmsg");
                        break;
                    }
                    not_ours = 1;
                }
            } else {
                DEBUGMSGTL(("pingCtlTable", "cc>=0,else\n"));
#ifdef SO_TIMESTAMP
                for (c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
                    if (c->cmsg_level != SOL_SOCKET ||
                        c->cmsg_type != SO_TIMESTAMP)
                        continue;
                    if (c->cmsg_len < CMSG_LEN(sizeof(struct timeval)))
                        continue;
                    recv_timep = (struct timeval *) CMSG_DATA(c);
                }
#endif

                if ((options & F_LATENCY) || recv_timep == NULL) {
                    if ((options & F_LATENCY) ||
                        ioctl(icmp_sock, SIOCGSTAMP, &recv_time))
                        netsnmp_get_monotonic_clock(&recv_time);
                    recv_timep = &recv_time;
                }

                not_ours =
                    parse_reply(&series, item, &msg, cc, addrbuf,
                                recv_timep, timep, uid, whereto,
                                &rtt_addend, options, interval, datalen,
                                timing, working_recverr, outpack, &rtt,
                                ident, &acked, &nreceived, &nrepeats,
                                &ntransmitted, &nchecksum, &nerrors, &tmin,
                                &tmax, &tsum, &tsum2, &confirm_flag,
                                &confirm, &pipesize, &current_temp);
            }

            /*
             * See? ... someone runs another ping on this host. 
             */
            if (not_ours)
                install_filter(icmp_sock, ident);

            /*
             * If nothing is in flight, "break" returns us to pinger. 
             */
            if (in_flight(&acked, &nreceived, &ntransmitted, &nerrors) ==
                0)
                break;

            /*
             * Otherwise, try to recvmsg() again. recvmsg()
             * * is nonblocking after the first iteration, so that
             * * if nothing is queued, it will receive EAGAIN
             * * and return to pinger. 
             */
        }
    }

    if (ntransmitted == item->pingCtlProbeCount) {

        if ((item->
             pingCtlTrapGeneration[0] & PINGTRAPGENERATION_TESTCOMPLETED)
            != 0) {
            send_ping_trap(item, pingTestCompleted,
                           sizeof(pingTestCompleted) / sizeof(oid));
        } else
            if ((item->
                 pingCtlTrapGeneration[0] & PINGTRAPGENERATION_TESTFAILED)
                != 0) {

            if (testFailed >= item->pingCtlTrapTestFailureFilter)
                send_ping_trap(item, pingTestFailed,
                               sizeof(pingTestFailed) / sizeof(oid));
        }

        else if ((item->
                  pingCtlTrapGeneration[0] &
                  PINGTRAPGENERATION_PROBEFAILED) != 0) {;
        } else {
            ;
        }

        series = 0;
        probeFailed = 0;
        testFailed = 0;

    }

    finish(options, hostname, interval, timing, &rtt, start_time, deadline,
           &npackets, &nreceived, &nrepeats, &ntransmitted, &nchecksum,
           &nerrors, &tmin, &tmax, &tsum, &tsum2, &pipesize, &cur_time);
}

int
gather_statistics(int *series, struct pingCtlTable_data *item, __u8 * ptr,
                  int cc, __u16 seq, int hops, int csfailed,
                  struct timeval *tv, time_t timep, int *rtt_addend,
                  int uid, int options, char *from, int interval,
                  int datalen, int timing, char *outpack, int *rtt,
                  __u16 * acked, long *nreceived, long *nrepeats,
                  long *ntransmitted, long *nchecksum, long *tmin,
                  long *tmax, long long *tsum, long long *tsum2,
                  int *confirm_flag, int *confirm, int *pipesize,
                  struct pingProbeHistoryTable_data *current_temp)
{
    int             dupflag = 0;
    long            triptime = 0;
    int             mx_dup_ck = MAX_DUP_CHK;

    netsnmp_variable_list *vars = NULL;
    struct pingResultsTable_data *StorageNew = NULL;
    struct pingProbeHistoryTable_data *temp = NULL;
    ++(*nreceived);
    *series = 0;
    if (!csfailed)
        acknowledge(seq, acked, ntransmitted, pipesize);

    if (timing && cc >= 8 + sizeof(struct timeval)) {
        struct timeval  tmp_tv;
        memcpy(&tmp_tv, ptr, sizeof(tmp_tv));

        tvsub(tv, &tmp_tv);
        triptime = tv->tv_sec * 1000000 + tv->tv_usec;
        if (triptime < 0) {
            snmp_log(LOG_INFO,
                     "Warning: invalid timestamp in ICMP response.\n");
            triptime = 0;
            if (!(options & F_LATENCY))
                options |= F_LATENCY;
        }
        if (!csfailed) {
            (*tsum) += triptime;
            (*tsum2) += (long long) triptime *(long long) triptime;
            if (triptime < (*tmin))
                (*tmin) = triptime;
            if (triptime > (*tmax))
                (*tmax) = triptime;
            if (!(*rtt))
                *rtt = triptime * 8;
            else
                *rtt += triptime - (*rtt) / 8;
            if (options & F_ADAPTIVE)
                update_interval(uid, interval, rtt_addend, rtt);
        }
    }

    if (csfailed) {
        ++(*nchecksum);
        --(*nreceived);
    } else if (TST(seq % mx_dup_ck)) {
        ++(*nrepeats);
        --(*nreceived);
        dupflag = 1;
    } else {
        SET(seq % mx_dup_ck);
        dupflag = 0;
    }
    *confirm = *confirm_flag;

    if (options & F_QUIET)
        return 1;

    if (options & F_FLOOD) {
        if (!csfailed)
            write(STDOUT_FILENO, "\b \b", 3);
        else
            write(STDOUT_FILENO, "\bC", 1);
    } else {
        int             i;
        __u8           *cp, *dp;

        DEBUGMSGTL(("pingCtlTable", "%d bytes from %s: icmp_seq=%u", cc, from,
                    seq));

        if (hops >= 0)
            DEBUGMSGTL(("pingCtlTable", " ttl=%d", hops));

        if (cc < datalen + 8) {
            DEBUGMSGTL(("pingCtlTable", " (truncated)\n"));
            return 1;
        }
        if (timing) {
            if (triptime >= 100000)
                DEBUGMSGTL(("pingCtlTable", " time=%ld ms", triptime / 1000));
            else if (triptime >= 10000)
                DEBUGMSGTL(("pingCtlTable", " time=%ld.%01ld ms",
                            triptime / 1000, (triptime % 1000) / 100));
            else if (triptime >= 1000)
                DEBUGMSGTL(("pingCtlTable", " time=%ld.%02ld ms",
                            triptime / 1000, (triptime % 1000) / 10));
            else
                DEBUGMSGTL(("pingCtlTable", " time=%ld.%03ld ms",
                            triptime / 1000, triptime % 1000));
        }
        if (dupflag)
            DEBUGMSGTL(("pingCtlTable", " (DUP!)"));
        if (csfailed)
            DEBUGMSGTL(("pingCtlTable", " (BAD CHECKSUM!)"));

        /*
         * check the data 
         */
        cp = ((u_char *) ptr) + sizeof(struct timeval);
        dp = (u_char *)&outpack[8 + sizeof(struct timeval)];
        for (i = sizeof(struct timeval); i < datalen; ++i, ++cp, ++dp) {
            if (*cp != *dp) {
                DEBUGMSGTL(("pingCtlTable",
                            "\nwrong data byte #%d should be 0x%x but was 0x%x",
                            i, *dp, *cp));
                cp = (u_char *) ptr + sizeof(struct timeval);
                for (i = sizeof(struct timeval); i < datalen; ++i, ++cp) {
                    if ((i % 32) == sizeof(struct timeval))
                        DEBUGMSGTL(("pingCtlTable", "\n#%d\t", i));
                    DEBUGMSGTL(("pingCtlTable", "%x ", *cp));
                }
                break;
            }
        }
    }


    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlOwnerIndex,
                              item->pingCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
                              item->pingCtlTestName,
                              item->pingCtlTestNameLen);

    StorageNew = header_complex_get(pingResultsTableStorage, vars);
    snmp_free_varbind(vars);
    if (!StorageNew)
        return SNMP_ERR_NOSUCHNAME;


    StorageNew->pingResultsMinRtt = *tmin;
    StorageNew->pingResultsMaxRtt = *tmax;
    StorageNew->pingResultsAverageRtt =
        (*tsum) / (StorageNew->pingResultsProbeResponses + 1);
    StorageNew->pingResultsProbeResponses =
        StorageNew->pingResultsProbeResponses + 1;
    StorageNew->pingResultsSendProbes =
        StorageNew->pingResultsSendProbes + 1;
    StorageNew->pingResultsRttSumOfSquares = *tsum2;

    StorageNew->pingResultsLastGoodProbe_time = timep;
    free(StorageNew->pingResultsLastGoodProbe);
    memdup(&StorageNew->pingResultsLastGoodProbe,
	date_n_time(&timep, &StorageNew->pingResultsLastGoodProbeLen), 11);

    /* ProbeHistory               */
    if (item->pingCtlMaxRows != 0) {
        temp = SNMP_MALLOC_STRUCT(pingProbeHistoryTable_data);

        temp->pingCtlOwnerIndex =
            (char *) malloc(item->pingCtlOwnerIndexLen + 1);
        memcpy(temp->pingCtlOwnerIndex, item->pingCtlOwnerIndex,
               item->pingCtlOwnerIndexLen + 1);
        temp->pingCtlOwnerIndex[item->pingCtlOwnerIndexLen] = '\0';
        temp->pingCtlOwnerIndexLen = item->pingCtlOwnerIndexLen;

        temp->pingCtlTestName =
            (char *) malloc(item->pingCtlTestNameLen + 1);
        memcpy(temp->pingCtlTestName, item->pingCtlTestName,
               item->pingCtlTestNameLen + 1);
        temp->pingCtlTestName[item->pingCtlTestNameLen] = '\0';
        temp->pingCtlTestNameLen = item->pingCtlTestNameLen;

        /* add lock to protect */
        pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&counter_mutex);
        if (item->pingProbeHistoryMaxIndex >= (unsigned long) (2147483647))
            item->pingProbeHistoryMaxIndex = 0;
        temp->pingProbeHistoryIndex = ++(item->pingProbeHistoryMaxIndex);
        pthread_mutex_unlock(&counter_mutex);
        /* end */


        temp->pingProbeHistoryResponse = triptime;
        temp->pingProbeHistoryStatus = 1;
        temp->pingProbeHistoryLastRC = 0;

	temp->pingProbeHistoryTime_time = timep;
	memdup(&temp->pingProbeHistoryTime,
	    date_n_time(&timep, &temp->pingProbeHistoryTimeLen), 11);

        if (StorageNew->pingResultsSendProbes == 1)
            item->pingProbeHis = temp;
        else {
            (current_temp)->next = temp;
        }

        current_temp = temp;

        if (StorageNew->pingResultsSendProbes >= item->pingCtlProbeCount) {
            current_temp->next = NULL;
        }

        if (item->pingProbeHis != NULL) {

            if (pingProbeHistoryTable_count(item) < item->pingCtlMaxRows) {
                if (pingProbeHistoryTable_add(current_temp) !=
                    SNMPERR_SUCCESS)
                    DEBUGMSGTL(("pingProbeHistoryTable",
                                "registered an entry error\n"));
            } else {
                pingProbeHistoryTable_delLast(item);

                if (pingProbeHistoryTable_add(current_temp) !=
                    SNMPERR_SUCCESS)
                    DEBUGMSGTL(("pingProbeHistoryTable",
                                "registered an entry error\n"));

            }

        }
    }
    return 0;
}

static long
llsqrt(long long a)
{
    long long       prev = ~((long long) 1 << 63);
    long long       x = a;

    if (x > 0) {
        while (x < prev) {
            prev = x;
            x = (x + (a / x)) / 2;
        }
    }

    return (long) x;
}

/*
 * finish --
 *      Print out statistics, and give up.
 */
void
finish(int options, char *hostname, int interval, int timing, int *rtt,
       struct timeval *start_time, int *deadline, long *npackets,
       long *nreceived, long *nrepeats, long *ntransmitted,
       long *nchecksum, long *nerrors, long *tmin, long *tmax,
       long long *tsum, long long *tsum2, int *pipesize,
       struct timeval *cur_time)
{
    struct timeval  tv = *cur_time;

    tvsub(&tv, start_time);

    putchar('\n');
    fflush(stdout);
    DEBUGMSGTL(("pingCtlTable", "--- %s ping statistics ---\n", hostname));

    if (*nrepeats)
        DEBUGMSGTL(("pingCtlTable", ", +%ld duplicates", *nrepeats));
    if (*nchecksum)
        DEBUGMSGTL(("pingCtlTable", ", +%ld corrupted", *nchecksum));
    if (*nerrors)
        DEBUGMSGTL(("pingCtlTable", ", +%ld errors", *nerrors));
    if (*ntransmitted) {
        DEBUGMSGTL(("pingCtlTable", ", %d%% loss",
                    (int) ((((long long) ((*ntransmitted) -
                                          (*nreceived))) * 100) /
                           (*ntransmitted))));
        DEBUGMSGTL(("pingCtlTable", ", time %ldms",
                    1000 * tv.tv_sec + tv.tv_usec / 1000));
    }
    putchar('\n');

    if ((*nreceived) && timing) {
        long            tmdev;

        (*tsum) /= (*nreceived) + (*nrepeats);
        (*tsum2) /= (*nreceived) + (*nrepeats);
        tmdev = llsqrt((*tsum2) - (*tsum) * (*tsum));

        DEBUGMSGTL(("pingCtlTable", "rtt min/avg/max/mdev = %ld.%03ld/%lu.%03ld"
                    "/%ld.%03ld/%ld.%03ld ms",
                    (*tmin) / 1000, (*tmin) % 1000,
                    (unsigned long) ((*tsum) / 1000), (long) ((*tsum) % 1000),
                    (*tmax) / 1000, (*tmax) % 1000, tmdev / 1000,
                    tmdev % 1000));
    }
    if ((*pipesize) > 1)
        DEBUGMSGTL(("pingCtlTable", ", pipe %d", *pipesize));
    if ((*ntransmitted) > 1
        && (!interval || (options & (F_FLOOD | F_ADAPTIVE)))) {
        int             ipg =
            (1000000 * (long long) tv.tv_sec +
             tv.tv_usec) / ((*ntransmitted) - 1);
        DEBUGMSGTL(("pingCtlTable", ", ipg/ewma %d.%03d/%d.%03d ms",
                    ipg / 1000, ipg % 1000,
                    (*rtt) / 8000, ((*rtt) / 8) % 1000));
    }
    putchar('\n');
    return;
    /* return(deadline ? (*nreceived)<(*npackets) : (*nreceived)==0); */
}


void
status(int timing, int *rtt, long *nreceived, long *nrepeats,
       long *ntransmitted, long *tmin, long *tmax, long long *tsum,
       long long *tsum2)
{
    int             loss = 0;
    long            tavg = 0;

    status_snapshot = 0;

    if (*ntransmitted)
        loss =
            (((long long) ((*ntransmitted) -
                           (*nreceived))) * 100) / (*ntransmitted);

    DEBUGMSGTL(("pingCtlTable", "\n%ld/%ld packets, %d%% loss", *ntransmitted,
                *nreceived, loss));

    if ((*nreceived) && timing) {
        tavg = (*tsum) / ((*nreceived) + (*nrepeats));

        DEBUGMSGTL(("pingCtlTable", ", min/avg/ewma/max = %ld.%03ld/%lu.%03ld"
                    "/%d.%03d/%ld.%03ld ms",
                    (*tmin) / 1000, (*tmin) % 1000, tavg / 1000, tavg % 1000,
                    (*rtt) / 8000, ((*rtt) / 8) % 1000, (*tmax) / 1000,
                    (*tmax) % 1000));
    }
    DEBUGMSGTL(("pingCtlTable", "\n"));
}


static __inline__ int
ipv6_addr_any(struct in6_addr *addr)
{
    static struct in6_addr in6_anyaddr;
    return (memcmp(addr, &in6_anyaddr, 16) == 0);
}

int
receive_error_msg(int icmp_sock, struct sockaddr_in6 *whereto, int options,
                  int *ident, long *nerrors)
{
    int             res;
    char            cbuf[512];
    struct iovec    iov;
    struct msghdr   msg;
    struct cmsghdr *cmsg;
    struct sock_extended_err *e;
    struct icmp6_hdr icmph;
    struct sockaddr_in6 target;
    int             net_errors = 0;
    int             local_errors = 0;
    int             saved_errno = errno;

    iov.iov_base = &icmph;
    iov.iov_len = sizeof(icmph);
    msg.msg_name = (void *) &target;
    msg.msg_namelen = sizeof(target);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    res = recvmsg(icmp_sock, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
    if (res < 0)
        goto out;

    e = NULL;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_IPV6) {
            if (cmsg->cmsg_type == IPV6_RECVERR)
                e = (struct sock_extended_err *) CMSG_DATA(cmsg);
        }
    }
    if (e == NULL)
        abort();

    if (e->ee_origin == SO_EE_ORIGIN_LOCAL) {
        local_errors++;
        if (options & F_QUIET)
            goto out;
        if (options & F_FLOOD)
            write(STDOUT_FILENO, "E", 1);
        else if (e->ee_errno != EMSGSIZE)
            snmp_log(LOG_ERR, "ping: local error: %s\n", strerror(e->ee_errno));
        else
            snmp_log(LOG_ERR, "ping: local error: Message too long, mtu=%u\n",
                     e->ee_info);
        (*nerrors)++;
    } else if (e->ee_origin == SO_EE_ORIGIN_ICMP6) {
        if (res < sizeof(icmph) ||
            memcmp(&target.sin6_addr, &(whereto->sin6_addr), 16) ||
            icmph.icmp6_type != ICMP6_ECHO_REQUEST ||
            icmph.icmp6_id != *ident) {
            /*
             * Not our error, not an error at all. Clear. 
             */
            saved_errno = 0;
            goto out;
        }

        net_errors++;
        (*nerrors)++;
        if (options & F_QUIET)
            goto out;
        if (options & F_FLOOD) {
            write(STDOUT_FILENO, "\bE", 2);
        } else {
            fflush(stdout);
        }
    }

  out:
    errno = saved_errno;
    return net_errors ? : -local_errors;
}

int
send_v6(int icmp_sock, int cmsglen, char *cmsgbuf,
        struct sockaddr_in6 *whereto, int datalen, int timing,
        char *outpack, int *ident, long *ntransmitted, int *confirm)
{
    struct icmp6_hdr *icmph;
    int             cc;
    int             i;
    int             mx_dup_ck = MAX_DUP_CHK;

    icmph = (struct icmp6_hdr *) outpack;
    icmph->icmp6_type = ICMP6_ECHO_REQUEST;
    icmph->icmp6_code = 0;
    icmph->icmp6_cksum = 0;
    icmph->icmp6_seq = (*ntransmitted) + 1;
    icmph->icmp6_id = *ident;

    CLR(icmph->icmp6_seq % mx_dup_ck);

    if (timing)
        gettimeofday((struct timeval *) &outpack[8],
                     (struct timezone *) NULL);

    cc = datalen + 8;           /* skips ICMP portion */

    if (cmsglen == 0) {
        i = sendto(icmp_sock, (char *) outpack, cc, *confirm,
                   (struct sockaddr *) whereto,
                   sizeof(struct sockaddr_in6));
    } else {
        struct msghdr   mhdr;
        struct iovec    iov;

        iov.iov_len = cc;
        iov.iov_base = outpack;

        mhdr.msg_name = whereto;
        mhdr.msg_namelen = sizeof(struct sockaddr_in6);
        mhdr.msg_iov = &iov;
        mhdr.msg_iovlen = 1;
        mhdr.msg_control = cmsgbuf;
        mhdr.msg_controllen = cmsglen;

        i = sendmsg(icmp_sock, &mhdr, *confirm);
    }
    *confirm = 0;

    return (cc == i ? 0 : i);
}

/*
 * parse_reply --
 *      Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
int
parse_reply(int *series, struct pingCtlTable_data *item,
            struct msghdr *msg, int cc, void *addr, struct timeval *tv,
            time_t timep, int uid, struct sockaddr_in6 *whereto,
            int *rtt_addend, int options, int interval, int datalen,
            int timing, int working_recverr, char *outpack, int *rtt,
            int *ident, __u16 * acked, long *nreceived, long *nrepeats,
            long *ntransmitted, long *nchecksum, long *nerrors, long *tmin,
            long *tmax, long long *tsum, long long *tsum2,
            int *confirm_flag, int *confirm, int *pipesize,
            struct pingProbeHistoryTable_data *current_temp)
{
    struct sockaddr_in6 *from = addr;
    __u8           *buf = msg->msg_iov->iov_base;
    struct cmsghdr *c;
    struct icmp6_hdr *icmph;
    int             hops = -1;


    for (c = CMSG_FIRSTHDR(msg); c; c = CMSG_NXTHDR(msg, c)) {
        if (c->cmsg_level != SOL_IPV6 || c->cmsg_type != IPV6_HOPLIMIT)
            continue;
        if (c->cmsg_len < CMSG_LEN(sizeof(int)))
            continue;
        hops = *(int *) CMSG_DATA(c);
    }


    /*
     * Now the ICMP part 
     */

    icmph = (struct icmp6_hdr *) buf;
    if (cc < 8) {
        if (options & F_VERBOSE)
            snmp_log(LOG_ERR, "ping: packet too short (%d bytes)\n", cc);
        return 1;
    }
    if (icmph->icmp6_type == ICMP6_ECHO_REPLY) {
        if (icmph->icmp6_id != *ident)
            return 1;
        if (gather_statistics(series, item, (__u8 *) (icmph + 1), cc,
                              icmph->icmp6_seq,
                              hops, 0, tv, timep, rtt_addend, uid, options,
                              pr_addr(&from->sin6_addr, options), interval,
                              datalen, timing, outpack, rtt, acked,
                              nreceived, nrepeats, ntransmitted, nchecksum,
                              tmin, tmax, tsum, tsum2, confirm_flag,
                              confirm, pipesize, current_temp))
            return 0;
    } else {
        int             nexthdr;
        struct ip6_hdr *iph1 = (struct ip6_hdr *) (icmph + 1);
        struct icmp6_hdr *icmph1 = (struct icmp6_hdr *) (iph1 + 1);

        /*
         * We must not ever fall here. All the messages but
         * * echo reply are blocked by filter and error are
         * * received with IPV6_RECVERR. Ugly code is preserved
         * * however, just to remember what crap we avoided
         * * using RECVRERR. :-)
         */

        if (cc < 8 + sizeof(struct ip6_hdr) + 8)
            return 1;

        if (memcmp(&iph1->ip6_dst, &(whereto->sin6_addr), 16))
            return 1;

        nexthdr = iph1->ip6_nxt;

        if (nexthdr == 44) {
            nexthdr = *(__u8 *) icmph1;
            icmph1++;
        }
        if (nexthdr == IPPROTO_ICMPV6) {
            if (icmph1->icmp6_type != ICMP6_ECHO_REQUEST ||
                icmph1->icmp6_id != *ident)
                return 1;
            acknowledge(icmph1->icmp6_seq, acked, ntransmitted,
                        pipesize);
            if (working_recverr)
                return 0;
            (*nerrors)++;
            if (options & F_FLOOD) {
                write(STDOUT_FILENO, "\bE", 2);
                return 0;
            }
            DEBUGMSGTL(("pingCtlTable", "From %s: icmp_seq=%u ",
                        pr_addr(&from->sin6_addr, options),
                        icmph1->icmp6_seq));
        } else {
            /*
             * We've got something other than an ECHOREPLY 
             */
            if (!(options & F_VERBOSE) || uid)
                return 1;
            DEBUGMSGTL(("pingCtlTable", "From %s: ",
                        pr_addr(&from->sin6_addr, options)));
        }
        /* pr_icmph(icmph->icmp6_type, icmph->icmp6_code, ntohl(icmph->icmp6_mtu)); */
    }

    if (!(options & F_FLOOD)) {
        if (options & F_AUDIBLE)
            putchar('\a');
        putchar('\n');
        fflush(stdout);
    }
    return 0;
}



#include <linux/filter.h>
void
install_filter(int icmp_sock, int *ident)
{
    static int      once;
    static struct sock_filter insns[] = {
        BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 4),  /* Load icmp echo ident */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0xAAAA, 0, 1),      /* Ours? */
        BPF_STMT(BPF_RET | BPF_K, ~0U), /* Yes, it passes. */
        BPF_STMT(BPF_LD | BPF_B | BPF_ABS, 0),  /* Load icmp type */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ICMP6_ECHO_REPLY, 1, 0),   /* Echo? */
        BPF_STMT(BPF_RET | BPF_K, ~0U), /* No. It passes. This must not happen. */
        BPF_STMT(BPF_RET | BPF_K, 0),   /* Echo with wrong ident. Reject. */
    };
    static struct sock_fprog filter = {
        sizeof insns / sizeof(insns[0]),
        insns
    };
    int id;

    if (once)
        return;
    once = 1;

    /*
     * Patch bpflet for current identifier. 
     */
    id = htons( *ident );
    insns[1] =
        (struct sock_filter) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                                      id, 0, 1);

    if (setsockopt
        (icmp_sock, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter)))
        perror("WARNING: failed to install socket filter\n");
}


/*
 * pr_addr --
 *      Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
static char    *
pr_addr(struct in6_addr *addr, int options)
{
    struct hostent *hp = NULL;

    if (!(options & F_NUMERIC))
        hp = netsnmp_gethostbyaddr((__u8 *) addr, sizeof(struct in6_addr),
                                   AF_INET6);

    return hp ? hp->h_name : pr_addr_n(addr);
}

static char    *
pr_addr_n(struct in6_addr *addr)
{
    static char     str[64];
    inet_ntop(AF_INET6, addr, str, sizeof(str));
    return str;
}
