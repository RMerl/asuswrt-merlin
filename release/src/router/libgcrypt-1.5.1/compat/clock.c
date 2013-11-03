/* clock.c - Replacement for WindowsCE
   Copyright (C) 2010 Free Software Foundation, Inc.

   This file is free software; as a special exception the author gives
   unlimited permission to copy and/or distribute it, with or without
   modifications, as long as this notice is preserved.

   This file is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY, to the extent permitted by law; without even
   the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_W32CE_SYSTEM
#include <windows.h>
#include <time.h>
#include <assert.h>

clock_t
_gcry_clock (void)
{
  assert (CLOCKS_PER_SEC == 1000);
#warning Replace by a correct implementaion.
  /* It seems that GetProcessTimes is available in the kernel but
     without a declaration.  If that fails we would need to walk over
     all threads and tally up the GetThreadTimes.  */

  return GetTickCount ();
}

#else
# error No replacement function for clock known
#endif
