/*
 * DisMan Event MIB:
 *     Implementation of the trigger table configure handling
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_callbacks.h>
#include "utilities/iquery.h"
#include "disman/event/mteObjects.h"
#include "disman/event/mteTrigger.h"
#include "disman/event/mteTriggerConf.h"

#include <ctype.h>

netsnmp_feature_require(iquery)

/** Initializes the mteTriggerConf module */
void
init_mteTriggerConf(void)
{
    init_trigger_table_data();

    /*
     * Register config handler for user-level (fixed) triggers ...
     */
    snmpd_register_const_config_handler("monitor",
                                        parse_mteMonitor,
                                        NULL,
                                        "triggername [-I] [-i OID | -o OID]* [-e event] expression ");
    snmpd_register_const_config_handler("defaultMonitors",
                                        parse_default_mteMonitors,
                                        NULL, "yes|no");
    snmpd_register_const_config_handler("linkUpDownNotifications",
                                        parse_linkUpDown_traps,
                                        NULL, "yes|no");

    /*
     * ... for persistent storage of various event table entries ...
     */
    snmpd_register_config_handler("_mteTTable",
                                   parse_mteTTable,   NULL, NULL);
    snmpd_register_config_handler("_mteTDTable",
                                   parse_mteTDTable,  NULL, NULL);
    snmpd_register_config_handler("_mteTExTable",
                                   parse_mteTExTable, NULL, NULL);
    snmpd_register_config_handler("_mteTBlTable",
                                   parse_mteTBlTable, NULL, NULL);
    snmpd_register_config_handler("_mteTThTable",
                                   parse_mteTThTable, NULL, NULL);

    /*
     * ... and backwards compatability with the previous implementation.
     */
    snmpd_register_config_handler("mteTriggerTable",
                                   parse_mteTriggerTable, NULL, NULL);

    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           store_mteTTable, NULL);
    snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                           SNMPD_CALLBACK_PRE_UPDATE_CONFIG,
                           clear_mteTTable, NULL);
}

/* ==============================
 *
 *       utility routines
 *
 * ============================== */

    /*
     * Find or create the specified trigger entry
     */
struct mteTrigger *
_find_mteTrigger_entry( const char *owner, char *tname )
{
    netsnmp_variable_list owner_var, tname_var;
    netsnmp_tdata_row *row;

        /*
         * If there's already an existing entry,
         *   then use that...
         */
    memset(&owner_var, 0, sizeof(netsnmp_variable_list));
    memset(&tname_var, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value(&owner_var, ASN_OCTET_STR, owner, strlen(owner));
    snmp_set_var_typed_value(&tname_var, ASN_PRIV_IMPLIED_OCTET_STR,
                                                        tname, strlen(tname));
    owner_var.next_variable = &tname_var;
    row = netsnmp_tdata_row_get_byidx( trigger_table_data, &owner_var );
        /*
         * ... otherwise, create a new one
         */
    if (!row)
        row = mteTrigger_createEntry( owner, tname, 0 );
    if (!row)
        return NULL;
    
    /* return (struct mteTrigger *)netsnmp_tdata_row_entry( row ); */
    return (struct mteTrigger *)row->data;
}

struct mteTrigger *
_find_typed_mteTrigger_entry( const char *owner, char *tname, int type )
{
    struct mteTrigger *entry = _find_mteTrigger_entry( owner, tname );
    if (!entry)
        return NULL;

    /*
     *  If this is an existing (i.e. valid) entry of the
     *    same type, then throw an error and discard it.
     *  But allow combined Existence/Boolean/Threshold trigger.
     */
    if ( entry &&
        (entry->flags & MTE_TRIGGER_FLAG_VALID) &&
        (entry->mteTriggerTest & type )) {
        config_perror("duplicate trigger name");
        return NULL;
    }
    return entry;
}


/* ================================================
 *
 *  Handlers for user-configured (static) triggers
 *
 * ================================================ */

int
_mteTrigger_callback_enable( int   majorID,    int   minorID,
                             void *serverargs, void *clientarg)
{
    struct mteTrigger *entry = (struct mteTrigger *)clientarg;
    mteTrigger_enable( entry );

    return 0;
}


void
parse_mteMonitor(const char *token, const char *line)
{
    char   buf[  SPRINT_MAX_LEN];
    char   tname[MTE_STR1_LEN+1];
    const char  *cp;
    long   test = 0;

    char   ename[MTE_STR1_LEN+1];
    long   flags = MTE_TRIGGER_FLAG_ENABLED |
                   MTE_TRIGGER_FLAG_ACTIVE  |
                   MTE_TRIGGER_FLAG_FIXED   |
                   MTE_TRIGGER_FLAG_VWILD   |
                   MTE_TRIGGER_FLAG_SYSUPT  |
                   MTE_TRIGGER_FLAG_VALID;
    long   idx     = 0;
    long   startup = 1;    /* ??? or 0 */
    long   repeat  = 600;
    netsnmp_session *sess = NULL;

    int    seen_name = 0;
    char   oid_name_buf[SPRINT_MAX_LEN];
    oid    name_buf[MAX_OID_LEN];
    size_t name_buf_len;
    u_char op    = 0;
    long   value = 0;

    struct mteObject  *object;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing disman monitor config (%s)\n", line));

    /*
     * Before parsing the configuration fully, first
     * skim through the config line in order to:
     *   a) locate the name for the trigger, and
     *   b) identify the type of trigger test
     *
     * This information will be used both for creating the trigger
     *  entry, and registering any additional payload objects.
     */
    memset( buf,   0, sizeof(buf));
    memset( tname, 0, sizeof(tname));
    memset( ename, 0, sizeof(ename));
    for (cp = copy_nword_const(line, buf, SPRINT_MAX_LEN);
         ;
         cp = copy_nword_const(cp,   buf, SPRINT_MAX_LEN)) {

        if ( buf[0] == '-' ) {
            switch (buf[1]) {
            case 't':
                /* No longer necessary */
                break;
            case 'd':
            case 'e':
            case 'o':
            case 'r':
            case 'u':
                /* skip option parameter */
                cp = skip_token_const( cp );
                break;
            case 'D':
            case 'I':
            case 's':
            case 'S':
                /* flag options */
                break;
            case 'i':
                /*
                 * '-i' can act as a flag or take a parameter.
                 *      Handle either case.
                 */
                if (cp && *cp != '-')
                    cp = skip_token_const( cp );
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                /* accept negative values */
            case '\0':
                /* and '-' placeholder value */
                break;
            default:
                config_perror("unrecognised option");
                return;
            }
        } else {
            /*
             * Save the first non-option parameter as the trigger name.
             *
             * This name will also be used to register entries in the
             *    mteObjectsTable, so insert a distinguishing prefix.
             * This will ensure combined trigger entries don't clash with
             *    each other, or with a similarly-named notification event.
             */
            if ( !tname[0] ) {
                tname[0] = '_';
                tname[1] = '_';   /* Placeholder */
                memcpy( tname+2, buf, MTE_STR1_LEN-2 );
            } else {
                /*
                 * This marks the beginning of the monitor expression,
                 *   so we don't need to scan any further
                 */
                break;
            }
        }
        if (!cp)
            break;
    }

    /*
     * Now let's examine the expression to determine the type of
     *   monitor being configured.  There are four possible forms:
     *     != OID  (or ! OID)     (existence test)
     *        OID                 (existence test)
     *        OID  op  VALUE      (boolean   test)
     *        OID  MIN MAX        (threshold test)
     */
    if ( *buf == '!' ) {
       /*
        * If the expression starts with '!=' or '!', then
        *  it must be the first style of existence test.
        */
       test = MTE_TRIGGER_EXISTENCE;
    } else {
       /*
        * Otherwise the first token is the OID to be monitored.
        *   Skip it and look at the next token (if any).
        */
        cp = copy_nword_const(cp,   buf, SPRINT_MAX_LEN);
        if (cp) {
            /*
             * If this is a numeric value, then it'll be the MIN
             *   field of a threshold test (the fourth form)
             * Otherwise it'll be the operation field of a
             *   boolean test (the third form)
             */
            if ( isdigit((unsigned char)(buf[0])) || buf[0] == '-' )
                test = MTE_TRIGGER_THRESHOLD;
            else
                test = MTE_TRIGGER_BOOLEAN;
        } else {
            /*
             * If there isn't a "next token", then this
             *   must be the second style of existence test.
             */
            test = MTE_TRIGGER_EXISTENCE;
        }
    }

    /*
     * Use the type of trigger test to update the trigger name buffer
     */
    switch (test) {
    case MTE_TRIGGER_BOOLEAN:
        tname[1] = 'B'; break;
    case MTE_TRIGGER_THRESHOLD:
        tname[1] = 'T'; break;
    case MTE_TRIGGER_EXISTENCE:
        tname[1] = 'X'; break;
    }
    
 
    
    /*
     * Now start parsing again at the beginning of the directive,
     *   extracting the various options...
     */
    for (cp = copy_nword_const(line, buf, SPRINT_MAX_LEN);
         ;
         cp = copy_nword_const(cp,   buf, SPRINT_MAX_LEN)) {

        if (buf[0] == '-' ) {
            switch (buf[1]) {
            case 'D':   /* delta sample value */
                flags |= MTE_TRIGGER_FLAG_DELTA;
                break;

            case 'd':   /* discontinuity OID (implies delta sample) */
                flags |= MTE_TRIGGER_FLAG_DELTA;
                if (buf[2] != 'i')
                    flags |= MTE_TRIGGER_FLAG_DWILD;
                memset( oid_name_buf, 0, sizeof(oid_name_buf));
                memset(     name_buf, 0, sizeof(    name_buf));
                name_buf_len = MAX_OID_LEN;
                cp = copy_nword_const(cp, oid_name_buf, MTE_STR1_LEN);
                if (!snmp_parse_oid(oid_name_buf, name_buf, &name_buf_len)) {
                    snmp_log(LOG_ERR, "discontinuity OID: %s\n", oid_name_buf);
                    config_perror("unknown discontinuity OID");
                    mteObjects_removeEntries( "snmpd.conf", tname );
                    return;
                }
                if ( snmp_oid_compare( name_buf, name_buf_len,
                                       _sysUpTime_instance,
                                       _sysUpTime_inst_len) != 0 )
                    flags &= ~MTE_TRIGGER_FLAG_SYSUPT;
                break;
    
            case 'e':   /*  event */
                cp     = copy_nword_const(cp, ename, MTE_STR1_LEN);
                break;
    
            case 'I':   /* value instance */
                flags &= ~MTE_TRIGGER_FLAG_VWILD;
                break;
    
                        /*
                         * "instance" flag:
                         *     either non-wildcarded mteTriggerValueID
                         *       (backwards compatability - see '-I')
                         *     or exact payload OID
                         *       (c.f. notificationEvent config)
                         */
            case 'i':
                if ( *cp == '-' ) {
                    /* Backwards compatibility - now '-I' */
                    flags &= ~MTE_TRIGGER_FLAG_VWILD;
                    continue;
                }
                idx++;
                cp     = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                object = mteObjects_addOID( "snmpd.conf", tname, idx, buf, 0 );
                if (!object) {
                    snmp_log(LOG_ERR, "Unknown payload OID: %s\n", buf);
                    config_perror("Unknown payload OID");
                    mteObjects_removeEntries( "snmpd.conf", tname );
                } else
                    idx = object->mteOIndex;
                break;
    
            case 'o':   /*  object  */
                idx++;
                cp     = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                object = mteObjects_addOID( "snmpd.conf", tname, idx, buf, 1 );
                if (!object) {
                    snmp_log(LOG_ERR, "Unknown payload OID: %s\n", buf);
                    config_perror("Unknown payload OID");
                    mteObjects_removeEntries( "snmpd.conf", tname );
                } else
                    idx = object->mteOIndex;
                break;
    
            case 'r':   /*  repeat frequency */
                cp     = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                repeat = strtoul(buf, NULL, 0);
                break;
    
            case 'S':   /* disable startup tests */
                startup = 0;
                break;

            case 's':   /* enable startup tests (default?) */
                startup = 1;
                break;

            case 't':   /* threshold test - already handled */
                break;
    
            case 'u':   /*  user */
                cp     = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                sess   = netsnmp_iquery_user_session(buf);
                if (NULL == sess) {
                    snmp_log(LOG_ERR, "user name %s not found\n", buf);
                    config_perror("Unknown user name\n");
                    mteObjects_removeEntries( "snmpd.conf", tname );
                    return;
                }
                break;
            }
        } else {
            /*
             * Skip the first non-option token - the trigger
             *  name (which has already been processed earlier).
             */
            if ( !seen_name ) {
                seen_name = 1;
            } else {
                /*
                 * But the next non-option token encountered will
                 *  mark the start of the expression to be monitored.
                 *
                 * There are three possible expression formats:
                 *      [op] OID               (existence tests)
                 *      OID op value           (boolean tests)
                 *      OID val val [val val]  (threshold tests)
                 * 
                 * Extract the OID, operation and (first) value.
                 */
                switch ( test ) {
                case MTE_TRIGGER_EXISTENCE:
                    /*
                     * Identify the existence operator (if any)...
                     */
                    op = MTE_EXIST_PRESENT;
                    if (buf[0] == '!') {
                       if (buf[1] == '=') {
                           op = MTE_EXIST_CHANGED;
                       } else {
                           op = MTE_EXIST_ABSENT;
                       }
                       cp = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                    }
                    /*
                     * ... then extract the monitored OID.
                     *     (ignoring anything that remains)
                     */
                    memcpy(oid_name_buf, buf, SPRINT_MAX_LEN);
                    cp = NULL;  /* To terminate the processing loop */
                    DEBUGMSGTL(("disman:event:conf", "%s: Exist (%s, %d)\n",
                                                     tname, oid_name_buf, op));
                    break;
    
                case MTE_TRIGGER_BOOLEAN:
                    /*
                     * Extract the monitored OID, and 
                     *   identify the boolean operator ...
                     */
                    memcpy(oid_name_buf, buf, SPRINT_MAX_LEN);
                    cp = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                    if (buf[0] == '!') {
                       op = MTE_BOOL_UNEQUAL;
                    } else if (buf[0] == '=') {
                       op = MTE_BOOL_EQUAL;
                    } else if (buf[0] == '<') {
                       if (buf[1] == '=') {
                           op = MTE_BOOL_LESSEQUAL;
                       } else {
                           op = MTE_BOOL_LESS;
                       }
                    } else if (buf[0] == '>') {
                       if (buf[1] == '=') {
                           op = MTE_BOOL_GREATEREQUAL;
                       } else {
                           op = MTE_BOOL_GREATER;
                       }
                    }
                    /*
                     * ... then extract the comparison value.
                     *     (ignoring anything that remains)
                     */
                    cp    = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                    value = strtol(buf, NULL, 0);
                    cp = NULL;  /* To terminate the processing loop */
                    DEBUGMSGTL(("disman:event:conf", "%s: Bool (%s, %d, %ld)\n",
                                              tname, oid_name_buf, op, value));
                    break;
    
                case MTE_TRIGGER_THRESHOLD:
                    /*
                     * Extract the monitored OID, and 
                     *   the first comparison value...
                     */
                    memcpy(oid_name_buf, buf, SPRINT_MAX_LEN);
                    memset(         buf,   0, SPRINT_MAX_LEN);
                    cp  = copy_nword_const(cp, buf, SPRINT_MAX_LEN);
                        value = strtol(buf, NULL, 0);
    
                    /*
                     * ... then save the rest of the line for later.
                     */
                    memset( buf, 0,  strlen(buf));
                    memcpy( buf, cp, strlen(cp));
                    cp = NULL;  /* To terminate the processing loop */
                    DEBUGMSGTL(("disman:event:conf", "%s: Thresh (%s, %ld, %s)\n",
                                              tname, oid_name_buf, value, buf));
                    break;
                }
            }
        }
        if (!cp)
            break;
    }

    if (NULL == sess) {
        sess = netsnmp_query_get_default_session();
        if (NULL == sess) {
            config_perror
                ("You must specify a default user name using the agentSecName token\n");
            mteObjects_removeEntries( "snmpd.conf", tname );
            return;
        }
    }

    /*
     *  ... and then create the new trigger entry...
     */
    entry = _find_typed_mteTrigger_entry( "snmpd.conf", tname+2, test );
    if (!entry) {
        /* mteObjects_removeEntries( "snmpd.conf", tname ); */
        return;
    }

    /*
     *  ... populate the type-independent fields...
     *     (setting the delta discontinuity OID first)
     */
    if ( (flags & MTE_TRIGGER_FLAG_DELTA) &&
        !(flags & MTE_TRIGGER_FLAG_SYSUPT)) {
        memset( entry->mteDeltaDiscontID, 0, sizeof(entry->mteDeltaDiscontID));
        memcpy( entry->mteDeltaDiscontID, name_buf, name_buf_len*sizeof(oid));
        entry->mteDeltaDiscontID_len = name_buf_len;
    }
    name_buf_len = MAX_OID_LEN;
    if (!snmp_parse_oid(oid_name_buf, name_buf, &name_buf_len)) {
        snmp_log(LOG_ERR, "trigger OID: %s\n", oid_name_buf);
        config_perror("unknown monitor OID");
        mteObjects_removeEntries( "snmpd.conf", tname );
        return;
    }
    entry->session               = sess;
    entry->flags                |= flags;
    entry->mteTriggerTest       |= test;
    entry->mteTriggerFrequency   = repeat;
    entry->mteTriggerValueID_len = name_buf_len;
    memcpy(entry->mteTriggerValueID, name_buf, name_buf_len*sizeof(oid));

    /*
     * ... and the relevant test-specific fields.
     */
    switch (test) {
    case MTE_TRIGGER_EXISTENCE:
        entry->mteTExTest = op;
        if (op != MTE_EXIST_CHANGED && startup)
            entry->mteTExStartup = op;
        if ( idx > 0 ) {
            /*
             * Refer to the objects for this trigger (if any)...
             */
            memset(entry->mteTExObjOwner, 0,     MTE_STR1_LEN+1);
            memcpy(entry->mteTExObjOwner, "snmpd.conf",      10);
            memcpy(entry->mteTExObjects,  tname, MTE_STR1_LEN+1);
        }
        if ( ename[0] ) {
            /*
             * ... and the specified event...
             */
            memset(entry->mteTExEvOwner,  0,     MTE_STR1_LEN+1);
            if ( ename[0] == '_' )
                memcpy(entry->mteTExEvOwner,  "_snmpd",       6);
            else
                memcpy(entry->mteTExEvOwner,  "snmpd.conf",  10);
            memcpy(entry->mteTExEvent,    ename, MTE_STR1_LEN+1);
        } else {
            /*
             * ... or the hardcoded default event.
             */
            memset(entry->mteTExEvOwner,  0,     MTE_STR1_LEN+1);
            memset(entry->mteTExEvent,    0,     MTE_STR1_LEN+1);
            memcpy(entry->mteTExEvOwner,  "_snmpd",           6);
            memcpy(entry->mteTExEvent,    "_mteTriggerFired", 16);
        }
        break;
    case MTE_TRIGGER_BOOLEAN:
        entry->mteTBoolComparison = op;
        entry->mteTBoolValue      = value;
        if (!startup)
            entry->flags &= ~MTE_TRIGGER_FLAG_BSTART;
        if ( idx > 0 ) {
            /*
             * Refer to the objects for this trigger (if any)...
             */
            memset(entry->mteTBoolObjOwner, 0,     MTE_STR1_LEN+1);
            memcpy(entry->mteTBoolObjOwner, "snmpd.conf",      10);
            memcpy(entry->mteTBoolObjects,  tname, MTE_STR1_LEN+1);
        }
        if ( ename[0] ) {
            /*
             * ... and the specified event...
             */
            memset(entry->mteTBoolEvOwner,  0,     MTE_STR1_LEN+1);
            if ( ename[0] == '_' )
                memcpy(entry->mteTBoolEvOwner,  "_snmpd",       6);
            else
                memcpy(entry->mteTBoolEvOwner,  "snmpd.conf",  10);
            memcpy(entry->mteTBoolEvent,    ename, MTE_STR1_LEN+1);
        } else {
            /*
             * ... or the hardcoded default event.
             */
            memset(entry->mteTBoolEvOwner,  0,     MTE_STR1_LEN+1);
            memset(entry->mteTBoolEvent,    0,     MTE_STR1_LEN+1);
            memcpy(entry->mteTBoolEvOwner,  "_snmpd",           6);
            memcpy(entry->mteTBoolEvent,    "_mteTriggerFired", 16);
        }
        break;
    case MTE_TRIGGER_THRESHOLD:
            entry->mteTThFallValue  = value;
            value = strtol(buf, NULL, 0);
            entry->mteTThRiseValue  = value;
            if (!startup)
                entry->mteTThStartup = 0;
            if ( idx > 0 ) {
                /*
                 * Refer to the objects for this trigger (if any)...
                 */
                memset(entry->mteTThObjOwner,   0,     MTE_STR1_LEN+1);
                memcpy(entry->mteTThObjOwner,   "snmpd.conf",      10);
                memcpy(entry->mteTThObjects,    tname, MTE_STR1_LEN+1);
            }
            if ( ename[0] ) {
                /*
                 * ... and the specified event...
                 *  (using the same event for all triggers)
                 */
                memset(entry->mteTThRiseOwner,  0,     MTE_STR1_LEN+1);
                if ( ename[0] == '_' )
                    memcpy(entry->mteTThRiseOwner,  "_snmpd",       6);
                else
                    memcpy(entry->mteTThRiseOwner,  "snmpd.conf",  10);
                memcpy(entry->mteTThRiseEvent,  ename, MTE_STR1_LEN+1);
                memset(entry->mteTThFallOwner,  0,     MTE_STR1_LEN+1);
                if ( ename[0] == '_' )
                    memcpy(entry->mteTThFallOwner,  "_snmpd",       6);
                else
                    memcpy(entry->mteTThFallOwner,  "snmpd.conf",  10);
                memcpy(entry->mteTThFallEvent,  ename, MTE_STR1_LEN+1);
            } else {
                /*
                 * ... or the hardcoded default events.
                 */
                memset(entry->mteTThRiseOwner,  0,     MTE_STR1_LEN+1);
                memset(entry->mteTThFallOwner,  0,     MTE_STR1_LEN+1);
                memset(entry->mteTThRiseEvent,  0,     MTE_STR1_LEN+1);
                memset(entry->mteTThFallEvent,  0,     MTE_STR1_LEN+1);
                memcpy(entry->mteTThRiseOwner,  "_snmpd",           6);
                memcpy(entry->mteTThFallOwner,  "_snmpd",           6);
                memcpy(entry->mteTThRiseEvent,  "_mteTriggerRising", 17);
                memcpy(entry->mteTThFallEvent,  "_mteTriggerFalling", 18);
            }
            cp = skip_token(buf);   /* skip riseThreshold value */

        /*
         * Parse and set (optional) Delta thresholds & events
         */
        if ( cp && *cp != '\0' ) {
            if (entry->flags & MTE_TRIGGER_FLAG_DELTA) {
                config_perror("Delta-threshold on delta-samples not supported");
                mteObjects_removeEntries( "snmpd.conf", tname );
                return;
            }
            value = strtol(cp, NULL, 0);
            entry->mteTThDFallValue  = value;
            cp = skip_token_const(cp);
            value = strtol(cp, NULL, 0);
            entry->mteTThDRiseValue  = value;
            /*
             * Set the events in the same way as before
             */
            if ( ename[0] ) {
                memset(entry->mteTThDRiseOwner,  0,     MTE_STR1_LEN+1);
                if ( ename[0] == '_' )
                    memcpy(entry->mteTThDRiseOwner,  "_snmpd",       6);
                else
                    memcpy(entry->mteTThDRiseOwner,  "snmpd.conf",  10);
                memcpy(entry->mteTThDRiseEvent,  ename, MTE_STR1_LEN+1);
                memset(entry->mteTThDFallOwner,  0,     MTE_STR1_LEN+1);
                if ( ename[0] == '_' )
                    memcpy(entry->mteTThDFallOwner,  "_snmpd",       6);
                else
                    memcpy(entry->mteTThDFallOwner,  "snmpd.conf",  10);
                memcpy(entry->mteTThDFallEvent,  ename, MTE_STR1_LEN+1);
            } else {
                memset(entry->mteTThDRiseOwner,  0,     MTE_STR1_LEN+1);
                memset(entry->mteTThDFallOwner,  0,     MTE_STR1_LEN+1);
                memset(entry->mteTThDRiseEvent,  0,     MTE_STR1_LEN+1);
                memset(entry->mteTThDFallEvent,  0,     MTE_STR1_LEN+1);
                memcpy(entry->mteTThDRiseOwner,  "_snmpd",           6);
                memcpy(entry->mteTThDFallOwner,  "_snmpd",           6);
                memcpy(entry->mteTThDRiseEvent,  "_mteTriggerRising", 17);
                memcpy(entry->mteTThDFallEvent,  "_mteTriggerFalling", 18);
            }
        }
 
        break;
    }
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, 
                           SNMP_CALLBACK_POST_READ_CONFIG,
                           _mteTrigger_callback_enable, entry );
    return;
}

void
parse_linkUpDown_traps(const char *token, const char *line)
{
    /*
     * XXX - This isn't strictly correct according to the
     *       definitions in IF-MIB, but will do for now.
     */
    if (strncmp( line, "yes", 3) == 0) {
        DEBUGMSGTL(("disman:event:conf", "Registering linkUpDown traps\n"));

        /* ifOperStatus */
        parse_mteMonitor("monitor",
            "-r 60 -S -e _linkUp   \"linkUp\"   .1.3.6.1.2.1.2.2.1.8 != 2");
        parse_mteMonitor("monitor",
            "-r 60 -S -e _linkDown \"linkDown\" .1.3.6.1.2.1.2.2.1.8 == 2");
    }
}


void
parse_default_mteMonitors(const char *token, const char *line)
{
    if (strncmp( line, "yes", 3) == 0) {
        DEBUGMSGTL(("disman:event:conf", "Registering default monitors\n"));

        parse_mteMonitor("monitor",
            "-o prNames -o prErrMessage   \"process table\" prErrorFlag  != 0");
        parse_mteMonitor("monitor",
            "-o memErrorName -o memSwapErrorMsg \"memory\"  memSwapError != 0");
        parse_mteMonitor("monitor",
            "-o extNames -o extOutput     \"extTable\"      extResult    != 0");
        parse_mteMonitor("monitor",
            "-o dskPath -o dskErrorMsg    \"dskTable\"      dskErrorFlag != 0");
        parse_mteMonitor("monitor",
            "-o laNames -o laErrMessage   \"laTable\"       laErrorFlag  != 0");
        parse_mteMonitor("monitor",
            "-o fileName -o fileErrorMsg  \"fileTable\"    fileErrorFlag != 0");
        parse_mteMonitor("monitor",
            "-o snmperrErrMessage         \"snmperrs\"  snmperrErrorFlag != 0");
    }
    return;
}

/* ================================================
 *
 *  Handlers for loading persistent trigger entries
 *
 * ================================================ */



/* 
 *  Entries for the main mteTriggerTable
 */

char *
_parse_mteTCols( char *line, struct mteTrigger *entry, int bcomp )
{
    void  *vp;
    size_t tmp;
    size_t len;

    len  = MTE_STR2_LEN; vp = entry->mteTriggerComment;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    if ( bcomp ) {
        /*
         * The newer style of config directive skips the
         *   mteTriggerTest and mteTriggerSampleType values,
         *   as these are set implicitly from the relevant
         *   config directives.
         * Backwards compatability with the previous (combined) 
         *   style reads these in explicitly.
         */
        len  = 1;
        vp   = &entry->mteTriggerTest;
        line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        if (tmp == 2)
            entry->flags |= MTE_TRIGGER_FLAG_DELTA;
    }
    vp   = entry->mteTriggerValueID;
    entry->mteTriggerValueID_len = MAX_OID_LEN;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->mteTriggerValueID_len);
    if (bcomp) {
        /*
         * The newer style combines the various boolean values
         *   into a single field (which comes later).
         * Backwards compatability means reading these individually. 
         */
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        if (tmp == TV_TRUE)
            entry->flags |= MTE_TRIGGER_FLAG_VWILD;
    }
    len  = MTE_STR2_LEN; vp = entry->mteTriggerTarget;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR2_LEN; vp = entry->mteTriggerContext;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    if (bcomp) {
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        if (tmp == TV_TRUE)
            entry->flags |= MTE_TRIGGER_FLAG_CWILD;
    }

    line = read_config_read_data(ASN_UNSIGNED,  line,
                                &entry->mteTriggerFrequency, NULL);

    len  = MTE_STR1_LEN; vp = entry->mteTriggerOOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN; vp = entry->mteTriggerObjects;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    /*
     * Assorted boolean flag values, combined into a single field
     */
    if (bcomp) {
        /*
         * Backwards compatability stores the mteTriggerEnabled
         *   and mteTriggerEntryStatus values separately...
         */
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        if (tmp == TV_TRUE)
            entry->flags |= MTE_TRIGGER_FLAG_ENABLED;
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        if (tmp == RS_ACTIVE)
            entry->flags |= MTE_TRIGGER_FLAG_ACTIVE;
    } else {
        /*
         * ... while the newer style combines all the assorted
         *       boolean values into this single field.
         */
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        entry->flags |= ( tmp &
            (MTE_TRIGGER_FLAG_VWILD  |MTE_TRIGGER_FLAG_CWILD|
             MTE_TRIGGER_FLAG_ENABLED|MTE_TRIGGER_FLAG_ACTIVE));
    }

    return line;
}

void
parse_mteTTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   tname[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteTriggerTable config...\n"));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( tname, 0, sizeof(tname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = tname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteTrigger_entry( owner, tname );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, tname));
    
    /*
     * Read in the accessible (trigger-independent) column values.
     */
    line = _parse_mteTCols( line, entry, 0 );
    /*
     * XXX - Will need to read in the 'iquery' access information
     */
    entry->flags |= MTE_TRIGGER_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}


/* 
 *  Entries from the mteTriggerDeltaTable
 */

char *
_parse_mteTDCols( char *line, struct mteTrigger *entry, int bcomp )
{
    void  *vp;
    size_t tmp;

    entry->mteDeltaDiscontID_len = MAX_OID_LEN;
    vp   = entry->mteDeltaDiscontID;
    line = read_config_read_data(ASN_OBJECT_ID, line, &vp,
                                &entry->mteDeltaDiscontID_len);
    line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
    if (bcomp) {
        if ( tmp == TV_TRUE )
            entry->flags |= MTE_TRIGGER_FLAG_DWILD;
    } else {
        if ( tmp & MTE_TRIGGER_FLAG_DWILD )
            entry->flags |= MTE_TRIGGER_FLAG_DWILD;
    }
    line = read_config_read_data(ASN_UNSIGNED,  line,
                                &entry->mteDeltaDiscontIDType, NULL);

    return line;
}

void
parse_mteTDTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   tname[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteTriggerDeltaTable config... "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( tname, 0, sizeof(tname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = tname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteTrigger_entry( owner, tname );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, tname));
    
    /*
     * Read in the accessible column values.
     */
    line = _parse_mteTDCols( line, entry, 0 );

    entry->flags |= (MTE_TRIGGER_FLAG_DELTA|
                     MTE_TRIGGER_FLAG_VALID);

    DEBUGMSG(("disman:event:conf", "\n"));
}


/* 
 *  Entries from the mteTriggerExistenceTable
 */

char *
_parse_mteTExCols( char *line, struct mteTrigger *entry, int bcomp )
{
    void  *vp;
    size_t tmp;
    size_t len;

    if (bcomp) {
        len  = 1;
        vp   = &entry->mteTExTest;
        line = read_config_read_data(ASN_OCTET_STR,  line, &vp, &len);
        len  = 1;
        vp   = &entry->mteTExStartup;
        line = read_config_read_data(ASN_OCTET_STR,  line, &vp, &len);
    } else {
        line  = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        entry->mteTExStartup = ( tmp & 0xff ); tmp >>= 8;
        entry->mteTExTest    = ( tmp & 0xff );
    }

    len  = MTE_STR1_LEN; vp = entry->mteTExObjOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len  = MTE_STR1_LEN; vp = entry->mteTExObjects;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    len  = MTE_STR1_LEN; vp = entry->mteTExEvOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len  = MTE_STR1_LEN; vp = entry->mteTExEvent;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    return line;
}

void
parse_mteTExTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   tname[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteTriggerExistenceTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( tname, 0, sizeof(tname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = tname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteTrigger_entry( owner, tname );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, tname));
    
    /*
     * Read in the accessible column values.
     *  (Note that the first two are combined into a single field)
     */
    line = _parse_mteTExCols( line, entry, 0 );

    entry->mteTriggerTest |= MTE_TRIGGER_EXISTENCE;
    entry->flags          |= MTE_TRIGGER_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}


/* 
 *  Entries from the mteTriggerBooleanTable
 */

char *
_parse_mteTBlCols( char *line, struct mteTrigger *entry, int bcomp )
{
    void  *vp;
    size_t tmp;
    size_t len;

    if (bcomp) {
        line = read_config_read_data(ASN_UNSIGNED,  line,
                                    &entry->mteTBoolComparison, NULL);
        line = read_config_read_data(ASN_INTEGER,   line,
                                    &entry->mteTBoolValue, NULL);
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        if (tmp == TV_TRUE)
            entry->flags |= MTE_TRIGGER_FLAG_BSTART;
    } else {
        line = read_config_read_data(ASN_UNSIGNED,  line, &tmp, NULL);
        entry->mteTBoolComparison = ( tmp & 0x0f );
        entry->flags             |= ( tmp & MTE_TRIGGER_FLAG_BSTART );
        line = read_config_read_data(ASN_INTEGER,   line,
                                    &entry->mteTBoolValue, NULL);
    }


    len  = MTE_STR1_LEN; vp = entry->mteTBoolObjOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len  = MTE_STR1_LEN; vp = entry->mteTBoolObjects;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    len  = MTE_STR1_LEN; vp = entry->mteTBoolEvOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len  = MTE_STR1_LEN; vp = entry->mteTBoolEvent;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);

    return line;
}

void
parse_mteTBlTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   tname[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteTriggerBooleanTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( tname, 0, sizeof(tname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = tname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteTrigger_entry( owner, tname );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, tname));
    
    /*
     * Read in the accessible column values.
     *  (Note that the first & third are combined into a single field)
     */
    line = _parse_mteTBlCols( line, entry, 0 );

    entry->mteTriggerTest |= MTE_TRIGGER_BOOLEAN;
    entry->flags          |= MTE_TRIGGER_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}


/* 
 *  Entries from the mteTriggerThresholdTable
 */

char *
_parse_mteTThCols( char *line, struct mteTrigger *entry, int bcomp )
{
    void  *vp;
    size_t len;

    line = read_config_read_data(ASN_UNSIGNED,  line,
                                &entry->mteTThStartup,    NULL);
    line = read_config_read_data(ASN_INTEGER,   line,
                                &entry->mteTThRiseValue,  NULL);
    line = read_config_read_data(ASN_INTEGER,   line,
                                &entry->mteTThFallValue,  NULL);
    line = read_config_read_data(ASN_INTEGER,   line,
                                &entry->mteTThDRiseValue, NULL);
    line = read_config_read_data(ASN_INTEGER,   line,
                                &entry->mteTThDFallValue, NULL);

    len  = MTE_STR1_LEN;   vp = entry->mteTThObjOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThObjects;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);

    len  = MTE_STR1_LEN;   vp = entry->mteTThRiseOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThRiseEvent;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThFallOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThFallEvent;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);

    len  = MTE_STR1_LEN;   vp = entry->mteTThDRiseOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThDRiseEvent;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThDFallOwner;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);
    len  = MTE_STR1_LEN;   vp = entry->mteTThDFallEvent;
    line = read_config_read_data(ASN_OCTET_STR, line, &vp, &len);

    return line;
}

void
parse_mteTThTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   tname[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing mteTriggerThresholdTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( tname, 0, sizeof(tname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = tname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteTrigger_entry( owner, tname );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, tname));
    
    /*
     * Read in the accessible column values.
     */
    line = _parse_mteTThCols( line, entry, 0 );

    entry->mteTriggerTest |= MTE_TRIGGER_THRESHOLD;
    entry->flags          |= MTE_TRIGGER_FLAG_VALID;

    DEBUGMSG(("disman:event:conf", "\n"));
}


/* 
 *  Backwards Compatability with the previous implementation
 */

void
parse_mteTriggerTable(const char *token, char *line)
{
    char   owner[MTE_STR1_LEN+1];
    char   tname[MTE_STR1_LEN+1];
    void  *vp;
    size_t len;
    struct mteTrigger *entry;

    DEBUGMSGTL(("disman:event:conf", "Parsing previous mteTriggerTable config...  "));

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof(owner));
    memset( tname, 0, sizeof(tname));
    len   = MTE_STR1_LEN; vp = owner;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    len   = MTE_STR1_LEN; vp = tname;
    line  = read_config_read_data(ASN_OCTET_STR, line, &vp,  &len);
    entry = _find_mteTrigger_entry( owner, tname );

    DEBUGMSG(("disman:event:conf", "(%s, %s) ", owner, tname));
    
    /*
     * Read in the accessible column values for each table in turn...
     *   (similar, though not identical to the newer style).
     */
    line = _parse_mteTCols(   line, entry, 1 );
    line = _parse_mteTDCols(  line, entry, 1 );
    line = _parse_mteTExCols( line, entry, 1 );
    line = _parse_mteTBlCols( line, entry, 1 );
    line = _parse_mteTThCols( line, entry, 1 );

    /*
     * ... and then read in the "local internal variables"
     *   XXX - TODO
     */
    entry->flags          |= MTE_TRIGGER_FLAG_VALID;

    /* XXX - mte_enable_trigger();  ??? */
    DEBUGMSG(("disman:event:conf", "\n"));
}

/* ===============================================
 *
 *  Handler for storing persistent trigger entries
 *
 * =============================================== */

int
store_mteTTable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char            line[SNMP_MAXBUF];
    char           *cptr, *cp;
    void           *vp;
    size_t          tint;
    netsnmp_tdata_row *row;
    struct mteTrigger  *entry;


    DEBUGMSGTL(("disman:event:conf", "Storing mteTriggerTable config:\n"));

    for (row = netsnmp_tdata_row_first( trigger_table_data );
         row;
         row = netsnmp_tdata_row_next( trigger_table_data, row )) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = (struct mteTrigger *)netsnmp_tdata_row_entry( row );
        if ( entry->flags & MTE_TRIGGER_FLAG_FIXED )
            continue;

        DEBUGMSGTL(("disman:event:conf", "  Storing (%s %s)\n",
                         entry->mteOwner, entry->mteTName));

        /*
         * Save the basic mteTriggerTable entry...
         */
        memset(line, 0, sizeof(line));
        strcat(line, "_mteTTable ");
        cptr = line + strlen(line);

        cp   = entry->mteOwner;          tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->mteTName;          tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->mteTriggerComment; tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        /*
         * ... (but skip the mteTriggerTest and
         *      assorted boolean flag fields)...
         */
        vp   = entry->mteTriggerValueID;
        tint = entry->mteTriggerValueID_len;
        cptr = read_config_store_data(   ASN_OBJECT_ID, cptr, &vp,  &tint );
        cp   = entry->mteTriggerTarget;  tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->mteTriggerContext; tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        tint = entry->mteTriggerFrequency;
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );
        cp   = entry->mteTriggerOOwner;  tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        cp   = entry->mteTriggerObjects; tint = strlen( cp );
        cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
        tint = entry->flags &
                  (MTE_TRIGGER_FLAG_VWILD  |MTE_TRIGGER_FLAG_CWILD|
                   MTE_TRIGGER_FLAG_ENABLED|MTE_TRIGGER_FLAG_ACTIVE);
        cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );
        /* XXX - Need to store the 'iquery' access information */
        snmpd_store_config(line);

        /*
         * ... then save the other (relevant) table entries separately,
         *       starting with mteDeltaDiscontinuityTable...
         */
        if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
            memset(line, 0, sizeof(line));
            strcat(line, "_mteTDTable ");
            cptr = line + strlen(line);

            cp   = entry->mteOwner;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTName;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );

            vp   = entry->mteDeltaDiscontID;
            tint = entry->mteDeltaDiscontID_len;
            cptr = read_config_store_data(   ASN_OBJECT_ID, cptr, &vp,  &tint );

            tint = entry->flags & MTE_TRIGGER_FLAG_DWILD;
            cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );
            tint = entry->mteDeltaDiscontIDType;
            cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

            snmpd_store_config(line);
        }

        /*
         * ... and the three type-specific trigger tables.
         */
        if ( entry->mteTriggerTest & MTE_TRIGGER_EXISTENCE ) {
            memset(line, 0, sizeof(line));
            strcat(line, "_mteTExTable ");
            cptr = line + strlen(line);

            cp   = entry->mteOwner;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTName;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );

            tint = (entry->mteTExTest    & 0xff) << 8;
            tint|= (entry->mteTExStartup & 0xff);
            cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );

            cp   = entry->mteTExObjOwner;  tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTExObjects;   tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            cp   = entry->mteTExEvOwner;   tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTExEvent;     tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            snmpd_store_config(line);
        }
        if ( entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN ) {
            memset(line, 0, sizeof(line));
            strcat(line, "_mteTBlTable ");
            cptr = line + strlen(line);

            cp   = entry->mteOwner;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTName;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );

            tint =  entry->mteTBoolComparison;
            tint |= (entry->flags & MTE_TRIGGER_FLAG_BSTART);
            cptr = read_config_store_data(   ASN_UNSIGNED,  cptr, &tint, NULL );
            tint =  entry->mteTBoolValue;
            cptr = read_config_store_data(   ASN_INTEGER,   cptr, &tint, NULL );

            cp   = entry->mteTBoolObjOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTBoolObjects; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            cp   = entry->mteTBoolEvOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTBoolEvent;   tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            snmpd_store_config(line);
        }
        if ( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) {
            memset(line, 0, sizeof(line));
            strcat(line, "_mteTThTable ");
            cptr = line + strlen(line);

            cp   = entry->mteOwner;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTName;          tint = strlen( cp );
            cptr = read_config_store_data(   ASN_OCTET_STR, cptr, &cp,  &tint );

            cptr = read_config_store_data(ASN_UNSIGNED, cptr,
                                         &entry->mteTThStartup,    NULL );
            cptr = read_config_store_data(ASN_INTEGER,  cptr,
                                         &entry->mteTThRiseValue,  NULL );
            cptr = read_config_store_data(ASN_INTEGER,  cptr,
                                         &entry->mteTThFallValue,  NULL );
            cptr = read_config_store_data(ASN_INTEGER,  cptr,
                                         &entry->mteTThDRiseValue, NULL );
            cptr = read_config_store_data(ASN_INTEGER,  cptr,
                                         &entry->mteTThDFallValue, NULL );

            cp   = entry->mteTThObjOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThObjects; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            cp   = entry->mteTThRiseOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThRiseEvent; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThFallOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThFallEvent; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            cp   = entry->mteTThDRiseOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThDRiseEvent; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThDFallOwner; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );
            cp   = entry->mteTThDFallEvent; tint = strlen( cp );
            cptr = read_config_store_data( ASN_OCTET_STR, cptr, &cp,  &tint );

            snmpd_store_config(line);
        }
    }

    DEBUGMSGTL(("disman:event:conf", "  done.\n"));
    return SNMPERR_SUCCESS;
}

int
clear_mteTTable(int majorID, int minorID, void *serverarg, void *clientarg)
{
    netsnmp_tdata_row *row;
   
    while (( row = netsnmp_tdata_row_first( trigger_table_data ))) {
        struct mteTrigger *entry = (struct mteTrigger *)
            netsnmp_tdata_remove_and_delete_row(trigger_table_data, row);
        if (entry) {
            /* Remove from the callbacks list and disable triggers */
            snmp_unregister_callback( SNMP_CALLBACK_LIBRARY,
                                      SNMP_CALLBACK_POST_READ_CONFIG,
                                      _mteTrigger_callback_enable, entry, 0 ); 
            mteTrigger_disable( entry );
            SNMP_FREE(entry);
        }
    }
    return SNMPERR_SUCCESS;
}
