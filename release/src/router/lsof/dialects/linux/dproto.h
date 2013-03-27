/*
 * dproto.h - Linux function prototypes for /proc-based lsof
 *
 * The _PROTOTYPE macro is defined in the common proto.h.
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * $Id: dproto.h,v 1.7 2008/04/15 13:32:26 abe Exp $
 */


#if	defined(HASSELINUX)
_PROTOTYPE(extern int enter_cntx_arg,(char *cnxt));
#endif	/* defined(HASSELINUX) */

_PROTOTYPE(extern int get_fields,(char *ln, char *sep, char ***fr, int *eb, int en));
_PROTOTYPE(extern void get_locks,(char *p));
_PROTOTYPE(extern int is_file_named,(char *p, int cd));
_PROTOTYPE(extern int make_proc_path,(char *pp, int lp, char **np, int *npl, char *sf));
_PROTOTYPE(extern FILE *open_proc_stream,(char *p, char *mode, char **buf, size_t *sz, int act));
_PROTOTYPE(extern void process_proc_node,(char *p, struct stat *s, int ss, struct stat *l, int ls));
_PROTOTYPE(extern void process_proc_sock,(char *p, struct stat *s, int ss, struct stat *l, int ls));
_PROTOTYPE(extern void set_net_paths,(char *p, int pl));
