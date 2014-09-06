/*
 * DisMan Expression MIB:
 *    Implementation of the expExpressionErrorTable MIB interface
 * See 'expExpression.c' for active behaviour of this table.
 *
 *  (Based on mib2c.table_data.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/expr/expExpression.h"
#include "disman/expr/expErrorTable.h"

netsnmp_feature_require(table_tdata)

/* Initializes the expExpressionErrorTable module */
void
init_expErrorTable(void)
{
    static oid  expErrorTable_oid[]   = { 1, 3, 6, 1, 2, 1, 90, 1, 2, 2 };
    size_t      expErrorTable_oid_len = OID_LENGTH(expErrorTable_oid);
    netsnmp_handler_registration    *reg;
    netsnmp_table_registration_info *table_info;

    /*
     * Ensure the expression table container is available...
     */
    init_expr_table_data();

    /*
     * ... then set up the MIB interface to the expExpressionErrorTable slice
     */
    reg = netsnmp_create_handler_registration("expErrorTable",
                                            expErrorTable_handler,
                                            expErrorTable_oid,
                                            expErrorTable_oid_len,
                                            HANDLER_CAN_RWRITE);

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                          /* index: expExpressionOwner */
                                     ASN_OCTET_STR,
                                          /* index: expExpressionName */
                                     ASN_OCTET_STR,
                                     0);

    table_info->min_column = COLUMN_EXPERRORTIME;
    table_info->max_column = COLUMN_EXPERRORINSTANCE;

    /* Register this using the (common) expr_table_data container */
    netsnmp_tdata_register(reg, expr_table_data, table_info);
    DEBUGMSGTL(("disman:expr:init", "Expression Error Table container (%p)\n",
                                     expr_table_data));
}


/** handles requests for the expExpressionErrorTable table */
int
expErrorTable_handler(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    struct expExpression       *entry;

    DEBUGMSGTL(("disman:expr:mib", "Expression Error Table handler (%d)\n",
                                    reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            entry = (struct expExpression *)
                    netsnmp_tdata_extract_entry(request);
            tinfo = netsnmp_extract_table_info(request);
            if (!entry || !(entry->flags & EXP_FLAG_VALID))
                continue;

            /*
             * "Entries only appear in this table ... when there
             *  has been an error for that [matching] expression"
             */
            if (entry->expErrorCount == 0)
                continue;

            switch (tinfo->colnum) {
            case COLUMN_EXPERRORTIME:
                snmp_set_var_typed_integer(request->requestvb, ASN_TIMETICKS,
                                           entry->expErrorTime);
                break;
            case COLUMN_EXPERRORINDEX:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->expErrorIndex);
                break;
            case COLUMN_EXPERRORCODE:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                           entry->expErrorCode);
                break;
            case COLUMN_EXPERRORINSTANCE:
                snmp_set_var_typed_value(request->requestvb, ASN_OBJECT_ID,
                              (u_char *) entry->expErrorInstance,
                                         entry->expErrorInst_len*sizeof(oid));
                break;
            }
        }
        break;
    }
    DEBUGMSGTL(("disman:expr:mib", "Expression Error handler - done \n"));
    return SNMP_ERR_NOERROR;
}
