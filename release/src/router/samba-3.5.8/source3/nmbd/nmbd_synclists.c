/* 
   Unix SMB/CIFS implementation.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   
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

/* this file handles asynchronous browse synchronisation requests. The
   requests are done by forking and putting the result in a file in the
   locks directory. We do it this way because we don't want nmbd to be
   blocked waiting for some server to respond on a TCP connection. This
   also allows us to have more than 1 sync going at once (tridge) */

#include "includes.h"

struct sync_record {
	struct sync_record *next, *prev;
	unstring workgroup;
	unstring server;
	char *fname;
	struct in_addr ip;
	pid_t pid;
};

/* a linked list of current sync connections */
static struct sync_record *syncs;

static XFILE *fp;

/*******************************************************************
  This is the NetServerEnum callback.
  Note sname and comment are in UNIX codepage format.
  ******************************************************************/

static void callback(const char *sname, uint32 stype, 
                     const char *comment, void *state)
{
	x_fprintf(fp,"\"%s\" %08X \"%s\"\n", sname, stype, comment);
}

/*******************************************************************
  Synchronise browse lists with another browse server.
  Log in on the remote server's SMB port to their IPC$ service,
  do a NetServerEnum and record the results in fname
******************************************************************/

static void sync_child(char *name, int nm_type, 
		       char *workgroup,
		       struct in_addr ip, bool local, bool servers,
		       char *fname)
{
	fstring unix_workgroup;
	struct cli_state *cli;
	uint32 local_type = local ? SV_TYPE_LOCAL_LIST_ONLY : 0;
	struct nmb_name called, calling;
	struct sockaddr_storage ss;
	NTSTATUS status;

	/* W2K DMB's return empty browse lists on port 445. Use 139.
	 * Patch from Andy Levine andyl@epicrealm.com.
	 */

	cli = cli_initialise();
	if (!cli) {
		return;
	}

	cli_set_port(cli, 139);

	in_addr_to_sockaddr_storage(&ss, ip);
	status = cli_connect(cli, name, &ss);
	if (!NT_STATUS_IS_OK(status)) {
		cli_shutdown(cli);
		return;
	}

	make_nmb_name(&calling, get_local_machine_name(), 0x0);
	make_nmb_name(&called , name, nm_type);

	if (!cli_session_request(cli, &calling, &called)) {
		cli_shutdown(cli);
		return;
	}

	status = cli_negprot(cli);
	if (!NT_STATUS_IS_OK(status)) {
		cli_shutdown(cli);
		return;
	}

	if (!NT_STATUS_IS_OK(cli_session_setup(cli, "", "", 1, "", 0,
					       workgroup))) {
		cli_shutdown(cli);
		return;
	}

	if (!NT_STATUS_IS_OK(cli_tcon_andx(cli, "IPC$", "IPC", "", 1))) {
		cli_shutdown(cli);
		return;
	}

	/* All the cli_XX functions take UNIX character set. */
	fstrcpy(unix_workgroup, cli->server_domain ? cli->server_domain : workgroup);

	/* Fetch a workgroup list. */
	cli_NetServerEnum(cli, unix_workgroup,
			  local_type|SV_TYPE_DOMAIN_ENUM, 
			  callback, NULL);
	
	/* Now fetch a server list. */
	if (servers) {
		fstrcpy(unix_workgroup, workgroup);
		cli_NetServerEnum(cli, unix_workgroup, 
				  local?SV_TYPE_LOCAL_LIST_ONLY:SV_TYPE_ALL,
				  callback, NULL);
	}
	
	cli_shutdown(cli);
}

/*******************************************************************
  initialise a browse sync with another browse server.  Log in on the
  remote server's SMB port to their IPC$ service, do a NetServerEnum
  and record the results
******************************************************************/

void sync_browse_lists(struct work_record *work,
		       char *name, int nm_type, 
		       struct in_addr ip, bool local, bool servers)
{
	struct sync_record *s;
	static int counter;

	START_PROFILE(sync_browse_lists);
	/* Check we're not trying to sync with ourselves. This can
	   happen if we are a domain *and* a local master browser. */
	if (ismyip_v4(ip)) {
done:
		END_PROFILE(sync_browse_lists);
		return;
	}

	s = SMB_MALLOC_P(struct sync_record);
	if (!s) goto done;

	ZERO_STRUCTP(s);

	unstrcpy(s->workgroup, work->work_group);
	unstrcpy(s->server, name);
	s->ip = ip;

	if (asprintf(&s->fname, "%s/sync.%d", lp_lockdir(), counter++) < 0) {
		SAFE_FREE(s);
		goto done;
	}
	/* Safe to use as 0 means no size change. */
	all_string_sub(s->fname,"//", "/", 0);

	DLIST_ADD(syncs, s);

	/* the parent forks and returns, leaving the child to do the
	   actual sync and call END_PROFILE*/
	CatchChild();
	if ((s->pid = sys_fork())) return;

	BlockSignals( False, SIGTERM );

	DEBUG(2,("Initiating browse sync for %s to %s(%s)\n",
		 work->work_group, name, inet_ntoa(ip)));

	fp = x_fopen(s->fname,O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (!fp) {
		END_PROFILE(sync_browse_lists);
		_exit(1);
	}

	sync_child(name, nm_type, work->work_group, ip, local, servers,
		   s->fname);

	x_fclose(fp);
	END_PROFILE(sync_browse_lists);
	_exit(0);
}

/**********************************************************************
 Handle one line from a completed sync file.
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
				unstrcpy(work->local_master_browser_name, comment);
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
 Read the completed sync info.
**********************************************************************/

static void complete_sync(struct sync_record *s)
{
	XFILE *f;
	char *server;
	char *type_str;
	unsigned type;
	char *comment;
	char line[1024];
	const char *ptr;
	int count=0;

	f = x_fopen(s->fname,O_RDONLY, 0);

	if (!f)
		return;

	while (!x_feof(f)) {
		TALLOC_CTX *frame = NULL;

		if (!fgets_slash(line,sizeof(line),f))
			continue;

		ptr = line;

		frame = talloc_stackframe();
		if (!next_token_talloc(frame,&ptr,&server,NULL) ||
		    !next_token_talloc(frame,&ptr,&type_str,NULL) ||
		    !next_token_talloc(frame,&ptr,&comment,NULL)) {
			TALLOC_FREE(frame);
			continue;
		}

		sscanf(type_str, "%X", &type);

		complete_one(s, server, type, comment);

		count++;
		TALLOC_FREE(frame);
	}
	x_fclose(f);

	unlink(s->fname);

	DEBUG(2,("sync with %s(%s) for workgroup %s completed (%d records)\n",
		 s->server, inet_ntoa(s->ip), s->workgroup, count));
}

/**********************************************************************
 Check for completion of any of the child processes.
**********************************************************************/

void sync_check_completion(void)
{
	struct sync_record *s, *next;

	for (s=syncs;s;s=next) {
		next = s->next;
		if (!process_exists_by_pid(s->pid)) {
			/* it has completed - grab the info */
			complete_sync(s);
			DLIST_REMOVE(syncs, s);
			SAFE_FREE(s->fname);
			SAFE_FREE(s);
		}
	}
}
