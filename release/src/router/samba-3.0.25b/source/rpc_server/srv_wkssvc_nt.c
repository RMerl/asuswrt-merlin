/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (C) Andrew Tridgell		1992-1997,
 *  Copyright (C) Gerald (Jerry) Carter		2006.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* This is the implementation of the wks interface. */

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/*******************************************************************
 Fill in the valiues for the struct wkssvc_NetWkstaInfo100.
 ********************************************************************/

static void create_wks_info_100(struct wkssvc_NetWkstaInfo100 *info100)
{
	pstring my_name;
	pstring domain;

	pstrcpy (my_name, global_myname());
	strupper_m(my_name);

	pstrcpy (domain, lp_workgroup());
	strupper_m(domain);
	
	info100->platform_id     = 0x000001f4; 	/* unknown */
	info100->version_major   = lp_major_announce_version(); 
	info100->version_minor   = lp_minor_announce_version();   

	info100->server_name = talloc_strdup( info100, my_name );
	info100->domain_name = talloc_strdup( info100, domain );

	return;
}

/********************************************************************
 only supports info level 100 at the moment.
 ********************************************************************/

WERROR _wkssvc_NetWkstaGetInfo( pipes_struct *p, struct wkssvc_NetWkstaGetInfo *r)
{
	struct wkssvc_NetWkstaInfo100 *wks100 = NULL;
	
	/* We only support info level 100 currently */
	
	if ( r->in.level != 100 ) {
		return WERR_UNKNOWN_LEVEL;
	}

	if ( (wks100 = TALLOC_ZERO_P(p->mem_ctx, struct wkssvc_NetWkstaInfo100)) == NULL ) {
		return WERR_NOMEM;
	}

	create_wks_info_100( wks100 );
	
	r->out.info->info100 = wks100;

	return WERR_OK;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetWkstaSetInfo( pipes_struct *p, struct wkssvc_NetWkstaSetInfo *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetWkstaEnumUsers( pipes_struct *p, struct wkssvc_NetWkstaEnumUsers *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRWKSTAUSERGETINFO( pipes_struct *p, struct WKSSVC_NETRWKSTAUSERGETINFO *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRWKSTAUSERSETINFO( pipes_struct *p, struct WKSSVC_NETRWKSTAUSERSETINFO *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetWkstaTransportEnum( pipes_struct *p, struct wkssvc_NetWkstaTransportEnum *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRWKSTATRANSPORTADD( pipes_struct *p, struct WKSSVC_NETRWKSTATRANSPORTADD *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRWKSTATRANSPORTDEL( pipes_struct *p, struct WKSSVC_NETRWKSTATRANSPORTDEL *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRUSEADD( pipes_struct *p, struct WKSSVC_NETRUSEADD *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRUSEGETINFO( pipes_struct *p, struct WKSSVC_NETRUSEGETINFO *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRUSEDEL( pipes_struct *p, struct WKSSVC_NETRUSEDEL *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRUSEENUM( pipes_struct *p, struct WKSSVC_NETRUSEENUM *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRMESSAGEBUFFERSEND( pipes_struct *p, struct WKSSVC_NETRMESSAGEBUFFERSEND *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRWORKSTATIONSTATISTICSGET( pipes_struct *p, struct WKSSVC_NETRWORKSTATIONSTATISTICSGET *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRLOGONDOMAINNAMEADD( pipes_struct *p, struct WKSSVC_NETRLOGONDOMAINNAMEADD *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRLOGONDOMAINNAMEDEL( pipes_struct *p, struct WKSSVC_NETRLOGONDOMAINNAMEDEL *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRJOINDOMAIN( pipes_struct *p, struct WKSSVC_NETRJOINDOMAIN *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRUNJOINDOMAIN( pipes_struct *p, struct WKSSVC_NETRUNJOINDOMAIN *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRRENAMEMACHINEINDOMAIN( pipes_struct *p, struct WKSSVC_NETRRENAMEMACHINEINDOMAIN *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRVALIDATENAME( pipes_struct *p, struct WKSSVC_NETRVALIDATENAME *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRGETJOININFORMATION( pipes_struct *p, struct WKSSVC_NETRGETJOININFORMATION *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRGETJOINABLEOUS( pipes_struct *p, struct WKSSVC_NETRGETJOINABLEOUS *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrJoinDomain2(pipes_struct *p, struct wkssvc_NetrJoinDomain2 *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrUnjoinDomain2(pipes_struct *p, struct wkssvc_NetrUnjoinDomain2 *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrRenameMachineInDomain2(pipes_struct *p, struct wkssvc_NetrRenameMachineInDomain2 *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRVALIDATENAME2( pipes_struct *p, struct WKSSVC_NETRVALIDATENAME2 *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRGETJOINABLEOUS2( pipes_struct *p, struct WKSSVC_NETRGETJOINABLEOUS2 *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrAddAlternateComputerName(pipes_struct *p, struct wkssvc_NetrAddAlternateComputerName *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrRemoveAlternateComputerName(pipes_struct *p, struct wkssvc_NetrRemoveAlternateComputerName *r)
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRSETPRIMARYCOMPUTERNAME( pipes_struct *p, struct WKSSVC_NETRSETPRIMARYCOMPUTERNAME *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _WKSSVC_NETRENUMERATECOMPUTERNAMES( pipes_struct *p, struct WKSSVC_NETRENUMERATECOMPUTERNAMES *r )
{
	/* FIXME: Add implementation code here */
	p->rng_fault_state = True;
	return WERR_NOT_SUPPORTED;
}

