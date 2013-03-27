/* Hardware event manager.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef HW_EVENTS_H
#define HW_EVENTS_H

/* Event manager customized for hardware models.

   This interface is discussed further in sim-events.h. */

struct hw_event;
typedef void (hw_event_callback) (struct hw *me, void *data);

struct hw_event *hw_event_queue_schedule
(struct hw *me,
 signed64 delta_time,
 hw_event_callback *handler,
 void *data);

struct hw_event *hw_event_queue_schedule_tracef
(struct hw *me,
 signed64 delta_time,
 hw_event_callback *handler,
 void *data,
 const char *fmt,
 ...) __attribute__ ((format (printf, 5, 6)));

struct hw_event *hw_event_queue_schedule_vtracef
(struct hw *me,
 signed64 delta_time,
 hw_event_callback *handler,
 void *data,
 const char *fmt,
 va_list ap);


void hw_event_queue_deschedule
(struct hw *me,
 struct hw_event *event_to_remove);

signed64 hw_event_queue_time
(struct hw *me);

/* Returns the time that remains before the event is raised. */
signed64 hw_event_remain_time
(struct hw *me, struct hw_event *event);

#endif
