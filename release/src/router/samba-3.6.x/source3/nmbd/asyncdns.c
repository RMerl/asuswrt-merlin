/*
   Unix SMB/CIFS implementation.
   a async DNS handler
   Copyright (C) Andrew Tridgell 1997-1998
   
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
#include "nmbd/nmbd.h"

/***************************************************************************
  Add a DNS result to the name cache.
****************************************************************************/

static struct name_record *add_dns_result(struct nmb_name *question, struct in_addr addr)
{
	int name_type = question->name_type;
	unstring qname;

	pull_ascii_nstring(qname, sizeof(qname), question->name);
  
	if (!addr.s_addr) {
		/* add the fail to WINS cache of names. give it 1 hour in the cache */
		DEBUG(3,("add_dns_result: Negative DNS answer for %s\n", qname));
		add_name_to_subnet( wins_server_subnet, qname, name_type,
				NB_ACTIVE, 60*60, DNSFAIL_NAME, 1, &addr );
		return NULL;
	}

	/* add it to our WINS cache of names. give it 2 hours in the cache */
	DEBUG(3,("add_dns_result: DNS gave answer for %s of %s\n", qname, inet_ntoa(addr)));

	add_name_to_subnet( wins_server_subnet, qname, name_type,
                              NB_ACTIVE, 2*60*60, DNS_NAME, 1, &addr);

	return find_name_on_subnet(wins_server_subnet, question, FIND_ANY_NAME);
}

#ifndef SYNC_DNS

static int fd_in = -1, fd_out = -1;
static pid_t child_pid = -1;
static int in_dns;

/* this is the structure that is passed between the parent and child */
struct query_record {
	struct nmb_name name;
	struct in_addr result;
};

/* a queue of pending requests waiting to be sent to the DNS child */
static struct packet_struct *dns_queue;

/* the packet currently being processed by the dns child */
static struct packet_struct *dns_current;


/***************************************************************************
  return the fd used to gather async dns replies. This is added to the select
  loop
  ****************************************************************************/

int asyncdns_fd(void)
{
	return fd_in;
}

/***************************************************************************
  handle DNS queries arriving from the parent
  ****************************************************************************/
static void asyncdns_process(void)
{
	struct query_record r;
	unstring qname;

	DEBUGLEVEL_CLASS[DBGC_ALL] = -1;

	while (1) {
		NTSTATUS status;

		status = read_data(fd_in, (char *)&r, sizeof(r));

		if (!NT_STATUS_IS_OK(status)) {
			break;
		}

		pull_ascii_nstring( qname, sizeof(qname), r.name.name);
		r.result.s_addr = interpret_addr(qname);

		if (write_data(fd_out, (char *)&r, sizeof(r)) != sizeof(r))
			break;
	}

	_exit(0);
}

/**************************************************************************** **
  catch a sigterm (in the child process - the parent has a different handler
  see nmbd.c for details).
  We need a separate term handler here so we don't release any 
  names that our parent is going to release, or overwrite a 
  WINS db that our parent is going to write.
 **************************************************************************** */

static void sig_term(int sig)
{
	_exit(0);
}

/***************************************************************************
 Called by the parent process when it receives a SIGTERM - also kills the
 child so we don't get child async dns processes lying around, causing trouble.
  ****************************************************************************/

void kill_async_dns_child(void)
{
	if (child_pid > 0) {
		kill(child_pid, SIGTERM);
		child_pid = -1;
	}
}

/***************************************************************************
  create a child process to handle DNS lookups
  ****************************************************************************/
void start_async_dns(void)
{
	int fd1[2], fd2[2];
	NTSTATUS status;

	CatchChild();

	if (pipe(fd1) || pipe(fd2)) {
		DEBUG(0,("can't create asyncdns pipes\n"));
		return;
	}

	child_pid = sys_fork();

	if (child_pid) {
		fd_in = fd1[0];
		fd_out = fd2[1];
		close(fd1[1]);
		close(fd2[0]);
		DEBUG(0,("started asyncdns process %d\n", (int)child_pid));
		return;
	}

	fd_in = fd2[0];
	fd_out = fd1[1];

	CatchSignal(SIGUSR2, SIG_IGN);
	CatchSignal(SIGUSR1, SIG_IGN);
	CatchSignal(SIGHUP, SIG_IGN);
        CatchSignal(SIGTERM, sig_term);

	status = reinit_after_fork(nmbd_messaging_context(),
				   nmbd_event_context(),
				   procid_self(), true);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("reinit_after_fork() failed\n"));
		smb_panic("reinit_after_fork() failed");
	}

	asyncdns_process();
}


/***************************************************************************
check if a particular name is already being queried
  ****************************************************************************/
static bool query_current(struct query_record *r)
{
	return dns_current &&
		nmb_name_equal(&r->name, 
			   &dns_current->packet.nmb.question.question_name);
}


/***************************************************************************
  write a query to the child process
  ****************************************************************************/
static bool write_child(struct packet_struct *p)
{
	struct query_record r;

	r.name = p->packet.nmb.question.question_name;

	return write_data(fd_out, (char *)&r, sizeof(r)) == sizeof(r);
}

/***************************************************************************
  check the DNS queue
  ****************************************************************************/
void run_dns_queue(void)
{
	struct query_record r;
	struct packet_struct *p, *p2;
	struct name_record *namerec;
	NTSTATUS status;

	if (fd_in == -1)
		return;

	if (!process_exists_by_pid(child_pid)) {
		close(fd_in);
		close(fd_out);
		start_async_dns();
	}

	status = read_data(fd_in, (char *)&r, sizeof(r));

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("read from child failed: %s\n", nt_errstr(status)));
		fd_in = -1;
		return;
	}

	namerec = add_dns_result(&r.name, r.result);

	if (dns_current) {
		if (query_current(&r)) {
			DEBUG(3,("DNS calling send_wins_name_query_response\n"));
			in_dns = 1;
			if(namerec == NULL)
				send_wins_name_query_response(NAM_ERR, dns_current, NULL);
			else
				send_wins_name_query_response(0,dns_current,namerec);
			in_dns = 0;
		}

		dns_current->locked = False;
		free_packet(dns_current);
		dns_current = NULL;
	}

	/* loop over the whole dns queue looking for entries that
	   match the result we just got */
	for (p = dns_queue; p;) {
		struct nmb_packet *nmb = &p->packet.nmb;
		struct nmb_name *question = &nmb->question.question_name;

		if (nmb_name_equal(question, &r.name)) {
			DEBUG(3,("DNS calling send_wins_name_query_response\n"));
			in_dns = 1;
			if(namerec == NULL)
				send_wins_name_query_response(NAM_ERR, p, NULL);
			else
				send_wins_name_query_response(0,p,namerec);
			in_dns = 0;
			p->locked = False;

			p2 = p->next;
			DLIST_REMOVE(dns_queue, p);
			free_packet(p);
			p = p2;
		} else {
			p = p->next;
		}
	}

	if (dns_queue) {
		dns_current = dns_queue;
		DLIST_REMOVE(dns_queue, dns_queue);

		if (!write_child(dns_current)) {
			DEBUG(3,("failed to send DNS query to child!\n"));
			return;
		}
	}
}

/***************************************************************************
queue a DNS query
  ****************************************************************************/

bool queue_dns_query(struct packet_struct *p,struct nmb_name *question)
{
	if (in_dns || fd_in == -1)
		return False;

	if (!dns_current) {
		if (!write_child(p)) {
			DEBUG(3,("failed to send DNS query to child!\n"));
			return False;
		}
		dns_current = p;
		p->locked = True;
	} else {
		p->locked = True;
		DLIST_ADD(dns_queue, p);
	}

	DEBUG(3,("added DNS query for %s\n", nmb_namestr(question)));
	return True;
}

#else


/***************************************************************************
  we use this when we can't do async DNS lookups
  ****************************************************************************/

bool queue_dns_query(struct packet_struct *p,struct nmb_name *question)
{
	struct name_record *namerec = NULL;
	struct in_addr dns_ip;
	unstring qname;

	pull_ascii_nstring(qname, sizeof(qname), question->name);

	DEBUG(3,("DNS search for %s - ", nmb_namestr(question)));

	dns_ip.s_addr = interpret_addr(qname);

	namerec = add_dns_result(question, dns_ip);
	if(namerec == NULL) {
		send_wins_name_query_response(NAM_ERR, p, NULL);
	} else {
		send_wins_name_query_response(0, p, namerec);
	}
	return False;
}

/***************************************************************************
 With sync dns there is no child to kill on SIGTERM.
  ****************************************************************************/

void kill_async_dns_child(void)
{
	return;
}
#endif
