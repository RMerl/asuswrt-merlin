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
#ifndef AGENT_HANDLER_H
#define AGENT_HANDLER_H

#ifdef __cplusplus
extern          "C" {
#endif

/** @file agent_handler.h
 *
 *  @addtogroup handler
 *
 * @{
 */

struct netsnmp_handler_registration_s;

/*
 * per mib handler flags.
 * NOTE: Lower bits are reserved for the agent handler's use.
 *       The high 4 bits (31-28) are reserved for use by the handler.
 */
#define MIB_HANDLER_AUTO_NEXT                   0x00000001
#define MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE     0x00000002
#define MIB_HANDLER_INSTANCE                    0x00000004

#define MIB_HANDLER_CUSTOM4                     0x10000000
#define MIB_HANDLER_CUSTOM3                     0x20000000
#define MIB_HANDLER_CUSTOM2                     0x40000000
#define MIB_HANDLER_CUSTOM1                     0x80000000


/** @typedef struct netsnmp_mib_handler_s netsnmp_mib_handler
 * Typedefs the netsnmp_mib_handler_s struct into  netsnmp_mib_handler */

/** @struct netsnmp_mib_handler_s
 *  the mib handler structure to be registered
 */
typedef struct netsnmp_mib_handler_s {
        char           *handler_name;
	/** for handler's internal use */
        void           *myvoid; 
        /** for agent_handler's internal use */
        int             flags;

        /** if you add more members, you probably also want to update */
        /** _clone_handler in agent_handler.c. */
	
        int             (*access_method) (struct netsnmp_mib_handler_s *,
                                          struct
                                          netsnmp_handler_registration_s *,
                                          struct
                                          netsnmp_agent_request_info_s *,
                                          struct netsnmp_request_info_s *);
        /** data clone hook for myvoid
         *  deep copy the myvoid member - default is to copy the pointer
         *  This method is only called if myvoid != NULL
         *  myvoid is the current myvoid pointer.
         *  returns NULL on failure
         */
        void *(*data_clone)(void *myvoid);
        /** data free hook for myvoid
         *  delete the myvoid member - default is to do nothing
         *  This method is only called if myvoid != NULL
         */
        void (*data_free)(void *myvoid); /**< data free hook for myvoid */

        struct netsnmp_mib_handler_s *next;
        struct netsnmp_mib_handler_s *prev;
} netsnmp_mib_handler;

/*
 * per registration flags
 */
#define HANDLER_CAN_GETANDGETNEXT     0x01       /* must be able to do both */
#define HANDLER_CAN_SET               0x02           /* implies create, too */
#define HANDLER_CAN_GETBULK           0x04
#define HANDLER_CAN_NOT_CREATE        0x08         /* auto set if ! CAN_SET */
#define HANDLER_CAN_BABY_STEP         0x10
#define HANDLER_CAN_STASH             0x20


#define HANDLER_CAN_RONLY   (HANDLER_CAN_GETANDGETNEXT)
#define HANDLER_CAN_RWRITE  (HANDLER_CAN_GETANDGETNEXT | HANDLER_CAN_SET)
#define HANDLER_CAN_SET_ONLY (HANDLER_CAN_SET | HANDLER_CAN_NOT_CREATE)
#define HANDLER_CAN_DEFAULT (HANDLER_CAN_RONLY | HANDLER_CAN_NOT_CREATE)

/** @typedef struct netsnmp_handler_registration_s netsnmp_handler_registration
 * Typedefs the netsnmp_handler_registration_s struct into netsnmp_handler_registration  */

/** @struct netsnmp_handler_registration_s
 *  Root registration info.
 *  The variables handlerName, contextName, and rootoid need to be allocated
 *  on the heap, when the registration structure is unregistered using
 *  unregister_mib_context() the code attempts to free them.
 */
typedef struct netsnmp_handler_registration_s {

	/** for mrTable listings, and other uses */
        char           *handlerName;
	/** NULL = default context */
        char           *contextName;    

        /**
         * where are we registered at? 
         */
        oid            *rootoid;
        size_t          rootoid_len;

        /**
         * handler details 
         */
        netsnmp_mib_handler *handler;
        int             modes;

        /**
         * more optional stuff 
         */
        int             priority;
        int             range_subid;
        oid             range_ubound;
        int             timeout;
        int             global_cacheid;

        /**
         * void ptr for registeree
         */
        void *          my_reg_void;

} netsnmp_handler_registration;

/*
 * function handler definitions 
 */

typedef int (Netsnmp_Node_Handler) (netsnmp_mib_handler *handler,
    /** pointer to registration struct */
    netsnmp_handler_registration *reginfo,
    /** pointer to current transaction */
    netsnmp_agent_request_info *reqinfo,
    netsnmp_request_info *requests);

    typedef struct netsnmp_handler_args_s {
        netsnmp_mib_handler *handler;
        netsnmp_handler_registration *reginfo;
        netsnmp_agent_request_info *reqinfo;
        netsnmp_request_info *requests;
    } netsnmp_handler_args;

    typedef struct netsnmp_delegated_cache_s {
        int             transaction_id;
        netsnmp_mib_handler *handler;
        netsnmp_handler_registration *reginfo;
        netsnmp_agent_request_info *reqinfo;
        netsnmp_request_info *requests;
        void           *localinfo;
    } netsnmp_delegated_cache;

    /*
     * handler API functions 
     */
    void            netsnmp_init_handler_conf(void);
    int             netsnmp_register_handler(netsnmp_handler_registration
                                             *reginfo);
    int             netsnmp_unregister_handler(netsnmp_handler_registration
                                               *reginfo);
    int            
        netsnmp_register_handler_nocallback(netsnmp_handler_registration
                                            *reginfo);
    int             netsnmp_inject_handler(netsnmp_handler_registration
                                           *reginfo,
                                           netsnmp_mib_handler *handler);
    int
        netsnmp_inject_handler_before(netsnmp_handler_registration *reginfo,
                                      netsnmp_mib_handler *handler,
                                      const char *before_what);
    netsnmp_mib_handler
        *netsnmp_find_handler_by_name(netsnmp_handler_registration
                                      *reginfo, const char *name);
    void          
        *netsnmp_find_handler_data_by_name(netsnmp_handler_registration
                                           *reginfo, const char *name);
    int             netsnmp_call_handlers(netsnmp_handler_registration
                                          *reginfo,
                                          netsnmp_agent_request_info
                                          *reqinfo,
                                          netsnmp_request_info *requests);
    int             netsnmp_call_handler(netsnmp_mib_handler *next_handler,
                                         netsnmp_handler_registration
                                         *reginfo,
                                         netsnmp_agent_request_info
                                         *reqinfo,
                                         netsnmp_request_info *requests);
    int             netsnmp_call_next_handler(netsnmp_mib_handler *current,
                                              netsnmp_handler_registration
                                              *reginfo,
                                              netsnmp_agent_request_info
                                              *reqinfo,
                                              netsnmp_request_info
                                              *requests);
    int             netsnmp_call_next_handler_one_request(netsnmp_mib_handler *current,
                                                          netsnmp_handler_registration *reginfo,
                                                          netsnmp_agent_request_info *reqinfo,
                                                          netsnmp_request_info *requests);
    
    netsnmp_mib_handler *netsnmp_create_handler(const char *name,
                                                Netsnmp_Node_Handler *
                                                handler_access_method);
    netsnmp_handler_registration *
    netsnmp_handler_registration_create(const char *name,
                                        netsnmp_mib_handler *handler,
                                        const oid * reg_oid, size_t reg_oid_len,
                                        int modes);
    netsnmp_handler_registration *
    netsnmp_create_handler_registration(const char *name, Netsnmp_Node_Handler*
                                        handler_access_method,
                                        const oid *reg_oid, size_t reg_oid_len,
                                        int modes);

    netsnmp_delegated_cache
        *netsnmp_create_delegated_cache(netsnmp_mib_handler *,
                                        netsnmp_handler_registration *,
                                        netsnmp_agent_request_info *,
                                        netsnmp_request_info *, void *);
    void netsnmp_free_delegated_cache(netsnmp_delegated_cache *dcache);
    netsnmp_delegated_cache
        *netsnmp_handler_check_cache(netsnmp_delegated_cache *dcache);
    void            netsnmp_register_handler_by_name(const char *,
                                                     netsnmp_mib_handler
                                                     *);

    void            netsnmp_clear_handler_list(void);

    void
        netsnmp_request_add_list_data(netsnmp_request_info *request,
                                      netsnmp_data_list *node);
    int netsnmp_request_remove_list_data(netsnmp_request_info *request,
                                         const char *name);

    int
        netsnmp_request_remove_list_data(netsnmp_request_info *request,
                                         const char *name);

    void    *netsnmp_request_get_list_data(netsnmp_request_info
                                                  *request,
                                                  const char *name);

    void
              netsnmp_free_request_data_set(netsnmp_request_info *request);

    void
             netsnmp_free_request_data_sets(netsnmp_request_info *request);

    void            netsnmp_handler_free(netsnmp_mib_handler *);
    netsnmp_mib_handler *netsnmp_handler_dup(netsnmp_mib_handler *);
    netsnmp_handler_registration
        *netsnmp_handler_registration_dup(netsnmp_handler_registration *);
    void           
        netsnmp_handler_registration_free(netsnmp_handler_registration *);

#define REQUEST_IS_DELEGATED     1
#define REQUEST_IS_NOT_DELEGATED 0
    void           
        netsnmp_handler_mark_requests_as_delegated(netsnmp_request_info *,
                                                   int);
    void           *netsnmp_handler_get_parent_data(netsnmp_request_info *,
                                                    const char *);

#ifdef __cplusplus
}
#endif

#endif                          /* AGENT_HANDLER_H */
/** @} */
