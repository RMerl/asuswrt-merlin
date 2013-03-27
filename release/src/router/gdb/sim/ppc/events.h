/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _EVENTS_H_
#define _EVENTS_H_

/* typedef struct _event_queue event_queue; */
/* typedef struct _event_entry_tag *event_entry_tag; */

typedef void event_handler(void *data);

INLINE_EVENTS\
(event_queue *) event_queue_create
(void);

INLINE_EVENTS\
(void) event_queue_init
(event_queue *queue);


/* (de)Schedule things to happen in the future. */

INLINE_EVENTS\
(event_entry_tag) event_queue_schedule
(event_queue *queue,
 signed64 delta_time,
 event_handler *handler,
 void *data);

INLINE_EVENTS\
(event_entry_tag) event_queue_schedule_after_signal
(event_queue *queue,
 signed64 delta_time,
 event_handler *handler,
 void *data);

INLINE_EVENTS\
(void) event_queue_deschedule
(event_queue *queue,
 event_entry_tag event_to_remove);


/* progress time.  In to parts so that if something is pending, the
   caller has a chance to save any cached state */

INLINE_EVENTS\
(int) event_queue_tick
(event_queue *queue);

INLINE_EVENTS\
(void) event_queue_process
(event_queue *events);


/* local concept of time */

INLINE_EVENTS\
(signed64) event_queue_time
(event_queue *queue);

#endif /* _EVENTS_H_ */
