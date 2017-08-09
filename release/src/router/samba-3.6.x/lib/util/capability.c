/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1998-2002
   
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

/**
 * @file
 * @brief Capabilities functions
 **/

/*
  capabilities fns - will be needed when we enable kernel oplocks
*/

#include "includes.h"
#include "system/network.h"
#include "system/wait.h"
#include "system/filesys.h"


#if defined(HAVE_IRIX_SPECIFIC_CAPABILITIES)
/**************************************************************************
 Try and abstract process capabilities (for systems that have them).
****************************************************************************/
static bool set_process_capability( uint32_t cap_flag, bool enable )
{
	if(cap_flag == KERNEL_OPLOCK_CAPABILITY) {
		cap_t cap = cap_get_proc();

		if (cap == NULL) {
			DEBUG(0,("set_process_capability: cap_get_proc failed. Error was %s\n",
				strerror(errno)));
			return false;
		}

		if(enable)
			cap->cap_effective |= CAP_NETWORK_MGT;
		else
			cap->cap_effective &= ~CAP_NETWORK_MGT;

		if (cap_set_proc(cap) == -1) {
			DEBUG(0,("set_process_capability: cap_set_proc failed. Error was %s\n",
				strerror(errno)));
			cap_free(cap);
			return false;
		}

		cap_free(cap);

		DEBUG(10,("set_process_capability: Set KERNEL_OPLOCK_CAPABILITY.\n"));
	}
	return true;
}

/**************************************************************************
 Try and abstract inherited process capabilities (for systems that have them).
****************************************************************************/

static bool set_inherited_process_capability( uint32_t cap_flag, bool enable )
{
	if(cap_flag == KERNEL_OPLOCK_CAPABILITY) {
		cap_t cap = cap_get_proc();

		if (cap == NULL) {
			DEBUG(0,("set_inherited_process_capability: cap_get_proc failed. Error was %s\n",
				strerror(errno)));
			return false;
		}

		if(enable)
			cap->cap_inheritable |= CAP_NETWORK_MGT;
		else
			cap->cap_inheritable &= ~CAP_NETWORK_MGT;

		if (cap_set_proc(cap) == -1) {
			DEBUG(0,("set_inherited_process_capability: cap_set_proc failed. Error was %s\n", 
				strerror(errno)));
			cap_free(cap);
			return false;
		}

		cap_free(cap);

		DEBUG(10,("set_inherited_process_capability: Set KERNEL_OPLOCK_CAPABILITY.\n"));
	}
	return true;
}
#endif
