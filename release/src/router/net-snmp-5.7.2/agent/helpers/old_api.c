#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/old_api.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/agent/agent_callbacks.h>

#include <stddef.h>

#define MIB_CLIENTS_ARE_EVIL 1

#ifdef HAVE_DMALLOC_H
static void free_wrapper(void * p)
{
    free(p);
}
#else
#define free_wrapper free
#endif

/*
 * don't use these! 
 */
void            set_current_agent_session(netsnmp_agent_session *asp);
netsnmp_agent_session *netsnmp_get_current_agent_session(void);

/** @defgroup old_api old_api
 *  Calls mib module code written in the old style of code.
 *  @ingroup handler
 *  This is a backwards compatilibity module that allows code written
 *  in the old API to be run under the new handler based architecture.
 *  Use it by calling netsnmp_register_old_api().
 *  @{
 */

/** returns a old_api handler that should be the final calling
 * handler.  Don't use this function.  Use the netsnmp_register_old_api()
 * function instead.
 */
netsnmp_mib_handler *
get_old_api_handler(void)
{
    return netsnmp_create_handler("old_api", netsnmp_old_api_helper);
}

struct variable *
netsnmp_duplicate_variable(struct variable *var)
{
    struct variable *var2 = NULL;
    
    if (var) {
        const int varsize = offsetof(struct variable, name) + var->namelen * sizeof(var->name[0]);
        var2 = malloc(varsize);
        if (var2)
            memcpy(var2, var, varsize);
    }
    return var2;
}

/** Registers an old API set into the mib tree.  Functionally this
 * mimics the old register_mib_context() function (and in fact the new
 * register_mib_context() function merely calls this new old_api one).
 */
int
netsnmp_register_old_api(const char *moduleName,
                         struct variable *var,
                         size_t varsize,
                         size_t numvars,
                         const oid * mibloc,
                         size_t mibloclen,
                         int priority,
                         int range_subid,
                         oid range_ubound,
                         netsnmp_session * ss,
                         const char *context, int timeout, int flags)
{

    unsigned int    i;

    /*
     * register all subtree nodes 
     */
    for (i = 0; i < numvars; i++) {
        struct variable *vp;
        netsnmp_handler_registration *reginfo =
            SNMP_MALLOC_TYPEDEF(netsnmp_handler_registration);
        if (reginfo == NULL)
            return SNMP_ERR_GENERR;

	vp = netsnmp_duplicate_variable((struct variable *)
					((char *) var + varsize * i));

        reginfo->handler = get_old_api_handler();
        reginfo->handlerName = strdup(moduleName);
        reginfo->rootoid_len = (mibloclen + vp->namelen);
        reginfo->rootoid =
            (oid *) malloc(reginfo->rootoid_len * sizeof(oid));
        if (reginfo->rootoid == NULL) {
            SNMP_FREE(vp);
            SNMP_FREE(reginfo->handlerName);
            SNMP_FREE(reginfo);
            return SNMP_ERR_GENERR;
        }

        memcpy(reginfo->rootoid, mibloc, mibloclen * sizeof(oid));
        memcpy(reginfo->rootoid + mibloclen, vp->name, vp->namelen
               * sizeof(oid));
        reginfo->handler->myvoid = (void *) vp;
        reginfo->handler->data_clone
	    = (void *(*)(void *))netsnmp_duplicate_variable;
        reginfo->handler->data_free = free;

        reginfo->priority = priority;
        reginfo->range_subid = range_subid;

        reginfo->range_ubound = range_ubound;
        reginfo->timeout = timeout;
        reginfo->contextName = (context) ? strdup(context) : NULL;
        reginfo->modes = HANDLER_CAN_RWRITE;

        /*
         * register ourselves in the mib tree 
         */
        if (netsnmp_register_handler(reginfo) != MIB_REGISTERED_OK) {
            /** netsnmp_handler_registration_free(reginfo); already freed */
            /* SNMP_FREE(vp); already freed */
        }
    }
    return SNMPERR_SUCCESS;
}

/** registers a row within a mib table */
int
netsnmp_register_mib_table_row(const char *moduleName,
                               struct variable *var,
                               size_t varsize,
                               size_t numvars,
                               oid * mibloc,
                               size_t mibloclen,
                               int priority,
                               int var_subid,
                               netsnmp_session * ss,
                               const char *context, int timeout, int flags)
{
    unsigned int    i = 0, rc = 0;
    oid             ubound = 0;

    for (i = 0; i < numvars; i++) {
        struct variable *vr =
            (struct variable *) ((char *) var + (i * varsize));
        netsnmp_handler_registration *r;
        if ( var_subid > (int)mibloclen ) {
            break;    /* doesn't make sense */
        }
        r = SNMP_MALLOC_TYPEDEF(netsnmp_handler_registration);

        if (r == NULL) {
            /*
             * Unregister whatever we have registered so far, and
             * return an error.  
             */
            rc = MIB_REGISTRATION_FAILED;
            break;
        }
        memset(r, 0, sizeof(netsnmp_handler_registration));

        r->handler = get_old_api_handler();
        r->handlerName = strdup(moduleName);

        if (r->handlerName == NULL) {
            netsnmp_handler_registration_free(r);
            break;
        }

        r->rootoid_len = mibloclen;
        r->rootoid = (oid *) malloc(r->rootoid_len * sizeof(oid));

        if (r->rootoid == NULL) {
            netsnmp_handler_registration_free(r);
            rc = MIB_REGISTRATION_FAILED;
            break;
        }
        memcpy(r->rootoid, mibloc, mibloclen * sizeof(oid));
        memcpy((u_char *) (r->rootoid + (var_subid - vr->namelen)), vr->name,
               vr->namelen * sizeof(oid));
        DEBUGMSGTL(("netsnmp_register_mib_table_row", "rootoid "));
        DEBUGMSGOID(("netsnmp_register_mib_table_row", r->rootoid,
                     r->rootoid_len));
        DEBUGMSG(("netsnmp_register_mib_table_row", "(%d)\n",
                     (var_subid - vr->namelen)));
        r->handler->myvoid = netsnmp_duplicate_variable(vr);
        r->handler->data_clone = (void *(*)(void *))netsnmp_duplicate_variable;
        r->handler->data_free = free;

        if (r->handler->myvoid == NULL) {
            netsnmp_handler_registration_free(r);
            rc = MIB_REGISTRATION_FAILED;
            break;
        }

        r->contextName = (context) ? strdup(context) : NULL;

        if (context != NULL && r->contextName == NULL) {
            netsnmp_handler_registration_free(r);
            rc = MIB_REGISTRATION_FAILED;
            break;
        }

        r->priority = priority;
        r->range_subid = 0;     /* var_subid; */
        r->range_ubound = 0;    /* range_ubound; */
        r->timeout = timeout;
        r->modes = HANDLER_CAN_RWRITE;

        /*
         * Register this column and row  
         */
        if ((rc =
             netsnmp_register_handler_nocallback(r)) !=
            MIB_REGISTERED_OK) {
            DEBUGMSGTL(("netsnmp_register_mib_table_row",
                        "register failed %d\n", rc));
            netsnmp_handler_registration_free(r);
            break;
        }

        if (vr->namelen > 0) {
            if (vr->name[vr->namelen - 1] > ubound) {
                ubound = vr->name[vr->namelen - 1];
            }
        }
    }

    if (rc == MIB_REGISTERED_OK) {
        struct register_parameters reg_parms;

        reg_parms.name = mibloc;
        reg_parms.namelen = mibloclen;
        reg_parms.priority = priority;
        reg_parms.flags = (u_char) flags;
        reg_parms.range_subid = var_subid;
        reg_parms.range_ubound = ubound;
        reg_parms.timeout = timeout;
        reg_parms.contextName = context;
        rc = snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                                 SNMPD_CALLBACK_REGISTER_OID, &reg_parms);
    }

    return rc;
}

/** implements the old_api handler */
int
netsnmp_old_api_helper(netsnmp_mib_handler *handler,
                       netsnmp_handler_registration *reginfo,
                       netsnmp_agent_request_info *reqinfo,
                       netsnmp_request_info *requests)
{

#if MIB_CLIENTS_ARE_EVIL
    oid             save[MAX_OID_LEN];
    size_t          savelen = 0;
#endif
    struct variable compat_var, *cvp = &compat_var;
    int             exact = 1;
    int             status;

    struct variable *vp;
    netsnmp_old_api_cache *cacheptr;
    netsnmp_agent_session *oldasp = NULL;
    u_char         *access = NULL;
    WriteMethod    *write_method = NULL;
    size_t          len;
    size_t          tmp_len;
    oid             tmp_name[MAX_OID_LEN];

    vp = (struct variable *) handler->myvoid;

    /*
     * create old variable structure with right information 
     */
    memcpy(cvp->name, reginfo->rootoid,
           reginfo->rootoid_len * sizeof(oid));
    cvp->namelen = reginfo->rootoid_len;
    cvp->type = vp->type;
    cvp->magic = vp->magic;
    cvp->acl = vp->acl;
    cvp->findVar = vp->findVar;

    switch (reqinfo->mode) {
    case MODE_GETNEXT:
    case MODE_GETBULK:
        exact = 0;
    }

    for (; requests; requests = requests->next) {

#if MIB_CLIENTS_ARE_EVIL
        savelen = requests->requestvb->name_length;
        memcpy(save, requests->requestvb->name, savelen * sizeof(oid));
#endif

        switch (reqinfo->mode) {
        case MODE_GET:
        case MODE_GETNEXT:
#ifndef NETSNMP_NO_WRITE_SUPPORT
        case MODE_SET_RESERVE1:
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
            /*
             * Actually call the old mib-module function 
             */
            if (vp && vp->findVar) {
                memcpy(tmp_name, requests->requestvb->name,
                                 requests->requestvb->name_length*sizeof(oid));
                tmp_len = requests->requestvb->name_length;
                access = (*(vp->findVar)) (cvp, tmp_name, &tmp_len,
                                           exact, &len, &write_method);
                snmp_set_var_objid( requests->requestvb, tmp_name, tmp_len );
            }
            else
                access = NULL;

#ifdef WWW_FIX
            if (IS_DELEGATED(cvp->type)) {
                add_method = (AddVarMethod *) statP;
                requests->delayed = 1;
                have_delegated = 1;
                continue;       /* WWW: This may not get to the right place */
            }
#endif

            /*
             * WWW: end range checking 
             */
            if (access) {
                /*
                 * result returned 
                 */
#ifndef NETSNMP_NO_WRITE_SUPPORT
                if (reqinfo->mode != MODE_SET_RESERVE1)
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
                    snmp_set_var_typed_value(requests->requestvb,
                                             cvp->type, access, len);
            } else {
                /*
                 * no result returned 
                 */
#if MIB_CLIENTS_ARE_EVIL
                if (access == NULL) {
                    if (netsnmp_oid_equals(requests->requestvb->name,
                                         requests->requestvb->name_length,
                                         save, savelen) != 0) {
                        DEBUGMSGTL(("old_api", "evil_client: %s\n",
                                    reginfo->handlerName));
                        memcpy(requests->requestvb->name, save,
                               savelen * sizeof(oid));
                        requests->requestvb->name_length = savelen;
                    }
                }
#endif
            }

            /*
             * AAA: fall through for everything that is a set (see BBB) 
             */
#ifndef NETSNMP_NO_WRITE_SUPPORT
            if (reqinfo->mode != MODE_SET_RESERVE1)
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
                break;

            cacheptr = SNMP_MALLOC_TYPEDEF(netsnmp_old_api_cache);
            if (!cacheptr)
                return netsnmp_set_request_error(reqinfo, requests,
                                                 SNMP_ERR_RESOURCEUNAVAILABLE);
            cacheptr->data = access;
            cacheptr->write_method = write_method;
            write_method = NULL;
            netsnmp_request_add_list_data(requests,
                                          netsnmp_create_data_list
                                          (OLD_API_NAME, cacheptr,
                                           &free_wrapper));
            /*
             * BBB: fall through for everything that is a set (see AAA) 
             */

        default:
            /*
             * WWW: explicitly list the SET conditions 
             */
            /*
             * (the rest of the) SET contions 
             */
            cacheptr =
                (netsnmp_old_api_cache *)
                netsnmp_request_get_list_data(requests, OLD_API_NAME);

            if (cacheptr == NULL || cacheptr->write_method == NULL) {
                /*
                 * WWW: try to set ourselves if possible? 
                 */
                return netsnmp_set_request_error(reqinfo, requests,
                                                 SNMP_ERR_NOTWRITABLE);
            }

            oldasp = netsnmp_get_current_agent_session();
            set_current_agent_session(reqinfo->asp);
            status =
                (*(cacheptr->write_method)) (reqinfo->mode,
                                             requests->requestvb->val.
                                             string,
                                             requests->requestvb->type,
                                             requests->requestvb->val_len,
                                             cacheptr->data,
                                             requests->requestvb->name,
                                             requests->requestvb->
                                             name_length);
            set_current_agent_session(oldasp);

            if (status != SNMP_ERR_NOERROR) {
                netsnmp_set_request_error(reqinfo, requests, status);
            }

            /*
             * clean up is done by the automatic freeing of the
             * cache stored in the request. 
             */

            break;
        }
    }
    return SNMP_ERR_NOERROR;
}

/** @} */

/*
 * don't use this! 
 */
static netsnmp_agent_session *current_agent_session = NULL;
netsnmp_agent_session *
netsnmp_get_current_agent_session(void)
{
    return current_agent_session;
}

/*
 * don't use this! 
 */
void
set_current_agent_session(netsnmp_agent_session *asp)
{
    current_agent_session = asp;
}
