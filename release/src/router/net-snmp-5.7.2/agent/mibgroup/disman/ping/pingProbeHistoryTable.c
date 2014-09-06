/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:pingProbeHistoryTable.c
 *File Description:Rows of pingProbeHistoryTable MIB read.
 *              
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
 *pingProbeHistoryTable_variables_oid:
 *
 */

oid             pingProbeHistoryTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 80, 1, 4 };

struct variable2 pingProbeHistoryTable_variables[] = {
    {COLUMN_PINGPROBEHISTORYRESPONSE, ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_pingProbeHistoryTable, 2, {1, 2}},
    {COLUMN_PINGPROBEHISTORYSTATUS,    ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_pingProbeHistoryTable, 2, {1, 3}},
    {COLUMN_PINGPROBEHISTORYLASTRC,    ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_pingProbeHistoryTable, 2, {1, 4}},
    {COLUMN_PINGPROBEHISTORYTIME,    ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_pingProbeHistoryTable, 2, {1, 5}}
};


/*
 * global storage of our data, saved in and configured by header_complex() 
 */

extern struct header_complex_index *pingCtlTableStorage;
extern struct header_complex_index *pingProbeHistoryTableStorage;
int
pingProbeHistoryTable_inadd(struct pingProbeHistoryTable_data *thedata);

void
pingProbeHistoryTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr = NULL;

    DEBUGMSGTL(("pingProbeHistoryTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        header_complex_extract_entry(&pingProbeHistoryTableStorage, hciptr);
        DEBUGMSGTL(("pingProbeHistoryTable", "cleaner  "));
    }

}
void
init_pingProbeHistoryTable(void)
{

    DEBUGMSGTL(("pingProbeHistoryTable", "initializing...  "));


    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("pingProbeHistoryTable", pingProbeHistoryTable_variables,
                 variable2, pingProbeHistoryTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("pingProbeHistoryTable",
                                  parse_pingProbeHistoryTable, NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_pingProbeHistoryTable, NULL);

    DEBUGMSGTL(("pingProbeHistoryTable", "done.\n"));
}

/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */

void
parse_pingProbeHistoryTable(const char *token, char *line)
{
    size_t          tmpint;
    struct pingProbeHistoryTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(pingProbeHistoryTable_data);

    DEBUGMSGTL(("pingProbeHistoryTable", "parsing config...  "));


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
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingProbeHistoryIndex, &tmpint);
    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->pingProbeHistoryResponse,
                              &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingProbeHistoryStatus,
                              &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->pingProbeHistoryLastRC,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->pingProbeHistoryTime,
                              &StorageTmp->pingProbeHistoryTimeLen);
    if (StorageTmp->pingProbeHistoryTime == NULL) {
        config_perror("invalid specification for pingProbeHistoryTime");
        return;
    }


    pingProbeHistoryTable_inadd(StorageTmp);

    /* pingProbeHistoryTable_cleaner(pingProbeHistoryTableStorage); */

    DEBUGMSGTL(("pingProbeHistoryTable", "done.\n"));
}





/*
 * store_pingProbeHistoryTable():
 *   stores .conf file entries needed to configure the mib.
 */

int
store_pingProbeHistoryTable(int majorID, int minorID, void *serverarg,
                            void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr;
    size_t          tmpint;
    struct pingProbeHistoryTable_data *StorageTmp;
    struct header_complex_index *hcindex;


    DEBUGMSGTL(("pingProbeHistoryTable", "storing data...  "));


    for (hcindex = pingProbeHistoryTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct pingProbeHistoryTable_data *) hcindex->data;

        if (StorageTmp->storageType != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "pingProbeHistoryTable ");
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
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->pingProbeHistoryIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->
                                       pingProbeHistoryResponse, &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingProbeHistoryStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->pingProbeHistoryLastRC,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->pingProbeHistoryTime,
                                       &StorageTmp->
                                       pingProbeHistoryTimeLen);

            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("pingProbeHistoryTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


int
pingProbeHistoryTable_inadd(struct pingProbeHistoryTable_data *thedata)
{
    netsnmp_variable_list *vars_list;
    vars_list = NULL;


    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->pingCtlOwnerIndex, thedata->pingCtlOwnerIndexLen);  /* pingCtlOwnerIndex */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR, (char *) thedata->pingCtlTestName, thedata->pingCtlTestNameLen);      /* pingCtlTestName */
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED, (char *) &thedata->pingProbeHistoryIndex, sizeof(thedata->pingProbeHistoryIndex));     /* pingProbeHistoryIndex */

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
    DEBUGMSGTL(("pingProbeHistoryTable", "registered an entry\n"));


    DEBUGMSGTL(("pingProbeHistoryTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


/*
 * var_pingProbeHistoryTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_pingProbeHistoryTable above.
 */
unsigned char  *
var_pingProbeHistoryTable(struct variable *vp,
                          oid * name,
                          size_t *length,
                          int exact,
                          size_t *var_len, WriteMethod ** write_method)
{


    struct pingProbeHistoryTable_data *StorageTmp = NULL;

    *write_method = NULL;
    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(pingProbeHistoryTableStorage, vp, name, length,
                        exact, var_len, write_method)) == NULL) {

        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */

    switch (vp->magic) {

    case COLUMN_PINGPROBEHISTORYRESPONSE:
        *var_len = sizeof(StorageTmp->pingProbeHistoryResponse);
        return (u_char *) & StorageTmp->pingProbeHistoryResponse;

    case COLUMN_PINGPROBEHISTORYSTATUS:
        *var_len = sizeof(StorageTmp->pingProbeHistoryStatus);
        return (u_char *) & StorageTmp->pingProbeHistoryStatus;

    case COLUMN_PINGPROBEHISTORYLASTRC:
        *var_len = sizeof(StorageTmp->pingProbeHistoryLastRC);
        return (u_char *) & StorageTmp->pingProbeHistoryLastRC;

    case COLUMN_PINGPROBEHISTORYTIME:
        *var_len = (StorageTmp->pingProbeHistoryTimeLen);
        return (u_char *) StorageTmp->pingProbeHistoryTime;

    default:
        ERROR_MSG("");
    }

    return NULL;
}
