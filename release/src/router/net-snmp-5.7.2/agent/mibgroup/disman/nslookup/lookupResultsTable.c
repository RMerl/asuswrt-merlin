/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:lookupResultsTable.c
 *File Description:Rows of the lookupResultsTable MIB read.
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

#include "lookupCtlTable.h"
#include "lookupResultsTable.h"
#include "header_complex.h"


oid             lookupResultsTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 82, 1, 4 };

struct variable2 lookupResultsTable_variables[] = {
    {COLUMN_LOOKUPRESULTSADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupResultsTable, 2, {1, 2}},
    {COLUMN_LOOKUPRESULTSADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_lookupResultsTable, 2, {1, 3}}
};

/*
 * global storage of our data, saved in and configured by header_complex() 
 */

extern struct header_complex_index *lookupCtlTableStorage;
extern struct header_complex_index *lookupResultsTableStorage;

int
lookupResultsTable_inadd(struct lookupResultsTable_data *thedata);

void
lookupResultsTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr = NULL;
    struct lookupResultsTable_data *StorageDel = NULL;
    DEBUGMSGTL(("lookupResultsTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        StorageDel =
            header_complex_extract_entry(&lookupResultsTableStorage,
                                         hciptr);
        if (StorageDel != NULL) {
            SNMP_FREE(StorageDel->lookupCtlOwnerIndex);
            SNMP_FREE(StorageDel->lookupCtlOperationName);
            SNMP_FREE(StorageDel->lookupResultsAddress);
            SNMP_FREE(StorageDel);
        }
        DEBUGMSGTL(("lookupResultsTable", "cleaner  "));
    }

}
void
init_lookupResultsTable(void)
{

    DEBUGMSGTL(("lookupResultsTable", "initializing...  "));


    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("lookupResultsTable", lookupResultsTable_variables,
                 variable2, lookupResultsTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("lookupResultsTable",
                                  parse_lookupResultsTable, NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_lookupResultsTable, NULL);

    DEBUGMSGTL(("lookupResultsTable", "done.\n"));
}

/*
 * parse_mteObjectsTable():
 *   parses .conf file entries needed to configure the mib.
 */

void
parse_lookupResultsTable(const char *token, char *line)
{
    size_t          tmpint;
    struct lookupResultsTable_data *StorageTmp =
        SNMP_MALLOC_STRUCT(lookupResultsTable_data);

    DEBUGMSGTL(("lookupResultsTable", "parsing config...  "));


    if (StorageTmp == NULL) {
        config_perror("malloc failure");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->lookupCtlOwnerIndex,
                              &StorageTmp->lookupCtlOwnerIndexLen);
    if (StorageTmp->lookupCtlOwnerIndex == NULL) {
        config_perror("invalid specification for lookupCtlOwnerIndex");
        return;
    }

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->lookupCtlOperationName,
                              &StorageTmp->lookupCtlOperationNameLen);
    if (StorageTmp->lookupCtlOperationName == NULL) {
        config_perror("invalid specification for lookupCtlOperationName");
        return;
    }

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->lookupResultsIndex, &tmpint);
    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->lookupResultsAddressType,
                              &tmpint);
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->lookupResultsAddress,
                              &StorageTmp->lookupResultsAddressLen);
    if (StorageTmp->lookupResultsAddress == NULL) {
        config_perror("invalid specification for lookupResultsAddress");
        return;
    }


    lookupResultsTable_inadd(StorageTmp);

    /* lookupResultsTable_cleaner(lookupResultsTableStorage); */

    DEBUGMSGTL(("lookupResultsTable", "done.\n"));
}





/*
 * store_lookupResultsTable():
 *   stores .conf file entries needed to configure the mib.
 */

int
store_lookupResultsTable(int majorID, int minorID, void *serverarg,
                         void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr = NULL;
    size_t          tmpint;
    struct lookupResultsTable_data *StorageTmp = NULL;
    struct header_complex_index *hcindex = NULL;


    DEBUGMSGTL(("lookupResultsTable", "storing data...  "));


    for (hcindex = lookupResultsTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct lookupResultsTable_data *) hcindex->data;

        if (StorageTmp->storagetype != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "lookupResultsTable ");
            cptr = line + strlen(line);

            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->lookupCtlOwnerIndex,
                                       &StorageTmp->
                                       lookupCtlOwnerIndexLen);

            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->lookupCtlOperationName,
                                       &StorageTmp->
                                       lookupCtlOperationNameLen);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->lookupResultsIndex,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       lookupResultsAddressType, &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->lookupResultsAddress,
                                       &StorageTmp->
                                       lookupResultsAddressLen);

            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("lookupResultsTable", "done.\n"));
    return SNMPERR_SUCCESS;
}


int
lookupResultsTable_inadd(struct lookupResultsTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;

    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
    		(char *) thedata->lookupCtlOwnerIndex,
		thedata->lookupCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
    		(char *) thedata->lookupCtlOperationName,
		thedata->lookupCtlOperationNameLen);
    snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED,
    		(char *) &thedata->lookupResultsIndex,
		sizeof(thedata->lookupResultsIndex));

    /*
     * XXX: fill in default row values here into StorageNew 
     * 
     */


    DEBUGMSGTL(("lookupResultsTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */

    header_complex_add_data(&lookupResultsTableStorage, vars_list,
                            thedata);
    DEBUGMSGTL(("lookupResultsTable", "registered an entry\n"));


    DEBUGMSGTL(("lookupResultsTable", "done.\n"));
    vars_list = NULL;
    return SNMPERR_SUCCESS;
}


/*
 * var_lookupResultsTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_lookupResultsTable above.
 */
unsigned char  *
var_lookupResultsTable(struct variable *vp,
                       oid * name,
                       size_t *length,
                       int exact,
                       size_t *var_len, WriteMethod ** write_method)
{


    struct lookupResultsTable_data *StorageTmp = NULL;

    *write_method = NULL;
    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(lookupResultsTableStorage, vp, name, length, exact,
                        var_len, write_method)) == NULL) {
        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */

    switch (vp->magic) {

    case COLUMN_LOOKUPRESULTSADDRESSTYPE:
        *var_len = sizeof(StorageTmp->lookupResultsAddressType);
        return (u_char *) & StorageTmp->lookupResultsAddressType;

    case COLUMN_LOOKUPRESULTSADDRESS:
        *var_len = (StorageTmp->lookupResultsAddressLen);
        return (u_char *) StorageTmp->lookupResultsAddress;

    default:
        ERROR_MSG("");
    }

    return NULL;
}
