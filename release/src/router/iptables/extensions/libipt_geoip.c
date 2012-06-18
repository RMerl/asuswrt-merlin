/* Shared library add-on to iptables to add geoip match support.
 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2004 Cookinglinux
 
 * For comments, bugs or suggestions, please contact
 * Samuel Jean       <sjean at cookinglinux.org>
 * Nicolas Bouliane  <nib at cookinglinux.org>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <stddef.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_geoip.h>

static void help(void)
{
   printf (
            "GeoIP v%s options:\n"
            "        [!]   --src-cc, --source-country country[,country,country,...]\n"
            "                                                     Match packet coming from (one of)\n"
            "                                                     the specified country(ies)\n"
            "\n"
            "        [!]   --dst-cc, --destination-country country[,country,country,...]\n"
            "                                                     Match packet going to (one of)\n"
            "                                                     the specified country(ies)\n"
            "\n"
            "           NOTE: The country is inputed by its ISO3166 code.\n"
            "\n"
            "\n", IPTABLES_VERSION
         );
}

static struct option opts[] = {
   {  "dst-cc",  1, 0, '2'  }, /* Alias for --destination-country */
   {  "destination-country",   1, 0, '2'  },
   {  "src-cc",  1, 0, '1'  }, /* Alias for --source-country */
   {  "source-country",  1, 0, '1'  },
   {  0  }
};

static void 
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
}

/* NOT IMPLEMENTED YET
static void geoip_free(struct geoip_info *oldmem)
{
}
*/

struct geoip_index {
   u_int16_t cc;
   u_int32_t offset;
} __attribute__ ((packed));

struct geoip_subnet *
get_country_subnets(u_int16_t cc, u_int32_t *count)
{
   FILE *ixfd, *dbfd;
   struct geoip_subnet *subnets;
   struct geoip_index *index;
   struct stat buf;
  
   size_t idxsz;
   u_int16_t i;
   
   u_int16_t db_cc = 0;
   u_int16_t db_nsubnets = 0;

   if ((ixfd = fopen("/var/geoip/geoipdb.idx", "r")) == NULL) {
         perror("/var/geoip/geoipdb.idx");
         exit_error(OTHER_PROBLEM,
               "geoip match: cannot open geoip's database index file");               
   }
   
   stat("/var/geoip/geoipdb.idx", &buf);
   idxsz = buf.st_size/sizeof(struct geoip_index);
   index = (struct geoip_index *)malloc(buf.st_size);

   fread(index, buf.st_size, 1, ixfd);

   for (i = 0; i < idxsz; i++)
      if (cc == index[i].cc)
         break;
   
   if (cc != index[i].cc)
      exit_error(OTHER_PROBLEM,
            "geoip match: sorry, '%c%c' isn't in the database\n", COUNTRY(cc));

   fclose(ixfd);

   if ((dbfd = fopen("/var/geoip/geoipdb.bin", "r")) == NULL) {
      perror("/var/geoip/geoipdb.bin");
      exit_error(OTHER_PROBLEM,
            "geoip match: cannot open geoip's database file");
   }

   fseek(dbfd, index[i].offset, SEEK_SET);
   fread(&db_cc, sizeof(u_int16_t), 1, dbfd);

   if (db_cc != cc)
      exit_error(OTHER_PROBLEM,
            "geoip match: this shouldn't happened, the database might be corrupted, or there's a bug.\n"
            "you should contact maintainers");
            
   fread(&db_nsubnets, sizeof(u_int16_t), 1, dbfd);

   subnets = (struct geoip_subnet*)malloc(db_nsubnets * sizeof(struct geoip_subnet));

   if (!subnets)
      exit_error(OTHER_PROBLEM,
            "geoip match: insufficient memory available");
   
   fread(subnets, db_nsubnets * sizeof(struct geoip_subnet), 1, dbfd);
   
   fclose(dbfd);
   free(index);
   *count = db_nsubnets;
   return subnets;
}
 
static struct geoip_info *
load_geoip_cc(u_int16_t cc)
{
   static struct geoip_info *ginfo;
   ginfo = malloc(sizeof(struct geoip_info));

   if (!ginfo)
      return NULL;
   
   ginfo->subnets = get_country_subnets(cc, &ginfo->count);
   ginfo->cc = cc;
   
   return ginfo;
}

static u_int16_t
check_geoip_cc(char *cc, u_int16_t cc_used[], u_int8_t count)
{
   u_int8_t i;
   u_int16_t cc_int16;

   if (strlen(cc) != 2) /* Country must be 2 chars long according
                                        to the ISO3166 standard */
    exit_error(PARAMETER_PROBLEM,
         "geoip match: invalid country code '%s'", cc);

   // Verification will fail if chars aren't uppercased.
   // Make sure they are..
   for (i = 0; i < 2; i++)
      if (isalnum(cc[i]) != 0)
         cc[i] = toupper(cc[i]);
      else
         exit_error(PARAMETER_PROBLEM,
               "geoip match:  invalid country code '%s'", cc);

   /* Convert chars into a single 16 bit integer.
    * FIXME:   This assumes that a country code is
    *          exactly 2 chars long. If this is
    *          going to change someday, this whole
    *          match will need to be rewritten, anyway.
    *                                  - SJ  */
   cc_int16 = (cc[0]<<8) + cc[1];

   // Check for presence of value in cc_used
   for (i = 0; i < count; i++)
      if (cc_int16 == cc_used[i])
         return 0; // Present, skip it!
   
   return cc_int16;
}

/* Based on libipt_multiport.c parsing code. */ 
static u_int8_t
parse_geoip_cc(const char *ccstr, u_int16_t *cc, struct geoip_info **mem)
{
   char *buffer, *cp, *next;
   u_int8_t i, count = 0;
   u_int16_t cctmp;

   buffer = strdup(ccstr);
   if (!buffer) exit_error(OTHER_PROBLEM,
         "geoip match: insufficient memory available");

   for (cp = buffer, i = 0; cp && i < IPT_GEOIP_MAX; cp = next, i++)
   {
      next = strchr(cp, ',');
      if (next) *next++ = '\0';
      
      if ((cctmp = check_geoip_cc(cp, cc, count)) != 0) {
         if ((mem[count++] = load_geoip_cc(cctmp)) == NULL)
            exit_error(OTHER_PROBLEM,
                  "geoip match: insufficient memory available");
         cc[count-1] = cctmp;
         }
   }
   
   if (cp) exit_error(PARAMETER_PROBLEM,
         "geoip match: too many countries specified");
   free(buffer);

   if (count == 0) exit_error(PARAMETER_PROBLEM,
         "geoip match: don't know what happened");
   
   return count;
}

static int 
parse(int c, char **argv, int invert, unsigned int *flags,
                 const struct ipt_entry *entry,
                 unsigned int *nfcache,
                 struct ipt_entry_match **match)
{
   struct ipt_geoip_info *info
      = (struct ipt_geoip_info *)(*match)->data;
  
    switch(c) {
      case '1':
         // Ensure that IPT_GEOIP_SRC *OR* IPT_GEOIP_DST haven't been used yet.
         if (*flags & (IPT_GEOIP_SRC | IPT_GEOIP_DST))
            exit_error(PARAMETER_PROBLEM,
                  "geoip match: only use --source-country *OR* --destination-country once!");
 
         *flags |= IPT_GEOIP_SRC;
         *nfcache |= NFC_IP_SRC;
         break;
         
      case '2':
         // Ensure that IPT_GEOIP_SRC *OR* IPT_GEOIP_DST haven't been used yet.
         if (*flags & (IPT_GEOIP_SRC | IPT_GEOIP_DST))
            exit_error(PARAMETER_PROBLEM,
                  "geoip match: only use --source-country *OR* --destination-country once!");
 
         *flags |= IPT_GEOIP_DST;
         *nfcache |= NFC_IP_DST;
         break;
      
      default:
         return 0;
    }
    
    if (invert)
       *flags |= IPT_GEOIP_INV;
   
    info->count = parse_geoip_cc(argv[optind-1], info->cc, info->mem);
    info->flags = *flags;
    info->refcount = NULL;
    //info->fini = &geoip_free;

    return 1;
}

static void 
final_check(unsigned int flags)
{
   if (!flags)
      exit_error(PARAMETER_PROBLEM,
            "geoip match: missing arguments");
}

static void 
print(const struct ipt_ip *ip,
                  const struct ipt_entry_match *match,
                  int numeric)
{
   const struct ipt_geoip_info *info
      = (const struct ipt_geoip_info *)match->data;
   
   u_int8_t i;
   
   if (info->flags & IPT_GEOIP_SRC)
      printf("Source ");
   else printf("Destination ");
   
   if (info->count > 1)
      printf("countries: ");
   else printf("country: ");
   
   if (info->flags & IPT_GEOIP_INV)
      printf("! ");
      
   for (i = 0; i < info->count; i++)
       printf("%s%c%c", i ? "," : "", COUNTRY(info->cc[i]));
   printf(" ");
}

static void 
save(const struct ipt_ip *ip,
                 const struct ipt_entry_match *match)
{
   const struct ipt_geoip_info *info
      = (const struct ipt_geoip_info *)match->data;
   u_int8_t i;

   if (info->flags & IPT_GEOIP_INV)
      printf("! ");
 
   if (info->flags & IPT_GEOIP_SRC)
      printf("--source-country ");
   else printf("--destination-country ");
      
   for (i = 0; i < info->count; i++)
      printf("%s%c%c", i ? "," : "", COUNTRY(info->cc[i]));
   printf(" ");
}

static struct iptables_match geoip = {
    .name            = "geoip",
    .version         = IPTABLES_VERSION,
    .size            = IPT_ALIGN(sizeof(struct ipt_geoip_info)),
    .userspacesize   = offsetof(struct ipt_geoip_info, mem),
    .help            = &help,
    .init            = &init,
    .parse           = &parse,
    .final_check     = &final_check,
    .print           = &print,
    .save            = &save,
    .extra_opts      = opts
};

void _init(void)
{
   register_match(&geoip);
}
