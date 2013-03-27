/* Code dealing with dummy stack frames, for GDB, the GNU debugger.

   Copyright (C) 2002, 2004, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

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

#if !defined (DUMMY_FRAME_H)
#define DUMMY_FRAME_H 1

struct frame_info;
struct regcache;
struct frame_unwind;
struct frame_id;

/* Push the information needed to identify, and unwind from, a dummy
   frame onto the dummy frame stack.  */

/* NOTE: cagney/2004-08-02: This interface will eventually need to be
   parameterized with the caller's thread - that will allow per-thread
   dummy-frame stacks and, hence, per-thread inferior function
   calls.  */

/* NOTE: cagney/2004-08-02: In the case of ABIs using push_dummy_code
   containing more than one instruction, this interface many need to
   be expanded so that it knowns the lower/upper extent of the dummy
   frame's code.  */

extern void dummy_frame_push (struct regcache *regcache,
			      const struct frame_id *dummy_id);

/* If the PC falls in a dummy frame, return a dummy frame
   unwinder.  */

extern const struct frame_unwind *const dummy_frame_unwind;

#endif /* !defined (DUMMY_FRAME_H)  */
