/*

 * Copyright Daniel Industries.
 *
 * Created by: Vipin Malik (vipin.malik@daniel.com)
 *
 * This code is released under the GPL version 2. See the file COPYING
 * for more details.
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.

  This program opens files in progression (file00001, file00002 etc),
  upto MAX_NUM_FILES and checks their CRC. If a file is not found or the
  CRC does not match it stops it's operation.

  Everything is logged in a logfile called './logfile'.

  If everything is ok this program sends a signal, via com1, to the remote
  power control box to power cycle this computer.

  This program then proceeds to create new files file0....file<MAX_NUM_FILES>
  in a endless loop and checksum each before closing them.

  STRUCTURE OF THE FILES:
  The fist int is the size of the file in bytes.
  The last 2 bytes are the CRC for the entire file.
  There is random data in between.

  The files are opened in the current dir.

  $Id: checkfs.c,v 1.8 2005/11/07 11:15:17 gleixner Exp $
  $Log: checkfs.c,v $
  Revision 1.8  2005/11/07 11:15:17  gleixner
  [MTD / JFFS2] Clean up trailing white spaces

  Revision 1.7  2001/06/21 23:04:17  dwmw2
  Initial import to MTD CVS

  Revision 1.6  2001/06/08 22:26:05  vipin
  Split the modbus comm part of the program (that sends the ok to pwr me down
  message) into another file "comm.c"

  Revision 1.5  2001/06/08 21:29:56  vipin
  fixed small issue with write() checking for < 0 instead of < (bytes to be written).
  Now it does the latter (as it should).

  Revision 1.4  2001/05/11 22:29:40  vipin
  Added a test to check and err out if the first int in file (which tells us
  how many bytes there are in the file) is zero. This will prevent a corrupt
  file with zero's in it from passing the crc test.

  Revision 1.3  2001/05/11 21:33:54  vipin
  Changed to use write() rather than fwrite() when creating new file. Additionally,
  and more important, it now does a single write() for the entire data. This will
  enable us to use this program to test for power fail *data* reliability when
  writing over an existing file, specially on powr fail "safe" file systems as
  jffs/jffs2. Also added a new cmdline parameter "-e" that specifies the max # of
  errors that can be tolerated. This should be set to ZERO to test for the above,
  as old data should be reliabily maintained if the newer write never "took" before
  power failed. If the write did succeed, then the newer data will have its own
  CRC in place when it gets checked => hence no error. In theory at least!


  Revision 1.2  2001/05/11 19:27:33  vipin
  Added cmd line args to change serial port, and specify max size of
  random files created. Some cleanup. Added -Wall to Makefile.

  Revision 1.1  2001/05/11 16:06:28  vipin
  Importing checkfs (the power fail test program) into CVS. This was
  originally done for NEWS. NEWS had a lot of version, this is
  based off the last one done for NEWS. The "makefiles" program
  is run once initially to create the files in the current dir.
  "checkfs" is then run on every powerup to check consistancy
  of the files. See checkfs.c for more details.


*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "common.h"



extern int do_pwr_dn(int fd, int cycleCnt);

#define CMDLINE_PORT "-p"
#define CMDLINE_MAXFILEBYTES "-s"
#define CMDLINE_MAXERROR "-e"
#define CMDLINE_HELPSHORT "-?"
#define CMDLINE_HELPLONG "--help"


int CycleCount;

char SerialDevice[255] = "/dev/ttyS0"; /* default, can be changed
                                        through cmd line. */

#define MAX_INTS_ALLOW 100000 /* max # of int's in the file written.
                                 Statis limit to size struct. */
float FileSizeMax = 1024.0; /*= (file size in bytes), MUST be float*/

int MaxErrAllowed = 1; /* default, can ge changed thru cmd line*/


/* Needed for CRC generation/checking */
static const unsigned short crc_ccitt_table[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};


/*
  Set's up the Linux serial port. Must be passed string to device to
  open. Parameters are fixed to 9600,e,1

  [A possible enhancement to this program would be to pass these
  parameters via the command line.]

  Returns file descriptor to open port. Use this fd to write to port
  and close it later, when done.
*/
int setupSerial (const char *dev) {
    int i, fd;
    struct termios tios;

    fd = open(dev,O_RDWR | O_NDELAY );
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", dev, strerror(errno));
        exit(1);
    }
    if (tcgetattr(fd, &tios) < 0) {
        fprintf(stderr, "Could not get terminal attributes: %s", strerror(errno));
        exit(1);
    }

    tios.c_cflag =
        CS7 |
        CREAD |			// Enable Receiver
        HUPCL |			// Hangup after close
        CLOCAL |                // Ignore modem control lines
        PARENB;			// Enable parity (even by default)



    tios.c_iflag      = IGNBRK; // Ignore break
    tios.c_oflag      = 0;
    tios.c_lflag      = 0;
    for(i = 0; i < NCCS; i++) {
        tios.c_cc[i] = '\0';           // no special characters
    }
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    cfsetospeed (&tios, B9600);
    cfsetispeed (&tios, B9600);

    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0) {
        fprintf(stderr, "Could not set attributes: ,%s", strerror(errno));
        exit(1);
    }
    return fd;
}





//A portion of this code was taken from the AX.25 HDLC packet driver
//in LINUX. Once I test and have a better understanding of what
//it is doing, it will be better commented.

//For now one can speculate that the CRC routine always expects the
//CRC to calculate out to 0xf0b8 (the hardcoded value at the end)
//and returns TRUE if it is and FALSE if it doesn't.
//Why don't people document better!!!!
int check_crc_ccitt(char *filename)
{
    FILE *fp;
    FILE *logfp;
    unsigned short crc = 0xffff;
    int len;
    char dataByte;
    int retry;
    char done;

    fp =   fopen(filename,"rb");
    if(!fp){
        logfp = fopen("logfile","a"); /*open for appending only.*/
        fprintf(logfp, "Verify checksum:Error! Cannot open filename passed for verify checksum: %s\n",filename);
        fclose(logfp);
        return FALSE;
    }


    /*the first int contains an int that is the length of the file in long.*/
    if(fread(&len, sizeof(int), 1, fp) != 1){
        logfp = fopen("logfile","a"); /*open for appending only.*/
        fprintf(logfp, "verify checksum:Error reading from file: %s\n", filename);
        fclose(fp);
        fclose(logfp);
        return FALSE;
    }

    /* printf("Checking %i bytes for CRC in \"%s\".\n", len, filename); */

    /* Make sure that we did not read 0 as the number of bytes in file. This
       check prevents a corrupt file with zero's in it from passing the
       CRC test. A good file will always have some data in it. */
    if(len == 0)
    {

        logfp = fopen("logfile","a"); /*open for appending only.*/
        fprintf(logfp, "verify checksum: first int claims there are 0 data in file. Error!: %s\n", filename);
        fclose(fp);
        fclose(logfp);
        return FALSE;
    }


    rewind(fp);
    len+=2; /*the file has two extra bytes at the end, it's checksum. Those
              two MUST also be included in the checksum calculation.
            */

    for (;len>0;len--){
        retry=5; /*retry 5 times*/
        done = FALSE;
        while(!done){
            if(fread(&dataByte, sizeof(char), 1, fp) != 1){
                retry--;
            }else{
                done = TRUE;
            }
            if(retry == 0){
                done = TRUE;
            }
        }
        if(!retry){
            logfp = fopen("logfile","a"); /*open for appending only.*/
            fprintf(logfp, "Unexpected end of file: %s\n", filename);
            fprintf(logfp, "...bytes left to be read %i.\n",len);
            fclose(logfp);
            fclose(fp);
            return FALSE;
        }
        crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ dataByte) & 0xff];
    }
    fclose(fp);
    if( (crc & 0xffff) != 0xf0b8){
        /*printf("The CRC of read file:%x\n", crc); */
        return FALSE;
    }
    return TRUE;
}/*end check_crc_ccitt() */



/*
  Sends "OK to power me down" message to the remote
  power cycling box, via the serial port.
  Also updates the num power cycle count in a local
  file.
  This file "./cycleCnt" must be present. This is
  initially (and once) created by the separate "makefiles.c"
  program.
*/
void send_pwrdn_ok(void){

    int fd;
    FILE *cyclefp;
    int cycle_fd;

    cyclefp =   fopen("cycleCnt","rb");
    if(!cyclefp){
        printf("expecting file \"cycleCnt\". Cannot continue.\n");
        exit(1);
    }
    if(fread(&CycleCount, sizeof(CycleCount),1,cyclefp) != 1){
        fprintf(stderr, "Error! Unexpected end of file cycleCnt.\n");
        exit(1);
    }
    fclose(cyclefp);

    CycleCount++;

    /*now write this puppy back*/
    cyclefp  = fopen("cycleCnt","wb");
    cycle_fd = fileno(cyclefp);
    if(!cyclefp){
        fprintf(stderr, "Error! cannot open file for write:\"cycleCnt\". Cannot continue.\n");
        exit(1);
    }
    if(fwrite(&CycleCount, sizeof(CycleCount), 1,cyclefp) !=1){
        fprintf(stderr, "Error writing to file cycleCnt. Cannot continue.\n");
        exit(1);
    }
    if(fdatasync(cycle_fd)){
        fprintf(stderr, "Error! cannot sync file buffer with disk.\n");
        exit(1);
    }

    fclose(cyclefp);
    (void)sync();

    printf("\n\n Sending Power down command to the remote box.\n");
    fd = setupSerial(SerialDevice);

    if(do_pwr_dn(fd, CycleCount) < 0)
    {
        fprintf(stderr, "Error sending power down command.\n");
        exit(1);
    }

    close(fd);
}//end send_pwrnd_ok()




/*
  Appends 16bit CRC at the end of numBytes long buffer.
  Make sure buf, extends at least 2 bytes beyond.
 */
void appendChecksum(char *buf, int numBytes){

    unsigned short crc = 0xffff;
    int index = 0;

    /* printf("Added CRC (2 bytes) to %i bytes.\n", numBytes); */

    for (; numBytes > 0; numBytes--){

        crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ buf[index++]) & 0xff];
    }
    crc ^= 0xffff;
    /*printf("The CRC: %x\n\n", crc);*/

    buf[index++] = crc;
    buf[index++] = crc >> 8;



}/*end checksum()*/





/*
  This guy make a new "random data file" with the filename
  passed to it. This file is checksummed with the checksum
  stored at the end. The first "int" in the file is the
  number of int's in it (this is needed to know how much
  data to read and checksum later).
*/
void make_new_file(char *filename){


    int dfd; /* data file descriptor */
    int rand_data;
    int data_size;
    int temp_size;
    int dataIndex = 0;
    int err;


    struct {
        int sizeInBytes; /* must be int */
        int dataInt[MAX_INTS_ALLOW+1]; /* how many int's can we write? */
    }__attribute((packed)) dataBuf;


    fprintf(stderr, "Creating File:%s. ", filename);

    if((dfd = open(filename, O_RDWR | O_CREAT | O_SYNC, S_IRWXU)) <= 0)
    {
        printf("Error! Cannot open file: %s\n",filename);
        perror("Error");
        exit(1);
    }

    /*now write a bunch of random binary data to the file*/
    /*first figure out how much data to write. That is random also.*/

    /*file should not be less than 5 ints long. (so that we have decent length files,
      that's all)*/
    while(
	((data_size = (int)(1+(int)((FileSizeMax/sizeof(int))*rand()/(RAND_MAX+1.0)))) < 5)
    );

    /* printf("Writing %i ints to the file.\n", data_size); */

    temp_size = data_size * sizeof(int);

    /* Make sure that all data is written in one go! This is important to
       check for reliability of file systems like JFFS/JFFS that purport to
       have "reliable" writes during powre fail.
     */

    dataBuf.sizeInBytes = temp_size;

    data_size--; /*one alrady written*/
    dataIndex = 0;

    while(data_size--){
        rand_data =  (int)(1 + (int)(10000.0*rand()/(RAND_MAX+1.0)));

        dataBuf.dataInt[dataIndex++] = rand_data;

    }

    /*now calculate the file checksum and append it to the end*/
    appendChecksum((char *)&dataBuf, dataBuf.sizeInBytes);

    /* Don't forget to increase the size of data written by the 2 chars of CRC at end.
     These 2 bytes are NOT included in the sizeInBytes field. */
    if((err = write(dfd, (void *)&dataBuf, dataBuf.sizeInBytes + sizeof(short))) <
       (dataBuf.sizeInBytes + sizeof(short))
    )
    {
        printf("Error writing data buffer to file. Written %i bytes rather than %i bytes.",
               err, dataBuf.sizeInBytes);
        perror("Error");
        exit(1);
    }

    /* Now that the data is (hopefully) safely written. I can truncate the file to the new
       length so that I can reclaim any unused space, if the older file was larger.
     */
    if(ftruncate(dfd, dataBuf.sizeInBytes + sizeof(short)) < 0)
    {
        perror("Error: Unable to truncate file.");
        exit(1);
    }


    close(dfd);


}//end make_new_file()



/*
  Show's help on stdout
 */
void printHelp(char **argv)
{
    printf("Usage:%s <options, defined below>\n", argv[0]);
    printf("%s </dev/ttyS0,1,2...>: Set com port to send ok to pwr dn msg on\n",
           CMDLINE_PORT);
    printf("%s <n>: Set Max size in bytes of each file to be created.\n",
           CMDLINE_MAXFILEBYTES);
    printf("%s <n>: Set Max errors allowed when checking all files for CRC on start.\n",
           CMDLINE_MAXERROR);
    printf("%s or %s: This Help screen.\n", CMDLINE_HELPSHORT,
           CMDLINE_HELPLONG);

}/* end printHelp()*/



void processCmdLine(int argc, char **argv)
{

    int cnt;

    /* skip past name of this program, process rest */
    for(cnt = 1; cnt < argc; cnt++)
    {
        if(strcmp(argv[cnt], CMDLINE_PORT) == 0)
        {
            strncpy(SerialDevice, argv[++cnt], sizeof(SerialDevice));
            continue;
        }else
            if(strcmp(argv[cnt], CMDLINE_MAXFILEBYTES) == 0)
            {
                FileSizeMax = (float)atoi(argv[++cnt]);
                if(FileSizeMax > (MAX_INTS_ALLOW*sizeof(int)))
                {
                    printf("Max file size allowed is %zu.\n",
                           MAX_INTS_ALLOW*sizeof(int));
                    exit(0);
                }

                continue;
            }else
                if(strcmp(argv[cnt], CMDLINE_HELPSHORT) == 0)
                {
                    printHelp(argv);
                    exit(0);

                }else
                    if(strcmp(argv[cnt], CMDLINE_HELPLONG) == 0)
                    {
                        printHelp(argv);
                        exit(0);
                    }else

                        if(strcmp(argv[cnt], CMDLINE_MAXERROR) == 0)
                        {
                            MaxErrAllowed = atoi(argv[++cnt]);
                        }
                        else
                        {
                            printf("Unknown cmd line option:%s\n", argv[cnt]);
                            printHelp(argv);
                            exit(0);

                        }
    }


}/* end processCmdLine() */





int main(int argc, char **argv){

    FILE *logfp;
    int log_fd;
    char filename[30];
    short filenameCounter = 0;
    unsigned short counter;
    short errorCnt = 0;
    time_t timep;
    char * time_string;
    unsigned int seed;

    if(argc >= 1)
    {
        processCmdLine(argc, argv);
    }

    /*
      First open MAX_NUM_FILES and make sure that the checksum is ok.
      Also make an intry into the logfile.
    */
    /* timestamp! */
    time(&timep);
    time_string = (char *)ctime((time_t *)&timep);

    /*start a new check, make a log entry and continue*/
    logfp = fopen("logfile","a"); /*open for appending only.*/
    log_fd = fileno(logfp);
    fprintf(logfp,"%s", time_string);
    fprintf(logfp,"Starting new check.\n");
    if(fdatasync(log_fd) == -1){
        fprintf(stderr,"Error! Cannot sync file data with disk.\n");
        exit(1);
    }

    fclose(logfp);
    (void)sync();

    /*
      Now check all random data files in this dir.
    */
    for(counter=0;counter<MAX_NUM_FILES;counter++){

        fprintf(stderr, "%i.", counter);

        /*create the filename in sequence. The number of files
          to check and the algorithm to create the filename is
          fixed and known in advance.*/
        sprintf(filename,"file%i",filenameCounter++);

        if(!check_crc_ccitt(filename)){
            /*oops, checksum does not match. Make an entry into the log file
              and decide if we can continue or not.*/
            fprintf(stderr, "crcError:%s ", filename);
            logfp = fopen("logfile","a"); /*open for appending only.*/
            log_fd = fileno(logfp);
            fprintf(logfp,"CRC error in file: %s\n", filename);
            if(fdatasync(log_fd) == -1){
                fprintf(stderr,"Error! Cannot sync file data with disk.\n");
                exit(1);
            }
            fclose(logfp);
            (void)sync();

            errorCnt++;

            if(errorCnt > MaxErrAllowed){
                logfp = fopen("logfile","a"); /*open for appending only.*/
                log_fd = fileno(logfp);
                fprintf(logfp,"\nMax Error count exceed. Stopping!\n");
                if(fdatasync(log_fd) == -1){
                    fprintf(stderr,"Error! Cannot sync file data with disk.\n");
                    exit(1);
                }
                fclose(logfp);
                (void)sync();

                fprintf(stderr, "Too many errors. See \"logfile\".\n");
                exit(1);
            }/* if too many errors */

            /*we have decided to continue, however first repair this file
              so that we do not cumulate errors across power cycles.*/
            make_new_file(filename);
        }
    }//for

    /*all files checked, make a log entry and continue*/
    logfp = fopen("logfile","a"); /*open for appending only.*/
    log_fd = fileno(logfp);
    fprintf(logfp,"All files checked. Total errors found: %i\n\n", errorCnt);
    if(fdatasync(log_fd)){
        fprintf(stderr, "Error! cannot sync file buffer with disk.\n");
        exit(1);
    }

    fclose(logfp);
    (void)sync();

    /*now send a message to the remote power box and have it start a random
      pwer down timer after which power will be killed to this unit.
    */
    send_pwrdn_ok();

    /*now go into a forever loop of writing to files and CRC'ing them on
      a continious basis.*/

    /*start from a random file #*/
    /*seed rand based on the current time*/
    seed = (unsigned int)time(NULL);
    srand(seed);

    filenameCounter=(int)(1+(int)((float)(MAX_NUM_FILES-1)*rand()/(RAND_MAX+1.0)));

    while(1){

        for(;filenameCounter<MAX_NUM_FILES;filenameCounter++){

            /*create the filename in sequence*/
            sprintf(filename,"file%i",filenameCounter);
            make_new_file(filename);
        }
        filenameCounter = 0;
    }

    exit(0); /* though we will never reach here, but keeps the compiler happy*/
}/*end main()*/
