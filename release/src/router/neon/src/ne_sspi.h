/* 
   Microsoft SSPI based authentication routines
   Copyright (C) 2004-2005, Vladimir Berezniker @ http://public.xdi.org/=vmpn

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#ifndef NE_SSPI_H
#define NE_SSPI_H

/* Win32 SSPI-based authentication interfaces.  PRIVATE TO NEON -- NOT
 * PART OF THE EXTERNAL API. */

#ifdef HAVE_SSPI

#include <windows.h>
#define SECURITY_WIN32
#include <security.h>

int ne_sspi_init(void);
int ne_sspi_deinit(void);

int ne_sspi_create_context(void **context, char * serverName, int ntlm);

int ne_sspi_destroy_context(void *context);

int ne_sspi_clear_context(void *context);

int ne_sspi_authenticate(void *context, const char *base64Token,
                         char **responseToken);

#endif /* HAVE_SSPI */

#endif /* NE_SSPI_H */
