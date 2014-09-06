/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:pingResultsTable.h
 *File Description:The head file of pingResultsTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */


#ifndef PINGRESULTSTABLE_H
#define PINGRESULTSTABLE_H

config_require(header_complex);

/*
 * function declarations 
 */
void            init_pingResultsTable(void);
FindVarMethod   var_pingResultsTable;
void            parse_pingResultsTable(const char *, char *);
SNMPCallback    store_pingResultsTable;


/*
 * column number definitions for table pingResultsTable 
 */
#define COLUMN_PINGRESULTSOPERSTATUS		1
#define COLUMN_PINGRESULTSIPTARGETADDRESSTYPE		2
#define COLUMN_PINGRESULTSIPTARGETADDRESS		3
#define COLUMN_PINGRESULTSMINRTT		4
#define COLUMN_PINGRESULTSMAXRTT		5
#define COLUMN_PINGRESULTSAVERAGERTT		6
#define COLUMN_PINGRESULTSPROBERESPONSES		7
#define COLUMN_PINGRESULTSSENTPROBES		8
#define COLUMN_PINGRESULTSRTTSUMOFSQUARES		9
#define COLUMN_PINGRESULTSLASTGOODPROBE		10
#endif                          /* PINGRESULTSTABLE_H */
