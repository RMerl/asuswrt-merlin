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

#ifndef LDAPCACHE_H
#define LDAPCACHE_H

/* 
 * We need to cache all LDAP querie results, they just take too long.
 * We do hashing with chaining. Two caches are needed:
 * 1) name -> uuid, indexed by a hash(f(): hashstring) of the name
 * 2) uuid -> name, indexed by a hash of the uuid(f(): hashuuid)
 * Both hash funcs result in a value 0-255 with which we index a array.
 * We malloc and free all elements as needed.
 * The cache caches for CACHESECONDS.
 */

#define CACHESECONDS 600

/******************************************************** 
 * Interface
 ********************************************************/

extern int search_cachebyname( const char *name, uuidtype_t *type, unsigned char *uuid);
extern int add_cachebyname( const char *inname, const uuidp_t inuuid, const uuidtype_t type, const unsigned long uid);
extern int search_cachebyuuid( uuidp_t uuidp, char **name, uuidtype_t *type);
extern int add_cachebyuuid( uuidp_t inuuid, const char *inname, uuidtype_t type, const unsigned long uid);

#endif /* LDAPCACHE_H */
