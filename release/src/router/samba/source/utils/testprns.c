/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   test printer setup
   Copyright (C) Karl Auer 1993, 1994-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Testbed for pcap.c
 *
 * This module simply checks a given printer name against the compiled-in
 * printcap file.
 *
 * The operation is performed with DEBUGLEVEL at 3.
 *
 * Useful for a quick check of a printcap file.
 *
 */

#include "includes.h"
#include "smb.h"

/* these live in util.c */
extern FILE *dbf;
extern int DEBUGLEVEL;

int main(int argc, char *argv[])
{
   char *pszTemp;

   TimeInit();

   setup_logging(argv[0],True);

   charset_initialise();

   if (argc < 2 || argc > 3)
      printf("Usage: testprns printername [printcapfile]\n");
   else
   {
      dbf = sys_fopen("test.log", "w");
      if (dbf == NULL) {
         printf("Unable to open logfile.\n");
      } else {
         DEBUGLEVEL = 3;
         pszTemp = (argc < 3) ? PRINTCAP_NAME : argv[2];
         printf("Looking for printer %s in printcap file %s\n", 
                 argv[1], pszTemp);
         if (!pcap_printername_ok(argv[1], pszTemp))
            printf("Printer name %s is not valid.\n", argv[1]);
         else
            printf("Printer name %s is valid.\n", argv[1]);
         fclose(dbf);
      }
   }
   return (0);
}

