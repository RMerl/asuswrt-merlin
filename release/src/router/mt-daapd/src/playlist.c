/*
 * $Id: playlist.c,v 1.1 2009-06-30 02:31:09 steven Exp $
 * iTunes-style smart playlists
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "db-memory.h"
#include "err.h"
#include "mp3-scanner.h"
#include "playlist.h"
#include "parser.h"

/* Externs */
extern int yyparse(void *);

/* Globals */
SMART_PLAYLIST pl_smart = { NULL, 0, NULL, NULL };
int pl_error=0;

/* Forwards */
void pl_dump(void);
void pl_dump_node(PL_NODE *pnode, int indent);
int pl_load(char *file);
int pl_eval_node(MP3FILE *pmp3, PL_NODE *pnode);

extern FILE *yyin;

/*
 * pl_dump
 *
 * Dump the playlist list for debugging
 */
void pl_dump(void) {
    SMART_PLAYLIST *pcurrent=pl_smart.next;

    while(pcurrent) {
	printf("Playlist %s:\n",pcurrent->name);
	pl_dump_node(pcurrent->root,1);
	pcurrent=pcurrent->next;
    }
}

/*
 * pl_dump_node
 *
 * recursively dump a node
 */
void pl_dump_node(PL_NODE *pnode, int indent) {
    int index;
    int not=0;
    unsigned int boolarg;
    char datebuffer[40];
    struct tm *ptm;

    for(index=0;index<indent;index++) {
	printf(" ");
    }

    if(pnode->op == AND) {
	printf("AND\n");
    } else if (pnode->op == OR) {
	printf("OR\n");
    }

    if((pnode->op == AND) || (pnode->op == OR)) {
	pl_dump_node(pnode->arg1.plval,indent+1);
	pl_dump_node(pnode->arg2.plval,indent+1);
	return;
    }

    switch(pnode->arg1.ival) {
    case ARTIST:
	printf("ARTIST ");
	break;
    case ALBUM:
	printf("ALBUM ");
	break;
    case GENRE:
	printf("GENRE ");
	break;
    case PATH:
	printf("PATH ");
	break;
    case COMPOSER:
	printf("COMPOSER ");
	break;
    case ORCHESTRA:
	printf("ORCHESTRA ");
	break;
    case CONDUCTOR:
	printf("CONDUCTOR ");
	break;
    case GROUPING:
	printf("GROUPING ");
	break;
    case TYPE:
	printf("TYPE ");
	break;
    case COMMENT:
	printf("COMMENT ");
	break;
    case YEAR:
	printf("YEAR ");
	break;
    case BPM:
	printf("BPM ");
	break;
    case BITRATE:
	printf("BITRATE ");
	break;
    case DATEADDED:
	printf("DATE ");
	break;
    default:
	printf ("<unknown tag> ");
	break;
    }

    boolarg=(pnode->op) & 0x7FFFFFFF;
    if(pnode->op & 0x80000000)
	not=1;

    switch(boolarg) {
    case IS:
	printf("%s",not? "IS NOT " : "IS ");
	break;
    case INCLUDES:
	printf("%s",not? "DOES NOT INCLUDE " : "INCLUDES ");
	break;
    case EQUALS:
	printf("EQUALS ");
	break;
    case LESS:
	printf("< ");
	break;
    case LESSEQUAL:
	printf("<= ");
	break;
    case GREATER:
	printf("> ");
	break;
    case GREATEREQUAL:
	printf(">= ");
	break;
    case BEFORE:
	printf("BEFORE ");
	break;
    case AFTER:
	printf("AFTER ");
	break;
    default:
	printf("<unknown boolop> ");
	break;
    }

    switch(pnode->type) {
    case T_STR:
	printf("%s\n",pnode->arg2.cval);
	break;
    case T_INT:
	printf("%d\n",pnode->arg2.ival);
	break;
    case T_DATE:
	ptm=localtime((time_t*)&pnode->arg2.ival);
	strftime(datebuffer,sizeof(datebuffer),"%Y-%m-%d",ptm);
	printf("%s\n",datebuffer);
	break;
    default:
	printf("<unknown type>\n");
	break;
    }
    return;
}

/*
 * pl_load
 *
 * Load a smart playlist
 */
int pl_load(char *file) {
    FILE *fin;
    int result;

    fin=fopen(file,"r");
    if(!fin) {
	return -1;
    }

    yyin=fin;
    result=yyparse(NULL);
    fclose(fin);

    if(pl_error) {
	return -1;
    }

    return 0;
}

/*
 * pl_register
 *
 * Register the playlists
 */
void pl_register(void) {
    SMART_PLAYLIST *pcurrent;

    /* register the playlists */
    DPRINTF(E_INF,L_PL,"Finished loading smart playlists\n");
    pcurrent=pl_smart.next;
    while(pcurrent) {
	DPRINTF(E_INF,L_PL,"Adding smart playlist %s as %d\n",pcurrent->name,pcurrent->id)
	    db_add_playlist(pcurrent->id, pcurrent->name,0,1);
	pcurrent=pcurrent->next;
    }
}

/*
 * pl_eval
 *
 * Run each MP3 file through the smart playlists
 */
void pl_eval(MP3FILE *pmp3) {
    SMART_PLAYLIST *pcurrent;

    pcurrent=pl_smart.next;
    while(pcurrent) {
	if(pl_eval_node(pmp3,pcurrent->root)) {
	    DPRINTF(E_DBG,L_PL,"Match song to playlist %s (%d)\n",pcurrent->name,pcurrent->id);
	    db_add_playlist_song(pcurrent->id, pmp3->id);
	}

	pcurrent=pcurrent->next;
    }
}


/*
 * pl_eval_node
 *
 * Test node status
 */
int pl_eval_node(MP3FILE *pmp3, PL_NODE *pnode) {
    int r_arg,r_arg2;
    char *cval=NULL;
    int ival=0;
    int boolarg;
    int not=0;
    int retval=0;

    r_arg=r_arg2=0;

    if((pnode->op == AND) || (pnode->op == OR)) {
	r_arg=pl_eval_node(pmp3,pnode->arg1.plval);
	if((pnode->op == AND) && !r_arg)
	    return 0;
	if((pnode->op == OR) && r_arg)
	    return 1;

	r_arg2=pl_eval_node(pmp3,pnode->arg2.plval);
	if(pnode->op == AND)
	   return r_arg && r_arg2;

	return r_arg || r_arg2;
    }

    /* Not an AND/OR node, so let's eval */
    switch(pnode->arg1.ival) {
    case ALBUM:
	cval=pmp3->album;
	break;
    case ARTIST:
	cval=pmp3->artist;
	break;
    case GENRE:
	cval=pmp3->genre;
	break;
    case PATH:
	cval=pmp3->path;
	break;
    case COMPOSER:
	cval=pmp3->composer;
	break;
    case ORCHESTRA:
	cval=pmp3->orchestra;
	break;
    case CONDUCTOR:
	cval=pmp3->conductor;
	break;
    case GROUPING:
	cval=pmp3->grouping;
	break;
    case TYPE:
	cval=pmp3->description;
	break;
    case COMMENT:
	cval=pmp3->comment;
	break;
    case YEAR:
	ival=pmp3->year;
	break;
    case BPM:
	ival=pmp3->bpm;
	break;
    case BITRATE:
        ival=pmp3->bitrate;
	break;
    case DATEADDED:
	ival=pmp3->time_added;
	break;
    default:
	DPRINTF(E_FATAL,L_PL,"Unknown token in playlist.  This can't happen!\n\n");
	break;
    }

    boolarg=(pnode->op) & 0x7FFFFFFF;
    if(pnode->op & 0x80000000)
	not=1;

    if(pnode->type==T_STR) {
        if(!cval)
	    cval = "";

	DPRINTF(E_DBG,L_PL,"Matching %s to %s\n",cval,pnode->arg2.cval);

	switch(boolarg) {
	case IS:
	    r_arg=strcasecmp(cval,pnode->arg2.cval);
	    retval = not ? r_arg : !r_arg;
	    break;
	case INCLUDES:
	    r_arg=(int)strcasestr(cval,pnode->arg2.cval);
	    retval = not ? !r_arg : r_arg;
	    break;
	}
    }

    if(pnode->type==T_DATE) {
	DPRINTF(E_DBG,L_PL,"Comparing (datewise) %d to %d\n",ival,pnode->arg2.ival);
	switch(boolarg) {
	case BEFORE:
	    r_arg=(ival < pnode->arg2.ival);
	    break;
	case AFTER:
	    r_arg=(ival > pnode->arg2.ival);
	    break;
	}
	retval=r_arg;
    }

    if(pnode->type==T_INT) {
	DPRINTF(E_DBG,L_PL,"Comparing %d to %d\n",ival,pnode->arg2.ival);

	switch(boolarg) {
	case EQUALS:
	    r_arg=(ival == pnode->arg2.ival);
	    break;
	case GREATER:
	    r_arg=(ival > pnode->arg2.ival);
	    break;
	case GREATEREQUAL:
	    r_arg=(ival >= pnode->arg2.ival);
	    break;
	case LESS:
	    r_arg=(ival < pnode->arg2.ival);
	    break;
	case LESSEQUAL:
	    r_arg=(ival <= pnode->arg2.ival);
	    break;
	}
	retval = not? !r_arg : r_arg;
    }

    DPRINTF(E_DBG,L_PL,"Returning %d\n",retval);
    return retval;
}



