/*
 * DisMan Expression MIB:
 *    Implementation of the expValueTable MIB interface
 * See 'expValue.c' for active evaluation of expressions.
 *
 *  (Based roughly on mib2c.raw-table.conf output)
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "expValue.h"
#include "expValueTable.h"

/** Initializes the expValueTable module */
void
init_expValueTable(void)
{
    static oid  expValueTable_oid[]   = { 1, 3, 6, 1, 2, 1, 90, 1, 3, 1 };
    size_t      expValueTable_oid_len = OID_LENGTH(expValueTable_oid);
    netsnmp_handler_registration *reg;
    netsnmp_table_registration_info *table_info;

    reg =
        netsnmp_create_handler_registration("expValueTable",
                                            expValueTable_handler,
                                            expValueTable_oid,
                                            expValueTable_oid_len,
                                            HANDLER_CAN_RONLY);

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    netsnmp_table_helper_add_indexes(table_info,
                                     ASN_OCTET_STR, /* expExpressionOwner */
                                     ASN_OCTET_STR, /* expExpressionName  */
                                                    /* expValueInstance   */
                                     ASN_PRIV_IMPLIED_OBJECT_ID,
                                     0);

    table_info->min_column = COLUMN_EXPVALUECOUNTER32VAL;
    table_info->max_column = COLUMN_EXPVALUECOUNTER64VAL;

    netsnmp_register_table(reg, table_info);
    DEBUGMSGTL(("disman:expr:init", "Expression Value Table\n"));
}


netsnmp_variable_list *
expValueTable_getEntry(netsnmp_variable_list * indexes,
                       int mode, unsigned int colnum)
{
    struct expExpression  *exp;
    netsnmp_variable_list *res, *vp, *vp2;
    oid nullInstance[] = {0, 0, 0};
    int  plen;
    size_t len;
    unsigned int type = colnum-1; /* column object subIDs and type
                                      enumerations are off by one. */

    if (!indexes || !indexes->next_variable ||
        !indexes->next_variable->next_variable ) {
        /* XXX - Shouldn't happen! */
        return NULL;
    }

    DEBUGMSGTL(( "disman:expr:val", "get (%d) entry (%s, %s, ", mode,
               indexes->val.string, indexes->next_variable->val.string));
    DEBUGMSGOID(("disman:expr:val",
               indexes->next_variable->next_variable->val.objid,
               indexes->next_variable->next_variable->val_len/sizeof(oid)));
    DEBUGMSG((   "disman:expr:val", ")\n"));

    /*
     * Locate the expression that we've been asked to evaluate
     */
    if (!indexes->val_len || !indexes->next_variable->val_len ) {
        /*
         * Incomplete expression specification
         */
        if (mode == MODE_GETNEXT || mode == MODE_GETBULK) {
            exp = expExpression_getFirstEntry();
            DEBUGMSGTL(( "disman:expr:val", "first entry (%p)\n", exp ));
        } else {
            DEBUGMSGTL(( "disman:expr:val", "incomplete request\n" ));
            return NULL;        /* No match */
        }
    } else {
        exp = expExpression_getEntry( (char*)indexes->val.string,
                                      (char*)indexes->next_variable->val.string);
        DEBUGMSGTL(( "disman:expr:val", "using entry (%p)\n", exp ));
    }

    /*
     * We know what type of value was requested,
     *   so ignore any non-matching expressions.
     */
    while (exp && exp->expValueType != type) {
        if (mode != MODE_GETNEXT && mode != MODE_GETBULK) {
            DEBUGMSGTL(( "disman:expr:val", "wrong type (%d != %ld)\n",
                          type, (exp ? exp->expValueType : 0 )));
            return NULL;        /* Wrong type */
        }
NEXT_EXP:
        exp = expExpression_getNextEntry( exp->expOwner, exp->expName );
        DEBUGMSGTL(( "disman:expr:val", "using next entry (%p)\n", exp ));
    }
    if (!exp) {
        DEBUGMSGTL(( "disman:expr:val", "no more entries\n"));
        return NULL;    /* No match (of the required type) */
    }


    /*
     * Now consider which instance of the chosen expression is needed
     */
    vp  = indexes->next_variable->next_variable;
    if ( mode == MODE_GET ) {
        /*
         * For a GET request, check that the specified value instance
         *   is valid, and evaluate the expression using this.
         */
        if ( !vp || !vp->val_len ) {
            DEBUGMSGTL(( "disman:expr:val", "no instance index\n"));
            return NULL;  /* No instance provided */
        }
        if ( vp->val.objid[0] != 0 ) {
            DEBUGMSGTL(( "disman:expr:val",
                         "non-zero instance (%" NETSNMP_PRIo "d)\n", vp->val.objid[0]));
            return NULL;  /* Invalid instance */
        }

        if (exp->expPrefix_len == 0 ) {
            /*
             * The only valid instance for a non-wildcarded
             *     expression is .0.0.0
             */
            if ( vp->val_len != 3*sizeof(oid) ||
                 vp->val.objid[1] != 0 ||
                 vp->val.objid[2] != 0 ) {
                DEBUGMSGTL(( "disman:expr:val", "invalid scalar instance\n"));
                return NULL;
            }
            res = expValue_evaluateExpression( exp, NULL, 0 );
            DEBUGMSGTL(( "disman:expr:val", "scalar get returned (%p)\n", res));
        } else {
            /*
             * Otherwise, skip the leading '.0' and use
             *   the remaining instance subidentifiers.
             */
            res = expValue_evaluateExpression( exp, vp->val.objid+1,
                                           vp->val_len/sizeof(oid)-1);
            DEBUGMSGTL(( "disman:expr:val", "w/card get returned (%p)\n", res));
        }
    } else {
        /*
         * For a GETNEXT request, identify the appropriate next
         *   value instance, and evaluate the expression using
         *   that, updating the index list appropriately.
         */
        if ( vp->val_len > 0 && vp->val.objid[0] != 0 ) {
            DEBUGMSGTL(( "disman:expr:val",
                         "non-zero next instance (%" NETSNMP_PRIo "d)\n", vp->val.objid[0]));
            return NULL;        /* All valid instances start with .0 */
        }
        plen = exp->expPrefix_len;
        if (plen == 0 ) {
            /*
             * The only valid instances for GETNEXT on a
             *   non-wildcarded expression are .0 and .0.0
             *   Anything else is too late.
             */
            if ((vp->val_len > 2*sizeof(oid)) ||
                (vp->val_len == 2*sizeof(oid) &&
                      vp->val.objid[1] != 0)) {
                DEBUGMSGTL(( "disman:expr:val", "invalid scalar next instance\n"));
                return NULL;        /* Invalid instance */
            }
     
            /*
             * Make sure the index varbind list refers to the
             *   (only) valid instance of this expression,
             *   and evaluate it.
             */
            snmp_set_var_typed_value( indexes, ASN_OCTET_STR,
                       (u_char*)exp->expOwner, strlen(exp->expOwner));
            snmp_set_var_typed_value( indexes->next_variable, ASN_OCTET_STR,
                       (u_char*)exp->expName,  strlen(exp->expName));
            snmp_set_var_typed_value( vp, ASN_PRIV_IMPLIED_OBJECT_ID,
                       (u_char*)nullInstance, 3*sizeof(oid));
            res = expValue_evaluateExpression( exp, NULL, 0 );
            DEBUGMSGTL(( "disman:expr:val", "scalar next returned (%p)\n", res));
        } else {
            /*
             * Now comes the interesting case - finding the
             *   appropriate instance of a wildcarded expression.
             */
            if ( vp->val_len == 0 ) {
                 if ( !exp->pvars ) {
                     DEBUGMSGTL(( "disman:expr:val", "no instances\n"));
                     goto NEXT_EXP;
                 }
                 DEBUGMSGTL(( "disman:expr:val", "using first instance\n"));
                 vp2 = exp->pvars;
            } else {
                 /*
                  * Search the list of instances for the (first) greater one
                  *   XXX - This comparison relies on the OID of the prefix
                  *         object being the same length as the wildcarded
                  *         parameter objects.  It ain't necessarily so.
                  */
                 for ( vp2 = exp->pvars; vp2; vp2 = vp2->next_variable ) {
                     if ( snmp_oid_compare( vp2->name        + plen,
                                            vp2->name_length - plen,
                                            vp->name,
                                            vp->name_length) < 0 ) {
                         DEBUGMSGTL(( "disman:expr:val", "next instance "));
                         DEBUGMSGOID(("disman:expr:val",  vp2->name, vp2->name_length ));
                         DEBUGMSG((   "disman:expr:val", "\n"));
                         break;
                     }
                 }
                 if (!vp2) {
                     DEBUGMSGTL(( "disman:expr:val", "no next instance\n"));
                     goto NEXT_EXP;
                 }
            }
            snmp_set_var_typed_value( indexes, ASN_OCTET_STR,
                       (u_char*)exp->expOwner, strlen(exp->expOwner));
            snmp_set_var_typed_value( indexes->next_variable, ASN_OCTET_STR,
                       (u_char*)exp->expName,  strlen(exp->expName));
            if (vp2) {
                len = vp2->name_length - exp->expPrefix_len;
                snmp_set_var_typed_value( vp, ASN_PRIV_IMPLIED_OBJECT_ID,
                      (u_char*)(vp2->name+exp->expPrefix_len), len);
            } else {
                len = 1;
            }
            res = expValue_evaluateExpression( exp, vp->val.objid+1, len-1);
            DEBUGMSGTL(( "disman:expr:val", "w/card next returned (%p)\n", res));
        }
    }
    return res;
}

/** handles requests for the expValueTable table */
int
expValueTable_handler(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests)
{

    netsnmp_request_info       *request;
    netsnmp_table_request_info *tinfo;
    netsnmp_variable_list      *value;
    oid    expValueOID[] = { 1, 3, 6, 1, 2, 1, 90, 1, 3, 1, 1, 99 };
    size_t expValueOID_len = OID_LENGTH(expValueOID);
    oid    name_buf[ MAX_OID_LEN ];
    size_t name_buf_len = MAX_OID_LEN;

    DEBUGMSGTL(("disman:expr:mib", "Expression Value Table handler (%d)\n",
                                    reqinfo->mode));
    switch (reqinfo->mode) {
    case MODE_GET:
    case MODE_GETNEXT:
        for (request = requests; request; request = request->next) {
            tinfo = netsnmp_extract_table_info(request);
            value = expValueTable_getEntry(tinfo->indexes,
                                           reqinfo->mode,
                                           tinfo->colnum);
            if (!value || !value->val.integer) {
                netsnmp_set_request_error(reqinfo, request,
                                         (reqinfo->mode == MODE_GET) ? 
                                                 SNMP_NOSUCHINSTANCE :
                                                 SNMP_ENDOFMIBVIEW);
                continue;
            }
            if ( reqinfo->mode == MODE_GETNEXT ) {
                 /*
                  * Need to update the request varbind OID
                  *   to match the instance just evaluated.
                  * (XXX - Is this the appropriate mechanism?)
                  */
                build_oid_noalloc( name_buf, MAX_OID_LEN, &name_buf_len,
                           expValueOID, expValueOID_len, tinfo->indexes );
                name_buf[ expValueOID_len -1 ] = tinfo->colnum;
                snmp_set_var_objid(request->requestvb, name_buf, name_buf_len);
            }

            switch (tinfo->colnum) {
            case COLUMN_EXPVALUECOUNTER32VAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_COUNTER,
                                          *value->val.integer);
                break;
            case COLUMN_EXPVALUEUNSIGNED32VAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_UNSIGNED,
                                          *value->val.integer);
                break;
            case COLUMN_EXPVALUETIMETICKSVAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_TIMETICKS,
                                          *value->val.integer);
                break;
            case COLUMN_EXPVALUEINTEGER32VAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_INTEGER,
                                          *value->val.integer);
                break;
            case COLUMN_EXPVALUEIPADDRESSVAL:
                snmp_set_var_typed_integer(request->requestvb, ASN_IPADDRESS,
                                          *value->val.integer);
                break;
            case COLUMN_EXPVALUEOCTETSTRINGVAL:
                snmp_set_var_typed_value(  request->requestvb, ASN_OCTET_STR,
                                           value->val.string,  value->val_len);
                break;
            case COLUMN_EXPVALUEOIDVAL:
                snmp_set_var_typed_value(  request->requestvb, ASN_OBJECT_ID,
                                   (u_char *)value->val.objid, value->val_len);
                break;
            case COLUMN_EXPVALUECOUNTER64VAL:
                snmp_set_var_typed_value(  request->requestvb, ASN_COUNTER64,
                               (u_char *)value->val.counter64, value->val_len);
                break;
            }
        }
        break;

    }
    DEBUGMSGTL(("disman:expr:mib", "Expression Value handler - done \n"));
    return SNMP_ERR_NOERROR;
}
