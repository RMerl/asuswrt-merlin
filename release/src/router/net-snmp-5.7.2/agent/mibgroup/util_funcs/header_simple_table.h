/*
 * util_funcs/header_simple_table.h:  utilitiy functions for extensible
 * groups.
 */
#ifndef _MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H
#define _MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

int header_simple_table(struct variable *, oid *, size_t *, int, size_t *,
                        WriteMethod **, int);

#ifdef __cplusplus
}
#endif

#endif /* _MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H */
