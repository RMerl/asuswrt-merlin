/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:lookupCtlTable.c
 *File Description:Rows of the lookupCtlTable MIB add , delete and read.Rows of lookupResultsTable
 *              MIB add and delete.
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

/*
 * This should always be included first before anything else 
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(header_complex_find_entry)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

#include <arpa/inet.h>
#include <netdb.h>

#include "lookupCtlTable.h"
#include "lookupResultsTable.h"
#include "header_complex.h"

#ifndef INETADDRESSTYPE_ENUMS
#define INETADDRESSTYPE_ENUMS

#define INETADDRESSTYPE_UNKNOWN  0
#define INETADDRESSTYPE_IPV4     1
#define INETADDRESSTYPE_IPV6     2
#define INETADDRESSTYPE_IPV4Z    3
#define INETADDRESSTYPE_IPV6Z    4
#define INETADDRESSTYPE_DNS     16

#endif                          /* INETADDRESSTYPE_ENUMS */

/*
 *For discontinuity checking.
 */

oid             lookupCtlTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 82, 1, 3 };


#ifndef NETSNMP_NO_WRITE_SUPPORT
struct variable2 lookupCtlTable_variables[] = {
    /*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix
     */
    {COLUMN_LOOKUPCTLTARGETADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_lookupCtlTable, 2, {1, 3}},
    {COLUMN_LOOKUPCTLTARGETADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_lookupCtlTable, 2, {1, 4}},
    {COLUMN_LOOKUPCTLOPERSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 5}},
    {COLUMN_LOOKUPCTLTIME,      ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 6}},
    {COLUMN_LOOKUPCTLRC,         ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 7}},
    {COLUMN_LOOKUPCTLROWSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_lookupCtlTable, 2, {1, 8}}
};
#else /* !NETSNMP_NO_WRITE_SUPPORT */
struct variable2 lookupCtlTable_variables[] = {
    /*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix
     */
    {COLUMN_LOOKUPCTLTARGETADDRESSTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 3}},
    {COLUMN_LOOKUPCTLTARGETADDRESS,   ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 4}},
    {COLUMN_LOOKUPCTLOPERSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 5}},
    {COLUMN_LOOKUPCTLTIME,      ASN_UNSIGNED, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 6}},
    {COLUMN_LOOKUPCTLRC,         ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 7}},
    {COLUMN_LOOKUPCTLROWSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_lookupCtlTable, 2, {1, 8}}
};
#endif /* !NETSNMP_NO_WRITE_SUPPORT */


/*
 * global storage of our data, saved in and configured by header_complex() 
 */

struct header_complex_index *lookupCtlTableStorage = NULL;
struct header_complex_index *lookupResultsTableStorage = NULL;

int modify_lookupCtlTime(struct lookupTable_data *thedata, unsigned long val);
int modify_lookupCtlOperStatus(struct lookupTable_data *thedata, long val);
int modify_lookupCtlRc(struct lookupTable_data *thedata, long val);

void
init_lookupCtlTable(void)
{
    DEBUGMSGTL(("lookupCtlTable", "initializing...  "));
    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("lookupCtlTable", lookupCtlTable_variables, variable2,
                 lookupCtlTable_variables_oid);


    /*
     * register our config handler(s) to deal with registrations 
     */
    snmpd_register_config_handler("lookupCtlTable", parse_lookupCtlTable,
                                  NULL, NULL);

    /*
     * we need to be called back later to store our data 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_lookupCtlTable, NULL);

    DEBUGMSGTL(("lookupCtlTable", "done.\n"));
}

struct lookupTable_data *
create_lookupTable_data(void)
{
    struct lookupTable_data *StorageNew = NULL;
    StorageNew = SNMP_MALLOC_STRUCT(lookupTable_data);
    if (StorageNew == NULL) {
        snmp_log(LOG_ERR, "Out in memory in nslookup-mib/create_lookupTable_date\n");
        exit(1);
    }
    StorageNew->lookupCtlTargetAddress = strdup("");
    StorageNew->lookupCtlTargetAddressLen = 0;
    StorageNew->lookupCtlOperStatus = 2L;
    StorageNew->lookupCtlTime = 0;
    StorageNew->storagetype = ST_NONVOLATILE;
    return StorageNew;
}

/*
 * lookupCtlTable_add(): adds a structure node to our data set 
 */
int
lookupCtlTable_add(struct lookupTable_data *thedata)
{
    netsnmp_variable_list *vars = NULL;

    DEBUGMSGTL(("lookupCtlTable", "adding data...  "));
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOwnerIndex,
        thedata->lookupCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOperationName,
        thedata->lookupCtlOperationNameLen);


    if (header_complex_add_data(&lookupCtlTableStorage, vars, thedata) ==
        NULL) {
        return SNMPERR_GENERR;
    }
    DEBUGMSGTL(("lookupCtlTable", "registered an entry\n"));
    vars = NULL;

    DEBUGMSGTL(("lookupCtlTable", "done.\n"));
    return SNMPERR_SUCCESS;
}

int
lookupResultsTable_add(struct lookupTable_data *thedata)
{
    netsnmp_variable_list *vars_list = NULL;
    struct lookupResultsTable_data *p = NULL;
    p = thedata->ResultsTable;
    if (thedata->ResultsTable != NULL)
        do {
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                (char *) p->lookupCtlOwnerIndex,
                p->lookupCtlOwnerIndexLen);
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_OCTET_STR,
                (char *) p->lookupCtlOperationName,
                p->lookupCtlOperationNameLen);
            snmp_varlist_add_variable(&vars_list, NULL, 0, ASN_UNSIGNED,
                (char *) &p->lookupResultsIndex,
                sizeof(p->lookupResultsIndex));

            DEBUGMSGTL(("lookupResultsTable", "adding data...  "));
            /*
             * add the index variables to the varbind list, which is 
             * used by header_complex to index the data 
             */

            if (header_complex_add_data
                (&lookupResultsTableStorage, vars_list, p) == NULL) {
                return SNMPERR_GENERR;
            }

            DEBUGMSGTL(("lookupResultsTable", "out finished\n"));
            vars_list = NULL;
            p = p->next;
        } while (p != NULL);

    DEBUGMSGTL(("lookupResultsTable", "done.\n"));
    return SNMPERR_SUCCESS;
}

void
lookupCtlTable_cleaner(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr = NULL;
    struct lookupTable_data *StorageDel = NULL;
    DEBUGMSGTL(("lookupCtlTable", "cleanerout  "));
    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        StorageDel =
            header_complex_extract_entry(&lookupCtlTableStorage, hciptr);
        if (StorageDel != NULL) {
            free(StorageDel->lookupCtlOwnerIndex);
            StorageDel->lookupCtlOwnerIndex = NULL;
            free(StorageDel->lookupCtlOperationName);
            StorageDel->lookupCtlOperationName = NULL;
            free(StorageDel->lookupCtlTargetAddress);
            StorageDel->lookupCtlTargetAddress = NULL;
            free(StorageDel);
            StorageDel = NULL;

        }
        DEBUGMSGTL(("lookupCtlTable", "cleaner  "));
    }
}

/*
 * parse_lookupCtlTable():
 *   parses .conf file entries needed to configure the mib.
 */
void
parse_lookupCtlTable(const char *token, char *line)
{
    size_t          tmpint;
    struct lookupTable_data *StorageTmp = SNMP_MALLOC_STRUCT(lookupTable_data);

    DEBUGMSGTL(("lookupCtlTable", "parsing config...  "));


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
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->lookupCtlTargetAddressType,
                              &tmpint);

    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &StorageTmp->lookupCtlTargetAddress,
                              &StorageTmp->lookupCtlTargetAddressLen);
    if (StorageTmp->lookupCtlTargetAddress == NULL) {
        config_perror("invalid specification for lookupCtlTargetAddress");
        return;
    }

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->lookupCtlOperStatus, &tmpint);

    line =
        read_config_read_data(ASN_UNSIGNED, line,
                              &StorageTmp->lookupCtlTime, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->lookupCtlRc, &tmpint);

    line =
        read_config_read_data(ASN_INTEGER, line,
                              &StorageTmp->lookupCtlRowStatus, &tmpint);


    StorageTmp->storagetype = ST_NONVOLATILE;
    lookupCtlTable_add(StorageTmp);
    /* lookupCtlTable_cleaner(lookupCtlTableStorage); */

    DEBUGMSGTL(("lookupCtlTable", "done.\n"));
}



/*
 * store_lookupCtlTable():
 *   stores .conf file entries needed to configure the mib.
 */
int
store_lookupCtlTable(int majorID, int minorID, void *serverarg,
                     void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr;
    size_t          tmpint;
    struct lookupTable_data *StorageTmp;
    struct header_complex_index *hcindex;

    DEBUGMSGTL(("lookupCtlTable", "storing data...  "));

    for (hcindex = lookupCtlTableStorage; hcindex != NULL;
         hcindex = hcindex->next) {
        StorageTmp = (struct lookupTable_data *) hcindex->data;

        if (StorageTmp->storagetype != ST_READONLY) {
            memset(line, 0, sizeof(line));
            strcat(line, "lookupCtlTable ");
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
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->
                                       lookupCtlTargetAddressType,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_OCTET_STR, cptr,
                                       &StorageTmp->lookupCtlTargetAddress,
                                       &StorageTmp->
                                       lookupCtlTargetAddressLen);

            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->lookupCtlOperStatus,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_UNSIGNED, cptr,
                                       &StorageTmp->lookupCtlTime,
                                       &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->lookupCtlRc, &tmpint);
            cptr =
                read_config_store_data(ASN_INTEGER, cptr,
                                       &StorageTmp->lookupCtlRowStatus,
                                       &tmpint);
            snmpd_store_config(line);
        }
    }
    DEBUGMSGTL(("lookupCtlTable", "done.\n"));
    return SNMPERR_SUCCESS;
}




/*
 * var_lookupCtlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_mteObjectsTable above.
 */
unsigned char  *
var_lookupCtlTable(struct variable *vp,
                   oid * name,
                   size_t *length,
                   int exact, size_t *var_len, WriteMethod ** write_method)
{
    struct lookupTable_data *StorageTmp = NULL;
        *write_method = NULL;

    /*
     * this assumes you have registered all your data properly
     */
    if ((StorageTmp =
         header_complex(lookupCtlTableStorage, vp, name, length, exact,
                        var_len, write_method)) == NULL) {
        if (vp->magic == COLUMN_LOOKUPCTLROWSTATUS)
            *write_method = write_lookupCtlRowStatus;
        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */
    switch (vp->magic) {
    case COLUMN_LOOKUPCTLTARGETADDRESSTYPE:
#ifndef NETSNMP_NO_WRITE_SUPPORT
        *write_method = write_lookupCtlTargetAddressType;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        *var_len = sizeof(StorageTmp->lookupCtlTargetAddressType);
        return (u_char *) & StorageTmp->lookupCtlTargetAddressType;

    case COLUMN_LOOKUPCTLTARGETADDRESS:
#ifndef NETSNMP_NO_WRITE_SUPPORT
        *write_method = write_lookupCtlTargetAddress;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        *var_len = (StorageTmp->lookupCtlTargetAddressLen);
        return (u_char *) StorageTmp->lookupCtlTargetAddress;

    case COLUMN_LOOKUPCTLOPERSTATUS:
        *var_len = sizeof(StorageTmp->lookupCtlOperStatus);
        return (u_char *) & StorageTmp->lookupCtlOperStatus;

    case COLUMN_LOOKUPCTLTIME:
        *var_len = sizeof(StorageTmp->lookupCtlTime);
        return (u_char *) & StorageTmp->lookupCtlTime;

    case COLUMN_LOOKUPCTLRC:
        *var_len = sizeof(StorageTmp->lookupCtlRc);
        return (u_char *) & StorageTmp->lookupCtlRc;

    case COLUMN_LOOKUPCTLROWSTATUS:
#ifndef NETSNMP_NO_WRITE_SUPPORT
        *write_method = write_lookupCtlRowStatus;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        *var_len = sizeof(StorageTmp->lookupCtlRowStatus);
        return (u_char *) & StorageTmp->lookupCtlRowStatus;

    default:
        ERROR_MSG("");
    }
    return NULL;
}


static struct lookupResultsTable_data *
add_result(struct lookupTable_data *item, int index,
        int iatype, const void *data, size_t data_len)
{
    struct lookupResultsTable_data *temp;
    temp = SNMP_MALLOC_STRUCT(lookupResultsTable_data);
    if (temp == NULL) {
        snmp_log(LOG_ERR, "Out of memory in nslookup-mib/run_lookup\n");
        return NULL;
    }
    temp->lookupResultsIndex = index;
    temp->next = NULL;

    temp->lookupCtlOwnerIndex = malloc(item->lookupCtlOwnerIndexLen + 1);
    if (temp->lookupCtlOwnerIndex == NULL) {
        snmp_log(LOG_ERR, "Out of memory in nslookup-mib/run_lookup\n");
        free(temp);
        return NULL;
    }
    memcpy(temp->lookupCtlOwnerIndex,
           item->lookupCtlOwnerIndex,
           item->lookupCtlOwnerIndexLen + 1);
    temp->lookupCtlOwnerIndex[item->lookupCtlOwnerIndexLen] = '\0';
    temp->lookupCtlOwnerIndexLen = item->lookupCtlOwnerIndexLen;

    temp->lookupCtlOperationName = malloc(item->lookupCtlOperationNameLen + 1);
    if (temp->lookupCtlOperationName == NULL) {
        snmp_log(LOG_ERR, "Out of memory in nslookup-mib/run_lookup\n");
        free(temp->lookupCtlOwnerIndex);
        free(temp);
        return NULL;
    }
    memcpy(temp->lookupCtlOperationName,
           item->lookupCtlOperationName,
           item->lookupCtlOperationNameLen + 1);
    temp->lookupCtlOperationName[item->lookupCtlOperationNameLen] = '\0';
    temp->lookupCtlOperationNameLen = item->lookupCtlOperationNameLen;

    temp->lookupResultsAddressType = iatype;
    temp->lookupResultsAddress = malloc(data_len + 1);
    memcpy(temp->lookupResultsAddress, data, data_len);
    temp->lookupResultsAddress[data_len] = '\0';
    temp->lookupResultsAddressLen = data_len;
    if (!item->ResultsTable)
        item->ResultsTable = temp;

    return temp;
}

void
run_lookup(struct lookupTable_data *item)
{
    long            addressType;
    char           *address = NULL;
    size_t          addresslen;
    struct lookupResultsTable_data *current = NULL;
    struct lookupResultsTable_data *temp = NULL;
    int             i = 0, n = 1;

    struct timeval  tpstart, tpend;
    unsigned long   timeuse, timeuse4 = 0, timeuse6 = 0;

    if (item == NULL)
        return;

    addressType = (long) item->lookupCtlTargetAddressType;
    addresslen = (size_t) item->lookupCtlTargetAddressLen;
    address = (char *) malloc(addresslen + 1);
    memcpy(address, item->lookupCtlTargetAddress, addresslen + 1);
    address[addresslen] = '\0';

    if (addressType == INETADDRESSTYPE_IPV4) {
        struct in_addr addr_in;
        struct hostent *lookup;

        if (!inet_aton(address, &addr_in)) {
            DEBUGMSGTL(("lookupResultsTable", "Invalid argument: %s\n",
                        address));
            modify_lookupCtlRc(item, 99);
            return;
        }

        netsnmp_get_monotonic_clock(&tpstart);
        lookup = netsnmp_gethostbyaddr(&addr_in, sizeof(addr_in), AF_INET);
        netsnmp_get_monotonic_clock(&tpend);
        timeuse = 1000000 * (tpend.tv_sec - tpstart.tv_sec) +
            tpend.tv_usec - tpstart.tv_usec;
        timeuse /= 1000;
        modify_lookupCtlTime(item, timeuse);
        modify_lookupCtlOperStatus(item, 3L);

        if (lookup == NULL) {
            DEBUGMSGTL(("lookupCtlTable",
                        "Can't get a network host entry for ipv4 address: %s\n",
                        address));
            modify_lookupCtlRc(item, h_errno);
            return;
        } else {
            modify_lookupCtlRc(item, 0L);
            if (lookup->h_name != NULL) {
                current = temp = add_result(item, n, INETADDRESSTYPE_DNS,
                            lookup->h_name, strlen(lookup->h_name));
                n = n + 1;
            }

            i = 0;
            while (lookup->h_aliases[i]) {
                temp = add_result(item, n, INETADDRESSTYPE_DNS,
                            lookup->h_aliases[i], strlen(lookup->h_aliases[i]));
                current->next = temp;
                current = temp;
                i = i + 1;
                n = n + 1;
            }
        }

        if (item->ResultsTable != NULL)
            if (lookupResultsTable_add(item) != SNMPERR_SUCCESS)
                DEBUGMSGTL(("lookupResultsTable",
                            "registered an entry error\n"));
        SNMP_FREE(address);
        return;
    }

    else if (addressType == INETADDRESSTYPE_DNS) {
        struct hostent *lookup;
#if HAVE_GETADDRINFO
        int             res;
        struct addrinfo *ais;
        struct addrinfo hints = { 0, AF_INET6, SOCK_DGRAM };
#endif

        netsnmp_get_monotonic_clock(&tpstart);
        lookup = netsnmp_gethostbyname(address);
        netsnmp_get_monotonic_clock(&tpend);
        timeuse4 = 1000000 * (tpend.tv_sec - tpstart.tv_sec) +
            tpend.tv_usec - tpstart.tv_usec;
        if (lookup == NULL) {
            DEBUGMSGTL(("lookupCtlTable",
                        "Can't get a network host entry for %s\n",
                        address));
            modify_lookupCtlRc(item, h_errno);
        } else {
            while (lookup->h_addr_list[i]) {
                char buf[64];
                int buflen;

                inet_ntop(lookup->h_addrtype, lookup->h_addr_list[i], buf, sizeof(buf));
                buflen = strlen(buf);
                switch (lookup->h_addrtype) {
                case AF_INET:
                    temp = add_result(item, n, INETADDRESSTYPE_IPV4, buf, buflen);
                    break;
                case AF_INET6:
                    temp = add_result(item, n, INETADDRESSTYPE_IPV6, buf, buflen);
                    break;
                default:
                    snmp_log(LOG_ERR, "nslookup-mib/run_lookup: Unknown address type %d\n", lookup->h_addrtype);
                    temp = add_result(item, n, INETADDRESSTYPE_UNKNOWN, "", 0);
                    break;
                }
                DEBUGMSGTL(("lookupCtlTable", "Adding %d %s\n", n, buf));

                if (n == 1)
                    item->ResultsTable = temp;
                else
                    current->next = temp;
                current = temp;
                n = n + 1;
                i = i + 1;
            }
        }

#if HAVE_GETADDRINFO
        netsnmp_get_monotonic_clock(&tpstart);
        res = netsnmp_getaddrinfo(address, NULL, &hints, &ais);
        netsnmp_get_monotonic_clock(&tpend);
        timeuse6 = 1000000 * (tpend.tv_sec - tpstart.tv_sec) +
            tpend.tv_usec - tpstart.tv_usec;

        if (res != 0) {
            DEBUGMSGTL(("lookupCtlTable",
                        "Can't get a ipv6 network host entry for %s\n",
                        address));
            modify_lookupCtlRc(item, res);
        } else {
            struct addrinfo *aip = ais;
            while (aip) {
                char buf[64];
                int buflen;

                switch (aip->ai_family) {
                case AF_INET:
                    inet_ntop(aip->ai_family,
                            &((struct sockaddr_in *)aip->ai_addr)->sin_addr,
                        buf, sizeof(buf));
                    buflen = strlen(buf);
                    temp = add_result(item, n, INETADDRESSTYPE_IPV4, buf, buflen);
                    break;
                case AF_INET6:
                    inet_ntop(aip->ai_family,
                            &((struct sockaddr_in6 *)aip->ai_addr)->sin6_addr,
                        buf, sizeof(buf));
                    buflen = strlen(buf);
                    temp = add_result(item, n, INETADDRESSTYPE_IPV6, buf, buflen);
                    break;
                default:
                    snmp_log(LOG_ERR, "nslookup-mib/run_lookup: Unknown address type %d\n", aip->ai_family);
                    temp = add_result(item, n, INETADDRESSTYPE_UNKNOWN, "", 0);
                    break;
                }
                DEBUGMSGTL(("lookupCtlTable", "Adding %d %s\n", n, buf));

                if (n == 1)
                    item->ResultsTable = temp;
                else
                    current->next = temp;
                current = temp;
                n = n + 1;
                aip = aip->ai_next;
            }
            freeaddrinfo(ais);
        }
#elif HAVE_GETHOSTBYNAME2
        netsnmp_get_monotonic_clock(&tpstart);
        lookup = gethostbyname2(address, AF_INET6);
        netsnmp_get_monotonic_clock(&tpend);
        timeuse6 = 1000000 * (tpend.tv_sec - tpstart.tv_sec) +
            tpend.tv_usec - tpstart.tv_usec;

        if (lookup == NULL) {
            DEBUGMSGTL(("lookupCtlTable",
                        "Can't get a ipv6 network host entry for %s\n",
                        address));
            modify_lookupCtlRc(item, h_errno);
        } else {
            i = 0;
            while (lookup->h_addr_list[i]) {
                char buf[64];
                int buflen;

                inet_ntop(lookup->h_addrtype, lookup->h_addr_list[i],
                        buf, sizeof(buf));
                buflen = strlen(buf);
                switch (lookup->h_addrtype) {
                case AF_INET:
                    temp = add_result(item, n, INETADDRESSTYPE_IPV4, buf, buflen);
                    break;
                case AF_INET6:
                    temp = add_result(item, n, INETADDRESSTYPE_IPV6, buf, buflen);
                    break;
                default:
                    snmp_log(LOG_ERR, "nslookup-mib/run_lookup: Unknown address type %d\n", lookup->h_addrtype);
                    temp = add_result(item, n, INETADDRESSTYPE_UNKNOWN, "", 0);
                    break;
                }
                DEBUGMSGTL(("lookupCtlTable", "Adding %d %s\n", n, buf));

                if (n == 1)
                    item->ResultsTable = temp;
                else
                    current->next = temp;
                current = temp;
                n = n + 1;
                i = i + 1;
            }
        }
#endif

        timeuse = timeuse4 + timeuse6;
        timeuse /= 1000;
        modify_lookupCtlTime(item, timeuse);
        modify_lookupCtlOperStatus(item, 3L);

        if (item->ResultsTable != NULL) {
            modify_lookupCtlRc(item, 0L);
            if (lookupResultsTable_add(item) != SNMPERR_SUCCESS)
                DEBUGMSGTL(("lookupResultsTable",
                            "registered an entry error\n"));
        }
        SNMP_FREE(address);
        return;
    }

    else if (addressType == INETADDRESSTYPE_IPV6) {
        struct in6_addr addr_in6;
        struct hostent *lookup;

        if (inet_pton(AF_INET6, address, &addr_in6) == 1)
            DEBUGMSGTL(("lookupCtlTable", "success! \n"));
        else
            DEBUGMSGTL(("lookupCtlTable", "error! \n"));

        netsnmp_get_monotonic_clock(&tpstart);
        lookup = netsnmp_gethostbyaddr(&addr_in6, sizeof(addr_in6), AF_INET6);
        netsnmp_get_monotonic_clock(&tpend);
        timeuse = 1000000 * (tpend.tv_sec - tpstart.tv_sec) +
            tpend.tv_usec - tpstart.tv_usec;
        timeuse /= 1000;
        modify_lookupCtlTime(item, timeuse);
        modify_lookupCtlOperStatus(item, 3L);

        if (lookup == NULL) {
            DEBUGMSGTL(("lookupCtlTable",
                        "Can't get a network host entry for %s\n",
                        address));
            modify_lookupCtlRc(item, h_errno);
            return;
        } else {
            modify_lookupCtlRc(item, 0L);
            if (lookup->h_name != NULL) {
                current = temp = add_result(item, n, INETADDRESSTYPE_DNS,
                            lookup->h_name, strlen(lookup->h_name));
                n = n + 1;
            }

            i = 0;
            while (lookup->h_aliases[i]) {
                current = temp = add_result(item, n, INETADDRESSTYPE_DNS,
                            lookup->h_aliases[i], strlen(lookup->h_aliases[i]));
                current->next = temp;
                current = temp;
                i = i + 1;
                n = n + 1;
            }

            if (item->ResultsTable != NULL)
                current->next = NULL;
            else
                current = NULL;
        }

        if (item->ResultsTable != NULL)
            if (lookupResultsTable_add(item) != SNMPERR_SUCCESS)
                DEBUGMSGTL(("lookupResultsTable",
                            "registered an entry error\n"));
        SNMP_FREE(address);
        return;
    } else {
        SNMP_FREE(address);
        return;
    }
}


int
modify_lookupCtlOperStatus(struct lookupTable_data *thedata, long val)
{
    netsnmp_variable_list *vars = NULL;
    struct lookupTable_data *StorageTmp = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOwnerIndex, thedata->lookupCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOperationName,
        thedata->lookupCtlOperationNameLen);


    if ((StorageTmp =
         header_complex_get(lookupCtlTableStorage, vars)) == NULL) {
        snmp_free_varbind(vars);
        vars = NULL;
        return SNMP_ERR_NOSUCHNAME;
    }
    StorageTmp->lookupCtlOperStatus = val;

    snmp_free_varbind(vars);
    vars = NULL;

    DEBUGMSGTL(("lookupOperStatus", "done.\n"));
    return SNMPERR_SUCCESS;
}

int
modify_lookupCtlTime(struct lookupTable_data *thedata, unsigned long val)
{
    netsnmp_variable_list *vars = NULL;
    struct lookupTable_data *StorageTmp = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOwnerIndex, thedata->lookupCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOperationName,
        thedata->lookupCtlOperationNameLen);


    if ((StorageTmp =
         header_complex_get(lookupCtlTableStorage, vars)) == NULL) {
        snmp_free_varbind(vars);
        vars = NULL;
        return SNMP_ERR_NOSUCHNAME;
    }
    StorageTmp->lookupCtlTime = val;

    snmp_free_varbind(vars);
    vars = NULL;

    DEBUGMSGTL(("lookupCtlTime", "done.\n"));
    return SNMPERR_SUCCESS;
}

int
modify_lookupCtlRc(struct lookupTable_data *thedata, long val)
{
    netsnmp_variable_list *vars = NULL;
    struct lookupTable_data *StorageTmp = NULL;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOwnerIndex, thedata->lookupCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOperationName,
        thedata->lookupCtlOperationNameLen);


    if ((StorageTmp =
         header_complex_get(lookupCtlTableStorage, vars)) == NULL) {
        snmp_free_varbind(vars);
        vars = NULL;
        return SNMP_ERR_NOSUCHNAME;
    }
    StorageTmp->lookupCtlRc = val;

    snmp_free_varbind(vars);
    vars = NULL;
    DEBUGMSGTL(("lookupOperStatus", "done.\n"));
    return SNMPERR_SUCCESS;
}


int
lookupResultsTable_del(struct lookupTable_data *thedata)
{
    struct header_complex_index *hciptr2 = NULL;
    struct lookupResultsTable_data *StorageDel = NULL;
    netsnmp_variable_list *vars = NULL;
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;

    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOwnerIndex, thedata->lookupCtlOwnerIndexLen);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR,
        (char *) thedata->lookupCtlOperationName,
        thedata->lookupCtlOperationNameLen);
    memset(newoid, '\0', MAX_OID_LEN * sizeof(oid));
    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, vars);


    snmp_free_varbind(vars);
    vars = NULL;
    for (hciptr2 = lookupResultsTableStorage; hciptr2 != NULL;
         hciptr2 = hciptr2->next) {
        if (snmp_oid_compare(newoid, newoid_len, hciptr2->name, newoid_len)
            == 0) {
            StorageDel =
                header_complex_extract_entry(&lookupResultsTableStorage,
                                             hciptr2);
            if (StorageDel != NULL) {
                SNMP_FREE(StorageDel->lookupCtlOwnerIndex);
                SNMP_FREE(StorageDel->lookupCtlOperationName);
                SNMP_FREE(StorageDel->lookupResultsAddress);
                SNMP_FREE(StorageDel);
            }
            DEBUGMSGTL(("lookupResultsTable", "delete  success!\n"));

        }
    }
    return SNMPERR_SUCCESS;
}


#ifndef NETSNMP_NO_WRITE_SUPPORT
int
write_lookupCtlTargetAddressType(int action,
                                 u_char * var_val,
                                 u_char var_val_type,
                                 size_t var_val_len,
                                 u_char * statP, oid * name,
                                 size_t name_len)
{
    static size_t   tmpvar;
    struct lookupTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(lookupCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);

    if ((StorageTmp =
         header_complex(lookupCtlTableStorage, NULL,
                        &name[sizeof(lookupCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storagetype == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->lookupCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }
    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            snmp_log(LOG_ERR,
                     "write to lookupCtlTargetAddressType not ASN_INTEGER\n");
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
        tmpvar = StorageTmp->lookupCtlTargetAddressType;
        StorageTmp->lookupCtlTargetAddressType = *((long *) var_val);
        break;

    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        StorageTmp->lookupCtlTargetAddressType = tmpvar;
        break;

    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */

        /** set up to save persistent store */
        snmp_store_needed(NULL);

        break;
    }
    return SNMP_ERR_NOERROR;
}



int
write_lookupCtlTargetAddress(int action,
                             u_char * var_val,
                             u_char var_val_type,
                             size_t var_val_len,
                             u_char * statP, oid * name, size_t name_len)
{
    static char    *tmpvar = NULL;
    static size_t   tmplen;
    struct lookupTable_data *StorageTmp = NULL;
    size_t          newlen =
        name_len - (sizeof(lookupCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);
    if ((StorageTmp =
         header_complex(lookupCtlTableStorage, NULL,
                        &name[sizeof(lookupCtlTable_variables_oid) /
                              sizeof(oid) + 3 - 1], &newlen, 1, NULL,
                        NULL)) == NULL)
        return SNMP_ERR_NOSUCHNAME;     /* remove if you support creation here */

    if (StorageTmp && StorageTmp->storagetype == ST_READONLY) {
        return SNMP_ERR_NOTWRITABLE;
    }

    if (StorageTmp && StorageTmp->lookupCtlRowStatus == RS_ACTIVE) {
        return SNMP_ERR_NOTWRITABLE;
    }

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_OCTET_STR) {
            snmp_log(LOG_ERR,
                     "write to lookupCtlTargetAddress not ASN_OCTET_STR\n");
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
        tmpvar = StorageTmp->lookupCtlTargetAddress;
        tmplen = StorageTmp->lookupCtlTargetAddressLen;

        if ((StorageTmp->lookupCtlTargetAddress =
             (char *) malloc(var_val_len + 1)) == NULL) {
            snmp_log(LOG_ERR, "Out of memory in nslookup-mib/write_lookupCtlTargetAddress\n");
            exit(1);
        }
        memcpy(StorageTmp->lookupCtlTargetAddress, var_val, var_val_len);
        StorageTmp->lookupCtlTargetAddress[var_val_len] = '\0';
        StorageTmp->lookupCtlTargetAddressLen = var_val_len;

        break;

    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        free(StorageTmp->lookupCtlTargetAddress);
        StorageTmp->lookupCtlTargetAddress = NULL;
        StorageTmp->lookupCtlTargetAddress = tmpvar;
        StorageTmp->lookupCtlTargetAddressLen = tmplen;

        break;

    case COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */

        free(tmpvar);
        tmpvar = NULL;

        /** set up to save persistent store */
        snmp_store_needed(NULL);

        break;
    }
    return SNMP_ERR_NOERROR;
}



int
write_lookupCtlRowStatus(int action,
                         u_char * var_val,
                         u_char var_val_type,
                         size_t var_val_len,
                         u_char * statP, oid * name, size_t name_len)
{
    struct lookupTable_data *StorageTmp = NULL;
    static struct lookupTable_data *StorageNew = NULL, *StorageDel = NULL;
    size_t          newlen =
        name_len - (sizeof(lookupCtlTable_variables_oid) / sizeof(oid) +
                    3 - 1);
    static int      old_value;
    int             set_value;
    static netsnmp_variable_list *vars, *vp;
    struct header_complex_index *hciptr = NULL;

    StorageTmp =
        header_complex(lookupCtlTableStorage, NULL,
                       &name[sizeof(lookupCtlTable_variables_oid) /
                             sizeof(oid) + 3 - 1], &newlen, 1, NULL, NULL);

    if (var_val_type != ASN_INTEGER || var_val == NULL) {
        snmp_log(LOG_ERR, "write to lookupCtlRowStatus not ASN_INTEGER\n");
        return SNMP_ERR_WRONGTYPE;
    }
    if (StorageTmp && StorageTmp->storagetype == ST_READONLY) {
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

            if (StorageTmp->lookupCtlRowStatus == RS_ACTIVE &&
                set_value != RS_DESTROY) {
                /*
                 * "Once made active an entry may not be modified except to 
                 * delete it."  XXX: doesn't this in fact apply to ALL
                 * columns of the table and not just this one?  
                 */
                return SNMP_ERR_INCONSISTENTVALUE;
            }
            if (StorageTmp->storagetype != ST_NONVOLATILE)
                return SNMP_ERR_NOTWRITABLE;
        }
        break;




    case RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        if (StorageTmp == NULL) {
            /*
             * creation 
             */
            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
            vars = NULL;

            /*
             * 将name为空的三个索引字段加到var变量列表的末尾 
             */
            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);  /* lookupCtlOwnerIndex */
            snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);  /* lookupCtlOperationName */

            if (header_complex_parse_oid
                (&
                 (name
                  [sizeof(lookupCtlTable_variables_oid) / sizeof(oid) +
                   2]), newlen, vars) != SNMPERR_SUCCESS) {
                /*
                 * XXX: free, zero vars 
                 */
                snmp_free_varbind(vars);
                vars = NULL;
                return SNMP_ERR_INCONSISTENTNAME;
            }
            vp = vars;
            StorageNew = create_lookupTable_data();
            StorageNew->lookupCtlOwnerIndex = malloc(vp->val_len + 1);
            memcpy(StorageNew->lookupCtlOwnerIndex, vp->val.string,
                   vp->val_len);
            StorageNew->lookupCtlOwnerIndex[vp->val_len] = '\0';
            StorageNew->lookupCtlOwnerIndexLen = vp->val_len;
            vp = vp->next_variable;

            StorageNew->lookupCtlOperationName = malloc(vp->val_len + 1);
            memcpy(StorageNew->lookupCtlOperationName, vp->val.string,
                   vp->val_len);
            StorageNew->lookupCtlOperationName[vp->val_len] = '\0';
            StorageNew->lookupCtlOperationNameLen = vp->val_len;
            vp = vp->next_variable;

            /*
             * XXX: fill in default row values here into StorageNew 
             */

            StorageNew->lookupCtlTargetAddressType = INETADDRESSTYPE_IPV4;

            StorageNew->lookupCtlRowStatus = set_value;

            snmp_free_varbind(vars);
            vars = NULL;

            /*
             * XXX: free, zero vars, no longer needed? 
             */
        }
        break;

    case FREE:
        /*
         * XXX: free, zero vars 
         */
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
            /*
             * row creation, so add it 
             */
            if (set_value == RS_DESTROY) {
                return SNMP_ERR_NOERROR;
            }
            if (StorageNew != NULL)
                DEBUGMSGTL(("lookupCtlTable",
                            "write_lookupCtlRowStatus entering new=%d...  \n",
                            action));
            lookupCtlTable_add(StorageNew);
            /*
             * XXX: ack, and if it is NULL? 
             */
        } else if (set_value != RS_DESTROY) {
            /*
             * set the flag? 
             */
            old_value = StorageTmp->lookupCtlRowStatus;
            StorageTmp->lookupCtlRowStatus = *((long *) var_val);
        } else {
            /*
             * destroy...  extract it for now 
             */
            DEBUGMSGTL(("lookupCtlTable",
                        "write_lookupCtlTable_delete 1 \n"));
            hciptr =
                header_complex_find_entry(lookupCtlTableStorage,
                                          StorageTmp);

            StorageDel =
                header_complex_extract_entry(&lookupCtlTableStorage,
                                             hciptr);
            lookupResultsTable_del(StorageTmp);

            DEBUGMSGTL(("lookupCtlTable",
                        "write_lookupCtlTable_delete  \n"));

        }
        break;

    case UNDO:
        /*
         * Back out any changes made in the ACTION case 
         */
        if (StorageTmp == NULL) {
            /*
             * row creation, so remove it again 
             */
            hciptr =
                header_complex_find_entry(lookupCtlTableStorage,
                                          StorageTmp);
            StorageDel =
                header_complex_extract_entry(&lookupCtlTableStorage,
                                             hciptr);

            lookupResultsTable_del(StorageTmp);

            /*
             * XXX: free it 
             */
        } else if (StorageDel != NULL) {
            /*
             * row deletion, so add it again 
             */
            lookupCtlTable_add(StorageDel);
            lookupResultsTable_add(StorageDel);
        } else {
            StorageTmp->lookupCtlRowStatus = old_value;
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
            SNMP_FREE(StorageDel->lookupCtlOwnerIndex);
            SNMP_FREE(StorageDel->lookupCtlOperationName);
            SNMP_FREE(StorageDel->lookupCtlTargetAddress);
            SNMP_FREE(StorageDel);
            /*
             * XXX: free it, its dead 
             */
        } else {
            if (StorageTmp
                && StorageTmp->lookupCtlRowStatus == RS_CREATEANDGO) {
                StorageTmp->lookupCtlRowStatus = RS_ACTIVE;
            } else if (StorageTmp &&
                       StorageTmp->lookupCtlRowStatus ==
                       RS_CREATEANDWAIT) {

                StorageTmp->lookupCtlRowStatus = RS_NOTINSERVICE;
            }
        }
        if (StorageTmp && StorageTmp->lookupCtlRowStatus == RS_ACTIVE) {
            DEBUGMSGTL(("lookupCtlTable",
                        "write_lookupCtlRowStatus entering runbefore=%ld...  \n",
                        StorageTmp->lookupCtlTargetAddressType));

            modify_lookupCtlOperStatus(StorageTmp, 2L);
            run_lookup((struct lookupTable_data *) StorageTmp);

        }

        /** set up to save persistent store */
        snmp_store_needed(NULL);

        break;
    }
    return SNMP_ERR_NOERROR;
}
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
