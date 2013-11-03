/*
 *  Interface to the cnid_dbd daemon that stores/retrieves CNIDs from a database.
 */


#ifndef _ATALK_CNID_DBD_PRIVATE_H
#define _ATALK_CNID_DBD_PRIVATE_H 1

#include <sys/stat.h>
#include <atalk/adouble.h>
#include <sys/param.h>

#include <atalk/cnid_private.h>

#define CNID_DBD_OP_OPEN        0x01
#define CNID_DBD_OP_CLOSE       0x02
#define CNID_DBD_OP_ADD         0x03
#define CNID_DBD_OP_GET         0x04
#define CNID_DBD_OP_RESOLVE     0x05
#define CNID_DBD_OP_LOOKUP      0x06
#define CNID_DBD_OP_UPDATE      0x07
#define CNID_DBD_OP_DELETE      0x08
#define CNID_DBD_OP_MANGLE_ADD  0x09
#define CNID_DBD_OP_MANGLE_GET  0x0a
#define CNID_DBD_OP_GETSTAMP    0x0b
#define CNID_DBD_OP_REBUILD_ADD 0x0c
#define CNID_DBD_OP_SEARCH      0x0d
#define CNID_DBD_OP_WIPE        0x0e

#define CNID_DBD_RES_OK            0x00
#define CNID_DBD_RES_NOTFOUND      0x01
#define CNID_DBD_RES_ERR_DB        0x02
#define CNID_DBD_RES_ERR_MAX       0x03
#define CNID_DBD_RES_ERR_DUPLCNID  0x04
#define CNID_DBD_RES_SRCH_CNT      0x05
#define CNID_DBD_RES_SRCH_DONE     0x06

#define DBD_MAX_SRCH_RSLTS 100

struct cnid_dbd_rqst {
    int     op;
    cnid_t  cnid;
    dev_t   dev;
    ino_t   ino;
    uint32_t type;
    cnid_t  did;
    const char *name;
    size_t  namelen;
};

struct cnid_dbd_rply {
    int     result;    
    cnid_t  cnid;
    cnid_t  did;
    char    *name;
    size_t  namelen;
};

typedef struct CNID_private {
    uint32_t magic;
    char      db_dir[MAXPATHLEN + 1]; /* Database directory without /.AppleDB appended */
    char      *cnidserver;
    char      *cnidport;
    int       fd;		/* File descriptor to cnid_dbd */
    char      stamp[ADEDLEN_PRIVSYN]; /* db timestamp */
    char      *client_stamp;
    size_t    stamp_size;
    int       notfirst;   /* already open before */
    int       changed;  /* stamp differ */
} CNID_private;


#endif /* include/atalk/cnid_dbd.h */
