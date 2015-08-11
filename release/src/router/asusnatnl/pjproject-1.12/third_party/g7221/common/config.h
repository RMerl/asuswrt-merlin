#ifndef __LIBG7221_CONFIG_H__
#define __LIBG7221_CONFIG_H__

#include <pj/config.h>

/**
 * Expand all basic operation functions as inline. Even if this is set to
 * zero, some critical functions would still be expanded as inline. Note
 * also that enabling this may generate some warning messages about functions
 * not being referenced (with gcc).
 *
 * Default: 0 (no)
 */
#ifndef PJMEDIA_LIBG7221_FUNCS_INLINED
#   define PJMEDIA_LIBG7221_FUNCS_INLINED   0
#endif

/* Declare/define a function that may be expanded as inline. */
#if PJMEDIA_LIBG7221_FUNCS_INLINED
#  define LIBG7221_DECL(type)  PJ_INLINE(type)
#  define LIBG7221_DEF(type)   PJ_INLINE(type)
#else
#  define LIBG7221_DECL(type)  PJ_DECL(type)
#  define LIBG7221_DEF(type)   PJ_DEF(type)
#endif

#endif /* __LIBG7221_CONFIG_H__ */
