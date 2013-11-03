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

#ifndef ACLLDAP_H
#define ACLLDAP_H

#include <atalk/uuid.h>		/* just for uuidtype_t*/

/******************************************************** 
 * Interface
 ********************************************************/

extern int ldap_getuuidfromname( const char *name, uuidtype_t type, char **uuid_string);
extern int ldap_getnamefromuuid( const char *uuidstr, char **name, uuidtype_t *type); 

#endif /* ACLLDAP_H */
