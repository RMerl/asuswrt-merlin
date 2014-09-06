#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

netsnmp_feature_require(ulong_instance)
netsnmp_feature_require(register_read_only_table_data)
netsnmp_feature_require(table_build_result)
netsnmp_feature_require(table_dataset)

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "testhandler.h"

#include <net-snmp/agent/table.h>
#include <net-snmp/agent/instance.h>
#include <net-snmp/agent/table_data.h>
#include <net-snmp/agent/table_dataset.h>

static oid      my_test_oid[4] = { 1, 2, 3, 4 };
static oid      my_table_oid[4] = { 1, 2, 3, 5 };
static oid      my_instance_oid[5] = { 1, 2, 3, 6, 1 };
static oid      my_data_table_oid[4] = { 1, 2, 3, 7 };
static oid      my_data_table_set_oid[4] = { 1, 2, 3, 8 };
static oid      my_data_ulong_instance[4] = { 1, 2, 3, 9 };

u_long          my_ulong = 0;

void
init_testhandler(void)
{
    /*
     * we're registering at .1.2.3.4 
     */
    netsnmp_handler_registration *my_test;
    netsnmp_table_registration_info *table_info;
    u_long          ind1;
    netsnmp_table_data *table;
    netsnmp_table_data_set *table_set;
    netsnmp_table_row *row;

    DEBUGMSGTL(("testhandler", "initializing\n"));

    /*
     * basic handler test
     */
    netsnmp_register_handler(netsnmp_create_handler_registration
                             ("myTest", my_test_handler, my_test_oid, 4,
                              HANDLER_CAN_RONLY));

    /*
     * instance handler test
     */

    netsnmp_register_instance(netsnmp_create_handler_registration
                              ("myInstance", my_test_instance_handler,
                               my_instance_oid, 5, HANDLER_CAN_RWRITE));

    netsnmp_register_ulong_instance("myulong",
                                    my_data_ulong_instance, 4,
                                    &my_ulong, NULL);

    /*
     * table helper test
     */

    my_test = netsnmp_create_handler_registration("myTable",
                                                  my_test_table_handler,
                                                  my_table_oid, 4,
                                                  HANDLER_CAN_RONLY);
    if (!my_test)
        return;

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    if (table_info == NULL)
        return;

    netsnmp_table_helper_add_indexes(table_info, ASN_INTEGER, ASN_INTEGER,
                                     0);
    table_info->min_column = 3;
    table_info->max_column = 3;
    netsnmp_register_table(my_test, table_info);

    /*
     * data table helper test
     */
    /*
     * we'll construct a simple table here with two indexes: an
     * integer and a string (why not).  It'll contain only one
     * column so the data pointer is merely the data in that
     * column. 
     */

    table = netsnmp_create_table_data("data_table_test");

    netsnmp_table_data_add_index(table, ASN_INTEGER);
    netsnmp_table_data_add_index(table, ASN_OCTET_STR);

    /*
     * 1 partridge in a pear tree 
     */
    row = netsnmp_create_table_data_row();
    ind1 = 1;
    netsnmp_table_row_add_index(row, ASN_INTEGER, &ind1, sizeof(ind1));
    netsnmp_table_row_add_index(row, ASN_OCTET_STR, "partridge",
                                strlen("partridge"));
    row->data = (void *) "pear tree";
    netsnmp_table_data_add_row(table, row);

    /*
     * 2 turtle doves 
     */
    row = netsnmp_create_table_data_row();
    ind1 = 2;
    netsnmp_table_row_add_index(row, ASN_INTEGER, &ind1, sizeof(ind1));
    netsnmp_table_row_add_index(row, ASN_OCTET_STR, "turtle",
                                strlen("turtle"));
    row->data = (void *) "doves";
    netsnmp_table_data_add_row(table, row);

    /*
     * we're going to register it as a normal table too, so we get the
     * automatically parsed column and index information 
     */
    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    if (table_info == NULL)
        return;

    netsnmp_table_helper_add_indexes(table_info, ASN_INTEGER,
                                     ASN_OCTET_STR, 0);
    table_info->min_column = 3;
    table_info->max_column = 3;

    netsnmp_register_read_only_table_data
        (netsnmp_create_handler_registration
         ("12days", my_data_table_handler, my_data_table_oid, 4,
          HANDLER_CAN_RONLY), table, table_info);

}

int
my_test_handler(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{

    oid             myoid1[] = { 1, 2, 3, 4, 5, 6 };
    static u_long   accesses = 0;

    DEBUGMSGTL(("testhandler", "Got request:\n"));
    /*
     * loop through requests 
     */
    while (requests) {
        netsnmp_variable_list *var = requests->requestvb;

        DEBUGMSGTL(("testhandler", "  oid:"));
        DEBUGMSGOID(("testhandler", var->name, var->name_length));
        DEBUGMSG(("testhandler", "\n"));

        switch (reqinfo->mode) {
        case MODE_GET:
            if (netsnmp_oid_equals(var->name, var->name_length, myoid1, 6)
                == 0) {
                snmp_set_var_typed_value(var, ASN_INTEGER,
                                         (u_char *) & accesses,
                                         sizeof(accesses));
                return SNMP_ERR_NOERROR;
            }
            break;

        case MODE_GETNEXT:
            if (snmp_oid_compare(var->name, var->name_length, myoid1, 6)
                < 0) {
                snmp_set_var_objid(var, myoid1, 6);
                snmp_set_var_typed_value(var, ASN_INTEGER,
                                         (u_char *) & accesses,
                                         sizeof(accesses));
                return SNMP_ERR_NOERROR;
            }
            break;

        default:
            netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
            break;
        }

        requests = requests->next;
    }
    return SNMP_ERR_NOERROR;
}

/*
 * functionally this is a simply a multiplication table for 12x12
 */

#define MAX_COLONE 12
#define MAX_COLTWO 12
#define RESULT_COLUMN 3
int
my_test_table_handler(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests)
{

    netsnmp_table_registration_info
        *handler_reg_info =
        (netsnmp_table_registration_info *) handler->prev->myvoid;

    netsnmp_table_request_info *table_info;
    u_long          result;
    int             x, y;

    while (requests) {
        netsnmp_variable_list *var = requests->requestvb;

        if (requests->processed != 0)
            continue;

        DEBUGMSGTL(("testhandler_table", "Got request:\n"));
        DEBUGMSGTL(("testhandler_table", "  oid:"));
        DEBUGMSGOID(("testhandler_table", var->name, var->name_length));
        DEBUGMSG(("testhandler_table", "\n"));

        table_info = netsnmp_extract_table_info(requests);
        if (table_info == NULL) {
            requests = requests->next;
            continue;
        }

        switch (reqinfo->mode) {
        case MODE_GETNEXT:
            /*
             * beyond our search range? 
             */
            if (table_info->colnum > RESULT_COLUMN)
                break;

            /*
             * below our minimum column? 
             */
            if (table_info->colnum < RESULT_COLUMN ||
                /*
                 * or no index specified 
                 */
                table_info->indexes->val.integer == 0) {
                table_info->colnum = RESULT_COLUMN;
                x = 0;
                y = 0;
            } else {
                x = *(table_info->indexes->val.integer);
                y = *(table_info->indexes->next_variable->val.integer);
            }

            if (table_info->number_indexes ==
                handler_reg_info->number_indexes) {
                y++;            /* GETNEXT is basically just y+1 for this table */
                if (y > MAX_COLTWO) {   /* (with wrapping) */
                    y = 0;
                    x++;
                }
            }
            if (x <= MAX_COLONE) {
                result = x * y;

                *(table_info->indexes->val.integer) = x;
                *(table_info->indexes->next_variable->val.integer) = y;
                netsnmp_table_build_result(reginfo, requests,
                                           table_info, ASN_INTEGER,
                                           (u_char *) & result,
                                           sizeof(result));
            }

            break;

        case MODE_GET:
            if (var->type == ASN_NULL) {        /* valid request if ASN_NULL */
                /*
                 * is it the right column? 
                 */
                if (table_info->colnum == RESULT_COLUMN &&
                    /*
                     * and within the max boundries? 
                     */
                    *(table_info->indexes->val.integer) <= MAX_COLONE &&
                    *(table_info->indexes->next_variable->val.integer)
                    <= MAX_COLTWO) {

                    /*
                     * then, the result is column1 * column2 
                     */
                    result = *(table_info->indexes->val.integer) *
                        *(table_info->indexes->next_variable->val.integer);
                    snmp_set_var_typed_value(var, ASN_INTEGER,
                                             (u_char *) & result,
                                             sizeof(result));
                }
            }
            break;

        }

        requests = requests->next;
    }

    return SNMP_ERR_NOERROR;
}

#define TESTHANDLER_SET_NAME "my_test"
int
my_test_instance_handler(netsnmp_mib_handler *handler,
                         netsnmp_handler_registration *reginfo,
                         netsnmp_agent_request_info *reqinfo,
                         netsnmp_request_info *requests)
{

    static u_long   accesses = 0;
    u_long         *accesses_cache = NULL;

    DEBUGMSGTL(("testhandler", "Got instance request:\n"));

    switch (reqinfo->mode) {
    case MODE_GET:
        snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
                                 (u_char *) & accesses, sizeof(accesses));
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        if (requests->requestvb->type != ASN_UNSIGNED)
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_WRONGTYPE);
        break;

    case MODE_SET_RESERVE2:
        /*
         * store old info for undo later 
         */
        memdup((u_char **) & accesses_cache,
               (u_char *) & accesses, sizeof(accesses));
        if (accesses_cache == NULL) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_RESOURCEUNAVAILABLE);
            return SNMP_ERR_NOERROR;
        }
        netsnmp_request_add_list_data(requests,
                                      netsnmp_create_data_list
                                      (TESTHANDLER_SET_NAME,
                                       accesses_cache, free));
        break;

    case MODE_SET_ACTION:
        /*
         * update current 
         */
        accesses = *(requests->requestvb->val.integer);
        DEBUGMSGTL(("testhandler", "updated accesses -> %d\n", accesses));
        break;

    case MODE_SET_UNDO:
        accesses =
            *((u_long *) netsnmp_request_get_list_data(requests,
                                                       TESTHANDLER_SET_NAME));
        break;

    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
        /*
         * nothing to do 
         */
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }

    return SNMP_ERR_NOERROR;
}

int
my_data_table_handler(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests)
{

    char           *column3;
    netsnmp_table_request_info *table_info;
    netsnmp_table_row *row;

    while (requests) {
        if (requests->processed) {
            requests = requests->next;
            continue;
        }

        /*
         * extract our stored data and table info 
         */
        row = netsnmp_extract_table_row(requests);
        table_info = netsnmp_extract_table_info(requests);
        if (row)
            column3 = (char *) row->data;
        if (!row || !table_info || !column3)
            continue;

        /*
         * there's only one column, we don't need to check if it's right 
         */
        netsnmp_table_data_build_result(reginfo, reqinfo, requests, row,
                                        table_info->colnum,
                                        ASN_OCTET_STR, column3,
                                        strlen(column3));
        requests = requests->next;
    }
    return SNMP_ERR_NOERROR;
}
