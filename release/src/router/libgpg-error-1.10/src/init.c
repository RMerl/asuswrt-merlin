/* init.c - Initialize the GnuPG error library.
   Copyright (C) 2005, 2010 g10 Code GmbH

   This file is part of libgpg-error.

   libgpg-error is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   libgpg-error is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <gpg-error.h>

#include "gettext.h"
#include "init.h"

#ifdef HAVE_W32CE_SYSTEM
# include "mkw32errmap.map.c"  /* Generated map_w32codes () */
#endif


/* Locale directory support.  */

#if HAVE_W32_SYSTEM

#include <windows.h>

static int tls_index = TLS_OUT_OF_INDEXES;  /* Index for the TLS functions.  */ 

static char *get_locale_dir (void);
static void drop_locale_dir (char *locale_dir);

#else /*!HAVE_W32_SYSTEM*/

#define get_locale_dir() LOCALEDIR
#define drop_locale_dir(dir)

#endif /*!HAVE_W32_SYSTEM*/


static void
real_init (void)
{
#ifdef ENABLE_NLS
  char *locale_dir;

  /* We only have to bind our locale directory to our text domain.  */
  locale_dir = get_locale_dir ();
  if (locale_dir)
    {
      bindtextdomain (PACKAGE, locale_dir);
      drop_locale_dir (locale_dir);
    }
#endif
}

/* Initialize the library.  This function should be run early.  */
gpg_error_t
gpg_err_init (void)
{
#ifdef HAVE_W32_SYSTEM
# ifdef DLL_EXPORT
  /* We always have a constructor and thus this function is called
     automatically.  Due to the way the C init code of mingw works,
     the constructors are called before our DllMain function is
     called.  The problem with that is that the TLS has not been setup
     and w32-gettext.c requires TLS.  To solve this we do nothing here
     but call the actual init code from our DllMain.  */
# else /*!DLL_EXPORT*/
  /* Note that if the TLS is actually used, we can't release the TLS
     as there is no way to know when a thread terminates (i.e. no
     thread-specific-atexit).  You are really better off to use the
     DLL! */
  if (tls_index == TLS_OUT_OF_INDEXES)
    {
      tls_index = TlsAlloc ();
      if (tls_index == TLS_OUT_OF_INDEXES)
        {
          /* No way to continue - commit suicide.  */
          abort ();
        }
      _gpg_w32__init_gettext_module ();
      real_init ();
    }
# endif /*!DLL_EXPORT*/
#else
  real_init ();
#endif
  return 0;
}


/* Deinitialize libgpg-error.  This function is only used in special
   circumstances.  No gpg-error function should be used after this
   function has been called.  A value of 0 passed for MODE
   deinitializes the entire libgpg-error, a value of 1 releases
   resources allocated for the current thread and only that thread may
   not anymore access libgpg-error after such a call.  Under Windows
   this function may be called from the DllMain function of a DLL
   which statically links to libgpg-error.  */
void
gpg_err_deinit (int mode)
{
#if defined (HAVE_W32_SYSTEM) && !defined(DLL_EXPORT)
  struct tls_space_s *tls;
  
  tls = TlsGetValue (tls_index);
  if (tls)
    {
      TlsSetValue (tls_index, NULL);
      LocalFree (tls);
    }

  if (mode == 0)
    {
      TlsFree (tls_index);
      tls_index = TLS_OUT_OF_INDEXES;
    }
#else
  (void)mode;
#endif
}



#ifdef HAVE_W32_SYSTEM

/* Return a malloced string encoded in UTF-8 from the wide char input
   string STRING.  Caller must free this value.  Returns NULL on
   failure.  Caller may use GetLastError to get the actual error
   number.  The result of calling this function with STRING set to
   NULL is not defined.  */
static char *
wchar_to_utf8 (const wchar_t *string)
{
  int n;
  char *result;

  /* Note, that CP_UTF8 is not defined in Windows versions earlier
     than NT.  */
  n = WideCharToMultiByte (CP_UTF8, 0, string, -1, NULL, 0, NULL, NULL);
  if (n < 0)
    return NULL;

  result = malloc (n+1);
  if (result)
    {
      n = WideCharToMultiByte (CP_UTF8, 0, string, -1, result, n, NULL, NULL);
      if (n < 0)
        {
          free (result);
          result = NULL;
        }
    }
  return result;
}


/* Return a malloced wide char string from an UTF-8 encoded input
   string STRING.  Caller must free this value.  Returns NULL on
   failure.  Caller may use GetLastError to get the actual error
   number.  The result of calling this function with STRING set to
   NULL is not defined.  */
static wchar_t *
utf8_to_wchar (const char *string)
{
  int n;
  wchar_t *result;

  n = MultiByteToWideChar (CP_UTF8, 0, string, -1, NULL, 0);
  if (n < 0)
    return NULL;

  result = malloc ((n+1) * sizeof *result);
  if (result)
    {
      n = MultiByteToWideChar (CP_UTF8, 0, string, -1, result, n);
      if (n < 0)
        {
          free (result);
          result = NULL;
        }
      return NULL;
    }
  return result;
}


static char *
get_locale_dir (void)
{
  static wchar_t moddir[MAX_PATH+5];
  char *result, *p;
  int nbytes;

  if (!GetModuleFileNameW (NULL, moddir, MAX_PATH))
    *moddir = 0;

#define SLDIR "\\share\\locale"
  if (*moddir)
    {
      nbytes = WideCharToMultiByte (CP_UTF8, 0, moddir, -1, NULL, 0, NULL, NULL);
      if (nbytes < 0)
        return NULL;
      
      result = malloc (nbytes + strlen (SLDIR) + 1);
      if (result)
        {
          nbytes = WideCharToMultiByte (CP_UTF8, 0, moddir, -1,
                                        result, nbytes, NULL, NULL);
          if (nbytes < 0)
            {
              free (result);
              result = NULL;
            }
          else
            {
              p = strrchr (result, '\\');
              if (p)
                *p = 0;
              /* If we are installed below "bin" strip that part and
                 use the top directory instead.

                 Background: Under Windows we don't install GnuPG
                 below bin/ but in the top directory with only share/,
                 lib/, and etc/ below it.  One of the reasons is to
                 keep the the length of the filenames at bay so not to
                 increase the limited length of the PATH envvar.
                 Another and more important reason, however, is that
                 the very first GPG versions on W32 were installed
                 into a flat directory structure and for best
                 compatibility with these versions we didn't changed
                 that later.  For WindowsCE we can right away install
                 it under bin, though.  The hack with detection of the
                 bin directory part allows us to eventually migrate to
                 such a directory layout under plain Windows without
                 the need to change libgpg-error.  */
              p = strrchr (result, '\\');
              if (p && !strcmp (p+1, "bin"))
                *p = 0;
              /* Append the static part.  */
              strcat (result, SLDIR);
            }
        }
    }
  else /* Use the old default value.  */
    {
      result = malloc (10 + strlen (SLDIR) + 1);
      if (result)
        {
          strcpy (result, "c:\\gnupg");
          strcat (result, SLDIR);
        }
    }  
#undef SLDIR  
  return result;
}


static void
drop_locale_dir (char *locale_dir)
{
  free (locale_dir);
}


/* Return the tls object.  This function is guaranteed to return a
   valid non-NULL object.  */
struct tls_space_s *
get_tls (void)
{
  struct tls_space_s *tls;

  tls = TlsGetValue (tls_index);
  if (!tls)
    {
      /* Called by a thread which existed before this DLL was loaded.
         Allocate the space.  */
      tls = LocalAlloc (LPTR, sizeof *tls);
      if (!tls)
        {
          /* No way to continue - commit suicide.  */
          abort ();
        }
      tls->gt_use_utf8 = 0;
      TlsSetValue (tls_index, tls);
    }
        
  return tls;
}


/* Return the value of the ERRNO variable.  This needs to be a
   function so that we can have a per-thread ERRNO.  This is used only
   on WindowsCE because that OS misses an errno.   */
#ifdef HAVE_W32CE_SYSTEM
int
_gpg_w32ce_get_errno (void)
{
  return map_w32codes ( GetLastError () );
}
#endif /*HAVE_W32CE_SYSTEM*/


/* Replacement strerror function for WindowsCE.  */
#ifdef HAVE_W32CE_SYSTEM
char *
_gpg_w32ce_strerror (int err)
{
  struct tls_space_s *tls = get_tls ();
  wchar_t tmpbuf[STRBUFFER_SIZE];
  int n;

  if (err == -1)
    err = _gpg_w32ce_get_errno ();

  /* Note: On a German HTC Touch Pro2 device I also tried
     LOCALE_USER_DEFAULT and LOCALE_SYSTEM_DEFAULT - both returned
     English messages.  */
  if (FormatMessageW (FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                      MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                      tmpbuf, STRBUFFER_SIZE -1,
                      NULL))
    {
      n = WideCharToMultiByte (CP_UTF8, 0, tmpbuf, -1,
                               tls->strerror_buffer,
                               sizeof tls->strerror_buffer -1,
                               NULL, NULL);
    }
  else
    n = -1;

  if (n < 0)
    snprintf (tls->strerror_buffer, sizeof tls->strerror_buffer -1,
              "[w32err=%d]", err);
  return tls->strerror_buffer;    
}
#endif /*HAVE_W32CE_SYSTEM*/


void
gpg_err_set_errno (int err)
{
#ifdef HAVE_W32CE_SYSTEM
  SetLastError (err);
#else /*!HAVE_W32CE_SYSTEM*/
  errno = err;
#endif /*!HAVE_W32CE_SYSTEM*/
}


/* Entry point called by the DLL loader.  */
#ifdef DLL_EXPORT
int WINAPI
DllMain (HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
  struct tls_space_s *tls;
  (void)reserved;

  switch (reason)
    {
    case DLL_PROCESS_ATTACH:
      tls_index = TlsAlloc ();
      if (tls_index == TLS_OUT_OF_INDEXES)
        return FALSE; 
      /* falltru.  */
    case DLL_THREAD_ATTACH:
      tls = LocalAlloc (LPTR, sizeof *tls);
      if (!tls)
        return FALSE;
      tls->gt_use_utf8 = 0;
      TlsSetValue (tls_index, tls);
      if (reason == DLL_PROCESS_ATTACH)
        {
          real_init ();
        }
      break;

    case DLL_THREAD_DETACH:
      tls = TlsGetValue (tls_index);
      if (tls)
        LocalFree (tls);
      break;

    case DLL_PROCESS_DETACH:
      tls = TlsGetValue (tls_index);
      if (tls)
        LocalFree (tls);
      TlsFree (tls_index);
      break;

    default:
      break;
    }
  
  return TRUE;
}
#endif /*DLL_EXPORT*/

#else /*!HAVE_W32_SYSTEM*/

void
gpg_err_set_errno (int err)
{
  errno = err;
}

#endif /*!HAVE_W32_SYSTEM*/
