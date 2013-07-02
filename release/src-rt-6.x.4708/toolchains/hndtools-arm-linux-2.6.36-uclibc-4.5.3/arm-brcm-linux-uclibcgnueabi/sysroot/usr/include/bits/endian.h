#ifndef _ENDIAN_H
# error "Never use <bits/endian.h> directly; include <endian.h> instead."
#endif

/* ARM can be either big or little endian.  */
#ifdef __ARMEB__
# define __BYTE_ORDER __BIG_ENDIAN
#else
# define __BYTE_ORDER __LITTLE_ENDIAN
#endif

/* FPA floating point units are always big-endian, irrespective of the
   CPU endianness.  VFP floating point units use the same endianness
   as the rest of the system.  */
#if defined __VFP_FP__ || defined __MAVERICK__
# define __FLOAT_WORD_ORDER __BYTE_ORDER
#else
# define __FLOAT_WORD_ORDER __BIG_ENDIAN
#endif
