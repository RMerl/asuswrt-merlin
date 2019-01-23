/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell              1992-1997,
   Copyright (C) Gerald (Jerry) Carter        2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SERVICES_H /* _SERVICES_H */
#define _SERVICES_H

#include "../librpc/gen_ndr/svcctl.h"

/* where we assume the location of the service control scripts */
#define SVCCTL_SCRIPT_DIR  "svcctl"

/*
 * dispatch table of functions to handle the =ServiceControl API
 */

typedef struct {
	/* functions for enumerating subkeys and values */
	WERROR 	(*stop_service)( const char *service, struct SERVICE_STATUS *status );
	WERROR 	(*start_service) ( const char *service );
	WERROR 	(*service_status)( const char *service, struct SERVICE_STATUS *status );
} SERVICE_CONTROL_OPS;

/* structure to store the service handle information  */

typedef struct _ServiceInfo {
	uint8			type;
	char			*name;
	uint32			access_granted;
	SERVICE_CONTROL_OPS	*ops;
} SERVICE_INFO;

#define SVC_HANDLE_IS_SCM			0x0000001
#define SVC_HANDLE_IS_SERVICE			0x0000002
#define SVC_HANDLE_IS_DBLOCK			0x0000003

#endif /* _SERICES_H */

