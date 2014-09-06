/*
 * instance.h 
 */
#ifndef NETSNMP_INSTANCE_H
#define NETSNMP_INSTANCE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The instance helper is designed to simplify the task of adding simple
 * * instances to the mib tree.
 */

/*
 * GETNEXTs are auto-converted to a GET.
 * * non-valid GETs are dropped.
 * * The client can assume that if you're called for a GET, it shouldn't
 * * have to check the oid at all.  Just answer.
 */

int
netsnmp_register_instance(netsnmp_handler_registration * reginfo);

int            
netsnmp_register_read_only_instance(netsnmp_handler_registration *reginfo);

#define INSTANCE_HANDLER_NAME "instance"

netsnmp_mib_handler *netsnmp_get_instance_handler(void);

int
netsnmp_register_read_only_ulong_instance(const char * name,
                                          const oid * reg_oid,
                                          size_t reg_oid_len,
                                          u_long * it,
                                          Netsnmp_Node_Handler * subhandler);
int
netsnmp_register_ulong_instance(const char * name,
                                const oid * reg_oid,
                                size_t reg_oid_len,
                                u_long * it,
                                Netsnmp_Node_Handler * subhandler);
int
netsnmp_register_read_only_counter32_instance(const char *name,
                                              const oid * reg_oid,
                                              size_t reg_oid_len,
                                              u_long * it,
                                              Netsnmp_Node_Handler *subhandler);
int
netsnmp_register_read_only_long_instance(const char *name,
                                         const oid * reg_oid,
                                         size_t reg_oid_len,
                                         long *it,
                                         Netsnmp_Node_Handler * subhandler);
int
netsnmp_register_long_instance(const char *name,
                               const oid * reg_oid,
                               size_t reg_oid_len,
                               long *it,
                               Netsnmp_Node_Handler * subhandler);

int
netsnmp_register_read_only_int_instance(const char *name,
                                        const oid * reg_oid,
                                        size_t reg_oid_len, int *it,
                                        Netsnmp_Node_Handler * subhandler);

int
netsnmp_register_int_instance(const char *name,
                              const oid * reg_oid,
                              size_t reg_oid_len, int *it,
                              Netsnmp_Node_Handler * subhandler);

/* identical functions that register a in a particular context */
int
netsnmp_register_read_only_ulong_instance_context(const char *name,
                                                  const oid * reg_oid,
                                                  size_t reg_oid_len,
                                                  u_long * it,
                                                  Netsnmp_Node_Handler
                                                  * subhandler,
                                                  const char *contextName);
int
netsnmp_register_ulong_instance_context(const char *name,
                                        const oid * reg_oid,
                                        size_t reg_oid_len,
                                        u_long * it,
                                        Netsnmp_Node_Handler * subhandler,
                                        const char *contextName);
int
netsnmp_register_read_only_counter32_instance_context(const char *name,
                                                      const oid * reg_oid,
                                                      size_t reg_oid_len,
                                                      u_long * it,
                                                      Netsnmp_Node_Handler
                                                      * subhandler,
                                                      const char *contextName);
int
netsnmp_register_read_only_long_instance_context(const char *name,
                                                 const oid * reg_oid,
                                                 size_t reg_oid_len,
                                                 long *it,
                                                 Netsnmp_Node_Handler
                                                 * subhandler,
                                                 const char *contextName);
int
netsnmp_register_long_instance_context(const char *name,
                                       const oid * reg_oid,
                                       size_t reg_oid_len,
                                       long *it,
                                       Netsnmp_Node_Handler * subhandler,
                                       const char *contextName);

int
netsnmp_register_read_only_int_instance_context(const char *name,
                                                const oid * reg_oid,
                                                size_t reg_oid_len, int *it,
                                                Netsnmp_Node_Handler *
                                                subhandler,
                                                const char *contextName);

int
netsnmp_register_int_instance_context(const char *name,
                                      const oid * reg_oid,
                                      size_t reg_oid_len, int *it,
                                      Netsnmp_Node_Handler * subhandler,
                                      const char *contextName);

int
netsnmp_register_num_file_instance(const char *name,
                                   const oid * reg_oid, size_t reg_oid_len,
                                   const char *file_name, int asn_type, int mode,
                                   Netsnmp_Node_Handler * subhandler,
                                   const char *contextName);

Netsnmp_Node_Handler netsnmp_instance_helper_handler;
Netsnmp_Node_Handler netsnmp_instance_num_file_handler;

#ifdef __cplusplus
}
#endif

#endif /** NETSNMP_INSTANCE_H */
