/* Align/Truncate a string in a given screen width
   Copyright (C) 2009-2010 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Pádraig Brady.  */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

#include "c.h"
#include "mbsalign.h"
#include "widechar.h"


#ifdef HAVE_WIDECHAR
/* Replace non printable chars.
   Note \t and \n etc. are non printable.
   Return 1 if replacement made, 0 otherwise.  */

static bool
wc_ensure_printable (wchar_t *wchars)
{
  bool replaced = false;
  wchar_t *wc = wchars;
  while (*wc)
    {
      if (!iswprint ((wint_t) *wc))
        {
          *wc = 0xFFFD; /* L'\uFFFD' (replacement char) */
          replaced = true;
        }
      wc++;
    }
  return replaced;
}

/* Truncate wchar string to width cells.
 * Returns number of cells used.  */

static size_t
wc_truncate (wchar_t *wc, size_t width)
{
  size_t cells = 0;
  int next_cells = 0;

  while (*wc)
    {
      next_cells = wcwidth (*wc);
      if (next_cells == -1) /* non printable */
        {
          *wc = 0xFFFD; /* L'\uFFFD' (replacement char) */
          next_cells = 1;
        }
      if (cells + next_cells > width)
        break;
      cells += next_cells;
      wc++;
    }
  *wc = L'\0';
  return cells;
}

/* FIXME: move this function to gnulib as it's missing on:
   OpenBSD 3.8, IRIX 5.3, Solaris 2.5.1, mingw, BeOS  */

static int
rpl_wcswidth (const wchar_t *s, size_t n)
{
  int ret = 0;

  while (n-- > 0 && *s != L'\0')
    {
      int nwidth = wcwidth (*s++);
      if (nwidth == -1)             /* non printable */
        return -1;
      if (ret > (INT_MAX - nwidth)) /* overflow */
        return -1;
      ret += nwidth;
    }

  return ret;
}
#endif

/* Truncate multi-byte string to @width and returns number of
 * bytes of the new string @str, and in @width returns number
 * of cells.
 */
size_t
mbs_truncate(char *str, size_t *width)
{
	ssize_t bytes = strlen(str);
#ifdef HAVE_WIDECHAR
	ssize_t sz = mbstowcs(NULL, str, 0);
	wchar_t *wcs = NULL;

	if (sz == (ssize_t) -1)
		goto done;

	wcs = malloc((sz + 1) * sizeof(wchar_t));
	if (!wcs)
		goto done;

	if (!mbstowcs(wcs, str, sz))
		goto done;
	*width = wc_truncate(wcs, *width);
	bytes = wcstombs(str, wcs, bytes);
done:
	free(wcs);
#else
	if (*width < bytes)
		bytes = *width;
#endif
	if (bytes >= 0)
		str[bytes] = '\0';
	return bytes;
}

/* Write N_SPACES space characters to DEST while ensuring
   nothing is written beyond DEST_END. A terminating NUL
   is always added to DEST.
   A pointer to the terminating NUL is returned.  */

static char*
mbs_align_pad (char *dest, const char* dest_end, size_t n_spaces)
{
  /* FIXME: Should we pad with "figure space" (\u2007)
     if non ascii data present?  */
  while (n_spaces-- && (dest < dest_end))
    *dest++ = ' ';
  *dest = '\0';
  return dest;
}

/* Align a string, SRC, in a field of *WIDTH columns, handling multi-byte
   characters; write the result into the DEST_SIZE-byte buffer, DEST.
   ALIGNMENT specifies whether to left- or right-justify or to center.
   If SRC requires more than *WIDTH columns, truncate it to fit.
   When centering, the number of trailing spaces may be one less than the
   number of leading spaces. The FLAGS parameter is unused at present.
   Return the length in bytes required for the final result, not counting
   the trailing NUL.  A return value of DEST_SIZE or larger means there
   wasn't enough space.  DEST will be NUL terminated in any case.
   Return (size_t) -1 upon error (invalid multi-byte sequence in SRC,
   or malloc failure), unless MBA_UNIBYTE_FALLBACK is specified.
   Update *WIDTH to indicate how many columns were used before padding.  */

size_t
mbsalign (const char *src, char *dest, size_t dest_size,
          size_t *width, mbs_align_t align, int flags)
{
  size_t ret = -1;
  size_t src_size = strlen (src) + 1;
  char *newstr = NULL;
  wchar_t *str_wc = NULL;
  const char *str_to_print = src;
  size_t n_cols = src_size - 1;
  size_t n_used_bytes = n_cols; /* Not including NUL */
  size_t n_spaces = 0;
  bool conversion = false;
  bool wc_enabled = false;

#ifdef HAVE_WIDECHAR
  /* In multi-byte locales convert to wide characters
     to allow easy truncation. Also determine number
     of screen columns used.  */
  if (MB_CUR_MAX > 1)
    {
      size_t src_chars = mbstowcs (NULL, src, 0);
      if (src_chars == (size_t) -1)
        {
          if (flags & MBA_UNIBYTE_FALLBACK)
            goto mbsalign_unibyte;
          else
            goto mbsalign_cleanup;
        }
      src_chars += 1; /* make space for NUL */
      str_wc = malloc (src_chars * sizeof (wchar_t));
      if (str_wc == NULL)
        {
          if (flags & MBA_UNIBYTE_FALLBACK)
            goto mbsalign_unibyte;
          else
            goto mbsalign_cleanup;
        }
      if (mbstowcs (str_wc, src, src_chars) != 0)
        {
          str_wc[src_chars - 1] = L'\0';
          wc_enabled = true;
          conversion = wc_ensure_printable (str_wc);
          n_cols = rpl_wcswidth (str_wc, src_chars);
        }
    }

  /* If we transformed or need to truncate the source string
     then create a modified copy of it.  */
  if (wc_enabled && (conversion || (n_cols > *width)))
    {
        if (conversion)
          {
             /* May have increased the size by converting
                \t to \uFFFD for example.  */
            src_size = wcstombs(NULL, str_wc, 0) + 1;
          }
        newstr = malloc (src_size);
        if (newstr == NULL)
        {
          if (flags & MBA_UNIBYTE_FALLBACK)
            goto mbsalign_unibyte;
          else
            goto mbsalign_cleanup;
        }
        str_to_print = newstr;
        n_cols = wc_truncate (str_wc, *width);
        n_used_bytes = wcstombs (newstr, str_wc, src_size);
    }
#endif

mbsalign_unibyte:

  if (n_cols > *width) /* Unibyte truncation required.  */
    {
      n_cols = *width;
      n_used_bytes = n_cols;
    }

  if (*width > n_cols) /* Padding required.  */
    n_spaces = *width - n_cols;

  /* indicate to caller how many cells needed (not including padding).  */
  *width = n_cols;

  /* indicate to caller how many bytes needed (not including NUL).  */
  ret = n_used_bytes + (n_spaces * 1);

  /* Write as much NUL terminated output to DEST as possible.  */
  if (dest_size != 0)
    {
      char *dest_end = dest + dest_size - 1;
      size_t start_spaces = n_spaces / 2 + n_spaces % 2;
      size_t end_spaces = n_spaces / 2;

      switch (align)
        {
        case MBS_ALIGN_CENTER:
          start_spaces = n_spaces / 2 + n_spaces % 2;
          end_spaces = n_spaces / 2;
          break;
        case MBS_ALIGN_LEFT:
          start_spaces = 0;
          end_spaces = n_spaces;
          break;
        case MBS_ALIGN_RIGHT:
          start_spaces = n_spaces;
          end_spaces = 0;
          break;
        }

      dest = mbs_align_pad (dest, dest_end, start_spaces);
      size_t space_left = dest_end - dest;
      dest = mempcpy (dest, str_to_print, min (n_used_bytes, space_left));
      mbs_align_pad (dest, dest_end, end_spaces);
    }

mbsalign_cleanup:

  free (str_wc);
  free (newstr);

  return ret;
}
