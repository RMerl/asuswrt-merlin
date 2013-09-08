/* set-mode-acl.c - set access control list equivalent to a mode

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

   Written by Paul Eggert and Andreas Gruenbacher, and Bruno Haible.  */

#include <config.h>

#include "acl.h"

#include "acl-internal.h"

#include "gettext.h"
#define _(msgid) gettext (msgid)


/* If DESC is a valid file descriptor use fchmod to change the
   file's mode to MODE on systems that have fchown. On systems
   that don't have fchown and if DESC is invalid, use chown on
   NAME instead.
   Return 0 if successful.  Return -1 and set errno upon failure.  */

int
chmod_or_fchmod (const char *name, int desc, mode_t mode)
{
  if (HAVE_FCHMOD && desc != -1)
    return fchmod (desc, mode);
  else
    return chmod (name, mode);
}

/* Set the access control lists of a file. If DESC is a valid file
   descriptor, use file descriptor operations where available, else use
   filename based operations on NAME.  If access control lists are not
   available, fchmod the target file to MODE.  Also sets the
   non-permission bits of the destination file (S_ISUID, S_ISGID, S_ISVTX)
   to those from MODE if any are set.
   Return 0 if successful.  Return -1 and set errno upon failure.  */

int
qset_acl (char const *name, int desc, mode_t mode)
{
#if USE_ACL
# if HAVE_ACL_GET_FILE
  /* POSIX 1003.1e draft 17 (abandoned) specific version.  */
  /* Linux, FreeBSD, MacOS X, IRIX, Tru64 */
#  if !HAVE_ACL_TYPE_EXTENDED
  /* Linux, FreeBSD, IRIX, Tru64 */

  /* We must also have acl_from_text and acl_delete_def_file.
     (acl_delete_def_file could be emulated with acl_init followed
      by acl_set_file, but acl_set_file with an empty acl is
      unspecified.)  */

#   ifndef HAVE_ACL_FROM_TEXT
#    error Must have acl_from_text (see POSIX 1003.1e draft 17).
#   endif
#   ifndef HAVE_ACL_DELETE_DEF_FILE
#    error Must have acl_delete_def_file (see POSIX 1003.1e draft 17).
#   endif

  acl_t acl;
  int ret;

  if (HAVE_ACL_FROM_MODE) /* Linux */
    {
      acl = acl_from_mode (mode);
      if (!acl)
        return -1;
    }
  else /* FreeBSD, IRIX, Tru64 */
    {
      /* If we were to create the ACL using the functions acl_init(),
         acl_create_entry(), acl_set_tag_type(), acl_set_qualifier(),
         acl_get_permset(), acl_clear_perm[s](), acl_add_perm(), we
         would need to create a qualifier.  I don't know how to do this.
         So create it using acl_from_text().  */

#   if HAVE_ACL_FREE_TEXT /* Tru64 */
      char acl_text[] = "u::---,g::---,o::---,";
#   else /* FreeBSD, IRIX */
      char acl_text[] = "u::---,g::---,o::---";
#   endif

      if (mode & S_IRUSR) acl_text[ 3] = 'r';
      if (mode & S_IWUSR) acl_text[ 4] = 'w';
      if (mode & S_IXUSR) acl_text[ 5] = 'x';
      if (mode & S_IRGRP) acl_text[10] = 'r';
      if (mode & S_IWGRP) acl_text[11] = 'w';
      if (mode & S_IXGRP) acl_text[12] = 'x';
      if (mode & S_IROTH) acl_text[17] = 'r';
      if (mode & S_IWOTH) acl_text[18] = 'w';
      if (mode & S_IXOTH) acl_text[19] = 'x';

      acl = acl_from_text (acl_text);
      if (!acl)
        return -1;
    }
  if (HAVE_ACL_SET_FD && desc != -1)
    ret = acl_set_fd (desc, acl);
  else
    ret = acl_set_file (name, ACL_TYPE_ACCESS, acl);
  if (ret != 0)
    {
      int saved_errno = errno;
      acl_free (acl);

      if (ACL_NOT_WELL_SUPPORTED (errno))
        return chmod_or_fchmod (name, desc, mode);
      else
        {
          errno = saved_errno;
          return -1;
        }
    }
  else
    acl_free (acl);

  if (S_ISDIR (mode) && acl_delete_def_file (name))
    return -1;

  if (!MODE_INSIDE_ACL || (mode & (S_ISUID | S_ISGID | S_ISVTX)))
    {
      /* We did not call chmod so far, and either the mode and the ACL are
         separate or special bits are to be set which don't fit into ACLs.  */
      return chmod_or_fchmod (name, desc, mode);
    }
  return 0;

#  else /* HAVE_ACL_TYPE_EXTENDED */
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

  /* Remove the ACL if the file has ACLs.  */
  if (HAVE_ACL_GET_FD && desc != -1)
    acl = acl_get_fd (desc);
  else
    acl = acl_get_file (name, ACL_TYPE_EXTENDED);
  if (acl)
    {
      acl_free (acl);

      acl = acl_init (0);
      if (acl)
        {
          if (HAVE_ACL_SET_FD && desc != -1)
            ret = acl_set_fd (desc, acl);
          else
            ret = acl_set_file (name, ACL_TYPE_EXTENDED, acl);
          if (ret != 0)
            {
              int saved_errno = errno;

              acl_free (acl);

              if (ACL_NOT_WELL_SUPPORTED (saved_errno))
                return chmod_or_fchmod (name, desc, mode);
              else
                {
                  errno = saved_errno;
                  return -1;
                }
            }
          acl_free (acl);
        }
    }

  /* Since !MODE_INSIDE_ACL, we have to call chmod explicitly.  */
  return chmod_or_fchmod (name, desc, mode);
#  endif

# elif HAVE_FACL && defined GETACLCNT /* Solaris, Cygwin, not HP-UX */

  int done_setacl = 0;

#  ifdef ACE_GETACL
  /* Solaris also has a different variant of ACLs, used in ZFS and NFSv4
     file systems (whereas the other ones are used in UFS file systems).  */

  /* The flags in the ace_t structure changed in a binary incompatible way
     when ACL_NO_TRIVIAL etc. were introduced in <sys/acl.h> version 1.15.
     How to distinguish the two conventions at runtime?
     We fetch the existing ACL.  In the old convention, usually three ACEs have
     a_flags = ACE_OWNER / ACE_GROUP / ACE_OTHER, in the range 0x0100..0x0400.
     In the new convention, these values are not used.  */
  int convention;

  {
    int count;
    ace_t *entries;

    for (;;)
      {
        if (desc != -1)
          count = facl (desc, ACE_GETACLCNT, 0, NULL);
        else
          count = acl (name, ACE_GETACLCNT, 0, NULL);
        if (count <= 0)
          {
            convention = -1;
            break;
          }
        entries = (ace_t *) malloc (count * sizeof (ace_t));
        if (entries == NULL)
          {
            errno = ENOMEM;
            return -1;
          }
        if ((desc != -1
             ? facl (desc, ACE_GETACL, count, entries)
             : acl (name, ACE_GETACL, count, entries))
            == count)
          {
            int i;

            convention = 0;
            for (i = 0; i < count; i++)
              if (entries[i].a_flags & (OLD_ACE_OWNER | OLD_ACE_GROUP | OLD_ACE_OTHER))
                {
                  convention = 1;
                  break;
                }
            free (entries);
            break;
          }
        /* Huh? The number of ACL entries changed since the last call.
           Repeat.  */
        free (entries);
      }
  }

  if (convention >= 0)
    {
      ace_t entries[6];
      int count;
      int ret;

      if (convention)
        {
          /* Running on Solaris 10.  */
          entries[0].a_type = OLD_ALLOW;
          entries[0].a_flags = OLD_ACE_OWNER;
          entries[0].a_who = 0; /* irrelevant */
          entries[0].a_access_mask = (mode >> 6) & 7;
          entries[1].a_type = OLD_ALLOW;
          entries[1].a_flags = OLD_ACE_GROUP;
          entries[1].a_who = 0; /* irrelevant */
          entries[1].a_access_mask = (mode >> 3) & 7;
          entries[2].a_type = OLD_ALLOW;
          entries[2].a_flags = OLD_ACE_OTHER;
          entries[2].a_who = 0;
          entries[2].a_access_mask = mode & 7;
          count = 3;
        }
      else
        {
          /* Running on Solaris 10 (newer version) or Solaris 11.
             The details here were found through "/bin/ls -lvd somefiles".  */
          entries[0].a_type = NEW_ACE_ACCESS_DENIED_ACE_TYPE;
          entries[0].a_flags = NEW_ACE_OWNER;
          entries[0].a_who = 0; /* irrelevant */
          entries[0].a_access_mask = 0;
          entries[1].a_type = NEW_ACE_ACCESS_ALLOWED_ACE_TYPE;
          entries[1].a_flags = NEW_ACE_OWNER;
          entries[1].a_who = 0; /* irrelevant */
          entries[1].a_access_mask = NEW_ACE_WRITE_NAMED_ATTRS
                                     | NEW_ACE_WRITE_ATTRIBUTES
                                     | NEW_ACE_WRITE_ACL
                                     | NEW_ACE_WRITE_OWNER;
          if (mode & 0400)
            entries[1].a_access_mask |= NEW_ACE_READ_DATA;
          else
            entries[0].a_access_mask |= NEW_ACE_READ_DATA;
          if (mode & 0200)
            entries[1].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          else
            entries[0].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          if (mode & 0100)
            entries[1].a_access_mask |= NEW_ACE_EXECUTE;
          else
            entries[0].a_access_mask |= NEW_ACE_EXECUTE;
          entries[2].a_type = NEW_ACE_ACCESS_DENIED_ACE_TYPE;
          entries[2].a_flags = NEW_ACE_GROUP | NEW_ACE_IDENTIFIER_GROUP;
          entries[2].a_who = 0; /* irrelevant */
          entries[2].a_access_mask = 0;
          entries[3].a_type = NEW_ACE_ACCESS_ALLOWED_ACE_TYPE;
          entries[3].a_flags = NEW_ACE_GROUP | NEW_ACE_IDENTIFIER_GROUP;
          entries[3].a_who = 0; /* irrelevant */
          entries[3].a_access_mask = 0;
          if (mode & 0040)
            entries[3].a_access_mask |= NEW_ACE_READ_DATA;
          else
            entries[2].a_access_mask |= NEW_ACE_READ_DATA;
          if (mode & 0020)
            entries[3].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          else
            entries[2].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          if (mode & 0010)
            entries[3].a_access_mask |= NEW_ACE_EXECUTE;
          else
            entries[2].a_access_mask |= NEW_ACE_EXECUTE;
          entries[4].a_type = NEW_ACE_ACCESS_DENIED_ACE_TYPE;
          entries[4].a_flags = ACE_EVERYONE;
          entries[4].a_who = 0;
          entries[4].a_access_mask = NEW_ACE_WRITE_NAMED_ATTRS
                                     | NEW_ACE_WRITE_ATTRIBUTES
                                     | NEW_ACE_WRITE_ACL
                                     | NEW_ACE_WRITE_OWNER;
          entries[5].a_type = NEW_ACE_ACCESS_ALLOWED_ACE_TYPE;
          entries[5].a_flags = ACE_EVERYONE;
          entries[5].a_who = 0;
          entries[5].a_access_mask = NEW_ACE_READ_NAMED_ATTRS
                                     | NEW_ACE_READ_ATTRIBUTES
                                     | NEW_ACE_READ_ACL
                                     | NEW_ACE_SYNCHRONIZE;
          if (mode & 0004)
            entries[5].a_access_mask |= NEW_ACE_READ_DATA;
          else
            entries[4].a_access_mask |= NEW_ACE_READ_DATA;
          if (mode & 0002)
            entries[5].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          else
            entries[4].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          if (mode & 0001)
            entries[5].a_access_mask |= NEW_ACE_EXECUTE;
          else
            entries[4].a_access_mask |= NEW_ACE_EXECUTE;
          count = 6;
        }
      if (desc != -1)
        ret = facl (desc, ACE_SETACL, count, entries);
      else
        ret = acl (name, ACE_SETACL, count, entries);
      if (ret < 0 && errno != EINVAL && errno != ENOTSUP)
        {
          if (errno == ENOSYS)
            return chmod_or_fchmod (name, desc, mode);
          return -1;
        }
      if (ret == 0)
        done_setacl = 1;
    }
#  endif

  if (!done_setacl)
    {
      aclent_t entries[3];
      int ret;

      entries[0].a_type = USER_OBJ;
      entries[0].a_id = 0; /* irrelevant */
      entries[0].a_perm = (mode >> 6) & 7;
      entries[1].a_type = GROUP_OBJ;
      entries[1].a_id = 0; /* irrelevant */
      entries[1].a_perm = (mode >> 3) & 7;
      entries[2].a_type = OTHER_OBJ;
      entries[2].a_id = 0;
      entries[2].a_perm = mode & 7;

      if (desc != -1)
        ret = facl (desc, SETACL,
                    sizeof (entries) / sizeof (aclent_t), entries);
      else
        ret = acl (name, SETACL,
                   sizeof (entries) / sizeof (aclent_t), entries);
      if (ret < 0)
        {
          if (errno == ENOSYS || errno == EOPNOTSUPP)
            return chmod_or_fchmod (name, desc, mode);
          return -1;
        }
    }

  if (!MODE_INSIDE_ACL || (mode & (S_ISUID | S_ISGID | S_ISVTX)))
    {
      /* We did not call chmod so far, so the special bits have not yet
         been set.  */
      return chmod_or_fchmod (name, desc, mode);
    }
  return 0;

# elif HAVE_GETACL /* HP-UX */

  struct stat statbuf;
  int ret;

  if (desc != -1)
    ret = fstat (desc, &statbuf);
  else
    ret = stat (name, &statbuf);
  if (ret < 0)
    return -1;

  {
    struct acl_entry entries[3];

    entries[0].uid = statbuf.st_uid;
    entries[0].gid = ACL_NSGROUP;
    entries[0].mode = (mode >> 6) & 7;
    entries[1].uid = ACL_NSUSER;
    entries[1].gid = statbuf.st_gid;
    entries[1].mode = (mode >> 3) & 7;
    entries[2].uid = ACL_NSUSER;
    entries[2].gid = ACL_NSGROUP;
    entries[2].mode = mode & 7;

    if (desc != -1)
      ret = fsetacl (desc, sizeof (entries) / sizeof (struct acl_entry), entries);
    else
      ret = setacl (name, sizeof (entries) / sizeof (struct acl_entry), entries);
  }
  if (ret < 0)
    {
      if (!(errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP))
        return -1;

#  if HAVE_ACLV_H /* HP-UX >= 11.11 */
      {
        struct acl entries[4];

        entries[0].a_type = USER_OBJ;
        entries[0].a_id = 0; /* irrelevant */
        entries[0].a_perm = (mode >> 6) & 7;
        entries[1].a_type = GROUP_OBJ;
        entries[1].a_id = 0; /* irrelevant */
        entries[1].a_perm = (mode >> 3) & 7;
        entries[2].a_type = CLASS_OBJ;
        entries[2].a_id = 0;
        entries[2].a_perm = (mode >> 3) & 7;
        entries[3].a_type = OTHER_OBJ;
        entries[3].a_id = 0;
        entries[3].a_perm = mode & 7;

        ret = aclsort (sizeof (entries) / sizeof (struct acl), 1, entries);
        if (ret > 0)
          abort ();
        if (ret < 0)
          {
            if (0)
              return chmod_or_fchmod (name, desc, mode);
            return -1;
          }

        ret = acl ((char *) name, ACL_SET,
                   sizeof (entries) / sizeof (struct acl), entries);
        if (ret < 0)
          {
            if (errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
              return chmod_or_fchmod (name, desc, mode);
            return -1;
          }
      }
#  else
      return chmod_or_fchmod (name, desc, mode);
#  endif
    }

  if (mode & (S_ISUID | S_ISGID | S_ISVTX))
    {
      /* We did not call chmod so far, so the special bits have not yet
         been set.  */
      return chmod_or_fchmod (name, desc, mode);
    }
  return 0;

# elif HAVE_ACLX_GET && defined ACL_AIX_WIP /* AIX */

  acl_type_list_t types;
  size_t types_size = sizeof (types);
  acl_type_t type;

  if (aclx_gettypes (name, &types, &types_size) < 0
      || types.num_entries == 0)
    return chmod_or_fchmod (name, desc, mode);

  /* XXX Do we need to clear all types of ACLs for the given file, or is it
     sufficient to clear the first one?  */
  type = types.entries[0];
  if (type.u64 == ACL_AIXC)
    {
      union { struct acl a; char room[128]; } u;
      int ret;

      u.a.acl_len = (char *) &u.a.acl_ext[0] - (char *) &u.a; /* no entries */
      u.a.acl_mode = mode & ~(S_IXACL | 0777);
      u.a.u_access = (mode >> 6) & 7;
      u.a.g_access = (mode >> 3) & 7;
      u.a.o_access = mode & 7;

      if (desc != -1)
        ret = aclx_fput (desc, SET_ACL | SET_MODE_S_BITS,
                         type, &u.a, u.a.acl_len, mode);
      else
        ret = aclx_put (name, SET_ACL | SET_MODE_S_BITS,
                        type, &u.a, u.a.acl_len, mode);
      if (!(ret < 0 && errno == ENOSYS))
        return ret;
    }
  else if (type.u64 == ACL_NFS4)
    {
      union { nfs4_acl_int_t a; char room[128]; } u;
      nfs4_ace_int_t *ace;
      int ret;

      u.a.aclVersion = NFS4_ACL_INT_STRUCT_VERSION;
      u.a.aclEntryN = 0;
      ace = &u.a.aclEntry[0];
      {
        ace->flags = ACE4_ID_SPECIAL;
        ace->aceWho.special_whoid = ACE4_WHO_OWNER;
        ace->aceType = ACE4_ACCESS_ALLOWED_ACE_TYPE;
        ace->aceFlags = 0;
        ace->aceMask =
          (mode & 0400 ? ACE4_READ_DATA | ACE4_LIST_DIRECTORY : 0)
          | (mode & 0200
             ? ACE4_WRITE_DATA | ACE4_ADD_FILE | ACE4_APPEND_DATA
               | ACE4_ADD_SUBDIRECTORY
             : 0)
          | (mode & 0100 ? ACE4_EXECUTE : 0);
        ace->aceWhoString[0] = '\0';
        ace->entryLen = (char *) &ace->aceWhoString[4] - (char *) ace;
        ace = (nfs4_ace_int_t *) (char *) &ace->aceWhoString[4];
        u.a.aclEntryN++;
      }
      {
        ace->flags = ACE4_ID_SPECIAL;
        ace->aceWho.special_whoid = ACE4_WHO_GROUP;
        ace->aceType = ACE4_ACCESS_ALLOWED_ACE_TYPE;
        ace->aceFlags = 0;
        ace->aceMask =
          (mode & 0040 ? ACE4_READ_DATA | ACE4_LIST_DIRECTORY : 0)
          | (mode & 0020
             ? ACE4_WRITE_DATA | ACE4_ADD_FILE | ACE4_APPEND_DATA
               | ACE4_ADD_SUBDIRECTORY
             : 0)
          | (mode & 0010 ? ACE4_EXECUTE : 0);
        ace->aceWhoString[0] = '\0';
        ace->entryLen = (char *) &ace->aceWhoString[4] - (char *) ace;
        ace = (nfs4_ace_int_t *) (char *) &ace->aceWhoString[4];
        u.a.aclEntryN++;
      }
      {
        ace->flags = ACE4_ID_SPECIAL;
        ace->aceWho.special_whoid = ACE4_WHO_EVERYONE;
        ace->aceType = ACE4_ACCESS_ALLOWED_ACE_TYPE;
        ace->aceFlags = 0;
        ace->aceMask =
          (mode & 0004 ? ACE4_READ_DATA | ACE4_LIST_DIRECTORY : 0)
          | (mode & 0002
             ? ACE4_WRITE_DATA | ACE4_ADD_FILE | ACE4_APPEND_DATA
               | ACE4_ADD_SUBDIRECTORY
             : 0)
          | (mode & 0001 ? ACE4_EXECUTE : 0);
        ace->aceWhoString[0] = '\0';
        ace->entryLen = (char *) &ace->aceWhoString[4] - (char *) ace;
        ace = (nfs4_ace_int_t *) (char *) &ace->aceWhoString[4];
        u.a.aclEntryN++;
      }
      u.a.aclLength = (char *) ace - (char *) &u.a;

      if (desc != -1)
        ret = aclx_fput (desc, SET_ACL | SET_MODE_S_BITS,
                         type, &u.a, u.a.aclLength, mode);
      else
        ret = aclx_put (name, SET_ACL | SET_MODE_S_BITS,
                        type, &u.a, u.a.aclLength, mode);
      if (!(ret < 0 && errno == ENOSYS))
        return ret;
    }

  return chmod_or_fchmod (name, desc, mode);

# elif HAVE_STATACL /* older AIX */

  union { struct acl a; char room[128]; } u;
  int ret;

  u.a.acl_len = (char *) &u.a.acl_ext[0] - (char *) &u.a; /* no entries */
  u.a.acl_mode = mode & ~(S_IXACL | 0777);
  u.a.u_access = (mode >> 6) & 7;
  u.a.g_access = (mode >> 3) & 7;
  u.a.o_access = mode & 7;

  if (desc != -1)
    ret = fchacl (desc, &u.a, u.a.acl_len);
  else
    ret = chacl (name, &u.a, u.a.acl_len);

  if (ret < 0 && errno == ENOSYS)
    return chmod_or_fchmod (name, desc, mode);

  return ret;

# elif HAVE_ACLSORT /* NonStop Kernel */

  struct acl entries[4];
  int ret;

  entries[0].a_type = USER_OBJ;
  entries[0].a_id = 0; /* irrelevant */
  entries[0].a_perm = (mode >> 6) & 7;
  entries[1].a_type = GROUP_OBJ;
  entries[1].a_id = 0; /* irrelevant */
  entries[1].a_perm = (mode >> 3) & 7;
  entries[2].a_type = CLASS_OBJ;
  entries[2].a_id = 0;
  entries[2].a_perm = (mode >> 3) & 7;
  entries[3].a_type = OTHER_OBJ;
  entries[3].a_id = 0;
  entries[3].a_perm = mode & 7;

  ret = aclsort (sizeof (entries) / sizeof (struct acl), 1, entries);
  if (ret > 0)
    abort ();
  if (ret < 0)
    {
      if (0)
        return chmod_or_fchmod (name, desc, mode);
      return -1;
    }

  ret = acl ((char *) name, ACL_SET,
             sizeof (entries) / sizeof (struct acl), entries);
  if (ret < 0)
    {
      if (0)
        return chmod_or_fchmod (name, desc, mode);
      return -1;
    }

  if (mode & (S_ISUID | S_ISGID | S_ISVTX))
    {
      /* We did not call chmod so far, so the special bits have not yet
         been set.  */
      return chmod_or_fchmod (name, desc, mode);
    }
  return 0;

# else /* Unknown flavor of ACLs */
  return chmod_or_fchmod (name, desc, mode);
# endif
#else /* !USE_ACL */
  return chmod_or_fchmod (name, desc, mode);
#endif
}

/* As with qset_acl, but also output a diagnostic on failure.  */

int
set_acl (char const *name, int desc, mode_t mode)
{
  int r = qset_acl (name, desc, mode);
  if (r != 0)
    error (0, errno, _("setting permissions for %s"), quote (name));
  return r;
}
