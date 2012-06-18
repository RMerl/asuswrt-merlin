/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
  **
  ** cache.c - cache handling routines
  **
  ** Part of the dproxy package by Matthew Pratt.
  **
  ** Copyright 1999 Matthew Pratt <mattpratt@yahoo.com>
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/
#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/file.h>
#include <ctype.h>
#include <unistd.h>

#include "cache.h"
#include "conf.h"

/** function prototypes for private functions*/
static int cache_byname(FILE * , char *, char *);
static int deny_byname(FILE * , char *);

/*****************************************************************************
 *  search for a host by its name.
 *
 *    This function first searches the cache for an entry. If the entry
 *    was not found there, we will look into a dhcp "leases" file.
 *
 *  @arg name  - name to find.
 *  @arg ip    - pointer to a buffer where to put the ip adress.
 *
 *  @return 0 if an entry was found, 1 if not.
*****************************************************************************/
int cache_lookup_name(char *name, char ip[BUF_SIZE])
{
  FILE * fp;

  debug( "cache_lookup_name(%s/%s)\n", name, ip);

  /** check the cache */
  if( (fp = fopen( config.cache_file , "r")) != NULL) {
	 int result = 0;
	 result = cache_byname(fp,name, ip);
	 fclose(fp);
	 if( result > 0 ) {
		return 1;
	 }
  }

  return 0;
}
/*****************************************************************************
 * lookup a hostname in the cache file.
 *
 * This function will not lock the cache !
 *
 *  @arg fp    - open file pointer for the cache file.
 *  @arg name  - name to find.
 *  @arg ip    - pointer to a buffer where to put the ip adress.
 *
 *  @return 0 if an entry was found, 1 if not.
 *****************************************************************************/
static int cache_byname(FILE * fp, char *name, char ip[BUF_SIZE])
{

  char line[BUF_SIZE];
  char *token;
  int i = 0;

  ip[0] = 0;

  /** make shure we are at the start of the cache */
  rewind(fp);

  while( fgets(line, BUF_SIZE, fp) ){
	 token = strtok( line, " ");
	 if( !strcasecmp( token, name) ){
		token = strtok( NULL, " ");
		while( isalnum(*token) || (*token=='.') )ip[i++] = *token++;
		ip[i] = 0;
		return 1;
	 }
  }

  return 0;
}

/*****************************************************************************/
int cache_lookup_ip(char *ip, char result[BUF_SIZE])
{
  FILE *fp;
  char line[BUF_SIZE];
  char *token;
  int i = 0;

  if( ip[0] == 0 )return 0;
  result[0] = 0;

  fp = fopen( config.cache_file , "r");
  if(!fp)return 0;
  while( fgets(line, BUF_SIZE, fp) ){
	 strtok( line, " ");
	 token = strtok( NULL, " ");
	 if( !strncasecmp( token, ip, strlen(ip) ) ){
		while( isalnum(line[i]) || line[i]=='.' || line[i]=='-' ){
			result[i] = line[i];
			i++;
		}
		result[i] = 0;
		fclose(fp);
		return 1;
	 }
  }
  fclose(fp);

  return 0;
}
/*****************************************************************************
* save the name to the list.
*
*
*****************************************************************************/
static uint32	dns_cache_count=0;	//added by CMC 8/4/2001
void cache_name_append(char *name, char *ip)
{

  FILE *fp;
  char dummy[BUF_SIZE];

  if ( dns_cache_count >= MAX_CACHE_NUM ){ //added by CMC 8/4/2001
  	debug("cache_name_append: over max cache num (%d)\n", MAX_CACHE_NUM);
  	return;
  }

  fp = fopen( config.cache_file, "a");
  if(!fp){
	 debug("Could not open cache file '%s' for writing",
		      config.cache_file);
	 return;
  }

  /** check if another process already added this host to the cache */
  if( cache_byname(fp, name, dummy) != 0 ) {
	  fclose(fp);
	  return;
  }

  /** make shure that we at the end of the file. */
  fseek(fp,0,SEEK_END);

  /** write new entry */
  if(strlen(ip)) {//JYWeng 20031215: add not to save name without ip
	  fprintf( fp, "%s %s %ld\n", name, ip, time(NULL) );
	  dns_cache_count++;	//added by CMC 10/29/2001
  }

  fclose(fp);
}
/*****************************************************************************/
void cache_purge(int older_than)
{
  FILE *in_fp, *out_fp;
  char line[BUF_SIZE];
  char old_cache[1024];
  char *name, *ip, *time_made;
  time_t now;

  debug("enter cache_purge(): %d sec\n", older_than);

  in_fp = fopen( config.cache_file , "r");
  if(!in_fp){
	 debug_perror("Could not open old cache file");
	 /*return;*/
  }

  if( in_fp ) {

    sprintf( old_cache, "%s.old", config.cache_file );
    if( rename( config.cache_file, old_cache ) < 0 ){
	 debug_perror("Could not move cache file");
	 fclose(in_fp);
	 return;
    }
  }

  out_fp = fopen( config.cache_file , "w");
  if(!out_fp){
	 if( in_fp ) {
	 	fclose(in_fp);
	 }
	 debug_perror("Could not open new cache file");
	 return;
  }

  cache_add_hosts_entries(out_fp);
  dns_cache_count = 0;	//added by CMC 8/4/2001

  if( in_fp ) {
    now = time(NULL);
    while( fgets(line, BUF_SIZE, in_fp) ){
	 name = strtok( line, " ");
	 ip = strtok( NULL, " ");
	 time_made = strtok( NULL, " ");
	 if(!time_made)continue;
	 if( now - atoi( time_made ) < older_than ){
	 	if(strlen(ip)) {//JYWeng 20031215: add not to save name without ip
			fprintf( out_fp, "%s %s %s", name, ip, time_made );
			dns_cache_count++; //added by CMC 8/4/2001
		}
	 }
    }

    fclose(in_fp);
    unlink(old_cache);
  }

  fclose(out_fp);
}
/*****************************************************************************/
void cache_add_hosts_entries(FILE *cache_file)
{
  FILE *hosts_fp;
  char line[BUF_SIZE];
  char *ip, *name;

  debug("cache_add_hosts_entreies()\n");

  hosts_fp = fopen( config.hosts_file , "r");
  if( !hosts_fp ) {
	debug_perror("can not open 'hosts'-file ");
	return;
  }

  while( fgets(line, BUF_SIZE, hosts_fp) ){
	 line[strlen(line) - 1] = 0; /* get rid of '\n' */
	 ip = strtok( line, " \t");
	 if( ip == NULL ) continue;  /* ignore blank lines */
	 if( ip[0] == '#' )continue; /* ignore comments */
	 while( (name = strtok( NULL, " \t" )) ){
	   if(name[0] == '#')break;
		//fprintf( cache_file, "%s %s %ld\n", name, ip, 0L );
		fprintf( cache_file, "%s %s\n", name, ip ); //CMC 10/29/2001
	 }

  }
  fclose(hosts_fp);
  debug("cache_add_hosts_entreies(): done\n");
}

/*****************************************************************************
 *  search for a host by its name. (added by CMC 2002/11/19)
 *
 *    This function searches the deny file for an entry.
 *
 *  @arg name  - name to find.
 *
 *  @return 1 if an entry was found, 0 if not.
*****************************************************************************/
int deny_lookup_name(char *name)
{
  FILE * fp;

  debug( "deny_lookup_name(%s)\n", name);

  if( (fp = fopen( config.deny_file , "r")) != NULL) {
	 int	result;
	 result = deny_byname(fp, name);
	 fclose(fp);
	 if( result > 0 ) {
		return 1;
	 }
  }

  return 0;
}

/*****************************************************************************
 * lookup a hostname in the deny file. (added by CMC 2002/11/19)
 *
 * This function will not lock the cache !
 *
 *  @arg fp    - open file pointer for the cache file.
 *  @arg name  - name to find.
 *
 *  @return 1 if an entry was found, 0 if not.
 *****************************************************************************/
static int deny_byname(FILE * fp, char *name)
{
  char line[BUF_SIZE];
  char *token;

  /** make shure we are at the start of the cache */
  rewind(fp);

  while( fgets(line, BUF_SIZE, fp) ){
	 if ( (token = strtok( line, " \n")) != NULL && strcasestr(name, token) ){
		return 1;
	 }
  }

  return 0;
}


