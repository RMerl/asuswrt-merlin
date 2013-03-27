/* Generic serial interface functions.

   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

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

#ifndef SER_BASE_H
#define SER_BASE_H

struct serial;
struct ui_file;

extern int generic_readchar (struct serial *scb, int timeout,
			     int (*do_readchar) (struct serial *scb,
						 int timeout));
extern void reschedule (struct serial *scb);
extern int ser_base_flush_output (struct serial *scb);
extern int ser_base_flush_input (struct serial *scb);
extern int ser_base_send_break (struct serial *scb);
extern void ser_base_raw (struct serial *scb);
extern serial_ttystate ser_base_get_tty_state (struct serial *scb);
extern int ser_base_set_tty_state (struct serial *scb,
				   serial_ttystate ttystate);
extern void ser_base_print_tty_state (struct serial *scb,
				      serial_ttystate ttystate,
				      struct ui_file *stream);
extern int ser_base_noflush_set_tty_state (struct serial *scb,
					   serial_ttystate new_ttystate,
					   serial_ttystate old_ttystate);
extern int ser_base_setbaudrate (struct serial *scb, int rate);
extern int ser_base_setstopbits (struct serial *scb, int rate);
extern int ser_base_drain_output (struct serial *scb);

extern int ser_base_write (struct serial *scb, const char *str, int len);

extern void ser_base_async (struct serial *scb, int async_p);
extern int ser_base_readchar (struct serial *scb, int timeout);

#endif
