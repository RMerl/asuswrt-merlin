/* xattr.h -- POSIX Extended Attribute support.

   Copyright (C) 2016 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

#include "wget.h"

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "xattr.h"

#ifdef USE_XATTR

static int
write_xattr_metadata (const char *name, const char *value, FILE *fp)
{
  int retval = -1;

  if (name && value && fp)
    {
      retval = fsetxattr (fileno (fp), name, value, strlen (value), 0);
      /* FreeBSD's extattr_set_fd returns the length of the extended attribute. */
      retval = (retval < 0) ? retval : 0;
      if (retval)
        DEBUGP (("Failed to set xattr %s.\n", quote(name)));
    }

  return retval;
}

#else /* USE_XATTR */

static int
write_xattr_metadata (const char *name, const char *value, FILE *fp)
{
  (void)name;
  (void)value;
  (void)fp;

  return 0;
}

#endif /* USE_XATTR */

int
set_file_metadata (const char *origin_url, const char *referrer_url, FILE *fp)
{
  /* Save metadata about where the file came from (requested, final URLs) to
   * user POSIX Extended Attributes of retrieved file.
   *
   * For more details about the user namespace see
   * [http://freedesktop.org/wiki/CommonExtendedAttributes] and
   * [http://0pointer.de/lennart/projects/mod_mime_xattr/].
   */
  int retval = -1;

  if (!origin_url || !fp)
    return retval;

  retval = write_xattr_metadata ("user.xdg.origin.url", escnonprint_uri (origin_url), fp);
  if ((!retval) && referrer_url)
    retval = write_xattr_metadata ("user.xdg.referrer.url", escnonprint_uri (referrer_url), fp);

  return retval;
}
