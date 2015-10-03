/**********************************************************
Date: 		OCT 28th, 2006
Project :	NET4900 Project: tftpd.c TFTP Server

Programers:	
Craig Holmes
Reza Rahmanian 


File:		TFTP Server (main)
Purpose:	A TFTP server that will accept a connections from
		a client and transefet files.
Notes:		Here we are using the sendto and recvfrom
		functions so the server and client can exchange data.
***********************************************************************/

/* Include our header which contains libaries and defines */
#include "tftp.h"



/* Function prototypes */
void tsend (char *, struct sockaddr_in, char *, int);
void tget (char *, struct sockaddr_in, char *, int);
int isnotvaliddir (char *);
void usage (void);

/* default values which can be controlled by command line */
int debug = 0;
char path[64] = "/tmp/";
int port = 69;
unsigned short int ackfreq = 1;
int datasize = 512;

int
main (int argc, char **argv)
{
  /*local variables */
  extern char *optarg;
  int sock, n, client_len, pid, status, opt, tid;
  char opcode, *bufindex, filename[196], mode[12];

  struct sockaddr_in server, client;	/*the address structure for both the server and client */

/* All of the following deals with command line switches */
  while ((opt = getopt (argc, argv, "dh:p:P:a:s:")) != -1)	/* this function is handy */
    {
      switch (opt)
	{
	case 'p':		/* path (opt required) */
	  if (!isnotvaliddir (optarg))
	    {
	      printf
		("Sorry, you specified an invalid/non-existant directory. Make sure the directory exists.\n");
	      return 0;
	    }
	  strncpy (path, optarg, sizeof (path) - 1);


	  break;
	case 'd':		/* debug mode (no opts) */
	  debug = 1;
	  break;
	case 'P':		/* Port (opt required) */
	  port = atoi (optarg);
	  break;
	case 'a':		/* ack frequency (opt required) */
	  ackfreq = atoi (optarg);
	  if (ackfreq > MAXACKFREQ)
	    {
	      printf
		("Sorry, you specified an ack frequency higher than the maximum allowed (Requested: %d Max: %d)\n",
		 ackfreq, MAXACKFREQ);
	      return 0;
	    }
	  else if (ackfreq == 0)
	    {
	      printf ("Sorry, you have to ack sometime.\n");
	      return 0;
	    }
	  break;
	case 's':		/* File chunk size (opt required) */
	  datasize = atoi (optarg);

	  if (datasize > MAXDATASIZE)
	    {
	      printf
		("Sorry, you specified a data size higher than the maximum allowed (Requested: %d Max: %d)\n",
		 datasize, MAXDATASIZE);
	      return 0;
	    }

	  break;
	case 'h':		/* Help (no opts) */
	  usage ();
	  return (0);
	  break;
	default:		/* everything else */
	  usage ();
	  return (0);
	  break;

	}
    }
/* Done dealing with switches */

  /*Create the socket, a -1 will show us an error */
  if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
      printf ("Server Socket could not be created");
      return 0;
    }
  /*set the address values for the server */
  server.sin_family = AF_INET;	/*address family for TCP and UDP */
  server.sin_addr.s_addr = htonl (INADDR_ANY);	/*use any address */
  server.sin_port = htons (port);	/*pick a free port */

  /*Bind the socket */
  if (bind (sock, (struct sockaddr *) &server, sizeof (server)) < 0)
    {
      printf
	("Server bind failed. Server already running? Proper permissions?\n");
      return (2);
    }
  if (!debug)
    {
      pid = fork ();

      if (pid != 0)		/* if pid != 0 then we are the parent */
	{
	  if (pid == -1)
	    {
	      printf ("Error: Fork failed!\n");
	      return 0;
	    }
	  else
	    {
	      printf ("Daemon Successfully forked (pid: %d)\n", pid);
	      return 1;
	    }
	}


    }
  else
    {
      printf
	("tftpd server is running in debug mode and will not fork. \nMulti-threaded mode disabled.\nServer is bound to port %d and awaiting connections\nfile path: %s\n",
	 ntohs (server.sin_port), path);
    }
  /*endless loop to get connections from the client */
  while (1)
    {
      client_len = sizeof (client);	/*get the length of the client */
      memset (buf, 0, BUFSIZ);	/*clear the buffer */
      /*the fs message */
      n = 0;
      while (errno == EAGAIN || n == 0)	/* This loop is required because on linux we have to acknowledge complete children with waitpid. Ugh. */
	{
	  waitpid (-1, &status, WNOHANG);
	  n =
	    recvfrom (sock, buf, BUFSIZ, MSG_DONTWAIT,
		      (struct sockaddr *) &client,
		      (socklen_t *) & client_len);
	  if (n < 0 && errno != EAGAIN)
	    {
	      printf ("The server could not receive from the client");
	      return 0;
	    }

	  usleep (1000);
	}

      if (debug)
	printf ("Connection from %s, port %d\n",
		inet_ntoa (client.sin_addr), ntohs (client.sin_port));

      bufindex = buf;		//start our pointer going
      if (bufindex++[0] != 0x00)
	{			//first TFTP packet byte needs to be null. Once the value is taken increment it.
	  if (debug)
	    printf ("Malformed tftp packet.\n");

	  return 0;
	}
      tid = ntohs (client.sin_port);	/* record the tid */
      opcode = *bufindex++;	//opcode is in the second byte.
      if (opcode == 1 || opcode == 2)	// RRQ or WRQ. The only two really valid packets on port 69
	{
	  strncpy (filename, bufindex, sizeof (filename) - 1);	/* Our pointer has been nudged along the recieved string so the first char is the beginning of the filename. This filename is null deliimited so we can use the str family of functions */
	  bufindex += strlen (filename) + 1;	/* move the pointer to after the filename + null byte in the string */
	  strncpy (mode, bufindex, sizeof (mode) - 1);	/* like the filename, we are at the beginning of the null delimited mode */
	  bufindex += strlen (mode) + 1;	/* move pointer... */
	  if (debug)
	    printf ("opcode: %x filename: %s packet size: %d mode: %s\n", opcode, filename, n, mode);	/*show the message to the server */

	}
      else
	{
	  if (debug)
	    printf ("opcode: %x size: %d \n", opcode, sizeof (n));	/*show the message to the server */

	}
      switch (opcode)		/* case one and two are valid on port 69 or server port... no other codes are */
	{
	case 1:
	  if (debug)
	    {
	      printf ("Opcode indicates file read request\n");
	      tsend (filename, client, mode, tid);
	    }
	  else
	    {
	      pid = fork ();

	      if (pid == 0)
		{		/* if we are pid != 0 then we are the parent */

		  tsend (filename, client, mode, tid);
		  exit (1);
		}
	    }
	  break;
	case 2:
	  if (debug)
	    {
	      printf ("Opcode indicates file write request\n");
	      tget (filename, client, mode, tid);
	    }
	  else
	    {
	      pid = fork ();

	      if (pid == 0)	/* if we are pid != 0 then we are the parent */
		{
		  tget (filename, client, mode, tid);
		  exit (1);
		}
	    }
	  break;
	default:
	  if (debug)
	    printf ("Invalid opcode detected. Ignoring packet.");
	  break;




	}			//end of while
    }
}

void
tget (char *pFilename, struct sockaddr_in client, char *pMode, int tid)
{
  /* local variables */
  int sock, len, client_len, opcode, i, j , n, flag = 1;
  unsigned short int count = 0, rcount = 0;
  unsigned char filebuf[MAXDATASIZE + 1];
  unsigned char packetbuf[MAXDATASIZE + 12];
  extern int errno;
  char filename[128], mode[12], fullpath[196], *bufindex, ackbuf[512];
  struct sockaddr_in data;
  FILE *fp;			/* pointer to the file we will be getting */

  strcpy (filename, pFilename);	//copy the pointer to the filename into a real array
  strcpy (mode, pMode);		//same as above


  if (debug)
    printf ("branched to file receive function\n");

  if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)	//startup a socket
    {
      printf ("Server reconnect for getting did not work correctly\n");
      return;
    }
  if (!strncasecmp (mode, "octet", 5) && !strncasecmp (mode, "netascii", 8))	/* these two are the only modes we accept */
    {
      if (!strncasecmp (mode, "mail", 4))
	len = sprintf ((char *) packetbuf,
		       "%c%c%c%cThis tftp server will not operate as a mail relay%c",
		       0x00, 0x05, 0x00, 0x04, 0x00);
      else
	len = sprintf ((char *) packetbuf,
		       "%c%c%c%cUnrecognized mode (%s)%c",
		       0x00, 0x05, 0x00, 0x04, mode, 0x00);
      if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  printf
	    ("Mismatch in number of sent bytes while trying to send mode error packet\n");
	}
      return;
    }
  if (strchr (filename, 0x5C) || strchr (filename, 0x2F))	//look for illegal characters in the filename string these are \ and /
    {
      if (debug)
	printf ("Client requested to upload bad file: forbidden name\n");
      len =
	sprintf ((char *) packetbuf,
		 "%c%c%c%cIllegal filename.(%s) You may not attempt to descend or ascend directories.%c",
		 0x00, 0x05, 0x00, 0x00, filename, 0x00);
      if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  printf
	    ("Mismatch in number of sent bytes while trying to send error packet\n");
	}
      return;

    }
  strcpy (fullpath, path);
  strncat (fullpath, filename, sizeof (fullpath) - 1);	//build the full file path by appending filename to path
  fp = fopen (fullpath, "w");	/* open the file for writing */
  if (fp == NULL)
    {				//if the pointer is null then the file can't be opened - Bad perms 
      if (debug)
	printf ("Server requested bad file: cannot open for writing (%s)\n",
		fullpath);
      len =
	sprintf ((char *) packetbuf,
		 "%c%c%c%cFile cannot be opened for writing (%s)%c", 0x00,
		 0x05, 0x00, 0x02, fullpath, 0x00);
      if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  printf
	    ("Mismatch in number of sent bytes while trying to send error packet\n");
	}
      return;
    }
  else				/* everything worked fine */
    {
      if (debug)
	printf ("Getting file... (destination: %s) \n", fullpath);

    }
  /* zero the buffer before we begin */
  memset (filebuf, 0, sizeof (filebuf));
  n = datasize + 4;
  do
    {
      /* zero buffers so if there are any errors only NULLs will be exposed */
      memset (packetbuf, 0, sizeof (packetbuf));
      memset (ackbuf, 0, sizeof (ackbuf));
      if (debug)
	printf ("== just entered do-while count: %d  n: %d\n", count, n);


      if (count == 0 || (count % ackfreq) == 0 || n != (datasize + 4))	/* ack the first packet, count % ackfreq will make it so we only ACK everyone ackfreq ACKs, ack the last packet */
	{
	  len = sprintf (ackbuf, "%c%c%c%c", 0x00, 0x04, 0x00, 0x00);
	  ackbuf[2] = (count & 0xFF00) >> 8;	//fill in the count (top number first)
	  ackbuf[3] = (count & 0x00FF);	//fill in the lower part of the count
	  if (debug)
	    printf ("Sending ack # %04d (length: %d)\n", count, len);

	  if (sendto
	      (sock, ackbuf, len, 0, (struct sockaddr *) &client,
	       sizeof (client)) != len)
	    {
	      if (debug)
		printf ("Mismatch in number of sent bytes\n");
	      return;
	    }
	}
      else if (debug)
	{
	  printf ("No ack required on packet count %d\n", count);
	}
      if (n != (datasize + 4))	/* remember if our datasize is less than a full packet this was the last packet to be received */
	{
	  if (debug)
	    printf
	      ("Last chunk detected (file chunk size: %d). exiting while loop\n",
	       n - 4);
	  goto done;		/* gotos are not optimal, but a good solution when exiting a multi-layer loop */
	}
      memset (filebuf, 0, sizeof (filebuf));
      count++;

      for (j = 0; j < RETRIES; j++)	/* this allows us to loop until we either break out by getting the correct ack OR time out because we've looped more than RETRIES times */
	{
	  client_len = sizeof (data);
	  errno = EAGAIN;	/* this allows us to enter the loop */
	  n = -1;
	  for (i = 0; errno == EAGAIN && i <= TIMEOUT && n < 0; i++)	/* this for loop will just keep checking the non-blocking socket until timeout */
	    {

	      n =
		recvfrom (sock, packetbuf, sizeof (packetbuf) - 1,
			  MSG_DONTWAIT, (struct sockaddr *) &data,
			  (socklen_t *) & client_len);
	      /*if (debug)
	         printf ("The value recieved is n: %d\n",n); */
	      usleep (1000);
	    }

	  if (n < 0 && errno != EAGAIN)	/* this will be true when there is an error that isn't the WOULD BLOCK error */
	    {
	      if (debug)
		printf
		  ("The server could not receive from the client (errno: %d n: %d)\n",
		   errno, n);

	      //resend packet
	    }
	  else if (n < 0 && errno == EAGAIN)	/* this is true when the error IS would block. This means we timed out */
	    {
	      if (debug)
		printf ("Timeout waiting for data (errno: %d == %d n: %d)\n",
			errno, EAGAIN, n);
	      //resend packet

	    }
	  else
	    {
	      if (client.sin_addr.s_addr != data.sin_addr.s_addr)	/* checks to ensure get from ip is same from ACK IP */
		{
		  if (debug)
		    printf
		      ("Error recieving file (data from invalid address)\n");
		  j--;
		  continue;	/* we aren't going to let another connection spoil our first connection */
		}

	      if (tid != ntohs (client.sin_port))	/* checks to ensure get from the correct TID */
		{
		  if (debug)
		    printf ("Error recieving file (data from invalid tid)\n");
		  len = sprintf ((char *) packetbuf,
				 "%c%c%c%cBad/Unknown TID%c",
				 0x00, 0x05, 0x00, 0x05, 0x00);
		  if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
		    {
		      printf
			("Mismatch in number of sent bytes while trying to send mode error packet\n");
		    }
		  j--;

		  continue;	/* we aren't going to let another connection spoil our first connection */
		}
/* this formatting code is just like the code in the main function */
	      bufindex = (char *) packetbuf;	//start our pointer going
	      if (bufindex++[0] != 0x00)
		printf ("bad first nullbyte!\n");
	      opcode = *bufindex++;
	      rcount = *bufindex++ << 8;
	      rcount &= 0xff00;
	      rcount += (*bufindex++ & 0x00ff);



	      memcpy ((char *) filebuf, bufindex, n - 4);	/* copy the rest of the packet (data portion) into our data array */
	      if (debug)
		printf
		  ("Remote host sent data packet #%d (Opcode: %d packetsize: %d filechunksize: %d)\n",
		   rcount, opcode, n, n - 4);
	      if (flag)
		{
		  if (n > 516)
		    datasize = n - 4;
		  flag = 0;
		}
	      if (opcode != 3 || rcount != count)	/* ack packet should have code 3 (data) and should be ack+1 the packet we just sent */
		{
		  if (debug)
		    printf
		      ("Badly ordered/invalid data packet (Got OP: %d Block: %d) (Wanted Op: 3 Block: %d)\n",
		       opcode, rcount, count);
/* sending error message */
		  if (opcode > 5)
		    {
		      len = sprintf ((char *) packetbuf,
				     "%c%c%c%cIllegal operation%c",
				     0x00, 0x05, 0x00, 0x04, 0x00);
		      if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
			{
			  printf
			    ("Mismatch in number of sent bytes while trying to send mode error packet\n");
			}
		    }

		}
	      else
		{
		  break;
		}
	    }


	  if (sendto
	      (sock, ackbuf, len, 0, (struct sockaddr *) &client,
	       sizeof (client)) != len)
	    {
	      if (debug)
		printf ("Mismatch in number of sent bytes\n");
	      return;
	    }

	}
      if (j == RETRIES)
	{
	  if (debug)
	    printf ("Data recieve Timeout. Aborting transfer\n");
	  fclose (fp);

	  return;
	}

    }

  while (fwrite (filebuf, 1, n - 4, fp) == n - 4);	/* if it doesn't write the file the length of the packet received less 4 then it didn't work */
  fclose (fp);
  sync ();
  if (debug)
    printf ("fclose and sync successful. File failed to recieve properly\n");
  return;

done:

  fclose (fp);
  sync ();
  if (debug)
    printf ("fclose and sync successful. File received successfully\n");

  return;
}







void
tsend (char *pFilename, struct sockaddr_in client, char *pMode, int tid)
{
  int sock, len, client_len, opcode, ssize = 0, n, i, j, bcount = 0;
  unsigned short int count = 0, rcount = 0, acked = 0;
  unsigned char filebuf[MAXDATASIZE + 1];
  unsigned char packetbuf[MAXACKFREQ][MAXDATASIZE + 12],
    recvbuf[MAXDATASIZE + 12];
  char filename[128], mode[12], fullpath[196], *bufindex;
  struct sockaddr_in ack;

  FILE *fp;			/* pointer to the file we will be sending */

  strcpy (filename, pFilename);	//copy the pointer to the filename into a real array
  strcpy (mode, pMode);		//same as above

  if (debug)
    printf ("branched to file send function\n");


  if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)	//startup a socket
    {
      printf ("Server reconnect for sending did not work correctly\n");
      return;
    }
  if (!strncasecmp (mode, "octet", 5) && !strncasecmp (mode, "netascii", 8))	/* these two are the only modes we accept */
    {
      if (!strncasecmp (mode, "mail", 4))
	len = sprintf ((char *) packetbuf[0],
		       "%c%c%c%cThis tftp server will not operate as a mail relay%c",
		       0x00, 0x05, 0x00, 0x04, 0x00);
      else
	len = sprintf ((char *) packetbuf[0],
		       "%c%c%c%cUnrecognized mode (%s)%c",
		       0x00, 0x05, 0x00, 0x04, mode, 0x00);
      if (sendto (sock, packetbuf[0], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  printf
	    ("Mismatch in number of sent bytes while trying to send mode error packet\n");
	}
      return;
    }
  if (strchr (filename, 0x5C) || strchr (filename, 0x2F))	//look for illegal characters in the filename string these are \ and /
    {
      if (debug)
	printf ("Server requested bad file: forbidden name\n");
      len =
	sprintf ((char *) packetbuf[0],
		 "%c%c%c%cIllegal filename.(%s) You may not attempt to descend or ascend directories.%c",
		 0x00, 0x05, 0x00, 0x00, filename, 0x00);
      if (sendto (sock, packetbuf[0], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  printf
	    ("Mismatch in number of sent bytes while trying to send error packet\n");
	}
      return;

    }
  strcpy (fullpath, path);
  strncat (fullpath, filename, sizeof (fullpath) - 1);	//build the full file path by appending filename to path
  fp = fopen (fullpath, "r");
  if (fp == NULL)
    {				//if the pointer is null then the file can't be opened - Bad perms OR no such file
      if (debug)
	printf ("Server requested bad file: file not found (%s)\n", fullpath);
      len =
	sprintf ((char *) packetbuf[0], "%c%c%c%cFile not found (%s)%c", 0x00,
		 0x05, 0x00, 0x01, fullpath, 0x00);
      if (sendto (sock, packetbuf[0], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  printf
	    ("Mismatch in number of sent bytes while trying to send error packet\n");
	}
      return;
    }
  else
    {
      if (debug)
	printf ("Sending file... (source: %s)\n", fullpath);

    }
  memset (filebuf, 0, sizeof (filebuf));
  while (1)			/* our break statement will escape us when we are done */
    {
      acked = 0;
      ssize = fread (filebuf, 1, datasize, fp);





      count++;			/* count number of datasize byte portions we read from the file */
      if (count == 1)		/* we always look for an ack on the FIRST packet */
	bcount = 0;
      else if (count == 2)	/* The second packet will always start our counter at zreo. This special case needs to exist to avoid a DBZ when count = 2 - 2 = 0 */
	bcount = 0;
      else
	bcount = (count - 2) % ackfreq;

      sprintf ((char *) packetbuf[bcount], "%c%c%c%c", 0x00, 0x03, 0x00, 0x00);	/* build data packet but write out the count as zero */
      memcpy ((char *) packetbuf[bcount] + 4, filebuf, ssize);
      len = 4 + ssize;
      packetbuf[bcount][2] = (count & 0xFF00) >> 8;	//fill in the count (top number first)
      packetbuf[bcount][3] = (count & 0x00FF);	//fill in the lower part of the count
      if (debug)
	printf ("Sending packet # %04d (length: %d file chunk: %d)\n",
		count, len, ssize);

      if (sendto (sock, packetbuf[bcount], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
	{
	  if (debug)
	    printf ("Mismatch in number of sent bytes\n");
	  return;
	}


      if ((count - 1) == 0 || ((count - 1) % ackfreq) == 0
	  || ssize != datasize)
	{
/* The following 'for' loop is used to recieve/timeout ACKs */
	  for (j = 0; j < RETRIES; j++)
	    {
	      client_len = sizeof (ack);
	      errno = EAGAIN;
	      n = -1;
	      for (i = 0; errno == EAGAIN && i <= TIMEOUT && n < 0; i++)
		{
		  n =
		    recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT,
			      (struct sockaddr *) &ack,
			      (socklen_t *) & client_len);

		  usleep (10);
		}
	      if (n < 0 && errno != EAGAIN)
		{
		  if (debug)
		    printf
		      ("The server could not receive from the client (errno: %d n: %d)\n",
		       errno, n);
		  //resend packet
		}
	      else if (n < 0 && errno == EAGAIN)
		{
		  if (debug)
		    printf ("Timeout waiting for ack (errno: %d n: %d)\n",
			    errno, n);
		  //resend packet

		}
	      else
		{

		  if (client.sin_addr.s_addr != ack.sin_addr.s_addr)	/* checks to ensure send to ip is same from ACK IP */
		    {
		      if (debug)
			printf
			  ("Error recieving ACK (ACK from invalid address)\n");
		      j--;	/* in this case someone else connected to our port. Ignore this fact and retry getting the ack */
		      continue;
		    }
		  if (tid != ntohs (client.sin_port))	/* checks to ensure get from the correct TID */
		    {
		      if (debug)
			printf
			  ("Error recieving file (data from invalid tid)\n");
		      len =
			sprintf ((char *) recvbuf,
				 "%c%c%c%cBad/Unknown TID%c", 0x00, 0x05,
				 0x00, 0x05, 0x00);
		      if (sendto (sock, recvbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
			{
			  printf
			    ("Mismatch in number of sent bytes while trying to send mode error packet\n");
			}
		      j--;

		      continue;	/* we aren't going to let another connection spoil our first connection */
		    }

/* this formatting code is just like the code in the main function */
		  bufindex = (char *) recvbuf;	//start our pointer going
		  if (bufindex++[0] != 0x00)
		    printf ("bad first nullbyte!\n");
		  opcode = *bufindex++;

		  rcount = *bufindex++ << 8;
		  rcount &= 0xff00;
		  rcount += (*bufindex++ & 0x00ff);
		  if (opcode != 4 || rcount != count)	/* ack packet should have code 4 (ack) and should be acking the packet we just sent */
		    {
		      if (debug)
			printf
			  ("Remote host failed to ACK proper data packet # %d (got OP: %d Block: %d)\n",
			   count, opcode, rcount);
/* sending error message */
		      if (opcode > 5)
			{
			  len = sprintf ((char *) recvbuf,
					 "%c%c%c%cIllegal operation%c",
					 0x00, 0x05, 0x00, 0x04, 0x00);
			  if (sendto (sock, recvbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* send the data packet */
			    {
			      printf
				("Mismatch in number of sent bytes while trying to send mode error packet\n");
			    }
			}
		      /* from here we will loop back and resend */
		    }
		  else
		    {
		      if (debug)
			printf ("Remote host successfully ACK'd (#%d)\n",
				rcount);
		      break;
		    }
		}
	      for (i = 0; i <= bcount; i++)
		{
		  if (sendto (sock, packetbuf[i], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)	/* resend the data packet */
		    {
		      if (debug)
			printf ("Mismatch in number of sent bytes\n");
		      return;
		    }
		  if (debug)
		    printf ("Ack(s) lost. Resending: %d\n",
			    count - bcount + i);
		}
	      if (debug)
		printf ("Ack(s) lost. Resending complete.\n");

	    }
/* The ack sending 'for' loop ends here */

	}
      else if (debug)
	{
	  printf ("Not attempting to recieve ack. Not required. count: %d\n",
		  count);
	  n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & client_len);	/* just do a quick check incase the remote host is trying with ackfreq = 1 */

	}

      if (j == RETRIES)
	{
	  if (debug)
	    printf ("Ack Timeout. Aborting transfer\n");
	  fclose (fp);

	  return;
	}
      if (ssize != datasize)
	break;

      memset (filebuf, 0, sizeof (filebuf));	/* fill the filebuf with zeros so that when the fread fills it, it is a null terminated string */
    }

  fclose (fp);
  if (debug)
    printf ("File sent successfully\n");

  return;
}

int
isnotvaliddir (char *pPath)	/* this function just makes sure that the directory passed to the server is valid and adds a trailing slash if not present */
{
  DIR *dp;
  int len;
  dp = opendir (pPath);
  if (dp == NULL)
    {
      return (0);
    }
  else
    {
      len = strlen (pPath);
      closedir (dp);
      if (pPath[len - 1] != '/')
	{
	  pPath[len] = '/';
	  pPath[len + 1] = 0;
	}




      return (1);
    }
}

void
usage (void)			/* prints program usage */
{
  printf
    ("Usage: tftpd [options] [path]\nOptions:\n-d (debug mode)\n-h (help; this message)\n-P <port>\n-a <ack freqency. Default 1>\n-s <data chunk size in bytes. Default 512>\n");
  return;
}
