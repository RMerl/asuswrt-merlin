/*
 * watcher.h 
 */
#ifndef NETSNMP_WATCHER_H
#define NETSNMP_WATCHER_H

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup watcher
 *  @{
 */

/*
 * if handler flag has this bit set, the timestamp will be
 * treated as a pointer to the timestamp. If this bit is
 * not set (the default), the timestamp is a struct timeval
 * that must be compared to the agent starttime.
 */
#define NETSNMP_WATCHER_DIRECT MIB_HANDLER_CUSTOM1

/** The size of the watched object is constant.
 *  @hideinitializer
 */
#define WATCHER_FIXED_SIZE     0x01
/** The maximum size of the watched object is stored in max_size.
 *  If WATCHER_SIZE_STRLEN is set then it is supposed that max_size + 1
 *  bytes could be stored in the buffer.
 *  @hideinitializer
 */
#define WATCHER_MAX_SIZE       0x02
/** If set then the variable data_size_p points to is supposed to hold the
 *  current size of the watched object and will be updated on writes.
 *  @hideinitializer
 *  @since Net-SNMP 5.5
 */
#define WATCHER_SIZE_IS_PTR    0x04
/** If set then data is suppposed to be a zero-terminated character array
 *  and both data_size and data_size_p are ignored. Additionally \\0 is a
 *  forbidden character in the data set.
 *  @hideinitializer
 *  @since Net-SNMP 5.5
 */
#define WATCHER_SIZE_STRLEN    0x08
/** If set then size is in units of object identifiers.
 *  This is useful if you have an OID and tracks the OID_LENGTH of it as
 *  opposed to it's size.
 *  @hideinitializer
 *  @since Net-SNMP 5.5.1
 */
#define WATCHER_SIZE_UNIT_OIDS 0x10

typedef struct netsnmp_watcher_info_s {
    void     *data;
    size_t    data_size;
    size_t    max_size;
    u_char    type;
    int       flags;
    size_t   *data_size_p;
} netsnmp_watcher_info;

/** @} */

int netsnmp_register_watched_instance( netsnmp_handler_registration *reginfo,
                                       netsnmp_watcher_info         *winfo);
int netsnmp_register_watched_instance2(netsnmp_handler_registration *reginfo,
                                       netsnmp_watcher_info         *winfo);
int netsnmp_register_watched_scalar(   netsnmp_handler_registration *reginfo,
                                       netsnmp_watcher_info         *winfo);
int netsnmp_register_watched_scalar2(  netsnmp_handler_registration *reginfo,
                                       netsnmp_watcher_info         *winfo);
int netsnmp_register_watched_timestamp(netsnmp_handler_registration *reginfo,
                                       marker_t timestamp);
int netsnmp_watched_timestamp_register(netsnmp_mib_handler *whandler,
                                       netsnmp_handler_registration *reginfo,
                                       marker_t timestamp);
int netsnmp_register_watched_spinlock(netsnmp_handler_registration *reginfo,
                                      int *spinlock);    

/*
 * Convenience registration calls
 */

int netsnmp_register_ulong_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              Netsnmp_Node_Handler * subhandler);
int netsnmp_register_read_only_ulong_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              Netsnmp_Node_Handler * subhandler);
int netsnmp_register_long_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              Netsnmp_Node_Handler * subhandler);
int netsnmp_register_read_only_long_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              Netsnmp_Node_Handler * subhandler);
int netsnmp_register_int_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              Netsnmp_Node_Handler * subhandler);
int netsnmp_register_read_only_int_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              Netsnmp_Node_Handler * subhandler);
int netsnmp_register_read_only_counter32_scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              Netsnmp_Node_Handler * subhandler);

#define WATCHER_HANDLER_NAME "watcher"

netsnmp_mib_handler  *netsnmp_get_watcher_handler(void);

netsnmp_watcher_info *
netsnmp_init_watcher_info(netsnmp_watcher_info *, void *, size_t, u_char, int);

netsnmp_watcher_info *
netsnmp_init_watcher_info6(netsnmp_watcher_info *,
			   void *, size_t, u_char, int, size_t, size_t*);

netsnmp_watcher_info *
netsnmp_create_watcher_info(void *, size_t, u_char, int);

netsnmp_watcher_info *
netsnmp_create_watcher_info6(void *, size_t, u_char, int, size_t, size_t*);

netsnmp_watcher_info *
netsnmp_clone_watcher_info(netsnmp_watcher_info *winfo);
void
netsnmp_owns_watcher_info(netsnmp_mib_handler *handler);

Netsnmp_Node_Handler  netsnmp_watcher_helper_handler;

netsnmp_mib_handler  *netsnmp_get_watched_timestamp_handler(void);
Netsnmp_Node_Handler  netsnmp_watched_timestamp_handler;

netsnmp_mib_handler  *netsnmp_get_watched_spinlock_handler(void);
Netsnmp_Node_Handler  netsnmp_watched_spinlock_handler;

#ifdef __cplusplus
}
#endif

#endif /** NETSNMP_WATCHER_H */
