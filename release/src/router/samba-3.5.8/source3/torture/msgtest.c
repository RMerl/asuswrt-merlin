/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Andrew Tridgell 2000
   
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

/*
  test code for internal messaging
 */

#include "includes.h"

static int pong_count;


/****************************************************************************
a useful function for testing the message system
****************************************************************************/
static void pong_message(struct messaging_context *msg_ctx,
			 void *private_data, 
			 uint32_t msg_type, 
			 struct server_id pid,
			 DATA_BLOB *data)
{
	pong_count++;
}

 int main(int argc, char *argv[])
{
	struct tevent_context *evt_ctx;
	struct messaging_context *msg_ctx;
	pid_t pid;
	int i, n;
	char buf[12];
	int ret;

	load_case_tables();

	setup_logging(argv[0],True);
	
	lp_load(get_dyn_CONFIGFILE(),False,False,False,True);

	if (!(evt_ctx = tevent_context_init(NULL)) ||
	    !(msg_ctx = messaging_init(NULL, server_id_self(), evt_ctx))) {
		fprintf(stderr, "could not init messaging context\n");
		exit(1);
	}
	
	if (argc != 3) {
		fprintf(stderr, "%s: Usage - %s pid count\n", argv[0],
			argv[0]);
		exit(1);
	}

	pid = atoi(argv[1]);
	n = atoi(argv[2]);

	messaging_register(msg_ctx, NULL, MSG_PONG, pong_message);

	for (i=0;i<n;i++) {
		messaging_send(msg_ctx, pid_to_procid(pid), MSG_PING,
			       &data_blob_null);
	}

	while (pong_count < i) {
		ret = tevent_loop_once(evt_ctx);
		if (ret != 0) {
			break;
		}
	}

	/* Now test that the duplicate filtering code works. */
	pong_count = 0;

	safe_strcpy(buf, "1234567890", sizeof(buf)-1);

	for (i=0;i<n;i++) {
		messaging_send(msg_ctx, pid_to_procid(getpid()), MSG_PING,
			       &data_blob_null);
		messaging_send_buf(msg_ctx, pid_to_procid(getpid()), MSG_PING,
				   (uint8 *)buf, 11);
	}

	for (i=0;i<n;i++) {
		ret = tevent_loop_once(evt_ctx);
		if (ret != 0) {
			break;
		}
	}

	if (pong_count != 2) {
		fprintf(stderr, "Duplicate filter failed (%d).\n", pong_count);
	}

	/* Speed testing */

	pong_count = 0;

	{
		struct timeval tv = timeval_current();
		size_t timelimit = n;
		size_t ping_count = 0;

		printf("Sending pings for %d seconds\n", (int)timelimit);
		while (timeval_elapsed(&tv) < timelimit) {		
			if(NT_STATUS_IS_OK(messaging_send_buf(
						   msg_ctx, pid_to_procid(pid),
						   MSG_PING,
						   (uint8 *)buf, 11)))
			   ping_count++;
			if(NT_STATUS_IS_OK(messaging_send(
						   msg_ctx, pid_to_procid(pid),
						   MSG_PING, &data_blob_null)))
			   ping_count++;

			while (ping_count > pong_count + 20) {
				ret = tevent_loop_once(evt_ctx);
				if (ret != 0) {
					break;
				}
			}
		}
		
		printf("waiting for %d remaining replies (done %d)\n", 
		       (int)(ping_count - pong_count), pong_count);
		while (timeval_elapsed(&tv) < 30 && pong_count < ping_count) {
			ret = tevent_loop_once(evt_ctx);
			if (ret != 0) {
				break;
			}
		}
		
		if (ping_count != pong_count) {
			fprintf(stderr, "ping test failed! received %d, sent "
				"%d\n", pong_count, (int)ping_count);
		}
		
		printf("ping rate of %.0f messages/sec\n", 
		       (ping_count+pong_count)/timeval_elapsed(&tv));
	}

	return (0);
}

