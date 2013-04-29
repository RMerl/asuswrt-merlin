/*
 * gcsv2bin convert the comma-separated-values from the Maxmind's (www.maxmind.com)
 * database to a truncated binary file used by the iptables/netfilter's geoip module.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2004, 2008
 * 
 * Nicolas Bouliane <acidfu@people.netfilter.org>
 * Samuel Jean <peejix@people.netfilter.org>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define COUNTRYCOUNT 253
extern int  optind;

static void usage(const char *cmd)
{
   printf(
         "\nUsage: %s [OPTION] [FILE]\n"
         "Convert the comma-separated-values from the inputed Maxmind's (www.maxmind.com)\n"
         "FILE to a truncated binary file used by the iptables/netfilter's geoip module.\n"
         "The created binary file is outputed in ./geoipdb.bin (the current directory).\n"
         "and the index file in ./geoipdb.idx\n\n"

         "\t-h\tGives this help display.\n"
         "\t-v\tDisplays this program's version.\n"
         ,cmd);
}

static void copyright() {
    printf(
         "\nCopyright (C) 2004, 2008\n"
         "Nicolas Bouliane <acidfu@people.netfilter.org>\n"
         "Samuel Jean <peejix@people.netfilter.org>\n"
         "This is free software; see the source for copying conditions. There is NO\n"
         "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n"
         );
}
   
struct geo_ipv4 {
   u_int16_t cc;
   u_int32_t begin_subnet;
   u_int32_t end_subnet;
};

char *get_element(char *data, char token, int index)
{
   char *pos, *e_pos, *ddata;
   int i = 0, set = 0;

   e_pos = ddata = strdup(data);

   do {
      pos = e_pos;
      e_pos = strchr(e_pos, token);

      i++;

      if (!(i<index)) {
         set = 1;         
      }
      else if (e_pos == NULL) {
         e_pos = strchr(pos, '\0');
         set = 1;
      }
      else
         e_pos++;
      
   } while (!set);
   
   *e_pos = '\0';

   free(ddata);
   return strdup(pos);
}

int sm_or_eq(struct geo_ipv4 *geoipdb, struct geo_ipv4 *pivot)
{
   if (geoipdb->cc < pivot->cc)
      return 1;

   else
      return (geoipdb->cc == pivot->cc) &&
             (geoipdb->begin_subnet < pivot->begin_subnet);
}

void swap(struct geo_ipv4 *geoipdb, u_int32_t i, u_int32_t j)
{
   struct geo_ipv4 tmp;

   tmp = geoipdb[i];
   geoipdb[i] = geoipdb[j];
   geoipdb[j] = tmp;
  
   return;
}

void quicksort(struct geo_ipv4 geoipdb[], int begin, int end)
{
   u_int32_t l, r;
   struct geo_ipv4 pivot;
   
   if (end - begin > 1) {
      pivot = geoipdb[begin];
      l = begin + 1;
      r = end;
      while(l < r) {
         if (sm_or_eq(&geoipdb[l], &pivot)) {
            l++;
         } else {
            r--;
            swap(geoipdb, l, r);
         }
      }
      l--;
      swap(geoipdb, begin, l);
      quicksort(geoipdb, begin, l);
      quicksort(geoipdb, r, end);
   }
   return;
}

void stat_sorted_country(struct geo_ipv4 *geoipdb, u_int16_t *stat_cc, int nb_range)
{
   u_int16_t cc = 0;
   int i,j = -1;

   for (i = 0; i < nb_range; i++) {

      if (cc != geoipdb[i].cc) {
         cc = geoipdb[i].cc;
         j++;
         stat_cc[j]++;
      }
      else {
         stat_cc[j]++;
      }
   }
   
   return;
}

int input_csv(const char *file, struct geo_ipv4 *geoipdb)
{
   FILE *ifd;
   int end = 0;
   int i = 0;
   char line[1024], *tmp;  

   if ((ifd = fopen(file, "r")) == NULL) {
      perror(file);
      return -1;
   }
   
   do {
      if (fscanf(ifd, "%[^\n]\n", line) == 0)
         end = 1;
   
      else {
         //we only need the element 3, 4 and 5
         //so, we get them and we fill the struct
         tmp = get_element(line, ',', 3);
         tmp[strlen(tmp) - 1] = '\0';
         geoipdb[i].begin_subnet = atoll(++tmp);
         free(--tmp);

         tmp = get_element(line, ',', 4);
         tmp[strlen(tmp) -1] = '\0';
         geoipdb[i].end_subnet = atoll(++tmp);
         free(--tmp);
  
         tmp = get_element(line, ',', 5);
         tmp[strlen(tmp) - 1] = '\0';
         geoipdb[i].cc = (tmp[1]<<8) + tmp[2];
         free(tmp);
         
         i++;
      }

      if(feof(ifd))
         end = 1;

  } while (!end);
  
   fclose(ifd);
   return i;   
}

int output_bin(struct geo_ipv4 *geoipdb, u_int16_t stat_cc[], int nb_subnet)
{
   FILE *ofd, *oxfd;
   int i, j, k;
   u_int32_t offset;

   if ((ofd = fopen("geoipdb.bin", "w")) == NULL) {
      perror("geoipdb.bin");
      return -1;
   }

   if ((oxfd = fopen("geoipdb.idx", "w")) == NULL) {
      perror("geoipdb.idx");
      return -1;
   }
   // i : the total number of subnet in all country
   // j : the number of subnet in the current country
   // k : iterate to write all subnet in the current country
   
   for (j = 0, i = 0; i < nb_subnet; i++) {
     
      //get the current offset
      offset = ftell(ofd);
      
      //write the country and its database's offset into the index file
      fwrite(&geoipdb[i].cc, sizeof(u_int16_t), 1, oxfd);
      fwrite(&offset, sizeof(u_int32_t), 1, oxfd); 

      //write the country and the number of subnet for this country
      fwrite(&geoipdb[i].cc, sizeof(u_int16_t), 1, ofd);
      fwrite(&stat_cc[j], sizeof(u_int16_t), 1, ofd);

      // stat_cc[j] is the number of subnet by country
      // `j' is the current country
      for (k = 0; k < stat_cc[j]; k++) {
         
         //write all subnet which belong to the previously written country
         fwrite(&geoipdb[i+k].begin_subnet, sizeof(u_int32_t), 1, ofd);
         fwrite(&geoipdb[i+k].end_subnet, sizeof(u_int32_t), 1, ofd);
      }
      //jump to the next country
      //we substract 1 because the element 0 count.
      i += stat_cc[j++]-1;
   }
   
   fclose(ofd);
   fclose(oxfd);
   return 0;
}

int csv2bin(const char *file)
{
   struct geo_ipv4 *geoipdb;
   struct stat sbuf;
   u_int16_t stat_cc[COUNTRYCOUNT] = {0};
   int nb_subnet = 0;
   
   //get the size of the CSV file
   stat(file, &sbuf);

   //statisticly the size of what we need
   //is about 3x smaller.
   geoipdb = malloc(sbuf.st_size/3);
  
   //mangle the input and fill the structure
   nb_subnet = input_csv(file, geoipdb);

   //sort the database by country and by
   //subnet
   quicksort(geoipdb, 0, nb_subnet);

   //get the number of subet by country
   stat_sorted_country(geoipdb, stat_cc, nb_subnet);

   //write the binary database and index file
   output_bin(geoipdb, stat_cc, nb_subnet);  
   
  return 0;
}

int main(int argc, char *argv[])
{  
   int optch;                    
   static char stropts[] = "hv";

   while ((optch = getopt(argc,argv,stropts)) != EOF)
      switch (optch) {
         case 'h' :
            usage(argv[0]);
            copyright();
            return 0;
         case 'v' :
            puts("gcsv2bin version 0.3a");
            copyright();
            return 0;
      }
   
   if (argc - optind < 1) {
      fprintf(stderr,
            "%s: missing input file\n"
            "Use -h for help.\n"
            ,argv[0]);
      
      return -1;
   }
   
   else
      if (csv2bin(argv[optind]) != 0)
         return -1;
         
   return 0;
}
