/* If we're being compiled as a .c file, rather than being included in
   d10v_sim.h, then ENDIAN_INLINE won't be defined yet.  */

#ifndef ENDIAN_INLINE
#define NO_ENDIAN_INLINE
#include "d10v_sim.h"
#define ENDIAN_INLINE
#endif

ENDIAN_INLINE uint16
get_word (x)
      uint8 *x;
{
#if (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)) && defined(__GNUC__)

  unsigned short word = *(unsigned short *)x;
  __asm__ ("xchgb %b0,%h0" : "=q" (word) : "0" (word));
  return word;

#elif defined(WORDS_BIGENDIAN)
  /* It is safe to do this on big endian hosts, since the d10v requires that words be
     aligned on 16-bit boundaries.  */
  return *(uint16 *)x;

#else
  return ((uint16)x[0]<<8) + x[1];
#endif
}

ENDIAN_INLINE uint32
get_longword (x)
      uint8 *x;
{
#if (defined(__i486__) || defined(__i586__) || defined(__i686__)) && defined(__GNUC__) && defined(USE_BSWAP)

  unsigned int long_word = *(unsigned *)x;
  __asm__ ("bswap %0" : "=r" (long_word) : "0" (long_word));
  return long_word;

#elif (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)) && defined(__GNUC__)

  unsigned int long_word = *(unsigned *)x;
  __asm__("xchgb %b0,%h0\n\t"		/* swap lower bytes	*/
	  "rorl $16,%0\n\t"		/* swap words		*/
	  "xchgb %b0,%h0"		/* swap higher bytes	*/
	  :"=q" (long_word)
	  : "0" (long_word));

  return long_word;

#elif (defined(_POWER) && defined(_AIX)) || (defined(__PPC__) && defined(__BIG_ENDIAN__))
  /* Power & PowerPC computers in big endian mode can handle unaligned loads&stores */
  return *(uint32 *)x;

#elif defined(WORDS_BIGENDIAN)
  /* long words must be aligned on at least 16-bit boundaries, so this should be safe.  */
  return (((uint32) *(uint16 *)x)<<16) | ((uint32) *(uint16 *)(x+2));

#else
  return ((uint32)x[0]<<24) + ((uint32)x[1]<<16) + ((uint32)x[2]<<8) + ((uint32)x[3]);
#endif
}

ENDIAN_INLINE int64
get_longlong (x)
      uint8 *x;
{
  uint32 top = get_longword (x);
  uint32 bottom = get_longword (x+4);
  return (((int64)top)<<32) | (int64)bottom;
}

ENDIAN_INLINE void
write_word (addr, data)
     uint8 *addr;
     uint16 data;
{
#if (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)) && defined(__GNUC__)

  __asm__ ("xchgb %b0,%h0" : "=q" (data) : "0" (data));
  *(uint16 *)addr = data;

#elif defined(WORDS_BIGENDIAN)
  /* It is safe to do this on big endian hosts, since the d10v requires that words be
     aligned on 16-bit boundaries.  */
  *(uint16 *)addr = data;

#else
  addr[0] = (data >> 8) & 0xff;
  addr[1] = data & 0xff;
#endif
}

ENDIAN_INLINE void
write_longword (addr, data)
     uint8 *addr;
     uint32 data;
{
#if (defined(__i486__) || defined(__i586__) || defined(__i686__)) && defined(__GNUC__) && defined(USE_BSWAP)

  __asm__ ("bswap %0" : "=r" (data) : "0" (data));
  *(uint32 *)addr = data;

#elif (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)) && defined(__GNUC__)

  __asm__("xchgb %b0,%h0\n\t"		/* swap lower bytes	*/
	  "rorl $16,%0\n\t"		/* swap words		*/
	  "xchgb %b0,%h0"		/* swap higher bytes	*/
	  :"=q" (data)
	  : "0" (data));

  *(uint32 *)addr = data;

#elif (defined(_POWER) && defined(_AIX)) || (defined(__PPC__) && defined(__BIG_ENDIAN__))
  /* Power & PowerPC computers in big endian mode can handle unaligned loads&stores */
  *(uint32 *)addr = data;

#elif defined(WORDS_BIGENDIAN)
  *(uint16 *)addr = (uint16)(data >> 16);
  *(uint16 *)(addr + 2) = (uint16)data;

#else
  addr[0] = (data >> 24) & 0xff;
  addr[1] = (data >> 16) & 0xff;
  addr[2] = (data >> 8) & 0xff;
  addr[3] = data & 0xff;
#endif
}

ENDIAN_INLINE void
write_longlong (addr, data)
     uint8 *addr;
     int64 data;
{
  write_longword (addr, (uint32)(data >> 32));
  write_longword (addr+4, (uint32)data);
}
