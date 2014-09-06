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
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/instance.h>

#include <stdlib.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/serialize.h>
#include <net-snmp/agent/read_only.h>

netsnmp_feature_provide(instance)
netsnmp_feature_child_of(instance, mib_helpers)

typedef struct netsnmp_num_file_instance_s {
    int   refcnt;
    char *file_name;
    FILE *filep;
    u_char type;
    int   flags;
} netsnmp_num_file_instance;

/** @defgroup instance instance
 *  Process individual MIB instances easily.
 *  @ingroup leaf
 *  @{
 */

static netsnmp_num_file_instance *
netsnmp_num_file_instance_ref(netsnmp_num_file_instance *nfi)
{
    nfi->refcnt++;
    return nfi;
}

static void
netsnmp_num_file_instance_deref(netsnmp_num_file_instance *nfi)
{
    if (--nfi->refcnt == 0) {
	free(nfi->file_name);
	free(nfi);
    }
}

/**
 * Creates an instance helper handler, calls netsnmp_create_handler, which
 * then could be registered, using netsnmp_register_handler().
 *
 * @return Returns a pointer to a netsnmp_mib_handler struct which contains
 *	the handler's name and the access method
 */
netsnmp_mib_handler *
netsnmp_get_instance_handler(void)
{
    return netsnmp_create_handler("instance",
                                  netsnmp_instance_helper_handler);
}

/**
 * This function registers an instance helper handler, which is a way of 
 * registering an exact OID such that GENEXT requests are handled entirely
 * by the helper. First need to inject it into the calling chain of the 
 * handler defined by the netsnmp_handler_registration struct, reginfo.  
 * The new handler is injected at the top of the list and will be the new
 * handler to be called first.  This function also injects a serialize 
 * handler before actually calling netsnmp_register_handle, registering 
 * reginfo.
 *
 * @param reginfo a handler registration structure which could get created
 *                using netsnmp_create_handler_registration.  Used to register
 *                an instance helper handler.
 *
 * @return
 *      MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED and MIB_DUPLICATE_REGISTRATION.
 */
int
netsnmp_register_instance(netsnmp_handler_registration *reginfo)
{
    netsnmp_mib_handler *handler = netsnmp_get_instance_handler();
    handler->flags |= MIB_HANDLER_INSTANCE;
    netsnmp_inject_handler(reginfo, handler);
    return netsnmp_register_serialize(reginfo);
}

/**
 * This function injects a "read only" handler into the handler chain 
 * prior to serializing/registering the handler.
 *
 * The only purpose of this "read only" handler is to return an
 * appropriate error for any requests passed to it in a SET mode.
 * Inserting it into your handler chain will ensure you're never
 * asked to perform a SET request so you can ignore those error
 * conditions.
 *
 * @param reginfo a handler registration structure which could get created
 *                using netsnmp_create_handler_registration.  Used to register
 *                a read only instance helper handler.
 *
 * @return
 *      MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED and MIB_DUPLICATE_REGISTRATION.
 */
int
netsnmp_register_read_only_instance(netsnmp_handler_registration *reginfo)
{
    netsnmp_inject_handler(reginfo, netsnmp_get_instance_handler());
    netsnmp_inject_handler(reginfo, netsnmp_get_read_only_handler());
    return netsnmp_register_serialize(reginfo);
}

static
netsnmp_handler_registration *
get_reg(const char *name,
        const char *ourname,
        const oid * reg_oid, size_t reg_oid_len,
        netsnmp_num_file_instance *it,
        int modes,
        Netsnmp_Node_Handler * scalarh, Netsnmp_Node_Handler * subhandler,
        const char *contextName)
{
    netsnmp_handler_registration *myreg;
    netsnmp_mib_handler *myhandler;

    if (subhandler) {
        myreg =
            netsnmp_create_handler_registration(name,
                                                subhandler,
                                                reg_oid, reg_oid_len,
                                                modes);
        myhandler = netsnmp_create_handler(ourname, scalarh);
        myhandler->myvoid = it;
	myhandler->data_clone = (void*(*)(void*))netsnmp_num_file_instance_ref;
	myhandler->data_free = (void(*)(void*))netsnmp_num_file_instance_deref;
        netsnmp_inject_handler(myreg, myhandler);
    } else {
        myreg =
            netsnmp_create_handler_registration(name,
                                                scalarh,
                                                reg_oid, reg_oid_len,
                                                modes);
        myreg->handler->myvoid = it;
	myreg->handler->data_clone
	    = (void *(*)(void *))netsnmp_num_file_instance_ref;
	myreg->handler->data_free
	    = (void (*)(void *))netsnmp_num_file_instance_deref;
    }
    if (contextName)
        myreg->contextName = strdup(contextName);
    return myreg;
}

/* Watched 'long' instances are writable on both 32-bit and 64-bit systems  */
netsnmp_feature_child_of(read_only_ulong_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_ULONG_INSTANCE
int
netsnmp_register_read_only_ulong_instance(const char *name,
                                          const oid * reg_oid,
                                          size_t reg_oid_len, u_long * it,
                                          Netsnmp_Node_Handler *
                                          subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(u_long),
                   ASN_UNSIGNED, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_ULONG_INSTANCE */

netsnmp_feature_child_of(ulong_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_ULONG_INSTANCE
int
netsnmp_register_ulong_instance(const char *name,
                                const oid * reg_oid, size_t reg_oid_len,
                                u_long * it,
                                Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(u_long),
                   ASN_UNSIGNED, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_ULONG_INSTANCE */

netsnmp_feature_child_of(read_only_counter32_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_COUNTER32_INSTANCE
int
netsnmp_register_read_only_counter32_instance(const char *name,
                                              const oid * reg_oid,
                                              size_t reg_oid_len,
                                              u_long * it,
                                              Netsnmp_Node_Handler *
                                              subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(u_long),
                   ASN_COUNTER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_COUNTER32_INSTANCE */

netsnmp_feature_child_of(read_only_long_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_LONG_INSTANCE
int
netsnmp_register_read_only_long_instance(const char *name,
                                         const oid * reg_oid,
                                         size_t reg_oid_len,
                                         long *it,
                                         Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(long), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_LONG_INSTANCE */

netsnmp_feature_child_of(long_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_LONG_INSTANCE
int
netsnmp_register_long_instance(const char *name,
                               const oid * reg_oid, size_t reg_oid_len,
                               long *it, Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(long), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_LONG_INSTANCE */

/* Watched 'int' instances are only writable on 32-bit systems  */
netsnmp_feature_child_of(read_only_uint_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_UINT_INSTANCE
int
netsnmp_register_read_only_uint_instance(const char *name,
                                         const oid * reg_oid,
                                         size_t reg_oid_len,
                                         unsigned int *it,
                                         Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(unsigned int),
                   ASN_UNSIGNED, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_UINT_INSTANCE */

netsnmp_feature_child_of(uint_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_UINT_INSTANCE
int
netsnmp_register_uint_instance(const char *name,
                               const oid * reg_oid, size_t reg_oid_len,
                               unsigned int *it, Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(unsigned int),
                   ASN_UNSIGNED, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_UINT_INSTANCE */

netsnmp_feature_child_of(read_only_int_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_INT_INSTANCE
int
netsnmp_register_read_only_int_instance(const char *name,
                                const oid * reg_oid, size_t reg_oid_len,
                                int *it, Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(int), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_INT_INSTANCE */

  /*
   * Compatibility with earlier (inconsistently named) routine
   */
netsnmp_feature_child_of(register_read_only_int_instance,netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_INT_INSTANCE
int
register_read_only_int_instance(const char *name,
                                const oid * reg_oid, size_t reg_oid_len,
                                int *it, Netsnmp_Node_Handler * subhandler)
{
  return netsnmp_register_read_only_int_instance(name,
                                reg_oid, reg_oid_len,
                                it, subhandler);
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_INT_INSTANCE */

/*
 * Context registrations
 */

netsnmp_feature_child_of(register_read_only_ulong_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_ULONG_INSTANCE_CONTEXT
int
netsnmp_register_read_only_ulong_instance_context(const char *name,
                                                  const oid * reg_oid,
                                                  size_t reg_oid_len,
                                                  u_long * it,
                                                  Netsnmp_Node_Handler *
                                                  subhandler,
                                                  const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(u_long), ASN_UNSIGNED, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_ULONG_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_ulong_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_ULONG_INSTANCE_CONTEXT
int
netsnmp_register_ulong_instance_context(const char *name,
                                        const oid * reg_oid, size_t reg_oid_len,
                                        u_long * it,
                                        Netsnmp_Node_Handler * subhandler,
                                        const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(u_long), ASN_UNSIGNED, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_ULONG_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_read_only_counter32_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_COUNTER32_INSTANCE_CONTEXT
int
netsnmp_register_read_only_counter32_instance_context(const char *name,
                                                      const oid * reg_oid,
                                                      size_t reg_oid_len,
                                                      u_long * it,
                                                      Netsnmp_Node_Handler *
                                                      subhandler,
                                                      const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(u_long), ASN_COUNTER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_COUNTER32_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_read_only_long_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_LONG_INSTANCE_CONTEXT
int
netsnmp_register_read_only_long_instance_context(const char *name,
                                                 const oid * reg_oid,
                                                 size_t reg_oid_len,
                                                 long *it,
                                                 Netsnmp_Node_Handler
                                                 *subhandler,
                                                 const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(long), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_LONG_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_long_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_LONG_INSTANCE_CONTEXT
int
netsnmp_register_long_instance_context(const char *name,
                                       const oid * reg_oid, size_t reg_oid_len,
                                       long *it,
                                       Netsnmp_Node_Handler * subhandler,
                                       const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(long), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_LONG_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_int_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_INT_INSTANCE_CONTEXT
int
netsnmp_register_int_instance_context(const char *name,
                                      const oid * reg_oid,
                                      size_t reg_oid_len,
                                      int *it,
                                      Netsnmp_Node_Handler * subhandler,
                                      const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(int), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_INT_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_read_only_int_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_INT_INSTANCE_CONTEXT
int
netsnmp_register_read_only_int_instance_context(const char *name,
                                                const oid * reg_oid,
                                                size_t reg_oid_len,
                                                int *it,
                                                Netsnmp_Node_Handler * subhandler,
                                                const char *contextName)
{
    netsnmp_handler_registration *myreg =
      netsnmp_create_handler_registration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return netsnmp_register_watched_instance2(
        myreg, netsnmp_create_watcher_info(
            (void *)it, sizeof(int), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_READ_ONLY_INT_INSTANCE_CONTEXT */

/*
 * Compatibility with earlier (inconsistently named) routine
 */
netsnmp_feature_child_of(read_only_int_instance_context,instance)
#ifndef NETSNMP_FEATURE_REMOVE_READ_ONLY_INT_INSTANCE_CONTEXT
int
register_read_only_int_instance_context(const char *name,
                                        const oid * reg_oid, size_t reg_oid_len,
                                        int *it,
                                        Netsnmp_Node_Handler * subhandler,
                                        const char *contextName)
{
    return netsnmp_register_read_only_int_instance_context(name,
                                                           reg_oid, reg_oid_len,
                                                           it, subhandler,
                                                           contextName);
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_ONLY_INT_INSTANCE_CONTEXT */

netsnmp_feature_child_of(register_num_file_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_NUM_FILE_INSTANCE
int
netsnmp_register_num_file_instance(const char *name,
                                   const oid * reg_oid, size_t reg_oid_len,
                                   const char *file_name, int asn_type, int mode,
                                   Netsnmp_Node_Handler * subhandler,
                                   const char *contextName)
{
    netsnmp_handler_registration *myreg;
    netsnmp_num_file_instance *nfi;

    if ((NULL == name) || (NULL == reg_oid) || (NULL == file_name)) {
        snmp_log(LOG_ERR, "bad parameter to netsnmp_register_num_file_instance\n");
        return MIB_REGISTRATION_FAILED;
    }

    nfi = SNMP_MALLOC_TYPEDEF(netsnmp_num_file_instance);
    if ((NULL == nfi) ||
        (NULL == (nfi->file_name = strdup(file_name)))) {
        snmp_log(LOG_ERR, "could not not allocate memory\n");
        if (NULL != nfi)
            free(nfi); /* SNMP_FREE overkill on local var */
        return MIB_REGISTRATION_FAILED;
    }

    nfi->refcnt = 1;
    myreg = get_reg(name, "file_num_handler", reg_oid, reg_oid_len, nfi,
                    mode, netsnmp_instance_num_file_handler,
                    subhandler, contextName);
    if (NULL == myreg) {
        netsnmp_num_file_instance_deref(nfi);
        return MIB_REGISTRATION_FAILED;
    }

    nfi->type = asn_type;

    if (HANDLER_CAN_RONLY == mode)
        return netsnmp_register_read_only_instance(myreg);

    return netsnmp_register_instance(myreg);
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_NUM_FILE_INSTANCE */

netsnmp_feature_child_of(register_int_instance,instance)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_INT_INSTANCE
/**
 * This function registers an int helper handler to a specified OID.
 *
 * @param name         the name used for registration pruposes.
 *
 * @param reg_oid      the OID where you want to register your integer at
 *
 * @param reg_oid_len  the length of the OID
 *
 * @param it           the integer value to be registered during initialization
 *
 * @param subhandler   a handler to do whatever you want to do, otherwise use
 *                     NULL to use the default int handler.
 *
 * @return
 *      MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED and MIB_DUPLICATE_REGISTRATION.
 */
int
netsnmp_register_int_instance(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int *it, Netsnmp_Node_Handler * subhandler)
{
    return netsnmp_register_watched_instance2(
               netsnmp_create_handler_registration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               netsnmp_create_watcher_info(
                   (void *)it, sizeof(int), ASN_INTEGER, WATCHER_FIXED_SIZE));
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_INT_INSTANCE */

#ifdef HAVE_DMALLOC_H
static void free_wrapper(void * p)
{
    free(p);
}
#else
#define free_wrapper free
#endif

int
netsnmp_instance_num_file_handler(netsnmp_mib_handler *handler,
                                  netsnmp_handler_registration *reginfo,
                                  netsnmp_agent_request_info *reqinfo,
                                  netsnmp_request_info *requests)
{
    netsnmp_num_file_instance *nfi;
    u_long it;
#ifndef NETSNMP_NO_WRITE_SUPPORT
    u_long *it_save;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    int rc;

    netsnmp_assert(NULL != handler);
    nfi = (netsnmp_num_file_instance *)handler->myvoid;
    netsnmp_assert(NULL != nfi);
    netsnmp_assert(NULL != nfi->file_name);

    DEBUGMSGTL(("netsnmp_instance_int_handler", "Got request:  %d\n",
                reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * data requests 
         */
    case MODE_GET:
	/*
	 * Use a long here, otherwise on 64 bit use of an int would fail
	 */
        netsnmp_assert(NULL == nfi->filep);
        nfi->filep = fopen(nfi->file_name, "r");
        if (NULL == nfi->filep) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_NOSUCHINSTANCE);
            return SNMP_ERR_NOERROR;
        }
        rc = fscanf(nfi->filep, (nfi->type == ASN_INTEGER) ? "%ld" : "%lu",
                    &it);
        fclose(nfi->filep);
        nfi->filep = NULL;
        if (rc != 1) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_NOSUCHINSTANCE);
            return SNMP_ERR_NOERROR;
        }
        snmp_set_var_typed_value(requests->requestvb, nfi->type,
                                 (u_char *) &it, sizeof(it));
        break;

        /*
         * SET requests.  Should only get here if registered RWRITE 
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        netsnmp_assert(NULL == nfi->filep);
        if (requests->requestvb->type != nfi->type)
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_WRONGTYPE);
        break;

    case MODE_SET_RESERVE2:
        netsnmp_assert(NULL == nfi->filep);
        nfi->filep = fopen(nfi->file_name, "w+");
        if (NULL == nfi->filep) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_NOTWRITABLE);
            return SNMP_ERR_NOERROR;
        }
        /*
         * store old info for undo later 
         */
        if (fscanf(nfi->filep, (nfi->type == ASN_INTEGER) ? "%ld" : "%lu",
                   &it) != 1) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_RESOURCEUNAVAILABLE);
            return SNMP_ERR_NOERROR;
        }

        memdup((u_char **) & it_save, (u_char *)&it, sizeof(u_long));
        if (it_save == NULL) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_RESOURCEUNAVAILABLE);
            return SNMP_ERR_NOERROR;
        }
        netsnmp_request_add_list_data(requests,
                                      netsnmp_create_data_list
                                      (INSTANCE_HANDLER_NAME, it_save,
                                       &free_wrapper));
        break;

    case MODE_SET_ACTION:
        /*
         * update current 
         */
        DEBUGMSGTL(("helper:instance", "updated %s -> %ld\n", nfi->file_name,
                    *(requests->requestvb->val.integer)));
        it = *(requests->requestvb->val.integer);
        rewind(nfi->filep); /* rewind to make sure we are at the beginning */
        rc = fprintf(nfi->filep, (nfi->type == ASN_INTEGER) ? "%ld" : "%lu",
                     it);
        if (rc < 0) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_GENERR);
            return SNMP_ERR_NOERROR;
        }
        break;

    case MODE_SET_UNDO:
        it =
            *((u_int *) netsnmp_request_get_list_data(requests,
                                                      INSTANCE_HANDLER_NAME));
        rc = fprintf(nfi->filep, (nfi->type == ASN_INTEGER) ? "%ld" : "%lu",
                     it);
        if (rc < 0)
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_UNDOFAILED);
        /** fall through */

    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
        if (NULL != nfi->filep) {
            fclose(nfi->filep);
            nfi->filep = NULL;
        }
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    default:
        snmp_log(LOG_ERR,
                 "netsnmp_instance_num_file_handler: illegal mode\n");
        netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
        return SNMP_ERR_NOERROR;
    }

    if (handler->next && handler->next->access_method)
        return netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                         requests);
    return SNMP_ERR_NOERROR;
}

int
netsnmp_instance_helper_handler(netsnmp_mib_handler *handler,
                                netsnmp_handler_registration *reginfo,
                                netsnmp_agent_request_info *reqinfo,
                                netsnmp_request_info *requests)
{

    netsnmp_variable_list *var = requests->requestvb;

    int             ret, cmp;

    DEBUGMSGTL(("helper:instance", "Got request:\n"));
    cmp = snmp_oid_compare(requests->requestvb->name,
                           requests->requestvb->name_length,
                           reginfo->rootoid, reginfo->rootoid_len);

    DEBUGMSGTL(("helper:instance", "  oid:"));
    DEBUGMSGOID(("helper:instance", var->name, var->name_length));
    DEBUGMSG(("helper:instance", "\n"));

    switch (reqinfo->mode) {
    case MODE_GET:
        if (cmp != 0) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_NOSUCHINSTANCE);
            return SNMP_ERR_NOERROR;
        } else {
            return netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                             requests);
        }
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_UNDO:
    case MODE_SET_FREE:
        if (cmp != 0) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_NOCREATION);
            return SNMP_ERR_NOERROR;
        } else {
            return netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                             requests);
        }
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    case MODE_GETNEXT:
        if (cmp < 0 || (cmp == 0 && requests->inclusive)) {
            reqinfo->mode = MODE_GET;
            snmp_set_var_objid(requests->requestvb, reginfo->rootoid,
                               reginfo->rootoid_len);
            ret =
                netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                          requests);
            reqinfo->mode = MODE_GETNEXT;
            /*
             * if the instance doesn't have data, set type to ASN_NULL
             * to move to the next sub-tree. Ignore delegated requests; they
             * might have data later on.
             */
            if (!requests->delegated &&
                (requests->requestvb->type == SNMP_NOSUCHINSTANCE ||
                 requests->requestvb->type == SNMP_NOSUCHOBJECT)) {
                requests->requestvb->type = ASN_NULL;
            }
            return ret;
        } else {
            return SNMP_ERR_NOERROR;
        }
        break;
    default:
        snmp_log(LOG_ERR,
                 "netsnmp_instance_helper_handler: illegal mode\n");
        netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
        return SNMP_ERR_NOERROR;
    }
    /*
     * got here only if illegal mode found 
     */
    return SNMP_ERR_GENERR;
}

/** @} 
 */
