#include <net-snmp/net-snmp-config.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_iterator.h>

#include "vacm_context.h"

static oid      vacm_context_oid[] = { 1, 3, 6, 1, 6, 3, 16, 1, 1 };

#define CONTEXTNAME_COLUMN 1

/*
 * return the index data from the first node in the agent's
 * subtree_context_cache list.
 */
netsnmp_variable_list *
get_first_context(void **my_loop_context, void **my_data_context,
                  netsnmp_variable_list * put_data,
                  netsnmp_iterator_info *iinfo)
{
    subtree_context_cache *context_ptr;
    context_ptr = get_top_context_cache();

    if (!context_ptr)
        return NULL;

    *my_loop_context = context_ptr;
    *my_data_context = context_ptr;

    snmp_set_var_value(put_data, context_ptr->context_name,
                       strlen(context_ptr->context_name));
    return put_data;
}

/*
 * return the next index data from the first node in the agent's
 * subtree_context_cache list.
 */
netsnmp_variable_list *
get_next_context(void **my_loop_context,
                 void **my_data_context,
                 netsnmp_variable_list * put_data,
                 netsnmp_iterator_info *iinfo)
{
    subtree_context_cache *context_ptr;

    if (!my_loop_context || !*my_loop_context)
        return NULL;

    context_ptr = (subtree_context_cache *) (*my_loop_context);
    context_ptr = context_ptr->next;
    *my_loop_context = context_ptr;
    *my_data_context = context_ptr;

    if (!context_ptr)
        return NULL;

    snmp_set_var_value(put_data, context_ptr->context_name,
                       strlen(context_ptr->context_name));
    return put_data;
}

void
init_vacm_context(void)
{
    /*
     * table vacm_context
     */
    netsnmp_handler_registration *my_handler;
    netsnmp_table_registration_info *table_info;
    netsnmp_iterator_info *iinfo;

    my_handler = netsnmp_create_handler_registration("vacm_context",
                                                     vacm_context_handler,
                                                     vacm_context_oid,
                                                     sizeof
                                                     (vacm_context_oid) /
                                                     sizeof(oid),
                                                     HANDLER_CAN_RONLY);

    if (!my_handler)
        return;

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    iinfo = SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);

    if (!table_info || !iinfo) {
        SNMP_FREE(table_info);
        SNMP_FREE(iinfo);
        SNMP_FREE(my_handler);
        return;
    }

    netsnmp_table_helper_add_index(table_info, ASN_OCTET_STR)
        table_info->min_column = 1;
    table_info->max_column = 1;
    iinfo->get_first_data_point = get_first_context;
    iinfo->get_next_data_point = get_next_context;
    iinfo->table_reginfo = table_info;
    netsnmp_register_table_iterator2(my_handler, iinfo);
}

/*
 * returns a list of known context names
 */

int
vacm_context_handler(netsnmp_mib_handler *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *requests)
{
    subtree_context_cache *context_ptr;

    for(; requests; requests = requests->next) {
        netsnmp_variable_list *var = requests->requestvb;

        if (requests->processed != 0)
            continue;


        context_ptr = (subtree_context_cache *)
            netsnmp_extract_iterator_context(requests);

        if (context_ptr == NULL) {
            snmp_log(LOG_ERR,
                     "vacm_context_handler called without data\n");
            continue;
        }

        switch (reqinfo->mode) {
        case MODE_GET:
            /*
             * if here we should have a context_ptr passed in already 
             */
            /*
             * only one column should ever reach us, so don't check it 
             */
            snmp_set_var_typed_value(var, ASN_OCTET_STR,
                                     context_ptr->context_name,
                                     strlen(context_ptr->context_name));

            break;
        default:
            /*
             * We should never get here, getnext already have been
             * handled by the table_iterator and we're read_only 
             */
            snmp_log(LOG_ERR,
                     "vacm_context table accessed as mode=%d.  We're improperly registered!",
                     reqinfo->mode);
            break;
        }
    }

    return SNMP_ERR_NOERROR;
}
