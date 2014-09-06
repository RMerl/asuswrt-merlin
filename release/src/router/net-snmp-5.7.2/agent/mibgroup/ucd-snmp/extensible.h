/*
 *  Template MIB group interface - extensible.h
 *
 */
#ifndef _MIBGROUP_EXTENSIBLE_H
#define _MIBGROUP_EXTENSIBLE_H

void            init_extensible(void);

config_require(util_funcs/header_simple_table)
config_require(util_funcs)
config_require(utilities/execute)

     extern FindVarMethod var_extensible_shell;
     extern WriteMethod fixExecError;
     extern FindVarMethod var_extensible_relocatable;
     netsnmp_subtree *find_extensible(netsnmp_subtree *, oid *, size_t, int);

/*
 * config file parsing routines 
 */
     void            extensible_free_config(void);
     void            extensible_parse_config(const char *, char *);
     void            execfix_parse_config(const char *, char *);
     int             extensible_unregister(int, int, void *, void *);

#include "mibdefs.h"

#define SHELLCOMMAND 3
#define SHELLRESULT 6
#define SHELLOUTPUT 7

#endif                          /* _MIBGROUP_EXTENSIBLE_H */
