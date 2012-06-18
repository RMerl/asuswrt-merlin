/* The MIPS architecture has selectable endianness.
   Linux/MIPS exists in two both little and big endian flavours and we
   want to be able to share the installed headerfiles between both,
   so we define __BYTE_ORDER based on GCC's predefines.  */

#ifndef _ENDIAN_H
# error "Never use <bits/endian.h> directly; include <endian.h> instead."
#endif

#ifdef __MIPSEB__
# define __BYTE_ORDER __BIG_ENDIAN
#else
# ifdef __MIPSEL__
#  define __BYTE_ORDER __LITTLE_ENDIAN
# endif
#endif
