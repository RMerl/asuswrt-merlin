/* Create & delete files at random, with random sizes.
 This is for testing the jffs filesystem code.

 Before you run filegen, you almost certainly want to set up jffs
 so that it runs out of RAM instead of flash.
 Edit linux/fs/jffs/jffs_fm.h, change: "#define JFFS_RAM_BLOCKS 0"
 to be non-zero.
*/



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*****************************************************/
void usage()
{
   puts("Usage: filegen [options]");
   puts("\t-v  = verbose");
   puts("\t-n# = Number to do.  Default 1000");
   puts("\t-s# = number of initial ones to skip.");
   puts("\t-f# = floor - min size.");
   puts("\t-c# = ceiling - max size.");
   puts("\t-x# = Write 4KB at a time until the disk is full.");
}


extern char *optarg;
extern int optind, opterr, optopt;


int num_eb = 1;	/* # of EB's to use for files. */


#define MAX_FIL 107		/* Max # of files. */
#define MAX_SIZE (75 * 1024)	/* Max file size. */

unsigned floor_siz = 0;
unsigned ceiling = MAX_SIZE;
unsigned mean, spread;

int slack = 8*1024;

int eb_size = 64 * 1024;	/* Erase block size (bytes). */
int num_to_do = 10;	/* The number of actions to do. */
int skip_to = 0;

int num_cr = 0;
int num_ov = 0;
int num_del = 0;
int act_num = 0;	/* Activity item number. */

#define FPATH "/jffs/fil_"

int size_on_disk = 0;   /* # of bytes currently on disk, not incl overhead. */
int max_on_disk = 0;	/* Max ever on disk (incl ohd) */
int num_on_disk = 0;	/* # of files  currently on disk. */

int min_siz = 999999;
int max_siz = 0;

int verb = 0;		/* Verbose */

/* File overhead--approximate.
 * 64 byte nodeheader per 4k page.
 */
#define FI_OHD(sz) (((sz+4095)/4096) * 72)

struct file_info {
   int fi_exist;	/* >0 = file exists, 0 = not. <0 = exist but skipped. */
   int fi_siz;		/* Size of the file. */
};

struct file_info files[MAX_FIL] = {{0}};
char *wbf;

void write_file();
void pr_info();
void do_err(char *what, char *fn);
int size_random();
void delete_a_file();
void del_to_fit(int s);
int pick_fil();
void goup();


/** MWC generator. */
long int random()
{
   static unsigned long z=362436069, w=521288629;
   long int iuni;

   z = 36969 * (z & 65535) + (z >> 16);
   w = 18000 * (w & 65535) + (w >> 16);
   iuni = ((z << 16) + w) & RAND_MAX;
   return(iuni);
}

#define ranf() ((double)random() / RAND_MAX)


/*******************************************************/
int main(int argc, char **argv)
{
   int c;
   int j;
   
   while((c = getopt(argc, argv, "vn:s:c:f:x")) != -1) {
      switch (c) {
      case 'v':
	 ++verb;
	 break;

      case 'n':
	 num_to_do = atoi(optarg);
	 break;

       case 's':
	 skip_to = atoi(optarg);
	 break;

       case 'f':
	 floor_siz = atoi(optarg);
	 break;

       case 'c':
	 ceiling = atoi(optarg);
	 break;

       case 'x':
	  goup();
	  exit(1);
	 break;

     case '?':
	 usage();
	 exit(1);
	 
      }
   }

   if (ceiling > num_eb * 64 * 1024 - slack)
      ceiling = num_eb * 64 * 1024 - slack;

   if (ceiling < floor_siz) {
      puts("Error: ceiling is below floor.");
      exit(1);
   }

   if (ceiling > 100000) {
      puts("Error: ceiling too large.");
      exit(1);
   }

   mean = (ceiling - floor_siz) / 3 + floor_siz;
   spread = (4 * (ceiling - floor_siz)) / 10 + floor_siz;
   
   //while (num_to_do--) { printf("%5u\n", size_random());} ; exit(0);
   wbf = malloc(ceiling);
   if (wbf == NULL)
      do_err("alloc", "");

   while (act_num < num_to_do) {
      j = random() % 100;
      if (j > 75)
	 delete_a_file();
      else
	 write_file();
      if (act_num % 100 == 0)
	 pr_info();
   }

   printf("Done.\n");
   pr_info();
   return (0);
}



/*******************************************************/
/* Select a file to do. */
/* Return its index in 'files'. */ 
int pick_fil()
{
   int i;

   i = random() % MAX_FIL;
   return(i);
}


/*******************************************************/
/* Write a file.
 * Pick random file & size. */
void write_file()
{
   int f, s;
   char fn[80];
   FILE *fp;
   int i;

   //printf("wrt\n");
   f = pick_fil();
   s = size_random();
   if (s > max_siz) max_siz = s;
   if (s < min_siz) min_siz = s;
   if (s + FI_OHD(s) > num_eb * eb_size - slack)
      return;	/* This file cannot fit. */
   del_to_fit(s);

   if (act_num >= num_to_do)
      return;
   ++act_num;
   sprintf(fn, "%s%02d", FPATH, f);
   if (verb)
      printf("Create %s of %5d #%d%s\n", fn, s, act_num,
	     act_num >= skip_to?"": " [skip]");
   if (act_num >= skip_to) {
      fp = fopen(fn, "w");
      if (!fp)
	 do_err("open", fn);
      i = fwrite(wbf, 1, s, fp);
      if (i < s) {
	 printf("Req size: %d  Wrote: %d\n", s, i);
	 do_err("write", fn);
      }
      fclose(fp);
   }
   if (files[f].fi_exist) {	/* Over-write existing file. */
      size_on_disk -= files[f].fi_siz;
      --num_on_disk;
      ++num_ov;
   }
   else
      ++num_cr;
   
   if (act_num >= skip_to)
      files[f].fi_exist = 1;
   else
      files[f].fi_exist = -1;
   files[f].fi_siz = s;
   size_on_disk += files[f].fi_siz;
   i = size_on_disk + FI_OHD(size_on_disk);
   if (max_on_disk < i)
      max_on_disk = i;  
   ++num_on_disk;
   if (verb > 1)
      printf("\t\t\tEst used: %d\n", size_on_disk + FI_OHD(size_on_disk));
}

/*******************************************************/
/* Delete enough file(s) so that this new one will fit. */
/* Allow for an extra PAGE on the write. */
void del_to_fit(int s)
{
   int i = 100;

	  
   while (i-- && s + FI_OHD(s) + size_on_disk + FI_OHD(size_on_disk)
	  > num_eb * eb_size - slack) {
/*       printf("must delete to fit\n"); */
/*       printf("This: %6d     exist:  %6d    tot: %6d     vs.  %6d   diff: %6d\n", */
/* 	  s + FI_OHD(s),  size_on_disk + FI_OHD(size_on_disk), */
/* 	  s + FI_OHD(s) +  size_on_disk + FI_OHD(size_on_disk), */
/* 	  num_eb * eb_size - slack, */
/* 	  num_eb * eb_size - slack - (s + FI_OHD(s) +  size_on_disk + FI_OHD(size_on_disk))); */
     delete_a_file();
   }
/*    printf("atxt: %6d     exist:  %6d    tot: %6d     vs.  %6d   diff: %6d\n", */
/* 	  s + FI_OHD(s),  size_on_disk + FI_OHD(size_on_disk), */
/* 	  s + FI_OHD(s) +  size_on_disk + FI_OHD(size_on_disk), */
/* 	  num_eb * eb_size - slack, */
/* 	  num_eb * eb_size - slack - (s + FI_OHD(s) +  size_on_disk + FI_OHD(size_on_disk))); */
}

/*******************************************************/
/* Pick a random file to delete. */
void delete_a_file()
{
   int f;
   char fn[80];
   int i = 0;
   
   //printf("daf\n");
   if (num_on_disk) {
      f = random() % MAX_FIL;
      for( ; !files[f].fi_exist ; ++f) {
	 if (f >= MAX_FIL-1)
	    f = -1;
      }
      ++act_num;
      sprintf(fn, "%s%02d", FPATH, f);
      if (verb)
	 printf("Delete %s #%d%s\n", fn, act_num,
		act_num >= skip_to ?"": " [skip]");
      if (act_num >= skip_to) {
	 if (files[f].fi_exist > 0)	
	    i = unlink(fn);
	 else
	    i = 0;
      }
      if (i != 0)
	 do_err("delete", fn);
      ++num_del;
      files[f].fi_exist = 0;
      size_on_disk -= files[f].fi_siz;
      files[f].fi_siz = 0;
      --num_on_disk;
   }
   if (verb > 1)
      printf("\t\t\tEst used: %d\n", size_on_disk + FI_OHD(size_on_disk));
}


/*******************************************************/
/* Return a gaussian distributed random number,
   between 0 and 74000. */
float box_muller(float m, float s)	/* normal random variate generator */
{				        /* mean m, standard deviation s */
   float x1, x2, w, y1;
   static float y2;
   static int use_last = 0;

   if (use_last)		        /* use value from previous call */
   {
      y1 = y2;
      use_last = 0;
   }
   else
   {
      do {
	 x1 = 2.0 * ranf() - 1.0;
	 x2 = 2.0 * ranf() - 1.0;
	 w = x1 * x1 + x2 * x2;
      } while ( w >= 1.0 );

      w = sqrt( (-2.0 * log( w ) ) / w );
      y1 = x1 * w;
      y2 = x2 * w;
      use_last = 1;
   }
   return( m + y1 * s );
}


/*******************************************************/
int size_random()
{
   int z;
   int try = 25;

   if (ceiling- floor_siz > 5000) {
      /* Small range -- use uniform distribution, */
      z = random() >> 3;
      z %= (ceiling- floor_siz);
      z += floor_siz;
      return z;
   }
   
   /* Larger range, use gausian (mormal) distribution. */
   do {
      z = box_muller(mean, spread);
   } while (try-- && (z < floor_siz || z >= ceiling));

      if (z < floor_siz) z = floor_siz;
      if (z > ceiling) z = ((z- floor_siz) % (ceiling- floor_siz)) + floor_siz;
   return z;
}

/*******************************************************/
void pr_info()
{
   printf("actions: %6d created: %6d   over-written: %6d  deleted: %6d\n",
	  act_num, num_cr, num_ov, num_del);
   printf("Min filesize: %5d  Max filesize: %5d  Max on disk: %5d\n",
	  min_siz, max_siz, max_on_disk);
   printf("%d files on disk totaling %d bytes\n",
	  num_on_disk, size_on_disk);  
   printf("\t\t\tEst used: %d\n", size_on_disk + FI_OHD(size_on_disk));
 
}
/*******************************************************/
/* Write 4kb at a time until we get an error. */
void goup()
{
   int fd;
   int i;
   int tsiz;

   wbf = malloc(2*4096);
   fd = open("/jffs/try", O_CREAT|O_WRONLY|O_TRUNC | O_SYNC, 0666);
   if (fd <= 0) {
      perror("open");
      exit(1);
   }
   for(tsiz = 0; tsiz < 512 ; ++tsiz) {
      i = write(fd, wbf, 4 * 1024);
      if (i < 1) {
	 perror("Died");
	 printf("Error.  Wrote %d*4kb  %dKB okay.  %5x\n", tsiz,
		tsiz * 4, tsiz*4096);
	 exit(1);
      }
   }
   printf("Wrote %d*4kb okay.  %5x\n", tsiz, tsiz*4096 );
   close(fd);
}

/*******************************************************/
void do_err(char *what, char *fn)
{
   int i = errno;
   
   printf("Fatal error on %s  filename '%s'.  Action # %d\n",
	  what, fn, act_num);
   errno = i;
   perror("");
   pr_info();
   exit(1);
}
