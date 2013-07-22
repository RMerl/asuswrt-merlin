/*
 ***********************************************************************
 *
 *  Copyright:          Daniel Measurement and Control, Inc.
 *                           9753 Pine Lake Drive
 *                             Houston, TX 77055
 *
 *  Created by: Vipin Malik
 *  Released under GPL by permission of Daniel Industries.
 *
 * This software is licensed under the GPL version 2. Plese see the file
 * COPYING for details on the license.
 *
 * NO WARRANTY: Absolutely no claims of warranty or fitness of purpose
 *              are made in this software. Please use at your own risk.
 *
  File: plotJittervsFill.c
  By: Vipin Malik

  About: This program reads in a jitter log file as created
  by the JitterTest.c program and extracts all the jitters
  in the file that are greater than a threshold specified
  as a parameter on the cmd line. It also extracts the
  amount of disk space at (form the "df" out that should also
  be present in the log file) after the jitter extracted.

  It writes the data to the stderr (where you may redirect it).
  It is suitable for plotting, as the data is written as
  COL1=UsedSpace COL2=Jitter

  $Id: plotJittervsFill.c,v 1.6 2005/11/07 11:15:21 gleixner Exp $
  $Log: plotJittervsFill.c,v $
  Revision 1.6  2005/11/07 11:15:21  gleixner
  [MTD / JFFS2] Clean up trailing white spaces

  Revision 1.5  2001/08/10 19:23:11  vipin
  Ready to be released under GPL! Added proper headers etc.

  Revision 1.4  2001/07/02 22:25:40  vipin
  Fixed couple of minor cosmetic typos.

  Revision 1.3  2001/07/02 14:46:46  vipin
  Added a debug option where it o/p's line numbers to debug funky values.

  Revision 1.2  2001/06/26 19:48:57  vipin
  Now prints out jitter values found at end of log file, after which
  no new "df" disk usage values were encountered. The last "df" disk usage
  encountered is printed for these orphaned values.

  Revision 1.1  2001/06/25 19:13:55  vipin
  Added new file- plotJittervsFill.c- that mines the data log file
  outputed from the fillFlash.sh script file and JitterTest.c file
  and produces output suitable to be plotted.
  This output plot may be examined to see visually the relationship
  of the Jitter vs disk usage of the fs under test.

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static char Version_string[] = "$Id: plotJittervsFill.c,v 1.6 2005/11/07 11:15:21 gleixner Exp $";
static char LogFile[250] = "InputLogFile.log";

static int JitterThreshold_ms = 1000;
static int Debug = 0; /* Debug level. Each "-d" on the cmd line increases the level */

#define TRUE  1
#define FALSE 0

#define MIN_JITTER_THRESHOLD 1 /* ms minimum jitter threshold */

void PrintHelpInfo(void)
{
    printf("Usage: plotJittervsFill [options] -f [--file] <input log file name> -t [--jitter_threshold] <jitter threshold in ms>\n");
    printf("[options]:\n-v [--version] Print version and exit\n");
    printf("-d Debug. Prints input file line number for each data point picked up.\n");
    printf("-h [--help] [-?] Print this help screen and exit.\n");
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

            if ((strcmp(argv[argNum],"--version") == 0) ||
                (strcmp(argv[argNum],"-v")        == 0)) {
                /* Print version information and exit. */
                printf("%s\n", Version_string);
                exit(0);
            }

            else if ((strcmp(argv[argNum],"--help") == 0) ||
                     (strcmp(argv[argNum],"-h")     == 0) ||
                     (strcmp(argv[argNum],"-?")     == 0)) {
                /* Print help information and exit. */
                PrintHelpInfo();
                exit(0);
            }

            else if ((strcmp(argv[argNum],"--file") == 0) ||
                     (strcmp(argv[argNum],"-f")     == 0)) {
                /* Set the name of the output file. */
                ++argNum;
                if (argNum < argc) {
                    strncpy(LogFile, argv[argNum], sizeof(LogFile));
                }
                else {
                    printf("*** Input file name not specified. ***\n");
                    exit(0);
                }
            }

            else if ((strcmp(argv[argNum],"--jitter_threshold") == 0) ||
                     (strcmp(argv[argNum],"-t") == 0)) {
                /* Set the file to read*/
                ++argNum;

                JitterThreshold_ms = atoi(argv[argNum]);

                if(JitterThreshold_ms < MIN_JITTER_THRESHOLD)
                {
                    printf("A jitter threshold less than %i ms is not allowed. Bye.\n",
                           MIN_JITTER_THRESHOLD);
                    exit(0);
                }
            }

            else if ((strcmp(argv[argNum],"-d") == 0))
            {
                /* Increment debug level */

                Debug++;
            }

            else {
                /* Unknown argument. Print help information and exit. */
                printf("Invalid option %s\n", argv[argNum]);
                printf("Try 'plotJittervsFill --help' for more information.\n");
                exit(0);
            }
        }
    }

    return;
}





int main(
    int argc,
    char *argv[])
{

    char lineBuf[1024]; /* how long a single line be? */
    int converted;
    int lineNo = 0;
    int cnt;

    FILE *fp;

    int junkInt1, junkInt2, junkInt3;
    float junkFloat1;
    float jitter_ms;

#define MAX_SAVE_BUFFER 1000 /* How many values will be picked up while searching for
                          a % disk full line (i.e. before they can be printed out)
                        */
    int saveJitter[MAX_SAVE_BUFFER]; /* lets us record multiple jitter values that exceed
                            our threshold till we find a "df" field- which is when
                            we can o/p all these values.
                         */
    int dataLineNo[MAX_SAVE_BUFFER]; /* The saved line #'s for the above. Printed if debug specified. */

    int saveJitterCnt = 0;
    int lookFor_df = FALSE;
    int dfPercent = -1; /* will be >= 0 if at least one found. The init value is a flag. */

    char junkStr1[500], junkStr2[500];

    HandleCmdLineArgs(argc, argv);

    if((fp = fopen(LogFile, "r")) == NULL)
    {
        printf("Unable to open input log file %s for read.\b", LogFile);
        perror("Error:");
        exit(1);
    }



    while(fgets(lineBuf, sizeof(lineBuf), fp) != NULL)
    {
        lineNo++;


        /* Are we looking for a "df" o/p line? (to see how full
           the flash is?)*/

        /* is there a "%" in this line? */
        if((strstr(lineBuf, "%") != NULL) && (lookFor_df))
        {
            converted = sscanf(lineBuf, "%s %i %i %i %i\n",
                               junkStr1, &junkInt1, &junkInt2, &junkInt3, &dfPercent);
            if(converted < 5)
            {
                printf("Line %i contains \"%%\", but expected fields not found. Skipping.\n", lineNo);
            }else
            {
                /* Now print out the saved jitter values (in col2) with this dfPercent value as the col1. */
                for(cnt = 0; cnt < saveJitterCnt; cnt++)
                {
                    if(Debug)
                    {
                        fprintf(stderr, "%i\t%i\t%i\n", (int)dataLineNo[cnt],
                                dfPercent, (int)saveJitter[cnt]);
                    }else
                    {
                        fprintf(stderr, "%i\t%i\n", dfPercent, (int)saveJitter[cnt]);
                    }


                }

                saveJitterCnt = 0; /* all flushed. Reset for next saves. */
                lookFor_df = FALSE;
            }

        }


        /* is there a "ms" in this line?*/
        if(strstr(lineBuf, "ms") == NULL)
        {
            continue;
        }

        /* grab the ms jitter value */
        converted = sscanf(lineBuf, "%f %s %f %s\n", &junkFloat1, junkStr1, &jitter_ms, junkStr2);
        if(converted < 4)
        {
            printf("Line %i contains \"ms\", but expected fields not found. Converted %i, Skipping.",
                   lineNo, converted);
            printf("1=%i, 2=%s.\n", junkInt1, junkStr1);
            continue; /* not our jitter line*/
        }

        /* Is the jitter value > threshold value? */
        if(abs(jitter_ms) > JitterThreshold_ms)
        {
            /* Found a jitter line that matches our crietrion.
               Now set flag to be on the look out for the next
               "df" output so that we can see how full the flash is.
            */

            if(saveJitterCnt < MAX_SAVE_BUFFER)
            {
                saveJitter[saveJitterCnt] = (int)abs(jitter_ms); /* why keep the (ms) jitter in float */
                dataLineNo[saveJitterCnt] = lineNo;
                saveJitterCnt++;
                lookFor_df = TRUE;
            }
            else
            {
                printf("Oops! I've run out of buffer space before I found a %% use line. Dropping itter value. Increase MAX_SAVE_BUFFER and recompile.\n");
            }


        }

    }


    /* Now print out any saved jitter values that were not printed out because we did not find
       and "df" after these were picked up. Only print if a "df" disk usage was ever found.
     */
    if(lookFor_df && (dfPercent >= 0))
    {
        /* Now print out the saved jitter values (in col2) with this dfPercent value as the col1. */
        for(cnt = 0; cnt < saveJitterCnt; cnt++)
        {
            fprintf(stderr, "%i\t%i\n", dfPercent, (int)saveJitter[cnt]);
        }
    }

    return 0;


}/* end main() */




