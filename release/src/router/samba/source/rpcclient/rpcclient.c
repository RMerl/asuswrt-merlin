/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB client
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
*/

#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"

#ifndef REGISTER
#define REGISTER 0
#endif

extern pstring debugf;
extern pstring global_myname;

extern pstring user_socket_options;


extern int DEBUGLEVEL;


extern file_info def_finfo;

#define CNV_LANG(s) dos2unix_format(s,False)
#define CNV_INPUT(s) unix2dos_format(s,True)

static int process_tok(fstring tok);
static void cmd_help(struct client_info *info);
static void cmd_quit(struct client_info *info);

static struct cli_state smbcli;
struct cli_state *smb_cli = &smbcli;

FILE *out_hnd;

/****************************************************************************
initialise smb client structure
****************************************************************************/
void rpcclient_init(void)
{
	memset((char *)smb_cli, '\0', sizeof(smb_cli));
	cli_initialise(smb_cli);
	smb_cli->capabilities |= CAP_NT_SMBS | CAP_STATUS32;
}

/****************************************************************************
make smb client connection
****************************************************************************/
static BOOL rpcclient_connect(struct client_info *info)
{
	struct nmb_name calling;
	struct nmb_name called;

	make_nmb_name(&called , dns_to_netbios_name(info->dest_host ), info->name_type);
	make_nmb_name(&calling, dns_to_netbios_name(info->myhostname), 0x0);

	if (!cli_establish_connection(smb_cli, 
	                          info->dest_host, &info->dest_ip, 
	                          &calling, &called,
	                          info->share, info->svc_type,
	                          False, True))
	{
		DEBUG(0,("rpcclient_connect: connection failed\n"));
		cli_shutdown(smb_cli);
		return False;
	}

	return True;
}

/****************************************************************************
stop the smb connection(s?)
****************************************************************************/
static void rpcclient_stop(void)
{
	cli_shutdown(smb_cli);
}
/****************************************************************************
 This defines the commands supported by this client
 ****************************************************************************/
struct
{
  char *name;
  void (*fn)(struct client_info*);
  char *description;
} commands[] = 
{
  {"regenum",    cmd_reg_enum,         "<keyname> Registry Enumeration (keys, values)"},
  {"regdeletekey",cmd_reg_delete_key,  "<keyname> Registry Key Delete"},
  {"regcreatekey",cmd_reg_create_key,  "<keyname> [keyclass] Registry Key Create"},
  {"regquerykey",cmd_reg_query_key,    "<keyname> Registry Key Query"},
  {"regdeleteval",cmd_reg_delete_val,  "<valname> Registry Value Delete"},
  {"regcreateval",cmd_reg_create_val,  "<valname> <valtype> <value> Registry Key Create"},
  {"reggetsec",  cmd_reg_get_key_sec,  "<keyname> Registry Key Security"},
  {"regtestsec", cmd_reg_test_key_sec, "<keyname> Test Registry Key Security"},
  {"ntlogin",    cmd_netlogon_login_test, "[username] [password] NT Domain login test"},
  {"wksinfo",    cmd_wks_query_info,   "Workstation Query Info"},
  {"srvinfo",    cmd_srv_query_info,   "Server Query Info"},
  {"srvsessions",cmd_srv_enum_sess,    "List sessions on a server"},
  {"srvshares",  cmd_srv_enum_shares,  "List shares on a server"},
  {"srvconnections",cmd_srv_enum_conn, "List connections on a server"},
  {"srvfiles",   cmd_srv_enum_files,   "List files on a server"},
  {"lsaquery",   cmd_lsa_query_info,   "Query Info Policy (domain member or server)"},
  {"lookupsids", cmd_lsa_lookup_sids,  "Resolve names from SIDs"},
  {"enumusers",  cmd_sam_enum_users,   "SAM User Database Query (experimental!)"},
  {"ntpass",     cmd_sam_ntchange_pwd, "NT SAM Password Change"},
  {"samuser",    cmd_sam_query_user,   "<username> SAM User Query (experimental!)"},
  {"samtest",    cmd_sam_test      ,   "SAM User Encrypted RPC test (experimental!)"},
  {"enumaliases",cmd_sam_enum_aliases, "SAM Aliases Database Query (experimental!)"},
#if 0
  {"enumgroups", cmd_sam_enum_groups,  "SAM Group Database Query (experimental!)"},
#endif
  {"samgroups",  cmd_sam_query_groups, "SAM Group Database Query (experimental!)"},
  {"quit",       cmd_quit,        "logoff the server"},
  {"q",          cmd_quit,        "logoff the server"},
  {"exit",       cmd_quit,        "logoff the server"},
  {"bye",        cmd_quit,        "logoff the server"},
  {"help",       cmd_help,        "[command] give help on a command"},
  {"?",          cmd_help,        "[command] give help on a command"},
  {"!",          NULL,            "run a shell command on the local system"},
  {"",           NULL,            NULL}
};


/****************************************************************************
do a (presumably graceful) quit...
****************************************************************************/
static void cmd_quit(struct client_info *info)
{
	rpcclient_stop();
#ifdef MEM_MAN
	{
		extern FILE* dbf;
		smb_mem_write_status(dbf);
		smb_mem_write_errors(dbf);
		smb_mem_write_verbose(dbf);
	}
#endif
	exit(0);
}

/****************************************************************************
help
****************************************************************************/
static void cmd_help(struct client_info *info)
{
  int i=0,j;
  fstring buf;

  if (next_token(NULL,buf,NULL, sizeof(buf)))
    {
      if ((i = process_tok(buf)) >= 0)
	fprintf(out_hnd, "HELP %s:\n\t%s\n\n",commands[i].name,commands[i].description);		    
    }
  else
    while (commands[i].description)
      {
	for (j=0; commands[i].description && (j<5); j++) {
	  fprintf(out_hnd, "%-15s",commands[i].name);
	  i++;
	}
	fprintf(out_hnd, "\n");
      }
}

/*******************************************************************
  lookup a command string in the list of commands, including 
  abbreviations
  ******************************************************************/
static int process_tok(fstring tok)
{
  int i = 0, matches = 0;
  int cmd=0;
  int tok_len = strlen(tok);
  
  while (commands[i].fn != NULL)
    {
      if (strequal(commands[i].name,tok))
	{
	  matches = 1;
	  cmd = i;
	  break;
	}
      else if (strnequal(commands[i].name, tok, tok_len))
	{
	  matches++;
	  cmd = i;
	}
      i++;
    }
  
  if (matches == 0)
    return(-1);
  else if (matches == 1)
    return(cmd);
  else
    return(-2);
}

/****************************************************************************
wait for keyboard activity, swallowing network packets
****************************************************************************/
static void wait_keyboard(struct cli_state *cli)
{
  fd_set fds;
  struct timeval timeout;
  
  while (1) 
    {
      FD_ZERO(&fds);
      FD_SET(cli->fd,&fds);
      FD_SET(fileno(stdin),&fds);

      timeout.tv_sec = 20;
      timeout.tv_usec = 0;
      sys_select(MAX(cli->fd,fileno(stdin))+1,&fds,&timeout);
      
      if (FD_ISSET(fileno(stdin),&fds))
  	return;

      /* We deliberately use receive_smb instead of
         client_receive_smb as we want to receive
         session keepalives and then drop them here.
       */
      if (FD_ISSET(cli->fd,&fds))
  	receive_smb(cli->fd,cli->inbuf,0);
    }  
}

/****************************************************************************
  process commands from the client
****************************************************************************/
static void do_command(struct client_info *info, char *tok, char *line)
{
	int i;

	if ((i = process_tok(tok)) >= 0)
	{
		commands[i].fn(info);
	}
	else if (i == -2)
	{
		fprintf(out_hnd, "%s: command abbreviation ambiguous\n", CNV_LANG(tok));
	}
	else
	{
		fprintf(out_hnd, "%s: command not found\n", CNV_LANG(tok));
	}
}

/****************************************************************************
  process commands from the client
****************************************************************************/
static BOOL process( struct client_info *info, char *cmd_str)
{
	pstring line;
	char *cmd = cmd_str;

	if (cmd[0] != '\0') while (cmd[0] != '\0')
	{
		char *p;
		fstring tok;

		if ((p = strchr(cmd, ';')) == 0)
		{
			strncpy(line, cmd, 999);
			line[1000] = '\0';
			cmd += strlen(cmd);
		}
		else
		{
			if (p - cmd > 999) p = cmd + 999;
			strncpy(line, cmd, p - cmd);
			line[p - cmd] = '\0';
			cmd = p + 1;
		}

		/* input language code to internal one */
		CNV_INPUT (line);

		/* get the first part of the command */
		{
			char *ptr = line;
			if (!next_token(&ptr,tok,NULL, sizeof(tok))) continue;
		}

		do_command(info, tok, line);
	}
	else while (!feof(stdin))
	{
		fstring tok;

		/* display a prompt */
		fprintf(out_hnd, "smb: %s> ", CNV_LANG(info->cur_dir));
		fflush(out_hnd);

#ifdef CLIX
		line[0] = wait_keyboard(smb_cli);
		/* this might not be such a good idea... */
		if ( line[0] == EOF)
		{
			break;
		}
#else
		wait_keyboard(smb_cli);
#endif

		/* and get a response */
#ifdef CLIX
		fgets( &line[1],999, stdin);
#else
		if (!fgets(line,1000,stdin))
		{
			break;
		}
#endif

		/* input language code to internal one */
		CNV_INPUT (line);

		/* special case - first char is ! */
		if (*line == '!')
		{
			system(line + 1);
			continue;
		}

		fprintf(out_hnd, "%s\n", line);

		/* get the first part of the command */
		{
			char *ptr = line;
			if (!next_token(&ptr,tok,NULL, sizeof(tok))) continue;
		}

		do_command(info, tok, line);
	}

	return(True);
}

/****************************************************************************
usage on the program
****************************************************************************/
static void usage(char *pname)
{
  fprintf(out_hnd, "Usage: %s service <password> [-d debuglevel] [-l log] ",
	   pname);

  fprintf(out_hnd, "\nVersion %s\n",VERSION);
  fprintf(out_hnd, "\t-d debuglevel         set the debuglevel\n");
  fprintf(out_hnd, "\t-l log basename.      Basename for log/debug files\n");
  fprintf(out_hnd, "\t-n netbios name.      Use this name as my netbios name\n");
  fprintf(out_hnd, "\t-N                    don't ask for a password\n");
  fprintf(out_hnd, "\t-m max protocol       set the max protocol level\n");
  fprintf(out_hnd, "\t-I dest IP            use this IP to connect to\n");
  fprintf(out_hnd, "\t-E                    write messages to stderr instead of stdout\n");
  fprintf(out_hnd, "\t-U username           set the network username\n");
  fprintf(out_hnd, "\t-W workgroup          set the workgroup name\n");
  fprintf(out_hnd, "\t-c command string     execute semicolon separated commands\n");
  fprintf(out_hnd, "\t-t terminal code      terminal i/o code {sjis|euc|jis7|jis8|junet|hex}\n");
  fprintf(out_hnd, "\n");
}

enum client_action
{
	CLIENT_NONE,
	CLIENT_IPC,
	CLIENT_SVC
};

/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	BOOL interactive = True;

	int opt;
	extern FILE *dbf;
	extern char *optarg;
	extern int optind;
	static pstring servicesf = CONFIGFILE;
	pstring term_code;
	char *p;
	BOOL got_pass = False;
	char *cmd_str="";
	mode_t myumask = 0755;
	enum client_action cli_action = CLIENT_NONE;

	struct client_info cli_info;

	pstring password; /* local copy only, if one is entered */

	out_hnd = stdout;
	fstrcpy(debugf, argv[0]);

	rpcclient_init();

#ifdef KANJI
	pstrcpy(term_code, KANJI);
#else /* KANJI */
	*term_code = 0;
#endif /* KANJI */

	DEBUGLEVEL = 2;

	cli_info.put_total_size = 0;
	cli_info.put_total_time_ms = 0;
	cli_info.get_total_size = 0;
	cli_info.get_total_time_ms = 0;

	cli_info.dir_total = 0;
	cli_info.newer_than = 0;
	cli_info.archive_level = 0;
	cli_info.print_mode = 1;

	cli_info.translation = False;
	cli_info.recurse_dir = False;
	cli_info.lowercase = False;
	cli_info.prompt = True;
	cli_info.abort_mget = True;

	cli_info.dest_ip.s_addr = 0;
	cli_info.name_type = 0x20;

	pstrcpy(cli_info.cur_dir , "\\");
	pstrcpy(cli_info.file_sel, "");
	pstrcpy(cli_info.base_dir, "");
	pstrcpy(smb_cli->domain, "");
	pstrcpy(smb_cli->user_name, "");
	pstrcpy(cli_info.myhostname, "");
	pstrcpy(cli_info.dest_host, "");

	pstrcpy(cli_info.svc_type, "A:");
	pstrcpy(cli_info.share, "");
	pstrcpy(cli_info.service, "");

	ZERO_STRUCT(cli_info.dom.level3_sid);
	ZERO_STRUCT(cli_info.dom.level5_sid);
	fstrcpy(cli_info.dom.level3_dom, "");
	fstrcpy(cli_info.dom.level5_dom, "");

	smb_cli->nt_pipe_fnum   = 0xffff;

	TimeInit();
	charset_initialise();

	myumask = umask(0);
	umask(myumask);

	if (!get_myname(global_myname))
	{
		fprintf(stderr, "Failed to get my hostname.\n");
	}

	if (getenv("USER"))
	{
		pstrcpy(smb_cli->user_name,getenv("USER"));

		/* modification to support userid%passwd syntax in the USER var
		25.Aug.97, jdblair@uab.edu */

		if ((p=strchr(smb_cli->user_name,'%')))
		{
			*p = 0;
			pstrcpy(password,p+1);
			got_pass = True;
			memset(strchr(getenv("USER"),'%')+1,'X',strlen(password));
		}
		strupper(smb_cli->user_name);
	}

	password[0] = 0;

	/* modification to support PASSWD environmental var
	   25.Aug.97, jdblair@uab.edu */
	if (getenv("PASSWD"))
	{
		pstrcpy(password,getenv("PASSWD"));
	}

	if (*smb_cli->user_name == 0 && getenv("LOGNAME"))
	{
		pstrcpy(smb_cli->user_name,getenv("LOGNAME"));
		strupper(smb_cli->user_name);
	}

	if (argc < 2)
	{
		usage(argv[0]);
		exit(1);
	}

	if (*argv[1] != '-')
	{

		pstrcpy(cli_info.service, argv[1]);  
		/* Convert any '/' characters in the service name to '\' characters */
		string_replace( cli_info.service, '/','\\');
		argc--;
		argv++;

		fprintf(out_hnd, "service: %s\n", cli_info.service);

		if (count_chars(cli_info.service,'\\') < 3)
		{
			usage(argv[0]);
			printf("\n%s: Not enough '\\' characters in service\n", cli_info.service);
			exit(1);
		}

		/*
		if (count_chars(cli_info.service,'\\') > 3)
		{
			usage(pname);
			printf("\n%s: Too many '\\' characters in service\n", cli_info.service);
			exit(1);
		}
		*/

		if (argc > 1 && (*argv[1] != '-'))
		{
			got_pass = True;
			pstrcpy(password,argv[1]);  
			memset(argv[1],'X',strlen(argv[1]));
			argc--;
			argv++;
		}

		cli_action = CLIENT_SVC;
	}

	while ((opt = getopt(argc, argv,"s:O:M:S:i:N:n:d:l:hI:EB:U:L:t:m:W:T:D:c:")) != EOF)
	{
		switch (opt)
		{
			case 'm':
			{
				/* FIXME ... max_protocol seems to be funny here */

				int max_protocol = 0;
				max_protocol = interpret_protocol(optarg,max_protocol);
				fprintf(stderr, "max protocol not currently supported\n");
				break;
			}

			case 'O':
			{
				pstrcpy(user_socket_options,optarg);
				break;	
			}

			case 'S':
			{
				pstrcpy(cli_info.dest_host,optarg);
				strupper(cli_info.dest_host);
				cli_action = CLIENT_IPC;
				break;
			}

			case 'i':
			{
				extern pstring global_scope;
				pstrcpy(global_scope, optarg);
				strupper(global_scope);
				break;
			}

			case 'U':
			{
				char *lp;
				pstrcpy(smb_cli->user_name,optarg);
				if ((lp=strchr(smb_cli->user_name,'%')))
				{
					*lp = 0;
					pstrcpy(password,lp+1);
					got_pass = True;
					memset(strchr(optarg,'%')+1,'X',strlen(password));
				}
				break;
			}

			case 'W':
			{
				pstrcpy(smb_cli->domain,optarg);
				break;
			}

			case 'E':
			{
				dbf = stderr;
				break;
			}

			case 'I':
			{
				cli_info.dest_ip = *interpret_addr2(optarg);
				if (zero_ip(cli_info.dest_ip))
				{
					exit(1);
				}
				break;
			}

			case 'n':
			{
				fstrcpy(global_myname, optarg);
				break;
			}

			case 'N':
			{
				got_pass = True;
				break;
			}

			case 'd':
			{
				if (*optarg == 'A')
					DEBUGLEVEL = 10000;
				else
					DEBUGLEVEL = atoi(optarg);
				break;
			}

			case 'l':
			{
				slprintf(debugf, sizeof(debugf)-1,
				         "%s.client", optarg);
				interactive = False;
				break;
			}

			case 'c':
			{
				cmd_str = optarg;
				got_pass = True;
				break;
			}

			case 'h':
			{
				usage(argv[0]);
				exit(0);
				break;
			}

			case 's':
			{
				pstrcpy(servicesf, optarg);
				break;
			}

			case 't':
			{
				pstrcpy(term_code, optarg);
				break;
			}

			default:
			{
				usage(argv[0]);
				exit(1);
				break;
			}
		}
	}

	setup_logging(debugf, interactive);

	if (cli_action == CLIENT_NONE)
	{
		usage(argv[0]);
		exit(1);
	}

	strupper(global_myname);
	fstrcpy(cli_info.myhostname, global_myname);

	DEBUG(3,("%s client started (version %s)\n",timestring(False),VERSION));

	if (!lp_load(servicesf,True, False, False))
	{
		fprintf(stderr, "Can't load %s - run testparm to debug it\n", servicesf);
	}

	codepage_initialise(lp_client_code_page());

	if (*smb_cli->domain == 0) pstrcpy(smb_cli->domain,lp_workgroup());

	load_interfaces();

	if (cli_action == CLIENT_IPC)
	{
		pstrcpy(cli_info.share, "IPC$");
		pstrcpy(cli_info.svc_type, "IPC");
	}

	fstrcpy(cli_info.mach_acct, cli_info.myhostname);
	strupper(cli_info.mach_acct);
	fstrcat(cli_info.mach_acct, "$");

	/* set the password cache info */
	if (got_pass)
	{
		if (password[0] == 0)
		{
			pwd_set_nullpwd(&(smb_cli->pwd));
		}
		else
		{
			pwd_make_lm_nt_16(&(smb_cli->pwd), password); /* generate 16 byte hashes */
		}
	}
	else 
	{
		pwd_read(&(smb_cli->pwd), "Enter Password:", True);
	}

	/* paranoia: destroy the local copy of the password */
	memset((char *)password, '\0', sizeof(password)); 

	/* establish connections.  nothing to stop these being re-established. */
	rpcclient_connect(&cli_info);

	DEBUG(5,("rpcclient_connect: smb_cli->fd:%d\n", smb_cli->fd));
	if (smb_cli->fd <= 0)
	{
		fprintf(stderr, "warning: connection could not be established to %s<%02x>\n",
		                 cli_info.dest_host, cli_info.name_type);
		fprintf(stderr, "this version of smbclient may crash if you proceed\n");
		exit(-1);
	}

	switch (cli_action)
	{
		case CLIENT_IPC:
		{
			process(&cli_info, cmd_str);
			break;
		}

		default:
		{
			fprintf(stderr, "unknown client action requested\n");
			break;
		}
	}

	rpcclient_stop();

	return(0);
}
