/*
 *
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2009
 * All Rights Reserved.  See COPYING.
 */

/* 
CNID salvation spec:
general rule: better safe then sorry, so we always delete CNIDs and assign
new ones in case of a lookup mismatch. afpd also sends us the CNID found
in the adouble file. In certain cases we can use this hint to determince
the right CNID.


The lines...

Id  Did T   Dev Inode   Name
============================
a   b   c   d   e       name1
-->
f   g   h   i   h       name2

...are the expected results of certain operations. (f) is the speced CNID, in some
cases it's only intermediate as described in the text and is overridden by another
spec.

1) UNIX rename (via mv) or inode reusage(!)
-------------------------------------------
Name is possibly changed (rename case) but inode is the same.
We should try to keep the CNID, but we cant, because inode reusage is probably
much to frequent.

rename:
15  2   f   1   1       file
-->
15  x   f   1   1       renamedfile

inode reusage:
15  2   f   1   1       file
-->
16  y   f   1   1       inodereusagefile

Result in dbd_lookup (-: not found, +: found):
+ devino
- didname

Possible solution:
None. Delete old data, file gets new CNID in both cases (rename and inode).
If we got a hint and hint matches the CNID from devino we keep it and update
the record.

2) UNIX mv from one folder to another
----------------------------------------
Name is unchanged and inode stays the same, but DID is different.
We should try to keep the CNID.

15  2   f   1   1       file
-->
15  x   f   1   1       file

Result in dbd_lookup:
+ devino
- didname

Possible solution:
strcmp names, if they match keep CNID. Unfortunately this also can't be
distinguished from a new file with a reused inode. So me must assign
a new CNID.
If we got a hint and hint matches the CNID from devino we keep it and update
the record.

3) Restore from backup ie change of inode number -- or emacs
------------------------------------------------------------

15  2   f   1   1       file
-->
15  2   f   1   2       file

Result in dbd_lookup:
- devino
+ didname

Possible fixup solution:
test-suite test235 tests and ensures that the CNID is _changed_. The reason for
this is somewhat lost in time, but nevertheless we believe our test suite.

Similar things happen with emas: emacs uses a backup file (file~). When saving
because of inode reusage of the fs, both files most likely exchange inodes.

15  2   f   1   1       file
16  2   f   1   2       file~
--> this would be nice:
15  2   f   1   2       file
16  2   f   1   1       file~
--> but for the reasons described above we must implement
17  2   f   1   2       file
18  2   f   1   1       file~

Result in dbd_lookup for the emacs case:
+ devino --> CNID: 16
+ didname -> CNID: 15
devino search and didname search result in different CNIDs !!

Possible fixup solution:
to be safe we must assign new CNIDs to both files.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */


#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/logger.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/cnid.h>

#include "pack.h"
#include "dbif.h"
#include "dbd.h"

/*
 *  This returns the CNID corresponding to a particular file.  It will also fix
 *  up the database if there's a problem.
 */

int dbd_lookup(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    unsigned char *buf;
    DBT key, devdata, diddata;
    int devino = 1, didname = 1; 
    int rc;
    cnid_t id_devino, id_didname;
    u_int32_t type_devino  = (unsigned)-1;
    u_int32_t type_didname = (unsigned)-1;
    int update = 0;
    
    memset(&key, 0, sizeof(key));
    memset(&diddata, 0, sizeof(diddata));
    memset(&devdata, 0, sizeof(devdata));

    rply->namelen = 0;
    rply->cnid = 0;

    LOG(log_maxdebug, logtype_cnid, "dbd_lookup(): START");
    
    buf = pack_cnid_data(rqst); 

    /* Look for a CNID.  We have two options: dev/ino or did/name.  If we
       only get a match in one of them, that means a file has moved. */
    key.data = buf + CNID_DEVINO_OFS;
    key.size = CNID_DEVINO_LEN;

    if ((rc = dbif_get(dbd, DBIF_IDX_DEVINO, &key, &devdata, 0))  < 0) {
        LOG(log_error, logtype_cnid, "dbd_lookup: Unable to get CNID %u, name %s",
            ntohl(rqst->did), rqst->name);
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }
    if (rc == 0) {
        devino = 0;
    }
    else {
        memcpy(&id_devino, devdata.data, sizeof(rply->cnid));
        memcpy(&type_devino, (char *)devdata.data +CNID_TYPE_OFS, sizeof(type_devino));
        type_devino = ntohl(type_devino);
    }

    key.data = buf + CNID_DID_OFS;
    key.size = CNID_DID_LEN + rqst->namelen + 1;

    if ((rc = dbif_get(dbd, DBIF_IDX_DIDNAME, &key, &diddata, 0))  < 0) {
        LOG(log_error, logtype_cnid, "dbd_lookup: Unable to get CNID %u, name %s",
            ntohl(rqst->did), rqst->name);
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }
    if (rc == 0) {
        didname = 0;
    }
    else {
        memcpy(&id_didname, diddata.data, sizeof(rply->cnid));
        memcpy(&type_didname, (char *)diddata.data +CNID_TYPE_OFS, sizeof(type_didname));
        type_didname = ntohl(type_didname);
    }

    LOG(log_maxdebug, logtype_cnid, "dbd_lookup(name:'%s', did:%u, dev/ino:0x%llx/0x%llx) {devino: %u, didname: %u}", 
        rqst->name, ntohl(rqst->did), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, devino, didname);

    /* Have we found anything at all ? */
    if (!devino && !didname) {  
        /* nothing found */
        LOG(log_debug, logtype_cnid, "dbd_lookup: name: '%s', did: %u, dev/ino: 0x%llx/0x%llx is not in the CNID database", 
            rqst->name, ntohl(rqst->did), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
        rply->result = CNID_DBD_RES_NOTFOUND;
        return 1;
    }

    /* Check for type (file/dir) mismatch */
    if ((devino && (type_devino != rqst->type)) || (didname && (type_didname != rqst->type))) {

        if (devino && (type_devino != rqst->type)) {
            /* one is a dir one is a file, remove from db */

            LOG(log_debug, logtype_cnid, "dbd_lookup(name:'%s', did:%u, dev/ino:0x%llx/0x%llx): type mismatch for devino", 
                rqst->name, ntohl(rqst->did), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);

            rqst->cnid = id_devino;
            rc = dbd_delete(dbd, rqst, rply, DBIF_CNID);
            rc += dbd_delete(dbd, rqst, rply, DBIF_IDX_DEVINO);
            rc += dbd_delete(dbd, rqst, rply, DBIF_IDX_DIDNAME);
            if (rc < 0) {
                LOG(log_error, logtype_cnid, "dbd_lookup(name:'%s', did:%u, dev/ino:0x%llx/0x%llx): error deleting type mismatch for devino", 
                    rqst->name, ntohl(rqst->did), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
                return -1;
            }
        }

        if (didname && (type_didname != rqst->type)) {
            /* same: one is a dir one is a file, remove from db */

            LOG(log_debug, logtype_cnid, "dbd_lookup(name:'%s', did:%u, dev/ino:0x%llx/0x%llx): type mismatch for didname", 
                rqst->name, ntohl(rqst->did), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);

            rqst->cnid = id_didname;
            rc = dbd_delete(dbd, rqst, rply, DBIF_CNID);
            rc += dbd_delete(dbd, rqst, rply, DBIF_IDX_DEVINO);
            rc += dbd_delete(dbd, rqst, rply, DBIF_IDX_DIDNAME);
            if (rc < 0) {
                LOG(log_error, logtype_cnid, "dbd_lookup(name:'%s', did:%u, dev/ino:0x%llx/0x%llx): error deleting type mismatch for didname", 
                    rqst->name, ntohl(rqst->did), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
                return -1;
            }
        }

        rply->result = CNID_DBD_RES_NOTFOUND;
        return 1;
    }

    if (devino && didname && id_devino == id_didname) {
        /* everything is fine */
        LOG(log_debug, logtype_cnid, "dbd_lookup(DID:%u/'%s',0x%llx/0x%llx): Got CNID: %u",
            ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, htonl(id_didname));
        rply->cnid = id_didname;
        rply->result = CNID_DBD_RES_OK;
        return 1;
    }

    if (devino && didname && id_devino != id_didname) {
        /* CNIDs don't match, something of a worst case, or possibly 3) emacs! */
        LOG(log_debug, logtype_cnid, "dbd_lookup: CNID mismatch: (DID:%u/'%s') --> %u , (0x%llx/0x%llx) --> %u",
            ntohl(rqst->did), rqst->name, ntohl(id_didname),
            (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, ntohl(id_devino));

        rqst->cnid = id_devino;
        if (dbd_delete(dbd, rqst, rply, DBIF_CNID) < 0)
            return -1;

        rqst->cnid = id_didname;
        if (dbd_delete(dbd, rqst, rply, DBIF_CNID) < 0)
            return -1;

        rply->result = CNID_DBD_RES_NOTFOUND;
        return 1;
    }

    if ( ! didname) {
        LOG(log_debug, logtype_cnid, "dbd_lookup(CNID hint: %u, DID:%u, \"%s\", 0x%llx/0x%llx): CNID resolve problem: server side rename oder reused inode",
            ntohl(rqst->cnid), ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
        if (rqst->cnid == id_devino) {
            LOG(log_debug, logtype_cnid, "dbd_lookup: server side mv (with resource fork)");
            update = 1;
        } else {
            rqst->cnid = id_devino;
            if (dbd_delete(dbd, rqst, rply, DBIF_CNID) < 0)
                return -1;
            rply->result = CNID_DBD_RES_NOTFOUND;
            rqst->cnid = CNID_INVALID; /* invalidate CNID hint */
            return 1;
        }
    }

    if ( ! devino) {
        LOG(log_debug, logtype_cnid, "dbd_lookup(DID:%u/'%s',0x%llx/0x%llx): CNID resolve problem: changed dev/ino",
            ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
        rqst->cnid = id_didname;
        if (dbd_delete(dbd, rqst, rply, DBIF_CNID) < 0)
            return -1;
        rply->result = CNID_DBD_RES_NOTFOUND;
        rqst->cnid = CNID_INVALID; /* invalidate CNID hint */
        return 1;
    }

    /* This is also a catch all if we've forgot to catch some possibility with the preceding ifs*/
    if (!update) {
        rply->result = CNID_DBD_RES_NOTFOUND;
        return 1;
    }

    /* Fix up the database */
    rc = dbd_update(dbd, rqst, rply);
    if (rc >0) {
        rply->cnid = rqst->cnid;
    }

    LOG(log_debug, logtype_cnid, "dbd_lookup(DID:%u/'%s',0x%llx/0x%llx): Got CNID (needed update): %u", 
        ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, ntohl(rply->cnid));

    return rc;
}
