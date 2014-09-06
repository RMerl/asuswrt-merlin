/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:lookupCtlTable.h
 *File Description:The head file of lookupCtlTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

#ifndef LOOKUPCTLTABLE_H
#define LOOKUPCTLTABLE_H

/*
 * we may use header_complex from the header_complex module 
 */


config_require(header_complex);

    /*
     * our storage structure(s) 
     */

struct lookupTable_data {
    char           *lookupCtlOwnerIndex;        /* string */
    size_t          lookupCtlOwnerIndexLen;
    char           *lookupCtlOperationName;     /* string */
    size_t          lookupCtlOperationNameLen;
    long            lookupCtlTargetAddressType; /* integer32 */
    char           *lookupCtlTargetAddress;     /* string */
    size_t          lookupCtlTargetAddressLen;
    long            lookupCtlOperStatus;        /* integer */
    unsigned long   lookupCtlTime;      /* unsigned integer */
    long            lookupCtlRc;        /* integer32 */
    long            lookupCtlRowStatus; /* integer */
    int             storagetype;

    struct lookupResultsTable_data *ResultsTable;
};


struct lookupResultsTable_data {
    struct lookupResultsTable_data *next;
    char           *lookupCtlOwnerIndex;        /* string */
    size_t          lookupCtlOwnerIndexLen;
    char           *lookupCtlOperationName;     /* string */
    size_t          lookupCtlOperationNameLen;
    unsigned long   lookupResultsIndex;
    long            lookupResultsAddressType;
    char           *lookupResultsAddress;
    size_t          lookupResultsAddressLen;
    int             storagetype;
};

/*
 * function declarations 
 */
void            init_lookupCtlTable(void);
FindVarMethod   var_lookupCtlTable;
void            parse_lookupCtlTable(const char *, char *);
SNMPCallback    store_lookupCtlTable;

#ifndef NETSNMP_NO_WRITE_SUPPORT 
WriteMethod     write_lookupCtlTargetAddressType;
WriteMethod     write_lookupCtlTargetAddress;
WriteMethod     write_lookupCtlRowStatus;

WriteMethod     write_lookupCtlRowStatus;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

/*
 * column number definitions for table lookupCtlTable 
 */
#define COLUMN_LOOKUPCTLOWNERINDEX		1
#define COLUMN_LOOKUPCTLOPERATIONNAME		2
#define COLUMN_LOOKUPCTLTARGETADDRESSTYPE	3
#define COLUMN_LOOKUPCTLTARGETADDRESS		4
#define COLUMN_LOOKUPCTLOPERSTATUS		5
#define COLUMN_LOOKUPCTLTIME			6
#define COLUMN_LOOKUPCTLRC			7
#define COLUMN_LOOKUPCTLROWSTATUS		8

#endif                          /* LOOKUPMIB_H */
