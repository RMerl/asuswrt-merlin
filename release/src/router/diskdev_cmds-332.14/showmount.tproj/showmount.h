#ifndef __SHOWMOUNT_H
#define __SHOWMOUNT_H

/* Constant defs */
#define ALL     1
#define DIRS    2

#define DODUMP          0x1
#define DOEXPORTS       0x2

struct mountlist {
        struct mountlist *ml_left;
        struct mountlist *ml_right;
        char    ml_host[RPCMNT_NAMELEN+1];
        char    ml_dirp[RPCMNT_PATHLEN+1];
};

struct grouplist {
        struct grouplist *gr_next;
        char    gr_name[RPCMNT_NAMELEN+1];
};

struct exportslist {
        struct exportslist *ex_next;
        struct grouplist *ex_groups;
        char    ex_dirp[RPCMNT_PATHLEN+1];
};

#endif /* __SHOWMOUNT_H */
