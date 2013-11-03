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

#ifndef _ATALK_AFP_H
#define _ATALK_AFP_H 1

#include <stdint.h>
#include <sys/types.h>

typedef uint16_t AFPUserBytes;

/* protocols */
#define AFPPROTO_ASP           1
#define AFPPROTO_DSI           2

/* server flags */
#define AFPSRVRINFO_COPY         (1<<0)  /* supports copyfile */
#define AFPSRVRINFO_PASSWD       (1<<1)  /* supports change password */
#define AFPSRVRINFO_NOSAVEPASSWD (1<<2)  /* don't allow save password */
#define AFPSRVRINFO_SRVMSGS      (1<<3)  /* supports server messages */
#define AFPSRVRINFO_SRVSIGNATURE (1<<4)  /* supports server signature */
#define AFPSRVRINFO_TCPIP        (1<<5)  /* supports tcpip */
#define AFPSRVRINFO_SRVNOTIFY    (1<<6)  /* supports server notifications */ 
#define AFPSRVRINFO_SRVRECONNECT (1<<7)  /* supports server reconnect */ 
#define AFPSRVRINFO_SRVRDIR      (1<<8)  /* supports directories service */ 
#define AFPSRVRINFO_SRVUTF8      (1<<9)  /* supports UTF8 names AFP 3.1 */ 
#define AFPSRVRINFO_UUID         (1<<10) /* supports UUIDs */
#define AFPSRVRINFO_EXTSLEEP     (1<<11) /* supports extended sleep */
#define AFPSRVRINFO_FASTBOZO     (1<<15) /* fast copying */

#define AFP_OK		0
#define AFPERR_DID1     -4000   /* not an afp error DID is 1*/
#define AFPERR_ACCESS	-5000   /* permission denied */
#define AFPERR_AUTHCONT	-5001   /* logincont */
#define AFPERR_BADUAM	-5002   /* uam doesn't exist */
#define AFPERR_BADVERS	-5003   /* bad afp version number */
#define AFPERR_BITMAP	-5004   /* invalid bitmap */
#define AFPERR_CANTMOVE -5005   /* can't move file */
#define AFPERR_DENYCONF	-5006   /* file synchronization locks conflict */
#define AFPERR_DIRNEMPT	-5007   /* directory not empty */
#define AFPERR_DFULL	-5008   /* disk full */
#define AFPERR_EOF	-5009   /* end of file -- catsearch and afp_read */
#define AFPERR_BUSY	-5010   /* FileBusy */
#define AFPERR_FLATVOL  -5011   /* volume doesn't support directories */
#define AFPERR_NOITEM	-5012   /* ItemNotFound */
#define AFPERR_LOCK     -5013   /* LockErr */
#define AFPERR_MISC     -5014   /* misc. err */
#define AFPERR_NLOCK    -5015   /* no more locks */
#define AFPERR_NOSRVR	-5016   /* no response by server at that address */
#define AFPERR_EXIST	-5017   /* object already exists */
#define AFPERR_NOOBJ	-5018   /* object not found */
#define AFPERR_PARAM	-5019   /* parameter error */
#define AFPERR_NORANGE  -5020   /* no range lock */
#define AFPERR_RANGEOVR -5021   /* range overlap */
#define AFPERR_SESSCLOS -5022   /* session closed */
#define AFPERR_NOTAUTH	-5023   /* user not authenticated */
#define AFPERR_NOOP	-5024   /* command not supported */
#define AFPERR_BADTYPE	-5025   /* object is the wrong type */
#define AFPERR_NFILE	-5026   /* too many files open */
#define AFPERR_SHUTDOWN	-5027   /* server is going down */
#define AFPERR_NORENAME -5028   /* can't rename */
#define AFPERR_NODIR	-5029   /* couldn't find directory */
#define AFPERR_ITYPE	-5030   /* wrong icon type */
#define AFPERR_VLOCK	-5031   /* volume locked */
#define AFPERR_OLOCK    -5032   /* object locked */
#define AFPERR_CTNSHRD  -5033   /* share point contains a share point */
#define AFPERR_NOID     -5034   /* file thread not found */
#define AFPERR_EXISTID  -5035   /* file already has an id */
#define AFPERR_DIFFVOL  -5036   /* different volume */
#define AFPERR_CATCHNG  -5037   /* catalog has changed */
#define AFPERR_SAMEOBJ  -5038   /* source file == destination file */
#define AFPERR_BADID    -5039   /* non-existent file id */
#define AFPERR_PWDSAME  -5040   /* same password/can't change password */
#define AFPERR_PWDSHORT -5041   /* password too short */
#define AFPERR_PWDEXPR  -5042   /* password expired */
#define AFPERR_INSHRD   -5043   /* folder being shared is inside a
				   shared folder. may be returned by
				   afpMoveAndRename. */
#define AFPERR_INTRASH  -5044   /* shared folder in trash. */
#define AFPERR_PWDCHNG  -5045   /* password needs to be changed */
#define AFPERR_PWDPOLCY -5046   /* password fails policy check */
#define AFPERR_USRLOGIN -5047   /* user already logged on */

/* AFP Attention Codes -- 4 bits */
#define AFPATTN_SHUTDOWN     (1 << 15)            /* shutdown/disconnect */
#define AFPATTN_CRASH        (1 << 14)            /* server crashed */
#define AFPATTN_MESG         (1 << 13)            /* server has message */
#define AFPATTN_NORECONNECT  (1 << 12)            /* don't reconnect */
/* server notification */
#define AFPATTN_NOTIFY       (AFPATTN_MESG | AFPATTN_NORECONNECT) 

/* extended bitmap -- 12 bits. volchanged is only useful w/ a server
 * notification, and time is only useful for shutdown. */
#define AFPATTN_VOLCHANGED   (1 << 0)             /* volume has changed */
#define AFPATTN_TIME(x)      ((x) & 0xfff)        /* time in minutes */

typedef enum {
  AFPMESG_LOGIN = 0,
  AFPMESG_SERVER = 1
} afpmessage_t;

/* extended sleep flag */
#define AFPZZZ_EXT_SLEEP  1
#define AFPZZZ_EXT_WAKEUP 2


/* AFP functions */
#define AFP_BYTELOCK	     1
#define AFP_CLOSEVOL     	 2
#define AFP_CLOSEDIR     	 3
#define AFP_CLOSEFORK	 	 4
#define AFP_COPYFILE 	 	 5
#define AFP_CREATEDIR		 6
#define AFP_CREATEFILE		 7
#define AFP_DELETE	 	     8
#define AFP_ENUMERATE	 	 9
#define AFP_FLUSH		    10
#define AFP_FLUSHFORK		11

#define AFP_GETFORKPARAM	14
#define AFP_GETSRVINFO  	15
#define AFP_GETSRVPARAM 	16
#define AFP_GETVOLPARAM		17
#define AFP_LOGIN       	18
#define AFP_LOGINCONT		19
#define AFP_LOGOUT      	20
#define AFP_MAPID		    21
#define AFP_MAPNAME		    22
#define AFP_MOVE		    23
#define AFP_OPENVOL     	24
#define AFP_OPENDIR		    25
#define AFP_OPENFORK		26
#define AFP_READ		    27
#define AFP_RENAME		    28
#define AFP_SETDIRPARAM		29
#define AFP_SETFILEPARAM	30
#define AFP_SETFORKPARAM	31
#define AFP_SETVOLPARAM		32
#define AFP_WRITE		    33
#define AFP_GETFLDRPARAM	34
#define AFP_SETFLDRPARAM	35
#define AFP_CHANGEPW    	36
#define AFP_GETUSERINFO     37
#define AFP_GETSRVRMSG		38
#define AFP_CREATEID		39
#define AFP_DELETEID		40
#define AFP_RESOLVEID		41
#define AFP_EXCHANGEFILE	42
#define AFP_CATSEARCH		43

#define AFP_OPENDT		    48
#define AFP_CLOSEDT		    49

#define AFP_GETICON         51
#define AFP_GTICNINFO       52
#define AFP_ADDAPPL         53
#define AFP_RMVAPPL         54

#define AFP_GETAPPL         55
#define AFP_ADDCMT          56
#define AFP_RMVCMT          57
#define AFP_GETCMT          58
#define AFP_ADDICON        192

/* version 3.0 */
#define AFP_BYTELOCK_EXT        59
#define AFP_CATSEARCH_EXT       67
#define AFP_ENUMERATE_EXT       66
#define AFP_READ_EXT            60
#define AFP_WRITE_EXT           61
#define AFP_LOGIN_EXT       	63
#define AFP_GETSESSTOKEN        64
#define AFP_DISCTOLDSESS        65

/* version 3.1 */
#define AFP_ENUMERATE_EXT2      68
#define AFP_SPOTLIGHT_PRIVATE   76
#define AFP_SYNCDIR             78
#define AFP_SYNCFORK            79
#define AFP_ZZZ                 122

/* version 3.2 */
#define AFP_GETEXTATTR          69
#define AFP_SETEXTATTR          70
#define AFP_REMOVEATTR          71
#define AFP_LISTEXTATTR         72
#define AFP_GETACL              73
#define AFP_SETACL              74
#define AFP_ACCESS              75

/* more defines */
#define REPLAYCACHE_SIZE 128

#endif
