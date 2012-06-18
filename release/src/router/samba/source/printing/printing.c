/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   printing routines
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
extern int DEBUGLEVEL;

static BOOL * lpq_cache_reset=NULL;

static int check_lpq_cache(int snum) {
  static int lpq_caches=0;
  int i;

  if (lpq_caches <= snum) {
      BOOL * p;
      p = (BOOL *) Realloc(lpq_cache_reset,(snum+1)*sizeof(BOOL));
      if (p) {
	 lpq_cache_reset=p;
	 for (i=lpq_caches; i<snum+1; i++) lpq_cache_reset[i] = False;
	 lpq_caches = snum+1;
      }
  }
  return lpq_caches;
}

void lpq_reset(int snum)
{
  if (check_lpq_cache(snum) > snum) lpq_cache_reset[snum]=True;
}


/****************************************************************************
Build the print command in the supplied buffer. This means getting the
print command for the service and inserting the printer name and the
print file name. Return NULL on error, else the passed buffer pointer.
****************************************************************************/
static char *build_print_command(connection_struct *conn,
				 char *command, 
				 char *syscmd, char *filename)
{
	int snum = SNUM(conn);
	char *tstr;
  
	/* get the print command for the service. */
	tstr = command;
	if (!syscmd || !tstr) {
		DEBUG(0,("No print command for service `%s'\n", 
			 SERVICE(snum)));
		return (NULL);
	}

	/* copy the command into the buffer for extensive meddling. */
	StrnCpy(syscmd, tstr, sizeof(pstring) - 1);
  
	/* look for "%s" in the string. If there is no %s, we cannot print. */   
	if (!strstr(syscmd, "%s") && !strstr(syscmd, "%f")) {
		DEBUG(2,("WARNING! No placeholder for the filename in the print command for service %s!\n", SERVICE(snum)));
	}
  
	pstring_sub(syscmd, "%s", filename);
	pstring_sub(syscmd, "%f", filename);
  
	/* Does the service have a printername? If not, make a fake
           and empty */
	/* printer name. That way a %p is treated sanely if no printer */
	/* name was specified to replace it. This eventuality is logged.  */
	tstr = PRINTERNAME(snum);
	if (tstr == NULL || tstr[0] == '\0') {
		DEBUG(3,( "No printer name - using %s.\n", SERVICE(snum)));
		tstr = SERVICE(snum);
	}
  
	pstring_sub(syscmd, "%p", tstr);
  
	standard_sub(conn,syscmd);
  
	return (syscmd);
}


/****************************************************************************
print a file - called on closing the file
****************************************************************************/
void print_file(connection_struct *conn, files_struct *file)
{
	pstring syscmd;
	int snum = SNUM(conn);
	char *tempstr;

	*syscmd = 0;

	if (dos_file_size(file->fsp_name) <= 0) {
		DEBUG(3,("Discarding null print job %s\n",file->fsp_name));
		dos_unlink(file->fsp_name);
		return;
	}

	tempstr = build_print_command(conn, 
				      PRINTCOMMAND(snum), 
				      syscmd, file->fsp_name);
	if (tempstr != NULL) {
		int ret = smbrun(syscmd,NULL,False);
		DEBUG(3,("Running the command `%s' gave %d\n",syscmd,ret));
	} else {
		DEBUG(0,("Null print command?\n"));
	}
  
	lpq_reset(snum);
}

static char *Months[13] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "Err"};


/*******************************************************************
process time fields
********************************************************************/
static time_t EntryTime(fstring tok[], int ptr, int count, int minimum)
{
  time_t jobtime,jobtime1;

  jobtime = time(NULL);		/* default case: take current time */
  if (count >= minimum) {
    struct tm *t;
    int i, day, hour, min, sec;
    char   *c;

    for (i=0; i<13; i++) if (!strncmp(tok[ptr], Months[i],3)) break; /* Find month */
    if (i<12) {
      t = localtime(&jobtime);
      day = atoi(tok[ptr+1]);
      c=(char *)(tok[ptr+2]);
      *(c+2)=0;
      hour = atoi(c);
      *(c+5)=0;
      min = atoi(c+3);
      if(*(c+6) != 0)sec = atoi(c+6);
      else  sec=0;

      if ((t->tm_mon < i)||
	  ((t->tm_mon == i)&&
	   ((t->tm_mday < day)||
	    ((t->tm_mday == day)&&
	     (t->tm_hour*60+t->tm_min < hour*60+min)))))
	t->tm_year--;		/* last year's print job */

      t->tm_mon = i;
      t->tm_mday = day;
      t->tm_hour = hour;
      t->tm_min = min;
      t->tm_sec = sec;
      jobtime1 = mktime(t);
      if (jobtime1 != (time_t)-1)
	jobtime = jobtime1;
    }
  }
  return jobtime;
}


/****************************************************************************
parse a lpq line

here is an example of lpq output under bsd

Warning: no daemon present
Rank   Owner      Job  Files                                 Total Size
1st    tridge     148  README                                8096 bytes

here is an example of lpq output under osf/1

Warning: no daemon present
Rank   Pri Owner      Job  Files                             Total Size
1st    0   tridge     148  README                            8096 bytes


<allan@umich.edu> June 30, 1998.
Modified to handle file names with spaces, like the parse_lpq_lprng code
further below.
****************************************************************************/
static BOOL parse_lpq_bsd(char *line,print_queue_struct *buf,BOOL first)
{
#ifdef	OSF1
#define	RANKTOK	0
#define	PRIOTOK 1
#define	USERTOK 2
#define	JOBTOK	3
#define	FILETOK	4
#define	TOTALTOK (count - 2)
#define	NTOK	6
#define	MAXTOK	128
#else	/* OSF1 */
#define	RANKTOK	0
#define	USERTOK 1
#define	JOBTOK	2
#define	FILETOK	3
#define	TOTALTOK (count - 2)
#define	NTOK	5
#define	MAXTOK	128
#endif	/* OSF1 */

  char *tok[MAXTOK];
  int  count = 0;
  pstring line2;

  pstrcpy(line2,line);

#ifdef	OSF1
  {
    size_t length;
    length = strlen(line2);
    if (line2[length-3] == ':')
      return(False);
  }
#endif	/* OSF1 */

  tok[0] = strtok(line2," \t");
  count++;

  while (((tok[count] = strtok(NULL," \t")) != NULL) && (count < MAXTOK)) {
    count++;
  }

  /* we must get at least NTOK tokens */
  if (count < NTOK)
    return(False);

  /* the Job and Total columns must be integer */
  if (!isdigit((int)*tok[JOBTOK]) || !isdigit((int)*tok[TOTALTOK])) return(False);

  buf->job = atoi(tok[JOBTOK]);
  buf->size = atoi(tok[TOTALTOK]);
  buf->status = strequal(tok[RANKTOK],"active")?LPQ_PRINTING:LPQ_QUEUED;
  buf->time = time(NULL);
  StrnCpy(buf->user,tok[USERTOK],sizeof(buf->user)-1);
  StrnCpy(buf->file,tok[FILETOK],sizeof(buf->file)-1);

  if ((FILETOK + 1) != TOTALTOK) {
    int bufsize;
    int i;

    bufsize = sizeof(buf->file) - strlen(buf->file) - 1;

    for (i = (FILETOK + 1); i < TOTALTOK; i++) {
      safe_strcat(buf->file," ",bufsize);
      safe_strcat(buf->file,tok[i],bufsize - 1);
      bufsize = sizeof(buf->file) - strlen(buf->file) - 1;
      if (bufsize <= 0) {
        break;
      }
    }
    /* Ensure null termination. */
    buf->file[sizeof(buf->file)-1] = '\0';
  }

#ifdef PRIOTOK
  buf->priority = atoi(tok[PRIOTOK]);
#else
  buf->priority = 1;
#endif
  return(True);
}

/*
<magnus@hum.auc.dk>
LPRng_time modifies the current date by inserting the hour and minute from
the lpq output.  The lpq time looks like "23:15:07"

<allan@umich.edu> June 30, 1998.
Modified to work with the re-written parse_lpq_lprng routine.
*/
static time_t LPRng_time(char *time_string)
{
  time_t jobtime;
  struct tm *t;

  jobtime = time(NULL);         /* default case: take current time */
  t = localtime(&jobtime);
  t->tm_hour = atoi(time_string);
  t->tm_min = atoi(time_string+3);
  t->tm_sec = atoi(time_string+6);
  jobtime = mktime(t);

  return jobtime;
}


/****************************************************************************
  parse a lprng lpq line
  <allan@umich.edu> June 30, 1998.
  Re-wrote this to handle file names with spaces, multiple file names on one
  lpq line, etc;
****************************************************************************/
static BOOL parse_lpq_lprng(char *line,print_queue_struct *buf,BOOL first)
{
#define	LPRNG_RANKTOK	0
#define	LPRNG_USERTOK	1
#define	LPRNG_PRIOTOK	2
#define	LPRNG_JOBTOK	3
#define	LPRNG_FILETOK	4
#define	LPRNG_TOTALTOK	(num_tok - 2)
#define	LPRNG_TIMETOK	(num_tok - 1)
#define	LPRNG_NTOK	7
#define	LPRNG_MAXTOK	128 /* PFMA just to keep us from running away. */

  char *tokarr[LPRNG_MAXTOK];
  char *cptr;
  int  num_tok = 0;
  pstring line2;

  pstrcpy(line2,line);
  tokarr[0] = strtok(line2," \t");
  num_tok++;
  while (((tokarr[num_tok] = strtok(NULL," \t")) != NULL)
         && (num_tok < LPRNG_MAXTOK)) {
    num_tok++;
  }

  /* We must get at least LPRNG_NTOK tokens. */
  if (num_tok < LPRNG_NTOK) {
    return(False);
  }

  if (!isdigit((int)*tokarr[LPRNG_JOBTOK]) || !isdigit((int)*tokarr[LPRNG_TOTALTOK])) {
    return(False);
  }

  buf->job  = atoi(tokarr[LPRNG_JOBTOK]);
  buf->size = atoi(tokarr[LPRNG_TOTALTOK]);

  if (strequal(tokarr[LPRNG_RANKTOK],"active")) {
    buf->status = LPQ_PRINTING;
  } else if (isdigit((int)*tokarr[LPRNG_RANKTOK])) {
    buf->status = LPQ_QUEUED;
  } else {
    buf->status = LPQ_PAUSED;
  }

  buf->priority = *tokarr[LPRNG_PRIOTOK] -'A';

  buf->time = LPRng_time(tokarr[LPRNG_TIMETOK]);

  StrnCpy(buf->user,tokarr[LPRNG_USERTOK],sizeof(buf->user)-1);

  /* The '@hostname' prevents windows from displaying the printing icon
   * for the current user on the taskbar.  Plop in a null.
   */

  if ((cptr = strchr(buf->user,'@')) != NULL) {
    *cptr = '\0';
  }

  StrnCpy(buf->file,tokarr[LPRNG_FILETOK],sizeof(buf->file)-1);

  if ((LPRNG_FILETOK + 1) != LPRNG_TOTALTOK) {
    int bufsize;
    int i;

    bufsize = sizeof(buf->file) - strlen(buf->file) - 1;

    for (i = (LPRNG_FILETOK + 1); i < LPRNG_TOTALTOK; i++) {
      safe_strcat(buf->file," ",bufsize);
      safe_strcat(buf->file,tokarr[i],bufsize - 1);
      bufsize = sizeof(buf->file) - strlen(buf->file) - 1;
      if (bufsize <= 0) {
        break;
      }
    }
    /* Ensure null termination. */
    buf->file[sizeof(buf->file)-1] = '\0';
  }

  return(True);
}



/*******************************************************************
parse lpq on an aix system

Queue   Dev   Status    Job Files              User         PP %   Blks  Cp Rnk
------- ----- --------- --- ------------------ ---------- ---- -- ----- --- ---
lazer   lazer READY
lazer   lazer RUNNING   537 6297doc.A          kvintus@IE    0 10  2445   1   1
              QUEUED    538 C.ps               root@IEDVB           124   1   2
              QUEUED    539 E.ps               root@IEDVB            28   1   3
              QUEUED    540 L.ps               root@IEDVB           172   1   4
              QUEUED    541 P.ps               root@IEDVB            22   1   5
********************************************************************/
static BOOL parse_lpq_aix(char *line,print_queue_struct *buf,BOOL first)
{
  fstring tok[11];
  int count=0;

  /* handle the case of "(standard input)" as a filename */
  pstring_sub(line,"standard input","STDIN");
  all_string_sub(line,"(","\"",0);
  all_string_sub(line,")","\"",0);

  for (count=0; 
       count<10 && 
	       next_token(&line,tok[count],NULL, sizeof(tok[count])); 
       count++) ;

  /* we must get 6 tokens */
  if (count < 10)
  {
      if ((count == 7) && ((strcmp(tok[0],"QUEUED") == 0) || (strcmp(tok[0],"HELD") == 0)))
      {
          /* the 2nd and 5th columns must be integer */
          if (!isdigit((int)*tok[1]) || !isdigit((int)*tok[4])) return(False);
          buf->size = atoi(tok[4]) * 1024;
          /* if the fname contains a space then use STDIN */
          if (strchr(tok[2],' '))
            fstrcpy(tok[2],"STDIN");

          /* only take the last part of the filename */
          {
            fstring tmp;
            char *p = strrchr(tok[2],'/');
            if (p)
              {
                fstrcpy(tmp,p+1);
                fstrcpy(tok[2],tmp);
              }
          }


          buf->job = atoi(tok[1]);
          buf->status = strequal(tok[0],"HELD")?LPQ_PAUSED:LPQ_QUEUED;
	  buf->priority = 0;
          buf->time = time(NULL);
          StrnCpy(buf->user,tok[3],sizeof(buf->user)-1);
          StrnCpy(buf->file,tok[2],sizeof(buf->file)-1);
      }
      else
      {
          DEBUG(6,("parse_lpq_aix count=%d\n", count));
          return(False);
      }
  }
  else
  {
      /* the 4th and 9th columns must be integer */
      if (!isdigit((int)*tok[3]) || !isdigit((int)*tok[8])) return(False);
      buf->size = atoi(tok[8]) * 1024;
      /* if the fname contains a space then use STDIN */
      if (strchr(tok[4],' '))
        fstrcpy(tok[4],"STDIN");

      /* only take the last part of the filename */
      {
        fstring tmp;
        char *p = strrchr(tok[4],'/');
        if (p)
          {
            fstrcpy(tmp,p+1);
            fstrcpy(tok[4],tmp);
          }
      }


      buf->job = atoi(tok[3]);
      buf->status = strequal(tok[2],"RUNNING")?LPQ_PRINTING:LPQ_QUEUED;
      buf->priority = 0;
      buf->time = time(NULL);
      StrnCpy(buf->user,tok[5],sizeof(buf->user)-1);
      StrnCpy(buf->file,tok[4],sizeof(buf->file)-1);
  }


  return(True);
}


/****************************************************************************
parse a lpq line
here is an example of lpq output under hpux; note there's no space after -o !
$> lpstat -oljplus
ljplus-2153         user           priority 0  Jan 19 08:14 on ljplus
      util.c                                  125697 bytes
      server.c				      110712 bytes
ljplus-2154         user           priority 0  Jan 19 08:14 from client
      (standard input)                          7551 bytes
****************************************************************************/
static BOOL parse_lpq_hpux(char * line, print_queue_struct *buf, BOOL first)
{
  /* must read two lines to process, therefore keep some values static */
  static BOOL header_line_ok=False, base_prio_reset=False;
  static fstring jobuser;
  static int jobid;
  static int jobprio;
  static time_t jobtime;
  static int jobstat=LPQ_QUEUED;
  /* to store minimum priority to print, lpstat command should be invoked
     with -p option first, to work */
  static int base_prio;
 
  int count;
  char htab = '\011';  
  fstring tok[12];

  /* If a line begins with a horizontal TAB, it is a subline type */
  
  if (line[0] == htab) { /* subline */
    /* check if it contains the base priority */
    if (!strncmp(line,"\tfence priority : ",18)) {
       base_prio=atoi(&line[18]);
       DEBUG(4, ("fence priority set at %d\n", base_prio));
    }
    if (!header_line_ok) return (False); /* incorrect header line */
    /* handle the case of "(standard input)" as a filename */
    pstring_sub(line,"standard input","STDIN");
    all_string_sub(line,"(","\"",0);
    all_string_sub(line,")","\"",0);
    
    for (count=0; count<2 && next_token(&line,tok[count],NULL,sizeof(tok[count])); count++) ;
    /* we must get 2 tokens */
    if (count < 2) return(False);
    
    /* the 2nd column must be integer */
    if (!isdigit((int)*tok[1])) return(False);
    
    /* if the fname contains a space then use STDIN */
    if (strchr(tok[0],' '))
      fstrcpy(tok[0],"STDIN");
    
    buf->size = atoi(tok[1]);
    StrnCpy(buf->file,tok[0],sizeof(buf->file)-1);
    
    /* fill things from header line */
    buf->time = jobtime;
    buf->job = jobid;
    buf->status = jobstat;
    buf->priority = jobprio;
    StrnCpy(buf->user,jobuser,sizeof(buf->user)-1);
    
    return(True);
  }
  else { /* header line */
    header_line_ok=False; /* reset it */
    if (first) {
       if (!base_prio_reset) {
	  base_prio=0; /* reset it */
	  base_prio_reset=True;
       }
    }
    else if (base_prio) base_prio_reset=False;
    
    /* handle the dash in the job id */
    pstring_sub(line,"-"," ");
    
    for (count=0; count<12 && next_token(&line,tok[count],NULL,sizeof(tok[count])); count++) ;
      
    /* we must get 8 tokens */
    if (count < 8) return(False);
    
    /* first token must be printer name (cannot check ?) */
    /* the 2nd, 5th & 7th column must be integer */
    if (!isdigit((int)*tok[1]) || !isdigit((int)*tok[4]) || !isdigit((int)*tok[6])) return(False);
    jobid = atoi(tok[1]);
    StrnCpy(jobuser,tok[2],sizeof(buf->user)-1);
    jobprio = atoi(tok[4]);
    
    /* process time */
    jobtime=EntryTime(tok, 5, count, 8);
    if (jobprio < base_prio) {
       jobstat = LPQ_PAUSED;
       DEBUG (4, ("job %d is paused: prio %d < %d; jobstat=%d\n", jobid, jobprio, base_prio, jobstat));
    }
    else {
       jobstat = LPQ_QUEUED;
       if ((count >8) && (((strequal(tok[8],"on")) ||
			   ((strequal(tok[8],"from")) && 
			    ((count > 10)&&(strequal(tok[10],"on")))))))
	 jobstat = LPQ_PRINTING;
    }
    
    header_line_ok=True; /* information is correct */
    return(False); /* need subline info to include into queuelist */
  }
}


/****************************************************************************
parse a lpstat line

here is an example of "lpstat -o dcslw" output under sysv

dcslw-896               tridge            4712   Dec 20 10:30:30 on dcslw
dcslw-897               tridge            4712   Dec 20 10:30:30 being held

****************************************************************************/
static BOOL parse_lpq_sysv(char *line,print_queue_struct *buf,BOOL first)
{
  fstring tok[9];
  int count=0;
  char *p;

  /* 
   * Handle the dash in the job id, but make sure that we skip over
   * the printer name in case we have a dash in that.
   * Patch from Dom.Mitchell@palmerharvey.co.uk.
   */

  /*
   * Move to the first space.
   */
  for (p = line ; !isspace(*p) && *p; p++)
    ;

  /*
   * Back up until the last '-' character or
   * start of line.
   */
  for (; (p >= line) && (*p != '-'); p--)
    ;

  if((p >= line) && (*p == '-'))
    *p = ' ';

  for (count=0; count<9 && next_token(&line,tok[count],NULL,sizeof(tok[count])); count++)
    ;

  /* we must get 7 tokens */
  if (count < 7)
    return(False);

  /* the 2nd and 4th, 6th columns must be integer */
  if (!isdigit((int)*tok[1]) || !isdigit((int)*tok[3]))
    return(False);
  if (!isdigit((int)*tok[5]))
    return(False);

  /* if the user contains a ! then trim the first part of it */  
  if ((p=strchr(tok[2],'!'))) {
      fstring tmp;
      fstrcpy(tmp,p+1);
      fstrcpy(tok[2],tmp);
  }

  buf->job = atoi(tok[1]);
  buf->size = atoi(tok[3]);
  if (count > 7 && strequal(tok[7],"on"))
    buf->status = LPQ_PRINTING;
  else if (count > 8 && strequal(tok[7],"being") && strequal(tok[8],"held"))
    buf->status = LPQ_PAUSED;
  else
    buf->status = LPQ_QUEUED;
  buf->priority = 0;
  buf->time = EntryTime(tok, 4, count, 7);
  StrnCpy(buf->user,tok[2],sizeof(buf->user)-1);
  StrnCpy(buf->file,tok[2],sizeof(buf->file)-1);
  return(True);
}

/****************************************************************************
parse a lpq line

here is an example of lpq output under qnx
Spooler: /qnx/spooler, on node 1
Printer: txt        (ready) 
0000:     root	[job #1    ]   active 1146 bytes	/etc/profile
0001:     root	[job #2    ]    ready 2378 bytes	/etc/install
0002:     root	[job #3    ]    ready 1146 bytes	-- standard input --
****************************************************************************/
static BOOL parse_lpq_qnx(char *line,print_queue_struct *buf,BOOL first)
{
  fstring tok[7];
  int count=0;

  DEBUG(4,("antes [%s]\n", line));

  /* handle the case of "-- standard input --" as a filename */
  pstring_sub(line,"standard input","STDIN");
  DEBUG(4,("despues [%s]\n", line));
  all_string_sub(line,"-- ","\"",0);
  all_string_sub(line," --","\"",0);
  DEBUG(4,("despues 1 [%s]\n", line));

  pstring_sub(line,"[job #","");
  pstring_sub(line,"]","");
  DEBUG(4,("despues 2 [%s]\n", line));

  
  
  for (count=0; count<7 && next_token(&line,tok[count],NULL,sizeof(tok[count])); count++) ;

  /* we must get 7 tokens */
  if (count < 7)
    return(False);

  /* the 3rd and 5th columns must be integer */
  if (!isdigit((int)*tok[2]) || !isdigit((int)*tok[4])) return(False);

  /* only take the last part of the filename */
  {
    fstring tmp;
    char *p = strrchr(tok[6],'/');
    if (p)
      {
	fstrcpy(tmp,p+1);
	fstrcpy(tok[6],tmp);
      }
  }
	

  buf->job = atoi(tok[2]);
  buf->size = atoi(tok[4]);
  buf->status = strequal(tok[3],"active")?LPQ_PRINTING:LPQ_QUEUED;
  buf->priority = 0;
  buf->time = time(NULL);
  StrnCpy(buf->user,tok[1],sizeof(buf->user)-1);
  StrnCpy(buf->file,tok[6],sizeof(buf->file)-1);
  return(True);
}


/****************************************************************************
  parse a lpq line for the plp printing system
  Bertrand Wallrich <Bertrand.Wallrich@loria.fr>

redone by tridge. Here is a sample queue:

Local  Printer 'lp2' (fjall):
  Printing (started at Jun 15 13:33:58, attempt 1).
    Rank Owner       Pr Opt  Job Host        Files           Size     Date
  active tridge      X  -    6   fjall       /etc/hosts      739      Jun 15 13:33
     3rd tridge      X  -    7   fjall       /etc/hosts      739      Jun 15 13:33

****************************************************************************/
static BOOL parse_lpq_plp(char *line,print_queue_struct *buf,BOOL first)
{
  fstring tok[11];
  int count=0;

  /* handle the case of "(standard input)" as a filename */
  pstring_sub(line,"stdin","STDIN");
  all_string_sub(line,"(","\"",0);
  all_string_sub(line,")","\"",0);
  
  for (count=0; count<11 && next_token(&line,tok[count],NULL,sizeof(tok[count])); count++) ;

  /* we must get 11 tokens */
  if (count < 11)
    return(False);

  /* the first must be "active" or begin with an integer */
  if (strcmp(tok[0],"active") && !isdigit((int)tok[0][0]))
    return(False);

  /* the 5th and 8th must be integer */
  if (!isdigit((int)*tok[4]) || !isdigit((int)*tok[7])) 
    return(False);

  /* if the fname contains a space then use STDIN */
  if (strchr(tok[6],' '))
    fstrcpy(tok[6],"STDIN");

  /* only take the last part of the filename */
  {
    fstring tmp;
    char *p = strrchr(tok[6],'/');
    if (p)
      {
        fstrcpy(tmp,p+1);
        fstrcpy(tok[6],tmp);
      }
  }


  buf->job = atoi(tok[4]);

  buf->size = atoi(tok[7]);
  if (strchr(tok[7],'K'))
    buf->size *= 1024;
  if (strchr(tok[7],'M'))
    buf->size *= 1024*1024;

  buf->status = strequal(tok[0],"active")?LPQ_PRINTING:LPQ_QUEUED;
  buf->priority = 0;
  buf->time = time(NULL);
  StrnCpy(buf->user,tok[1],sizeof(buf->user)-1);
  StrnCpy(buf->file,tok[6],sizeof(buf->file)-1);
  return(True);
}

/****************************************************************************
parse a qstat line

here is an example of "qstat -l -d qms" output under softq

Queue qms: 2 jobs; daemon active (313); enabled; accepting;
 job-ID   submission-time     pri     size owner      title 
205980: H 98/03/09 13:04:05     0    15733 stephenf   chap1.ps
206086:>  98/03/12 17:24:40     0      659 chris      -
206087:   98/03/12 17:24:45     0     4876 chris      -
Total:      21268 bytes in queue


****************************************************************************/
static BOOL parse_lpq_softq(char *line,print_queue_struct *buf,BOOL first)
{
  fstring tok[10];
  int count=0;

  /* mung all the ":"s to spaces*/
  pstring_sub(line,":"," ");
  
  for (count=0; count<10 && next_token(&line,tok[count],NULL,sizeof(tok[count])); count++) ;

  /* we must get 9 tokens */
  if (count < 9)
    return(False);

  /* the 1st and 7th columns must be integer */
  if (!isdigit((int)*tok[0]) || !isdigit((int)*tok[6]))  return(False);
  /* if the 2nd column is either '>' or 'H' then the 7th and 8th must be
   * integer, else it's the 6th and 7th that must be
   */
  if (*tok[1] == 'H' || *tok[1] == '>')
    {
      if (!isdigit((int)*tok[7]))
        return(False);
      buf->status = *tok[1] == '>' ? LPQ_PRINTING : LPQ_PAUSED;
      count = 1;
    }
  else
    {
      if (!isdigit((int)*tok[5]))
        return(False);
      buf->status = LPQ_QUEUED;
      count = 0;
    }
	

  buf->job = atoi(tok[0]);
  buf->size = atoi(tok[count+6]);
  buf->priority = atoi(tok[count+5]);
  StrnCpy(buf->user,tok[count+7],sizeof(buf->user)-1);
  StrnCpy(buf->file,tok[count+8],sizeof(buf->file)-1);
  buf->time = time(NULL);		/* default case: take current time */
  {
    time_t jobtime;
    struct tm *t;

    t = localtime(&buf->time);
    t->tm_mday = atoi(tok[count+2]+6);
    t->tm_mon  = atoi(tok[count+2]+3);
    switch (*tok[count+2])
    {
    case 7: case 8: case 9: t->tm_year = atoi(tok[count+2]); break;
    default:                t->tm_year = atoi(tok[count+2]); break;
    }

    t->tm_hour = atoi(tok[count+3]);
    t->tm_min = atoi(tok[count+4]);
    t->tm_sec = atoi(tok[count+5]);
    jobtime = mktime(t);
    if (jobtime != (time_t)-1)
      buf->time = jobtime; 
  }

  return(True);
}


char *stat0_strings[] = { "enabled", "online", "idle", "no entries", "free", "ready", NULL };
char *stat1_strings[] = { "offline", "disabled", "down", "off", "waiting", "no daemon", NULL };
char *stat2_strings[] = { "jam", "paper", "error", "responding", "not accepting", "not running", "turned off", NULL };

/****************************************************************************
parse a lpq line. Choose printing style
****************************************************************************/
static BOOL parse_lpq_entry(int snum,char *line,
			    print_queue_struct *buf,
			    print_status_struct *status,BOOL first)
{
  BOOL ret;

  switch (lp_printing(snum))
    {
    case PRINT_SYSV:
      ret = parse_lpq_sysv(line,buf,first);
      break;
    case PRINT_AIX:      
      ret = parse_lpq_aix(line,buf,first);
      break;
    case PRINT_HPUX:
      ret = parse_lpq_hpux(line,buf,first);
      break;
    case PRINT_QNX:
      ret = parse_lpq_qnx(line,buf,first);
      break;
    case PRINT_LPRNG:
      ret = parse_lpq_lprng(line,buf,first);
      break;
    case PRINT_PLP:
      ret = parse_lpq_plp(line,buf,first);
      break;
    case PRINT_SOFTQ:
      ret = parse_lpq_softq(line,buf,first);
      break;
    default:
      ret = parse_lpq_bsd(line,buf,first);
      break;
    }

#ifdef LPQ_GUEST_TO_USER
  if (ret) {
    extern pstring sesssetup_user;
    /* change guest entries to the current logged in user to make
       them appear deletable to windows */
    if (sesssetup_user[0] && strequal(buf->user,lp_guestaccount(snum)))
      pstrcpy(buf->user,sesssetup_user);
  }
#endif

  /* We don't want the newline in the status message. */
  {
    char *p = strchr(line,'\n');
    if (p) *p = 0;
  }

  /* in the LPRNG case, we skip lines starting by a space.*/
  if (line && !ret && (lp_printing(snum)==PRINT_LPRNG) )
  {
  	if (line[0]==' ')
		return ret;
  }


  if (status && !ret)
    {
      /* a few simple checks to see if the line might be a
         printer status line: 
	 handle them so that most severe condition is shown */
      int i;
      strlower(line);
      
      switch (status->status) {
      case LPSTAT_OK:
	for (i=0; stat0_strings[i]; i++)
	  if (strstr(line,stat0_strings[i])) {
	    StrnCpy(status->message,line,sizeof(status->message)-1);
	    status->status=LPSTAT_OK;
	    return ret;
	  }
      case LPSTAT_STOPPED:
	for (i=0; stat1_strings[i]; i++)
	  if (strstr(line,stat1_strings[i])) {
	    StrnCpy(status->message,line,sizeof(status->message)-1);
	    status->status=LPSTAT_STOPPED;
	    return ret;
	  }
      case LPSTAT_ERROR:
	for (i=0; stat2_strings[i]; i++)
	  if (strstr(line,stat2_strings[i])) {
	    StrnCpy(status->message,line,sizeof(status->message)-1);
	    status->status=LPSTAT_ERROR;
	    return ret;
	  }
	break;
      }
    }

  return(ret);
}

/****************************************************************************
get a printer queue
****************************************************************************/
int get_printqueue(int snum, 
		   connection_struct *conn,print_queue_struct **queue,
		   print_status_struct *status)
{
	char *lpq_command = lp_lpqcommand(snum);
	char *printername = PRINTERNAME(snum);
	int ret=0,count=0;
	pstring syscmd;
	fstring outfile;
	pstring line;
	FILE *f;
	SMB_STRUCT_STAT sbuf;
	BOOL dorun=True;
	int cachetime = lp_lpqcachetime();
	
	*line = 0;
	check_lpq_cache(snum);
	
	if (!printername || !*printername) {
		DEBUG(6,("xx replacing printer name with service (snum=(%s,%d))\n",
			 lp_servicename(snum),snum));
		printername = lp_servicename(snum);
	}
    
	if (!lpq_command || !(*lpq_command)) {
		DEBUG(5,("No lpq command\n"));
		return(0);
	}
    
	pstrcpy(syscmd,lpq_command);
	pstring_sub(syscmd,"%p",printername);

	standard_sub(conn,syscmd);

	slprintf(outfile,sizeof(outfile)-1, "%s/lpq.%08x",lp_lockdir(),str_checksum(syscmd));
  
	if (!lpq_cache_reset[snum] && cachetime && !sys_stat(outfile,&sbuf)) {
		if (time(NULL) - sbuf.st_mtime < cachetime) {
			DEBUG(3,("Using cached lpq output\n"));
			dorun = False;
		}
	}

	if (dorun) {
		ret = smbrun(syscmd,outfile,True);
		DEBUG(3,("Running the command `%s' gave %d\n",syscmd,ret));
	}

	lpq_cache_reset[snum] = False;

	f = sys_fopen(outfile,"r");
	if (!f) {
		return(0);
	}

	if (status) {
		fstrcpy(status->message,"");
		status->status = LPSTAT_OK;
	}
	
	while (fgets(line,sizeof(pstring),f)) {
		DEBUG(6,("QUEUE2: %s\n",line));
		
		*queue = Realloc(*queue,sizeof(print_queue_struct)*(count+1));
		if (! *queue) {
			count = 0;
			break;
		}

		memset((char *)&(*queue)[count],'\0',sizeof(**queue));
	  
		/* parse it */
		if (!parse_lpq_entry(snum,line,
				     &(*queue)[count],status,count==0))
			continue;
		
		count++;
	}	      

	fclose(f);
	
	if (!cachetime) {
		unlink(outfile);
	}
	return(count);
}


/****************************************************************************
delete a printer queue entry
****************************************************************************/
void del_printqueue(connection_struct *conn,int snum,int jobid)
{
  char *lprm_command = lp_lprmcommand(snum);
  char *printername = PRINTERNAME(snum);
  pstring syscmd;
  char jobstr[20];
  int ret;

  if (!printername || !*printername)
    {
      DEBUG(6,("replacing printer name with service (snum=(%s,%d))\n",
	    lp_servicename(snum),snum));
      printername = lp_servicename(snum);
    }
    
  if (!lprm_command || !(*lprm_command))
    {
      DEBUG(5,("No lprm command\n"));
      return;
    }
    
  slprintf(jobstr,sizeof(jobstr)-1,"%d",jobid);

  pstrcpy(syscmd,lprm_command);
  pstring_sub(syscmd,"%p",printername);
  pstring_sub(syscmd,"%j",jobstr);
  standard_sub(conn,syscmd);

  ret = smbrun(syscmd,NULL,False);
  DEBUG(3,("Running the command `%s' gave %d\n",syscmd,ret));  
  lpq_reset(snum); /* queue has changed */
}

/****************************************************************************
change status of a printer queue entry
****************************************************************************/
void status_printjob(connection_struct *conn,int snum,int jobid,int status)
{
  char *lpstatus_command = 
    (status==LPQ_PAUSED?lp_lppausecommand(snum):lp_lpresumecommand(snum));
  char *printername = PRINTERNAME(snum);
  pstring syscmd;
  char jobstr[20];
  int ret;

  if (!printername || !*printername)
    {
      DEBUG(6,("replacing printer name with service (snum=(%s,%d))\n",
	    lp_servicename(snum),snum));
      printername = lp_servicename(snum);
    }
    
  if (!lpstatus_command || !(*lpstatus_command))
    {
      DEBUG(5,("No lpstatus command to %s job\n",
	       (status==LPQ_PAUSED?"pause":"resume")));
      return;
    }
    
  slprintf(jobstr,sizeof(jobstr)-1,"%d",jobid);

  pstrcpy(syscmd,lpstatus_command);
  pstring_sub(syscmd,"%p",printername);
  pstring_sub(syscmd,"%j",jobstr);
  standard_sub(conn,syscmd);

  ret = smbrun(syscmd,NULL,False);
  DEBUG(3,("Running the command `%s' gave %d\n",syscmd,ret));  
  lpq_reset(snum); /* queue has changed */
}



/****************************************************************************
we encode print job numbers over the wire so that when we get them back we can
tell not only what print job they are but also what service it belongs to,
this is to overcome the problem that windows clients tend to send the wrong
service number when doing print queue manipulation!
****************************************************************************/
int printjob_encode(int snum, int job)
{
	return ((snum&0xFF)<<8) | (job & 0xFF);
}

/****************************************************************************
and now decode them again ...
****************************************************************************/
void printjob_decode(int jobid, int *snum, int *job)
{
	(*snum) = (jobid >> 8) & 0xFF;
	(*job) = jobid & 0xFF;
}

/****************************************************************************
 Change status of a printer queue
****************************************************************************/

void status_printqueue(connection_struct *conn,int snum,int status)
{
  char *queuestatus_command = (status==LPSTAT_STOPPED ? 
                               lp_queuepausecommand(snum):lp_queueresumecommand(snum));
  char *printername = PRINTERNAME(snum);
  pstring syscmd;
  int ret;

  if (!printername || !*printername) {
    DEBUG(6,("replacing printer name with service (snum=(%s,%d))\n",
          lp_servicename(snum),snum));
    printername = lp_servicename(snum);
  }

  if (!queuestatus_command || !(*queuestatus_command)) {
    DEBUG(5,("No queuestatus command to %s job\n",
          (status==LPSTAT_STOPPED?"pause":"resume")));
    return;
  }

  pstrcpy(syscmd,queuestatus_command);
  pstring_sub(syscmd,"%p",printername);
  standard_sub(conn,syscmd);

  ret = smbrun(syscmd,NULL,False);
  DEBUG(3,("Running the command `%s' gave %d\n",syscmd,ret));
  lpq_reset(snum); /* queue has changed */
}



/***************************************************************************
auto-load printer services
***************************************************************************/
static void add_all_printers(void)
{
	int printers = lp_servicenumber(PRINTERS_NAME);

	if (printers < 0) return;

	pcap_printer_fn(lp_add_one_printer);
}

/***************************************************************************
auto-load some homes and printer services
***************************************************************************/
static void add_auto_printers(void)
{
	char *p;
	int printers;
	char *str = lp_auto_services();

	if (!str) return;

	printers = lp_servicenumber(PRINTERS_NAME);

	if (printers < 0) return;
	
	for (p=strtok(str,LIST_SEP);p;p=strtok(NULL,LIST_SEP)) {
		if (lp_servicenumber(p) >= 0) continue;
		
		if (pcap_printername_ok(p,NULL)) {
			lp_add_printer(p,printers);
		}
	}
}

/***************************************************************************
load automatic printer services
***************************************************************************/
void load_printers(void)
{
	add_auto_printers();
	if (lp_load_printers())
		add_all_printers();
}
