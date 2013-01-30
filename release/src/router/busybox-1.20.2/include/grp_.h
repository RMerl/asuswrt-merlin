/* vi: set sw=4 ts=4: */
/* Copyright (C) 1991,92,95,96,97,98,99,2000,01 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */
/*
 * POSIX Standard: 9.2.1 Group Database Access	<grp.h>
 */
#ifndef BB_GRP_H
#define BB_GRP_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

/* This file is #included after #include <grp.h>
 * We will use libc-defined structures, but will #define function names
 * so that function calls are directed to bb_internal_XXX replacements
 */
#undef endgrent
#define setgrent     bb_internal_setgrent
#define endgrent     bb_internal_endgrent
#define getgrent     bb_internal_getgrent
#define fgetgrent    bb_internal_fgetgrent
#define putgrent     bb_internal_putgrent
#define getgrgid     bb_internal_getgrgid
#define getgrnam     bb_internal_getgrnam
#define getgrent_r   bb_internal_getgrent_r
#define getgrgid_r   bb_internal_getgrgid_r
#define getgrnam_r   bb_internal_getgrnam_r
#define fgetgrent_r  bb_internal_fgetgrent_r
#define getgrouplist bb_internal_getgrouplist
#define initgroups   bb_internal_initgroups


/* All function names below should be remapped by #defines above
 * in order to not collide with libc names. */


/* Rewind the group-file stream.  */
extern void setgrent(void);

/* Close the group-file stream.  */
extern void endgrent(void);

#ifdef UNUSED_SINCE_WE_AVOID_STATIC_BUFS
/* Read an entry from the group-file stream, opening it if necessary.  */
extern struct group *getgrent(void);

/* Read a group entry from STREAM.  */
extern struct group *fgetgrent(FILE *__stream);

/* Write the given entry onto the given stream.  */
extern int putgrent(const struct group *__restrict __p,
		     FILE *__restrict __f);
#endif

/* Search for an entry with a matching group ID.  */
extern struct group *getgrgid(gid_t __gid);

/* Search for an entry with a matching group name.  */
extern struct group *getgrnam(const char *__name);

/* Reentrant versions of some of the functions above.

   PLEASE NOTE: the `getgrent_r' function is not (yet) standardized.
   The interface may change in later versions of this library.  But
   the interface is designed following the principals used for the
   other reentrant functions so the chances are good this is what the
   POSIX people would choose.  */

extern int getgrent_r(struct group *__restrict __resultbuf,
		       char *__restrict __buffer, size_t __buflen,
		       struct group **__restrict __result);

/* Search for an entry with a matching group ID.  */
extern int getgrgid_r(gid_t __gid, struct group *__restrict __resultbuf,
		       char *__restrict __buffer, size_t __buflen,
		       struct group **__restrict __result);

/* Search for an entry with a matching group name.  */
extern int getgrnam_r(const char *__restrict __name,
		       struct group *__restrict __resultbuf,
		       char *__restrict __buffer, size_t __buflen,
		       struct group **__restrict __result);

/* Read a group entry from STREAM.  This function is not standardized
   an probably never will.  */
extern int fgetgrent_r(FILE *__restrict __stream,
			struct group *__restrict __resultbuf,
			char *__restrict __buffer, size_t __buflen,
			struct group **__restrict __result);

/* Store at most *NGROUPS members of the group set for USER into
   *GROUPS.  Also include GROUP.  The actual number of groups found is
   returned in *NGROUPS.  Return -1 if the if *NGROUPS is too small.  */
extern int getgrouplist(const char *__user, gid_t __group,
			 gid_t *__groups, int *__ngroups);

/* Initialize the group set for the current user
   by reading the group database and using all groups
   of which USER is a member.  Also include GROUP.  */
extern int initgroups(const char *__user, gid_t __group);

POP_SAVED_FUNCTION_VISIBILITY

#endif
