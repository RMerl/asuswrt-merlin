/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   service (connection) opening and closing
   Copyright (C) Andrew Tridgell 1992-1998
   
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

#include "includes.h"

extern int DEBUGLEVEL;

extern struct timeval smb_last_time;
extern int case_default;
extern BOOL case_preserve;
extern BOOL short_case_preserve;
extern BOOL case_mangle;
extern BOOL case_sensitive;
extern BOOL use_mangled_map;
extern fstring remote_machine;
extern pstring sesssetup_user;
extern fstring remote_machine;


/****************************************************************************
load parameters specific to a connection/service
****************************************************************************/
BOOL become_service(connection_struct *conn,BOOL do_chdir)
{
	extern char magic_char;
	static connection_struct *last_conn;
	int snum;

	if (!conn)  {
		last_conn = NULL;
		return(False);
	}

	conn->lastused = smb_last_time.tv_sec;

	snum = SNUM(conn);
  
	if (do_chdir &&
	    dos_ChDir(conn->connectpath) != 0 &&
	    dos_ChDir(conn->origpath) != 0) {
		DEBUG(0,("chdir (%s) failed\n",
			 conn->connectpath));
		return(False);
	}

	if (conn == last_conn)
		return(True);

	last_conn = conn;

	case_default = lp_defaultcase(snum);
	case_preserve = lp_preservecase(snum);
	short_case_preserve = lp_shortpreservecase(snum);
	case_mangle = lp_casemangle(snum);
	case_sensitive = lp_casesensitive(snum);
	magic_char = lp_magicchar(snum);
	use_mangled_map = (*lp_mangled_map(snum) ? True:False);
	return(True);
}


/****************************************************************************
  find a service entry
****************************************************************************/
int find_service(char *service)
{
   int iService;

   all_string_sub(service,"\\","/",0);

   iService = lp_servicenumber(service);

   /* now handle the special case of a home directory */
   if (iService < 0)
   {
     int iHomeService;

     /* We check that the HOMES_NAME exists before calling expensive
	functions like get_user_home_dir */

     if ((iHomeService = lp_servicenumber(HOMES_NAME)) >= 0)
     {

      char *phome_dir = get_user_home_dir(service);

      if(!phome_dir)
      {
        /*
         * Try mapping the servicename, it may
         * be a Windows to unix mapped user name.
         */
        if(map_username(service))
          phome_dir = get_user_home_dir(service);
      }

      DEBUG(3,("checking for home directory %s gave %s\n",service,
            phome_dir?phome_dir:"(NULL)"));

      if (phome_dir)
      {   
	lp_add_home(service,iHomeService,phome_dir);
	iService = lp_servicenumber(service);
      }
     }
   }
#ifdef PRINTING
   /* If we still don't have a service, attempt to add it as a printer. */
   if (iService < 0)
   {
      int iPrinterService;

      if ((iPrinterService = lp_servicenumber(PRINTERS_NAME)) >= 0)
      {
         char *pszTemp;

         DEBUG(3,("checking whether %s is a valid printer name...\n", service));
         pszTemp = PRINTCAP;
         if ((pszTemp != NULL) && pcap_printername_ok(service, pszTemp))
         {
            DEBUG(3,("%s is a valid printer name\n", service));
            DEBUG(3,("adding %s as a printer service\n", service));
            lp_add_printer(service,iPrinterService);
            iService = lp_servicenumber(service);
            if (iService < 0)
               DEBUG(0,("failed to add %s as a printer service!\n", service));
         }
         else
            DEBUG(3,("%s is not a valid printer name\n", service));
      }
   }
#endif
   /* just possibly it's a default service? */
   if (iService < 0) 
   {
     char *pdefservice = lp_defaultservice();
     if (pdefservice && *pdefservice && 
	 !strequal(pdefservice,service) &&
	 !strstr(service,".."))
     {
       /*
        * We need to do a local copy here as lp_defaultservice() 
        * returns one of the rotating lp_string buffers that
        * could get overwritten by the recursive find_service() call
        * below. Fix from Josef Hinteregger <joehtg@joehtg.co.at>.
        */
       pstring defservice;
       pstrcpy(defservice, pdefservice);
       iService = find_service(defservice);
       if (iService >= 0)
       {
         all_string_sub(service,"_","/",0);
         iService = lp_add_service(service,iService);
       }
     }
   }

   if (iService >= 0)
     if (!VALID_SNUM(iService))
     {
       DEBUG(0,("Invalid snum %d for %s\n",iService,service));
       iService = -1;
     }

   if (iService < 0)
     DEBUG(3,("find_service() failed to find service %s\n", service));

   return (iService);
}


/****************************************************************************
  make a connection to a service
****************************************************************************/
connection_struct *make_connection(char *service,char *user,char *password, int pwlen, char *dev,uint16 vuid, int *ecode)
{
	int snum;
	struct passwd *pass = NULL;
	BOOL guest = False;
	BOOL force = False;
	extern int Client;
	connection_struct *conn;
	int ret;

	strlower(service);

	snum = find_service(service);
	if (snum < 0) {
		extern int Client;
		if (strequal(service,"IPC$")) {
			DEBUG(3,("refusing IPC connection\n"));
			*ecode = ERRnoipc;
			return NULL;
		}

		DEBUG(2,("%s (%s) couldn't find service %s\n",
			 remote_machine, client_addr(Client), service));
		*ecode = ERRinvnetname;
		return NULL;
	}

	if (strequal(service,HOMES_NAME)) {
		if (*user && Get_Pwnam(user,True)) {
			fstring dos_username;
			fstrcpy(dos_username, user);
			unix_to_dos(dos_username, True);
			return(make_connection(dos_username,user,password,
					       pwlen,dev,vuid,ecode));
		}

		if(lp_security() != SEC_SHARE) {
			if (validated_username(vuid)) {
				fstring dos_username;
				fstrcpy(user,validated_username(vuid));
				fstrcpy(dos_username, user);
				unix_to_dos(dos_username, True);
				return(make_connection(dos_username,user,password,pwlen,dev,vuid,ecode));
			}
		} else {
			/* Security = share. Try with sesssetup_user
			 * as the username.  */
			if(*sesssetup_user) {
				fstring dos_username;
				fstrcpy(user,sesssetup_user);
				fstrcpy(dos_username, user);
				unix_to_dos(dos_username, True);
				return(make_connection(dos_username,user,password,pwlen,dev,vuid,ecode));
			}
		}
	}

	if (!lp_snum_ok(snum) || 
	    !check_access(Client, 
			  lp_hostsallow(snum), lp_hostsdeny(snum))) {    
		*ecode = ERRaccess;
		return NULL;
	}

	/* you can only connect to the IPC$ service as an ipc device */
	if (strequal(service,"IPC$"))
		pstrcpy(dev,"IPC");
	
	if (*dev == '?' || !*dev) {
		if (lp_print_ok(snum)) {
			pstrcpy(dev,"LPT1:");
		} else {
			pstrcpy(dev,"A:");
		}
	}

	/* if the request is as a printer and you can't print then refuse */
	strupper(dev);
	if (!lp_print_ok(snum) && (strncmp(dev,"LPT",3) == 0)) {
		DEBUG(1,("Attempt to connect to non-printer as a printer\n"));
		*ecode = ERRinvdevice;
		return NULL;
	}

	/* lowercase the user name */
	strlower(user);

	/* add it as a possible user name */
	add_session_user(service);

	/* shall we let them in? */
	if (!authorise_login(snum,user,password,pwlen,&guest,&force,vuid)) {
		DEBUG( 2, ( "Invalid username/password for %s\n", service ) );
		*ecode = ERRbadpw;
		return NULL;
	}
  
	conn = conn_new();
	if (!conn) {
		DEBUG(0,("Couldn't find free connection.\n"));
		*ecode = ERRnoresource;
		conn_free(conn);
		return NULL;
	}

	/* find out some info about the user */
	pass = Get_Pwnam(user,True);

	if (pass == NULL) {
		DEBUG(0,( "Couldn't find account %s\n",user));
		*ecode = ERRbaduid;
		conn_free(conn);
		return NULL;
	}

	conn->read_only = lp_readonly(snum);

	{
		pstring list;
		StrnCpy(list,lp_readlist(snum),sizeof(pstring)-1);
		pstring_sub(list,"%S",service);

		if (user_in_list(user,list))
			conn->read_only = True;
		
		StrnCpy(list,lp_writelist(snum),sizeof(pstring)-1);
		pstring_sub(list,"%S",service);
		
		if (user_in_list(user,list))
			conn->read_only = False;    
	}

	/* admin user check */
	
	/* JRA - original code denied admin user if the share was
	   marked read_only. Changed as I don't think this is needed,
	   but old code left in case there is a problem here.
	*/
	if (user_in_list(user,lp_admin_users(snum)) 
#if 0
	    && !conn->read_only
#endif
	    ) {
		conn->admin_user = True;
		DEBUG(0,("%s logged in as admin user (root privileges)\n",user));
	} else {
		conn->admin_user = False;
	}
    
	conn->force_user = force;
	conn->vuid = vuid;
	conn->uid = pass->pw_uid;
	conn->gid = pass->pw_gid;
	safe_strcpy(conn->client_address, client_addr(Client), sizeof(conn->client_address)-1);
	conn->num_files_open = 0;
	conn->lastused = time(NULL);
	conn->service = snum;
	conn->used = True;
	conn->printer = (strncmp(dev,"LPT",3) == 0);
	conn->ipc = (strncmp(dev,"IPC",3) == 0);
	conn->dirptr = NULL;
	conn->veto_list = NULL;
	conn->hide_list = NULL;
	conn->veto_oplock_list = NULL;
	string_set(&conn->dirpath,"");
	string_set(&conn->user,user);
	
	/*
	 * If force user is true, then store the
	 * given userid and also the primary groupid
	 * of the user we're forcing.
	 */
	
	if (*lp_force_user(snum)) {
		struct passwd *pass2;
		pstring fuser;
		pstrcpy(fuser,lp_force_user(snum));

		/* Allow %S to be used by force user. */
		pstring_sub(fuser,"%S",service);

		pass2 = (struct passwd *)Get_Pwnam(fuser,True);
		if (pass2) {
			conn->uid = pass2->pw_uid;
			conn->gid = pass2->pw_gid;
			string_set(&conn->user,fuser);
			fstrcpy(user,fuser);
			conn->force_user = True;
			DEBUG(3,("Forced user %s\n",fuser));	  
		} else {
			DEBUG(1,("Couldn't find user %s\n",fuser));
		}
	}

#ifdef HAVE_GETGRNAM 
	/*
	 * If force group is true, then override
	 * any groupid stored for the connecting user.
	 */
	
	if (*lp_force_group(snum)) {
		struct group *gptr;
		pstring gname;
		pstring tmp_gname;
		BOOL user_must_be_member = False;
		
		StrnCpy(tmp_gname,lp_force_group(snum),sizeof(pstring)-1);

		if (tmp_gname[0] == '+') {
			user_must_be_member = True;
			StrnCpy(gname,&tmp_gname[1],sizeof(pstring)-2);
		} else {
			StrnCpy(gname,tmp_gname,sizeof(pstring)-1);
		}
		/* default service may be a group name 		*/
		pstring_sub(gname,"%S",service);
		gptr = (struct group *)getgrnam(gname);
		
		if (gptr) {
			/*
			 * If the user has been forced and the forced group starts
			 * with a '+', then we only set the group to be the forced
			 * group if the forced user is a member of that group.
			 * Otherwise, the meaning of the '+' would be ignored.
			 */
			if (conn->force_user && user_must_be_member) {
				int i;
				for (i = 0; gptr->gr_mem[i] != NULL; i++) {
					if (strcmp(user,gptr->gr_mem[i]) == 0) {
						conn->gid = gptr->gr_gid;
						DEBUG(3,("Forced group %s for member %s\n",gname,user));
						break;
					}
				}
			} else {
				conn->gid = gptr->gr_gid;
				DEBUG(3,("Forced group %s\n",gname));
			}
		} else {
			DEBUG(1,("Couldn't find group %s\n",gname));
		}
	}
#endif /* HAVE_GETGRNAM */

	{
		pstring s;
		pstrcpy(s,lp_pathname(snum));
		standard_sub(conn,s);
		string_set(&conn->connectpath,s);
		DEBUG(3,("Connect path is %s\n",s));
	}

	/* groups stuff added by ih */
	conn->ngroups = 0;
	conn->groups = NULL;
	
	if (!IS_IPC(conn)) {
		/* Find all the groups this uid is in and
		   store them. Used by become_user() */
		setup_groups(conn->user,conn->uid,conn->gid,
			     &conn->ngroups,&conn->groups);
		
		/* check number of connections */
		if (!claim_connection(conn,
				      lp_servicename(SNUM(conn)),
				      lp_max_connections(SNUM(conn)),
				      False)) {
			DEBUG(1,("too many connections - rejected\n"));
			*ecode = ERRnoresource;
			conn_free(conn);
			return NULL;
		}  
		
		if (lp_status(SNUM(conn)))
			claim_connection(conn,"STATUS.",
					 MAXSTATUS,False);
	} /* IS_IPC */
	
	/* execute any "root preexec = " line */
	if (*lp_rootpreexec(SNUM(conn))) {
		pstring cmd;
		pstrcpy(cmd,lp_rootpreexec(SNUM(conn)));
		standard_sub(conn,cmd);
		DEBUG(5,("cmd=%s\n",cmd));
		ret = smbrun(cmd,NULL,False);
		if (ret != 0 && lp_rootpreexec_close(SNUM(conn))) {
			DEBUG(1,("preexec gave %d - failing connection\n", ret));
			conn_free(conn);
			*ecode = ERRsrverror;
			return NULL;
		}
	}
	
	if (!become_user(conn, conn->vuid)) {
		DEBUG(0,("Can't become connected user!\n"));
		if (!IS_IPC(conn)) {
			yield_connection(conn,
					 lp_servicename(SNUM(conn)),
					 lp_max_connections(SNUM(conn)));
			if (lp_status(SNUM(conn))) {
				yield_connection(conn,"STATUS.",MAXSTATUS);
			}
		}
		conn_free(conn);
		*ecode = ERRbadpw;
		return NULL;
	}
	
	if (dos_ChDir(conn->connectpath) != 0) {
		DEBUG(0,("Can't change directory to %s (%s)\n",
			 conn->connectpath,strerror(errno)));
		unbecome_user();
		if (!IS_IPC(conn)) {
			yield_connection(conn,
					 lp_servicename(SNUM(conn)),
					 lp_max_connections(SNUM(conn)));
			if (lp_status(SNUM(conn))) 
				yield_connection(conn,"STATUS.",MAXSTATUS);
		}
		conn_free(conn);
		*ecode = ERRinvnetname;
		return NULL;
	}
	
	string_set(&conn->origpath,conn->connectpath);
	
#if SOFTLINK_OPTIMISATION
	/* resolve any soft links early */
	{
		pstring s;
		pstrcpy(s,conn->connectpath);
		dos_GetWd(s);
		string_set(&conn->connectpath,s);
		dos_ChDir(conn->connectpath);
	}
#endif
	
	add_session_user(user);
		
	/* execute any "preexec = " line */
	if (*lp_preexec(SNUM(conn))) {
		pstring cmd;
		pstrcpy(cmd,lp_preexec(SNUM(conn)));
		standard_sub(conn,cmd);
		ret = smbrun(cmd,NULL,False);
		if (ret != 0 && lp_preexec_close(SNUM(conn))) {
			DEBUG(1,("preexec gave %d - failing connection\n", ret));
			conn_free(conn);
			*ecode = ERRsrverror;
			return NULL;
		}
	}

	/*
	 * Print out the 'connected as' stuff here as we need
	 * to know the effective uid and gid we will be using.
	 */

	if( DEBUGLVL( IS_IPC(conn) ? 3 : 1 ) ) {
		dbgtext( "%s (%s) ", remote_machine, conn->client_address );
		dbgtext( "connect to service %s ", lp_servicename(SNUM(conn)) );
		dbgtext( "as user %s ", user );
		dbgtext( "(uid=%d, gid=%d) ", (int)geteuid(), (int)getegid() );
		dbgtext( "(pid %d)\n", (int)getpid() );
	}
	
	/* we've finished with the sensitive stuff */
	unbecome_user();
	
	/* Add veto/hide lists */
	if (!IS_IPC(conn) && !IS_PRINT(conn)) {
		set_namearray( &conn->veto_list, lp_veto_files(SNUM(conn)));
		set_namearray( &conn->hide_list, lp_hide_files(SNUM(conn)));
		set_namearray( &conn->veto_oplock_list, lp_veto_oplocks(SNUM(conn)));
	}
	
	return(conn);
}


/****************************************************************************
close a cnum
****************************************************************************/
void close_cnum(connection_struct *conn, uint16 vuid)
{
	DirCacheFlush(SNUM(conn));

	unbecome_user();

	DEBUG(IS_IPC(conn)?3:1, ("%s (%s) closed connection to service %s\n",
				 remote_machine,conn->client_address,
				 lp_servicename(SNUM(conn))));

	yield_connection(conn,
			 lp_servicename(SNUM(conn)),
			 lp_max_connections(SNUM(conn)));

	if (lp_status(SNUM(conn)))
		yield_connection(conn,"STATUS.",MAXSTATUS);

	file_close_conn(conn);
	dptr_closecnum(conn);

	/* execute any "postexec = " line */
	if (*lp_postexec(SNUM(conn)) && 
	    become_user(conn, vuid))  {
		pstring cmd;
		pstrcpy(cmd,lp_postexec(SNUM(conn)));
		standard_sub(conn,cmd);
		smbrun(cmd,NULL,False);
		unbecome_user();
	}

	unbecome_user();
	/* execute any "root postexec = " line */
	if (*lp_rootpostexec(SNUM(conn)))  {
		pstring cmd;
		pstrcpy(cmd,lp_rootpostexec(SNUM(conn)));
		standard_sub(conn,cmd);
		smbrun(cmd,NULL,False);
	}
	
	conn_free(conn);
}
