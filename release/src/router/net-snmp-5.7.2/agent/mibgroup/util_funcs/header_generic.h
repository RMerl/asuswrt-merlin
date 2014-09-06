/*
 *  util_funcs/header_generic.h:  utilitiy functions for extensible groups.
 */
#ifndef _MIBGROUP_UTIL_FUNCS_HEADER_GENERIC_H
#define _MIBGROUP_UTIL_FUNCS_HEADER_GENERIC_H

#ifdef __cplusplus
extern "C" {
#endif

int header_generic(struct variable *, oid *, size_t *, int, size_t *,
                   WriteMethod **);

#ifdef __cplusplus
}
#endif

#endif /* _MIBGROUP_UTIL_FUNCS_HEADER_GENERIC_H */
