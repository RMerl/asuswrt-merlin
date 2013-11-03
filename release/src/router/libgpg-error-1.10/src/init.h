/* init.h - Declarations for init.c
   Copyright (C) 2010 g10 Code GmbH

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

#ifndef INIT_H
#define INIT_H

#if HAVE_W32_SYSTEM

/* Forward declaration - defined in w32-gettext.c.  */
struct loaded_domain;

/* An item for a linked list of loaded domains. */
struct domainlist_s
{
  struct domainlist_s *next;
  char *dname;                   /* Directory name for the mo file.   */
  char *fname;                   /* File name for the MO file.  */
  int load_failed;               /* True if loading the domain failed. */
  struct loaded_domain *domain;  /* NULL if not loaded.  Never changed
                                    once set to non-NULL. */
  char name[1];                  /* Name of the domain.  Never changed
                                    once set. */
};



/* 119 bytes for an error message should be enough.  With this size we
   can assume that the allocation does not take up more than 128 bytes
   per thread.  Note that this is only used for W32CE.  */
#define STRBUFFER_SIZE 120

/* The TLS space definition. */
struct tls_space_s
{
  /* Flag used by w32-gettext.  */
  int gt_use_utf8;
  
#ifdef HAVE_W32CE_SYSTEM
  char strerror_buffer[STRBUFFER_SIZE];
#endif
};

/* Return the TLS.  */
struct tls_space_s *get_tls (void);


/* Explicit constructor for w32-gettext.c  */
#ifndef DLL_EXPORT
void _gpg_w32__init_gettext_module (void);
#endif

#endif /*HAVE_W32_SYSTEM*/

#endif /*INIT_H*/
