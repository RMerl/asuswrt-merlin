/* 
   Unix SMB/CIFS implementation.

   SMB composite request interfaces

   Copyright (C) Volker Lendecke 2005
   
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

#ifndef __WB_ASYNC_HELPERS_H__
#define __WB_ASYNC_HELPERS_H__

#include "librpc/gen_ndr/lsa.h"

struct wb_sid_object {
	enum lsa_SidType type;
	struct dom_sid *sid;
	const char *domain;
	const char *name;
};

#endif /* __WB_ASYNC_HELPERS_H__ */
