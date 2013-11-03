
/*-------------------------------------------------------------------------*/
/**
   @file    dictionary.h
   @author  N. Devillard
   @date    Sep 2007
   @brief   Implements a dictionary for string variables.

   This module implements a simple dictionary object, i.e. a list
   of string/string associations. This object is useful to store e.g.
   informations retrieved from a configuration file (ini files).
*/
/*--------------------------------------------------------------------------*/

/*
	$Author: ndevilla $
	$Date: 2007-11-23 21:37:00 $
*/

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

/*---------------------------------------------------------------------------
   								Includes
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*---------------------------------------------------------------------------
   								New types
 ---------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
/**
  @brief	Dictionary object

  This object contains a list of string/string associations. Each
  association is identified by a unique string key. Looking up values
  in the dictionary is speeded up by the use of a (hopefully collision-free)
  hash function.
 */
/*-------------------------------------------------------------------------*/
typedef struct _dictionary_ {
	int				n ;		/** Number of entries in dictionary */
	int				size ;	/** Storage size */
	char 	    **	val ;	/** List of string values */
	char 	    **  key ;	/** List of string keys */
	unsigned	 *	hash ;	/** List of hash values for keys */
} dictionary ;


/*---------------------------------------------------------------------------
  							Function prototypes
 ---------------------------------------------------------------------------*/

unsigned   atalkdict_hash  (char * key);
dictionary *atalkdict_new  (int size);
void       atalkdict_del   (dictionary * vd);
const char *atalkdict_get  (const dictionary * d, const char *section, const char * key, const char * def);
int        atalkdict_set   (dictionary * vd, char *section, char * key, char * val);
void       atalkdict_unset (dictionary * d, char *section, char * key);
void       atalkdict_dump  (dictionary * d, FILE * out);

#endif
