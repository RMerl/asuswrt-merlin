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
 * $Id: child.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _CHILD_H_
#define _CHILD_H_ 1

/* PROTOTYPES */
pid_t plp_waitpid (pid_t pid, plp_status_t *statusPtr, int options);
void Dump_pinfo( char *title, struct line_list *p ) ;
int Countpid(void);
void Killchildren( int sig );
pid_t dofork( int new_process_group );
plp_signal_t cleanup_USR1 (int passed_signal);
plp_signal_t cleanup_HUP (int passed_signal);
plp_signal_t cleanup_INT (int passed_signal);
plp_signal_t cleanup_QUIT (int passed_signal);
plp_signal_t cleanup_TERM (int passed_signal);
void Max_open( int fd );
plp_signal_t cleanup (int passed_signal);
void Dump_unfreed_mem(char *title);

#endif
