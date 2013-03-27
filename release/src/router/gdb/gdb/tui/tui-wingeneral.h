/* General window behavior.

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004, 2007
   Free Software Foundation, Inc.

   Contributed by Hewlett-Packard Company.

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

#ifndef TUI_WINGENERAL_H
#define TUI_WINGENERAL_H

struct tui_win_info;
struct tui_gen_win_info;

extern void tui_unhighlight_win (struct tui_win_info *);
extern void tui_make_visible (struct tui_gen_win_info *);
extern void tui_make_invisible (struct tui_gen_win_info *);
extern void tui_make_all_visible (void);
extern void tui_make_all_invisible (void);
extern void tui_make_window (struct tui_gen_win_info *, int);
extern struct tui_win_info *tui_copy_win (struct tui_win_info *);
extern void tui_box_win (struct tui_gen_win_info *, int);
extern void tui_highlight_win (struct tui_win_info *);
extern void tui_check_and_display_highlight_if_needed (struct tui_win_info *);
extern void tui_refresh_all (struct tui_win_info **);
extern void tui_delete_win (WINDOW *window);
extern void tui_refresh_win (struct tui_gen_win_info *);

#endif
