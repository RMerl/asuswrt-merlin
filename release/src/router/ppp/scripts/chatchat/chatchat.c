/* ************************************************************************* 
* NAME: chatchat.c
*
* DESCRIPTION:
*
* This program creates a pipe for the chat process to read. The user
* can supply information (like a password) that will be picked up
* by chat and sent just like the regular contents of a chat script.
*
* Usage is:
*
* chatchat <filename>
*
*   where <filename> matches the option given in the chat script.
*
* for instance the chat script fragment:
*
*       ...
*        name:           \\dmyname  \
*        word:           @/var/tmp/p \
*       ...
*                                   ^
*                    (note: leave some whitespace after the filename)
*
* expect "name:", reply with a delay followed by "myname"
* expect "word:", reply with the data read from the pipe /var/tmp/p 
*
* the matching usage of chatchat would be:
*
* chatchat /var/tmp/p
*
* eg:
*
* $chatchat /var/tmp/p
* ...
*                           some other process eventually starts:
*                           chat ...
*                           chat parses the "@/var/tmp/p" option and opens
*                              /var/tmp/p
*  (chatchat prompts:)
*
* type PIN into SecurID card
*   enter resulting passcode: [user inputs something]
*
*                           chat reads /var/tmp/p & gets what the
*                              user typed at chatchat's "enter string" prompt
*                           chat removes the pipe file
*                           chat sends the user's input as a response in
*                              place of "@/var/tmp/p"
*                             
* PROCESS:
*
* gcc -g -o chatchat chatchat.c
*
* 
* GLOBALS: none
*
* REFERENCES:
*
* see the man pages and documentation that come with the 'chat' program
* (part of the ppp package). you will need to use the modified chat
* program that accepts the '@' operator.
*
* LIMITATIONS:
*
* REVISION HISTORY:
*
*   STR                Description                          Author
*
*   23-Mar-99          initial coding                        gpk
*   12-May-99	       unlink the pipe after closing	     paulus
*
* TARGET: ANSI C
* This program is in the public domain.
* 
*
* ************************************************************************* */




#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* MAXINPUT - the data typed into chatchat must be fewer   */
/* characters than this.                                 */

#define MAXINPUT 80






/* ************************************************************************* 


   NAME:  main


   USAGE: 

   int argc;
   char * argv[];

   main(argc, argv[]);

   returns: int

   DESCRIPTION:
                 if the pipe file name is given on the command line,
		    create the pipe, prompt the user and put whatever
		    is typed into the pipe.

		 returns -1 on error
		     else # characters entered
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     25-Mar-99               initial coding                           gpk

 ************************************************************************* */

int main(int argc, char * argv[])
{
  int retval;
  
  int create_and_write_pipe(char * pipename);

  if (argc != 2)
    {
      fprintf(stderr, "usage: %s pipename\n", argv[0]);
      retval = -1;
    }
  else
    {
     retval =  create_and_write_pipe(argv[1]);
    }
  return (retval);
}




/* ************************************************************************* 


   NAME:  create_and_write_pipe


   USAGE: 

   int some_int;
   char * pipename;

   some_int =  create_and_write_pipe(pipename);

   returns: int

   DESCRIPTION:
                 given the pipename, create the pipe, open it,
		 prompt the user for a string to put into the
		 pipe, write the string, and close the pipe

		 on error, print out an error message and return -1
		 
		 returns -1 on error
		   else #bytes written into the pipe
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES: 

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     25-Mar-99               initial coding                           gpk
     12-May-99		     remove pipe after closing		     paulus

 ************************************************************************* */

int create_and_write_pipe(char * pipename)
{
  int retval, created, pipefd, nread, nwritten;
  char input[MAXINPUT];
  char errstring[180];
  
  int create_pipe(char * pipename);
  int write_to_pipe(int pipefd, char * input, int nchar);

  created = create_pipe(pipename);

  if (-1 == created)
    {
      sprintf(errstring, "unable to create pipe '%s'", pipename);
      perror(errstring);
      retval = -1;
    }
  else
    {

      /* note: this open won't succeed until chat has the pipe  */
      /* open and ready to read. this makes for nice timing.    */
      
      pipefd = open(pipename, O_WRONLY);

      if (-1 == pipefd)
	{
	  sprintf(errstring, "unable to open pipe '%s'", pipename);
	  perror(errstring);
	  retval =  -1;
	}
      else
	{
	  fprintf(stderr, "%s \n %s",
		  "type PIN into SecurID card and",
		  "enter resulting passcode:");
	  nread = read(STDIN_FILENO, (void *)input, MAXINPUT);

	  
	  if (0 >= nread)
	    {
	      perror("unable to read from stdin");
	      retval = -1;
	    }
	  else
	    {
	      /* munch off the newline character, chat supplies  */
	      /* a return when it sends the string out.          */
	      input[nread -1] = 0;
	      nread--;
	      nwritten = write_to_pipe(pipefd, input, nread);
	      /* printf("wrote [%d]: '%s'\n", nwritten, input); */
	      retval = nwritten;
	    }
	  close(pipefd);

	  /* Now make the pipe go away.  It won't actually go away
	     completely until chat closes it. */
	  if (unlink(pipename) < 0)
	      perror("Warning: couldn't remove pipe");
	}
    }
  return(retval);
}







/* ************************************************************************* 


   NAME:  create_pipe


   USAGE: 

   int some_int;
   char * pipename;
   
   some_int =  create_pipe(pipename);
   
   returns: int
   
   DESCRIPTION:
                 create a pipe of the given name

		 if there is an error (like the pipe already exists)
		    print an error message and return
		    
		 return -1 on failure else success

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     25-Mar-99               initial coding                           gpk

 ************************************************************************* */

int create_pipe(char * pipename)
{
  mode_t old_umask;
  int created;

  /* hijack the umask temporarily to get the mode I want  */
  /* on the pipe.                                         */
  
  old_umask = umask(000);

  created = mknod(pipename, S_IFIFO | S_IRWXU | S_IWGRP | S_IWOTH,
		  (dev_t)NULL);

  /* now restore umask.  */
  
  (void)umask(old_umask);
  
  if (-1 == created)
    {
      perror("unable to create pipe");
    }

  return(created);
}






/* ************************************************************************* 


   NAME:  write_to_pipe


   USAGE: 

   int some_int;
   int pipefd;
   char * input;
   int nchar;

   some_int =  write_to_pipe(pipefd, input, nchar);

   returns: int

   DESCRIPTION:
                 write nchars of data from input to pipefd

		 on error print a message to stderr

		 return -1 on error, else # bytes written
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     25-Mar-99               initial coding                           gpk
     12-May-99		     don't write count word first	      paulus

 ************************************************************************* */

int write_to_pipe(int pipefd, char * input, int nchar)
{
  int nwritten;

  /* nwritten = write(pipefd, (void *)&nchar, sizeof(nchar)); */
  nwritten = write(pipefd, (void *)input, nchar);

  if (-1 == nwritten)
    {
      perror("unable to write to pipe");
    }

  return(nwritten);
}
