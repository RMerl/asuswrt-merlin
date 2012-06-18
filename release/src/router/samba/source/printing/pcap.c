/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   printcap parsing
   Copyright (C) Karl Auer 1993-1998

   Re-working by Martin Kiff, 1994
   
   Re-written again by Andrew Tridgell

   Modified for SVID support by Norm Jacobs, 1997

   Modified for CUPS support by Michael Sweet, 1999
   
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

/*
 *  Parse printcap file.
 *
 *  This module does exactly one thing - it looks into the printcap file
 *  and tells callers if a specified string appears as a printer name.
 *
 *  The way this module looks at the printcap file is very simplistic.
 *  Only the local printcap file is inspected (no searching of NIS
 *  databases etc).
 *
 *  There are assumed to be one or more printer names per record, held
 *  as a set of sub-fields separated by vertical bar symbols ('|') in the
 *  first field of the record. The field separator is assumed to be a colon
 *  ':' and the record separator a newline.
 * 
 *  Lines ending with a backspace '\' are assumed to flag that the following
 *  line is a continuation line so that a set of lines can be read as one
 *  printcap entry.
 *
 *  A line stating with a hash '#' is assumed to be a comment and is ignored
 *  Comments are discarded before the record is strung together from the
 *  set of continuation lines.
 *
 *  Opening a pipe for "lpc status" and reading that would probably 
 *  be pretty effective. Code to do this already exists in the freely
 *  distributable PCNFS server code.
 *
 *  Modified to call SVID/XPG4 support if printcap name is set to "lpstat"
 *  in smb.conf under Solaris.
 *
 *  Modified to call CUPS support if printcap name is set to "cups"
 *  in smb.conf.
 */

#include "includes.h"

#include "smb.h"

extern int DEBUGLEVEL;

#ifdef AIX
/*  ******************************************
     Extend for AIX system and qconfig file
       from 'boulard@univ-rennes1.fr
    ****************************************** */
static int strlocate(char *xpLine,char *xpS)
{
	int iS,iL,iRet;
	char *p;
	iS = strlen(xpS);
	iL = strlen(xpLine);

	iRet = 0;
	p = xpLine;
	while (iL >= iS)
	{
		if (strncmp(p,xpS,iS) == 0) {iRet =1;break;};
		p++;
		iL--;
	}
	/*DEBUG(3,(" strlocate %s in line '%s',ret=%d\n",xpS,xpLine,iRet));*/
	
	return(iRet);
}
	
	
/* ******************************************************************* */
/* *    Scan qconfig and search all virtual printer (device printer) * */
/* ******************************************************************* */
static void ScanQconfig_fn(char *psz,void (*fn)(char *, char *))
{
	int iEtat;
	FILE *pfile;
	char *line,*p;
	pstring name,comment;
	line  = NULL;
	*name = 0;
	*comment = 0;

	if ((pfile = sys_fopen(psz, "r")) == NULL)
	{
	      DEBUG(0,( "Unable to open qconfig file %s for read!\n", psz));
	      return;
	}

	iEtat = 0;
	/* scan qconfig file for searching <printername>:	*/
	for (;(line = fgets_slash(NULL,sizeof(pstring),pfile)); free(line))
	{
		if (*line == '*' || *line == 0)
		continue;
		switch (iEtat)
		{
			case 0: /* locate an entry */
			 if (*line == '\t' || *line == ' ') continue;
			 if ((p=strchr(line,':')))
			 {
			 	*p = '\0';
				p = strtok(line,":");
				if (strcmp(p,"bsh")!=0)
				  {
				    pstrcpy(name,p);
				    iEtat = 1;
				    continue;
				  }
			 }
			 break;
			case 1: /* scanning device stanza */
			 if (*line == '*' || *line == 0) continue;
		  	 if (*line != '\t' && *line != ' ')
		  	 {
		  	   /* name is found without stanza device  */
		  	   /* probably a good printer ???		*/
		  	   fn(name,comment);
		  	   iEtat = 0;
		  	   continue;
		  	  }
		  	
		  	  if (strlocate(line,"backend"))
		  	  {
		  		/* it's a device, not a virtual printer*/
		  		iEtat = 0;
		  	  }
		  	  else if (strlocate(line,"device"))
		  	  {
		  		/* it's a good virtual printer */
		  		fn(name,comment);
		  		iEtat = 0;
		  		continue;
		  	  }
		  	  break;
		}
	}
	fclose(pfile);
}

/* Scan qconfig file and locate de printername */

static BOOL ScanQconfig(char *psz,char *pszPrintername)
{
	int iLg,iEtat;
	FILE *pfile;
	char *pName;
	char *line;

	pName = NULL;
	line  = NULL;
	if ((pszPrintername!= NULL) && ((iLg = strlen(pszPrintername)) > 0))
	 pName = malloc(iLg+10);
	if (pName == NULL)
	{
		DEBUG(0,(" Unable to allocate memory for printer %s\n",pszPrintername));
		return(False);
	}
	if ((pfile = sys_fopen(psz, "r")) == NULL)
	{
	      DEBUG(0,( "Unable to open qconfig file %s for read!\n", psz));
	      free(pName);
	      return(False);
	}
	slprintf(pName, iLg + 9, "%s:",pszPrintername);
	iLg = strlen(pName);
	/*DEBUG(3,( " Looking for entry %s\n",pName));*/
	iEtat = 0;
	/* scan qconfig file for searching <printername>:	*/
	for (;(line = fgets_slash(NULL,sizeof(pstring),pfile)); free(line))
	{
		if (*line == '*' || *line == 0)
		continue;
		switch (iEtat)
		{
			case 0: /* scanning entry */
			 if (strncmp(line,pName,iLg) == 0)
			 {
			 	iEtat = 1;
			 	continue;
			 }
			 break;
			case 1: /* scanning device stanza */
			 if (*line == '*' || *line == 0) continue;
		  	 if (*line != '\t' && *line != ' ')
		  	 {
		  	   /* name is found without stanza device  */
		  	   /* probably a good printer ???		*/
		  	   free (line);
		  	   free(pName);
		  	   fclose(pfile);
		  	   return(True);
		  	  }
		  	
		  	  if (strlocate(line,"backend"))
		  	  {
		  		/* it's a device, not a virtual printer*/
		  		iEtat = 0;
		  	  }
		  	  else if (strlocate(line,"device"))
		  	  {
		  		/* it's a good virtual printer */
		  		free (line);
		  		free(pName);
		  		fclose(pfile);
		  		return(True);
		  	  }
		  	  break;
		}
	}
	free (pName);
	fclose(pfile);
	return(False);
}
#endif /* AIX */


/***************************************************************************
Scan printcap file pszPrintcapname for a printer called pszPrintername. 
Return True if found, else False. Returns False on error, too, after logging 
the error at level 0. For generality, the printcap name may be passed - if
passed as NULL, the configuration will be queried for the name.
***************************************************************************/
BOOL pcap_printername_ok(char *pszPrintername, char *pszPrintcapname)
{
  char *line=NULL;
  char *psz;
  char *p,*q;
  FILE *pfile;

  if (pszPrintername == NULL || pszPrintername[0] == '\0')
    {
      DEBUG(0,( "Attempt to locate null printername! Internal error?\n"));
      return(False);
    }

  /* only go looking if no printcap name supplied */
  if ((psz = pszPrintcapname) == NULL || psz[0] == '\0')
    if (((psz = lp_printcapname()) == NULL) || (psz[0] == '\0'))
      {
	DEBUG(0,( "No printcap file name configured!\n"));
	return(False);
      }

#ifdef HAVE_LIBCUPS
    if (strequal(psz, "cups"))
       return (cups_printername_ok(pszPrintername));
#endif /* HAVE_LIBCUPS */

#ifdef SYSV
    if (strequal(psz, "lpstat"))
       return (sysv_printername_ok(pszPrintername));
#endif

#ifdef AIX
  if (strlocate(psz,"/qconfig"))
     return(ScanQconfig(psz,pszPrintername));
#endif

  if ((pfile = sys_fopen(psz, "r")) == NULL)
    {
      DEBUG(0,( "Unable to open printcap file %s for read!\n", psz));
      return(False);
    }

  for (;(line = fgets_slash(NULL,sizeof(pstring),pfile)); free(line))
    {
      if (*line == '#' || *line == 0)
	continue;

      /* now we have a real printer line - cut it off at the first : */      
      p = strchr(line,':');
      if (p) *p = 0;
      
      /* now just check if the name is in the list */
      /* NOTE: I avoid strtok as the fn calling this one may be using it */
      for (p=line; p; p=q)
	{
	  if ((q = strchr(p,'|'))) *q++ = 0;

	  if (strequal(p,pszPrintername))
	    {
	      /* normalise the case */
	      pstrcpy(pszPrintername,p);
	      free(line);
	      fclose(pfile);
	      return(True);	      
	    }
	  p = q;
	}
    }

  fclose(pfile);
  return(False);
}


/***************************************************************************
run a function on each printer name in the printcap file. The function is 
passed the primary name and the comment (if possible)
***************************************************************************/
void pcap_printer_fn(void (*fn)(char *, char *))
{
  pstring name,comment;
  char *line;
  char *psz;
  char *p,*q;
  FILE *pfile;

  /* only go looking if no printcap name supplied */
  if (((psz = lp_printcapname()) == NULL) || (psz[0] == '\0'))
    {
      DEBUG(0,( "No printcap file name configured!\n"));
      return;
    }

#ifdef HAVE_LIBCUPS
    if (strequal(psz, "cups")) {
      cups_printer_fn(fn);
      return;
    }
#endif /* HAVE_LIBCUPS */

#ifdef SYSV
    if (strequal(psz, "lpstat")) {
      sysv_printer_fn(fn);
      return;
    }
#endif

#ifdef AIX
  if (strlocate(psz,"/qconfig"))
  {
  	ScanQconfig_fn(psz,fn);
     return;
  }
#endif

  if ((pfile = sys_fopen(psz, "r")) == NULL)
    {
      DEBUG(0,( "Unable to open printcap file %s for read!\n", psz));
      return;
    }

  for (;(line = fgets_slash(NULL,sizeof(pstring),pfile)); free(line))
    {
      if (*line == '#' || *line == 0)
	continue;

      /* now we have a real printer line - cut it off at the first : */      
      p = strchr(line,':');
      if (p) *p = 0;
      
      /* now find the most likely printer name and comment 
       this is pure guesswork, but it's better than nothing */
      *name = 0;
      *comment = 0;
      for (p=line; p; p=q)
	{
	  BOOL has_punctuation;
	  if ((q = strchr(p,'|'))) *q++ = 0;

	  has_punctuation = (strchr(p,' ') || strchr(p,'(') || strchr(p,')'));

	  if (strlen(p)>strlen(comment) && has_punctuation)
	    {
	      StrnCpy(comment,p,sizeof(comment)-1);
	      continue;
	    }

	  if (strlen(p) <= MAXPRINTERLEN && strlen(p)>strlen(name) && !has_punctuation)
	    {
	      if (!*comment) pstrcpy(comment,name);
	      pstrcpy(name,p);
	      continue;
	    }

	  if (!strchr(comment,' ') && 
	      strlen(p) > strlen(comment))
	    {
	      StrnCpy(comment,p,sizeof(comment)-1);
	      continue;
	    }
	}

      comment[60] = 0;
      name[MAXPRINTERLEN] = 0;

      if (*name)
	fn(name,comment);
    }
  fclose(pfile);
}
