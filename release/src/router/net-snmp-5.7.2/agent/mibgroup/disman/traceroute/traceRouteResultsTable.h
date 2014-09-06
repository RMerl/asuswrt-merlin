/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:traceRouteResultsTable.h
 *File Description:The head file of traceRouteResultsTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

#ifndef TRACEROUTERESULTSTABLE_H
#define TRACEROUTERESULTSTABLE_H


config_require(header_complex);

/*
 * function declarations 
 */
void            init_traceRouteResultsTable(void);
FindVarMethod   var_traceRouteResultsTable;
void            parse_traceRouteResultsTable(const char *, char *);
SNMPCallback    store_traceRouteResultsTable;


/*
 * column number definitions for table traceRouteResultsTable 
 */
#define COLUMN_TRACEROUTERESULTSOPERSTATUS		1
#define COLUMN_TRACEROUTERESULTSCURHOPCOUNT		2
#define COLUMN_TRACEROUTERESULTSCURPROBECOUNT		3
#define COLUMN_TRACEROUTERESULTSIPTGTADDRTYPE		4
#define COLUMN_TRACEROUTERESULTSIPTGTADDR		5
#define COLUMN_TRACEROUTERESULTSTESTATTEMPTS		6
#define COLUMN_TRACEROUTERESULTSTESTSUCCESSES		7
#define COLUMN_TRACEROUTERESULTSLASTGOODPATH		8
#endif                          /* TRACEROUTERESULTSTABLE_H */
