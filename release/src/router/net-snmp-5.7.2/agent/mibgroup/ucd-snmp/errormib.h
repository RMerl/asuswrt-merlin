/*
 *  Errormibess watching mib group
 */
#ifndef _MIBGROUP_ERRORMIB_H
#define _MIBGROUP_ERRORMIB_H

void            init_errormib(void);

config_require(util_funcs/header_generic)

     void            setPerrorstatus(const char *);
     void            seterrorstatus(const char *, int);
     extern FindVarMethod var_extensible_errors;

#include "mibdefs.h"

#endif                          /* _MIBGROUP_ERRORMIB_H */
