/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Groupname handling
   Copyright (C) Jeremy Allison 1998.
   
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

#ifdef USING_GROUPNAME_MAP

#include "includes.h"
extern int DEBUGLEVEL;
extern DOM_SID global_sam_sid;


/**************************************************************************
 Groupname map functionality. The code loads a groupname map file and
 (currently) loads it into a linked list. This is slow and memory
 hungry, but can be changed into a more efficient storage format
 if the demands on it become excessive.
***************************************************************************/

typedef struct groupname_map {
   ubi_slNode next;

   char *windows_name;
   DOM_SID windows_sid;
   char *unix_name;
   gid_t unix_gid;
} groupname_map_entry;

static ubi_slList groupname_map_list;

/**************************************************************************
 Delete all the entries in the groupname map list.
***************************************************************************/

static void delete_groupname_map_list(void)
{
  groupname_map_entry *gmep;

  while((gmep = (groupname_map_entry *)ubi_slRemHead( &groupname_map_list )) != NULL) {
    if(gmep->windows_name)
      free(gmep->windows_name);
    if(gmep->unix_name)
      free(gmep->unix_name);
    free((char *)gmep);
  }
}

/**************************************************************************
 Load a groupname map file. Sets last accessed timestamp.
***************************************************************************/

void load_groupname_map(void)
{
  static time_t groupmap_file_last_modified = (time_t)0;
  static BOOL initialized = False;
  char *groupname_map_file = lp_groupname_map();
  SMB_STRUCT_STAT st;
  FILE *fp;
  char *s;
  pstring buf;
  groupname_map_entry *new_ep;

  if(!initialized) {
    ubi_slInitList( &groupname_map_list );
    initialized = True;
  }

  if (!*groupname_map_file)
    return;

  if(sys_stat(groupname_map_file, &st) != 0) {
    DEBUG(0, ("load_groupname_map: Unable to stat file %s. Error was %s\n",
               groupname_map_file, strerror(errno) ));
    return;
  }

  /*
   * Check if file has changed.
   */
  if( st.st_mtime <= groupmap_file_last_modified)
    return;

  groupmap_file_last_modified = st.st_mtime;

  /*
   * Load the file.
   */

  fp = sys_fopen(groupname_map_file,"r");
  if (!fp) {
    DEBUG(0,("load_groupname_map: can't open groupname map %s. Error was %s\n",
          groupname_map_file, strerror(errno)));
    return;
  }

  /*
   * Throw away any previous list.
   */
  delete_groupname_map_list();

  DEBUG(4,("load_groupname_map: Scanning groupname map %s\n",groupname_map_file));

  while((s=fgets_slash(buf,sizeof(buf),fp))!=NULL) {
    pstring unixname;
    pstring windows_name;
    struct group *gptr;
    DOM_SID tmp_sid;

    DEBUG(10,("load_groupname_map: Read line |%s|\n", s));

    if (!*s || strchr("#;",*s))
      continue;

    if(!next_token(&s,unixname, "\t\n\r=", sizeof(unixname)))
      continue;

    if(!next_token(&s,windows_name, "\t\n\r=", sizeof(windows_name)))
      continue;

    trim_string(unixname, " ", " ");
    trim_string(windows_name, " ", " ");

    if (!*windows_name)
      continue;

    if(!*unixname)
      continue;

    DEBUG(5,("load_groupname_map: unixname = %s, windowsname = %s.\n",
             unixname, windows_name));

    /*
     * Attempt to get the unix gid_t for this name.
     */

    if((gptr = (struct group *)getgrnam(unixname)) == NULL) {
      DEBUG(0,("load_groupname_map: getgrnam for group %s failed.\
Error was %s.\n", unixname, strerror(errno) ));
      continue;
    }

    /*
     * Now map to an NT SID.
     */

    if(!lookup_wellknown_sid_from_name(windows_name, &tmp_sid)) {
      /*
       * It's not a well known name, convert the UNIX gid_t
       * to a rid within this domain SID.
       */
      tmp_sid = global_sam_sid;
      tmp_sid.sub_auths[tmp_sid.num_auths++] = 
                    pdb_gid_to_group_rid((gid_t)gptr->gr_gid);
    }

    /*
     * Create the list entry and add it onto the list.
     */

    if((new_ep = (groupname_map_entry *)malloc( sizeof(groupname_map_entry) ))== NULL) {
      DEBUG(0,("load_groupname_map: malloc fail for groupname_map_entry.\n"));
      fclose(fp);
      return;
    } 

    new_ep->unix_gid = gptr->gr_gid;
    new_ep->windows_sid = tmp_sid;
    new_ep->windows_name = strdup( windows_name );
    new_ep->unix_name = strdup( unixname );

    if(new_ep->windows_name == NULL || new_ep->unix_name == NULL) {
      DEBUG(0,("load_groupname_map: malloc fail for names in groupname_map_entry.\n"));
      fclose(fp);
      if(new_ep->windows_name != NULL)
        free(new_ep->windows_name);
      if(new_ep->unix_name != NULL)
        free(new_ep->unix_name);
      free((char *)new_ep);
      return;
    }
    memset((char *)&new_ep->next, '\0', sizeof(new_ep->next) );

    ubi_slAddHead( &groupname_map_list, (ubi_slNode *)new_ep);
  }

  DEBUG(10,("load_groupname_map: Added %ld entries to groupname map.\n",
	    ubi_slCount(&groupname_map_list)));
           
  fclose(fp);
}

/***********************************************************
 Lookup a SID entry by gid_t.
************************************************************/

void map_gid_to_sid( gid_t gid, DOM_SID *psid)
{
  groupname_map_entry *gmep;

  /*
   * Initialize and load if not already loaded.
   */
  load_groupname_map();

  for( gmep = (groupname_map_entry *)ubi_slFirst( &groupname_map_list);
       gmep; gmep = (groupname_map_entry *)ubi_slNext( gmep )) {

    if( gmep->unix_gid == gid) {
      *psid = gmep->windows_sid;
      DEBUG(7,("map_gid_to_sid: Mapping unix group %s to windows group %s.\n",
               gmep->unix_name, gmep->windows_name ));
      return;
    }
  }

  /*
   * If there's no map, convert the UNIX gid_t
   * to a rid within this domain SID.
   */
  *psid = global_sam_sid;
  psid->sub_auths[psid->num_auths++] = pdb_gid_to_group_rid(gid);

  return;
}
#else /* USING_GROUPNAME_MAP */
 void load_groupname_map(void) {;}
#endif /* USING_GROUPNAME_MAP */
