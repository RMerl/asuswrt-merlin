/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:traceRouteHopsTable.h
 *File Description:The head file of traceRouteHopsTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */


#ifndef TRACEROUTEHOPSTABLE_H
#define TRACEROUTEHOPSTABLE_H

config_require(header_complex);

/*
 * function declarations 
 */
void            init_traceRouteHopsTable(void);
FindVarMethod   var_traceRouteHopsTable;
void            parse_traceRouteHopsTable(const char *, char *);
SNMPCallback    store_traceRouteHopsTable;

/*
 * column number definitions for table traceRouteHopsTable 
 */
#define COLUMN_TRACEROUTEHOPSHOPINDEX		1
#define COLUMN_TRACEROUTEHOPSIPTGTADDRESSTYPE		2
#define COLUMN_TRACEROUTEHOPSIPTGTADDRESS		3
#define COLUMN_TRACEROUTEHOPSMINRTT		4
#define COLUMN_TRACEROUTEHOPSMAXRTT		5
#define COLUMN_TRACEROUTEHOPSAVERAGERTT		6
#define COLUMN_TRACEROUTEHOPSRTTSUMOFSQUARES		7
#define COLUMN_TRACEROUTEHOPSSENTPROBES		8
#define COLUMN_TRACEROUTEHOPSPROBERESPONSES		9
#define COLUMN_TRACEROUTEHOPSLASTGOODPROBE		10
#endif                          /* TRACEROUTEHOPSTABLE_H */
