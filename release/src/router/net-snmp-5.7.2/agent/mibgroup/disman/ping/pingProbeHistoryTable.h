/*
 *Copyright(c)2004,Cisco URP imburses and Network Information Center in Beijing University of Posts and Telecommunications researches.
 *
 *All right reserved
 *
 *File Name:pingProbeHistoryTable.h
 *File Description:The head file of pingProbeHistoryTable.c
 *
 *Current Version:1.0
 *Author:ChenJing
 *Date:2004.8.20
 */

#ifndef PINGPROBEHISTORYTABLE_H
#define PINGPROBEHISTORYTABLE_H

config_require(header_complex);

/*
 * function declarations 
 */
void            init_pingProbeHistoryTable(void);
FindVarMethod   var_pingProbeHistoryTable;
void            parse_pingProbeHistoryTable(const char *, char *);
SNMPCallback    store_pingProbeHistoryTable;


/*
 * column number definitions for table pingProbeHistoryTable 
 */
#define COLUMN_PINGPROBEHISTORYINDEX		1
#define COLUMN_PINGPROBEHISTORYRESPONSE		2
#define COLUMN_PINGPROBEHISTORYSTATUS		3
#define COLUMN_PINGPROBEHISTORYLASTRC		4
#define COLUMN_PINGPROBEHISTORYTIME		5
#endif                          /* PINGPROBEHISTORYTABLE_H */
