/* Hardware memory allocator.
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


#ifndef HW_ALLOC_H
#define HW_ALLOC_H

/* Mechanism for associating memory allocated by a device to that
   device.

   When a device is deleted any remaining memory regions associated to
   it are reclaimed.

   FIXME: Perhaphs this can be generalized. Perhaphs it should not
   be. */


#define HW_ZALLOC(me,type) (type*) hw_zalloc (me, sizeof (type))
#define HW_MALLOC(me,type) (type*) hw_malloc (me, sizeof (type))
#define HW_NZALLOC(ME,TYPE,N) (TYPE*) hw_zalloc (me, sizeof (TYPE) * (N))

extern void *hw_zalloc (struct hw *me, unsigned long size);
extern void *hw_malloc (struct hw *me, unsigned long size);

extern void hw_free (struct hw *me, void *);


/* Duplicate a string allocating memory using the per-device heap */

extern char *hw_strdup (struct hw *me, const char *str);

#endif
