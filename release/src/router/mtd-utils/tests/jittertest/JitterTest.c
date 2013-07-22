/***********************************************************************
 *
 *  Copyright:          Daniel Measurement and Control, Inc.
 *                           9753 Pine Lake Drive
 *                             Houston, TX 77055
 *
 *  Created by: Vipin Malik and Gail Murray
 *  Released under GPL by permission of Daniel Industries.
 *
 * This software is licensed under the GPL version 2. Plese see the file
 * COPYING for details on the license.
 *
 * NO WARRANTY: Absolutely no claims of warranty or fitness of purpose
 *              are made in this software. Please use at your own risk.
 *
 *  Filename:     JitterTest.c
 *
 *  Description:  Program to be used to measure wake jitter.
 *                See README file for more info.
 *
 *
 *  Revision History:
 *  $Id: JitterTest.c,v 1.13 2005/11/07 11:15:20 gleixner Exp $
 *  $Log: JitterTest.c,v $
 *  Revision 1.13  2005/11/07 11:15:20  gleixner
 *  [MTD / JFFS2] Clean up trailing white spaces
 *
 *  Revision 1.12  2001/08/10 19:23:11  vipin
 *  Ready to be released under GPL! Added proper headers etc.
 *
 *  Revision 1.11  2001/07/09 15:35:50  vipin
 *  Couple of new features:1. The program runs by default as a "regular"
 *  (SCHED_OTHER) task by default, unless the -p n cmd line parameter is
 *  specified. It then runs as SCHED_RR at that priority.
 *  2. Added ability to send SIGSTOP/SIGCONT to a specified PID. This
 *  would presumably be the PID of the JFFS2 GC task. SIGSTOP is sent
 *  before writing to the fs, and a SIGCONT after the write is done.
 *  3. The "pad" data now written to the file on the "fs under test" is
 *  random, not sequential as previously.
 *
 *  Revision 1.10  2001/06/27 19:14:24  vipin
 *  Now console file to log at can be specified from cmd line. This can enable
 *  you to run two instances of the program- one logging to the /dev/console
 *  and another to a regular file (if you want the data) or /dev/null if you don't.
 *
 *  Revision 1.9  2001/06/25 20:21:31  vipin
 *  This is the latest version, NOT the one last checked in!
 *
 *  Revision 1.7  2001/06/18 22:36:19  vipin
 *  Fix minor typo that excluded '\n' from msg on console.
 *
 *  Revision 1.6  2001/06/18 21:17:50  vipin
 *  Added option to specify the amount of data written to outfile each period.
 *  The regular info is written, but is then padded to the requested size.
 *  This will enable testing of different sized writes to JFFS fs.
 *
 *  Revision 1.5  2001/06/08 19:36:23  vipin
 *  All write() are now checked for return err code.
 *
 *  Revision 1.4  2001/06/06 23:10:31  vipin
 *  Added README file.
 *  In JitterTest.c: Changed operation of periodic timer to one shot. The timer is now
 *  reset when the task wakes. This way every "jitter" is from the last time and
 *  jitters from previous times are not accumulated and screw aroud with our display.
 *
 *  All jitter is now +ve. (as it should be). Also added a "read file" functionality
 *  to test for jitter in task if we have to read from JFFS fs.
 *  The program now also prints data to console- where it can be logged, interspersed with
 *  other "interesting" printk's from the kernel and drivers (flash sector erases etc.)
 *
 *  Revision 1.3  2001/03/01 19:20:39  gmurray
 *  Add priority scheduling. Shortened name of default output file.
 *  Changed default interrupt period. Output delta time and jitter
 *  instead of time of day.
 *
 *  Revision 1.2  2001/02/28 16:20:19  vipin
 *  Added version control Id and log fields.
 *
 ***********************************************************************/
/*************************** Included Files ***************************/
#include <stdio.h>      /* fopen, printf, fprintf, fclose */
#include <string.h>     /* strcpy, strcmp */
#include <stdlib.h>     /* exit, atol, atoi */
#include <sys/time.h>   /* setitimer, settimeofday, gettimeofday */
#include <time.h>	/* time */
#include <signal.h>     /* signal */
#include <sched.h>      /* sched_setscheduler, sched_get_priority_min,*/
/*   sched_get_priority_max */
#include <unistd.h>     /* gettimeofday, sleep */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>



/**************************** Enumerations ****************************/
enum timerActions
    {
        ONESHOT,
        AUTORESETTING
    };



/****************************** Constants *****************************/
/* Exit error codes */
#define EXIT_FILE_OPEN_ERR        (1)     /* error opening output file*/
#define EXIT_REG_SIGALRM_ERR      (2)     /* error registering SIGALRM*/
#define EXIT_REG_SIGINT_ERR       (3)     /* error registering SIGINT */
#define EXIT_INV_INT_PERIOD       (4)     /* error invalid int. period*/
#define EXIT_MIN_PRIORITY_ERR     (5)     /* error, minimum priority  */
#define EXIT_MAX_PRIORITY_ERR     (6)     /* error, maximum priority  */
#define EXIT_INV_SCHED_PRIORITY   (7)     /* error, invalid priority  */
#define EXIT_SET_SCHEDULER_ERR    (8)     /* error, set scheduler     */
#define EXIT_PREV_TIME_OF_DAY_ERR (9)     /* error, init. prev.       */
/*   time of day            */

#define MAX_FILE_NAME_LEN         (32)    /* maximum file name length */

#define STRINGS_EQUAL             ((int) 0) /* strcmp value if equal  */

#define MIN_INT_PERIOD_MILLISEC   (   5L) /* minimum interrupt period */
#define MAX_INT_PERIOD_MILLISEC   (5000L) /* maximum interrupt period */
#define DEF_INT_PERIOD_MILLISEC   (  10L) /* default interrupt period */

#define READ_FILE_MESSAGE "This is a junk file. Must contain at least 1 byte though!\n"

/* The user can specify that the program pad out the write file to
   a given number of bytes. But a minimum number needs to be written. This
   will contain the jitter info.
*/
#define MIN_WRITE_BYTES     30
#define DEFAULT_WRITE_BYTES 30
#define MAX_WRITE_BYTES 4096

/* used for gen a printable ascii random # between spc and '~' */
#define MIN_ASCII 32     /* <SPACE> can be char*/
#define MAX_ASCII 126.0 /* needs to be float. See man rand()  */

/*----------------------------------------------------------------------
 * It appears that the preprocessor can't handle multi-line #if
 * statements. Thus, the check on the default is divided into two
 * #if statements.
 *---------------------------------------------------------------------*/
#if (DEF_INT_PERIOD_MILLISEC < MIN_INT_PERIOD_MILLISEC)
#error *** Invalid default interrupt period. ***
#endif

#if (DEF_INT_PERIOD_MILLISEC > MAX_INT_PERIOD_MILLISEC)
#error *** Invalid default interrupt period. ***
#endif


#define TRUE                      1  /* Boolean true value       */
#define FALSE                     0

/* Time conversion constants. */
#define MILLISEC_PER_SEC          (1000L) /* milliseconds per second  */
#define MICROSEC_PER_MILLISEC     (1000L) /* microsecs per millisec   */
#define MICROSEC_PER_SEC          (1000000L) /* microsecs per second  */

#define PRIORITY_POLICY           ((int) SCHED_RR) /* If specified iwth "-p" */



/************************** Module Variables **************************/
/* version identifier (value supplied by CVS)*/
static const char Version[] = "$Id: JitterTest.c,v 1.13 2005/11/07 11:15:20 gleixner Exp $";

static char OutFileName[MAX_FILE_NAME_LEN+1];  /* output file name            */
static char LogFile[MAX_FILE_NAME_LEN+1] = "/dev/console"; /* default */
static char ReadFile[MAX_FILE_NAME_LEN+1]; /* This file is read. Should
                                              contain at least 1 byte */

static int WriteBytes = DEFAULT_WRITE_BYTES; /* pad out file to these many bytes. */
static int Fd1;                        /* fd where the above buffer if o/p */
static int Cfd;                        /* fd to console (or file specified) */
static int Fd2;                        /* fd for the ReadFile         */
static int DoRead = FALSE;             /* should we attempt to ReadFile?*/
static long InterruptPeriodMilliSec;   /* interrupt period, millisec  */
static int MinPriority;                /* minimum scheduler priority  */
static int MaxPriority;                /* maximum scheduler priority  */
static int RequestedPriority;          /* requested priority          */
static struct itimerval ITimer;        /* interrupt timer structure   */
static struct timeval PrevTimeVal;     /* previous time structure     */
static struct timeval CurrTimeVal;     /* current time structure      */
static long LastMaxDiff = 0; /* keeps track of worst jitter encountered */

static int GrabKProfile = FALSE; /* To help determine system bottle necks
                                    this parameter can be set. This causes
                                    the /proc/profile file to be read and
                                    stored in unique filenames in current
                                    dir, and indication to be o/p on the
                                    /dev/console also.
                                 */
static long ProfileTriggerMSecs = 15000l; /* Jitter time in seconds that triggers
                                       a snapshot of the profile to be taken

                                    */
static int SignalGCTask = FALSE; /* should be signal SIGSTOP/SIGCONT to gc task?*/
static int GCTaskPID;

static int RunAsRTTask = FALSE; /* default action unless priority is
				   specified on cmd line */


/********************* Local Function Prototypes **********************/
void HandleCmdLineArgs(int argc, char *argv[]);
void SetFileName(char * pFileName);
void SetInterruptPeriod(char * pASCIIInterruptPeriodMilliSec);
void SetSchedulerPriority(char * pASCIISchedulerPriority);

void PrintVersionInfo(void);
void PrintHelpInfo(void);

int Write(int fd, void *buf, size_t bytes, int lineNo);

void InitITimer(struct itimerval * pITimer, int action);

/* For catching timer interrupts (SIGALRM). */
void AlarmHandler(int sigNum);

/* For catching Ctrl+C SIGINT. */
void SignalHandler(int sigNum);



/***********************************************************************
 *  main function
 *  return: exit code
 ***********************************************************************/
int main(
    int argc,
    char *argv[])
{
    struct sched_param schedParam;

    int mypri;
    char tmpBuf[200];


    strcpy(OutFileName,"jitter.dat");
    InterruptPeriodMilliSec = MIN_INT_PERIOD_MILLISEC;

    /* Get the minimum and maximum priorities. */
    MinPriority = sched_get_priority_min(PRIORITY_POLICY);
    MaxPriority = sched_get_priority_max(PRIORITY_POLICY);
    if (MinPriority == -1) {
        printf("\n*** Unable to get minimum priority. ***\n");
        exit(EXIT_MIN_PRIORITY_ERR);
    }
    if (MaxPriority == -1) {
        printf("\n*** Unable to get maximum priority. ***\n");
        exit(EXIT_MAX_PRIORITY_ERR);
    }

    /* Set requested priority to minimum value as default. */
    RequestedPriority = MinPriority;

    HandleCmdLineArgs(argc, argv);

    if(mlockall(MCL_CURRENT|MCL_FUTURE) < 0){
        printf("Could not lock task into memory. Bye\n");
        perror("Error");
    }

    if(RunAsRTTask){

        /* Set the priority. */
        schedParam.sched_priority = RequestedPriority;
	if (sched_setscheduler(
			       0,
			       PRIORITY_POLICY,
			       &schedParam) != (int) 0) {
	  printf("Exiting: Unsuccessful sched_setscheduler.\n");
	  close(Fd1);
	  exit(EXIT_SET_SCHEDULER_ERR);
	}


	/* Double check as I have some doubts that it's really
	   running at realtime priority! */
	if((mypri = sched_getscheduler(0)) != RequestedPriority)
	  {
	    printf("Not running with request priority %i. running with priority %i instead!\n",
		   RequestedPriority, mypri);
	  }else
	    {
	      printf("Running with %i priority. Good!\n", mypri);
	    }

    }

    /*------------------------- Initializations --------------------------*/
    if((Fd1 = open(OutFileName, O_RDWR|O_CREAT|O_SYNC, S_IRWXU)) <= 0)
    {
        perror("Cannot open outfile for write:");
        exit(1);
    }

    /* If a request is made to read from a specified file, then create that
       file and fill with junk data so that there is something there to read.
    */
    if(DoRead)
    {

        if((Fd2 = open(ReadFile, O_RDWR|O_CREAT|O_SYNC|O_TRUNC, S_IRWXU)) <= 0)
        {
            perror("cannot open read file:");
            exit(1);
        }else
        {

            /* Don't really care how much we write here */
            if(write(Fd2, READ_FILE_MESSAGE, strlen(READ_FILE_MESSAGE)) < 0)
            {
                perror("Problems writing to readfile:");
                exit(1);
            }
            lseek(Fd2, 0, SEEK_SET); /* position back to byte #0 */
        }
    }



    /* Also log output to console. This way we can capture it
       on a serial console to a log file.
    */
    if((Cfd   = open(LogFile, O_WRONLY|O_SYNC)) <= 0)
    {
        perror("cannot open o/p logfile:");
        exit(1);
    }


    /* Set-up handler for periodic interrupt. */
    if (signal(SIGALRM, &AlarmHandler) == SIG_ERR) {
        printf("Couldn't register signal handler for SIGALRM.\n");
        sprintf(tmpBuf,
                "Couldn't register signal handler for SIGALRM.\n");
        Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

        close(Fd1);
        exit(EXIT_REG_SIGALRM_ERR);
    }

    /* Set-up handler for Ctrl+C to exit the program. */
    if (signal(SIGINT, &SignalHandler) == SIG_ERR) {
        printf("Couldn't register signal handler for SIGINT.\n");
        sprintf(tmpBuf,
                "Couldn't register signal handler for SIGINT.\n");
        Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

        close(Fd1);
        exit(EXIT_REG_SIGINT_ERR);
    }

    printf("Press Ctrl+C to exit the program.\n");
    printf("Output File:        %s\n", OutFileName);
    printf("Scheduler priority: %d\n", RequestedPriority);
    sprintf(tmpBuf, "\nScheduler priority: %d\n",
            RequestedPriority);
    Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

    Write(Cfd, tmpBuf, strlen(tmpBuf), __LINE__);

    printf("Interrupt period:   %ld milliseconds\n",
           InterruptPeriodMilliSec);
    sprintf(tmpBuf, "Interrupt period:   %ld milliseconds\n",
            InterruptPeriodMilliSec);

    Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

    Write(Cfd, tmpBuf, strlen(tmpBuf), __LINE__);


    fflush(0);



    /* Initialize the periodic timer. */
    InitITimer(&ITimer, ONESHOT);

    /* Initialize "previous" time. */
    if (gettimeofday(&PrevTimeVal, NULL) != (int) 0) {
        printf("Exiting - ");
        printf("Unable to initialize previous time of day.\n");
        sprintf(tmpBuf, "Exiting - ");
        Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

        sprintf(tmpBuf,
                "Unable to initialize previous time of day.\n");

        Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

    }

    /* Start the periodic timer. */
    setitimer(ITIMER_REAL, &ITimer, NULL);


    while(TRUE) {    /* Intentional infinite loop. */
        /* Sleep for one second. */
        sleep((unsigned int) 1);
    }

    /* Just in case. File should be closed in SignalHandler. */
    close(Fd1);
    close(Cfd);

    return 0;
}




/***********************************************************************
 *                                SignalHandler
 *  This is a handler for the SIGINT signal. It is assumed that the
 *  SIGINT is due to the user pressing Ctrl+C to halt the program.
 *  output: N/A
 ***********************************************************************/
void SignalHandler(
    int sigNum)
{

    char tmpBuf[200];

    /* Note sigNum not used. */
    printf("Ctrl+C detected. Worst Jitter time was:%fms.\nJitterTest exiting.\n",
           (float)LastMaxDiff/1000.0);

    sprintf(tmpBuf,
            "\nCtrl+C detected. Worst Jitter time was:%fms\nJitterTest exiting.\n",
            (float)LastMaxDiff/1000.0);
    Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

    Write(Cfd, tmpBuf, strlen(tmpBuf), __LINE__);

    close(Fd1);
    close(Cfd);
    exit(0);
}





/*
  A snapshot of the /proc/profile needs to be taken.
  This is stored as a new file every time, and the
  stats reset by doing a (any) write to the /proc/profile
  file.
 */
void doGrabKProfile(int jitterusec, char *fileName)
{
    int fdSnapshot;
    int fdProfile;
    int readBytes;
    char readBuf[4096];

    if((fdSnapshot = open(fileName, O_WRONLY | O_CREAT, S_IRWXU)) <= 0)
    {
        fprintf(stderr, "Could not open file %s.\n", fileName);
        perror("Error:");
        return;
    }

    if((fdProfile = open("/proc/profile", O_RDWR)) <= 0)
    {
        fprintf(stderr, "Could not open file /proc/profile. Make sure you booted with profile=2\n");
        close(fdSnapshot);
        return;
    }

    while((readBytes = read(fdProfile, readBuf, sizeof(readBuf))) > 0)
    {
	int writeBytes = write(fdSnapshot, readBuf, readBytes);
	if (writeBytes != readBytes) {
		perror("write error");
		break;
	}
    }

    close(fdSnapshot);

    if(write(fdProfile, readBuf, 1) != 1)
    {
        perror("Could Not clear profile by writing to /proc/profile:");
    }

    close(fdProfile);



}/* end doGrabKProfile()*/


/*
  Call this routine to clear the kernel profiling buffer /proc/profile
*/
void clearProfileBuf(void){


  int fdProfile;
  char readBuf[10];


  if((fdProfile = open("/proc/profile", O_RDWR)) <= 0)
    {
      fprintf(stderr, "Could not open file /proc/profile. Make sure you booted with profile=2\n");
      return;
    }


  if(write(fdProfile, readBuf, 1) != 1)
    {
      perror("Could Not clear profile by writing to /proc/profile:");
    }

  close(fdProfile);


}/* end clearProfileBuf() */





/***********************************************************************
 *                                AlarmHandler
 *  This is a handler for the SIGALRM signal (due to the periodic
 *  timer interrupt). It prints the time (seconds) to
 *  the output file.
 *  output: N/A
 ***********************************************************************/
void AlarmHandler(
    int sigNum)                     /* signal number (not used)         */
{

    long timeDiffusec;  /* diff time in micro seconds */
    long intervalusec;


    char tmpBuf[MAX_WRITE_BYTES];
    int cntr;
    char padChar;

    static int profileFileNo = 0;
    char profileFileName[150];

    static int seedStarter = 0; /* carries over rand info to next time
				   where time() will be the same as this time
				   if invoked < 1sec apart.
				*/

    if (gettimeofday(&CurrTimeVal, NULL) == (int) 0) {

        /*----------------------------------------------------------------
         * Compute the elapsed time between the current and previous
         * time of day values and store the result.
         *
         * Print the elapsed time to the output file.
         *---------------------------------------------------------------*/

        timeDiffusec = (long)(((((long long)CurrTimeVal.tv_sec) * 1000000L) + CurrTimeVal.tv_usec) -
                        (((long long)PrevTimeVal.tv_sec * 1000000L) + PrevTimeVal.tv_usec));

        sprintf(tmpBuf," %f ms  ", (float)timeDiffusec/1000.0);

        intervalusec = InterruptPeriodMilliSec * 1000L;

        timeDiffusec = timeDiffusec - intervalusec;

        sprintf(&tmpBuf[strlen(tmpBuf)]," %f ms", (float)timeDiffusec/1000.0);


	/* should we send a SIGSTOP/SIGCONT to the specified PID? */
	if(SignalGCTask){

	  if(kill(GCTaskPID, SIGSTOP) < 0){

	    perror("error:");
	  }
	}


        /* Store some historical #'s */
        if(abs(timeDiffusec) > LastMaxDiff)
	{
            LastMaxDiff = abs(timeDiffusec);
            sprintf(&tmpBuf[strlen(tmpBuf)],"!");

	    if((GrabKProfile == TRUE) && (ProfileTriggerMSecs < (abs(timeDiffusec)/1000)))
	      {
		  sprintf(profileFileName, "JitterTest.profilesnap-%i", profileFileNo);

		  /* go and grab the kernel performance profile. */
		  doGrabKProfile(timeDiffusec, profileFileName);
		  profileFileNo++; /* unique name next time */

		  /* Say so on the console so that a marker gets put in the console log */
		  sprintf(&tmpBuf[strlen(tmpBuf)],"<Profile saved in file:%s>",
			  profileFileName);

	      }

        }




        sprintf(&tmpBuf[strlen(tmpBuf)],"\n"); /* CR for the data going out of the console */

        Write(Cfd, tmpBuf, strlen(tmpBuf), __LINE__);


        /* The "-1" below takes out the '\n' at the end that we appended for the msg printed on
         the console.*/
        sprintf(&tmpBuf[strlen(tmpBuf)-1]," PadBytes:");

        /* Now pad the output file if requested by user. */
        if(WriteBytes > MIN_WRITE_BYTES)
        {

	    /* start from a new place every time */
	    srand(time(NULL) + seedStarter);

            /* already written MIN_WRITE_BYTES by now */
            for(cntr = strlen(tmpBuf); cntr < WriteBytes - 1 ; cntr++) /* "-1" adj for '\n' at end */
            {
	        /* don't accept any # < 'SPACE' */
	        padChar = (char)(MIN_ASCII+(int)((MAX_ASCII-(float)MIN_ASCII)*rand()/(RAND_MAX+1.0)));


                /*
                padChar = (cntr % (126-33)) + 33;
		*/

                tmpBuf[cntr] = padChar;
            }

	    seedStarter = tmpBuf[cntr-1]; /* save for next time */

            tmpBuf[cntr] = '\n'; /* CR for the data going into the outfile. */
            tmpBuf[cntr+1] = '\0'; /* NULL terminate the string */
        }

        /* write out the entire line to the output file. */
        Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);


        /* Read a byte from the specified file */
        if(DoRead)
        {

	    cntr = read(Fd2, tmpBuf, 1);
	    if (cntr < 0)
		perror("read error");
            lseek(Fd2, 0, SEEK_SET); /* back to start */
        }


        /* Start the periodic timer again. */
        setitimer(ITIMER_REAL, &ITimer, NULL);


        /* Update previous time with current time. */
        PrevTimeVal.tv_sec  = CurrTimeVal.tv_sec;
        PrevTimeVal.tv_usec = CurrTimeVal.tv_usec;
    }

    else {
        sprintf(tmpBuf, "gettimeofday error \n");
        Write(Fd1, tmpBuf, strlen(tmpBuf), __LINE__);

        printf("gettimeofday error \n");
    }

    /* now clear the profiling buffer */
    if(GrabKProfile == TRUE){

      clearProfileBuf();
    }

    /* should we send a SIGSTOP/SIGCONT to the specified PID? */
    if(SignalGCTask){

      if(kill(GCTaskPID, SIGCONT) < 0){

	perror("error:");
      }
    }


    return;
}



/***********************************************************************
 *                                 InitITimer
 *  This function initializes the 'struct itimerval' structure whose
 *  address is passed to interrupt every InterruptPeriodMilliSec.
 *  output: N/A
 ***********************************************************************/
void InitITimer(
    struct itimerval * pITimer,      /* pointer to interrupt timer struct*/
    int action)                      /* ONESHOT or autosetting? */
{
    long seconds;                   /* seconds portion of int. period   */
    long microSeconds;              /* microsec. portion of int. period */

    /*--------------------------------------------------------------------
     * Divide the desired interrupt period into its seconds and
     * microseconds components.
     *-------------------------------------------------------------------*/
    if (InterruptPeriodMilliSec < MILLISEC_PER_SEC)  {
        seconds = 0L;
        microSeconds = InterruptPeriodMilliSec * MICROSEC_PER_MILLISEC;
    }
    else {
        seconds = InterruptPeriodMilliSec / MILLISEC_PER_SEC;
        microSeconds =
            (InterruptPeriodMilliSec - (seconds * MILLISEC_PER_SEC)) *
            MICROSEC_PER_MILLISEC;
    }

    /* Initialize the interrupt period structure. */
    pITimer->it_value.tv_sec     = seconds;
    pITimer->it_value.tv_usec    = microSeconds;

    if(action == ONESHOT)
    {
        /* This will (should) prevent the timer from restarting itself */
        pITimer->it_interval.tv_sec  = 0;
        pITimer->it_interval.tv_usec = 0;
    }else
    {
        pITimer->it_interval.tv_sec  = seconds;
        pITimer->it_interval.tv_usec = microSeconds;

    }

    return;
}


/***********************************************************************
 *                             HandleCmdLineArgs
 *  This function handles the command line arguments.
 *  output: stack size
 ***********************************************************************/
void HandleCmdLineArgs(
    int argc,                       /* number of command-line arguments */
    char *argv[])                   /* ptrs to command-line arguments   */
{
    int argNum;                     /* argument number                  */

    if (argc > (int) 1) {

        for (argNum = (int) 1; argNum < argc; argNum++) {

            /* The command line contains an argument. */

            if ((strcmp(argv[argNum],"--version") == STRINGS_EQUAL) ||
                (strcmp(argv[argNum],"-v")        == STRINGS_EQUAL)) {
                /* Print version information and exit. */
                PrintVersionInfo();
                exit(0);
            }

            else if ((strcmp(argv[argNum],"--help") == STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-h")     == STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-?")     == STRINGS_EQUAL)) {
                /* Print help information and exit. */
                PrintHelpInfo();
                exit(0);
            }

            else if ((strcmp(argv[argNum],"--file") == STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-f")     == STRINGS_EQUAL)) {
                /* Set the name of the output file. */
                ++argNum;
                if (argNum < argc) {
                    SetFileName(argv[argNum]);
                }
                else {
                    printf("*** Output file name not specified. ***\n");
                    printf("Default output file name will be used.\n");
                }
            }

            else if ((strcmp(argv[argNum],"--time") == STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-t")     == STRINGS_EQUAL)) {
                /* Set the interrupt period. */
                ++argNum;
                if (argNum < argc) {
                    SetInterruptPeriod(argv[argNum]);
                }
                else {
                    printf("*** Interrupt period not specified. ***\n");
                    printf("Default interrupt period will be used.\n");
                }

            }

            else if ((strcmp(argv[argNum],"--priority") ==
                      STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-p")       == STRINGS_EQUAL)) {
                /* Set the scheduler priority. */
                ++argNum;
                if (argNum < argc) {
                    SetSchedulerPriority(argv[argNum]);
                }
                else {
                    printf("*** Scheduler priority not specified. ***\n");
                    printf("Default scheduler priority will be used.\n");
                }

            }

            else if ((strcmp(argv[argNum],"--readfile") ==
                      STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-r")       == STRINGS_EQUAL)) {
                /* Set the file to read*/
                ++argNum;

                strncpy(ReadFile, argv[argNum], sizeof(ReadFile));
                DoRead = TRUE;
            }

            else if ((strcmp(argv[argNum],"--write_bytes") ==
                      STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-w")       == STRINGS_EQUAL)) {
                /* Set the file to read*/
                ++argNum;

                WriteBytes = atoi(argv[argNum]);

                if(WriteBytes < MIN_WRITE_BYTES)
                {
                    printf("Writing less than %i bytes is not allowed. Bye.\n",
                           MIN_WRITE_BYTES);
                    exit(0);
                }


            }

            else if ((strcmp(argv[argNum],"--consolefile") ==
                      STRINGS_EQUAL) ||
                     (strcmp(argv[argNum],"-c")       == STRINGS_EQUAL)) {
	      /* Set the file to log console log on. */
	      ++argNum;

	      strncpy(LogFile, argv[argNum], sizeof(LogFile));
            }

            else if ((strcmp(argv[argNum],"--grab_kprofile") ==
                      STRINGS_EQUAL))
	      {
                /* We will read the /proc/profile file on configurable timeout */
                GrabKProfile = TRUE;

                ++argNum;

                /* If the jittter is > this #, then the profile is grabbed. */
                ProfileTriggerMSecs = (long) atoi(argv[argNum]);

		if(ProfileTriggerMSecs <= 0){

		  printf("Illegal value for profile trigger threshold.\n");
		  exit(0);
		}
	      }

            else if ((strcmp(argv[argNum],"--siggc") ==
                      STRINGS_EQUAL))
	      {
                /* We will SIGSTOP/SIGCONT the specified pid */
                SignalGCTask = TRUE;

                ++argNum;

                GCTaskPID = atoi(argv[argNum]);

		if(ProfileTriggerMSecs <= 0){

		  printf("Illegal value for JFFS(2) GC task pid.\n");
		  exit(0);
		}
	      }


            else {
	      /* Unknown argument. Print help information and exit. */
	      printf("Invalid option %s\n", argv[argNum]);
	      printf("Try 'JitterTest --help' for more information.\n");
	      exit(0);
            }
        }
    }

    return;
}


/***********************************************************************
 *                                SetFileName
 *  This function sets the output file name.
 *  output: N/A
 ***********************************************************************/
void SetFileName(
    char * pFileName)               /* ptr to desired output file name  */
{
    size_t fileNameLen;             /* file name length (bytes)         */

    /* Check file name length. */
    fileNameLen = strlen(pFileName);
    if (fileNameLen > (size_t) MAX_FILE_NAME_LEN) {
        printf("File name %s exceeds maximum length %d.\n",
               pFileName, MAX_FILE_NAME_LEN);
        exit(0);
    }

    /* File name length is OK so save the file name. */
    strcpy(OutFileName, pFileName);

    return;
}


/***********************************************************************
 *                             SetInterruptPeriod
 *  This function sets the interrupt period.
 *  output: N/A
 ***********************************************************************/
void SetInterruptPeriod(
    char *                          /* ptr to desired interrupt period  */
    pASCIIInterruptPeriodMilliSec)
{
    long period;                    /* interrupt period                 */

    period = atol(pASCIIInterruptPeriodMilliSec);
    if ((period < MIN_INT_PERIOD_MILLISEC) ||
        (period > MAX_INT_PERIOD_MILLISEC)) {
        printf("Invalid interrupt period: %ld ms.\n", period);
        exit(EXIT_INV_INT_PERIOD);
    }
    else {
        InterruptPeriodMilliSec = period;
    }
    return;
}


/***********************************************************************
 *                            SetSchedulerPriority
 *  This function sets the desired scheduler priority.
 *  output: N/A
 ***********************************************************************/
void SetSchedulerPriority(
    char * pASCIISchedulerPriority) /* ptr to desired scheduler priority*/
{
    int priority;                   /* desired scheduler priority value */

    priority = atoi(pASCIISchedulerPriority);
    if ((priority < MinPriority) ||
        (priority > MaxPriority)) {
        printf("Scheduler priority %d outside of range [%d, %d]\n",
               priority, MinPriority, MaxPriority);
        exit(EXIT_INV_SCHED_PRIORITY);
    }
    else {
        RequestedPriority = priority;
	RunAsRTTask = TRUE; /* We shall run as a POSIX real time task */
    }
    return;
}


/***********************************************************************
 *                              PrintVersionInfo
 *  This function prints version information.
 *  output: N/A
 ***********************************************************************/
void PrintVersionInfo(void)
{
    printf("JitterTest version %s\n", Version);
    printf("Copyright (c) 2001, Daniel Industries, Inc.\n");
    return;
}


/***********************************************************************
 *                               PrintHelpInfo
 *  This function prints help information.
 *  output: N/A
 ***********************************************************************/
void PrintHelpInfo(void)
{
    printf("Usage: JitterTest [options]\n");
    printf("       *** Requires root privileges. ***\n");
    printf("Option:\n");
    printf("  [-h, --help, -?]           Print this message and exit.\n");
    printf("  [-v, --version]            ");
    printf(         "Print the version number of JitterTest and exit.\n");
    printf("  [-f FILE, --file FILE]     Set output file name to FILE. Typically you would put this on the fs under test\n");
    printf("  [-c FILE, --consolefile]   Set device or file to write the console log to.\n\tTypically you would set this to /dev/console and save it on another computer.\n");
    printf("  [-w BYTES, --write_bytes BYTES  Write BYTES to FILE each period.\n");
    printf("  [-r FILE, --readfile FILE]     Also read 1 byte every cycle from FILE. FILE will be created and filled with data.\n");
    printf("  [-t <n>, --time <n>]       ");
    printf(         "Set interrupt period to <n> milliseconds.\n");
    printf("                           ");
    printf(         "Range: [%ld, %ld] milliseconds.\n",
                    MIN_INT_PERIOD_MILLISEC, MAX_INT_PERIOD_MILLISEC);
    printf("  [-p <n>, --priority <n>]  ");
    printf(         "Set scheduler priority to <n>.\n");
    printf("                           ");
    printf(         "Range: [%d, %d] (higher number = higher priority)\n",
                    MinPriority, MaxPriority);
    printf("  [--grab_kprofile <THRESHOLD in ms>]    Read the /proc/profile if jitter is > THRESHOLD and store in file.\n");
    printf("  [--siggc <PID>]   Before writing to fs send SIGSTOP to PID. After write send SIGCONT\n");
    return;

}


/* A common write that checks for write errors and exits. Pass it __LINE__ for lineNo */
int Write(int fd, void *buf, size_t bytes, int lineNo)
{

    int err;

    err = write(fd, buf, bytes);

    if(err < bytes)
    {

        printf("Write Error at line %i! Wanted to write %zu bytes, but wrote only %i bytes.\n",
               lineNo, bytes, err);
        perror("Write did not complete. Error. Bye:"); /* show error from errno. */
	exit(1);

    }

    return err;

}/* end Write*/
