/* Specific command window processing.

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007
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

#include "defs.h"
#include <ctype.h>
#include "tui/tui.h"
#include "tui/tui-data.h"
#include "tui/tui-win.h"
#include "tui/tui-io.h"

#include "gdb_curses.h"
#include "gdb_string.h"


/*****************************************
** STATIC LOCAL FUNCTIONS FORWARD DECLS    **
******************************************/



/*****************************************
** PUBLIC FUNCTIONS                        **
******************************************/

/* Dispatch the correct tui function based upon the control
   character.  */
unsigned int
tui_dispatch_ctrl_char (unsigned int ch)
{
  struct tui_win_info *win_info = tui_win_with_focus ();

  /* Handle the CTRL-L refresh for each window.  */
  if (ch == '\f')
    tui_refresh_all_win ();

  /* If the command window has the logical focus, or no-one does
     assume it is the command window; in this case, pass the character
     on through and do nothing here.  */
  if (win_info == NULL || win_info == TUI_CMD_WIN)
    return ch;
  else
    {
      unsigned int c = 0, ch_copy = ch;
      int i;
      char *term;

      /* If this is an xterm, page next/prev keys aren't returned by
         keypad as a single char, so we must handle them here.  Seems
         like a bug in the curses library?  */
      term = (char *) getenv ("TERM");
      if (term)
	{
	  for (i = 0; term[i]; i++)
	    term[i] = toupper (term[i]);
	  if ((strcmp (term, "XTERM") == 0) 
	      && key_is_start_sequence (ch))
	    {
	      unsigned int page_ch = 0;
	      unsigned int tmp_char;
              WINDOW *w = TUI_CMD_WIN->generic.handle;

	      tmp_char = 0;
	      while (!key_is_end_sequence (tmp_char))
		{
		  tmp_char = (int) wgetch (w);
		  if (tmp_char == ERR)
		    {
		      return ch;
		    }
		  if (!tmp_char)
		    break;
		  if (tmp_char == 53)
		    page_ch = KEY_PPAGE;
		  else if (tmp_char == 54)
		    page_ch = KEY_NPAGE;
		  else
		    {
		      return 0;
		    }
		}
	      ch_copy = page_ch;
	    }
	}

      switch (ch_copy)
	{
	case KEY_NPAGE:
	  tui_scroll_forward (win_info, 0);
	  break;
	case KEY_PPAGE:
	  tui_scroll_backward (win_info, 0);
	  break;
	case KEY_DOWN:
	case KEY_SF:
	  tui_scroll_forward (win_info, 1);
	  break;
	case KEY_UP:
	case KEY_SR:
	  tui_scroll_backward (win_info, 1);
	  break;
	case KEY_RIGHT:
	  tui_scroll_left (win_info, 1);
	  break;
	case KEY_LEFT:
	  tui_scroll_right (win_info, 1);
	  break;
	case '\f':
          break;
	default:
	  c = ch_copy;
	  break;
	}
      return c;
    }
}
