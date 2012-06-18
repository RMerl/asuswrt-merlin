/* 
   Unix SMB/Netbios implementation.
   Version 2.0.
   SMB backend for the Common UNIX Printing System ("CUPS")
   Copyright 1999 by Easy Software Products
   Copyright Andrew Tridgell 1994-1998
   
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

#define NO_SYSLOG

#include "includes.h"

/*
 * Globals...
 */

extern BOOL		in_client;	/* Boolean for client library */
extern struct in_addr	ipzero;		/* Any address */


/*
 * Local functions...
 */

static struct cli_state	*smb_connect(char *, char *, char *, char *, char *);
static int		smb_print(struct cli_state *, char *, FILE *);


/*
 * 'main()' - Main entry for SMB backend.
 */

 int				/* O - Exit status */
 main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		copies;		/* Number of copies */
  char		uri[1024],	/* URI */
		*sep,		/* Pointer to separator */
		*username,	/* Username */
		*password,	/* Password */
		*workgroup,	/* Workgroup */
		*server,	/* Server name */
		*printer;	/* Printer name */
  FILE		*fp;		/* File to print */
  int		status;		/* Status of LPD job */
  struct cli_state *cli;	/* SMB interface */

  /* we expect the URI in argv[0]. Detect the case where it is in argv[1] and cope */
  if (argc > 2 && strncmp(argv[0],"smb://", 6) && !strncmp(argv[1],"smb://", 6)) {
	  argv++;
	  argc--;
  }


  if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s [DEVICE_URI] job-id user title copies options [file]\n",
            argv[0]);
    fputs("       The DEVICE_URI environment variable can also contain the\n", stderr);
    fputs("       destination printer:\n", stderr);
    fputs("\n", stderr);
    fputs("           smb://[username:password@][workgroup/]server/printer\n", stderr);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, print data from stdin...
  */

  if (argc == 6)
  {
   /*
    * Print from Copy stdin to a temporary file...
    */

    fp     = stdin;
    copies = 1;
  }
  else if ((fp = fopen(argv[6], "rb")) == NULL)
  {
    perror("ERROR: Unable to open print file");
    return (1);
  }
  else
    copies = atoi(argv[4]);

 /*
  * Fine the URI...
  */

  if (strncmp(argv[0], "smb://", 6) == 0)
    strncpy(uri, argv[0], sizeof(uri) - 1);
  else if (getenv("DEVICE_URI") != NULL)
    strncpy(uri, getenv("DEVICE_URI"), sizeof(uri) - 1);
  else
  {
    fputs("ERROR: No device URI found in argv[0] or DEVICE_URI environment variable!\n", stderr);
    return (1);
  }

  uri[sizeof(uri) - 1] = '\0';

 /*
  * Extract the destination from the URI...
  */

  if ((sep = strrchr(uri, '@')) != NULL)
  {
    username = uri + 6;
    *sep++ = '\0';

    server = sep;

   /*
    * Extract password as needed...
    */

    if ((password = strchr(username, ':')) != NULL)
      *password++ = '\0';
    else
      password = "";
  }
  else
  {
    username = "";
    password = "";
    server   = uri + 6;
  }

  if ((sep = strchr(server, '/')) == NULL)
  {
    fputs("ERROR: Bad URI - need printer name!\n", stderr);
    return (1);
  }

  *sep++ = '\0';
  printer = sep;

  if ((sep = strchr(printer, '/')) != NULL)
  {
   /*
    * Convert to smb://[username:password@]workgroup/server/printer...
    */

    *sep++ = '\0';

    workgroup = server;
    server    = printer;
    printer   = sep;
  }
  else
    workgroup = NULL;

 /*
  * Setup the SAMBA server state...
  */

  setup_logging("smbspool", True);

  TimeInit();
  charset_initialise();

  in_client = True;   /* Make sure that we tell lp_load we are */

  if (!lp_load(CONFIGFILE, True, False, False))
  {
    fprintf(stderr, "ERROR: Can't load %s - run testparm to debug it\n", CONFIGFILE);
    return (1);
  }

  if (workgroup == NULL)
    workgroup = lp_workgroup();

  codepage_initialise(lp_client_code_page());

  load_interfaces();

  if ((cli = smb_connect(workgroup, server, printer, username, password)) == NULL)
  {
    perror("ERROR: Unable to connect to SAMBA host");
    return (1);
  }

 /*
  * Queue the job...
  */

  for (i = 0; i < copies; i ++)
    if ((status = smb_print(cli, argv[3] /* title */, fp)) != 0)
      break;

  cli_shutdown(cli);

 /*
  * Return the queue status...
  */

  return (status);
}


/*
 * 'smb_connect()' - Return a connection to a server.
 */

static struct cli_state *		/* O - SMB connection */
smb_connect(char *workgroup,		/* I - Workgroup */
            char *server,		/* I - Server */
            char *share,		/* I - Printer */
            char *username,		/* I - Username */
            char *password)		/* I - Password */
{
  struct cli_state	*c;		/* New connection */
  struct nmb_name	called,		/* NMB name of server */
			calling;	/* NMB name of client */
  struct in_addr	ip;		/* IP address of server */
  pstring		myname;		/* Client name */


 /*
  * Get the names and addresses of the client and server...
  */

  get_myname(myname);  

  ip = ipzero;

  make_nmb_name(&calling, myname, 0x0);
  make_nmb_name(&called, server, 0x20);

 /*
  * Open a new connection to the SMB server...
  */

  if ((c = cli_initialise(NULL)) == NULL)
  {
    fputs("ERROR: cli_initialize() failed...\n", stderr);
    return (NULL);
  }

  if (!cli_set_port(c, SMB_PORT))
  {
    fputs("ERROR: cli_set_port() failed...\n", stderr);
    return (NULL);
  }

  if (!cli_connect(c, server, &ip))
  {
    fputs("ERROR: cli_connect() failed...\n", stderr);
    return (NULL);
  }

  if (!cli_session_request(c, &calling, &called))
  {
    fputs("ERROR: cli_session_request() failed...\n", stderr);
    return (NULL);
  }

  if (!cli_negprot(c))
  {
    fputs("ERROR: SMB protocol negotiation failed\n", stderr);
    cli_shutdown(c);
    return (NULL);
  }

 /*
  * Do password stuff...
  */

  if (!cli_session_setup(c, username, 
			 password, strlen(password),
			 password, strlen(password),
			 workgroup))
  {
    fprintf(stderr, "ERROR: SMB session setup failed: %s\n", cli_errstr(c));
    return (NULL);
  }

  if (!cli_send_tconX(c, share, "?????",
		      password, strlen(password)+1))
  {
    fprintf(stderr, "ERROR: SMB tree connect failed: %s\n", cli_errstr(c));
    cli_shutdown(c);
    return (NULL);
  }

 /*
  * Return the new connection...
  */

  return (c);
}


/*
 * 'smb_print()' - Queue a job for printing using the SMB protocol.
 */

static int				/* O - 0 = success, non-0 = failure */
smb_print(struct cli_state *cli,	/* I - SMB connection */
          char             *title,	/* I - Title/job name */
          FILE             *fp)		/* I - File to print */
{
  int	fnum;		/* File number */
  int	nbytes,		/* Number of bytes read */
	tbytes;		/* Total bytes read */
  char	buffer[8192];	/* Buffer for copy */


 /*
  * Open the printer device...
  */

  if ((fnum = cli_open(cli, title, O_WRONLY | O_CREAT | O_TRUNC, DENY_NONE)) == -1)
  {
    fprintf(stderr, "ERROR: %s opening remote file %s\n",
            cli_errstr(cli), title);
    return (1);
  }

 /*
  * Copy the file to the printer...
  */

  if (fp != stdin)
    rewind(fp);

  tbytes = 0;

  while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
  {
    if (cli_write(cli, fnum, 0, buffer, tbytes, nbytes) != nbytes)
    {
      fprintf(stderr, "ERROR: Error writing file: %s\n", cli_errstr(cli));
      break;
    }

    tbytes += nbytes;
  } 

  if (!cli_close(cli, fnum))
  {
    fprintf(stderr, "ERROR: %s closing remote file %s\n",
            cli_errstr(cli), title);
    return (1);
  }
  else
    return (0);
}
