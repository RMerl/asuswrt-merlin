/*
 *  Host Resources MIB - system group interface - hr_system.h
 *
 */
#ifndef _MIBGROUP_HRSYSTEM_H
#define _MIBGROUP_HRSYSTEM_H

extern void     init_hr_system(void);
extern FindVarMethod var_hrsys;

#if defined(HAVE_MKTIME) && defined(HAVE_STIME)
#ifndef NETSNMP_NO_WRITE_SUPPORT
int ns_set_time(int action, u_char * var_val, u_char var_val_type, size_t var_val_len, u_char * statP, oid * name, size_t name_len);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
#endif

#endif                          /* _MIBGROUP_HRSYSTEM_H */
