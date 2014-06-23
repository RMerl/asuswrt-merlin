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

/* "Times" API : bridgeTime, rootTimes, portTimes, designatedTimes, msgTimes */
 
#include "base.h"

int
STP_compare_times (IN TIMEVALUES_T *t1, IN TIMEVALUES_T *t2)
{
  if (t1->MessageAge < t2->MessageAge)     return -1;
  if (t1->MessageAge > t2->MessageAge)     return  1;

  if (t1->MaxAge < t2->MaxAge)             return -2;
  if (t1->MaxAge > t2->MaxAge)             return  2;

  if (t1->ForwardDelay < t2->ForwardDelay) return -3;
  if (t1->ForwardDelay > t2->ForwardDelay) return  3;

  if (t1->HelloTime < t2->HelloTime)       return -4;
  if (t1->HelloTime > t2->HelloTime)       return  4;

  return 0;
}

void
STP_get_times (IN BPDU_BODY_T *b, OUT TIMEVALUES_T *v)
{
  v->MessageAge =   ntohs (*((unsigned short*) b->message_age))   >> 8;
  v->MaxAge =       ntohs (*((unsigned short*) b->max_age))       >> 8;
  v->ForwardDelay = ntohs (*((unsigned short*) b->forward_delay)) >> 8;
  v->HelloTime =    ntohs (*((unsigned short*) b->hello_time))    >> 8;
}

void
STP_set_times (IN TIMEVALUES_T *v, OUT BPDU_BODY_T *b)
{
  unsigned short mt;
  #define STP_SET_TIME(f, t)        \
     mt = htons (f << 8);           \
     memcpy (t, &mt, 2); 
  
  STP_SET_TIME(v->MessageAge,   b->message_age);
  STP_SET_TIME(v->MaxAge,       b->max_age);
  STP_SET_TIME(v->ForwardDelay, b->forward_delay);
  STP_SET_TIME(v->HelloTime,    b->hello_time);
}

void 
STP_copy_times (OUT TIMEVALUES_T *t, IN TIMEVALUES_T *f)
{
  t->MessageAge = f->MessageAge;
  t->MaxAge = f->MaxAge;
  t->ForwardDelay = f->ForwardDelay;
  t->HelloTime = f->HelloTime;
}

