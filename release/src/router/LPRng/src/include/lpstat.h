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
 * $Id: lpstat.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _LPSTAT_H_
#define _LPSTAT_H_ 1
EXTERN int LP_mode;			/* LP mode */
EXTERN int Longformat;      /* Long format */
EXTERN int Displayformat;   /* Display format */
EXTERN int All_printers;    /* show all printers */
EXTERN int Status_line_count; /* number of status lines */
EXTERN int Clear_scr;       /* clear screen */
EXTERN int Interval;        /* display interval */

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
void Show_status(char **argv, int display_format);
int Read_status_info( char *host, int sock,
	int output, int timeout, int display_format,
	int status_line_count );
int Add_val( char **var, char *val );
void Get_parms(int argc, char *argv[] );
void usage(void);

#endif
