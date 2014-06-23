/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#include "ctl_functions.h"
#include "ctl_socket.h"
#include "ctl_socket_client.h"
#include "log.h"

CLIENT_SIDE_FUNCTION(enable_bridge_rstp)
    CLIENT_SIDE_FUNCTION(get_bridge_state)
    CLIENT_SIDE_FUNCTION(set_bridge_config)
    CLIENT_SIDE_FUNCTION(get_port_state)
    CLIENT_SIDE_FUNCTION(set_port_config)
    CLIENT_SIDE_FUNCTION(set_debug_level)
#include <base.h>
const char *CTL_error_explanation(int err_no)
{
#define CHOOSE(a) #a
	static const char *rstp_error_names[] = RSTP_ERRORS;
	static const char *ctl_error_names[] = { CTL_ERRORS };

#undef CHOOSE
	if (err_no < 0)
		return "Error doing ctl command";
	else if (err_no >= STP_OK && err_no < STP_LAST_DUMMY)
		return rstp_error_names[err_no];
	else if (err_no > Err_Dummy_Start && err_no < Err_Dummy_End)
		return ctl_error_names[err_no - Err_Dummy_Start - 1];

	static char buf[32];
	sprintf(buf, "Unknown error code %d", err_no);
	return buf;
}

void Dprintf(int level, const char *fmt, ...)
{
	char logbuf[256];
	logbuf[sizeof(logbuf) - 1] = 0;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(logbuf, sizeof(logbuf) - 1, fmt, ap);
	va_end(ap);
	printf("%s\n", logbuf);
}
