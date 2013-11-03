#ifndef LIBATALK_CDB_PRIVATE_H
#define LIBATALK_CDB_PRIVATE_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>

#include <db.h>

#include <atalk/logger.h>
#include <atalk/adouble.h>
#include <atalk/cnid_private.h>
#include <atalk/cnid.h>
#include <atalk/util.h>
#include "cnid_cdb.h"

/* record structure 
   cnid                4
   dev                 8
   inode               8
   type/last cnid      4 Not used    
   did                 4
   name                strlen(name) +1

primary key 
cnid
secondary keys
dev/inode
did/name   

*/

typedef struct cnid_record { /* helper for debug don't use */
  unsigned int  cnid;
  dev_t    dev;
  ino_t    inode;
  unsigned int  type;
  unsigned int  did;
  char          name[50];
} cnid_record;

typedef struct CNID_private {
    u_int32_t magic;
    DB *db_cnid;
    DB *db_didname;
    DB *db_devino;
    DB_ENV *dbenv;
    int lockfd, flags;
    char lock_file[MAXPATHLEN + 1];
} CNID_private;

/* XXX stuff below is outdate */
/* on-disk data format (in network byte order where appropriate) --
 * db_cnid:      (key: cnid)
 * name          size (in bytes)
 * dev           4
 * ino           4
 * did           4
 * unix name     strlen(name) + 1 
 *
 * db_didname:   (key: did/unix name)
 * -- this also caches the bits of .AppleDouble used by FPGetFilDirParam
 *    so that we don't have to open the header file.
 *    NOTE: FPCatSearch has to search through all of the directories as
 *          this stuff doesn't get entered until needed.
 *          if the entire volume is in the database, though, we can use
 *          cursor operations to make this faster.
 *
 *    version number is stored with did/name key of 0/0
 *
 * cnid          4
 * modfiller     4 (dates only use 4 bytes right now, but we leave space 
 * moddate       4  for 8. moddate is also used to keep this info 
 * createfiller  4  up-to-date.)
 * createdate    4
 * backfiller    4
 * backupdate    4
 * accfiller     4 (unused)
 * accdate       4 (unused)
 * AFP info      4 (stores a couple permission bits as well)
 * finder info   32
 * prodos info   8
 * rforkfiller   4
 * rforklen      4
 * macname       32 (nul-terminated)
 * shortname     12 (nul-terminated)
 * longname      longnamelen (nul-terminated)
 * ---------------
 *             132 bytes + longnamelen
 * 
 * db_devino:    (key: dev/ino) 
 * -- this is only used for consistency checks and isn't 1-1
 * cnid          4 
 *
 * these correspond to the different path types. longname is for the
 * 255 unicode character names (path type == ?), macname is for the
 * 32-character names (path type == 2), and shortname is for the
 * 8+3-character names (path type == 1).
 *
 * db_longname: (key: did/longname)
 * name          namelen = strlen(name) + 1
 *
 * db_macname:   (key: did/macname)
 * name          namelen = strlen(name) + 1
 *
 * db_shortname: (key: did/shortname)
 * name namelen = strlen(name) + 1 
 */

/* construct db_cnid data. NOTE: this is not re-entrant.  */
extern unsigned char *make_cnid_data (u_int32_t flags, const struct stat *,const cnid_t ,
                                       const char *, const size_t );

#endif /* atalk/cnid/cnid_private.h */
