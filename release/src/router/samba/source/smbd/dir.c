/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Directory handling routines
   Copyright (C) Andrew Tridgell 1992-1998
   
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

#include "includes.h"

extern int DEBUGLEVEL;

/*
   This module implements directory related functions for Samba.
*/

typedef struct _dptr_struct {
	struct _dptr_struct *next, *prev;
	int dnum;
	uint16 spid;
	connection_struct *conn;
	void *ptr;
	BOOL expect_close;
	char *wcard; /* Field only used for trans2_ searches */
	uint16 attr; /* Field only used for trans2_ searches */
	char *path;
} dptr_struct;

static struct bitmap *dptr_bmap;
static dptr_struct *dirptrs;

static int dptrs_open = 0;

#define INVALID_DPTR_KEY (-3)

/****************************************************************************
 Initialise the dir bitmap.
****************************************************************************/

void init_dptrs(void)
{
  static BOOL dptrs_init=False;

  if (dptrs_init)
    return;

  dptr_bmap = bitmap_allocate(MAX_DIRECTORY_HANDLES);

  if (!dptr_bmap)
    exit_server("out of memory in init_dptrs\n");

  dptrs_init = True;
}

/****************************************************************************
 Idle a dptr - the directory is closed but the control info is kept.
****************************************************************************/

static void dptr_idle(dptr_struct *dptr)
{
  if (dptr->ptr) {
    DEBUG(4,("Idling dptr dnum %d\n",dptr->dnum));
    dptrs_open--;
    CloseDir(dptr->ptr);
    dptr->ptr = NULL;
  }
}

/****************************************************************************
 Idle the oldest dptr.
****************************************************************************/

static void dptr_idleoldest(void)
{
  dptr_struct *dptr;

  /*
   * Go to the end of the list.
   */
  for(dptr = dirptrs; dptr && dptr->next; dptr = dptr->next)
    ;

  if(!dptr) {
    DEBUG(0,("No dptrs available to idle ?\n"));
    return;
  }

  /*
   * Idle the oldest pointer.
   */

  for(; dptr; dptr = dptr->prev) {
    if (dptr->ptr) {
      dptr_idle(dptr);
      return;
    }
  }
}

/****************************************************************************
 Get the dptr_struct for a dir index.
****************************************************************************/

static dptr_struct *dptr_get(int key, BOOL forclose)
{
  dptr_struct *dptr;

  for(dptr = dirptrs; dptr; dptr = dptr->next) {
    if(dptr->dnum == key) {
      if (!forclose && !dptr->ptr) {
        if (dptrs_open >= MAX_OPEN_DIRECTORIES)
          dptr_idleoldest();
        DEBUG(4,("Reopening dptr key %d\n",key));
        if ((dptr->ptr = OpenDir(dptr->conn, dptr->path, True)))
          dptrs_open++;
      }
      DLIST_PROMOTE(dirptrs,dptr);
      return dptr;
    }
  }
  return(NULL);
}

/****************************************************************************
 Get the dptr ptr for a dir index.
****************************************************************************/

static void *dptr_ptr(int key)
{
  dptr_struct *dptr = dptr_get(key, False);

  if (dptr)
    return(dptr->ptr);
  return(NULL);
}

/****************************************************************************
 Get the dir path for a dir index.
****************************************************************************/

char *dptr_path(int key)
{
  dptr_struct *dptr = dptr_get(key, False);

  if (dptr)
    return(dptr->path);
  return(NULL);
}

/****************************************************************************
 Get the dir wcard for a dir index (lanman2 specific).
****************************************************************************/

char *dptr_wcard(int key)
{
  dptr_struct *dptr = dptr_get(key, False);

  if (dptr)
    return(dptr->wcard);
  return(NULL);
}

/****************************************************************************
 Set the dir wcard for a dir index (lanman2 specific).
 Returns 0 on ok, 1 on fail.
****************************************************************************/

BOOL dptr_set_wcard(int key, char *wcard)
{
  dptr_struct *dptr = dptr_get(key, False);

  if (dptr) {
    dptr->wcard = wcard;
    return True;
  }
  return False;
}

/****************************************************************************
 Set the dir attrib for a dir index (lanman2 specific).
 Returns 0 on ok, 1 on fail.
****************************************************************************/

BOOL dptr_set_attr(int key, uint16 attr)
{
  dptr_struct *dptr = dptr_get(key, False);

  if (dptr) {
    dptr->attr = attr;
    return True;
  }
  return False;
}

/****************************************************************************
 Get the dir attrib for a dir index (lanman2 specific)
****************************************************************************/

uint16 dptr_attr(int key)
{
  dptr_struct *dptr = dptr_get(key, False);

  if (dptr)
    return(dptr->attr);
  return(0);
}

/****************************************************************************
 Close a dptr (internal func).
****************************************************************************/

static void dptr_close_internal(dptr_struct *dptr)
{
  DEBUG(4,("closing dptr key %d\n",dptr->dnum));

  DLIST_REMOVE(dirptrs, dptr);

  /* 
   * Free the dnum in the bitmap. Remember the dnum value is always 
   * biased by one with respect to the bitmap.
   */

  if(bitmap_query( dptr_bmap, dptr->dnum - 1) != True) {
    DEBUG(0,("dptr_close_internal : Error - closing dnum = %d and bitmap not set !\n",
			dptr->dnum ));
  }

  bitmap_clear(dptr_bmap, dptr->dnum - 1);

  if (dptr->ptr) {
    CloseDir(dptr->ptr);
    dptrs_open--;
  }

  /* Lanman 2 specific code */
  if (dptr->wcard)
    free(dptr->wcard);
  string_set(&dptr->path,"");
  free((char *)dptr);
}

/****************************************************************************
 Close a dptr given a key.
****************************************************************************/

void dptr_close(int *key)
{
  dptr_struct *dptr;

  if(*key == INVALID_DPTR_KEY)
    return;

  /* OS/2 seems to use -1 to indicate "close all directories" */
  if (*key == -1) {
    dptr_struct *next;
    for(dptr = dirptrs; dptr; dptr = next) {
      next = dptr->next;
      dptr_close_internal(dptr);
    }
    *key = INVALID_DPTR_KEY;
    return;
  }

  dptr = dptr_get(*key, True);

  if (!dptr) {
    DEBUG(0,("Invalid key %d given to dptr_close\n", *key));
    return;
  }

  dptr_close_internal(dptr);

  *key = INVALID_DPTR_KEY;
}

/****************************************************************************
 Close all dptrs for a cnum.
****************************************************************************/

void dptr_closecnum(connection_struct *conn)
{
  dptr_struct *dptr, *next;
  for(dptr = dirptrs; dptr; dptr = next) {
    next = dptr->next;
    if (dptr->conn == conn)
      dptr_close_internal(dptr);
  }
}

/****************************************************************************
 Idle all dptrs for a cnum.
****************************************************************************/

void dptr_idlecnum(connection_struct *conn)
{
  dptr_struct *dptr;
  for(dptr = dirptrs; dptr; dptr = dptr->next) {
    if (dptr->conn == conn && dptr->ptr)
      dptr_idle(dptr);
  }
}

/****************************************************************************
 Close a dptr that matches a given path, only if it matches the spid also.
****************************************************************************/

void dptr_closepath(char *path,uint16 spid)
{
  dptr_struct *dptr, *next;
  for(dptr = dirptrs; dptr; dptr = next) {
    next = dptr->next;
    if (spid == dptr->spid && strequal(dptr->path,path))
      dptr_close_internal(dptr);
  }
}

/****************************************************************************
 Start a directory listing.
****************************************************************************/

static BOOL start_dir(connection_struct *conn,char *directory)
{
  DEBUG(5,("start_dir dir=%s\n",directory));

  if (!check_name(directory,conn))
    return(False);
  
  if (! *directory)
    directory = ".";

  conn->dirptr = OpenDir(conn, directory, True);
  if (conn->dirptr) {    
    dptrs_open++;
    string_set(&conn->dirpath,directory);
    return(True);
  }
  
  return(False);
}

/****************************************************************************
 Try and close the oldest handle not marked for
 expect close in the hope that the client has
 finished with that one.
****************************************************************************/

static void dptr_close_oldest(BOOL old)
{
  dptr_struct *dptr;

  /*
   * Go to the end of the list.
   */
  for(dptr = dirptrs; dptr && dptr->next; dptr = dptr->next)
    ;

  if(!dptr) {
    DEBUG(0,("No old dptrs available to close oldest ?\n"));
    return;
  }

  /*
   * If 'old' is true, close the oldest oldhandle dnum (ie. 1 < dnum < 256) that
   * does not have expect_close set. If 'old' is false, close
   * one of the new dnum handles.
   */

  for(; dptr; dptr = dptr->prev) {
    if ((old && (dptr->dnum < 256) && !dptr->expect_close) ||
        (!old && (dptr->dnum > 255))) {
      dptr_close_internal(dptr);
      return;
    }
  }
}

/****************************************************************************
 Create a new dir ptr. If the flag old_handle is true then we must allocate
 from the bitmap range 0 - 255 as old SMBsearch directory handles are only
 one byte long. If old_handle is false we allocate from the range
 256 - MAX_DIRECTORY_HANDLES. We bias the number we return by 1 to ensure
 a directory handle is never zero. All the above is folklore taught to
 me at Andrew's knee.... :-) :-). JRA.
****************************************************************************/

int dptr_create(connection_struct *conn,char *path, BOOL old_handle, BOOL expect_close,uint16 spid)
{
  dptr_struct *dptr;

  if (!start_dir(conn,path))
    return(-2); /* Code to say use a unix error return code. */

  if (dptrs_open >= MAX_OPEN_DIRECTORIES)
    dptr_idleoldest();

  dptr = (dptr_struct *)malloc(sizeof(dptr_struct));
  if(!dptr) {
    DEBUG(0,("malloc fail in dptr_create.\n"));
    return -1;
  }

  ZERO_STRUCTP(dptr);

  if(old_handle) {

    /*
     * This is an old-style SMBsearch request. Ensure the
     * value we return will fit in the range 1-255.
     */

    dptr->dnum = bitmap_find(dptr_bmap, 0);

    if(dptr->dnum == -1 || dptr->dnum > 254) {

      /*
       * Try and close the oldest handle not marked for
       * expect close in the hope that the client has
       * finished with that one.
       */

      dptr_close_oldest(True);

      /* Now try again... */
      dptr->dnum = bitmap_find(dptr_bmap, 0);

      if(dptr->dnum == -1 || dptr->dnum > 254) {
        DEBUG(0,("dptr_create: returned %d: Error - all old dirptrs in use ?\n", dptr->dnum));
        free((char *)dptr);
        return -1;
      }
    }
  } else {

    /*
     * This is a new-style trans2 request. Allocate from
     * a range that will return 256 - MAX_DIRECTORY_HANDLES.
     */

    dptr->dnum = bitmap_find(dptr_bmap, 255);

    if(dptr->dnum == -1 || dptr->dnum < 255) {

      /*
       * Try and close the oldest handle close in the hope that
       * the client has finished with that one. This will only
       * happen in the case of the Win98 client bug where it leaks
       * directory handles.
       */

      dptr_close_oldest(False);

      /* Now try again... */
      dptr->dnum = bitmap_find(dptr_bmap, 255);

      if(dptr->dnum == -1 || dptr->dnum < 255) {
        DEBUG(0,("dptr_create: returned %d: Error - all new dirptrs in use ?\n", dptr->dnum));
        free((char *)dptr);
        return -1;
      }
    }
  }

  bitmap_set(dptr_bmap, dptr->dnum);

  dptr->dnum += 1; /* Always bias the dnum by one - no zero dnums allowed. */

  dptr->ptr = conn->dirptr;
  string_set(&dptr->path,path);
  dptr->conn = conn;
  dptr->spid = spid;
  dptr->expect_close = expect_close;
  dptr->wcard = NULL; /* Only used in lanman2 searches */
  dptr->attr = 0; /* Only used in lanman2 searches */

  DLIST_ADD(dirptrs, dptr);

  DEBUG(3,("creating new dirptr %d for path %s, expect_close = %d\n",
	   dptr->dnum,path,expect_close));  

  return(dptr->dnum);
}

/****************************************************************************
 Fill the 5 byte server reserved dptr field.
****************************************************************************/

BOOL dptr_fill(char *buf1,unsigned int key)
{
  unsigned char *buf = (unsigned char *)buf1;
  void *p = dptr_ptr(key);
  uint32 offset;
  if (!p) {
    DEBUG(1,("filling null dirptr %d\n",key));
    return(False);
  }
  offset = TellDir(p);
  DEBUG(6,("fill on key %u dirptr 0x%lx now at %d\n",key,
	   (long)p,(int)offset));
  buf[0] = key;
  SIVAL(buf,1,offset | DPTR_MASK);
  return(True);
}

/****************************************************************************
 Fetch the dir ptr and seek it given the 5 byte server field.
****************************************************************************/

void *dptr_fetch(char *buf,int *num)
{
  unsigned int key = *(unsigned char *)buf;
  void *p = dptr_ptr(key);
  uint32 offset;
  if (!p) {
    DEBUG(3,("fetched null dirptr %d\n",key));
    return(NULL);
  }
  *num = key;
  offset = IVAL(buf,1)&~DPTR_MASK;
  SeekDir(p,offset);
  DEBUG(3,("fetching dirptr %d for path %s at offset %d\n",
	   key,dptr_path(key),offset));
  return(p);
}

/****************************************************************************
 Fetch the dir ptr.
****************************************************************************/

void *dptr_fetch_lanman2(int dptr_num)
{
  void *p = dptr_ptr(dptr_num);

  if (!p) {
    DEBUG(3,("fetched null dirptr %d\n",dptr_num));
    return(NULL);
  }
  DEBUG(3,("fetching dirptr %d for path %s\n",dptr_num,dptr_path(dptr_num)));
  return(p);
}

/****************************************************************************
 Check a filetype for being valid.
****************************************************************************/

BOOL dir_check_ftype(connection_struct *conn,int mode,SMB_STRUCT_STAT *st,int dirtype)
{
  if (((mode & ~dirtype) & (aHIDDEN | aSYSTEM | aDIR)) != 0)
    return False;
  return True;
}

/****************************************************************************
 Get an 8.3 directory entry.
****************************************************************************/

BOOL get_dir_entry(connection_struct *conn,char *mask,int dirtype,char *fname,
                   SMB_OFF_T *size,int *mode,time_t *date,BOOL check_descend)
{
  char *dname;
  BOOL found = False;
  SMB_STRUCT_STAT sbuf;
  pstring path;
  pstring pathreal;
  BOOL isrootdir;
  pstring filename;
  BOOL needslash;

  *path = *pathreal = *filename = 0;

  isrootdir = (strequal(conn->dirpath,"./") ||
	       strequal(conn->dirpath,".") ||
	       strequal(conn->dirpath,"/"));
  
  needslash = ( conn->dirpath[strlen(conn->dirpath) -1] != '/');

  if (!conn->dirptr)
    return(False);
  
  while (!found)
  {
    BOOL filename_is_mask = False;
    dname = ReadDirName(conn->dirptr);

    DEBUG(6,("readdir on dirptr 0x%lx now at offset %d\n",
          (long)conn->dirptr,TellDir(conn->dirptr)));
      
    if (dname == NULL) 
      return(False);
      
    pstrcpy(filename,dname);      

    if ((filename_is_mask = (strcmp(filename,mask) == 0)) ||
        (name_map_mangle(filename,True,False,SNUM(conn)) &&
         mask_match(filename,mask,False,False)))
    {
      if (isrootdir && (strequal(filename,"..") || strequal(filename,".")))
        continue;

      pstrcpy(fname,filename);
      *path = 0;
      pstrcpy(path,conn->dirpath);
      if(needslash)
        pstrcat(path,"/");
      pstrcpy(pathreal,path);
      pstrcat(path,fname);
      pstrcat(pathreal,dname);
      if (dos_stat(pathreal,&sbuf) != 0) 
      {
        DEBUG(5,("Couldn't stat 1 [%s]. Error = %s\n",path, strerror(errno) ));
        continue;
      }

      if (check_descend && !strequal(fname,".") && !strequal(fname,".."))
        continue;
	  
      *mode = dos_mode(conn,pathreal,&sbuf);

      if (!dir_check_ftype(conn,*mode,&sbuf,dirtype)) 
      {
        DEBUG(5,("[%s] attribs didn't match %x\n",filename,dirtype));
        continue;
      }

      if (!filename_is_mask)
      {
        /* Now we can allow the mangled cache to be updated */
        pstrcpy(filename,dname);
        name_map_mangle(filename,True,True,SNUM(conn));
      }

      *size = sbuf.st_size;
      *date = sbuf.st_mtime;

      DEBUG(5,("get_dir_entry found %s fname=%s\n",pathreal,fname));
	  
      found = True;
    }
  }

  return(found);
}



typedef struct
{
  int pos;
  int numentries;
  int mallocsize;
  char *data;
  char *current;
} Dir;


/*******************************************************************
 Open a directory.
********************************************************************/

void *OpenDir(connection_struct *conn, char *name, BOOL use_veto)
{
  Dir *dirp;
  char *n;
  DIR *p = dos_opendir(name);
  int used=0;

  if (!p) return(NULL);
  dirp = (Dir *)malloc(sizeof(Dir));
  if (!dirp) {
    closedir(p);
    return(NULL);
  }
  dirp->pos = dirp->numentries = dirp->mallocsize = 0;
  dirp->data = dirp->current = NULL;

  while ((n = dos_readdirname(p)))
  {
    int l = strlen(n)+1;

    /* If it's a vetoed file, pretend it doesn't even exist */
    if (use_veto && conn && IS_VETO_PATH(conn, n)) continue;

    if (used + l > dirp->mallocsize) {
      int s = MAX(used+l,used+2000);
      char *r;
      r = (char *)Realloc(dirp->data,s);
      if (!r) {
	DEBUG(0,("Out of memory in OpenDir\n"));
	break;
      }
      dirp->data = r;
      dirp->mallocsize = s;
      dirp->current = dirp->data;
    }
    pstrcpy(dirp->data+used,n);
    used += l;
    dirp->numentries++;
  }

  closedir(p);
  return((void *)dirp);
}


/*******************************************************************
 Close a directory.
********************************************************************/

void CloseDir(void *p)
{
  Dir *dirp = (Dir *)p;
  if (!dirp) return;    
  if (dirp->data) free(dirp->data);
  free(dirp);
}

/*******************************************************************
 Read from a directory.
********************************************************************/

char *ReadDirName(void *p)
{
  char *ret;
  Dir *dirp = (Dir *)p;

  if (!dirp || !dirp->current || dirp->pos >= dirp->numentries) return(NULL);

  ret = dirp->current;
  dirp->current = skip_string(dirp->current,1);
  dirp->pos++;

  return(ret);
}


/*******************************************************************
 Seek a dir.
********************************************************************/

BOOL SeekDir(void *p,int pos)
{
  Dir *dirp = (Dir *)p;

  if (!dirp) return(False);

  if (pos < dirp->pos) {
    dirp->current = dirp->data;
    dirp->pos = 0;
  }

  while (dirp->pos < pos && ReadDirName(p)) ;

  return(dirp->pos == pos);
}

/*******************************************************************
 Tell a dir position.
********************************************************************/

int TellDir(void *p)
{
  Dir *dirp = (Dir *)p;

  if (!dirp) return(-1);
  
  return(dirp->pos);
}


/* -------------------------------------------------------------------------- **
 * This section manages a global directory cache.
 * (It should probably be split into a separate module.  crh)
 * -------------------------------------------------------------------------- **
 */

typedef struct
  {
  ubi_dlNode  node;
  char       *path;
  char       *name;
  char       *dname;
  int         snum;
  } dir_cache_entry;

static ubi_dlNewList( dir_cache );

void DirCacheAdd( char *path, char *name, char *dname, int snum )
  /* ------------------------------------------------------------------------ **
   * Add an entry to the directory cache.
   *
   *  Input:  path  -
   *          name  -
   *          dname -
   *          snum  -
   *
   *  Output: None.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int               pathlen;
  int               namelen;
  dir_cache_entry  *entry;

  /* Allocate the structure & string space in one go so that it can be freed
   * in one call to free().
   */
  pathlen = strlen( path ) +1;  /* Bytes required to store path (with nul). */
  namelen = strlen( name ) +1;  /* Bytes required to store name (with nul). */
  entry = (dir_cache_entry *)malloc( sizeof( dir_cache_entry )
                                   + pathlen
                                   + namelen
                                   + strlen( dname ) +1 );
  if( NULL == entry )   /* Not adding to the cache is not fatal,  */
    return;             /* so just return as if nothing happened. */

  /* Set pointers correctly and load values. */
  entry->path  = pstrcpy( (char *)&entry[1],       path);
  entry->name  = pstrcpy( &(entry->path[pathlen]), name);
  entry->dname = pstrcpy( &(entry->name[namelen]), dname);
  entry->snum  = snum;

  /* Add the new entry to the linked list. */
  (void)ubi_dlAddHead( dir_cache, entry );
  DEBUG( 4, ("Added dir cache entry %s %s -> %s\n", path, name, dname ) );

  /* Free excess cache entries. */
  while( DIRCACHESIZE < dir_cache->count )
    free( ubi_dlRemTail( dir_cache ) );

  } /* DirCacheAdd */


char *DirCacheCheck( char *path, char *name, int snum )
  /* ------------------------------------------------------------------------ **
   * Search for an entry to the directory cache.
   *
   *  Input:  path  -
   *          name  -
   *          snum  -
   *
   *  Output: The dname string of the located entry, or NULL if the entry was
   *          not found.
   *
   *  Notes:  This uses a linear search, which is is okay because of
   *          the small size of the cache.  Use a splay tree or hash
   *          for large caches.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  dir_cache_entry *entry;

  for( entry = (dir_cache_entry *)ubi_dlFirst( dir_cache );
       NULL != entry;
       entry = (dir_cache_entry *)ubi_dlNext( entry ) )
    {
    if( entry->snum == snum
        && 0 == strcmp( name, entry->name )
        && 0 == strcmp( path, entry->path ) )
      {
      DEBUG(4, ("Got dir cache hit on %s %s -> %s\n",path,name,entry->dname));
      return( entry->dname );
      }
    }

  return(NULL);
  } /* DirCacheCheck */

void DirCacheFlush(int snum)
  /* ------------------------------------------------------------------------ **
   * Remove all cache entries which have an snum that matches the input.
   *
   *  Input:  snum  -
   *
   *  Output: None.
   *
   * ------------------------------------------------------------------------ **
   */
{
	dir_cache_entry *entry;
	ubi_dlNodePtr    next;

	for(entry = (dir_cache_entry *)ubi_dlFirst( dir_cache ); 
	    NULL != entry; )  {
		next = ubi_dlNext( entry );
		if( entry->snum == snum )
			free( ubi_dlRemThis( dir_cache, entry ) );
		entry = (dir_cache_entry *)next;
	}
} /* DirCacheFlush */

/* -------------------------------------------------------------------------- **
 * End of the section that manages the global directory cache.
 * -------------------------------------------------------------------------- **
 */


