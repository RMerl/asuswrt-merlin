#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/scalar.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/library/snmp_logging.h>
#include "agent/nsLogging.h"

netsnmp_feature_require(logging_external)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(table_iterator_insert_context)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

/*
 * OID and columns for the logging table.
 */

#define  NSLOGGING_TYPE		3
#define  NSLOGGING_MAXLEVEL	4
#define  NSLOGGING_STATUS	5

void
init_nsLogging(void)
{
    netsnmp_table_registration_info *table_info;
    netsnmp_iterator_info           *iinfo;

    const oid nsLoggingTable_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 7, 2, 1};

    /*
     * Register the table.
     * We need to define the column structure and indexing....
     */

    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
    if (!table_info) {
        return;
    }
    netsnmp_table_helper_add_indexes(table_info, ASN_INTEGER,
                                                 ASN_PRIV_IMPLIED_OCTET_STR, 0);
    table_info->min_column = NSLOGGING_TYPE;
    table_info->max_column = NSLOGGING_STATUS;


    /*
     * .... and the iteration information ....
     */
    iinfo      = SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
    if (!iinfo) {
        return;
    }
    iinfo->get_first_data_point = get_first_logging_entry;
    iinfo->get_next_data_point  = get_next_logging_entry;
    iinfo->table_reginfo        = table_info;


    /*
     * .... and register the table with the agent.
     */
    netsnmp_register_table_iterator2(
        netsnmp_create_handler_registration(
            "tzLoggingTable", handle_nsLoggingTable,
            nsLoggingTable_oid, OID_LENGTH(nsLoggingTable_oid),
            HANDLER_CAN_RWRITE),
        iinfo);
}


/*
 * nsLoggingTable handling
 */

netsnmp_variable_list *
get_first_logging_entry(void **loop_context, void **data_context,
                      netsnmp_variable_list *index,
                      netsnmp_iterator_info *data)
{
    long temp;
    netsnmp_log_handler  *logh_head = get_logh_head();
    if ( !logh_head )
        return NULL;

    temp = logh_head->priority;
    snmp_set_var_value(index, (u_char*)&temp,
		                 sizeof(temp));
    if ( logh_head->token )
        snmp_set_var_value(index->next_variable, (const u_char*)logh_head->token,
		                                   strlen(logh_head->token));
    else
        snmp_set_var_value(index->next_variable, NULL, 0);
    *loop_context = (void*)logh_head;
    *data_context = (void*)logh_head;
    return index;
}

netsnmp_variable_list *
get_next_logging_entry(void **loop_context, void **data_context,
                      netsnmp_variable_list *index,
                      netsnmp_iterator_info *data)
{
    long temp;
    netsnmp_log_handler *logh = (netsnmp_log_handler *)*loop_context;
    logh = logh->next;

    if ( !logh )
        return NULL;

    temp = logh->priority;
    snmp_set_var_value(index, (u_char*)&temp,
		                 sizeof(temp));
    if ( logh->token )
        snmp_set_var_value(index->next_variable, (const u_char*)logh->token,
		                                   strlen(logh->token));
    else
        snmp_set_var_value(index->next_variable, NULL, 0);
    *loop_context = (void*)logh;
    *data_context = (void*)logh;
    return index;
}


int
handle_nsLoggingTable(netsnmp_mib_handler *handler,
                netsnmp_handler_registration *reginfo,
                netsnmp_agent_request_info *reqinfo,
                netsnmp_request_info *requests)
{
    long temp;
    netsnmp_request_info       *request     = NULL;
    netsnmp_table_request_info *table_info  = NULL;
    netsnmp_log_handler        *logh        = NULL;
    netsnmp_variable_list      *idx         = NULL;

    switch (reqinfo->mode) {

    case MODE_GET:
        for (request=requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            logh = (netsnmp_log_handler*)netsnmp_extract_iterator_context(request);
            table_info  =                netsnmp_extract_table_info(request);

            switch (table_info->colnum) {
            case NSLOGGING_TYPE:
                if (!logh) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
                    continue;
		}
		temp = logh->type;
	        snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                         (u_char*)&temp,
                                            sizeof(temp));
	        break;

            case NSLOGGING_MAXLEVEL:
                if (!logh) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
                    continue;
		}
		temp = logh->pri_max;
	        snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                         (u_char*)&temp,
                                            sizeof(temp));
	        break;

            case NSLOGGING_STATUS:
                if (!logh) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHINSTANCE);
                    continue;
		}
		temp = (logh->type ?
	                   (logh->enabled ?
	                      RS_ACTIVE:
	                      RS_NOTINSERVICE) :
	                    RS_NOTREADY);
	        snmp_set_var_typed_value(request->requestvb, ASN_INTEGER,
                                         (u_char*)&temp, sizeof(temp));
	        break;

            default:
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
                continue;
	    }
	}
	break;


#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        for (request=requests; request; request=request->next) {
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            logh = (netsnmp_log_handler*)netsnmp_extract_iterator_context(request);
            table_info  =                 netsnmp_extract_table_info(request);

            switch (table_info->colnum) {
            case NSLOGGING_TYPE:
                if ( request->requestvb->type != ASN_INTEGER ) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                if (*request->requestvb->val.integer < 0 ) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
		/*
		 * It's OK to create a new logging entry
		 *  (either in one go, or built up using createAndWait)
		 *  but it's not possible to change the type of an entry
		 *  once it's been created.
		 */
                if (logh && logh->type) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_ERR_NOTWRITABLE);
                    return SNMP_ERR_NOTWRITABLE;
		}
	        break;

            case NSLOGGING_MAXLEVEL:
                if ( request->requestvb->type != ASN_INTEGER ) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
                if (*request->requestvb->val.integer < 0 ||
                    *request->requestvb->val.integer > 7 ) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
	        break;

            case NSLOGGING_STATUS:
                if ( request->requestvb->type != ASN_INTEGER ) {
                    netsnmp_set_request_error(reqinfo, request, SNMP_ERR_WRONGTYPE);
                    return SNMP_ERR_WRONGTYPE;
                }
		switch ( *request->requestvb->val.integer ) {
                case RS_ACTIVE:
                case RS_NOTINSERVICE:
                    /*
		     * Can only work on existing rows
		     */
                    if (!logh) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_INCONSISTENTVALUE);
                        return SNMP_ERR_INCONSISTENTVALUE;
                    }
	            break;

                case RS_CREATEANDWAIT:
                case RS_CREATEANDGO:
                    /*
		     * Can only work with new rows
		     */
                    if (logh) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_INCONSISTENTVALUE);
                        return SNMP_ERR_INCONSISTENTVALUE;
                    }

                    /*
                     *  Normally, we'd create the row at a later stage
                     *   (probably during the RESERVE2 or ACTION passes)
                     *
                     *  But we need to check that the values are
                     *   consistent during the ACTION pass (which is the
                     *   latest that an error can be safely handled),
                     *   so the values all need to be set up before this
                     *      (i.e. during the RESERVE2 pass)
                     *  So the new row needs to be created before that
                     *   in order to have somewhere to put them.
                     *
                     *  That's why we're doing this here.
                     */
                    idx = table_info->indexes;
	            logh = netsnmp_register_loghandler(
				    /* not really, but we need a valid type */
				    NETSNMP_LOGHANDLER_STDOUT,
				    *idx->val.integer);
                    if (!logh) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_GENERR); /* ??? */
                        return SNMP_ERR_GENERR;
                    }
                    idx = idx->next_variable;
	            logh->type  = 0;
	            logh->token = strdup((char *) idx->val.string);
                    netsnmp_insert_iterator_context(request, (void*)logh);
	            break;

                case RS_DESTROY:
                    /*
		     * Can work with new or existing rows
		     */
                    break;

                case RS_NOTREADY:
		default:
                    netsnmp_set_request_error(reqinfo, request,
                                              SNMP_ERR_WRONGVALUE);
                    return SNMP_ERR_WRONGVALUE;
                }
                break;

            default:
                netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
                return SNMP_NOSUCHOBJECT;
	    }
	}
	break;


    case MODE_SET_RESERVE2:
        for (request=requests; request; request=request->next) {
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            logh = (netsnmp_log_handler*)netsnmp_extract_iterator_context(request);
            table_info  =                 netsnmp_extract_table_info(request);

            switch (table_info->colnum) {
            case NSLOGGING_TYPE:
                /*
		 * If we're creating a row using createAndGo,
		 * we need to set the type early, so that we
		 * can validate it in the ACTION pass.
		 *
		 * Remember that we need to be able to reverse this
		 */
                if ( logh )
                    logh->type = *request->requestvb->val.integer;
	        break;
            /*
	     * Don't need to handle nsLogToken or nsLogStatus in this pass
	     */
	    }
	}
	break;

    case MODE_SET_ACTION:
        for (request=requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            logh = (netsnmp_log_handler*)netsnmp_extract_iterator_context(request);
            table_info  =                 netsnmp_extract_table_info(request);

            switch (table_info->colnum) {
            case NSLOGGING_STATUS:
                /*
		 * This is where we can check the internal consistency
		 * of the request.  Basically, for a row to be marked
		 * 'active', then there needs to be a valid type value.
		 */
		switch ( *request->requestvb->val.integer ) {
                case RS_ACTIVE:
                case RS_CREATEANDGO:
                    if ( !logh || !logh->type ) {
                        netsnmp_set_request_error(reqinfo, request,
                                                  SNMP_ERR_INCONSISTENTVALUE);
                        return SNMP_ERR_INCONSISTENTVALUE;
		    }
	            break;
		}
	        break;
            /*
	     * Don't need to handle nsLogToken or nsLogType in this pass
	     */
	    }
	}
	break;

    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        /*
         * If any resources were allocated in either of the
         *  two RESERVE passes, they need to be released here,
         *  and any assignments (in RESERVE2) reversed.
         *
         * Nothing additional will have been done during ACTION
         *  so this same code can do for UNDO as well.
         */
        for (request=requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            logh = (netsnmp_log_handler*)netsnmp_extract_iterator_context(request);
            table_info  =                 netsnmp_extract_table_info(request);

            switch (table_info->colnum) {
            case NSLOGGING_TYPE:
                /*
		 * If we've been setting the type, and the request
		 * has failed, then revert to an unset type.
		 *
		 * We need to be careful here - if the reason it failed is
		 *  that the type was already set, then we shouldn't "undo"
		 *  the assignment (since it won't actually have been made).
		 *
		 * Check the current value against the 'new' one.  If they're
		 * the same, then this is probably a successful assignment,
		 * and the failure was elsewhere, so we need to undo it.
		 *  (Or else there was an attempt to write the same value!)
		 */
                if ( logh && logh->type == *request->requestvb->val.integer )
                    logh->type = 0;
	        break;

            case NSLOGGING_STATUS:
                temp = *request->requestvb->val.integer;
                if ( logh && ( temp == RS_CREATEANDGO ||
                               temp == RS_CREATEANDWAIT)) {
		    netsnmp_remove_loghandler( logh );
		}
	        break;
            /*
	     * Don't need to handle nsLogToken in this pass
	     */
	    }
	}
	break;


    case MODE_SET_COMMIT:
        for (request=requests; request; request=request->next) {
            if (request->processed != 0)
                continue;
            if ( request->status != 0 ) {
                return SNMP_ERR_NOERROR;	/* Already got an error */
            }
            logh = (netsnmp_log_handler*)netsnmp_extract_iterator_context(request);
            if (!logh) {
                netsnmp_set_request_error(reqinfo, request, SNMP_ERR_COMMITFAILED);
                return SNMP_ERR_COMMITFAILED;	/* Shouldn't happen! */
            }
            table_info  =                 netsnmp_extract_table_info(request);

            switch (table_info->colnum) {
            case NSLOGGING_MAXLEVEL:
                logh->pri_max = *request->requestvb->val.integer;
	        break;

            case NSLOGGING_STATUS:
                switch (*request->requestvb->val.integer) {
                    case RS_ACTIVE:
                    case RS_CREATEANDGO:
                        netsnmp_enable_this_loghandler(logh);
                        break;
                    case RS_NOTINSERVICE:
                    case RS_CREATEANDWAIT:
                        netsnmp_disable_this_loghandler(logh);
                        break;
		    case RS_DESTROY:
		        netsnmp_remove_loghandler( logh );
                        break;
		}
	        break;
	    }
	}
	break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    }

    return SNMP_ERR_NOERROR;
}
