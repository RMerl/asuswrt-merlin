#ifndef _MIBGROUP_UTIL_FUNCS_RESTART_H
#define _MIBGROUP_UTIL_FUNCS_RESTART_H

#ifdef __cplusplus
extern "C" {
#endif

RETSIGTYPE      restart_doit(int);
WriteMethod     restart_hook;

#ifdef __cplusplus
}
#endif

#endif /* _MIBGROUP_UTIL_FUNCS_RESTART_H */
