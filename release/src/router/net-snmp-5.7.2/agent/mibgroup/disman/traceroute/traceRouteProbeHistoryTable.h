/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:traceRouteProbeHistoryTable.h
 *File Description:The head file of traceRouteProbeHistoryTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

#ifndef TRACEROUTEPROBEHISTORYTABLE_H
#define TRACEROUTEPROBEHISTORYTABLE_H

config_require(header_complex);

/*
 * function declarations 
 */
void            init_traceRouteProbeHistoryTable(void);
FindVarMethod   var_traceRouteProbeHistoryTable;
void            parse_traceRouteProbeHistoryTable(const char *, char *);
SNMPCallback    store_traceRouteProbeHistoryTable;

/*
 * column number definitions for table traceRouteProbeHistoryTable 
 */
#define COLUMN_TRACEROUTEPROBEHISTORYINDEX		1
#define COLUMN_TRACEROUTEPROBEHISTORYHOPINDEX		2
#define COLUMN_TRACEROUTEPROBEHISTORYPROBEINDEX		3
#define COLUMN_TRACEROUTEPROBEHISTORYHADDRTYPE		4
#define COLUMN_TRACEROUTEPROBEHISTORYHADDR		5
#define COLUMN_TRACEROUTEPROBEHISTORYRESPONSE		6
#define COLUMN_TRACEROUTEPROBEHISTORYSTATUS		7
#define COLUMN_TRACEROUTEPROBEHISTORYLASTRC		8
#define COLUMN_TRACEROUTEPROBEHISTORYTIME		9
#endif                          /* TRACEROUTEPROBEHISTORYTABLE_H */
