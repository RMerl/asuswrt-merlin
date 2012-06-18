/* 
   samba -- Unix SMB/CIFS implementation.
   Copyright (C) 2002 by Martin Pool
   
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

#include "includes.h"

/**
 * @file dmallocmsg.c
 *
 * Glue code to cause dmalloc info to come out when we receive a prod
 * over samba messaging.
 **/

#ifdef ENABLE_DMALLOC
static unsigned long our_dm_mark = 0;
#endif /* ENABLE_DMALLOC */


/**
 * Respond to a POOL_USAGE message by sending back string form of memory
 * usage stats.
 **/
static void msg_req_dmalloc_mark(struct messaging_context *msg,
				 void *private_data,
				 uint32_t msg_type,
				 struct server_id server_id,
				 DATA_BLOB *data)
{
#ifdef ENABLE_DMALLOC
	our_dm_mark = dmalloc_mark();
	DEBUG(2,("Got MSG_REQ_DMALLOC_MARK: mark set\n"));
#else
	DEBUG(2,("Got MSG_REQ_DMALLOC_MARK but dmalloc not in this process\n"));
#endif
}



static void msg_req_dmalloc_log_changed(struct messaging_context *msg,
					void *private_data,
					uint32_t msg_type,
					struct server_id server_id,
					DATA_BLOB *data)
{
#ifdef ENABLE_DMALLOC
	dmalloc_log_changed(our_dm_mark, True, True, True);
	DEBUG(2,("Got MSG_REQ_DMALLOC_LOG_CHANGED: done\n"));
#else
	DEBUG(2,("Got MSG_REQ_DMALLOC_LOG_CHANGED but dmalloc not in this process\n"));
#endif
}


/**
 * Register handler for MSG_REQ_POOL_USAGE
 **/
void register_dmalloc_msgs(struct messaging_context *msg_ctx)
{
	messaging_register(msg_ctx, NULL, MSG_REQ_DMALLOC_MARK,
			   msg_req_dmalloc_mark);
	messaging_register(msg_ctx, NULL, MSG_REQ_DMALLOC_LOG_CHANGED,
			   msg_req_dmalloc_log_changed);
	DEBUG(2, ("Registered MSG_REQ_DMALLOC_MARK and LOG_CHANGED\n"));
}	
