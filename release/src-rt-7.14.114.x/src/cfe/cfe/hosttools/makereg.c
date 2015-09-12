/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Register Table Generator			File: makereg.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  Warning: don't eat your lunch before reading this program.
    *  It's nasty!
    *
    *  This program generates tables of SOC registers and values
    *  that are easy for us to display.
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

#if defined(_MSC_VER) || defined(__CYGWIN__)
#define TEXTMODE "t"
#else
#define TEXTMODE ""
#endif

typedef struct Cons {
    char *str;
    int num;
} CONS;

typedef struct RegInfo {
    unsigned long reg_mask;
    char *reg_agent;
    int reg_agentidx;
    char *reg_addr;
    char *reg_inst;
    char *reg_subinst;
    char *reg_printfunc;
    char *reg_description;
} REGINFO;

typedef struct ConstInfo {
    char *name;
    unsigned int value;
} CONSTINFO;

#define MAXCONST 64
CONSTINFO constants[MAXCONST];
int constcnt = 0;

#define MAXREGS 2000
REGINFO allregs[MAXREGS];
int regcnt = 0;

int maskcnt = 0;

#define MAXAGENTS 32
char *agentnames[MAXAGENTS];
int agentcnt;

#define CMD_AGENT	1
#define CMD_ENDAGENT	2

CONS commands[] = {
    {"!agent",CMD_AGENT},
    {"!endagent",CMD_ENDAGENT},
    {NULL,0}};


int assoc(CONS *list,char *str)
{
    while (list->str) {
	if (strcmp(list->str,str) == 0) return list->num;
	list++;
	}

    return -1;
}

char *gettoken(char **ptr)
{
    char *p = *ptr;
    char *ret;

    /* skip white space */

    while (*p && isspace(*p)) p++;
    ret = p;

    /* check for end of string */

    if (!*p) {
	*ptr = p;
	return NULL;
	}

    /* skip non-whitespace */

    while (*p) {
	if (isspace(*p)) break;

	/* do quoted strings */

	if (*p == '"') {
	    p++;
	    ret = p;
	    while (*p && (*p != '"')) p++;
	    if (*p == '"') *p = '\0';
	    }

        p++;

	}

    if (*p) {
        *p++ = '\0';
        }
    *ptr = p;

    return ret;
}

static int readline(FILE *str,char *dest,int destlen)
{
    char *x;

    for (;;) {
	if (!fgets(dest,destlen,str)) return -1;
	if (x = strchr(dest,'\n')) *x = '\0';
	if (dest[0] == '\0') continue;
	if (dest[0] == ';') continue;
	break;
	}

    return 0;
}


static void fatal(char *str,char *val)
{
    fprintf(stderr,"fatal error: %s %s\n",str,val ? val : "");
    exit(1);
}

static unsigned int newmask(void)
{
    int res;

    res = maskcnt;

    if (maskcnt == 32) {
	fatal("Out of mask bits",NULL);
	}

    maskcnt++;

    return 1<<res;
}

static void addconst(char *name,unsigned int val)
{
    if (constcnt == MAXCONST) {
	fatal("Out of constant space",NULL);
	}

    constants[constcnt].name = strdup(name);
    constants[constcnt].value = val;

    constcnt++;
}

static void addreg(
    char *agentname,
    int agentidx,
    unsigned long mask,
    char *addr,
    char *inst,
    char *subinst,
    char *printfunc,
    char *description)
{
    allregs[regcnt].reg_mask = mask;
    allregs[regcnt].reg_addr = strdup(addr);
    allregs[regcnt].reg_agent = strdup(agentname);
    allregs[regcnt].reg_agentidx = agentidx;
    allregs[regcnt].reg_inst = strdup(inst);
    allregs[regcnt].reg_subinst = strdup(subinst);
    allregs[regcnt].reg_printfunc = strdup(printfunc);
    allregs[regcnt].reg_description = strdup(description);
    regcnt++;
}


static void macroexpand(char *instr,char *exp,char *outstr)
{
    while (*instr) {
	if (*instr == '$') {
	    strcpy(outstr,exp);
	    outstr += strlen(outstr);
	    instr++;
	    }
	else {
	    *outstr++ = *instr++;
	    }
	}

    *outstr = '\0';
}


static void doagentcmd(FILE *str,char *line)
{
    char *agentname;
    char *instances;
    char *inst;
    char *ptr;
    char regline[500];
    char cumlname[100];
    REGINFO regs[100];
    char temp[20];
    int rmax = 0;
    int idx;
    unsigned int cumlmask;
    int agentidx;

    agentname = gettoken(&line);
    instances = gettoken(&line);
    if (!instances) {
	strcpy(temp,"*");
	instances = temp;
	}

    fprintf(stderr,"Agent %s Instances %s\n",agentname,instances);

    if (agentcnt == MAXAGENTS) {
	fatal("Out of agent slots\n",NULL);
	}

    agentnames[agentcnt] = strdup(agentname);
    agentidx = agentcnt;
    agentcnt++;

    regline[0] = '\0';

    while ((readline(str,regline,sizeof(regline)) >= 0) && (rmax < 100)) {
	char *atext,*subinst,*pfunc,*descr;

	if (regline[0] == '!') break;

	ptr = regline;
	atext = gettoken(&ptr);
	subinst = gettoken(&ptr);
	pfunc = gettoken(&ptr);
	descr = gettoken(&ptr);

	if (!descr) {
	    fatal("Missing fields for ",atext);
	    }

	regs[rmax].reg_addr = strdup(atext);
	regs[rmax].reg_subinst = strdup(subinst);
	regs[rmax].reg_printfunc = strdup(pfunc);
	regs[rmax].reg_description = strdup(descr);
	regs[rmax].reg_mask = 0;
	rmax++;
	}

    if (rmax == 100) fatal("Too many registers in section ",agentname);

    inst = strtok(instances,",");

    cumlmask = 0;
    while (inst) {
	char defname[100];
	unsigned int curmask;

	sprintf(defname,"SOC_AGENT_%s%s",
		agentname,inst[0] == '*' ? "" : inst);

	curmask = newmask();
	cumlmask |= curmask;

	addconst(defname,curmask);

	for (idx = 0; idx < rmax; idx++) {
	    char descr[100];
	    char atext[200];

	    macroexpand(regs[idx].reg_addr,inst,atext);
	    strcpy(descr,regs[idx].reg_description);

	    addreg(agentname,
		   agentidx,
		   curmask,
		   atext,
		   inst,
		   regs[idx].reg_subinst,
		   regs[idx].reg_printfunc,
		   descr);
	    }
	inst = strtok(NULL,",");
	}

    if (instances[0] != '*') {
	sprintf(cumlname,"SOC_AGENT_%s",agentname);
	addconst(cumlname,cumlmask);
	}
}

static void docommand(FILE *str,char *line)
{
    char *cmd;

    cmd = gettoken(&line);
    if (!cmd) return;

    switch (assoc(commands,cmd)) {
	case CMD_AGENT:
	    doagentcmd(str,line);
	    break;
	default:
	    fatal("Invalid command",cmd);
	    break;
	}
    
}

static int ingestfile(FILE *str)
{
    char line[500];

    while (readline(str,line,sizeof(line)) >= 0) {
	if (line[0] == '!') {
	    docommand(str,line);
	    }
	else {
	    fatal("Command string required before register data",NULL);
	    }
	}
}


void saveincfile(char *fname)
{
    FILE *str;
    int idx;

    str = fopen(fname,"w" TEXTMODE);
    if (!str) {
	perror(fname);
	exit(1);
	}

    fprintf(str,"\n\n");

    fprintf(str,"#ifndef %s\n",constants[0].name);
    for (idx = 0; idx < constcnt; idx++) {
	fprintf(str,"#define %s 0x%08X\n",constants[idx].name,
		constants[idx].value);
	}
    fprintf(str,"#endif\n");

    fprintf(str,"\n\n");

    fprintf(str,"#ifdef _CFE_\n");
    fprintf(str,"#ifdef __ASSEMBLER__\n");
    fprintf(str,"\t.globl socstatetable\n");
    fprintf(str,"socstatetable:\n");
    for (idx = 0; idx < regcnt; idx++) {
	fprintf(str,"\t\t.word 0x%08X,%s\n",
		allregs[idx].reg_mask,allregs[idx].reg_addr);
	}
    fprintf(str,"\t\t.word 0,0\n");
    fprintf(str,"#endif\n");

    fprintf(str,"\n\n");
    fprintf(str,"#ifndef __ASSEMBLER__\n");

    /* Addr Agent Inst Subinst Mask Printfunc Descr */

    fprintf(str,"char *socagents[] = {\n");
    for (idx = 0; idx < agentcnt; idx++) {
	fprintf(str,"\t\"%s\",\n",agentnames[idx]);
	}
    fprintf(str,"\tNULL};\n\n");

    fprintf(str,"const socreg_t socregs[] = {\n");
    for (idx = 0; idx < regcnt; idx++) {
	fprintf(str,"   {%s,%d,\"%s\",\"%s\",\n     0x%08X,%s,\"%s\"},\n",
		allregs[idx].reg_addr,
		allregs[idx].reg_agentidx,
		allregs[idx].reg_inst,
		allregs[idx].reg_subinst,
		allregs[idx].reg_mask,
		allregs[idx].reg_printfunc,
		allregs[idx].reg_description);
	}
    fprintf(str,"   {0,0,NULL,NULL,0,NULL,NULL}};\n\n");

    fprintf(str,"#endif\n");
    fprintf(str,"#endif\n");
    fclose(str);
}

int main(int argc,char *argv[])
{
    FILE *str;
    int idx;

    if (argc != 3) {
	fprintf(stderr,"usage: makereg inputfile outputfile\n");
	exit(1);
	}

    str = fopen(argv[1],"r" TEXTMODE);

    if (!str) {
	perror(argv[1]);
	exit(1);
	}

    ingestfile(str);

    fclose(str);

    fprintf(stderr,"Total registers: %d\n",regcnt);

    saveincfile(argv[2]);

    exit(0);
    return 0;
}
