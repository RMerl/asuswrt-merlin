/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Jelmer Vernooij 2005

     ** NOTE! The following LGPL license applies to the tevent
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "replace.h"
#include "tevent.h"
#include "tevent_internal.h"

/********************************************************************
 * Debug wrapper functions, modeled (with lot's of code copied as is)
 * after the ev debug wrapper functions
 ********************************************************************/

/*
  this allows the user to choose their own debug function
*/
int tevent_set_debug(struct tevent_context *ev,
		     void (*debug)(void *context,
				   enum tevent_debug_level level,
				   const char *fmt,
				   va_list ap) PRINTF_ATTRIBUTE(3,0),
		     void *context)
{
	ev->debug_ops.debug = debug;
	ev->debug_ops.context = context;
	return 0;
}

/*
  debug function for ev_set_debug_stderr
*/
static void tevent_debug_stderr(void *private_data,
				enum tevent_debug_level level,
				const char *fmt,
				va_list ap) PRINTF_ATTRIBUTE(3,0);
static void tevent_debug_stderr(void *private_data,
				enum tevent_debug_level level,
				const char *fmt, va_list ap)
{
	if (level <= TEVENT_DEBUG_WARNING) {
		vfprintf(stderr, fmt, ap);
	}
}

/*
  convenience function to setup debug messages on stderr
  messages of level TEVENT_DEBUG_WARNING and higher are printed
*/
int tevent_set_debug_stderr(struct tevent_context *ev)
{
	return tevent_set_debug(ev, tevent_debug_stderr, ev);
}

/*
 * log a message
 *
 * The default debug action is to ignore debugging messages.
 * This is the most appropriate action for a library.
 * Applications using the library must decide where to
 * redirect debugging messages
*/
void tevent_debug(struct tevent_context *ev, enum tevent_debug_level level,
		  const char *fmt, ...)
{
	va_list ap;
	if (!ev) {
		return;
	}
	if (ev->debug_ops.debug == NULL) {
		return;
	}
	va_start(ap, fmt);
	ev->debug_ops.debug(ev->debug_ops.context, level, fmt, ap);
	va_end(ap);
}

