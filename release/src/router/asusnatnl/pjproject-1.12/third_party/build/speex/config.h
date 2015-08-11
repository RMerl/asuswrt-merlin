#include <pj/types.h>

/* Check if we need to use the fixed point version */
#if !defined(PJ_HAS_FLOATING_POINT) || PJ_HAS_FLOATING_POINT==0
#   define FIXED_POINT
#   define USE_KISS_FFT
#else 
#   define FLOATING_POINT
#   define USE_SMALLFT
#endif

#define EXPORT

#if (defined(PJ_WIN32) && PJ_WIN32!=0) || \
    (defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE != 0) 
#   include "../../speex/win32/config.h"
#else
#define inline __inline
#define restrict
#endif

#ifdef _MSC_VER
#   pragma warning(disable: 4100)   // unreferenced formal parameter
#   pragma warning(disable: 4101)   // unreferenced local variable
#   pragma warning(disable: 4244)   // conversion from 'double ' to 'float '
#   pragma warning(disable: 4305)   // truncation from 'const double ' to 'float '
#   pragma warning(disable: 4018)   // signed/unsigned mismatch
//#   pragma warning(disable: 4701)   // local variable used without initialized
#endif

#include <pj/log.h>

/*
 * Override miscellaneous Speex functions.
 */
#define OVERRIDE_SPEEX_ERROR
#define speex_error(str) PJ_LOG(4,("speex", "error: %s", str))

#define OVERRIDE_SPEEX_WARNING
#define speex_warning(str) PJ_LOG(5,("speex", "warning: %s", str))

#define OVERRIDE_SPEEX_WARNING_INT
#define speex_warning_int(str,val)  PJ_LOG(5,("speex", "warning: %s: %d", str, val))

