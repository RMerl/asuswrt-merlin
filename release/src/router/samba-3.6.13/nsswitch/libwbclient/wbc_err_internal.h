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

#ifndef _WBC_ERR_INTERNAL_H
#define _WBC_ERR_INTERNAL_H

/* Private macros */

#define BAIL_ON_WBC_ERROR(x)			\
	do {					\
		if (!WBC_ERROR_IS_OK(x)) {	\
			goto done;		\
		}				\
	} while(0);

#define BAIL_ON_PTR_ERROR(x, status)                    \
	do {						\
		if ((x) == NULL) {			\
			status = WBC_ERR_NO_MEMORY;	\
			goto done;			\
		} else {				\
			status = WBC_ERR_SUCCESS;	\
		}					\
	} while (0);


#endif	/* _WBC_ERR_INTERNAL_H */
