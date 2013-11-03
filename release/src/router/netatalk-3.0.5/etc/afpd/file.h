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

#ifndef AFPD_FILE_H 
#define AFPD_FILE_H 1

/*#include <sys/stat.h>*/ /* including it here causes some confusion */
#include <sys/param.h>
#include <arpa/inet.h>
#include <atalk/adouble.h>
#include <atalk/globals.h>

#include "volume.h"
#include "directory.h"

extern const u_char	ufinderi[];

#define FILPBIT_ATTR	 0
#define FILPBIT_PDID	 1
#define FILPBIT_CDATE	 2
#define FILPBIT_MDATE	 3
#define FILPBIT_BDATE	 4
#define FILPBIT_FINFO	 5
#define FILPBIT_LNAME	 6
#define FILPBIT_SNAME	 7
#define FILPBIT_FNUM	 8
#define FILPBIT_DFLEN	 9
#define FILPBIT_RFLEN	 10
#define FILPBIT_EXTDFLEN 11
#define FILPBIT_PDINFO   13    /* ProDOS Info/ UTF8 name */
#define FILPBIT_EXTRFLEN 14
#define FILPBIT_UNIXPR   15

#define kTextEncodingUTF8 0x08000103

typedef enum {
                                        /* Mac OS encodings*/
  kTextEncodingMacRoman         = 0L,
  kTextEncodingMacJapanese      = 1,
  kTextEncodingMacChineseTrad   = 2,
  kTextEncodingMacKorean        = 3,
  kTextEncodingMacArabic        = 4,
  kTextEncodingMacHebrew        = 5,
  kTextEncodingMacGreek         = 6,
  kTextEncodingMacCyrillic      = 7,
  kTextEncodingMacDevanagari    = 9,
  kTextEncodingMacGurmukhi      = 10,
  kTextEncodingMacGujarati      = 11,
  kTextEncodingMacOriya         = 12,
  kTextEncodingMacBengali       = 13,
  kTextEncodingMacTamil         = 14,
  kTextEncodingMacTelugu        = 15,
  kTextEncodingMacKannada       = 16,
  kTextEncodingMacMalayalam     = 17,
  kTextEncodingMacSinhalese     = 18,
  kTextEncodingMacBurmese       = 19,
  kTextEncodingMacKhmer         = 20,
  kTextEncodingMacThai          = 21,
  kTextEncodingMacLaotian       = 22,
  kTextEncodingMacGeorgian      = 23,
  kTextEncodingMacArmenian      = 24,
  kTextEncodingMacChineseSimp   = 25,
  kTextEncodingMacTibetan       = 26,
  kTextEncodingMacMongolian     = 27,
  kTextEncodingMacEthiopic      = 28,
  kTextEncodingMacCentralEurRoman = 29,
  kTextEncodingMacVietnamese    = 30,
  kTextEncodingMacExtArabic     = 31,   /* The following use script code 0, smRoman*/
  kTextEncodingMacSymbol        = 33,
  kTextEncodingMacDingbats      = 34,
  kTextEncodingMacTurkish       = 35,
  kTextEncodingMacCroatian      = 36,
  kTextEncodingMacIcelandic     = 37,
  kTextEncodingMacRomanian      = 38,
  kTextEncodingMacCeltic        = 39,
  kTextEncodingMacGaelic        = 40,
  kTextEncodingMacKeyboardGlyphs = 41,
} kTextEncoding_t;

extern char *set_name   (const struct vol *, char *, cnid_t, char *, cnid_t, uint32_t );

extern struct extmap	*getextmap (const char *);
extern struct extmap	*getdefextmap (void);

extern int getfilparams (const AFPObj *obj, struct vol *, uint16_t, struct path *,
                         struct dir *, char *buf, size_t *, int);
extern int setfilparams (const AFPObj *obj, struct vol *, struct path *, uint16_t, char *);
extern int renamefile   (struct vol *, struct dir *, int, char *, char *, char *, struct adouble *);
extern int copyfile     (struct vol *, struct vol *, struct dir *, int, char *, char *, char *, struct adouble *);
extern int deletefile   (const struct vol *, int, char *, int);

extern int getmetadata  (const AFPObj *obj, struct vol *vol, uint16_t bitmap, struct path *path, 
                         struct dir *dir, char *buf, size_t *buflen, struct adouble *adp);

extern void *get_finderinfo (const struct vol *, const char *, struct adouble *, void *, int);

extern size_t mtoUTF8   (const struct vol *, const char *, size_t , char *, size_t );
extern int  copy_path_name (const struct vol *, char *, char *i);

extern uint32_t get_id  (struct vol *,
                         struct adouble *,
                         const struct stat *,
                         cnid_t ,
                         const char *,
                         int );

/* FP functions */
int afp_exchangefiles (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_setfilparams (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_copyfile (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_createfile (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_createid (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_resolveid (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_deleteid (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

#endif
