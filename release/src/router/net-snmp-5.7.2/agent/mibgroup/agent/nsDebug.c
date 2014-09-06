#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/scalar.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "agent/nsDebug.h"

#define nsConfigDebug 1, 3, 6, 1, 4, 1, 8072, 1, 7, 1

void
init_nsDebug(void)
{
    /*
     * OIDs for the debugging control scalar objects
     *
     * Note that these we're registering the full object rather
     *  than the (sole) valid instance in each case, in order
     *  to handle requests for invalid instances properly.
     */
    const oid nsDebugEnabled_oid[]    = { nsConfigDebug, 1};
    const oid nsDebugOutputAll_oid[]  = { nsConfigDebug, 2};
    const oid nsDebugDumpPdu_oid[]    = { nsConfigDebug, 3};

    /*
     * ... and for the token table.
     */

#define  DBGTOKEN_PREFIX	2
#define  DBGTOKEN_ENABLED	3
#define  DBGTOKEN_STATUS	4
    const oid nsDebugTokenTable_oid[] = { nsConfigDebug, 4};

    netsnmp_table_registration_info *table_info;
    netsnmp_iterator_info           *iinfo;

    /*
     * Register the scalar objects...
     */
    DEBUGMSGTL(("nsDebugScalars", "Initializing\n"));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration(
            "nsDebugEnabled", handle_nsDebugEnabled,
            nsDebugEnabled_oid, OID_LENGTH(nsDebugEnabled_oid),
            HANDLER_CAN_RWRITE)
        );
    netsnmp_register_scalar(
        netsnmp_create_handler_registration(
            "nsDebugOutputAll", handle_nsDebugOutputAll,
            nsDebugOutputAll_oid, OID_LENGTH(nsDebugOutputAll_oid),
            HANDLER_CAN_RWRITE)
        );
    netsnmp_register_scalar(
        netsnmp_create_handler_registration(
            "nsDebugDumpPdu", handle_nsDebugDumpPdu,
            nsDebugDumpPdu_oid, OID_LENGTH(nsDebugDumpPdu_oid),
            HANDLER_CAN_RWRITE)
        );

    /*
     * ... and the table.
     * We need to define the column structure and indexing....
     */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    if (!table_info) {
        return;
    }
    netsnmp_table_helper_add_indexes(table_info, ASN_PRIV_IMPLIED_OCTET_STR, 0);
    table_info->min_column = DBGTOKEN_STATUS;
    table_info->max_column = DBGTOKEN_STATUS;


    /*
     * .... and the iteration information ....
     */
    iinfo      = SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
    if (!iinfo) {
        return;
    }
    iinfo->get_first_data_point = get_first_debug_entry;
    iinfo->get_next_data_point = get_next_debug_entry;
    iinfo->table_reginfo        = table_info;


    /*
     * .... and register the table with the agent.
     */
    netsnmp_register_table_iterator2(
        netsnmp_create_handler_registration(
            "tzDebugTable", handle_nsDebugTable,
            nsDebugTokenTable_oid, OID_LENGTH(nsDebugTokenTable_oid),
            HANDLER_CAN_RWRITE),
        iinfo);
}


int
handle_nsDebugEnabled(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{
    long enabled;
    netsnmp_request_info *request=NULL;

    switch (reqinfo->mode) {

    case MODE_GET:
	enabled = snmp_get_do_debugging();
	if ( enabled==0 )
	    enabled=2;		/* false */
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
	    snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                     (u_char*)&enabled, sizeof(enabled));
	}
	break;


#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            if ( request->requestvb->type != ASN_INTEGER ) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                return SNMP_ERR_WRONGTYPE;
            }
            if (( *request->requestvb->val.integer != 1 ) &&
                ( *request->requestvb->val.integer != 2 )) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGVALUE);
                return SNMP_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->val.integer;
	if (enabled == 2 )	/* false */
	    enabled = 0;
	snmp_set_do_debugging( enabled );
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }

    return SNMP_ERR_NOERROR;
}


int
handle_nsDebugOutputAll(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{
    long enabled;
    netsnmp_request_info *request=NULL;

    switch (reqinfo->mode) {

    case MODE_GET:
	enabled = snmp_get_do_debugging();
	if ( enabled==0 )
	    enabled=2;		/* false */
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
	    snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                     (u_char*)&enabled, sizeof(enabled));
	}
	break;


#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            if ( request->requestvb->type != ASN_INTEGER ) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                return SNMP_ERR_WRONGTYPE;
            }
            if (( *request->requestvb->val.integer != 1 ) &&
                ( *request->requestvb->val.integer != 2 )) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGVALUE);
                return SNMP_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->val.integer;
	if (enabled == 2 )	/* false */
	    enabled = 0;
	snmp_set_do_debugging( enabled );
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }

    return SNMP_ERR_NOERROR;
}


int
handle_nsDebugDumpPdu(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{
    long enabled;
    netsnmp_request_info *request=NULL;

    switch (reqinfo->mode) {

    case MODE_GET:
	enabled = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
	                                 NETSNMP_DS_LIB_DUMP_PACKET);
	if ( enabled==0 )
	    enabled=2;		/* false */
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
	    snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                     (u_char*)&enabled, sizeof(enabled));
	}
	break;


#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            if ( request->requestvb->type != ASN_INTEGER ) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                return SNMP_ERR_WRONGTYPE;
            }
            if (( *request->requestvb->val.integer != 1 ) &&
                ( *request->requestvb->val.integer != 2 )) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGVALUE);
                return SNMP_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->val.integer;
	if (enabled == 2 )	/* false */
	    enabled = 0;
	netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
	                       NETSNMP_DS_LIB_DUMP_PACKET, enabled);
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }

    return SNMP_ERR_NOERROR;
}

/*
 * var_tzIntTableFixed():
 *   Handle the tzIntTable as a fixed table of NUMBER_TZ_ENTRIES rows,
 *    with the timezone offset hardwired to be the same as the index.
 */

netsnmp_variable_list *
get_first_debug_entry(void **loop_context, void **data_context,
                      netsnmp_variable_list *index,
                      netsnmp_iterator_info *data)
{
    int i;

    for (i=0; i<debug_num_tokens; i++) {
        /* skip excluded til mib is updated */
        if (dbg_tokens[i].token_name && (dbg_tokens[i].enabled != 2))
            break;
    }
    if ( i==debug_num_tokens )
        return NULL;

    snmp_set_var_value(index, dbg_tokens[i].token_name,
		       strlen(dbg_tokens[i].token_name));
    *loop_context = (void*)(intptr_t)i;
    *data_context = (void*)&dbg_tokens[i];
    return index;
}

netsnmp_variable_list *
get_next_debug_entry(void **loop_context, void **data_context,
                      netsnmp_variable_list *index,
                      netsnmp_iterator_info *data)
{
    int i = (int)(intptr_t)*loop_context;

    for (i++; i<debug_num_tokens; i++) {
        /* skip excluded til mib is updated */
        if (dbg_tokens[i].token_name && (dbg_tokens[i].enabled != 2))
            break;
    }
    if ( i==debug_num_tokens )
        return NULL;

    snmp_set_var_value(index, dbg_tokens[i].token_name,
		       strlen(dbg_tokens[i].token_name));
    *loop_context = (void*)(intptr_t)i;
    *data_context = (void*)&dbg_tokens[i];
    return index;
}


int
handle_nsDebugTable(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{
    long status;
    netsnmp_request_info       *request    =NULL;
    netsnmp_table_request_info *table_info    =NULL;
    netsnmp_token_descr        *debug_entry=NULL;

    switch (reqinfo->mode) {

    case MODE_GET:
        for (request=requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            debug_entry = (netsnmp_token_descr*)
                           netsnmp_extract_iterator_context(request);
            if (!debug_entry)
                continue;
	    status = (debug_entry->enabled ? RS_ACTIVE : RS_NOTINSERVICE);
	    snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                     (u_char*)&status, sizeof(status));
	}
	break;


#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            if ( request->requestvb->type != ASN_INTEGER ) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                return SNMP_ERR_WRONGTYPE;
            }

            debug_entry = (netsnmp_token_descr*)
                           netsnmp_extract_iterator_context(request);
            switch (*request->requestvb->val.integer) {
            case RS_ACTIVE:
            case RS_NOTINSERVICE:
                /*
		 * These operations require an existing row
		 */
                if (!debug_entry) {
		    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_INCONSISTENTVALUE;
		}
		break;

            case RS_CREATEANDWAIT:
            case RS_CREATEANDGO:
                /*
		 * These operations assume the row doesn't already exist
		 */
                if (debug_entry) {
		    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_INCONSISTENTVALUE);
                    return SNMP_ERR_INCONSISTENTVALUE;
		}
		break;

            case RS_DESTROY:
                /*
		 * This operation can work regardless
		 */
		break;

            case RS_NOTREADY:
            default:
		netsnmp_set_request_error(reqinfo, request,
                                          SNMP_ERR_WRONGVALUE);
                return SNMP_ERR_WRONGVALUE;
	    }
        }
        break;


    case MODE_SET_COMMIT:
	for (request = requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }

            switch (*request->requestvb->val.integer) {
            case RS_ACTIVE:
            case RS_NOTINSERVICE:
                /*
		 * Update the enabled field appropriately
		 */
                debug_entry = (netsnmp_token_descr*)
                               netsnmp_extract_iterator_context(request);
                if (debug_entry)
                    debug_entry->enabled =
                        (*request->requestvb->val.integer == RS_ACTIVE);
		break;

            case RS_CREATEANDWAIT:
            case RS_CREATEANDGO:
                /*
		 * Create the entry, and set the enabled field appropriately
		 */
                table_info = netsnmp_extract_table_info(request);
                debug_register_tokens((char *) table_info->indexes->val.string);
#ifdef UMMMMM
                if (*request->requestvb->val.integer == RS_CREATEANDWAIT) {
		    /* XXX - how to locate the entry ??  */
		    debug_entry->enabled = 0;
		}
#endif
		break;

            case RS_DESTROY:
                /*
		 * XXX - there's no "remove" API  :-(
		 */
                debug_entry = (netsnmp_token_descr*)
                               netsnmp_extract_iterator_context(request);
                if (debug_entry) {
		    debug_entry->enabled = 0;
		    free(debug_entry->token_name);
		    debug_entry->token_name = NULL;
		}
		break;
	    }
        }
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }

    return SNMP_ERR_NOERROR;
}

