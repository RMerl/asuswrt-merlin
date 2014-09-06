/*
 *  Process watching mib group
 */
#ifndef _MIBGROUP_PROC_H
#define _MIBGROUP_PROC_H

config_require(util_funcs)

     void            init_proc(void);

     extern FindVarMethod var_extensible_proc;
     extern WriteMethod fixProcError;
     int             sh_count_procs(char *);

/*
 * config file parsing routines 
 */
     void            proc_free_config(void);
     void            proc_parse_config(const char *, char *);
     void            procfix_parse_config(const char *, char *);

#include "mibdefs.h"

#define PROCMIN 3
#define PROCMAX 4
#define PROCCOUNT 5

#endif                          /* _MIBGROUP_PROC_H */
