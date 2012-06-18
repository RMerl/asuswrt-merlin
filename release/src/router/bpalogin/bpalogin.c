/*
**	BPALogin - lightweight portable BIDS2 login client
**	Copyright (c) 2001-3 Shane Hyde, and others.
** 
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
** 
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
** 
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
*/ 

#include "bpalogin.h"

/*
**  Main processing loop.  Logs in, handles heartbeats, and logs out on request
**
**  Returns - 0 - We are no longer connected, and should not retry.
**            1 - We are no longer connnected and should retry.
*/
int mainloop(struct session * s)
{
	int err;
	struct sockaddr_in listenaddr;
	struct hostent * he;
	int addrsize;
	FILE *fp;

	s->lastheartbeat = 0;
	s->sequence = 0;
	s->listensock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	s->localaddr.sin_family = AF_INET;
	s->localaddr.sin_port = htons(s->localport);
	if(strcmp(s->localaddress,""))
	{
		s->debug(0,"Using local address %s\n",s->localaddress);
		he = gethostbyname(s->localaddress);

		if(he)
		{
			s->localaddr.sin_addr.s_addr = *((int*)(he->h_addr_list[0]));
		}
		else
		{
			s->localaddr.sin_addr.s_addr = inet_addr(s->localaddress);
		}
	}
	else
	{
		s->localaddr.sin_addr.s_addr = INADDR_ANY;
	}

	addrsize = sizeof(struct sockaddr_in);
	err = bind(s->listensock,(struct sockaddr *)&s->localaddr,sizeof(s->localaddr));
	err = getsockname(s->listensock,(struct sockaddr *)&listenaddr,&addrsize);

	//s->sessionid = time(NULL);
	s->sessionid = get_time(NULL);

	s->listenport = ntohs(listenaddr.sin_port);
	strcpy(s->osname,"whocares");
	strcpy(s->osrelease,"notme");

	he = gethostbyname(s->authserver);

	if(he)
	{
		s->authhost.sin_addr.s_addr = *((int*)(he->h_addr_list[0]));
	}
	else
	{
		s->authhost.sin_addr.s_addr = inet_addr(s->authserver);
	}
	
	s->authhost.sin_port = htons(s->authport);
	s->authhost.sin_family = AF_INET;

	s->debug(0,"Auth host = %s:%d\n",s->authserver,s->authport);
	s->debug(0,"Auth domain = %s\n",s->authdomain);
	s->debug(0,"Listening on port %d\n",s->listenport);

	if(test_connect_success 	// by honor, for debug
	   || login(s))
	{
		s->onconnected(s->listenport);
		if(!handle_heartbeats(s))
		{
			int i;
			s->ondisconnected(0);
			s->noncritical("Sleeping for 10 seconds (1)");
			for(i=0;i<10;i++)
			{
				if(s->shutdown)
					return 0;
				else
					sleep(1);
			}
		}
		else
		{
			closesocket(s->listensock);
			return 0;
		}
		closesocket(s->listensock);
	}
	else
	{
		int i;
		s->noncritical("Sleeping for 10 seconds (2)");
		for(i=0;i<10;i++)
		{
			if(s->shutdown)
				return 0;
			else
				sleep(1);
		}
		closesocket(s->listensock);
	}
	return 1;
}

int
log_to_file(char *buf)  // add by honor
{
	FILE *fp;

	if ((fp = fopen("/tmp/ppp/log", "w"))) {
		fprintf(fp, "%s", buf);
		fclose(fp);
		return 1;
	}
	return 0;
}

