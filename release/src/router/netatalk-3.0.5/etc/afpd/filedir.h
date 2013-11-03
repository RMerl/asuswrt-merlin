#ifndef AFPD_FILEDIR_H
#define AFPD_FILEDIR_H 1

#include <sys/stat.h>
#include <atalk/globals.h>
#include "volume.h"

extern struct afp_options default_options;

extern char		*ctoupath (const struct vol *, struct dir *,
                                char *);
extern char		*absupath (const struct vol *, struct dir *,
                                char *);
extern int		veto_file (const char *veto_str, const char *path);
extern int 		check_name (const struct vol *vol, char *name);

extern int matchfile2dirperms (char *, struct vol *, int);
extern int delete_vetoed_files(struct vol *vol, const char *upath, bool in_vetodir);

/* FP functions */
int afp_moveandrename (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_rename (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_delete (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_getfildirparams (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_setfildirparams (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

#endif
