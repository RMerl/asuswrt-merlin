/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#include "epoll_loop.h"
#include "bridge_ctl.h"
#include "ctl_socket_server.h"
#include "netif_utils.h"
#include "packet.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <syslog.h>
#include <errno.h>

static int become_daemon = 1;
static int is_daemon = 0;
int log_level = LOG_LEVEL_DEFAULT;

int main(int argc, char *argv[])
{
	int c,ret;
	while ((c = getopt(argc, argv, "dv:")) != -1) {
		switch (c) {
		case 'd':
			become_daemon = 0;
			break;
		case 'v':
			{
				char *end;
				long l;
				l = strtoul(optarg, &end, 0);
				if (*optarg == 0 || *end != 0
				    || l > LOG_LEVEL_MAX) {
					ERROR("Invalid loglevel %s", optarg);
					exit(1);
				}
				log_level = l;
			}
			break;
		default:
			return -1;
		}
	}

	TST(init_epoll() == 0, -1);
	TST(ctl_socket_init() == 0, -1);
	TST(packet_sock_init() == 0, -1);
	TST(netsock_init() == 0, -1);
	TST(init_bridge_ops() == 0, -1);
	if (become_daemon) {
		FILE *f = fopen("/var/run/rstpd.pid", "w");
		if (!f) {
			ERROR("can't open /var/run/rstp.pid");
			return -1;
		}
		openlog("rstpd", 0, LOG_DAEMON);
		ret = daemon(0, 0);
		if (ret) {
			ERROR("daemon() failed");
			return -1;		    
		}
		is_daemon = 1;
		fprintf(f, "%d", getpid());
		fclose(f);
	}
	return epoll_main_loop();
}

/*********************** Logging *********************/

#include <stdarg.h>
#include <time.h>

void vDprintf(int level, const char *fmt, va_list ap)
{
	if (level > log_level)
		return;

	if (!is_daemon) {
		char logbuf[256];
		logbuf[255] = 0;
		time_t clock;
		struct tm *local_tm;
		time(&clock);
		local_tm = localtime(&clock);
		int l =
		    strftime(logbuf, sizeof(logbuf) - 1, "%F %T ", local_tm);
		vsnprintf(logbuf + l, sizeof(logbuf) - l - 1, fmt, ap);
		printf("%s\n", logbuf);
	} else {
		vsyslog((level <= LOG_LEVEL_INFO) ? LOG_INFO : LOG_DEBUG, fmt,
			ap);
	}
}

void Dprintf(int level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vDprintf(level, fmt, ap);
	va_end(ap);
}
