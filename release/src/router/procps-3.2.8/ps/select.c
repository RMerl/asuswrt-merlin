/*
 * Copyright 1998-2002 by Albert Cahalan; all rights resered.         
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version  
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */                                 

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "../proc/readproc.h"
#include "../proc/procps.h"

//#define process_group_leader(p) ((p)->pgid    == (p)->tgid)
//#define some_other_user(p)      ((p)->euid    != cached_euid)
#define has_our_euid(p)         ((unsigned)(p)->euid    == (unsigned)cached_euid)
#define on_our_tty(p)           ((unsigned)(p)->tty == (unsigned)cached_tty)
#define running(p)              (((p)->state=='R')||((p)->state=='D'))
#define session_leader(p)       ((p)->session == (p)->tgid)
#define without_a_tty(p)        (!(p)->tty)

static unsigned long select_bits = 0;

/***** prepare select_bits for use */
const char *select_bits_setup(void){
  int switch_val = 0;
  /* don't want a 'g' screwing up simple_select */
  if(!simple_select && !prefer_bsd_defaults){
    select_bits = 0xaa00; /* the STANDARD selection */
    return NULL;
  }
  /* For every BSD but SunOS, the 'g' option is a NOP. (enabled by default) */
  if( !(personality & PER_NO_DEFAULT_g) && !(simple_select&(SS_U_a|SS_U_d)) )
    switch_val = simple_select|SS_B_g;
  else
    switch_val = simple_select;
  switch(switch_val){
  /* UNIX options */
  case SS_U_a | SS_U_d:           select_bits = 0x3f3f; break; /* 3333 or 3f3f */
  case SS_U_a:                    select_bits = 0x0303; break; /* 0303 or 0f0f */
  case SS_U_d:                    select_bits = 0x3333; break;
  /* SunOS 4 only (others have 'g' enabled all the time) */
  case 0:                         select_bits = 0x0202; break;
  case                   SS_B_a:  select_bits = 0x0303; break;
  case          SS_B_x         :  select_bits = 0x2222; break;
  case          SS_B_x | SS_B_a:  select_bits = 0x3333; break;
  /* General BSD options */
  case SS_B_g                  :  select_bits = 0x0a0a; break;
  case SS_B_g |          SS_B_a:  select_bits = 0x0f0f; break;
  case SS_B_g | SS_B_x         :  select_bits = 0xaaaa; break;
  case SS_B_g | SS_B_x | SS_B_a:  /* convert to -e instead of using 0xffff */
    all_processes = 1;
    simple_select = 0;
    break;
  default:
    return "Process selection options conflict.";
    break;
  }
  return NULL;
}

/***** selected by simple option? */
static int table_accept(proc_t *buf){
  unsigned proc_index;
  proc_index = (has_our_euid(buf)    <<0)
             | (session_leader(buf)  <<1)
             | (without_a_tty(buf)   <<2)
             | (on_our_tty(buf)      <<3);
  return (select_bits & (1<<proc_index));
}

/***** selected by some kind of list? */
static int proc_was_listed(proc_t *buf){
  selection_node *sn = selection_list;
  int i;
  if(!sn) return 0;
  while(sn){
    switch(sn->typecode){
    default:
      printf("Internal error in ps! Please report this bug.\n");

#define return_if_match(foo,bar) \
        i=sn->n; while(i--) \
        if((unsigned)(buf->foo) == (unsigned)(*(sn->u+i)).bar) \
        return 1

    break; case SEL_RUID: return_if_match(ruid,uid);
    break; case SEL_EUID: return_if_match(euid,uid);
    break; case SEL_SUID: return_if_match(suid,uid);
    break; case SEL_FUID: return_if_match(fuid,uid);

    break; case SEL_RGID: return_if_match(rgid,gid);
    break; case SEL_EGID: return_if_match(egid,gid);
    break; case SEL_SGID: return_if_match(sgid,gid);
    break; case SEL_FGID: return_if_match(fgid,gid);

    break; case SEL_PGRP: return_if_match(pgrp,pid);
    break; case SEL_PID : return_if_match(tgid,pid);
    break; case SEL_PPID: return_if_match(ppid,ppid);
    break; case SEL_TTY : return_if_match(tty,tty);
    break; case SEL_SESS: return_if_match(session,pid);

    break; case SEL_COMM: i=sn->n; while(i--)
    if(!strncmp( buf->cmd, (*(sn->u+i)).cmd, 15 )) return 1;



#undef return_if_match

    }
    sn = sn->next;
  }
  return 0;
}


/***** This must satisfy Unix98 and as much BSD as possible */
int want_this_proc(proc_t *buf){
  int accepted_proc = 1; /* assume success */
  /* elsewhere, convert T to list, U sets x implicitly */

  /* handle -e -A */
  if(all_processes) goto finish;

  /* use table for -a a d g x */
  if((simple_select || !selection_list))
    if(table_accept(buf)) goto finish;

  /* search lists */
  if(proc_was_listed(buf)) goto finish;

  /* fail, fall through to loose ends */
  accepted_proc = 0;

  /* do r N */
finish:
  if(running_only && !running(buf)) accepted_proc = 0;
  if(negate_selection) return !accepted_proc;
  return accepted_proc;
}
