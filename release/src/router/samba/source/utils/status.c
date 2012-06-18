/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   status reporting
   Copyright (C) Andrew Tridgell 1994-1998
   
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

   Revision History:

   12 aug 96: Erik.Devriendt@te6.siemens.be
   added support for shared memory implementation of share mode locking

   21-Jul-1998: rsharpe@ns.aus.com (Richard Sharpe)
   Added -L (locks only) -S (shares only) flags and code

*/

/*
 * This program reports current SMB connections
 */

#define NO_SYSLOG

#include "includes.h"

struct connect_record crec;

struct session_record{
  pid_t pid;
  uid_t uid;
  char machine[31];
  time_t start;
  struct session_record *next;
} *srecs;

extern int DEBUGLEVEL;
extern FILE *dbf;

static pstring Ucrit_username = "";                   /* added by OH */
pid_t	Ucrit_pid[100];  /* Ugly !!! */        /* added by OH */
int            Ucrit_MaxPid=0;                        /* added by OH */
unsigned int   Ucrit_IsActive = 0;                    /* added by OH */

int            shares_only = 0;            /* Added by RJS */
int            locks_only  = 0;            /* Added by RJS */

/* we need these because we link to locking*.o */
 void become_root(BOOL save_dir) {}
 void unbecome_root(BOOL restore_dir) {}


/* added by OH */
static void Ucrit_addUsername(char *username)
{
	pstrcpy(Ucrit_username, username);
	if(strlen(Ucrit_username) > 0)
		Ucrit_IsActive = 1;
}

static unsigned int Ucrit_checkUsername(char *username)
{
	if ( !Ucrit_IsActive) return 1;
	if (strcmp(Ucrit_username,username) ==0) return 1;
	return 0;
}

static void Ucrit_addPid(pid_t pid)
{
	int i;
	if ( !Ucrit_IsActive) return;
	for (i=0;i<Ucrit_MaxPid;i++)
		if( pid == Ucrit_pid[i] ) return;
	Ucrit_pid[Ucrit_MaxPid++] = pid;
}

static unsigned int Ucrit_checkPid(pid_t pid)
{
	int i;
	if ( !Ucrit_IsActive) return 1;
	for (i=0;i<Ucrit_MaxPid;i++)
		if( pid == Ucrit_pid[i] ) return 1;
	return 0;
}


static void print_share_mode(share_mode_entry *e, char *fname)
{
	static int count;
	if (count==0) {
		printf("Locked files:\n");
		printf("Pid    DenyMode   R/W        Oplock           Name\n");
		printf("--------------------------------------------------\n");
	}
	count++;

	if (Ucrit_checkPid(e->pid)) {
          printf("%-5d  ",(int)e->pid);
	  switch ((e->share_mode>>4)&0xF) {
	  case DENY_NONE: printf("DENY_NONE  "); break;
	  case DENY_ALL:  printf("DENY_ALL   "); break;
	  case DENY_DOS:  printf("DENY_DOS   "); break;
	  case DENY_READ: printf("DENY_READ  "); break;
	  case DENY_WRITE:printf("DENY_WRITE "); break;
	  }
	  switch (e->share_mode&0xF) {
	  case 0: printf("RDONLY     "); break;
	  case 1: printf("WRONLY     "); break;
	  case 2: printf("RDWR       "); break;
	  }

	  if((e->op_type & 
	     (EXCLUSIVE_OPLOCK|BATCH_OPLOCK)) == 
	      (EXCLUSIVE_OPLOCK|BATCH_OPLOCK))
		printf("EXCLUSIVE+BATCH ");
	  else if (e->op_type & EXCLUSIVE_OPLOCK)
		printf("EXCLUSIVE       ");
	  else if (e->op_type & BATCH_OPLOCK)
		printf("BATCH           ");
	  else if (e->op_type & LEVEL_II_OPLOCK)
		printf("LEVEL_II        ");
	  else
		printf("NONE            ");

	  printf(" %s   %s",dos_to_unix(fname,False),
             asctime(LocalTime((time_t *)&e->time.tv_sec)));
	}
}


/*******************************************************************
 dump the elements of the profile structure
  ******************************************************************/
static int profile_dump(void)
{
#ifndef WITH_PROFILE
	fprintf(stderr,"ERROR: not compiled with profile support\n");
	return -1;
#else
	if (!profile_setup(True)) {
		fprintf(stderr,"Failed to initialise profile memory\n");
		return -1;
	}

	printf("smb_count:\t%u\n", profile_p->smb_count);
	printf("uid_changes:\t%u\n", profile_p->uid_changes);
	return 0;
#endif
}



 int main(int argc, char *argv[])
{
  FILE *f;
  pstring fname;
  int c;
  static pstring servicesf = CONFIGFILE;
  extern char *optarg;
  int verbose = 0, brief =0;
  BOOL processes_only=False;
  pid_t last_pid=(pid_t)0;
  struct session_record *ptr;
  int profile_only = 0;

  TimeInit();
  setup_logging(argv[0],True);

  charset_initialise();

  DEBUGLEVEL = 0;
  dbf = stderr;

  if (getuid() != geteuid()) {
    printf("smbstatus should not be run setuid\n");
    return(1);
  }

  while ((c = getopt(argc, argv, "pdLSs:u:bP")) != EOF) {
    switch (c) {
    case 'b':
      brief = 1;
      break;
    case 'd':
      verbose = 1;
      break;
    case 'L':
      locks_only = 1;
      break;
    case 'p':
      processes_only = 1;
      break;
    case 'P':
      profile_only = 1;
      break;
    case 'S':
      shares_only = 1;
      break;
    case 's':
      pstrcpy(servicesf, optarg);
      break;
    case 'u':                                       /* added by OH */
      Ucrit_addUsername(optarg);                    /* added by OH */
      break;
    default:
      fprintf(stderr, "Usage: %s [-P] [-d] [-L] [-p] [-S] [-s configfile] [-u username]\n", *argv); /* changed by OH */
      return (-1);
    }
  }

  if (!lp_load(servicesf,False,False,False)) {
    fprintf(stderr, "Can't load %s - run testparm to debug it\n", servicesf);
    return (-1);
  }

  if (verbose) {
    printf("using configfile = %s\n", servicesf);
    printf("lockdir = %s\n", *lp_lockdir() ? lp_lockdir() : "NULL");
  }

  if (profile_only) {
	return profile_dump();
  }

  pstrcpy(fname,lp_lockdir());
  standard_sub_basic(fname);
  trim_string(fname,"","/");
  pstrcat(fname,"/STATUS..LCK");

  f = sys_fopen(fname,"r");
  if (!f) {
    printf("Couldn't open status file %s\n",fname);
    if (!lp_status(-1))
      printf("You need to have status=yes in your smb config file\n");
    return(0);
  }
  else if (verbose) {
    printf("Opened status file %s\n", fname);
  }

  if (!locks_only) {

    if (!processes_only) {
      printf("\nSamba version %s\n",VERSION);

      if (brief)
	{
	  printf("PID     Username  Machine                       Time logged in\n");
	  printf("-------------------------------------------------------------------\n");
	}
      else
	{
	  printf("Service      uid      gid      pid     machine\n");
	  printf("----------------------------------------------\n");
	}
    }

    while (!feof(f))
      {
	if (fread(&crec,sizeof(crec),1,f) != 1)
	  break;
	if (crec.cnum == -1) continue;
	if ( crec.magic == 0x280267 && process_exists(crec.pid) 
	     && Ucrit_checkUsername(uidtoname(crec.uid))                      /* added by OH */
	     )
	  {
	    if (brief)
	      {
		ptr=srecs;
		while (ptr!=NULL)
		  {
		    if ((ptr->pid==crec.pid)&&(strncmp(ptr->machine,crec.machine,30)==0)) 
		      {
			if (ptr->start > crec.start)
			  ptr->start=crec.start;
			break;
		      }
		    ptr=ptr->next;
		  }
		if (ptr==NULL)
		  {
		    ptr=(struct session_record *) malloc(sizeof(struct session_record));
		    ptr->uid=crec.uid;
		    ptr->pid=crec.pid;
		    ptr->start=crec.start;
		    strncpy(ptr->machine,crec.machine,30);
		    ptr->machine[30]='\0';
		    ptr->next=srecs;
		    srecs=ptr;
		  }
	      }
	    else
	      {
		Ucrit_addPid(crec.pid);                                             /* added by OH */
		if (processes_only) {
		  if (last_pid != crec.pid)
		    printf("%d\n",(int)crec.pid);
		  last_pid = crec.pid; /* XXXX we can still get repeats, have to
				    add a sort at some time */
		}
		else	  
		  printf("%-10.10s   %-8s %-8s %5d   %-8s (%s) %s",
			 crec.name,uidtoname(crec.uid),gidtoname(crec.gid),(int)crec.pid,
			 crec.machine,crec.addr,
			 asctime(LocalTime(&crec.start)));
	      }
	  }
      }
    fclose(f);
  }
  if (processes_only) exit(0);
  
  if (brief)
  {
    ptr=srecs;
    while (ptr!=NULL)
    {
      printf("%-8d%-10.10s%-30.30s%s",(int)ptr->pid,uidtoname(ptr->uid),ptr->machine,asctime(LocalTime(&(ptr->start))));
    ptr=ptr->next;
    }
    printf("\n");
    exit(0);
  }

  printf("\n");

  if (!shares_only) {
	  if (!locking_init(1)) {
		  printf("Can't initialise shared memory - exiting\n");
		  exit(1);
	  }

	  if (share_mode_forall(print_share_mode) <= 0)
		  printf("No locked files\n");
	  
	  printf("\n");
	  
	  share_status(stdout);
	  
	  locking_end();
  }

  return (0);
}

