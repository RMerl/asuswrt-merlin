/* Test whether a file has a nontrivial access control list.

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


#if USE_ACL && HAVE_ACL_GET_FILE

# if HAVE_ACL_TYPE_EXTENDED /* MacOS X */

/* ACL is an ACL, from a file, stored as type ACL_TYPE_EXTENDED.
   Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial.  */
int
acl_extended_nontrivial (acl_t acl)
{
  /* acl is non-trivial if it is non-empty.  */
  return (acl_entries (acl) > 0);
}

# else /* Linux, FreeBSD, IRIX, Tru64 */

/* ACL is an ACL, from a file, stored as type ACL_TYPE_ACCESS.
   Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.
   Return -1 and set errno upon failure to determine it.  */
int
acl_access_nontrivial (acl_t acl)
{
  /* acl is non-trivial if it has some entries other than for "user::",
     "group::", and "other::".  Normally these three should be present
     at least, allowing us to write
        return (3 < acl_entries (acl));
     but the following code is more robust.  */
#  if HAVE_ACL_FIRST_ENTRY /* Linux, FreeBSD */

  acl_entry_t ace;
  int got_one;

  for (got_one = acl_get_entry (acl, ACL_FIRST_ENTRY, &ace);
       got_one > 0;
       got_one = acl_get_entry (acl, ACL_NEXT_ENTRY, &ace))
    {
      acl_tag_t tag;
      if (acl_get_tag_type (ace, &tag) < 0)
        return -1;
      if (!(tag == ACL_USER_OBJ || tag == ACL_GROUP_OBJ || tag == ACL_OTHER))
        return 1;
    }
  return got_one;

#  else /* IRIX, Tru64 */
#   if HAVE_ACL_TO_SHORT_TEXT /* IRIX */
  /* Don't use acl_get_entry: it is undocumented.  */

  int count = acl->acl_cnt;
  int i;

  for (i = 0; i < count; i++)
    {
      acl_entry_t ace = &acl->acl_entry[i];
      acl_tag_t tag = ace->ae_tag;

      if (!(tag == ACL_USER_OBJ || tag == ACL_GROUP_OBJ
            || tag == ACL_OTHER_OBJ))
        return 1;
    }
  return 0;

#   endif
#   if HAVE_ACL_FREE_TEXT /* Tru64 */
  /* Don't use acl_get_entry: it takes only one argument and does not work.  */

  int count = acl->acl_num;
  acl_entry_t ace;

  for (ace = acl->acl_first; count > 0; ace = ace->next, count--)
    {
      acl_tag_t tag;
      acl_perm_t perm;

      tag = ace->entry->acl_type;
      if (!(tag == ACL_USER_OBJ || tag == ACL_GROUP_OBJ || tag == ACL_OTHER))
        return 1;

      perm = ace->entry->acl_perm;
      /* On Tru64, perm can also contain non-standard bits such as
         PERM_INSERT, PERM_DELETE, PERM_MODIFY, PERM_LOOKUP, ... */
      if ((perm & ~(ACL_READ | ACL_WRITE | ACL_EXECUTE)) != 0)
        return 1;
    }
  return 0;

#   endif
#  endif
}

# endif


#elif USE_ACL && HAVE_FACL && defined GETACL /* Solaris, Cygwin, not HP-UX */

/* Test an ACL retrieved with GETACL.
   Return 1 if the given ACL, consisting of COUNT entries, is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
acl_nontrivial (int count, aclent_t *entries)
{
  int i;

  for (i = 0; i < count; i++)
    {
      aclent_t *ace = &entries[i];

      /* Note: If ace->a_type = USER_OBJ, ace->a_id is the st_uid from stat().
         If ace->a_type = GROUP_OBJ, ace->a_id is the st_gid from stat().
         We don't need to check ace->a_id in these cases.  */
      if (!(ace->a_type == USER_OBJ
            || ace->a_type == GROUP_OBJ
            || ace->a_type == OTHER_OBJ
            /* Note: Cygwin does not return a CLASS_OBJ ("mask:") entry
               sometimes.  */
            || ace->a_type == CLASS_OBJ))
        return 1;
    }
  return 0;
}

# ifdef ACE_GETACL

/* Test an ACL retrieved with ACE_GETACL.
   Return 1 if the given ACL, consisting of COUNT entries, is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
acl_ace_nontrivial (int count, ace_t *entries)
{
  int i;

  /* The flags in the ace_t structure changed in a binary incompatible way
     when ACL_NO_TRIVIAL etc. were introduced in <sys/acl.h> version 1.15.
     How to distinguish the two conventions at runtime?
     In the old convention, usually three ACEs have a_flags = ACE_OWNER /
     ACE_GROUP / ACE_OTHER, in the range 0x0100..0x0400.  In the new
     convention, these values are not used.  */
  int old_convention = 0;

  for (i = 0; i < count; i++)
    if (entries[i].a_flags & (OLD_ACE_OWNER | OLD_ACE_GROUP | OLD_ACE_OTHER))
      {
        old_convention = 1;
        break;
      }

  if (old_convention)
    /* Running on Solaris 10.  */
    for (i = 0; i < count; i++)
      {
        ace_t *ace = &entries[i];

        /* Note:
           If ace->a_flags = ACE_OWNER, ace->a_who is the st_uid from stat().
           If ace->a_flags = ACE_GROUP, ace->a_who is the st_gid from stat().
           We don't need to check ace->a_who in these cases.  */
        if (!(ace->a_type == OLD_ALLOW
              && (ace->a_flags == OLD_ACE_OWNER
                  || ace->a_flags == OLD_ACE_GROUP
                  || ace->a_flags == OLD_ACE_OTHER)))
          return 1;
      }
  else
    {
      /* Running on Solaris 10 (newer version) or Solaris 11.  */
      unsigned int access_masks[6] =
        {
          0, /* owner@ deny */
          0, /* owner@ allow */
          0, /* group@ deny */
          0, /* group@ allow */
          0, /* everyone@ deny */
          0  /* everyone@ allow */
        };

      for (i = 0; i < count; i++)
        {
          ace_t *ace = &entries[i];
          unsigned int index1;
          unsigned int index2;

          if (ace->a_type == NEW_ACE_ACCESS_ALLOWED_ACE_TYPE)
            index1 = 1;
          else if (ace->a_type == NEW_ACE_ACCESS_DENIED_ACE_TYPE)
            index1 = 0;
          else
            return 1;

          if (ace->a_flags == NEW_ACE_OWNER)
            index2 = 0;
          else if (ace->a_flags == (NEW_ACE_GROUP | NEW_ACE_IDENTIFIER_GROUP))
            index2 = 2;
          else if (ace->a_flags == ACE_EVERYONE)
            index2 = 4;
          else
            return 1;

          access_masks[index1 + index2] |= ace->a_access_mask;
        }

      /* The same bit shouldn't be both allowed and denied.  */
      if (access_masks[0] & access_masks[1])
        return 1;
      if (access_masks[2] & access_masks[3])
        return 1;
      if (access_masks[4] & access_masks[5])
        return 1;

      /* Check minimum masks.  */
      if ((NEW_ACE_WRITE_NAMED_ATTRS
           | NEW_ACE_WRITE_ATTRIBUTES
           | NEW_ACE_WRITE_ACL
           | NEW_ACE_WRITE_OWNER)
          & ~ access_masks[1])
        return 1;
      access_masks[1] &= ~(NEW_ACE_WRITE_NAMED_ATTRS
                           | NEW_ACE_WRITE_ATTRIBUTES
                           | NEW_ACE_WRITE_ACL
                           | NEW_ACE_WRITE_OWNER);
      if ((NEW_ACE_WRITE_NAMED_ATTRS
           | NEW_ACE_WRITE_ATTRIBUTES
           | NEW_ACE_WRITE_ACL
           | NEW_ACE_WRITE_OWNER)
          & ~ access_masks[4])
        return 1;
      access_masks[4] &= ~(NEW_ACE_WRITE_NAMED_ATTRS
                           | NEW_ACE_WRITE_ATTRIBUTES
                           | NEW_ACE_WRITE_ACL
                           | NEW_ACE_WRITE_OWNER);
      if ((NEW_ACE_READ_NAMED_ATTRS
           | NEW_ACE_READ_ATTRIBUTES
           | NEW_ACE_READ_ACL
           | NEW_ACE_SYNCHRONIZE)
          & ~ access_masks[5])
        return 1;
      access_masks[5] &= ~(NEW_ACE_READ_NAMED_ATTRS
                           | NEW_ACE_READ_ATTRIBUTES
                           | NEW_ACE_READ_ACL
                           | NEW_ACE_SYNCHRONIZE);

      /* Check the allowed or denied bits.  */
      if ((access_masks[0] | access_masks[1])
          != (NEW_ACE_READ_DATA
              | NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA
              | NEW_ACE_EXECUTE))
        return 1;
      if ((access_masks[2] | access_masks[3])
          != (NEW_ACE_READ_DATA
              | NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA
              | NEW_ACE_EXECUTE))
        return 1;
      if ((access_masks[4] | access_masks[5])
          != (NEW_ACE_READ_DATA
              | NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA
              | NEW_ACE_EXECUTE))
        return 1;

      /* Check that the NEW_ACE_WRITE_DATA and NEW_ACE_APPEND_DATA bits are
         either both allowed or both denied.  */
      if (((access_masks[0] & NEW_ACE_WRITE_DATA) != 0)
          != ((access_masks[0] & NEW_ACE_APPEND_DATA) != 0))
        return 1;
      if (((access_masks[2] & NEW_ACE_WRITE_DATA) != 0)
          != ((access_masks[2] & NEW_ACE_APPEND_DATA) != 0))
        return 1;
      if (((access_masks[4] & NEW_ACE_WRITE_DATA) != 0)
          != ((access_masks[4] & NEW_ACE_APPEND_DATA) != 0))
        return 1;
    }

  return 0;
}

# endif

#elif USE_ACL && HAVE_GETACL /* HP-UX */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
acl_nontrivial (int count, struct acl_entry *entries, struct stat *sb)
{
  int i;

  for (i = 0; i < count; i++)
    {
      struct acl_entry *ace = &entries[i];

      if (!((ace->uid == sb->st_uid && ace->gid == ACL_NSGROUP)
            || (ace->uid == ACL_NSUSER && ace->gid == sb->st_gid)
            || (ace->uid == ACL_NSUSER && ace->gid == ACL_NSGROUP)))
        return 1;
    }
  return 0;
}

# if HAVE_ACLV_H /* HP-UX >= 11.11 */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
aclv_nontrivial (int count, struct acl *entries)
{
  int i;

  for (i = 0; i < count; i++)
    {
      struct acl *ace = &entries[i];

      /* Note: If ace->a_type = USER_OBJ, ace->a_id is the st_uid from stat().
         If ace->a_type = GROUP_OBJ, ace->a_id is the st_gid from stat().
         We don't need to check ace->a_id in these cases.  */
      if (!(ace->a_type == USER_OBJ /* no need to check ace->a_id here */
            || ace->a_type == GROUP_OBJ /* no need to check ace->a_id here */
            || ace->a_type == CLASS_OBJ
            || ace->a_type == OTHER_OBJ))
        return 1;
    }
  return 0;
}

# endif

#elif USE_ACL && (HAVE_ACLX_GET || HAVE_STATACL) /* AIX */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
acl_nontrivial (struct acl *a)
{
  /* The normal way to iterate through an ACL is like this:
       struct acl_entry *ace;
       for (ace = a->acl_ext; ace != acl_last (a); ace = acl_nxt (ace))
         {
           struct ace_id *aei;
           switch (ace->ace_type)
             {
             case ACC_PERMIT:
             case ACC_DENY:
             case ACC_SPECIFY:
               ...;
             }
           for (aei = ace->ace_id; aei != id_last (ace); aei = id_nxt (aei))
             ...
         }
   */
  return (acl_last (a) != a->acl_ext ? 1 : 0);
}

# if HAVE_ACLX_GET && defined ACL_AIX_WIP /* newer AIX */

/* Return 1 if the given ACL is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
acl_nfs4_nontrivial (nfs4_acl_int_t *a)
{
#  if 1 /* let's try this first */
  return (a->aclEntryN > 0 ? 1 : 0);
#  else
  int count = a->aclEntryN;
  int i;

  for (i = 0; i < count; i++)
    {
      nfs4_ace_int_t *ace = &a->aclEntry[i];

      if (!((ace->flags & ACE4_ID_SPECIAL) != 0
            && (ace->aceWho.special_whoid == ACE4_WHO_OWNER
                || ace->aceWho.special_whoid == ACE4_WHO_GROUP
                || ace->aceWho.special_whoid == ACE4_WHO_EVERYONE)
            && ace->aceType == ACE4_ACCESS_ALLOWED_ACE_TYPE
            && ace->aceFlags == 0
            && (ace->aceMask & ~(ACE4_READ_DATA | ACE4_LIST_DIRECTORY
                                 | ACE4_WRITE_DATA | ACE4_ADD_FILE
                                 | ACE4_EXECUTE)) == 0))
        return 1;
    }
  return 0;
#  endif
}

# endif

#elif USE_ACL && HAVE_ACLSORT /* NonStop Kernel */

/* Test an ACL retrieved with ACL_GET.
   Return 1 if the given ACL, consisting of COUNT entries, is non-trivial.
   Return 0 if it is trivial, i.e. equivalent to a simple stat() mode.  */
int
acl_nontrivial (int count, struct acl *entries)
{
  int i;

  for (i = 0; i < count; i++)
    {
      struct acl *ace = &entries[i];

      /* Note: If ace->a_type = USER_OBJ, ace->a_id is the st_uid from stat().
         If ace->a_type = GROUP_OBJ, ace->a_id is the st_gid from stat().
         We don't need to check ace->a_id in these cases.  */
      if (!(ace->a_type == USER_OBJ /* no need to check ace->a_id here */
            || ace->a_type == GROUP_OBJ /* no need to check ace->a_id here */
            || ace->a_type == CLASS_OBJ
            || ace->a_type == OTHER_OBJ))
        return 1;
    }
  return 0;
}

#endif


/* Return 1 if NAME has a nontrivial access control list, 0 if NAME
   only has no or a base access control list, and -1 (setting errno)
   on error.  SB must be set to the stat buffer of FILE.  */

int
file_has_acl (char const *name, struct stat const *sb)
{
#if USE_ACL
  if (! S_ISLNK (sb->st_mode))
    {
# if HAVE_ACL_GET_FILE

      /* POSIX 1003.1e (draft 17 -- abandoned) specific version.  */
      /* Linux, FreeBSD, MacOS X, IRIX, Tru64 */
      int ret;

      if (HAVE_ACL_EXTENDED_FILE || HAVE_ACL_EXTENDED_FILE_NOFOLLOW) /* Linux */
        {
#  if HAVE_ACL_EXTENDED_FILE_NOFOLLOW
          /* acl_extended_file_nofollow() uses lgetxattr() in order to prevent
             unnecessary mounts, but it returns the same result as we already
             know that NAME is not a symbolic link at this point (modulo the
             TOCTTOU race condition).  */
          ret = acl_extended_file_nofollow (name);
#  else
          /* On Linux, acl_extended_file is an optimized function: It only
             makes two calls to getxattr(), one for ACL_TYPE_ACCESS, one for
             ACL_TYPE_DEFAULT.  */
          ret = acl_extended_file (name);
#  endif
        }
      else /* FreeBSD, MacOS X, IRIX, Tru64 */
        {
#  if HAVE_ACL_TYPE_EXTENDED /* MacOS X */
          /* On MacOS X, acl_get_file (name, ACL_TYPE_ACCESS)
             and acl_get_file (name, ACL_TYPE_DEFAULT)
             always return NULL / EINVAL.  There is no point in making
             these two useless calls.  The real ACL is retrieved through
             acl_get_file (name, ACL_TYPE_EXTENDED).  */
          acl_t acl = acl_get_file (name, ACL_TYPE_EXTENDED);
          if (acl)
            {
              ret = acl_extended_nontrivial (acl);
              acl_free (acl);
            }
          else
            ret = -1;
#  else /* FreeBSD, IRIX, Tru64 */
          acl_t acl = acl_get_file (name, ACL_TYPE_ACCESS);
          if (acl)
            {
              int saved_errno;

              ret = acl_access_nontrivial (acl);
              saved_errno = errno;
              acl_free (acl);
              errno = saved_errno;
#   if HAVE_ACL_FREE_TEXT /* Tru64 */
              /* On OSF/1, acl_get_file (name, ACL_TYPE_DEFAULT) always
                 returns NULL with errno not set.  There is no point in
                 making this call.  */
#   else /* FreeBSD, IRIX */
              /* On Linux, FreeBSD, IRIX, acl_get_file (name, ACL_TYPE_ACCESS)
                 and acl_get_file (name, ACL_TYPE_DEFAULT) on a directory
                 either both succeed or both fail; it depends on the
                 file system.  Therefore there is no point in making the second
                 call if the first one already failed.  */
              if (ret == 0 && S_ISDIR (sb->st_mode))
                {
                  acl = acl_get_file (name, ACL_TYPE_DEFAULT);
                  if (acl)
                    {
                      ret = (0 < acl_entries (acl));
                      acl_free (acl);
                    }
                  else
                    ret = -1;
                }
#   endif
            }
          else
            ret = -1;
#  endif
        }
      if (ret < 0)
        return ACL_NOT_WELL_SUPPORTED (errno) ? 0 : -1;
      return ret;

# elif HAVE_FACL && defined GETACLCNT /* Solaris, Cygwin, not HP-UX */

#  if defined ACL_NO_TRIVIAL

      /* Solaris 10 (newer version), which has additional API declared in
         <sys/acl.h> (acl_t) and implemented in libsec (acl_set, acl_trivial,
         acl_fromtext, ...).  */
      return acl_trivial (name);

#  else /* Solaris, Cygwin, general case */

      /* Solaris 2.5 through Solaris 10, Cygwin, and contemporaneous versions
         of Unixware.  The acl() call returns the access and default ACL both
         at once.  */
      int count;
      {
        aclent_t *entries;

        for (;;)
          {
            count = acl (name, GETACLCNT, 0, NULL);

            if (count < 0)
              {
                if (errno == ENOSYS || errno == ENOTSUP)
                  break;
                else
                  return -1;
              }

            if (count == 0)
              break;

            /* Don't use MIN_ACL_ENTRIES:  It's set to 4 on Cygwin, but Cygwin
               returns only 3 entries for files with no ACL.  But this is safe:
               If there are more than 4 entries, there cannot be only the
               "user::", "group::", "other:", and "mask:" entries.  */
            if (count > 4)
              return 1;

            entries = (aclent_t *) malloc (count * sizeof (aclent_t));
            if (entries == NULL)
              {
                errno = ENOMEM;
                return -1;
              }
            if (acl (name, GETACL, count, entries) == count)
              {
                if (acl_nontrivial (count, entries))
                  {
                    free (entries);
                    return 1;
                  }
                free (entries);
                break;
              }
            /* Huh? The number of ACL entries changed since the last call.
               Repeat.  */
            free (entries);
          }
      }

#   ifdef ACE_GETACL
      /* Solaris also has a different variant of ACLs, used in ZFS and NFSv4
         file systems (whereas the other ones are used in UFS file systems).  */
      {
        ace_t *entries;

        for (;;)
          {
            count = acl (name, ACE_GETACLCNT, 0, NULL);

            if (count < 0)
              {
                if (errno == ENOSYS || errno == EINVAL)
                  break;
                else
                  return -1;
              }

            if (count == 0)
              break;

            /* In the old (original Solaris 10) convention:
               If there are more than 3 entries, there cannot be only the
               ACE_OWNER, ACE_GROUP, ACE_OTHER entries.
               In the newer Solaris 10 and Solaris 11 convention:
               If there are more than 6 entries, there cannot be only the
               ACE_OWNER, ACE_GROUP, ACE_EVERYONE entries, each once with
               NEW_ACE_ACCESS_ALLOWED_ACE_TYPE and once with
               NEW_ACE_ACCESS_DENIED_ACE_TYPE.  */
            if (count > 6)
              return 1;

            entries = (ace_t *) malloc (count * sizeof (ace_t));
            if (entries == NULL)
              {
                errno = ENOMEM;
                return -1;
              }
            if (acl (name, ACE_GETACL, count, entries) == count)
              {
                if (acl_ace_nontrivial (count, entries))
                  {
                    free (entries);
                    return 1;
                  }
                free (entries);
                break;
              }
            /* Huh? The number of ACL entries changed since the last call.
               Repeat.  */
            free (entries);
          }
      }
#   endif

      return 0;
#  endif

# elif HAVE_GETACL /* HP-UX */

      for (;;)
        {
          int count;
          struct acl_entry entries[NACLENTRIES];

          count = getacl (name, 0, NULL);

          if (count < 0)
            {
              /* ENOSYS is seen on newer HP-UX versions.
                 EOPNOTSUPP is typically seen on NFS mounts.
                 ENOTSUP was seen on Quantum StorNext file systems (cvfs).  */
              if (errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP)
                break;
              else
                return -1;
            }

          if (count == 0)
            return 0;

          if (count > NACLENTRIES)
            /* If NACLENTRIES cannot be trusted, use dynamic memory
               allocation.  */
            abort ();

          /* If there are more than 3 entries, there cannot be only the
             (uid,%), (%,gid), (%,%) entries.  */
          if (count > 3)
            return 1;

          if (getacl (name, count, entries) == count)
            {
              struct stat statbuf;

              if (stat (name, &statbuf) < 0)
                return -1;

              return acl_nontrivial (count, entries, &statbuf);
            }
          /* Huh? The number of ACL entries changed since the last call.
             Repeat.  */
        }

#  if HAVE_ACLV_H /* HP-UX >= 11.11 */

      for (;;)
        {
          int count;
          struct acl entries[NACLVENTRIES];

          count = acl ((char *) name, ACL_CNT, NACLVENTRIES, entries);

          if (count < 0)
            {
              /* EOPNOTSUPP is seen on NFS in HP-UX 11.11, 11.23.
                 EINVAL is seen on NFS in HP-UX 11.31.  */
              if (errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
                break;
              else
                return -1;
            }

          if (count == 0)
            return 0;

          if (count > NACLVENTRIES)
            /* If NACLVENTRIES cannot be trusted, use dynamic memory
               allocation.  */
            abort ();

          /* If there are more than 4 entries, there cannot be only the
             four base ACL entries.  */
          if (count > 4)
            return 1;

          if (acl ((char *) name, ACL_GET, count, entries) == count)
            return aclv_nontrivial (count, entries);
          /* Huh? The number of ACL entries changed since the last call.
             Repeat.  */
        }

#  endif

# elif HAVE_ACLX_GET && defined ACL_AIX_WIP /* AIX */

      acl_type_t type;
      char aclbuf[1024];
      void *acl = aclbuf;
      size_t aclsize = sizeof (aclbuf);
      mode_t mode;

      for (;;)
        {
          /* The docs say that type being 0 is equivalent to ACL_ANY, but it
             is not true, in AIX 5.3.  */
          type.u64 = ACL_ANY;
          if (aclx_get (name, 0, &type, aclbuf, &aclsize, &mode) >= 0)
            break;
          if (errno == ENOSYS)
            return 0;
          if (errno != ENOSPC)
            {
              if (acl != aclbuf)
                {
                  int saved_errno = errno;
                  free (acl);
                  errno = saved_errno;
                }
              return -1;
            }
          aclsize = 2 * aclsize;
          if (acl != aclbuf)
            free (acl);
          acl = malloc (aclsize);
          if (acl == NULL)
            {
              errno = ENOMEM;
              return -1;
            }
        }

      if (type.u64 == ACL_AIXC)
        {
          int result = acl_nontrivial ((struct acl *) acl);
          if (acl != aclbuf)
            free (acl);
          return result;
        }
      else if (type.u64 == ACL_NFS4)
        {
          int result = acl_nfs4_nontrivial ((nfs4_acl_int_t *) acl);
          if (acl != aclbuf)
            free (acl);
          return result;
        }
      else
        {
          /* A newer type of ACL has been introduced in the system.
             We should better support it.  */
          if (acl != aclbuf)
            free (acl);
          errno = EINVAL;
          return -1;
        }

# elif HAVE_STATACL /* older AIX */

      union { struct acl a; char room[4096]; } u;

      if (statacl (name, STX_NORMAL, &u.a, sizeof (u)) < 0)
        return -1;

      return acl_nontrivial (&u.a);

# elif HAVE_ACLSORT /* NonStop Kernel */

      int count;
      struct acl entries[NACLENTRIES];

      for (;;)
        {
          count = acl ((char *) name, ACL_CNT, NACLENTRIES, NULL);

          if (count < 0)
            {
              if (errno == ENOSYS || errno == ENOTSUP)
                break;
              else
                return -1;
            }

          if (count == 0)
            return 0;

          if (count > NACLENTRIES)
            /* If NACLENTRIES cannot be trusted, use dynamic memory
               allocation.  */
            abort ();

          /* If there are more than 4 entries, there cannot be only the
             four base ACL entries.  */
          if (count > 4)
            return 1;

          if (acl ((char *) name, ACL_GET, count, entries) == count)
            return acl_nontrivial (count, entries);
          /* Huh? The number of ACL entries changed since the last call.
             Repeat.  */
        }

# endif
    }
#endif

  return 0;
}
