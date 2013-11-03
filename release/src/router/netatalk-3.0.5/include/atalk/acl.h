/*
   Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef ATALK_ACL_H
#define ATALK_ACL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ACLS

#define O_NETATALK_ACL (O_NOFOLLOW << 1)

#ifdef HAVE_SOLARIS_ACLS
#include <sys/acl.h>

#define chmod_acl nfsv4_chmod

extern int get_nfsv4_acl(const char *name, ace_t **retAces);
extern int strip_trivial_aces(ace_t **saces, int sacecount);
extern int strip_nontrivial_aces(ace_t **saces, int sacecount);
extern ace_t *concat_aces(ace_t *aces1, int ace1count, ace_t *aces2, int ace2count);
extern int nfsv4_chmod(char *name, mode_t mode);

#endif  /* HAVE_SOLARIS_ACLS */

#ifdef HAVE_POSIX_ACLS
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/acl.h>

#define chmod_acl posix_chmod
#define fchmod_acl posix_fchmod

extern int posix_chmod(const char *name, mode_t mode);
extern int posix_fchmod(int fd, mode_t mode);

#endif /* HAVE_POSIX_ACLS */

extern int remove_acl_vfs(const char *name);

#else /* HAVE_ACLS=no */

#define O_NETATALK_ACL 0
#define chmod_acl chmod

#endif /* HAVE_ACLS */

#endif /* ATALK_ACL_H */
