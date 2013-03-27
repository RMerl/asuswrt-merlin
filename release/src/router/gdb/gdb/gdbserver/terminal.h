/* Terminal interface definitions for the GDB remote server.
   Copyright (C) 2002, Free Software Foundation, Inc.

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

#if !defined (TERMINAL_H)
#define TERMINAL_H 1

/* Autoconf will have defined HAVE_TERMIOS_H, HAVE_TERMIO_H,
   and HAVE_SGTTY_H for us as appropriate.  */

#if defined(HAVE_TERMIOS_H)
#define HAVE_TERMIOS
#include <termios.h>
#else /* ! HAVE_TERMIOS_H */
#if defined(HAVE_TERMIO_H)
#define HAVE_TERMIO
#include <termio.h>

#undef TIOCGETP
#define TIOCGETP TCGETA
#undef TIOCSETN
#define TIOCSETN TCSETA
#undef TIOCSETP
#define TIOCSETP TCSETAF
#define TERMINAL struct termio
#else /* ! HAVE_TERMIO_H */
#ifdef HAVE_SGTTY_H
#define HAVE_SGTTY
#include <fcntl.h>
#include <sgtty.h>
#include <sys/ioctl.h>
#define TERMINAL struct sgttyb
#endif
#endif
#endif

#endif /* !defined (TERMINAL_H) */
