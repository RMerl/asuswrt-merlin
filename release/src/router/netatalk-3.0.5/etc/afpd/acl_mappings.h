/*
   Copyright (c) 2008,2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#ifndef ACL_MAPPINGS
#define ACL_MAPPINGS

#ifdef HAVE_SOLARIS_ACLS
#include <sys/acl.h>
#endif

#include "acls.h"

/* 
 * Stuff for mapping between ACL implementations
 */

/* Solaris 10u8 still hasn't got ACE_INHERITED_ACE */
#ifndef ACE_INHERITED_ACE
#define ACE_INHERITED_ACE 0x0080
#endif

struct ace_rights_map {
    uint32_t from;
    uint32_t to;
};

#ifdef HAVE_SOLARIS_ACLS
struct ace_rights_map nfsv4_to_darwin_rights[] = {
    {ACE_READ_DATA,         DARWIN_ACE_READ_DATA},
    {ACE_WRITE_DATA,        DARWIN_ACE_WRITE_DATA},
    {ACE_APPEND_DATA,       DARWIN_ACE_APPEND_DATA},
    {ACE_READ_NAMED_ATTRS,  DARWIN_ACE_READ_EXTATTRIBUTES},
    {ACE_WRITE_NAMED_ATTRS, DARWIN_ACE_WRITE_EXTATTRIBUTES},
    {ACE_EXECUTE,           DARWIN_ACE_EXECUTE},
    {ACE_DELETE_CHILD,      DARWIN_ACE_DELETE_CHILD},
    {ACE_READ_ATTRIBUTES,   DARWIN_ACE_READ_ATTRIBUTES},
    {ACE_WRITE_ATTRIBUTES,  DARWIN_ACE_WRITE_ATTRIBUTES},
    {ACE_DELETE,            DARWIN_ACE_DELETE},
    {ACE_READ_ACL,          DARWIN_ACE_READ_SECURITY},
    {ACE_WRITE_ACL,         DARWIN_ACE_WRITE_SECURITY},
    {ACE_WRITE_OWNER,       DARWIN_ACE_TAKE_OWNERSHIP},
    {0,0}
};

struct ace_rights_map darwin_to_nfsv4_rights[] = {
    {DARWIN_ACE_READ_DATA,           ACE_READ_DATA},
    {DARWIN_ACE_WRITE_DATA,          ACE_WRITE_DATA},
    {DARWIN_ACE_APPEND_DATA,         ACE_APPEND_DATA},
    {DARWIN_ACE_READ_EXTATTRIBUTES,  ACE_READ_NAMED_ATTRS},
    {DARWIN_ACE_WRITE_EXTATTRIBUTES, ACE_WRITE_NAMED_ATTRS},
    {DARWIN_ACE_EXECUTE,             ACE_EXECUTE},
    {DARWIN_ACE_DELETE_CHILD,        ACE_DELETE_CHILD},
    {DARWIN_ACE_READ_ATTRIBUTES,     ACE_READ_ATTRIBUTES},
    {DARWIN_ACE_WRITE_ATTRIBUTES,    ACE_WRITE_ATTRIBUTES},
    {DARWIN_ACE_DELETE,              ACE_DELETE},
    {DARWIN_ACE_READ_SECURITY,       ACE_READ_ACL},
    {DARWIN_ACE_WRITE_SECURITY,      ACE_WRITE_ACL},
    {DARWIN_ACE_TAKE_OWNERSHIP,      ACE_WRITE_OWNER},
    {0,0}
};

struct nfsv4_to_darwin_flags_map {
    uint16_t from;
    uint32_t to;
};

struct nfsv4_to_darwin_flags_map nfsv4_to_darwin_flags[] = {
    {ACE_FILE_INHERIT_ACE,         DARWIN_ACE_FLAGS_FILE_INHERIT},
    {ACE_DIRECTORY_INHERIT_ACE,    DARWIN_ACE_FLAGS_DIRECTORY_INHERIT},
    {ACE_NO_PROPAGATE_INHERIT_ACE, DARWIN_ACE_FLAGS_LIMIT_INHERIT},
    {ACE_INHERIT_ONLY_ACE,         DARWIN_ACE_FLAGS_ONLY_INHERIT},
    {ACE_INHERITED_ACE,            DARWIN_ACE_FLAGS_INHERITED},
    {0,0}
};

struct darwin_to_nfsv4_flags_map {
    uint32_t from;
    uint16_t to;
};

struct darwin_to_nfsv4_flags_map darwin_to_nfsv4_flags[] = {
    {DARWIN_ACE_FLAGS_FILE_INHERIT,      ACE_FILE_INHERIT_ACE},
    {DARWIN_ACE_FLAGS_DIRECTORY_INHERIT, ACE_DIRECTORY_INHERIT_ACE},
    {DARWIN_ACE_FLAGS_LIMIT_INHERIT,     ACE_NO_PROPAGATE_INHERIT_ACE},
    {DARWIN_ACE_FLAGS_ONLY_INHERIT,      ACE_INHERIT_ONLY_ACE},
    {DARWIN_ACE_FLAGS_INHERITED,         ACE_INHERITED_ACE},
    {0,0}
};
#endif /* HAVE_SOLARIS_ACLS */

#endif	/* ACL_MAPPINGS */
