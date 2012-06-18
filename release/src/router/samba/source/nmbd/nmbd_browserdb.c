/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   Copyright (C) Christopher R. Hertel 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
*/
/* -------------------------------------------------------------------------- **
 * Modified July 1998 by CRH.
 *  I converted this module to use the canned doubly-linked lists.  I also
 *  added comments above the functions where possible.
 */

#include "includes.h"

extern int DEBUGLEVEL;

/* -------------------------------------------------------------------------- **
 * Variables...
 *
 *  lmb_browserlist - This is our local master browser list. 
 */

ubi_dlNewList( lmb_browserlist );


/* -------------------------------------------------------------------------- **
 * Functions...
 */

/* ************************************************************************** **
 * Remove and free a browser list entry.
 *
 *  Input:  browc - A pointer to the entry to be removed from the list and
 *                  freed.
 *  Output: none.
 *
 * ************************************************************************** **
 */
static void remove_lmb_browser_entry( struct browse_cache_record *browc )
  {
  free( (char *)ubi_dlRemThis( lmb_browserlist, browc ) );
  } /* remove_lmb_browser_entry */

/* ************************************************************************** **
 * Update a browser death time.
 *
 *  Input:  browc - Pointer to the entry to be updated.
 *  Output: none.
 *
 * ************************************************************************** **
 */
void update_browser_death_time( struct browse_cache_record *browc )
  {
  /* Allow the new lmb to miss an announce period before we remove it. */
  browc->death_time = time(NULL) + ( (CHECK_TIME_MST_ANNOUNCE + 2) * 60 );
  } /* update_browser_death_time */

/* ************************************************************************** **
 * Create a browser entry and add it to the local master browser list.
 *
 *  Input:  work_name
 *          browser_name
 *          ip
 *
 *  Output: Pointer to the new entry, or NULL if malloc() failed.
 *
 * ************************************************************************** **
 */
struct browse_cache_record *create_browser_in_lmb_cache( char *work_name, 
                                                         char *browser_name, 
                                                         struct in_addr ip )
  {
  struct browse_cache_record *browc;
  time_t now = time( NULL );

  browc = (struct browse_cache_record *)malloc( sizeof( *browc ) );

  if( NULL == browc )
    {
    DEBUG( 0, ("create_browser_in_lmb_cache: malloc fail !\n") );
    return( NULL );
    }

  memset( (char *)browc, '\0', sizeof( *browc ) );
  
  /* For a new lmb entry we want to sync with it after one minute. This
     will allow it time to send out a local announce and build its
     browse list.
   */
  browc->sync_time = now + 60;

  /* Allow the new lmb to miss an announce period before we remove it. */
  browc->death_time = now + ( (CHECK_TIME_MST_ANNOUNCE + 2) * 60 );

  StrnCpy(  browc->lmb_name, browser_name, sizeof(browc->lmb_name)-1 );
  StrnCpy(  browc->work_group, work_name, sizeof(browc->work_group)-1 );
  strupper( browc->lmb_name );
  strupper( browc->work_group );
  
  browc->ip = ip;
 
  (void)ubi_dlAddTail( lmb_browserlist, browc );

  if( DEBUGLVL( 3 ) )
    {
    Debug1( "nmbd_browserdb:create_browser_in_lmb_cache()\n" );
    Debug1( "  Added lmb cache entry for workgroup %s ", browc->work_group );
    Debug1( "name %s IP %s ", browc->lmb_name, inet_ntoa(ip) );
    Debug1( "ttl %d\n", (int)browc->death_time );
    }
  
  return( browc );
  } /* create_browser_in_lmb_cache */

/* ************************************************************************** **
 * Find a browser entry in the local master browser list.
 *
 *  Input:  browser_name  - The name for which to search.
 *
 *  Output: A pointer to the matching entry, or NULL if no match was found.
 *
 * ************************************************************************** **
 */
struct browse_cache_record *find_browser_in_lmb_cache( char *browser_name )
  {
  struct browse_cache_record *browc;

  for( browc = (struct browse_cache_record *)ubi_dlFirst( lmb_browserlist );
       browc;
       browc = (struct browse_cache_record *)ubi_dlNext( browc ) )
    if( strequal( browser_name, browc->lmb_name ) )
      break;

  return( browc );
  } /* find_browser_in_lmb_cache */

/* ************************************************************************** **
 *  Expire timed out browsers in the browserlist.
 *
 *  Input:  t - Expiration time.  Entries with death times less than this
 *              value will be removed from the list.
 *  Output: none.
 *
 * ************************************************************************** **
 */
void expire_lmb_browsers( time_t t )
  {
  struct browse_cache_record *browc;
  struct browse_cache_record *nextbrowc;

  for( browc = (struct browse_cache_record *)ubi_dlFirst( lmb_browserlist );
       browc;
       browc = nextbrowc )
    {
    nextbrowc = (struct browse_cache_record *)ubi_dlNext( browc );

    if( browc->death_time < t )
      {
      if( DEBUGLVL( 3 ) )
        {
        Debug1( "nmbd_browserdb:expire_lmb_browsers()\n" );
        Debug1( "  Removing timed out lmb entry %s\n", browc->lmb_name );
        }
      remove_lmb_browser_entry( browc );
      }
    }
  } /* expire_lmb_browsers */
