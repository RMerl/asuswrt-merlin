/*
 * DisMan Expression MIB:
 *    Core implementation of expression object handling
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/expr/expObject.h"
#include "disman/expr/expExpression.h"

netsnmp_tdata *expObject_table_data;

    /*
     * Initializes the container for the expression object table,
     * regardless of which module is initialised first.
     */
void
init_expObject_table_data(void)
{
    DEBUGMSGTL(("disman:expr:init", "init expObject container\n"));
    if (!expObject_table_data) {
         expObject_table_data = netsnmp_tdata_create_table("expObjectTable", 0);
         DEBUGMSGTL(("disman:expr:init", "create expObject container (%p)\n",
                                          expObject_table_data));
    }
}

/* Initialize the expObject module */
void
init_expObject(void)
{
    init_expObject_table_data();
}


/*
 * Create a new row in the object table 
 */
struct expObject *
expObject_createEntry(const char *expOwner, const char *expName, long expIndex, int fixed)
{
    netsnmp_tdata_row *row;

    row = expObject_createRow(expOwner, expName, expIndex, fixed);
    return row ? (struct expObject *)row->data : NULL;
}

netsnmp_tdata_row *
expObject_createRow( const char *expOwner, const char *expName, long expIndex, int fixed)
{
    struct expObject  *entry;
    netsnmp_tdata_row *row;
    size_t expOwner_len = (expOwner) ? strlen(expOwner) : 0;
    size_t expName_len  = (expName)  ? strlen(expName)  : 0;

    /*
     * Create the expObject entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = SNMP_MALLOC_TYPEDEF(struct expObject);
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
    if (expOwner)
        memcpy(entry->expOwner, expOwner, expOwner_len);
    netsnmp_tdata_row_add_index(row, ASN_OCTET_STR,
                                entry->expOwner, expOwner_len);
    if (expName)
        memcpy(entry->expName,  expName,  expName_len);
    netsnmp_tdata_row_add_index(row, ASN_OCTET_STR,
                                entry->expName, expName_len);
    entry->expObjectIndex = expIndex;
    netsnmp_tdata_row_add_index(row, ASN_INTEGER,
                               &entry->expObjectIndex, sizeof(long));

    entry->expObjectSampleType     = 1;  /* absoluteValue */
    entry->expObjDiscontinuityType = 1;  /* timeTicks     */
    if (fixed)
        entry->flags |= EXP_OBJ_FLAG_FIXED;

    /*
     * ... and insert the row into the table container.
     */
    netsnmp_tdata_add_row(expObject_table_data, row);
    return row;
}

/*
 * Remove a row from the expression object table 
 */
void
expObject_removeEntry(netsnmp_tdata_row * row)
{
    struct expObject *entry;

    if (!row)
        return;                 /* Nothing to remove */
    entry = (struct expObject *)
        netsnmp_tdata_remove_and_delete_row(expObject_table_data, row);
    if (entry) {
        if (entry->vars      ) snmp_free_varbind( entry->vars      );
        if (entry->old_vars  ) snmp_free_varbind( entry->old_vars  );
        if (entry->dvars     ) snmp_free_varbind( entry->dvars     );
        if (entry->old_dvars ) snmp_free_varbind( entry->old_dvars );
        if (entry->cvars     ) snmp_free_varbind( entry->cvars     );
        SNMP_FREE(entry);
    }
}


netsnmp_tdata_row *
expObject_getFirst( const char *expOwner, const char *expName )
{
    netsnmp_tdata_row *row;
    struct expObject  *entry;
    netsnmp_variable_list owner_var;
    netsnmp_variable_list  name_var;

    if (!expOwner || !expName)
        return NULL;

    /*
     * Find the first object entry that could potentially
     *   refer to the specified expression...
     */
    memset(&owner_var, 0, sizeof(netsnmp_variable_list));
    memset(&name_var,  0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value( &owner_var, ASN_OCTET_STR,
                       (const u_char*)expOwner, strlen(expOwner));
    snmp_set_var_typed_value( &name_var,  ASN_OCTET_STR,
                       (const u_char*)expName,  strlen(expName));
    owner_var.next_variable = &name_var;
    row = netsnmp_tdata_row_next_byidx( expObject_table_data, &owner_var );

    /*
     * ... and check that it does!
     */
    if (!row || !row->data)
        return NULL;
    entry = (struct expObject *)row->data;

    if ((strcmp( entry->expOwner, expOwner ) != 0) ||
        (strcmp( entry->expName,  expName  ) != 0))
        return NULL;

    return row;
}

netsnmp_tdata_row *
expObject_getNext( netsnmp_tdata_row *thisRow )
{
    struct expObject  *thisEntry;
    struct expObject  *nextEntry;
    netsnmp_tdata_row *nextRow;

    if (!thisRow || !thisRow->data)
        return NULL;
    thisEntry = (struct expObject *)thisRow->data;

    /*
     * Retrieve the next row, and check whether this
     *   refers to the same expression too.
     */
    nextRow = netsnmp_tdata_row_next( expObject_table_data, thisRow );

    if (!nextRow || !nextRow->data)
        return NULL;
    nextEntry = (struct expObject *)nextRow->data;

    if ((strcmp( nextEntry->expOwner, thisEntry->expOwner ) != 0) ||
        (strcmp( nextEntry->expName,  thisEntry->expName  ) != 0))
        return NULL;

    return nextRow;
}


netsnmp_variable_list *
_expObject_buildList( oid *root, size_t root_len, size_t prefix_len,
                      netsnmp_variable_list *template_list )
{
    netsnmp_variable_list *query_list = NULL;
    netsnmp_variable_list *vp1,  *vp2 = NULL;
    oid    name[ MAX_OID_LEN ];
    int i;

    if ( prefix_len ) {
        /*
         * For wildcarded objects, build a list of all relevant
         *   instances, using the template_list to provide the
         *   necessary instance suffixes.
         */
        memset( name, 0, sizeof(name));
        memcpy( name, root, root_len*sizeof(oid));
        for ( vp1 = template_list; vp1; vp1=vp1->next_variable ) {
            /*
             * Append a new varbind to the list for this object ...
             */
            if ( !query_list ) {
                query_list = (netsnmp_variable_list*)
                                 SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                vp2 = query_list;
            } else {
                vp2->next_variable = (netsnmp_variable_list*)
                                 SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
                vp2 = vp2->next_variable;
            }
            /*
             * ... and set the OID using the template suffix
             */
            for ( i=0; i <= vp1->name_length - prefix_len; i++)
                name[ root_len+i ] = vp1->name[ prefix_len+i ];
            snmp_set_var_objid( vp2, name, root_len+i );
        }
    } else {
        /*
         * Otherwise, just use the (single) OID provided.
         */
        query_list = (netsnmp_variable_list*)
                                 SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
        snmp_set_var_objid( query_list, root, root_len );
    }
    return query_list;
}


void
expObject_getData( struct expExpression  *expr, struct expObject  *obj )
{
    netsnmp_variable_list *var;

    /*
     * Retrieve and store the basic object value(s)
     *   (keeping the previous values if necessary)
     */
    if (obj->flags & EXP_OBJ_FLAG_PREFIX ) {
        /*
         * If this is the expExpressionPrefix object, then
         *   we already have the necessary list of values.
         *   There's no need to retrieve it again.
         * This also takes care of releasing the prefix list
         *   once the results are no longer needed.
         */
        var = expr->pvars;
    } else {
        if (!(obj->flags & EXP_OBJ_FLAG_OWILD ))
            /*
             * Set up the request 'list' for an
             *   exact (non-wildcarded) object.
             */
            var = _expObject_buildList( obj->expObjectID,
                                        obj->expObjectID_len, 0, NULL );
        else {
            if ( !expr->expPrefix_len ) {
                /*
                 * You can't really have wildcarded objects unless
                 *   the expression as a whole is wildcarded too.
                 */
                return;
            }
            /*
             * Set up the request list for a wildcarded object
             */
            var = _expObject_buildList( obj->expObjectID,
                                        obj->expObjectID_len,
                                       expr->expPrefix_len,
                                       expr->pvars );
        }
        netsnmp_query_get( var, expr->session );
    }
    
    if ( obj->expObjectSampleType != EXPSAMPLETYPE_ABSOLUTE ) {
        /*
         * For Delta (and Changed) samples, we need
         *   to store the previous value as well.
         */
        if ( obj->old_vars )
            snmp_free_varbind( obj->old_vars );
        obj->old_vars = obj->vars;
    } else
        snmp_free_varbind( obj->vars );

    obj->vars = var;


    /*
     * For Delta samples, there may be a discontinuity marker
     *   (or set of wildcarded markers) to be sampled as well.
     *   This necessarily requires storing the previous marker(s).
     */
    if (( obj->expObjectSampleType != EXPSAMPLETYPE_ABSOLUTE ) &&
        ( obj->flags & EXP_OBJ_FLAG_DDISC )) {

        if ( obj->flags & EXP_OBJ_FLAG_DWILD )
            var = _expObject_buildList( obj->expObjDeltaD,
                                        obj->expObjDeltaD_len,
                                       expr->expPrefix_len,
                                       expr->pvars );
        else
            var = _expObject_buildList( obj->expObjDeltaD,
                                        obj->expObjDeltaD_len, 0, NULL );
        netsnmp_query_get( var, expr->session );
        if ( obj->old_dvars )
            snmp_free_varbind( obj->old_dvars );
        obj->old_dvars = obj->dvars;
        obj->dvars     = var;
    }

    /*
     * If there's an expObjectConditional value specified
     *   (or set of wildcarded values) then add these to the
     *   ever-growing collection of retrieved values.
     */
    if ( obj->expObjCond_len ) {
        if ( obj->flags & EXP_OBJ_FLAG_CWILD )
            var = _expObject_buildList( obj->expObjCond,
                                        obj->expObjCond_len,
                                       expr->expPrefix_len,
                                       expr->pvars );
        else
            var = _expObject_buildList( obj->expObjCond,
                                        obj->expObjCond_len, 0, NULL );
        /*
         * XXX - Check when to use GetNext
         *
         *    (The MIB description seems bogus?)
         */
        netsnmp_query_get( var, expr->session );
        if ( obj->cvars )
            snmp_free_varbind( obj->cvars );
        obj->cvars = var;
    }
}
