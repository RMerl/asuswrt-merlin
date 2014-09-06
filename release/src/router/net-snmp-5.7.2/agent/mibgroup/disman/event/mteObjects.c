/*
 * DisMan Event MIB:
 *     Core implementation of the object handling behaviour
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteObjects.h"

netsnmp_tdata *objects_table_data;

    /*
     * Initialize the container for the object table
     * regardless of which initialisation routine is called first.
     */

void
init_objects_table_data(void)
{
    if (!objects_table_data)
        objects_table_data = netsnmp_tdata_create_table("mteObjectsTable", 0);
}



SNMPCallback _init_default_mteObject_lists;

/** Initializes the mteObjects module */
void
init_mteObjects(void)
{
    init_objects_table_data();

    /*
     * Insert fixed object lists for the default trigger
     * notifications, once the MIB files have been read in.
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, 
                           SNMP_CALLBACK_POST_READ_CONFIG,
                           _init_default_mteObject_lists, NULL);
}


void
_init_default_mteObject( const char *oname, const char *object, int index, int wcard)
{
    struct mteObject *entry;

    entry = mteObjects_addOID( "_snmpd", oname, index, object, 0 );
    if (entry) {
        entry->flags |= MTE_OBJECT_FLAG_ACTIVE|
                        MTE_OBJECT_FLAG_FIXED |
                        MTE_OBJECT_FLAG_VALID;
        if (wcard)
            entry->flags |= MTE_OBJECT_FLAG_WILD;
    }
}

int
_init_default_mteObject_lists( int majorID, int minorID,
                               void *serverargs, void *clientarg)
{
    static int _defaults_init = 0;

    if (_defaults_init)
        return 0;
                                                   /* mteHotTrigger     */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.1", 1, 0);
                                                   /* mteHotTargetName  */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.2", 2, 0);
                                                   /* mteHotContextName */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.3", 3, 0);
                                                   /* mteHotOID         */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.4", 4, 0);
                                                   /* mteHotValue       */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.5", 5, 0);


                                                   /* mteHotTrigger     */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.1", 1, 0);
                                                   /* mteHotTargetName  */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.2", 2, 0);
                                                   /* mteHotContextName */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.3", 3, 0);
                                                   /* mteHotOID         */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.4", 4, 0);
                                                   /* mteFailedReason   */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.6", 5, 0);

                                                   /* ifIndex       */
    _init_default_mteObject( "_linkUpDown", ".1.3.6.1.2.1.2.2.1.1", 1, 1);
                                                   /* ifAdminStatus */
    _init_default_mteObject( "_linkUpDown", ".1.3.6.1.2.1.2.2.1.7", 2, 1);
                                                   /* ifOperStatus  */
    _init_default_mteObject( "_linkUpDown", ".1.3.6.1.2.1.2.2.1.8", 3, 1);

    _defaults_init = 1;
    return 0;
}

    /* ===================================================
     *
     * APIs for maintaining the contents of the mteObjectsTable container.
     *
     * =================================================== */

/*
 * Create a new row in the object table 
 */
netsnmp_tdata_row      *
mteObjects_createEntry(const char *owner, const char *oname, int index, int flags)
{
    struct mteObject   *entry;
    netsnmp_tdata_row  *row, *row2;
    size_t owner_len = (owner) ? strlen(owner) : 0;
    size_t oname_len = (oname) ? strlen(oname) : 0;

    /*
     * Create the mteObjects entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = SNMP_MALLOC_TYPEDEF(struct mteObject);
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
    if (owner)
        memcpy(entry->mteOwner, owner, owner_len);
    netsnmp_tdata_row_add_index(row,         ASN_OCTET_STR,
                                entry->mteOwner, owner_len);
    if (oname)
        memcpy(entry->mteOName, oname, oname_len);
    netsnmp_tdata_row_add_index(row,         ASN_OCTET_STR,
                                entry->mteOName, oname_len);
    entry->mteOIndex = index;
    netsnmp_tdata_row_add_index(row, ASN_INTEGER,
                                &entry->mteOIndex, sizeof(long));

    entry->mteObjectID_len = 2;  /* .0.0 */
    if (flags & MTE_OBJECT_FLAG_FIXED)
        entry->flags |= MTE_OBJECT_FLAG_FIXED;

    /*
     * Check whether there's already a row with the same indexes
     *   (XXX - relies on private internal data ???)
     */
    row2 = netsnmp_tdata_row_get_byoid(objects_table_data,
                                       row->oid_index.oids,
                                       row->oid_index.len); 
    if (row2) {
        if (flags & MTE_OBJECT_FLAG_NEXT) {
            /*
             * If appropriate, keep incrementing the final
             * index value until we find a free slot...
             */
            while (row2) {
                row->oid_index.oids[row->oid_index.len]++;
                row2 = netsnmp_tdata_row_get_byoid(objects_table_data,
                                                   row->oid_index.oids,
                                                  row->oid_index.len); 
            }
        } else {
            /*
             * ... otherwise, this is an error.
             *     Tidy up, and return failure.
             */
            netsnmp_tdata_delete_row(row);
            SNMP_FREE(entry);
            return NULL;
        }
    }

    /*
     * ... finally, insert the row into the (common) table container
     */
    netsnmp_tdata_add_row(objects_table_data, row);
    return row;
}


/*
 * Add a row to the object table 
 */
struct mteObject *
mteObjects_addOID(const char *owner, const char *oname, int index,
                  const char *oid_name_buf, int wild )
{
    netsnmp_tdata_row *row;
    struct mteObject  *entry;
    oid    name_buf[ MAX_OID_LEN ];
    size_t name_buf_len;

    name_buf_len = MAX_OID_LEN;
    if (!snmp_parse_oid(oid_name_buf, name_buf, &name_buf_len)) {
        snmp_log(LOG_ERR, "payload OID: %s\n", oid_name_buf);
        config_perror("unknown payload OID");
        return NULL;
    }

    row = mteObjects_createEntry(owner, oname, index,
                           MTE_OBJECT_FLAG_FIXED|MTE_OBJECT_FLAG_NEXT);
    entry = (struct mteObject *)row->data;

    entry->mteObjectID_len = name_buf_len;
    memcpy(entry->mteObjectID, name_buf, name_buf_len*sizeof(oid));
    if (wild)
        entry->flags |= MTE_OBJECT_FLAG_WILD;
    entry->flags     |= MTE_OBJECT_FLAG_VALID|
                        MTE_OBJECT_FLAG_ACTIVE;

    return entry;
}


/*
 * Remove a row from the event table 
 */
void
mteObjects_removeEntry(netsnmp_tdata_row *row)
{
    struct mteObject *entry;

    if (!row)
        return;                 /* Nothing to remove */
    entry = (struct mteObject *)
        netsnmp_tdata_remove_and_delete_row(objects_table_data, row);
    SNMP_FREE(entry);
}


/*
 * Remove all matching rows from the event table 
 */
void
mteObjects_removeEntries( const char *owner, char *oname )
{
    netsnmp_tdata_row     *row;
    netsnmp_variable_list  owner_var, oname_var;

    memset(&owner_var, 0, sizeof(owner_var));
    memset(&oname_var, 0, sizeof(oname_var));
    snmp_set_var_typed_value( &owner_var, ASN_OCTET_STR,
                               owner,     strlen(owner));
    snmp_set_var_typed_value( &oname_var, ASN_OCTET_STR,
                               oname,     strlen(oname));
    owner_var.next_variable = &oname_var;

    row = netsnmp_tdata_row_next_byidx( objects_table_data, &owner_var );

    while (row && !netsnmp_tdata_compare_subtree_idx( row, &owner_var )) {
        mteObjects_removeEntry(row);
        row = netsnmp_tdata_row_next_byidx( objects_table_data, &owner_var );
    }
    return;
}


    /* ===================================================
     *
     * API for retrieving a list of matching objects
     *
     * =================================================== */

int
mteObjects_vblist( netsnmp_variable_list *vblist,
               char *owner,  char   *oname,
               oid  *suffix, size_t sfx_len )
{
    netsnmp_tdata_row     *row;
    struct mteObject      *entry;
    netsnmp_variable_list  owner_var, oname_var;
    netsnmp_variable_list *var = vblist;
    oid    name[MAX_OID_LEN];
    size_t name_len;

    if (!oname || !*oname) {
        DEBUGMSGTL(("disman:event:objects", "No objects to add (%s)\n",
                                         owner));
        return 1;   /* Empty object name means nothing to add */
    }

    DEBUGMSGTL(("disman:event:objects", "Objects add (%s, %s)\n",
                                         owner, oname ));

    /*
     * Retrieve any matching entries from the mteObjectTable
     *  and add them to the specified varbind list.
     */
    memset(&owner_var, 0, sizeof(owner_var));
    memset(&oname_var, 0, sizeof(oname_var));
    snmp_set_var_typed_value( &owner_var, ASN_OCTET_STR,
                               owner,      strlen(owner));
    snmp_set_var_typed_value( &oname_var, ASN_OCTET_STR,
                               oname,      strlen(oname));
    owner_var.next_variable = &oname_var;

    row = netsnmp_tdata_row_next_byidx( objects_table_data, &owner_var );

    while (row && !netsnmp_tdata_compare_subtree_idx( row, &owner_var )) {
        entry = (struct mteObject *)netsnmp_tdata_row_entry(row);

        memset(name, 0, MAX_OID_LEN);
        memcpy(name, entry->mteObjectID,
                     entry->mteObjectID_len*sizeof(oid));
        name_len = entry->mteObjectID_len;

        /*
         * If the trigger value is wildcarded (sfx_len > 0),
         *    *and* this object entry is wildcarded,
         *    then add the supplied instance suffix.
         * Otherwise use the Object OID as it stands.
         */
        if (sfx_len &&
            entry->flags & MTE_OBJECT_FLAG_WILD) {
            memcpy(&name[name_len], suffix, sfx_len*sizeof(oid));
            name_len += sfx_len;
        }
        snmp_varlist_add_variable( &var, name, name_len, ASN_NULL, NULL, 0);

        row = netsnmp_tdata_row_next( objects_table_data, row );
    }
    return 0;
}


int
mteObjects_internal_vblist( netsnmp_variable_list *vblist,
                            char   *oname,
                            struct mteTrigger *trigger,
                            netsnmp_session   *sess)
{
    netsnmp_variable_list *var = NULL, *vp;
    oid mteHotTrigger[] = {1, 3, 6, 1, 2, 1, 88, 2, 1, 1, 0};
    oid mteHotTarget[]  = {1, 3, 6, 1, 2, 1, 88, 2, 1, 2, 0};
    oid mteHotContext[] = {1, 3, 6, 1, 2, 1, 88, 2, 1, 3, 0};
    oid mteHotOID[]     = {1, 3, 6, 1, 2, 1, 88, 2, 1, 4, 0};
    oid mteHotValue[]   = {1, 3, 6, 1, 2, 1, 88, 2, 1, 5, 0};

    oid ifIndexOid[]    = {1, 3, 6, 1, 2, 1, 2, 2, 1, 1, 0};
    oid ifAdminStatus[] = {1, 3, 6, 1, 2, 1, 2, 2, 1, 7, 0};
    oid ifOperStatus[]  = {1, 3, 6, 1, 2, 1, 2, 2, 1, 8, 0};

    oid if_index;

    /*
     * Construct the varbinds for this (internal) event...
     */
    if (!strcmp(oname, "_triggerFire")) {

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
    } else if ((!strcmp(oname, "_linkUpDown"  ))) {
        /*
         * The ifOperStatus varbind that triggered this entry
         *  is held in the trigger->mteTriggerFired field
         *
         * We can retrieve the ifIndex and ifOperStatus values
         *  from this varbind.  But first we need to tweak the
         *  static ifXXX OID arrays to include the correct index.
         *  (or this could be passed in from the calling routine?)
         *
         * Unfortunately we don't have the current AdminStatus value,
         *  so we'll need to make another query to retrieve that.
         */
        if_index = trigger->mteTriggerFired->name[10];
        ifIndexOid[    10 ] = if_index;
        ifAdminStatus[ 10 ] = if_index;
        ifOperStatus[  10 ] = if_index;
        snmp_varlist_add_variable( &var,
               ifIndexOid, OID_LENGTH(ifIndexOid),
               ASN_INTEGER, &if_index, sizeof(if_index));

               /* Set up a dummy varbind for ifAdminStatus... */
        snmp_varlist_add_variable( &var,
               ifAdminStatus, OID_LENGTH(ifAdminStatus),
               ASN_INTEGER,
               trigger->mteTriggerFired->val.integer,
               trigger->mteTriggerFired->val_len);
               /* ... then retrieve the actual value */
        netsnmp_query_get( var->next_variable, sess );

        snmp_varlist_add_variable( &var,
               ifOperStatus, OID_LENGTH(ifOperStatus),
               ASN_INTEGER,
               trigger->mteTriggerFired->val.integer,
               trigger->mteTriggerFired->val_len);
    } else {
        DEBUGMSGTL(("disman:event:objects",
                    "Unknown internal objects tag (%s)\n", oname));
        return 1;
    }

    /*
     * ... and insert them into the main varbind list
     *     (at the point specified)
     */
    for (vp = var; vp && vp->next_variable; vp=vp->next_variable)
        ;
    vp->next_variable     = vblist->next_variable;
    vblist->next_variable = var;
    return 0;
}
