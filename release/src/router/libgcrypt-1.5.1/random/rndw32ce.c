/* rndw32ce.c  -  W32CE entropy gatherer
 * Copyright (C) 2010 Free Software Foundation, Inc.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <windows.h>
#include <wincrypt.h>

#include "types.h"
#include "g10lib.h"
#include "rand-internal.h"


/* The Microsoft docs say that it is suggested to see the buffer with
   some extra random.  We do this, despite that it is a questionable
   suggestion as the OS as better means of collecting entropy than an
   application.  */
static size_t filler_used;
static size_t filler_length;
static unsigned char *filler_buffer;

static void
filler (const void *data, size_t datalen, enum random_origins dummy)
{
  (void)dummy;
  if (filler_used + datalen > filler_length)
    datalen = filler_length - filler_used;
  memcpy (filler_buffer + filler_used, data, datalen);
  filler_used += datalen;
}


static void
fillup_buffer (unsigned char *buffer, size_t length)
{
  filler_used = 0;
  filler_length = length;
  filler_buffer = buffer;

  while (filler_used < length)
    _gcry_rndw32ce_gather_random_fast (filler, 0);
}


int
_gcry_rndw32ce_gather_random (void (*add)(const void*, size_t,
                                          enum random_origins),
                              enum random_origins origin,
                              size_t length, int level )
{
  HCRYPTPROV prov;
  unsigned char buffer [256];
  DWORD buflen;

  if (!level)
    return 0;

  /* Note that LENGTH is not really important because the caller
     checks the returned lengths and calls this function until it
     feels that enough entropy has been gathered.  */

  buflen = sizeof buffer;
  if (length+8 < buflen)
    buflen = length+8;  /* Return a bit more than requested.  */

  if (!CryptAcquireContext (&prov, NULL, NULL, PROV_RSA_FULL,
                           (CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) )
    log_debug ("CryptAcquireContext failed: rc=%d\n", (int)GetLastError ());
  else
    {
      fillup_buffer (buffer, buflen);
      if (!CryptGenRandom (prov, buflen, buffer))
        log_debug ("CryptGenRandom(%d) failed: rc=%d\n",
                   (int)buflen, (int)GetLastError ());
      else
        (*add) (buffer, buflen, origin);
      CryptReleaseContext (prov, 0);
      wipememory (buffer, sizeof buffer);
    }

  return 0;
}



void
_gcry_rndw32ce_gather_random_fast (void (*add)(const void*, size_t,
                                             enum random_origins),
                                   enum random_origins origin)
{

  /* Add word sized values.  */
  {
#   define ADD(t,f)  do {                                          \
      t along = (f);                                               \
      memcpy (bufptr, &along, sizeof (along));                     \
      bufptr += sizeof (along);                                    \
    } while (0)
    unsigned char buffer[20*sizeof(ulong)], *bufptr;

    bufptr = buffer;
    ADD (HWND,   GetActiveWindow ());
    ADD (HWND,   GetCapture ());
    ADD (HWND,   GetClipboardOwner ());
    ADD (HANDLE, GetCurrentProcess ());
    ADD (DWORD,  GetCurrentProcessId ());
    ADD (HANDLE, GetCurrentThread ());
    ADD (DWORD,  GetCurrentThreadId ());
    ADD (HWND,   GetDesktopWindow ());
    ADD (HWND,   GetFocus ());
    ADD (DWORD,  GetMessagePos ());
    ADD (HWND,   GetOpenClipboardWindow ());
    ADD (HWND,   GetProcessHeap ());
    ADD (DWORD,  GetQueueStatus (QS_ALLEVENTS));
    ADD (DWORD,  GetTickCount ());

    gcry_assert ( bufptr-buffer < sizeof (buffer) );
    (*add) ( buffer, bufptr-buffer, origin );
#   undef ADD
  }

  /* Get multiword system information: Current caret position, current
     mouse cursor position.  */
  {
    POINT point;

    GetCaretPos (&point);
    (*add) ( &point, sizeof (point), origin );
    GetCursorPos (&point);
    (*add) ( &point, sizeof (point), origin );
  }

  /* Get percent of memory in use, bytes of physical memory, bytes of
     free physical memory, bytes in paging file, free bytes in paging
     file, user bytes of address space, and free user bytes.  */
  {
    MEMORYSTATUS memoryStatus;

    memoryStatus.dwLength = sizeof (MEMORYSTATUS);
    GlobalMemoryStatus (&memoryStatus);
    (*add) ( &memoryStatus, sizeof (memoryStatus), origin );
  }


  /* Get thread and process creation time, exit time, time in kernel
     mode, and time in user mode in 100ns intervals.  */
  {
    HANDLE handle;
    FILETIME creationTime, exitTime, kernelTime, userTime;

    handle = GetCurrentThread ();
    GetThreadTimes (handle, &creationTime, &exitTime,
                    &kernelTime, &userTime);
    (*add) ( &creationTime, sizeof (creationTime), origin );
    (*add) ( &exitTime, sizeof (exitTime), origin );
    (*add) ( &kernelTime, sizeof (kernelTime), origin );
    (*add) ( &userTime, sizeof (userTime), origin );

    handle = GetCurrentThread ();
    GetThreadTimes (handle, &creationTime, &exitTime,
                     &kernelTime, &userTime);
    (*add) ( &creationTime, sizeof (creationTime), origin );
    (*add) ( &exitTime, sizeof (exitTime), origin );
    (*add) ( &kernelTime, sizeof (kernelTime), origin );
    (*add) ( &userTime, sizeof (userTime), origin );

  }


  /* In case the OEM provides a high precision timer get this.  If
     none is available the default implementation returns the
     GetTickCount.  */
  {
    LARGE_INTEGER performanceCount;

    if (QueryPerformanceCounter (&performanceCount))
      (*add) (&performanceCount, sizeof (performanceCount), origin);
  }

}
