/* 
   Unix SMB/CIFS implementation.
   Copyright (C) Andrew Tridgell 2000
   
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

/*
  test code for internal messaging
 */

#include "includes.h"

static int pong_count;


/****************************************************************************
a useful function for testing the message system
****************************************************************************/
static void pong_message(int msg_type, struct process_id src,
			 void *buf, size_t len, void *private_data)
{
	pong_count++;
}

 int main(int argc, char *argv[])
{
	pid_t pid;
	int i, n;
	char buf[12];

	load_case_tables();

	setup_logging(argv[0],True);
	
	lp_load(dyn_CONFIGFILE,False,False,False,True);

	message_init();

	if (argc != 3) {
		fprintf(stderr, "%s: Usage - %s pid count\n", argv[0],
			argv[0]);
		exit(1);
	}

	pid = atoi(argv[1]);
	n = atoi(argv[2]);

	message_register(MSG_PONG, pong_message, NULL);

	for (i=0;i<n;i++) {
		message_send_pid(pid_to_procid(pid), MSG_PING, NULL, 0, True);
	}

	while (pong_count < i) {
		message_dispatch();
		smb_msleep(1);
	}

	/* Now test that the duplicate filtering code works. */
	pong_count = 0;

	safe_strcpy(buf, "1234567890", sizeof(buf)-1);

	for (i=0;i<n;i++) {
		message_send_pid(pid_to_procid(getpid()), MSG_PING,
				 NULL, 0, False);
		message_send_pid(pid_to_procid(getpid()), MSG_PING,
				 buf, 11, False);
	}

	for (i=0;i<n;i++) {
		message_dispatch();
		smb_msleep(1);
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
			if(NT_STATUS_IS_OK(message_send_pid(pid_to_procid(pid),
							    MSG_PING,
							    buf, 11, False)))
			   ping_count++;
			if(NT_STATUS_IS_OK(message_send_pid(pid_to_procid(pid),
							    MSG_PING,
							    NULL, 0, False)))
			   ping_count++;

			while (ping_count > pong_count + 20) {
				message_dispatch();
			}
		}
		
		printf("waiting for %d remaining replies (done %d)\n", 
		       (int)(ping_count - pong_count), pong_count);
		while (timeval_elapsed(&tv) < 30 && pong_count < ping_count) {
			message_dispatch();
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

