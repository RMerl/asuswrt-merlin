/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:traceRouteCtlTable.c
 *File Description:Rows of traceRouteCtlTable MIB add delete ans read.
 *              Rows of traceRouteResultsTable MIB add and delete.
 *              Rows of traceRouteProbeHistoryTable MIB add and delete.
 *              Rows of traceRouteHopsTable MIB add and delete.
 *              The main function is also here.
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <pthread.h>
#include <math.h>

#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(header_complex_find_entry)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

#include "traceRouteCtlTable.h"
#include "traceRouteResultsTable.h"
#include "traceRouteProbeHistoryTable.h"
#include "traceRouteHopsTable.h"
#include "header_complex.h"

oid             traceRouteCtlTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 81, 1, 2 };

/* trap */
oid             traceRoutePathChange[] = { 1, 3, 6, 1, 2, 1, 81, 0, 1 };
oid             traceRouteTestFailed[] = { 1, 3, 6, 1, 2, 1, 81, 0, 2 };
oid             traceRouteTestCompleted[] = { 1, 3, 6, 1, 2, 1, 81, 0, 3 };

struct variable2 traceRouteCtlTable_variables[] = {
    /*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix 
     */

    {COLUMN_TRACEROUTECTLTARGETADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 3}},
    {COLUMN_TRACEROUTECTLTARGETADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 4}},
    {COLUMN_TRACEROUTECTLBYPASSROUTETABLE,  ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 5}},
    {COLUMN_TRACEROUTECTLDATASIZE,     ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 6}},
    {COLUMN_TRACEROUTECTLTIMEOUT,      ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 7}},
    {COLUMN_TRACEROUTECTLPROBESPERHOP, ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 8}},
    {COLUMN_TRACEROUTECTLPORT,         ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 9}},
    {COLUMN_TRACEROUTECTLMAXTTL,       ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 10}},
    {COLUMN_TRACEROUTECTLDSFIELD,      ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 11}},
    {COLUMN_TRACEROUTECTLSOURCEADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 12}},
    {COLUMN_TRACEROUTECTLSOURCEADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 13}},
    {COLUMN_TRACEROUTECTLIFINDEX,       ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 14}},
    {COLUMN_TRACEROUTECTLMISCOPTIONS, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 15}},
    {COLUMN_TRACEROUTECTLMAXFAILURES,  ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 16}},
    {COLUMN_TRACEROUTECTLDONTFRAGMENT,  ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 17}},
    {COLUMN_TRACEROUTECTLINITIALTTL,   ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 18}},
    {COLUMN_TRACEROUTECTLFREQUENCY,    ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 19}},
    {COLUMN_TRACEROUTECTLSTORAGETYPE,   ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 20}},
    {COLUMN_TRACEROUTECTLADMINSTATUS,   ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 21}},
    {COLUMN_TRACEROUTECTLDESCR,       ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 22}},
    {COLUMN_TRACEROUTECTLMAXROWS,      ASN_UNSIGNED, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 23}},
    {COLUMN_TRACEROUTECTLTRAPGENERATION,  ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 24}},
    {COLUMN_TRACEROUTECTLCREATEHOPSENTRIES, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 25}},
    {COLUMN_TRACEROUTECTLTYPE,        ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 26}},
    {COLUMN_TRACEROUTECTLROWSTATUS,     ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_traceRouteCtlTable, 2, {1, 27}}

};

/*
 * global storage of our data, saved in and configured by header_complex() 
 */

struct header_complex_index *traceRouteCtlTableStorage = NULL;
struct header_complex_index *traceRouteResultsTableStorage = NULL;
struct header_complex_index *traceRouteProbeHistoryTableStorage = NULL;
struct header_complex_index *traceRouteHopsTableStorage = NULL;

int
traceRouteResultsTable_add(struct traceRouteCtlTable_data *thedata);
int
traceRouteResultsTable_del(struct traceRouteCtlTable_data *thedata);

void
init_traceRouteCtlTable(void)
{
    DEBUGMSGTL(("traceRouteCtlTable", "initializing...  "));
    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("traceRouteCtlTable", traceRouteCtlTable_variables,
                 variable2, traceRouteCtlTable_variables_oid);

    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("traceRouteCtlTable",
                                  parse_traceRouteCtlTable, NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_traceRouteCtlTable, NULL);

    DEBUGMSGTL(("traceRouteCtlTable", "done.\n"));
}


void
init_trResultsTable(struct traceRouteCtlTable_data *item)
{
    struct traceRouteResultsTable_data *StorageTmp = NULL;
    netsnmp_variable_list *vars = NULL;
    char           *host = NULL;

    host =
        (char *) malloc(sizeof(char) *
                        (item->traceRouteCtlTargetAddressLen + 1));

    if (host == NULL) {
        DEBUGMSGTL(("traceRouteCtlTable", "host calloc %s\n",
                    strerror(errno)));
        exit(1);
    }

    memset(host, '\0',
	   sizeof(char) * (item->traceRouteCtlTargetAddressLen + 1));
    strcpy(host, item->traceRouteCtlTargetAddress);
    host[item->traceRouteCtlTargetAddressLen] = '\0';

    StorageTmp = SNMP_MALLOC_STRUCT(traceRouteResultsTable_data);
    if (StorageTmp == NULL) {
        DEBUGMSGTL(("traceRouteCtlTable", "StorageTmp malloc %s\n",
                    strerror(errno)));
        exit(1);
    }

    StorageTmp->traceRouteCtlOwnerIndex =
        (char *) malloc(sizeof(char) *
                        (item->traceRouteCtlOwnerIndexLen + 1));
    if (StorageTmp->traceRouteCtlOwnerIndex == NULL) {
        DEBUGMSGTL(("traceRouteCtlTable",
                    "traceRouteCtlOwnerIndex malloc %s\n",
                    strerror(errno)));
        exit(1);
    }

    memcpy(StorageTmp->traceRouteCtlOwnerIndex,
           item->traceRouteCtlOwnerIndex,
           item->traceRouteCtlOwnerIndexLen + 1);
    StorageTmp->traceRouteCtlOwnerIndex[item->traceRouteCtlOwnerIndexLen] =
        '\0';
    StorageTmp->traceRouteCtlOwnerIndexLen =
        item->traceRouteCtlOwnerIndexLen;

    StorageTmp->traceRouteCtlTestName =
        (char *) malloc(sizeof(char) *
                        (item->traceRouteCtlTestNameLen + 1));
    if (StorageTmp->traceRouteCtlTestName == NULL) {
        DEBUGMSGTL(("traceRouteCtlTable",
                    "traceRouteCtlTestName malloc %s\n", strerror(errno)));
        exit(1);
    }

    memcpy(StorageTmp->traceRouteCtlTestName, item->traceRouteCtlTestName,
           item->traceRouteCtlTestNameLen + 1);
    StorageTmp->traceRouteCtlTestName[item->traceRouteCtlTestNameLen] =
        '\0';
    StorageTmp->traceRouteCtlTestNameLen = item->traceRouteCtlTestNameLen;

    StorageTmp->traceRouteResultsOperStatus = 1;

    if (item->traceRouteCtlTargetAddressType == 1
        || item->traceRouteCtlTargetAddressType == 16) {
        struct sockaddr whereto;        /* Who to try to reach */
        register struct sockaddr_in *to = (struct sockaddr_in *) &whereto;
        register struct hostinfo *hi = NULL;
        hi = gethostinfo(host);
        if (hi == NULL) {
            DEBUGMSGTL(("traceRouteCtlTable", "hi calloc %s\n",
                        strerror(errno)));
            exit(1);
        }

        setsin(to, hi->addrs[0]);
        if (inet_ntoa(to->sin_addr) == NULL) {
            StorageTmp->traceRouteResultsIpTgtAddrType = 0;
            StorageTmp->traceRouteResultsIpTgtAddr = strdup("");
            StorageTmp->traceRouteResultsIpTgtAddrLen = 0;
        } else {
            StorageTmp->traceRouteResultsIpTgtAddrType = 1;
            StorageTmp->traceRouteResultsIpTgtAddr =
                (char *) malloc(sizeof(char) *
                                (strlen(inet_ntoa(to->sin_addr)) + 1));
            if (StorageTmp->traceRouteResultsIpTgtAddr == NULL) {
                DEBUGMSGTL(("traceRouteCtlTable",
                            "traceRouteResultsIpTgtAddr malloc %s\n",
                            strerror(errno)));
                exit(1);
            }

            memcpy(StorageTmp->traceRouteResultsIpTgtAddr,
                   inet_ntoa(to->sin_addr),
                   strlen(inet_ntoa(to->sin_addr)) + 1);
            StorageTmp->
                traceRouteResultsIpTgtAddr[strlen(inet_ntoa(to->sin_addr))]
                = '\0';
            StorageTmp->traceRouteResultsIpTgtAddrLen =
                strlen(inet_ntoa(to->sin_addr));
        }
    }
    if (item->traceRouteCtlTargetAddressType == 2) {

        struct sockaddr_in6 whereto;    /* Who to try to reach */
        register struct sockaddr_in6 *to =
            (struct sockaddr_in6 *) &whereto;
        struct hostent *hp = NULL;
        /* struct hostenv hp; */
        char            pa[64];
        memset(pa, '\0', 64);

        to->sin6_family = AF_INET6;
        to->sin6_port = htons(33434);

        if (inet_pton(AF_INET6, host, &to->sin6_addr) > 0) {
            StorageTmp->traceRouteResultsIpTgtAddrType = 2;
            StorageTmp->traceRouteResultsIpTgtAddr =
                (char *) malloc(sizeof(char) * (strlen(host) + 1));
            if (StorageTmp->traceRouteResultsIpTgtAddr == NULL) {
                DEBUGMSGTL(("traceRouteCtlTable",
                            "traceRouteResultsIpTgtAddr malloc %s\n",
                            strerror(errno)));
                exit(1);
            }
            memset(StorageTmp->traceRouteResultsIpTgtAddr, '\0',
                  sizeof(char) * (strlen(host) + 1));
            memcpy(StorageTmp->traceRouteResultsIpTgtAddr, host,
                   strlen(host) + 1);
            StorageTmp->traceRouteResultsIpTgtAddr[strlen(host)] = '\0';
            StorageTmp->traceRouteResultsIpTgtAddrLen = strlen(host);
        } else {
            hp = gethostbyname2(host, AF_INET6);
            if (hp != NULL) {
                const char     *hostname;
                memmove((caddr_t) & to->sin6_addr, hp->h_addr, 16);
                hostname = inet_ntop(AF_INET6, &to->sin6_addr, pa, 64);
                StorageTmp->traceRouteResultsIpTgtAddrType = 2;
                StorageTmp->traceRouteResultsIpTgtAddr =
                    (char *) malloc(sizeof(char) * (strlen(hostname) + 1));
                if (StorageTmp->traceRouteResultsIpTgtAddr == NULL) {
                    DEBUGMSGTL(("traceRouteCtlTable",
                                "traceRouteResultsIpTgtAddr malloc %s\n",
                                strerror(errno)));
                    exit(1);
                }
                memset(StorageTmp->traceRouteResultsIpTgtAddr, '\0',
                      sizeof(char) * (strlen(host) + 1));
                memcpy(StorageTmp->traceRouteResultsIpTgtAddr, hostname,
                       strlen(hostname) + 1);
                StorageTmp->traceRouteResultsIpTgtAddr[strlen(hostname)] =
                    '\0';
                StorageTmp->traceRouteResultsIpTgtAddrLen =
                    strlen(hostname);
            } else {
                DEBUGMSGTL(("traceRouteCtlTable",
                            "traceroute: unknown host %s\n", host));

                StorageTmp->traceRouteResultsIpTgtAddrType = 0;
                StorageTmp->traceRouteResultsIpTgtAddr = strdup("");
                StorageTmp->traceRouteResultsIpTgtAddrLen = 0;
            }
        }
    }

    StorageTmp->traceRouteResultsCurHopCount = 0;
    StorageTmp->traceRouteResultsCurProbeCount = 0;
    StorageTmp->traceRouteResultsTestAttempts = 0;
    StorageTmp->traceRouteResultsTestSuccesses = 0;

    StorageTmp->traceRouteResultsLastGoodPathLen = 0;

    item->traceRouteResults = StorageTmp;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen); /*  traceRouteCtlOwnerIndex  */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen);     /*  traceRouteCtlTestName  */
    if ((header_complex_get(traceRouteResultsTableStorage, vars)) != NULL) {
        traceRouteResultsTable_del(item);
    }
    snmp_free_varbind(vars);
    vars = NULL;
    if (item->traceRouteResults != NULL) {
        if (traceRouteResultsTable_add(item) != SNMPERR_SUCCESS) {
            DEBUGMSGTL(("traceRouteResultsTable",
                        "init an entry error\n"));
        }
    }

}



int
modify_trResultsOper(struct traceRouteCtlTable_data *thedata, long val)
{
    netsnmp_variable_list *vars = NULL;
    struct traceRouteResultsTable_data *StorageTmp = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlTestName */

    if ((StorageTmp =
         header_complex_get(traceRouteResultsTableStorage,
                            vars)) == NULL) {
        snmp_free_varbind(vars);
        vars = NULL;
        return SNMP_ERR_NOSUCHNAME;
    } else {
        StorageTmp->traceRouteResultsOperStatus = val;
        DEBUGMSGTL(("traceRouteResultsOperStatus", "done.\n"));
        snmp_free_varbind(vars);
        vars = NULL;
        return SNMPERR_SUCCESS;
    }
}


struct traceRouteCtlTable_data *
create_traceRouteCtlTable_data(void)
{
    struct traceRouteCtlTable_data *StorageNew = NULL;
    StorageNew = SNMP_MALLOC_STRUCT(traceRouteCtlTable_data);
    if (StorageNew == NULL) {
        exit(1);
    }
    StorageNew->traceRouteCtlTargetAddressType = 1;
    StorageNew->traceRouteCtlTargetAddress = strdup("");
    StorageNew->traceRouteCtlTargetAddressLen = 0;
    StorageNew->traceRouteCtlByPassRouteTable = 2;
    StorageNew->traceRouteCtlDataSize = 0;
    StorageNew->traceRouteCtlTimeOut = 3;
    StorageNew->traceRouteCtlProbesPerHop = 3;
    StorageNew->traceRouteCtlPort = 33434;
    StorageNew->traceRouteCtlMaxTtl = 30;
    StorageNew->traceRouteCtlDSField = 0;
    StorageNew->traceRouteCtlSourceAddressType = 0;
    StorageNew->traceRouteCtlSourceAddress = strdup("");
    StorageNew->traceRouteCtlSourceAddressLen = 0;
    StorageNew->traceRouteCtlIfIndex = 0;
    StorageNew->traceRouteCtlMiscOptions = strdup("");
    StorageNew->traceRouteCtlMiscOptionsLen = 0;
    StorageNew->traceRouteCtlMaxFailures = 5;
    StorageNew->traceRouteCtlDontFragment = 2;
    StorageNew->traceRouteCtlInitialTtl = 1;
    StorageNew->traceRouteCtlFrequency = 0;
    StorageNew->traceRouteCtlStorageType = ST_NONVOLATILE;
    StorageNew->traceRouteCtlAdminStatus = 2;
    StorageNew->traceRouteCtlDescr = (char *) malloc(strlen("00") + 1);
    if (StorageNew->traceRouteCtlDescr == NULL) {
        exit(1);
    }
    memcpy(StorageNew->traceRouteCtlDescr, "00", strlen("00") + 1);
    StorageNew->traceRouteCtlDescr[strlen("00")] = '\0';
    StorageNew->traceRouteCtlDescrLen =
        strlen(StorageNew->traceRouteCtlDescr);

    StorageNew->traceRouteCtlMaxRows = 50;
    StorageNew->traceRouteCtlTrapGeneration = strdup("");
    StorageNew->traceRouteCtlTrapGenerationLen = 0;
    StorageNew->traceRouteCtlCreateHopsEntries = 2;

    StorageNew->traceRouteCtlType = calloc(1, sizeof(oid) * sizeof(2)); /* 0.0 */
    StorageNew->traceRouteCtlTypeLen = 2;

    StorageNew->traceRouteResults = NULL;
    StorageNew->traceRouteProbeHis = NULL;
    StorageNew->traceRouteHops = NULL;

    StorageNew->storageType = ST_NONVOLATILE;
    /* StorageNew->traceRouteProbeHistoryMaxIndex=0; */
    return StorageNew;
}


/*
 * traceRouteCtlTable_add(): adds a structure node to our data set 
 */
int
traceRouteCtlTable_add(struct traceRouteCtlTable_data *thedata)
{
    netsnmp_variable_list *vars = NULL;


    DEBUGMSGTL(("traceRouteCtlTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */


    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlOperationName */

    if (header_complex_add_data(&traceRouteCtlTableStorage, vars, thedata)
        == NULL) {
        vars = NULL;
        return SNMPERR_GENERR;
    } else {

        DEBUGMSGTL(("traceRouteCtlTable", "registered an entry\n"));


        DEBUGMSGTL(("traceRouteCtlTable", "done.\n"));
        vars = NULL;
        return SNMPERR_SUCCESS;
    }
}

int
traceRouteResultsTable_add(struct traceRouteCtlTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;
    struct traceRouteResultsTable_data *p = NULL;
    p = thedata->traceRouteResults;
    if (thedata->traceRouteResults != NULL) {
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) p->traceRouteCtlOwnerIndex, p->traceRouteCtlOwnerIndexLen);      /* traceRouteCtlOwnerIndex */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) p->traceRouteCtlTestName, p->traceRouteCtlTestNameLen);  /* traceRouteCtlTestName */
        DEBUGMSGTL(("traceRouteResultsTable", "adding data...  "));
        /*
         * add the index variables to the varbind list, which is 
         * used by header_complex to index the data 
         */

        header_complex_add_data(&traceRouteResultsTableStorage, vars_list,
                                p);
        DEBUGMSGTL(("traceRouteResultsTable", "out finished\n"));
        vars_list = NULL;
        DEBUGMSGTL(("traceRouteResultsTable", "done.\n"));
        return SNMPERR_SUCCESS;
    } else {
        vars_list = NULL;
        DEBUGMSGTL(("traceRouteResultsTable", "error.\n"));
        return SNMP_ERR_INCONSISTENTNAME;
    }
}


int
traceRouteProbeHistoryTable_add(struct traceRouteProbeHistoryTable_data
                                *thedata)
{
    netsnmp_variable_list *vars_list = NULL;
    if (thedata != NULL) {
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);  /* traceRouteCtlOwnerIndex */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);      /* traceRouteCtlTestName */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteProbeHistoryIndex, sizeof(thedata->traceRouteProbeHistoryIndex));     /* traceRouteProbeHistoryIndex */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteProbeHistoryHopIndex, sizeof(thedata->traceRouteProbeHistoryHopIndex));       /* traceRouteProbeHistoryHopIndex */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteProbeHistoryProbeIndex, sizeof(thedata->traceRouteProbeHistoryProbeIndex));   /* traceRouteProbeHistoryProbeIndex */

        DEBUGMSGTL(("traceRouteProbeHistoryTable", "adding data...  "));
        /*
         * add the index variables to the varbind list, which is 
         * used by header_complex to index the data 
         */

        if (header_complex_add_data
            (&traceRouteProbeHistoryTableStorage, vars_list,
             thedata) == NULL) {
            vars_list = NULL;
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            DEBUGMSGTL(("traceRouteProbeHistoryTable", "out finished\n"));

            vars_list = NULL;

            DEBUGMSGTL(("traceRouteProbeHistoryTable", "done.\n"));
            return SNMPERR_SUCCESS;
        }
    } else {
        return SNMP_ERR_INCONSISTENTNAME;
    }
}

int
traceRouteProbeHistoryTable_addall(struct traceRouteCtlTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;
    struct traceRouteProbeHistoryTable_data *p = NULL;
    p = thedata->traceRouteProbeHis;
    if (thedata->traceRouteProbeHis != NULL)
        do {
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) p->traceRouteCtlOwnerIndex, p->traceRouteCtlOwnerIndexLen);  /* traceRouteCtlOwnerIndex */
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) p->traceRouteCtlTestName, p->traceRouteCtlTestNameLen);      /* traceRouteCtlTestName */
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &p->traceRouteProbeHistoryIndex, sizeof(p->traceRouteProbeHistoryIndex));     /* traceRouteProbeHistoryIndex */
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &p->traceRouteProbeHistoryHopIndex, sizeof(p->traceRouteProbeHistoryHopIndex));       /* traceRouteProbeHistoryHopIndex */
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &p->traceRouteProbeHistoryProbeIndex, sizeof(p->traceRouteProbeHistoryProbeIndex));   /* traceRouteProbeHistoryProbeIndex */

            DEBUGMSGTL(("traceRouteProbeHistoryTable",
                        "adding data...  "));
            /*
             * add the index variables to the varbind list, which is 
             * used by header_complex to index the data 
             */

            if (header_complex_add_data
                (&traceRouteProbeHistoryTableStorage, vars_list,
                 p) == NULL) {
                vars_list = NULL;
                return SNMP_ERR_INCONSISTENTNAME;
            } else {

                struct header_complex_index *temp = NULL;
                temp = traceRouteProbeHistoryTableStorage;
                if (traceRouteProbeHistoryTableStorage != NULL)
                    do {
                        DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                    "adding data,vars_oid="));
                        DEBUGMSGOID(("traceRouteProbeHistoryTable",
                                    temp->name, temp->namelen));
                        DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                    "\n "));
                        temp = temp->next;
                    } while (temp != NULL);

                DEBUGMSGTL(("traceRouteProbeHistoryTable",
                            "out finished\n"));
                DEBUGMSGTL(("traceRouteProbeHistoryTable", "done.\n"));
                vars_list = NULL;
                return SNMPERR_SUCCESS;
            }

            p = p->next;
        } while (p != NULL);
    else {
        return SNMP_ERR_INCONSISTENTNAME;
    }

}



int
traceRouteHopsTable_add(struct traceRouteHopsTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;

    if (thedata != NULL) {
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);  /* traceRouteCtlOwnerIndex */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);      /* traceRouteCtlTestName */
        snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->traceRouteHopsHopIndex, sizeof(thedata->traceRouteHopsHopIndex));       /* traceRouteHopsHopIndex */

        DEBUGMSGTL(("traceRouteHopsTable", "adding data...  "));
        /*
         * add the index variables to the varbind list, which is 
         * used by header_complex to index the data 
         */

        if (header_complex_add_data
            (&traceRouteHopsTableStorage, vars_list, thedata) == NULL) {
            vars_list = NULL;
            return SNMP_ERR_INCONSISTENTNAME;
        } else {
            DEBUGMSGTL(("traceRouteHopsTable", "out finished\n"));
            DEBUGMSGTL(("traceRouteHopsTable", "done.\n"));
            vars_list = NULL;
            return SNMPERR_SUCCESS;
        }
    }
    return SNMPERR_GENERR;
}

int
traceRouteHopsTable_addall(struct traceRouteCtlTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;
    struct traceRouteHopsTable_data *p = NULL;
    vars_list = NULL;
    p = thedata->traceRouteHops;
    if (thedata->traceRouteHops != NULL) {
        do {
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) p->traceRouteCtlOwnerIndex, p->traceRouteCtlOwnerIndexLen);  /* traceRouteCtlOwnerIndex */
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) p->traceRouteCtlTestName, p->traceRouteCtlTestNameLen);      /* traceRouteCtlTestName */
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &p->traceRouteHopsHopIndex, sizeof(p->traceRouteHopsHopIndex));       /* traceRouteHopsHopIndex */

            DEBUGMSGTL(("traceRouteHopsTable", "adding data...  "));
            /*
             * add the index variables to the varbind list, which is 
             * used by header_complex to index the data 
             */

            if (header_complex_add_data
                (&traceRouteHopsTableStorage, vars_list, p) == NULL) {
                vars_list = NULL;
                return SNMP_ERR_INCONSISTENTNAME;
            } else {

                struct header_complex_index *temp = NULL;
                temp = traceRouteHopsTableStorage;
                if (traceRouteHopsTableStorage != NULL)
                    do {
                        DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                    "adding data,vars_oid="));
                        DEBUGMSGOID(("traceRouteProbeHistoryTable",
                                    temp->name, temp->namelen));
                        DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                    "\n "));
                        temp = temp->next;
                    } while (temp != NULL);
                DEBUGMSGTL(("traceRouteHopsTable", "out finished\n"));

                vars_list = NULL;
            }
            p = p->next;
        } while (p != NULL);
        DEBUGMSGTL(("traceRouteHopsTable", "done.\n"));
        return SNMPERR_SUCCESS;
    } else {
        return SNMP_ERR_INCONSISTENTNAME;
    }

}


unsigned long
traceRouteProbeHistoryTable_count(struct traceRouteCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;
    unsigned long   count = 0;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlTestName */

    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);

    vars = NULL;
    for (hciptr2 = traceRouteProbeHistoryTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            count = count + 1;
        }
    }
    return count;
}



unsigned long
traceRouteHopsTable_count(struct traceRouteCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;
    unsigned long   count = 0;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlOperationName */

    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);

    vars = NULL;
    for (hciptr2 = traceRouteHopsTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            count = count + 1;
        }
    }
    return count;
}



void
traceRouteProbeHistoryTable_delLast(struct traceRouteCtlTable_data
                                    *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    struct header_complex_index *hcilast = NULL;
    struct traceRouteProbeHistoryTable_data *StorageTmp = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len = 0;
    time_t          last_time = 2147483647;
    time_t          tp;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlOperationName */

    memset(newoid, '\0', sizeof(oid) * MAX_OID_LEN);
    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);

    for (hcilast = hciptr2 = traceRouteProbeHistoryTableStorage;
         hciptr2 != NULL; hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {

            StorageTmp =
                header_complex_get_from_oid
                (traceRouteProbeHistoryTableStorage, hciptr2->name,
                 hciptr2->namelen);
            tp = StorageTmp->traceRouteProbeHistoryTime_time;

            if (last_time > tp) {
                last_time = tp;
                hcilast = hciptr2;
            }

        }
    }
    header_complex_extract_entry(&traceRouteProbeHistoryTableStorage, hcilast);
    DEBUGMSGTL(("traceRouteProbeHistoryTable",
                "delete the last one success!\n"));
    vars = NULL;
}



void
traceRouteCtlTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr = NULL;
    struct traceRouteCtlTable_data *StorageDel = NULL;
    DEBUGMSGTL(("traceRouteCtlTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        StorageDel =
            header_complex_extract_entry(&traceRouteCtlTableStorage,
                                         hciptr);
        if (StorageDel != NULL) {
            free(StorageDel->traceRouteCtlOwnerIndex);
            StorageDel->traceRouteCtlOwnerIndex = NULL;
            free(StorageDel->traceRouteCtlTestName);
            StorageDel->traceRouteCtlTestName = NULL;
            free(StorageDel->traceRouteCtlTargetAddress);
            StorageDel->traceRouteCtlTargetAddress = NULL;
            free(StorageDel->traceRouteCtlSourceAddress);
            StorageDel->traceRouteCtlSourceAddress = NULL;
            free(StorageDel->traceRouteCtlMiscOptions);
            StorageDel->traceRouteCtlMiscOptions = NULL;
            free(StorageDel->traceRouteCtlDescr);
            StorageDel->traceRouteCtlDescr = NULL;
            free(StorageDel->traceRouteCtlTrapGeneration);
            StorageDel->traceRouteCtlTrapGeneration = NULL;
            free(StorageDel->traceRouteCtlType);
            StorageDel->traceRouteCtlType = NULL;
            free(StorageDel);
            StorageDel = NULL;

        }
        DEBUGMSGTL(("traceRouteCtlTable", "cleaner  "));
    }
}


/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */
void
parse_traceRouteCtlTable(const char *token, char *line)
{
    size_t          tmpint;
    struct traceRouteCtlTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(traceRouteCtlTable_data);

    DEBUGMSGTL(("traceRouteCtlTable", "parsing config...  "));


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
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlTargetAddressType,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlTargetAddress,
                              &StorageTmp->traceRouteCtlTargetAddressLen);
    if (StorageTmp->traceRouteCtlTargetAddress == NULL) {
        config_perror
            ("invalid specification for traceRouteCtlTargetAddress");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlByPassRouteTable,
                              &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlDataSize, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlTimeOut, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlProbesPerHop,
                              &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlPort, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlMaxTtl, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlDSField, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlSourceAddressType,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlSourceAddress,
                              &StorageTmp->traceRouteCtlSourceAddressLen);
    if (StorageTmp->traceRouteCtlSourceAddress == NULL) {
        config_perror
            ("invalid specification for traceRouteCtlSourceAddress");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlIfIndex, &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlMiscOptions,
                              &StorageTmp->traceRouteCtlMiscOptionsLen);
    if (StorageTmp->traceRouteCtlMiscOptions == NULL) {
        config_perror
            ("invalid specification for traceRouteCtlMiscOptions");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlMaxFailures,
                              &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlDontFragment,
                              &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlInitialTtl,
                              &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlFrequency,
                              &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlStorageType,
                              &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlAdminStatus,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlDescr,
                              &StorageTmp->traceRouteCtlDescrLen);
    if (StorageTmp->traceRouteCtlDescr == NULL) {
        config_perror("invalid specification for traceRouteCtlTrapDescr");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteCtlMaxRows, &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->traceRouteCtlTrapGeneration,
                              &StorageTmp->traceRouteCtlTrapGenerationLen);
    if (StorageTmp->traceRouteCtlTrapGeneration == NULL) {
        config_perror
            ("invalid specification for traceRouteCtlTrapGeneration");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlCreateHopsEntries,
                              &tmpint);

    line =
        read_config_read_data(ASN_OBJECT_ID, line,
                              &StorageTmp->traceRouteCtlType,
                              &StorageTmp->traceRouteCtlTypeLen);
    if (StorageTmp->traceRouteCtlType == NULL) {
        config_perror("invalid specification for traceRouteCtlType");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->traceRouteCtlRowStatus,
                              &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->traceRouteProbeHistoryMaxIndex,
                              &tmpint);

    StorageTmp->storageType = ST_NONVOLATILE;
    traceRouteCtlTable_add(StorageTmp);
    /*     traceRouteCtlTable_cleaner(traceRouteCtlTableStorage); */

    DEBUGMSGTL(("traceRouteCtlTable", "done.\n"));
}



/*
 * store_traceRouteCtlTable():
 *   stores .conf file entries needed to configure the mib.
 */
int
store_traceRouteCtlTable(int majorID, int minorID, void *serverarg,
                         void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr = NULL;
    size_t          tmpint;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    struct header_complex_index *hcindex = NULL;


    DEBUGMSGTL(("traceRouteCtlTable", "storing data...  "));


    for (hcindex = traceRouteCtlTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct traceRouteCtlTable_data *) hcindex->data;

        if (StorageTmp->storageType != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "traceRouteCtlTable ");
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
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlTargetAddressType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteCtlTargetAddress,
                                       &StorageTmp->
                                       traceRouteCtlTargetAddressLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlByPassRouteTable,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlDataSize,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlTimeOut,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteCtlProbesPerHop, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlPort,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlMaxTtl,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlDSField,
                                       &tmpint);

            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlSourceAddressType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteCtlSourceAddress,
                                       &StorageTmp->
                                       traceRouteCtlSourceAddressLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->traceRouteCtlIfIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteCtlMiscOptions,
                                       &StorageTmp->
                                       traceRouteCtlMiscOptionsLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteCtlMaxFailures, &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlDontFragment, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteCtlInitialTtl, &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlFrequency,
                                       &tmpint);

            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlStorageType, &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlAdminStatus, &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->traceRouteCtlDescr,
                                       &StorageTmp->traceRouteCtlDescrLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->traceRouteCtlMaxRows,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->
                                       traceRouteCtlTrapGeneration,
                                       &StorageTmp->
                                       traceRouteCtlTrapGenerationLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       traceRouteCtlCreateHopsEntries,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OBJECT_ID, cptr,
                                       &StorageTmp->traceRouteCtlType,
                                       &StorageTmp->traceRouteCtlTypeLen);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->traceRouteCtlRowStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       traceRouteProbeHistoryMaxIndex,
                                       &tmpint);



            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("traceRouteCtlTable", "done.\n"));
    return SNMPERR_SUCCESS;
}




/*
 * var_traceRouteCtlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_mteObjectsTable above.
 */
unsigned char  *
var_traceRouteCtlTable(struct variable *vp,
                       oid * name,
                       size_t *length,
                       int exact,
                       size_t *var_len, WriteMethod ** write_method)
{

    struct traceRouteCtlTable_data *StorageTmp = NULL;

    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, vp, name, length, exact,
                        var_len, write_method)) == NULL) {
        if (vp->magic == COLUMN_TRACEROUTECTLROWSTATUS)
            *write_method = write_traceRouteCtlRowStatus;

        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */
    switch (vp->magic) {


    case COLUMN_TRACEROUTECTLTARGETADDRESSTYPE:
        *write_method = write_traceRouteCtlTargetAddressType;
        *var_len = sizeof(StorageTmp->traceRouteCtlTargetAddressType);

        return (u_char *) & StorageTmp->traceRouteCtlTargetAddressType;

    case COLUMN_TRACEROUTECTLTARGETADDRESS:
        *write_method = write_traceRouteCtlTargetAddress;
        *var_len = (StorageTmp->traceRouteCtlTargetAddressLen);

        return (u_char *) StorageTmp->traceRouteCtlTargetAddress;

    case COLUMN_TRACEROUTECTLBYPASSROUTETABLE:
        *write_method = write_traceRouteCtlByPassRouteTable;
        *var_len = sizeof(StorageTmp->traceRouteCtlByPassRouteTable);

        return (u_char *) & StorageTmp->traceRouteCtlByPassRouteTable;

    case COLUMN_TRACEROUTECTLDATASIZE:
        *write_method = write_traceRouteCtlDataSize;
        *var_len = sizeof(StorageTmp->traceRouteCtlDataSize);

        return (u_char *) & StorageTmp->traceRouteCtlDataSize;

    case COLUMN_TRACEROUTECTLTIMEOUT:
        *write_method = write_traceRouteCtlTimeOut;
        *var_len = sizeof(StorageTmp->traceRouteCtlTimeOut);

        return (u_char *) & StorageTmp->traceRouteCtlTimeOut;

    case COLUMN_TRACEROUTECTLPROBESPERHOP:
        *write_method = write_traceRouteCtlProbesPerHop;
        *var_len = sizeof(StorageTmp->traceRouteCtlProbesPerHop);

        return (u_char *) & StorageTmp->traceRouteCtlProbesPerHop;

    case COLUMN_TRACEROUTECTLPORT:
        *write_method = write_traceRouteCtlPort;
        *var_len = sizeof(StorageTmp->traceRouteCtlPort);

        return (u_char *) & StorageTmp->traceRouteCtlPort;

    case COLUMN_TRACEROUTECTLMAXTTL:
        *write_method = write_traceRouteCtlMaxTtl;
        *var_len = sizeof(StorageTmp->traceRouteCtlMaxTtl);

        return (u_char *) & StorageTmp->traceRouteCtlMaxTtl;

    case COLUMN_TRACEROUTECTLDSFIELD:
        *write_method = write_traceRouteCtlDSField;
        *var_len = sizeof(StorageTmp->traceRouteCtlDSField);

        return (u_char *) & StorageTmp->traceRouteCtlDSField;

    case COLUMN_TRACEROUTECTLSOURCEADDRESSTYPE:
        *write_method = write_traceRouteCtlSourceAddressType;
        *var_len = sizeof(StorageTmp->traceRouteCtlSourceAddressType);

        return (u_char *) & StorageTmp->traceRouteCtlSourceAddressType;

    case COLUMN_TRACEROUTECTLSOURCEADDRESS:
        *write_method = write_traceRouteCtlSourceAddress;
        *var_len = (StorageTmp->traceRouteCtlSourceAddressLen);

        return (u_char *) StorageTmp->traceRouteCtlSourceAddress;

    case COLUMN_TRACEROUTECTLIFINDEX:
        *write_method = write_traceRouteCtlIfIndex;
        *var_len = sizeof(StorageTmp->traceRouteCtlIfIndex);

        return (u_char *) & StorageTmp->traceRouteCtlIfIndex;

    case COLUMN_TRACEROUTECTLMISCOPTIONS:
        *write_method = write_traceRouteCtlMiscOptions;
        *var_len = (StorageTmp->traceRouteCtlMiscOptionsLen);

        return (u_char *) StorageTmp->traceRouteCtlMiscOptions;

    case COLUMN_TRACEROUTECTLMAXFAILURES:
        *write_method = write_traceRouteCtlMaxFailures;
        *var_len = sizeof(StorageTmp->traceRouteCtlMaxFailures);

        return (u_char *) & StorageTmp->traceRouteCtlMaxFailures;

    case COLUMN_TRACEROUTECTLDONTFRAGMENT:
        *write_method = write_traceRouteCtlDontFragment;
        *var_len = sizeof(StorageTmp->traceRouteCtlDontFragment);

        return (u_char *) & StorageTmp->traceRouteCtlDontFragment;

    case COLUMN_TRACEROUTECTLINITIALTTL:
        *write_method = write_traceRouteCtlInitialTtl;
        *var_len = sizeof(StorageTmp->traceRouteCtlInitialTtl);

        return (u_char *) & StorageTmp->traceRouteCtlInitialTtl;

    case COLUMN_TRACEROUTECTLFREQUENCY:
        *write_method = write_traceRouteCtlFrequency;
        *var_len = sizeof(StorageTmp->traceRouteCtlFrequency);

        return (u_char *) & StorageTmp->traceRouteCtlFrequency;

    case COLUMN_TRACEROUTECTLSTORAGETYPE:
        *write_method = write_traceRouteCtlStorageType;
        *var_len = sizeof(StorageTmp->traceRouteCtlStorageType);

        return (u_char *) & StorageTmp->traceRouteCtlStorageType;

    case COLUMN_TRACEROUTECTLADMINSTATUS:
        *write_method = write_traceRouteCtlAdminStatus;
        *var_len = sizeof(StorageTmp->traceRouteCtlAdminStatus);

        return (u_char *) & StorageTmp->traceRouteCtlAdminStatus;

    case COLUMN_TRACEROUTECTLDESCR:
        *write_method = write_traceRouteCtlDescr;
        *var_len = (StorageTmp->traceRouteCtlDescrLen);

        return (u_char *) StorageTmp->traceRouteCtlDescr;

    case COLUMN_TRACEROUTECTLMAXROWS:
        *write_method = write_traceRouteCtlMaxRows;
        *var_len = sizeof(StorageTmp->traceRouteCtlMaxRows);

        return (u_char *) & StorageTmp->traceRouteCtlMaxRows;

    case COLUMN_TRACEROUTECTLTRAPGENERATION:
        *write_method = write_traceRouteCtlTrapGeneration;
        *var_len = (StorageTmp->traceRouteCtlTrapGenerationLen);

        return (u_char *) StorageTmp->traceRouteCtlTrapGeneration;

    case COLUMN_TRACEROUTECTLCREATEHOPSENTRIES:
        *write_method = write_traceRouteCtlCreateHopsEntries;
        *var_len = sizeof(StorageTmp->traceRouteCtlCreateHopsEntries);

        return (u_char *) & StorageTmp->traceRouteCtlCreateHopsEntries;

    case COLUMN_TRACEROUTECTLTYPE:
        *write_method = write_traceRouteCtlType;
        *var_len = (StorageTmp->traceRouteCtlTypeLen) * sizeof(oid);

        return (u_char *) StorageTmp->traceRouteCtlType;

    case COLUMN_TRACEROUTECTLROWSTATUS:
        *write_method = write_traceRouteCtlRowStatus;
        *var_len = sizeof(StorageTmp->traceRouteCtlRowStatus);

        return (u_char *) & StorageTmp->traceRouteCtlRowStatus;

    default:
        ERROR_MSG("");
    }
    return NULL;
}



int
traceRouteResultsTable_del(struct traceRouteCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len = 0;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlTestName */

    memset(newoid, '\0', sizeof(oid) * MAX_OID_LEN);
    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);

    for (hciptr2 = traceRouteResultsTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            header_complex_extract_entry(&traceRouteResultsTableStorage,
                                         hciptr2);
            DEBUGMSGTL(("traceRouteResultsTable", "delete  success!\n"));

        }
    }
    vars = NULL;
    return SNMPERR_SUCCESS;
}




int
traceRouteProbeHistoryTable_del(struct traceRouteCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len = 0;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlOperationName */

    memset(newoid, '\0', sizeof(oid) * MAX_OID_LEN);

    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);

    for (hciptr2 = traceRouteProbeHistoryTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            header_complex_extract_entry(&traceRouteProbeHistoryTableStorage,
                                         hciptr2);
            DEBUGMSGTL(("traceRouteProbeHistoryTable",
                        "delete  success!\n"));

        }
    }
    vars = NULL;
    return SNMPERR_SUCCESS;
}


int
traceRouteHopsTable_del(struct traceRouteCtlTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len = 0;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlOwnerIndex, thedata->traceRouteCtlOwnerIndexLen);   /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) thedata->traceRouteCtlTestName, thedata->traceRouteCtlTestNameLen);       /* traceRouteCtlTestName */

    memset(newoid, '\0', sizeof(oid) * MAX_OID_LEN);

    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);

    for (hciptr2 = traceRouteHopsTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            header_complex_extract_entry(&traceRouteHopsTableStorage, hciptr2);
            DEBUGMSGTL(("traceRouteHopsTable", "delete  success!\n"));

        }
    }
    vars = NULL;
    return SNMPERR_SUCCESS;
}

/*
 * send trap 
 */

void
send_traceRoute_trap(struct traceRouteCtlTable_data *item,
                     oid * trap_oid, size_t trap_oid_len)
{
    static oid      objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };     /* snmpTrapIOD.0 */
    struct traceRouteHopsTable_data *StorageHops = NULL;
    netsnmp_variable_list *var_list = NULL;
    netsnmp_variable_list *vars = NULL;
    netsnmp_variable_list *var_hops = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len = 0;

    oid             indexoid[MAX_OID_LEN];
    size_t          indexoid_len = 0;

    struct header_complex_index *hciptr;
    oid             tempoid[MAX_OID_LEN];
    size_t          tempoid_len = 0;

    oid             traceRouteCtlTargetAddress[] =
        { 1, 3, 6, 1, 2, 1, 81, 1, 2, 1, 4 };
    oid             traceRouteHopsIpTgAddress[] =
        { 1, 3, 6, 1, 2, 1, 81, 1, 5, 1, 3 };

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen); /* traceRouteCtlOwnerIndex */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen);     /* traceRouteCtlTestName */

    /*
     * snmpTrap oid 
     */
    snmp_varlist_add_variable(&var_list, objid_snmptrap,
                              sizeof(objid_snmptrap) / sizeof(oid),
                              ASN_OBJECT_ID, (u_char *) trap_oid,
                              trap_oid_len * sizeof(oid));

    /*
     * traceRouteCtlTargetAddress 
     */
    memset(newoid, '\0', MAX_OID_LEN * sizeof(oid));
    header_complex_generate_oid(newoid, &newoid_len,
                                traceRouteCtlTargetAddress,
                                sizeof(traceRouteCtlTargetAddress) /
                                sizeof(oid), vars);

    snmp_varlist_add_variable(&var_list, newoid,
                              newoid_len,
                              ASN_OCTET_STR,
                              (u_char *) item->traceRouteCtlTargetAddress,
                              item->traceRouteCtlTargetAddressLen);

    for (hciptr = traceRouteHopsTableStorage; hciptr != NULL;
         hciptr = hciptr->next) {
	memset(indexoid, '\0', MAX_OID_LEN * sizeof(oid));
        header_complex_generate_oid(indexoid, &indexoid_len, NULL, 0,
                                    vars);
        if (snmp_oid_compare
            (indexoid, indexoid_len, hciptr->name, indexoid_len) == 0) {
            StorageHops =
                (struct traceRouteHopsTable_data *)
                header_complex_get_from_oid(traceRouteHopsTableStorage,
                                            hciptr->name, hciptr->namelen);
            memset(tempoid, '\0', MAX_OID_LEN * sizeof(oid));
            header_complex_generate_oid(tempoid, &tempoid_len,
                                        traceRouteHopsIpTgAddress,
                                        sizeof(traceRouteHopsIpTgAddress) /
                                        sizeof(oid), vars);
            snmp_varlist_add_variable(&var_hops, NULL, 0, ASN_UNSIGNED, (char *) &StorageHops->traceRouteHopsHopIndex, sizeof(StorageHops->traceRouteHopsHopIndex));    /* traceRouteCtlTestName */
            memset(newoid, '\0', MAX_OID_LEN * sizeof(oid));
            header_complex_generate_oid(newoid, &newoid_len, tempoid,
                                        tempoid_len, var_hops);
            snmp_varlist_add_variable(&var_list, newoid, newoid_len,
                                      ASN_OCTET_STR,
                                      (u_char *) StorageHops->
                                      traceRouteHopsIpTgtAddress,
                                      StorageHops->
                                      traceRouteHopsIpTgtAddressLen);

            var_hops = NULL;
        }
    }

    /*
     * XXX: stuff based on event table 
     */

    DEBUGMSG(("pingTest:send_traceRoute_trap", "success!\n"));

    send_v2trap(var_list);
    snmp_free_varbind(vars);
    vars = NULL;
    snmp_free_varbind(var_list);
    var_list = NULL;
}


int
write_traceRouteCtlTargetAddressType(int action,
                                     u_char * var_val,
                                     u_char var_val_type,
                                     size_t var_val_len,
                                     u_char * statP, oid * name,
                                     size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddressType not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlTargetAddressType;
        StorageTmp->traceRouteCtlTargetAddressType = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlTargetAddressType = tmpvar;
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
write_traceRouteCtlTargetAddress(int action,
                                 u_char * var_val,
                                 u_char var_val_type,
                                 size_t var_val_len,
                                 u_char * statP, oid * name,
                                 size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddress not ASN_OCTET_STR\n");
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
        tmpvar = StorageTmp->traceRouteCtlTargetAddress;
        tmplen = StorageTmp->traceRouteCtlTargetAddressLen;

        StorageTmp->traceRouteCtlTargetAddress =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->traceRouteCtlTargetAddress == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->traceRouteCtlTargetAddress, var_val,
               var_val_len);
        StorageTmp->traceRouteCtlTargetAddress[var_val_len] = '\0';
        StorageTmp->traceRouteCtlTargetAddressLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->traceRouteCtlTargetAddress);
        StorageTmp->traceRouteCtlTargetAddress = tmpvar;
        StorageTmp->traceRouteCtlTargetAddressLen = tmplen;
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
write_traceRouteCtlByPassRouteTable(int action,
                                    u_char * var_val,
                                    u_char var_val_type,
                                    size_t var_val_len,
                                    u_char * statP, oid * name,
                                    size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddressType not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlByPassRouteTable;
        StorageTmp->traceRouteCtlByPassRouteTable = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlByPassRouteTable = tmpvar;
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
write_traceRouteCtlDataSize(int action,
                            u_char * var_val,
                            u_char var_val_type,
                            size_t var_val_len,
                            u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDataSize not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlDataSize;
        if ((*((long *) var_val)) >= 0 && (*((long *) var_val)) <= 65507)
            StorageTmp->traceRouteCtlDataSize = *((long *) var_val);
        else
            StorageTmp->traceRouteCtlDataSize = 56;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlDataSize = tmpvar;
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
write_traceRouteCtlTimeOut(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDataSize not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlTimeOut;

        if ((*((long *) var_val)) >= 1 && (*((long *) var_val)) <= 60)
            StorageTmp->traceRouteCtlTimeOut = *((long *) var_val);
        else
            StorageTmp->traceRouteCtlTimeOut = 3;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlTimeOut = tmpvar;
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
write_traceRouteCtlProbesPerHop(int action,
                                u_char * var_val,
                                u_char var_val_type,
                                size_t var_val_len,
                                u_char * statP, oid * name,
                                size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDataSize not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlProbesPerHop;

        if ((*((long *) var_val)) >= 1 && (*((long *) var_val)) <= 10)
            StorageTmp->traceRouteCtlProbesPerHop = *((long *) var_val);
        else
            StorageTmp->traceRouteCtlProbesPerHop = 3;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlProbesPerHop = tmpvar;
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
write_traceRouteCtlPort(int action,
                        u_char * var_val,
                        u_char var_val_type,
                        size_t var_val_len,
                        u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddressType not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlPort;
        StorageTmp->traceRouteCtlPort = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlPort = tmpvar;
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
write_traceRouteCtlMaxTtl(int action,
                          u_char * var_val,
                          u_char var_val_type,
                          size_t var_val_len,
                          u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDataSize not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlMaxTtl;
        if ((*((long *) var_val)) >= 1 && (*((long *) var_val)) <= 255)
            StorageTmp->traceRouteCtlMaxTtl = *((long *) var_val);
        else
            StorageTmp->traceRouteCtlMaxTtl = 30;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlMaxTtl = tmpvar;
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
write_traceRouteCtlDSField(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDataSize not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlDSField;
        StorageTmp->traceRouteCtlDSField = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlDSField = tmpvar;
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
write_traceRouteCtlSourceAddressType(int action,
                                     u_char * var_val,
                                     u_char var_val_type,
                                     size_t var_val_len,
                                     u_char * statP, oid * name,
                                     size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlMaxRows not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlSourceAddressType;
        StorageTmp->traceRouteCtlSourceAddressType = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlSourceAddressType = tmpvar;
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
write_traceRouteCtlSourceAddress(int action,
                                 u_char * var_val,
                                 u_char var_val_type,
                                 size_t var_val_len,
                                 u_char * statP, oid * name,
                                 size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddress not ASN_OCTET_STR\n");
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
        tmpvar = StorageTmp->traceRouteCtlSourceAddress;
        tmplen = StorageTmp->traceRouteCtlSourceAddressLen;
        StorageTmp->traceRouteCtlSourceAddress =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->traceRouteCtlSourceAddress == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->traceRouteCtlSourceAddress, var_val,
               var_val_len + 1);
        StorageTmp->traceRouteCtlSourceAddress[var_val_len] = '\0';
        StorageTmp->traceRouteCtlSourceAddressLen = var_val_len;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->traceRouteCtlSourceAddress);
        StorageTmp->traceRouteCtlSourceAddress = tmpvar;
        StorageTmp->traceRouteCtlSourceAddressLen = tmplen;
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
write_traceRouteCtlIfIndex(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlMaxRows not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlIfIndex;
        StorageTmp->traceRouteCtlIfIndex = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlIfIndex = tmpvar;
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
write_traceRouteCtlMiscOptions(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddress not ASN_OCTET_STR\n");
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
        tmpvar = StorageTmp->traceRouteCtlMiscOptions;
        tmplen = StorageTmp->traceRouteCtlMiscOptionsLen;
        StorageTmp->traceRouteCtlMiscOptions =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->traceRouteCtlMiscOptions == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->traceRouteCtlMiscOptions, var_val,
               var_val_len + 1);
        StorageTmp->traceRouteCtlMiscOptions[var_val_len] = '\0';
        StorageTmp->traceRouteCtlMiscOptionsLen = var_val_len;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->traceRouteCtlMiscOptions);
        StorageTmp->traceRouteCtlMiscOptions = tmpvar;
        StorageTmp->traceRouteCtlMiscOptionsLen = tmplen;
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
write_traceRouteCtlMaxFailures(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTrapTestFailureFilter not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlMaxFailures;
        if ((*((long *) var_val)) >= 0 && (*((long *) var_val)) <= 15)
            StorageTmp->traceRouteCtlMaxFailures = *((long *) var_val);
        else
            StorageTmp->traceRouteCtlMaxFailures = 1;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlMaxFailures = tmpvar;
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
write_traceRouteCtlDontFragment(int action,
                                u_char * var_val,
                                u_char var_val_type,
                                size_t var_val_len,
                                u_char * statP, oid * name,
                                size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlMaxRows not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlDontFragment;
        StorageTmp->traceRouteCtlDontFragment = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlDontFragment = tmpvar;
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
write_traceRouteCtlInitialTtl(int action,
                              u_char * var_val,
                              u_char var_val_type,
                              size_t var_val_len,
                              u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTrapTestFailureFilter not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlInitialTtl;
        if ((*((long *) var_val)) >= 0 && (*((long *) var_val)) <= 255)
            StorageTmp->traceRouteCtlInitialTtl = *((long *) var_val);
        else
            StorageTmp->traceRouteCtlInitialTtl = 1;
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlInitialTtl = tmpvar;
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
write_traceRouteCtlFrequency(int action,
                             u_char * var_val,
                             u_char var_val_type,
                             size_t var_val_len,
                             u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlSourceAddressType not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlFrequency;
        StorageTmp->traceRouteCtlFrequency = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlFrequency = tmpvar;
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
write_traceRouteCtlStorageType(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    int             set_value;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }


    set_value = *((long *) var_val);

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlMaxRows not ASN_INTEGER\n");
            return SNMP_ERR_WRONGTYPE;
        }

        if ((StorageTmp->traceRouteCtlStorageType == 2
             || StorageTmp->traceRouteCtlStorageType == 3)
            && (set_value == 4 || set_value == 5)) {
            return SNMP_ERR_WRONGVALUE;
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
        tmpvar = StorageTmp->traceRouteCtlStorageType;
        StorageTmp->traceRouteCtlStorageType = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlStorageType = tmpvar;
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
write_traceRouteCtlAdminStatus(int action,
                               u_char * var_val,
                               u_char var_val_type,
                               size_t var_val_len,
                               u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    netsnmp_variable_list *vars = NULL;
    struct traceRouteResultsTable_data *StorageNew = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
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
                     "write to traceRouteCtlIfIndex not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlAdminStatus;
        StorageTmp->traceRouteCtlAdminStatus = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlAdminStatus = tmpvar;
        break;


    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */

        snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) StorageTmp->traceRouteCtlOwnerIndex, StorageTmp->traceRouteCtlOwnerIndexLen); /*  traceRouteCtlOwnerIndex  */
        snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) StorageTmp->traceRouteCtlTestName, StorageTmp->traceRouteCtlTestNameLen);     /*  traceRouteCtlTestName  */
        StorageNew =
            header_complex_get(traceRouteResultsTableStorage, vars);

        if (StorageTmp->traceRouteCtlAdminStatus == 1
            && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
            if (StorageNew == NULL)
                init_trResultsTable(StorageTmp);
            else {
                StorageTmp->traceRouteResults->
                    traceRouteResultsOperStatus = 1;
                modify_trResultsOper(StorageTmp, 1);
            }
            if (StorageTmp->traceRouteCtlFrequency != 0)
                StorageTmp->timer_id =
                    snmp_alarm_register(StorageTmp->traceRouteCtlFrequency,
                                        SA_REPEAT, run_traceRoute,
                                        StorageTmp);
            else
                StorageTmp->timer_id = snmp_alarm_register(1, 0, run_traceRoute,
                                                           StorageTmp);

        } else if (StorageTmp->traceRouteCtlAdminStatus == 2
                   && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
            snmp_alarm_unregister(StorageTmp->timer_id);
            if (StorageNew == NULL)
                init_trResultsTable(StorageTmp);
            else {
                StorageTmp->traceRouteResults->
                    traceRouteResultsOperStatus = 2;
                modify_trResultsOper(StorageTmp, 2);
            }
        }
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}




int
write_traceRouteCtlDescr(int action,
                         u_char * var_val,
                         u_char var_val_type,
                         size_t var_val_len,
                         u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);


    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddress not ASN_OCTET_STR\n");
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
        tmpvar = StorageTmp->traceRouteCtlDescr;
        tmplen = StorageTmp->traceRouteCtlDescrLen;

        StorageTmp->traceRouteCtlDescr = (char *) malloc(var_val_len + 1);
        if (StorageTmp->traceRouteCtlDescr == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->traceRouteCtlDescr, var_val, var_val_len + 1);
        StorageTmp->traceRouteCtlDescr[var_val_len] = '\0';
        StorageTmp->traceRouteCtlDescrLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->traceRouteCtlDescr);
        StorageTmp->traceRouteCtlDescr = tmpvar;
        StorageTmp->traceRouteCtlDescrLen = tmplen;
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
write_traceRouteCtlMaxRows(int action,
                           u_char * var_val,
                           u_char var_val_type,
                           size_t var_val_len,
                           u_char * statP, oid * name, size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_UNSIGNED) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDSField not ASN_UNSIGNED\n");
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
        tmpvar = StorageTmp->traceRouteCtlMaxRows;
        StorageTmp->traceRouteCtlMaxRows = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlMaxRows = tmpvar;
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
write_traceRouteCtlTrapGeneration(int action,
                                  u_char * var_val,
                                  u_char var_val_type,
                                  size_t var_val_len,
                                  u_char * statP, oid * name,
                                  size_t name_len)
{
    static char    *tmpvar;
    static size_t   tmplen;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);
    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlTargetAddress not ASN_OCTET_STR\n");
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
        tmpvar = StorageTmp->traceRouteCtlTrapGeneration;
        tmplen = StorageTmp->traceRouteCtlTrapGenerationLen;
        StorageTmp->traceRouteCtlTrapGeneration =
            (char *) malloc(var_val_len + 1);
        if (StorageTmp->traceRouteCtlTrapGeneration == NULL) {
            exit(1);
        }

        memcpy(StorageTmp->traceRouteCtlTrapGeneration, var_val,
               var_val_len + 1);
        StorageTmp->traceRouteCtlTrapGeneration[var_val_len] = '\0';
        StorageTmp->traceRouteCtlTrapGenerationLen = var_val_len;

        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->traceRouteCtlTrapGeneration);
        StorageTmp->traceRouteCtlTrapGeneration = tmpvar;
        StorageTmp->traceRouteCtlTrapGenerationLen = tmplen;
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
write_traceRouteCtlCreateHopsEntries(int action,
                                     u_char * var_val,
                                     u_char var_val_type,
                                     size_t var_val_len,
                                     u_char * statP, oid * name,
                                     size_t name_len)
{
    static size_t   tmpvar;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlDSField not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->traceRouteCtlCreateHopsEntries;
        StorageTmp->traceRouteCtlCreateHopsEntries = *((long *) var_val);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->traceRouteCtlCreateHopsEntries = tmpvar;
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
write_traceRouteCtlType(int action,
                        u_char * var_val,
                        u_char var_val_type,
                        size_t var_val_len,
                        u_char * statP, oid * name, size_t name_len)
{
    static oid     *tmpvar;
    static size_t   tmplen;
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);

    if ((StorageTmp =
         header_complex(traceRouteCtlTableStorage, NULL,
                        &name[sizeof(traceRouteCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storageType == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OBJECT_ID) {
            snmp_log(LOG_ERR,
                     "write to traceRouteCtlType not ASN_OBJECT_ID\n");
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
        tmpvar = StorageTmp->traceRouteCtlType;
        tmplen = StorageTmp->traceRouteCtlTypeLen;

        StorageTmp->traceRouteCtlType = (oid *) malloc(var_val_len);
        if (StorageTmp->traceRouteCtlType == NULL) {
            exit(1);
        }
        memcpy(StorageTmp->traceRouteCtlType, var_val, var_val_len);
        StorageTmp->traceRouteCtlTypeLen = var_val_len / sizeof(oid);
        break;


    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        SNMP_FREE(StorageTmp->traceRouteCtlType);
        StorageTmp->traceRouteCtlType = tmpvar;
        StorageTmp->traceRouteCtlTypeLen = tmplen;
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
write_traceRouteCtlRowStatus(int action,
                             u_char * var_val,
                             u_char var_val_type,
                             size_t var_val_len,
                             u_char * statP, oid * name, size_t name_len)
{
    struct traceRouteCtlTable_data *StorageTmp = NULL;
    static struct traceRouteCtlTable_data *StorageNew = NULL;
    static struct traceRouteCtlTable_data *StorageDel = NULL;
    size_t          newlen =
        name_len -
        (sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) + 3 - 1);
    static int      old_value;
    int             set_value;
    static netsnmp_variable_list *vars = NULL;
    struct header_complex_index *hciptr = NULL;

    StorageTmp =
        header_complex(traceRouteCtlTableStorage, NULL,
                       &name[sizeof(traceRouteCtlTable_variables_oid) /
                             sizeof(oid) + 3 - 1], &newlen, 1, NULL, NULL);

    if (var_val_type != ASN_INTEGER || var_val == NULL) {
        snmp_log(LOG_ERR,
                 "write to traceRouteCtlRowStatus not ASN_INTEGER\n");
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

            if (StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE &&
                set_value != RS_DESTROY) {
                /*
                 * "Once made active an entry may not be modified except to 
                 * delete it."  XXX: doesn't this in fact apply to ALL
                 * columns of the table and not just this one?  
                 */

                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (StorageTmp->storageType != ST_NONVOLATILE) {

                return SNMP_ERR_NOTWRITABLE;
            }
        }

        break;




    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        if (StorageTmp == NULL) {

            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
            /*
             * creation 
             */


            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);  /* traceRouteCtlOwnerIndex */
            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);  /* traceRouteCtlTestName */

            if (header_complex_parse_oid
                (&
                 (name
                  [sizeof(traceRouteCtlTable_variables_oid) / sizeof(oid) +
                   2]), newlen, vars) != SNMPERR_SUCCESS) {
                /*
                 * XXX: free, zero vars 
                 */
                return SNMP_ERR_INCONSISTENTNAME;
            }


            StorageNew = create_traceRouteCtlTable_data();
            if (vars->val_len <= 32) {
                StorageNew->traceRouteCtlOwnerIndex =
                    malloc(vars->val_len + 1);
                memcpy(StorageNew->traceRouteCtlOwnerIndex,
                       vars->val.string, vars->val_len);
                StorageNew->traceRouteCtlOwnerIndex[vars->val_len] = '\0';
                StorageNew->traceRouteCtlOwnerIndexLen = vars->val_len;
            } else {
                StorageNew->traceRouteCtlOwnerIndex = malloc(33);
                memcpy(StorageNew->traceRouteCtlOwnerIndex,
                       vars->val.string, 32);
                StorageNew->traceRouteCtlOwnerIndex[32] = '\0';
                StorageNew->traceRouteCtlOwnerIndexLen = 32;
            }

            vars = vars->next_variable;

            if (vars->val_len <= 32) {
                StorageNew->traceRouteCtlTestName =
                    malloc(vars->val_len + 1);
                memcpy(StorageNew->traceRouteCtlTestName, vars->val.string,
                       vars->val_len);
                StorageNew->traceRouteCtlTestName[vars->val_len] = '\0';
                StorageNew->traceRouteCtlTestNameLen = vars->val_len;
            } else {
                StorageNew->traceRouteCtlTestName = malloc(33);
                memcpy(StorageNew->traceRouteCtlTestName, vars->val.string,
                       32);
                StorageNew->traceRouteCtlTestName[32] = '\0';
                StorageNew->traceRouteCtlTestNameLen = 32;
            }
            vars = vars->next_variable;

            /*
             * XXX: fill in default row values here into StorageNew 
             */

            StorageNew->traceRouteCtlRowStatus = set_value;


            /*
             * XXX: free, zero vars, no longer needed? 
             */
        }
        snmp_free_varbind(vars);
        vars = NULL;
        break;

    case FREE:
        /*
         * XXX: free, zero vars 
         */
        snmp_free_varbind(vars);
        vars = NULL;
        /*
         * Release any resources that have been allocated 
         */
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
            if (StorageNew != NULL) {
#if 1
                DEBUGMSGTL(("traceRouteCtlTable",
                            "write_traceRouteCtlRowStatus entering new=%d...  \n",
                            action));
#endif
                traceRouteCtlTable_add(StorageNew);
            }

            /*
             * XXX: ack, and if it is NULL? 
             */
        } else if (set_value != RS_DESTROY) {
            /*
             * set the flag? 
             */
            old_value = StorageTmp->traceRouteCtlRowStatus;
            StorageTmp->traceRouteCtlRowStatus = *((long *) var_val);
        } else {
            /*
             * destroy...  extract it for now 
             */

            hciptr =
                header_complex_find_entry(traceRouteCtlTableStorage,
                                          StorageTmp);
            StorageDel =
                header_complex_extract_entry(&traceRouteCtlTableStorage,
                                             hciptr);
            snmp_alarm_unregister(StorageDel->timer_id);

            traceRouteResultsTable_del(StorageTmp);
            traceRouteProbeHistoryTable_del(StorageTmp);
            traceRouteHopsTable_del(StorageTmp);
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
                header_complex_find_entry(traceRouteCtlTableStorage,
                                          StorageTmp);
            StorageDel =
                header_complex_extract_entry(&traceRouteCtlTableStorage,
                                             hciptr);
            /*
             * XXX: free it 
             */
        } else if (StorageDel != NULL) {
            /*
             * row deletion, so add it again 
             */
            traceRouteCtlTable_add(StorageDel);
            traceRouteResultsTable_add(StorageDel);
            traceRouteProbeHistoryTable_addall(StorageDel);
            traceRouteHopsTable_addall(StorageDel);
        } else {
            StorageTmp->traceRouteCtlRowStatus = old_value;
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
            free(StorageDel->traceRouteCtlOwnerIndex);
            StorageDel->traceRouteCtlOwnerIndex = NULL;
            free(StorageDel->traceRouteCtlTestName);
            StorageDel->traceRouteCtlTestName = NULL;
            free(StorageDel->traceRouteCtlTargetAddress);
            StorageDel->traceRouteCtlTargetAddress = NULL;
            free(StorageDel->traceRouteCtlSourceAddress);
            StorageDel->traceRouteCtlSourceAddress = NULL;
            free(StorageDel->traceRouteCtlMiscOptions);
            StorageDel->traceRouteCtlMiscOptions = NULL;
            free(StorageDel->traceRouteCtlDescr);
            StorageDel->traceRouteCtlDescr = NULL;
            free(StorageDel->traceRouteCtlTrapGeneration);
            StorageDel->traceRouteCtlTrapGeneration = NULL;
            free(StorageDel->traceRouteCtlType);
            StorageDel->traceRouteCtlType = NULL;
            free(StorageDel);
            StorageDel = NULL;

            /*
             * XXX: free it, its dead 
             */
        } else {
            if (StorageTmp
                && StorageTmp->traceRouteCtlRowStatus == RS_CREATEANDGO) {
                StorageTmp->traceRouteCtlRowStatus = RS_ACTIVE;
            } else if (StorageTmp &&
                       StorageTmp->traceRouteCtlRowStatus ==
                       RS_CREATEANDWAIT) {

                StorageTmp->traceRouteCtlRowStatus = RS_NOTINSERVICE;
            }
        }
        if (StorageTmp && StorageTmp->traceRouteCtlRowStatus == RS_ACTIVE) {
#if 1
            DEBUGMSGTL(("traceRouteCtlTable",
                        "write_traceRouteCtlRowStatus entering runbefore=%ld...  \n",
                        StorageTmp->traceRouteCtlTargetAddressType));

#endif
            if (StorageTmp->traceRouteCtlAdminStatus == 1) {
                init_trResultsTable(StorageTmp);
                if (StorageTmp->traceRouteCtlFrequency != 0)
                    StorageTmp->timer_id =
                        snmp_alarm_register(StorageTmp->
                                            traceRouteCtlFrequency,
                                            SA_REPEAT, run_traceRoute,
                                            StorageTmp);
                else
                    StorageTmp->timer_id =
                        snmp_alarm_register(1, 0, run_traceRoute, StorageTmp);

            }

        }
        snmp_store_needed(NULL);
        break;
    }
    return SNMP_ERR_NOERROR;
}


void
run_traceRoute(unsigned int clientreg, void *clientarg)
{
    struct traceRouteCtlTable_data *item = clientarg;
    u_short         port = item->traceRouteCtlPort;     /* start udp dest port # for probe packets 相当于ctlport */
    int             waittime = item->traceRouteCtlTimeOut;      /* time to wait for response (in seconds) 相等于ctltimeout */
    int             nprobes = item->traceRouteCtlProbesPerHop;

    if (item->traceRouteCtlInitialTtl > item->traceRouteCtlMaxTtl) {
        DEBUGMSGTL(("traceRouteCtlTable",
                    "first ttl (%lu) may not be greater than max ttl (%lu)\n",
                    item->traceRouteCtlInitialTtl,
                    item->traceRouteCtlMaxTtl));
        return;
    }

    char           *old_HopsAddress[255];
    int             count = 0;
    int             flag = 0;

    if (item->traceRouteCtlTargetAddressType == 1
        || item->traceRouteCtlTargetAddressType == 16) {
        register int    code, n;
        const    char  *cp;
        register const char *err;
        register u_char *outp;
        register u_int32_t *ap;
        struct sockaddr whereto;        /* Who to try to reach */
        struct sockaddr wherefrom;      /* Who we are */

        register struct sockaddr_in *from =
            (struct sockaddr_in *) &wherefrom;
        register struct sockaddr_in *to = (struct sockaddr_in *) &whereto;
        register struct hostinfo *hi;
        int             on = 1;
        register struct protoent *pe;
        register int    ttl, probe, i;
        register int    seq = 0;
        int             tos = 0, settos = 0;
        register int    lsrr = 0;
        register u_short off = 0;
        struct ifaddrlist *al;
        char            errbuf[132];
        int             minpacket = 0;  /* min ip packet size */


        struct ip      *outip;  /* last output (udp) packet */
        struct udphdr  *outudp; /* last output (udp) packet */
        int             packlen = 0;    /* total length of packet */
        int             optlen = 0;     /* length of ip options */
        int             options = 0;    /* socket options */
        int             s;      /* receive (icmp) socket file descriptor */
        int             sndsock;        /* send (udp/icmp) socket file descriptor */

        u_short         ident;
        /*
         * loose source route gateway list (including room for final destination) 
         */
        u_int32_t       gwlist[NGATEWAYS + 1];
        static const char devnull[] = "/dev/null";
        char           *device = NULL;
        char           *source = NULL;
        char           *hostname;
        u_int           pausemsecs = 0;
        u_char          packet[512];    /* last inbound (icmp) packet */

        int             pmtu = 0;       /* Path MTU Discovery (RFC1191) */

        struct outdata *outdata;        /* last output (udp) packet */

        minpacket = sizeof(*outip) + sizeof(*outdata) + optlen;
        minpacket += sizeof(*outudp);
        packlen = minpacket;    /* minimum sized packet */

        hostname =
            (char *) malloc(item->traceRouteCtlTargetAddressLen + 1);
        if (hostname == NULL)
            return;
        memcpy(hostname, item->traceRouteCtlTargetAddress,
               item->traceRouteCtlTargetAddressLen + 1);
        hostname[item->traceRouteCtlTargetAddressLen] = '\0';

        hi = gethostinfo(hostname);
        setsin(to, hi->addrs[0]);
        if (hi->n > 1)
            DEBUGMSGTL(("traceRouteCtlTable",
                        "Warning: %s has multiple addresses; using %s\n",
                        hostname, inet_ntoa(to->sin_addr)));
        hostname = hi->name;
        hi->name = NULL;
        freehostinfo(hi);


        netsnmp_set_line_buffering(stdout);

        outip = (struct ip *) malloc(packlen);
        if (outip == NULL) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "malloc: %s\n", strerror(errno)));
            exit(1);
        }
        memset((char *) outip, 0, packlen);

        outip->ip_v = IPVERSION;
        if (settos)
            outip->ip_tos = tos;
#ifdef BYTESWAP_IP_HDR
        outip->ip_len = htons(packlen);
        outip->ip_off = htons(off);
#else
        outip->ip_len = packlen;
        outip->ip_off = off;
#endif
        outp = (u_char *) (outip + 1);
#ifdef HAVE_RAW_OPTIONS
        if (lsrr > 0) {
            register u_char *optlist;

            optlist = outp;
            outp += optlen;

            /*
             * final hop 
             */
            gwlist[lsrr] = to->sin_addr.s_addr;

            outip->ip_dst.s_addr = gwlist[0];

            /*
             * force 4 byte alignment 
             */
            optlist[0] = IPOPT_NOP;
            /*
             * loose source route option 
             */
            optlist[1] = IPOPT_LSRR;
            i = lsrr * sizeof(gwlist[0]);
            optlist[2] = i + 3;
            /*
             * Pointer to LSRR addresses 
             */
            optlist[3] = IPOPT_MINOFF;
            memcpy(optlist + 4, gwlist + 1, i);
        } else
#endif
            outip->ip_dst = to->sin_addr;
        outip->ip_hl = (outp - (u_char *) outip) >> 2;
        ident = (getpid() & 0xffff) | 0x8000;

        outip->ip_p = IPPROTO_UDP;

        outudp = (struct udphdr *) outp;
        outudp->source = htons(ident);
        outudp->len =
            htons((u_short) (packlen - (sizeof(*outip) + optlen)));
        outdata = (struct outdata *) (outudp + 1);

        cp = "icmp";
        if ((pe = getprotobyname(cp)) == NULL) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "unknown protocol %s\n", cp));
            exit(1);
        }

        /*
         * Insure the socket fds won't be 0, 1 or 2 
         */
        if (open(devnull, O_RDONLY) < 0 ||
            open(devnull, O_RDONLY) < 0 || open(devnull, O_RDONLY) < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "open \"%s\": %s\n", devnull, strerror(errno)));
            exit(1);
        }
        if ((s = socket(AF_INET, SOCK_RAW, pe->p_proto)) < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "icmp socket: %s\n", strerror(errno)));
            exit(1);
        }
        if (options & SO_DEBUG)
            (void) setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *) &on,
                              sizeof(on));
        if (options & SO_DONTROUTE)
            (void) setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *) &on,
                              sizeof(on));
#ifndef __hpux
        printf("raw\n");
        sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
#else
        printf("udp\n");
        sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
#endif
        if (sndsock < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "raw socket: %s\n", strerror(errno)));
            exit(1);
        }
#if defined(IP_OPTIONS) && !defined(HAVE_RAW_OPTIONS)
        if (lsrr > 0) {
            u_char          optlist[MAX_IPOPTLEN];

            cp = "ip";
            if ((pe = getprotobyname(cp)) == NULL) {
                DEBUGMSGTL(("traceRouteCtlTable",
                            "unknown protocol %s\n", cp));
                exit(1);
            }
            /*
             * final hop 
             */
            gwlist[lsrr] = to->sin_addr.s_addr;
            ++lsrr;

            /*
             * force 4 byte alignment 
             */
            optlist[0] = IPOPT_NOP;
            /*
             * loose source route option 
             */
            optlist[1] = IPOPT_LSRR;
            i = lsrr * sizeof(gwlist[0]);
            optlist[2] = i + 3;
            /*
             * Pointer to LSRR addresses 
             */
            optlist[3] = IPOPT_MINOFF;
            memcpy(optlist + 4, gwlist, i);

            if ((setsockopt(sndsock, pe->p_proto, IP_OPTIONS,
                            (char *) optlist,
                            i + sizeof(gwlist[0]))) < 0) {
                DEBUGMSGTL(("traceRouteCtlTable", "IP_OPTIONS: %s\n",
                            strerror(errno)));
                exit(1);
            }
        }
#endif
#ifdef SO_SNDBUF
        if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *) &packlen,
                       sizeof(packlen)) < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "SO_SNDBUF: %s\n", strerror(errno)));
            exit(1);
        }
#endif
#ifdef IP_HDRINCL
        if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *) &on,
                       sizeof(on)) < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "IP_HDRINCL: %s\n", strerror(errno)));
            exit(1);
        }
#else
#ifdef IP_TOS
        if (settos && setsockopt(sndsock, IPPROTO_IP, IP_TOS,
                                 (char *) &tos, sizeof(tos)) < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        "setsockopt tos %d: %s\n", strerror(errno)));
            exit(1);
        }
#endif
#endif
        if (options & SO_DEBUG)
            (void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG, (char *) &on,
                              sizeof(on));
        if (options & SO_DONTROUTE)
            (void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
                              (char *) &on, sizeof(on));
        /*
         * Get the interface address list 
         */
        n = ifaddrlist(&al, errbuf);
        if (n < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        " ifaddrlist: %s\n", errbuf));
            exit(1);
        }
        if (n == 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        " Can't find any network interfaces\n"));

            exit(1);
        }

        /*
         * Look for a specific device 
         */
        if (device != NULL) {
            for (i = n; i > 0; --i, ++al)
                if (strcmp(device, al->device) == 0)
                    break;
            if (i <= 0) {
                DEBUGMSGTL(("traceRouteCtlTable",
                            " Can't find interface %.32s\n", device));

                exit(1);
            }
        }
        /*
         * Determine our source address 
         */
        if (source == NULL) {
            /*
             * If a device was specified, use the interface address.
             * Otherwise, try to determine our source address.
             */
            if (device != NULL)
                setsin(from, al->addr);
            else if ((err = findsaddr(to, from)) != NULL) {
                DEBUGMSGTL(("traceRouteCtlTable",
                            " findsaddr: %s\n", err));
                exit(1);
            }

        } else {
            hi = gethostinfo(source);
            source = hi->name;
            hi->name = NULL;
            /*
             * If the device was specified make sure it
             * corresponds to the source address specified.
             * Otherwise, use the first address (and warn if
             * there are more than one).
             */
            if (device != NULL) {
                for (i = hi->n, ap = hi->addrs; i > 0; --i, ++ap)
                    if (*ap == al->addr)
                        break;
                if (i <= 0) {
                    DEBUGMSGTL(("traceRouteCtlTable",
                                " %s is not on interface %.32s\n",
                                source, device));

                    exit(1);
                }
                setsin(from, *ap);
            } else {
                setsin(from, hi->addrs[0]);
                if (hi->n > 1)
                    DEBUGMSGTL(("traceRouteCtlTable",
                                " Warning: %s has multiple addresses; using %s\n",
                                source, inet_ntoa(from->sin_addr)));

            }
            freehostinfo(hi);
        }
        /*
         * Revert to non-privileged user after opening sockets 
         */
        setgid(getgid());
        setuid(getuid());

        outip->ip_src = from->sin_addr;
#ifndef IP_HDRINCL
        if (bind(sndsock, (struct sockaddr *) from, sizeof(*from)) < 0) {
            DEBUGMSGTL(("traceRouteCtlTable",
                        " bind: %s\n", strerror(errno)));
            exit(1);
        }
#endif
        DEBUGMSGTL(("traceRouteCtlTable",
                    " to %s (%s)", hostname, inet_ntoa(to->sin_addr)));

        if (source)
            DEBUGMSGTL(("traceRouteCtlTable", " from %s", source));

        DEBUGMSGTL(("traceRouteCtlTable",
                    ", %lu hops max, %d byte packets\n",
                    item->traceRouteCtlMaxTtl, packlen));
        (void) fflush(stderr);

        struct traceRouteResultsTable_data *StorageResults = NULL;
        netsnmp_variable_list *vars_results = NULL;

        struct traceRouteHopsTable_data *temp = NULL;
        struct traceRouteHopsTable_data *current_temp = NULL;
        struct traceRouteHopsTable_data *current = NULL;

        unsigned long   index = 0;

        struct traceRouteProbeHistoryTable_data *temp_his = NULL;
        struct traceRouteProbeHistoryTable_data *current_temp_his = NULL;

        snmp_varlist_add_variable(&vars_results, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen);     /*  traceRouteCtlOwnerIndex  */
        snmp_varlist_add_variable(&vars_results, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen); /*  traceRouteCtlTestName  */
        if ((StorageResults =
             header_complex_get(traceRouteResultsTableStorage,
                                vars_results)) == NULL)
            return;
        snmp_free_varbind(vars_results);
        vars_results = NULL;


        for (ttl = item->traceRouteCtlInitialTtl;
             ttl <= item->traceRouteCtlMaxTtl; ++ttl) {

            u_int32_t       lastaddr = 0;
            int             gotlastaddr = 0;
            int             got_there = 0;
            int             unreachable = 0;
            int             sentfirst = 0;
            time_t          timep = 0;

            StorageResults->traceRouteResultsCurHopCount = ttl;
            if (item->traceRouteCtlCreateHopsEntries == 1) {
                if (ttl == item->traceRouteCtlInitialTtl) {
                    int             k = 0;
                    count = traceRouteHopsTable_count(item);


                    struct traceRouteHopsTable_data *StorageTmp = NULL;
                    struct header_complex_index *hciptr2 = NULL;
                    netsnmp_variable_list *vars = NULL;
                    oid             newoid[MAX_OID_LEN];
                    size_t          newoid_len;

                    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen); /* traceRouteCtlOwnerIndex */
                    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen);     /* traceRouteCtlTestName */

                    header_complex_generate_oid(newoid, &newoid_len, NULL,
                                                0, vars);

                    for (hciptr2 = traceRouteHopsTableStorage;
                         hciptr2 != NULL; hciptr2 = hciptr2->next) {
                        if (snmp_oid_compare
                            (newoid, newoid_len, hciptr2->name,
                             newoid_len) == 0) {
                            StorageTmp =
                                header_complex_extract_entry
                                (&traceRouteHopsTableStorage, hciptr2);

                            old_HopsAddress[k] =
                                (char *) malloc(StorageTmp->
                                                traceRouteHopsIpTgtAddressLen
                                                + 1);
                            if (old_HopsAddress[k] == NULL) {
                                exit(1);
                            }
                            memdup((u_char **) & old_HopsAddress[k],
                                   StorageTmp->traceRouteHopsIpTgtAddress,
                                   StorageTmp->
                                   traceRouteHopsIpTgtAddressLen + 1);
                            old_HopsAddress[k][StorageTmp->
                                               traceRouteHopsIpTgtAddressLen]
                                = '\0';

                            k++;
                            StorageTmp = NULL;
                        }
                    }
                    traceRouteHopsTable_del(item);
                    index = 0;
                }

                temp = SNMP_MALLOC_STRUCT(traceRouteHopsTable_data);
                temp->traceRouteCtlOwnerIndex =
                    (char *) malloc(item->traceRouteCtlOwnerIndexLen + 1);
                memcpy(temp->traceRouteCtlOwnerIndex,
                       item->traceRouteCtlOwnerIndex,
                       item->traceRouteCtlOwnerIndexLen + 1);
                temp->traceRouteCtlOwnerIndex[item->
                                              traceRouteCtlOwnerIndexLen] =
                    '\0';
                temp->traceRouteCtlOwnerIndexLen =
                    item->traceRouteCtlOwnerIndexLen;

                temp->traceRouteCtlTestName =
                    (char *) malloc(item->traceRouteCtlTestNameLen + 1);
                memcpy(temp->traceRouteCtlTestName,
                       item->traceRouteCtlTestName,
                       item->traceRouteCtlTestNameLen + 1);
                temp->traceRouteCtlTestName[item->
                                            traceRouteCtlTestNameLen] =
                    '\0';
                temp->traceRouteCtlTestNameLen =
                    item->traceRouteCtlTestNameLen;

                /* add lock to protect */
                pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
                pthread_mutex_lock(&counter_mutex);
                temp->traceRouteHopsHopIndex = ++index;
                pthread_mutex_unlock(&counter_mutex);
                /* endsadsadsad */


                temp->traceRouteHopsIpTgtAddressType = 0;
                temp->traceRouteHopsIpTgtAddress = strdup("");
                temp->traceRouteHopsIpTgtAddressLen = 0;
                temp->traceRouteHopsMinRtt = 0;
                temp->traceRouteHopsMaxRtt = 0;
                temp->traceRouteHopsAverageRtt = 0;
                temp->traceRouteHopsRttSumOfSquares = 0;
                temp->traceRouteHopsSentProbes = 0;
                temp->traceRouteHopsProbeResponses = 0;

                temp->traceRouteHopsLastGoodProbeLen = 0;
                if (index == 1)
                    item->traceRouteHops = temp;
                else {
                    (current_temp)->next = temp;
                }

                current_temp = temp;

                if (index + 1 >= item->traceRouteCtlMaxTtl) {
                    current_temp->next = NULL;
                }

                if (item->traceRouteHops != NULL)

                    if (traceRouteHopsTable_add(current_temp) !=
                        SNMPERR_SUCCESS)
                        DEBUGMSGTL(("traceRouteHopsTable",
                                    "registered an entry error\n"));

            }
            register unsigned long maxRtt = 0;
            register unsigned long minRtt = 0;
            register unsigned long averageRtt = 0;
            register unsigned long sumRtt = 0;
            register unsigned long responseProbe = 0;
            register unsigned long sumOfSquare = 0;
            for (probe = 0; probe < nprobes; ++probe) {
                register int    cc;
                struct timeval  t1, t2;
                struct timezone tz;
                register struct ip *ip = NULL;
                register unsigned long Rtt = 0;

                if (sentfirst && pausemsecs > 0)
                    usleep(pausemsecs * 1000);
                (void) gettimeofday(&t1, &tz);
                send_probe(to, ++seq, ttl, &t1, outip, outudp, packlen,
                           optlen, hostname, ident, sndsock, port,
                           outdata);
                ++sentfirst;
                while ((cc =
                        wait_for_reply(s, from, &t1, packet,
                                       waittime)) != 0) {
                    (void) gettimeofday(&t2, &tz);
                    timep = 0;
                    time(&timep);
                    i = packet_ok(packet, cc, from, seq, ident, pmtu,
                                  port);
                    /*
                     * Skip short packet 
                     */
                    if (i == 0)
                        continue;
                    if (!gotlastaddr || from->sin_addr.s_addr != lastaddr) {
                        register struct ip *ip;
                        register int    hlen;
                        ip = (struct ip *) packet;
                        hlen = ip->ip_hl << 2;
                        cc -= hlen;
                        DEBUGMSGTL(("traceRouteCtlTable",
                                    " %s", inet_ntoa(from->sin_addr)));


                        lastaddr = from->sin_addr.s_addr;
                        ++gotlastaddr;
                    }
                    Rtt = deltaT(&t1, &t2);
                    responseProbe = responseProbe + 1;
                    if (probe == 0) {
                        minRtt = Rtt;
                        maxRtt = Rtt;
                        averageRtt = Rtt;
                        sumRtt = Rtt;
                        sumOfSquare = Rtt * Rtt;
                    } else {
                        if (Rtt < minRtt)
                            minRtt = Rtt;
                        if (Rtt > maxRtt)
                            maxRtt = Rtt;
                        sumRtt = (sumRtt) + Rtt;
                        averageRtt =
                            round((double) (sumRtt) /
                                  (double) responseProbe);
                        sumOfSquare = sumOfSquare + Rtt * Rtt;
                    }

                    StorageResults->traceRouteResultsCurProbeCount =
                        probe + 1;
                    if (i == -2) {
#ifndef ARCHAIC
                        ip = (struct ip *) packet;
                        if (ip->ip_ttl <= 1)
                            Printf(" !");
#endif
                        ++got_there;
                        break;
                    }
                    /*
                     * time exceeded in transit 
                     */
                    if (i == -1)
                        break;
                    code = i - 1;
                    switch (code) {

                    case ICMP_UNREACH_PORT:
#ifndef ARCHAIC
                        ip = (struct ip *) packet;
                        if (ip->ip_ttl <= 1)
                            Printf(" !");
#endif
                        ++got_there;
                        break;

                    case ICMP_UNREACH_NET:
                        ++unreachable;
                        Printf(" !N");
                        break;

                    case ICMP_UNREACH_HOST:
                        ++unreachable;
                        Printf(" !H");
                        break;

                    case ICMP_UNREACH_PROTOCOL:
                        ++got_there;
                        Printf(" !P");
                        break;

                    case ICMP_UNREACH_NEEDFRAG:
                        ++unreachable;
                        Printf(" !F-%d", pmtu);
                        break;

                    case ICMP_UNREACH_SRCFAIL:
                        ++unreachable;
                        Printf(" !S");
                        break;

                    case ICMP_UNREACH_FILTER_PROHIB:
                        ++unreachable;
                        Printf(" !X");
                        break;

                    case ICMP_UNREACH_HOST_PRECEDENCE:
                        ++unreachable;
                        Printf(" !V");
                        break;

                    case ICMP_UNREACH_PRECEDENCE_CUTOFF:
                        ++unreachable;
                        Printf(" !C");
                        break;

                    default:
                        ++unreachable;
                        Printf(" !<%d>", code);
                        break;
                    }
                    break;
                }
                if (cc == 0) {
                    timep = 0;
                    time(&timep);
                    Printf(" *");
                    Rtt = (item->traceRouteCtlTimeOut) * 1000;
                }
                if (item->traceRouteCtlMaxRows != 0) {

                    temp_his =
                        SNMP_MALLOC_STRUCT
                        (traceRouteProbeHistoryTable_data);
                    temp_his->traceRouteCtlOwnerIndex =
                        (char *) malloc(item->traceRouteCtlOwnerIndexLen +
                                        1);
                    memcpy(temp_his->traceRouteCtlOwnerIndex,
                           item->traceRouteCtlOwnerIndex,
                           item->traceRouteCtlOwnerIndexLen + 1);
                    temp_his->traceRouteCtlOwnerIndex[item->
                                                      traceRouteCtlOwnerIndexLen]
                        = '\0';
                    temp_his->traceRouteCtlOwnerIndexLen =
                        item->traceRouteCtlOwnerIndexLen;

                    temp_his->traceRouteCtlTestName =
                        (char *) malloc(item->traceRouteCtlTestNameLen +
                                        1);
                    memcpy(temp_his->traceRouteCtlTestName,
                           item->traceRouteCtlTestName,
                           item->traceRouteCtlTestNameLen + 1);
                    temp_his->traceRouteCtlTestName[item->
                                                    traceRouteCtlTestNameLen]
                        = '\0';
                    temp_his->traceRouteCtlTestNameLen =
                        item->traceRouteCtlTestNameLen;

                    /* add lock to protect */
                    pthread_mutex_t counter_mutex =
                        PTHREAD_MUTEX_INITIALIZER;
                    pthread_mutex_lock(&counter_mutex);
                    if (item->traceRouteProbeHistoryMaxIndex >=
                        (unsigned long) (2147483647))
                        item->traceRouteProbeHistoryMaxIndex = 0;
                    temp_his->traceRouteProbeHistoryIndex =
                        ++(item->traceRouteProbeHistoryMaxIndex);
                    pthread_mutex_unlock(&counter_mutex);
                    /* endsadsadsad */
                    temp_his->traceRouteProbeHistoryHopIndex = ttl;
                    temp_his->traceRouteProbeHistoryProbeIndex = probe + 1;

                    temp_his->traceRouteProbeHistoryHAddrType = 1;
                    temp_his->traceRouteProbeHistoryHAddr =
                        (char *) malloc(strlen(inet_ntoa(from->sin_addr)) +
                                        1);
                    strcpy(temp_his->traceRouteProbeHistoryHAddr,
                           (inet_ntoa(from->sin_addr)));
                    temp_his->
                        traceRouteProbeHistoryHAddr[strlen
                                                    (inet_ntoa
                                                     (from->sin_addr))] =
                        '\0';
                    temp_his->traceRouteProbeHistoryHAddrLen =
                        strlen(inet_ntoa(from->sin_addr));

                    temp_his->traceRouteProbeHistoryResponse = Rtt;
                    temp_his->traceRouteProbeHistoryStatus = 1;
                    temp_his->traceRouteProbeHistoryLastRC = 0;

		    temp_his->traceRouteProbeHistoryTime_time = timep;
                    memdup(&temp_his->traceRouteProbeHistoryTime,
                        date_n_time(&timep,
			    &temp_his->traceRouteProbeHistoryTimeLen), 11);
                    if (probe == 0)
                        item->traceRouteProbeHis = temp_his;
                    else {
                        (current_temp_his)->next = temp_his;
                    }

                    current_temp_his = temp_his;

                    if (probe + 1 >= nprobes) {
                        current_temp_his->next = NULL;

                    }

                    if (item->traceRouteProbeHis != NULL) {
                        if (traceRouteProbeHistoryTable_count(item) <
                            item->traceRouteCtlMaxRows) {
                            if (traceRouteProbeHistoryTable_add
                                (current_temp_his) != SNMPERR_SUCCESS)
                                DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                            "registered an entry error\n"));
                        } else {
                            traceRouteProbeHistoryTable_delLast(item);
                            if (traceRouteProbeHistoryTable_add
                                (current_temp_his) != SNMPERR_SUCCESS)
                                DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                            "registered an entry error\n"));

                        }
		    }

                }

                if (item->traceRouteCtlCreateHopsEntries == 1) {
                    netsnmp_variable_list *vars_hops = NULL;
                    snmp_varlist_add_variable(&vars_hops, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen);    /*  traceRouteCtlOwnerIndex  */
                    snmp_varlist_add_variable(&vars_hops, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen);        /*  traceRouteCtlTestName  */
                    snmp_varlist_add_variable(&vars_hops, NULL, 0, ASN_UNSIGNED, (char *) &index, sizeof(index));       /*  traceRouteHopsIndex  */
                    if ((current =
                         header_complex_get(traceRouteHopsTableStorage,
                                            vars_hops)) == NULL)
                        return;
                    snmp_free_varbind(vars_hops);
                    vars_hops = NULL;

                    current->traceRouteHopsIpTgtAddressType = 1;
                    current->traceRouteHopsIpTgtAddress =
                        (char *) malloc(strlen(inet_ntoa(from->sin_addr)) +
                                        1);
                    current->traceRouteHopsIpTgtAddress =
                        strdup(inet_ntoa(from->sin_addr));
                    current->
                        traceRouteHopsIpTgtAddress[strlen
                                                   (inet_ntoa
                                                    (from->sin_addr))] =
                        '\0';
                    current->traceRouteHopsIpTgtAddressLen =
                        strlen(inet_ntoa(from->sin_addr));
                    if (count != 0) {
                        if (strcmp
                            (old_HopsAddress[index - 1],
                             current->traceRouteHopsIpTgtAddress) != 0)
                            flag = 1;
                    }

                    current->traceRouteHopsIpTgtAddressLen =
                        strlen(inet_ntoa(from->sin_addr));
                    current->traceRouteHopsMinRtt = minRtt;
                    current->traceRouteHopsMaxRtt = maxRtt;
                    current->traceRouteHopsAverageRtt = averageRtt;
                    current->traceRouteHopsRttSumOfSquares = sumOfSquare;
                    current->traceRouteHopsSentProbes = probe + 1;
                    current->traceRouteHopsProbeResponses = responseProbe;
		    current->traceRouteHopsLastGoodProbe_time = timep;
                    memdup(&current->traceRouteHopsLastGoodProbe,
                        date_n_time(&timep,
			    &current->traceRouteHopsLastGoodProbeLen), 11);
                }

                (void) fflush(stdout);
            }
            putchar('\n');


            if (got_there
                || (unreachable > 0 && unreachable >= nprobes - 1)) {

                if (got_there != 0) {
                    StorageResults->traceRouteResultsTestAttempts =
                        StorageResults->traceRouteResultsTestAttempts + 1;

                    StorageResults->traceRouteResultsTestSuccesses =
                        StorageResults->traceRouteResultsTestSuccesses + 1;

		    StorageResults->traceRouteResultsLastGoodPath_time = timep;
                    memdup(&StorageResults->traceRouteResultsLastGoodPath,
                        date_n_time(&timep,
			    &StorageResults->traceRouteResultsLastGoodPathLen), 11);
                    if ((item->
                         traceRouteCtlTrapGeneration[0] &
                         TRACEROUTETRAPGENERATION_TESTCOMPLETED) != 0) {
                        DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                    "TEST completed!\n"));
                        send_traceRoute_trap(item, traceRouteTestCompleted,
                                             sizeof
                                             (traceRouteTestCompleted) /
                                             sizeof(oid));
                    }
                }

                else {
                    StorageResults->traceRouteResultsTestAttempts =
                        StorageResults->traceRouteResultsTestAttempts + 1;
                    if ((item->
                         traceRouteCtlTrapGeneration[0] &
                         TRACEROUTETRAPGENERATION_TESTFAILED) != 0) {
                        DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                    "test Failed!\n"));
                        send_traceRoute_trap(item, traceRouteTestFailed,
                                       sizeof(traceRouteTestFailed) /
                                       sizeof(oid));
                    }
                }
                break;

            } else if (ttl == item->traceRouteCtlMaxTtl
                       && (probe + 1) == nprobes) {
                StorageResults->traceRouteResultsTestAttempts =
                    StorageResults->traceRouteResultsTestAttempts + 1;

                if ((item->
                     traceRouteCtlTrapGeneration[0] &
                     TRACEROUTETRAPGENERATION_TESTFAILED) != 0) {
                    DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                "test Failed!\n"));
                    send_traceRoute_trap(item, traceRouteTestFailed,
                                   sizeof(traceRouteTestFailed) /
                                   sizeof(oid));
                }
            }

        }

        close(sndsock);

        if (flag == 1) {
            DEBUGMSGTL(("traceRouteProbeHistoryTable", "path changed!\n"));
            send_traceRoute_trap(item, traceRoutePathChange,
                                 sizeof(traceRoutePathChange) /
                                 sizeof(oid));
        }

        int             k = 0;
        for (k = 0; k < count; k++) {
            free(old_HopsAddress[k]);
            old_HopsAddress[k] = NULL;
        }
    }
    if (item->traceRouteCtlTargetAddressType == 2) {
        int             icmp_sock = 0;  /* receive (icmp) socket file descriptor */
        int             sndsock = 0;    /* send (udp) socket file descriptor */

        struct sockaddr_in6 whereto;    /* Who to try to reach */

        struct sockaddr_in6 saddr;
        struct sockaddr_in6 firsthop;
        char           *source = NULL;
        char           *device = NULL;
        char           *hostname = NULL;

        pid_t           ident = 0;
        u_short         port = 32768 + 666;     /* start udp dest port # for probe packets */
        int             options = 0;    /* socket options */
        int             waittime = 5;   /* time to wait for response (in seconds) */

        char           *sendbuff = NULL;
        int             datalen = sizeof(struct pkt_format);

        u_char          packet[512];    /* last inbound (icmp) packet */

        char            pa[64];
        struct hostent *hp = NULL;
        struct sockaddr_in6 from, *to = NULL;
        int             i = 0, on = 0, probe = 0, seq = 0, tos =
            0, ttl = 0;
        int             socket_errno = 0;

        icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        socket_errno = errno;

        setuid(getuid());

        on = 1;
        seq = tos = 0;
        to = (struct sockaddr_in6 *) &whereto;

        hostname =
            (char *) malloc(item->traceRouteCtlTargetAddressLen + 1);
        memcpy(hostname, item->traceRouteCtlTargetAddress,
               item->traceRouteCtlTargetAddressLen + 1);
        hostname[item->traceRouteCtlTargetAddressLen] = '\0';

        setlinebuf(stdout);

        memset(&whereto, '\0', sizeof(struct sockaddr_in6));

        to->sin6_family = AF_INET6;
        to->sin6_port = htons(port);

        if (inet_pton(AF_INET6, hostname, &to->sin6_addr) <= 0) {
            hp = gethostbyname2(hostname, AF_INET6);
            if (hp != NULL) {
                memmove((caddr_t) & to->sin6_addr, hp->h_addr, 16);
                hostname = (char *) hp->h_name;
            } else {
                (void) fprintf(stderr,
                               "traceroute: unknown host %s\n", hostname);
                return;
            }
        }
        firsthop = *to;

        datalen = item->traceRouteCtlDataSize;
        if (datalen < (int) sizeof(struct pkt_format)
            || datalen >= MAXPACKET) {
            Fprintf(stderr,
                    "traceroute: packet size must be %d <= s < %d.\n",
                    (int) sizeof(struct pkt_format), MAXPACKET);
            datalen = 16;
        }

        ident = getpid();

        sendbuff = malloc(datalen);
        if (sendbuff == NULL) {
            fprintf(stderr, "malloc failed\n");
            return;
        }

        if (icmp_sock < 0) {
            errno = socket_errno;
            perror("traceroute6: icmp socket");
            return;
        }

        if (options & SO_DEBUG)
            setsockopt(icmp_sock, SOL_SOCKET, SO_DEBUG,
                       (char *) &on, sizeof(on));
        if (options & SO_DONTROUTE)
            setsockopt(icmp_sock, SOL_SOCKET, SO_DONTROUTE,
                       (char *) &on, sizeof(on));

        if ((sndsock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
            perror("traceroute: UDP socket");
            return;
        }
#ifdef SO_SNDBUF
        if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *) &datalen,
                       sizeof(datalen)) < 0) {
            perror("traceroute: SO_SNDBUF");
            return;
        }
#endif                          /* SO_SNDBUF */

        if (options & SO_DEBUG)
            (void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
                              (char *) &on, sizeof(on));
        if (options & SO_DONTROUTE)
            (void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
                              (char *) &on, sizeof(on));

        if (source == NULL) {
            socklen_t       alen;
            int             probe_fd = socket(AF_INET6, SOCK_DGRAM, 0);

            if (probe_fd < 0) {
                perror("socket");
                return;
            }
            if (device) {
                if (setsockopt
                    (probe_fd, SOL_SOCKET, SO_BINDTODEVICE, device,
                     strlen(device) + 1) == -1)
                    perror("WARNING: interface is ignored");
            }
            firsthop.sin6_port = htons(1025);
            if (connect
                (probe_fd, (struct sockaddr *) &firsthop,
                 sizeof(firsthop)) == -1) {
                perror("connect");
                return;
            }
            alen = sizeof(saddr);
            if (getsockname(probe_fd, (struct sockaddr *) &saddr, &alen) ==
                -1) {
                perror("getsockname");
                return;
            }
            saddr.sin6_port = 0;
            close(probe_fd);
        } else {
            memset(&saddr, '\0', sizeof(struct sockaddr_in6));
            saddr.sin6_family = AF_INET6;
            if (inet_pton(AF_INET6, source, &saddr.sin6_addr) < 0) {
                Printf("traceroute: unknown addr %s\n", source);
                return;
            }
        }

        if (bind(sndsock, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
            perror("traceroute: bind sending socket");
            return;
        }
        if (bind(icmp_sock, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
            perror("traceroute: bind icmp6 socket");
            return;
        }

        Fprintf(stderr, "traceroute to %s (%s)", hostname,
                inet_ntop(AF_INET6, &to->sin6_addr, pa, 64));

        Fprintf(stderr, " from %s",
                inet_ntop(AF_INET6, &saddr.sin6_addr, pa, 64));
        Fprintf(stderr, ", %lu hops max, %d byte packets\n",
                item->traceRouteCtlMaxTtl, datalen);
        (void) fflush(stderr);


        struct traceRouteResultsTable_data *StorageResults = NULL;
        netsnmp_variable_list *vars_results = NULL;

        struct traceRouteHopsTable_data *temp = NULL;
        struct traceRouteHopsTable_data *current_temp = NULL;
        struct traceRouteHopsTable_data *current = NULL;

        unsigned long   index = 0;

        struct traceRouteProbeHistoryTable_data *temp_his = NULL;
        struct traceRouteProbeHistoryTable_data *current_temp_his = NULL;

        snmp_varlist_add_variable(&vars_results, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen);     /*  traceRouteCtlOwnerIndex  */
        snmp_varlist_add_variable(&vars_results, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen); /*  traceRouteCtlTestName  */
        if ((StorageResults =
             header_complex_get(traceRouteResultsTableStorage,
                                vars_results)) == NULL)
            return;
        snmp_free_varbind(vars_results);
        vars_results = NULL;

        for (ttl = item->traceRouteCtlInitialTtl;
             ttl <= item->traceRouteCtlMaxTtl; ++ttl) {
            struct in6_addr lastaddr = { {{0,}} };
            int             got_there = 0;
            int             unreachable = 0;
            time_t          timep = 0;
            Printf("%2d ", ttl);


            StorageResults->traceRouteResultsCurHopCount = ttl;
            if (item->traceRouteCtlCreateHopsEntries == 1) {
                if (ttl == item->traceRouteCtlInitialTtl) {

                    int             k = 0;
                    count = traceRouteHopsTable_count(item);
                    struct traceRouteHopsTable_data *StorageTmp = NULL;
                    struct header_complex_index *hciptr2 = NULL;
                    netsnmp_variable_list *vars = NULL;
                    oid             newoid[MAX_OID_LEN];
                    size_t          newoid_len;

                    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen); /* traceRouteCtlOwnerIndex */
                    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen);     /* traceRouteCtlTestName */

                    header_complex_generate_oid(newoid, &newoid_len, NULL,
                                                0, vars);

                    snmp_free_varbind(vars);
                    vars = NULL;

                    for (hciptr2 = traceRouteHopsTableStorage;
                         hciptr2 != NULL; hciptr2 = hciptr2->next) {
                        if (snmp_oid_compare
                            (newoid, newoid_len, hciptr2->name,
                             newoid_len) == 0) {
                            StorageTmp =
                                header_complex_extract_entry
                                (&traceRouteHopsTableStorage, hciptr2);

                            old_HopsAddress[k] =
                                (char *) malloc(StorageTmp->
                                                traceRouteHopsIpTgtAddressLen
                                                + 1);
                            if (old_HopsAddress[k] == NULL) {
                                exit(1);
                            }
                            memdup((u_char **) & old_HopsAddress[k],
                                   StorageTmp->traceRouteHopsIpTgtAddress,
                                   StorageTmp->
                                   traceRouteHopsIpTgtAddressLen + 1);
                            old_HopsAddress[k][StorageTmp->
                                               traceRouteHopsIpTgtAddressLen]
                                = '\0';

                            k++;
                        }
                    }
                    traceRouteHopsTable_del(item);
                    index = 0;
                }

                temp = SNMP_MALLOC_STRUCT(traceRouteHopsTable_data);
                temp->traceRouteCtlOwnerIndex =
                    (char *) malloc(item->traceRouteCtlOwnerIndexLen + 1);
                memcpy(temp->traceRouteCtlOwnerIndex,
                       item->traceRouteCtlOwnerIndex,
                       item->traceRouteCtlOwnerIndexLen + 1);
                temp->traceRouteCtlOwnerIndex[item->
                                              traceRouteCtlOwnerIndexLen] =
                    '\0';
                temp->traceRouteCtlOwnerIndexLen =
                    item->traceRouteCtlOwnerIndexLen;

                temp->traceRouteCtlTestName =
                    (char *) malloc(item->traceRouteCtlTestNameLen + 1);
                memcpy(temp->traceRouteCtlTestName,
                       item->traceRouteCtlTestName,
                       item->traceRouteCtlTestNameLen + 1);
                temp->traceRouteCtlTestName[item->
                                            traceRouteCtlTestNameLen] =
                    '\0';
                temp->traceRouteCtlTestNameLen =
                    item->traceRouteCtlTestNameLen;

                /* add lock to protect */
                pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
                pthread_mutex_lock(&counter_mutex);
                temp->traceRouteHopsHopIndex = ++index;
                pthread_mutex_unlock(&counter_mutex);
                /* endsadsadsad */


                temp->traceRouteHopsIpTgtAddressType = 0;
                temp->traceRouteHopsIpTgtAddress = strdup("");
                temp->traceRouteHopsIpTgtAddressLen = 0;
                temp->traceRouteHopsMinRtt = 0;
                temp->traceRouteHopsMaxRtt = 0;
                temp->traceRouteHopsAverageRtt = 0;
                temp->traceRouteHopsRttSumOfSquares = 0;
                temp->traceRouteHopsSentProbes = 0;
                temp->traceRouteHopsProbeResponses = 0;

                temp->traceRouteHopsLastGoodProbeLen = 0;
                if (index == 1)
                    item->traceRouteHops = temp;
                else {
                    (current_temp)->next = temp;
                }

                current_temp = temp;

                if (index >= item->traceRouteCtlMaxTtl) {
                    current_temp->next = NULL;
                }

                if (item->traceRouteHops != NULL)

                    if (traceRouteHopsTable_add(current_temp) !=
                        SNMPERR_SUCCESS)
                        DEBUGMSGTL(("traceRouteHopsTable",
                                    "registered an entry error\n"));

            }

            register unsigned long maxRtt = 0;
            register unsigned long minRtt = 0;
            register unsigned long averageRtt = 0;
            register unsigned long sumRtt = 0;
            register unsigned long responseProbe = 0;
            register unsigned long sumOfSquare = 0;
            for (probe = 0; probe < nprobes; ++probe) {
                int             cc = 0, reset_timer = 0;
                struct timeval  t1, t2;
                struct timezone tz;
                register unsigned long Rtt = 0;

                gettimeofday(&t1, &tz);

                send_probe_v6(++seq, ttl, sendbuff, ident, &tz, sndsock,
                              datalen, &whereto, hostname);
                reset_timer = 1;

                while ((cc =
                        wait_for_reply_v6(icmp_sock, &from, reset_timer,
                                          waittime, icmp_sock,
                                          packet)) != 0) {
                    gettimeofday(&t2, &tz);
                    timep = 0;
                    time(&timep);
                    if ((i =
                         packet_ok_v6(packet, cc, &from, seq, &t1,
                                      ident))) {
                        reset_timer = 1;
                        if (memcmp
                            (&from.sin6_addr, &lastaddr,
                             sizeof(struct in6_addr))) {

                            memcpy(&lastaddr,
                                   &from.sin6_addr,
                                   sizeof(struct in6_addr));
                        }

                        Rtt = deltaT(&t1, &t2);
                        responseProbe = responseProbe + 1;
                        if (probe == 0) {
                            minRtt = Rtt;
                            maxRtt = Rtt;
                            averageRtt = Rtt;
                            sumRtt = Rtt;
                            sumOfSquare = Rtt * Rtt;
                        } else {
                            if (Rtt < minRtt)
                                minRtt = Rtt;
                            if (Rtt > maxRtt)
                                maxRtt = Rtt;
                            sumRtt = (sumRtt) + Rtt;
                            averageRtt =
                                round((double) (sumRtt) /
                                      (double) responseProbe);
                            sumOfSquare = sumOfSquare + Rtt * Rtt;
                        }

                        StorageResults->traceRouteResultsCurProbeCount =
                            probe + 1;


                        switch (i - 1) {
                        case ICMP6_DST_UNREACH_NOPORT:
                            ++got_there;
                            break;

                        case ICMP6_DST_UNREACH_NOROUTE:
                            ++unreachable;
                            Printf(" !N");
                            break;
                        case ICMP6_DST_UNREACH_ADDR:
                            ++unreachable;
                            Printf(" !H");
                            break;

                        case ICMP6_DST_UNREACH_ADMIN:
                            ++unreachable;
                            Printf(" !S");
                            break;
                        }
                        break;
                    } else
                        reset_timer = 0;
                }
                if (cc == 0) {
                    timep = 0;
                    time(&timep);
                    Printf(" *");
                    Rtt = (item->traceRouteCtlTimeOut) * 1000;
                }

                if (item->traceRouteCtlMaxRows != 0) {

                    temp_his =
                        SNMP_MALLOC_STRUCT
                        (traceRouteProbeHistoryTable_data);
                    temp_his->traceRouteCtlOwnerIndex =
                        (char *) malloc(item->traceRouteCtlOwnerIndexLen +
                                        1);
                    memcpy(temp_his->traceRouteCtlOwnerIndex,
                           item->traceRouteCtlOwnerIndex,
                           item->traceRouteCtlOwnerIndexLen + 1);
                    temp_his->traceRouteCtlOwnerIndex[item->
                                                      traceRouteCtlOwnerIndexLen]
                        = '\0';
                    temp_his->traceRouteCtlOwnerIndexLen =
                        item->traceRouteCtlOwnerIndexLen;

                    temp_his->traceRouteCtlTestName =
                        (char *) malloc(item->traceRouteCtlTestNameLen +
                                        1);
                    memcpy(temp_his->traceRouteCtlTestName,
                           item->traceRouteCtlTestName,
                           item->traceRouteCtlTestNameLen + 1);
                    temp_his->traceRouteCtlTestName[item->
                                                    traceRouteCtlTestNameLen]
                        = '\0';
                    temp_his->traceRouteCtlTestNameLen =
                        item->traceRouteCtlTestNameLen;

                    /* add lock to protect */
                    pthread_mutex_t counter_mutex =
                        PTHREAD_MUTEX_INITIALIZER;
                    pthread_mutex_lock(&counter_mutex);
                    if (item->traceRouteProbeHistoryMaxIndex >=
                        (unsigned long) (2147483647))
                        item->traceRouteProbeHistoryMaxIndex = 0;
                    temp_his->traceRouteProbeHistoryIndex =
                        ++(item->traceRouteProbeHistoryMaxIndex);
                    pthread_mutex_unlock(&counter_mutex);
                    /* endsadsadsad */
                    temp_his->traceRouteProbeHistoryHopIndex = ttl;
                    temp_his->traceRouteProbeHistoryProbeIndex = probe + 1;

                    temp_his->traceRouteProbeHistoryHAddrType = 2;
                    temp_his->traceRouteProbeHistoryHAddr =
                        (char *)
                        malloc(strlen
                               (inet_ntop
                                (AF_INET6, &from.sin6_addr, pa, 64)) + 1);
                    temp_his->traceRouteProbeHistoryHAddr =
                        strdup(inet_ntop
                               (AF_INET6, &from.sin6_addr, pa, 64));
                    temp_his->
                        traceRouteProbeHistoryHAddr[strlen
                                                    (inet_ntop
                                                     (AF_INET6,
                                                      &from.sin6_addr, pa,
                                                      64))] = '\0';
                    temp_his->traceRouteProbeHistoryHAddrLen =
                        strlen(inet_ntop
                               (AF_INET6, &from.sin6_addr, pa, 64));

                    temp_his->traceRouteProbeHistoryResponse = Rtt;
                    temp_his->traceRouteProbeHistoryStatus = 1;
                    temp_his->traceRouteProbeHistoryLastRC = 0;

		    temp_his->traceRouteProbeHistoryTime_time = timep;
                    memdup(&temp_his->traceRouteProbeHistoryTime,
                        date_n_time(&timep,
			    &temp_his->traceRouteProbeHistoryTimeLen), 11);

                    if (probe == 0)
                        item->traceRouteProbeHis = temp_his;
                    else {
                        (current_temp_his)->next = temp_his;
                    }

                    current_temp_his = temp_his;

                    if (probe + 1 >= nprobes) {
                        current_temp_his->next = NULL;
                    }

                    if (item->traceRouteProbeHis != NULL) {
                        if (traceRouteProbeHistoryTable_count(item) <
                            item->traceRouteCtlMaxRows) {
                            if (traceRouteProbeHistoryTable_add
                                (current_temp_his) != SNMPERR_SUCCESS)
                                DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                            "registered an entry error\n"));
                        } else {
                            traceRouteProbeHistoryTable_delLast(item);
                            if (traceRouteProbeHistoryTable_add
                                (current_temp_his) != SNMPERR_SUCCESS)
                                DEBUGMSGTL(("traceRouteProbeHistoryTable",
                                            "registered an entry error\n"));

                        }
		    }

                }
                if (item->traceRouteCtlCreateHopsEntries == 1) {
                    netsnmp_variable_list *vars_hops = NULL;
                    snmp_varlist_add_variable(&vars_hops, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlOwnerIndex, item->traceRouteCtlOwnerIndexLen);    /*  traceRouteCtlOwnerIndex  */
                    snmp_varlist_add_variable(&vars_hops, NULL, 0, ASN_OCTET_STR, (char *) item->traceRouteCtlTestName, item->traceRouteCtlTestNameLen);        /*  traceRouteCtlTestName  */
                    snmp_varlist_add_variable(&vars_hops, NULL, 0, ASN_UNSIGNED, (char *) &index, sizeof(index));       /*  traceRouteHopsIndex  */
                    if ((current =
                         header_complex_get(traceRouteHopsTableStorage,
                                            vars_hops)) == NULL)
                        return;
                    current->traceRouteHopsIpTgtAddressType = 2;
                    current->traceRouteHopsIpTgtAddress =
                        (char *)
                        malloc(strlen
                               (inet_ntop
                                (AF_INET6, &from.sin6_addr, pa, 64)) + 1);
                    current->traceRouteHopsIpTgtAddress =
                        strdup(inet_ntop
                               (AF_INET6, &from.sin6_addr, pa, 64));
                    current->
                        traceRouteHopsIpTgtAddress[strlen
                                                   (inet_ntop
                                                    (AF_INET6,
                                                     &from.sin6_addr, pa,
                                                     64))] = '\0';

                    if (count != 0) {
                        if (strcmp
                            (old_HopsAddress[index - 1],
                             current->traceRouteHopsIpTgtAddress) != 0)
                            flag = 1;
                    }

                    current->traceRouteHopsIpTgtAddressLen =
                        strlen(inet_ntop
                               (AF_INET6, &from.sin6_addr, pa, 64));
                    current->traceRouteHopsMinRtt = minRtt;
                    current->traceRouteHopsMaxRtt = maxRtt;
                    current->traceRouteHopsAverageRtt = averageRtt;
                    current->traceRouteHopsRttSumOfSquares = sumOfSquare;
                    current->traceRouteHopsSentProbes = probe + 1;
                    current->traceRouteHopsProbeResponses = responseProbe;
		    current->traceRouteHopsLastGoodProbe_time = timep;
                    memdup(&current->traceRouteHopsLastGoodProbe,
                        date_n_time(&timep,
			    &current->traceRouteHopsLastGoodProbeLen), 11);

                    snmp_free_varbind(vars_hops);
                    vars_hops = NULL;
                }


                (void) fflush(stdout);
            }
            putchar('\n');


            if (got_there || unreachable >= nprobes - 1) {


                if (got_there != 0) {
                    StorageResults->traceRouteResultsTestAttempts =
                        StorageResults->traceRouteResultsTestAttempts + 1;

                    StorageResults->traceRouteResultsTestSuccesses =
                        StorageResults->traceRouteResultsTestSuccesses + 1;
		    StorageResults->traceRouteResultsLastGoodPath_time = timep;
                    memdup(&StorageResults->traceRouteResultsLastGoodPath,
                        date_n_time(&timep,
			    &StorageResults->traceRouteResultsLastGoodPathLen), 11);
                    if ((item->
                         traceRouteCtlTrapGeneration[0] &
                         TRACEROUTETRAPGENERATION_TESTCOMPLETED) != 0) {
                        printf("TEST completed!\n");
                        send_traceRoute_trap(item, traceRouteTestCompleted,
                                             sizeof
                                             (traceRouteTestCompleted) /
                                             sizeof(oid));
                    }
                }

                else {
                    StorageResults->traceRouteResultsTestAttempts =
                        StorageResults->traceRouteResultsTestAttempts + 1;
                    if ((item->
                         traceRouteCtlTrapGeneration[0] &
                         TRACEROUTETRAPGENERATION_TESTFAILED) != 0) {
                        printf("test Failed!\n");
                        send_traceRoute_trap(item, traceRouteTestFailed,
                                             sizeof(traceRouteTestFailed) /
                                             sizeof(oid));
                    }
                }
                break;

            } else if (ttl == item->traceRouteCtlMaxTtl
                       && (probe + 1) == nprobes) {
                StorageResults->traceRouteResultsTestAttempts =
                    StorageResults->traceRouteResultsTestAttempts + 1;

                if ((item->
                     traceRouteCtlTrapGeneration[0] &
                     TRACEROUTETRAPGENERATION_TESTFAILED) != 0) {
                    printf("test Failed!\n");
                    send_traceRoute_trap(item, traceRouteTestFailed,
                                   sizeof(traceRouteTestFailed) /
                                   sizeof(oid));
                }
            }

        }

        close(sndsock);

        if (flag == 1) {
            printf("path changed!\n");
            send_traceRoute_trap(item, traceRoutePathChange,
                                 sizeof(traceRoutePathChange) /
                                 sizeof(oid));
        }

        int             k = 0;
        for (k = 0; k < count; k++) {
            free(old_HopsAddress[k]);
            old_HopsAddress[k] = NULL;
        }

    }
    return;
}


int
wait_for_reply(register int sock, register struct sockaddr_in *fromp,
               register const struct timeval *tp, u_char * packet,
               int waittime)
{
    fd_set          fds;
    struct timeval  now, wait;
    struct timezone tz;
    register int    cc = 0;
    socklen_t       fromlen = sizeof(*fromp);

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    wait.tv_sec = tp->tv_sec + waittime;
    wait.tv_usec = tp->tv_usec;
    (void) gettimeofday(&now, &tz);
    tvsub(&wait, &now);
    if (select(sock + 1, &fds, NULL, NULL, &wait) > 0)
        cc = recvfrom(sock, (char *) packet, 512, 0,
                      (struct sockaddr *) fromp, &fromlen);
    return (cc);
}


int
wait_for_reply_v6(int sock, struct sockaddr_in6 *from, int reset_timer,
                  int waittime, int icmp_sock, u_char * packet)
{
    fd_set          fds;
    static struct timeval wait;
    int             cc = 0;
    socklen_t       fromlen = sizeof(*from);

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    if (reset_timer) {
        /*
         * traceroute could hang if someone else has a ping
         * running and our ICMP reply gets dropped but we don't
         * realize it because we keep waking up to handle those
         * other ICMP packets that keep coming in.  To fix this,
         * "reset_timer" will only be true if the last packet that
         * came in was for us or if this is the first time we're
         * waiting for a reply since sending out a probe.  Note
         * that this takes advantage of the select() feature on
         * Linux where the remaining timeout is written to the
         * struct timeval area.
         */
        wait.tv_sec = waittime;
        wait.tv_usec = 0;
    }

    if (select(sock + 1, &fds, (fd_set *) 0, (fd_set *) 0, &wait) > 0) {
        cc = recvfrom(icmp_sock, (char *) packet, 512, 0,
                      (struct sockaddr *) from, &fromlen);
    }

    return (cc);
}

/*
 * send_probe() uses the BSD-ish udpiphdr.
 * Define something that looks enough like it to work.
 */
struct udpiphdr {
   struct iphdr ui_i;
   struct udphdr ui_u;
};
#define ui_src ui_i.saddr
#define ui_dst ui_i.daddr
#define ui_pr ui_i.protocol
#define ui_len ui_i.tot_len

void
send_probe(struct sockaddr_in *whereto, register int seq, int ttl,
           register struct timeval *tp, register struct ip *outip,
           register struct udphdr *outudp, int packlen, int optlen,
           char *hostname, u_short ident, int sndsock, u_short port,
           struct outdata *outdata)
{
    register int    cc = 0;
    register struct udpiphdr *ui = NULL, *oui = NULL;
    struct ip       tip;

    outip->ip_ttl = ttl;
#ifndef __hpux
    outip->ip_id = htons(ident + seq);
#endif

    /*
     * In most cases, the kernel will recalculate the ip checksum.
     * But we must do it anyway so that the udp checksum comes out
     * right.
     */

    outip->ip_sum =
        in_checksum((u_short *) outip, sizeof(*outip) + optlen);
    if (outip->ip_sum == 0)
        outip->ip_sum = 0xffff;


    /*
     * Payload 
     */
    outdata->seq = seq;
    outdata->ttl = ttl;
    outdata->tv = *tp;


    outudp->dest = htons(port + seq);


    /*
     * Checksum (we must save and restore ip header) 
     */
    tip = *outip;
    ui = (struct udpiphdr *) outip;
    oui = (struct udpiphdr *) &tip;
    /*
     * Easier to zero and put back things that are ok 
     */
    memset((char *) ui, 0, sizeof(ui->ui_i));
    ui->ui_src = oui->ui_src;
    ui->ui_dst = oui->ui_dst;
    ui->ui_pr = oui->ui_pr;
    ui->ui_len = outudp->len;
    outudp->check = 0;
    outudp->check = in_checksum((u_short *) ui, packlen);
    if (outudp->check == 0)
        outudp->check = 0xffff;
    *outip = tip;


    /*
     * XXX undocumented debugging hack 
     */


#if !defined(IP_HDRINCL) && defined(IP_TTL)
    printf("ttl\n");
    if (setsockopt(sndsock, IPPROTO_IP, IP_TTL,
                   (char *) &ttl, sizeof(ttl)) < 0) {
        Fprintf(stderr, "%s: setsockopt ttl %d: %s\n",
                "traceroute", ttl, strerror(errno));
        exit(1);
    }
#endif

#ifdef __hpux

    Printf("whereto=%s\n",
           inet_ntoa(((struct sockaddr_in *) whereto)->sin_addr));
    cc = sendto(sndsock, (char *) outudp,
                packlen - (sizeof(*outip) + optlen), 0, whereto,
                sizeof(*whereto));
    if (cc > 0)
        cc += sizeof(*outip) + optlen;
#else

    cc = sendto(sndsock, (char *) outip,
                packlen, 0, whereto, sizeof(*whereto));
#endif
    if (cc < 0 || cc != packlen) {
        if (cc < 0)
            Fprintf(stderr, "%s: sendto: %s\n", "traceroute", strerror(errno));
        Printf("%s: wrote %s %d chars, ret=%d\n",
               "traceroute", hostname, packlen, cc);
        (void) fflush(stdout);
    }
}



void
send_probe_v6(int seq, int ttl, char *sendbuff, pid_t ident,
              struct timezone *tz, int sndsock, int datalen,
              struct sockaddr_in6 *whereto, char *hostname)
{
    struct pkt_format *pkt = (struct pkt_format *) sendbuff;
    int             i = 0;

    pkt->ident = htonl(ident);
    pkt->seq = htonl(seq);
    gettimeofday(&pkt->tv, tz);

    i = setsockopt(sndsock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl,
                   sizeof(int));
    if (i < 0) {
        perror("setsockopt");
        exit(1);
    }

    do {
        i = sendto(sndsock, sendbuff, datalen, 0,
                   (struct sockaddr *) whereto,
                   sizeof(struct sockaddr_in6));
    } while (i < 0 && errno == ECONNREFUSED);

    if (i < 0 || i != datalen) {
        if (i < 0)
            perror("sendto");
        Printf("traceroute: wrote %s %d chars, ret=%d\n", hostname,
               datalen, i);
        (void) fflush(stdout);
    }
}


unsigned long
deltaT(struct timeval *t1p, struct timeval *t2p)
{
    register unsigned long dt;

    dt = (unsigned long) ((long) (t2p->tv_sec - t1p->tv_sec) * 1000 +
                          (long) (t2p->tv_usec - t1p->tv_usec) / 1000);
    return (dt);
}


int
packet_ok(register u_char * buf, int cc, register struct sockaddr_in *from,
          register int seq, u_short ident, int pmtu, u_short port)
{
    register struct icmp *icp = NULL;
    register u_char type, code;
    register int    hlen = 0;
#ifndef ARCHAIC
    register struct ip *ip = NULL;

    ip = (struct ip *) buf;
    hlen = ip->ip_hl << 2;
    if (cc < hlen + ICMP_MINLEN) {

        return (0);
    }
    cc -= hlen;
    icp = (struct icmp *) (buf + hlen);
#else
    icp = (struct icmp *) buf;
#endif
    type = icp->icmp_type;
    code = icp->icmp_code;
    /*
     * Path MTU Discovery (RFC1191) 
     */
    if (code != ICMP_UNREACH_NEEDFRAG)
        pmtu = 0;
    else {
#ifdef HAVE_ICMP_NEXTMTU
        pmtu = ntohs(icp->icmp_nextmtu);
#else
        pmtu = ntohs(((struct my_pmtu *) &icp->icmp_void)->ipm_nextmtu);
#endif
    }
    if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
        type == ICMP_UNREACH || type == ICMP_ECHOREPLY) {
        register struct ip *hip;
        register struct udphdr *up;

        hip = &icp->icmp_ip;
        hlen = hip->ip_hl << 2;
        up = (struct udphdr *) ((u_char *) hip + hlen);
        /*
         * XXX 8 is a magic number 
         */
        if (hlen + 12 <= cc &&
            hip->ip_p == IPPROTO_UDP &&
            up->source == htons(ident) && up->dest == htons(port + seq))
            return (type == ICMP_TIMXCEED ? -1 : code + 1);
    }


    return (0);
}




int
packet_ok_v6(u_char * buf, int cc, struct sockaddr_in6 *from, int seq,
             struct timeval *tv, pid_t ident)
{
    struct icmp6_hdr *icp = NULL;
    u_char          type, code;

    icp = (struct icmp6_hdr *) buf;

    type = icp->icmp6_type;
    code = icp->icmp6_code;

    if ((type == ICMP6_TIME_EXCEEDED && code == ICMP6_TIME_EXCEED_TRANSIT) ||
        type == ICMP6_DST_UNREACH) {
        struct ip6_hdr  *hip = NULL;
        struct udphdr  *up = NULL;
        int             nexthdr = 0;

        hip = (struct ip6_hdr *) (icp + 1);
        up = (struct udphdr *) (hip + 1);
        nexthdr = hip->ip6_nxt;

        if (nexthdr == 44) {
            nexthdr = *(unsigned char *) up;
            up++;
        }
        if (nexthdr == IPPROTO_UDP) {
            struct pkt_format *pkt;

            pkt = (struct pkt_format *) (up + 1);

            if (ntohl(pkt->ident) == ident && ntohl(pkt->seq) == seq) {
                *tv = pkt->tv;
                return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
            }
        }

    }

    return (0);
}


/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */

u_short
in_checksum(register u_short * addr, register int len)
{
    register int    nleft = len;
    register u_short *w = addr;
    register u_short answer;
    register int    sum = 0;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /*
     * mop up an odd byte, if necessary 
     */
    if (nleft == 1)
        sum += *(u_char *) w;

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return (answer);
}

/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be >= in.
 */
void
tvsub(register struct timeval *out, register struct timeval *in)
{

    if ((out->tv_usec -= in->tv_usec) < 0) {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}


struct hostinfo *
gethostinfo(register char *hostname)
{
    register int    n;
    register struct hostent *hp = NULL;
    register struct hostinfo *hi = NULL;
    register char **p = NULL;
    register u_int32_t addr, *ap = NULL;

    if (strlen(hostname) > 64) {
        Fprintf(stderr, "%s: hostname \"%.32s...\" is too long\n",
                "traceroute", hostname);
        exit(1);
    }
    hi = calloc(1, sizeof(*hi));
    if (hi == NULL) {
        Fprintf(stderr, "%s: calloc %s\n", "traceroute", strerror(errno));
        exit(1);
    }
    addr = inet_addr(hostname);
    if ((int32_t) addr != -1) {
        hi->name = strdup(hostname);
        hi->n = 1;
        hi->addrs = calloc(1, sizeof(hi->addrs[0]));
        if (hi->addrs == NULL) {
            Fprintf(stderr, "%s: calloc %s\n", "traceroute", strerror(errno));
            exit(1);
        }
        hi->addrs[0] = addr;
        return (hi);
    }

    hp = netsnmp_gethostbyname(hostname);
    if (hp == NULL) {
        Fprintf(stderr, "%s: unknown host %s\n", "traceroute", hostname);
        printf("hp=NULL\n");
        exit(1);
    }
    if (hp->h_addrtype != AF_INET || hp->h_length != 4) {
        Fprintf(stderr, "%s: bad host %s\n", "traceroute", hostname);
        exit(1);
    }
    hi->name = strdup(hp->h_name);
    for (n = 0, p = hp->h_addr_list; *p != NULL; ++n, ++p)
        continue;
    hi->n = n;
    hi->addrs = calloc(n, sizeof(hi->addrs[0]));
    if (hi->addrs == NULL) {
        Fprintf(stderr, "%s: calloc %s\n", "traceroute", strerror(errno));
        exit(1);
    }
    for (ap = hi->addrs, p = hp->h_addr_list; *p != NULL; ++ap, ++p)
        memcpy(ap, *p, sizeof(*ap));
    return (hi);
}

void
freehostinfo(register struct hostinfo *hi)
{
    if (hi->name != NULL) {
        free(hi->name);
        hi->name = NULL;
    }
    free((char *) hi->addrs);
    free((char *) hi);
}

void
setsin(register struct sockaddr_in *sin, register u_int32_t addr)
{

    memset(sin, 0, sizeof(*sin));
#ifdef HAVE_SOCKADDR_SA_LEN
    sin->sin_len = sizeof(*sin);
#endif
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = addr;
}


/*
 * Return the source address for the given destination address
 */
const char     *
findsaddr(register const struct sockaddr_in *to,
          register struct sockaddr_in *from)
{
    register int    i, n;
    register FILE  *f;
    register u_int32_t mask;
    u_int32_t       dest, tmask;
    struct ifaddrlist *al;
    char            buf[256], tdevice[256], device[256];
    static char     errbuf[132];
    static const char route[] = "/proc/net/route";

    if ((f = fopen(route, "r")) == NULL) {
        sprintf(errbuf, "open %s: %.128s", route, strerror(errno));
        return (errbuf);
    }

    /*
     * Find the appropriate interface 
     */
    n = 0;
    mask = 0;
    device[0] = '\0';
    while (fgets(buf, sizeof(buf), f) != NULL) {
        ++n;
        if (n == 1 && strncmp(buf, "Iface", 5) == 0)
            continue;
        if ((i = sscanf(buf, "%s %x %*s %*s %*s %*s %*s %x",
                        tdevice, &dest, &tmask)) != 3)
            return ("junk in buffer");
        if ((to->sin_addr.s_addr & tmask) == dest &&
            (tmask > mask || mask == 0)) {
            mask = tmask;
            strcpy(device, tdevice);
        }
    }
    fclose(f);

    if (device[0] == '\0')
        return ("Can't find interface");

    /*
     * Get the interface address list 
     */
    if ((n = ifaddrlist(&al, errbuf)) < 0)
        return (errbuf);

    if (n == 0)
        return ("Can't find any network interfaces");

    /*
     * Find our appropriate source address 
     */
    for (i = n; i > 0; --i, ++al)
        if (strcmp(device, al->device) == 0)
            break;
    if (i <= 0) {
        sprintf(errbuf, "Can't find interface \"%.32s\"", device);
        return (errbuf);
    }

    setsin(from, al->addr);
    return (NULL);
}

int
ifaddrlist(register struct ifaddrlist **ipaddrp, register char *errbuf)
{
    register int    fd, nipaddr;
#ifdef HAVE_SOCKADDR_SA_LEN
    register int    n;
#endif
    register struct ifreq *ifrp, *ifend, *ifnext;
    register struct sockaddr_in *sin;
    register struct ifaddrlist *al;
    struct ifconf   ifc;
    struct ifreq    ibuf[(32 * 1024) / sizeof(struct ifreq)], ifr;
#define MAX_IPADDR (sizeof(ibuf) / sizeof(ibuf[0]))
    static struct ifaddrlist ifaddrlist[MAX_IPADDR];
    char            device[sizeof(ifr.ifr_name) + 1];

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        (void) sprintf(errbuf, "socket: %s", strerror(errno));
        return (-1);
    }
    ifc.ifc_len = sizeof(ibuf);
    ifc.ifc_buf = (caddr_t) ibuf;

    if (ioctl(fd, SIOCGIFCONF, (char *) &ifc) < 0 ||
        ifc.ifc_len < sizeof(struct ifreq)) {
        if (errno == EINVAL)
            (void) sprintf(errbuf,
                           "SIOCGIFCONF: ifreq struct too small (%d bytes)",
                           (int)sizeof(ibuf));
        else
            (void) sprintf(errbuf, "SIOCGIFCONF: %s", strerror(errno));
        (void) close(fd);
        return (-1);
    }
    ifrp = ibuf;
    ifend = (struct ifreq *) ((char *) ibuf + ifc.ifc_len);

    al = ifaddrlist;
    nipaddr = 0;
    for (; ifrp < ifend; ifrp = ifnext) {
#ifdef HAVE_SOCKADDR_SA_LEN
        n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
        if (n < sizeof(*ifrp))
            ifnext = ifrp + 1;
        else
            ifnext = (struct ifreq *) ((char *) ifrp + n);
        if (ifrp->ifr_addr.sa_family != AF_INET)
            continue;
#else
        ifnext = ifrp + 1;
#endif
        /*
         * Need a template to preserve address info that is
         * used below to locate the next entry.  (Otherwise,
         * SIOCGIFFLAGS stomps over it because the requests
         * are returned in a union.)
         */
        strlcpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
        if (ioctl(fd, SIOCGIFFLAGS, (char *) &ifr) < 0) {
            if (errno == ENXIO)
                continue;
            (void) sprintf(errbuf, "SIOCGIFFLAGS: %.*s: %s",
                           (int) sizeof(ifr.ifr_name), ifr.ifr_name,
                           strerror(errno));
            (void) close(fd);
            return (-1);
        }

        /*
         * Must be up 
         */
        if ((ifr.ifr_flags & IFF_UP) == 0)
            continue;

        sprintf(device, "%.*s", (int) sizeof(ifr.ifr_name), ifr.ifr_name);
#ifdef sun
        /*
         * Ignore sun virtual interfaces 
         */
        if (strchr(device, ':') != NULL)
            continue;
#endif
        if (ioctl(fd, SIOCGIFADDR, (char *) &ifr) < 0) {
            (void) sprintf(errbuf, "SIOCGIFADDR: %s: %s",
                           device, strerror(errno));
            (void) close(fd);
            return (-1);
        }

        if (nipaddr >= MAX_IPADDR) {
            (void) sprintf(errbuf, "Too many interfaces (%d)", nipaddr);
            (void) close(fd);
            return (-1);
        }
        sin = (struct sockaddr_in *) &ifr.ifr_addr;
        al->addr = sin->sin_addr.s_addr;
        al->device = strdup(device);
        ++al;
        ++nipaddr;
    }
    (void) close(fd);

    *ipaddrp = ifaddrlist;
    return (nipaddr);
}
