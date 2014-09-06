#ifndef SCHEDTABLE_H
#define SCHEDTABLE_H

config_require(disman/schedule/schedCore)
config_add_mib(DISMAN-SCHEDULE-MIB)

/*
 * function declarations 
 */
void            init_schedTable(void);
void            shutdown_schedTable(void);
Netsnmp_Node_Handler schedTable_handler;

/*
 * column number definitions for table schedTable 
 */
#define COLUMN_SCHEDOWNER		1
#define COLUMN_SCHEDNAME		2
#define COLUMN_SCHEDDESCR		3
#define COLUMN_SCHEDINTERVAL		4
#define COLUMN_SCHEDWEEKDAY		5
#define COLUMN_SCHEDMONTH		6
#define COLUMN_SCHEDDAY			7
#define COLUMN_SCHEDHOUR		8
#define COLUMN_SCHEDMINUTE		9
#define COLUMN_SCHEDCONTEXTNAME		10
#define COLUMN_SCHEDVARIABLE		11
#define COLUMN_SCHEDVALUE		12
#define COLUMN_SCHEDTYPE		13
#define COLUMN_SCHEDADMINSTATUS		14
#define COLUMN_SCHEDOPERSTATUS		15
#define COLUMN_SCHEDFAILURES		16
#define COLUMN_SCHEDLASTFAILURE		17
#define COLUMN_SCHEDLASTFAILED		18
#define COLUMN_SCHEDSTORAGETYPE		19
#define COLUMN_SCHEDROWSTATUS		20
#define COLUMN_SCHEDTRIGGERS		21

#endif                          /* SCHEDTABLE_H */
