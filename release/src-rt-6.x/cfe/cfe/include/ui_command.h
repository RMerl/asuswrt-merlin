/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Command interpreter defs			File: ui_command.h
    *  
    *  This file contains structures related to the
    *  command interpreter.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#ifndef _LIB_QUEUE_H
#include "lib_queue.h"
#endif

typedef struct ui_cmdsw_s {
    int swidx;
    char *swname;
    char *swvalue;
} ui_cmdsw_t;

#define MAX_TOKENS	64
#define MAX_SWITCHES	16
#define MAX_TOKEN_SIZE  1000
typedef struct ui_cmdline_s {
    int argc;
    char *argv[MAX_TOKENS];
    int swc;
    ui_cmdsw_t swv[MAX_SWITCHES];
    int (*func)(struct ui_cmdline_s *,int argc,char *argv[]);
    int argidx;
    char *ref;
    char *usage;
    char *switches;
} ui_cmdline_t;

typedef struct ui_command_s { 
    queue_t list;
    int term;
    char *ptr;
    queue_t head;
} ui_command_t;

typedef struct ui_token_s {
    queue_t qb;
    char token;
} ui_token_t;

#define CMD_TERM_EOL	0
#define CMD_TERM_SEMI	1
#define CMD_TERM_AND	2
#define CMD_TERM_OR	3

int cmd_sw_value(ui_cmdline_t *cmd,char *swname,char **swvalue);
int cmd_sw_posn(ui_cmdline_t *cmd,char *swname);
char *cmd_sw_name(ui_cmdline_t *cmd,int swidx);
int cmd_sw_isset(ui_cmdline_t *cmd,char *swname);
char *cmd_getarg(ui_cmdline_t *cmd,int argnum);
void cmd_free(ui_cmdline_t *cmd);
int cmd_sw_validate(ui_cmdline_t *cmd,char *validstr);
void cmd_parse(ui_cmdline_t *cmd,char *line);
int cmd_addcmd(char *command,
	       int (*func)(ui_cmdline_t *,int argc,char *argv[]),
	       void *ref,
	       char *help,
	       char *usage,
	       char *switches);
int cmd_lookup(queue_t *head,ui_cmdline_t *cmd);
void cmd_init(void);
int cmd_getcommand(ui_cmdline_t *cmd);
void cmd_showusage(ui_cmdline_t *cmd);



#define CMD_ERR_INVALID	-1
#define CMD_ERR_AMBIGUOUS -2
#define CMD_ERR_BLANK -3


/*  *********************************************************************
    *  Prototypes (public routines)
    ********************************************************************* */

const char *ui_errstring(int errcode);
int ui_showerror(int errcode,char *tmplt,...);
int ui_showusage(ui_cmdline_t *cmd);
int ui_docommands(char *str);
#define ui_docommand(str) ui_docommands(str)
void ui_restart(int);
int ui_init_cmddisp(void);


/*  *********************************************************************
    *  Prototypes (internal routines)
    ********************************************************************* */

ui_command_t *cmd_readcommand(queue_t *head);
void cmd_build_list(queue_t *qb,char *buf);
void cmd_walk_and_expand(queue_t *qb);
ui_command_t *cmd_readcommand(queue_t *head);
void cmd_free_tokens(queue_t *list);
void cmd_build_cmdline(queue_t *head, ui_cmdline_t *cmd);
