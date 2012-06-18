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
 * $Id: checkpc.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _CHECKPC_H_
#define _CHECKPC_H_ 1

/* PROTOTYPES */
int main( int argc, char *argv[], char *envp[] );
void mkdir_path( char *path );
void Scan_printer(struct line_list *spooldirs);
void Check_executable_filter( char *id, char *filter_str );
void Make_write_file( char *file, char *printer );
void usage(void);
int getage( char *age );
int getk( char *age );
int Check_file( char  *path, int fix, int age, int rmflag );
int Fix_create_dir( char  *path, struct stat *statb );
int Fix_owner( char *path );
int Fix_perms( char *path, int perms );
int Check_spool_dir( char *path, int owner );
void Test_port(int ruid, int euid, char *serial_line );
void Fix_clean( char *s, int no );
int Check_path_list( char *plist, int allow_missing );

#endif
