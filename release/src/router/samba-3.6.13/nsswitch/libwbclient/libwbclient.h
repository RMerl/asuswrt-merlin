/*
   Unix SMB/CIFS implementation.

   Winbind client API

   Copyright (C) Gerald (Jerry) Carter 2007

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBWBCLIENT_H
#define _LIBWBCLIENT_H

/* Super header including necessary public and private header files
   for building the wbclient library.  __DO NOT__ define anything
   in this file.  Only include other headers. */

/* Winbind headers */

#include "nsswitch/winbind_nss_config.h"
#include "nsswitch/winbind_struct_protocol.h"

/* Public headers */

#include "wbclient.h"

/* Private headers */

#include "wbc_err_internal.h"
#include "wbclient_internal.h"


#endif      /* _LIBWBCLIENT_H */
