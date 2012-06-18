/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: control.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/


#ifndef _CONTROL_H_
#define _CONTROL_H_ 1

#define  OP_START		1
#define  OP_STOP		2
#define  OP_ENABLE		3
#define  OP_DISABLE		4
#define  OP_ABORT		6
#define  OP_KILL		7
#define  OP_HOLD		8
#define  OP_RELEASE		9
#define  OP_TOPQ		10
#define  OP_LPQ			11
#define  OP_LPRM		12
#define  OP_STATUS		13
#define  OP_REDIRECT	14
#define  OP_LPD			15
#define  OP_PRINTCAP	16
#define  OP_UP			17
#define  OP_DOWN		18
#define  OP_REREAD		19
#define  OP_MOVE		20
#define  OP_DEBUG		21
#define  OP_HOLDALL		22
#define  OP_NOHOLDALL	23
#define  OP_CLASS 		24
#define  OP_DEFAULTQ	25
#define  OP_ACTIVE		26
#define  OP_REDO		27
#define  OP_CLIENT		28
#define  OP_SERVER		29
#define  OP_MSG			30
#define  OP_DEFAULTS	31
#define  OP_FLUSH		32
#define  OP_LANG		33

/* PROTOTYPES */

int Get_controlword( char *s );
char *Get_controlstr( int c );

#endif
