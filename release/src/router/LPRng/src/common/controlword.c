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
 *
 ***************************************************************************/

 static char *const _id =
"$Id: controlword.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


#include "lp.h"
#include "control.h"
/**** ENDINCLUDE ****/

 static struct keywords controlwords[] = {

{ "ABORT", N_("ABORT"), OP_ABORT, 0, 0, 0, 0},
{ "ACTIVE", N_("ACTIVE"), OP_ACTIVE, 0, 0, 0, 0},
{ "CLASS", N_("CLASS"), OP_CLASS, 0, 0, 0, 0},
{ "CLIENT", N_("CLIENT"), OP_CLIENT, 0, 0, 0, 0},
{ "DEBUG", N_("DEBUG"), OP_DEBUG, 0, 0, 0, 0},
{ "DEFAULTQ", N_("DEFAULTQ"), OP_DEFAULTQ, 0, 0, 0, 0},
{ "DISABLE", N_("DISABLE"), OP_DISABLE, 0, 0, 0, 0},
{ "DOWN", N_("DOWN"), OP_DOWN, 0, 0, 0, 0},
{ "ENABLE", N_("ENABLE"), OP_ENABLE, 0, 0, 0, 0},
{ "HOLD", N_("HOLD"), OP_HOLD, 0, 0, 0, 0},
{ "HOLDALL", N_("HOLDALL"), OP_HOLDALL, 0, 0, 0, 0},
{ "KILL", N_("KILL"), OP_KILL, 0, 0, 0, 0},
{ "LPD", N_("LPD"), OP_LPD, 0, 0, 0, 0},
{ "LPQ", N_("LPQ"), OP_LPQ, 0, 0, 0, 0},
{ "LPRM", N_("LPRM"), OP_LPRM, 0, 0, 0, 0},
{ "MOVE", N_("MOVE"), OP_MOVE, 0, 0, 0, 0},
{ "MSG", N_("MSG"), OP_MSG, 0, 0, 0, 0},
{ "NOHOLDALL", N_("NOHOLDALL"), OP_NOHOLDALL, 0, 0, 0, 0},
{ "PRINTCAP", N_("PRINTCAP"), OP_PRINTCAP, 0, 0, 0, 0},
{ "REDIRECT", N_("REDIRECT"), OP_REDIRECT, 0, 0, 0, 0},
{ "REDO", N_("REDO"), OP_REDO, 0, 0, 0, 0},
{ "RELEASE", N_("RELEASE"), OP_RELEASE, 0, 0, 0, 0},
{ "REREAD", N_("REREAD"), OP_REREAD, 0, 0, 0, 0},
{ "START", N_("START"), OP_START, 0, 0, 0, 0},
{ "STATUS", N_("STATUS"), OP_STATUS, 0, 0, 0, 0},
{ "STOP", N_("STOP"), OP_STOP, 0, 0, 0, 0},
{ "TOPQ", N_("TOPQ"), OP_TOPQ, 0, 0, 0, 0},
{ "UP", N_("UP"), OP_UP, 0, 0, 0, 0},
{ "SERVER", N_("SERVER"), OP_SERVER, 0, 0, 0, 0},
{ "DEFAULTS", N_("DEFAULTS"), OP_DEFAULTS, 0, 0, 0, 0},
{ "FLUSH", N_("FLUSH"), OP_FLUSH, 0, 0, 0, 0},
{ "LANG", N_("LANG"), OP_LANG, 0, 0, 0, 0},

{0,0,0,0,0,0,0}
};


/***************************************************************************
 * Get_controlword()
 * - decode the control word and return a key
 ***************************************************************************/

int Get_controlword( char *s )
{
	return( Get_keyval( s, controlwords ) );
}

char *Get_controlstr( int c )
{
	return( Get_keystr( c, controlwords ) );
}
