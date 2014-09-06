#ifndef NETSNMP_EXTEND_H
#define NETSNMP_EXTEND_H

config_require( util_funcs/header_simple_table )
config_require( utilities/execute )
config_add_mib(NET-SNMP-EXTEND-MIB)

typedef struct netsnmp_extend_s {
    char    *token;
    char    *command;
    char    *args;
    char    *input;
    char    *old_command;
    char    *old_args;
    char    *old_input;

    int      out_len;
    char    *output;
    int      numlines;
    char   **lines;
    int      result;

    int      flags;
    netsnmp_cache     *cache;
    netsnmp_table_row *row;
    netsnmp_table_data *dinfo;
    struct netsnmp_extend_s *next;
} netsnmp_extend;

void                 init_extend(void);
void                 shutdown_extend(void);
Netsnmp_Node_Handler handle_nsExtendConfigTable;
Netsnmp_Node_Handler handle_nsExtendOutput1Table;
Netsnmp_Node_Handler handle_nsExtendOutput2Table;
void                 extend_parse_config(const char*, char*);

#define COLUMN_EXTCFG_COMMAND	2
#define COLUMN_EXTCFG_ARGS	3
#define COLUMN_EXTCFG_INPUT	4
#define COLUMN_EXTCFG_CACHETIME	5
#define COLUMN_EXTCFG_EXECTYPE	6
#define COLUMN_EXTCFG_RUNTYPE	7
#define COLUMN_EXTCFG_STORAGE	20
#define COLUMN_EXTCFG_STATUS	21
#define COLUMN_EXTCFG_FIRST_COLUMN	COLUMN_EXTCFG_COMMAND
#define COLUMN_EXTCFG_LAST_COLUMN	COLUMN_EXTCFG_STATUS

#define COLUMN_EXTOUT1_OUTLEN	0	/* DROPPED */
#define COLUMN_EXTOUT1_OUTPUT1	1	/* First Line */
#define COLUMN_EXTOUT1_OUTPUT2	2	/* Full Output */
#define COLUMN_EXTOUT1_NUMLINES	3
#define COLUMN_EXTOUT1_RESULT	4
#define COLUMN_EXTOUT1_FIRST_COLUMN	COLUMN_EXTOUT1_OUTPUT1
#define COLUMN_EXTOUT1_LAST_COLUMN	COLUMN_EXTOUT1_RESULT

#define COLUMN_EXTOUT2_OUTLINE	2
#define COLUMN_EXTOUT2_FIRST_COLUMN	COLUMN_EXTOUT2_OUTLINE
#define COLUMN_EXTOUT2_LAST_COLUMN	COLUMN_EXTOUT2_OUTLINE

#define NS_EXTEND_FLAGS_ACTIVE      0x01
#define NS_EXTEND_FLAGS_SHELL       0x02
#define NS_EXTEND_FLAGS_WRITEABLE   0x04
#define NS_EXTEND_FLAGS_CONFIG      0x08

#define NS_EXTEND_ETYPE_EXEC    1
#define NS_EXTEND_ETYPE_SHELL   2
#define NS_EXTEND_RTYPE_RONLY   1
#define NS_EXTEND_RTYPE_RWRITE  2

#endif /* NETSNMP_EXTEND_H */
