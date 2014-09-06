/*
 *  pass: pass through extensiblity
 */
#ifndef _MIBGROUP_PASS_PERSIST_H
#define _MIBGROUP_PASS_PERSIST_H

config_require(ucd-snmp/pass_common)
config_require(util_funcs)
config_require(utilities/execute)

void            init_pass_persist(void);
void            shutdown_pass_persist(void);
extern FindVarMethod var_extensible_pass_persist;
extern WriteMethod setPassPersist;

/*
 * config file parsing routines 
 */
void            pass_persist_free_config(void);
void            pass_persist_parse_config(const char *, char *);
int             pass_persist_compare(const void *, const void *);

#include "mibdefs.h"

#endif                          /* _MIBGROUP_PASS_PERSIST_H */
