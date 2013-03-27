/* TUI layout window management.

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

#ifndef TUI_LAYOUT_H
#define TUI_LAYOUT_H

#include "tui/tui.h"
#include "tui/tui-data.h"

extern void tui_add_win_to_layout (enum tui_win_type);
extern int tui_default_win_height (enum tui_win_type, 
				   enum tui_layout_type);
extern int tui_default_win_viewport_height (enum tui_win_type,
					    enum tui_layout_type);
extern enum tui_status tui_set_layout (enum tui_layout_type,
				       enum tui_register_display_type);

#endif /*TUI_LAYOUT_H */
