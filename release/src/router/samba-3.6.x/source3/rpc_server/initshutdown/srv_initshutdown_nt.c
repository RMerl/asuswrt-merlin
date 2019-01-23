/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell               1992-1997.
 *  Copyright (C) Gerald Carter                 2006.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Implementation of registry functions. */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_initshutdown.h"
#include "../librpc/gen_ndr/srv_winreg.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV


/*******************************************************************
 ********************************************************************/
WERROR _initshutdown_Init(struct pipes_struct *p, struct initshutdown_Init *r)
{
	struct winreg_InitiateSystemShutdownEx s;

	s.in.hostname = r->in.hostname;
	s.in.message = r->in.message;
	s.in.timeout = r->in.timeout;
	s.in.force_apps = r->in.force_apps;
	s.in.do_reboot = r->in.do_reboot;
	s.in.reason = 0;

	/* thunk down to _winreg_InitiateSystemShutdownEx()
	   (just returns a status) */

	return _winreg_InitiateSystemShutdownEx( p, &s );
}

/*******************************************************************
 ********************************************************************/

WERROR _initshutdown_InitEx(struct pipes_struct *p, struct initshutdown_InitEx *r)
{
	struct winreg_InitiateSystemShutdownEx s;
	s.in.hostname = r->in.hostname;
	s.in.message = r->in.message;
	s.in.timeout = r->in.timeout;
	s.in.force_apps = r->in.force_apps;
	s.in.do_reboot = r->in.do_reboot;
	s.in.reason = r->in.reason;

	return _winreg_InitiateSystemShutdownEx( p, &s);
}




/*******************************************************************
 reg_abort_shutdwon
 ********************************************************************/

WERROR _initshutdown_Abort(struct pipes_struct *p, struct initshutdown_Abort *r)
{
	struct winreg_AbortSystemShutdown s;
	s.in.server = r->in.server;
	return _winreg_AbortSystemShutdown( p, &s );
}
