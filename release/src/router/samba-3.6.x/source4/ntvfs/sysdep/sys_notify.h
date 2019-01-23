/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "librpc/gen_ndr/s4_notify.h"
#include "param/share.h"

struct sys_notify_context;

typedef void (*sys_notify_callback_t)(struct sys_notify_context *, 
				      void *, struct notify_event *ev);

typedef NTSTATUS (*notify_watch_t)(struct sys_notify_context *ctx, 
				   struct notify_entry *e,
				   sys_notify_callback_t callback,
				   void *private_data, 
				   void *handle_p);

struct sys_notify_context {
	struct tevent_context *ev;
	void *private_data; /* for use of backend */
	const char *name;
	notify_watch_t notify_watch;
};

struct sys_notify_backend {
	const char *name;
	notify_watch_t notify_watch;
};

NTSTATUS sys_notify_register(struct sys_notify_backend *backend);
struct sys_notify_context *sys_notify_context_create(struct share_config *scfg,
						     TALLOC_CTX *mem_ctx, 
						     struct tevent_context *ev);
NTSTATUS sys_notify_watch(struct sys_notify_context *ctx, struct notify_entry *e,
			  sys_notify_callback_t callback, void *private_data, 
			  void *handle);
NTSTATUS sys_notify_init(void);
