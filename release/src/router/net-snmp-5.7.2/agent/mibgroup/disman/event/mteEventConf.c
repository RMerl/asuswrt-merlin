/*
 * DisMan Event MIB:
 *     Implementation of the event table configure handling
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_callbacks.h>
#include "disman/event/mteObjects.h"
#include "disman/event/mteEvent.h"
#include "disman/event/mteEventConf.h"

netsnmp_feature_require(iquery)

/** Initializes the mteEventsConf module */
void
init_mteEventConf(void)
{
    init_event_table_data();

    /*
     * Register config handlers for user-level (fixed) events....
     */
    snmpd_register_config_handler("notificationEvent",
                                   parse_notificationEvent, NULL,
                                   "eventname notifyOID [-m] [-i OID|-o OID]*");
    snmpd_register_config_handler("setEvent",
                                   parse_setEvent,          NULL,
                                   "eventname [-I] OID = value");

    netsnmp_ds_register_config(ASN_BOOLEAN,
                   netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                         NETSNMP_DS_LIB_APPTYPE),
                   "strictDisman", NETSNMP_DS_APPLICATION_ID,
                                   NETSNMP_DS_AGENT_STRICT_DISMAN);

    /*
     * ... and for persistent storage of dynamic event table entries.
     *
     * (The previous implementation didn't store these entries,
     *  so we don't need to worry about backwards compatability)
     */
    snmpd_register_config_handler("_mteETable",
                                   parse_mteETable, NULL, NULL);
    snmpd_register_config_handler("_mteENotTable",
                                   parse_mteENotTable, NULL, NULL);
    snmpd_register_config_handler("_mteESetTable",
                                   parse_mteESetTable, NULL, NULL);

    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_mteETable, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_PRE_UPDATE_CONFIG,
                           clear_mteETable, NULL);
}


/* ==============================
 *
 *       utility routines
 *
 * ============================== */

    /*
     * Find or create the specified event entry
     */
static struct mteEvent *
_find_mteEvent_entry( const char *owner, const char *ename )
{
    netsnmp_variable_list owner_var, ename_var;
    netsnmp_tdata_row *row;
        /*
         * If there's already an existing entry,
         *   then use that...
         */
    memset(&owner_var, 0, sizeof(netsnmp_variable_list));
    memset(&ename_var, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value(&owner_var, ASN_OCTET_STR, owner, strlen(owner));
    snmp_set_var_typed_value(&ename_var, ASN_PRIV_IMPLIED_OCTET_STR,
                                                        ename, strlen(ename));
    owner_var.next_variable = &ename_var;
    row = netsnmp_tdata_row_get_byidx( event_table_data, &owner_var );
        /*
         * ... otherwise, create a new one
         */
    if (!row)
        row = mteEvent_createEntry( owner, ename, 0 );
    if (!row)
        return NULL;

    /* return (struct mteEvent *)netsnmp_tdata_row_entry( row ); */
    return (struct mteEvent *)row->data;
}

static struct mteEvent *
_find_typed_mteEvent_entry( const char *owner, const char *ename, int type )
{
    struct mteEvent *entry = _find_mteEvent_entry( owner, ename );
    if (!entry)
        return NULL;

    /*
     *  If this is an existing (i.e. valid) entry of the
     *    same type, then throw an error and discard it.
     *  But allow combined Set/Notification events.
     */
    if ( entry &&
        (entry->flags & MTE_EVENT_FLAG_VALID) &&
        (entry->mteEventActions & type )) {
        config_perror("error: duplicate event name");
        return NULL;
    }
    return entry;
}


/* ==============================
 *
 *  User-configured (static) events
 *
 * ============================== */

void
parse_notificationEvent( const char *token, char *line )
{
    char   ename[MTE_STR1_LEN+1];
    char   buf[SPRINT_MAX_LEN];
    oid    name_buf[MAX_OID_LEN];
    size_t name_buf_len;
    struct mteEvent  *entry;
    struct mteObject *object;
    int    wild = 1;
    int    idx  = 0;
    char  *cp;
#ifndef NETSNMP_DISABLE_MIB_LOADING
    struct tree         *tp;
#endif
    struct varbind_list *var;

    DEBUGMSGTL(("disman:event:conf", "Parsing notificationEvent config\n"));

    /*
     * The event name could be used directly to index the mteObjectsTable.
     * But it's quite possible that the same name could also be used to
     * set up a mteTriggerTable entry (with trigger-specific objects).
     *
     * To avoid such a clash, we'll add a prefix ("_E").
     */
    memset(ename, 0, sizeof(ename));
    ename[0] = '_';
    ename[1] = 'E';
    cp = copy_nword(line, ename+2,  MTE_STR1_LEN-2);
    if (!cp || ename[2] == '\0') {
        config_perror("syntax error: no event name");
        return;
    }
    
    /*
     *  Parse the notification OID field ...
     */
    cp = copy_nword(cp, buf,  SPRINT_MAX_LEN);
    if ( buf[0] == '\0' ) {
        config_perror("syntax error: no notification OID");
        return;
    }
    name_buf_len = MAX_OID_LEN;
    if (!snmp_parse_oid(buf, name_buf, &name_buf_len)) {
        snmp_log(LOG_ERR, "notificationEvent OID: %s\n", buf);
        config_perror("unknown notification OID");
        return;
    }

    /*
     *  ... and the relevant object/instances.
     */
    if ( cp && *cp=='-' && *(cp+1)=='m' ) {
#ifdef NETSNMP_DISABLE_MIB_LOADING
        config_perror("Can't use -m if MIB loading is disabled");
        return;
#else
        /*
         * Use the MIB definition to add the standard
         *   notification payload to the mteObjectsTable.
         */
        cp = skip_token( cp );
        tp = get_tree( name_buf, name_buf_len, get_tree_head());
        if (!tp) {
            config_perror("Can't locate notification payload info");
            return;
        }
        for (var = tp->varbinds; var; var=var->next) {
            idx++;
            object = mteObjects_addOID( "snmpd.conf", ename, idx,
                                         var->vblabel, wild );
            idx    = object->mteOIndex;
        }
#endif
    }
    while (cp) {
        if ( *cp == '-' ) {
            switch (*(cp+1)) {
            case 'm':
                config_perror("-m option must come first");
                return;
            case 'i':   /* exact instance */
            case 'w':   /* "not-wild" (backward compatability) */
                wild = 0;
                break;
            case 'o':   /* wildcarded object  */
                wild = 1;
                break;
            default:
                config_perror("unrecognised option");
                return;
            }
            cp = skip_token( cp );
            if (!cp) {
                config_perror("missing parameter");
                return;
            }
        }
        idx++;
        cp     = copy_nword(cp, buf,  SPRINT_MAX_LEN);
        object = mteObjects_addOID( "snmpd.conf", ename, idx, buf, wild );
        idx    = object->mteOIndex;
        wild   = 1;    /* default to wildcarded objects */
    }

    /*
     *  If the entry has parsed successfully, then create,
     *     populate and activate the new event entry.
     */
    entry = _find_typed_mteEvent_entry("snmpd.conf", ename+2,
                                       MTE_EVENT_NOTIFICATION);
    if (!entry) {
        mteObjects_removeEntries( "snmpd.conf", ename );
        return;
    }
    entry->mteNotification_len = name_buf_len;
    memcpy( entry->mteNotification, name_buf, name_buf_len*sizeof(oid));
    memcpy( entry->mteNotifyOwner,  "snmpd.conf",  10 );
    memcpy( entry->mteNotifyObjects, ename, MTE_STR1_LEN );
    entry->mteEventActions |= MTE_EVENT_NOTIFICATION;
    entry->flags           |= MTE_EVENT_FLAG_ENABLED |
                              MTE_EVENT_FLAG_ACTIVE  |
                              MTE_EVENT_FLAG_FIXED   |
                              MTE_EVENT_FLAG_VALID;
    return;
}

void
parse_setEvent( const char *token, char *line )
{
    char   ename[MTE_STR1_LEN+1];
    char   buf[SPRINT_MAX_LEN];
    oid    name_buf[MAX_OID_LEN];
    size_t name_buf_len;
    long   value;
    int    wild = 1;
    struct mteEvent  *entry;
    char  *cp;

    DEBUGMSGTL(("disman:event:conf", "Parsing setEvent config...  "));

    memset( ename, 0, sizeof(ename));
    cp = copy_nword(line, ename,  MTE_STR1_LEN);
    if (!cp || ename[0] == '\0') {
        config_perror("syntax error: no event name");
        return;
    }

    if (cp && *cp=='-' && *(cp+1)=='I') {
        wild = 0;               /* an instance assignment */
        cp = skip_token( cp );
    }

    /*
     *  Parse the SET assignment in the form "OID = value"
     */
    cp = copy_nword(cp, buf,  SPRINT_MAX_LEN);
    if ( buf[0] == '\0' ) {
        config_perror("syntax error: no set OID");
        return;
    }
    name_buf_len = MAX_OID_LEN;
    if (!snmp_parse_oid(buf, name_buf, &name_buf_len)) {
        snmp_log(LOG_ERR, "setEvent OID: %s\n", buf);
        config_perror("unknown set OID");
        return;
    }
    if (cp && *cp == '=') {
        cp = skip_token( cp );   /* skip the '=' assignment character */
    }
    if (!cp) {
        config_perror("syntax error: missing set value");
        return;
    }

    value = strtol( cp, NULL, 0);

    /*
     *  If the entry has parsed successfully, then create,
     *     populate and activate the new event entry.
     */
    entry = _find_typed_mteEvent_entry("snmpd.conf", ename, MTE_EVENT_SET);
    if (!entry) {
        return;
    }
    memcpy( entry->mteSetOID, name_buf, name_buf_len*sizeof(oid));
    entry->mteSetOID_len = name_buf_len;
    entry->mteSetValue   = value;
    if (wild)
        entry->flags       |= MTE_SET_FLAG_OBJWILD;
    entry->mteEventActions |= MTE_EVENT_SET;
    entry->flags           |= MTE_EVENT_FLAG_ENABLED |
                              MTE_EVENT_FLAG_ACTIVE  |
                              MTE_EVENT_FLAG_FIXED   |
                              MTE_EVENT_FLAG_VALID;
    return;
}


/* ==============================
 *
 *  Persistent (dynamic) configuration
 *
 * ============================== */

void
parse_mteETable(const char *token, char *line )
{
    char   owner[MTE_STR1_LEN+1];
    char   ename[MTE_STR1_LEN+1];
    void  *vp;
    size_t tmp;
    size_t len;
    struct mteEvent  *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteEventTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( ename, 0, sizeof(ename));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = ename;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteEvent_entry( owner, ename );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, ename));
    
    /*
     * Read in the accessible (event-independent) column values.
     */
    len   = MTE_STR2_LEN; vp = entry->mteEventComment;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    /*
     * Skip the mteEventAction field, and note that the
     *   boolean values are combined into a single field.
     */
    line  = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
    entry->flags |= (tmp &
        (MTE_EVENT_FLAG_ENABLED|MTE_EVENT_FLAG_ACTIVE));
    /*
     * XXX - Will need to read in the 'iquery' access information
     */
    entry->flags |= MTE_EVENT_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}


void
parse_mteENotTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   ename[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteEvent  *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteENotifyTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( ename, 0, sizeof(ename));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = ename;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteEvent_entry( owner, ename );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, ename));

    /*
     * Read in the accessible column values.
     */
    vp = entry->mteNotification;
    line  = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                 &entry->mteNotification_len);
    len   = MTE_STR1_LEN; vp = entry->mteNotifyOwner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = entry->mteNotifyObjects;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    entry->mteEventActions |= MTE_EVENT_NOTIFICATION;
    entry->flags           |= MTE_EVENT_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}


void
parse_mteESetTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   ename[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteEvent  *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteESetTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( ename, 0, sizeof(ename));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = ename;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteEvent_entry( owner, ename );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, ename));

    /*
     * Read in the accessible column values.
     */
    vp = entry->mteSetOID;
    line  = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                 &entry->mteSetOID_len);
    line  = read_config_read_data(ASN_UNSIGNED,  line,
                                 &entry->mteSetValue, &len);
    len   = MTE_STR2_LEN; vp = entry->mteSetTarget;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR2_LEN; vp = entry->mteSetContext;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    entry->mteEventActions |= MTE_EVENT_SET;
    entry->flags           |= MTE_EVENT_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}



int
store_mteETable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr, *cp;
    void           *vp;
    size_t          tint;
    netsnmp_tdata_row *row;
    struct mteEvent  *entry;


    DEBUGMSGTL(("disman:event:conf", "Storing mteEventTable config:\n"));

    for (row = netsnmp_tdata_row_first( event_table_data );
         row;
         row = netsnmp_tdata_row_next( event_table_data, row )) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = (struct mteEvent *)netsnmp_tdata_row_entry( row );
        if ( entry->flags & MTE_EVENT_FLAG_FIXED )
            continue;

        DEBUGMSGTL(("disman:event:conf", "  Storing (%s %s)\n",
                         entry->mteOwner, entry->mteEName));

        /*
         * Save the basic mteEventTable entry...
         */
        memset(line, 0, sizeof(line));
        strcat(line, "_mteETable ");
        cptr = line + strlen(line);

        cp   = entry->mteOwner;        tint = strlen( cp );
        cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->mteEName;        tint = strlen( cp );
        cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->mteEventComment; tint = strlen( cp );
        cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
        /* ... (but skip the mteEventAction field)... */
        tint = entry->flags & (MTE_EVENT_FLAG_ENABLED|MTE_EVENT_FLAG_ACTIVE); 
        cptr = read_config_store_data( ASN_UNSIGNED,  cptr, &tint, NULL );
        /* XXX - Need to store the 'iquery' access information */
        snmpd_store_config(line);

        /*
         * ... then save Notify and/or Set entries separately
         *   (The mteEventAction bits will be set when these are read in).
         */
        if ( entry->mteEventActions & MTE_EVENT_NOTIFICATION ) {
            memset(line, 0, sizeof(line));
            strcat(line, "_mteENotTable ");
            cptr = line + strlen(line);
    
            cp = entry->mteOwner;         tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            cp = entry->mteEName;         tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            vp   = entry->mteNotification;
            cptr = read_config_store_data(ASN_OBJECT_ID, cptr, &vp,
                                          &entry->mteNotification_len);
            cp = entry->mteNotifyOwner;   tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            cp = entry->mteNotifyObjects; tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            snmpd_store_config(line);
        }

        if ( entry->mteEventActions & MTE_EVENT_SET ) {
            memset(line, 0, sizeof(line));
            strcat(line, "_mteESetTable ");
            cptr = line + strlen(line);
    
            cp = entry->mteOwner;         tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            cp = entry->mteEName;         tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            vp   = entry->mteSetOID;
            cptr = read_config_store_data(ASN_OBJECT_ID, cptr, &vp,
                                          &entry->mteSetOID_len);
            tint = entry->mteSetValue;
            cptr = read_config_store_data(ASN_INTEGER,   cptr, &tint, NULL);
            cp = entry->mteSetTarget;     tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            cp = entry->mteSetContext;    tint = strlen( cp );
            cptr = read_config_store_data(ASN_OCTET_STR, cptr, &cp, &tint );
            tint = entry->flags & (MTE_SET_FLAG_OBJWILD|MTE_SET_FLAG_CTXWILD); 
            cptr = read_config_store_data(ASN_UNSIGNED,  cptr, &tint, NULL);
            snmpd_store_config(line);
        }
    }

    DEBUGMSGTL(("disman:event:conf", "  done.\n"));
    return SNMPERR_SUCCESS;
}

int
clear_mteETable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    netsnmp_tdata_row    *row;
    netsnmp_variable_list owner_var;

    /*
     * We're only interested in entries set up via the config files
     */
    memset( &owner_var, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value( &owner_var,  ASN_OCTET_STR,
                             "snmpd.conf", strlen("snmpd.conf"));
    while (( row = netsnmp_tdata_row_next_byidx( event_table_data,
                                                &owner_var ))) {
        /*
         * XXX - check for owner of "snmpd.conf"
         *       and break at the end of these
         */
        netsnmp_tdata_remove_and_delete_row( event_table_data, row );
    }
    return SNMPERR_SUCCESS;
}
