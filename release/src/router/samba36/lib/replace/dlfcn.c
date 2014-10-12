/* 
   Unix SMB/CIFS implementation.
   Samba system utilities
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1998-2002
   Copyright (C) Jelmer Vernooij 2006

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#ifdef HAVE_DL_H
#include <dl.h>
#endif

#ifndef HAVE_DLOPEN
#ifdef DLOPEN_TAKES_UNSIGNED_FLAGS
void *rep_dlopen(const char *name, unsigned int flags)
#else
void *rep_dlopen(const char *name, int flags)
#endif
{
#ifdef HAVE_SHL_LOAD
	if (name == NULL)
		return PROG_HANDLE;
	return (void *)shl_load(name, flags, 0);
#else
	return NULL;
#endif
}
#endif

#ifndef HAVE_DLSYM
void *rep_dlsym(void *handle, const char *symbol)
{
#ifdef HAVE_SHL_FINDSYM
	void *sym_addr;
	if (!shl_findsym((shl_t *)&handle, symbol, TYPE_UNDEFINED, &sym_addr))
		return sym_addr;
#endif
    return NULL;
}
#endif

#ifndef HAVE_DLERROR
char *rep_dlerror(void)
{
	return "dynamic loading of objects not supported on this platform";
}
#endif

#ifndef HAVE_DLCLOSE
int rep_dlclose(void *handle)
{
#ifdef HAVE_SHL_CLOSE
	return shl_unload((shl_t)handle);
#else
	return 0;
#endif
}
#endif
