#ifndef _system_readline_h
#define _system_readline_h
/* 
   Unix SMB/CIFS implementation.

   readline wrappers
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_LIBREADLINE
#  ifdef HAVE_READLINE_READLINE_H
#    include <readline/readline.h>
#    ifdef HAVE_READLINE_HISTORY_H
#      include <readline/history.h>
#    endif
#  else
#    ifdef HAVE_READLINE_H
#      include <readline.h>
#      ifdef HAVE_HISTORY_H
#        include <history.h>
#      endif
#    else
#      undef HAVE_LIBREADLINE
#    endif
#  endif
#endif

#ifdef HAVE_NEW_LIBREADLINE
#  define RL_COMPLETION_CAST (rl_completion_func_t *)
#else
/* This type is missing from libreadline<4.0  (approximately) */
#  define RL_COMPLETION_CAST
#endif /* HAVE_NEW_LIBREADLINE */

#endif
