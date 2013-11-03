/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>  /* to pick up NULL */
#include <sys/stat.h> /* works around a bug */
#include <atalk/logger.h>

#include <atalk/afp.h>
#include <atalk/uam.h>
#include <atalk/globals.h>

/* grab the FP functions */
#include "auth.h" 
#include "desktop.h"
#include "switch.h"
#include "fork.h"
#include "file.h"
#include "directory.h"
#include "filedir.h"
#include "status.h"
#include "misc.h"
#ifdef HAVE_ACLS
#include "acls.h"
#endif

static int afp_null(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_,  size_t *rbuflen)
{
    LOG(log_info, logtype_afpd, "afp_null handle %d", *ibuf );
    *rbuflen = 0;
    return( AFPERR_NOOP );
}

/*
 * Routines marked "NULL" are not AFP functions.
 * Routines marked "afp_null" are AFP functions
 * which are not yet implemented. A fine line...
 */
static AFPCmd preauth_switch[] = {
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*   0 -   7 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*   8 -  15 */
    NULL, NULL, afp_login, afp_logincont,
    afp_logout, NULL, NULL, NULL,				/*  16 -  23 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  24 -  31 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  32 -  39 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  40 -  47 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  48 -  55 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, afp_login_ext,				/*  56 -  63 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  64 -  71 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  72 -  79 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  80 -  87 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  88 -  95 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  96 - 103 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 104 - 111 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 112 - 119 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 120 - 127 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 128 - 135 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 136 - 143 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 144 - 151 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 152 - 159 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 160 - 167 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 168 - 175 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 176 - 183 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 184 - 191 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 192 - 199 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 200 - 207 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 208 - 215 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 216 - 223 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 224 - 231 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 232 - 239 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 240 - 247 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 248 - 255 */
};

AFPCmd *afp_switch = preauth_switch;

AFPCmd postauth_switch[] = {
    NULL, afp_bytelock, afp_closevol, afp_closedir,
    afp_closefork, afp_copyfile, afp_createdir, afp_createfile,	/*   0 -   7 */
    afp_delete, afp_enumerate, afp_flush, afp_flushfork,
    afp_null, afp_null, afp_getforkparams, afp_getsrvrinfo,	/*   8 -  15 */
    afp_getsrvrparms, afp_getvolparams, afp_login, afp_logincont,
    afp_logout, afp_mapid, afp_mapname, afp_moveandrename,	/*  16 -  23 */
    afp_openvol, afp_opendir, afp_openfork, afp_read,
    afp_rename, afp_setdirparams, afp_setfilparams, afp_setforkparams,
    /*  24 -  31 */
    afp_setvolparams, afp_write, afp_getfildirparams, afp_setfildirparams,
    afp_changepw, afp_getuserinfo, afp_getsrvrmesg, afp_createid, /*  32 -  39 */
    afp_deleteid, afp_resolveid, afp_exchangefiles, afp_catsearch,
    afp_null, afp_null, afp_null, afp_null,			/*  40 -  47 */
    afp_opendt, afp_closedt, afp_null, afp_geticon,
    afp_geticoninfo, afp_addappl, afp_rmvappl, afp_getappl,	/*  48 -  55 */
    afp_addcomment, afp_rmvcomment, afp_getcomment, NULL,
    NULL, NULL, NULL, NULL,					/*  56 -  63 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  64 -  71 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, afp_syncdir, afp_syncfork,	/*  72 -  79 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  80 -  87 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/*  88 -  95 */
    NULL, NULL, NULL, NULL,
    afp_getdiracl, afp_setdiracl, afp_afschangepw, NULL,	/*  96 - 103 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 104 - 111 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 112 - 119 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 120 - 127 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 128 - 135 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 136 - 143 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 144 - 151 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 152 - 159 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 160 - 167 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 168 - 175 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 176 - 183 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 184 - 191 */
    afp_addicon, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 192 - 199 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 200 - 207 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 208 - 215 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 216 - 223 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 224 - 231 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 232 - 239 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 240 - 247 */
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,					/* 248 - 255 */
};


/* add a new function if it's specified. return the old function in
 * "old" if there's a pointer there. */
int uam_afpserver_action(const int id, const int which,
                         AFPCmd new_table, AFPCmd *old)
{
    switch (which) {
    case UAM_AFPSERVER_PREAUTH:
        if (old)
            *old = preauth_switch[id];
        if (new_table)
            preauth_switch[id] = new_table;
        break;
    case UAM_AFPSERVER_POSTAUTH:
        if (old)
            *old = postauth_switch[id];
        if (new_table)
            postauth_switch[id] = new_table;
        break;
    default:
        LOG(log_debug, logtype_afpd, "uam_afpserver_action: unknown switch %d[%d]",
            which, id);
        return -1;
    }

    return 0;
}
