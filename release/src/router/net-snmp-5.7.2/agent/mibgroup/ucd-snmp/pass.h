/*
 *  pass: pass through extensiblity
 */
#ifndef _MIBGROUP_PASS_H
#define _MIBGROUP_PASS_H

void            init_pass(void);

config_require(ucd-snmp/pass_common)
config_require(util_funcs)
config_require(utilities/execute)
config_add_mib(NET-SNMP-PASS-MIB)

extern FindVarMethod var_extensible_pass;
WriteMethod     setPass;
int             pass_compare(const void *, const void *);

/*
 * config file parsing routines 
 */
void            pass_free_config(void);
void            pass_parse_config(const char *, char *);

#include "mibdefs.h"

#endif                          /* _MIBGROUP_PASS_H */
