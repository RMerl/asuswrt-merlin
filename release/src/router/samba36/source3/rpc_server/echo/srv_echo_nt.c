/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines for rpcecho
 *  Copyright (C) Tim Potter                   2003
 *  Copyright (C) Jelmer Vernooij              2006
 *  Copyright (C) Gerald (Jerry) Carter        2007
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

/* This is the interface to the rpcecho pipe. */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_echo.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/* Add one to the input and return it */

void _echo_AddOne(struct pipes_struct *p, struct echo_AddOne *r )
{
	DEBUG(10, ("_echo_AddOne\n"));

	*r->out.out_data = r->in.in_data + 1;
}

/* Echo back an array of data */

void _echo_EchoData(struct pipes_struct *p, struct echo_EchoData *r)
{
	DEBUG(10, ("_echo_EchoData\n"));

	if ( r->in.len == 0 ) {
		r->out.out_data = NULL;
		return;
	}

	r->out.out_data = TALLOC_ARRAY(p->mem_ctx, uint8, r->in.len);
	memcpy( r->out.out_data, r->in.in_data, r->in.len );
	return;
}

/* Sink an array of data */

void _echo_SinkData(struct pipes_struct *p, struct echo_SinkData *r)
{
	DEBUG(10, ("_echo_SinkData\n"));

	/* My that was some yummy data! */
	return;
}

/* Source an array of data */

void _echo_SourceData(struct pipes_struct *p, struct echo_SourceData *r)
{
	uint32 i;

	DEBUG(10, ("_echo_SourceData\n"));

	if ( r->in.len == 0 ) {
		r->out.data = NULL;
		return;
	}

	r->out.data = TALLOC_ARRAY(p->mem_ctx, uint8, r->in.len );

	for (i = 0; i < r->in.len; i++ ) {
		r->out.data[i] = i & 0xff;
	}

	return;
}

void _echo_TestCall(struct pipes_struct *p, struct echo_TestCall *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return;
}

NTSTATUS _echo_TestCall2(struct pipes_struct *p, struct echo_TestCall2 *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_OK;
}

uint32 _echo_TestSleep(struct pipes_struct *p, struct echo_TestSleep *r)
{
	smb_msleep(r->in.seconds * 1000);
	return 0;
}

void _echo_TestEnum(struct pipes_struct *p, struct echo_TestEnum *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return;
}

void _echo_TestSurrounding(struct pipes_struct *p, struct echo_TestSurrounding *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return;
}

uint16 _echo_TestDoublePointer(struct pipes_struct *p, struct echo_TestDoublePointer *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return 0;
}
