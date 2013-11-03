/* 
 * Copyright (c) 1998,1999 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT for more information.
 *
 * Because fcntl locks are
 * process-oriented, we need to keep around a list of file descriptors
 * that refer to the same file.
 *
 * TODO: fix the race when reading/writing.
 *       keep a pool of both locks and reference counters around so that
 *       we can save on mallocs. we should also use a tree to keep things
 *       sorted.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/adouble.h>
#include <atalk/logger.h>
#include <atalk/compat.h>
#include <atalk/errchk.h>
#include <atalk/util.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include <string.h>

#include "ad_lock.h"

static const char *shmdstrfromoff(off_t off)
{
    switch (off) {
    case AD_FILELOCK_OPEN_WR:
        return "OPEN_WR_DATA";
    case AD_FILELOCK_OPEN_RD:
        return "OPEN_RD_DATA";
    case AD_FILELOCK_RSRC_OPEN_WR:
        return "OPEN_WR_RSRC";
    case AD_FILELOCK_RSRC_OPEN_RD:
        return "OPEN_RD_RSRC";
    case AD_FILELOCK_DENY_WR:
        return "DENY_WR_DATA";
    case AD_FILELOCK_DENY_RD:
        return "DENY_RD_DATA";
    case AD_FILELOCK_RSRC_DENY_WR:
        return "DENY_WR_RSRC";
    case AD_FILELOCK_RSRC_DENY_RD:
        return "DENY_RD_RSRC";
    case AD_FILELOCK_OPEN_NONE:
        return "OPEN_NONE_DATA";
    case AD_FILELOCK_RSRC_OPEN_NONE:
        return "OPEN_NONE_RSRC";
    default:
        return "-";
    }
}

/* ----------------------- */
static int set_lock(int fd, int cmd,  struct flock *lock)
{
    EC_INIT;

    LOG(log_debug, logtype_ad, "set_lock(fd: %d, %s, %s, off: %jd (%s), len: %jd): BEGIN",
        fd, cmd == F_SETLK ? "F_SETLK" : "F_GETLK",
        lock->l_type == F_RDLCK ? "F_RDLCK" : lock->l_type == F_WRLCK ? "F_WRLCK" : "F_UNLCK",
        (intmax_t)lock->l_start,
        shmdstrfromoff(lock->l_start),
        (intmax_t)lock->l_len);

    if (fd == AD_SYMLINK) {
        if (cmd == F_GETLK)
            lock->l_type = F_UNLCK;
        return 0;
    }

    EC_NEG1( fcntl(fd, cmd, lock) );

EC_CLEANUP:
    EC_EXIT;
}

/* ----------------------- */
static int XLATE_FCNTL_LOCK(int type) 
{
    switch(type) {
    case ADLOCK_RD:
        return F_RDLCK;
    case ADLOCK_WR:
         return F_WRLCK;
    case ADLOCK_CLR:
         return F_UNLCK;
    }
    return -1;
}

/* ----------------------- */
static int OVERLAP(off_t a, off_t alen, off_t b, off_t blen) 
{
    return (!alen && a <= b) || 
        (!blen && b <= a) || 
        ( (a + alen > b) && (b + blen > a) );
}

/* allocation for lock regions. we allocate aggressively and shrink
 * only in large chunks. */
#define ARRAY_BLOCK_SIZE 10 
#define ARRAY_FREE_DELTA 100

/* remove a lock and compact space if necessary */
static void adf_freelock(struct ad_fd *ad, const int i)
{
    adf_lock_t *lock = ad->adf_lock + i;

    if (--(*lock->refcount) < 1) {
	free(lock->refcount); 
    lock->lock.l_type = F_UNLCK;
    set_lock(ad->adf_fd, F_SETLK, &lock->lock); /* unlock */
    }

    ad->adf_lockcount--;

    /* move another lock into the empty space */
    if (i < ad->adf_lockcount) {
        memcpy(lock, lock + ad->adf_lockcount - i, sizeof(adf_lock_t));
    }

    /* free extra cruft if we go past a boundary. we always want to
     * keep at least some stuff around for allocations. this wastes
     * a bit of space to save time on reallocations. */
    if ((ad->adf_lockmax > ARRAY_FREE_DELTA) &&
	(ad->adf_lockcount + ARRAY_FREE_DELTA < ad->adf_lockmax)) {
	    struct adf_lock_t *tmp;

	    tmp = (struct adf_lock_t *) 
		    realloc(ad->adf_lock, sizeof(adf_lock_t)*
			    (ad->adf_lockcount + ARRAY_FREE_DELTA));
	    if (tmp) {
		ad->adf_lock = tmp;
		ad->adf_lockmax = ad->adf_lockcount + ARRAY_FREE_DELTA;
	    }
    }
}


/* this needs to deal with the following cases:
 * 1) free all UNIX byterange lock from any fork
 * 2) free all locks of the requested fork
 *
 * i converted to using arrays of locks. everytime a lock
 * gets removed, we shift all of the locks down.
 */
static void adf_unlock(struct adouble *ad, struct ad_fd *adf, const int fork, int unlckbrl)
{
    adf_lock_t *lock = adf->adf_lock;
    int i;

    for (i = 0; i < adf->adf_lockcount; i++) {
        if ((unlckbrl && lock[i].lock.l_start < AD_FILELOCK_BASE)
            || lock[i].user == fork) {
            /* we're really going to delete this lock. note: read locks
               are the only ones that allow refcounts > 1 */
            adf_freelock(adf, i);
            /* we shifted things down, so we need to backtrack */
            i--; 
            /* unlikely but realloc may have change adf_lock */
            lock = adf->adf_lock;       
        }
    }
}

/* relock any byte lock that overlaps off/len. unlock everything
 * else. */
static void adf_relockrange(struct ad_fd *ad, int fd, off_t off, off_t len)
{
    adf_lock_t *lock = ad->adf_lock;
    int i;
    
    for (i = 0; i < ad->adf_lockcount; i++) {
        if (OVERLAP(off, len, lock[i].lock.l_start, lock[i].lock.l_len)) 
            set_lock(fd, F_SETLK, &lock[i].lock);
    }
}


/* find a byte lock that overlaps off/len for a particular open fork */
static int adf_findlock(struct ad_fd *ad,
                        const int fork, const int type,
                        const off_t off,
                        const off_t len)
{
  adf_lock_t *lock = ad->adf_lock;
  int i;
  
  for (i = 0; i < ad->adf_lockcount; i++) {
    if ((((type & ADLOCK_RD) && (lock[i].lock.l_type == F_RDLCK)) ||
	((type & ADLOCK_WR) && (lock[i].lock.l_type == F_WRLCK))) &&
	(lock[i].user == fork) && 
	OVERLAP(off, len, lock[i].lock.l_start, lock[i].lock.l_len)) {
      return i;
    }
  }
  return -1;
}


/* search other fork lock lists */
static int adf_findxlock(struct ad_fd *ad, 
                         const int fork, const int type,
                         const off_t off,
                         const off_t len)
{
    adf_lock_t *lock = ad->adf_lock;
    int i;
  
    for (i = 0; i < ad->adf_lockcount; i++) {
        if ((((type & ADLOCK_RD) && (lock[i].lock.l_type == F_RDLCK))
             ||
             ((type & ADLOCK_WR) && (lock[i].lock.l_type == F_WRLCK)))
            &&
            (lock[i].user != fork)
            && 
            OVERLAP(off, len, lock[i].lock.l_start, lock[i].lock.l_len)) 
            return i;
    } 
    return -1;
}

/* okay, this needs to do the following:
 * 1) check current list of locks. error on conflict.
 * 2) apply the lock. error on conflict with another process.
 * 3) update the list of locks this file has.
 * 
 * NOTE: this treats synchronization locks a little differently. we
 *       do the following things for those:
 *       1) if the header file exists, all the locks go in the beginning
 *          of that.
 *       2) if the header file doesn't exist, we stick the locks
 *          in the locations specified by AD_FILELOCK_RD/WR.
 */

/* -------------- 
	translate a resource fork lock to an offset
*/
static off_t rf2off(off_t off)
{
    off_t start = off;
	if (off == AD_FILELOCK_OPEN_WR)
		start = AD_FILELOCK_RSRC_OPEN_WR;
	else if (off == AD_FILELOCK_OPEN_RD)
		start = AD_FILELOCK_RSRC_OPEN_RD;
    else if (off == AD_FILELOCK_DENY_RD)
		start = AD_FILELOCK_RSRC_DENY_RD;
	else if (off == AD_FILELOCK_DENY_WR)
		start = AD_FILELOCK_RSRC_DENY_WR;
	else if (off == AD_FILELOCK_OPEN_NONE)
		start = AD_FILELOCK_RSRC_OPEN_NONE;
	return start;
}

/*!
 * Test a lock
 *
 * (1) Test against our own locks array
 * (2) Test fcntl lock, locks from other processes
 *
 * @param adf     (r) handle
 * @param off     (r) offset
 * @param len     (r) lenght
 *
 * @returns           1 if there's an existing lock, 0 if there's no lock,
 *                    -1 in case any error occured
 */
static int testlock(const struct ad_fd *adf, off_t off, off_t len)
{
    struct flock lock;
    adf_lock_t *plock;
    int i;

    lock.l_start = off;

    plock = adf->adf_lock;
    lock.l_whence = SEEK_SET;
    lock.l_len = len;

    /* (1) Do we have a lock ? */
    for (i = 0; i < adf->adf_lockcount; i++) {
        if (OVERLAP(lock.l_start, 1, plock[i].lock.l_start, plock[i].lock.l_len)) 
            return 1;
    }

    /* (2) Does another process have a lock? */
    lock.l_type = (adf->adf_flags & O_RDWR) ? F_WRLCK : F_RDLCK;

    if (set_lock(adf->adf_fd, F_GETLK, &lock) < 0) {
        /* is that kind of error possible ?*/
        return (errno == EACCES || errno == EAGAIN) ? 1 : -1;
    }
  
    if (lock.l_type == F_UNLCK) {
        return 0;
    }
    return 1;
}

#define LTYPE2STRBUFSIZ 128
static const char *locktypetostr(int type)
{
    int first = 1;
    static char buf[LTYPE2STRBUFSIZ];

    buf[0] = 0;

    if (type == 0) {
        strlcat(buf, "CLR", LTYPE2STRBUFSIZ);
        first = 0;
        return buf;
    }
    if (type & ADLOCK_RD) {
        if (!first)
            strlcat(buf, "|", LTYPE2STRBUFSIZ);
        strlcat(buf, "RD", LTYPE2STRBUFSIZ);
        first = 0;
    }
    if (type & ADLOCK_WR) {
        if (!first)
            strlcat(buf, "|", LTYPE2STRBUFSIZ);
        strlcat(buf, "WR", LTYPE2STRBUFSIZ);
        first = 0;
    }
    if (type & ADLOCK_UPGRADE) {
        if (!first)
            strlcat(buf, "|", LTYPE2STRBUFSIZ);
        strlcat(buf, "UPG", LTYPE2STRBUFSIZ);
        first = 0;
    }
    if (type & ADLOCK_FILELOCK) {
        if (!first)
            strlcat(buf, "|", LTYPE2STRBUFSIZ);
        strlcat(buf, "FILELOCK", LTYPE2STRBUFSIZ);
        first = 0;
    }

    return buf;
}

/******************************************************************************
 * Public functions
 ******************************************************************************/

int ad_lock(struct adouble *ad, uint32_t eid, int locktype, off_t off, off_t len, int fork)
{
    struct flock lock;
    struct ad_fd *adf;
    adf_lock_t *adflock;
    int oldlock;
    int i;
    int type;  
    int ret = 0, fcntl_lock_err = 0;

    LOG(log_debug, logtype_ad, "ad_lock(%s, %s, off: %jd (%s), len: %jd): BEGIN",
        eid == ADEID_DFORK ? "data" : "reso",
        locktypetostr(locktype),
        (intmax_t)off,
        shmdstrfromoff(off),
        (intmax_t)len);

    if ((locktype & ADLOCK_FILELOCK) && (len != 1))
        AFP_PANIC("lock API error");

    type = locktype;

    if (eid == ADEID_DFORK) {
        adf = &ad->ad_data_fork;
        lock.l_start = off;
    } else { /* rfork */
        if (type & ADLOCK_FILELOCK) {
            adf = &ad->ad_data_fork;
            lock.l_start = rf2off(off);
        } else {
            adf = ad->ad_rfp;
            lock.l_start = off + ad_getentryoff(ad, ADEID_RFORK);
        }
    }

    /* NOTE: we can't write lock a read-only file. on those, we just
     * make sure that we have a read lock set. that way, we at least prevent
     * someone else from really setting a deny read/write on the file. 
     */
    if (!(adf->adf_flags & O_RDWR) && (type & ADLOCK_WR)) {
        type = (type & ~ADLOCK_WR) | ADLOCK_RD;
    }
  
    lock.l_type = XLATE_FCNTL_LOCK(type & ADLOCK_MASK);
    lock.l_whence = SEEK_SET;
    lock.l_len = len;

    /* byte_lock(len=-1) lock whole file */
    if (len == BYTELOCK_MAX) {
        lock.l_len -= lock.l_start; /* otherwise  EOVERFLOW error */
    }

    /* see if it's locked by another fork. 
     * NOTE: this guarantees that any existing locks must be at most
     * read locks. we use ADLOCK_WR/RD because F_RD/WRLCK aren't
     * guaranteed to be ORable. */
    if (adf_findxlock(adf, fork, ADLOCK_WR | 
                      ((type & ADLOCK_WR) ? ADLOCK_RD : 0), 
                      lock.l_start, lock.l_len) > -1) {
        errno = EACCES;
        ret = -1;
        goto exit;
    }
  
    /* look for any existing lock that we may have */
    i = adf_findlock(adf, fork, ADLOCK_RD | ADLOCK_WR, lock.l_start, lock.l_len);
    adflock = (i < 0) ? NULL : adf->adf_lock + i;

    /* here's what we check for:
       1) we're trying to re-lock a lock, but we didn't specify an update.
       2) we're trying to free only part of a lock. 
       3) we're trying to free a non-existent lock. */
    if ( (!adflock && (lock.l_type == F_UNLCK))
         ||
         (adflock
          && !(type & ADLOCK_UPGRADE)
          && ((lock.l_type != F_UNLCK)
              || (adflock->lock.l_start != lock.l_start)
              || (adflock->lock.l_len != lock.l_len) ))
        ) {
        errno = EINVAL;
        ret = -1;
        goto exit;
    }


    /* now, update our list of locks */
    /* clear the lock */
    if (lock.l_type == F_UNLCK) { 
        adf_freelock(adf, i);
        goto exit;
    }

    /* attempt to lock the file. */
    if (set_lock(adf->adf_fd, F_SETLK, &lock) < 0) {
        ret = -1;
        goto exit;
    }

    /* we upgraded this lock. */
    if (adflock && (type & ADLOCK_UPGRADE)) {
        memcpy(&adflock->lock, &lock, sizeof(lock));
        goto exit;
    } 

    /* it wasn't an upgrade */
    oldlock = -1;
    if (lock.l_type == F_RDLCK) {
        oldlock = adf_findxlock(adf, fork, ADLOCK_RD, lock.l_start, lock.l_len);
    } 
    
    /* no more space. this will also happen if lockmax == lockcount == 0 */
    if (adf->adf_lockmax == adf->adf_lockcount) { 
        adf_lock_t *tmp = (adf_lock_t *) 
            realloc(adf->adf_lock, sizeof(adf_lock_t)*
                    (adf->adf_lockmax + ARRAY_BLOCK_SIZE));
        if (!tmp) {
            ret = fcntl_lock_err = -1;
            goto exit;
        }
        adf->adf_lock = tmp;
        adf->adf_lockmax += ARRAY_BLOCK_SIZE;
    } 
    adflock = adf->adf_lock + adf->adf_lockcount;

    /* fill in fields */
    memcpy(&adflock->lock, &lock, sizeof(lock));
    adflock->user = fork;
    if (oldlock > -1) {
        adflock->refcount = (adf->adf_lock + oldlock)->refcount;
    } else if ((adflock->refcount = calloc(1, sizeof(int))) == NULL) {
        ret = fcntl_lock_err = 1;
        goto exit;
    }
  
    (*adflock->refcount)++;
    adf->adf_lockcount++;

exit:
    if (ret != 0) {
        if (fcntl_lock_err != 0) {
            lock.l_type = F_UNLCK;
            set_lock(adf->adf_fd, F_SETLK, &lock);
        }
    }
    LOG(log_debug, logtype_ad, "ad_lock: END: %d", ret);
    return ret;
}

int ad_tmplock(struct adouble *ad, uint32_t eid, int locktype, off_t off, off_t len, int fork)
{
    struct flock lock;
    struct ad_fd *adf;
    int err;
    int type;  

    LOG(log_debug, logtype_ad, "ad_tmplock(%s, %s, off: %jd (%s), len: %jd): BEGIN",
        eid == ADEID_DFORK ? "data" : "reso",
        locktypetostr(locktype),
        (intmax_t)off,
        shmdstrfromoff(off),
        (intmax_t)len);

    lock.l_start = off;
    type = locktype;

    if (eid == ADEID_DFORK) {
        adf = &ad->ad_data_fork;
    } else {
        adf = &ad->ad_resource_fork;
        if (adf->adf_fd == -1) {
            /* there's no resource fork. return success */
            err = 0;
            goto exit;
        }
        /* if ADLOCK_FILELOCK we want a lock from offset 0
         * it's used when deleting a file:
         * in open we put read locks on meta datas
         * in delete a write locks on the whole file
         * so if the file is open by somebody else it fails
         */
        if (!(type & ADLOCK_FILELOCK))
            lock.l_start += ad_getentryoff(ad, eid);
    }

    if (!(adf->adf_flags & O_RDWR) && (type & ADLOCK_WR)) {
        type = (type & ~ADLOCK_WR) | ADLOCK_RD;
    }
  
    lock.l_type = XLATE_FCNTL_LOCK(type & ADLOCK_MASK);
    lock.l_whence = SEEK_SET;
    lock.l_len = len;

    /* see if it's locked by another fork. */
    if (fork && adf_findxlock(adf, fork,
                              ADLOCK_WR | ((type & ADLOCK_WR) ? ADLOCK_RD : 0), 
                              lock.l_start, lock.l_len) > -1) {
        errno = EACCES;
        err = -1;
        goto exit;
    }

    /* okay, we might have ranges byte-locked. we need to make sure that
     * we restore the appropriate ranges once we're done. so, we check
     * for overlap on an unlock and relock. 
     * XXX: in the future, all the byte locks will be sorted and contiguous.
     *      we just want to upgrade all the locks and then downgrade them
     *      here. */
    err = set_lock(adf->adf_fd, F_SETLK, &lock);
    if (!err && (lock.l_type == F_UNLCK))
        adf_relockrange(adf, adf->adf_fd, lock.l_start, len);

exit:
    LOG(log_debug, logtype_ad, "ad_tmplock: END: %d", err);
    return err;
}

/* --------------------- */
void ad_unlock(struct adouble *ad, const int fork, int unlckbrl)
{
    LOG(log_debug, logtype_ad, "ad_unlock(unlckbrl: %d): BEGIN", unlckbrl);

    if (ad_data_fileno(ad) != -1) {
        adf_unlock(ad, &ad->ad_data_fork, fork, unlckbrl);
    }
    if (ad_reso_fileno(ad) != -1) {
        adf_unlock(ad, &ad->ad_resource_fork, fork, unlckbrl);
    }

    LOG(log_debug, logtype_ad, "ad_unlock: END");
}

/*!
 * Test for a share mode lock
 *
 * @param ad      (rw) handle
 * @param eid     (r)  datafork or ressource fork
 * @param off     (r)  sharemode lock to test
 *
 * @returns           1 if there's an existing lock, 0 if there's no lock,
 *                    -1 in case any error occured
 */
int ad_testlock(struct adouble *ad, int eid, const off_t off)
{
    int ret = 0;
    off_t lock_offset;

    LOG(log_debug, logtype_ad, "ad_testlock(%s, off: %jd (%s): BEGIN",
        eid == ADEID_DFORK ? "data" : "reso",
        (intmax_t)off,
        shmdstrfromoff(off));

    if (eid == ADEID_DFORK) {
        lock_offset = off;
    } else { /* rfork */
        lock_offset = rf2off(off);
    }

    ret = testlock(&ad->ad_data_fork, lock_offset, 1);

    LOG(log_debug, logtype_ad, "ad_testlock: END: %d", ret);
    return ret;
}

/*!
 * Return if a file is open by another process.
 *
 * Optimized for the common case:
 * - there's no locks held by another process (clients)
 * - or we already know the answer and don't need to test (attrbits)
 *
 * @param ad          (rw) handle
 * @param attrbits    (r)  forks opened by us
 * @returns                bitflags ATTRBIT_DOPEN | ATTRBIT_ROPEN if
 *                         other process has fork of file opened 
 */
uint16_t ad_openforks(struct adouble *ad, uint16_t attrbits)
{
    uint16_t ret = 0;
    off_t off;
    off_t len;

    if (ad_data_fileno(ad) == -1)
        return 0;

    if (!(attrbits & (ATTRBIT_DOPEN | ATTRBIT_ROPEN))) {
        /* Test all 4 locks at once */
        off = AD_FILELOCK_OPEN_WR;
        len = 4;
        if (testlock(&ad->ad_data_fork, off, len) == 0)
            return 0;
    }

    /* either there's a lock or we already know one fork is open */

    if (!(attrbits & ATTRBIT_DOPEN)) {
        off = AD_FILELOCK_OPEN_WR;
        ret = testlock(&ad->ad_data_fork, off, 2) > 0 ? ATTRBIT_DOPEN : 0;
    }

    if (!(attrbits & ATTRBIT_ROPEN)) {
        off = AD_FILELOCK_RSRC_OPEN_WR;
        ret |= testlock(&ad->ad_data_fork, off, 2) > 0? ATTRBIT_ROPEN : 0;
    }

    return ret;
}
