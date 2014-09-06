/*
 * DisMan Event MIB:
 *     Core implementation of the event handling behaviour
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteEvent.h"
#include "disman/event/mteTrigger.h"
#include "disman/event/mteObjects.h"

netsnmp_feature_child_of(disman_debugging, libnetsnmpmibs)
netsnmp_feature_child_of(mteevent, libnetsnmpmibs)
netsnmp_feature_child_of(mteevent_removeentry, mteevent)

netsnmp_tdata *event_table_data;

    /*
     * Initialize the container for the (combined) mteEvent*Table,
     * regardless of which table initialisation routine is called first.
     */

void
init_event_table_data(void)
{
    DEBUGMSGTL(("disman:event:init", "init event container\n"));
    if (!event_table_data) {
        event_table_data = netsnmp_tdata_create_table("mteEventTable", 0);
        DEBUGMSGTL(("disman:event:init", "create event container (%p)\n",
                                      event_table_data));
    }
}

void _init_default_mteEvent( const char *event, const char *oname, int specific );
void _init_link_mteEvent(    const char *event, const char *oname, int specific );
void _init_builtin_mteEvent( const char *event, const char *oname,
                            oid *trapOID, size_t trapOID_len );


/** Initializes the mteEvent module */
void
init_mteEvent(void)
{
    static int _defaults_init = 0;
    init_event_table_data();

    /*
     * Insert fixed events for the default trigger notifications
     * 
     * NB: internal events (with an owner of "_snmpd") will not in
     * fact refer to the mteObjectsTable for the payload varbinds.
     * The routine mteObjects_internal_vblist() hardcodes the 
     * appropriate varbinds for these internal events.
     *   This routine will need to be updated whenever a new 
     * internal event is added.
     */
    if ( _defaults_init)
        return;

    _init_default_mteEvent( "mteTriggerFired",    "_triggerFire", 1 );
    _init_default_mteEvent( "mteTriggerRising",   "_triggerFire", 2 );
    _init_default_mteEvent( "mteTriggerFalling",  "_triggerFire", 3 );
    _init_default_mteEvent( "mteTriggerFailure",  "_triggerFail", 4 );

    _init_link_mteEvent( "linkDown", "_linkUpDown", 3 );
    _init_link_mteEvent( "linkUp",   "_linkUpDown", 4 );
    _defaults_init = 1;
}

void
_init_builtin_mteEvent( const char *event, const char *oname, oid *trapOID, size_t trapOID_len )
{
    char ename[ MTE_STR1_LEN+1 ];
    netsnmp_tdata_row *row;
    struct mteEvent   *entry;

    memset(ename, 0, sizeof(ename));
    ename[0] = '_';
    memcpy(ename+1, event, strlen(event));

    row = mteEvent_createEntry( "_snmpd", ename, 1 );
    if (!row || !row->data)
        return;
    entry = (struct mteEvent *)row->data;

    entry->mteEventActions     = MTE_EVENT_NOTIFICATION;
    entry->mteNotification_len = trapOID_len;
    memcpy( entry->mteNotification, trapOID, trapOID_len*sizeof(oid));
    memcpy( entry->mteNotifyOwner, "_snmpd", 6 );
    memcpy( entry->mteNotifyObjects,  oname, strlen(oname));
    entry->flags |= MTE_EVENT_FLAG_ENABLED|
                    MTE_EVENT_FLAG_ACTIVE|
                    MTE_EVENT_FLAG_VALID;
}

void
_init_default_mteEvent( const char *event, const char *oname, int specific )
{
    oid    mteTrapOID[]   = {1, 3, 6, 1, 2, 1, 88, 2, 0, 99 /* placeholder */};
    size_t mteTrapOID_len = OID_LENGTH(mteTrapOID);

    mteTrapOID[ mteTrapOID_len-1 ] = specific;
   _init_builtin_mteEvent( event, oname, mteTrapOID, mteTrapOID_len );
}


void
_init_link_mteEvent( const char *event, const char *oname, int specific )
{
    oid    mteTrapOID[]   = {1, 3, 6, 1, 6, 3, 1, 1, 5, 99 /* placeholder */};
    size_t mteTrapOID_len = OID_LENGTH(mteTrapOID);

    mteTrapOID[ mteTrapOID_len-1 ] = specific;
   _init_builtin_mteEvent( event, oname, mteTrapOID, mteTrapOID_len );
}


    /* ===================================================
     *
     * APIs for maintaining the contents of the (combined)
     *    mteEvent*Table container.
     *
     * =================================================== */

#ifndef NETSNMP_FEATURE_REMOVE_DISMAN_DEBUGGING
void
_mteEvent_dump(void)
{
    struct mteEvent *entry;
    netsnmp_tdata_row *row;
    int i = 0;

    for (row = netsnmp_tdata_row_first(event_table_data);
         row;
         row = netsnmp_tdata_row_next(event_table_data, row)) {
        entry = (struct mteEvent *)row->data;
        DEBUGMSGTL(("disman:event:dump", "EventTable entry %d: ", i));
        DEBUGMSGOID(("disman:event:dump", row->oid_index.oids, row->oid_index.len));
        DEBUGMSG(("disman:event:dump", "(%s, %s)",
                                         row->indexes->val.string,
                                         row->indexes->next_variable->val.string));
        DEBUGMSG(("disman:event:dump", ": %p, %p\n", row, entry));
        i++;
    }
    DEBUGMSGTL(("disman:event:dump", "EventTable %d entries\n", i));
}
#endif /* NETSNMP_FEATURE_REMOVE_DISMAN_DEBUGGING */

/*
 * Create a new row in the event table 
 */
netsnmp_tdata_row *
mteEvent_createEntry(const char *mteOwner, const char *mteEName, int fixed)
{
    struct mteEvent *entry;
    netsnmp_tdata_row *row;
    size_t mteOwner_len = (mteOwner) ? strlen(mteOwner) : 0;
    size_t mteEName_len = (mteEName) ? strlen(mteEName) : 0;

    DEBUGMSGTL(("disman:event:table", "Create event entry (%s, %s)\n",
                                       mteOwner, mteEName));
    /*
     * Create the mteEvent entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = SNMP_MALLOC_TYPEDEF(struct mteEvent);
    if (!entry)
        return NULL;

    row = netsnmp_tdata_create_row();
    if (!row) {
        SNMP_FREE(entry);
        return NULL;
    }
    row->data = entry;

    /*
     * ... initialize this row with the indexes supplied
     *     and the default values for the row...
     */
    if (mteOwner)
        memcpy(entry->mteOwner, mteOwner, mteOwner_len);
    netsnmp_table_row_add_index(row, ASN_OCTET_STR,
                                entry->mteOwner, mteOwner_len);
    if (mteEName)
        memcpy(entry->mteEName, mteEName, mteEName_len);
    netsnmp_table_row_add_index(row, ASN_PRIV_IMPLIED_OCTET_STR,
                                entry->mteEName, mteEName_len);

    entry->mteNotification_len = 2;  /* .0.0 */
    if (fixed)
        entry->flags |= MTE_EVENT_FLAG_FIXED;

    /*
     * ... and insert the row into the (common) table container
     */
    netsnmp_tdata_add_row(event_table_data, row);
    DEBUGMSGTL(("disman:event:table", "Event entry created\n"));
    return row;
}


#ifndef NETSNMP_FEATURE_REMOVE_MTEEVENT_REMOVEENTRY
/*
 * Remove a row from the event table 
 */
void
mteEvent_removeEntry(netsnmp_tdata_row *row)
{
    struct mteEvent *entry;

    if (!row)
        return;                 /* Nothing to remove */
    entry = (struct mteEvent *)
        netsnmp_tdata_remove_and_delete_row(event_table_data, row);
    SNMP_FREE(entry);
}
#endif /* NETSNMP_FEATURE_REMOVE_MTEEVENT_REMOVEENTRY */

    /* ===================================================
     *
     * APIs for processing the firing of an event
     *
     * =================================================== */

int
_mteEvent_fire_notify( struct mteEvent    *event,
                       struct mteTrigger  *trigger,
                       oid *suffix, size_t sfx_len );
#ifndef NETSNMP_NO_WRITE_SUPPORT
int
_mteEvent_fire_set(    struct mteEvent    *event,
                       struct mteTrigger  *trigger,
                       oid *suffix, size_t sfx_len );
#endif /* NETSNMP_NO_WRITE_SUPPORT */

int
mteEvent_fire( char *owner, char *event,      /* Event to invoke    */
               struct mteTrigger *trigger,    /* Trigger that fired */
               oid  *suffix, size_t s_len )   /* Matching instance  */
{
    struct mteEvent *entry;
    int fired = 0;
    netsnmp_variable_list owner_var, event_var;

    DEBUGMSGTL(("disman:event:fire", "Event fired (%s, %s)\n",
                                      owner, event));

    /*
     * Retrieve the entry for the specified event
     */
    memset( &owner_var, 0, sizeof(owner_var));
    memset( &event_var, 0, sizeof(event_var));
    snmp_set_var_typed_value(&owner_var, ASN_OCTET_STR, owner, strlen(owner));
    snmp_set_var_typed_value(&event_var, ASN_PRIV_IMPLIED_OCTET_STR,
                                                        event, strlen(event));
    owner_var.next_variable = &event_var;
    entry = (struct mteEvent *)
                netsnmp_tdata_row_entry(
                    netsnmp_tdata_row_get_byidx( event_table_data, &owner_var ));
    if (!entry) {
        DEBUGMSGTL(("disman:event:fire", "No matching event\n"));
        return -1;
    }

    if (entry->mteEventActions & MTE_EVENT_NOTIFICATION) {
        DEBUGMSGTL(("disman:event:fire", "Firing notification event\n"));
        _mteEvent_fire_notify( entry, trigger, suffix, s_len );
        fired = 1;
    }
#ifndef NETSNMP_NO_WRITE_SUPPORT
    if (entry->mteEventActions & MTE_EVENT_SET) {
        DEBUGMSGTL(("disman:event:fire", "Firing set event\n"));
        _mteEvent_fire_set( entry, trigger, suffix, s_len );
        fired = 1;
    }
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    if (!fired)
        DEBUGMSGTL(("disman:event:fire", "Matched event is empty\n"));

    return fired;
}


#ifdef __NOT_NEEDED
void
_insert_internal_objects( netsnmp_variable_list *vblist, char *oname,
                          struct mteTrigger *trigger)
{
    netsnmp_variable_list *var = NULL, *vp;
    oid mteHotTrigger[] = {1, 3, 6, 1, 2, 1, 88, 2, 1, 1, 0};
    oid mteHotTarget[]  = {1, 3, 6, 1, 2, 1, 88, 2, 1, 2, 0};
    oid mteHotContext[] = {1, 3, 6, 1, 2, 1, 88, 2, 1, 3, 0};
    oid mteHotOID[]     = {1, 3, 6, 1, 2, 1, 88, 2, 1, 4, 0};
    oid mteHotValue[]   = {1, 3, 6, 1, 2, 1, 88, 2, 1, 5, 0};

    /*
     * Construct the varbinds for this (internal) event...
     */
    if ((!strcmp(oname, "_mteTriggerFired"  )) ||
        (!strcmp(oname, "_mteTriggerRising" )) ||
        (!strcmp(oname, "_mteTriggerFalling")) ||
        (!strcmp(oname, "_triggerFire"))) {

        snmp_varlist_add_variable( &var,
               mteHotTrigger, OID_LENGTH(mteHotTrigger),
               ASN_OCTET_STR, trigger->mteTName,
                       strlen(trigger->mteTName));
        snmp_varlist_add_variable( &var,
               mteHotTarget,  OID_LENGTH(mteHotTarget),
               ASN_OCTET_STR, trigger->mteTriggerTarget,
                       strlen(trigger->mteTriggerTarget));
        snmp_varlist_add_variable( &var,
               mteHotContext, OID_LENGTH(mteHotContext),
               ASN_OCTET_STR, trigger->mteTriggerContext,
                       strlen(trigger->mteTriggerContext));
        snmp_varlist_add_variable( &var,
               mteHotOID,     OID_LENGTH(mteHotOID),
               ASN_OBJECT_ID, (char *)trigger->mteTriggerFired->name,
                              trigger->mteTriggerFired->name_length*sizeof(oid));
        snmp_varlist_add_variable( &var,
               mteHotValue,   OID_LENGTH(mteHotValue),
                              trigger->mteTriggerFired->type,
                              trigger->mteTriggerFired->val.string,
                              trigger->mteTriggerFired->val_len);
    } else {
        DEBUGMSGTL(("disman:event:fire",
                    "Unknown internal objects tag (%s)\n", oname));
        return;
    }

    /*
     * ... and insert them into the main varbind list
     *     (at the point specified)
     */
    for (vp = var; vp && vp->next_variable; vp=vp->next_variable)
        ;
    vp->next_variable     = vblist->next_variable;
    vblist->next_variable = var;
}
#endif /* __NOT_NEEDED */

int
_mteEvent_fire_notify( struct mteEvent   *entry,     /* The event to fire  */
                       struct mteTrigger *trigger,   /* Trigger that fired */
                       oid *suffix, size_t sfx_len ) /* Matching instance  */
{
    netsnmp_variable_list *var, *v2;
    extern const oid       snmptrap_oid[];
    extern const size_t    snmptrap_oid_len;
    netsnmp_session       *s;

         /*
          * The Event-MIB specification says that objects from the
          *   mteEventTable should come after those from the trigger,
          *   but things actually work better if these come first.
          * Allow the agent to be configured either way.
          */
    int strictOrdering = netsnmp_ds_get_boolean(
                             NETSNMP_DS_APPLICATION_ID,
                             NETSNMP_DS_AGENT_STRICT_DISMAN);

    var = (netsnmp_variable_list *)SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
    if (!var)
        return -1;

    /*
     * Set the basic notification OID...
     */
    memset(var, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_objid( var, snmptrap_oid, snmptrap_oid_len );
    snmp_set_var_typed_value( var, ASN_OBJECT_ID,
                    (u_char *)entry->mteNotification,
                              entry->mteNotification_len*sizeof(oid));

    /*
     * ... then add the specified objects from the Objects Table.
     *
     * Strictly speaking, the objects from the EventTable are meant
     *   to be listed last (after the various trigger objects).
     * But logically things actually work better if the event objects
     *   are placed first.  So this code handles things either way :-)
     */

    if (!strictOrdering) {
        DEBUGMSGTL(("disman:event:fire", "Adding event objects (first)\n"));
        if (strcmp(entry->mteNotifyOwner, "_snmpd") != 0)
            mteObjects_vblist( var, entry->mteNotifyOwner,
                                     entry->mteNotifyObjects,
                                     suffix, sfx_len );
    }

    DEBUGMSGTL(("disman:event:fire", "Adding trigger objects (general)\n"));
    mteObjects_vblist( var, trigger->mteTriggerOOwner,
                             trigger->mteTriggerObjects,
                             suffix, sfx_len );
    DEBUGMSGTL(("disman:event:fire", "Adding trigger objects (specific)\n"));
    mteObjects_vblist( var, trigger->mteTriggerXOwner,
                             trigger->mteTriggerXObjects,
                             suffix, sfx_len );

    if (strictOrdering) {
        DEBUGMSGTL(("disman:event:fire", "Adding event objects (last)\n"));
        if (strcmp(entry->mteNotifyOwner, "_snmpd") != 0)
            mteObjects_vblist( var, entry->mteNotifyOwner,
                                     entry->mteNotifyObjects,
                                     suffix, sfx_len );
    }

    /*
     * Query the agent to retrieve the necessary values...
     *   (skipping the initial snmpTrapOID varbind)
     */
    v2 = var->next_variable;
    if (entry->session)
        s = entry->session;
    else
        s = trigger->session;
    netsnmp_query_get( v2, s );

    /*
     * ... add any "internal" objects...
     * (skipped by the processing above, and best handled directly)
     */
    if (strcmp(entry->mteNotifyOwner, "_snmpd") == 0) {
        DEBUGMSGTL(("disman:event:fire", "Adding event objects (internal)\n"));
        if ( !strictOrdering ) {
            mteObjects_internal_vblist(var, entry->mteNotifyObjects, trigger, s);
        } else {
            for (v2 = var; v2 && v2->next_variable; v2=v2->next_variable)
                ;
            mteObjects_internal_vblist(v2, entry->mteNotifyObjects, trigger, s);
        }
    }

    /*
     * ... and send the resulting varbind list as a notification
     */
    send_v2trap( var );
    snmp_free_varbind( var );
    return 0;
}


#ifndef NETSNMP_NO_WRITE_SUPPORT
int
_mteEvent_fire_set( struct mteEvent   *entry,      /* The event to fire */
                    struct mteTrigger *trigger,    /* Trigger that fired */
                    oid  *suffix, size_t sfx_len ) /* Matching instance */
{
    netsnmp_variable_list var;
    oid    set_oid[ MAX_OID_LEN ];
    size_t set_len;

    /*
     * Set the basic assignment OID...
     */
    memset(set_oid, 0, sizeof(set_oid));
    memcpy(set_oid, entry->mteSetOID, entry->mteSetOID_len*sizeof(oid));
    set_len = entry->mteSetOID_len;

    /*
     * ... if the trigger value is wildcarded (sfx_len > 0),
     *       *and* the SET event entry is wildcarded,
     *        then add the supplied instance suffix...
     */
    if (sfx_len &&
        entry->flags & MTE_SET_FLAG_OBJWILD) {
        memcpy( &set_oid[set_len], suffix, sfx_len*sizeof(oid));
        set_len += sfx_len;
    }

    /*
     * ... finally build the assignment varbind,
     *        and pass it to be acted on.
     *
     * XXX: Need to handle (remote) targets and non-default contexts
     */
    memset( &var, 0, sizeof(var));
    snmp_set_var_objid( &var, set_oid, set_len );
    snmp_set_var_typed_integer( &var, ASN_INTEGER, entry->mteSetValue );
    if (entry->session)
        return netsnmp_query_set( &var, entry->session );
    else
        return netsnmp_query_set( &var, trigger->session );

    /* XXX - Need to check result */
}
#endif /* NETSNMP_NO_WRITE_SUPPORT */
