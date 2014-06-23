/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1t, 802.1w) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

#ifndef _CLI_API__
#define _CLI_API__

#define MAX_CLI_BUFF	80
#define MAXPARAMNUM     6
#define MAXPARAMLEN     40
#define MAX_CLI_PROMT	24
#define MAX_SELECTOR	12

typedef int CLI_CMD_CLBK (int argc, char** argv);

typedef enum {
  CMD_PAR_NUMBER,
  CMD_PAR_STRING,
  CMD_PAR_BOOL_YN,
  CMD_PAR_ENUM,
} CMD_PARAM_TYPE_T;

typedef struct cmd_par_number_limits_s {
  unsigned long min;
  unsigned long max;
} CMD_PAR_LIMITS;

typedef struct cmd_par_string_selector_s {
  char* string_value;
  char* string_help;
} CMD_PAR_SELECTOR;

typedef struct cmd_par_dscr_s {
  char*			param_help;
  CMD_PARAM_TYPE_T	param_type;
  CMD_PAR_LIMITS	number_limits;
  CMD_PAR_SELECTOR	string_selector[MAX_SELECTOR];
  char*			default_value;
} CMD_PAR_DSCR_T;

typedef struct cmd_dscr_s {
  char*			cmd_name;
  char*			cmd_help;
  CMD_PAR_DSCR_T	param[MAXPARAMNUM];
  CLI_CMD_CLBK*		clbk;
} CMD_DSCR_T;

#define THE_COMMAND(x, y)	{x, y, {
#define PARAM_NUMBER(x,zmin,zmax,def)	{x,CMD_PAR_NUMBER, {zmin, zmax}, {}, def},
#define PARAM_STRING(x, def)		{x,CMD_PAR_STRING, {},           {}, def},
#define PARAM_ENUM(x) 			{x,CMD_PAR_ENUM,   {},           {
#define PARAM_ENUM_SEL(x, y)		{x, y},
#define PARAM_ENUM_DEFAULT(def)		}, def},
#define PARAM_BOOL(x,yesd,nod,def)	{x, CMD_PAR_ENUM,  {}, {{"y",yesd},{"n",nod}},def}
#define THE_FUNC(x)			}, &x}, 
#define END_OF_LANG	{NULL,NULL}

char *get_prompt (void); /* this function not from the lib ! */

void cli_debug_dump_args (char* title, int argc, char** argv);

void cli_register_language (const CMD_DSCR_T* cmd_list);
void usage (void);
int cli_execute_command (const char* line);
#ifdef OLD_READLINE
void rl_read_cli (void);
#else
void rl_read_cli (char *);
#endif
void rl_init (void);
void rl_shutdown (void);
char* UT_sprint_time_stamp (void);

#endif /* _CLI_API__ */

