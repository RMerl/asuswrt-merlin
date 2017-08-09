/* 
   Unix SMB/CIFS implementation.

   process model manager - structures

   Copyright (C) Andrew Tridgell 1992-2005
   Copyright (C) James J Myers 2003 <myersjj@samba.org>
   Copyright (C) Stefan (metze) Metzmacher 2004-2005
   
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

#ifndef __PROCESS_MODEL_H__
#define __PROCESS_MODEL_H__

#include "lib/socket/socket.h"
#include "smbd/service.h"
#include "smbd/process_model_proto.h"

/* modules can use the following to determine if the interface has changed
 * please increment the version number after each interface change
 * with a comment and maybe update struct process_model_critical_sizes.
 */
/* version 1 - initial version - metze */
#define PROCESS_MODEL_VERSION 1

/* the process model operations structure - contains function pointers to 
   the model-specific implementations of each operation */
struct model_ops {
	/* the name of the process_model */
	const char *name;

	/* called at startup when the model is selected */
	void (*model_init)(void);

	/* function to accept new connection */
	void (*accept_connection)(struct tevent_context *, 
				  struct loadparm_context *,
				  struct socket_context *, 
				  void (*)(struct tevent_context *, 
					   struct loadparm_context *,
					   struct socket_context *, 
					   struct server_id , void *), 
				  void *);

	/* function to create a task */
	void (*new_task)(struct tevent_context *, 
			 struct loadparm_context *lp_ctx,
			 const char *service_name,
			 void (*)(struct tevent_context *, 
				  struct loadparm_context *, struct server_id, 
				  void *),
			 void *);

	/* function to terminate a connection or task */
	void (*terminate)(struct tevent_context *, struct loadparm_context *lp_ctx,
			  const char *reason);

	/* function to set a title for the connection or task */
	void (*set_title)(struct tevent_context *, const char *title);
};

/* this structure is used by modules to determine the size of some critical types */
struct process_model_critical_sizes {
	int interface_version;
	int sizeof_model_ops;
};

extern const struct model_ops single_ops;

const struct model_ops *process_model_startup(const char *model);
NTSTATUS register_process_model(const struct model_ops *ops);
NTSTATUS process_model_init(struct loadparm_context *lp_ctx);

#endif /* __PROCESS_MODEL_H__ */
