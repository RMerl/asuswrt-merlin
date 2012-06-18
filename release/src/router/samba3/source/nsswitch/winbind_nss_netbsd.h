/* 
   Unix SMB/CIFS implementation.

   NetBSD loadable authentication module, providing identification 
   routines against Samba winbind/Windows NT Domain

   Copyright (C) Luke Mewburn 2004-2005
  
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA  02111-1307, USA.   
*/

#ifndef _WINBIND_NSS_NETBSD_H
#define _WINBIND_NSS_NETBSD_H

#include <nsswitch.h>

	/* dynamic nsswitch with "new" getpw* nsdispatch API available */
#if defined(NSS_MODULE_INTERFACE_VERSION) && defined(HAVE_GETPWENT_R)

typedef int NSS_STATUS;

#define NSS_STATUS_SUCCESS     NS_SUCCESS
#define NSS_STATUS_NOTFOUND    NS_NOTFOUND
#define NSS_STATUS_UNAVAIL     NS_UNAVAIL
#define NSS_STATUS_TRYAGAIN    NS_TRYAGAIN

#endif /* NSS_MODULE_INTERFACE_VERSION && HAVE_GETPWENT_R */

#endif /* _WINBIND_NSS_NETBSD_H */
