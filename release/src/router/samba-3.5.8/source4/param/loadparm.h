/* 
   Unix SMB/CIFS implementation.
   
   type definitions for loadparm

   Copyright (C) Karl Auer 1993-1998

   Largely re-written by Andrew Tridgell, September 1994

   Copyright (C) Simo Sorce 2001
   Copyright (C) Alexander Bokovoy 2002
   Copyright (C) Stefan (metze) Metzmacher 2002
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* the following are used by loadparm for option lists */
typedef enum {
	P_BOOL,P_INTEGER,P_OCTAL,P_BYTES,P_LIST,P_STRING,P_USTRING,P_ENUM
} parm_type;

typedef enum {
	P_LOCAL,P_GLOBAL,P_NONE
} parm_class;

struct enum_list {
	int value;
	const char *name;
};

struct loadparm_context;

struct parm_struct {
	const char *label;
	parm_type type;
	parm_class pclass;
	int offset;
	bool (*special)(struct loadparm_context *, const char *, char **);
	const struct enum_list *enum_list;
	union {
		int bvalue;
		int ivalue;
		char *svalue;
		char cvalue;
		const char **lvalue;
	} def;
};

#define FLAG_DEFAULT    0x0001 /* this option was a default */
#define FLAG_CMDLINE    0x0002 /* this option was set from the command line */

#ifndef PRINTERS_NAME
#define PRINTERS_NAME "printers"
#endif

#ifndef HOMES_NAME
#define HOMES_NAME "homes"
#endif

/* This defines the section name in the configuration file that will contain */
/* global parameters - that is, parameters relating to the whole server, not */
/* just services. This name is then reserved, and may not be used as a       */
/* a service name. It will default to "global" if not defined here.          */
#ifndef GLOBAL_NAME
#define GLOBAL_NAME "global"
#define GLOBAL_NAME2 "globals"
#endif

/* The default workgroup - usually overridden in smb.conf */
#ifndef DEFAULT_WORKGROUP
#define DEFAULT_WORKGROUP "WORKGROUP"
#endif

/*
 * Default passwd chat script.
 */
#ifndef DEFAULT_PASSWD_CHAT
#define DEFAULT_PASSWD_CHAT "*new*password* %n\\n *new*password* %n\\n *changed*"
#endif

/* Max number of jobs per print queue. */
#ifndef PRINT_MAX_JOBID
#define PRINT_MAX_JOBID 10000
#endif


