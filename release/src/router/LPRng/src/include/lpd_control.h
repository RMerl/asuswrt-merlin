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
 * $Id: lpd_control.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _LPD_CONTROL_H_
#define _LPD_CONTROL_H_ 1

/* PROTOTYPES */
int Job_control( int *sock, char *input );
void Do_printer_work( char *user, int action, int *sock,
	struct line_list *tokens, char *error, int errorlen );
void Do_queue_control( char *user, int action, int *sock,
	struct line_list *tokens, char *error, int errorlen );
int Do_control_file( int action, int *sock,
	struct line_list *tokens, char *error, int errorlen, char *option );
int Do_control_lpq( char *user, int action,
	struct line_list *tokens );
int Do_control_status( int *sock,
	char *error, int errorlen );
int Do_control_redirect( int *sock,
	struct line_list *tokens, char *error, int errorlen );
int Do_control_class( int *sock,
	struct line_list *tokens, char *error, int errorlen );
int Do_control_debug( int *sock,
	struct line_list *tokens, char *error, int errorlen );
int Do_control_printcap( int *sock );
int Do_control_defaultq( int *sock );

#endif
