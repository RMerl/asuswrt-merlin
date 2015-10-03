/**********************************************************
Date: 		NOV 28th, 2006
Project :	TFTP Client
Programers:	
Jonathan Felske
Andrew Fullard 
Craig Holmes
Reza Rahmanian 
Adam Tomalty 
File:		TFTP Client (main)
Purpose:	A TFTP client that will request a connections from
		the server and transefet files.
Notes:		Here we are using the sendto and recvfrom
		functions so the server and client can exchange data.
***********************************************************/

#include "tftp.h"

/*a function to print the Help menu*/
void help (char *);
/*a function to create the request packet, read or write*/
int req_packet (int opcode, char *filename, char *mode, char buf[]);
/*a function to creat an ACK packet*/
int ack_packet (int block, char buf[]);
/*a function to create the Error packets*/
int err_packet (int err_code, char *err_msg, char buf[]);
/*a function that will print the ip:port pair of the server or client, plus data sent or recieved*/
void ip_port (struct sockaddr_in host);
/*a function to send a file to the server*/
void tsend (char *pFilename, struct sockaddr_in server, char *pMode,
	    int sock);
/*a function to get a file from the server*/
void tget (char *pFilename, struct sockaddr_in server, char *pMode, int sock);


/* default values which can be controlled by command line */
char path[64] = "/tmp/";
int port = 69;
unsigned short int ackfreq = 1;
int datasize = 512;
int debug = 0, w_size = 1, p_length = 512;


int
main (int argc, char **argv)
{
  /*local variables */
  extern char *optarg;
  int sock, server_len, len, opt;	//,n;
  char opcode, filename[196], mode[12] = "octet";
  struct hostent *host;		/*for host information */
  struct sockaddr_in server;	//, client; /*the address structure for both the server and client */
  FILE *fp;			/*a pointer to a file that the client will send or get from the server */

  if (argc < 2)
    {
      help (argv[0]);
      return 0;
    }
  if (!(host = gethostbyname (argv[1])))
    {
      perror ("Client could not get host address information");
      exit (2);
    }

/* All of the following deals with command line switches */
  while ((opt = getopt (argc, argv, "dnoh:P:p:g:l:w:")) != -1)	/* this function is handy */
    {
      switch (opt)
	{
	case 'd':		/* debug mode (no opts) */
	  debug = 1;
	  break;
	case 'P':		/* Port (opt required) */
	  port = atoi (optarg);
	  if (debug)
	    {
	      printf ("Client: The port number is: %d\n", port);
	    }
	  break;
	case 'p':		/* put a file on the server */
	  strncpy (filename, optarg, sizeof (filename) - 1);
	  opcode = WRQ;
	  fp = fopen (filename, "r");	/*opened the file for reading */
	  if (fp == NULL)
	    {
	      printf ("Client: file could not be opened\n");
	      return 0;
	    }
	  if (debug)
	    {
	      printf ("Client: The file name is: %s and can be read",
		      filename);
	    }
	  fclose (fp);
	  break;
	case 'g':		/*get a file from the server */
	  strncpy (filename, optarg, sizeof (filename) - 1);
	  opcode = RRQ;
	  fp = fopen (filename, "w");	/*opened the file for writting */
	  if (fp == NULL)
	    {
	      printf ("Client: file could not be created\n");
	      return 0;
	    }
	  if (debug)
	    {
	      printf ("Client: The file name is: %s and it has been created",
		      filename);
	    }
	  fclose (fp);
	  break;
	case 'w':		/* Get the window size */
	  ackfreq = atoi (optarg);
	  if (debug)
	    {
	      printf ("Client: Window size is: %i\n", ackfreq);
	    }
	  //ackfreq = atoi (optarg);
	  if (ackfreq > MAXACKFREQ)
	    {
	      printf
		("Client: Sorry, you specified an ack frequency higher than the maximum allowed (Requested: %d Max: %d)\n",
		 ackfreq, MAXACKFREQ);
	      return 0;
	    }
	  else if (w_size == 0)
	    {
	      printf ("Client: Sorry, you have to ack sometime.\n");
	      return 0;
	    }
	  break;
	case 'l':		/* packet length */
	  datasize = atoi (optarg);
	  if (debug)
	    {
	      printf ("Client: Packet length is: %i bytes\n", datasize);
	    }
	  if (datasize > MAXDATASIZE)
	    {
	      printf
		("Client: Sorry, you specified a data size higher than the maximum allowed (Requested: %d Max: %d)\n",
		 datasize, MAXDATASIZE);
	      return 0;
	    }
	  break;
	case 'h':		/* Help (no opts) */
	  help (argv[0]);
	  return (0);
	  break;
	case 'o':
	  strncpy (mode, "octet", sizeof (mode) - 1);
	  if (debug)
	    {
	      printf ("Client: The mode is set to octet\n");
	    }
	  break;
	case 'n':
	  strncpy (mode, "netascii", sizeof (mode) - 1);
	  if (debug)
	    {
	      printf ("Client: The mode is set to netascii\n");
	    }
	  break;
	default:		/* everything else */
	  help (argv[0]);
	  return (0);
	  break;
	}			//end of switch
    }				//end of while loop

/*check for valid input*/
  if (argc < 5 || argc > 12)
    {
      printf ("Client: wrong number of arguments: %d\n", argc);
      help (argv[0]);
      exit (ERROR);
    }

/* Done dealing with switches and the command line*/


  /*Create the socket, a -1 will show us an error */
  if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
      printf ("Client: Socket could not be created");
      return 0;
    }


/*set the address values for the server */
  memset (&server, 0, sizeof (server));	/*Clear the structure */
  server.sin_family = AF_INET;	/*address family for TCP and UDP */
  memcpy (&server.sin_addr, host->h_addr, host->h_length);	/*set address of server taken from gethostbyname function */
  //server.sin_addr.s_addr = htonl (INADDR_ANY); /*use any address */
  server.sin_port = htons (port);	/*pick a free port */


  server_len = sizeof (server);	/*get the length of the server address */

  if (debug)
    printf ("Client: size of server_address is : %d bytes\n", server_len);

  memset (buf, 0, BUFSIZ);	/*clear the buffer */
  /* this is the first request message */
  len = req_packet (opcode, filename, mode, buf);

  if (debug)
    printf ("Client: The request packt's length is: %d bytes\n", len);

  if (sendto (sock, buf, len, 0, (struct sockaddr *) &server, server_len) !=
      len)
    {
      perror ("Client: sendto has returend an error");
      exit (ERROR);
    }
  if (debug)
    ip_port (server);
  switch (opcode)
    {
    case RRQ:			/*read from the server, download */
      tget (filename, server, mode, sock);
      break;
    case WRQ:			/*write to the server, upload */
      tsend (filename, server, mode, sock);
      break;
    default:
      printf ("Invalid opcode detected. Ignoring packet.");
    }
  close (sock);			/* close the socket */
  return 1;
}				//end of main


/******************************************************************
		Function deffinitions
*******************************************************************/

/*usage of the command:
./tftpc server -P port# -g(get file)|-p(put file) <filename> -w (window size) -l (packetlength)
if windowsize and packet length are not chosen then a default value of 1, for the
window size and 512 bytes for the packet length will be set.*/
/* a function to display the help menu*/
void
help (char *app)
{
  printf
    ("Usage:\n%s server [-h] [-d] [-P port] [-g] | [-p] [file-name] [-w size] [-l length] [-o] [-n]\n",
     app);
  printf
    ("Options:\n-h (help; this message)\n-d (Debug mode)\n-P port(Port number default is 69)\n-g (get a file from the server)\n-p (send a file to the server)\n");
  printf
    ("-w size (set window size, default is 1)\n-l len (set max packet length, default is 512 bytes)\n");
  printf
    ("-o for octet file transfer (default).\n-n for netascii file transfer\n");
}

/* A function that will create a request packet*/
/*I'm not sure if I need the last 0x00 because sprintf will end the string with \0*/
int
req_packet (int opcode, char *filename, char *mode, char buf[])
{
  int len;
  len =
    sprintf (buf, "%c%c%s%c%s%c", 0x00, opcode, filename, 0x00, mode, 0x00);
  if (len == 0)
    {
      printf ("Error in creating the request packet\n");	/*could not print to the client buffer */
      exit (ERROR);
    }
  if (debug)
    {
      printf ("I am creating the request packet.\n");
    }
  return len;
}



/* A function that will create an ACK packet*/
/*problem that we will have here is that we can only get up to 255 blocks*/
int
ack_packet (int block, char buf[])
{
  int len;
  len = sprintf (buf, "%c%c%c%c", 0x00, ACK, 0x00, 0x00);
  buf[2] = (block & 0xFF00) >> 8;
  buf[3] = (block & 0x00FF);
  if (len == 0)
    {
      printf ("Error in creating the ACK packet\n");	/*could not print to the client buffer */
      exit (ERROR);
    }
  if (debug)
    {
      printf ("I am creating the ACK packet.\n");
    }
  return len;
}


/* A function that will create an error packet based on the error code*/
int
err_packet (int err_code, char *err_msg, char buf[])
{
  int len;
  memset (buf, 0, sizeof (buf));
  len =
    sprintf (buf, "%c%c%c%c%s%c", 0x00, ERR, 0x00, err_code, err_msg, 0x00);
  if (len == 0)
    {
      printf ("Error in creating the ACK packet\n");	/*could not print to the client buffer */
      exit (ERROR);
    }
  if (debug)
    {
      printf ("I am creating an ERROR packet.\n");
    }
  return len;
}

/* A function used for debuging to show the port numbers used*/
void
ip_port (struct sockaddr_in host)
{
  printf ("The IP port pair for the host is: IP:%s Port:%d \n",
	  inet_ntoa (host.sin_addr), ntohs (host.sin_port));
}

/*
*This function is called when the client would like to upload a file to the server.
*/
void
tsend (char *pFilename, struct sockaddr_in server, char *pMode, int sock)
{
  int len, server_len, opcode, ssize = 0, n, i, j, bcount = 0, tid;
  unsigned short int count = 0, rcount = 0, acked = 0;
  unsigned char filebuf[MAXDATASIZE + 1];
  unsigned char packetbuf[MAXACKFREQ][MAXDATASIZE + 12],
    recvbuf[MAXDATASIZE + 12];
  char filename[128], mode[12], *bufindex;	//fullpath[196],
  struct sockaddr_in ack;

  FILE *fp;			/* pointer to the file we will be sending */

  strcpy (filename, pFilename);	//copy the pointer to the filename into a real array
  strcpy (mode, pMode);		//same as above

  if (debug)
    printf ("Client: branched to file send function\n");

/*At this point I have to wait to recieve an ACK from the server before I start sending the file*/
  /*open the file to read */
  fp = fopen (filename, "r");
  if (fp == NULL)
    {				//if the pointer is null then the file can't be opened - Bad perms OR no such file
      if (debug)
	printf ("Client: sending bad file: file not found (%s)\n", filename);
      return;
    }
  else
    {
      if (debug)
	printf ("Client: Sending file... (source: %s)\n", filename);

    }
//get ACK for WRQ
/* The following 'for' loop is used to recieve/timeout ACKs */
  for (j = 0; j < RETRIES - 2; j++)
    {
      server_len = sizeof (ack);
      errno = EAGAIN;
      n = -1;
      for (i = 0; errno == EAGAIN && i <= TIMEOUT && n < 0; i++)
	{
	  n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT,
			(struct sockaddr *) &ack, (socklen_t *) & server_len);
	  usleep (1000);
	}


      /* if(debug)
         ip_port (ack);    print the vlaue recived from the server */


      tid = ntohs (ack.sin_port);	//get the tid of the server.
      server.sin_port = htons (tid);	//set the tid for rest of the transfer


      if (n < 0 && errno != EAGAIN)
	{
	  if (debug)
	    printf
	      ("Client: could not receive from the server (errno: %d n: %d)\n",
	       errno, n);
	  //resend packet
	}
      else if (n < 0 && errno == EAGAIN)
	{
	  if (debug)
	    printf ("Client: Timeout waiting for ack (errno: %d n: %d)\n",
		    errno, n);
	  //resend packet
	}
      else
	{			/*changed client to server here */
	  if (server.sin_addr.s_addr != ack.sin_addr.s_addr)	/* checks to ensure send to ip is same from ACK IP */
	    {
	      if (debug)
		printf
		  ("Client: Error recieving ACK (ACK from invalid address)\n");
	      j--;		/* in this case someone else connected to our port. Ignore this fact and retry getting the ack */
	      continue;
	    }
	  if (tid != ntohs (server.sin_port))	/* checks to ensure get from the correct TID */
	    {
	      if (debug)
		printf
		  ("Client: Error recieving file (data from invalid tid)\n");
	      len = err_packet (5, err_msg[5], buf);
	      if (sendto (sock, buf, len, 0, (struct sockaddr *) &server, sizeof (server)) != len)	/* send the data packet */
		{
		  printf
		    ("Client: Mismatch in number of sent bytes while trying to send mode error packet\n");
		}
	      /* if (debug)
	         ip_port(server); */
	      j--;
	      continue;		/* we aren't going to let another connection spoil our first connection */
	    }

/* this formatting code is just like the code in the main function */
	  bufindex = (char *) recvbuf;	//start our pointer going
	  if (bufindex++[0] != 0x00)
	    printf ("Client: bad first nullbyte!\n");
	  opcode = *bufindex++;

	  rcount = *bufindex++ << 8;
	  rcount &= 0xff00;
	  rcount += (*bufindex++ & 0x00ff);
	  if (opcode != 4 || rcount != count)	/* ack packet should have code 4 (ack) and should be acking the packet we just sent */
	    {
	      if (debug)
		printf
		  ("Client: Remote host failed to ACK proper data packet # %d (got OP: %d Block: %d)\n",
		   count, opcode, rcount);
/* sending error message */
	      if (opcode > 5)
		{

		  len = err_packet (4, err_msg[4], buf);
		  if (sendto (sock, buf, len, 0, (struct sockaddr *) &server, sizeof (server)) != len)	/* send the data packet */
		    {
		      printf
			("Client: Mismatch in number of sent bytes while trying to send mode error packet\n");
		    }
		}
	      /* from here we will loop back and resend */
	      /* if (debug)
	         ip_port(server); */
	    }
	  else
	    {
	      if (debug)
		printf ("Client: Remote host successfully ACK'd (#%d)\n",
			rcount);
	      break;
	    }
	}			//end of else
      if (debug)
	printf ("Client: Ack(s) lost. Resending complete.\n");

    }
/* The ack sending 'for' loop ends here */



  memset (filebuf, 0, sizeof (filebuf));	//clear the filebuf
  while (1)			/* our break statement will escape us when we are done */
    {
      acked = 0;
      ssize = fread (filebuf, 1, datasize, fp);
      if (debug)
	{
	  printf
	    ("The first data block has been read from the file and will be sent to the server\n");
	  printf ("The size read from the file is: %d\n", ssize);
	}

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
	printf ("Client: Sending packet # %04d (length: %d file chunk: %d)\n",
		count, len, ssize);
      /* send the data packet */
      if (sendto
	  (sock, packetbuf[bcount], len, 0, (struct sockaddr *) &server,
	   sizeof (server)) != len)
	{
	  if (debug)
	    printf ("Client: Mismatch in number of sent bytes\n");
	  return;
	}
      if (debug)
	{
	  ip_port (server);
	  printf ("==count: %d  bcount: %d  ssize: %d  datasize: %d\n", count,
		  bcount, ssize, datasize);
	}
      //if ((count - 1) == 0 || ((count - 1) % ackfreq) == 0 || ssize != datasize)
      if (((count) % ackfreq) == 0 || ssize != datasize)
	{
	  if (debug)
	    printf ("-- I will get an ACK\n");
/* The following 'for' loop is used to recieve/timeout ACKs */
	  for (j = 0; j < RETRIES; j++)
	    {
	      server_len = sizeof (ack);
	      errno = EAGAIN;
	      n = -1;
	      for (i = 0; errno == EAGAIN && i <= TIMEOUT && n < 0; i++)
		{
		  n =
		    recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT,
			      (struct sockaddr *) &ack,
			      (socklen_t *) & server_len);
		  /* if (debug)
		     ip_port(ack); */
		  usleep (1000);
		}
	      if (n < 0 && errno != EAGAIN)
		{
		  if (debug)
		    printf
		      ("Client: could not receive from the server (errno: %d n: %d)\n",
		       errno, n);
		  //resend packet
		}
	      else if (n < 0 && errno == EAGAIN)
		{
		  if (debug)
		    printf
		      ("Client: Timeout waiting for ack (errno: %d n: %d)\n",
		       errno, n);
		  //resend packet
		}
	      else
		{		/* checks to ensure send to ip is same from ACK IP */
		  if (server.sin_addr.s_addr != ack.sin_addr.s_addr)
		    {
		      if (debug)
			printf
			  ("Client: Error recieving ACK (ACK from invalid address)\n");
		      /* in this case someone else connected to our port. Ignore this fact and retry getting the ack */
		      j--;
		      continue;
		    }
		  if (tid != ntohs (server.sin_port))	/* checks to ensure get from the correct TID */
		    {
		      if (debug)
			printf
			  ("Client: Error recieving file (data from invalid tid)\n");
		      len = err_packet (5, err_msg[5], buf);
		      /* send the data packet */
		      if (sendto
			  (sock, buf, len, 0, (struct sockaddr *) &server,
			   sizeof (server)) != len)
			{
			  printf
			    ("Client: Mismatch in number of sent bytes while trying to send mode error packet\n");
			}
		      /*if (debug)
		         ip_port(server);  */
		      j--;

		      continue;	/* we aren't going to let another connection spoil our first connection */
		    }

/* this formatting code is just like the code in the main function */
		  bufindex = (char *) recvbuf;	//start our pointer going
		  if (bufindex++[0] != 0x00)
		    printf ("Client: bad first nullbyte!\n");
		  opcode = *bufindex++;

		  rcount = *bufindex++ << 8;
		  rcount &= 0xff00;
		  rcount += (*bufindex++ & 0x00ff);
		  if (opcode != 4 || rcount != count)	/* ack packet should have code 4 (ack) and should be acking the packet we just sent */
		    {
		      if (debug)
			printf
			  ("Client: Remote host failed to ACK proper data packet # %d (got OP: %d Block: %d)\n",
			   count, opcode, rcount);
		      /* sending error message */
		      if (opcode > 5)
			{
			  len = err_packet (4, err_msg[4], buf);
			  if (sendto (sock, buf, len, 0, (struct sockaddr *) &server, sizeof (server)) != len)	/* send the data packet */
			    {
			      printf
				("Client: Mismatch in number of sent bytes while trying to send mode error packet\n");
			    }
			  /*if (debug)
			     ip_port(server); */
			}
		      /* from here we will loop back and resend */
		    }
		  else
		    {
		      if (debug)
			printf
			  ("Client: Remote host successfully ACK'd (#%d)\n",
			   rcount);
		      break;
		    }
		}
	      for (i = 0; i <= bcount; i++)
		{
		  if (sendto (sock, packetbuf[i], len, 0, (struct sockaddr *) &server, sizeof (server)) != len)	/* resend the data packet */
		    {
		      if (debug)
			printf ("Client: Mismatch in number of sent bytes\n");
		      return;
		    }
		  if (debug)
		    {
		      printf ("Client: Ack(s) lost. Resending: %d\n",
			      count - bcount + i);
		      ip_port (server);
		    }
		}
	      if (debug)
		printf ("Client: Ack(s) lost. Resending complete.\n");

	    }
/* The ack sending 'for' loop ends here */

	}
      else if (debug)
	{
	  printf
	    ("Client: Not attempting to recieve ack. Not required. count: %d\n",
	     count);
	  n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & server_len);	/* just do a quick check incase the remote host is trying with ackfreq = 1 */
	  /*if (debug)
	     ip_port(ack); */
	}

      if (j == RETRIES)
	{
	  if (debug)
	    printf ("Client: Ack Timeout. Aborting transfer\n");
	  fclose (fp);

	  return;
	}
      if (ssize != datasize)
	break;

      memset (filebuf, 0, sizeof (filebuf));	/* fill the filebuf with zeros so that when the fread fills it, it is a null terminated string */
    }

  fclose (fp);
  if (debug)
    printf ("Client: File sent successfully\n");

  return;
}				//end of tsend function


/*
*This function is called when the client would like to download a file from the server.
*/
void
tget (char *pFilename, struct sockaddr_in server, char *pMode, int sock)
{
  /* local variables */
  int len, server_len, opcode, i, j, n, tid = 0, flag = 1;
  unsigned short int count = 0, rcount = 0;
  unsigned char filebuf[MAXDATASIZE + 1];
  unsigned char packetbuf[MAXDATASIZE + 12];
  extern int errno;
  char filename[128], mode[12], *bufindex, ackbuf[512];
  struct sockaddr_in data;
  FILE *fp;			/* pointer to the file we will be getting */

  strcpy (filename, pFilename);	//copy the pointer to the filename into a real array
  strcpy (mode, pMode);		//same as above


  if (debug)
    printf ("branched to file receive function\n");

  fp = fopen (filename, "w");	/* open the file for writing */
  if (fp == NULL)
    {				//if the pointer is null then the file can't be opened - Bad perms 
      if (debug)
	printf ("Client requested bad file: cannot open for writing (%s)\n",
		filename);
      return;
    }
  else				/* File is open and ready to be written to */
    {
      if (debug)
	printf ("Getting file... (destination: %s) \n", filename);
    }
/* zero the buffer before we begin */
  memset (filebuf, 0, sizeof (filebuf));
  n = datasize + 4;
  do
    {
      /* zero buffers so if there are any errors only NULLs will be exposed */
      memset (packetbuf, 0, sizeof (packetbuf));
      memset (ackbuf, 0, sizeof (ackbuf));
      if (n != (datasize + 4))	/* remember if our datasize is less than a full packet this was the last packet to be received */
	{
	  if (debug)
	    printf
	      ("Last chunk detected (file chunk size: %d). exiting while loop\n",
	       n - 4);
	  len = sprintf (ackbuf, "%c%c%c%c", 0x00, 0x04, 0x00, 0x00);
	  ackbuf[2] = (count & 0xFF00) >> 8;	//fill in the count (top number first)
	  ackbuf[3] = (count & 0x00FF);	//fill in the lower part of the count
	  if (debug)
	    printf ("Sending ack # %04d (length: %d)\n", count, len);
	  if (sendto
	      (sock, ackbuf, len, 0, (struct sockaddr *) &server,
	       sizeof (server)) != len)
	    {
	      if (debug)
		printf ("Mismatch in number of sent bytes\n");
	      return;
	    }
	  if (debug)
	    printf ("The Client has sent an ACK for packet\n");
	  goto done;		/* gotos are not optimal, but a good solution when exiting a multi-layer loop */
	}

      count++;

      for (j = 0; j < RETRIES; j++)	/* this allows us to loop until we either break out by getting the correct ack OR time out because we've looped more than RETRIES times */
	{
	  server_len = sizeof (data);
	  errno = EAGAIN;	/* this allows us to enter the loop */
	  n = -1;
	  for (i = 0; errno == EAGAIN && i <= TIMEOUT && n < 0; i++)	/* this for loop will just keep checking the non-blocking socket until timeout */
	    {

	      n =
		recvfrom (sock, packetbuf, sizeof (packetbuf) - 1,
			  MSG_DONTWAIT, (struct sockaddr *) &data,
			  (socklen_t *) & server_len);
	      usleep (1000);
	    }
	  /*if(debug)
	     ip_port (data);  print the vlaue recived from the server */
	  if (!tid)
	    {
	      tid = ntohs (data.sin_port);	//get the tid of the server.
	      server.sin_port = htons (tid);	//set the tid for rest of the transfer 
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
	      if (server.sin_addr.s_addr != data.sin_addr.s_addr)	/* checks to ensure get from ip is same from ACK IP */
		{
		  if (debug)
		    printf
		      ("Error recieving file (data from invalid address)\n");
		  j--;
		  continue;	/* we aren't going to let another connection spoil our first connection */
		}
	      if (tid != ntohs (server.sin_port))	/* checks to ensure get from the correct TID */
		{
		  if (debug)
		    printf ("Error recieving file (data from invalid tid)\n");
		  len = sprintf ((char *) packetbuf,
				 "%c%c%c%cBad/Unknown TID%c",
				 0x00, 0x05, 0x00, 0x05, 0x00);
		  if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &server, sizeof (server)) != len)	/* send the data packet */
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
		  else if (n < 516)
		    datasize = 512;
		  flag = 0;
		}
	      if (opcode != 3)	/* ack packet should have code 3 (data) and should be ack+1 the packet we just sent */
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
		      if (sendto (sock, packetbuf, len, 0, (struct sockaddr *) &server, sizeof (server)) != len)	/* send the data packet */
			{
			  printf
			    ("Mismatch in number of sent bytes while trying to send mode error packet\n");
			}
		    }
		}
	      else
		{
		  len = sprintf (ackbuf, "%c%c%c%c", 0x00, 0x04, 0x00, 0x00);
		  ackbuf[2] = (count & 0xFF00) >> 8;	//fill in the count (top number first)
		  ackbuf[3] = (count & 0x00FF);	//fill in the lower part of the count
		  if (debug)
		    printf ("Sending ack # %04d (length: %d)\n", count, len);
		  if (((count - 1) % ackfreq) == 0)
		    {
		      if (sendto
			  (sock, ackbuf, len, 0, (struct sockaddr *) &server,
			   sizeof (server)) != len)
			{
			  if (debug)
			    printf ("Mismatch in number of sent bytes\n");
			  return;
			}
		      if (debug)
			printf ("The client has sent an ACK for packet %d\n",
				count);
		    }		//check for ackfreq
		  else if (count == 1)
		    {
		      if (sendto
			  (sock, ackbuf, len, 0, (struct sockaddr *) &server,
			   sizeof (server)) != len)
			{
			  if (debug)
			    printf ("Mismatch in number of sent bytes\n");
			  return;
			}
		      if (debug)
			printf ("The Client has sent an ACK for packet 1\n");
		    }
		  break;
		}		//end of else
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
    printf
      ("fclose and sync unsuccessful. File failed to recieve properly\n");
  return;

done:

  fclose (fp);
  sync ();
  if (debug)
    printf ("fclose and sync successful. File received successfully\n");
  return;
}
