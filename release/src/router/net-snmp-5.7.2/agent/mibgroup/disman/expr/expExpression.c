/*
 * DisMan Expression MIB:
 *    Core implementation of the expression handling behaviour
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/expr/expExpression.h"
#include "disman/expr/expObject.h"

netsnmp_tdata *expr_table_data;

    /*
     * Initializes the container for the expExpression table,
     * regardless of which module initialisation routine is called first.
     */
void
init_expr_table_data(void)
{
    DEBUGMSGTL(("disman:expr:init", "init expression container\n"));
    if (!expr_table_data) {
         expr_table_data = netsnmp_tdata_create_table("expExpressionTable", 0);
         DEBUGMSGTL(("disman:expr:init", "create expression container (%p)\n",
                                          expr_table_data));
    }
}

/** Initialize the expExpression module */
void
init_expExpression(void)
{
    init_expr_table_data();
}


    /* ===================================================
     *
     * APIs for maintaining the contents of the
     *    expression table container.
     *
     * =================================================== */

void
_mteExpr_dump(void)
{
    struct mteExpression *entry;
    netsnmp_tdata_row *row;
    int i = 0;

    for (row = netsnmp_tdata_row_first(expr_table_data);
         row;
         row = netsnmp_tdata_row_next(expr_table_data, row)) {
        entry = (struct mteExpression *)row->data;
        DEBUGMSGTL(("disman:expr:dump", "ExpressionTable entry %d: ", i));
        DEBUGMSGOID(("disman:expr:dump", row->oid_index.oids, row->oid_index.len));
        DEBUGMSG(("disman:expr:dump", "(%s, %s)",
                                         row->indexes->val.string,
                                         row->indexes->next_variable->val.string));
        DEBUGMSG(("disman:expr:dump", ": %p, %p\n", row, entry));
        i++;
    }
    DEBUGMSGTL(("disman:expr:dump", "ExpressionTable %d entries\n", i));
}



/*
 * Create a new row in the expression table 
 */
struct expExpression *
expExpression_createEntry(const char *expOwner, const char *expName, int fixed)
{
    netsnmp_tdata_row    *row;

    row = expExpression_createRow(expOwner, expName, fixed);
    return row ? (struct expExpression *)row->data : NULL;
}
 

netsnmp_tdata_row *
expExpression_createRow(const char *expOwner, const char *expName, int fixed)
{
    struct expExpression *entry;
    netsnmp_tdata_row    *row;
    size_t expOwner_len = (expOwner) ? strlen(expOwner) : 0;
    size_t expName_len  = (expName)  ? strlen(expName)  : 0;

    /*
     * Create the expExpression entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = SNMP_MALLOC_TYPEDEF(struct expExpression);
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

    entry->expValueType  = EXPVALTYPE_COUNTER;
    entry->expErrorCount = 0;
    if (fixed)
        entry->flags |= EXP_FLAG_FIXED;

    /*
     * ... and insert the row into the table container.
     */
    netsnmp_tdata_add_row(expr_table_data, row);
    DEBUGMSGTL(("disman:expr:table", "Expression entry created (%s, %s)\n",
                                      expOwner, expName));
    return row;
}

/*
 * Remove a row from the expression table 
 */
void
expExpression_removeEntry(netsnmp_tdata_row *row)
{
    struct expExpression *entry;

    if (!row)
        return;                 /* Nothing to remove */
    entry = (struct expExpression *)
        netsnmp_tdata_remove_and_delete_row(expr_table_data, row);
    if (entry) {
        /* expExpression_disable( entry ) */
        SNMP_FREE(entry);
    }
}


struct expExpression *
expExpression_getFirstEntry( void )
{
    return (struct expExpression *)
        netsnmp_tdata_row_entry(netsnmp_tdata_row_first(expr_table_data));
}

struct expExpression *
expExpression_getNextEntry(const  char *owner, const char *name )
{
    netsnmp_variable_list owner_var, name_var;

    memset(&owner_var, 0, sizeof(netsnmp_variable_list));
    memset(&name_var,  0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value( &owner_var, ASN_OCTET_STR,
                          (const u_char*)owner, strlen(owner));
    snmp_set_var_typed_value( &name_var,  ASN_OCTET_STR,
                          (const u_char*)name,  strlen(name));
    owner_var.next_variable = &name_var;

    return (struct expExpression *)
        netsnmp_tdata_row_entry(
            netsnmp_tdata_row_next_byidx(expr_table_data, &owner_var));
}

struct expExpression *
expExpression_getEntry(const char *owner, const char *name )
{
    netsnmp_variable_list owner_var, name_var;

    memset(&owner_var, 0, sizeof(netsnmp_variable_list));
    memset(&name_var,  0, sizeof(netsnmp_variable_list));
    snmp_set_var_typed_value( &owner_var, ASN_OCTET_STR,
                          (const u_char*)owner, strlen(owner));
    snmp_set_var_typed_value( &name_var,  ASN_OCTET_STR,
                          (const u_char*)name,  strlen(name));
    owner_var.next_variable = &name_var;

    return (struct expExpression *)
        netsnmp_tdata_row_entry(
            netsnmp_tdata_row_get_byidx(expr_table_data, &owner_var));
}


    /* ===================================================
     *
     * APIs for evaluating an expression - data gathering
     *
     * =================================================== */



/*
 *  Gather the data necessary for evaluating an expression.
 *
 *  This will retrieve *all* the data relevant for all
 *    instances of this expression, rather than just the
 *    just the values needed for expanding a given instance.
 */
void
expExpression_getData( unsigned int reg, void *clientarg )
{
    struct expExpression  *entry = (struct expExpression *)clientarg;
    netsnmp_tdata_row     *row;
    netsnmp_variable_list *var;
    int ret;

    if ( !entry && reg ) {
        snmp_alarm_unregister( reg );
        return;
    }

    if (( entry->expExpression[0] == '\0' ) ||
        !(entry->flags & EXP_FLAG_ACTIVE)   ||
        !(entry->flags & EXP_FLAG_VALID))
        return;

    DEBUGMSGTL(("disman:expr:run", "Gathering expression data (%s, %s)\n",
                                    entry->expOwner, entry->expName));

    /*
     *  This routine can be called in two situations:
     *    - regularly by 'snmp_alarm'  (reg != 0)
     *         (as part of ongoing delta-value sampling)
     *    - on-demand                  (reg == 0)
     *         (for evaluating a particular entry)
     *
     *  If a regularly sampled expression (entry->alarm != 0)
     *    is invoked on-demand (reg == 0), then use the most
     *    recent sampled values, rather than retrieving them again.
     */
    if ( !reg && entry->alarm )
        return;

    /*
     * XXX - may want to implement caching for on-demand evaluation
     *       of non-regularly sampled expressions.
     */

    /*
     * For a wildcarded expression, expExpressionPrefix is used
     *   to determine which object instances to retrieve.
     * (For a non-wildcarded expression, we already know
     *   explicitly which object instances will be needed).
     *
     * If we walk this object here, then the results can be
     *   used to build the necessary GET requests for each
     *   individual parameter object (within expObject_getData)
     *
     * This will probably be simpler (and definitely more efficient)
     *   than walking the object instances separately and merging
     *   merging the results).
     *
     * NB: Releasing any old results is handled by expObject_getData.
     *     Assigning to 'entry->pvars' without first releasing the
     *     previous contents does *not* introduce a memory leak.
     */
    if ( entry->expPrefix_len ) {
        var = (netsnmp_variable_list *)
                   SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
        snmp_set_var_objid( var, entry->expPrefix, entry->expPrefix_len);
        ret = netsnmp_query_walk( var, entry->session );
        DEBUGMSGTL(("disman:expr:run", "Walk returned %d\n", ret ));
        entry->pvars = var;
    }

    /* XXX - retrieve sysUpTime.0 value, and check for discontinuity */
  /*
    entry->flags &= ~EXP_FLAG_SYSUT;
    var = (netsnmp_variable_list *)
               SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
    snmp_set_var_objid( var, sysUT_oid, sysUT_oid_len );
    netsnmp_query_get( var, entry->session );
    if ( *var->val.integer != entry->sysUpTime ) {
        entry->flags |=  EXP_FLAG_SYSUT;
        entry->sysUpTime = *var->val.integer;
    }
    snmp_free_varbind(var);
   */

    /*
     * Loop through the list of relevant objects,
     *   and retrieve the corresponding values.
     */
    for ( row = expObject_getFirst(  entry->expOwner, entry->expName );
          row;
          row = expObject_getNext( row )) {

        /* XXX - may need to check whether owner/name still match */
        expObject_getData( entry, (struct expObject *)row->data);
    }
}


void
expExpression_enable( struct expExpression *entry )
{
    DEBUGMSG(("disman:expr:run", "Enabling %s\n", entry->expName));
    if (!entry)
        return;

    if (entry->alarm) {
        /* or explicitly call expExpression_disable ?? */
        snmp_alarm_unregister( entry->alarm );
        entry->alarm = 0;
    }

    if (entry->expDeltaInterval) {
        entry->alarm = snmp_alarm_register(
                           entry->expDeltaInterval, SA_REPEAT,
                           expExpression_getData, entry );
        expExpression_getData( entry->alarm, (void*)entry );
    }
}

void
expExpression_disable( struct expExpression *entry )
{
    if (!entry)
        return;

    if (entry->alarm) {
        snmp_alarm_unregister( entry->alarm );
        entry->alarm = 0;
        /* Perhaps release any previous results ?? */
    }
}


long _expExpression_MaxCount = 0;
long _expExpression_countEntries(void)
{
    struct expExpression *entry;
    netsnmp_tdata_row *row;
    long count = 0;

    for (row = netsnmp_tdata_row_first(expr_table_data);
         row;
         row = netsnmp_tdata_row_next(expr_table_data, row)) {
        entry  = (struct expExpression *)row->data;
        count += entry->count;
    }

    return count;
}

long expExpression_getNumEntries(int max)
{
    long count;
    /* XXX - implement some form of caching ??? */
    count = _expExpression_countEntries();
    if ( count > _expExpression_MaxCount )
        _expExpression_MaxCount = count;
    
    return ( max ?  _expExpression_MaxCount : count);
}
