/* $ DVCS ID: $jer|,523/lhos,KYTP!41023161\b"?" <<= DO NOT DELETE! */

/* ddate.c .. converts boring normal dates to fun Discordian Date -><-
   written  the 65th day of The Aftermath in the Year of Our Lady of 
   Discord 3157 by Druel the Chaotic aka Jeremy Johnson aka
   mpython@gnu.ai.mit.edu  
      28 Sever St Apt #3
      Worcester MA 01609

   and I'm not responsible if this program messes anything up (except your 
   mind, I'm responsible for that)

   (k) YOLD 3161 and all time before and after.
   Reprint, reuse, and recycle what you wish.
   This program is in the public domain.  Distribute freely.  Or not.

   Majorly hacked, extended and bogotified/debogotified on 
   Sweetmorn, Bureaucracy 42, 3161 YOLD, by Lee H:. O:. Smith, KYTP, 
   aka Andrew Bulhak, aka acb@dev.null.org

   and I'm not responsible if this program messes anything up (except your 
   mind, I'm responsible for that) (and that goes for me as well --lhos)

   Version history:
   Bureflux 3161:      First release of enhanced ddate with format strings
   59 Bcy, 3161:       PRAISE_BOB and KILL_BOB options split, other minor
                       changes.

   1999-02-22 Arkadiusz Miskiewicz <misiek@pld.ORG.PL>
   - added Native Language Support

   2000-03-17 Burt Holzman <bnh@iname.com>
   - added range checks for dates
*/

/* configuration options  VVVVV   READ THIS!!! */

/* If you wish ddate(1) to print the date in the same format as Druel's 
 * original ddate when called in immediate mode, define OLD_IMMEDIATE_FMT 
 */

#define OLD_IMMEDIATE_FMT

/* If you wish to use the US format for aneristic dates (m-d-y), as opposed to
 * the Commonwealth format, define US_FORMAT.
 */

/* #define US_FORMAT */

/* If you are ideologically, theologically or otherwise opposed to the 
 * Church of the SubGenius and do not wish your copy of ddate(1) to contain
 * code for counting down to X-Day, undefine KILL_BOB */

#define KILL_BOB 13013

/* If you wish ddate(1) to contain SubGenius slogans, define PRAISE_BOB */

/*#define PRAISE_BOB 13013*/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "nls.h"
#include "c.h"

#ifndef __GNUC__
#define inline /* foo */
#endif

#ifdef KILL_BOB
int xday_countdown(int yday, int year);
#endif


/* string constants */

char *day_long[5] = { 
    "Sweetmorn", "Boomtime", "Pungenday", "Prickle-Prickle", "Setting Orange"
};

char *day_short[5] = {"SM","BT","PD","PP","SO"};

char *season_long[5] = { 
    "Chaos", "Discord", "Confusion", "Bureaucracy", "The Aftermath"
};

char *season_short[5] = {"Chs", "Dsc", "Cfn", "Bcy", "Afm"};

char *holyday[5][2] = { 
    { "Mungday", "Chaoflux" },
    { "Mojoday", "Discoflux" },
    { "Syaday",  "Confuflux" },
    { "Zaraday", "Bureflux" },
    { "Maladay", "Afflux" }
};

struct disc_time {
    int season; /* 0-4 */
    int day; /* 0-72 */
    int yday; /* 0-365 */
    int year; /* 3066- */
};

char *excl[] = {
    "Hail Eris!", "All Hail Discordia!", "Kallisti!", "Fnord.", "Or not.",
    "Wibble.", "Pzat!", "P'tang!", "Frink!",
#ifdef PRAISE_BOB
    "Slack!", "Praise \"Bob\"!", "Or kill me.",
#endif /* PRAISE_BOB */
    /* randomness, from the Net and other places. Feel free to add (after
       checking with the relevant authorities, of course). */
    "Grudnuk demand sustenance!", "Keep the Lasagna flying!",
    "You are what you see.",
    "Or is it?", "This statement is false.",
#if defined(linux) || defined (__linux__) || defined (__linux)
    "Hail Eris, Hack Linux!",
#endif
    ""
};

char default_fmt[] = "%{%A, %B %d%}, %Y YOLD";
char *default_immediate_fmt=
#ifdef OLD_IMMEDIATE_FMT
"Today is %{%A, the %e day of %B%} in the YOLD %Y%N%nCelebrate %H"
#else
default_fmt
#endif
;

#define DY(y) (y+1166)

static inline char *ending(int i) {
	return i/10==1?"th":(i%10==1?"st":(i%10==2?"nd":(i%10==3?"rd":"th")));
}

static inline int leapp(int i) {
	return (!(DY(i)%4))&&((DY(i)%100)||(!(DY(i)%400)));
}

/* select a random string */
static inline char *sel(char **strings, int num) {
	return(strings[random()%num]);
}

void print(struct disc_time,char **); /* old */
void format(char *buf, const char* fmt, struct disc_time dt);
/* read a fortune file */
int load_fortunes(char *fn, char *delim, char** result);

struct disc_time convert(int,int);
struct disc_time makeday(int,int,int);

int
main (int argc, char *argv[]) {
    long t;
    struct tm *eris;
    int bob,raw;
    struct disc_time hastur;
    char schwa[23*17], *fnord=0;
    int pi;
    char *progname, *p;

    progname = argv[0];
    if ((p = strrchr(progname, '/')) != NULL)
	progname = p+1;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    srandom(time(NULL));
    /* do args here */
    for(pi=1; pi<argc; pi++) {
	switch(argv[pi][0]) {
	case '+': fnord=argv[pi]+1; break;
	case '-': 
	    switch(argv[pi][1]) {
	    case 'V':
		printf(_("%s (%s)\n"), progname, PACKAGE_STRING);
	    default: goto usage;
	    }
	default: goto thud;
	}
    }

  thud:
    if (argc-pi==3){ 
	int moe=atoi(argv[pi]), larry=atoi(argv[pi+1]), curly=atoi(argv[pi+2]);
	hastur=makeday(
#ifdef US_FORMAT
	    moe,larry,
#else
	    larry,moe,
#endif
	    curly);
	if (hastur.season == -1) {
		printf("Invalid date -- out of range\n");
		return -1;
	}
	fnord=fnord?fnord:default_fmt;
    } else if (argc!=pi) { 
      usage:
	fprintf(stderr,_("usage: %s [+format] [day month year]\n"), argv[0]);
	exit(1);
    } else {
	t= time(NULL);
	eris=localtime(&t);
	bob=eris->tm_yday; /* days since Jan 1. */
	raw=eris->tm_year; /* years since 1980 */
	hastur=convert(bob,raw);
	fnord=fnord?fnord:default_immediate_fmt;
    }
    format(schwa, fnord, hastur);
    printf("%s\n", schwa);
   
    return 0;
}

void format(char *buf, const char* fmt, struct disc_time dt)
{
    int tib_start=-1, tib_end=0;
    int i, fmtlen=strlen(fmt);
    char *bufptr=buf;

/*    fprintf(stderr, "format(%p, \"%s\", dt)\n", buf, fmt);*/

    /* first, find extents of St. Tib's Day area, if defined */
    for(i=0; i<fmtlen; i++) {
	if(fmt[i]=='%') {
	    switch(fmt[i+1]) {
	    case 'A':
	    case 'a':
	    case 'd':
	    case 'e':
		if(tib_start>0)	    tib_end=i+1;
		else		    tib_start=i;
		break;
	    case '{': tib_start=i; break;
	    case '}': tib_end=i+1; break;
	    }
	}
    }

    /* now do the formatting */
    buf[0]=0;

    for(i=0; i<fmtlen; i++) {
	if((i==tib_start) && (dt.day==-1)) {
	    /* handle St. Tib's Day */
	    strcpy(bufptr, _("St. Tib's Day"));
	    bufptr += strlen(bufptr);
	    i=tib_end;
	} else {
	    if(fmt[i]=='%') {
		char *wibble=0, snarf[23];
		switch(fmt[++i]) {
		case 'A': wibble=day_long[dt.yday%5]; break;
		case 'a': wibble=day_short[dt.yday%5]; break;
		case 'B': wibble=season_long[dt.season]; break;
		case 'b': wibble=season_short[dt.season]; break;
		case 'd': sprintf(snarf, "%d", dt.day+1); wibble=snarf; break;
		case 'e': sprintf(snarf, "%d%s", dt.day+1, ending(dt.day+1)); 
		    wibble=snarf; break;
		case 'H': if(dt.day==4||dt.day==49)
		    wibble=holyday[dt.season][dt.day==49]; break;
		case 'N': if(dt.day!=4&&dt.day!=49) goto eschaton; break;
		case 'n': *(bufptr++)='\n'; break;
		case 't': *(bufptr++)='\t'; break;

		case 'Y': sprintf(snarf, "%d", dt.year); wibble=snarf; break;
		case '.': wibble=sel(excl, ARRAY_SIZE(excl));
		    break;
#ifdef KILL_BOB
		case 'X': sprintf(snarf, "%d", 
				  xday_countdown(dt.yday, dt.year));
				  wibble = snarf; break;
#endif /* KILL_BOB */
		}
		if(wibble) {
/*		    fprintf(stderr, "wibble = (%s)\n", wibble);*/
		    strcpy(bufptr, wibble); bufptr+=strlen(wibble);
		}
	    } else {
		*(bufptr++) = fmt[i];
	    }
	}
    }
  eschaton:
    *(bufptr)=0;
}

struct disc_time makeday(int imonth,int iday,int iyear) /*i for input */
{ 
    struct disc_time funkychickens;
    
    int cal[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    int dayspast=0;

    memset(&funkychickens,0,sizeof(funkychickens));
    /* basic range checks */
    if (imonth < 1 || imonth > 12) {
	    funkychickens.season = -1;
	    return funkychickens;
    }
    if (iday < 1 || iday > cal[imonth-1]) {
	    if (!(imonth == 2 && iday == 29 && iyear%4 == 0 &&
		  (iyear%100 != 0 || iyear%400 == 0))) {
		    funkychickens.season = -1;
		    return funkychickens;
	    }
    }
    
    imonth--;
    funkychickens.year= iyear+1166;
    while(imonth>0) { dayspast+=cal[--imonth]; }
    funkychickens.day=dayspast+iday-1;
    funkychickens.season=0;
    if((funkychickens.year%4)==2) {
	if (funkychickens.day==59 && iday==29)  funkychickens.day=-1;
    }
    funkychickens.yday=funkychickens.day;
/*               note: EQUAL SIGN...hopefully that fixes it */
    while(funkychickens.day>=73) {
	funkychickens.season++;
	funkychickens.day-=73;
    }
    return funkychickens;
}

struct disc_time convert(int nday, int nyear)
{  struct disc_time funkychickens;
   
   funkychickens.year = nyear+3066;
   funkychickens.day=nday;
   funkychickens.season=0;
   if ((funkychickens.year%4)==2)
     {if (funkychickens.day==59)
	funkychickens.day=-1;
     else if (funkychickens.day >59)
       funkychickens.day-=1;
    }
   funkychickens.yday=funkychickens.day;
   while (funkychickens.day>=73)
     { funkychickens.season++;
       funkychickens.day-=73;
     }
   return funkychickens;
  
 }

#ifdef KILL_BOB

/* Code for counting down to X-Day, X-Day being Cfn 40, 3164 
 *
 * After `X-Day' passed without incident, the CoSG declared that it had 
 * got the year upside down --- X-Day is actually in 8661 AD rather than 
 * 1998 AD.
 *
 * Thus, the True X-Day is Cfn 40, 9827.
 *
 */

int xday_countdown(int yday, int year) {
    int r=(185-yday)+(((yday<59)&&(leapp(year)))?1:0);
    while(year<9827) r+=(leapp(++year)?366:365);
    while(year>9827) r-=(leapp(year--)?366:365);
    return r;
}

#endif
