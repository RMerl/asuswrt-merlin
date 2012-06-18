/*
 * $Id: util.c,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1995,1996,1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#include <includes.h>
#include <radiusclient.h>

/*
 * Function: rc_str2tm
 *
 * Purpose: Turns printable string into correct tm struct entries.
 *
 */

static const char * months[] =
		{
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};

void rc_str2tm (char *valstr, struct tm *tm)
{
	int             i;

	/* Get the month */
	for (i = 0; i < 12; i++)
	{
		if (strncmp (months[i], valstr, 3) == 0)
		{
			tm->tm_mon = i;
			i = 13;
		}
	}

	/* Get the Day */
	tm->tm_mday = atoi (&valstr[4]);

	/* Now the year */
	tm->tm_year = atoi (&valstr[7]) - 1900;
}

void rc_mdelay(int msecs)
{
	struct timeval tv;

	tv.tv_sec = (int) msecs / 1000;
	tv.tv_usec = (msecs % 1000) * 1000;

	select(0,(fd_set *)NULL,(fd_set *)NULL,(fd_set *)NULL, &tv);
}

/*
 * Function: rc_mksid
 *
 * Purpose: generate a quite unique string
 *
 * Remarks: not that unique at all...
 *
 */

char *
rc_mksid (void)
{
  static char buf[15];
  static unsigned short int cnt = 0;
  sprintf (buf, "%08lX%04X%02hX",
	   (unsigned long int) time (NULL),
	   (unsigned int) getpid (),
	   cnt & 0xFF);
  cnt++;
  return buf;
}
