/*
 *
 * Copyright (c) 1999 Adrian Sun (asun@zoology.washington.edu)
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * Copyright (c) 2002 netatalk
 *
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/afp.h>
#include <atalk/globals.h>

const char *AfpNum2name(int num)
{
	switch(num) {
    case AFP_BYTELOCK      : return "AFP_BYTELOCK";         /*   1 */
	case AFP_CLOSEVOL      : return "AFP_CLOSEVOL";  	    /*   2 */
	case AFP_CLOSEDIR      : return "AFP_CLOSEDIR";  	    /*   3 */
	case AFP_CLOSEFORK     : return "AFP_CLOSEFORK";  	    /*   4 */
	case AFP_COPYFILE      : return "AFP_COPYFILE";  	    /*   5 */
	case AFP_CREATEDIR     : return "AFP_CREATEDIR";  	    /*   6 */
	case AFP_CREATEFILE    : return "AFP_CREATEFILE";  	    /*   7 */
	case AFP_DELETE        : return "AFP_DELETE"; 		    /*   8 */
	case AFP_ENUMERATE     : return "AFP_ENUMERATE"; 	    /*   9 */
	case AFP_FLUSH         : return "AFP_FLUSH";		    /*	10 */
	case AFP_FLUSHFORK     : return "AFP_FLUSHFORK";	    /*	11 */

	case AFP_GETFORKPARAM  : return "AFP_GETFORKPARAM";     /*	14 */
	case AFP_GETSRVINFO    : return "AFP_GETSRVINFO"; 	    /* 	15 */
	case AFP_GETSRVPARAM   : return "AFP_GETSRVPARAM";      /*	16 */
	case AFP_GETVOLPARAM   : return "AFP_GETVOLPARAM";      /*	17 */
	case AFP_LOGIN         : return "AFP_LOGIN";  		    /*  18 */
    case AFP_LOGINCONT     : return "AFP_LOGINCONT";        /*  19 */
	case AFP_LOGOUT        : return "AFP_LOGOUT";  		    /*  20 */
    case AFP_MAPID         : return "AFP_MAPID";            /*  21 */
    case AFP_MAPNAME       : return "AFP_MAPNAME";          /*  22 */
    case AFP_MOVE          : return "AFP_MOVE";             /*  23 */
	case AFP_OPENVOL       : return "AFP_OPENVOL";  	    /*  24 */
	case AFP_OPENDIR       : return "AFP_OPENDIR";  	    /*  25 */
	case AFP_OPENFORK      : return "AFP_OPENFORK";  	    /*  26 */
	case AFP_READ		   : return "AFP_READ";			    /*  27 */
    case AFP_RENAME        : return "AFP_RENAME";           /*  28 */
    case AFP_SETDIRPARAM   : return "AFP_SETDIRPARAM";      /*  29 */
    case AFP_SETFILEPARAM  : return "AFP_SETFILEPARAM";     /*  30 */
    case AFP_SETFORKPARAM  : return "AFP_SETFORKPARAM";     /*  31 */
    case AFP_SETVOLPARAM   : return "AFP_SETVOLPARAM ";     /*  32 */
	case AFP_WRITE 		   : return "AFP_WRITE";		    /*  33 */
	case AFP_GETFLDRPARAM  : return "AFP_GETFLDRPARAM";	    /*  34 */	
	case AFP_SETFLDRPARAM  : return "AFP_SETFLDRPARAM";	    /*  35 */	
	case AFP_CHANGEPW      : return "AFP_CHANGEPW";	        /*  36 */	
    case AFP_GETUSERINFO   : return "AFP_GETUSERINFO";      /*  37 */
    case AFP_GETSRVRMSG    : return "AFP_GETSRVRMSG";       /*  38 */
    case AFP_CREATEID      : return "AFP_CREATEID";         /*  39 */
    case AFP_DELETEID      : return "AFP_DELETEID";         /*  40 */
    case AFP_RESOLVEID     : return "AFP_RESOLVEID";        /*  41 */
    case AFP_EXCHANGEFILE  : return "AFP_EXCHANGEFILE";     /*  42 */
    case AFP_CATSEARCH     : return "AFP_CATSEARCH";        /*  43 */

	case AFP_OPENDT        : return "AFP_OPENDT";			/*  48 */
	case AFP_CLOSEDT       : return "AFP_CLOSEDT";		    /*	49 */
    case AFP_GETICON       : return "AFP_GETICON";		    /*	51 */
    case AFP_GTICNINFO     : return "AFP_GTICNINFO";		/*	52 */
    case AFP_ADDAPPL       : return "AFP_ADDAPPL";  		/*	53 */
    case AFP_RMVAPPL       : return "AFP_RMVAPPL";  		/*	54 */
    case AFP_GETAPPL       : return "AFP_GETAPPL";      	/*	55 */
    case AFP_ADDCMT        : return "AFP_ADDCMT";       	/*	56 */
	case AFP_RMVCMT        : return "AFP_RMVCMT";       	/*	57 */
	case AFP_GETCMT        : return "AFP_GETCMT";       	/*	58 */
	case AFP_ADDICON       : return "AFP_ADDICON";       	/* 192 */
    /* version 3.0 */
    case AFP_BYTELOCK_EXT  : return "AFP_BYTELOCK_EXT";     /*  59 */
    case AFP_READ_EXT      : return "AFP_READ_EXT";         /*  60 */
    case AFP_WRITE_EXT     : return "AFP_WRITE_EXT";        /*  61 */
    case AFP_GETSESSTOKEN  : return "AFP_GETSESSTOKEN";     /*  64 */ 
    case AFP_LOGIN_EXT     : return "AFP_LOGIN_EXT";        /*  63 */
    case AFP_DISCTOLDSESS  : return "AFP_DISCTOLDSESS";     /*  65 */
    case AFP_ENUMERATE_EXT : return "AFP_ENUMERATE_EXT";    /*  66 */
    case AFP_CATSEARCH_EXT : return "AFP_CATSEARCH_EXT";    /*  67 */
    /* version 3.1 */
    case AFP_ENUMERATE_EXT2: return "AFP_ENUMERATE_EXT2";   /*  68 */
    case AFP_ZZZ  : return "AFP_ZZZ";	    /* 122 */
    /* version 3.2 */															  
    case AFP_GETEXTATTR         : return "AFP_GETEXTATTR";	      /* 69 */
    case AFP_SETEXTATTR         : return "AFP_SETEXTATTR";	      /* 70 */
    case AFP_REMOVEATTR         : return "AFP_REMOVEATTR";	      /* 71 */
    case AFP_LISTEXTATTR        : return "AFP_LISTEXTATTR";	      /* 72 */
    case AFP_GETACL             : return "AFP_GETACL";	          /* 73 */
    case AFP_SETACL             : return "AFP_SETACL";	          /* 74 */
    case AFP_ACCESS             : return "AFP_ACCESS";	          /* 75 */
    case AFP_SPOTLIGHT_PRIVATE  : return "AFP_SPOTLIGHT_PRIVATE"; /* 76 */
    case AFP_SYNCDIR            : return "AFP_SYNCDIR";	          /* 78 */
    case AFP_SYNCFORK           : return "AFP_SYNCFORK";	      /* 79 */
	}
	return "not yet defined";								  
}

#define AFPERR2NAME(err) case err : return #err

const char *AfpErr2name(int err)
{
    switch (err) {
        AFPERR2NAME(AFP_OK);
        AFPERR2NAME(AFPERR_DID1);
        AFPERR2NAME(AFPERR_ACCESS);
        AFPERR2NAME(AFPERR_AUTHCONT);
        AFPERR2NAME(AFPERR_BADUAM);
        AFPERR2NAME(AFPERR_BADVERS);
        AFPERR2NAME(AFPERR_BITMAP);
        AFPERR2NAME(AFPERR_CANTMOVE);
        AFPERR2NAME(AFPERR_DENYCONF);
        AFPERR2NAME(AFPERR_DIRNEMPT);
        AFPERR2NAME(AFPERR_DFULL);
        AFPERR2NAME(AFPERR_EOF);
        AFPERR2NAME(AFPERR_BUSY);
        AFPERR2NAME(AFPERR_FLATVOL);
        AFPERR2NAME(AFPERR_NOITEM);
        AFPERR2NAME(AFPERR_LOCK);
        AFPERR2NAME(AFPERR_MISC);
        AFPERR2NAME(AFPERR_NLOCK);
        AFPERR2NAME(AFPERR_NOSRVR);
        AFPERR2NAME(AFPERR_EXIST);
        AFPERR2NAME(AFPERR_NOOBJ);
        AFPERR2NAME(AFPERR_PARAM);
        AFPERR2NAME(AFPERR_NORANGE);
        AFPERR2NAME(AFPERR_RANGEOVR);
        AFPERR2NAME(AFPERR_SESSCLOS);
        AFPERR2NAME(AFPERR_NOTAUTH);
        AFPERR2NAME(AFPERR_NOOP);
        AFPERR2NAME(AFPERR_BADTYPE);
        AFPERR2NAME(AFPERR_NFILE);
        AFPERR2NAME(AFPERR_SHUTDOWN);
        AFPERR2NAME(AFPERR_NORENAME);
        AFPERR2NAME(AFPERR_NODIR);
        AFPERR2NAME(AFPERR_ITYPE);
        AFPERR2NAME(AFPERR_VLOCK);
        AFPERR2NAME(AFPERR_OLOCK);
        AFPERR2NAME(AFPERR_CTNSHRD);
        AFPERR2NAME(AFPERR_NOID);
        AFPERR2NAME(AFPERR_EXISTID);
        AFPERR2NAME(AFPERR_DIFFVOL);
        AFPERR2NAME(AFPERR_CATCHNG);
        AFPERR2NAME(AFPERR_SAMEOBJ);
        AFPERR2NAME(AFPERR_BADID);
        AFPERR2NAME(AFPERR_PWDSAME);
        AFPERR2NAME(AFPERR_PWDSHORT);
        AFPERR2NAME(AFPERR_PWDEXPR);
        AFPERR2NAME(AFPERR_INSHRD);
        AFPERR2NAME(AFPERR_INTRASH);
        AFPERR2NAME(AFPERR_PWDCHNG);
        AFPERR2NAME(AFPERR_PWDPOLCY);
        AFPERR2NAME(AFPERR_USRLOGIN);
    }
	return "not yet defined";
}
