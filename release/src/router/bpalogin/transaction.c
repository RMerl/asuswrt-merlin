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

void add_trans_data(struct transaction * t,char *p,int count)
{
	memcpy(t->data+t->length,p,count);
	t->length += count;
}

void add_int2field(struct transaction * t,INT2 v)
{
	add_trans_data(t,(char *)&v,2);
}

void add_int4field(struct transaction * t,INT4 v)
{
	add_trans_data(t,(char *)&v,4);
}

void add_string(struct transaction * t,char *s)
{
	add_int2field(t,htons((INT2)(strlen(s)+4)));
	add_trans_data(t,s,strlen(s));
}

void add_data(struct transaction * t,char *s,int c)
{
	add_int2field(t,htons((INT2)(c+4)));
	add_trans_data(t,s,c);
}

void add_field_string(struct session *s,struct transaction * t,INT2 fn,char * p)
{
	//s->debug(2,"Sending parm %d value %s\n",fn,p);

	add_int2field(t,htons(fn));
	add_string(t,p);
}

void add_field_data(struct session *s,struct transaction * t,INT2 fn,char * p,int c)
{
	//s->debug(2,"Sending parm %d value %s\n",fn,p);

	add_int2field(t,htons(fn));
	add_data(t,p,c);
}

void add_field_INT2(struct session *s,struct transaction * t,INT2 fn,INT2 v)
{
	//s->debug(2,"Sending parm %d value %d\n",fn,v);

	add_int2field(t,htons(fn));
	add_int2field(t,htons(6));
	add_int2field(t,htons(v));
}

void add_field_INT4(struct session *s,struct transaction * t,INT2 fn,INT4 v)
{
	//s->debug(2,"Sending parm %d value %d\n",fn,v);

	add_int2field(t,htons(fn));
	add_int2field(t,htons(8));
	add_int4field(t,htonl(v));
}

void start_transaction(struct transaction * t,INT2 msgtype,INT4 sessionid)
{
	t->length = 0;
	memset(t->data,0,1512);
	add_int2field(t,htons(msgtype));
	add_int2field(t,htons(0));
	add_int4field(t,htonl(sessionid));
}

void dump_transaction(struct session * s,struct transaction * t)
{
return;
	int i;
	char * p = t->data;
	char pkt[1500];
	char *pktp = &pkt;
	memset(pkt,0,1500);
	i = 0;
#if 0	// by tallest debug 0210
	while(i < t->length)
	{
		s->debug(3,"%d:%02x ",i,(unsigned char)*(p));

		p++;
		i++;
	}
	s->debug(3,"\n");
#endif
#if 0
	s->debug(3,"transaction data : ===== start =====");
	while(i < t->length)
        {
                sprintf(pktp,"%02x ",(unsigned char)*(p));
                
		pktp = pktp+3;                                                                                          
                p++;
                i++;
		if(!(i%32)){
			s->debug(3,"%s;",pktp-96);
		}

		if(i == t->length){
			int j;
			j = (i%32)*3;
			s->debug(3,"%s;",pktp-j);
		}
        }
	//syslog(LOG_DEBUG,"data: %s",pkt);
	s->debug(3,"transaction data : ===== end =====");
#endif
}

void dump_sent_transaction(struct session *s,struct transaction * t)
{
	s->debug(3,"Sent transaction:\n");
	dump_transaction(s,t);
}

void dump_recv_transaction(struct session *s,struct transaction * t)
{
	s->debug(3,"Received transaction:\n");
	dump_transaction(s,t);
}

void send_transaction(struct session *s,int socket,struct transaction * t)
{
	int r;
	INT2 * lll;

	lll = (INT2 *)(t->data+2);
	*lll = htons((INT2)t->length);

	r = send(socket,(void *)t,t->length,0);
	dump_sent_transaction(s,t);
}

void send_udp_transaction(struct session * s,struct transaction * t)
{
	int r;
	
	INT2 * lll;

	lll = (INT2 *)(t->data+2);
	*lll = htons((INT2)t->length);

	s->fromaddr.sin_port = htons(s->statusport);
	r = sendto(s->listensock,(void *)t,t->length,0,(struct sockaddr *)&s->fromaddr,sizeof(s->fromaddr));
	dump_sent_transaction(s,t);
}

INT2 receive_transaction(struct session *s,int socket,struct transaction * t)
{
	INT2 * v;
	int r = recv(socket,(char *)t,1500,0);

	t->length = r;
	dump_recv_transaction(s,t);

	v = (INT2 *)t;

	return r>0?ntohs(*v):0;
}

int check_hb_packet(struct session * s,struct transaction * t,int length)
{
	INT2 type;

	type = ntohs(*(INT2 *)t);
	if(type != T_MSG_STATUS_REQ) 
	{
		s->debug(0,"Incorrect transaction type %d",type);
		return 0;
	}

	if(length != 8)
	{
		s->debug(0,"Incorrect transaction length %d",length);
		return 0;
	}
	return 1;
}

INT2 receive_udp_transaction(struct session *s,int socket,struct transaction * t,struct sockaddr_in *addr)
{
	struct timeval timeval;
	fd_set readfds;
	INT2 * v;
	int l = sizeof(struct sockaddr_in);
	int r,i;

	timeval.tv_sec = s->maxheartbeat;
	timeval.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(socket,&readfds);

	r = select(socket+1,&readfds,NULL,NULL,&timeval);

	if(r == -1)
	{
		return (INT2) 0xfffe;
	}
	else if(!r)
	{
		return (INT2) 0xffff;
	}
	else
	{
		r = recvfrom(socket,(char *)t,1500,0,(struct sockaddr*)addr,&l);
		if(r==-1)
			return (INT2) 0xfffe;

		//if(s->lastheartbeat + s->minheartbeat > time(NULL))
		if(s->lastheartbeat + s->minheartbeat > get_time(NULL))
		{
        		s->recenthb++;
        		if(s->recenthb > 3)
        		{
        			s->debug(0,"Heartbeats arriving too quickly - discarding\n");
				return (INT2)0xfffd;

        		}
		}
		else
        		s->recenthb = 0;

		//s->lastheartbeat = time(NULL);
		s->lastheartbeat = get_time(NULL);
	}

	for(i = 0;i<s->tsmcount;i++)
	{
		if(addr->sin_addr.s_addr == s->tsmlist_in[i].sin_addr.s_addr)
			break;
	}
	if(i == s->tsmcount)
	{
		s->debug(0,"Received a heartbeat from unexpected source %s:%d\n",inet_ntoa(addr->sin_addr),ntohs(addr->sin_port));
		return (INT2)0xfffd;
	}

	t->length = r;

	dump_recv_transaction(s,t);
	/*
	**  Lets make sure this packet is structured correctly
	*/
	if(!check_hb_packet(s,t,r))
	{
		return (INT2)0xfffd;
	}

	dump_recv_transaction(s,t);

	v = (INT2 *)t;

	return r>0?ntohs(*v):0;
}

char * locate_parm(struct transaction * t,INT2 parm)
{
	char * p = t->data,*pp,*lp,*vp;
	INT2 p1;
	INT2 l;
	int i;

	i = 8;
	p += 8;
	while(i < t->length)
	{
		pp = p;
		lp = p+2;
		vp = p+4;

		l = read_INT2(lp);
		p1 = read_INT2(pp);

		if(parm == p1)
		{
			return vp;
		}
		p += l;
		i += l;
	}
	return NULL;
}

int extract_valueINT2(struct session *s,struct transaction * t,INT2 parm,INT2 *v)
{
	INT2 * pp = (INT2 *)locate_parm(t,parm);
	if(pp)
	{
		*v = read_INT2(pp);
		s->debug(2,"Received parm %d value %d\n",parm,*v);

		return TRUE;
	}
	return FALSE;
}

int extract_valueINT4(struct session *s,struct transaction * t,INT2 parm,INT4 *v)
{
	INT4 * pp = (INT4 *)locate_parm(t,parm);
	if(pp)
	{
		*v = read_INT4(pp);
		s->debug(2,"Received parm %d value %d\n",parm,*v);

		return TRUE;
	}
	return FALSE;
}

int extract_valuestring(struct session *s,struct transaction * t,INT2 parm,char * v)
{
	char * pp = locate_parm(t,parm);
	if(pp)
	{
		INT2 l = read_INT2(pp-2);
		memcpy(v,pp,l-4);
		*(v+l-4) = 0;
		s->debug(2,"Received parm %d value %s\n",parm,v);

		return TRUE;
	}
	return FALSE;
}

INT2 read_INT2(void *pp)
{
#ifdef __i386
	return ntohs(*((INT2*)pp));
#else
	unsigned char * p = (unsigned char *)pp;
	return (((INT2)(*p))<<8) + ((INT2)(*(p+1)));
#endif
}

INT4 read_INT4(void *pp)
{
#ifdef __i386
	return ntohl(*((INT4*)pp));
#else
	unsigned char * p = (unsigned char *)pp;
	return (((INT4)(*p))<<24) + (((INT4)(*(p+1)))<<16) + (((INT4)(*(p+2)))<<8) + (((INT4)(*(p+3))));
#endif
}
