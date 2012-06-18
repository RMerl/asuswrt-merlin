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
  ** conf.c
  **
  ** Part of the drpoxy package by Matthew Pratt. 
  **
  ** Copyright 1999 Jeroen Vreeken (pe1rxq@chello.nl)
  **
  ** This software is licensed under the terms of the GNU General 
  ** Public License (GPL). Please see the file COPYING for details.
  ** 
  **
*/

/*
  **
  ** History:
  **
  **   changes by Jeroen Vreeken (pe1rxq@chello.nl)
  **       - Created conf_load, conf_defaults and got them working.
  **       - Added cmdparse and bool functions.
  **       - Added connected
  **       - Added purge time
  **       - Added deny file
  **
*/

/**
 * How to add a config option :
 *   1. add a #define for a default value at 'dproxy.h'. Don't
 *      forget that #ifdef ... #endif stuff there.
 *      
 *   2. add a field to 'struct config' in 'config.h'
 *
 *   3. add a default value to initialisation of 
 *      'config_defaults' below.
 *      
 *   4. add a entry to the config_params array below, if your
 *      option should be configurable by the config file.
 */ 
#include <string.h>
#include "conf.h"

struct config config;  
struct config config_defaults = {
  NAME_SERVER_DEFAULT,
  1 ,
  PPP_DETECT_DEFAULT ,
  PURGE_TIME_DEFAULT ,
  CONFIG_FILE_DEFAULT, 
  RESOLV_FILE_DEFAULT,
  DENY_FILE_DEFAULT ,
  CACHE_FILE_DEFAULT, 
  HOSTS_FILE_DEFAULT, 
  PPP_DEV_DEFAULT, 
  DHCP_LEASES_DEFAULT, 
  DEBUG_FILE_DEFAULT, 
};

static void copy_bool(char *, void *);
static void print_bool(FILE * fd , void * value ) ;
static void init_int(char *, void *);
static void copy_int(char *, void *);
static void print_int(FILE * fd , void * value ) ;
static void copy_string(char *, void *);
static void print_string(FILE * fd , void * value ) ;

config_param config_params[] = {
  { 
     "name_server" ,
     "# This is the IP of the upstream nameserver that dproxy queries\n",
     &config.name_server,
     &config_defaults.name_server,
     copy_string ,
     copy_string ,
     print_string
  } ,
   { 
     "ppp_detect" ,
     "# Do you want to check for a connection?\n"
     "# Set this only if you are using a ppp or isdn connection!\n",
     &config.ppp_detect ,
     &config_defaults.ppp_detect ,
     init_int,
     copy_bool ,
     print_bool 
  } ,
   { 
     "purge_time" ,
     "# This will set the purge time, the time for a cached lookup\n"
     "# become invalid (in seconds)\n",
     &config.purge_time,
     &config_defaults.purge_time,
     init_int,
     copy_int ,
     print_int
  } ,
  { 
     "resolv_file" ,
     "# Location of the resolv.conf file\n",
     &config.resolv_file,
     &config_defaults.resolv_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  { 
     "deny_file" ,
     "# Edit this file if you want some sites blocked....\n",
     &config.deny_file,
     &config_defaults.deny_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  { 
     "cache_file" ,
     "# Location of the cache file\n",
     &config.cache_file,
     &config_defaults.cache_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  { 
     "hosts_file" ,
     "# Location of the hosts file.\n",
     &config.hosts_file,
     &config_defaults.hosts_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  { 
     "dhcp_lease_file" ,
     "# If you use dhcp, set this to tell dproxy where to find\n"
     "# the dhcp leases.\n",
     &config.dhcp_lease_file,
     &config_defaults.dhcp_lease_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  { 
     "ppp_dev" ,
     "# dproxy monitors this file to determine when the machine is\n"
     "# connected to the net\n", 
     &config.ppp_device_file,
     &config_defaults.ppp_device_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  { 
     "debug_file" ,
     "# Debug info log file\n" 
     "# If you want dproxy to log debug info specify a file here.\n",
     &config.debug_file,
     &config_defaults.debug_file,
     copy_string ,
     copy_string ,
     print_string
  } ,
  /*
   * end-of-array indicator, must be present and everything below
   * this line will be ignored.
   */
  { NULL , NULL , NULL, NULL, NULL , NULL }
};

/**************************************************************************
    Main function, called from dproxy.c
*/
int conf_load (char *conf_file)
{
  FILE *fp;
  char line[256], *cmd = NULL, *arg1 = NULL;
  
  conf_defaults();	/* load default settings first */
  
  fp = fopen (conf_file, "r");
  if (!fp) {	/* specified file does not exist... try default file */
	 fp = fopen (config.config_file, "r");
	 if (!fp) {	/* there is no config file.... use defaults */
		perror("no config file");
		return 0;
	 }
  } else {
	 strcpy(config.config_file, conf_file);
  }
  while (fgets(line, 256, fp)) {
	 if (!(line[0]=='#')) {	/* skip lines with comment */
		line[strlen(line) - 1] = 0; /* kill '\n' */
		cmd = strtok( line, " =" );
		arg1 = strtok( NULL, " =");
		conf_cmdparse(cmd, arg1);
	 }
  }
  fclose(fp);
  return 0;
}
/*****************************************************************************/
void conf_cmdparse(char *cmd, char *arg1)
{
  int i = 0;

  if( !cmd )return;
  if( !arg1 )return;
  
  while( config_params[i].param_name != NULL ) {
	if(!strncasecmp(cmd, config_params[i].param_name , 
			CONF_PATH_LEN + 50)) 
	{
	    config_params[i].copy( arg1, config_params[i].conf_value );
	    return;
	}
	i++;
  }

  fprintf( stderr, "Unkown config option: \"%s\"\n", cmd ); 
  return;

}

/************************************************************************
 * copy functions 
 *
 *   copy_bool   - convert a bool representation to int
 *   copy_int    - convert a string to int
 *   copy_string - just a string copy
 *   
 * @param str -   A char *, pointing to a string representation of the
 *                value.
 * @param value - points to the place where to store the value. 
************************************************************************/
static void copy_bool (char *str, void * val)
{
	if ( !strcmp(str, "1") || 
	     !strcasecmp(str, "yes") || 
	     !strcasecmp(str,"on")) 
	{
		*((int *)val) = 1;
	}else
	  *((int *)val) = 0;
}
static void copy_int(char *str, void * val) {
	*((int *)val) = atoi(str);
}
static void copy_string(char *str, void * val) {
	strncpy((char *)val, str , CONF_PATH_LEN );
}
static void init_int(char *str, void * val) {
	*((int *)val) = *((int *)str);
}
/************************************************************************
 * print functions  -
 *
 * Take a config value, convert it to human readable form 
 * and print it out.
 *
 *   print_bool   -  print a boolean value
 *   print_int    -  print a string
 *   print_string - print a string value
 *   
 * @param fd - File descriptor for output.
 * @param value - pointer to the config value.
************************************************************************/
static void print_bool(FILE * fd , void * value ) {
  if( *((int *)value) ) {
    fprintf(fd,"yes");
  } else {
    fprintf(fd,"no");
  }
}
static void print_int(FILE * fd , void * value ) {
    fprintf(fd,"%d", *((int *) value) );
}
static void print_string(FILE * fd, void * value) {
    fprintf(fd,"%s", ((char *)value) );
}
/************************************************************************
 * print the configuration on stdout.
************************************************************************/
void conf_print() {
   int i = 0;
   FILE * fd;

   fd = stdout;
   while( config_params[i].param_name != NULL ) {
	fprintf(fd,"# param %s \n" , config_params[i].param_name );
	fprintf(fd, config_params[i].comment);
	fprintf(fd, "#\n");
	fprintf(fd, "# Default : " );
	config_params[i].print(fd,config_params[i].def_value );
	fprintf(fd, "\n#\n");
	fprintf(fd, "%s = " , config_params[i].param_name );
	config_params[i].print(fd,config_params[i].conf_value );
	fprintf(fd, "\n\n");

	i++;
   }
}
/************************************************************************
    Load default settings first
*/
void conf_defaults (void)
{
  int i = 0;
  while( config_params[i].param_name != NULL ) {
	config_params[i].init( config_params[i].def_value,
			       config_params[i].conf_value );
	i++;
  }

  config.daemon_mode = 1;
  config.dhcp_lease_file[0] = 0;
  config.debug_file[0] = 0;
  return;

}

