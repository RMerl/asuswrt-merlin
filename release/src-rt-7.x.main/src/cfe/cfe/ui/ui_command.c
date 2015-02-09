/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  UI Command Dispatch			File: ui_command.c
    *  
    *  This module contains routines to maintain the command table,
    *  parse and execute commands
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


#include <stdarg.h>

#include "lib_queue.h"

#include "lib_types.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_error.h"
#include "env_subr.h"
#include "cfe.h"

#include "ui_command.h"

#define MAX_EXPAND	16

typedef struct cmdtab_s {
    struct cmdtab_s *sibling;
    struct cmdtab_s *child;
    char *cmdword;
    int (*func)(ui_cmdline_t *,int argc,char *argv[]);
    void *ref;
    char *help;
    char *usage;
    char *switches;
} cmdtab_t;

cmdtab_t *cmd_root;


#define myisalpha(x) (((x)>='A')&&((x)<='Z')&&((x)>='a')&&((x)<='z'))
#define myisdigit(x) (((x)>='0')&&((x)<='9'))
#define myisquote(x) (((x)=='\'')||((x)=='"'))

char *varchars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        "0123456789_?";

static char *tokenbreaks = " =\t\n\'\"&|;";
static char *spacechars = " \t";

static char *cmd_eat_quoted_arg(queue_t *head,ui_token_t *t);


static inline int is_white_space(ui_token_t *t) 
{
    return (strchr(spacechars,t->token) != NULL);
}

int cmd_sw_value(ui_cmdline_t *cmd,char *swname,char **swvalue)
{
    int idx;

    for (idx = 0; idx < cmd->swc; idx++) {
	if (strcmp(swname,cmd->swv[idx].swname) == 0) {
	    *swvalue = cmd->swv[idx].swvalue;
	    return 1;
	    }
	}

    return 0;
}

int cmd_sw_posn(ui_cmdline_t *cmd,char *swname)
{
    int idx;

    for (idx = 0; idx < cmd->swc; idx++) {
	if (strcmp(swname,cmd->swv[idx].swname) == 0) {
	    return cmd->swv[idx].swidx;
	    }
	}

    return -1;
}

char *cmd_sw_name(ui_cmdline_t *cmd,int swidx)
{
    if ((swidx < 0) || (swidx >= cmd->swc)) return NULL;

    return cmd->swv[swidx].swname;
}


int cmd_sw_isset(ui_cmdline_t *cmd,char *swname)
{
    int idx;

    for (idx = 0; idx < cmd->swc; idx++) {
	if (strcmp(swname,cmd->swv[idx].swname) == 0) {
	    return 1;
	    }
	}

    return 0;
}

char *cmd_getarg(ui_cmdline_t *cmd,int argnum)
{
    argnum += cmd->argidx;
    if ((argnum < 0) || (argnum >= cmd->argc)) return NULL;
    return cmd->argv[argnum];
}

void cmd_free(ui_cmdline_t *cmd)
{
    int idx;

    for (idx = 0; idx < cmd->argc; idx++) {
	KFREE(cmd->argv[idx]);
	}

    for (idx = 0; idx < cmd->swc; idx++) {
	KFREE(cmd->swv[idx].swname);
	}

    cmd->argc = 0;
    cmd->swc = 0;
}

int cmd_sw_validate(ui_cmdline_t *cmd,char *validstr)
{
    char *vdup;
    char *vptr;
    char *vnext;
    char atype;
    char *x;
    int idx;
    int valid;

    if (cmd->swc == 0) return -1;

    vdup = strdup(validstr);

    for (idx = 0; idx < cmd->swc; idx++) {
	vptr = vdup;

	vnext = vptr;
	valid = 0;

	while (vnext) {

	    /*
	     * Eat the next switch description from the valid string
	     */
	    x = strchr(vptr,'|');
	    if (x) {
		*x = '\0';
		vnext = x+1;
		}
	    else {
		vnext = NULL;
		}

	    /*
	     * Get the expected arg type, if any 
	     */
	    x = strchr(vptr,'=');
	    if (x) {
		atype = *(x+1);
		*x = 0;
		}
	    else {
		if ((x = strchr(vptr,';'))) *x = 0;
		atype = 0;
		}


	    if (strcmp(vptr,cmd->swv[idx].swname) == 0) {
		/* Value not needed and not supplied */
		if ((atype == 0) && (cmd->swv[idx].swvalue == NULL)) {
		    valid = 1;
		    }
		/* value needed and supplied */
		if ((atype != 0) && (cmd->swv[idx].swvalue != NULL)) {
		    valid = 1;
		    }
		strcpy(vdup,validstr);
		break;
		}

	    /*
	     * Otherwise, next!
	     */

	    strcpy(vdup,validstr);
	    vptr = vnext;
	    }

	/*
	 * If not valid, return index of bad switch
	 */

	if (valid == 0) {
	    KFREE(vdup);
	    return idx;
	    }

	}

    /*
     * Return -1 if everything went well.  A little strange,
     * but it's easier this way.
     */

    KFREE(vdup);
    return -1;
}

static cmdtab_t *cmd_findword(cmdtab_t *list,char *cmdword)
{
    while (list) {
	if (strcmp(cmdword,list->cmdword) == 0) return list;
	list = list->sibling;
	}

    return NULL;
}


void cmd_build_cmdline(queue_t *head, ui_cmdline_t *cmd)
{
    ui_token_t *t;
    ui_token_t *next;

    memset(cmd, 0, sizeof(ui_cmdline_t));

    t = (ui_token_t *) q_deqnext(head);

    while (t != NULL) {
	if (is_white_space(t)) {
	    /* do nothing */
	    } 
	else if (t->token != '-') {
	    if(cmd->argc < MAX_TOKENS){
		cmd->argv[cmd->argc] = cmd_eat_quoted_arg(head,t);
		cmd->argc++;
		}
	    /* Token is a switch */
	    }
	else {
	    if (cmd->swc < MAX_SWITCHES) {
		cmd->swv[cmd->swc].swname = lib_strdup(&(t->token));

		if (t->qb.q_next != head) {			/* more tokens */
		    next = (ui_token_t *) t->qb.q_next;
		    if (next->token == '=') {			/* switch has value */
			KFREE(t);				/* Free switch name */
			t = (ui_token_t *) q_deqnext(head);	/* eat equal sign */
			KFREE(t);				/* and free it */
			t = (ui_token_t *) q_deqnext(head);	/* now have value */
			if (t != NULL) {
			    cmd->swv[cmd->swc].swvalue = cmd_eat_quoted_arg(head,t);
			    }
			}
		    else {					/* no value */
			cmd->swv[cmd->swc].swvalue = NULL;
			}
		    }
		/* 
		 * swidx is the index of the argument that this
		 * switch precedes.  So, if you have "foo -d bar",
		 * swidx for "-d" would be 1.
		 */
		cmd->swv[cmd->swc].swidx = cmd->argc;
		cmd->swc++;	
		}
	    }
	KFREE(t);
	t = (ui_token_t *) q_deqnext(head);
	}


}

int cmd_addcmd(char *command,
	       int (*func)(ui_cmdline_t *,int argc,char *argv[]),
	       void *ref,
	       char *help,
	       char *usage,
	       char *switches)
{
    cmdtab_t **list = &cmd_root;
    cmdtab_t *cmd = NULL;
    queue_t tokens;
    queue_t *cur;
    ui_token_t *t;

    cmd_build_list(&tokens,command);
    cur = tokens.q_next;

    while (cur != &tokens) {
	t = (ui_token_t *) cur;
	if (!is_white_space(t)) {
	    cmd = cmd_findword(*list,&(t->token));
	    if (!cmd) {
		cmd = KMALLOC(sizeof(cmdtab_t)+strlen(&(t->token))+1,0);
		memset(cmd,0,sizeof(cmdtab_t));
		cmd->cmdword = (char *) (cmd+1);
		strcpy(cmd->cmdword,&(t->token));
		cmd->sibling = *list;
		*list = cmd;
		}
	    list = &(cmd->child);
	    }
	cur = cur->q_next;
	}

    cmd_free_tokens(&tokens);

    if (!cmd) return -1;

    cmd->func = func;
    cmd->usage = usage;
    cmd->ref = ref;
    cmd->help = help;
    cmd->switches = switches;

    return 0;
}



static void _dumpindented(char *str,int amt)
{
    int idx;
    char *dupstr;
    char *end;
    char *ptr;

    dupstr = strdup(str);

    ptr = dupstr;

    while (*ptr) {
	for (idx = 0; idx < amt; idx++) printf(" ");

	end = strchr(ptr,'\n');

	if (end) *end++ = '\0';
	else end = ptr + strlen(ptr);

	printf("%s\n",ptr);
	ptr = end;
	}

    KFREE(dupstr);
}

static void _dumpswitches(char *str)
{
    char *switches;
    char *end;
    char *ptr;
    char *semi;
    char *newline;

    switches = strdup(str);

    ptr = switches;

    while (*ptr) {
	end = strchr(ptr,'|');
	if (end) *end++ = '\0';
	else end = ptr + strlen(ptr);

	printf("     ");
	if ((semi = strchr(ptr,';'))) {
	    *semi++ = '\0';
	    newline = strchr(semi,'\n');
	    if (newline) *newline++ = '\0';
	    printf("%-12s %s\n",ptr,semi);
	    if (newline) _dumpindented(newline,5+12+1);
	    }
	else {
	    printf("%-12s (no information)\n",ptr);
	    }
	ptr = end;
	}

    KFREE(switches);
}

static void _dumpcmds(cmdtab_t *cmd,int level,char **words,int verbose)
{
    int idx;
    int len;

    while (cmd) {
	len = 0;
	words[level] = cmd->cmdword;
	if (cmd->func) {
	    for (idx = 0; idx < level; idx++) {
		printf("%s ",words[idx]);
		len += strlen(words[idx])+1;
		}
	    printf("%s",cmd->cmdword);
	    len += strlen(cmd->cmdword);
	    for (idx = len; idx < 20; idx++) printf(" ");
	    printf("%s\n",cmd->help);
	    if (verbose) {
		printf("\n");
		_dumpindented(cmd->usage,5);
		printf("\n");
		_dumpswitches(cmd->switches);
		printf("\n");
		}
	    }
	_dumpcmds(cmd->child,level+1,words,verbose);
	cmd = cmd->sibling;
	}
}

static void dumpcmds(int verbose)
{
    char *words[20];

    _dumpcmds(cmd_root,0,words,verbose);
}


static void _showpossible(ui_cmdline_t *cline,cmdtab_t *cmd)
{
    int i;

    if (cline->argidx == 0) {
	printf("Available commands: ");
	}
    else {
	printf("Available \"");
	for (i = 0; i < cline->argidx; i++) {
	    printf("%s%s",(i == 0) ? "" : " ",cline->argv[i]);
	    }
	printf("\" commands: ");
	}

    while (cmd) {
	printf("%s",cmd->cmdword);
	if (cmd->sibling) printf(", ");
	cmd = cmd->sibling;
	}

    printf("\n");
}

static int cmd_help(ui_cmdline_t *cmd,int argc,char *argv[])
{
    cmdtab_t **tab;
    cmdtab_t *cword;
    int idx;

    if (argc == 0) {
	printf("Available commands:\n\n");
	dumpcmds(0);
	printf("\n");
	printf("For more information about a command, enter 'help command-name'\n");
	}
    else {
	idx = 0;
	tab = &cmd_root;
	cword = NULL;

	for (;;) {
	    cword = cmd_findword(*tab,argv[idx]);
	    if (!cword) break;
	    if (cword->func != NULL) break;
	    idx++;
	    tab = &(cword->child);
	    if (idx >= argc) break;
	    }

	if (cword == NULL) {
	    printf("No help available for '%s'.\n\n",argv[idx]);
	    printf("Type 'help' for a list of commands.\n");
	    return -1;
	    }

	if (!cword->func && (idx >= argc)) {
	    printf("No help available for '%s'.\n\n",cword->cmdword);
	    printf("Type 'help' for a list of commands.\n");
	    return -1;
	    }

	printf("\n  SUMMARY\n\n");
	_dumpindented(cword->help,5);
	printf("\n  USAGE\n\n");
	_dumpindented(cword->usage,5);
	if (cword->switches && cword->switches[0]) {
	    printf("\n  OPTIONS\n\n");
	    _dumpswitches(cword->switches);
	    }
	printf("\n");
	}

    return 0;
}

void cmd_init(void)
{
    cmd_root = NULL;

    cmd_addcmd("help",
	       cmd_help,
	       NULL,
	       "Obtain help for CFE commands",
	       "help [command]\n\n"
	       "Without any parameters, the 'help' command will display a summary\n"
	       "of available commands.  For more details on a command, type 'help'\n"
	       "and the command name.",
	       "");
}


int cmd_lookup(queue_t *head,ui_cmdline_t *cmd)
{
    cmdtab_t **tab;
    cmdtab_t *cword;
    int idx;

    /*
     * Reset the command line
     */

    memset(cmd,0,sizeof(ui_cmdline_t));

    /*
     * Break it up into tokens
     */
    
    cmd_build_cmdline(head, cmd);

    if (cmd->argc == 0) return CMD_ERR_BLANK;

    /*
     * Start walking the tree looking for a function	
     * to execute.
     */
  
    idx = 0;
    tab = &cmd_root;
    cword = NULL;

    for (;;) {
	cword = cmd_findword(*tab,cmd->argv[idx]);
	if (!cword) break;
	if (cword->func != NULL) break;
	idx++;
	tab = &(cword->child);
	if (idx >= cmd->argc) break;
	}

    cmd->argidx = idx;


    if (cword == NULL) {
	printf("Invalid command: \"%s\"\n", cmd->argv[idx]);
	_showpossible(cmd,*tab);
	printf("\n");
	return CMD_ERR_INVALID;
	}

    if (!cword->func && (idx >= cmd->argc)) {
	printf("Incomplete command: \"%s\"\n",cmd->argv[idx-1]);
	_showpossible(cmd,*tab);
	printf("\n");
	return CMD_ERR_AMBIGUOUS;
	}

    cmd->argidx++;
    cmd->ref = cword->ref;
    cmd->usage = cword->usage;
    cmd->switches = cword->switches;
    cmd->func = cword->func;

    return 0;
}


void cmd_showusage(ui_cmdline_t *cmd)
{
    printf("\n");
    _dumpindented(cmd->usage,5);
    printf("\n");
    if (cmd->switches[0]) {
	_dumpswitches(cmd->switches);
	printf("\n");
	}
}


static void cmd_eat_leading_white(queue_t *head)
{
    ui_token_t *t;

    while (!q_isempty(head)) {
	t = (ui_token_t *) q_getfirst(head);
	if (is_white_space(t)) {
	    q_dequeue(&(t->qb));
	    KFREE(t);
	    }
	else break;
	}
}

ui_command_t *cmd_readcommand(queue_t *head)
{
    char *ptr;
    int insquote = FALSE;
    int indquote = FALSE;
    ui_command_t *cmd;
    int term = CMD_TERM_EOL;
    ui_token_t *t;

    cmd_eat_leading_white(head);

    if (q_isempty(head)) return NULL;

    cmd = (ui_command_t *) KMALLOC(sizeof(ui_command_t),0);
    q_init(&(cmd->head));

    while ((t = (ui_token_t *) q_deqnext(head))) {

	ptr = &(t->token);

	if (!insquote && !indquote) {
	    if ((*ptr == ';') || (*ptr == '\n')) {
		term = CMD_TERM_SEMI;
		break;
		}	
	    if ((*ptr == '&') && (*(ptr+1) == '&')) {
		term = CMD_TERM_AND;
		break;
		}
	    if ((*ptr == '|') && (*(ptr+1) == '|')) {
		term = CMD_TERM_OR;
		break;
		}
	    }

	if (*ptr == '\'') {
	    insquote = !insquote;
	    }

	if (!insquote) {
	    if (*ptr == '"') {
		indquote = !indquote;
		}
	    }

	q_enqueue(&(cmd->head),&(t->qb));
		
	}

    cmd->term = term;

    /* If we got out by finding a command separator, eat the separator */
    if (term != CMD_TERM_EOL) {
	KFREE(t);
	}

    return cmd;
}



static ui_token_t *make_token(char *str,int len)
{
    ui_token_t *t = (ui_token_t *) KMALLOC(sizeof(ui_token_t) + len,0);

    memcpy(&(t->token),str,len);
    (&(t->token))[len] = 0;

    return t;
}

void cmd_build_list(queue_t *qb,char *buf)
{
    char *cur = buf, *start = NULL, *fin = NULL;
    ui_token_t *t;

    q_init(qb);

    start = cur;
    while(*cur != '\0'){
	if (*cur == '&' && *(cur + 1) != '&') {
	    /* Do nothing if we have only one & */
	    }
	else if (*cur == '|' && *(cur + 1) != '|') {
	    /* Do nothing if we have only one | */
	    }
	else if (((*cur == ' ')||(*cur == '\t')) &&
		 ((*(cur - 1) == ' ')||(*(cur - 1) == '\t'))) {
	    /* Make one big token for white space */
	    }
	else {

	    if (strchr(tokenbreaks,*cur)) {
		if (cur != buf) {
		    fin = cur;
		    t = make_token(start,fin-start);
		    q_enqueue(qb,&(t->qb));
		    start = cur; /* Start new token */
		    }
		}
	    else {
		/* If we are on a normal character but the last character was */
		/* a special char we need to start a new token */

		if ((cur > buf) && strchr(tokenbreaks,*(cur-1))) {
		    fin = cur;
		    t = make_token(start,fin-start);
		    q_enqueue(qb,&(t->qb));
		    start = cur; /* Start new token */
		    }
		else {
		    /* If the last charecter wasn't special keep going with */
		    /* current token */
		    }


		}

	    }
	cur++;
	}

    fin = cur;

    if (fin-start > 0) {
	t = make_token(start,fin-start);
	q_enqueue(qb,&(t->qb));
	}

    return;
}

static int is_command_separator(ui_token_t *t)
{
    char *string = &(t->token);
    int sep = 0;

    switch(*string){
	case ';':
	    sep = 1;
	    break;
	case '&':
	    if(*(string + 1) == '&')
		sep = 1;
	    break;
	case '|':
	    if(*(string + 1) == '|')
		sep = 1;
	default:
	    break;
	}

    return(sep);
}

static char *cmd_eat_quoted_arg(queue_t *head,ui_token_t *t)
{
    int dquote = 0;
    int squote = 0;
    queue_t qlist;
    queue_t *q;
    char *dest;
    int maxlen = 0;

    /*
     * If it's not a quoted string, just return this token.
     */

    if (!myisquote(t->token)) {
	dest = lib_strdup(&(t->token));
	/* Note: caller deletes original token */
	return dest;
	}

    /*
     * Otherwise, eat tokens in the quotes.
     */

    q_init(&qlist);

    if (t->token == '"') dquote = 1;
    else squote = 1;			/* must be one or the other */

    t = (ui_token_t *) q_deqnext(head);
    while (t != NULL) {
	/* A single quote can only be terminated by another single quote */
	if (squote && (t->token == '\'')) {
	    KFREE(t);
	    break;
	    }
	/* A double quote is only honored if not in a single quote */
	if (dquote && !squote && (t->token == '\"')) {
	    KFREE(t);
	    break;
	    }
	/* Otherwise, keep this token. */
	q_enqueue(&qlist,(queue_t *) t);
	t = (ui_token_t *) q_deqnext(head);
	}

    /*
     * Go back through what we collected and figure out the string length.
     */

    for (q = qlist.q_next; q != &qlist; q = q->q_next) {
	maxlen += strlen(&(((ui_token_t *) q)->token));
	}

    dest = KMALLOC(maxlen+1,0);
    if (!dest) return NULL;

    *dest = '\0';

    while ((t = (ui_token_t *) q_deqnext(&qlist))) {
	strcat(dest,&(t->token));
	KFREE(t);
	}

    return dest;
}

static void cmd_append_tokens(queue_t *qb,char *str)
{
    queue_t *qq;
    queue_t explist;

    cmd_build_list(&explist,str);

    while ((qq = q_deqnext(&explist))) {
	q_enqueue(qb,qq);
	}
}



void cmd_walk_and_expand (queue_t *qb)
{
    queue_t *q;
    queue_t newq;
    ui_token_t *t;
    int alias_check = TRUE;
    int insquote = FALSE;
    char *envstr;

    q_init(&newq);

    while ((t = (ui_token_t *) q_deqnext(qb))) {
	if (t->token == '\'') {
	    alias_check = FALSE;
	    insquote = !insquote;
	    /* Check to see if we should try to expand this token */
	    }
	else if (!insquote) {
	    if (alias_check && !strchr(tokenbreaks,t->token) && 
		(envstr = env_getenv(&(t->token)))) {
		/* Aliases: stick into token stream if no environment found */
		cmd_append_tokens(&newq,envstr);
		KFREE(t);
		t = NULL;
		}
	    else if (t->token == '$') {
		/* non-aliases: remove from token stream if no env found */
		envstr = env_getenv(&(t->token)+1);
		if (envstr) cmd_append_tokens(&newq,envstr);
		KFREE(t);
		t = NULL;
		}
	    else {
		/* Drop down below, keep this token as-is and append */
		}
	    }

	/*
	 * If token was not removed, add it to the new queue
	 */

	if (t) {
	    q_enqueue(&newq,&(t->qb));
	    alias_check = is_command_separator(t);
	    }

	}

    /*
     * Put everything back on the original list. 
     */

    while ((q = q_deqnext(&newq))) {
	q_enqueue(qb,q);
	}

}

void cmd_free_tokens(queue_t *list)
{
    queue_t *q;

    while ((q = q_deqnext(list))) {
	KFREE(q);
	}
}
