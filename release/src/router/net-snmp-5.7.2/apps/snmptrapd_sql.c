/*
 * File       : snmptrapd_sql
 * Author     : Robert Story
 *
 * Copyright © 2009 Science Logic, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 *
 * This file implements a handler for snmptrapd which will cache incoming
 * traps and then write them to a MySQL database.
 *
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#ifdef NETSNMP_USE_MYSQL

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "snmptrapd_handlers.h"
#include "snmptrapd_auth.h"
#include "snmptrapd_log.h"

/*
 * SQL includes
 */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#include <mysql/my_global.h>
#include <mysql/my_sys.h>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>

netsnmp_feature_require(container_fifo)

/*
 * define a structure to hold all the file globals
 */
typedef struct netsnmp_sql_globals_t {
    char        *host_name;       /* server host (def=localhost) */
    char        *user_name;       /* username (def=login name) */
    char        *password;        /* password (def=none) */
    u_int        port_num;        /* port number (built-in value) */
    char        *socket_name;     /* socket name (built-in value) */
    const char  *db_name;         /* database name (def=none) */
    u_int        flags;           /* connection flags (none) */
    MYSQL       *conn;            /* connection */
    u_char       connected;       /* connected flag */
    const char  *groups[3];
    MYSQL_STMT  *trap_stmt, *vb_stmt; /* prepared statements */
    u_int        alarm_id;        /* id of periodic save alarm */
    netsnmp_container *queue;     /* container; traps pending database write */
    u_int        queue_max;       /* auto save queue when it gets this big */
    int          queue_interval;  /* auto save every N seconds */
} netsnmp_sql_globals;

static netsnmp_sql_globals _sql = {
    NULL,                  /* host */
    NULL,                  /* username */
    NULL,                  /* password */
    0,                     /* port */
    NULL,                  /* socket */
    "net_snmp",            /* database */
    0,                     /* conn flags */
    NULL,                  /* connection */
    0,                     /* connected */
    { "client", "snmptrapd", NULL },  /* groups to read from .my.cnf */
    NULL,                  /* trap_stmt */
    NULL,                  /* vb_stmt */
    0,                     /* alarm_id */
    NULL,                  /* queue */
    1,                     /* queue_max */
    -1                     /* queue_interval */
};

/*
 * log traps as text, or binary blobs?
 */
#define NETSNMP_MYSQL_TRAP_VALUE_TEXT 1

/*
 * We will be using prepared statements for performance reasons. This
 * requires a sql bind structure for each cell to be inserted in the
 * database. We will be using 2 global static structures to bind to,
 * and a netsnmp container to store the necessary data until it is
 * written to the database. Fixed size buffers are also used to
 * simplify memory management.
 */
/** enums for the trap fields to be bound */
enum{
    TBIND_DATE = 0,           /* time received */
    TBIND_HOST,               /* src ip */
    TBIND_USER,               /* auth/user information */
    TBIND_TYPE,               /* pdu type */
    TBIND_VER,                /* snmp version */
    TBIND_REQID,              /* request id */
    TBIND_OID,                /* trap OID */
    TBIND_TRANSPORT,          /* transport */
    TBIND_SECURITY_MODEL,     /* security model */
    TBIND_v3_MSGID,           /* v3 msg id */
    TBIND_v3_SECURITY_LEVEL,  /* security level */
    TBIND_v3_CONTEXT_NAME,    /* context */
    TBIND_v3_CONTEXT_ENGINE,  /* context engine id */
    TBIND_v3_SECURITY_NAME,   /* security name */
    TBIND_v3_SECURITY_ENGINE, /* security engine id */
    TBIND_MAX
};

/** enums for the varbind fields to be bound */
enum {
    VBIND_ID = 0,             /* trap_id */
    VBIND_OID,                /* varbind oid */
    VBIND_TYPE,               /* varbind type */
    VBIND_VAL,                /* varbind value */
    VBIND_MAX
};

/** buffer struct for varbind data */
typedef struct sql_vb_buf_t {

    char      *oid;
    u_long     oid_len;

    u_char    *val;
    u_long     val_len;

    uint16_t   type;

} sql_vb_buf;

/** buffer struct for trap data */
typedef struct sql_buf_t {
    char      *host;
    u_long     host_len;

    char      *oid;
    u_long     oid_len;

    char      *user;
    u_long     user_len;

    MYSQL_TIME time;
    uint16_t   version, type;
    uint32_t   reqid;

    char      *transport;
    u_long     transport_len;

    uint16_t   security_level, security_model;
    uint32_t   msgid;

    char      *context;
    u_long     context_len;

    char      *context_engine;
    u_long     context_engine_len;

    char      *security_name;
    u_long     security_name_len;

    char      *security_engine;
    u_long     security_engine_len;

    netsnmp_container *varbinds;

    char       logged;
} sql_buf;

/*
 * static bind structures, plus 2 static buffers to bind to.
 */
static MYSQL_BIND _tbind[TBIND_MAX], _vbind[VBIND_MAX];
static my_bool    _no_v3;

static void _sql_process_queue(u_int dontcare, void *meeither);

/*
 * parse the sqlMaxQueue configuration token
 */
static void
_parse_queue_fmt(const char *token, char *cptr)
{
    _sql.queue_max = atoi(cptr);
    DEBUGMSGTL(("sql:queue","queue max now %d\n", _sql.queue_max));
}

/*
 * parse the sqlSaveInterval configuration token
 */
static void
_parse_interval_fmt(const char *token, char *cptr)
{
    _sql.queue_interval = atoi(cptr);
    DEBUGMSGTL(("sql:queue","queue interval now %d seconds\n",
                _sql.queue_interval));
}

/*
 * register sql related configuration tokens
 */
void
snmptrapd_register_sql_configs( void )
{
    register_config_handler("snmptrapd", "sqlMaxQueue",
                            _parse_queue_fmt, NULL, "integer");
    register_config_handler("snmptrapd", "sqlSaveInterval",
                            _parse_interval_fmt, NULL, "seconds");
}

static void
netsnmp_sql_disconnected(void)
{
    DEBUGMSGTL(("sql:connection","disconnected\n"));

    _sql.connected = 0;

    /** release prepared statements */
    if (_sql.trap_stmt) {
        mysql_stmt_close(_sql.trap_stmt);
        _sql.trap_stmt = NULL;
    }
    if (_sql.vb_stmt) {
        mysql_stmt_close(_sql.vb_stmt);
        _sql.vb_stmt = NULL;
    }
}

/*
 * convenience function to log mysql errors
 */
static void
netsnmp_sql_error(const char *message)
{
    u_int err = mysql_errno(_sql.conn);
    snmp_log(LOG_ERR, "%s\n", message);
    if (_sql.conn != NULL) {
#if MYSQL_VERSION_ID >= 40101
        snmp_log(LOG_ERR, "Error %u (%s): %s\n",
                 err, mysql_sqlstate(_sql.conn), mysql_error(_sql.conn));
#else
        snmp(LOG_ERR, "Error %u: %s\n",
             mysql_errno(_sql.conn), mysql_error(_sql.conn));
#endif
    }
    if (CR_SERVER_GONE_ERROR == err)
        netsnmp_sql_disconnected();
}

/*
 * convenience function to log mysql statement errors
 */
static void
netsnmp_sql_stmt_error (MYSQL_STMT *stmt, const char *message)
{
    u_int err = mysql_errno(_sql.conn);

    snmp_log(LOG_ERR, "%s\n", message);
    if (stmt) {
        snmp_log(LOG_ERR, "SQL Error %u (%s): %s\n",
                 mysql_stmt_errno(stmt), mysql_stmt_sqlstate(stmt),
                 mysql_stmt_error(stmt));
    }
    
    if (CR_SERVER_GONE_ERROR == err)
        netsnmp_sql_disconnected();
}

/*
 * sql cleanup function, called at exit
 */
static void
netsnmp_mysql_cleanup(void)
{
    DEBUGMSGTL(("sql:cleanup"," called\n"));

    /** unregister alarm */
    if (_sql.alarm_id)
        snmp_alarm_unregister(_sql.alarm_id);

    /** save any queued traps */
    if (CONTAINER_SIZE(_sql.queue))
        _sql_process_queue(0,NULL);

    CONTAINER_FREE(_sql.queue);
    _sql.queue = NULL;

    if (_sql.trap_stmt) {
        mysql_stmt_close(_sql.trap_stmt);
        _sql.trap_stmt = NULL;
    }
    if (_sql.vb_stmt) {
        mysql_stmt_close(_sql.vb_stmt);
        _sql.vb_stmt = NULL;
    }
    
    /** disconnect from server */
    netsnmp_sql_disconnected();

    if (_sql.conn) {
        mysql_close(_sql.conn);
        _sql.conn = NULL;
    }

    mysql_library_end();
}

/*
 * setup (initialize, prepare and bind) a prepared statement
 */
static int
netsnmp_mysql_bind(const char *text, size_t text_size, MYSQL_STMT **stmt,
                   MYSQL_BIND *bind)
{
    if ((NULL == text) || (NULL == stmt) || (NULL == bind)) {
        snmp_log(LOG_ERR,"invalid paramaters to netsnmp_mysql_bind()\n");
        return -1;
    }

    *stmt = mysql_stmt_init(_sql.conn);
    if (NULL == *stmt) {
        netsnmp_sql_error("could not initialize trap statement handler");
        return -1;
    }

    if (mysql_stmt_prepare(*stmt, text, text_size) != 0) {
        netsnmp_sql_stmt_error(*stmt, "Could not prepare INSERT");
        mysql_stmt_close(*stmt);
        *stmt = NULL;
        return -1;
    }

    return 0;
}

/*
 * connect to the database and do initial setup
 */
static int
netsnmp_mysql_connect(void)
{
    char trap_stmt[] = "INSERT INTO notifications "
        "(date_time, host, auth, type, version, request_id, snmpTrapOID, transport, security_model, v3msgid, v3security_level, v3context_name, v3context_engine, v3security_name, v3security_engine) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    char vb_stmt[] = "INSERT INTO varbinds "
        "(trap_id, oid, type, value) VALUES (?,?,?,?)";

    /** initialize connection handler */
    if (_sql.connected)
        return 0;

    DEBUGMSGTL(("sql:connection","connecting\n"));

    /** connect to server */
    if (mysql_real_connect (_sql.conn, _sql.host_name, _sql.user_name,
                            _sql.password, _sql.db_name, _sql.port_num,
                            _sql.socket_name, _sql.flags) == NULL) {
        netsnmp_sql_error("mysql_real_connect() failed");
        goto err;
    }
    _sql.connected = 1;

    /** disable autocommit */
    if(0 != mysql_autocommit(_sql.conn, 0)) {
        netsnmp_sql_error("mysql_autocommit(0) failed");
        goto err;
    }

    netsnmp_assert((_sql.trap_stmt == NULL) && (_sql.vb_stmt == NULL));

    /** prepared statement for inserts */
    if (0 != netsnmp_mysql_bind(trap_stmt,sizeof(trap_stmt), &_sql.trap_stmt,
                                _tbind))
        goto err;

    if (0 != netsnmp_mysql_bind(vb_stmt,sizeof(vb_stmt),&_sql.vb_stmt,
                                _vbind)) {
        mysql_stmt_close(_sql.trap_stmt);
        _sql.trap_stmt = NULL;
        goto err;
    }

    return 0;

  err:
    if (_sql.connected)
        _sql.connected = 0;

    return -1;
}

/** one-time initialization for mysql */
int
netsnmp_mysql_init(void)
{
    int not_argc = 0, i;
    char *not_args[] = { NULL };
    char **not_argv = not_args;
    netsnmp_trapd_handler *traph;

    DEBUGMSGTL(("sql:init","called\n"));

    /** negative or 0 interval disables sql logging */
    if (_sql.queue_interval <= 0) {
        DEBUGMSGTL(("sql:init",
                    "mysql not enabled (sqlSaveInterval is <= 0)\n"));
        return 0;
    }

    /** create queue for storing traps til they are written to the db */
    _sql.queue = netsnmp_container_find("fifo");
    if (NULL == _sql.queue) {
        snmp_log(LOG_ERR, "Could not allocate sql buf container\n");
        return -1;
    }

#ifdef HAVE_BROKEN_LIBMYSQLCLIENT
    my_init();
#else
    MY_INIT("snmptrapd");
#endif

    /** load .my.cnf values */
    load_defaults ("my", _sql.groups, &not_argc, &not_argv);
    for(i=0; i < not_argc; ++i) {
        if (NULL == not_argv[i])
            continue;
        if (strncmp(not_argv[i],"--password=",11) == 0)
            _sql.password = &not_argv[i][11];
        else if (strncmp(not_argv[i],"--host=",7) == 0)
            _sql.host_name = &not_argv[i][7];
        else if (strncmp(not_argv[i],"--user=",7) == 0)
            _sql.user_name = &not_argv[i][7];
        else if (strncmp(not_argv[i],"--port=",7) == 0)
            _sql.port_num = atoi(&not_argv[i][7]);
        else if (strncmp(not_argv[i],"--socket=",9) == 0)
            _sql.socket_name = &not_argv[i][9];
        else
            snmp_log(LOG_WARNING, "unknown argument[%d] %s\n", i, not_argv[i]);
    }

    /** init bind structures */
    memset(_tbind, 0x0, sizeof(_tbind));
    memset(_vbind, 0x0, sizeof(_vbind));

    /** trap static bindings */
    _tbind[TBIND_HOST].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_HOST].length = &_tbind[TBIND_HOST].buffer_length;

    _tbind[TBIND_OID].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_OID].length = &_tbind[TBIND_OID].buffer_length;

    _tbind[TBIND_REQID].buffer_type = MYSQL_TYPE_LONG;
    _tbind[TBIND_REQID].is_unsigned = 1;

    _tbind[TBIND_VER].buffer_type = MYSQL_TYPE_SHORT;
    _tbind[TBIND_VER].is_unsigned = 1;

    _tbind[TBIND_TYPE].buffer_type = MYSQL_TYPE_SHORT;
    _tbind[TBIND_TYPE].is_unsigned = 1;

    _tbind[TBIND_DATE].buffer_type = MYSQL_TYPE_DATETIME;

    _tbind[TBIND_USER].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_USER].length = &_tbind[TBIND_USER].buffer_length;

    _tbind[TBIND_TRANSPORT].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_TRANSPORT].length = &_tbind[TBIND_TRANSPORT].buffer_length;

    _tbind[TBIND_SECURITY_MODEL].buffer_type = MYSQL_TYPE_SHORT;
    _tbind[TBIND_SECURITY_MODEL].is_unsigned = 1;

    _tbind[TBIND_v3_MSGID].buffer_type = MYSQL_TYPE_LONG;
    _tbind[TBIND_v3_MSGID].is_unsigned = 1;
    _tbind[TBIND_v3_SECURITY_LEVEL].buffer_type = MYSQL_TYPE_SHORT;
    _tbind[TBIND_v3_SECURITY_LEVEL].is_unsigned = 1;
    _tbind[TBIND_v3_CONTEXT_NAME].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_v3_CONTEXT_ENGINE].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_v3_SECURITY_NAME].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_v3_SECURITY_NAME].length =
        &_tbind[TBIND_v3_SECURITY_NAME].buffer_length;
    _tbind[TBIND_v3_CONTEXT_NAME].length =
        &_tbind[TBIND_v3_CONTEXT_NAME].buffer_length;
    _tbind[TBIND_v3_SECURITY_ENGINE].buffer_type = MYSQL_TYPE_STRING;
    _tbind[TBIND_v3_SECURITY_ENGINE].length =
        &_tbind[TBIND_v3_SECURITY_ENGINE].buffer_length;
    _tbind[TBIND_v3_CONTEXT_ENGINE].length =
        &_tbind[TBIND_v3_CONTEXT_ENGINE].buffer_length;

    _tbind[TBIND_v3_MSGID].is_null =
        _tbind[TBIND_v3_SECURITY_LEVEL].is_null =
        _tbind[TBIND_v3_CONTEXT_NAME].is_null =
        _tbind[TBIND_v3_CONTEXT_ENGINE].is_null =
        _tbind[TBIND_v3_SECURITY_NAME].is_null =
        _tbind[TBIND_v3_SECURITY_ENGINE].is_null = &_no_v3;
    
    /** variable static bindings */
    _vbind[VBIND_ID].buffer_type = MYSQL_TYPE_LONG;
    _vbind[VBIND_ID].is_unsigned = 1;

    _vbind[VBIND_OID].buffer_type = MYSQL_TYPE_STRING;
    _vbind[VBIND_OID].length = &_vbind[VBIND_OID].buffer_length;

    _vbind[VBIND_TYPE].buffer_type = MYSQL_TYPE_SHORT;
    _vbind[VBIND_TYPE].is_unsigned = 1;

#ifdef NETSNMP_MYSQL_TRAP_VALUE_TEXT
    _vbind[VBIND_VAL].buffer_type = MYSQL_TYPE_STRING;
#else
    _vbind[VBIND_VAL].buffer_type = MYSQL_TYPE_BLOB;
#endif
    _vbind[VBIND_VAL].length = &_vbind[VBIND_VAL].buffer_length;

    _sql.conn = mysql_init (NULL);
    if (_sql.conn == NULL) {
        netsnmp_sql_error("mysql_init() failed (out of memory?)");
        return -1;
    }

    /** try to connect; we'll try again later if we fail */
    (void) netsnmp_mysql_connect();

    /** register periodic queue save */
    _sql.alarm_id = snmp_alarm_register(_sql.queue_interval, /* seconds */
                                        1,                   /* repeat */
                                        _sql_process_queue,  /* function */
                                        NULL);               /* client args */

    /** add handler */
    traph = netsnmp_add_global_traphandler(NETSNMPTRAPD_PRE_HANDLER,
                                           mysql_handler);
    if (NULL == traph) {
        snmp_log(LOG_ERR, "Could not allocate sql trap handler\n");
        return -1;
    }
    traph->authtypes = TRAP_AUTH_LOG;

    atexit(netsnmp_mysql_cleanup);
    return 0;
}

/*
 * log CSV version of trap.
 * dontcare param is there so this function can be passed directly
 * to CONTAINER_FOR_EACH.
 */
static void
_sql_log(sql_buf *sqlb, void* dontcare)
{
    netsnmp_iterator     *it;
    sql_vb_buf           *sqlvb;

    if ((NULL == sqlb) || sqlb->logged)
        return;

    /*
     * log trap info
     * nothing done to protect against data insertion attacks with
     * respect to bad data (commas, newlines, etc)
     */
    snmp_log(LOG_ERR,
             "trap:%d-%d-%d %d:%d:%d,%s,%d,%d,%d,%s,%s,%d,%d,%d,%s,%s,%s,%s\n",
             sqlb->time.year,sqlb->time.month,sqlb->time.day,
             sqlb->time.hour,sqlb->time.minute,sqlb->time.second,
             sqlb->user,
             sqlb->type, sqlb->version, sqlb->reqid, sqlb->oid,
             sqlb->transport, sqlb->security_model, sqlb->msgid,
             sqlb->security_level, sqlb->context,
             sqlb->context_engine, sqlb->security_name,
             sqlb->security_engine);

    sqlb->logged = 1; /* prevent multiple logging */

    it = CONTAINER_ITERATOR(sqlb->varbinds);
    if (NULL == it) {
        snmp_log(LOG_ERR,
                 "error creating iterator; incomplete trap logged\n");
        return;
    }

    /** log varbind info */
    for( sqlvb = ITERATOR_FIRST(it); sqlvb; sqlvb = ITERATOR_NEXT(it)) {
#ifdef NETSNMP_MYSQL_TRAP_VALUE_TEXT
        snmp_log(LOG_ERR,"varbind:%s,%s\n", sqlvb->oid, sqlvb->val);
#else
        char *hex;
        int len = binary_to_hex(sqlvb->val, sqlvb->val_len, &hex);
        if (hex) {
            snmp_log(LOG_ERR,"varbind:%d,%s,%s\n", sqlvb->oid, hex);
            free(hex);
        }
        else {
            snmp_log(LOG_ERR,"malloc failed for varbind hex value\n");
            snmp_log(LOG_ERR,"varbind:%s,\n", sqlvb->oid);
        }
#endif
    }
    ITERATOR_RELEASE(it);
   
}

/*
 * free a buffer
 * dontcare param is there so this function can be passed directly
 * to CONTAINER_FOR_EACH.
 */
static void
_sql_vb_buf_free(sql_vb_buf *sqlvb, void* dontcare)
{
    if (NULL == sqlvb)
        return;

    SNMP_FREE(sqlvb->oid);
    SNMP_FREE(sqlvb->val);

    free(sqlvb);
}

/*
 * free a buffer
 * dontcare param is there so this function can be passed directly
 * to CONTAINER_FOR_EACH.
 */
static void
_sql_buf_free(sql_buf *sqlb, void* dontcare)
{
    if (NULL == sqlb)
        return;

    /** do varbinds first */
    if (sqlb->varbinds) {
        CONTAINER_CLEAR(sqlb->varbinds,
                        (netsnmp_container_obj_func*)_sql_vb_buf_free, NULL);
        CONTAINER_FREE(sqlb->varbinds);
    }

    SNMP_FREE(sqlb->host);
    SNMP_FREE(sqlb->oid);
    SNMP_FREE(sqlb->user);

    SNMP_FREE(sqlb->context);
    SNMP_FREE(sqlb->security_name);
    SNMP_FREE(sqlb->context_engine);
    SNMP_FREE(sqlb->security_engine);
    SNMP_FREE(sqlb->transport);

    free(sqlb);
}

/*
 * allocate buffer to store trap and varbinds
 */
static sql_buf *
_sql_buf_get(void)
{
    sql_buf *sqlb;

    /** buffer for trap info */
    sqlb = SNMP_MALLOC_TYPEDEF(sql_buf);
    if (NULL == sqlb)
        return NULL;
    
    /** fifo for varbinds */
    sqlb->varbinds = netsnmp_container_find("fifo");
    if (NULL == sqlb->varbinds) {
        free(sqlb);
        return NULL;
    }

    return sqlb;
}

/*
 * save info from incoming trap
 *
 * return 0 on success, anything else is an error
 */
static int
_sql_save_trap_info(sql_buf *sqlb, netsnmp_pdu  *pdu,
                    netsnmp_transport     *transport)
{
    static oid   trapoids[] = { 1, 3, 6, 1, 6, 3, 1, 1, 5, 0 };
    oid         *trap_oid, tmp_oid[MAX_OID_LEN];
    time_t       now;
    struct tm   *cur_time;
    size_t       tmp_size;
    size_t       buf_host_len_t, buf_oid_len_t, buf_user_len_t;
    int          oid_overflow, trap_oid_len;
    netsnmp_variable_list *vars;

    if ((NULL == sqlb) || (NULL == pdu) || (NULL == transport))
        return -1;

    DEBUGMSGTL(("sql:queue", "queueing incoming trap\n"));

    /** time */
    (void) time(&now);
    cur_time = localtime(&now);
    sqlb->time.year = cur_time->tm_year + 1900;
    sqlb->time.month = cur_time->tm_mon + 1;
    sqlb->time.day = cur_time->tm_mday;
    sqlb->time.hour = cur_time->tm_hour;
    sqlb->time.minute = cur_time->tm_min;
    sqlb->time.second = cur_time->tm_sec;
    sqlb->time.second_part = 0;
    sqlb->time.neg = 0;

    /** host name */
    buf_host_len_t = 0;
    tmp_size = sizeof(sqlb->host);
    realloc_format_trap((u_char**)&sqlb->host, &tmp_size,
                        &buf_host_len_t, 1, "%B", pdu, transport);
    sqlb->host_len = buf_host_len_t;

    /* snmpTrapOID */
    if (pdu->command == SNMP_MSG_TRAP) {
        /*
         * convert a v1 trap to a v2 varbind
         */
        if (pdu->trap_type == SNMP_TRAP_ENTERPRISESPECIFIC) {
            trap_oid_len = pdu->enterprise_length;
            memcpy(tmp_oid, pdu->enterprise, sizeof(oid) * trap_oid_len);
            if (tmp_oid[trap_oid_len - 1] != 0)
                tmp_oid[trap_oid_len++] = 0;
            tmp_oid[trap_oid_len++] = pdu->specific_type;
            trap_oid = tmp_oid;
        } else {
            trapoids[9] = pdu->trap_type + 1;
            trap_oid = trapoids;
            trap_oid_len = OID_LENGTH(trapoids);
        }
    }
    else {
        vars = pdu->variables;
        if (vars && vars->next_variable) {
            trap_oid_len = vars->next_variable->val_len / sizeof(oid);
            trap_oid = vars->next_variable->val.objid;
        }
        else {
            static oid null_oid[] = { 0, 0 };
            trap_oid_len = OID_LENGTH(null_oid);
            trap_oid = null_oid;
        }
    }
    tmp_size = 0;
    buf_oid_len_t = oid_overflow = 0;
    netsnmp_sprint_realloc_objid_tree((u_char**)&sqlb->oid,&tmp_size,
                                      &buf_oid_len_t, 1, &oid_overflow,
                                      trap_oid, trap_oid_len);
    sqlb->oid_len = buf_oid_len_t;
    if (oid_overflow)
        snmp_log(LOG_WARNING,"OID truncated in sql buffer\n");

    /** request id */
    sqlb->reqid = pdu->reqid;

    /** version (convert to 1 based, for sql enum) */
    sqlb->version = pdu->version + 1;

    /** command type (convert to 1 based, for sql enum) */
    sqlb->type = pdu->command - 159;

    /** community string/user name */
    tmp_size = 0;
    buf_user_len_t = 0;
    realloc_format_trap((u_char**)&sqlb->user, &tmp_size,
                        &buf_user_len_t, 1, "%u", pdu, transport);
    sqlb->user_len = buf_user_len_t;

    /** transport */
    sqlb->transport = transport->f_fmtaddr(transport, pdu->transport_data,
                                           pdu->transport_data_length);

    /** security model */
    sqlb->security_model = pdu->securityModel;

    if ((SNMP_MP_MODEL_SNMPv3+1) == sqlb->version) {

        sqlb->msgid = pdu->msgid;
        sqlb->security_level = pdu->securityLevel;

        if (pdu->contextName) {
            sqlb->context = netsnmp_strdup_and_null((u_char*)pdu->contextName,
                                                    pdu->contextNameLen);
            sqlb->context_len = pdu->contextNameLen;
        }
        if (pdu->contextEngineID) {
            sqlb->context_engine_len = 
                binary_to_hex(pdu->contextEngineID, pdu->contextEngineIDLen,
                              &sqlb->context_engine);
        }

        if (pdu->securityName) {
            sqlb->security_name =
                netsnmp_strdup_and_null((u_char*)pdu->securityName,
                                        pdu->securityNameLen);
            sqlb->security_name_len = pdu->securityNameLen;
        }
        if (pdu->securityEngineID) {
            sqlb->security_engine_len = 
                binary_to_hex(pdu->securityEngineID, pdu->securityEngineIDLen,
                              &sqlb->security_engine);
        }
    }

    return 0;
}

/*
 * save varbind info from incoming trap
 *
 * return 0 on success, anything else is an error
 */
static int
_sql_save_varbind_info(sql_buf *sqlb, netsnmp_pdu  *pdu)
{
    netsnmp_variable_list *var;
    sql_vb_buf       *sqlvb;
    size_t            tmp_size, buf_oid_len_t;
    int               oid_overflow, rc;
#ifdef NETSNMP_MYSQL_TRAP_VALUE_TEXT
    size_t            buf_val_len_t;
#endif

    if ((NULL == sqlb) || (NULL == pdu))
        return -1;

    var = pdu->variables;
    while(var) {
        sqlvb = SNMP_MALLOC_TYPEDEF(sql_vb_buf);
        if (NULL == sqlvb)
            break;

        /** OID */
        tmp_size = 0;
        buf_oid_len_t = oid_overflow = 0;
        netsnmp_sprint_realloc_objid_tree((u_char**)&sqlvb->oid, &tmp_size,
                                          &buf_oid_len_t,
                                          1, &oid_overflow, var->name,
                                          var->name_length);
        sqlvb->oid_len = buf_oid_len_t;
        if (oid_overflow)
            snmp_log(LOG_WARNING,"OID truncated in sql insert\n");
        
        /** type */
        if (var->type > ASN_OBJECT_ID)
            /** convert application types to sql enum */
            sqlvb->type = ASN_OBJECT_ID + 1 + (var->type & ~ASN_APPLICATION);
        else
            sqlvb->type = var->type;

        /** value */
#ifdef NETSNMP_MYSQL_TRAP_VALUE_TEXT
        tmp_size = 0;
        buf_val_len_t = 0;
        sprint_realloc_by_type((u_char**)&sqlvb->val, &tmp_size,
                               &buf_val_len_t, 1, var, 0, 0, 0);
        sqlvb->val_len = buf_val_len_t;
#else
        memdup(&sqlvb->val, var->val.string, var->val_len);
        sqlvb->val_len = var->val_len;
#endif

        var = var->next_variable;

        /** insert into container */
        rc = CONTAINER_INSERT(sqlb->varbinds,sqlvb);
        if(rc)
            snmp_log(LOG_ERR, "couldn't insert varbind into trap container\n");
    }

    return 0;
}

/*
 * sql trap handler
 */
int
mysql_handler(netsnmp_pdu           *pdu,
              netsnmp_transport     *transport,
              netsnmp_trapd_handler *handler)
{
    sql_buf     *sqlb;
    int          old_format, rc;

    DEBUGMSGTL(("sql:handler", "called\n"));

    /** allocate a buffer to save data */
    sqlb = _sql_buf_get();
    if (NULL == sqlb) {
        snmp_log(LOG_ERR, "Could not allocate trap sql buffer\n");
        return syslog_handler( pdu, transport, handler );
    }

    /** save OID output format and change to numeric */
    old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                       NETSNMP_OID_OUTPUT_NUMERIC);


    rc = _sql_save_trap_info(sqlb, pdu, transport);
    rc = _sql_save_varbind_info(sqlb, pdu);

    /** restore previous OID output format */
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                       old_format);

    /** insert into queue */
    rc = CONTAINER_INSERT(_sql.queue, sqlb);
    if(rc) {
        snmp_log(LOG_ERR, "Could not log queue sql trap buffer\n");
        _sql_log(sqlb, NULL);
        _sql_buf_free(sqlb, 0);
        return -1;
    }

    /** save queue if size is > max */
    if (CONTAINER_SIZE(_sql.queue) >= _sql.queue_max)
        _sql_process_queue(0,NULL);

    return 0;
}

/*
 * save a buffered trap to sql database
 */
static void
_sql_save(sql_buf *sqlb, void *dontcare)
{
    netsnmp_iterator     *it;
    sql_vb_buf           *sqlvb;
    u_long                trap_id;

    /*
     * don't even try if we don't have a database connection
     */
    if (0 == _sql.connected) {
        _sql_log(sqlb, NULL);
        return;
    }

    /*
     * the prepared statements are bound to the static buffer objects,
     * so copy the queued data to the static version.
     */
    _tbind[TBIND_HOST].buffer = sqlb->host;
    _tbind[TBIND_HOST].buffer_length = sqlb->host_len;

    _tbind[TBIND_OID].buffer = sqlb->oid;
    _tbind[TBIND_OID].buffer_length = sqlb->oid_len;

    _tbind[TBIND_REQID].buffer = (void *)&sqlb->reqid;
    _tbind[TBIND_VER].buffer = (void *)&sqlb->version;
    _tbind[TBIND_TYPE].buffer = (void *)&sqlb->type;
    _tbind[TBIND_SECURITY_MODEL].buffer = (void *)&sqlb->security_model;

    _tbind[TBIND_DATE].buffer = (void *)&sqlb->time;

    _tbind[TBIND_USER].buffer = sqlb->user;
    _tbind[TBIND_USER].buffer_length = sqlb->user_len;

    _tbind[TBIND_TRANSPORT].buffer = sqlb->transport;
    if (sqlb->transport)
        _tbind[TBIND_TRANSPORT].buffer_length = strlen(sqlb->transport);
    else
        _tbind[TBIND_TRANSPORT].buffer_length = 0;


    if ((SNMP_MP_MODEL_SNMPv3+1) == sqlb->version) {
        _no_v3 = 0;

        _tbind[TBIND_v3_MSGID].buffer = &sqlb->msgid;
        
        _tbind[TBIND_v3_SECURITY_LEVEL].buffer = &sqlb->security_level;
        
        _tbind[TBIND_v3_CONTEXT_NAME].buffer = sqlb->context;
        _tbind[TBIND_v3_CONTEXT_NAME].buffer_length = sqlb->context_len;

        _tbind[TBIND_v3_CONTEXT_ENGINE].buffer = sqlb->context_engine;
        _tbind[TBIND_v3_CONTEXT_ENGINE].buffer_length =
            sqlb->context_engine_len;

        _tbind[TBIND_v3_SECURITY_NAME].buffer = sqlb->security_name;
        _tbind[TBIND_v3_SECURITY_NAME].buffer_length = sqlb->security_name_len;

        _tbind[TBIND_v3_SECURITY_ENGINE].buffer = sqlb->security_engine;
        _tbind[TBIND_v3_SECURITY_ENGINE].buffer_length =
            sqlb->security_engine_len;
    }
    else {
        _no_v3 = 1;
    }

    if (mysql_stmt_bind_param(_sql.trap_stmt, _tbind) != 0) {
        netsnmp_sql_stmt_error(_sql.trap_stmt,
                               "Could not bind parameters for INSERT");
        _sql_log(sqlb, NULL);
        return;
    }

    /** execute the prepared statement */
    if (mysql_stmt_execute(_sql.trap_stmt) != 0) {
        netsnmp_sql_stmt_error(_sql.trap_stmt,
                               "Could not execute insert statement for trap");
        _sql_log(sqlb, NULL);
        return;
    }
    trap_id = mysql_insert_id(_sql.conn);

    /*
     * iterate over the varbinds, copy data and insert
     */
    it = CONTAINER_ITERATOR(sqlb->varbinds);
    if (NULL == it) {
        snmp_log(LOG_ERR,"Could not allocate iterator\n");
        _sql_log(sqlb, NULL);
        return;
    }

    for( sqlvb = ITERATOR_FIRST(it); sqlvb; sqlvb = ITERATOR_NEXT(it)) {

        _vbind[VBIND_ID].buffer = (void *)&trap_id;
        _vbind[VBIND_TYPE].buffer = (void *)&sqlvb->type;

        _vbind[VBIND_OID].buffer = sqlvb->oid;
        _vbind[VBIND_OID].buffer_length = sqlvb->oid_len;

        _vbind[VBIND_VAL].buffer = sqlvb->val;
        _vbind[VBIND_VAL].buffer_length = sqlvb->val_len;

        if (mysql_stmt_bind_param(_sql.vb_stmt, _vbind) != 0) {
            netsnmp_sql_stmt_error(_sql.vb_stmt,
                                   "Could not bind parameters for INSERT");
            _sql_log(sqlb, NULL);
            break;
        }

        if (mysql_stmt_execute(_sql.vb_stmt) != 0) {
            netsnmp_sql_stmt_error(_sql.vb_stmt,
                                   "Could not execute insert statement for varbind");
            _sql_log(sqlb, NULL);
            break;
        }
    }
    ITERATOR_RELEASE(it);
}

/*
 * process (save) queued items to sql database.
 *
 * dontcare & meeither are dummy params so this function can be used
 * as a netsnmp_alarm callback function.
 */
static void
_sql_process_queue(u_int dontcare, void *meeither)
{
    int        rc;

    /** bail if the queue is empty */
    if( 0 == CONTAINER_SIZE(_sql.queue))
        return;

    DEBUGMSGT(("sql:process", "processing %d queued traps\n",
               (int)CONTAINER_SIZE(_sql.queue)));

    /*
     * if we don't have a database connection, try to reconnect. We
     * don't care if we fail - traps will be logged in that case.
     */
    if (0 == _sql.connected) {
        DEBUGMSGT(("sql:process", "no sql connection; reconnecting\n"));
        (void) netsnmp_mysql_connect();
    }

    CONTAINER_FOR_EACH(_sql.queue, (netsnmp_container_obj_func*)_sql_save,
                       NULL);

    if (_sql.connected) {
        rc = mysql_commit(_sql.conn);
        if (rc) { /* nuts... now what? */
            netsnmp_sql_error("commit failed");
            CONTAINER_FOR_EACH(_sql.queue,
                               (netsnmp_container_obj_func*)_sql_log,
                               NULL);
        }
    }

    CONTAINER_CLEAR(_sql.queue, (netsnmp_container_obj_func*)_sql_buf_free,
                    NULL);
}

#else
int unused;	/* Suppress "empty translation unit" warning */
#endif /* NETSNMP_USE_MYSQL */
