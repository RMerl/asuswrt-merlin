/*

 * Copyright Daniel Industries.

 * Created by: Vipin Malik (vipin.malik@daniel.com)
 *
 * This is GPL code. See the file COPYING for more details
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.

 * $Id: makefiles.c,v 1.2 2005/11/07 11:15:17 gleixner Exp $

This program creates MAX_NUM_FILES files (file00001, file00002 etc) and
fills them with random numbers till they are a random length. Then it checksums
the files (with the checksum as the last two bytes) and closes the file.

The fist int is the size of the file in bytes.

It then opens another file and the process continues.

The files are opened in the current dir.

*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#define FILESIZE_MAX    20000.0 /* for each file in sizeof(int). Must be a float #
                                   Hence, 20000.0 = 20000*4 = 80KB max file size
                                 */

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

//This code was taken from the AX.25 HDLC packet driver
//in LINUX. Once I test and have a better understanding of what
//it is doing, it will be better commented.

//For now one can speculate that the CRC routine always expects the
//CRC to calculate out to 0xf0b8 (the hardcoded value at the end)
//and returns TRUE if it is and FALSE if it doesn't.
//Why don't people document better!!!!
void check_crc_ccitt(char *filename)
{
  FILE *fp;
  unsigned short crc = 0xffff;
  int len;
  char dataByte;
  int retry;

  fp =   fopen(filename,"rb");
  if(!fp){
    printf("Verify checksum:Error! Cannot open filename passed for verify checksum: %s\n",filename);
    exit(1);
  }
  /*the first int contains an int that is the length of the file in long.*/
  if(fread(&len, sizeof(int), 1, fp) != 1){
    printf("verify checksum:Error reading from file: %s", filename);
    fclose(fp);
    exit(1);
  }
  rewind(fp);
  len+=2; /*the file has two extra bytes at the end, it's checksum. Those
	   two MUST also be included in the checksum calculation.
	  */

  for (;len>0;len--){
    retry=5; /*retry 5 times*/
    while(!fread(&dataByte, sizeof(char), 1, fp) && retry--);
    if(!retry){
      printf("Unexpected error reading from file: %s\n", filename);
      printf("...bytes left to be read %i.\n\n",len);
      fclose(fp);
      exit(1);
    }
    crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ dataByte) & 0xff];
  }
  fclose(fp);
  if( (crc & 0xffff) != 0xf0b8){
    printf("Verify checksum: Error in file %s.\n\n",filename);
    exit(1);
  }
}//end check_crc_ccitt()



/*this routine opens a file 'filename' and checksumn's the entire
 contents, and then appends the checksum at the end of the file,
 closes the file and returns.
*/
void checksum(char *filename){

  FILE *fp;
  unsigned short crc = 0xffff;
  int len;
  char dataByte;
  int retry;

  fp =   fopen(filename,"rb");
  if(!fp){
    printf("Error! Cannot open filename passed for checksum: %s\n",filename);
    exit(1);
  }
  /*the first int contains an int that is the length of the file in longs.*/
  if(fread(&len, sizeof(int), 1, fp) != 1){
    printf("Error reading from file: %s", filename);
    fclose(fp);
    exit(1);
  }
  printf("Calculating checksum on %i bytes.\n",len);
  rewind(fp); /*the # of bytes int is also included in the checksum.*/

  for (;len>0;len--){
    retry=5; /*retry 5 times*/
    while(!fread(&dataByte, sizeof(char), 1, fp) && retry--);
    if(!retry){
      printf("Unexpected error reading from file: %s\n", filename);
      printf("...bytes left to be read %i.\n\n",len);
      fclose(fp);
      exit(1);
    }
    crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ dataByte) & 0xff];
  }
  crc ^= 0xffff;
  printf("The CRC: %x\n\n", crc);

  /*the CRC has been calculated. now close the file and open it in append mode*/
  fclose(fp);

  fp =   fopen(filename,"ab"); /*open in append mode. CRC goes at the end.*/
  if(!fp){
    printf("Error! Cannot open filename to update checksum: %s\n",filename);
    exit(1);
  }
  if(fwrite(&crc, sizeof(crc), 1, fp) != 1){
    printf("error! unable to update the file for checksum.\n");
    fclose(fp);
    exit(1);
  }
  fflush(fp);
  fclose(fp);


}/*end checksum()*/



int main(void){

  FILE *fp, *cyclefp;
  int cycleCount;
  int rand_data;
  int data_size;
  int temp_size;
  char filename[30];
  short filenameCounter = 0;
  unsigned short counter;
  unsigned short numberFiles;

  numberFiles = MAX_NUM_FILES;

  for(counter=0;counter<numberFiles;counter++){
    /*create the filename in sequence*/
    sprintf(filename,"file%i",filenameCounter++);
    fp =   fopen(filename,"wb");
    if(!fp){
      printf("Error! Cannot open file: %s\n",filename);
      exit(1);
    }
    /*now write a bunch of random binary data to the file*/
    /*first figure out how much data to write. That is random also.*/

    while(
	  ((data_size = (int)(1 + (int)(FILESIZE_MAX*rand()/(RAND_MAX+1.0)))) < 100)
	  )/*file should not be less than 100 ints long. (so that we have decent length files, that's all)*/

    printf("Writing %i ints to the file.\n", data_size);

    temp_size = data_size * sizeof(int);

    if(!fwrite(&temp_size, sizeof(int), 1, fp)){
      printf("File write error!!.\n");
      fclose(fp);
      exit(1);
    }
    data_size--; /*one alrady written*/

    while(data_size--){
      rand_data =  (int)(1 + (int)(10000.0*rand()/(RAND_MAX+1.0)));
      if(!fwrite(&rand_data, sizeof(int), 1, fp)){
	printf("File write error!!.\n");
	fclose(fp);
	exit(1);
      }
    }
    fflush(fp);
    fclose(fp);
    /*now calculate the file checksum and append it to the end*/
    checksum(filename);
    /*this is just a test. Check the CRC to amek sure that it is OK.*/
    check_crc_ccitt(filename);
  }

  /*now make a file called "cycleCnt" and put a binary (int)0 in it.
   This file keeps count as to how many cycles have taken place!*/
  cyclefp =   fopen("cycleCnt","wb");
  if(!cyclefp){
    printf("cannot open file \"cycleCnt\". Cannot continue.\n");
    exit(1);
  }
  cycleCount = 0;
  if(fwrite(&cycleCount, sizeof(cycleCount), 1,cyclefp) !=1){
    printf("Error writing to file cycleCnt. Cannot continue.\n");
    exit(1);
  }
  fclose(cyclefp);
  exit(0);

}/*end main()*/







