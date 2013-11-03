
/*-------------------------------------------------------------------------*/
/**
   @file    iniparser.h
   @author  N. Devillard
   @date    Sep 2007
   @version 3.0
   @brief   Parser for ini files.
*/
/*--------------------------------------------------------------------------*/

/*
*/

#ifndef _INIPARSER_H_
#define _INIPARSER_H_

/*---------------------------------------------------------------------------
   								Includes
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The following #include is necessary on many Unixes but not Linux.
 * It is not needed for Windows platforms.
 * Uncomment it if needed.
 */
/* #include <unistd.h> */

#include "dictionary.h"

int        atalk_iniparser_getnsec(const dictionary * d);
const char *atalk_iniparser_getsecname(const dictionary * d, int n);
void       atalk_iniparser_dump_ini(const dictionary * d, FILE * f);
void       atalk_iniparser_dump(const dictionary * d, FILE * f);
const char *atalk_iniparser_getstring(const dictionary * d, const char *section, const char * key, const char * def);
char       *atalk_iniparser_getstrdup(const dictionary * d, const char *section, const char * key, const char * def);
int        atalk_iniparser_getint(const dictionary * d, const char *section, const char * key, int notfound);
double     atalk_iniparser_getdouble(const dictionary * d, const char *section, const char * key, double notfound);
int        atalk_iniparser_getboolean(const dictionary * d, const char *section, const char * key, int notfound);
int        atalk_iniparser_set(dictionary * ini, char *section, char * key, char * val);
void       atalk_iniparser_unset(dictionary * ini, char *section, char * key);
int        atalk_iniparser_find_entry(const dictionary * ini, const char * entry) ;
dictionary *atalk_iniparser_load(const char * ininame);
void       atalk_iniparser_freedict(dictionary * d);

#endif
