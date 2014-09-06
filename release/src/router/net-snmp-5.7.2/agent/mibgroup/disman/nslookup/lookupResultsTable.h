/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:lookupResultsTable.h
 *File Description:The head file of lookupResultsTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */


#ifndef LOOKUPRESULTSTABLE_H
#define LOOKUPRESULTSTABLE_H

/*
 * we may use header_complex from the header_complex module 
 */


config_require(header_complex);

/*
 * function declarations 
 */
void            init_lookupResultsTable(void);
FindVarMethod   var_lookupResultsTable;
void            parse_lookupResultsTable(const char *, char *);
SNMPCallback    store_lookupResultsTable;

/*
 * column number definitions for table lookupResultsTable 
 */
#define COLUMN_LOOKUPRESULTSINDEX		1
#define COLUMN_LOOKUPRESULTSADDRESSTYPE		2
#define COLUMN_LOOKUPRESULTSADDRESS		3
#endif                          /* LOOKUPMIB_H */
