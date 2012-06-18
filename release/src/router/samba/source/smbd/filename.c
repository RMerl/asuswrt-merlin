/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   filename handling routines
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1999-200
   Copyright (C) Ying Chen 2000
   
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

/*
 * New hash table stat cache code added by Ying Chen.
 */

#include "includes.h"

extern int DEBUGLEVEL;
extern BOOL case_sensitive;
extern BOOL case_preserve;
extern BOOL short_case_preserve;
extern fstring remote_machine;
extern BOOL use_mangled_map;

static BOOL scan_directory(char *path, char *name,connection_struct *conn,BOOL docache);

/****************************************************************************
 Check if two filenames are equal.
 This needs to be careful about whether we are case sensitive.
****************************************************************************/
static BOOL fname_equal(char *name1, char *name2)
{
  int l1 = strlen(name1);
  int l2 = strlen(name2);

  /* handle filenames ending in a single dot */
  if (l1-l2 == 1 && name1[l1-1] == '.' && lp_strip_dot())
    {
      BOOL ret;
      name1[l1-1] = 0;
      ret = fname_equal(name1,name2);
      name1[l1-1] = '.';
      return(ret);
    }

  if (l2-l1 == 1 && name2[l2-1] == '.' && lp_strip_dot())
    {
      BOOL ret;
      name2[l2-1] = 0;
      ret = fname_equal(name1,name2);
      name2[l2-1] = '.';
      return(ret);
    }

  /* now normal filename handling */
  if (case_sensitive)
    return(strcmp(name1,name2) == 0);

  return(strequal(name1,name2));
}


/****************************************************************************
 Mangle the 2nd name and check if it is then equal to the first name.
****************************************************************************/
static BOOL mangled_equal(char *name1, char *name2)
{
  pstring tmpname;

  if (is_8_3(name2, True))
    return(False);

  pstrcpy(tmpname,name2);
  mangle_name_83(tmpname);

  return(strequal(name1,tmpname));
}

/****************************************************************************
 Stat cache code used in unix_convert.
*****************************************************************************/

static int global_stat_cache_lookups;
static int global_stat_cache_misses;
static int global_stat_cache_hits;

/****************************************************************************
 Stat cache statistics code.
*****************************************************************************/

void print_stat_cache_statistics(void)
{
  double eff;

  if(global_stat_cache_lookups == 0)
    return;

  eff = (100.0* (double)global_stat_cache_hits)/(double)global_stat_cache_lookups;

  DEBUG(0,("stat cache stats: lookups = %d, hits = %d, misses = %d, \
stat cache was %f%% effective.\n", global_stat_cache_lookups,
       global_stat_cache_hits, global_stat_cache_misses, eff ));
}

typedef struct {
  int name_len;
  char names[2]; /* This is extended via malloc... */
} stat_cache_entry;

#define INIT_STAT_CACHE_SIZE 512
static hash_table stat_cache;

/****************************************************************************
 Add an entry into the stat cache.
*****************************************************************************/

static void stat_cache_add( char *full_orig_name, char *orig_translated_path)
{
  stat_cache_entry *scp;
  stat_cache_entry *found_scp;
  pstring orig_name;
  pstring translated_path;
  int namelen;
  hash_element *hash_elem;

  if (!lp_stat_cache()) return;

  namelen = strlen(orig_translated_path);

  /*
   * Don't cache trivial valid directory entries.
   */
  if((*full_orig_name == '\0') || (strcmp(full_orig_name, ".") == 0) ||
     (strcmp(full_orig_name, "..") == 0))
    return;

  /*
   * If we are in case insentive mode, we need to
   * store names that need no translation - else, it
   * would be a waste.
   */

  if(case_sensitive && (strcmp(full_orig_name, orig_translated_path) == 0))
    return;

  /*
   * Remove any trailing '/' characters from the
   * translated path.
   */

  pstrcpy(translated_path, orig_translated_path);
  if(translated_path[namelen-1] == '/') {
    translated_path[namelen-1] = '\0';
    namelen--;
  }

  /*
   * We will only replace namelen characters 
   * of full_orig_name.
   * StrnCpy always null terminates.
   */

  StrnCpy(orig_name, full_orig_name, MIN(namelen, sizeof(orig_name)-1));
  if(!case_sensitive)
    strupper( orig_name );

  /*
   * Check this name doesn't exist in the cache before we 
   * add it.
   */

  if ((hash_elem = hash_lookup(&stat_cache, orig_name)) != NULL) {
    found_scp = (stat_cache_entry *)(hash_elem->value);
    if (strcmp((found_scp->names+found_scp->name_len+1), translated_path) == 0) {
      return;
    } else {
      hash_remove(&stat_cache, hash_elem);
      if((scp = (stat_cache_entry *)malloc(sizeof(stat_cache_entry)+2*namelen)) == NULL) {
        DEBUG(0,("stat_cache_add: Out of memory !\n"));
        return;
      }
      pstrcpy(scp->names, orig_name);
      pstrcpy((scp->names+namelen+1), translated_path);
      scp->name_len = namelen;
      hash_insert(&stat_cache, (char *)scp, orig_name);
    }
    return;
  } else {

    /*
     * New entry.
     */

    if((scp = (stat_cache_entry *)malloc(sizeof(stat_cache_entry)+2*namelen)) == NULL) {
      DEBUG(0,("stat_cache_add: Out of memory !\n"));
      return;
    }
    pstrcpy(scp->names, orig_name);
    pstrcpy(scp->names+namelen+1, translated_path);
    scp->name_len = namelen;
    hash_insert(&stat_cache, (char *)scp, orig_name);
  }

  DEBUG(5,("stat_cache_add: Added entry %s -> %s\n", scp->names, (scp->names+scp->name_len+1)));
}

/****************************************************************************
 Look through the stat cache for an entry - promote it to the top if found.
 Return True if we translated (and did a scuccessful stat on) the entire name.
*****************************************************************************/

static BOOL stat_cache_lookup( char *name, char *dirpath, char **start, SMB_STRUCT_STAT *pst)
{
  stat_cache_entry *scp;
  char *trans_name;
  pstring chk_name;
  int namelen;
  hash_element *hash_elem;
  char *sp;

  if (!lp_stat_cache())
    return False;
 
  namelen = strlen(name);

  *start = name;
  global_stat_cache_lookups++;

  /*
   * Don't lookup trivial valid directory entries.
   */
  if((*name == '\0') || (strcmp(name, ".") == 0) || (strcmp(name, "..") == 0)) {
    global_stat_cache_misses++;
    return False;
  }

  pstrcpy(chk_name, name);
  if(!case_sensitive)
    strupper( chk_name );

  while (1) {
    hash_elem = hash_lookup(&stat_cache, chk_name);
    if(hash_elem == NULL) {
      /*
       * Didn't find it - remove last component for next try.
       */
      sp = strrchr(chk_name, '/');
      if (sp) {
        *sp = '\0';
      } else {
        /*
         * We reached the end of the name - no match.
         */
        global_stat_cache_misses++;
        return False;
      }
      if((*chk_name == '\0') || (strcmp(chk_name, ".") == 0)
                          || (strcmp(chk_name, "..") == 0)) {
        global_stat_cache_misses++;
        return False;
      }
    } else {
      scp = (stat_cache_entry *)(hash_elem->value);
      global_stat_cache_hits++;
      trans_name = scp->names+scp->name_len+1;
      if(dos_stat( trans_name, pst) != 0) {
        /* Discard this entry - it doesn't exist in the filesystem.  */
        hash_remove(&stat_cache, hash_elem);
        return False;
      }
      memcpy(name, trans_name, scp->name_len);
      *start = &name[scp->name_len];
      if(**start == '/')
        ++*start;
      StrnCpy( dirpath, trans_name, name - (*start));
      return (namelen == scp->name_len);
    }
  }
}

/****************************************************************************
This routine is called to convert names from the dos namespace to unix
namespace. It needs to handle any case conversions, mangling, format
changes etc.

We assume that we have already done a chdir() to the right "root" directory
for this service.

The function will return False if some part of the name except for the last
part cannot be resolved

If the saved_last_component != 0, then the unmodified last component
of the pathname is returned there. This is used in an exceptional
case in reply_mv (so far). If saved_last_component == 0 then nothing
is returned there.

The bad_path arg is set to True if the filename walk failed. This is
used to pick the correct error code to return between ENOENT and ENOTDIR
as Windows applications depend on ERRbadpath being returned if a component
of a pathname does not exist.
****************************************************************************/

BOOL unix_convert(char *name,connection_struct *conn,char *saved_last_component, 
                  BOOL *bad_path, SMB_STRUCT_STAT *pst)
{
  SMB_STRUCT_STAT st;
  char *start, *end;
  pstring dirpath;
  pstring orig_path;
  BOOL component_was_mangled = False;
  BOOL name_has_wildcard = False;
#if 0
  /* Andrew's conservative code... JRA. */
  extern char magic_char;
#endif

  DEBUG(5, ("unix_convert called on file \"%s\"\n", name));

  *dirpath = 0;
  *bad_path = False;
  if(pst) {
    ZERO_STRUCTP(pst);
  }

  if(saved_last_component)
    *saved_last_component = 0;

  /* 
   * Convert to basic unix format - removing \ chars and cleaning it up.
   */

  unix_format(name);
  unix_clean_name(name);

  /* 
   * Names must be relative to the root of the service - trim any leading /.
   * also trim trailing /'s.
   */

  trim_string(name,"/","/");

  /*
   * If we trimmed down to a single '\0' character
   * then we should use the "." directory to avoid
   * searching the cache, but not if we are in a
   * printing share.
   */

  if (!*name && (!conn -> printer)) {
    name[0] = '.';
    name[1] = '\0';
  }

  /*
   * Ensure saved_last_component is valid even if file exists.
   */

  if(saved_last_component) {
    end = strrchr(name, '/');
    if(end)
      pstrcpy(saved_last_component, end + 1);
    else
      pstrcpy(saved_last_component, name);
  }

  if (!case_sensitive && 
      (!case_preserve || (is_8_3(name, False) && !short_case_preserve)))
    strnorm(name);

  /* 
   * Check if it's a printer file.
   */
  if (conn->printer) {
    if ((! *name) || strchr(name,'/') || !is_8_3(name, True)) {
      char *s;
      fstring name2;
      slprintf(name2,sizeof(name2)-1,"%.6s.XXXXXX",remote_machine);

      /* 
       * Sanitise the name.
       */

      for (s=name2 ; *s ; s++)
        if (!issafe(*s)) *s = '_';
      pstrcpy(name,(char *)smbd_mktemp(name2));	  
    }      
    return(True);
  }

  /*
   * If we trimmed down to a single '\0' character
   * then we will be using the "." directory.
   * As we know this is valid we can return true here.
   */

  if(!*name)
    return(True);

  start = name;
  while (strncmp(start,"./",2) == 0)
    start += 2;

  pstrcpy(orig_path, name);

  if(stat_cache_lookup( name, dirpath, &start, &st)) {
    if(pst)
      *pst = st;
    return True;
  }

  /* 
   * stat the name - if it exists then we are all done!
   */

  if (dos_stat(name,&st) == 0) {
    stat_cache_add(orig_path, name);
    DEBUG(5,("conversion finished %s -> %s\n",orig_path, name));
    if(pst)
      *pst = st;
    return(True);
  }

  DEBUG(5,("unix_convert begin: name = %s, dirpath = %s, start = %s\n",
        name, dirpath, start));

  /* 
   * A special case - if we don't have any mangling chars and are case
   * sensitive then searching won't help.
   */

  if (case_sensitive && !is_mangled(name) && 
      !lp_strip_dot() && !use_mangled_map)
    return(False);

  if(strchr(start,'?') || strchr(start,'*'))
    name_has_wildcard = True;

  /* 
   * is_mangled() was changed to look at an entire pathname, not 
   * just a component. JRA.
   */

  if(is_mangled(start))
    component_was_mangled = True;

#if 0
  /* Keep Andrew's conservative code around, just in case. JRA. */
  /* this is an extremely conservative test for mangled names. */
  if (strchr(start,magic_char))
    component_was_mangled = True;
#endif

  /* 
   * Now we need to recursively match the name against the real 
   * directory structure.
   */

  /* 
   * Match each part of the path name separately, trying the names
   * as is first, then trying to scan the directory for matching names.
   */

  for (; start ; start = (end?end+1:(char *)NULL)) {
      /* 
       * Pinpoint the end of this section of the filename.
       */
      end = strchr(start, '/');

      /* 
       * Chop the name at this point.
       */
      if (end) 
        *end = 0;

      if(saved_last_component != 0)
        pstrcpy(saved_last_component, end ? end + 1 : start);

      /* 
       * Check if the name exists up to this point.
       */
      if (dos_stat(name, &st) == 0) {
        /*
         * It exists. it must either be a directory or this must be
         * the last part of the path for it to be OK.
         */
        if (end && !(st.st_mode & S_IFDIR)) {
          /*
           * An intermediate part of the name isn't a directory.
            */
          DEBUG(5,("Not a dir %s\n",start));
          *end = '/';
          return(False);
        }

      } else {
        pstring rest;

        *rest = 0;

        /*
         * Remember the rest of the pathname so it can be restored
         * later.
         */

        if (end)
          pstrcpy(rest,end+1);

        /*
         * Try to find this part of the path in the directory.
         */

        if (strchr(start,'?') || strchr(start,'*') ||
            !scan_directory(dirpath, start, conn, end?True:False)) {
          if (end) {
            /*
             * An intermediate part of the name can't be found.
             */
            DEBUG(5,("Intermediate not found %s\n",start));
            *end = '/';

            /* 
             * We need to return the fact that the intermediate
             * name resolution failed. This is used to return an
             * error of ERRbadpath rather than ERRbadfile. Some
             * Windows applications depend on the difference between
             * these two errors.
             */
            *bad_path = True;
            return(False);
          }
	      
          /* 
           * Just the last part of the name doesn't exist.
	       * We may need to strupper() or strlower() it in case
           * this conversion is being used for file creation 
           * purposes. If the filename is of mixed case then 
           * don't normalise it.
           */

          if (!case_preserve && (!strhasupper(start) || !strhaslower(start)))		
            strnorm(start);

          /*
           * check on the mangled stack to see if we can recover the 
           * base of the filename.
           */

          if (is_mangled(start)) {
            check_mangled_cache( start );
          }

          DEBUG(5,("New file %s\n",start));
          return(True); 
        }

      /* 
       * Restore the rest of the string.
       */
      if (end) {
        pstrcpy(start+strlen(start)+1,rest);
        end = start + strlen(start);
      }
    } /* end else */

    /* 
     * Add to the dirpath that we have resolved so far.
     */
    if (*dirpath)
      pstrcat(dirpath,"/");

    pstrcat(dirpath,start);

    /*
     * Don't cache a name with mangled or wildcard components
     * as this can change the size.
     */

    if(!component_was_mangled && !name_has_wildcard)
      stat_cache_add(orig_path, dirpath);

    /* 
     * Restore the / that we wiped out earlier.
     */
    if (end)
      *end = '/';
  }
  
  /*
   * Don't cache a name with mangled or wildcard components
   * as this can change the size.
   */

  if(!component_was_mangled && !name_has_wildcard)
    stat_cache_add(orig_path, name);

  /* 
   * The name has been resolved.
   */

  DEBUG(5,("conversion finished %s -> %s\n",orig_path, name));
  return(True);
}


/****************************************************************************
check a filename - possibly caling reducename

This is called by every routine before it allows an operation on a filename.
It does any final confirmation necessary to ensure that the filename is
a valid one for the user to access.
****************************************************************************/
BOOL check_name(char *name,connection_struct *conn)
{
  BOOL ret;

  errno = 0;

  if (IS_VETO_PATH(conn, name))  {
	  DEBUG(5,("file path name %s vetoed\n",name));
	  return(0);
  }

  ret = reduce_name(name,conn->connectpath,lp_widelinks(SNUM(conn)));

  /* Check if we are allowing users to follow symlinks */
  /* Patch from David Clerc <David.Clerc@cui.unige.ch>
     University of Geneva */

#ifdef S_ISLNK
  if (!lp_symlinks(SNUM(conn)))
    {
      SMB_STRUCT_STAT statbuf;
      if ( (dos_lstat(name,&statbuf) != -1) &&
          (S_ISLNK(statbuf.st_mode)) )
        {
          DEBUG(3,("check_name: denied: file path name %s is a symlink\n",name));
          ret=0; 
        }
    }
#endif

  if (!ret)
    DEBUG(5,("check_name on %s failed\n",name));

  return(ret);
}


/****************************************************************************
scan a directory to find a filename, matching without case sensitivity

If the name looks like a mangled name then try via the mangling functions
****************************************************************************/
static BOOL scan_directory(char *path, char *name,connection_struct *conn,BOOL docache)
{
  void *cur_dir;
  char *dname;
  BOOL mangled;
  pstring name2;

  mangled = is_mangled(name);

  /* handle null paths */
  if (*path == 0)
    path = ".";

  if (docache && (dname = DirCacheCheck(path,name,SNUM(conn)))) {
    pstrcpy(name, dname);	
    return(True);
  }      

  /*
   * The incoming name can be mangled, and if we de-mangle it
   * here it will not compare correctly against the filename (name2)
   * read from the directory and then mangled by the name_map_mangle()
   * call. We need to mangle both names or neither.
   * (JRA).
   */
  if (mangled)
    mangled = !check_mangled_cache( name );

  /* open the directory */
  if (!(cur_dir = OpenDir(conn, path, True))) 
    {
      DEBUG(3,("scan dir didn't open dir [%s]\n",path));
      return(False);
    }

  /* now scan for matching names */
  while ((dname = ReadDirName(cur_dir))) 
    {
      if (*dname == '.' &&
	  (strequal(dname,".") || strequal(dname,"..")))
	continue;

      pstrcpy(name2,dname);
      if (!name_map_mangle(name2,False,True,SNUM(conn)))
        continue;

      if ((mangled && mangled_equal(name,name2))
	  || fname_equal(name, name2))
	{
	  /* we've found the file, change it's name and return */
	  if (docache) DirCacheAdd(path,name,dname,SNUM(conn));
	  pstrcpy(name, dname);
	  CloseDir(cur_dir);
	  return(True);
	}
    }

  CloseDir(cur_dir);
  return(False);
}

/*************************************************************************** **
 * Initializes or clears the stat cache.
 *
 *  Input:  none.
 *  Output: none.
 *
 * ************************************************************************** **
 */
BOOL reset_stat_cache( void )
{
  return hash_table_init( &stat_cache, INIT_STAT_CACHE_SIZE, (compare_function)(strcmp));
} /* reset_stat_cache  */
