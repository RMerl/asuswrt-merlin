/* Portable <curses.h>.

   Copyright (C) 2004, 2006, 2007 Free Software Foundation, Inc.

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

#ifndef GDB_CURSES_H
#define GDB_CURSES_H 1

#if defined (HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined (HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined (HAVE_CURSESX_H)
#include <cursesX.h>
#elif defined (HAVE_CURSES_H)
#include <curses.h>
#endif

#if defined (HAVE_NCURSES_TERM_H)
#include <ncurses/term.h>
#elif defined (HAVE_TERM_H)
#include <term.h>
#else
/* On MinGW, a real termcap library is usually not present.  Stub versions
   of the termcap functions will be built from win32-termcap.c.  Readline
   provides its own extern declarations when there's no termcap.h; do the
   same here for the termcap functions used in GDB.  */
extern int tgetnum (const char *);
#endif

#endif /* gdb_curses.h */
