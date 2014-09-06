/*
 * DisMan Event MIB:
 *     Core implementation of the trigger handling behaviour
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteTrigger.h"
#include "disman/event/mteEvent.h"

netsnmp_feature_child_of(disman_debugging, libnetsnmpmibs)
netsnmp_feature_child_of(mtetrigger, libnetsnmpmibs)
netsnmp_feature_child_of(mtetrigger_removeentry, mtetrigger)

netsnmp_tdata *trigger_table_data;

oid    _sysUpTime_instance[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
size_t _sysUpTime_inst_len   = OID_LENGTH(_sysUpTime_instance);

long mteTriggerFailures;

    /*
     * Initialize the container for the (combined) mteTrigger*Table,
     * regardless of which table initialisation routine is called first.
     */

void
init_trigger_table_data(void)
{
    DEBUGMSGTL(("disman:event:init", "init trigger container\n"));
    if (!trigger_table_data) {
        trigger_table_data = netsnmp_tdata_create_table("mteTriggerTable", 0);
        if (!trigger_table_data) {
            snmp_log(LOG_ERR, "failed to create mteTriggerTable");
            return;
        }
        DEBUGMSGTL(("disman:event:init", "create trigger container (%p)\n",
                                          trigger_table_data));
    }
    mteTriggerFailures = 0;
}


/** Initializes the mteTrigger module */
void
init_mteTrigger(void)
{
    init_trigger_table_data();

}

    /* ===================================================
     *
     * APIs for maintaining the contents of the (combined)
     *    mteTrigger*Table container.
     *
     * =================================================== */

#ifndef NETSNMP_FEATURE_REMOVE_DISMAN_DEBUGGING
void
_mteTrigger_dump(void)
{
    struct mteTrigger *entry;
    netsnmp_tdata_row *row;
    int i = 0;

    for (row = netsnmp_tdata_row_first(trigger_table_data);
         row;
         row = netsnmp_tdata_row_next(trigger_table_data, row)) {
        entry = (struct mteTrigger *)row->data;
        DEBUGMSGTL(("disman:event:dump", "TriggerTable entry %d: ", i));
        DEBUGMSGOID(("disman:event:dump", row->oid_index.oids, row->oid_index.len));
        DEBUGMSG(("disman:event:dump", "(%s, %s)",
                                         row->indexes->val.string,
                                         row->indexes->next_variable->val.string));
        DEBUGMSG(("disman:event:dump", ": %p, %p\n", row, entry));
        i++;
    }
    DEBUGMSGTL(("disman:event:dump", "TriggerTable %d entries\n", i));
}
#endif /* NETSNMP_FEATURE_REMOVE_DISMAN_DEBUGGING */

/*
 * Create a new row in the trigger table 
 */
netsnmp_tdata_row *
mteTrigger_createEntry(const char *mteOwner, char *mteTName, int fixed)
{
    struct mteTrigger *entry;
    netsnmp_tdata_row *row;
    size_t mteOwner_len = (mteOwner) ? strlen(mteOwner) : 0;
    size_t mteTName_len = (mteTName) ? strlen(mteTName) : 0;

    DEBUGMSGTL(("disman:event:table", "Create trigger entry (%s, %s)\n",
                                       mteOwner, mteTName));
    /*
     * Create the mteTrigger entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = SNMP_MALLOC_TYPEDEF(struct mteTrigger);
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
    if (mteTName)
        memcpy(entry->mteTName, mteTName, mteTName_len);
    netsnmp_table_row_add_index(row, ASN_PRIV_IMPLIED_OCTET_STR,
                                entry->mteTName, mteTName_len);

  /* entry->mteTriggerTest         = MTE_TRIGGER_BOOLEAN; */
    entry->mteTriggerValueID_len  = 2;  /* .0.0 */
    entry->mteTriggerFrequency    = 600;
    memcpy(entry->mteDeltaDiscontID, _sysUpTime_instance,
                              sizeof(_sysUpTime_instance));
    entry->mteDeltaDiscontID_len  =  _sysUpTime_inst_len;
    entry->mteDeltaDiscontIDType  = MTE_DELTAD_TTICKS;
    entry->flags                 |= MTE_TRIGGER_FLAG_SYSUPT;
    entry->mteTExTest             = (MTE_EXIST_PRESENT | MTE_EXIST_ABSENT);
    entry->mteTExStartup          = (MTE_EXIST_PRESENT | MTE_EXIST_ABSENT);
    entry->mteTBoolComparison     = MTE_BOOL_UNEQUAL;
    entry->flags                 |= MTE_TRIGGER_FLAG_BSTART;
    entry->mteTThStartup          = MTE_THRESH_START_RISEFALL;

    if (fixed)
        entry->flags |= MTE_TRIGGER_FLAG_FIXED;

    /*
     * ... and insert the row into the (common) table container
     */
    netsnmp_tdata_add_row(trigger_table_data, row);
    DEBUGMSGTL(("disman:event:table", "Trigger entry created\n"));
    return row;
}

#ifndef NETSNMP_FEATURE_REMOVE_MTETRIGGER_REMOVEENTRY
/*
 * Remove a row from the trigger table 
 */
void
mteTrigger_removeEntry(netsnmp_tdata_row *row)
{
    struct mteTrigger *entry;

    if (!row)
        return;                 /* Nothing to remove */
    entry = (struct mteTrigger *)
        netsnmp_tdata_remove_and_delete_row(trigger_table_data, row);
    if (entry) {
        mteTrigger_disable( entry );
        SNMP_FREE(entry);
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_MTETRIGGER_REMOVEENTRY */

    /* ===================================================
     *
     * APIs for evaluating a trigger,
     *   and firing the appropriate event
     *
     * =================================================== */
const char *_ops[] = { "",
                "!=", /* MTE_BOOL_UNEQUAL      */
                "==", /* MTE_BOOL_EQUAL        */
                "<",  /* MTE_BOOL_LESS         */
                "<=", /* MTE_BOOL_LESSEQUAL    */
                ">",  /* MTE_BOOL_GREATER      */
                ">="  /* MTE_BOOL_GREATEREQUAL */  };

void
_mteTrigger_failure( /* int error, */ const char *msg )
{
    /*
     * XXX - Send an mteTriggerFailure trap
     *           (if configured to do so)
     */
    mteTriggerFailures++;
    snmp_log(LOG_ERR, "%s\n", msg );
    return;
}

void
mteTrigger_run( unsigned int reg, void *clientarg)
{
    struct mteTrigger *entry = (struct mteTrigger *)clientarg;
    netsnmp_variable_list *var, *vtmp;
    netsnmp_variable_list *vp1, *vp1_prev;
    netsnmp_variable_list *vp2, *vp2_prev;
    netsnmp_variable_list *dvar = NULL;
    netsnmp_variable_list *dv1  = NULL, *dv2 = NULL;
    netsnmp_variable_list sysUT_var;
    int  cmp = 0, n, n2;
    long value;
    const char *reason;

    if (!entry) {
        snmp_alarm_unregister( reg );
        return;
    }
    if (!(entry->flags & MTE_TRIGGER_FLAG_ENABLED ) ||
        !(entry->flags & MTE_TRIGGER_FLAG_ACTIVE  ) ||
        !(entry->flags & MTE_TRIGGER_FLAG_VALID  )) {
        return;
    }

    {
	extern netsnmp_agent_session *netsnmp_processing_set;
	if (netsnmp_processing_set) {
	    /*
	     * netsnmp_handle_request will not be responsive to our efforts to
	     *	Retrieve the requested MIB value(s)...
	     * so we will skip it.
	     * https://sourceforge.net/tracker/
	     *	index.php?func=detail&aid=1557406&group_id=12694&atid=112694
	     */
	    DEBUGMSGTL(("disman:event:trigger:monitor",
		"Skipping trigger (%s) while netsnmp_processing_set\n",
		entry->mteTName));
	    return;
	}
    }

    /*
     * Retrieve the requested MIB value(s)...
     */
    DEBUGMSGTL(( "disman:event:trigger:monitor", "Running trigger (%s)\n", entry->mteTName));
    var = (netsnmp_variable_list *)SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
    if (!var) {
        _mteTrigger_failure("failed to create mteTrigger query varbind");
        return;
    }
    snmp_set_var_objid( var, entry->mteTriggerValueID,
                             entry->mteTriggerValueID_len );
    if ( entry->flags & MTE_TRIGGER_FLAG_VWILD ) {
        n = netsnmp_query_walk( var, entry->session );
    } else {
        n = netsnmp_query_get(  var, entry->session );
    }
    if ( n != SNMP_ERR_NOERROR ) {
        DEBUGMSGTL(( "disman:event:trigger:monitor", "Trigger query (%s) failed: %d\n",
                           (( entry->flags & MTE_TRIGGER_FLAG_VWILD ) ? "walk" : "get"), n));
        _mteTrigger_failure( "failed to run mteTrigger query" );
        snmp_free_varbind(var);
        return;
    }

    /*
     * ... canonicalise the results (to simplify later comparisons)...
     */

    vp1 = var;                vp1_prev = NULL;
    vp2 = entry->old_results; vp2_prev = NULL;
    entry->count=0;
    while (vp1) {
           /*
            * Flatten various missing values/exceptions into a single form
            */ 
	switch (vp1->type) {
        case SNMP_NOSUCHINSTANCE:
        case SNMP_NOSUCHOBJECT:
        case ASN_PRIV_RETRY:   /* Internal only ? */
            vp1->type = ASN_NULL;
        }
           /*
            * Keep track of how many entries have been retrieved.
            */ 
           entry->count++;
        
           /*
            * Ensure previous and current result match
            *  (with corresponding entries in both lists)
            * and set the flags indicating which triggers are armed
            */ 
        if (vp2) {
            cmp = snmp_oid_compare(vp1->name, vp1->name_length,
                                   vp2->name, vp2->name_length);
            if ( cmp < 0 ) {
                /*
                 * If a new value has appeared, insert a matching
                 * dummy entry into the previous result list.
                 *
                 * XXX - check how this is best done.
                 */
                vtmp = SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                if (!vtmp) {
                    _mteTrigger_failure(
                          "failed to create mteTrigger temp varbind");
                    snmp_free_varbind(var);
                    return;
                }
                vtmp->type = ASN_NULL;
                snmp_set_var_objid( vtmp, vp1->name, vp1->name_length );
                vtmp->next_variable = vp2;
                if (vp2_prev) {
                    vp2_prev->next_variable = vtmp;
                } else {
                    entry->old_results      = vtmp;
                }
                vp2_prev   = vtmp;
                vp1->index = MTE_ARMED_ALL;	/* XXX - plus a new flag */
                vp1_prev   = vp1;
                vp1        = vp1->next_variable;
            }
            else if ( cmp == 0 ) {
                /*
                 * If it's a continuing entry, just copy across the armed flags
                 */
                vp1->index = vp2->index;
                vp1_prev   = vp1;
                vp1        = vp1->next_variable;
                vp2_prev   = vp2;
                vp2        = vp2->next_variable;
            } else {
                /*
                 * If a value has just disappeared, insert a
                 * matching dummy entry into the current result list.
                 *
                 * XXX - check how this is best done.
                 *
                 */
                if ( vp2->type != ASN_NULL ) {
                    vtmp = SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                    if (!vtmp) {
                        _mteTrigger_failure(
                                 "failed to create mteTrigger temp varbind");
                        snmp_free_varbind(var);
                        return;
                    }
                    vtmp->type = ASN_NULL;
                    snmp_set_var_objid( vtmp, vp2->name, vp2->name_length );
                    vtmp->next_variable = vp1;
                    if (vp1_prev) {
                        vp1_prev->next_variable = vtmp;
                    } else {
                        var                     = vtmp;
                    }
                    vp1_prev = vtmp;
                    vp2_prev = vp2;
                    vp2      = vp2->next_variable;
                } else {
                    /*
                     * But only if this entry has *just* disappeared.  If the
                     * entry from the last run was a dummy too, then remove it.
                     *   (leaving vp2_prev unchanged)
                     */
                    vtmp = vp2;
                    if (vp2_prev) {
                        vp2_prev->next_variable = vp2->next_variable;
                    } else {
                        entry->old_results      = vp2->next_variable;
                    }
                    vp2  = vp2->next_variable;
                    vtmp->next_variable = NULL;
                    snmp_free_varbind( vtmp );
                }
            }
        } else {
            /*
             * No more old results to compare.
             * Either all remaining values have only just been created ...
             *   (and we need to create dummy 'old' entries for them)
             */
            if ( vp2_prev ) {
                vtmp = SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                if (!vtmp) {
                    _mteTrigger_failure(
                             "failed to create mteTrigger temp varbind");
                    snmp_free_varbind(var);
                    return;
                }
                vtmp->type = ASN_NULL;
                snmp_set_var_objid( vtmp, vp1->name, vp1->name_length );
                vtmp->next_variable     = vp2_prev->next_variable;
                vp2_prev->next_variable = vtmp;
                vp2_prev                = vtmp;
            }
            /*
             * ... or this is the first run through
             *   (and there were no old results at all)
             *
             * In either case, mark the current entry as armed and new.
             * Note that we no longer need to maintain 'vp1_prev'
             */
            vp1->index = MTE_ARMED_ALL;	/* XXX - plus a new flag */
            vp1        = vp1->next_variable;
        }
    }

    /*
     * ... and then work through these result(s), deciding
     *     whether or not to trigger the corresponding event.
     *
     *  Note that there's no point in evaluating Existence or
     *    Boolean tests if there's no corresponding event.
     *   (Even if the trigger matched, nothing would be done anyway).
     */
    if ((entry->mteTriggerTest & MTE_TRIGGER_EXISTENCE) &&
        (entry->mteTExEvent[0] != '\0' )) {
        /*
         * If we don't have a record of previous results,
         * this must be the first time through, so consider
         * the mteTriggerExistenceStartup tests.
         */
        if ( !entry->old_results ) {
            /*
             * With the 'present(0)' test, the trigger should fire
             *   for each value in the varbind list returned
             *   (whether the monitored value is wildcarded or not).
             */
            if (entry->mteTExTest & entry->mteTExStartup & MTE_EXIST_PRESENT) {
                for (vp1 = var; vp1; vp1=vp1->next_variable) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing initial existence test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire",
                                 " (present)\n"));;
                    entry->mteTriggerXOwner   = entry->mteTExObjOwner;
                    entry->mteTriggerXObjects = entry->mteTExObjects;
                    entry->mteTriggerFired    = vp1;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire(entry->mteTExEvOwner, entry->mteTExEvent, 
                                  entry, vp1->name+n, vp1->name_length-n);
                }
            }
            /*
             * An initial 'absent(1)' test only makes sense when
             *   monitoring a non-wildcarded OID (how would we know
             *   which rows of the table "ought" to exist, but don't?)
             */
            if (entry->mteTExTest & entry->mteTExStartup & MTE_EXIST_ABSENT) {
                if (!(entry->flags & MTE_TRIGGER_FLAG_VWILD) &&
                    var->type == ASN_NULL ) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing initial existence test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 var->name, var->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire",
                                 " (absent)\n"));;
                    entry->mteTriggerXOwner   = entry->mteTExObjOwner;
                    entry->mteTriggerXObjects = entry->mteTExObjects;
                    /*
                     * It's unclear what value the 'mteHotValue' payload
                     *  should take when a monitored instance does not
                     *  exist on startup. The only sensible option is
                     *  to report a NULL value, but this clashes with
                     * the syntax of the mteHotValue MIB object.
                     */
                    entry->mteTriggerFired    = var;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire(entry->mteTExEvOwner, entry->mteTExEvent, 
                                  entry, var->name+n, var->name_length-n);
                }
            }
        } /* !old_results */
            /*
             * Otherwise, compare the current set of results with
             * the previous ones, looking for changes.  We can
             * assume that the two lists match (see above).
             */
        else {
            for (vp1 = var, vp2 = entry->old_results;
                 vp1;
                 vp1=vp1->next_variable, vp2=vp2->next_variable) {

                /* Use this field to indicate that the trigger should fire */
                entry->mteTriggerFired = NULL;
                reason                 = NULL;

                if ((entry->mteTExTest & MTE_EXIST_PRESENT) &&
                    (vp1->type != ASN_NULL) &&
                    (vp2->type == ASN_NULL)) {
                    /* A new instance has appeared */
                    entry->mteTriggerFired = vp1;
                    reason = "(present)";

                } else if ((entry->mteTExTest & MTE_EXIST_ABSENT) &&
                    (vp1->type == ASN_NULL) &&
                    (vp2->type != ASN_NULL)) {

                    /*
                     * A previous instance has disappeared.
                     *
                     * It's unclear what value the 'mteHotValue' payload
                     *  should take when this happens - the previous
                     *  value (vp2), or a NULL value (vp1) ?
                     * NULL makes more sense logically, but clashes
                     *  with the syntax of the mteHotValue MIB object.
                     */
                    entry->mteTriggerFired = vp2;
                    reason = "(absent)";

                } else if ((entry->mteTExTest & MTE_EXIST_CHANGED) &&
                    ((vp1->val_len != vp2->val_len) || 
                     (memcmp( vp1->val.string, vp2->val.string,
                              vp1->val_len) != 0 ))) {
                    /*
                     * This comparison detects changes in *any* type
                     *  of value, numeric or string (or even OID).
                     *
                     * Unfortunately, the default 'mteTriggerFired'
                     *  notification payload can't report non-numeric
                     *  changes properly (see syntax of 'mteHotValue')
                     */
                    entry->mteTriggerFired = vp1;
                    reason = "(changed)";
                }
                if ( entry->mteTriggerFired ) {
                    /*
                     * One of the above tests has matched,
                     *   so fire the trigger.
                     */
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing existence test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire",
                                 " %s\n", reason));;
                    entry->mteTriggerXOwner   = entry->mteTExObjOwner;
                    entry->mteTriggerXObjects = entry->mteTExObjects;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire(entry->mteTExEvOwner, entry->mteTExEvent, 
                                  entry, vp1->name+n, vp1->name_length-n);
                }
            }
        } /* !old_results - end of else block */
    } /* MTE_TRIGGER_EXISTENCE */


    if (( entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN   ) ||
        ( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD )) {
        /*
         * Although Existence tests can work with any syntax values,
         * Boolean and Threshold tests are integer-only.  Ensure that
         * the returned value(s) are appropriate.
         *
         * Note that we only need to check the first value, since all
         *  instances of a given object should have the same syntax.
         */
        switch (var->type) {
        case ASN_INTEGER:
        case ASN_COUNTER:
        case ASN_GAUGE:
        case ASN_TIMETICKS:
        case ASN_UINTEGER:
        case ASN_COUNTER64:
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_COUNTER64:
        case ASN_OPAQUE_U64:
        case ASN_OPAQUE_I64:
#endif
            /* OK */
            break;
        default:
            /*
             * Other syntax values can't be used for Boolean/Theshold
             * tests. Report this as an error, and then rotate the
             * results ready for the next run, (which will presumably
             * also detect this as an error once again!)
             */
            DEBUGMSGTL(( "disman:event:trigger:fire",
                         "Returned non-integer result(s): "));
            DEBUGMSGOID(("disman:event:trigger:fire",
                         var->name, var->name_length));
            DEBUGMSG((   "disman:event:trigger:fire",
                         " (boolean/threshold) %d\n", var->type));;
            snmp_free_varbind( entry->old_results );
            entry->old_results = var;
            return;
        }


        /*
         * Retrieve the discontinuity markers for delta-valued samples.
         * (including sysUpTime.0 if not specified explicitly).
         */
        if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
            /*
             * We'll need sysUpTime.0 regardless...
             */
            DEBUGMSGTL(("disman:event:delta", "retrieve sysUpTime.0\n"));
            memset( &sysUT_var, 0, sizeof( netsnmp_variable_list ));
            snmp_set_var_objid( &sysUT_var, _sysUpTime_instance,
                                            _sysUpTime_inst_len );
            netsnmp_query_get(  &sysUT_var, entry->session );

            if (!(entry->flags & MTE_TRIGGER_FLAG_SYSUPT)) {
                /*
                 * ... but only retrieve the configured discontinuity
                 *      marker(s) if they refer to something different.
                 */
                DEBUGMSGTL(( "disman:event:delta",
                             "retrieve discontinuity marker(s): "));
                DEBUGMSGOID(("disman:event:delta", entry->mteDeltaDiscontID,
                                               entry->mteDeltaDiscontID_len ));
                DEBUGMSG((   "disman:event:delta", " %s\n", 
                     (entry->flags & MTE_TRIGGER_FLAG_DWILD ? " (wild)" : "")));

                dvar = (netsnmp_variable_list *)
                                SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                if (!dvar) {
                    _mteTrigger_failure(
                            "failed to create mteTrigger delta query varbind");
                    return;
                }
                snmp_set_var_objid( dvar, entry->mteDeltaDiscontID,
                                          entry->mteDeltaDiscontID_len );
                if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                    n = netsnmp_query_walk( dvar, entry->session );
                } else {
                    n = netsnmp_query_get(  dvar, entry->session );
                }
                if ( n != SNMP_ERR_NOERROR ) {
                    _mteTrigger_failure( "failed to run mteTrigger delta query" );
                    snmp_free_varbind( dvar );
                    return;
                }
            }

            /*
             * We can't calculate delta values the first time through,
             *  so there's no point in evaluating the remaining tests.
             *
             * Save the results (and discontinuity markers),
             *   ready for the next run.
             */
            if ( !entry->old_results ) {
                entry->old_results =  var;
                entry->old_deltaDs = dvar;
                entry->sysUpTime   = *sysUT_var.val.integer;
                return;
            }
            /*
             * If the sysUpTime marker has been reset (or strictly,
             *   has advanced by less than the monitor frequency),
             *  there's no point in trying the remaining tests.
             */

            if (*sysUT_var.val.integer < entry->sysUpTime) {
                DEBUGMSGTL(( "disman:event:delta",
                             "single discontinuity: (sysUT)\n"));
                snmp_free_varbind( entry->old_results );
                snmp_free_varbind( entry->old_deltaDs );
                entry->old_results =  var;
                entry->old_deltaDs = dvar;
                entry->sysUpTime   = *sysUT_var.val.integer;
                return;
            }
            /*
             * Similarly if a separate (non-wildcarded) discontinuity
             *  marker has changed, then there's no
             *  point in trying to evaluate these tests either.
             */
            if (!(entry->flags & MTE_TRIGGER_FLAG_DWILD)  &&
                !(entry->flags & MTE_TRIGGER_FLAG_SYSUPT) &&
                  (!entry->old_deltaDs ||
                   (entry->old_deltaDs->val.integer != dvar->val.integer))) {
                DEBUGMSGTL((  "disman:event:delta", "single discontinuity: ("));
                DEBUGMSGOID(( "disman:event:delta", entry->mteDeltaDiscontID,
                                           entry->mteDeltaDiscontID_len));
                DEBUGMSG((    "disman:event:delta", ")\n"));
                snmp_free_varbind( entry->old_results );
                snmp_free_varbind( entry->old_deltaDs );
                entry->old_results =  var;
                entry->old_deltaDs = dvar;
                entry->sysUpTime   = *sysUT_var.val.integer;
                return;
            }

            /*
             * Ensure that the list of (wildcarded) discontinuity 
             *  markers matches the list of monitored values
             *  (inserting/removing discontinuity varbinds as needed)
             *
             * XXX - An alternative approach would be to use the list
             *    of monitored values (instance subidentifiers) to build
             *    the exact list of delta markers to retrieve earlier.
             */
            if (entry->flags & MTE_TRIGGER_FLAG_DWILD) {
                vp1      =  var;
                vp2      = dvar;
                vp2_prev = NULL;
                n  = entry->mteTriggerValueID_len;
                n2 = entry->mteDeltaDiscontID_len;
                while (vp1) {
                    /*
                     * For each monitored instance, check whether
                     *   there's a matching discontinuity entry.
                     */
                    cmp = snmp_oid_compare(vp1->name+n,  vp1->name_length-n,
                                           vp2->name+n2, vp2->name_length-n2 );
                    if ( cmp < 0 ) {
                        /*
                         * If a discontinuity entry is missing,
                         *   insert a (dummy) varbind.
                         * The corresponding delta calculation will
                         *   fail, but this simplifies the later code.
                         */
                        vtmp = (netsnmp_variable_list *)
                                SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                        if (!vtmp) {
                            _mteTrigger_failure(
                                  "failed to create mteTrigger discontinuity varbind");
                            snmp_free_varbind(dvar);
                            return;
                        }
                        snmp_set_var_objid(vtmp, entry->mteDeltaDiscontID,
                                                 entry->mteDeltaDiscontID_len);
                            /* XXX - append instance subids */
                        vtmp->next_variable     = vp2;
                        vp2_prev->next_variable = vtmp;
                        vp2_prev                = vtmp;
                        vp1 = vp1->next_variable;
                    } else if ( cmp == 0 ) {
                        /*
                         * Matching discontinuity entry -  all OK.
                         */
                        vp2_prev = vp2;
                        vp2      = vp2->next_variable;
                        vp1      = vp1->next_variable;
                    } else {
                        /*
                         * Remove unneeded discontinuity entry
                         */
                        vtmp = vp2;
                        vp2_prev->next_variable = vp2->next_variable;
                        vp2                     = vp2->next_variable;
                        vtmp->next_variable = NULL;
                        snmp_free_varbind( vtmp );
                    }
                }
                /*
                 * XXX - Now need to ensure that the old list of
                 *   delta discontinuity markers matches as well.
                 */
            }
        } /* delta samples */
    } /* Boolean/Threshold test checks */



    /*
     * Only run the Boolean tests if there's an event to be triggered
     */
    if ((entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN) &&
        (entry->mteTBoolEvent[0] != '\0' )) {

        if (entry->flags & MTE_TRIGGER_FLAG_DELTA) {
            vp2 = entry->old_results;
            if (entry->flags & MTE_TRIGGER_FLAG_DWILD) {
                dv1 = dvar;
                dv2 = entry->old_deltaDs;
            }
        }
        for ( vp1 = var; vp1; vp1=vp1->next_variable ) {
            /*
             * Determine the value to be monitored...
             */
            if ( !vp1->val.integer ) {  /* No value */
                if ( vp2 )
                    vp2 = vp2->next_variable;
                continue;
            }
            if (entry->flags & MTE_TRIGGER_FLAG_DELTA) {
                if (entry->flags & MTE_TRIGGER_FLAG_DWILD) {
                    /*
                     * We've already checked any non-wildcarded
                     *   discontinuity markers (inc. sysUpTime.0).
                     * Validate this particular sample against
                     *   the relevant wildcarded marker...
                     */
                    if ((dv1->type == ASN_NULL)  ||
                        (dv1->type != dv2->type) ||
                        (*dv1->val.integer != *dv2->val.integer)) {
                        /*
                         * Bogus or changed discontinuity marker.
                         * Need to skip this sample.
                         */
    DEBUGMSGTL(( "disman:event:delta", "discontinuity occurred: "));
    DEBUGMSGOID(("disman:event:delta", vp1->name,
                                       vp1->name_length ));
    DEBUGMSG((   "disman:event:delta", " \n" ));
                        vp2 = vp2->next_variable;
                        continue;
                    }
                }
                /*
                 * ... and check there is a previous sample to calculate
                 *   the delta value against (regardless of whether the
                 *   discontinuity marker was wildcarded or not).
                 */
                if (vp2->type == ASN_NULL) {
    DEBUGMSGTL(( "disman:event:delta", "missing sample: "));
    DEBUGMSGOID(("disman:event:delta", vp1->name,
                                       vp1->name_length ));
    DEBUGMSG((   "disman:event:delta", " \n" ));
                    vp2 = vp2->next_variable;
                    continue;
                }
                value = (*vp1->val.integer - *vp2->val.integer);
    DEBUGMSGTL(( "disman:event:delta", "delta sample: "));
    DEBUGMSGOID(("disman:event:delta", vp1->name,
                                       vp1->name_length ));
    DEBUGMSG((   "disman:event:delta", " (%ld - %ld) = %ld\n",
                *vp1->val.integer,  *vp2->val.integer, value));
                vp2 = vp2->next_variable;
            } else {
                value = *vp1->val.integer;
            }

            /*
             * ... evaluate the comparison ...
             */
            switch (entry->mteTBoolComparison) {
            case MTE_BOOL_UNEQUAL:
                cmp = ( value != entry->mteTBoolValue );
                break;
            case MTE_BOOL_EQUAL:
                cmp = ( value == entry->mteTBoolValue );
                break;
            case MTE_BOOL_LESS:
                cmp = ( value <  entry->mteTBoolValue );
                break;
            case MTE_BOOL_LESSEQUAL:
                cmp = ( value <= entry->mteTBoolValue );
                break;
            case MTE_BOOL_GREATER:
                cmp = ( value >  entry->mteTBoolValue );
                break;
            case MTE_BOOL_GREATEREQUAL:
                cmp = ( value >= entry->mteTBoolValue );
                break;
            }
    DEBUGMSGTL(( "disman:event:delta", "Bool comparison: (%ld %s %ld) %d\n",
                          value, _ops[entry->mteTBoolComparison],
                          entry->mteTBoolValue, cmp));

            /*
             * ... and decide whether to trigger the event.
             *    (using the 'index' field of the varbind structure
             *     to remember whether the trigger has already fired)
             */
            if ( cmp ) {
              if (vp1->index & MTE_ARMED_BOOLEAN ) {
                vp1->index &= ~MTE_ARMED_BOOLEAN;
                /*
                 * NB: Clear the trigger armed flag even if the
                 *   (starting) event dosn't actually fire.
                 *   Otherwise initially true (but suppressed)
                 *   triggers will fire on the *second* probe.
                 */
                if ( entry->old_results ||
                    (entry->flags & MTE_TRIGGER_FLAG_BSTART)) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing boolean test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire", "%s\n",
                                  (entry->old_results ? "" : " (startup)")));
                    entry->mteTriggerXOwner   = entry->mteTBoolObjOwner;
                    entry->mteTriggerXObjects = entry->mteTBoolObjects;
                    /*
                     * XXX - when firing a delta-based trigger, should
                     *   'mteHotValue' report the actual value sampled
                     *   (as here), or the delta that triggered the event ?
                     */
                    entry->mteTriggerFired    = vp1;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire(entry->mteTBoolEvOwner, entry->mteTBoolEvent, 
                                  entry, vp1->name+n, vp1->name_length-n);
                }
              }
            } else {
                vp1->index |= MTE_ARMED_BOOLEAN;
            }
        }
    }


    /*
     * Only run the basic threshold tests if there's an event to
     *    be triggered.  (Either rising or falling will do)
     */
    if (( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) &&
        ((entry->mteTThRiseEvent[0] != '\0' ) ||
         (entry->mteTThFallEvent[0] != '\0' ))) {

        /*
         * The same delta-sample validation from Boolean
         *   tests also applies here too.
         */
        if (entry->flags & MTE_TRIGGER_FLAG_DELTA) {
            vp2 = entry->old_results;
            if (entry->flags & MTE_TRIGGER_FLAG_DWILD) {
                dv1 = dvar;
                dv2 = entry->old_deltaDs;
            }
        }
        for ( vp1 = var; vp1; vp1=vp1->next_variable ) {
            /*
             * Determine the value to be monitored...
             */
            if ( !vp1->val.integer ) {  /* No value */
                if ( vp2 )
                    vp2 = vp2->next_variable;
                continue;
            }
            if (entry->flags & MTE_TRIGGER_FLAG_DELTA) {
                if (entry->flags & MTE_TRIGGER_FLAG_DWILD) {
                    /*
                     * We've already checked any non-wildcarded
                     *   discontinuity markers (inc. sysUpTime.0).
                     * Validate this particular sample against
                     *   the relevant wildcarded marker...
                     */
                    if ((dv1->type == ASN_NULL)  ||
                        (dv1->type != dv2->type) ||
                        (*dv1->val.integer != *dv2->val.integer)) {
                        /*
                         * Bogus or changed discontinuity marker.
                         * Need to skip this sample.
                         */
                        vp2 = vp2->next_variable;
                        continue;
                    }
                }
                /*
                 * ... and check there is a previous sample to calculate
                 *   the delta value against (regardless of whether the
                 *   discontinuity marker was wildcarded or not).
                 */
                if (vp2->type == ASN_NULL) {
                    vp2 = vp2->next_variable;
                    continue;
                }
                value = (*vp1->val.integer - *vp2->val.integer);
                vp2 = vp2->next_variable;
            } else {
                value = *vp1->val.integer;
            }

            /*
             * ... evaluate the single-value comparisons,
             *     and decide whether to trigger the event.
             */
            cmp = vp1->index;   /* working copy of 'armed' flags */
            if ( value >= entry->mteTThRiseValue ) {
              if (cmp & MTE_ARMED_TH_RISE ) {
                cmp &= ~MTE_ARMED_TH_RISE;
                cmp |=  MTE_ARMED_TH_FALL;
                /*
                 * NB: Clear the trigger armed flag even if the
                 *   (starting) event dosn't actually fire.
                 *   Otherwise initially true (but suppressed)
                 *   triggers will fire on the *second* probe.
                 * Similarly for falling thresholds (see below).
                 */
                if ( entry->old_results ||
                    (entry->mteTThStartup & MTE_THRESH_START_RISE)) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing rising threshold test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire", "%s\n",
                                 (entry->old_results ? "" : " (startup)")));
                    /*
                     * If no riseEvent is configured, we need still to
                     *  set the armed flags appropriately, but there's
                     *  no point in trying to fire the (missing) event.
                     */
                    if (entry->mteTThRiseEvent[0] != '\0' ) {
                        entry->mteTriggerXOwner   = entry->mteTThObjOwner;
                        entry->mteTriggerXObjects = entry->mteTThObjects;
                        entry->mteTriggerFired    = vp1;
                        n = entry->mteTriggerValueID_len;
                        mteEvent_fire(entry->mteTThRiseOwner,
                                      entry->mteTThRiseEvent, 
                                      entry, vp1->name+n, vp1->name_length-n);
                    }
                }
              }
            }

            if ( value <= entry->mteTThFallValue ) {
              if (cmp & MTE_ARMED_TH_FALL ) {
                cmp &= ~MTE_ARMED_TH_FALL;
                cmp |=  MTE_ARMED_TH_RISE;
                /* Clear the trigger armed flag (see above) */
                if ( entry->old_results ||
                    (entry->mteTThStartup & MTE_THRESH_START_FALL)) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing falling threshold test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire", "%s\n",
                                 (entry->old_results ? "" : " (startup)")));
                    /*
                     * Similarly, if no fallEvent is configured,
                     *  there's no point in trying to fire it either.
                     */
                    if (entry->mteTThRiseEvent[0] != '\0' ) {
                        entry->mteTriggerXOwner   = entry->mteTThObjOwner;
                        entry->mteTriggerXObjects = entry->mteTThObjects;
                        entry->mteTriggerFired    = vp1;
                        n = entry->mteTriggerValueID_len;
                        mteEvent_fire(entry->mteTThFallOwner,
                                      entry->mteTThFallEvent, 
                                      entry, vp1->name+n, vp1->name_length-n);
                    }
                }
              }
            }
            vp1->index = cmp;
        }
    }

    /*
     * The same processing also works for delta-threshold tests (if configured)
     */
    if (( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) &&
        ((entry->mteTThDRiseEvent[0] != '\0' ) ||
         (entry->mteTThDFallEvent[0] != '\0' ))) {

        /*
         * Delta-threshold tests can only be used with
         *   absolute valued samples.
         */
        vp2 = entry->old_results;
        if (entry->flags & MTE_TRIGGER_FLAG_DELTA) {
            DEBUGMSGTL(( "disman:event:trigger",
                         "Delta-threshold on delta-sample\n"));
        } else if ( vp2 != NULL ) {
          for ( vp1 = var; vp1; vp1=vp1->next_variable ) {
            /*
             * Determine the value to be monitored...
             *  (similar to previous delta-sample processing,
             *   but without the discontinuity marker checks)
             */
            if (!vp2) {
                break;   /* Run out of 'old' values */
            }
            if (( !vp1->val.integer ) ||
                (vp2->type == ASN_NULL)) {
                vp2 = vp2->next_variable;
                continue;
            }
            value = (*vp1->val.integer - *vp2->val.integer);
            vp2 = vp2->next_variable;

            /*
             * ... evaluate the single-value comparisons,
             *     and decide whether to trigger the event.
             */
            cmp = vp1->index;   /* working copy of 'armed' flags */
            if ( value >= entry->mteTThDRiseValue ) {
                if (vp1->index & MTE_ARMED_TH_DRISE ) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing rising delta threshold test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire", "\n"));
                    cmp &= ~MTE_ARMED_TH_DRISE;
                    cmp |=  MTE_ARMED_TH_DFALL;
                    /*
                     * If no riseEvent is configured, we need still to
                     *  set the armed flags appropriately, but there's
                     *  no point in trying to fire the (missing) event.
                     */
                    if (entry->mteTThDRiseEvent[0] != '\0' ) {
                        entry->mteTriggerXOwner   = entry->mteTThObjOwner;
                        entry->mteTriggerXObjects = entry->mteTThObjects;
                        entry->mteTriggerFired    = vp1;
                        n = entry->mteTriggerValueID_len;
                        mteEvent_fire(entry->mteTThDRiseOwner,
                                      entry->mteTThDRiseEvent, 
                                      entry, vp1->name+n, vp1->name_length-n);
                    }
                }
            }

            if ( value <= entry->mteTThDFallValue ) {
                if (vp1->index & MTE_ARMED_TH_DFALL ) {
                    DEBUGMSGTL(( "disman:event:trigger:fire",
                                 "Firing falling delta threshold test: "));
                    DEBUGMSGOID(("disman:event:trigger:fire",
                                 vp1->name, vp1->name_length));
                    DEBUGMSG((   "disman:event:trigger:fire", "\n"));
                    cmp &= ~MTE_ARMED_TH_DFALL;
                    cmp |=  MTE_ARMED_TH_DRISE;
                    /*
                     * Similarly, if no fallEvent is configured,
                     *  there's no point in trying to fire it either.
                     */
                    if (entry->mteTThDRiseEvent[0] != '\0' ) {
                        entry->mteTriggerXOwner   = entry->mteTThObjOwner;
                        entry->mteTriggerXObjects = entry->mteTThObjects;
                        entry->mteTriggerFired    = vp1;
                        n = entry->mteTriggerValueID_len;
                        mteEvent_fire(entry->mteTThDFallOwner,
                                      entry->mteTThDFallEvent, 
                                      entry, vp1->name+n, vp1->name_length-n);
                    }
                }
            }
            vp1->index = cmp;
          }
        }
    }

    /*
     * Finally, rotate the results - ready for the next run.
     */
    snmp_free_varbind( entry->old_results );
    entry->old_results = var;
    if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
        snmp_free_varbind( entry->old_deltaDs );
        entry->old_deltaDs = dvar;
        entry->sysUpTime   = *sysUT_var.val.integer;
    }
}

void
mteTrigger_enable( struct mteTrigger *entry )
{
    if (!entry)
        return;

    if (entry->alarm) {
        /* XXX - or explicitly call mteTrigger_disable ?? */
        snmp_alarm_unregister( entry->alarm );
        entry->alarm = 0;
    }

    if (entry->mteTriggerFrequency) {
        /*
         * register once to run ASAP, and another to run
         * at the trigger frequency
         */
        snmp_alarm_register(0, 0, mteTrigger_run, entry );
        entry->alarm = snmp_alarm_register(
                           entry->mteTriggerFrequency, SA_REPEAT,
                           mteTrigger_run, entry );
    }
}

void
mteTrigger_disable( struct mteTrigger *entry )
{
    if (!entry)
        return;

    if (entry->alarm) {
        snmp_alarm_unregister( entry->alarm );
        entry->alarm = 0;
        /* XXX - perhaps release any previous results */
    }
}

long _mteTrigger_MaxCount = 0;
long _mteTrigger_countEntries(void)
{
    struct mteTrigger *entry;
    netsnmp_tdata_row *row;
    long count = 0;

    for (row = netsnmp_tdata_row_first(trigger_table_data);
         row;
         row = netsnmp_tdata_row_next(trigger_table_data, row)) {
        entry  = (struct mteTrigger *)row->data;
        count += entry->count;
    }

    return count;
}

long mteTrigger_getNumEntries(int max)
{
    long count;
    /* XXX - implement some form of caching ??? */
    count = _mteTrigger_countEntries();
    if ( count > _mteTrigger_MaxCount )
        _mteTrigger_MaxCount = count;
    
    return ( max ?  _mteTrigger_MaxCount : count);
}
