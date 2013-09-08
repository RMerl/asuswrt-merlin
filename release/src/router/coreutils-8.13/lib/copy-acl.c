/* copy-acl.c - copy access control list from one file to another file

   Copyright (C) 2002-2003, 2005-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Written by Paul Eggert, Andreas Grünbacher, and Bruno Haible.  */

#include <config.h>

#include "acl.h"

#include "acl-internal.h"

#include "gettext.h"
#define _(msgid) gettext (msgid)


/* Copy access control lists from one file to another. If SOURCE_DESC is
   a valid file descriptor, use file descriptor operations, else use
   filename based operations on SRC_NAME. Likewise for DEST_DESC and
   DST_NAME.
   If access control lists are not available, fchmod the target file to
   MODE.  Also sets the non-permission bits of the destination file
   (S_ISUID, S_ISGID, S_ISVTX) to those from MODE if any are set.
   Return 0 if successful.
   Return -2 and set errno for an error relating to the source file.
   Return -1 and set errno for an error relating to the destination file.  */

static int
qcopy_acl (const char *src_name, int source_desc, const char *dst_name,
           int dest_desc, mode_t mode)
{
#if USE_ACL && HAVE_ACL_GET_FILE
  /* POSIX 1003.1e (draft 17 -- abandoned) specific version.  */
  /* Linux, FreeBSD, MacOS X, IRIX, Tru64 */
# if !HAVE_ACL_TYPE_EXTENDED
  /* Linux, FreeBSD, IRIX, Tru64 */

  acl_t acl;
  int ret;

  if (HAVE_ACL_GET_FD && source_desc != -1)
    acl = acl_get_fd (source_desc);
  else
    acl = acl_get_file (src_name, ACL_TYPE_ACCESS);
  if (acl == NULL)
    {
      if (ACL_NOT_WELL_SUPPORTED (errno))
        return qset_acl (dst_name, dest_desc, mode);
      else
        return -2;
    }

  if (HAVE_ACL_SET_FD && dest_desc != -1)
    ret = acl_set_fd (dest_desc, acl);
  else
    ret = acl_set_file (dst_name, ACL_TYPE_ACCESS, acl);
  if (ret != 0)
    {
      int saved_errno = errno;

      if (ACL_NOT_WELL_SUPPORTED (errno) && !acl_access_nontrivial (acl))
        {
          acl_free (acl);
          return chmod_or_fchmod (dst_name, dest_desc, mode);
        }
      else
        {
          acl_free (acl);
          chmod_or_fchmod (dst_name, dest_desc, mode);
          errno = saved_errno;
          return -1;
        }
    }
  else
    acl_free (acl);

  if (!MODE_INSIDE_ACL || (mode & (S_ISUID | S_ISGID | S_ISVTX)))
    {
      /* We did not call chmod so far, and either the mode and the ACL are
         separate or special bits are to be set which don't fit into ACLs.  */

      if (chmod_or_fchmod (dst_name, dest_desc, mode) != 0)
        return -1;
    }

  if (S_ISDIR (mode))
    {
      acl = acl_get_file (src_name, ACL_TYPE_DEFAULT);
      if (acl == NULL)
        return -2;

      if (acl_set_file (dst_name, ACL_TYPE_DEFAULT, acl))
        {
          int saved_errno = errno;

          acl_free (acl);
          errno = saved_errno;
          return -1;
        }
      else
        acl_free (acl);
    }
  return 0;

# else /* HAVE_ACL_TYPE_EXTENDED */
  /* MacOS X */

  /* On MacOS X,  acl_get_file (name, ACL_TYPE_ACCESS)
     and          acl_get_file (name, ACL_TYPE_DEFAULT)
     always return NULL / EINVAL.  You have to use
                  acl_get_file (name, ACL_TYPE_EXTENDED)
     or           acl_get_fd (open (name, ...))
     to retrieve an ACL.
     On the other hand,
                  acl_set_file (name, ACL_TYPE_ACCESS, acl)
     and          acl_set_file (name, ACL_TYPE_DEFAULT, acl)
     have the same effect as
                  acl_set_file (name, ACL_TYPE_EXTENDED, acl):
     Each of these calls sets the file's ACL.  */

  acl_t acl;
  int ret;

  if (HAVE_ACL_GET_FD && source_desc != -1)
    acl = acl_get_fd (source_desc);
  else
    acl = acl_get_file (src_name, ACL_TYPE_EXTENDED);
  if (acl == NULL)
    {
      if (ACL_NOT_WELL_SUPPORTED (errno))
        return qset_acl (dst_name, dest_desc, mode);
      else
        return -2;
    }

  if (HAVE_ACL_SET_FD && dest_desc != -1)
    ret = acl_set_fd (dest_desc, acl);
  else
    ret = acl_set_file (dst_name, ACL_TYPE_EXTENDED, acl);
  if (ret != 0)
    {
      int saved_errno = errno;

      if (ACL_NOT_WELL_SUPPORTED (errno) && !acl_extended_nontrivial (acl))
        {
          acl_free (acl);
          return chmod_or_fchmod (dst_name, dest_desc, mode);
        }
      else
        {
          acl_free (acl);
          chmod_or_fchmod (dst_name, dest_desc, mode);
          errno = saved_errno;
          return -1;
        }
    }
  else
    acl_free (acl);

  /* Since !MODE_INSIDE_ACL, we have to call chmod explicitly.  */
  return chmod_or_fchmod (dst_name, dest_desc, mode);

# endif

#elif USE_ACL && defined GETACL /* Solaris, Cygwin, not HP-UX */

  /* Solaris 2.5 through Solaris 10, Cygwin, and contemporaneous versions
     of Unixware.  The acl() call returns the access and default ACL both
     at once.  */
# ifdef ACE_GETACL
  int ace_count;
  ace_t *ace_entries;
# endif
  int count;
  aclent_t *entries;
  int did_chmod;
  int saved_errno;
  int ret;

# ifdef ACE_GETACL
  /* Solaris also has a different variant of ACLs, used in ZFS and NFSv4
     file systems (whereas the other ones are used in UFS file systems).
     There is an API
       pathconf (name, _PC_ACL_ENABLED)
       fpathconf (desc, _PC_ACL_ENABLED)
     that allows to determine which of the two kinds of ACLs is supported
     for the given file.  But some file systems may implement this call
     incorrectly, so better not use it.
     When fetching the source ACL, we simply fetch both ACL types.
     When setting the destination ACL, we try either ACL types, assuming
     that the kernel will translate the ACL from one form to the other.
     (See in <http://docs.sun.com/app/docs/doc/819-2241/6n4huc7ia?l=en&a=view>
     the description of ENOTSUP.)  */
  for (;;)
    {
      ace_count = (source_desc != -1
                   ? facl (source_desc, ACE_GETACLCNT, 0, NULL)
                   : acl (src_name, ACE_GETACLCNT, 0, NULL));

      if (ace_count < 0)
        {
          if (errno == ENOSYS || errno == EINVAL)
            {
              ace_count = 0;
              ace_entries = NULL;
              break;
            }
          else
            return -2;
        }

      if (ace_count == 0)
        {
          ace_entries = NULL;
          break;
        }

      ace_entries = (ace_t *) malloc (ace_count * sizeof (ace_t));
      if (ace_entries == NULL)
        {
          errno = ENOMEM;
          return -2;
        }

      if ((source_desc != -1
           ? facl (source_desc, ACE_GETACL, ace_count, ace_entries)
           : acl (src_name, ACE_GETACL, ace_count, ace_entries))
          == ace_count)
        break;
      /* Huh? The number of ACL entries changed since the last call.
         Repeat.  */
    }
# endif

  for (;;)
    {
      count = (source_desc != -1
               ? facl (source_desc, GETACLCNT, 0, NULL)
               : acl (src_name, GETACLCNT, 0, NULL));

      if (count < 0)
        {
          if (errno == ENOSYS || errno == ENOTSUP || errno == EOPNOTSUPP)
            {
              count = 0;
              entries = NULL;
              break;
            }
          else
            return -2;
        }

      if (count == 0)
        {
          entries = NULL;
          break;
        }

      entries = (aclent_t *) malloc (count * sizeof (aclent_t));
      if (entries == NULL)
        {
          errno = ENOMEM;
          return -2;
        }

      if ((source_desc != -1
           ? facl (source_desc, GETACL, count, entries)
           : acl (src_name, GETACL, count, entries))
          == count)
        break;
      /* Huh? The number of ACL entries changed since the last call.
         Repeat.  */
    }

  /* Is there an ACL of either kind?  */
# ifdef ACE_GETACL
  if (ace_count == 0)
# endif
    if (count == 0)
      return qset_acl (dst_name, dest_desc, mode);

  did_chmod = 0; /* set to 1 once the mode bits in 0777 have been set */
  saved_errno = 0; /* the first non-ignorable error code */

  if (!MODE_INSIDE_ACL)
    {
      /* On Cygwin, it is necessary to call chmod before acl, because
         chmod can change the contents of the ACL (in ways that don't
         change the allowed accesses, but still visible).  */
      if (chmod_or_fchmod (dst_name, dest_desc, mode) != 0)
        saved_errno = errno;
      did_chmod = 1;
    }

  /* If both ace_entries and entries are available, try SETACL before
     ACE_SETACL, because SETACL cannot fail with ENOTSUP whereas ACE_SETACL
     can.  */

  if (count > 0)
    {
      ret = (dest_desc != -1
             ? facl (dest_desc, SETACL, count, entries)
             : acl (dst_name, SETACL, count, entries));
      if (ret < 0 && saved_errno == 0)
        {
          saved_errno = errno;
          if ((errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
              && !acl_nontrivial (count, entries))
            saved_errno = 0;
        }
      else
        did_chmod = 1;
    }
  free (entries);

# ifdef ACE_GETACL
  if (ace_count > 0)
    {
      ret = (dest_desc != -1
             ? facl (dest_desc, ACE_SETACL, ace_count, ace_entries)
             : acl (dst_name, ACE_SETACL, ace_count, ace_entries));
      if (ret < 0 && saved_errno == 0)
        {
          saved_errno = errno;
          if ((errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)
              && !acl_ace_nontrivial (ace_count, ace_entries))
            saved_errno = 0;
        }
    }
  free (ace_entries);
# endif

  if (MODE_INSIDE_ACL
      && did_chmod <= ((mode & (S_ISUID | S_ISGID | S_ISVTX)) ? 1 : 0))
    {
      /* We did not call chmod so far, and either the mode and the ACL are
         separate or special bits are to be set which don't fit into ACLs.  */

      if (chmod_or_fchmod (dst_name, dest_desc, mode) != 0)
        {
          if (saved_errno == 0)
            saved_errno = errno;
        }
    }

  if (saved_errno)
    {
      errno = saved_errno;
      return -1;
    }
  return 0;

#elif USE_ACL && HAVE_GETACL /* HP-UX */

  int count;
  struct acl_entry entries[NACLENTRIES];
# if HAVE_ACLV_H
  int aclv_count;
  struct acl aclv_entries[NACLVENTRIES];
# endif
  int did_chmod;
  int saved_errno;
  int ret;

  for (;;)
    {
      count = (source_desc != -1
               ? fgetacl (source_desc, 0, NULL)
               : getacl (src_name, 0, NULL));

      if (count < 0)
        {
          if (errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP)
            {
              count = 0;
              break;
            }
          else
            return -2;
        }

      if (count == 0)
        break;

      if (count > NACLENTRIES)
        /* If NACLENTRIES cannot be trusted, use dynamic memory allocation.  */
        abort ();

      if ((source_desc != -1
           ? fgetacl (source_desc, count, entries)
           : getacl (src_name, count, entries))
          == count)
        break;
      /* Huh? The number of ACL entries changed since the last call.
         Repeat.  */
    }

# if HAVE_ACLV_H
  for (;;)
    {
      aclv_count = acl ((char *) src_name, ACL_CNT, NACLVENTRIES, aclv_entries);

      if (aclv_count < 0)
        {
          if (errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
            {
              count = 0;
              break;
            }
          else
            return -2;
        }

      if (aclv_count == 0)
        break;

      if (aclv_count > NACLVENTRIES)
        /* If NACLVENTRIES cannot be trusted, use dynamic memory allocation.  */
        abort ();

      if (acl ((char *) src_name, ACL_GET, aclv_count, aclv_entries)
          == aclv_count)
        break;
      /* Huh? The number of ACL entries changed since the last call.
         Repeat.  */
    }
# endif

  if (count == 0)
# if HAVE_ACLV_H
    if (aclv_count == 0)
# endif
      return qset_acl (dst_name, dest_desc, mode);

  did_chmod = 0; /* set to 1 once the mode bits in 0777 have been set */
  saved_errno = 0; /* the first non-ignorable error code */

  if (count > 0)
    {
      ret = (dest_desc != -1
             ? fsetacl (dest_desc, count, entries)
             : setacl (dst_name, count, entries));
      if (ret < 0 && saved_errno == 0)
        {
          saved_errno = errno;
          if (errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP)
            {
              struct stat source_statbuf;

              if ((source_desc != -1
                   ? fstat (source_desc, &source_statbuf)
                   : stat (src_name, &source_statbuf)) == 0)
                {
                  if (!acl_nontrivial (count, entries, &source_statbuf))
                    saved_errno = 0;
                }
              else
                saved_errno = errno;
            }
        }
      else
        did_chmod = 1;
    }

# if HAVE_ACLV_H
  if (aclv_count > 0)
    {
      ret = acl ((char *) dst_name, ACL_SET, aclv_count, aclv_entries);
      if (ret < 0 && saved_errno == 0)
        {
          saved_errno = errno;
          if (errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
            {
              if (!aclv_nontrivial (aclv_count, aclv_entries))
                saved_errno = 0;
            }
        }
      else
        did_chmod = 1;
    }
# endif

  if (did_chmod <= ((mode & (S_ISUID | S_ISGID | S_ISVTX)) ? 1 : 0))
    {
      /* We did not call chmod so far, and special bits are to be set which
         don't fit into ACLs.  */

      if (chmod_or_fchmod (dst_name, dest_desc, mode) != 0)
        {
          if (saved_errno == 0)
            saved_errno = errno;
        }
    }

  if (saved_errno)
    {
      errno = saved_errno;
      return -1;
    }
  return 0;

#elif USE_ACL && HAVE_ACLX_GET && 0 /* AIX */

  /* TODO */

#elif USE_ACL && HAVE_STATACL /* older AIX */

  union { struct acl a; char room[4096]; } u;
  int ret;

  if ((source_desc != -1
       ? fstatacl (source_desc, STX_NORMAL, &u.a, sizeof (u))
       : statacl (src_name, STX_NORMAL, &u.a, sizeof (u)))
      < 0)
    return -2;

  ret = (dest_desc != -1
         ? fchacl (dest_desc, &u.a, u.a.acl_len)
         : chacl (dst_name, &u.a, u.a.acl_len));
  if (ret < 0)
    {
      int saved_errno = errno;

      chmod_or_fchmod (dst_name, dest_desc, mode);
      errno = saved_errno;
      return -1;
    }

  /* No need to call chmod_or_fchmod at this point, since the mode bits
     S_ISUID, S_ISGID, S_ISVTX are also stored in the ACL.  */

  return 0;

#elif USE_ACL && HAVE_ACLSORT /* NonStop Kernel */

  int count;
  struct acl entries[NACLENTRIES];
  int ret;

  for (;;)
    {
      count = acl ((char *) src_name, ACL_CNT, NACLENTRIES, NULL);

      if (count < 0)
        {
          if (0)
            {
              count = 0;
              break;
            }
          else
            return -2;
        }

      if (count == 0)
        break;

      if (count > NACLENTRIES)
        /* If NACLENTRIES cannot be trusted, use dynamic memory allocation.  */
        abort ();

      if (acl ((char *) src_name, ACL_GET, count, entries) == count)
        break;
      /* Huh? The number of ACL entries changed since the last call.
         Repeat.  */
    }

  if (count == 0)
    return qset_acl (dst_name, dest_desc, mode);

  ret = acl ((char *) dst_name, ACL_SET, count, entries);
  if (ret < 0)
    {
      int saved_errno = errno;

      if (0)
        {
          if (!acl_nontrivial (count, entries))
            return chmod_or_fchmod (dst_name, dest_desc, mode);
        }

      chmod_or_fchmod (dst_name, dest_desc, mode);
      errno = saved_errno;
      return -1;
    }

  if (mode & (S_ISUID | S_ISGID | S_ISVTX))
    {
      /* We did not call chmod so far, and either the mode and the ACL are
         separate or special bits are to be set which don't fit into ACLs.  */

      return chmod_or_fchmod (dst_name, dest_desc, mode);
    }
  return 0;

#else

  return qset_acl (dst_name, dest_desc, mode);

#endif
}


/* Copy access control lists from one file to another. If SOURCE_DESC is
   a valid file descriptor, use file descriptor operations, else use
   filename based operations on SRC_NAME. Likewise for DEST_DESC and
   DST_NAME.
   If access control lists are not available, fchmod the target file to
   MODE.  Also sets the non-permission bits of the destination file
   (S_ISUID, S_ISGID, S_ISVTX) to those from MODE if any are set.
   Return 0 if successful, otherwise output a diagnostic and return -1.  */

int
copy_acl (const char *src_name, int source_desc, const char *dst_name,
          int dest_desc, mode_t mode)
{
  int ret = qcopy_acl (src_name, source_desc, dst_name, dest_desc, mode);
  switch (ret)
    {
    case -2:
      error (0, errno, "%s", quote (src_name));
      return -1;

    case -1:
      error (0, errno, _("preserving permissions for %s"), quote (dst_name));
      return -1;

    default:
      return 0;
    }
}
