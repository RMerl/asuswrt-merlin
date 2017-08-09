/*
   Unix SMB/CIFS implementation.

   common events code for immediate events

   Copyright (C) Stefan Metzmacher 2009

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
#include "tevent_util.h"

static void tevent_common_immediate_cancel(struct tevent_immediate *im)
{
	if (!im->event_ctx) {
		return;
	}

	tevent_debug(im->event_ctx, TEVENT_DEBUG_TRACE,
		     "Cancel immediate event %p \"%s\"\n",
		     im, im->handler_name);

	/* let the backend free im->additional_data */
	if (im->cancel_fn) {
		im->cancel_fn(im);
	}

	DLIST_REMOVE(im->event_ctx->immediate_events, im);
	im->event_ctx		= NULL;
	im->handler		= NULL;
	im->private_data	= NULL;
	im->handler_name	= NULL;
	im->schedule_location	= NULL;
	im->cancel_fn		= NULL;
	im->additional_data	= NULL;

	talloc_set_destructor(im, NULL);
}

/*
  destroy an immediate event
*/
static int tevent_common_immediate_destructor(struct tevent_immediate *im)
{
	tevent_common_immediate_cancel(im);
	return 0;
}

/*
 * schedule an immediate event on
 */
void tevent_common_schedule_immediate(struct tevent_immediate *im,
				      struct tevent_context *ev,
				      tevent_immediate_handler_t handler,
				      void *private_data,
				      const char *handler_name,
				      const char *location)
{
	tevent_common_immediate_cancel(im);

	if (!handler) {
		return;
	}

	im->event_ctx		= ev;
	im->handler		= handler;
	im->private_data	= private_data;
	im->handler_name	= handler_name;
	im->schedule_location	= location;
	im->cancel_fn		= NULL;
	im->additional_data	= NULL;

	DLIST_ADD_END(ev->immediate_events, im, struct tevent_immediate *);
	talloc_set_destructor(im, tevent_common_immediate_destructor);

	tevent_debug(ev, TEVENT_DEBUG_TRACE,
		     "Schedule immediate event \"%s\": %p\n",
		     handler_name, im);
}

/*
  trigger the first immediate event and return true
  if no event was triggered return false
*/
bool tevent_common_loop_immediate(struct tevent_context *ev)
{
	struct tevent_immediate *im = ev->immediate_events;
	tevent_immediate_handler_t handler;
	void *private_data;

	if (!im) {
		return false;
	}

	tevent_debug(ev, TEVENT_DEBUG_TRACE,
		     "Run immediate event \"%s\": %p\n",
		     im->handler_name, im);

	/*
	 * remember the handler and then clear the event
	 * the handler might reschedule the event
	 */
	handler = im->handler;
	private_data = im->private_data;

	DLIST_REMOVE(im->event_ctx->immediate_events, im);
	im->event_ctx		= NULL;
	im->handler		= NULL;
	im->private_data	= NULL;
	im->handler_name	= NULL;
	im->schedule_location	= NULL;
	im->cancel_fn		= NULL;
	im->additional_data	= NULL;

	talloc_set_destructor(im, NULL);

	handler(ev, im, private_data);

	return true;
}

