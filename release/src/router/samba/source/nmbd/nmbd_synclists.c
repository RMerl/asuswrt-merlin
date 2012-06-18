/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   
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

/* this file handles asynchronous browse synchronisation requests. The
   requests are done by forking and putting the result in a file in the
   locks directory. We do it this way because we don't want nmbd to be
   blocked waiting for some server to respond on a TCP connection. This
   also allows us to have more than 1 sync going at once (tridge) */

#include "includes.h"
#include "smb.h"

extern int DEBUGLEVEL;

struct sync_record {
	struct sync_record *next, *prev;
	fstring workgroup;
	fstring server;
	pstring fname;
	struct in_addr ip;
	pid_t pid;
};

/* a linked list of current sync connections */
static struct sync_record *syncs;

static FILE *fp;

/*******************************************************************
  This is the NetServerEnum callback.
  ******************************************************************/
static void callback(const char *sname, uint32 stype, const char *comment)
{
	fprintf(fp,"\"%s\" %08X \"%s\"\n", sname, stype, comment);
}


/*******************************************************************
  Synchronise browse lists with another browse server.
  Log in on the remote server's SMB port to their IPC$ service,
  do a NetServerEnum and record the results in fname
******************************************************************/
static void sync_child(char *name, int nm_type, 
		       char *workgroup,
		       struct in_addr ip, BOOL local, BOOL servers,
		       char *fname)
{
	extern fstring local_machine;
	static struct cli_state cli;
	uint32 local_type = local ? SV_TYPE_LOCAL_LIST_ONLY : 0;
	struct nmb_name called, calling;

	if (!cli_initialise(&cli) || !cli_connect(&cli, name, &ip)) {
		fclose(fp);
		return;
	}

	make_nmb_name(&calling, local_machine, 0x0);
	make_nmb_name(&called , name, nm_type);

	if (!cli_session_request(&cli, &calling, &called))
	{
		cli_shutdown(&cli);
		fclose(fp);
		return;
	}

	if (!cli_negprot(&cli)) {
		cli_shutdown(&cli);
		return;
	}

	if (!cli_session_setup(&cli, "", "", 1, "", 0, workgroup)) {
		cli_shutdown(&cli);
		return;
	}

	if (!cli_send_tconX(&cli, "IPC$", "IPC", "", 1)) {
		cli_shutdown(&cli);
		return;
	}

	/* Fetch a workgroup list. */
	cli_NetServerEnum(&cli, cli.server_domain?cli.server_domain:workgroup, 
			  local_type|SV_TYPE_DOMAIN_ENUM,
			  callback);
	
	/* Now fetch a server list. */
	if (servers) {
		cli_NetServerEnum(&cli, workgroup, 
				  local?SV_TYPE_LOCAL_LIST_ONLY:SV_TYPE_ALL,
				  callback);
	}
	
	cli_shutdown(&cli);
}


/*******************************************************************
  initialise a browse sync with another browse server.  Log in on the
  remote server's SMB port to their IPC$ service, do a NetServerEnum
  and record the results
******************************************************************/
void sync_browse_lists(struct work_record *work,
		       char *name, int nm_type, 
		       struct in_addr ip, BOOL local, BOOL servers)
{
	struct sync_record *s;
	static int counter;

	/* Check we're not trying to sync with ourselves. This can
	   happen if we are a domain *and* a local master browser. */
	if (ismyip(ip)) {
		return;
	}

	s = (struct sync_record *)malloc(sizeof(*s));
	if (!s) return;

	ZERO_STRUCTP(s);
	
	fstrcpy(s->workgroup, work->work_group);
	fstrcpy(s->server, name);
	s->ip = ip;

	slprintf(s->fname, sizeof(pstring)-1,
		 "%s/sync.%d", lp_lockdir(), counter++);
	all_string_sub(s->fname,"//", "/", 0);
	
	DLIST_ADD(syncs, s);

	/* the parent forks and returns, leaving the child to do the
	   actual sync */
	CatchChild();
	if ((s->pid = fork())) return;

	BlockSignals( False, SIGTERM );

	DEBUG(2,("Initiating browse sync for %s to %s(%s)\n",
		 work->work_group, name, inet_ntoa(ip)));

	fp = sys_fopen(s->fname,"w");
	if (!fp) _exit(1);	

	sync_child(name, nm_type, work->work_group, ip, local, servers,
		   s->fname);

	fclose(fp);
	_exit(0);
}

/**********************************************************************
handle one line from a completed sync file
 **********************************************************************/
static void complete_one(struct sync_record *s, 
			 char *sname, uint32 stype, char *comment)
{
	struct work_record *work;
	struct server_record *servrec;

	stype &= ~SV_TYPE_LOCAL_LIST_ONLY;

	if (stype & SV_TYPE_DOMAIN_ENUM) {
		/* See if we can find the workgroup on this subnet. */
		if((work=find_workgroup_on_subnet(unicast_subnet, sname))) {
			/* We already know about this workgroup -
                           update the ttl. */
			update_workgroup_ttl(work,lp_max_ttl());
		} else {
			/* Create the workgroup on the subnet. */
			work = create_workgroup_on_subnet(unicast_subnet, 
							  sname, lp_max_ttl());
			if (work) {
				/* remember who the master is */
				fstrcpy(work->local_master_browser_name, 
					comment);
			}
		}
		return;
	} 

	work = find_workgroup_on_subnet(unicast_subnet, s->workgroup);
	if (!work) {
		DEBUG(3,("workgroup %s doesn't exist on unicast subnet?\n",
			 s->workgroup));
		return;
	}

	if ((servrec = find_server_in_workgroup( work, sname))) {
		/* Check that this is not a locally known
		   server - if so ignore the entry. */
		if(!(servrec->serv.type & SV_TYPE_LOCAL_LIST_ONLY)) {
			/* We already know about this server - update
                           the ttl. */
			update_server_ttl(servrec, lp_max_ttl());
			/* Update the type. */
			servrec->serv.type = stype;
		}
		return;
	} 

	/* Create the server in the workgroup. */ 
	create_server_on_workgroup(work, sname,stype, lp_max_ttl(), comment);
}
		

/**********************************************************************
read the completed sync info
 **********************************************************************/
static void complete_sync(struct sync_record *s)
{
	FILE *f;
	fstring server, type_str;
	unsigned type;
	pstring comment;
	pstring line;
	char *ptr;
	int count=0;

	f = sys_fopen(s->fname,"r");

	if (!f) return;
	
	while (!feof(f)) {
		
		if (!fgets_slash(line,sizeof(pstring),f)) continue;
		
		ptr = line;

		if (!next_token(&ptr,server,NULL,sizeof(server)) ||
		    !next_token(&ptr,type_str,NULL, sizeof(type_str)) ||
		    !next_token(&ptr,comment,NULL, sizeof(comment))) {
			continue;
		}

		sscanf(type_str, "%X", &type);

		complete_one(s, server, type, comment);

		count++;
	}

	fclose(f);

	unlink(s->fname);

	DEBUG(2,("sync with %s(%s) for workgroup %s completed (%d records)\n",
		 s->server, inet_ntoa(s->ip), s->workgroup, count));
}

/**********************************************************************
check for completion of any of the child processes
 **********************************************************************/
void sync_check_completion(void)
{
	struct sync_record *s, *next;

	for (s=syncs;s;s=next) {
		next = s->next;
		if (!process_exists(s->pid)) {
			/* it has completed - grab the info */
			complete_sync(s);
			DLIST_REMOVE(syncs, s);
			ZERO_STRUCTP(s);
			free(s);
		}
	}
}
