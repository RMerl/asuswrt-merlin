/*
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _BRCTL_H
#define _BRCTL_H

struct command
{
	int		nargs;
	const char	*name;
	int		(*func)(int argc, char *const* argv);
	const char 	*help;
};

const struct command *command_lookup(const char *cmd);
void command_help(const struct command *);
void command_helpall(void);

void br_dump_bridge_id(const unsigned char *x);
void br_show_timer(const struct timeval *tv);
void br_dump_interface_list(const char *br);
void br_dump_info(const char *br, const struct bridge_info *bri);

#endif
