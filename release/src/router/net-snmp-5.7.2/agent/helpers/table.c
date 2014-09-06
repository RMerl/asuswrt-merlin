/*
 * table.c 
 */

/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/table.h>

#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(oid_stash)
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/library/snmp_assert.h>

netsnmp_feature_child_of(table_all, mib_helpers)

netsnmp_feature_child_of(table_build_result, table_all)
netsnmp_feature_child_of(table_get_or_create_row_stash, table_all)
netsnmp_feature_child_of(registration_owns_table_info, table_all)
netsnmp_feature_child_of(table_sparse, table_all)

static void     table_helper_cleanup(netsnmp_agent_request_info *reqinfo,
                                     netsnmp_request_info *request,
                                     int status);
static void     table_data_free_func(void *data);
static int
sparse_table_helper_handler(netsnmp_mib_handler *handler,
                            netsnmp_handler_registration *reginfo,
                            netsnmp_agent_request_info *reqinfo,
                            netsnmp_request_info *requests);

/** @defgroup table table
 *  Helps you implement a table.
 *  @ingroup handler
 *
 *  This handler helps you implement a table by doing some of the
 *  processing for you.
 *  
 *  This handler truly shows the power of the new handler mechanism.
 *  By creating a table handler and injecting it into your calling
 *  chain, or by using the netsnmp_register_table() function to register your
 *  table, you get access to some pre-parsed information.
 *  Specifically, the table handler pulls out the column number and
 *  indexes from the request oid so that you don't have to do the
 *  complex work to do that parsing within your own code.
 *
 *  To do this, the table handler needs to know up front how your
 *  table is structured.  To inform it about this, you fill in a
 *  table_registeration_info structure that is passed to the table
 *  handler.  It contains the asn index types for the table as well as
 *  the minimum and maximum column that should be used.
 *  
 *  @{
 */

/** Given a netsnmp_table_registration_info object, creates a table handler.
 *  You can use this table handler by injecting it into a calling
 *  chain.  When the handler gets called, it'll do processing and
 *  store it's information into the request->parent_data structure.
 *
 *  The table helper handler pulls out the column number and indexes from 
 *  the request oid so that you don't have to do the complex work of
 *  parsing within your own code.
 *
 *  @param tabreq is a pointer to a netsnmp_table_registration_info struct.
 *	The table handler needs to know up front how your table is structured.
 *	A netsnmp_table_registeration_info structure that is 
 *	passed to the table handler should contain the asn index types for the 
 *	table as well as the minimum and maximum column that should be used.
 *
 *  @return Returns a pointer to a netsnmp_mib_handler struct which contains
 *	the handler's name and the access method
 *
 */
netsnmp_mib_handler *
netsnmp_get_table_handler(netsnmp_table_registration_info *tabreq)
{
    netsnmp_mib_handler *ret = NULL;

    if (!tabreq) {
        snmp_log(LOG_INFO, "netsnmp_get_table_handler(NULL) called\n");
        return NULL;
    }

    ret = netsnmp_create_handler(TABLE_HANDLER_NAME, table_helper_handler);
    if (ret) {
        ret->myvoid = (void *) tabreq;
        tabreq->number_indexes = count_varbinds(tabreq->indexes);
    }
    return ret;
}

/** Configures a handler such that table registration information is freed by
 *  netsnmp_handler_free(). Should only be called if handler->myvoid points to
 *  an object of type netsnmp_table_registration_info.
 */
void netsnmp_handler_owns_table_info(netsnmp_mib_handler *handler)
{
    netsnmp_assert(handler);
    netsnmp_assert(handler->myvoid);
    handler->data_clone
	= (void *(*)(void *)) netsnmp_table_registration_info_clone;
    handler->data_free
	= (void (*)(void *)) netsnmp_table_registration_info_free;
}

/** Configures a handler such that table registration information is freed by
 *  netsnmp_handler_free(). Should only be called if reg->handler->myvoid
 *  points to an object of type netsnmp_table_registration_info.
 */
#ifndef NETSNMP_FEATURE_REMOVE_REGISTRATION_OWNS_TABLE_INFO
void netsnmp_registration_owns_table_info(netsnmp_handler_registration *reg)
{
    if (reg)
        netsnmp_handler_owns_table_info(reg->handler);
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTRATION_OWNS_TABLE_INFO */

/** creates a table handler given the netsnmp_table_registration_info object,
 *  inserts it into the request chain and then calls
 *  netsnmp_register_handler() to register the table into the agent.
 */
int
netsnmp_register_table(netsnmp_handler_registration *reginfo,
                       netsnmp_table_registration_info *tabreq)
{
    int rc = netsnmp_inject_handler(reginfo, netsnmp_get_table_handler(tabreq));
    if (SNMPERR_SUCCESS != rc)
        return rc;

    return netsnmp_register_handler(reginfo);
}

int
netsnmp_unregister_table(netsnmp_handler_registration *reginfo)
{
    /* Locate "this" reginfo */
    /* SNMP_FREE(reginfo->myvoid); */
    return netsnmp_unregister_handler(reginfo);
}

/** Extracts the processed table information from a given request.
 *  Call this from subhandlers on a request to extract the processed
 *  netsnmp_request_info information.  The resulting information includes the
 *  index values and the column number.
 *
 * @param request populated netsnmp request structure
 *
 * @return populated netsnmp_table_request_info structure
 */
NETSNMP_INLINE netsnmp_table_request_info *
netsnmp_extract_table_info(netsnmp_request_info *request)
{
    return (netsnmp_table_request_info *)
        netsnmp_request_get_list_data(request, TABLE_HANDLER_NAME);
}

/** extracts the registered netsnmp_table_registration_info object from a
 *  netsnmp_handler_registration object */
netsnmp_table_registration_info *
netsnmp_find_table_registration_info(netsnmp_handler_registration *reginfo)
{
    return (netsnmp_table_registration_info *)
        netsnmp_find_handler_data_by_name(reginfo, TABLE_HANDLER_NAME);
}

/** implements the table helper handler */
int
table_helper_handler(netsnmp_mib_handler *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *requests)
{

    netsnmp_request_info *request;
    netsnmp_table_registration_info *tbl_info;
    int             oid_index_pos;
    unsigned int    oid_column_pos;
    unsigned int    tmp_idx;
    ssize_t 	    tmp_len;
    int             incomplete, out_of_range;
    int             status = SNMP_ERR_NOERROR, need_processing = 0;
    oid            *tmp_name;
    netsnmp_table_request_info *tbl_req_info;
    netsnmp_variable_list *vb;

    if (!reginfo || !handler)
        return SNMPERR_GENERR;

    oid_index_pos  = reginfo->rootoid_len + 2;
    oid_column_pos = reginfo->rootoid_len + 1;
    tbl_info = (netsnmp_table_registration_info *) handler->myvoid;

    if ((!handler->myvoid) || (!tbl_info->indexes)) {
        snmp_log(LOG_ERR, "improperly registered table found\n");
        snmp_log(LOG_ERR, "name: %s, table info: %p, indexes: %p\n",
                 handler->handler_name, handler->myvoid, tbl_info->indexes);

        /*
         * XXX-rks: unregister table? 
         */
        return SNMP_ERR_GENERR;
    }

    DEBUGIF("helper:table:req") {
        DEBUGMSGTL(("helper:table:req",
                    "Got %s (%d) mode request for handler %s: base oid:",
                    se_find_label_in_slist("agent_mode", reqinfo->mode),
                    reqinfo->mode, handler->handler_name));
        DEBUGMSGOID(("helper:table:req", reginfo->rootoid,
                     reginfo->rootoid_len));
        DEBUGMSG(("helper:table:req", "\n"));
    }
    
    /*
     * if the agent request info has a state reference, then this is a 
     * later pass of a set request and we can skip all the lookup stuff.
     *
     * xxx-rks: this might break for handlers which only handle one varbind
     * at a time... those handlers should not save data by their handler_name
     * in the netsnmp_agent_request_info. 
     */
    if (netsnmp_agent_get_list_data(reqinfo, handler->next->handler_name)) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
        if (MODE_IS_SET(reqinfo->mode)) {
            return netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                             requests);
        } else {
#endif /* NETSNMP_NO_WRITE_SUPPORT */
/** XXX-rks: memory leak. add cleanup handler? */
            netsnmp_free_agent_data_sets(reqinfo);
#ifndef NETSNMP_NO_WRITE_SUPPORT
        }
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }

#ifndef NETSNMP_NO_WRITE_SUPPORT
    if ( MODE_IS_SET(reqinfo->mode) &&
         (reqinfo->mode != MODE_SET_RESERVE1)) {
        /*
         * for later set modes, we can skip all the index parsing,
         * and we always need to let child handlers have a chance
         * to clean up, if they were called in the first place (i.e. have
         * a valid table info pointer).
         */
        if(NULL == netsnmp_extract_table_info(requests)) {
            DEBUGMSGTL(("helper:table","no table info for set - skipping\n"));
        }
        else
            need_processing = 1;
    }
    else {
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        /*
         * for GETS, only continue if we have at least one valid request.
         * for RESERVE1, only continue if we have indexes for all requests.
         */
           
    /*
     * loop through requests
     */

    for (request = requests; request; request = request->next) {
        netsnmp_variable_list *var = request->requestvb;

        DEBUGMSGOID(("verbose:table", var->name, var->name_length));
        DEBUGMSG(("verbose:table", "\n"));

        if (request->processed) {
            DEBUGMSG(("verbose:table", "already processed\n"));
            continue;
        }
        netsnmp_assert(request->status == SNMP_ERR_NOERROR);

        /*
         * this should probably be handled further up 
         */
        if ((reqinfo->mode == MODE_GET) && (var->type != ASN_NULL)) {
            /*
             * valid request if ASN_NULL 
             */
            DEBUGMSGTL(("helper:table",
                        "  GET var type is not ASN_NULL\n"));
            netsnmp_set_request_error(reqinfo, request,
                                      SNMP_ERR_WRONGTYPE);
            continue;
        }

#ifndef NETSNMP_NO_WRITE_SUPPORT
        if (reqinfo->mode == MODE_SET_RESERVE1) {
            DEBUGIF("helper:table:set") {
                u_char         *buf = NULL;
                size_t          buf_len = 0, out_len = 0;
                DEBUGMSGTL(("helper:table:set", " SET_REQUEST for OID: "));
                DEBUGMSGOID(("helper:table:set", var->name, var->name_length));
                out_len = 0;
                if (sprint_realloc_by_type(&buf, &buf_len, &out_len, 1,
                                           var, NULL, NULL, NULL)) {
                    DEBUGMSG(("helper:table:set"," type=%d(%02x), value=%s\n",
                              var->type, var->type, buf));
                } else {
                    if (buf != NULL) {
                        DEBUGMSG(("helper:table:set",
                                  " type=%d(%02x), value=%s [TRUNCATED]\n",
                                  var->type, var->type, buf));
                    } else {
                        DEBUGMSG(("helper:table:set",
                                  " type=%d(%02x), value=[NIL] [TRUNCATED]\n",
                                  var->type, var->type));
                    }
                }
                if (buf != NULL) {
                    free(buf);
                }
            }
        }
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        /*
         * check to make sure its in table range 
         */

        out_of_range = 0;
        /*
         * if our root oid is > var->name and this is not a GETNEXT, 
         * then the oid is out of range. (only compare up to shorter 
         * length) 
         */
        if (reginfo->rootoid_len > var->name_length)
            tmp_len = var->name_length;
        else
            tmp_len = reginfo->rootoid_len;
        if (snmp_oid_compare(reginfo->rootoid, reginfo->rootoid_len,
                             var->name, tmp_len) > 0) {
            if (reqinfo->mode == MODE_GETNEXT) {
                if (var->name != var->name_loc)
                    SNMP_FREE(var->name);
                snmp_set_var_objid(var, reginfo->rootoid,
                                   reginfo->rootoid_len);
            } else {
                DEBUGMSGTL(("helper:table", "  oid is out of range.\n"));
                out_of_range = 1;
            }
        }
        /*
         * if var->name is longer than the root, make sure it is 
         * table.1 (table.ENTRY).  
         */
        else if ((var->name_length > reginfo->rootoid_len) &&
                 (var->name[reginfo->rootoid_len] != 1)) {
            if ((var->name[reginfo->rootoid_len] < 1) &&
                (reqinfo->mode == MODE_GETNEXT)) {
                var->name[reginfo->rootoid_len] = 1;
                var->name_length = reginfo->rootoid_len;
            } else {
                out_of_range = 1;
                DEBUGMSGTL(("helper:table", "  oid is out of range.\n"));
            }
        }
        /*
         * if it is not in range, then mark it in the request list 
         * because we can't process it, and set an error so
         * nobody else wastes time trying to process it either.  
         */
        if (out_of_range) {
            DEBUGMSGTL(("helper:table", "  Not processed: "));
            DEBUGMSGOID(("helper:table", var->name, var->name_length));
            DEBUGMSG(("helper:table", "\n"));

            /*
             *  Reject requests of the form 'myTable.N'   (N != 1)
             */
#ifndef NETSNMP_NO_WRITE_SUPPORT
            if (reqinfo->mode == MODE_SET_RESERVE1)
                table_helper_cleanup(reqinfo, request,
                                     SNMP_ERR_NOTWRITABLE);
            else
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            if (reqinfo->mode == MODE_GET)
                table_helper_cleanup(reqinfo, request,
                                     SNMP_NOSUCHOBJECT);
            continue;
        }


        /*
         * Check column ranges; set-up to pull out indexes from OID. 
         */

        incomplete = 0;
        tbl_req_info = netsnmp_extract_table_info(request);
        if (NULL == tbl_req_info) {
            tbl_req_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_request_info);
            if (tbl_req_info == NULL) {
                table_helper_cleanup(reqinfo, request,
                                     SNMP_ERR_GENERR);
                continue;
            }
            tbl_req_info->reg_info = tbl_info;
            tbl_req_info->indexes = snmp_clone_varbind(tbl_info->indexes);
            tbl_req_info->number_indexes = 0;       /* none yet */
            netsnmp_request_add_list_data(request,
                                          netsnmp_create_data_list
                                          (TABLE_HANDLER_NAME,
                                           (void *) tbl_req_info,
                                           table_data_free_func));
        } else {
            DEBUGMSGTL(("helper:table", "  using existing tbl_req_info\n "));
        }

        /*
         * do we have a column?
         */
        if (var->name_length > oid_column_pos) {
            /*
             * oid is long enough to contain COLUMN info
             */
            DEBUGMSGTL(("helper:table:col",
                        "  have at least a column (%" NETSNMP_PRIo "d)\n",
                        var->name[oid_column_pos]));
            if (var->name[oid_column_pos] < tbl_info->min_column) {
                DEBUGMSGTL(("helper:table:col",
                            "    but it's less than min (%d)\n",
                            tbl_info->min_column));
                if (reqinfo->mode == MODE_GETNEXT) {
                    /*
                     * fix column, truncate useless column info 
                     */
                    var->name_length = oid_column_pos;
                    tbl_req_info->colnum = tbl_info->min_column;
                } else
                    out_of_range = 1;
            } else if (var->name[oid_column_pos] > tbl_info->max_column)
                out_of_range = 1;
            else
                tbl_req_info->colnum = var->name[oid_column_pos];

            if (out_of_range) {
                /*
                 * this is out of range...  remove from requests, free
                 * memory 
                 */
                DEBUGMSGTL(("helper:table",
                            "    oid is out of range. Not processed: "));
                DEBUGMSGOID(("helper:table", var->name, var->name_length));
                DEBUGMSG(("helper:table", "\n"));

                /*
                 *  Reject requests of the form 'myEntry.N'   (invalid N)
                 */
#ifndef NETSNMP_NO_WRITE_SUPPORT
                if (reqinfo->mode == MODE_SET_RESERVE1)
                    table_helper_cleanup(reqinfo, request,
                                         SNMP_ERR_NOTWRITABLE);
                else if (reqinfo->mode == MODE_GET)
#endif /* NETSNMP_NO_WRITE_SUPPORT */
                    table_helper_cleanup(reqinfo, request,
                                         SNMP_NOSUCHOBJECT);
                continue;
            }
            /*
             * use column verification 
             */
            else if (tbl_info->valid_columns) {
                tbl_req_info->colnum =
                    netsnmp_closest_column(var->name[oid_column_pos],
                                           tbl_info->valid_columns);
                DEBUGMSGTL(("helper:table:col", "    closest column is %d\n",
                            tbl_req_info->colnum));
                /*
                 * xxx-rks: document why the continue...
                 */
                if (tbl_req_info->colnum == 0)
                    continue;
                if (tbl_req_info->colnum != var->name[oid_column_pos]) {
                    DEBUGMSGTL(("helper:table:col",
                                "    which doesn't match req "
                                "%" NETSNMP_PRIo "d - truncating index info\n",
                                var->name[oid_column_pos]));
                    /*
                     * different column! truncate useless index info 
                     */
                    var->name_length = oid_column_pos + 1; /* pos is 0 based */
                }
            }
            /*
             * var->name_length may have changed - check again 
             */
            if ((int)var->name_length <= oid_index_pos) { /* pos is 0 based */
                DEBUGMSGTL(("helper:table", "    not enough for indexes\n"));
                tbl_req_info->index_oid_len = 0; /** none available */
            } else {
                /*
                 * oid is long enough to contain INDEX info
                 */
                tbl_req_info->index_oid_len =
                    var->name_length - oid_index_pos;
                DEBUGMSGTL(("helper:table", "    have %lu bytes of index\n",
                            (unsigned long)tbl_req_info->index_oid_len));
                netsnmp_assert(tbl_req_info->index_oid_len < MAX_OID_LEN);
                memcpy(tbl_req_info->index_oid, &var->name[oid_index_pos],
                       tbl_req_info->index_oid_len * sizeof(oid));
                tmp_name = tbl_req_info->index_oid;
            }
        } else if (reqinfo->mode == MODE_GETNEXT ||
                   reqinfo->mode == MODE_GETBULK) {
            /*
             * oid is NOT long enough to contain column or index info, so start
             * at the minimum column. Set index oid len to 0 because we don't
             * have any index info in the OID.
             */
            DEBUGMSGTL(("helper:table", "  no column/index in request\n"));
            tbl_req_info->index_oid_len = 0;
            tbl_req_info->colnum = tbl_info->min_column;
        } else {
            /*
             * oid is NOT long enough to contain index info,
             * so we can't do anything with it.
             *
             * Reject requests of the form 'myTable' or 'myEntry'
             */
            if (reqinfo->mode == MODE_GET ) {
                table_helper_cleanup(reqinfo, request, SNMP_NOSUCHOBJECT);
#ifndef NETSNMP_NO_WRITE_SUPPORT
            } else if (reqinfo->mode == MODE_SET_RESERVE1 ) {
                table_helper_cleanup(reqinfo, request, SNMP_ERR_NOTWRITABLE);
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            }
            continue;
        }

        /*
         * set up tmp_len to be the number of OIDs we have beyond the column;
         * these should be the index(s) for the table. If the index_oid_len
         * is 0, set tmp_len to -1 so that when we try to parse the index below,
         * we just zero fill everything.
         */
        if (tbl_req_info->index_oid_len == 0) {
            incomplete = 1;
            tmp_len = -1;
        } else
            tmp_len = tbl_req_info->index_oid_len;


        /*
         * for each index type, try to extract the index from var->name
         */
        DEBUGMSGTL(("helper:table", "  looking for %d indexes\n",
                    tbl_info->number_indexes));
        for (tmp_idx = 0, vb = tbl_req_info->indexes;
             tmp_idx < tbl_info->number_indexes;
             ++tmp_idx, vb = vb->next_variable) {
            size_t parsed_oid_len;

            if (incomplete && tmp_len) {
                /*
                 * incomplete/illegal OID, set up dummy 0 to parse 
                 */
                DEBUGMSGTL(("helper:table",
                            "  oid indexes not complete: "));
                DEBUGMSGOID(("helper:table", var->name, var->name_length));
                DEBUGMSG(("helper:table", "\n"));

                /*
                 * no sense in trying anymore if this is a GET/SET. 
                 *
                 * Reject requests of the form 'myObject'   (no instance)
                 */
                tmp_len = 0;
                tmp_name = NULL;
                break;
            }
            /*
             * try and parse current index 
             */
            netsnmp_assert(tmp_len >= 0);
            parsed_oid_len = tmp_len;
            if (parse_one_oid_index(&tmp_name, &parsed_oid_len,
                                    vb, 1) != SNMPERR_SUCCESS) {
                incomplete = 1;
                tmp_len = -1;   /* is this necessary? Better safe than
                                 * sorry */
            } else {
                tmp_len = parsed_oid_len;
                DEBUGMSGTL(("helper:table", "  got 1 (incomplete=%d)\n",
                            incomplete));
                /*
                 * do not count incomplete indexes 
                 */
                if (incomplete)
                    continue;
                ++tbl_req_info->number_indexes; /** got one ok */
                if (tmp_len <= 0) {
                    incomplete = 1;
                    tmp_len = -1;       /* is this necessary? Better safe
                                         * than sorry */
                }
            }
        }                       /** for loop */

        DEBUGIF("helper:table:results") {
                unsigned int    count;
                u_char         *buf = NULL;
                size_t          buf_len = 0, out_len = 0;
                DEBUGMSGTL(("helper:table:results", "  found %d indexes\n",
                            tbl_req_info->number_indexes));
                DEBUGMSGTL(("helper:table:results",
                            "  column: %d, indexes: %d",
                            tbl_req_info->colnum,
                            tbl_req_info->number_indexes));
                for (vb = tbl_req_info->indexes, count = 0;
                     vb && count < tbl_req_info->number_indexes;
                     count++, vb = vb->next_variable) {
                    out_len = 0;
                    if (sprint_realloc_by_type(&buf, &buf_len, &out_len, 1,
                                               vb, NULL, NULL, NULL)) {
                        DEBUGMSG(("helper:table:results",
                                  "   index: type=%d(%02x), value=%s",
                                  vb->type, vb->type, buf));
                    } else {
                        if (buf != NULL) {
                            DEBUGMSG(("helper:table:results",
                                      "   index: type=%d(%02x), value=%s [TRUNCATED]",
                                      vb->type, vb->type, buf));
                        } else {
                            DEBUGMSG(("helper:table:results",
                                      "   index: type=%d(%02x), value=[NIL] [TRUNCATED]",
                                      vb->type, vb->type));
                        }
                    }
                }
                if (buf != NULL) {
                    free(buf);
                }
                DEBUGMSG(("helper:table:results", "\n"));
        }


        /*
         * do we have sufficient index info to continue?
         */

        if ((reqinfo->mode != MODE_GETNEXT) &&
            ((tbl_req_info->number_indexes != tbl_info->number_indexes) ||
             (tmp_len != -1))) {

            DEBUGMSGTL(("helper:table",
                        "invalid index(es) for table - skipping\n"));

#ifndef NETSNMP_NO_WRITE_SUPPORT
            if ( MODE_IS_SET(reqinfo->mode) ) {
                /*
                 * no point in continuing without indexes for set.
                 */
                netsnmp_assert(reqinfo->mode == MODE_SET_RESERVE1);
                /** clear first request so we wont try to run FREE mode */
                netsnmp_free_request_data_sets(requests);
                /** set actual error */
                table_helper_cleanup(reqinfo, request, SNMP_ERR_NOCREATION);
                need_processing = 0; /* don't call next handler */
                break;
            }
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            table_helper_cleanup(reqinfo, request, SNMP_NOSUCHINSTANCE);
            continue;
        }
        netsnmp_assert(request->status == SNMP_ERR_NOERROR);
        
        ++need_processing;

    }                           /* for each request */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    }
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    /*
     * bail if there is nothing for our child handlers
     */
    if (0 == need_processing)
        return status;

    /*
     * call our child access function 
     */
    status =
        netsnmp_call_next_handler(handler, reginfo, reqinfo, requests);

    /*
     * check for sparse tables
     */
    if (reqinfo->mode == MODE_GETNEXT)
        sparse_table_helper_handler( handler, reginfo, reqinfo, requests );

    return status;
}

#define SPARSE_TABLE_HANDLER_NAME "sparse_table"

/** implements the sparse table helper handler
 * @internal
 *
 * @note
 * This function is static to prevent others from calling it
 * directly. It it automatically called by the table helper,
 * 
 */
static int
sparse_table_helper_handler(netsnmp_mib_handler *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *requests)
{
    int             status = SNMP_ERR_NOERROR;
    netsnmp_request_info *request;
    oid             coloid[MAX_OID_LEN];
    netsnmp_table_request_info *table_info;

    /*
     * since we don't call child handlers, warn if one was registered
     * beneath us. A special exception for the table helper, which calls
     * the handler directly. Use handle custom flag to only log once.
     */
    if((table_helper_handler != handler->access_method) &&
       (NULL != handler->next)) {
        /*
         * always warn if called without our own handler. If we
         * have our own handler, use custom bit 1 to only log once.
         */
        if((sparse_table_helper_handler != handler->access_method) ||
           !(handler->flags & MIB_HANDLER_CUSTOM1)) {
            snmp_log(LOG_WARNING, "handler (%s) registered after sparse table "
                     "hander will not be called\n",
                     handler->next->handler_name ?
                     handler->next->handler_name : "" );
            if(sparse_table_helper_handler == handler->access_method)
                handler->flags |= MIB_HANDLER_CUSTOM1;
        }
    }

    if (reqinfo->mode == MODE_GETNEXT) {
        for(request = requests ; request; request = request->next) {
            if ((request->requestvb->type == ASN_NULL && request->processed) ||
                request->delegated)
                continue;
            if (request->requestvb->type == SNMP_NOSUCHINSTANCE) {
                /*
                 * get next skipped this value for this column, we
                 * need to keep searching forward 
                 */
                DEBUGMSGT(("sparse", "retry for NOSUCHINSTANCE\n"));
                request->requestvb->type = ASN_PRIV_RETRY;
            }
            if (request->requestvb->type == SNMP_NOSUCHOBJECT ||
                request->requestvb->type == SNMP_ENDOFMIBVIEW) {
                /*
                 * get next has completely finished with this column,
                 * so we need to try with the next column (if any)
                 */
                DEBUGMSGT(("sparse", "retry for NOSUCHOBJECT\n"));
                table_info = netsnmp_extract_table_info(request);
                table_info->colnum = netsnmp_table_next_column(table_info);
                if (0 != table_info->colnum) {
                    memcpy(coloid, reginfo->rootoid,
                           reginfo->rootoid_len * sizeof(oid));
                    coloid[reginfo->rootoid_len]   = 1;   /* table.entry node */
                    coloid[reginfo->rootoid_len+1] = table_info->colnum;
                    snmp_set_var_objid(request->requestvb,
                                       coloid, reginfo->rootoid_len + 2);
                    
                    request->requestvb->type = ASN_PRIV_RETRY;
                }
                else {
                    /*
                     * If we don't have column info, reset to null so
                     * the agent will move on to the next table.
                     */
                    request->requestvb->type = ASN_NULL;
                }
            }
        }
    }
    return status;
}

/** create sparse table handler
 */
#ifndef NETSNMP_FEATURE_REMOVE_TABLE_SPARSE
netsnmp_mib_handler *
netsnmp_sparse_table_handler_get(void)
{
    return netsnmp_create_handler(SPARSE_TABLE_HANDLER_NAME,
                                  sparse_table_helper_handler);
}

/** creates a table handler given the netsnmp_table_registration_info object,
 *  inserts it into the request chain and then calls
 *  netsnmp_register_handler() to register the table into the agent.
 */
int
netsnmp_sparse_table_register(netsnmp_handler_registration *reginfo,
                       netsnmp_table_registration_info *tabreq)
{
    netsnmp_mib_handler *handler1, *handler2;
    int rc;

    handler1 = netsnmp_create_handler(SPARSE_TABLE_HANDLER_NAME,
                                     sparse_table_helper_handler);
    if (NULL == handler1)
        return SNMP_ERR_GENERR;

    handler2 = netsnmp_get_table_handler(tabreq);
    if (NULL == handler2 ) {
        netsnmp_handler_free(handler1);
        return SNMP_ERR_GENERR;
    }

    rc = netsnmp_inject_handler(reginfo, handler1);
    if (SNMPERR_SUCCESS != rc) {
        netsnmp_handler_free(handler1);
        netsnmp_handler_free(handler2);
        return rc;
    }

    rc = netsnmp_inject_handler(reginfo, handler2);
    if (SNMPERR_SUCCESS != rc) {
        /** handler1 is in reginfo... remove and free?? */
        netsnmp_handler_free(handler2);
        return rc;
    }

    /** both handlers now in reginfo, so nothing to do on error */
    return netsnmp_register_handler(reginfo);
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_SPARSE */


#ifndef NETSNMP_FEATURE_REMOVE_TABLE_BUILD_RESULT
/** Builds the result to be returned to the agent given the table information.
 *  Use this function to return results from lowel level handlers to
 *  the agent.  It takes care of building the proper resulting oid
 *  (containing proper indexing) and inserts the result value into the
 *  returning varbind.
 */
int
netsnmp_table_build_result(netsnmp_handler_registration *reginfo,
                           netsnmp_request_info *reqinfo,
                           netsnmp_table_request_info *table_info,
                           u_char type, u_char * result, size_t result_len)
{

    netsnmp_variable_list *var;

    if (!reqinfo || !table_info)
        return SNMPERR_GENERR;

    var = reqinfo->requestvb;

    if (var->name != var->name_loc)
        free(var->name);
    var->name = NULL;

    if (netsnmp_table_build_oid(reginfo, reqinfo, table_info) !=
        SNMPERR_SUCCESS)
        return SNMPERR_GENERR;

    snmp_set_var_typed_value(var, type, result, result_len);

    return SNMPERR_SUCCESS;
}

/** given a registration info object, a request object and the table
 *  info object it builds the request->requestvb->name oid from the
 *  index values and column information found in the table_info
 *  object. Index values are extracted from the table_info varbinds.
 */
int
netsnmp_table_build_oid(netsnmp_handler_registration *reginfo,
                        netsnmp_request_info *reqinfo,
                        netsnmp_table_request_info *table_info)
{
    oid             tmpoid[MAX_OID_LEN];
    netsnmp_variable_list *var;

    if (!reginfo || !reqinfo || !table_info)
        return SNMPERR_GENERR;

    /*
     * xxx-rks: inefficent. we do a copy here, then build_oid does it
     *          again. either come up with a new utility routine, or
     *          do some hijinks here to eliminate extra copy.
     *          Probably could make sure all callers have the
     *          index & variable list updated, and use
     *          netsnmp_table_build_oid_from_index() instead of all this.
     */
    memcpy(tmpoid, reginfo->rootoid, reginfo->rootoid_len * sizeof(oid));
    tmpoid[reginfo->rootoid_len] = 1;   /** .Entry */
    tmpoid[reginfo->rootoid_len + 1] = table_info->colnum; /** .column */

    var = reqinfo->requestvb;
    if (build_oid(&var->name, &var->name_length,
                  tmpoid, reginfo->rootoid_len + 2, table_info->indexes)
        != SNMPERR_SUCCESS)
        return SNMPERR_GENERR;

    return SNMPERR_SUCCESS;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_BUILD_RESULT */

/** given a registration info object, a request object and the table
 *  info object it builds the request->requestvb->name oid from the
 *  index values and column information found in the table_info
 *  object.  Index values are extracted from the table_info index oid.
 */
int
netsnmp_table_build_oid_from_index(netsnmp_handler_registration *reginfo,
                                   netsnmp_request_info *reqinfo,
                                   netsnmp_table_request_info *table_info)
{
    oid             tmpoid[MAX_OID_LEN];
    netsnmp_variable_list *var;
    int             len;

    if (!reginfo || !reqinfo || !table_info)
        return SNMPERR_GENERR;

    var = reqinfo->requestvb;
    len = reginfo->rootoid_len;
    memcpy(tmpoid, reginfo->rootoid, len * sizeof(oid));
    tmpoid[len++] = 1;          /* .Entry */
    tmpoid[len++] = table_info->colnum; /* .column */
    memcpy(&tmpoid[len], table_info->index_oid,
           table_info->index_oid_len * sizeof(oid));
    len += table_info->index_oid_len;
    snmp_set_var_objid( var, tmpoid, len );

    return SNMPERR_SUCCESS;
}

/** parses an OID into table indexses */
int
netsnmp_update_variable_list_from_index(netsnmp_table_request_info *tri)
{
    if (!tri)
        return SNMPERR_GENERR;

    /*
     * free any existing allocated memory, then parse oid into varbinds
     */
    snmp_reset_var_buffers( tri->indexes);

    return parse_oid_indexes(tri->index_oid, tri->index_oid_len,
                             tri->indexes);
}

/** builds an oid given a set of indexes. */
int
netsnmp_update_indexes_from_variable_list(netsnmp_table_request_info *tri)
{
    if (!tri)
        return SNMPERR_GENERR;

    return build_oid_noalloc(tri->index_oid, sizeof(tri->index_oid),
                             &tri->index_oid_len, NULL, 0, tri->indexes);
}

/**
 * checks the original request against the current data being passed in if 
 * its greater than the request oid but less than the current valid
 * return, set the current valid return to the new value.
 * 
 * returns 1 if outvar was replaced with the oid from newvar (success).
 * returns 0 if not. 
 */
int
netsnmp_check_getnext_reply(netsnmp_request_info *request,
                            oid * prefix,
                            size_t prefix_len,
                            netsnmp_variable_list * newvar,
                            netsnmp_variable_list ** outvar)
{
    oid      myname[MAX_OID_LEN];
    size_t   myname_len;

    build_oid_noalloc(myname, MAX_OID_LEN, &myname_len,
                      prefix, prefix_len, newvar);
    /*
     * is the build of the new indexes less than our current result 
     */
    if ((!(*outvar) || snmp_oid_compare(myname + prefix_len,
                                        myname_len - prefix_len,
                                        (*outvar)->name + prefix_len,
                                        (*outvar)->name_length -
                                        prefix_len) < 0)) {
        /*
         * and greater than the requested oid 
         */
        if (snmp_oid_compare(myname, myname_len,
                             request->requestvb->name,
                             request->requestvb->name_length) > 0) {
            /*
             * the new result must be better than the old 
             */
#ifdef ONLY_WORKS_WITH_ONE_VARBIND
            if (!*outvar)
                *outvar = snmp_clone_varbind(newvar);
	    else
                /* 
                 * TODO: walk the full varbind list, setting
                 *       *all* the values - not just the first.
                 */
                snmp_set_var_typed_value(*outvar, newvar->type,
				newvar->val.string, newvar->val_len);
#else  /* Interim replacement approach - less efficient, but it works! */
            if (*outvar)
                snmp_free_varbind(*outvar);
            *outvar = snmp_clone_varbind(newvar);
#endif
            snmp_set_var_objid(*outvar, myname, myname_len);

            return 1;
        }
    }
    return 0;
}

netsnmp_table_registration_info *
netsnmp_table_registration_info_clone(netsnmp_table_registration_info *tri)
{
    netsnmp_table_registration_info *copy;
    copy = malloc(sizeof(*copy));
    if (copy) {
        *copy = *tri;
        copy->indexes = snmp_clone_varbind(tri->indexes);
        if (!copy->indexes) {
            free(copy);
            copy = NULL;
        }
    }
    return copy;
}

void
netsnmp_table_registration_info_free(netsnmp_table_registration_info *tri)
{
    if (NULL == tri)
        return;

    if (NULL != tri->indexes)
        snmp_free_varbind(tri->indexes);

#if 0
    /*
     * sigh... example use of valid_columns points to static memory,
     * so freeing it would be bad... we'll just have to live with any
     * leaks, for now...
     */
#endif

    free(tri);
}

/** @} */

/*
 * internal routines 
 */
void
table_data_free_func(void *data)
{
    netsnmp_table_request_info *info = (netsnmp_table_request_info *) data;
    if (!info)
        return;
    snmp_free_varbind(info->indexes);
    free(info);
}



static void
table_helper_cleanup(netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *request, int status)
{
    netsnmp_set_request_error(reqinfo, request, status);
    netsnmp_free_request_data_sets(request);
    if (!request)
        return;
    request->parent_data = NULL;
}


/*
 * find the closest column to current (which may be current).
 *
 * called when a table runs out of rows for column X. This
 * function is called with current = X + 1, to verify that
 * X + 1 is a valid column, or find the next closest column if not.
 *
 * All list types should be sorted, lowest to highest.
 */
unsigned int
netsnmp_closest_column(unsigned int current,
                       netsnmp_column_info *valid_columns)
{
    unsigned int    closest = 0;
    int             idx;

    if (valid_columns == NULL)
        return 0;

    for( ; valid_columns; valid_columns = valid_columns->next) {

        if (valid_columns->isRange) {
            /*
             * if current < low range, it might be closest.
             * otherwise, if it's < high range, current is in
             * the range, and thus is an exact match.
             */
            if (current < valid_columns->details.range[0]) {
                if ( (valid_columns->details.range[0] < closest) ||
                     (0 == closest)) {
                    closest = valid_columns->details.range[0];
                }
            } else if (current <= valid_columns->details.range[1]) {
                closest = current;
                break;       /* can not get any closer! */
            }

        } /* range */
        else {                  /* list */
            /*
             * if current < first item, no need to iterate over list.
             * that item is either closest, or not.
             */
            if (current < valid_columns->details.list[0]) {
                if ((valid_columns->details.list[0] < closest) ||
                    (0 == closest))
                    closest = valid_columns->details.list[0];
                continue;
            }

            /** if current > last item in list, no need to iterate */
            if (current >
                valid_columns->details.list[(int)valid_columns->list_count - 1])
                continue;       /* not in list range. */

            /** skip anything less than current*/
            for (idx = 0; valid_columns->details.list[idx] < current; ++idx)
                ;
            
            /** check for exact match */
            if (current == valid_columns->details.list[idx]) {
                closest = current;
                break;      /* can not get any closer! */
            }
            
            /** list[idx] > current; is it < closest? */
            if ((valid_columns->details.list[idx] < closest) ||
                (0 == closest))
                closest = valid_columns->details.list[idx];

        }                       /* list */
    }                           /* for */

    return closest;
}

/**
 * This function can be used to setup the table's definition within
 * your module's initialize function, it takes a variable index parameter list
 * for example: the table_info structure is followed by two integer index types
 * netsnmp_table_helper_add_indexes(
 *                  table_info,   
 *	            ASN_INTEGER,  
 *		    ASN_INTEGER,  
 *		    0);
 *
 * @param tinfo is a pointer to a netsnmp_table_registration_info struct.
 *	The table handler needs to know up front how your table is structured.
 *	A netsnmp_table_registeration_info structure that is 
 *	passed to the table handler should contain the asn index types for the 
 *	table as well as the minimum and maximum column that should be used.
 *
 * @return void
 *
 */
void
netsnmp_table_helper_add_indexes(netsnmp_table_registration_info *tinfo,
                                 ...)
{
    va_list         debugargs;
    int             type;

    va_start(debugargs, tinfo);
    while ((type = va_arg(debugargs, int)) != 0) {
        netsnmp_table_helper_add_index(tinfo, type);
    }
    va_end(debugargs);
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
static void
_row_stash_data_list_free(void *ptr) {
    netsnmp_oid_stash_node **tmp = (netsnmp_oid_stash_node **)ptr;
    netsnmp_oid_stash_free(tmp, NULL);
    free(ptr);
}

#ifndef NETSNMP_FEATURE_REMOVE_TABLE_GET_OR_CREATE_ROW_STASH
/** returns a row-wide place to store data in.
    @todo This function will likely change to add free pointer functions. */
netsnmp_oid_stash_node **
netsnmp_table_get_or_create_row_stash(netsnmp_agent_request_info *reqinfo,
                                      const u_char * storage_name)
{
    netsnmp_oid_stash_node **stashp = NULL;
    stashp = (netsnmp_oid_stash_node **)
        netsnmp_agent_get_list_data(reqinfo, (const char *) storage_name);

    if (!stashp) {
        /*
         * hasn't be created yet.  we create it here. 
         */
        stashp = SNMP_MALLOC_TYPEDEF(netsnmp_oid_stash_node *);

        if (!stashp)
            return NULL;        /* ack. out of mem */

        netsnmp_agent_add_list_data(reqinfo,
                                    netsnmp_create_data_list((const char *) storage_name,
                                                             stashp,
                                                             _row_stash_data_list_free));
    }
    return stashp;
}
#endif /* NETSNMP_FEATURE_REMOVE_TABLE_GET_OR_CREATE_ROW_STASH */
#endif /* NETSNMP_NO_WRITE_SUPPORT */

/*
 * advance the table info colnum to the next column, or 0 if there are no more
 *
 * @return new column, or 0 if there are no more
 */
unsigned int
netsnmp_table_next_column(netsnmp_table_request_info *table_info)
{
    if (NULL == table_info)
        return 0;

    /*
     * try and validate next column
     */
    if (table_info->reg_info->valid_columns)
        return netsnmp_closest_column(table_info->colnum + 1,
                                      table_info->reg_info->valid_columns);
    
    /*
     * can't validate. assume 1..max_column are valid
     */
    if (table_info->colnum < table_info->reg_info->max_column)
        return table_info->colnum + 1;
    
    return 0; /* out of range */
}
