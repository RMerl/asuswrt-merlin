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

#ifndef AFP_UUID_H
#define AFP_UUID_H

#define UUID_BINSIZE 16

typedef const unsigned char *uuidp_t;
typedef unsigned char atalk_uuid_t[UUID_BINSIZE];

typedef enum {UUID_USER   = 1,
              UUID_GROUP  = 2,
              UUID_ENOENT = 4} /* used as bit flag */
    uuidtype_t;
#define UUIDTYPESTR_MASK 3
extern char *uuidtype[];

/******************************************************** 
 * Interface
 ********************************************************/

extern int getuuidfromname( const char *name, uuidtype_t type, unsigned char *uuid);
extern int getnamefromuuid( const unsigned char *uuid, char **name, uuidtype_t *type);
extern void localuuid_from_id(unsigned char *buf, uuidtype_t type, unsigned int id);
extern const char *uuid_bin2string(const unsigned char *uuid);
extern void uuid_string2bin( const char *uuidstring, unsigned char *uuid);
extern void uuidcache_dump(void);

#endif /* AFP_UUID_H */
