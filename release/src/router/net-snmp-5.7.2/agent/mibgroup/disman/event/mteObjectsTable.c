/*
 * DisMan Event MIB:
 *     Implementation of the mteObjectsTable MIB interface
 * See 'mteObjects.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/event/mteObjects.h"
#include "disman/event/mteObjectsTable.h"

netsnmp_feature_require(table_tdata)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_oid)
netsnmp_feature_require(check_vb_truthvalue)
netsnmp_feature_require(table_tdata_insert_row)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static netsnmp_table_registration_info *table_info;

/** Initializes the mteObjectsTable module */
void
init_mteObjectsTable(void)

{
    static oid mteObjectsTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 3, 1 };
    size_t     mteObjectsTable_oid_len = OID_LENGTH(mteObjectsTable_oid);
    netsnmp_handler_registration    *reg;

    /*
     * Ensure the object table container is available...
     */
    init_objects_table_data();

    /*
     * ... then set up the MIB interface to this table
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    reg = netsnmp_create_handler_registration("mteObjectsTable",
                                            mteObjectsTable_handler,
                                            mteObjectsTable_oid,
                                            mteObjectsTable_oid_len,
                                            HANDLER_CAN_RWRITE);
#else /* !NETSNMP_NO_WRITE_SUPPORT */
    reg = netsnmp_create_handler_registration("mteObjectsTable",
                                            mteObjectsTable_handler,
                                            mteObjectsTable_oid,
                                            mteObjectsTable_oid_len,
                                            HANDLER_CAN_RONLY);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* index: mteOwner */
                                     ASN_OCTET_STR, /* index: mteObjectsName */
                                     ASN_UNSIGNED,  /* index: mteObjectsIndex */
                                     0);

    table_info->min_column = COLUMN_MTEOBJECTSID;
    table_info->max_column = COLUMN_MTEOBJECTSENTRYSTATUS;


    netsnmp_tdata_register(reg, objects_table_data, table_info);
}

void
shutdown_mteObjectsTable(void)
{
    if (table_info) {
	netsnmp_table_registration_info_free(table_info);
	table_info = NULL;
    }
}


/** handles requests for the mteObjectsTable table */
int
mteObjectsTable_handler(netsnmp_mib_handler *handler,
                        netsnmp_handler_registration *reginfo,
                        netsnmp_agent_request_info *reqinfo,
                        netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_tdata_row          *row;
    struct mteObject           *entry;
    char mteOwner[MTE_STR1_LEN+1];
    char mteOName[MTE_STR1_LEN+1];
    long ret;

    DEBUGMSGTL(("disman:event:mib", "ObjTable handler (%d)\n", reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteObject *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            if (!entry) {
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
                continue;
            }
            switch (tinfo->colnum) {
            case COLUMN_MTEOBJECTSID:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->mteObjectID,
                                         entry->mteObjectID_len*sizeof(oid));
                break;
            case COLUMN_MTEOBJECTSIDWILDCARD:
                ret = (entry->flags & MTE_OBJECT_FLAG_WILD ) ?
                           TV_TRUE : TV_FALSE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                ret = (entry->flags & MTE_OBJECT_FLAG_ACTIVE ) ?
                           RS_ACTIVE : RS_NOTINSERVICE;
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER, ret);
                break;
            }
        }
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
        /*
         * Write-support
         */
    case MODE_SET_RESERVE1:

        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteObject *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEOBJECTSID:
                ret = netsnmp_check_vb_oid( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /*
                 * Can't modify the OID of an active row
                 *  (an unnecessary restriction, IMO)
                 */
                if (entry &&
                    entry->flags & MTE_OBJECT_FLAG_ACTIVE ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEOBJECTSIDWILDCARD:
                ret = netsnmp_check_vb_truthvalue( request->requestvb );
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /*
                 * Can't modify the wildcarding of an active row
                 *  (an unnecessary restriction, IMO)
                 */
                if (entry &&
                    entry->flags & MTE_OBJECT_FLAG_ACTIVE ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                ret = netsnmp_check_vb_rowstatus(request->requestvb,
                          (entry ? RS_ACTIVE : RS_NONEXISTENT));
                if (ret != SNMP_ERR_NOERROR) {
                    netsnmp_set_request_error(reqinfo, request, ret);
                    return SNMP_ERR_NOERROR;
                }
                /* An active row can only be deleted */
                if (entry &&
                    entry->flags & MTE_OBJECT_FLAG_ACTIVE &&
                    *request->requestvb->val.integer == RS_NOTINSERVICE ) {
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_NOERROR;
                }
                break;
            default:
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_NOTWRITABLE);
                return SNMP_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_RESERVE2:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset(mteOwner, 0, sizeof(mteOwner));
                    memcpy(mteOwner, tinfo->indexes->val.string,
                                     tinfo->indexes->val_len);
                    memset(mteOName, 0, sizeof(mteOName));
                    memcpy(mteOName,
                           tinfo->indexes->next_variable->val.string,
                           tinfo->indexes->next_variable->val_len);
                    ret = *tinfo->indexes->next_variable->next_variable->val.integer;

                    row = mteObjects_createEntry(mteOwner, mteOName, ret, 0);
                    if (!row) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_RESOURCEUNAVAILABLE);
                        return SNMP_ERR_NOERROR;
                    }
                    netsnmp_insert_tdata_row( request, row );
                }
            }
        }
        break;

    case MODE_SET_FREE:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_CREATEANDGO:
                case RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */ 
                    entry = (struct mteObject *)
                                netsnmp_tdata_extract_entry(request);
                    if (entry &&
                      !(entry->flags & MTE_OBJECT_FLAG_VALID)) {
                        row = (netsnmp_tdata_row *)
                                netsnmp_tdata_extract_row(request);
                        mteObjects_removeEntry( row );
                    }
                }
            }
        }
        break;

    case MODE_SET_ACTION:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteObject *) netsnmp_tdata_extract_entry(request);
            if (!entry) {
                /*
                 * New rows must be created via the RowStatus column
                 */
                netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_NOCREATION);
                                      /* or inconsistentName? */
                return SNMP_ERR_NOERROR;

            }
        }
        break;

    case MODE_SET_UNDO:
        break;

    case MODE_SET_COMMIT:
        /*
         * All these assignments are "unfailable", so it's
         *  (reasonably) safe to apply them in the Commit phase
         */
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct mteObject *) netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);

            switch (tinfo->colnum) {
            case COLUMN_MTEOBJECTSID:
                memset(entry->mteObjectID, 0, sizeof(entry->mteObjectID));
                memcpy(entry->mteObjectID, request->requestvb->val.objid,
                                           request->requestvb->val_len);
                entry->mteObjectID_len = request->requestvb->val_len/sizeof(oid);
                break;

            case COLUMN_MTEOBJECTSIDWILDCARD:
                if (*request->requestvb->val.integer == TV_TRUE)
                    entry->flags |=  MTE_OBJECT_FLAG_WILD;
                else
                    entry->flags &= ~MTE_OBJECT_FLAG_WILD;
                break;

            case COLUMN_MTEOBJECTSENTRYSTATUS:
                switch (*request->requestvb->val.integer) {
                case RS_ACTIVE:
                    entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
                    break;
                case RS_CREATEANDGO:
                    entry->flags |= MTE_OBJECT_FLAG_VALID;
                    entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
                    break;
                case RS_CREATEANDWAIT:
                    entry->flags |= MTE_OBJECT_FLAG_VALID;
                    break;

                case RS_DESTROY:
                    row = (netsnmp_tdata_row *)
                               netsnmp_tdata_extract_row(request);
                    mteObjects_removeEntry(row);
                }
            }
        }

        /** set up to save persistent store */
        snmp_store_needed(NULL);

        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 
    }
    return SNMP_ERR_NOERROR;
}
